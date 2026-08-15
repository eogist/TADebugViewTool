#include "TADebugViewWorkflowRegistry.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TADebugViewPresetRegistry.h"

#define LOCTEXT_NAMESPACE "TADebugViewWorkflowRegistry"

DEFINE_LOG_CATEGORY_STATIC(LogTADebugViewWorkflowRegistry, Log, All);

namespace TADebugViewTool
{
namespace
{
constexpr int32 WorkflowSchemaVersion = 2;
constexpr TCHAR WorkflowSchemaFormat[] = TEXT("TADebugViewToolWorkflows");
constexpr TCHAR PluginName[] = TEXT("TADebugViewTool");
constexpr TCHAR OverrideRelativePath[] = TEXT("TADebugViewTool/WorkflowOverrides.json");

TArray<FWorkflowPreset> CachedDefaultWorkflows;
TArray<FWorkflowPreset> CachedEffectiveWorkflows;
enum class EWorkflowFileLoadStatus : uint8
{
	Missing,
	Valid,
	Invalid
};

TArray<FWorkflowPreset> CachedOverrideWorkflows;
TArray<FString> CachedDiagnostics;
EWorkflowFileLoadStatus CachedOverrideLoadStatus = EWorkflowFileLoadStatus::Missing;
FString CachedOverrideFileHash;
bool bCachedOverrideFileExists = false;
bool bWorkflowCacheDirty = true;

FString GetDefaultWorkflowFilePath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	return Plugin.IsValid()
		? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/DefaultWorkflows.json"))
		: FString();
}

FString GetOverrideWorkflowFilePath()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), OverrideRelativePath);
}

FString HashWorkflowFileText(const FString& Text)
{
	FTCHARToUTF8 Utf8Text(*Text);
	FMD5 Hasher;
	Hasher.Update(reinterpret_cast<const uint8*>(Utf8Text.Get()), Utf8Text.Length());
	uint8 Digest[16];
	Hasher.Final(Digest);
	return BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
}

