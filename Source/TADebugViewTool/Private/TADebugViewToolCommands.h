#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FTADebugViewToolCommands final : public TCommands<FTADebugViewToolCommands>
{
public:
	FTADebugViewToolCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> GetFavoriteCommand(int32 FavoriteIndex) const;

	TSharedPtr<FUICommandInfo> OpenPanel;
	TSharedPtr<FUICommandInfo> ResetDebug;
	TSharedPtr<FUICommandInfo> ExecuteFavorite1;
	TSharedPtr<FUICommandInfo> ExecuteFavorite2;
	TSharedPtr<FUICommandInfo> ExecuteFavorite3;
	TSharedPtr<FUICommandInfo> ExecuteFavorite4;
	TSharedPtr<FUICommandInfo> ExecuteFavorite5;
};
