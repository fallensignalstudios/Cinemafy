#include "SCinemafyHub.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyStoryLibrary.h"
#include "CinemafyTutorial.h"
#include "CinemafyTypes.h"
#include "SCinemafyShotBrowser.h"
#include "SCinemafyEditPanel.h"
#include "SCinemafyMulticam.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyHub"

const FName SCinemafyHub::TabName(TEXT("CinemafyHub"));

namespace
{
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& Tooltip, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tooltip)
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); });
	}

	/** A dropdown listing every (non-hidden) value of an enum. */
	template <typename TEnum>
	TSharedRef<SWidget> MakeEnumDropdown(const FText& Label, const FText& Tooltip, TFunction<void(TEnum)> OnPicked)
	{
		return SNew(SComboButton)
			.ToolTipText(Tooltip)
			.HAlign(HAlign_Center)
			.ButtonContent()
			[
				SNew(STextBlock).Text(Label)
			]
			.OnGetMenuContent_Lambda([OnPicked]()
			{
				FMenuBuilder Menu(true, nullptr);
				const UEnum* Enum = StaticEnum<TEnum>();
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
				{
					if (Enum->HasMetaData(TEXT("Hidden"), Index))
					{
						continue;
					}
					const TEnum Value = static_cast<TEnum>(Enum->GetValueByIndex(Index));
					Menu.AddMenuEntry(Enum->GetDisplayNameTextByIndex(Index), Enum->GetToolTipTextByIndex(Index), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([OnPicked, Value]() { OnPicked(Value); })));
				}
				return Menu.MakeWidget();
			});
	}
}

TSharedRef<SWidget> SCinemafyHub::MakeSection(const FText& Title, const TArray<TSharedRef<SWidget>>& Buttons)
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f));
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		Grid->AddSlot(Index % 2, Index / 2)
		[
			Buttons[Index]
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 10.f, 6.f, 2.f)
		[
			SNew(STextBlock)
			.Text(Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			.ColorAndOpacity(FLinearColor(1.f, 0.62f, 0.15f))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3.f)
		[
			Grid
		];
}