bool ReadWorkflowFileSnapshot(const FString& FilePath, bool& bOutExists, FString& OutHash, FString& OutError)
{
	bOutExists = IFileManager::Get().FileExists(*FilePath);
	OutHash.Reset();
	if (!bOutExists)
	{
		return true;
	}

	FString FileText;
	if (!FFileHelper::LoadFileToString(FileText, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not read workflow file for conflict detection: %s"), *FilePath);
		return false;
	}
	OutHash = HashWorkflowFileText(FileText);
	return true;
}

bool EnsureOverrideSnapshotIsWritable(FString& OutError)
{
	if (CachedOverrideLoadStatus == EWorkflowFileLoadStatus::Invalid)
	{
		OutError = TEXT("WorkflowOverrides.json is invalid or unreadable. Repair it, restore its .bak file, or revert it from source control before saving.");
		return false;
	}

	bool bCurrentExists = false;
	FString CurrentHash;
	if (!ReadWorkflowFileSnapshot(GetOverrideWorkflowFilePath(), bCurrentExists, CurrentHash, OutError))
	{
		bWorkflowCacheDirty = true;
		return false;
	}
	if (bCurrentExists != bCachedOverrideFileExists
		|| (bCurrentExists && CurrentHash != CachedOverrideFileHash))
	{
		OutError = TEXT("WorkflowOverrides.json changed outside this editor. The registry has been reloaded; review the new contents and try again.");
		bWorkflowCacheDirty = true;
		return false;
	}
	return true;
}

TOptional<EViewModeIndex> GetViewModeFromConfigValue(const FString& Value)
{
	static const TMap<FString, EViewModeIndex> ViewModeMap =
	{
		{ TEXT("VMI_Lit"), VMI_Lit },
		{ TEXT("VMI_Unlit"), VMI_Unlit },
		{ TEXT("VMI_Wireframe"), VMI_Wireframe },
		{ TEXT("VMI_Lit_DetailLighting"), VMI_Lit_DetailLighting },
		{ TEXT("VMI_LightingOnly"), VMI_LightingOnly },
		{ TEXT("VMI_ShaderComplexity"), VMI_ShaderComplexity },
		{ TEXT("VMI_QuadOverdraw"), VMI_QuadOverdraw },
		{ TEXT("VMI_ShaderComplexityWithQuadOverdraw"), VMI_ShaderComplexityWithQuadOverdraw },
		{ TEXT("VMI_MaterialTextureScaleAccuracy"), VMI_MaterialTextureScaleAccuracy },
		{ TEXT("VMI_LightComplexity"), VMI_LightComplexity },
		{ TEXT("VMI_LightmapDensity"), VMI_LightmapDensity },
		{ TEXT("VMI_StationaryLightOverlap"), VMI_StationaryLightOverlap },
		{ TEXT("VMI_ReflectionOverride"), VMI_ReflectionOverride },
		{ TEXT("VMI_CollisionPawn"), VMI_CollisionPawn },
		{ TEXT("VMI_CollisionVisibility"), VMI_CollisionVisibility },
		{ TEXT("VMI_LODColoration"), VMI_LODColoration },
		{ TEXT("VMI_VisualizeVirtualTexture"), VMI_VisualizeVirtualTexture }
	};

	if (const EViewModeIndex* ViewMode = ViewModeMap.Find(Value))
	{
		return *ViewMode;
	}
	return TOptional<EViewModeIndex>();
}

bool IsVisualizationModeSupported(ETADebugViewCustomActionType ActionType, FName VisualizationMode)
{
	EPresetActionType RuntimeType;
	switch (ActionType)
	{
	case ETADebugViewCustomActionType::NaniteVisualization:
		RuntimeType = EPresetActionType::NaniteVisualization;
		break;
	case ETADebugViewCustomActionType::LumenVisualization:
		RuntimeType = EPresetActionType::LumenVisualization;
		break;
	case ETADebugViewCustomActionType::VirtualShadowMapVisualization:
		RuntimeType = EPresetActionType::VirtualShadowMapVisualization;
		break;
	default:
		return false;
	}

	for (const FDebugViewGroup& Group : GetPresetGroups())
	{
		if (Group.Presets.ContainsByPredicate([RuntimeType, VisualizationMode](const FDebugViewPreset& Preset)
		{
			return Preset.ActionType == RuntimeType && Preset.VisualizationMode == VisualizationMode;
		}))
		{
			return true;
		}
	}
	return false;
}

bool IsCommandValueSupported(const FString& Command)
{
	if (Command.IsEmpty())
	{
		return false;
	}
	for (const TCHAR Character : Command)
	{
		if (Character < TEXT(' '))
		{
			return false;
		}
	}
	return true;
}

FString GetViewModeConfigValue(EViewModeIndex ViewMode)
{
	switch (ViewMode)
	{
	case VMI_Lit: return TEXT("VMI_Lit");
	case VMI_Unlit: return TEXT("VMI_Unlit");
	case VMI_Wireframe: return TEXT("VMI_Wireframe");
	case VMI_Lit_DetailLighting: return TEXT("VMI_Lit_DetailLighting");
	case VMI_LightingOnly: return TEXT("VMI_LightingOnly");
	case VMI_ShaderComplexity: return TEXT("VMI_ShaderComplexity");
	case VMI_QuadOverdraw: return TEXT("VMI_QuadOverdraw");
	case VMI_ShaderComplexityWithQuadOverdraw: return TEXT("VMI_ShaderComplexityWithQuadOverdraw");
	case VMI_MaterialTextureScaleAccuracy: return TEXT("VMI_MaterialTextureScaleAccuracy");
	case VMI_LightComplexity: return TEXT("VMI_LightComplexity");
	case VMI_LightmapDensity: return TEXT("VMI_LightmapDensity");
	case VMI_StationaryLightOverlap: return TEXT("VMI_StationaryLightOverlap");
	case VMI_ReflectionOverride: return TEXT("VMI_ReflectionOverride");
	case VMI_CollisionPawn: return TEXT("VMI_CollisionPawn");
	case VMI_CollisionVisibility: return TEXT("VMI_CollisionVisibility");
	case VMI_LODColoration: return TEXT("VMI_LODColoration");
	case VMI_VisualizeVirtualTexture: return TEXT("VMI_VisualizeVirtualTexture");
	default: return FString();
	}
}

bool ParseActionType(const FString& TypeString, ETADebugViewCustomActionType& OutActionType)
{
	if (TypeString.Equals(TEXT("ViewMode"), ESearchCase::IgnoreCase))
	{
		OutActionType = ETADebugViewCustomActionType::ViewMode;
		return true;
	}
	if (TypeString.Equals(TEXT("Nanite"), ESearchCase::IgnoreCase)
		|| TypeString.Equals(TEXT("NaniteVisualization"), ESearchCase::IgnoreCase))
	{
		OutActionType = ETADebugViewCustomActionType::NaniteVisualization;
		return true;
	}
	if (TypeString.Equals(TEXT("Lumen"), ESearchCase::IgnoreCase)
		|| TypeString.Equals(TEXT("LumenVisualization"), ESearchCase::IgnoreCase))
	{
		OutActionType = ETADebugViewCustomActionType::LumenVisualization;
		return true;
	}
	if (TypeString.Equals(TEXT("VSM"), ESearchCase::IgnoreCase)
		|| TypeString.Equals(TEXT("VirtualShadowMapVisualization"), ESearchCase::IgnoreCase))
	{
		OutActionType = ETADebugViewCustomActionType::VirtualShadowMapVisualization;
		return true;
	}
	if (TypeString.Equals(TEXT("Command"), ESearchCase::IgnoreCase))
	{
		OutActionType = ETADebugViewCustomActionType::Command;
		return true;
	}
	return false;
}

FString GetActionTypeString(ETADebugViewCustomActionType ActionType)
{
	switch (ActionType)
	{
	case ETADebugViewCustomActionType::ViewMode: return TEXT("ViewMode");
	case ETADebugViewCustomActionType::NaniteVisualization: return TEXT("Nanite");
	case ETADebugViewCustomActionType::LumenVisualization: return TEXT("Lumen");
	case ETADebugViewCustomActionType::VirtualShadowMapVisualization: return TEXT("VSM");
	case ETADebugViewCustomActionType::Command: return TEXT("Command");
	default: return TEXT("Invalid");
	}
}

bool ParseActionArray(
	const TArray<TSharedPtr<FJsonValue>>& JsonActions,
	TArray<FTADebugViewCustomAction>& OutActions,
	const FString& WorkflowId,
	const TCHAR* FieldName,
	TArray<FString>& OutDiagnostics)
{
	OutActions.Reset();
	for (int32 ActionIndex = 0; ActionIndex < JsonActions.Num(); ++ActionIndex)
	{
		const TSharedPtr<FJsonObject> ActionObject = JsonActions[ActionIndex].IsValid()
			? JsonActions[ActionIndex]->AsObject()
			: nullptr;
		if (!ActionObject.IsValid())
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s: %s action %d is not an object."), *WorkflowId, FieldName, ActionIndex + 1));
			return false;
		}

		FString TypeString;
		FString Value;
		ETADebugViewCustomActionType ActionType;
		if (!ActionObject->TryGetStringField(TEXT("type"), TypeString)
			|| !ParseActionType(TypeString, ActionType)
			|| !ActionObject->TryGetStringField(TEXT("value"), Value))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s: %s action %d has an invalid type or value."), *WorkflowId, FieldName, ActionIndex + 1));
			return false;
		}

		FTADebugViewCustomAction SavedAction(ActionType, Value.TrimStartAndEnd());
		if (!IsWorkflowActionRuntimeValid(SavedAction))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s: %s action %d is unsupported: %s."), *WorkflowId, FieldName, ActionIndex + 1, *Value));
			return false;
		}
		OutActions.Add(MoveTemp(SavedAction));
	}
	return true;
}

