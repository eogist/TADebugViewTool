#include "TADebugViewExecutor.h"

#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
// LevelEditorViewport.h declares an override of EditorViewportClient's UE_DEPRECATED(5.4, ...)
// DropObjectsAtCoordinates overload without marking the override itself deprecated, so merely
// including this engine header emits C4996 regardless of whether this plugin calls that API.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "LevelEditorViewport.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#include "ShowFlags.h"
#include "TADebugViewPresetRegistry.h"
#include "TADebugViewWorkflowRegistry.h"

namespace
{
constexpr TCHAR ShowFlagCommandPrefix[] = TEXT("showflag.");

int32 FindShowFlagIndexCaseInsensitive(const FString& FlagName)
{
	const int32 ExactIndex = FEngineShowFlags::FindIndexByName(*FlagName);
	if (ExactIndex != INDEX_NONE)
	{
		return ExactIndex;
	}

	struct FShowFlagFinder
	{
		explicit FShowFlagFinder(const FString& InFlagName)
			: FlagName(InFlagName)
		{
		}

		bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
		{
			if (InName.Equals(FlagName, ESearchCase::IgnoreCase))
			{
				Index = static_cast<int32>(InIndex);
				return false;
			}

			return true;
		}

		bool OnCustomShowFlag(uint32 InIndex, const FString& InName)
		{
			return OnEngineShowFlag(InIndex, InName);
		}

		FString FlagName;
		int32 Index = INDEX_NONE;
	};

	FShowFlagFinder Finder(FlagName);
	FEngineShowFlags::IterateAllFlags(Finder);
	return Finder.Index;
}

FText GetVisualizationStatusForViewport(const FLevelEditorViewportClient& ViewportClient)
{
	for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
	{
		for (const TADebugViewTool::FDebugViewPreset& Preset : Group.Presets)
		{
			if (Preset.ActionType == TADebugViewTool::EPresetActionType::NaniteVisualization && ViewportClient.IsNaniteVisualizationModeSelected(Preset.VisualizationMode))
			{
				return FText::FromString(FString::Printf(TEXT("Nanite: %s"), *Preset.Label.ToString()));
			}

			if (Preset.ActionType == TADebugViewTool::EPresetActionType::LumenVisualization && ViewportClient.IsLumenVisualizationModeSelected(Preset.VisualizationMode))
			{
				return FText::FromString(FString::Printf(TEXT("Lumen: %s"), *Preset.Label.ToString()));
			}

			if (Preset.ActionType == TADebugViewTool::EPresetActionType::VirtualShadowMapVisualization && ViewportClient.IsVirtualShadowMapVisualizationModeSelected(Preset.VisualizationMode))
			{
				return FText::FromString(FString::Printf(TEXT("VSM: %s"), *Preset.Label.ToString()));
			}
		}
	}

	return FText::GetEmpty();
}
}

