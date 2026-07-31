#pragma once

#include "CoreMinimal.h"
#include "TADebugViewCustomPresetSettings.h"
#include "TADebugViewPresetTypes.h"

namespace TADebugViewTool
{
class FTADebugViewExecutor;

void InitializeQuickAccessDefaults();

bool ResolveQuickAction(
	const FTADebugViewQuickAction& QuickAction,
	TOptional<FDebugViewPreset>& OutDebugPreset,
	TOptional<FWorkflowPreset>& OutWorkflowPreset);

bool RemoveStaleQuickActions();

bool ExecuteDebugPresetAction(const FDebugViewPreset& Preset, FTADebugViewExecutor& Executor);
bool ExecuteWorkflowPresetAction(const FWorkflowPreset& WorkflowPreset, FTADebugViewExecutor& Executor);
bool ExecuteQuickAction(const FTADebugViewQuickAction& QuickAction, FTADebugViewExecutor& Executor);
bool ExecuteFavoriteAction(int32 FavoriteIndex, FTADebugViewExecutor& Executor);
bool ExecuteWorkflowById(FName WorkflowId, FTADebugViewExecutor& Executor);
}