void SCinemafyHub::Construct(const FArguments& InArgs)
{
	using Lib = UCinemafyEditorLibrary;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(LOCTEXT("Scene", "Scene"), {
					MakeEnumDropdown<ECinemafyDirectorPreset>(LOCTEXT("Director", "Scene Director"),
						LOCTEXT("DirectorTip", "Create an AI director for the selected characters."),
						[](ECinemafyDirectorPreset Style) { Lib::CreateSceneDirectorFromSelection(Style); }),
					MakeButton(LOCTEXT("ShotBrowser", "Shot Browser"), LOCTEXT("ShotBrowserTip", "Film strip, pacing and director controls."),
						[]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyShotBrowser::TabName); }),
					MakeButton(LOCTEXT("EditPanel", "Cinemafy Edit"), LOCTEXT("EditPanelTip", "Razor, ripple, roll, transitions, titles, ducking, markers and grade."),
						[]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyEditPanel::TabName); }),
					MakeButton(LOCTEXT("Multicam", "Multicam"), LOCTEXT("MulticamTip", "Live camera grid: play and tap 1-9 to cut."),
						[]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyMulticam::TabName); }),
					MakeButton(LOCTEXT("Blend", "Gameplay Blend"), LOCTEXT("BlendTip", "Make the open sequence blend in from and out to gameplay."),
						[]() { Lib::SetUpGameplayBlendForOpenSequence(1.f, 1.f, true); }),
					MakeButton(LOCTEXT("AddToSeq", "Add Cameras to Sequence"), LOCTEXT("AddToSeqTip", "Cut to the selected cameras at the playhead."),
						[]() { Lib::AddSelectedCamerasToSequence(); }),
				})
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(LOCTEXT("Performance", "Performance"), {
					MakeButton(LOCTEXT("Choreograph", "Choreograph"), LOCTEXT("ChoreographTip", "Select a character, click points, type what they do (walk, vault, sword attack, taunt)."),
						[]() { Lib::ChoreographSelection(); }),
					MakeButton(LOCTEXT("MoveLibrary", "Build Move Library"), LOCTEXT("MoveLibraryTip", "Gather the project's attacks, gestures and reactions into a curatable asset."),
						[]() { Lib::CreateMoveLibraryFromFolder(TEXT("/Game")); }),
					MakeButton(LOCTEXT("Tutorial", "Tutorial"), LOCTEXT("TutorialTip", "A step-by-step walk from a script to a cut scene."),
						[]() { UCinemafyTutorialLibrary::ShowTutorial(); }),
					MakeButton(LOCTEXT("StoryBoard", "Story Board"), LOCTEXT("StoryBoardTip", "Every scene of your script as a card: previs, direct, takes, progress."),
						[]() { UCinemafyStoryLibrary::OpenStoryBoard(nullptr); }),
					MakeButton(LOCTEXT("ImportScript", "Import Script"), LOCTEXT("ImportScriptTip", "Break a full script (Fountain, Final Draft, or text) into acts, chapters and scenes: the project's checklist."),
						[]() { Lib::ImportScriptWithDialog(); }),
				})
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(LOCTEXT("Shots", "Shots"), {
					MakeEnumDropdown<ECinemafyShotType>(LOCTEXT("CreateShot", "Create Shot"),
						LOCTEXT("CreateShotTip", "Select the subject, then their partner (then anyone else for group shots)."),
						[](ECinemafyShotType Type) { Lib::CreateShotFromSelection(Type, false); }),
					MakeEnumDropdown<ECinemafyShotType>(LOCTEXT("CreateShotFlipped", "Create Shot (other side)"),
						LOCTEXT("CreateShotFlippedTip", "Same shots, from the other side of the line."),
						[](ECinemafyShotType Type) { Lib::CreateShotFromSelection(Type, true); }),
					MakeEnumDropdown<ECinemafyPrime>(LOCTEXT("Lens", "Lens Kit"),
						LOCTEXT("LensTip", "Put a cinema prime on the selected cameras."),
						[](ECinemafyPrime Prime) { Lib::ApplyPrimeToSelection(Prime, 2.8f); }),
				})
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(LOCTEXT("Camera", "Camera"), {
					MakeEnumDropdown<ECinemafyCameraFeel>(LOCTEXT("Feel", "Camera Feel"),
						LOCTEXT("FeelTip", "Operator feel for the selected cameras."),
						[](ECinemafyCameraFeel Feel) { Lib::ApplyCameraFeelToSelection(Feel); }),
					MakeEnumDropdown<ECinemafyCameraMove>(LOCTEXT("Move", "Camera Move"),
						LOCTEXT("MoveTip", "An eased move on the selected cameras (key Progress 0 -> 1)."),
						[](ECinemafyCameraMove Move) { Lib::ApplyCameraMoveToSelection(Move); }),
					MakeEnumDropdown<ECinemafyLook>(LOCTEXT("Look", "Look"),
						LOCTEXT("LookTip", "Grade the selected cameras."),
						[](ECinemafyLook Look) { Lib::ApplyLookToSelection(Look, 1.f); }),
					MakeButton(LOCTEXT("Scope", "Letterbox 2.39"), LOCTEXT("ScopeTip", "Scope framing on the selected cameras."),
						[]() { Lib::SetLetterboxOnSelection(2.39f); }),
					MakeButton(LOCTEXT("Flat", "Letterbox 1.85"), LOCTEXT("FlatTip", "Flat framing on the selected cameras."),
						[]() { Lib::SetLetterboxOnSelection(1.85f); }),
					MakeButton(LOCTEXT("NoBars", "No Letterbox"), LOCTEXT("NoBarsTip", "Back to full 16:9."),
						[]() { Lib::SetLetterboxOnSelection(0.f); }),
				})
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(LOCTEXT("Characters", "Characters"), {
					MakeEnumDropdown<ECinemafyLightingStyle>(LOCTEXT("Light", "Light Rig"),
						LOCTEXT("LightTip", "Light the selected character for the selected camera (or restyle selected rigs)."),
						[](ECinemafyLightingStyle Style) { Lib::ApplyLightingStyleToSelection(Style); }),
					MakeEnumDropdown<ECinemafyTemperament>(LOCTEXT("Listen", "Add Listening"),
						LOCTEXT("ListenTip", "Characters react while others talk (two selected = a conversation)."),
						[](ECinemafyTemperament Temperament) { Lib::AddListeningToSelection(Temperament); }),
					MakeEnumDropdown<ECinemafyEmotion>(LOCTEXT("Emotion", "Face Emotion"),
						LOCTEXT("EmotionTip", "Cinemafy Face + emotion on the selected characters."),
						[](ECinemafyEmotion Emotion) { Lib::ApplyFaceEmotionToSelection(Emotion); }),
					MakeButton(LOCTEXT("Gaze", "Add Gaze"), LOCTEXT("GazeTip", "Living eyes on the selected characters."),
						[]() { Lib::AddGazeToSelection(); }),
				})
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(LOCTEXT("Review", "Review"), {
					MakeButton(LOCTEXT("Continuity", "Check Continuity"), LOCTEXT("ContinuityTip", "Review every cut in the open sequence."),
						[]() { Lib::CheckSequenceContinuity(); }),
					MakeButton(LOCTEXT("Line", "Check 180 Line"), LOCTEXT("LineTip", "Select two characters and your cameras."),
						[]() { Lib::CheckLineOfActionForSelection(); }),
				})
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
