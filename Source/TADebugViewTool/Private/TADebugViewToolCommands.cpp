#include "TADebugViewToolCommands.h"

#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FTADebugViewToolCommands"

FTADebugViewToolCommands::FTADebugViewToolCommands()
	: TCommands<FTADebugViewToolCommands>(
		TEXT("TADebugViewTool"),
		LOCTEXT("CommandContext", "TA Debug Views"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FTADebugViewToolCommands::RegisterCommands()
{
	const EModifierKey::Type QuickAccessChord = EModifierKey::Alt | EModifierKey::Shift;

	UI_COMMAND(OpenPanel, "TA Debug Views", "Open or focus the TA Debug Views panel.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::D));
	// Alt+Shift+R is already owned by Level Editor's Open Actor in Reference Viewer command.
	UI_COMMAND(ResetDebug, "Reset Debug", "Run the TA Debug Views reset workflow.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::Zero));
	UI_COMMAND(ExecuteFavorite1, "Execute Favorite 1", "Run the first TA Debug Views favorite.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::One));
	UI_COMMAND(ExecuteFavorite2, "Execute Favorite 2", "Run the second TA Debug Views favorite.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::Two));
	UI_COMMAND(ExecuteFavorite3, "Execute Favorite 3", "Run the third TA Debug Views favorite.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::Three));
	UI_COMMAND(ExecuteFavorite4, "Execute Favorite 4", "Run the fourth TA Debug Views favorite.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::Four));
	UI_COMMAND(ExecuteFavorite5, "Execute Favorite 5", "Run the fifth TA Debug Views favorite.", EUserInterfaceActionType::Button, FInputChord(QuickAccessChord, EKeys::Five));
}

TSharedPtr<FUICommandInfo> FTADebugViewToolCommands::GetFavoriteCommand(int32 FavoriteIndex) const
{
	switch (FavoriteIndex)
	{
	case 0:
		return ExecuteFavorite1;
	case 1:
		return ExecuteFavorite2;
	case 2:
		return ExecuteFavorite3;
	case 3:
		return ExecuteFavorite4;
	case 4:
		return ExecuteFavorite5;
	default:
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
