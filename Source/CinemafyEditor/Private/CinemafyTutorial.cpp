#include "CinemafyTutorial.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafyProduction.h"
#include "CinemafyScript.h"
#include "CinemafyStoryLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "GameFramework/Character.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "LevelSequence.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditorViewport.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyTutorial"

namespace CinemafyTutorialState
{
	const TCHAR* Section = TEXT("Cinemafy");
	TWeakPtr<SWindow> Window;

	bool GetFlag(const TCHAR* Key)
	{
		bool bValue = false;
		return GConfig && GConfig->GetBool(Section, Key, bValue, GEditorPerProjectIni) && bValue;
	}

	void SetFlag(const TCHAR* Key, bool bValue)
	{
		if (GConfig)
		{
			GConfig->SetBool(Section, Key, bValue, GEditorPerProjectIni);
			GConfig->Flush(false, GEditorPerProjectIni);
		}
	}

	/** The script the tutorial's buttons act on: the Story Board's, else any in the project. */
	UCinemafyScriptProject* CurrentProject()
	{
		FString Remembered;
		if (GConfig && GConfig->GetString(Section, TEXT("StoryBoardProject"), Remembered, GEditorPerProjectIni) && !Remembered.IsEmpty())
		{
			if (UCinemafyScriptProject* Project = Cast<UCinemafyScriptProject>(FSoftObjectPath(Remembered).TryLoad()))
			{
				return Project;
			}
		}
		const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UCinemafyScriptProject::StaticClass()->GetClassPathName(), Assets);
		return Assets.Num() > 0 ? Cast<UCinemafyScriptProject>(Assets[0].GetAsset()) : nullptr;
	}

	/** Runs Action on the current script, or says how to get one. */
	void WithProject(TFunction<void(UCinemafyScriptProject*)> Action)
	{
		if (UCinemafyScriptProject* Project = CurrentProject())
		{
			Action(Project);
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoScript", "Import a script first (or try the sample story on the first page)."));
		}
	}

	struct FAction
	{
		FText Label;
		FText Tip;
		TFunction<void()> Do;
	};

	struct FStep
	{
		FText Title;
		FText Body;
		TArray<FAction> Actions;
	};

	TArray<FStep> Steps()
	{
		TArray<FStep> Out;
		Out.Add(FStep{ LOCTEXT("S1T", "Welcome to Cinemafy"),
			LOCTEXT("S1B", "Cinemafy turns your writing into scenes. You bring the script; it breaks the story down, puts your characters on set, stages the action, voices the lines, shoots coverage and cuts it together. Then you refine whatever you like.\n\nStart with your own script (Fountain, Final Draft, or plain text, including game scripts with \"NAME: line\"), or try the short sample story."),
			{
				{ LOCTEXT("Sample", "Try the sample story"), LOCTEXT("SampleTip", "Imports a three-scene bridge confrontation and opens it on the Story Board."),
					[]() { UCinemafyTutorialLibrary::ImportSampleScript(); } },
				{ LOCTEXT("Import", "Import my script"), LOCTEXT("ImportTip", "Pick a .fountain, .fdx or .txt file."),
					[]() { if (UCinemafyScriptProject* P = UCinemafyEditorLibrary::ImportScriptWithDialog()) { UCinemafyStoryLibrary::OpenStoryBoard(P); } } },
			} });
		Out.Add(FStep{ LOCTEXT("S2T", "Your story, scene by scene"),
			LOCTEXT("S2B", "The Story Board shows every scene as a card, grouped into acts and chapters. If your script doesn't mark them, Cinemafy places act breaks on the story's turns and groups chapters by time and place (hover a heading to see why).\n\nCard colors show progress: grey not started, blue blocking, purple recorded, green directed, gold final. Hover a card for its synopsis, what each character carries in from earlier scenes (injuries, props, costumes), and continuity warnings."),
			{
				{ LOCTEXT("Board", "Open the Story Board"), FText::GetEmpty(), []() { UCinemafyStoryLibrary::OpenStoryBoard(CurrentProject()); } },
			} });
		Out.Add(FStep{ LOCTEXT("S3T", "Cast and sets"),
			LOCTEXT("S3B", "Tell Cinemafy who plays whom and where scenes happen. Cast & Sets lists every character and location from the script: point each character at a character in your level (or a Blueprint to spawn), pick a scratch voice, and add looks for continuity (a pistol prop, a bloodied material, a limp).\n\nThen place Cinemafy Marks on your set, named the way the script talks: \"console\", \"lift\", \"window\". When the script says \"Kade walks to the console\", that's where she goes. No set yet? Build the sample set."),
			{
				{ LOCTEXT("Cast", "Fill Cast & Sets"), FText::GetEmpty(), []() { WithProject([](UCinemafyScriptProject* P)
					{
						UCinemafyStoryLibrary::BuildCastAndSets(P);
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(P);
					}); } },
				{ LOCTEXT("Mark", "Place a mark here"), LOCTEXT("MarkTip", "Drops a mark where the viewport is looking; rename it in the Details panel."),
					[]() { UCinemafyStoryLibrary::PlaceMark(TEXT("mark"), FString()); } },
				{ LOCTEXT("SampleSet", "Build the sample set"), LOCTEXT("SampleSetTip", "A floor, a crate and marks for the sample story, in front of the viewport."),
					[]() { WithProject([](UCinemafyScriptProject* P) { FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(UCinemafyTutorialLibrary::BuildSampleSet(P))); }); } },
			} });
		Out.Add(FStep{ LOCTEXT("S4T", "Hear it"),
			LOCTEXT("S4B", "Scratch Voices reads every line aloud with your computer's voices, so timing, pacing and lip movement work before anyone records. Characters with a Cinemafy Face talk along.\n\nWhen real voice-over arrives, name the files by line id (from the paperwork) and Assign Recorded Voices swaps them in; re-direct and the cut re-times to the performances."),
			{
				{ LOCTEXT("Scratch", "Generate scratch voices"), FText::GetEmpty(), []() { WithProject([](UCinemafyScriptProject* P)
					{
						const int32 Voiced = UCinemafyStoryLibrary::GenerateScratchVoices(P, FString(), false);
						FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("Voiced", "Voiced {0} line(s)."), Voiced));
					}); } },
			} });
		Out.Add(FStep{ LOCTEXT("S5T", "Stage a scene"),
			LOCTEXT("S5B", "Previs on a card stages that scene straight from the script: the cast steps onto their marks dressed for the scene, walks, vaults, draws and strikes as written, and says their lines on cue. Cinemafy records the take, picks a camera style for the scene's mood, shoots coverage and cuts it.\n\nThe report says what it staged and what it couldn't (a missing mark, a character not on set)."),
			{
				{ LOCTEXT("PrevisFirst", "Previs the first scene"), FText::GetEmpty(), []() { WithProject([](UCinemafyScriptProject* P)
					{
						const TArray<FCinemafyScriptScene*> Scenes = P->GetAllScenes();
						if (Scenes.Num() > 0)
						{
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(UCinemafyStoryLibrary::PrevisScene(P, Scenes[0]->Id, true).Left(3000)));
						}
					}); } },
			} });
		Out.Add(FStep{ LOCTEXT("S6T", "Make it yours"),
			LOCTEXT("S6B", "Everything Cinemafy made is ordinary Unreal. Choreographies hold each character's beats: click a beat to change it, or Choreograph to add more (\"sword attack\", \"vault\", walk \"a line in quotes\"). Perform & Record re-shoots. The Scene Director re-cuts in another style with one click. Cinemafy Edit (in Sequencer) trims, ripples and adds transitions and titles. New Take keeps the old version beside the new one."),
			{
				{ LOCTEXT("Hub", "Open the Cinemafy Hub"), FText::GetEmpty(), []() { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("CinemafyHub"))); } },
			} });
		Out.Add(FStep{ LOCTEXT("S7T", "Finish and keep track"),
			LOCTEXT("S7B", "Cut Together builds a master sequence per act and one for the whole story. Dailies renders scenes for review; add review notes on the cards. Paperwork writes voice-over scripts per actor, subtitles, a localization table, shot lists and a storyboard. The Checklist tracks progress and continuity.\n\nMaking a game? Choices in the script (\"* Spare him -> MED BAY\") branch the story; the Story Player runs it in game, with the player's own character in their scenes, and Scene Triggers start scenes when the player walks in.\n\nThis tutorial lives in the Hub whenever you want it back."),
			{
				{ LOCTEXT("Together", "Cut it together"), FText::GetEmpty(), []() { WithProject([](UCinemafyScriptProject* P)
					{
						if (ULevelSequence* Film = UCinemafyStoryLibrary::BuildMasterSequences(P))
						{
							GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Film);
						}
					}); } },
				{ LOCTEXT("Paper", "Write the paperwork"), FText::GetEmpty(), []() { WithProject([](UCinemafyScriptProject* P)
					{
						FPlatformProcess::ExploreFolder(*UCinemafyStoryLibrary::ExportPaperwork(P));
					}); } },
			} });
		return Out;
	}
}

