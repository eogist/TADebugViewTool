#include "Modules/ModuleManager.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "STADebugViewPanel.h"
#include "Styling/AppStyle.h"
#include "TADebugViewExecutor.h"
#include "TADebugViewPresetRegistry.h"
#include "TADebugViewQuickActionRuntime.h"
#include "TADebugViewToolCommands.h"
#include "TADebugViewToolConstants.h"
#include "TADebugViewWorkflowRegistry.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FTADebugViewToolModule"

namespace
{
const FName TADebugViewToolTabName(TEXT("TADebugViewTool"));
}

class FTADebugViewToolModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			TADebugViewToolTabName,
			FOnSpawnTab::CreateRaw(this, &FTADebugViewToolModule::SpawnDebugViewPanelTab),
			FCanSpawnTab::CreateRaw(this, &FTADebugViewToolModule::CanSpawnDebugViewPanelTab))
			.SetDisplayName(LOCTEXT("TabTitle", "TA Debug Views"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Open the TA debug view control panel."))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.GameSettings")))
			.SetAutoGenerateMenuEntry(false);

		TADebugViewTool::GetEffectiveWorkflowPresets();
		TADebugViewTool::InitializeQuickAccessDefaults();
		FTADebugViewToolCommands::Register();
		BindCommands();

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTADebugViewToolModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (TSharedPtr<SDockTab> ExistingTab = FGlobalTabmanager::Get()->FindExistingLiveTab(TADebugViewToolTabName))
		{
			ExistingTab->RequestCloseTab();
		}
		ActivePanel.Reset();
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TADebugViewToolTabName);

		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
		}

		PluginCommands.Reset();
		FTADebugViewToolCommands::Unregister();
	}

private:
	void BindCommands()
	{
		PluginCommands = MakeShared<FUICommandList>();

		const FTADebugViewToolCommands& Commands = FTADebugViewToolCommands::Get();
		PluginCommands->MapAction(
			Commands.OpenPanel,
			FExecuteAction::CreateRaw(this, &FTADebugViewToolModule::OpenDebugViewPanel));
		PluginCommands->MapAction(
			Commands.ResetDebug,
			FExecuteAction::CreateRaw(this, &FTADebugViewToolModule::ExecuteResetDebugShortcut));

		for (int32 FavoriteIndex = 0; FavoriteIndex < TADebugViewTool::FavoriteShortcutCount; ++FavoriteIndex)
		{
			// GetFavoriteCommand returns nullptr if FavoriteShortcutCount ever exceeds the number of
			// UI_COMMAND entries defined in FTADebugViewToolCommands; MapAction requires a valid command.
			if (const TSharedPtr<FUICommandInfo> FavoriteCommand = Commands.GetFavoriteCommand(FavoriteIndex))
			{
				PluginCommands->MapAction(
					FavoriteCommand,
					FExecuteAction::CreateRaw(this, &FTADebugViewToolModule::ExecuteFavoriteShortcut, FavoriteIndex));
			}
		}

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		const FTADebugViewToolCommands& Commands = FTADebugViewToolCommands::Get();

		if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("TADebugViewTool"));
			Section.AddMenuEntry(
				TEXT("TADebugViewTool_OpenPanel"),
				Commands.OpenPanel,
				LOCTEXT("OpenPanelMenuLabel", "TA Debug Views Panel"),
				TAttribute<FText>(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.GameSettings")),
				NAME_None);
			Section.AddSubMenu(
				TEXT("TADebugViewTool_SubMenu"),
				LOCTEXT("MenuLabel", "TA Debug Views Menu"),
				LOCTEXT("MenuTooltip", "Quick access to common editor debug views and profiling commands."),
				FNewToolMenuDelegate::CreateRaw(this, &FTADebugViewToolModule::FillToolMenu),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.GameSettings")));
		}

		if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar")))
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("PluginTools"));
			FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
				Commands.OpenPanel,
				LOCTEXT("ToolbarLabel", "TA Debug"),
				LOCTEXT("ToolbarTooltip", "Open the TA debug view control panel."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.GameSettings")));
			Section.AddEntry(Entry);
		}
	}

	TSharedRef<SDockTab> SpawnDebugViewPanelTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		TSharedPtr<STADebugViewPanel> DebugViewPanel;
		TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SAssignNew(DebugViewPanel, STADebugViewPanel)
				.Executor(&Executor)
			];

		ActivePanel = DebugViewPanel;
		return DockTab;
	}

	bool CanSpawnDebugViewPanelTab(const FSpawnTabArgs&) const
	{
		// Saved editor layouts may remember this Nomad tab as open. Only explicit
		// plugin actions are allowed to create it, so layout restoration stays closed.
		return bAllowManualPanelSpawn;
	}

	void OpenDebugViewPanel()
	{
		bAllowManualPanelSpawn = true;
		FGlobalTabmanager::Get()->TryInvokeTab(TADebugViewToolTabName);
		bAllowManualPanelSpawn = false;
	}

	void FillToolMenu(UToolMenu* Menu)
	{
		for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
		{
			FToolMenuSection& Section = Menu->AddSection(Group.Id, Group.Label);
			for (const TADebugViewTool::FDebugViewPreset& Preset : Group.Presets)
			{
				Section.AddMenuEntry(
					Preset.Id,
					Preset.Label,
					Preset.Tooltip,
					Preset.Icon,
					MakePresetAction(Preset));
			}
		}
	}

	FUIAction MakePresetAction(const TADebugViewTool::FDebugViewPreset& Preset)
	{
		return FUIAction(FExecuteAction::CreateRaw(&Executor, &TADebugViewTool::FTADebugViewExecutor::ExecutePreset, Preset));
	}

	void ExecuteResetDebugShortcut()
	{
		if (TADebugViewTool::ExecuteWorkflowById(FName(TEXT("TADebugWorkflow_ResetDebug")), Executor))
		{
			RefreshOpenPanelQuickAccess();
		}
	}

	void ExecuteFavoriteShortcut(int32 FavoriteIndex)
	{
		if (TADebugViewTool::ExecuteFavoriteAction(FavoriteIndex, Executor))
		{
			RefreshOpenPanelQuickAccess();
		}
	}

	void RefreshOpenPanelQuickAccess()
	{
		if (TSharedPtr<STADebugViewPanel> Panel = ActivePanel.Pin())
		{
			Panel->RefreshQuickAccess();
		}
	}

	TADebugViewTool::FTADebugViewExecutor Executor;
	TSharedPtr<FUICommandList> PluginCommands;
	TWeakPtr<STADebugViewPanel> ActivePanel;
	bool bAllowManualPanelSpawn = false;
};

IMPLEMENT_MODULE(FTADebugViewToolModule, TADebugViewTool)

#undef LOCTEXT_NAMESPACE