namespace TADebugViewTool
{
void FTADebugViewExecutor::ExecutePreset(FDebugViewPreset Preset) const
{
	ExecuteDebugViewAction(FDebugViewAction::FromPreset(Preset));
}

void FTADebugViewExecutor::ExecutePresetFromPanel(FDebugViewPreset Preset)
{
	const bool bHadActiveWorkflow = !ActiveWorkflowPresetId.IsNone();
	if (bHadActiveWorkflow)
	{
		DeactivateActiveWorkflow();
	}

	if (!bHadActiveWorkflow && Preset.ActionType != EPresetActionType::Command && IsPresetActive(Preset))
	{
		const TArray<FLevelEditorViewportClient*> ViewportClients = GetTargetLevelViewportClients();
		SetEditorViewMode(VMI_Lit, ViewportClients);
		return;
	}

	ExecutePreset(Preset);
}

void FTADebugViewExecutor::ExecuteWorkflowPreset(const FWorkflowPreset& WorkflowPreset)
{
	const bool bWasActive = ActiveWorkflowPresetId == WorkflowPreset.Id;
	if (bWasActive)
	{
		DeactivateActiveWorkflow();
		return;
	}

	if (!ActiveWorkflowPresetId.IsNone())
	{
		DeactivateActiveWorkflow();
	}

	if (!WorkflowPreset.DeactivateActions.IsEmpty())
	{
		CaptureWorkflowViewportState(WorkflowPreset);
	}

	for (const FDebugViewAction& Action : WorkflowPreset.ActivateActions)
	{
		ExecuteDebugViewAction(Action);
	}

	if (WorkflowPreset.DeactivateActions.IsEmpty())
	{
		ActiveWorkflowPresetId = NAME_None;
		ActiveWorkflowDeactivateActions.Reset();
		ActiveWorkflowViewportSnapshots.Reset();
		ActiveWorkflowConsoleVariableSnapshots.Reset();
	}
	else
	{
		ActiveWorkflowPresetId = WorkflowPreset.Id;
		ActiveWorkflowDeactivateActions = WorkflowPreset.DeactivateActions;
	}
}

void FTADebugViewExecutor::SynchronizeFromCurrentViewportState()
{
	if (!ActiveWorkflowPresetId.IsNone())
	{
		return;
	}

	const TArray<FLevelEditorViewportClient*> ViewportClients = GetTargetLevelViewportClients();
	if (ViewportClients.IsEmpty())
	{
		return;
	}

	const FWorkflowPreset* BestMatch = nullptr;
	int32 BestMatchScore = 0;
	for (const FWorkflowPreset& WorkflowPreset : GetEffectiveWorkflowPresets())
	{
		// One-shot actions such as Reset Debug do not represent a persistent
		// workflow state and must never be adopted on startup.
		if (WorkflowPreset.DeactivateActions.IsEmpty())
		{
			continue;
		}

		int32 MatchScore = 0;
		if (DoesWorkflowMatchCurrentState(WorkflowPreset, ViewportClients, MatchScore)
			&& MatchScore > BestMatchScore)
		{
			BestMatch = &WorkflowPreset;
			BestMatchScore = MatchScore;
		}
	}

	if (!BestMatch)
	{
		return;
	}

	ActiveWorkflowPresetId = BestMatch->Id;
	ActiveWorkflowDeactivateActions = BestMatch->DeactivateActions;

	// The previous editor process owned the original pre-workflow snapshot, so
	// it cannot be restored after a restart. Keeping these arrays empty tells
	// DeactivateActiveWorkflow to use the workflow's explicit restore actions.
	ActiveWorkflowViewportSnapshots.Reset();
	ActiveWorkflowConsoleVariableSnapshots.Reset();
}

void FTADebugViewExecutor::SetViewportTarget(EDebugViewportTarget InViewportTarget)
{
	if (ViewportTarget == InViewportTarget)
	{
		return;
	}

	if (!ActiveWorkflowPresetId.IsNone())
	{
		DeactivateActiveWorkflow();
	}

	ViewportTarget = InViewportTarget;
}

EDebugViewportTarget FTADebugViewExecutor::GetViewportTarget() const
{
	return ViewportTarget;
}

FText FTADebugViewExecutor::GetViewportTargetLabel() const
{
	if (ViewportTarget == EDebugViewportTarget::AllPerspective)
	{
		return FText::FromString(TEXT("All Perspective"));
	}

	if (ViewportTarget == EDebugViewportTarget::AllLevel)
	{
		return FText::FromString(TEXT("All Viewports"));
	}

	return FText::FromString(TEXT("Active / Preferred"));
}

FText FTADebugViewExecutor::GetViewportTargetStatusText() const
{
	return FText::FromString(FString::Printf(TEXT("%s (%d)"), *GetViewportTargetLabel().ToString(), GetTargetViewportCount()));
}

// Status text is returned as a bare value. The panel status bar supplies the
// "ViewMode" / "Visualization" / "Target" labels alongside it.
FText FTADebugViewExecutor::GetViewModeStatusText() const
{
	const FLevelEditorViewportClient* ViewportClient = GetStatusLevelViewportClient();
	if (!ViewportClient)
	{
		return FText::FromString(TEXT("No viewport"));
	}

	return UViewModeUtils::GetViewModeDisplayName(ViewportClient->GetViewMode());
}

FText FTADebugViewExecutor::GetVisualizationStatusText() const
{
	const FLevelEditorViewportClient* ViewportClient = GetStatusLevelViewportClient();
	if (!ViewportClient)
	{
		return FText::FromString(TEXT("None"));
	}

	const FText VisualizationText = GetVisualizationStatusForViewport(*ViewportClient);
	return VisualizationText.IsEmpty()
		? FText::FromString(TEXT("None"))
		: VisualizationText;
}

int32 FTADebugViewExecutor::GetTargetViewportCount() const
{
	return GetTargetLevelViewportClients().Num();
}

bool FTADebugViewExecutor::IsPresetActive(const FDebugViewPreset& Preset) const
{
	const TArray<FLevelEditorViewportClient*> ViewportClients = GetTargetLevelViewportClients();
	return IsPresetActiveOnViewports(Preset, ViewportClients);
}

TSet<FName> FTADebugViewExecutor::GetActiveDebugPresetIds() const
{
	TSet<FName> ActivePresetIds;
	const TArray<FLevelEditorViewportClient*> ViewportClients = GetTargetLevelViewportClients();
	if (ViewportClients.IsEmpty())
	{
		return ActivePresetIds;
	}

	for (const FDebugViewGroup& Group : GetPresetGroups())
	{
		for (const FDebugViewPreset& Preset : Group.Presets)
		{
			if (IsPresetActiveOnViewports(Preset, ViewportClients))
			{
				ActivePresetIds.Add(Preset.Id);
			}
		}
	}

	return ActivePresetIds;
}

bool FTADebugViewExecutor::IsPresetActiveOnViewports(const FDebugViewPreset& Preset, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	if (ViewportClients.IsEmpty())
	{
		return false;
	}

	for (const FLevelEditorViewportClient* ViewportClient : ViewportClients)
	{
		if (!ViewportClient)
		{
			return false;
		}

		if (Preset.ActionType == EPresetActionType::ViewMode && !ViewportClient->IsViewModeEnabled(Preset.ViewMode))
		{
			return false;
		}

		if (Preset.ActionType == EPresetActionType::NaniteVisualization && !ViewportClient->IsNaniteVisualizationModeSelected(Preset.VisualizationMode))
		{
			return false;
		}

		if (Preset.ActionType == EPresetActionType::LumenVisualization && !ViewportClient->IsLumenVisualizationModeSelected(Preset.VisualizationMode))
		{
			return false;
		}

		if (Preset.ActionType == EPresetActionType::VirtualShadowMapVisualization && !ViewportClient->IsVirtualShadowMapVisualizationModeSelected(Preset.VisualizationMode))
		{
			return false;
		}

		if (Preset.ActionType == EPresetActionType::Command)
		{
			return false;
		}
	}

	return true;
}

bool FTADebugViewExecutor::DoesWorkflowMatchCurrentState(
	const FWorkflowPreset& WorkflowPreset,
	const TArray<FLevelEditorViewportClient*>& ViewportClients,
	int32& OutMatchScore) const
{
	OutMatchScore = 0;
	for (const FDebugViewAction& Action : WorkflowPreset.ActivateActions)
	{
		if (!DoesActionMatchCurrentState(Action, ViewportClients, OutMatchScore))
		{
			return false;
		}
	}

	// At least one queryable state is required. This prevents workflows made
	// entirely of fire-and-forget commands from being reported as active.
	return OutMatchScore > 0;
}

bool FTADebugViewExecutor::DoesActionMatchCurrentState(
	const FDebugViewAction& Action,
	const TArray<FLevelEditorViewportClient*>& ViewportClients,
	int32& InOutMatchScore) const
{
	if (ViewportClients.IsEmpty())
	{
		return false;
	}

	if (Action.ActionType == EPresetActionType::Command)
	{
		return DoesCommandMatchCurrentState(Action.Commands, ViewportClients, InOutMatchScore);
	}

	++InOutMatchScore;
	for (const FLevelEditorViewportClient* ViewportClient : ViewportClients)
	{
		if (!ViewportClient)
		{
			return false;
		}

		if (Action.ActionType == EPresetActionType::ViewMode
			&& !ViewportClient->IsViewModeEnabled(Action.ViewModeIndex))
		{
			return false;
		}
		if (Action.ActionType == EPresetActionType::NaniteVisualization
			&& !ViewportClient->IsNaniteVisualizationModeSelected(Action.VisualizationMode))
		{
			return false;
		}
		if (Action.ActionType == EPresetActionType::LumenVisualization
			&& !ViewportClient->IsLumenVisualizationModeSelected(Action.VisualizationMode))
		{
			return false;
		}
		if (Action.ActionType == EPresetActionType::VirtualShadowMapVisualization
			&& !ViewportClient->IsVirtualShadowMapVisualizationModeSelected(Action.VisualizationMode))
		{
			return false;
		}
	}

	return true;
}

bool FTADebugViewExecutor::DoesCommandMatchCurrentState(
	const FString& Commands,
	const TArray<FLevelEditorViewportClient*>& ViewportClients,
	int32& InOutMatchScore) const
{
	TArray<FString> ParsedCommands;
	Commands.ParseIntoArray(ParsedCommands, TEXT(";"), true);
	for (FString RawCommand : ParsedCommands)
	{
		RawCommand = RawCommand.TrimStartAndEnd();
		if (RawCommand.IsEmpty())
		{
			continue;
		}

		if (RawCommand.StartsWith(TEXT("stat "), ESearchCase::IgnoreCase))
		{
			const FString StatName = RawCommand.RightChop(5).TrimStartAndEnd();
			// "stat none" is a transition that clears previous overlays before
			// enabling the workflow's desired stats; it is not an active state.
			if (StatName.IsEmpty() || StatName.Equals(TEXT("none"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			++InOutMatchScore;
			for (const FLevelEditorViewportClient* ViewportClient : ViewportClients)
			{
				const TArray<FString>* EnabledStats = ViewportClient ? ViewportClient->GetEnabledStats() : nullptr;
				if (!EnabledStats || !EnabledStats->ContainsByPredicate([&StatName](const FString& EnabledStat)
				{
					return EnabledStat.Equals(StatName, ESearchCase::IgnoreCase);
				}))
				{
					return false;
				}
			}
			continue;
		}

		if (RawCommand.StartsWith(ShowFlagCommandPrefix, ESearchCase::IgnoreCase))
		{
			FString FlagCommand;
			FString ExpectedValue;
			if (!RawCommand.Split(TEXT(" "), &FlagCommand, &ExpectedValue)
				&& !RawCommand.Split(TEXT("="), &FlagCommand, &ExpectedValue))
			{
				// Toggle-only showflag commands have no deterministic state.
				continue;
			}

			FString FlagName;
			if (!TryExtractShowFlagName(RawCommand, FlagName))
			{
				continue;
			}

			ExpectedValue = ExpectedValue.TrimStartAndEnd();
			const bool bExpectedEnabled =
				ExpectedValue.Equals(TEXT("1"))
				|| ExpectedValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)
				|| ExpectedValue.Equals(TEXT("on"), ESearchCase::IgnoreCase);
			const bool bExpectedDisabled =
				ExpectedValue.Equals(TEXT("0"))
				|| ExpectedValue.Equals(TEXT("false"), ESearchCase::IgnoreCase)
				|| ExpectedValue.Equals(TEXT("off"), ESearchCase::IgnoreCase);
			if (!bExpectedEnabled && !bExpectedDisabled)
			{
				continue;
			}

			++InOutMatchScore;
			const int32 FlagIndex = FindShowFlagIndexCaseInsensitive(FlagName);
			if (FlagIndex == INDEX_NONE)
			{
				return false;
			}

			for (const FLevelEditorViewportClient* ViewportClient : ViewportClients)
			{
				if (!ViewportClient
					|| ViewportClient->EngineShowFlags.GetSingleFlag(static_cast<uint32>(FlagIndex)) != bExpectedEnabled)
				{
					return false;
				}
			}
			continue;
		}

		FString VariableName;
		FString ExpectedValue;
		if (!RawCommand.Split(TEXT(" "), &VariableName, &ExpectedValue)
			&& !RawCommand.Split(TEXT("="), &VariableName, &ExpectedValue))
		{
			continue;
		}

		VariableName = VariableName.TrimStartAndEnd();
		ExpectedValue = ExpectedValue.TrimStartAndEnd();
		IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*VariableName);
		if (!ConsoleVariable || ExpectedValue.IsEmpty())
		{
			continue;
		}

		++InOutMatchScore;
		const FString CurrentValue = ConsoleVariable->GetString();
		const bool bStringMatches = CurrentValue.Equals(ExpectedValue, ESearchCase::IgnoreCase);
		const bool bNumericMatches = CurrentValue.IsNumeric()
			&& ExpectedValue.IsNumeric()
			&& FMath::IsNearlyEqual(FCString::Atod(*CurrentValue), FCString::Atod(*ExpectedValue));
		if (!bStringMatches && !bNumericMatches)
		{
			return false;
		}
	}

	return true;
}

FName FTADebugViewExecutor::GetActiveWorkflowPresetId() const
{
	return ActiveWorkflowPresetId;
}

FLevelEditorViewportClient* FTADebugViewExecutor::GetActiveOrPreferredLevelViewportClient() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
	{
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient && ViewportClient->Viewport == ActiveViewport)
			{
				return ViewportClient;
			}
		}
	}

	if (GCurrentLevelEditingViewportClient)
	{
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient == GCurrentLevelEditingViewportClient)
			{
				return ViewportClient;
			}
		}
	}

	return GetFallbackLevelViewportClient();
}