bool ParseWorkflowObject(
	const TSharedPtr<FJsonObject>& WorkflowObject,
	EWorkflowPresetSource Source,
	FWorkflowPreset& OutWorkflow,
	TArray<FString>& OutDiagnostics)
{
	if (!WorkflowObject.IsValid())
	{
		OutDiagnostics.Add(TEXT("Workflow entry is not an object."));
		return false;
	}

	FString Id;
	FString Label;
	FString Tooltip;
	FString IconName = TEXT("Icons.Settings");
	if (!WorkflowObject->TryGetStringField(TEXT("id"), Id)
		|| !WorkflowObject->TryGetStringField(TEXT("label"), Label)
		|| !WorkflowObject->TryGetStringField(TEXT("tooltip"), Tooltip))
	{
		OutDiagnostics.Add(TEXT("Workflow entry is missing id, label, or tooltip."));
		return false;
	}
	WorkflowObject->TryGetStringField(TEXT("icon"), IconName);
	Id.TrimStartAndEndInline();
	Label.TrimStartAndEndInline();
	Tooltip.TrimStartAndEndInline();
	IconName.TrimStartAndEndInline();
	if (IconName.IsEmpty())
	{
		IconName = TEXT("Icons.Settings");
	}
	if (Id.IsEmpty() || Label.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Workflow id and label must not be empty or whitespace."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ActivateJson = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* DeactivateJson = nullptr;
	if (!WorkflowObject->TryGetArrayField(TEXT("activateActions"), ActivateJson) || !ActivateJson)
	{
		OutDiagnostics.Add(FString::Printf(TEXT("%s: activateActions is missing."), *Id));
		return false;
	}
	WorkflowObject->TryGetArrayField(TEXT("deactivateActions"), DeactivateJson);

	TArray<FTADebugViewCustomAction> ActivateActions;
	TArray<FTADebugViewCustomAction> DeactivateActions;
	if (!ParseActionArray(*ActivateJson, ActivateActions, Id, TEXT("Activate"), OutDiagnostics)
		|| ActivateActions.IsEmpty())
	{
		OutDiagnostics.Add(FString::Printf(TEXT("%s: at least one valid activate action is required."), *Id));
		return false;
	}
	if (DeactivateJson && !ParseActionArray(*DeactivateJson, DeactivateActions, Id, TEXT("Restore"), OutDiagnostics))
	{
		return false;
	}

	TArray<FDebugViewAction> RuntimeActivate;
	TArray<FDebugViewAction> RuntimeDeactivate;
	for (const FTADebugViewCustomAction& SavedAction : ActivateActions)
	{
		FDebugViewAction RuntimeAction;
		if (ConvertSavedActionToRuntimeAction(SavedAction, RuntimeAction))
		{
			RuntimeActivate.Add(MoveTemp(RuntimeAction));
		}
	}
	for (const FTADebugViewCustomAction& SavedAction : DeactivateActions)
	{
		FDebugViewAction RuntimeAction;
		if (ConvertSavedActionToRuntimeAction(SavedAction, RuntimeAction))
		{
			RuntimeDeactivate.Add(MoveTemp(RuntimeAction));
		}
	}

	OutWorkflow = FWorkflowPreset(
		*Id,
		FText::FromString(Label),
		FText::FromString(Tooltip),
		FName(*IconName),
		MoveTemp(RuntimeActivate),
		MoveTemp(RuntimeDeactivate),
		Source);
	return !OutWorkflow.Id.IsNone() && !OutWorkflow.Label.IsEmpty();
}

EWorkflowFileLoadStatus LoadWorkflowFile(
	const FString& FilePath,
	EWorkflowPresetSource Source,
	bool bAllowMissing,
	bool bRequireAtLeastOneWorkflow,
	TArray<FWorkflowPreset>& OutWorkflows,
	TArray<FString>& OutDiagnostics,
	FString* OutFileHash = nullptr)
{
	OutWorkflows.Reset();
	if (OutFileHash)
	{
		OutFileHash->Reset();
	}
	if (FilePath.IsEmpty() || !IFileManager::Get().FileExists(*FilePath))
	{
		if (!bAllowMissing)
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Workflow JSON is missing: %s"), *FilePath));
		}
		return bAllowMissing ? EWorkflowFileLoadStatus::Missing : EWorkflowFileLoadStatus::Invalid;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Could not read workflow JSON: %s"), *FilePath));
		return EWorkflowFileLoadStatus::Invalid;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Workflow JSON is invalid: %s"), *FilePath));
		return EWorkflowFileLoadStatus::Invalid;
	}

	FString Format;
	double Version = 0;
	if (!RootObject->TryGetStringField(TEXT("format"), Format)
		|| Format != WorkflowSchemaFormat
		|| !RootObject->TryGetNumberField(TEXT("version"), Version)
		|| !FMath::IsFinite(Version)
		|| Version != static_cast<double>(WorkflowSchemaVersion))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Unsupported workflow JSON format or version: %s"), *FilePath));
		return EWorkflowFileLoadStatus::Invalid;
	}

	const TArray<TSharedPtr<FJsonValue>>* WorkflowValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("workflows"), WorkflowValues) || !WorkflowValues)
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Workflow JSON has no workflows array: %s"), *FilePath));
		return EWorkflowFileLoadStatus::Invalid;
	}
	if (bRequireAtLeastOneWorkflow && WorkflowValues->IsEmpty())
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Workflow JSON contains no workflows: %s"), *FilePath));
		return EWorkflowFileLoadStatus::Invalid;
	}

	TArray<FWorkflowPreset> ParsedWorkflows;
	TSet<FName> SeenIds;
	for (const TSharedPtr<FJsonValue>& WorkflowValue : *WorkflowValues)
	{
		FWorkflowPreset Workflow(
			TEXT("Invalid"),
			FText::GetEmpty(),
			FText::GetEmpty(),
			TEXT("Icons.Settings"),
			TArray<FDebugViewAction>(),
			TArray<FDebugViewAction>(),
			Source);
		if (!ParseWorkflowObject(WorkflowValue.IsValid() ? WorkflowValue->AsObject() : nullptr, Source, Workflow, OutDiagnostics))
		{
			return EWorkflowFileLoadStatus::Invalid;
		}
		if (SeenIds.Contains(Workflow.Id))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Duplicate workflow id: %s"), *Workflow.Id.ToString()));
			return EWorkflowFileLoadStatus::Invalid;
		}
		SeenIds.Add(Workflow.Id);
		ParsedWorkflows.Add(MoveTemp(Workflow));
	}

	OutWorkflows = MoveTemp(ParsedWorkflows);
	if (OutFileHash)
	{
		*OutFileHash = HashWorkflowFileText(JsonText);
	}
	return EWorkflowFileLoadStatus::Valid;
}

