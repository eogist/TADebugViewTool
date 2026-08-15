#pragma once

#include "CoreMinimal.h"
#include "TADebugViewPresetTypes.h"
#include "Templates/Function.h"

class FLevelEditorViewportClient;

namespace TADebugViewTool
{
enum class EDebugViewportTarget : uint8
{
	ActivePreferred,
	AllPerspective,
	AllLevel
};

class FTADebugViewExecutor
{
public:
	void ExecutePreset(FDebugViewPreset Preset) const;
	void ExecutePresetFromPanel(FDebugViewPreset Preset);
	void ExecuteWorkflowPreset(const FWorkflowPreset& WorkflowPreset);
	void ResetDebugState();
	void SynchronizeFromCurrentViewportState();

	void SetViewportTarget(EDebugViewportTarget InViewportTarget);
	EDebugViewportTarget GetViewportTarget() const;
	FText GetViewportTargetLabel() const;
	FText GetViewportTargetStatusText() const;
	FText GetViewModeStatusText() const;
	FText GetVisualizationStatusText() const;
	int32 GetTargetViewportCount() const;
	bool IsPresetActive(const FDebugViewPreset& Preset) const;
	TSet<FName> GetActiveDebugPresetIds() const;
	FName GetActiveWorkflowPresetId() const;

private:
	struct FShowFlagStateSnapshot
	{
		FShowFlagStateSnapshot() = default;
		FShowFlagStateSnapshot(uint32 InFlagIndex, bool bInWasEnabled)
			: FlagIndex(InFlagIndex)
			, bWasEnabled(bInWasEnabled)
		{
		}

		uint32 FlagIndex = 0;
		bool bWasEnabled = false;
	};

	struct FViewportStateSnapshot
	{
		void Reset()
		{
			bIsValid = false;
			ViewportClient = nullptr;
			ViewMode = VMI_Lit;
			bHasNaniteVisualizationMode = false;
			NaniteVisualizationMode = NAME_None;
			bHasLumenVisualizationMode = false;
			LumenVisualizationMode = NAME_None;
			bHasVirtualShadowMapVisualizationMode = false;
			VirtualShadowMapVisualizationMode = NAME_None;
			ShowFlags.Reset();
			EnabledStats.Reset();
		}

		bool bIsValid = false;
		FLevelEditorViewportClient* ViewportClient = nullptr;
		EViewModeIndex ViewMode = VMI_Lit;
		bool bHasNaniteVisualizationMode = false;
		FName NaniteVisualizationMode;
		bool bHasLumenVisualizationMode = false;
		FName LumenVisualizationMode;
		bool bHasVirtualShadowMapVisualizationMode = false;
		FName VirtualShadowMapVisualizationMode;
		TArray<FShowFlagStateSnapshot> ShowFlags;
		TArray<FString> EnabledStats;
	};

	struct FConsoleVariableStateSnapshot
	{
		FString Name;
		FString Value;
		uint32 SetByFlags = 0;
	};

	FLevelEditorViewportClient* GetActiveOrPreferredLevelViewportClient() const;
	FLevelEditorViewportClient* GetFallbackLevelViewportClient() const;
	FLevelEditorViewportClient* GetStatusLevelViewportClient() const;
	TArray<FLevelEditorViewportClient*> GetTargetLevelViewportClients() const;
	TArray<FLevelEditorViewportClient*> GetSnapshotLevelViewportClients() const;
	bool IsPresetActiveOnViewports(const FDebugViewPreset& Preset, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	bool DoesWorkflowMatchCurrentState(
		const FWorkflowPreset& WorkflowPreset,
		const TArray<FLevelEditorViewportClient*>& ViewportClients,
		int32& OutMatchScore) const;
	bool DoesActionMatchCurrentState(
		const FDebugViewAction& Action,
		const TArray<FLevelEditorViewportClient*>& ViewportClients,
		int32& InOutMatchScore) const;
	bool DoesCommandMatchCurrentState(
		const FString& Commands,
		const TArray<FLevelEditorViewportClient*>& ViewportClients,
		int32& InOutMatchScore) const;
	void ApplyToLevelViewports(const TArray<FLevelEditorViewportClient*>& ViewportClients, TFunctionRef<void(FLevelEditorViewportClient&)> ApplyMode) const;
	void ExecuteDebugViewAction(const FDebugViewAction& Action) const;
	void ExecuteDebugViewActionOnViewports(const FDebugViewAction& Action, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	void DeactivateActiveWorkflow();
	void CaptureWorkflowViewportState(const FWorkflowPreset& WorkflowPreset);
	void CaptureVisualizationState(FLevelEditorViewportClient& ViewportClient, FViewportStateSnapshot& Snapshot) const;
	void CaptureShowFlagStateFromActions(const TArray<FDebugViewAction>& Actions, FViewportStateSnapshot& Snapshot, FLevelEditorViewportClient& ViewportClient) const;
	void CaptureShowFlagStateFromCommand(const FString& Commands, FViewportStateSnapshot& Snapshot, FLevelEditorViewportClient& ViewportClient) const;
	void CaptureConsoleVariableStateFromActions(const TArray<FDebugViewAction>& Actions);
	void CaptureConsoleVariableStateFromCommand(const FString& Commands);
	bool TryExtractShowFlagName(const FString& Command, FString& OutFlagName) const;
	void RestoreWorkflowViewportState();
	void SetEditorViewMode(EViewModeIndex ViewMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	void SetNaniteVisualizationMode(FName VisualizationMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	void SetLumenVisualizationMode(FName VisualizationMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	void SetVirtualShadowMapVisualizationMode(FName VisualizationMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	void ExecuteCommandList(FString Commands, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;
	bool TryApplyShowFlagCommand(const FString& Command, const TArray<FLevelEditorViewportClient*>& ViewportClients) const;

	EDebugViewportTarget ViewportTarget = EDebugViewportTarget::ActivePreferred;
	FName ActiveWorkflowPresetId;
	TArray<FDebugViewAction> ActiveWorkflowDeactivateActions;
	TArray<FViewportStateSnapshot> ActiveWorkflowViewportSnapshots;
	TArray<FConsoleVariableStateSnapshot> ActiveWorkflowConsoleVariableSnapshots;
};
}
