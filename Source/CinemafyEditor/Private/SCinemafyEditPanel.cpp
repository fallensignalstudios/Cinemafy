#include "SCinemafyEditPanel.h"
#include "CinemafyEditLibrary.h"
#include "SCinemafyMulticam.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyEditPanel"

const FName SCinemafyEditPanel::TabName(TEXT("CinemafyEdit"));

TSharedRef<SWidget> SCinemafyEditPanel::MakeButton(const FText& Label, const FText& Tooltip, TFunction<void()> OnClick)
{
	return SNew(SButton)
		.Text(Label)
		.ToolTipText(Tooltip)
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); });
}

TSharedRef<SWidget> SCinemafyEditPanel::MakeLabeled(const FText& Label, TSharedRef<SWidget> Content)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SBox).MinDesiredWidth(70.f)
			[
				SNew(STextBlock).Text(Label)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			Content
		];
}

TSharedRef<SWidget> SCinemafyEditPanel::MakeSection(const FText& SectionTitle, const TArray<TSharedRef<SWidget>>& Rows)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).Text(SectionTitle).Font(FAppStyle::GetFontStyle("BoldFont"))
		];
	for (const TSharedRef<SWidget>& Row : Rows)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 2.f)[Row];
	}
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.f)
		[
			Box
		];
}

template <typename TEnum>
TSharedRef<SWidget> SCinemafyEditPanel::MakeEnumPicker(TEnum* Value)
{
	return SNew(SComboButton)
		.OnGetMenuContent_Lambda([Value]()
		{
			FMenuBuilder Menu(true, nullptr);
			const UEnum* Enum = StaticEnum<TEnum>();
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
			{
				if (Enum->HasMetaData(TEXT("Hidden"), Index))
				{
					continue;
				}
				const TEnum Option = static_cast<TEnum>(Enum->GetValueByIndex(Index));
				Menu.AddMenuEntry(Enum->GetDisplayNameTextByIndex(Index), Enum->GetToolTipTextByIndex(Index), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Value, Option]() { *Value = Option; })));
			}
			return Menu.MakeWidget();
		})
		.ButtonContent()
		[
			SNew(STextBlock).Text_Lambda([Value]() { return StaticEnum<TEnum>()->GetDisplayNameTextByValue(static_cast<int64>(*Value)); })
		];
}

