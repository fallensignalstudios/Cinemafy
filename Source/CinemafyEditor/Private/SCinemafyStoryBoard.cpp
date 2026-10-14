#include "SCinemafyStoryBoard.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafySceneDirector.h"
#include "CinemafyScript.h"
#include "CinemafyStoryLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "LevelSequence.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyStoryBoard"

const FName SCinemafyStoryBoard::TabName(TEXT("CinemafyStoryBoard"));
TWeakPtr<SCinemafyStoryBoard> SCinemafyStoryBoard::Open;

namespace CinemafyStoryBoardUI
{
	FLinearColor StatusColor(ECinemafySceneStatus Status)
	{
		switch (Status)
		{
		case ECinemafySceneStatus::Blocking: return FLinearColor(0.20f, 0.45f, 0.95f);
		case ECinemafySceneStatus::Recorded: return FLinearColor(0.60f, 0.35f, 0.90f);
		case ECinemafySceneStatus::Directed: return FLinearColor(0.25f, 0.75f, 0.35f);
		case ECinemafySceneStatus::Final:    return FLinearColor(0.95f, 0.75f, 0.20f);
		case ECinemafySceneStatus::Cut:      return FLinearColor(0.45f, 0.12f, 0.12f);
		default:                             return FLinearColor(0.35f, 0.35f, 0.35f);
		}
	}

	FString Clock(float Seconds)
	{
		const int32 Total = FMath::RoundToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	TSharedRef<SWidget> ToolButton(const FText& Label, const FText& Tip, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tip)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); });
	}

	ACinemafySceneDirector* FindDirector(const UCinemafyScriptProject* Project, const FString& SceneId)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
		{
			if (It->ScriptProject == Project && It->ScriptSceneId == SceneId)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

void SCinemafyStoryBoard::Construct(const FArguments& InArgs)
{
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);
	Open = SharedThis(this);

	FString Remembered;
	if (GConfig && GConfig->GetString(TEXT("Cinemafy"), TEXT("StoryBoardProject"), Remembered, GEditorPerProjectIni) && !Remembered.IsEmpty())
	{
		Project = Cast<UCinemafyScriptProject>(FSoftObjectPath(Remembered).TryLoad());
	}

	ChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SCinemafyStoryBoard::OnObjectChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			MakeToolbar()
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(Body, SVerticalBox)
			]
		]
	];
	Rebuild();
}

SCinemafyStoryBoard::~SCinemafyStoryBoard()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ChangedHandle);
}

void SCinemafyStoryBoard::SetProject(UCinemafyScriptProject* InProject)
{
	if (GConfig && InProject)
	{
		GConfig->SetString(TEXT("Cinemafy"), TEXT("StoryBoardProject"), *FSoftObjectPath(InProject).ToString(), GEditorPerProjectIni);
	}
	if (TSharedPtr<SCinemafyStoryBoard> Board = Open.Pin())
	{
		Board->Project = InProject;
		Board->Rebuild();
	}
}

void SCinemafyStoryBoard::OnObjectChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object && Object == Project.Get())
	{
		Rebuild();
	}
}

TSharedRef<SWidget> SCinemafyStoryBoard::MakeProjectPicker()
{
	return SNew(SComboButton)
		.ButtonContent()
		[
			SNew(STextBlock).Text_Lambda([this]()
			{
				return Project.IsValid() ? FText::FromString(Project->Title.IsEmpty() ? Project->GetName() : Project->Title) : LOCTEXT("PickProject", "Pick a script...");
			})
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FMenuBuilder Menu(true, nullptr);
			const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			TArray<FAssetData> Assets;
			Registry.GetAssetsByClass(UCinemafyScriptProject::StaticClass()->GetClassPathName(), Assets);
			for (const FAssetData& Asset : Assets)
			{
				Menu.AddMenuEntry(FText::FromName(Asset.AssetName), FText::FromString(Asset.GetObjectPathString()), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Asset]() { SetProject(Cast<UCinemafyScriptProject>(Asset.GetAsset())); })));
			}
			if (Assets.Num() == 0)
			{
				Menu.AddMenuEntry(LOCTEXT("NoProjects", "No scripts yet: Import Script"), FText::GetEmpty(), FSlateIcon(), FUIAction());
			}
			return Menu.MakeWidget();
		});
}

