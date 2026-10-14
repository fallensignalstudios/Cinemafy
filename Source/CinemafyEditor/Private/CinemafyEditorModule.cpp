#include "CinemafyEditorModule.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyTypes.h"
#include "SCinemafyShotBrowser.h"
#include "SCinemafyHub.h"
#include "CinemafySceneDirector.h"
#include "CinemafyChoreography.h"
#include "CinemafyChoreoTools.h"
#include "CinemafyEditCommands.h"
#include "CinemafyEditLibrary.h"
#include "SCinemafyEditPanel.h"
#include "SCinemafyMulticam.h"
#include "ISequencerModule.h"
#include "Misc/MessageDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "CinemafyEditor"

namespace
{
	/** Adds one menu entry per (non-hidden) enum value. */
	template <typename TEnum, typename TFunc>
	void AddEnumEntries(FToolMenuSection& Section, TFunc&& OnPicked)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}

			const TEnum Value = static_cast<TEnum>(Enum->GetValueByIndex(Index));
			Section.AddMenuEntry(
				FName(*Enum->GetNameStringByIndex(Index)),
				Enum->GetDisplayNameTextByIndex(Index),
				Enum->GetToolTipTextByIndex(Index),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Value, OnPicked]() { OnPicked(Value); })));
		}
	}

	void FillShotsMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Shots", LOCTEXT("ShotsSection", "Select subject, then counterpart"));
		AddEnumEntries<ECinemafyShotType>(Section, [](ECinemafyShotType Type) { UCinemafyEditorLibrary::CreateShotFromSelection(Type, false); });

		FToolMenuSection& Flipped = Menu->AddSection("ShotsFlipped", LOCTEXT("ShotsFlippedSection", "Other side of the line"));
		Flipped.AddSubMenu("Flipped", LOCTEXT("Flipped", "Flipped Side"), LOCTEXT("FlippedTip", "Same shots, from the other side of the line of action."),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				FToolMenuSection& S = SubMenu->AddSection("Shots");
				AddEnumEntries<ECinemafyShotType>(S, [](ECinemafyShotType Type) { UCinemafyEditorLibrary::CreateShotFromSelection(Type, true); });
			}));
	}

	void FillLensMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Lens", LOCTEXT("LensSection", "Prime (f/2.8)"));
		AddEnumEntries<ECinemafyPrime>(Section, [](ECinemafyPrime Prime) { UCinemafyEditorLibrary::ApplyPrimeToSelection(Prime, 2.8f); });
	}

	void FillFeelMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Feel", LOCTEXT("FeelSection", "Camera Feel"));
		AddEnumEntries<ECinemafyCameraFeel>(Section, [](ECinemafyCameraFeel Feel) { UCinemafyEditorLibrary::ApplyCameraFeelToSelection(Feel); });
	}

	void FillMoveMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Move", LOCTEXT("MoveSection", "Key Progress 0 -> 1 in Sequencer"));
		AddEnumEntries<ECinemafyCameraMove>(Section, [](ECinemafyCameraMove Move) { UCinemafyEditorLibrary::ApplyCameraMoveToSelection(Move); });
	}

	void FillLightMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Light", LOCTEXT("LightSection", "Select a character (+ camera), or Light Rigs to restyle"));
		AddEnumEntries<ECinemafyLightingStyle>(Section, [](ECinemafyLightingStyle Style) { UCinemafyEditorLibrary::ApplyLightingStyleToSelection(Style); });
	}

	void FillListenMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Listen", LOCTEXT("ListenSection", "Select characters (two = a conversation)"));
		AddEnumEntries<ECinemafyTemperament>(Section, [](ECinemafyTemperament Temperament) { UCinemafyEditorLibrary::AddListeningToSelection(Temperament); });
	}

	void FillFaceMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Face", LOCTEXT("FaceSection", "Adds Cinemafy Face if needed (needs the Cinemafy Face anim node)"));
		AddEnumEntries<ECinemafyEmotion>(Section, [](ECinemafyEmotion Emotion) { UCinemafyEditorLibrary::ApplyFaceEmotionToSelection(Emotion); });
	}

	void FillDirectorMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Director", LOCTEXT("DirectorSection", "Select the characters in the scene"));
		AddEnumEntries<ECinemafyDirectorPreset>(Section, [](ECinemafyDirectorPreset Style) { UCinemafyEditorLibrary::CreateSceneDirectorFromSelection(Style); });
	}

	void FillLookMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Look", LOCTEXT("LookSection", "Look"));
		AddEnumEntries<ECinemafyLook>(Section, [](ECinemafyLook Look) { UCinemafyEditorLibrary::ApplyLookToSelection(Look, 1.f); });

		FToolMenuSection& Letterbox = Menu->AddSection("Letterbox", LOCTEXT("LetterboxSection", "Letterbox"));
		Letterbox.AddMenuEntry("Scope", LOCTEXT("Scope", "2.39:1 Scope"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetLetterboxOnSelection(2.39f); })));
		Letterbox.AddMenuEntry("Flat", LOCTEXT("Flat", "1.85:1 Flat"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetLetterboxOnSelection(1.85f); })));
		Letterbox.AddMenuEntry("NoLetterbox", LOCTEXT("NoLetterbox", "None (16:9)"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetLetterboxOnSelection(0.f); })));
	}
}