TSharedPtr<FJsonObject> SerializeWorkflow(const FWorkflowPreset& Workflow)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("id"), Workflow.Id.ToString());
	Object->SetStringField(TEXT("label"), Workflow.Label.ToString());
	Object->SetStringField(TEXT("tooltip"), Workflow.Tooltip.ToString());
	Object->SetStringField(TEXT("icon"), Workflow.IconName.ToString());

	auto SerializeActions = [](const TArray<FDebugViewAction>& RuntimeActions)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FDebugViewAction& RuntimeAction : RuntimeActions)
		{
			FTADebugViewCustomAction SavedAction;
			if (!ConvertRuntimeActionToSavedAction(RuntimeAction, SavedAction))
			{
				continue;
			}
			TSharedPtr<FJsonObject> ActionObject = MakeShared<FJsonObject>();
			ActionObject->SetStringField(TEXT("type"), GetActionTypeString(SavedAction.ActionType));
			ActionObject->SetStringField(TEXT("value"), SavedAction.Value);
			Values.Add(MakeShared<FJsonValueObject>(ActionObject));
		}
		return Values;
	};

	Object->SetArrayField(TEXT("activateActions"), SerializeActions(Workflow.ActivateActions));
	Object->SetArrayField(TEXT("deactivateActions"), SerializeActions(Workflow.DeactivateActions));
	return Object;
}

