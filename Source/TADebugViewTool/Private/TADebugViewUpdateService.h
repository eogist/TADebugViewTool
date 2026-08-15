#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace TADebugViewTool
{
enum class EUpdateCheckState : uint8
{
	NotChecked,
	Checking,
	UpToDate,
	UpdateAvailable,
	Failed
};

class FTADebugViewUpdateService
{
public:
	void Initialize();
	void Shutdown();
	void CheckForUpdates(bool bForce);
	void OpenLatestRelease() const;

	EUpdateCheckState GetState() const;
	const FString& GetCurrentVersion() const;
	const FString& GetLatestVersion() const;
	const FString& GetLatestReleaseUrl() const;
	const FText& GetError() const;
	bool CanOpenLatestRelease() const;

	static TOptional<int32> CompareSemanticVersions(const FString& LeftVersion, const FString& RightVersion);
	static bool TryBuildTrustedReleaseUrl(const FString& ReleaseTag, FString& OutReleaseUrl);

private:
	void HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);

	FHttpRequestPtr ActiveRequest;
	EUpdateCheckState State = EUpdateCheckState::NotChecked;
	FString CurrentVersion;
	FString LatestVersion;
	FString LatestReleaseUrl;
	FText Error;
	bool bAutomaticCheckAttempted = false;
};
}
