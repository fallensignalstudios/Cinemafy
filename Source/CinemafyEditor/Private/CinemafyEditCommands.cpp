#include "CinemafyEditCommands.h"
#include "CinemafyEditLibrary.h"
#include "Framework/Commands/UICommandList.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "CinemafyEditCommands"

FCinemafyEditCommands::FCinemafyEditCommands()
	: TCommands<FCinemafyEditCommands>(TEXT("CinemafyEdit"), LOCTEXT("Context", "Cinemafy Edit"), NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FCinemafyEditCommands::RegisterCommands()
{
	UI_COMMAND(Razor, "Razor", "Split the shot under the playhead.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::K));
	UI_COMMAND(RazorAllTracks, "Razor All Tracks", "Split every section under the playhead.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::K));
	UI_COMMAND(RippleDelete, "Ripple Delete Shot", "Remove the shot under the playhead and close the gap on every track.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::Delete));
	UI_COMMAND(RippleTrimStart, "Ripple Trim Start", "Trim the shot's head to the playhead and pull everything after it left.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Q));
	UI_COMMAND(RippleTrimEnd, "Ripple Trim End", "Trim the shot's tail from the playhead and pull everything after it left.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::W));
	UI_COMMAND(RollEarlier, "Roll Cut Earlier", "Move the nearest cut one frame earlier.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Comma));
	UI_COMMAND(RollLater, "Roll Cut Later", "Move the nearest cut one frame later.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Period));
	UI_COMMAND(PreviousCut, "Previous Cut", "Jump to the previous cut.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Up));
	UI_COMMAND(NextCut, "Next Cut", "Jump to the next cut.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Down));
	UI_COMMAND(AddTransition, "Add Default Transition", "Put a wipe on the cut nearest the playhead.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::D));
}

void FCinemafyEditCommands::BindTo(const TSharedRef<FUICommandList>& List)
{
	using Lib = UCinemafyEditLibrary;
	const FCinemafyEditCommands& Commands = Get();
	List->MapAction(Commands.Razor, FExecuteAction::CreateLambda([]() { Lib::RazorAtPlayhead(false); }));
	List->MapAction(Commands.RazorAllTracks, FExecuteAction::CreateLambda([]() { Lib::RazorAtPlayhead(true); }));
	List->MapAction(Commands.RippleDelete, FExecuteAction::CreateLambda([]() { Lib::RippleDeleteShotAtPlayhead(); }));
	List->MapAction(Commands.RippleTrimStart, FExecuteAction::CreateLambda([]() { Lib::RippleTrimStartToPlayhead(); }));
	List->MapAction(Commands.RippleTrimEnd, FExecuteAction::CreateLambda([]() { Lib::RippleTrimEndToPlayhead(); }));
	List->MapAction(Commands.RollEarlier, FExecuteAction::CreateLambda([]() { Lib::RollNearestCut(-1); }));
	List->MapAction(Commands.RollLater, FExecuteAction::CreateLambda([]() { Lib::RollNearestCut(1); }));
	List->MapAction(Commands.PreviousCut, FExecuteAction::CreateLambda([]() { Lib::GoToCut(false); }));
	List->MapAction(Commands.NextCut, FExecuteAction::CreateLambda([]() { Lib::GoToCut(true); }));
	List->MapAction(Commands.AddTransition, FExecuteAction::CreateLambda([]()
	{
		Lib::AddTransitionAtNearestCut(ECinemafyTransition::Wipe, 0.7f, ECinemafyWipeDirection::Alternate);
	}));
}

#undef LOCTEXT_NAMESPACE