bool SaveOverrideWorkflows(const TArray<FWorkflowPreset>& Overrides, FString& OutError)
{
	if (!EnsureOverrideSnapshotIsWritable(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("format"), WorkflowSchemaFormat);
	RootObject->SetNumberField(TEXT("version"), WorkflowSchemaVersion);

	TArray<TSharedPtr<FJsonValue>> WorkflowValues;
	for (const FWorkflowPreset& Workflow : Overrides)
	{
		WorkflowValues.Add(MakeShared<FJsonValueObject>(SerializeWorkflow(Workflow)));
	}
	RootObject->SetArrayField(TEXT("workflows"), WorkflowValues);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("Could not serialize workflow overrides.");
		return false;
	}

	const FString FinalPath = GetOverrideWorkflowFilePath();
	const FString Directory = FPaths::GetPath(FinalPath);
	const FString TempPath = FString::Printf(
		TEXT("%s.%s.tmp"),
		*FinalPath,
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString BackupPath = FinalPath + TEXT(".bak");
	if (!IFileManager::Get().MakeDirectory(*Directory, true)
		|| !FFileHelper::SaveStringToFile(JsonText, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Could not stage workflow overrides: %s"), *TempPath);
		return false;
	}

	TArray<FWorkflowPreset> ValidationWorkflows;
	TArray<FString> ValidationDiagnostics;
	if (LoadWorkflowFile(
		TempPath,
		EWorkflowPresetSource::UserCreated,
		false,
		false,
		ValidationWorkflows,
		ValidationDiagnostics) != EWorkflowFileLoadStatus::Valid)
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		OutError = ValidationDiagnostics.IsEmpty() ? TEXT("Staged workflow overrides failed validation.") : ValidationDiagnostics[0];
		return false;
	}

	if (IFileManager::Get().FileExists(*FinalPath)
		&& IFileManager::Get().Copy(*BackupPath, *FinalPath, true, false) != 0)
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		OutError = FString::Printf(TEXT("Could not back up workflow overrides: %s"), *BackupPath);
		return false;
	}
	if (!IFileManager::Get().Move(*FinalPath, *TempPath, true, false, false, true))
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		OutError = FString::Printf(TEXT("Could not replace workflow overrides: %s"), *FinalPath);
		return false;
	}

	CachedOverrideLoadStatus = EWorkflowFileLoadStatus::Valid;
	bCachedOverrideFileExists = true;
	CachedOverrideFileHash = HashWorkflowFileText(JsonText);
	return true;
}

