#pragma once

#include "CoreMinimal.h"
#include "TADebugViewCustomPresetSettings.h"
#include "TADebugViewPresetTypes.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SHorizontalBox;
class SEditableTextBox;
class SComboButton;
class SMenuAnchor;
class SSearchBox;

namespace TADebugViewTool
{
enum class EDebugViewportTarget : uint8;
class FTADebugViewExecutor;
}

class STADebugViewPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STADebugViewPanel)
	{
	}
		SLATE_ARGUMENT(TADebugViewTool::FTADebugViewExecutor*, Executor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void RefreshQuickAccess();
	virtual void OnFocusChanging(
		const FWeakWidgetPath& PreviousFocusPath,
		const FWidgetPath& NewWidgetPath,
		const FFocusEvent& InFocusEvent) override;
	virtual FReply OnPreviewMouseButtonDown(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent) override;

private:
	enum class EPanelPage : uint8
	{
		Workflows,
		DebugViews,
		Advanced,
		Help,
		Diagnostics,
		Count
	};

	// Translates a saved LastPanelPage value into the current EPanelPage numbering.
	// bAlreadyMigrated selects whether the value uses the current page list or the
	// older one that still had a PresetEditor page.
	static EPanelPage MigrateSavedPanelPage(uint8 SavedPanelPageValue, bool bAlreadyMigrated);

	enum class ECustomActionListKind : uint8
	{
		Activate,
		Deactivate
	};

	enum class ESearchCategory : uint8
	{
		All,
		Workflows,
		DebugViews,
		Advanced
	};

	TSharedRef<SWidget> MakeNavigationBar();
	TSharedRef<SWidget> MakeNavigationButton(const FText& Label, EPanelPage Page);
	TSharedRef<SWidget> MakeHeaderBar();
	TSharedRef<SWidget> MakeViewportTargetButton(TADebugViewTool::EDebugViewportTarget Target, const FText& Label);
	TSharedRef<SWidget> MakeLiveStatusBar();
	TSharedRef<SWidget> MakeStatusChip(const FText& Label, TAttribute<FText> Value);
	TSharedRef<SWidget> MakeSearchArea();
	TSharedRef<SWidget> MakeSearchCategoryMenu();
	TSharedRef<SWidget> MakeQuickAccessArea();
	TSharedRef<SWidget> MakeQuickActionButton(
		const FTADebugViewQuickAction& QuickAction,
		int32 FavoriteIndex,
		bool bCloseFavoritesMenu = false);
	TSharedRef<SWidget> MakeQuickAccessFavoritesMenu();
	TSharedRef<SWidget> MakeFavoriteToggleButton(const FTADebugViewQuickAction& QuickAction);
	TSharedRef<SWidget> MakeWorkflowsPage();
	TSharedRef<SWidget> MakeContextInspector();
	TSharedRef<SWidget> MakeWorkflowDetails(const TADebugViewTool::FWorkflowPreset& WorkflowPreset);
	TSharedRef<SWidget> MakeDebugViewsPage();
	TSharedRef<SWidget> MakeDebugViewRow(const TADebugViewTool::FDebugViewPreset& Preset);
	TSharedRef<SWidget> MakeAdvancedPage();
	TSharedRef<SWidget> MakeUtilityRow(const TADebugViewTool::FDebugViewPreset& Preset);
	TSharedRef<SWidget> MakeHelpPage();
	TSharedRef<SWidget> MakeHelpBlock(const FText& Heading, const TArray<TSharedRef<SWidget>>& Rows);
	TSharedRef<SWidget> MakeDiagnosticsPage();
	TSharedRef<SWidget> MakeSectionHeading(const FText& Label);
	TSharedRef<SWidget> MakeWorkflowPresetButton(TADebugViewTool::FWorkflowPreset WorkflowPreset);
	TSharedRef<SWidget> MakeDebugGroupButton(const TADebugViewTool::FDebugViewGroup& Group);
	TSharedRef<SWidget> MakeCustomPresetEditor();
	TSharedRef<SWidget> MakeEditorTextField(
		const FText& Label,
		TAttribute<FText> Text,
		const FOnTextChanged& OnTextChanged,
		bool bMultiLine = false);
	TSharedRef<SWidget> MakeActionListEditor(
		const FText& Label,
		TArray<FTADebugViewCustomAction>& Actions,
		TSharedPtr<SVerticalBox>& ActionListBox,
		ECustomActionListKind ActionListKind);
	TSharedRef<SWidget> MakeActionRow(TArray<FTADebugViewCustomAction>& Actions, int32 ActionIndex, ECustomActionListKind ActionListKind);
	TSharedRef<SWidget> MakeActionTypeCombo(TArray<FTADebugViewCustomAction>& Actions, int32 ActionIndex, ECustomActionListKind ActionListKind);
	TSharedRef<SWidget> MakeActionValueWidget(TArray<FTADebugViewCustomAction>& Actions, int32 ActionIndex);

	void RebuildCustomPresetButtons();
	void RebuildContextInspector();
	void RebuildDebugPresetButtons();
	void RebuildDebugGroupList();
	void RebuildQuickAccess();
	bool ChangeQuickAccessPage(int32 PageDelta);
	void RebuildSearchResults();
	void RebuildDiagnostics();
	void RebuildActionListEditor(
		TArray<FTADebugViewCustomAction>& Actions,
		const TSharedPtr<SVerticalBox>& ActionListBox,
		ECustomActionListKind ActionListKind);
	void RebuildEditedActionLists();
	void LoadUserPreferences();
	void SaveUserPreferences() const;
	void SavePanelPagePreference() const;
	void SaveDebugGroupPreference() const;
	void SaveViewportTargetPreference() const;
	void RefreshAllPresetUI();
	void RefreshStatusCache();
	EActiveTimerReturnType UpdateStatusCache(double CurrentTime, float DeltaTime);
	void OnSearchTextChanged(const FText& Text);
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	void SetSearchCategory(ESearchCategory Category);
	FText GetSearchCategoryLabel() const;
	void OnWorkflowFilterTextChanged(const FText& Text);
	bool DoesWorkflowMatchFilter(const TADebugViewTool::FWorkflowPreset& WorkflowPreset) const;
	void ExecuteQuickAction(const FTADebugViewQuickAction& QuickAction);
	void ExecuteWorkflowPresetFromPanel(TADebugViewTool::FWorkflowPreset WorkflowPreset);
	void ExecuteDebugPresetFromPanel(TADebugViewTool::FDebugViewPreset Preset);
	void ToggleFavoriteAction(const FTADebugViewQuickAction& QuickAction);
	bool IsFavoriteAction(const FTADebugViewQuickAction& QuickAction) const;
	bool IsQuickActionActiveCached(const FTADebugViewQuickAction& QuickAction) const;
	bool IsDebugPresetActiveCached(FName PresetId) const;
	void RemoveStaleQuickActions();
	void LoadCustomPresetForEditing(const FString& PresetId);
	void SelectWorkflow(FName WorkflowId);
	void AddNewCustomPreset();
	void SaveEditedCustomPreset();
	void DeleteEditedCustomPreset();
	void ResetEditedWorkflowToDefault();
	void CancelWorkflowEditing();
	bool CanSaveEditedCustomPreset() const;
	FText GetCustomPresetEditorTitle() const;
	FText GetActiveWorkflowStatusText() const;
	FText GetHeaderWorkflowText() const;
	FText GetEditorSourceNoteText() const;
	FText GetEditedNameError() const;
	FText GetPageCountText(EPanelPage Page) const;
	void RefreshDiagnosticsCache();
	FText GetQuickActionShortcutText(const FTADebugViewQuickAction& QuickAction) const;
	bool IsWorkflowActive(FName WorkflowId) const;

	TADebugViewTool::FTADebugViewExecutor* Executor = nullptr;
	TSharedPtr<SHorizontalBox> QuickAccessBox;
	TSharedPtr<SHorizontalBox> QuickAccessPageDotsBox;
	TSharedPtr<SComboButton> QuickAccessFavoritesButton;
	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SMenuAnchor> SearchResultsAnchor;
	TSharedPtr<SVerticalBox> SearchResultsBox;
	TSharedPtr<SSearchBox> WorkflowFilterBox;
	TSharedPtr<SVerticalBox> DiagnosticsBox;
	TSharedPtr<SVerticalBox> CustomPresetListBox;
	TSharedPtr<SVerticalBox> ContextDetailsBox;
	TSharedPtr<SVerticalBox> DebugPresetListBox;
	TSharedPtr<SVerticalBox> DebugGroupListBox;
	TSharedPtr<SVerticalBox> ActivateActionListBox;
	TSharedPtr<SVerticalBox> DeactivateActionListBox;
	EPanelPage ActivePage = EPanelPage::Workflows;
	int32 QuickAccessPageIndex = 0;
	FName SelectedDebugGroupId;
	FName SelectedWorkflowId;
	FString EditingPresetId;
	FString EditingLabel;
	FString EditingTooltip;
	TArray<FTADebugViewCustomAction> EditingActivateActions;
	TArray<FTADebugViewCustomAction> EditingDeactivateActions;
	TADebugViewTool::EWorkflowPresetSource EditingWorkflowSource = TADebugViewTool::EWorkflowPresetSource::UserCreated;
	FName EditingIconName = TEXT("Icons.Settings");
	bool bWorkflowEditorOpen = false;
	bool bCreatingWorkflow = false;
	// Set when Save was attempted with an invalid name so the editor can show an
	// inline message instead of only disabling the Save button.
	FText EditedNameError;
	TArray<FTADebugViewQuickAction> FilteredSearchActions;
	FString SearchText;
	FString WorkflowFilterText;
	ESearchCategory SearchCategory = ESearchCategory::All;
	FText CachedTargetStatus;
	FText CachedViewModeStatus;
	FText CachedVisualizationStatus;
	FText CachedWorkflowStatus;
	FText CachedDiagnosticsStatus;
	// Number of failing diagnostic checks, cached so the status chip and the nav
	// badge can be read from per-frame attribute lambdas without re-running them.
	int32 CachedDiagnosticsFailureCount = 0;
	TSet<FName> CachedActiveDebugPresetIds;
};
