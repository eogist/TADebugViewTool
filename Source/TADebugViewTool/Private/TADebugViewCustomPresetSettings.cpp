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
	if (ActionType != Other.ActionType)
	{
		return false;
	}

	if (ActionType == ETADebugViewQuickActionType::ConsoleCommand)
	{
		return Id.TrimStartAndEnd().Equals(Other.Id.TrimStartAndEnd(), ESearchCase::IgnoreCase);
	}

	return FName(*Id.TrimStartAndEnd()) == FName(*Other.Id.TrimStartAndEnd());
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
	Id.TrimStartAndEndInline();
	if (Id.IsEmpty())
	{
		Id = BuildStableCustomPresetId(*this);
	}
}

void FTADebugViewCustomWorkflowPreset::MigrateLegacyCommands()
{
	const auto AppendLegacyCommandIfMissing = [](FString& LegacyCommands, TArray<FTADebugViewCustomAction>& Actions)
	{
		const FString NormalizedCommands = LegacyCommands.TrimStartAndEnd();
		if (NormalizedCommands.IsEmpty())
		{
			return;
		}

		const bool bAlreadyRepresented = Actions.ContainsByPredicate([&NormalizedCommands](const FTADebugViewCustomAction& Action)
		{
			return Action.ActionType == ETADebugViewCustomActionType::Command
				&& Action.Value.TrimStartAndEnd().Equals(NormalizedCommands, ESearchCase::IgnoreCase);
		});
		if (!bAlreadyRepresented)
		{
			Actions.Add(FTADebugViewCustomAction(ETADebugViewCustomActionType::Command, NormalizedCommands));
		}
		LegacyCommands.Reset();
	};

	AppendLegacyCommandIfMissing(ActivateCommands, ActivateActions);
	AppendLegacyCommandIfMissing(DeactivateCommands, DeactivateActions);
}

UTADebugViewCustomPresetSettings* UTADebugViewCustomPresetSettings::GetMutable()
{
	return GetMutableDefault<UTADebugViewCustomPresetSettings>();
}

void UTADebugViewCustomPresetSettings::SaveUserSettings()
{
	// CustomWorkflowPresets is read-only legacy storage. Normal preference saves must
	// not consume or clear its command fields: explicit project import performs that
	// conversion on a copy and marks completion only after the project file is saved.
	for (FTADebugViewCustomWorkflowPreset& Preset : CustomWorkflowPresets)
	{
		Preset.EnsureId();
	}

	// Workflow edits and explicit legacy import invalidate the registry themselves;
	// ordinary preference saves only persist per-user state.
	SaveConfig();
}
