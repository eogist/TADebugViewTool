#include "TADebugViewUpdateService.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "TADebugViewUpdateService"

namespace
{
constexpr float UpdateCheckTimeoutSeconds = 10.0f;
constexpr TCHAR LatestReleaseApiUrl[] = TEXT("https://api.github.com/repos/eogist/TADebugViewTool/releases/latest");
constexpr TCHAR TrustedReleaseUrlPrefix[] = TEXT("https://github.com/eogist/TADebugViewTool/releases/tag/");

struct FSemanticVersionIdentifier
{
	FString Value;
	uint64 NumericValue = 0;
	bool bIsNumeric = false;
};

struct FSemanticVersion
{
	uint64 Major = 0;
	uint64 Minor = 0;
	uint64 Patch = 0;
	TArray<FSemanticVersionIdentifier> Prerelease;
};

bool IsValidIdentifierCharacter(TCHAR Character)
{
	return FChar::IsAlnum(Character) || Character == TEXT('-');
}

bool ParseNumericIdentifier(const FString& Value, uint64& OutValue, bool bRejectLeadingZero)
{
	if (Value.IsEmpty() || (bRejectLeadingZero && Value.Len() > 1 && Value[0] == TEXT('0')))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsDigit(Character))
		{
			return false;
		}
	}
	return LexTryParseString(OutValue, *Value);
}

bool ValidateIdentifierList(const FString& Value, bool bPrerelease, TArray<FSemanticVersionIdentifier>* OutIdentifiers)
{
	if (Value.IsEmpty())
	{
		return false;
	}

	TArray<FString> Parts;
	Value.ParseIntoArray(Parts, TEXT("."), false);
	for (const FString& Part : Parts)
	{
		if (Part.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Part)
		{
			if (!IsValidIdentifierCharacter(Character))
			{
				return false;
			}
		}

		if (OutIdentifiers)
		{
			bool bAllDigits = true;
			for (const TCHAR Character : Part)
			{
				bAllDigits &= FChar::IsDigit(Character);
			}
			if (bPrerelease && bAllDigits && Part.Len() > 1 && Part[0] == TEXT('0'))
			{
				return false;
			}

			FSemanticVersionIdentifier& Identifier = OutIdentifiers->AddDefaulted_GetRef();
			Identifier.Value = Part;
			if (bAllDigits && !ParseNumericIdentifier(Part, Identifier.NumericValue, false))
			{
				return false;
			}
			Identifier.bIsNumeric = bAllDigits;
		}
	}
	return true;
}

TOptional<FSemanticVersion> ParseSemanticVersion(const FString& Version)
{
	FString Remaining = Version.TrimStartAndEnd();
	if (Remaining.StartsWith(TEXT("v"), ESearchCase::IgnoreCase))
	{
		Remaining.RightChopInline(1, EAllowShrinking::No);
	}

	FString BuildMetadata;
	FString VersionWithoutBuildMetadata;
	if (Remaining.Split(TEXT("+"), &VersionWithoutBuildMetadata, &BuildMetadata))
	{
		if (BuildMetadata.Contains(TEXT("+")) || !ValidateIdentifierList(BuildMetadata, false, nullptr))
		{
			return {};
		}
		Remaining = MoveTemp(VersionWithoutBuildMetadata);
	}

	FString CoreVersion = Remaining;
	FString Prerelease;
	Remaining.Split(TEXT("-"), &CoreVersion, &Prerelease);

	TArray<FString> CoreParts;
	CoreVersion.ParseIntoArray(CoreParts, TEXT("."), false);
	if (CoreParts.Num() != 3)
	{
		return {};
	}

	FSemanticVersion Parsed;
	if (!ParseNumericIdentifier(CoreParts[0], Parsed.Major, true)
		|| !ParseNumericIdentifier(CoreParts[1], Parsed.Minor, true)
		|| !ParseNumericIdentifier(CoreParts[2], Parsed.Patch, true))
	{
		return {};
	}
	if (!Prerelease.IsEmpty() && !ValidateIdentifierList(Prerelease, true, &Parsed.Prerelease))
	{
		return {};
	}
	return Parsed;
}

int32 CompareIdentifier(const FSemanticVersionIdentifier& Left, const FSemanticVersionIdentifier& Right)
{
	if (Left.bIsNumeric && Right.bIsNumeric)
	{
		return Left.NumericValue < Right.NumericValue ? -1 : Left.NumericValue > Right.NumericValue ? 1 : 0;
	}
	if (Left.bIsNumeric != Right.bIsNumeric)
	{
		return Left.bIsNumeric ? -1 : 1;
	}
	return FMath::Clamp(Left.Value.Compare(Right.Value, ESearchCase::CaseSensitive), -1, 1);
}
}

namespace TADebugViewTool
{
void FTADebugViewUpdateService::Initialize()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TADebugViewTool")))
	{
		CurrentVersion = Plugin->GetDescriptor().VersionName;
	}
}

void FTADebugViewUpdateService::Shutdown()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
}

void FTADebugViewUpdateService::CheckForUpdates(bool bForce)
{
	if (State == EUpdateCheckState::Checking || (!bForce && bAutomaticCheckAttempted))
	{
		return;
	}
	bAutomaticCheckAttempted = true;
	LatestVersion.Reset();
	LatestReleaseUrl.Reset();
	Error = FText::GetEmpty();
	State = EUpdateCheckState::Checking;

	if (!ParseSemanticVersion(CurrentVersion).IsSet())
	{
		State = EUpdateCheckState::Failed;
		Error = LOCTEXT("MissingLocalVersion", "Could not read a valid installed plugin version.");
		return;
	}

	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetURL(LatestReleaseApiUrl);
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetHeader(TEXT("Accept"), TEXT("application/vnd.github+json"));
	ActiveRequest->SetHeader(TEXT("User-Agent"), TEXT("TADebugViewTool-UpdateCheck"));
	ActiveRequest->SetHeader(TEXT("X-GitHub-Api-Version"), TEXT("2022-11-28"));
	ActiveRequest->SetTimeout(UpdateCheckTimeoutSeconds);
	ActiveRequest->OnProcessRequestComplete().BindRaw(this, &FTADebugViewUpdateService::HandleRequestComplete);
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		State = EUpdateCheckState::Failed;
		Error = LOCTEXT("RequestStartError", "The update request could not be started.");
	}
}