/** The tutorial pages. */
class SCinemafyTutorial : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyTutorial) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Steps = CinemafyTutorialState::Steps();
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(18.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("StepOf", "Step {0} of {1}"), Index + 1, Steps.Num()); })
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 10.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return Steps[Index].Title; })
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
					.ColorAndOpacity(FLinearColor(1.f, 0.62f, 0.15f))
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return Steps[Index].Body; })
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f)
				[
					SAssignNew(ActionRow, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return CinemafyTutorialState::GetFlag(TEXT("bTutorialDone")) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState State) { UCinemafyTutorialLibrary::SetTutorialDone(State != ECheckBoxState::Checked); })
						[
							SNew(STextBlock).Text(LOCTEXT("ShowAgain", "Show this when the project opens"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Back", "Back"))
						.IsEnabled_Lambda([this]() { return Index > 0; })
						.OnClicked_Lambda([this]() { Go(Index - 1); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text_Lambda([this]() { return Index + 1 < Steps.Num() ? LOCTEXT("Next", "Next") : LOCTEXT("Finish", "Finish"); })
						.OnClicked_Lambda([this]()
						{
							if (Index + 1 < Steps.Num())
							{
								Go(Index + 1);
							}
							else
							{
								UCinemafyTutorialLibrary::SetTutorialDone(true);
								UCinemafyTutorialLibrary::CloseTutorial();
							}
							return FReply::Handled();
						})
					]
				]
			]
		];
		Go(0);
	}