FLevelEditorViewportClient* FTADebugViewExecutor::GetFallbackLevelViewportClient() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (ViewportClient && ViewportClient->IsPerspective())
		{
			return ViewportClient;
		}
	}

	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (ViewportClient)
		{
			return ViewportClient;
		}
	}

	return nullptr;
}

FLevelEditorViewportClient* FTADebugViewExecutor::GetStatusLevelViewportClient() const
{
	const TArray<FLevelEditorViewportClient*> ViewportClients = GetTargetLevelViewportClients();
	return ViewportClients.IsEmpty() ? nullptr : ViewportClients[0];
}

TArray<FLevelEditorViewportClient*> FTADebugViewExecutor::GetTargetLevelViewportClients() const
{
	TArray<FLevelEditorViewportClient*> ViewportClients;
	if (!GEditor)
	{
		return ViewportClients;
	}

	if (ViewportTarget == EDebugViewportTarget::ActivePreferred)
	{
		if (FLevelEditorViewportClient* ViewportClient = GetActiveOrPreferredLevelViewportClient())
		{
			ViewportClients.Add(ViewportClient);
		}

		return ViewportClients;
	}

	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (!ViewportClient)
		{
			continue;
		}

		if (ViewportTarget == EDebugViewportTarget::AllPerspective && !ViewportClient->IsPerspective())
		{
			continue;
		}

		ViewportClients.Add(ViewportClient);
	}

	if (ViewportClients.IsEmpty())
	{
		if (FLevelEditorViewportClient* ViewportClient = GetActiveOrPreferredLevelViewportClient())
		{
			ViewportClients.Add(ViewportClient);
		}
	}

	return ViewportClients;
}

