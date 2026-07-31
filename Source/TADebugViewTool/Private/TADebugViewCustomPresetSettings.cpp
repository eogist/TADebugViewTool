#include "TADebugViewCustomPresetSettings.h"

namespace
{
FString NormalizePresetIdPart(FString Value)
{
	Value.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Value.TrimStartAndEndInline();
	return Value;
}

void AppendActionIdPart(const FTADebugViewCustomAction& Action, FString& OutIdSource)
{
	OutIdSource += FString::Printf(TEXT("%d:"), static_cast<int32>(Action.ActionType));
	OutIdSource += NormalizePresetIdPart(Action.Value);
	OutIdSource += TEXT("|");
}

FString BuildStableCustomPresetId(const FTADebugViewCustomWorkflowPreset& Preset)
{
	FString IdSource;
	IdSource += NormalizePresetIdPart(Preset.Label);
	IdSource += TEXT("|");
	IdSource += NormalizePresetIdPart(Preset.Tooltip);
	IdSource += TEXT("|Activate:");

	if (Preset.ActivateActions.IsEmpty())
	{
		IdSource += NormalizePresetIdPart(Preset.ActivateCommands);
	}
	else
	{
		for (const FTADebugViewCustomAction& Action : Preset.ActivateActions)
		{
			AppendActionIdPart(Action, IdSource);
		}
	}

	IdSource += TEXT("|Deactivate:");
	if (Preset.DeactivateActions.IsEmpty())
	{
		IdSource += NormalizePresetIdPart(Preset.DeactivateCommands);
	}
	else
	{
		for (const FTADebugViewCustomAction& Action : Preset.DeactivateActions)
		{
			AppendActionIdPart(Action, IdSource);
		}
	}

	return FString::Printf(TEXT("TADebugCustom_%08x"), GetTypeHash(IdSource));
}
}

FTADebugViewQuickAction::FTADebugViewQuickAction()
	: ActionType(ETADebugViewQuickActionType::WorkflowPreset)
{
}

FTADebugViewQuickAction::FTADebugViewQuickAction(ETADebugViewQuickActionType InActionType, const FString& InId)
	: ActionType(InActionType)
	, Id(InId)
{
}

bool FTADebugViewQuickAction::IsValid() const
{
	return !Id.TrimStartAndEnd().IsEmpty();
}

bool FTADebugViewQuickAction::Matches(const FTADebugViewQuickAction& Other) const
{
	return ActionType == Other.ActionType && Id == Other.Id;
}

FTADebugViewCustomAction::FTADebugViewCustomAction()
	: ActionType(ETADebugViewCustomActionType::ViewMode)
	, Value(TEXT("VMI_Lit"))
{
}

FTADebugViewCustomAction::FTADebugViewCustomAction(ETADebugViewCustomActionType InActionType, const FString& InValue)
	: ActionType(InActionType)
	, Value(InValue)
{
}

FTADebugViewCustomWorkflowPreset::FTADebugViewCustomWorkflowPreset()
	: Label(TEXT("Custom Preset"))
	, Tooltip(TEXT("Run custom debug commands."))
{
}

void FTADebugViewCustomWorkflowPreset::EnsureId()
{
	if (Id.IsEmpty())
	{
		Id = BuildStableCustomPresetId(*this);
	}
}

void FTADebugViewCustomWorkflowPreset::MigrateLegacyCommands()
{
	if (!ActivateCommands.TrimStartAndEnd().IsEmpty())
	{
		if (ActivateActions.IsEmpty())
		{
			ActivateActions.Add(FTADebugViewCustomAction(ETADebugViewCustomActionType::Command, ActivateCommands));
		}
		ActivateCommands.Reset();
	}

	if (!DeactivateCommands.TrimStartAndEnd().IsEmpty())
	{
		if (DeactivateActions.IsEmpty())
		{
			DeactivateActions.Add(FTADebugViewCustomAction(ETADebugViewCustomActionType::Command, DeactivateCommands));
		}
		DeactivateCommands.Reset();
	}
}

UTADebugViewCustomPresetSettings* UTADebugViewCustomPresetSettings::GetMutable()
{
	return GetMutableDefault<UTADebugViewCustomPresetSettings>();
}

void UTADebugViewCustomPresetSettings::SaveUserSettings()
{
	// CustomWorkflowPresets is legacy storage: nothing writes new entries to it any more,
	// since workflow edits go to Project/Config/TADebugViewTool/WorkflowOverrides.json.
	// It is still normalized here so the one-time migration reads stable, unique Ids.
	// Order is preserved, so an Id collision only ever suffixes the later duplicate and
	// existing Favorites references stay valid.
	TSet<FString> UsedPresetIds;
	for (FTADebugViewCustomWorkflowPreset& Preset : CustomWorkflowPresets)
	{
		Preset.EnsureId();
		Preset.MigrateLegacyCommands();

		const FString BaseId = Preset.Id;
		int32 Suffix = 2;
		while (UsedPresetIds.Contains(Preset.Id))
		{
			Preset.Id = FString::Printf(TEXT("%s_%d"), *BaseId, Suffix++);
		}

		UsedPresetIds.Add(Preset.Id);
	}

	// No workflow cache invalidation here. This saves favorites and panel state, and
	// the legacy CustomWorkflowPresets array is now only read by the one-time
	// migration. Workflow edits go through SaveWorkflowOverride / ResetWorkflowToDefault
	// / DeleteUserWorkflow, which invalidate the registry themselves. Invalidating
	// here would also re-dirty the cache mid-rebuild, since the migration calls back
	// into this function from inside RebuildWorkflowCache.
	SaveConfig();
}
