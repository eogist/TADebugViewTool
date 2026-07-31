#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

namespace TADebugViewTool
{
inline FName ResolveWorkflowIconName(FName IconName)
{
	if (IconName == TEXT("Icons.Material"))
	{
		return TEXT("ClassIcon.Material");
	}
	if (IconName == TEXT("Icons.Performance"))
	{
		return TEXT("Profiler.Tab");
	}
	return IconName.IsNone() ? FName(TEXT("Icons.Settings")) : IconName;
}

enum class EWorkflowPresetSource : uint8
{
	Default,
	Modified,
	UserCreated
};

enum class EPresetActionType
{
	ViewMode,
	NaniteVisualization,
	LumenVisualization,
	VirtualShadowMapVisualization,
	Command
};

struct FDebugViewPreset
{
	FDebugViewPreset(
		const TCHAR* InId,
		const FText& InLabel,
		const FText& InTooltip,
		EViewModeIndex InViewMode,
		const FName InIconName)
		: Id(InId)
		, Label(InLabel)
		, Tooltip(InTooltip)
		, ActionType(EPresetActionType::ViewMode)
		, ViewMode(InViewMode)
		, Icon(FAppStyle::GetAppStyleSetName(), InIconName)
	{
	}

	FDebugViewPreset(
		const TCHAR* InId,
		const FText& InLabel,
		const FText& InTooltip,
		EPresetActionType InActionType,
		const TCHAR* InVisualizationMode,
		const FName InIconName)
		: Id(InId)
		, Label(InLabel)
		, Tooltip(InTooltip)
		, ActionType(InActionType)
		, ViewMode(VMI_Lit)
		, VisualizationMode(InVisualizationMode)
		, Icon(FAppStyle::GetAppStyleSetName(), InIconName)
	{
	}

	FDebugViewPreset(
		const TCHAR* InId,
		const FText& InLabel,
		const FText& InTooltip,
		const TCHAR* InCommands,
		const FName InIconName)
		: Id(InId)
		, Label(InLabel)
		, Tooltip(InTooltip)
		, ActionType(EPresetActionType::Command)
		, ViewMode(VMI_Lit)
		, Commands(InCommands)
		, Icon(FAppStyle::GetAppStyleSetName(), InIconName)
	{
	}

	FName Id;
	FText Label;
	FText Tooltip;
	EPresetActionType ActionType;
	EViewModeIndex ViewMode;
	FName VisualizationMode;
	FString Commands;
	FSlateIcon Icon;
};

struct FDebugViewAction
{
	static FDebugViewAction ViewMode(EViewModeIndex InViewMode)
	{
		FDebugViewAction Action;
		Action.ActionType = EPresetActionType::ViewMode;
		Action.ViewModeIndex = InViewMode;
		return Action;
	}

	static FDebugViewAction Nanite(const TCHAR* InVisualizationMode)
	{
		FDebugViewAction Action;
		Action.ActionType = EPresetActionType::NaniteVisualization;
		Action.VisualizationMode = InVisualizationMode;
		return Action;
	}

	static FDebugViewAction Lumen(const TCHAR* InVisualizationMode)
	{
		FDebugViewAction Action;
		Action.ActionType = EPresetActionType::LumenVisualization;
		Action.VisualizationMode = InVisualizationMode;
		return Action;
	}

	static FDebugViewAction VirtualShadowMap(const TCHAR* InVisualizationMode)
	{
		FDebugViewAction Action;
		Action.ActionType = EPresetActionType::VirtualShadowMapVisualization;
		Action.VisualizationMode = InVisualizationMode;
		return Action;
	}

	static FDebugViewAction Command(const TCHAR* InCommands)
	{
		FDebugViewAction Action;
		Action.ActionType = EPresetActionType::Command;
		Action.Commands = InCommands;
		return Action;
	}

	static FDebugViewAction FromPreset(const FDebugViewPreset& Preset)
	{
		FDebugViewAction Action;
		Action.ActionType = Preset.ActionType;
		Action.ViewModeIndex = Preset.ViewMode;
		Action.VisualizationMode = Preset.VisualizationMode;
		Action.Commands = Preset.Commands;
		return Action;
	}

	EPresetActionType ActionType = EPresetActionType::ViewMode;
	EViewModeIndex ViewModeIndex = VMI_Lit;
	FName VisualizationMode;
	FString Commands;
};

struct FWorkflowPreset
{
	FWorkflowPreset(
		const TCHAR* InId,
		const FText& InLabel,
		const FText& InTooltip,
		const FName InIconName,
		TArray<FDebugViewAction>&& InActivateActions,
		TArray<FDebugViewAction>&& InDeactivateActions,
		EWorkflowPresetSource InSource = EWorkflowPresetSource::Default)
		: Id(InId)
		, Label(InLabel)
		, Tooltip(InTooltip)
		, IconName(ResolveWorkflowIconName(InIconName))
		, Icon(FAppStyle::GetAppStyleSetName(), ResolveWorkflowIconName(InIconName))
		, ActivateActions(MoveTemp(InActivateActions))
		, DeactivateActions(MoveTemp(InDeactivateActions))
		, Source(InSource)
	{
	}

	FName Id;
	FText Label;
	FText Tooltip;
	FName IconName;
	FSlateIcon Icon;
	TArray<FDebugViewAction> ActivateActions;
	TArray<FDebugViewAction> DeactivateActions;
	EWorkflowPresetSource Source = EWorkflowPresetSource::Default;
};

struct FDebugViewGroup
{
	FDebugViewGroup(const TCHAR* InId, const FText& InLabel)
		: Id(InId)
		, Label(InLabel)
	{
	}

	FName Id;
	FText Label;
	TArray<FDebugViewPreset> Presets;
};
}
