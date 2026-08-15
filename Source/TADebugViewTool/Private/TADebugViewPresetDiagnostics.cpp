#include "TADebugViewPresetDiagnostics.h"

#include "TADebugViewCustomPresetSettings.h"
#include "TADebugViewPresetRegistry.h"
#include "TADebugViewQuickActionRuntime.h"
#include "TADebugViewWorkflowRegistry.h"

#define LOCTEXT_NAMESPACE "TADebugViewPresetDiagnostics"

namespace TADebugViewTool
{
namespace
{
void AddIssue(TArray<FPresetDiagnosticIssue>& Issues, EPresetDiagnosticSeverity Severity, const FString& PresetLabel, const FString& Message)
{
	FPresetDiagnosticIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.Severity = Severity;
	Issue.PresetLabel = PresetLabel;
	Issue.Message = Message;
}
}

TArray<FPresetDiagnosticIssue> RunPresetDiagnostics()
{
	TArray<FPresetDiagnosticIssue> Issues;
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		AddIssue(Issues, EPresetDiagnosticSeverity::Error, TEXT("Settings"), TEXT("Could not load preset settings."));
		return Issues;
	}

	const EPresetDiagnosticSeverity RegistrySeverity = AreWorkflowOverridesWriteProtected()
		? EPresetDiagnosticSeverity::Error
		: EPresetDiagnosticSeverity::Warning;
	for (const FString& RegistryDiagnostic : GetWorkflowRegistryDiagnostics())
	{
		AddIssue(Issues, RegistrySeverity, TEXT("Workflow Registry"), RegistryDiagnostic);
	}

	TSet<FName> SeenIds;
	for (const FWorkflowPreset& WorkflowPreset : GetEffectiveWorkflowPresets())
	{
		const FString Id = WorkflowPreset.Id.ToString();
		const FString Label = WorkflowPreset.Label.IsEmpty() ? Id : WorkflowPreset.Label.ToString();
		if (WorkflowPreset.Id.IsNone())
		{
			AddIssue(Issues, EPresetDiagnosticSeverity::Error, Label, TEXT("Workflow has no stable Id."));
		}
		else if (SeenIds.Contains(WorkflowPreset.Id))
		{
			AddIssue(Issues, EPresetDiagnosticSeverity::Error, Label, TEXT("Workflow Id is duplicated."));
		}
		else
		{
			SeenIds.Add(WorkflowPreset.Id);
		}
		if (WorkflowPreset.Label.IsEmpty())
		{
			AddIssue(Issues, EPresetDiagnosticSeverity::Error, Label, TEXT("Workflow name is empty."));
		}
		if (WorkflowPreset.ActivateActions.IsEmpty())
		{
			AddIssue(Issues, EPresetDiagnosticSeverity::Error, Label, TEXT("Workflow has no activation actions."));
		}

		const auto ValidateRuntimeActions = [&Issues, &Label](const TArray<FDebugViewAction>& Actions, const TCHAR* CollectionName)
		{
			for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
			{
				FTADebugViewCustomAction SavedAction;
				if (!ConvertRuntimeActionToSavedAction(Actions[ActionIndex], SavedAction)
					|| !IsWorkflowActionRuntimeValid(SavedAction))
				{
					AddIssue(
						Issues,
						EPresetDiagnosticSeverity::Error,
						Label,
						FString::Printf(TEXT("%s action %d has an unsupported value."), CollectionName, ActionIndex + 1));
				}
			}
		};
		ValidateRuntimeActions(WorkflowPreset.ActivateActions, TEXT("Activate"));
		ValidateRuntimeActions(WorkflowPreset.DeactivateActions, TEXT("Restore"));
	}

	auto ValidateQuickActions = [&Issues](const TArray<FTADebugViewQuickAction>& Actions, const TCHAR* CollectionName)
	{
		for (const FTADebugViewQuickAction& Action : Actions)
		{
			TOptional<FDebugViewPreset> DebugPreset;
			TOptional<FWorkflowPreset> WorkflowPreset;
			if (!ResolveQuickAction(Action, DebugPreset, WorkflowPreset))
			{
				AddIssue(Issues, EPresetDiagnosticSeverity::Warning, CollectionName, FString::Printf(TEXT("Stale reference: %s"), *Action.Id));
			}
		}
	};
	ValidateQuickActions(Settings->FavoriteActions, TEXT("Favorites"));
	return Issues;
}