private:
	void Go(int32 NewIndex)
	{
		Index = FMath::Clamp(NewIndex, 0, Steps.Num() - 1);
		ActionRow->ClearChildren();
		for (const CinemafyTutorialState::FAction& Action : Steps[Index].Actions)
		{
			TFunction<void()> Do = Action.Do;
			ActionRow->AddSlot()
			[
				SNew(SButton)
				.Text(Action.Label)
				.ToolTipText(Action.Tip)
				.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
			];
		}
	}

	TArray<CinemafyTutorialState::FStep> Steps;
	TSharedPtr<SWrapBox> ActionRow;
	int32 Index = 0;
};

void UCinemafyTutorialLibrary::ShowTutorial()
{
	if (TSharedPtr<SWindow> Existing = CinemafyTutorialState::Window.Pin())
	{
		Existing->BringToFront();
		return;
	}
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("Title", "Cinemafy: from script to screen"))
		.ClientSize(FVector2D(620.f, 470.f))
		.SupportsMaximize(false)
		.SupportsMinimize(true)
		[
			SNew(SCinemafyTutorial)
		];
	CinemafyTutorialState::Window = Window;
	FSlateApplication::Get().AddWindow(Window);
}

void UCinemafyTutorialLibrary::CloseTutorial()
{
	if (TSharedPtr<SWindow> Window = CinemafyTutorialState::Window.Pin())
	{
		Window->RequestDestroyWindow();
	}
	CinemafyTutorialState::Window.Reset();
}

void UCinemafyTutorialLibrary::NoteAutomationSession()
{
	if (!CinemafyTutorialState::GetFlag(TEXT("bAutomationDetected")))
	{
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: automation detected (MCP tool call); the tutorial won't open on its own in this project."));
		CinemafyTutorialState::SetFlag(TEXT("bAutomationDetected"), true);
	}
	CloseTutorial();
}

void UCinemafyTutorialLibrary::SetTutorialDone(bool bDone)
{
	CinemafyTutorialState::SetFlag(TEXT("bTutorialDone"), bDone);
}

bool UCinemafyTutorialLibrary::ShouldAutoShow()
{
	if (CinemafyTutorialState::GetFlag(TEXT("bTutorialDone")) || CinemafyTutorialState::GetFlag(TEXT("bAutomationDetected")))
	{
		return false;
	}
	if (FApp::IsUnattended() || IsRunningCommandlet() || GIsAutomationTesting || !FApp::CanEverRender()
		|| FParse::Param(FCommandLine::Get(), TEXT("NoCinemafyTutorial")) || !FPlatformMisc::GetEnvironmentVariable(TEXT("CINEMAFY_NO_TUTORIAL")).IsEmpty())
	{
		return false;
	}
	return true;
}

void UCinemafyTutorialLibrary::ScheduleAutoShow()
{
	auto Arm = []()
	{
		// A moment after the editor is up, so an agent that connects right away is seen first.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			if (ShouldAutoShow() && GEditor && !GEditor->PlayWorld)
			{
				ShowTutorial();
			}
			return false;
		}), 8.f);
	};
	if (GEngine && GEngine->IsInitialized())
	{
		Arm();
	}
	else
	{
		FCoreDelegates::GetOnPostEngineInit().AddLambda(Arm);
	}
}

UCinemafyScriptProject* UCinemafyTutorialLibrary::ImportSampleScript()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Cinemafy"));
	const FString Path = Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Docs") / TEXT("samples") / TEXT("TheBridge.fountain") : FString();
	UCinemafyScriptProject* Project = Path.IsEmpty() ? nullptr : UCinemafyEditorLibrary::ImportScriptFile(FPaths::ConvertRelativePathToFull(Path), TEXT("TheBridge"));
	if (Project)
	{
		UCinemafyStoryLibrary::OpenStoryBoard(Project);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSample", "The sample script wasn't found in the plugin's Docs/samples folder."));
	}
	return Project;
}