void FTADebugViewUpdateService::HandleRequestComplete(FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
{
	ActiveRequest.Reset();
	if (!bSucceeded || !Response.IsValid())
	{
		State = EUpdateCheckState::Failed;
		Error = LOCTEXT("NetworkError", "Could not reach GitHub. Check the network connection and try again.");
		return;
	}
	if (Response->GetResponseCode() != 200)
	{
		State = EUpdateCheckState::Failed;
		Error = FText::Format(
			LOCTEXT("HttpError", "GitHub returned HTTP {0}. Try again later."),
			FText::AsNumber(Response->GetResponseCode()));
		return;
	}

	TSharedPtr<FJsonObject> ReleaseObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, ReleaseObject) || !ReleaseObject.IsValid())
	{
		State = EUpdateCheckState::Failed;
		Error = LOCTEXT("InvalidResponse", "GitHub returned an unreadable release response.");
		return;
	}

	FString ReleaseTag;
	if (!ReleaseObject->TryGetStringField(TEXT("tag_name"), ReleaseTag) || ReleaseTag.IsEmpty())
	{
		State = EUpdateCheckState::Failed;
		Error = LOCTEXT("MissingFields", "The latest GitHub Release is missing version information.");
		return;
	}

	const TOptional<int32> Comparison = CompareSemanticVersions(CurrentVersion, ReleaseTag);
	if (!Comparison.IsSet() || !TryBuildTrustedReleaseUrl(ReleaseTag, LatestReleaseUrl))
	{
		State = EUpdateCheckState::Failed;
		Error = FText::Format(
			LOCTEXT("InvalidVersion", "Could not compare installed version {0} with release {1}."),
			FText::FromString(CurrentVersion),
			FText::FromString(ReleaseTag));
		return;
	}

	LatestVersion = ReleaseTag;
	LatestVersion.RemoveFromStart(TEXT("v"), ESearchCase::IgnoreCase);
	State = Comparison.GetValue() < 0 ? EUpdateCheckState::UpdateAvailable : EUpdateCheckState::UpToDate;
}

void FTADebugViewUpdateService::OpenLatestRelease() const
{
	if (CanOpenLatestRelease())
	{
		FPlatformProcess::LaunchURL(*LatestReleaseUrl, nullptr, nullptr);
	}
}

EUpdateCheckState FTADebugViewUpdateService::GetState() const
{
	return State;
}

const FString& FTADebugViewUpdateService::GetCurrentVersion() const
{
	return CurrentVersion;
}

const FString& FTADebugViewUpdateService::GetLatestVersion() const
{
	return LatestVersion;
}

const FString& FTADebugViewUpdateService::GetLatestReleaseUrl() const
{
	return LatestReleaseUrl;
}

const FText& FTADebugViewUpdateService::GetError() const
{
	return Error;
}

bool FTADebugViewUpdateService::CanOpenLatestRelease() const
{
	return !LatestReleaseUrl.IsEmpty();
}

TOptional<int32> FTADebugViewUpdateService::CompareSemanticVersions(const FString& LeftVersion, const FString& RightVersion)
{
	const TOptional<FSemanticVersion> Left = ParseSemanticVersion(LeftVersion);
	const TOptional<FSemanticVersion> Right = ParseSemanticVersion(RightVersion);
	if (!Left.IsSet() || !Right.IsSet())
	{
		return {};
	}

	const uint64 LeftCore[] = { Left->Major, Left->Minor, Left->Patch };
	const uint64 RightCore[] = { Right->Major, Right->Minor, Right->Patch };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (LeftCore[Index] != RightCore[Index])
		{
			return LeftCore[Index] < RightCore[Index] ? -1 : 1;
		}
	}

	if (Left->Prerelease.IsEmpty() != Right->Prerelease.IsEmpty())
	{
		return Left->Prerelease.IsEmpty() ? 1 : -1;
	}
	for (int32 Index = 0; Index < FMath::Min(Left->Prerelease.Num(), Right->Prerelease.Num()); ++Index)
	{
		const int32 Comparison = CompareIdentifier(Left->Prerelease[Index], Right->Prerelease[Index]);
		if (Comparison != 0)
		{
			return Comparison;
		}
	}
	return Left->Prerelease.Num() < Right->Prerelease.Num()
		? -1
		: Left->Prerelease.Num() > Right->Prerelease.Num() ? 1 : 0;
}

bool FTADebugViewUpdateService::TryBuildTrustedReleaseUrl(const FString& ReleaseTag, FString& OutReleaseUrl)
{
	OutReleaseUrl.Reset();
	if (!ParseSemanticVersion(ReleaseTag).IsSet())
	{
		return false;
	}

	FString NormalizedTag = ReleaseTag.TrimStartAndEnd();
	if (!NormalizedTag.StartsWith(TEXT("v"), ESearchCase::IgnoreCase))
	{
		NormalizedTag = TEXT("v") + NormalizedTag;
	}
	OutReleaseUrl = FString(TrustedReleaseUrlPrefix) + NormalizedTag;
	return true;
}
}

#undef LOCTEXT_NAMESPACE