TArray<FPresetDiagnosticCheck> RunPresetDiagnosticChecks()
{
	TArray<FPresetDiagnosticCheck> Checks;
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();

	auto AddCheck = [&Checks](const FText& Label, const FText& Detail, bool bPassed, EPresetDiagnosticSeverity FailureSeverity)
	{
		FPresetDiagnosticCheck& Check = Checks.AddDefaulted_GetRef();
		Check.Label = Label;
		Check.Detail = Detail;
		Check.bPassed = bPassed;
		Check.Severity = bPassed ? EPresetDiagnosticSeverity::Info : FailureSeverity;
	};

	if (!Settings)
	{
		AddCheck(
			LOCTEXT("CheckSettings", "Preset Settings"),
			LOCTEXT("CheckSettingsFailed", "Could not load preset settings."),
			false,
			EPresetDiagnosticSeverity::Error);
		return Checks;
	}

	const TArray<FWorkflowPreset>& Workflows = GetEffectiveWorkflowPresets();
	const TArray<FString>& RegistryDiagnostics = GetWorkflowRegistryDiagnostics();
	const bool bRegistryPassed = RegistryDiagnostics.IsEmpty();
	AddCheck(
		LOCTEXT("CheckWorkflowRegistry", "Workflow Registry"),
		bRegistryPassed
			? LOCTEXT("CheckWorkflowRegistryPassed", "Default and project workflow files loaded successfully.")
			: FText::Format(
				LOCTEXT("CheckWorkflowRegistryFailed", "{0} registry diagnostic(s) require attention."),
				FText::AsNumber(RegistryDiagnostics.Num())),
		bRegistryPassed,
		AreWorkflowOverridesWriteProtected() ? EPresetDiagnosticSeverity::Error : EPresetDiagnosticSeverity::Warning);

	// 1. Stable workflow Ids.
	TSet<FName> SeenIds;
	int32 DuplicateIdCount = 0;
	int32 MissingIdCount = 0;
	for (const FWorkflowPreset& WorkflowPreset : Workflows)
	{
		if (WorkflowPreset.Id.IsNone())
		{
			++MissingIdCount;
			continue;
		}
		bool bAlreadySeen = false;
		SeenIds.Add(WorkflowPreset.Id, &bAlreadySeen);
		if (bAlreadySeen)
		{
			++DuplicateIdCount;
		}
	}

	const bool bIdsPassed = DuplicateIdCount == 0 && MissingIdCount == 0;
	AddCheck(
		LOCTEXT("CheckWorkflowId", "Workflow Id"),
		bIdsPassed
			? FText::Format(
				LOCTEXT("CheckWorkflowIdPassed", "{0} stable Id(s), no duplicates found."),
				FText::AsNumber(Workflows.Num()))
			: FText::Format(
				LOCTEXT("CheckWorkflowIdFailed", "{0} duplicate Id(s) and {1} missing Id(s)."),
				FText::AsNumber(DuplicateIdCount),
				FText::AsNumber(MissingIdCount)),
		bIdsPassed,
		EPresetDiagnosticSeverity::Error);

	// 2. Action values, including workflow JSON load and parse diagnostics.
	int32 ActivateActionCount = 0;
	int32 RestoreActionCount = 0;
	int32 EmptyActivateWorkflowCount = 0;
	int32 InvalidActionCount = 0;
	for (const FWorkflowPreset& WorkflowPreset : Workflows)
	{
		ActivateActionCount += WorkflowPreset.ActivateActions.Num();
		RestoreActionCount += WorkflowPreset.DeactivateActions.Num();
		if (WorkflowPreset.ActivateActions.IsEmpty())
		{
			++EmptyActivateWorkflowCount;
		}

		const auto CountInvalidActions = [&InvalidActionCount](const TArray<FDebugViewAction>& Actions)
		{
			for (const FDebugViewAction& RuntimeAction : Actions)
			{
				FTADebugViewCustomAction SavedAction;
				if (!ConvertRuntimeActionToSavedAction(RuntimeAction, SavedAction)
					|| !IsWorkflowActionRuntimeValid(SavedAction))
				{
					++InvalidActionCount;
				}
			}
		};
		CountInvalidActions(WorkflowPreset.ActivateActions);
		CountInvalidActions(WorkflowPreset.DeactivateActions);
	}

	const bool bActionsPassed = EmptyActivateWorkflowCount == 0 && InvalidActionCount == 0;
	AddCheck(
		LOCTEXT("CheckActionValues", "Action Values"),
		bActionsPassed
			? FText::Format(
				LOCTEXT("CheckActionValuesPassed", "{0} activate and {1} restore action(s) are valid."),
				FText::AsNumber(ActivateActionCount),
				FText::AsNumber(RestoreActionCount))
			: FText::Format(
				LOCTEXT("CheckActionValuesFailed", "{0} workflow(s) have no activate action and {1} action value(s) are invalid."),
				FText::AsNumber(EmptyActivateWorkflowCount),
				FText::AsNumber(InvalidActionCount)),
		bActionsPassed,
		EPresetDiagnosticSeverity::Error);

	// 3. Quick access references. Favorites is the only quick-access collection, so
	// this stays in step with what RunPresetDiagnostics reports in the detail rows.
	int32 StaleFavoriteCount = 0;
	for (const FTADebugViewQuickAction& Action : Settings->FavoriteActions)
	{
		TOptional<FDebugViewPreset> DebugPreset;
		TOptional<FWorkflowPreset> WorkflowPreset;
		if (!ResolveQuickAction(Action, DebugPreset, WorkflowPreset))
		{
			++StaleFavoriteCount;
		}
	}

	const bool bFavoritesPassed = StaleFavoriteCount == 0;
	AddCheck(
		LOCTEXT("CheckQuickAccess", "Quick Access References"),
		bFavoritesPassed
			? LOCTEXT("CheckQuickAccessPassed", "Favorites contains no stale references.")
			: FText::Format(
				LOCTEXT("CheckQuickAccessFailed", "{0} stale Favorites reference(s). Use Clean Stale References."),
				FText::AsNumber(StaleFavoriteCount)),
		bFavoritesPassed,
		EPresetDiagnosticSeverity::Warning);

	// 4. Legacy command fields awaiting migration into the action list format.
	int32 LegacyCommandPresetCount = 0;
	for (const FTADebugViewCustomWorkflowPreset& LegacyPreset : Settings->CustomWorkflowPresets)
	{
		if (!LegacyPreset.ActivateCommands.TrimStartAndEnd().IsEmpty() ||
			!LegacyPreset.DeactivateCommands.TrimStartAndEnd().IsEmpty())
		{
			++LegacyCommandPresetCount;
		}
	}

	const bool bLegacyPassed = LegacyCommandPresetCount == 0;
	AddCheck(
		LOCTEXT("CheckLegacyFields", "Legacy Command Fields"),
		bLegacyPassed
			? LOCTEXT("CheckLegacyFieldsPassed", "No migration required.")
			: FText::Format(
				LOCTEXT("CheckLegacyFieldsFailed", "{0} legacy preset(s) still use the command string fields."),
				FText::AsNumber(LegacyCommandPresetCount)),
		bLegacyPassed,
		EPresetDiagnosticSeverity::Warning);

	return Checks;
}