void FCinemafyEditorModule::StartupModule()
{
	// The Scene Director lives in the runtime module; its Direct / Clear buttons are answered here.
	DirectorRequestHandle = ACinemafySceneDirector::OnEditorRequest.AddLambda([](ACinemafySceneDirector* Director, ECinemafyDirectorRequest Request)
	{
		switch (Request)
		{
		case ECinemafyDirectorRequest::Direct:     UCinemafyEditorLibrary::DirectSceneInSequencer(Director); break;
		case ECinemafyDirectorRequest::Clear:      UCinemafyEditorLibrary::ClearDirectedShots(Director); break;
		case ECinemafyDirectorRequest::LearnStyle: UCinemafyEditorLibrary::LearnDirectorStyleFromSequence(Director); break;
		case ECinemafyDirectorRequest::SaveStyleAsset: UCinemafyEditorLibrary::SaveDirectorStyleAsAsset(Director); break;
		case ECinemafyDirectorRequest::ImportScript:   UCinemafyEditorLibrary::ImportDirectorScript(Director, Director->ScriptFile.FilePath); break;
		case ECinemafyDirectorRequest::AssignPerformances:
		{
			const int32 Filled = UCinemafyEditorLibrary::AssignDirectorPerformances(Director, Director->PerformanceFolder.Path);
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("CinemafyEditor", "AssignedTakes",
				"Assigned {0} take(s) to the script. Hit Direct to cut the scene with them."), Filled));
			break;
		}
		case ECinemafyDirectorRequest::AnalyzeAudio:
		{
			const int32 Count = UCinemafyEditorLibrary::AnalyzeDirectorAudio(Director);
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("CinemafyEditor", "AnalyzedLines",
				"Listened to {0} line(s). Durations and Audio Intensity are updated on each line (see the Output Log for details)."), Count));
			break;
		}
		}
	});

	// Choreography buttons, and the take recorder that listens for performances in play-in-editor.
	ChoreoRequestHandle = ACinemafyChoreography::OnEditorRequest.AddLambda([](ACinemafyChoreography* Choreography, ECinemafyChoreoRequest Request)
	{
		switch (Request)
		{
		case ECinemafyChoreoRequest::Choreograph:      FCinemafyChoreoMode::Start(Choreography); break;
		case ECinemafyChoreoRequest::PerformAndRecord: CinemafyChoreoTake::PerformAndRecord(Choreography); break;
		case ECinemafyChoreoRequest::DirectShots:      CinemafyChoreoTake::DirectShots(Choreography); break;
		case ECinemafyChoreoRequest::SaveProfile:      UCinemafyEditorLibrary::SaveWiringAsProfile(Choreography); break;
		case ECinemafyChoreoRequest::CheckWiring:
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(UCinemafyEditorLibrary::GetWiringReport(Choreography)));
			break;
		}
	});
	CinemafyChoreoTake::Initialize();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SCinemafyShotBrowser::TabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SCinemafyShotBrowser)
				];
		}))
		.SetDisplayName(LOCTEXT("ShotBrowserTab", "Cinemafy Shot Browser"))
		.SetTooltipText(LOCTEXT("ShotBrowserTabTip", "Film strip of every camera cut in the open Level Sequence."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SCinemafyHub::TabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SCinemafyHub)
				];
		}))
		.SetDisplayName(LOCTEXT("HubTab", "Cinemafy"))
		.SetTooltipText(LOCTEXT("HubTabTip", "Every Cinemafy tool in one panel."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SCinemafyEditPanel::TabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SCinemafyEditPanel)
				];
		}))
		.SetDisplayName(LOCTEXT("EditTab", "Cinemafy Edit"))
		.SetTooltipText(LOCTEXT("EditTabTip", "Video-editor tools for the open Level Sequence."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SCinemafyMulticam::TabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SCinemafyMulticam)
				];
		}))
		.SetDisplayName(LOCTEXT("MulticamTab", "Cinemafy Multicam"))
		.SetTooltipText(LOCTEXT("MulticamTabTip", "Live camera grid: play and tap 1-9 to cut."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Premiere-style shortcuts while Sequencer has focus.
	FCinemafyEditCommands::Register();
	// Command bindings belong to each Sequencer instance, so bind as each one opens.
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
	SequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateLambda([](TSharedRef<ISequencer> Sequencer)
	{
		if (TSharedPtr<FUICommandList> SequencerCommands = Sequencer->GetCommandBindings(ESequencerCommandBindings::Sequencer))
		{
			FCinemafyEditCommands::BindTo(SequencerCommands.ToSharedRef());
		}
	}));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCinemafyEditorModule::RegisterMenus));
}