FString UCinemafyTutorialLibrary::BuildSampleSet(UCinemafyScriptProject* Project)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !Project)
	{
		return TEXT("Open a level and import the sample script first.");
	}
	TArray<FString> Report;
	const FScopedTransaction Transaction(LOCTEXT("SampleSet", "Cinemafy: Sample Set"));

	// Where the viewport looks.
	FVector Origin = FVector::ZeroVector;
	if (FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient)
	{
		FHitResult Hit;
		const FVector Eye = Client->GetViewLocation();
		const FVector Forward = Client->GetViewRotation().Vector();
		Origin = World->LineTraceSingleByChannel(Hit, Eye, Eye + Forward * 5000.f, ECC_Visibility) ? Hit.ImpactPoint : Eye + Forward * 1500.f;
	}

	auto Shape = [World](const TCHAR* Mesh, const FVector& Location, const FVector& Scale, const FString& Label) -> AStaticMeshActor*
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		if (Actor)
		{
			Actor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
			Actor->SetActorScale3D(Scale);
			Actor->SetActorLabel(Label);
		}
		return Actor;
	};
	Shape(TEXT("/Engine/BasicShapes/Plane.Plane"), Origin, FVector(30.f, 30.f, 1.f), TEXT("Sample_Floor"));
	Shape(TEXT("/Engine/BasicShapes/Cube.Cube"), Origin + FVector(500.f, 0.f, 50.f), FVector(0.8f, 1.6f, 1.f), TEXT("cargo crate"));
	Report.Add(TEXT("Built a floor and a cargo crate."));

	struct FMarkSpec { const TCHAR* Name; FVector Offset; bool bStart; };
	const FMarkSpec Marks[] = {
		{ TEXT("lift"), FVector(-900.f, -200.f, 0.f), false },
		{ TEXT("console"), FVector(300.f, 450.f, 0.f), false },
		{ TEXT("corridor"), FVector(1100.f, -700.f, 0.f), false },
		{ TEXT("start"), FVector(-800.f, -350.f, 0.f), true },
		{ TEXT("start"), FVector(200.f, 250.f, 0.f), true },
	};
	for (const FMarkSpec& Spec : Marks)
	{
		ACinemafyMark* Mark = World->SpawnActor<ACinemafyMark>(Origin + Spec.Offset, FRotator::ZeroRotator);
		if (Mark)
		{
			Mark->MarkName = Spec.Name;
			Mark->bStartMark = Spec.bStart;
			Mark->SetActorLabel(FString::Printf(TEXT("Mark_%s"), Spec.Name));
			Mark->RerunConstructionScripts();
		}
	}
	Report.Add(TEXT("Placed marks: lift, console, corridor, and two start marks."));

	// A character to play the cast, from the usual templates.
	static const TCHAR* Candidates[] = {
		TEXT("/Game/Blueprints/CBP_SandboxCharacter.CBP_SandboxCharacter_C"),
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"),
		TEXT("/Game/ThirdPersonBP/Blueprints/ThirdPersonCharacter.ThirdPersonCharacter_C"),
		TEXT("/Game/Characters/Mannequins/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C") };
	UClass* Found = nullptr;
	for (const TCHAR* Candidate : Candidates)
	{
		Found = LoadClass<ACharacter>(nullptr, Candidate, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Found)
		{
			break;
		}
	}
	UCinemafyStoryLibrary::BuildCastAndSets(Project);
	Project->Modify();
	int32 Cast = 0;
	for (FCinemafyCastMember& Member : Project->CastMembers)
	{
		if (Member.CharacterClass.IsNull() && Found)
		{
			Member.CharacterClass = TSoftClassPtr<ACharacter>(Found);
			TArray<FString> Words;
			Member.ScriptName.ParseIntoArrayWS(Words);
			const FString Last = Words.Num() > 0 ? Words.Last().ToLower() : FString(TEXT("extra"));
			Member.ActorLabel = Last.Left(1).ToUpper() + Last.Mid(1);
			++Cast;
		}
	}
	Report.Add(Found ? FString::Printf(TEXT("Cast %d character(s) as %s."), Cast, *Found->GetName())
		: FString(TEXT("No template character found: set a Character Class for each cast member under Cast & Sets (a GASP or Third Person character works best).")));
	Report.Add(TEXT("Now Previs scene 1.1.1 on the Story Board. For walking around things, add a Nav Mesh Bounds Volume over the floor."));
	return FString::Join(Report, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