bool AreActionsEquivalent(const TArray<FDebugViewAction>& Left, const TArray<FDebugViewAction>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Left.Num(); ++Index)
	{
		const FDebugViewAction& LeftAction = Left[Index];
		const FDebugViewAction& RightAction = Right[Index];
		if (LeftAction.ActionType != RightAction.ActionType
			|| LeftAction.ViewModeIndex != RightAction.ViewModeIndex
			|| LeftAction.VisualizationMode != RightAction.VisualizationMode
			|| !LeftAction.Commands.TrimStartAndEnd().Equals(RightAction.Commands.TrimStartAndEnd(), ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}

bool AreWorkflowsEquivalent(const FWorkflowPreset& Left, const FWorkflowPreset& Right)
{
	return Left.Id == Right.Id
		&& Left.Label.ToString().Equals(Right.Label.ToString(), ESearchCase::IgnoreCase)
		&& Left.Tooltip.ToString().Equals(Right.Tooltip.ToString(), ESearchCase::IgnoreCase)
		&& Left.IconName == Right.IconName
		&& AreActionsEquivalent(Left.ActivateActions, Right.ActivateActions)
		&& AreActionsEquivalent(Left.DeactivateActions, Right.DeactivateActions);
}

bool BuildLegacyWorkflow(FTADebugViewCustomWorkflowPreset LegacyPreset, FWorkflowPreset& OutWorkflow, FString& OutError)
{
	LegacyPreset.EnsureId();
	LegacyPreset.MigrateLegacyCommands();
	LegacyPreset.Label.TrimStartAndEndInline();
	LegacyPreset.Tooltip.TrimStartAndEndInline();
	if (LegacyPreset.Label.IsEmpty() || LegacyPreset.ActivateActions.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Legacy workflow %s has no name or activation actions."), *LegacyPreset.Id);
		return false;
	}

	TArray<FDebugViewAction> ActivateActions;
	TArray<FDebugViewAction> DeactivateActions;
	for (const FTADebugViewCustomAction& SavedAction : LegacyPreset.ActivateActions)
	{
		FDebugViewAction RuntimeAction;
		if (!ConvertSavedActionToRuntimeAction(SavedAction, RuntimeAction))
		{
			OutError = FString::Printf(TEXT("Legacy workflow %s has an invalid activation action: %s"), *LegacyPreset.Id, *SavedAction.Value);
			return false;
		}
		ActivateActions.Add(MoveTemp(RuntimeAction));
	}
	for (const FTADebugViewCustomAction& SavedAction : LegacyPreset.DeactivateActions)
	{
		FDebugViewAction RuntimeAction;
		if (!ConvertSavedActionToRuntimeAction(SavedAction, RuntimeAction))
		{
			OutError = FString::Printf(TEXT("Legacy workflow %s has an invalid restore action: %s"), *LegacyPreset.Id, *SavedAction.Value);
			return false;
		}
		DeactivateActions.Add(MoveTemp(RuntimeAction));
	}

	OutWorkflow = FWorkflowPreset(
		*LegacyPreset.Id,
		FText::FromString(LegacyPreset.Label),
		FText::FromString(LegacyPreset.Tooltip),
		TEXT("Icons.Settings"),
		MoveTemp(ActivateActions),
		MoveTemp(DeactivateActions),
		EWorkflowPresetSource::UserCreated);
	return true;
}

void RebuildWorkflowCache()
{
	if (!bWorkflowCacheDirty)
	{
		return;
	}
	bWorkflowCacheDirty = false;
	CachedDiagnostics.Reset();
	CachedDefaultWorkflows.Reset();
	CachedEffectiveWorkflows.Reset();
	CachedOverrideWorkflows.Reset();

	if (LoadWorkflowFile(
		GetDefaultWorkflowFilePath(),
		EWorkflowPresetSource::Default,
		false,
		true,
		CachedDefaultWorkflows,
		CachedDiagnostics) != EWorkflowFileLoadStatus::Valid)
	{
		CachedDefaultWorkflows = GetWorkflowPresets();
		CachedDiagnostics.Add(TEXT("Using compiled workflow defaults because DefaultWorkflows.json could not be loaded."));
	}

	const FString OverridePath = GetOverrideWorkflowFilePath();
	CachedOverrideLoadStatus = LoadWorkflowFile(
		OverridePath,
		EWorkflowPresetSource::UserCreated,
		true,
		false,
		CachedOverrideWorkflows,
		CachedDiagnostics,
		&CachedOverrideFileHash);
	bCachedOverrideFileExists = IFileManager::Get().FileExists(*OverridePath);
	if (CachedOverrideLoadStatus == EWorkflowFileLoadStatus::Invalid)
	{
		CachedOverrideWorkflows.Reset();
		CachedDiagnostics.Add(TEXT("Workflow overrides are write-protected until the invalid file is repaired or restored."));
	}
	if (HasPendingLegacyWorkflowImport())
	{
		CachedDiagnostics.Add(TEXT("Legacy personal workflows are waiting for explicit import into the project-shared override file."));
	}

	CachedEffectiveWorkflows = CachedDefaultWorkflows;
	for (FWorkflowPreset& Override : CachedOverrideWorkflows)
	{
		if (Override.Id == FName(TEXT("TADebugWorkflow_ResetDebug")))
		{
			CachedDiagnostics.Add(TEXT("The Reset Debug override was ignored because this system workflow is protected."));
			continue;
		}
		const int32 DefaultIndex = CachedDefaultWorkflows.IndexOfByPredicate([&Override](const FWorkflowPreset& DefaultWorkflow)
		{
			return DefaultWorkflow.Id == Override.Id;
		});
		if (DefaultIndex != INDEX_NONE)
		{
			Override.Source = EWorkflowPresetSource::Modified;
			CachedEffectiveWorkflows[DefaultIndex] = Override;
		}
		else
		{
			Override.Source = EWorkflowPresetSource::UserCreated;
			CachedEffectiveWorkflows.Add(Override);
		}
	}
}
}

const TArray<FWorkflowPreset>& GetEffectiveWorkflowPresets()
{
	RebuildWorkflowCache();
	return CachedEffectiveWorkflows;
}

const TArray<FWorkflowPreset>& GetDefaultWorkflowPresets()
{
	RebuildWorkflowCache();
	return CachedDefaultWorkflows;
}

const FWorkflowPreset* FindEffectiveWorkflowPreset(FName WorkflowId)
{
	return GetEffectiveWorkflowPresets().FindByPredicate([WorkflowId](const FWorkflowPreset& Workflow)
	{
		return Workflow.Id == WorkflowId;
	});
}

const FWorkflowPreset* FindDefaultWorkflowPreset(FName WorkflowId)
{
	return GetDefaultWorkflowPresets().FindByPredicate([WorkflowId](const FWorkflowPreset& Workflow)
	{
		return Workflow.Id == WorkflowId;
	});
}

bool SaveWorkflowOverride(
	FName WorkflowId,
	const FString& Label,
	const FString& Tooltip,
	FName IconName,
	const TArray<FTADebugViewCustomAction>& ActivateActions,
	const TArray<FTADebugViewCustomAction>& DeactivateActions,
	FName& OutWorkflowId,
	FString& OutError)
{
	RebuildWorkflowCache();
	if (WorkflowId == FName(TEXT("TADebugWorkflow_ResetDebug")))
	{
		OutError = TEXT("Reset Debug is a protected system workflow and cannot be modified.");
		return false;
	}
	if (Label.TrimStartAndEnd().IsEmpty() || ActivateActions.IsEmpty())
	{
		OutError = TEXT("Workflow name and at least one activate action are required.");
		return false;
	}

	for (const FTADebugViewCustomAction& Action : ActivateActions)
	{
		if (!IsWorkflowActionRuntimeValid(Action))
		{
			OutError = FString::Printf(TEXT("Invalid activate action: %s"), *Action.Value);
			return false;
		}
	}
	for (const FTADebugViewCustomAction& Action : DeactivateActions)
	{
		if (!IsWorkflowActionRuntimeValid(Action))
		{
			OutError = FString::Printf(TEXT("Invalid restore action: %s"), *Action.Value);
			return false;
		}
	}

	if (WorkflowId.IsNone())
	{
		do
		{
			WorkflowId = FName(*FString::Printf(TEXT("TADebugCustom_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12)));
		}
		while (FindEffectiveWorkflowPreset(WorkflowId) != nullptr);
	}

	TArray<FDebugViewAction> RuntimeActivate;
	TArray<FDebugViewAction> RuntimeDeactivate;
	for (const FTADebugViewCustomAction& Action : ActivateActions)
	{
		FDebugViewAction RuntimeAction;
		ConvertSavedActionToRuntimeAction(Action, RuntimeAction);
		RuntimeActivate.Add(MoveTemp(RuntimeAction));
	}
	for (const FTADebugViewCustomAction& Action : DeactivateActions)
	{
		FDebugViewAction RuntimeAction;
		ConvertSavedActionToRuntimeAction(Action, RuntimeAction);
		RuntimeDeactivate.Add(MoveTemp(RuntimeAction));
	}

	const bool bIsDefaultOverride = FindDefaultWorkflowPreset(WorkflowId) != nullptr;
	FWorkflowPreset SavedWorkflow(
		*WorkflowId.ToString(),
		FText::FromString(Label.TrimStartAndEnd()),
		FText::FromString(Tooltip.TrimStartAndEnd()),
		IconName.IsNone() ? FName(TEXT("Icons.Settings")) : IconName,
		MoveTemp(RuntimeActivate),
		MoveTemp(RuntimeDeactivate),
		bIsDefaultOverride ? EWorkflowPresetSource::Modified : EWorkflowPresetSource::UserCreated);

	TArray<FWorkflowPreset> CandidateOverrides = CachedOverrideWorkflows;
	const int32 ExistingIndex = CandidateOverrides.IndexOfByPredicate([WorkflowId](const FWorkflowPreset& Existing)
	{
		return Existing.Id == WorkflowId;
	});
	if (ExistingIndex == INDEX_NONE)
	{
		CandidateOverrides.Add(MoveTemp(SavedWorkflow));
	}
	else
	{
		CandidateOverrides[ExistingIndex] = MoveTemp(SavedWorkflow);
	}

	if (!SaveOverrideWorkflows(CandidateOverrides, OutError))
	{
		bWorkflowCacheDirty = true;
		return false;
	}
	OutWorkflowId = WorkflowId;
	InvalidateEffectiveWorkflowPresetCache();
	return true;
}

bool ResetWorkflowToDefault(FName WorkflowId, FString& OutError)
{
	RebuildWorkflowCache();
	if (!FindDefaultWorkflowPreset(WorkflowId))
	{
		OutError = TEXT("Only a modified default workflow can be reset.");
		return false;
	}
	TArray<FWorkflowPreset> CandidateOverrides = CachedOverrideWorkflows;
	const int32 RemovedCount = CandidateOverrides.RemoveAll([WorkflowId](const FWorkflowPreset& Override)
	{
		return Override.Id == WorkflowId;
	});
	if (RemovedCount == 0)
	{
		OutError = TEXT("The default workflow has no project override to reset.");
		return false;
	}
	if (!SaveOverrideWorkflows(CandidateOverrides, OutError))
	{
		bWorkflowCacheDirty = true;
		return false;
	}
	InvalidateEffectiveWorkflowPresetCache();
	return true;
}

bool DeleteUserWorkflow(FName WorkflowId, FString& OutError)
{
	RebuildWorkflowCache();
	if (FindDefaultWorkflowPreset(WorkflowId))
	{
		OutError = TEXT("Default workflows cannot be deleted.");
		return false;
	}
	TArray<FWorkflowPreset> CandidateOverrides = CachedOverrideWorkflows;
	const int32 RemovedCount = CandidateOverrides.RemoveAll([WorkflowId](const FWorkflowPreset& Override)
	{
		return Override.Id == WorkflowId;
	});
	if (RemovedCount == 0)
	{
		OutError = TEXT("Workflow was not found.");
		return false;
	}
	if (!SaveOverrideWorkflows(CandidateOverrides, OutError))
	{
		bWorkflowCacheDirty = true;
		return false;
	}
	InvalidateEffectiveWorkflowPresetCache();
	return true;
}

bool AreWorkflowOverridesWriteProtected()
{
	RebuildWorkflowCache();
	return CachedOverrideLoadStatus == EWorkflowFileLoadStatus::Invalid;
}

bool HasPendingLegacyWorkflowImport()
{
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	return Settings
		&& !Settings->bHasMigratedWorkflowOverridesV2
		&& !Settings->CustomWorkflowPresets.IsEmpty();
}

bool ImportLegacyWorkflows(FString& OutError)
{
	RebuildWorkflowCache();
	UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable();
	if (!Settings)
	{
		OutError = TEXT("Could not load legacy workflow settings.");
		return false;
	}
	if (!HasPendingLegacyWorkflowImport())
	{
		return true;
	}
	if (!EnsureOverrideSnapshotIsWritable(OutError))
	{
		return false;
	}

	TArray<FWorkflowPreset> CandidateOverrides = CachedOverrideWorkflows;
	for (const FTADebugViewCustomWorkflowPreset& LegacyPreset : Settings->CustomWorkflowPresets)
	{
		FWorkflowPreset MigratedWorkflow(
			TEXT("Invalid"),
			FText::GetEmpty(),
			FText::GetEmpty(),
			TEXT("Icons.Settings"),
			TArray<FDebugViewAction>(),
			TArray<FDebugViewAction>(),
			EWorkflowPresetSource::UserCreated);
		if (!BuildLegacyWorkflow(LegacyPreset, MigratedWorkflow, OutError))
		{
			return false;
		}

		const FWorkflowPreset* Existing = CandidateOverrides.FindByPredicate([&MigratedWorkflow](const FWorkflowPreset& Workflow)
		{
			return Workflow.Id == MigratedWorkflow.Id;
		});
		if (Existing)
		{
			if (!AreWorkflowsEquivalent(*Existing, MigratedWorkflow))
			{
				OutError = FString::Printf(
					TEXT("Legacy workflow %s conflicts with a different project workflow using the same Id."),
					*MigratedWorkflow.Id.ToString());
				return false;
			}
			continue;
		}
		CandidateOverrides.Add(MoveTemp(MigratedWorkflow));
	}

	if (!SaveOverrideWorkflows(CandidateOverrides, OutError))
	{
		return false;
	}
	Settings->bHasMigratedWorkflowOverridesV2 = true;
	Settings->SaveUserSettings();
	InvalidateEffectiveWorkflowPresetCache();
	return true;
}

bool ConvertRuntimeActionToSavedAction(const FDebugViewAction& RuntimeAction, FTADebugViewCustomAction& OutSavedAction)
{
	switch (RuntimeAction.ActionType)
	{
	case EPresetActionType::ViewMode:
		OutSavedAction = FTADebugViewCustomAction(ETADebugViewCustomActionType::ViewMode, GetViewModeConfigValue(RuntimeAction.ViewModeIndex));
		break;
	case EPresetActionType::NaniteVisualization:
		OutSavedAction = FTADebugViewCustomAction(ETADebugViewCustomActionType::NaniteVisualization, RuntimeAction.VisualizationMode.ToString());
		break;
	case EPresetActionType::LumenVisualization:
		OutSavedAction = FTADebugViewCustomAction(ETADebugViewCustomActionType::LumenVisualization, RuntimeAction.VisualizationMode.ToString());
		break;
	case EPresetActionType::VirtualShadowMapVisualization:
		OutSavedAction = FTADebugViewCustomAction(ETADebugViewCustomActionType::VirtualShadowMapVisualization, RuntimeAction.VisualizationMode.ToString());
		break;
	case EPresetActionType::Command:
		OutSavedAction = FTADebugViewCustomAction(ETADebugViewCustomActionType::Command, RuntimeAction.Commands);
		break;
	default:
		return false;
	}
	return !OutSavedAction.Value.IsEmpty();
}

bool ConvertSavedActionToRuntimeAction(const FTADebugViewCustomAction& SavedAction, FDebugViewAction& OutRuntimeAction)
{
	const FString Value = SavedAction.Value.TrimStartAndEnd();
	if (Value.IsEmpty())
	{
		return false;
	}
	switch (SavedAction.ActionType)
	{
	case ETADebugViewCustomActionType::ViewMode:
	{
		const TOptional<EViewModeIndex> ViewMode = GetViewModeFromConfigValue(Value);
		if (!ViewMode.IsSet())
		{
			return false;
		}
		OutRuntimeAction = FDebugViewAction::ViewMode(*ViewMode);
		return true;
	}
	case ETADebugViewCustomActionType::NaniteVisualization:
		if (!IsVisualizationModeSupported(SavedAction.ActionType, FName(*Value)))
		{
			return false;
		}
		OutRuntimeAction = FDebugViewAction::Nanite(*Value);
		return true;
	case ETADebugViewCustomActionType::LumenVisualization:
		if (!IsVisualizationModeSupported(SavedAction.ActionType, FName(*Value)))
		{
			return false;
		}
		OutRuntimeAction = FDebugViewAction::Lumen(*Value);
		return true;
	case ETADebugViewCustomActionType::VirtualShadowMapVisualization:
		if (!IsVisualizationModeSupported(SavedAction.ActionType, FName(*Value)))
		{
			return false;
		}
		OutRuntimeAction = FDebugViewAction::VirtualShadowMap(*Value);
		return true;
	case ETADebugViewCustomActionType::Command:
		if (!IsCommandValueSupported(Value))
		{
			return false;
		}
		OutRuntimeAction = FDebugViewAction::Command(*Value);
		return true;
	default:
		return false;
	}
}

bool IsWorkflowActionRuntimeValid(const FTADebugViewCustomAction& Action)
{
	FDebugViewAction RuntimeAction;
	return ConvertSavedActionToRuntimeAction(Action, RuntimeAction);
}

FText GetWorkflowSourceLabel(EWorkflowPresetSource Source)
{
	switch (Source)
	{
	case EWorkflowPresetSource::Modified: return LOCTEXT("WorkflowSourceModified", "Modified");
	case EWorkflowPresetSource::UserCreated: return LOCTEXT("WorkflowSourceUserCreated", "Project Created");
	default: return LOCTEXT("WorkflowSourceDefault", "Default");
	}
}

void InvalidateEffectiveWorkflowPresetCache()
{
	bWorkflowCacheDirty = true;
}

const TArray<FString>& GetWorkflowRegistryDiagnostics()
{
	RebuildWorkflowCache();
	return CachedDiagnostics;
}

#if WITH_DEV_AUTOMATION_TESTS
bool LoadWorkflowFileForTesting(
	const FString& FilePath,
	bool bRequireAtLeastOneWorkflow,
	TArray<FWorkflowPreset>& OutWorkflows,
	TArray<FString>& OutDiagnostics)
{
	return LoadWorkflowFile(
		FilePath,
		EWorkflowPresetSource::UserCreated,
		false,
		bRequireAtLeastOneWorkflow,
		OutWorkflows,
		OutDiagnostics) == EWorkflowFileLoadStatus::Valid;
}
#endif
}

#undef LOCTEXT_NAMESPACE