void FCinemafyEditorModule::ShutdownModule()
{
	ACinemafySceneDirector::OnEditorRequest.Remove(DirectorRequestHandle);
	ACinemafyChoreography::OnEditorRequest.Remove(ChoreoRequestHandle);
	FCinemafyChoreoMode::Stop();
	CinemafyChoreoTake::Shutdown();

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SCinemafyShotBrowser::TabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SCinemafyHub::TabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SCinemafyEditPanel::TabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SCinemafyMulticam::TabName);
	}
	if (ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>(TEXT("Sequencer")))
	{
		SequencerModule->UnregisterOnSequencerCreated(SequencerCreatedHandle);
	}
	FCinemafyEditCommands::Unregister();
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FCinemafyEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = ToolsMenu->AddSection("Cinemafy", LOCTEXT("CinemafySection", "Cinemafy"));

	Section.AddMenuEntry("CinemafyHub", LOCTEXT("HubMenu", "Cinemafy Hub"),
		LOCTEXT("HubMenuTip", "Open one panel with every Cinemafy tool."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyHub::TabName); })));

	Section.AddMenuEntry("CinemafyEdit", LOCTEXT("EditMenu", "Cinemafy Edit"),
		LOCTEXT("EditMenuTip", "Video-editor tools for the open sequence: razor, ripple, roll, transitions, titles, ducking, markers, grade."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyEditPanel::TabName); })));

	Section.AddMenuEntry("CinemafyMulticam", LOCTEXT("MulticamMenu", "Multicam Switcher"),
		LOCTEXT("MulticamMenuTip", "A live grid of your cameras: play the sequence and tap 1-9 to cut."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyMulticam::TabName); })));

	// A Cinemafy Edit dropdown on Sequencer's own toolbar.
	if (UToolMenu* SequencerToolbar = UToolMenus::Get()->ExtendMenu("Sequencer.MainToolBar"))
	{
		FToolMenuSection& EditSection = SequencerToolbar->FindOrAddSection("CinemafyEdit");
		EditSection.AddEntry(FToolMenuEntry::InitComboButton("CinemafyEditCombo", FUIAction(),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* Menu)
			{
				using Lib = UCinemafyEditLibrary;
				FToolMenuSection& Edit = Menu->AddSection("Edit", LOCTEXT("ComboEdit", "Edit at Playhead"));
				// Explicit actions (the toolbar has no command list of ours), labeled with the shortcut.
				auto AddCommand = [&Edit](const TSharedPtr<FUICommandInfo>& Info, TFunction<void()> Run)
				{
					const FText Chord = Info->GetFirstValidChord()->GetInputText();
					Edit.AddMenuEntry(Info->GetCommandName(), FText::Format(LOCTEXT("ComboEntry", "{0}  ({1})"), Info->GetLabel(), Chord),
						Info->GetDescription(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([Run]() { Run(); })));
				};
				const FCinemafyEditCommands& Commands = FCinemafyEditCommands::Get();
				AddCommand(Commands.Razor, []() { Lib::RazorAtPlayhead(false); });
				AddCommand(Commands.RazorAllTracks, []() { Lib::RazorAtPlayhead(true); });
				AddCommand(Commands.RippleDelete, []() { Lib::RippleDeleteShotAtPlayhead(); });
				AddCommand(Commands.RippleTrimStart, []() { Lib::RippleTrimStartToPlayhead(); });
				AddCommand(Commands.RippleTrimEnd, []() { Lib::RippleTrimEndToPlayhead(); });
				AddCommand(Commands.RollEarlier, []() { Lib::RollNearestCut(-1); });
				AddCommand(Commands.RollLater, []() { Lib::RollNearestCut(1); });
				AddCommand(Commands.PreviousCut, []() { Lib::GoToCut(false); });
				AddCommand(Commands.NextCut, []() { Lib::GoToCut(true); });
				FToolMenuSection& More = Menu->AddSection("More", LOCTEXT("ComboMore", "More"));
				More.AddMenuEntry("Wipe", LOCTEXT("ComboWipe", "Wipe Nearest Cut"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() { Lib::AddTransitionAtNearestCut(ECinemafyTransition::Wipe, 0.7f, ECinemafyWipeDirection::Alternate); })));
				More.AddMenuEntry("Dissolve", LOCTEXT("ComboDissolve", "Dissolve Nearest Cut"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() { Lib::AddTransitionAtNearestCut(ECinemafyTransition::Dissolve, 0.8f, ECinemafyWipeDirection::LeftToRight); })));
				More.AddMenuEntry("Fade", LOCTEXT("ComboFade", "Fade Through Black"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() { Lib::AddTransitionAtNearestCut(ECinemafyTransition::FadeThroughBlack, 1.f, ECinemafyWipeDirection::LeftToRight); })));
				More.AddMenuEntry("Duck", LOCTEXT("ComboDuck", "Duck Music Under Dialogue"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() { Lib::DuckMusicUnderDialogue(-10.f, 0.3f); })));
				More.AddMenuEntry("OpenEdit", LOCTEXT("ComboOpen", "Open Cinemafy Edit..."), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyEditPanel::TabName); })));
				More.AddMenuEntry("OpenMulticam", LOCTEXT("ComboMulticam", "Multicam Switcher..."), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyMulticam::TabName); })));
			}),
			LOCTEXT("EditCombo", "Cinemafy Edit"),
			LOCTEXT("EditComboTip", "Premiere-style edits, transitions and audio tools."),
			FSlateIcon()));
	}

	Section.AddMenuEntry("CinemafyShotBrowser", LOCTEXT("ShotBrowserMenu", "Shot Browser"),
		LOCTEXT("ShotBrowserMenuTip", "Open a film strip of every camera cut in the open Level Sequence."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyShotBrowser::TabName); })));

	Section.AddMenuEntry("CinemafyChoreograph", LOCTEXT("ChoreographMenu", "Choreograph Character"),
		LOCTEXT("ChoreographMenuTip", "Select a character, then click points in the level and type what they do there: walk, vault, sprint, climb, sword attack, taunt. Perform & Record bakes it into Sequencer."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::ChoreographSelection(); })));

	Section.AddMenuEntry("CinemafyMoveLibrary", LOCTEXT("MoveLibraryMenu", "Build Move Library from /Game"),
		LOCTEXT("MoveLibraryMenuTip", "Collect every attack, gesture and reaction animation in the project into a Move Library asset you can curate."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::CreateMoveLibraryFromFolder(TEXT("/Game")); })));

	Section.AddSubMenu("CinemafyDirector", LOCTEXT("DirectorMenu", "Scene Director"),
		LOCTEXT("DirectorMenuTip", "Create an AI director for the selected characters: write lines, pick a style, hit Direct."),
		FNewToolMenuDelegate::CreateStatic(&FillDirectorMenu));

	Section.AddSubMenu("CinemafyShots", LOCTEXT("ShotsMenu", "Create Shot"),
		LOCTEXT("ShotsMenuTip", "Spawn a framed, eye-focused camera. Select the subject first, then who they're talking to."),
		FNewToolMenuDelegate::CreateStatic(&FillShotsMenu));

	Section.AddSubMenu("CinemafyLens", LOCTEXT("LensMenu", "Lens Kit"),
		LOCTEXT("LensMenuTip", "Put a cinema prime on the selected cameras."),
		FNewToolMenuDelegate::CreateStatic(&FillLensMenu));

	Section.AddSubMenu("CinemafyFeel", LOCTEXT("FeelMenu", "Camera Feel"),
		LOCTEXT("FeelMenuTip", "Give the selected cameras an operator's touch."),
		FNewToolMenuDelegate::CreateStatic(&FillFeelMenu));

	Section.AddSubMenu("CinemafyMove", LOCTEXT("MoveMenu", "Camera Move"),
		LOCTEXT("MoveMenuTip", "Give the selected cameras an eased operator move: push, truck, pedestal, crane or orbit."),
		FNewToolMenuDelegate::CreateStatic(&FillMoveMenu));

	Section.AddSubMenu("CinemafyLight", LOCTEXT("LightMenu", "Light Rig"),
		LOCTEXT("LightMenuTip", "Light the selected character for the selected camera: key, fill, rim and eye light that follow them."),
		FNewToolMenuDelegate::CreateStatic(&FillLightMenu));

	Section.AddSubMenu("CinemafyLook", LOCTEXT("LookMenu", "Look & Letterbox"),
		LOCTEXT("LookMenuTip", "Grade and frame the selected cameras."),
		FNewToolMenuDelegate::CreateStatic(&FillLookMenu));

	Section.AddMenuEntry("CinemafyAddToSequence", LOCTEXT("AddToSequenceMenu", "Add Cameras to Sequence"),
		LOCTEXT("AddToSequenceMenuTip", "Add the selected cameras to the open Level Sequence and cut to them at the playhead."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::AddSelectedCamerasToSequence(); })));

	Section.AddMenuEntry("CinemafyGameplayBlend", LOCTEXT("GameplayBlendMenu", "Set Up Gameplay Blend"),
		LOCTEXT("GameplayBlendMenuTip", "Make the open sequence blend in from and back out to the gameplay camera (1s each), easing character animation too. Play it with the Cinemafy Cutscene subsystem."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetUpGameplayBlendForOpenSequence(1.f, 1.f, true); })));

	Section.AddMenuEntry("CinemafyContinuity", LOCTEXT("ContinuityMenu", "Check Sequence Continuity"),
		LOCTEXT("ContinuityMenuTip", "Review every cut in the open sequence: line crossings, jump cuts, flash cuts, overlong shots."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::CheckSequenceContinuity(); })));

	Section.AddMenuEntry("CinemafyLineCheck", LOCTEXT("LineCheckMenu", "Check 180-Degree Line"),
		LOCTEXT("LineCheckMenuTip", "Select two characters and your cameras. Flags cameras across the line of action."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::CheckLineOfActionForSelection(); })));

	Section.AddSubMenu("CinemafyListen", LOCTEXT("ListenMenu", "Add Listening"),
		LOCTEXT("ListenMenuTip", "Characters react while others talk: nods in pauses, blinks on emphasis, thinking glances. Pick a temperament."),
		FNewToolMenuDelegate::CreateStatic(&FillListenMenu));

	Section.AddSubMenu("CinemafyFace", LOCTEXT("FaceMenu", "Face Emotion"),
		LOCTEXT("FaceMenuTip", "Give the selected characters a Cinemafy Face (performance control, cleanup, dynamics) and an emotion."),
		FNewToolMenuDelegate::CreateStatic(&FillFaceMenu));

	Section.AddMenuEntry("CinemafyGaze", LOCTEXT("GazeMenu", "Add Gaze to Characters"),
		LOCTEXT("GazeMenuTip", "Add living eyes (saccades, head follow, blinks) to the selected characters."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::AddGazeToSelection(); })));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCinemafyEditorModule, CinemafyEditor)
