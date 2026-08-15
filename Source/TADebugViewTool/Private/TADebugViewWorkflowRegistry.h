#pragma once

#include "CoreMinimal.h"
#include "TADebugViewCustomPresetSettings.h"
#include "TADebugViewPresetTypes.h"

namespace TADebugViewTool
{
const TArray<FWorkflowPreset>& GetEffectiveWorkflowPresets();
const TArray<FWorkflowPreset>& GetDefaultWorkflowPresets();
const FWorkflowPreset* FindEffectiveWorkflowPreset(FName WorkflowId);
const FWorkflowPreset* FindDefaultWorkflowPreset(FName WorkflowId);

bool SaveWorkflowOverride(
	FName WorkflowId,
	const FString& Label,
	const FString& Tooltip,
	FName IconName,
	const TArray<FTADebugViewCustomAction>& ActivateActions,
	const TArray<FTADebugViewCustomAction>& DeactivateActions,
	FName& OutWorkflowId,
	FString& OutError);

bool ResetWorkflowToDefault(FName WorkflowId, FString& OutError);
bool DeleteUserWorkflow(FName WorkflowId, FString& OutError);

bool AreWorkflowOverridesWriteProtected();
bool HasPendingLegacyWorkflowImport();
bool ImportLegacyWorkflows(FString& OutError);

bool ConvertRuntimeActionToSavedAction(const FDebugViewAction& RuntimeAction, FTADebugViewCustomAction& OutSavedAction);
bool ConvertSavedActionToRuntimeAction(const FTADebugViewCustomAction& SavedAction, FDebugViewAction& OutRuntimeAction);
bool IsWorkflowActionRuntimeValid(const FTADebugViewCustomAction& Action);

FText GetWorkflowSourceLabel(EWorkflowPresetSource Source);
void InvalidateEffectiveWorkflowPresetCache();
const TArray<FString>& GetWorkflowRegistryDiagnostics();

#if WITH_DEV_AUTOMATION_TESTS
bool LoadWorkflowFileForTesting(
	const FString& FilePath,
	bool bRequireAtLeastOneWorkflow,
	TArray<FWorkflowPreset>& OutWorkflows,
	TArray<FString>& OutDiagnostics);
#endif
}
