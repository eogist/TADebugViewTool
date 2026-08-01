#include "TADebugViewQuickActionRuntime.h"

#include "HAL/IConsoleManager.h"
#include "TADebugViewExecutor.h"
#include "TADebugViewPresetRegistry.h"
#include "TADebugViewToolConstants.h"
#include "TADebugViewWorkflowRegistry.h"

namespace TADebugViewTool
{
namespace
{
FTADebugViewQuickAction MakeQuickAction(ETADebugViewQuickActionType ActionType, FName Id)
{
	return FTADebugViewQuickAction(ActionType, Id.ToString());
}
}

void InitializeQuickAccessDefaults()
{
	UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable();
	if (!Settings || Settings->bHasInitializedQuickAccessDefaults)
	{
		return;
	}

	static const TArray<FTADebugViewQuickAction> DefaultFavorites =
	{
		FTADebugViewQuickAction(ETADebugViewQuickActionType::WorkflowPreset, TEXT("TADebugWorkflow_MaterialCost")),
		FTADebugViewQuickAction(ETADebugViewQuickActionType::WorkflowPreset, TEXT("TADebugWorkflow_NaniteAudit")),
		FTADebugViewQuickAction(ETADebugViewQuickActionType::WorkflowPreset, TEXT("TADebugWorkflow_LumenCheck")),
		FTADebugViewQuickAction(ETADebugViewQuickActionType::WorkflowPreset, TEXT("TADebugWorkflow_VSMCache"))
	};
	checkf(DefaultFavorites.Num() <= FavoriteShortcutCount, TEXT("Default favorite count exceeds FavoriteShortcutCount; add more Alt+Shift+N shortcuts or trim the defaults."));

	Settings->FavoriteActions = DefaultFavorites;
	Settings->bHasInitializedQuickAccessDefaults = true;
	Settings->SaveUserSettings();
}

bool ResolveQuickAction(
	const FTADebugViewQuickAction& QuickAction,
	TOptional<FDebugViewPreset>& OutDebugPreset,
	TOptional<FWorkflowPreset>& OutWorkflowPreset)
{
	OutDebugPreset.Reset();
	OutWorkflowPreset.Reset();

	if (!QuickAction.IsValid())
	{
		return false;
	}

	const FName QuickActionId(*QuickAction.Id);
	if (QuickAction.ActionType == ETADebugViewQuickActionType::DebugPreset)
	{
		for (const FDebugViewGroup& Group : GetPresetGroups())
		{
			for (const FDebugViewPreset& Preset : Group.Presets)
			{
				if (Preset.Id == QuickActionId)
				{
					OutDebugPreset = Preset;
					return true;
				}
			}
		}

		return false;
	}
	if (QuickAction.ActionType == ETADebugViewQuickActionType::ConsoleCommand)
	{
		IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(*QuickAction.Id);
		if (!ConsoleObject)
		{
			return false;
		}

		FString HelpText = ConsoleObject->GetHelp();
		if (IConsoleVariable* ConsoleVariable = ConsoleObject->AsVariable())
		{
			const FString CurrentValue = ConsoleVariable->GetString();
			HelpText = HelpText.IsEmpty()
				? FString::Printf(TEXT("Current value: %s"), *CurrentValue)
				: FString::Printf(TEXT("Current value: %s\n%s"), *CurrentValue, *HelpText);
		}
		else if (HelpText.IsEmpty())
		{
			HelpText = TEXT("Registered UE console command.");
		}

		const FString RuntimePresetId = FString::Printf(
			TEXT("TADebugConsole_%08x"),
			GetTypeHash(QuickAction.Id));
		OutDebugPreset.Emplace(
			*RuntimePresetId,
			FText::FromString(QuickAction.Id),
			FText::FromString(HelpText),
			*QuickAction.Id,
			FName(TEXT("Icons.Settings")));
		return true;
	}
	if (QuickAction.ActionType != ETADebugViewQuickActionType::WorkflowPreset)
	{
		return false;
	}

	for (const FWorkflowPreset& WorkflowPreset : GetEffectiveWorkflowPresets())
	{
		if (WorkflowPreset.Id == QuickActionId)
		{
			OutWorkflowPreset = WorkflowPreset;
			return true;
		}
	}

	return false;
}

bool RemoveStaleQuickActions()
{
	UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable();
	if (!Settings)
	{
		return false;
	}

	const auto IsStaleAction = [](const FTADebugViewQuickAction& QuickAction)
	{
		TOptional<FDebugViewPreset> DebugPreset;
		TOptional<FWorkflowPreset> WorkflowPreset;
		return !ResolveQuickAction(QuickAction, DebugPreset, WorkflowPreset);
	};

	if (Settings->FavoriteActions.RemoveAll(IsStaleAction) > 0)
	{
		Settings->SaveUserSettings();
		return true;
	}

	return false;
}

bool ExecuteDebugPresetAction(const FDebugViewPreset& Preset, FTADebugViewExecutor& Executor)
{
	Executor.ExecutePresetFromPanel(Preset);
	return true;
}

bool ExecuteWorkflowPresetAction(const FWorkflowPreset& WorkflowPreset, FTADebugViewExecutor& Executor)
{
	Executor.ExecuteWorkflowPreset(WorkflowPreset);
	return true;
}

bool ExecuteQuickAction(const FTADebugViewQuickAction& QuickAction, FTADebugViewExecutor& Executor)
{
	TOptional<FDebugViewPreset> DebugPreset;
	TOptional<FWorkflowPreset> WorkflowPreset;
	if (!ResolveQuickAction(QuickAction, DebugPreset, WorkflowPreset))
	{
		RemoveStaleQuickActions();
		return false;
	}

	if (DebugPreset.IsSet())
	{
		return ExecuteDebugPresetAction(*DebugPreset, Executor);
	}

	if (WorkflowPreset.IsSet())
	{
		return ExecuteWorkflowPresetAction(*WorkflowPreset, Executor);
	}

	return false;
}

bool ExecuteFavoriteAction(int32 FavoriteIndex, FTADebugViewExecutor& Executor)
{
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings || !Settings->FavoriteActions.IsValidIndex(FavoriteIndex))
	{
		return false;
	}

	const FTADebugViewQuickAction QuickAction = Settings->FavoriteActions[FavoriteIndex];
	return ExecuteQuickAction(QuickAction, Executor);
}

bool ExecuteWorkflowById(FName WorkflowId, FTADebugViewExecutor& Executor)
{
	if (WorkflowId.IsNone())
	{
		return false;
	}

	return ExecuteQuickAction(MakeQuickAction(ETADebugViewQuickActionType::WorkflowPreset, WorkflowId), Executor);
}
}