TArray<FLevelEditorViewportClient*> FTADebugViewExecutor::GetSnapshotLevelViewportClients() const
{
	TArray<FLevelEditorViewportClient*> ViewportClients;
	if (!GEditor)
	{
		return ViewportClients;
	}

	for (const FViewportStateSnapshot& Snapshot : ActiveWorkflowViewportSnapshots)
	{
		if (!Snapshot.bIsValid)
		{
			continue;
		}

		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient == Snapshot.ViewportClient)
			{
				ViewportClients.Add(ViewportClient);
				break;
			}
		}
	}

	return ViewportClients;
}

void FTADebugViewExecutor::ApplyToLevelViewports(const TArray<FLevelEditorViewportClient*>& ViewportClients, TFunctionRef<void(FLevelEditorViewportClient&)> ApplyMode) const
{
	for (FLevelEditorViewportClient* ViewportClient : ViewportClients)
	{
		if (!ViewportClient)
		{
			continue;
		}

		ApplyMode(*ViewportClient);
		ViewportClient->Invalidate();
	}

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

void FTADebugViewExecutor::DeactivateActiveWorkflow()
{
	if (ActiveWorkflowPresetId.IsNone())
	{
		return;
	}

	const TArray<FLevelEditorViewportClient*> SnapshotViewports = GetSnapshotLevelViewportClients();
	const TArray<FLevelEditorViewportClient*> DeactivationViewports =
		SnapshotViewports.IsEmpty() ? GetTargetLevelViewportClients() : SnapshotViewports;
	for (const FDebugViewAction& Action : ActiveWorkflowDeactivateActions)
	{
		ExecuteDebugViewActionOnViewports(Action, DeactivationViewports);
	}

	RestoreWorkflowViewportState();
	ActiveWorkflowPresetId = NAME_None;
	ActiveWorkflowDeactivateActions.Reset();
	ActiveWorkflowViewportSnapshots.Reset();
	ActiveWorkflowConsoleVariableSnapshots.Reset();
}

void FTADebugViewExecutor::CaptureWorkflowViewportState(const FWorkflowPreset& WorkflowPreset)
{
	ActiveWorkflowViewportSnapshots.Reset();
	ActiveWorkflowConsoleVariableSnapshots.Reset();
	CaptureConsoleVariableStateFromActions(WorkflowPreset.ActivateActions);
	CaptureConsoleVariableStateFromActions(WorkflowPreset.DeactivateActions);

	for (FLevelEditorViewportClient* ViewportClient : GetTargetLevelViewportClients())
	{
		if (!ViewportClient)
		{
			continue;
		}

		FViewportStateSnapshot& Snapshot = ActiveWorkflowViewportSnapshots.AddDefaulted_GetRef();
		Snapshot.bIsValid = true;
		Snapshot.ViewportClient = ViewportClient;
		Snapshot.ViewMode = ViewportClient->GetViewMode();
		if (const TArray<FString>* EnabledStats = ViewportClient->GetEnabledStats())
		{
			Snapshot.EnabledStats = *EnabledStats;
		}
		CaptureVisualizationState(*ViewportClient, Snapshot);
		CaptureShowFlagStateFromActions(WorkflowPreset.ActivateActions, Snapshot, *ViewportClient);
		CaptureShowFlagStateFromActions(WorkflowPreset.DeactivateActions, Snapshot, *ViewportClient);
	}
}

void FTADebugViewExecutor::CaptureConsoleVariableStateFromActions(const TArray<FDebugViewAction>& Actions)
{
	for (const FDebugViewAction& Action : Actions)
	{
		if (Action.ActionType == EPresetActionType::Command)
		{
			CaptureConsoleVariableStateFromCommand(Action.Commands);
		}
	}
}

void FTADebugViewExecutor::CaptureConsoleVariableStateFromCommand(const FString& Commands)
{
	TArray<FString> ParsedCommands;
	Commands.ParseIntoArray(ParsedCommands, TEXT(";"), true);
	for (FString RawCommand : ParsedCommands)
	{
		RawCommand = RawCommand.TrimStartAndEnd();
		FString VariableName;
		FString UnusedValue;
		if (!RawCommand.Split(TEXT(" "), &VariableName, &UnusedValue) && !RawCommand.Split(TEXT("="), &VariableName, &UnusedValue))
		{
			continue;
		}
		VariableName = VariableName.TrimStartAndEnd();
		if (VariableName.IsEmpty() || VariableName.StartsWith(TEXT("showflag."), ESearchCase::IgnoreCase) || VariableName.Equals(TEXT("stat"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const bool bAlreadyCaptured = ActiveWorkflowConsoleVariableSnapshots.ContainsByPredicate([&VariableName](const FConsoleVariableStateSnapshot& Snapshot)
		{
			return Snapshot.Name.Equals(VariableName, ESearchCase::IgnoreCase);
		});
		if (bAlreadyCaptured)
		{
			continue;
		}

		if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*VariableName))
		{
			FConsoleVariableStateSnapshot& Snapshot = ActiveWorkflowConsoleVariableSnapshots.AddDefaulted_GetRef();
			Snapshot.Name = VariableName;
			Snapshot.Value = ConsoleVariable->GetString();
			Snapshot.SetByFlags = ConsoleVariable->GetFlags() & ECVF_SetByMask;
		}
	}
}

void FTADebugViewExecutor::CaptureVisualizationState(FLevelEditorViewportClient& ViewportClient, FViewportStateSnapshot& Snapshot) const
{
	if (ViewportClient.IsNaniteVisualizationModeSelected(ViewportClient.CurrentNaniteVisualizationMode))
	{
		Snapshot.bHasNaniteVisualizationMode = true;
		Snapshot.NaniteVisualizationMode = ViewportClient.CurrentNaniteVisualizationMode;
	}

	if (ViewportClient.IsLumenVisualizationModeSelected(ViewportClient.CurrentLumenVisualizationMode))
	{
		Snapshot.bHasLumenVisualizationMode = true;
		Snapshot.LumenVisualizationMode = ViewportClient.CurrentLumenVisualizationMode;
	}

	if (ViewportClient.IsVirtualShadowMapVisualizationModeSelected(ViewportClient.CurrentVirtualShadowMapVisualizationMode))
	{
		Snapshot.bHasVirtualShadowMapVisualizationMode = true;
		Snapshot.VirtualShadowMapVisualizationMode = ViewportClient.CurrentVirtualShadowMapVisualizationMode;
	}
}

void FTADebugViewExecutor::CaptureShowFlagStateFromActions(const TArray<FDebugViewAction>& Actions, FViewportStateSnapshot& Snapshot, FLevelEditorViewportClient& ViewportClient) const
{
	for (const FDebugViewAction& Action : Actions)
	{
		if (Action.ActionType == EPresetActionType::Command)
		{
			CaptureShowFlagStateFromCommand(Action.Commands, Snapshot, ViewportClient);
		}
	}
}

void FTADebugViewExecutor::CaptureShowFlagStateFromCommand(const FString& Commands, FViewportStateSnapshot& Snapshot, FLevelEditorViewportClient& ViewportClient) const
{
	TArray<FString> ParsedCommands;
	Commands.ParseIntoArray(ParsedCommands, TEXT(";"), true);

	for (FString& RawCommand : ParsedCommands)
	{
		FString FlagName;
		if (!TryExtractShowFlagName(RawCommand, FlagName))
		{
			continue;
		}

		const int32 FlagIndex = FindShowFlagIndexCaseInsensitive(FlagName);
		if (FlagIndex == INDEX_NONE)
		{
			continue;
		}

		const uint32 FlagIndexAsUInt = static_cast<uint32>(FlagIndex);
		const bool bAlreadyCaptured = Snapshot.ShowFlags.ContainsByPredicate([FlagIndexAsUInt](const FShowFlagStateSnapshot& ShowFlag)
		{
			return ShowFlag.FlagIndex == FlagIndexAsUInt;
		});

		if (!bAlreadyCaptured)
		{
			Snapshot.ShowFlags.Emplace(FlagIndexAsUInt, ViewportClient.EngineShowFlags.GetSingleFlag(FlagIndexAsUInt));
		}
	}
}

bool FTADebugViewExecutor::TryExtractShowFlagName(const FString& Command, FString& OutFlagName) const
{
	FString TrimmedCommand = Command.TrimStartAndEnd();
	if (!TrimmedCommand.StartsWith(ShowFlagCommandPrefix, ESearchCase::IgnoreCase))
	{
		return false;
	}

	FString Remainder = TrimmedCommand.RightChop(FCString::Strlen(ShowFlagCommandPrefix)).TrimStartAndEnd();
	if (Remainder.IsEmpty())
	{
		return false;
	}

	FString FlagName;
	FString UnusedValue;
	if (!Remainder.Split(TEXT(" "), &FlagName, &UnusedValue))
	{
		Remainder.Split(TEXT("="), &FlagName, &UnusedValue);
	}

	OutFlagName = (FlagName.IsEmpty() ? Remainder : FlagName).TrimStartAndEnd();
	return !OutFlagName.IsEmpty();
}

void FTADebugViewExecutor::RestoreWorkflowViewportState()
{
	for (const FViewportStateSnapshot& Snapshot : ActiveWorkflowViewportSnapshots)
	{
		if (!Snapshot.bIsValid || !GEditor)
		{
			continue;
		}

		FLevelEditorViewportClient* SnapshotViewportClient = nullptr;
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient == Snapshot.ViewportClient)
			{
				SnapshotViewportClient = ViewportClient;
				break;
			}
		}

		if (!SnapshotViewportClient)
		{
			continue;
		}

		SnapshotViewportClient->ChangeNaniteVisualizationMode(NAME_None);
		SnapshotViewportClient->ChangeLumenVisualizationMode(NAME_None);
		SnapshotViewportClient->ChangeVirtualShadowMapVisualizationMode(NAME_None);
		SnapshotViewportClient->SetViewMode(Snapshot.ViewMode);

		if (Snapshot.bHasNaniteVisualizationMode)
		{
			SnapshotViewportClient->ChangeNaniteVisualizationMode(Snapshot.NaniteVisualizationMode);
		}

		if (Snapshot.bHasLumenVisualizationMode)
		{
			SnapshotViewportClient->ChangeLumenVisualizationMode(Snapshot.LumenVisualizationMode);
		}

		if (Snapshot.bHasVirtualShadowMapVisualizationMode)
		{
			SnapshotViewportClient->ChangeVirtualShadowMapVisualizationMode(Snapshot.VirtualShadowMapVisualizationMode);
		}

		for (const FShowFlagStateSnapshot& ShowFlag : Snapshot.ShowFlags)
		{
			SnapshotViewportClient->EngineShowFlags.SetSingleFlag(ShowFlag.FlagIndex, ShowFlag.bWasEnabled);
		}
		SnapshotViewportClient->SetEnabledStats(Snapshot.EnabledStats);

		SnapshotViewportClient->Invalidate();
	}

	for (const FConsoleVariableStateSnapshot& Snapshot : ActiveWorkflowConsoleVariableSnapshots)
	{
		if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*Snapshot.Name))
		{
			const EConsoleVariableFlags OriginalSetBy = static_cast<EConsoleVariableFlags>(Snapshot.SetByFlags);
			if (OriginalSetBy == ECVF_SetByConsole)
			{
				ConsoleVariable->Set(*Snapshot.Value, ECVF_SetByConsole);
			}
			else
			{
				ConsoleVariable->Unset(ECVF_SetByConsole);
			}
		}
	}

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