TSharedRef<SWidget> SCinemafyStoryBoard::MakeToolbar()
{
	using namespace CinemafyStoryBoardUI;
	auto WithProject = [this](TFunction<void(UCinemafyScriptProject*)> Action)
	{
		return [this, Action]()
		{
			if (UCinemafyScriptProject* P = Project.Get())
			{
				Action(P);
				Rebuild();
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NeedProject", "Import or pick a script first."));
			}
		};
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f))
			+ SWrapBox::Slot()[ MakeProjectPicker() ]
			+ SWrapBox::Slot()[ ToolButton(LOCTEXT("Import", "Import Script"), LOCTEXT("ImportTip", "Read a script into a new project."),
				[]() { SetProject(UCinemafyEditorLibrary::ImportScriptWithDialog()); }) ]
			+ SWrapBox::Slot()[ ToolButton(LOCTEXT("CastSets", "Cast & Sets"), LOCTEXT("CastSetsTip", "Fill the cast and sets from the script, match the level, and open the project to fill in characters, voices and looks."),
				WithProject([](UCinemafyScriptProject* P)
				{
					UCinemafyStoryLibrary::BuildCastAndSets(P);
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(P);
				})) ]
			+ SWrapBox::Slot()[ ToolButton(LOCTEXT("Map", "Map Scenes"), LOCTEXT("MapTip", "Find this level's Scene Directors and mark their scenes' progress."),
				WithProject([](UCinemafyScriptProject* P) { UCinemafyEditorLibrary::MapScriptScenesInLevel(P); })) ]
			+ SWrapBox::Slot()[ ToolButton(LOCTEXT("Masters", "Cut Together"), LOCTEXT("MastersTip", "Build a master sequence per act and for the whole story, and open it."),
				WithProject([](UCinemafyScriptProject* P)
				{
					if (ULevelSequence* Film = UCinemafyStoryLibrary::BuildMasterSequences(P))
					{
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Film);
					}
				})) ]
			+ SWrapBox::Slot()[ ToolButton(LOCTEXT("Checklist", "Checklist"), LOCTEXT("ChecklistTip", "Write the progress and continuity checklist and open it."),
				WithProject([](UCinemafyScriptProject* P)
				{
					const FString Path = UCinemafyEditorLibrary::ExportScriptChecklist(P);
					FPlatformProcess::LaunchFileInDefaultExternalApplication(*Path);
				})) ]
			+ SWrapBox::Slot()[ ToolButton(LOCTEXT("Refresh", "Refresh"), LOCTEXT("RefreshTip", "Redraw the board."), [this]() { Rebuild(); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 6.f, 2.f, 0.f)
		[
			SNew(STextBlock).Text_Lambda([this]()
			{
				const UCinemafyScriptProject* P = Project.Get();
				if (!P)
				{
					return LOCTEXT("Empty", "Import a script (Fountain, Final Draft or text) to see it here, scene by scene.");
				}
				const int32 Scenes = P->GetAllScenes().Num();
				return FText::Format(LOCTEXT("Progress", "{0} scenes, {1}% directed. Grey: not started | Blue: blocking | Purple: recorded | Green: directed | Gold: final"),
					Scenes, FMath::RoundToInt(P->GetProgress() * 100.f));
			})
		];
}

TSharedRef<SWidget> SCinemafyStoryBoard::MakeCard(const FCinemafyScriptScene& Scene)
{
	using namespace CinemafyStoryBoardUI;
	const FString SceneId = Scene.Id;
	TWeakObjectPtr<UCinemafyScriptProject> WeakProject = Project;
	auto Act = [this, WeakProject, SceneId](TFunction<void(UCinemafyScriptProject*, const FString&)> Action)
	{
		return [this, WeakProject, SceneId, Action]()
		{
			if (UCinemafyScriptProject* P = WeakProject.Get())
			{
				Action(P, SceneId);
				Rebuild();
			}
		};
	};

	// The tooltip: synopsis, carry-ins, warnings.
	FString Tip = Scene.Synopsis;
	for (const FCinemafyContinuityFact& Fact : Scene.CarriedIn)
	{
		Tip += FString::Printf(TEXT("\nCarry in: %s %s %s"), *Fact.Character, *Fact.Category.ToLower(), *Fact.Fact);
	}
	for (const FString& Warning : Scene.Warnings)
	{
		Tip += TEXT("\nWarning: ") + Warning;
	}

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(StatusColor(Scene.Status))
			.Padding(FMargin(0.f, 2.f))
		];

	if (!Scene.Sequence.IsNull())
	{
		const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		const FAssetData Asset = Registry.GetAssetByObjectPath(Scene.Sequence.ToSoftObjectPath());
		if (Asset.IsValid())
		{
			TSharedRef<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(Asset, 224, 126, ThumbnailPool);
			Card->AddSlot().AutoHeight().Padding(2.f)
			[
				SNew(SBox).WidthOverride(224.f).HeightOverride(126.f)
				[
					Thumbnail->MakeThumbnailWidget()
				]
			];
		}
	}

	const UEnum* StatusEnum = StaticEnum<ECinemafySceneStatus>();
	FString Meta = FString::Printf(TEXT("~%s | %s"), *Clock(Scene.EstimatedSeconds),
		*StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(Scene.Status)).ToString());
	if (Scene.Takes.Num() > 0)
	{
		Meta += FString::Printf(TEXT(" | %d take(s)"), Scene.Takes.Num() + 1);
	}
	if (Scene.bHasAction)
	{
		Meta += TEXT(" | action");
	}

	Card->AddSlot().AutoHeight().Padding(4.f, 4.f, 4.f, 0.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(TEXT("%s  %s"), *Scene.Id, *Scene.Heading)))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.AutoWrapText(true)
	];
	Card->AddSlot().AutoHeight().Padding(4.f, 2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Join(Scene.Characters, TEXT(", "))))
		.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
		.AutoWrapText(true)
	];
	Card->AddSlot().AutoHeight().Padding(4.f, 0.f)
	[
		SNew(STextBlock).Text(FText::FromString(Meta))
	];
	if (Scene.Warnings.Num() > 0)
	{
		Card->AddSlot().AutoHeight().Padding(4.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("Warnings", "{0} continuity warning(s)"), Scene.Warnings.Num()))
			.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.3f))
		];
	}

	TArray<TSoftObjectPtr<ULevelSequence>> Takes = Scene.Takes;
	Card->AddSlot().AutoHeight().Padding(2.f, 4.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			ToolButton(LOCTEXT("Previs", "Previs"), LOCTEXT("PrevisTip", "Stage this scene from the script and record a take."),
				Act([](UCinemafyScriptProject* P, const FString& Id)
				{
					const FString Report = UCinemafyStoryLibrary::PrevisScene(P, Id, true);
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report.Left(3000)));
				}))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			ToolButton(LOCTEXT("OpenScene", "Open"), LOCTEXT("OpenSceneTip", "Open the scene's sequence (made if it doesn't exist)."),
				Act([](UCinemafyScriptProject* P, const FString& Id) { UCinemafyStoryLibrary::OpenSceneSequence(P, Id); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			ToolButton(LOCTEXT("DirectScene", "Direct"), LOCTEXT("DirectSceneTip", "Cut the scene with its Scene Director (one is made from the script if needed)."),
				Act([](UCinemafyScriptProject* P, const FString& Id)
				{
					ACinemafySceneDirector* Director = FindDirector(P, Id);
					if (!Director)
					{
						Director = UCinemafyEditorLibrary::CreateDirectorForScene(P, Id);
					}
					if (Director)
					{
						UCinemafyStoryLibrary::OpenSceneSequence(P, Id);
						UCinemafyEditorLibrary::DirectSceneInSequencer(Director);
					}
				}))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SComboButton)
			.ButtonContent()[ SNew(STextBlock).Text(LOCTEXT("More", "...")) ]
			.OnGetMenuContent_Lambda([this, WeakProject, SceneId, Takes]()
			{
				FMenuBuilder Menu(true, nullptr);
				Menu.BeginSection("Status", LOCTEXT("StatusSection", "Status"));
				const UEnum* Enum = StaticEnum<ECinemafySceneStatus>();
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
				{
					const ECinemafySceneStatus Value = static_cast<ECinemafySceneStatus>(Enum->GetValueByIndex(Index));
					Menu.AddMenuEntry(Enum->GetDisplayNameTextByIndex(Index), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, WeakProject, SceneId, Value]()
					{
						UCinemafyEditorLibrary::SetScriptSceneStatus(WeakProject.Get(), SceneId, Value);
						Rebuild();
					})));
				}
				Menu.EndSection();
				Menu.BeginSection("Scene", LOCTEXT("SceneSection", "Scene"));
				auto Entry = [&Menu, this, WeakProject, SceneId](const FText& Label, TFunction<void(UCinemafyScriptProject*)> Action)
				{
					Menu.AddMenuEntry(Label, FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, WeakProject, Action]()
					{
						if (UCinemafyScriptProject* P = WeakProject.Get())
						{
							Action(P);
							Rebuild();
						}
					})));
				};
				Entry(LOCTEXT("StageOnly", "Stage without recording"), [SceneId](UCinemafyScriptProject* P) { UCinemafyStoryLibrary::PrevisScene(P, SceneId, false); });
				Entry(LOCTEXT("Continuity", "Dress the cast for this scene"), [SceneId](UCinemafyScriptProject* P) { UCinemafyStoryLibrary::ApplySceneContinuity(P, SceneId); });
				Entry(LOCTEXT("NewDirector", "New Scene Director from the script"), [SceneId](UCinemafyScriptProject* P) { UCinemafyEditorLibrary::CreateDirectorForScene(P, SceneId); });
				Entry(LOCTEXT("NewTake", "New take (keep this one)"), [SceneId](UCinemafyScriptProject* P) { UCinemafyStoryLibrary::NewTake(P, SceneId); });
				for (int32 Take = 0; Take < Takes.Num(); ++Take)
				{
					Entry(FText::Format(LOCTEXT("UseTake", "Use take: {0}"), FText::FromString(Takes[Take].GetAssetName())),
						[SceneId, Take](UCinemafyScriptProject* P) { UCinemafyStoryLibrary::UseTake(P, SceneId, Take); });
				}
				Entry(LOCTEXT("Earlier", "Move earlier"), [SceneId](UCinemafyScriptProject* P) { UCinemafyStoryLibrary::MoveScene(P, SceneId, -1); });
				Entry(LOCTEXT("Later", "Move later"), [SceneId](UCinemafyScriptProject* P) { UCinemafyStoryLibrary::MoveScene(P, SceneId, 1); });
				Menu.EndSection();
				return Menu.MakeWidget();
			})
		]
	];

	return SNew(SBox)
		.WidthOverride(232.f)
		.ToolTipText(FText::FromString(Tip))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2.f)
			[
				Card
			]
		];
}

