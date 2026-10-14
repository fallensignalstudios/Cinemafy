#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FUICommandList;

/**
 * Premiere-style shortcuts for Cinemafy Edit, active while Sequencer has focus.
 * Rebind them under Editor Preferences > Keyboard Shortcuts > Cinemafy Edit.
 */
class FCinemafyEditCommands : public TCommands<FCinemafyEditCommands>
{
public:
	FCinemafyEditCommands();

	virtual void RegisterCommands() override;

	/** Maps every command onto a command list (Sequencer's). */
	static void BindTo(const TSharedRef<FUICommandList>& List);

	TSharedPtr<FUICommandInfo> Razor;
	TSharedPtr<FUICommandInfo> RazorAllTracks;
	TSharedPtr<FUICommandInfo> RippleDelete;
	TSharedPtr<FUICommandInfo> RippleTrimStart;
	TSharedPtr<FUICommandInfo> RippleTrimEnd;
	TSharedPtr<FUICommandInfo> RollEarlier;
	TSharedPtr<FUICommandInfo> RollLater;
	TSharedPtr<FUICommandInfo> PreviousCut;
	TSharedPtr<FUICommandInfo> NextCut;
	TSharedPtr<FUICommandInfo> AddTransition;
};