void FTADebugViewExecutor::ExecuteDebugViewAction(const FDebugViewAction& Action) const
{
	ExecuteDebugViewActionOnViewports(Action, GetTargetLevelViewportClients());
}

void FTADebugViewExecutor::ExecuteDebugViewActionOnViewports(const FDebugViewAction& Action, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	if (Action.ActionType == EPresetActionType::ViewMode)
	{
		SetEditorViewMode(Action.ViewModeIndex, ViewportClients);
		return;
	}

	if (Action.ActionType == EPresetActionType::NaniteVisualization)
	{
		SetNaniteVisualizationMode(Action.VisualizationMode, ViewportClients);
		return;
	}

	if (Action.ActionType == EPresetActionType::LumenVisualization)
	{
		SetLumenVisualizationMode(Action.VisualizationMode, ViewportClients);
		return;
	}

	if (Action.ActionType == EPresetActionType::VirtualShadowMapVisualization)
	{
		SetVirtualShadowMapVisualizationMode(Action.VisualizationMode, ViewportClients);
		return;
	}

	ExecuteCommandList(Action.Commands, ViewportClients);
}

void FTADebugViewExecutor::SetEditorViewMode(EViewModeIndex ViewMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	ApplyToLevelViewports(ViewportClients, [ViewMode](FLevelEditorViewportClient& ViewportClient)
	{
		ViewportClient.ChangeNaniteVisualizationMode(NAME_None);
		ViewportClient.ChangeLumenVisualizationMode(NAME_None);
		ViewportClient.ChangeVirtualShadowMapVisualizationMode(NAME_None);
		ViewportClient.SetViewMode(ViewMode);
	});
}