void SCinemafyStoryBoard::Rebuild()
{
	if (!Body.IsValid())
	{
		return;
	}
	Body->ClearChildren();
	const UCinemafyScriptProject* P = Project.Get();
	if (!P)
	{
		return;
	}

	for (const FCinemafyScriptAct& Act : P->Acts)
	{
		int32 Done = 0, Count = 0;
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				Count += Scene.Status == ECinemafySceneStatus::Cut ? 0 : 1;
				Done += Scene.Status == ECinemafySceneStatus::Directed || Scene.Status == ECinemafySceneStatus::Final ? 1 : 0;
			}
		}
		Body->AddSlot().AutoHeight().Padding(6.f, 12.f, 6.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%s  %s   (%d/%d directed)"), *Act.Id, *Act.Title, Done, Count)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.ColorAndOpacity(FLinearColor(1.f, 0.62f, 0.15f))
			.ToolTipText(FText::FromString(Act.Why))
		];
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			Body->AddSlot().AutoHeight().Padding(10.f, 8.f, 6.f, 2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  %s%s"), *Chapter.Id, *Chapter.Title, Chapter.bInferred ? TEXT("  (grouped by Cinemafy)") : TEXT(""))))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ToolTipText(FText::FromString(Chapter.Why))
			];
			TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				Row->AddSlot()[ MakeCard(Scene) ];
			}
			Body->AddSlot().AutoHeight().Padding(10.f, 2.f)
			[
				Row
			];
		}
	}
}

#undef LOCTEXT_NAMESPACE
