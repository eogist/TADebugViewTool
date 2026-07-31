#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TADebugViewCustomPresetSettings.generated.h"

UENUM()
enum class ETADebugViewCustomActionType : uint8
{
	ViewMode,
	NaniteVisualization,
	LumenVisualization,
	VirtualShadowMapVisualization,
	Command
};

UENUM()
enum class ETADebugViewQuickActionType : uint8
{
	DebugPreset,
	WorkflowPreset
};

USTRUCT()
struct FTADebugViewQuickAction
{
	GENERATED_BODY()

	FTADebugViewQuickAction();
	FTADebugViewQuickAction(ETADebugViewQuickActionType InActionType, const FString& InId);

	bool IsValid() const;
	bool Matches(const FTADebugViewQuickAction& Other) const;

	UPROPERTY(EditAnywhere, Category = "Quick Action")
	ETADebugViewQuickActionType ActionType;

	UPROPERTY(EditAnywhere, Category = "Quick Action")
	FString Id;
};

USTRUCT()
struct FTADebugViewCustomAction
{
	GENERATED_BODY()

	FTADebugViewCustomAction();
	FTADebugViewCustomAction(ETADebugViewCustomActionType InActionType, const FString& InValue);

	UPROPERTY(EditAnywhere, Category = "Action")
	ETADebugViewCustomActionType ActionType;

	UPROPERTY(EditAnywhere, Category = "Action")
	FString Value;
};

USTRUCT()
struct FTADebugViewCustomWorkflowPreset
{
	GENERATED_BODY()

	FTADebugViewCustomWorkflowPreset();

	void EnsureId();
	void MigrateLegacyCommands();

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString Id;

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString Label;

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString Tooltip;

	UPROPERTY(EditAnywhere, Category = "Preset")
	TArray<FTADebugViewCustomAction> ActivateActions;

	UPROPERTY(EditAnywhere, Category = "Preset")
	TArray<FTADebugViewCustomAction> DeactivateActions;

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString ActivateCommands;

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString DeactivateCommands;
};

UCLASS(config = EditorPerProjectUserSettings)
class UTADebugViewCustomPresetSettings final : public UObject
{
	GENERATED_BODY()

public:
	static UTADebugViewCustomPresetSettings* GetMutable();

	void SaveUserSettings();

	UPROPERTY(config, EditAnywhere, Category = "TA Debug Views")
	TArray<FTADebugViewCustomWorkflowPreset> CustomWorkflowPresets;

	UPROPERTY(config, EditAnywhere, Category = "TA Debug Views")
	TArray<FTADebugViewQuickAction> FavoriteActions;

	UPROPERTY(config, EditAnywhere, Category = "TA Debug Views")
	uint8 LastViewportTarget = 0;

	UPROPERTY(config, EditAnywhere, Category = "TA Debug Views")
	uint8 LastPanelPage = 0;

	UPROPERTY(config, EditAnywhere, Category = "TA Debug Views")
	FString LastDebugGroupId;

	UPROPERTY(config, EditAnywhere, Category = "TA Debug Views")
	bool bHasInitializedQuickAccessDefaults = false;

	// Legacy EditorPerProjectUserSettings workflows are migrated once into
	// Project/Config/TADebugViewTool/WorkflowOverrides.json.
	UPROPERTY(config)
	bool bHasMigratedWorkflowOverridesV2 = false;

	// LastPanelPage was written against a page list that included a PresetEditor
	// entry. Both numberings occupy the same value range, so this flag is the only
	// way to tell a stale index from a current one. Set once the remap has run.
	UPROPERTY(config)
	bool bHasMigratedPanelPageV2 = false;
};