FText GetDiagnosticCheckStatusLabel(const FPresetDiagnosticCheck& Check)
{
	if (Check.bPassed)
	{
		return LOCTEXT("CheckStatusPassed", "Passed");
	}

	return Check.Severity == EPresetDiagnosticSeverity::Error
		? LOCTEXT("CheckStatusError", "Error")
		: LOCTEXT("CheckStatusWarning", "Warning");
}

FLinearColor GetDiagnosticCheckStatusColor(const FPresetDiagnosticCheck& Check)
{
	if (Check.bPassed)
	{
		return FLinearColor(0.24f, 0.72f, 0.42f, 1.0f);
	}

	return GetDiagnosticSeverityColor(Check.Severity);
}

FText GetDiagnosticSeverityLabel(EPresetDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EPresetDiagnosticSeverity::Error: return LOCTEXT("SeverityError", "Error");
	case EPresetDiagnosticSeverity::Warning: return LOCTEXT("SeverityWarning", "Warning");
	default: return LOCTEXT("SeverityInfo", "Info");
	}
}

FLinearColor GetDiagnosticSeverityColor(EPresetDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EPresetDiagnosticSeverity::Error: return FLinearColor(0.75f, 0.12f, 0.12f, 1.0f);
	case EPresetDiagnosticSeverity::Warning: return FLinearColor(0.85f, 0.55f, 0.08f, 1.0f);
	default: return FLinearColor(0.15f, 0.45f, 0.80f, 1.0f);
	}
}
}

#undef LOCTEXT_NAMESPACE