void FTADebugViewExecutor::SetNaniteVisualizationMode(FName VisualizationMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	ApplyToLevelViewports(ViewportClients, [VisualizationMode](FLevelEditorViewportClient& ViewportClient)
	{
		ViewportClient.ChangeNaniteVisualizationMode(VisualizationMode);
	});
}

void FTADebugViewExecutor::SetLumenVisualizationMode(FName VisualizationMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	ApplyToLevelViewports(ViewportClients, [VisualizationMode](FLevelEditorViewportClient& ViewportClient)
	{
		ViewportClient.ChangeLumenVisualizationMode(VisualizationMode);
	});
}

void FTADebugViewExecutor::SetVirtualShadowMapVisualizationMode(FName VisualizationMode, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	ApplyToLevelViewports(ViewportClients, [VisualizationMode](FLevelEditorViewportClient& ViewportClient)
	{
		ViewportClient.ChangeVirtualShadowMapVisualizationMode(VisualizationMode);
	});
}

void FTADebugViewExecutor::ExecuteCommandList(FString Commands, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	TArray<FString> ParsedCommands;
	Commands.ParseIntoArray(ParsedCommands, TEXT(";"), true);

	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	for (FString& RawCommand : ParsedCommands)
	{
		const FString Command = RawCommand.TrimStartAndEnd();
		if (Command.IsEmpty())
		{
			continue;
		}

		if (TryApplyShowFlagCommand(Command, ViewportClients))
		{
			continue;
		}

		bool bHandled = false;
		if (GEditor)
		{
			bHandled = GEditor->Exec(World, *Command);
		}

		if (!bHandled && GEngine)
		{
			GEngine->Exec(World, *Command);
		}
	}

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

bool FTADebugViewExecutor::TryApplyShowFlagCommand(const FString& Command, const TArray<FLevelEditorViewportClient*>& ViewportClients) const
{
	FString FlagName;
	if (!TryExtractShowFlagName(Command, FlagName))
	{
		return false;
	}

	const int32 FlagIndex = FindShowFlagIndexCaseInsensitive(FlagName);
	if (FlagIndex == INDEX_NONE)
	{
		return false;
	}

	FString Remainder = Command.TrimStartAndEnd().RightChop(FCString::Strlen(ShowFlagCommandPrefix)).TrimStartAndEnd();
	FString ParsedFlagName;
	FString ValueString;
	if (!Remainder.Split(TEXT(" "), &ParsedFlagName, &ValueString))
	{
		Remainder.Split(TEXT("="), &ParsedFlagName, &ValueString);
	}

	ValueString = ValueString.TrimStartAndEnd();
	if (ValueString.IsEmpty())
	{
		return false;
	}

	bool bSet = false;
	if (ValueString.Equals(TEXT("1"), ESearchCase::IgnoreCase) || ValueString.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueString.Equals(TEXT("on"), ESearchCase::IgnoreCase))
	{
		bSet = true;
	}
	else if (ValueString.Equals(TEXT("0"), ESearchCase::IgnoreCase) || ValueString.Equals(TEXT("false"), ESearchCase::IgnoreCase) || ValueString.Equals(TEXT("off"), ESearchCase::IgnoreCase))
	{
		bSet = false;
	}
	else
	{
		return false;
	}

	const uint32 FlagIndexAsUInt = static_cast<uint32>(FlagIndex);
	ApplyToLevelViewports(ViewportClients, [FlagIndexAsUInt, bSet](FLevelEditorViewportClient& ViewportClient)
	{
		ViewportClient.EngineShowFlags.SetSingleFlag(FlagIndexAsUInt, bSet);
	});

	return true;
}
}