void SCinemafyEditPanel::Construct(const FArguments& InArgs)
{
	using Lib = UCinemafyEditLibrary;
	Title.Style = ECinemafyTitleStyle::LowerThird;
	Title.Duration = 4.f;

	auto Wrap = [](std::initializer_list<TSharedRef<SWidget>> Widgets)
	{
		TSharedRef<SWrapBox> Box = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
		for (const TSharedRef<SWidget>& Widget : Widgets)
		{
			Box->AddSlot()[Widget];
		}
		return Box;
	};

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(6.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("Intro", "Works on the Level Sequence open in Sequencer, at the playhead (or the cut nearest it). Everything is undoable. Ripples move every track."))
			]

			// --- Edit ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("Edit", "Edit"), {
					Wrap({
						MakeButton(LOCTEXT("Razor", "Razor"), LOCTEXT("RazorTip", "Split the shot under the playhead (Ctrl+K)."), []() { Lib::RazorAtPlayhead(false); }),
						MakeButton(LOCTEXT("RazorAll", "Razor All Tracks"), LOCTEXT("RazorAllTip", "Split every section under the playhead (Ctrl+Shift+K)."), []() { Lib::RazorAtPlayhead(true); }),
						MakeButton(LOCTEXT("RippleDelete", "Ripple Delete Shot"), LOCTEXT("RippleDeleteTip", "Remove the shot and close the gap on every track (Shift+Delete)."), []() { Lib::RippleDeleteShotAtPlayhead(); }),
						MakeButton(LOCTEXT("TrimStart", "Ripple Trim Start (Q)"), LOCTEXT("TrimStartTip", "Cut the shot's head to the playhead, pull everything left (Alt+Q)."), []() { Lib::RippleTrimStartToPlayhead(); }),
						MakeButton(LOCTEXT("TrimEnd", "Ripple Trim End (W)"), LOCTEXT("TrimEndTip", "Cut the shot's tail from the playhead, pull everything left (Alt+W)."), []() { Lib::RippleTrimEndToPlayhead(); }),
					}),
					Wrap({
						MakeButton(LOCTEXT("RollEarlier", "Roll -1"), LOCTEXT("RollEarlierTip", "Move the nearest cut a frame earlier (Alt+,)."), []() { Lib::RollNearestCut(-1); }),
						MakeButton(LOCTEXT("RollLater", "Roll +1"), LOCTEXT("RollLaterTip", "Move the nearest cut a frame later (Alt+.)."), []() { Lib::RollNearestCut(1); }),
						MakeButton(LOCTEXT("RollHere", "Roll Cut to Playhead"), LOCTEXT("RollHereTip", "Move the nearest cut to the playhead."), []() { Lib::RollNearestCutToPlayhead(); }),
						MakeButton(LOCTEXT("Swap", "Swap With Next"), LOCTEXT("SwapTip", "Trade places with the next shot (best with locked-off cameras)."), []() { Lib::SwapShotWithNext(); }),
						MakeButton(LOCTEXT("PrevCut", "< Prev Cut"), LOCTEXT("PrevCutTip", "Jump to the previous cut (Alt+Up)."), []() { Lib::GoToCut(false); }),
						MakeButton(LOCTEXT("NextCut", "Next Cut >"), LOCTEXT("NextCutTip", "Jump to the next cut (Alt+Down)."), []() { Lib::GoToCut(true); }),
					})
				})
			]

			// --- Transitions ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("Transitions", "Transitions"), {
					MakeLabeled(LOCTEXT("Type", "Type"), MakeEnumPicker<ECinemafyTransition>(&TransitionType)),
					MakeLabeled(LOCTEXT("Direction", "Wipe"), MakeEnumPicker<ECinemafyWipeDirection>(&WipeDirection)),
					MakeLabeled(LOCTEXT("Length", "Seconds"),
						SNew(SSpinBox<float>).MinValue(0.1f).MaxValue(4.f).Delta(0.05f)
						.Value_Lambda([this]() { return TransitionSeconds; })
						.OnValueChanged_Lambda([this](float V) { TransitionSeconds = V; })),
					Wrap({
						MakeButton(LOCTEXT("AddTransition", "Add at Nearest Cut"), LOCTEXT("AddTransitionTip", "Put this transition on the cut nearest the playhead (Ctrl+Alt+D). Fades show everywhere; the rest play in game."),
							[this]() { Lib::AddTransitionAtNearestCut(TransitionType, TransitionSeconds, WipeDirection); }),
						MakeButton(LOCTEXT("RemoveTransition", "Remove"), LOCTEXT("RemoveTransitionTip", "Take transitions off the nearest cut."),
							[]() { Lib::RemoveTransitionsAtNearestCut(); }),
					})
				})
			]

			// --- Titles ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("Titles", "Titles"), {
					MakeLabeled(LOCTEXT("Style", "Style"), MakeEnumPicker<ECinemafyTitleStyle>(&Title.Style)),
					MakeLabeled(LOCTEXT("TitleText", "Title"),
						SNew(SEditableTextBox).HintText(LOCTEXT("TitleHint", "Tarrik Vane / Dagobah / Chapter One"))
						.OnTextChanged_Lambda([this](const FText& T) { Title.Title = T; })),
					MakeLabeled(LOCTEXT("SubText", "Subtitle"),
						SNew(SEditableTextBox).HintText(LOCTEXT("SubHint", "Role / Day / intro line for a crawl"))
						.OnTextChanged_Lambda([this](const FText& T) { Title.Subtitle = T; })),
					MakeLabeled(LOCTEXT("BodyText", "Crawl"),
						SNew(SBox).MinDesiredHeight(60.f)
						[
							SNew(SMultiLineEditableTextBox).HintText(LOCTEXT("BodyHint", "Paragraphs for the opening crawl"))
							.OnTextChanged_Lambda([this](const FText& T) { Title.Body = T; })
						]),
					MakeLabeled(LOCTEXT("TitleSeconds", "Seconds"),
						SNew(SSpinBox<float>).MinValue(0.5f).MaxValue(120.f).Delta(0.5f)
						.Value_Lambda([this]() { return Title.Duration; })
						.OnValueChanged_Lambda([this](float V) { Title.Duration = V; })),
					MakeButton(LOCTEXT("AddTitle", "Add Title at Playhead"), LOCTEXT("AddTitleTip", "Titles show in play and in game (crawls look best over a starfield)."),
						[this]() { Lib::AddTitleAtPlayhead(Title); })
				})
			]

			// --- Audio ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("Audio", "Audio"), {
					MakeLabeled(LOCTEXT("Duck", "Duck dB"),
						SNew(SSpinBox<float>).MinValue(-30.f).MaxValue(0.f).Delta(1.f)
						.Value_Lambda([this]() { return DuckDecibels; })
						.OnValueChanged_Lambda([this](float V) { DuckDecibels = V; })),
					MakeLabeled(LOCTEXT("SplitFrames", "J/L frames"),
						SNew(SSpinBox<int32>).MinValue(1).MaxValue(48)
						.Value_Lambda([this]() { return SplitFrames; })
						.OnValueChanged_Lambda([this](int32 V) { SplitFrames = V; })),
					Wrap({
						MakeButton(LOCTEXT("DuckMusic", "Duck Music Under Dialogue"), LOCTEXT("DuckMusicTip", "Dip the music under every line, with smooth ramps."),
							[this]() { Lib::DuckMusicUnderDialogue(DuckDecibels, 0.3f); }),
						MakeButton(LOCTEXT("JCut", "J-Cut"), LOCTEXT("JCutTip", "Picture cuts later: the next line's sound leads in under the old shot."),
							[this]() { Lib::RollNearestCut(SplitFrames); }),
						MakeButton(LOCTEXT("LCut", "L-Cut"), LOCTEXT("LCutTip", "Picture cuts earlier: the line's sound carries over the next shot."),
							[this]() { Lib::RollNearestCut(-SplitFrames); }),
					})
				})
			]

			// --- Markers ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("Markers", "Markers & Music"), {
					MakeLabeled(LOCTEXT("MarkerLabel", "Label"),
						SNew(SEditableTextBox).OnTextChanged_Lambda([this](const FText& T) { MarkerLabel = T.ToString(); })),
					Wrap({
						MakeButton(LOCTEXT("AddMarker", "Add Marker"), LOCTEXT("AddMarkerTip", "Mark the playhead."), [this]() { Lib::AddMarkerAtPlayhead(MarkerLabel); }),
						MakeButton(LOCTEXT("BeatMarkers", "Beat Markers"), LOCTEXT("BeatMarkersTip", "Hear the music's beat and mark every beat (bars labeled)."), []() { Lib::AddMusicBeatMarkers(); }),
						MakeButton(LOCTEXT("SnapCuts", "Snap Cuts to Markers"), LOCTEXT("SnapCutsTip", "Roll every cut within 6 frames of a marker onto it."), []() { Lib::SnapCutsToMarkers(6); }),
					})
				})
			]

			// --- Grade ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("Grade", "Grade"), {
					Wrap({
						MakeButton(LOCTEXT("CopyGrade", "Copy Grade"), LOCTEXT("CopyGradeTip", "From the selected camera, or the shot under the playhead (focus stays per shot)."), []() { Lib::CopyGrade(); }),
						MakeButton(LOCTEXT("PasteGrade", "Paste to Selected"), LOCTEXT("PasteGradeTip", "Onto the selected cameras."), []() { Lib::PasteGradeToSelection(); }),
						MakeButton(LOCTEXT("GradeAll", "Apply to All Shots"), LOCTEXT("GradeAllTip", "Match every camera in the sequence."), []() { Lib::ApplyGradeToAllShots(); }),
					})
				})
			]

			// --- Multicam ---
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				MakeSection(LOCTEXT("MulticamSection", "Multicam"), {
					MakeButton(LOCTEXT("OpenMulticam", "Open Multicam Switcher"), LOCTEXT("OpenMulticamTip", "Live camera grid: press play and tap 1-9 to cut."),
						[]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyMulticam::TabName); })
				})
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
