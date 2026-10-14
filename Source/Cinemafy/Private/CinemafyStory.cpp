#include "CinemafyStory.h"
#include "CinemafyCutsceneSubsystem.h"
#include "CinemafyModule.h"
#include "SCinemafyChoices.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"

namespace CinemafyStoryFlow
{
	const FCinemafyScriptScene* Find(const UCinemafyScriptProject* Project, const FString& SceneId)
	{
		if (!Project)
		{
			return nullptr;
		}
		for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
		{
			if (Scene->Id == SceneId || FString::FromInt(Scene->Number) == SceneId)
			{
				return Scene;
			}
		}
		return nullptr;
	}

	/** The next scene in script order that can play (not cut, its conditions met); "END" if none. */
	FString NextInOrder(const UCinemafyScriptProject* Project, const FString& SceneId, const TSet<FName>& Flags)
	{
		bool bAfter = false;
		for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
		{
			if (bAfter && Scene->Status != ECinemafySceneStatus::Cut && UCinemafyScriptLibrary::FlagsHold(Scene->RequireFlags, Flags))
			{
				return Scene->Id;
			}
			bAfter |= Scene->Id == SceneId;
		}
		return TEXT("END");
	}

	/** -1 = no pending choice. Set by the choice widget, handled on the next tick (the widget is gone by then). */
	int32 PendingChoice = -1;
}

FString UCinemafyStorySubsystem::NextSceneAfter(const UCinemafyScriptProject* InProject, const FString& SceneId, const TSet<FName>& InFlags, TArray<FCinemafyScriptChoice>& OutChoices)
{
	OutChoices.Reset();
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(InProject, SceneId);
	if (!Scene)
	{
		return TEXT("END");
	}
	for (const FCinemafyScriptChoice& Choice : Scene->Choices)
	{
		if (UCinemafyScriptLibrary::FlagsHold(Choice.RequireFlags, InFlags))
		{
			OutChoices.Add(Choice);
		}
	}
	if (OutChoices.Num() > 0)
	{
		return FString();
	}
	if (!Scene->DivertSceneId.IsEmpty())
	{
		return Scene->DivertSceneId;
	}
	return CinemafyStoryFlow::NextInOrder(InProject, Scene->Id, InFlags);
}

bool UCinemafyStorySubsystem::PlayScene(UCinemafyScriptProject* InProject, const FString& SceneId, bool bInFollowStory)
{
	int32 Insert = 0;
	const FString BaseId = UCinemafyScriptLibrary::SplitInsertId(SceneId, Insert);
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(InProject, BaseId);
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!Scene || !World)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Story: no scene %s to play."), *SceneId);
		return false;
	}
	// Mid-bridge, "<scene>#<n>" (a Scene Trigger at the next stretch of play) continues it.
	if (bAwaitingGameplay && Scene->Id == CurrentSceneId && Insert > CurrentInsert)
	{
		bAwaitingGameplay = false;
		return StartPresentation(*Scene, Insert);
	}
	HideChoices();
	Project = InProject;
	bFollowStory = bInFollowStory;
	CurrentSceneId = Scene->Id;
	bAwaitingChoice = false;
	bAwaitingGameplay = false;

	// A scene whose conditions don't hold is passed over.
	if (!UCinemafyScriptLibrary::FlagsHold(Scene->RequireFlags, Flags))
	{
		Advance();
		return true;
	}

	ApplyFlags(Scene->SetFlags);
	ScenesSeen.AddUnique(Scene->Id);
	OnSceneStarted.Broadcast(Scene->Id);
	return StartPresentation(*Scene, Scene->Inserts.Num() > 0 ? FMath::Clamp(Insert, 1, Scene->Inserts.Num()) : 0);
}

bool UCinemafyStorySubsystem::StartPresentation(const FCinemafyScriptScene& Scene, int32 Insert)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	CurrentInsert = Insert;
	ULevelSequence* Sequence = Scene.Inserts.IsValidIndex(Insert - 1) ? Scene.Inserts[Insert - 1].Sequence.LoadSynchronous() : Scene.Sequence.LoadSynchronous();
	UCinemafyCutsceneSubsystem* Cutscenes = World ? World->GetSubsystem<UCinemafyCutsceneSubsystem>() : nullptr;
	if (!Sequence || !Cutscenes)
	{
		if (Insert > 0)
		{
			// An insert with no sequence yet plays as nothing: the bridge carries on to its next stretch of play.
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Story: %s insert %d has no sequence yet."), *Scene.Id, Insert);
			PresentationDone();
			return false;
		}
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Story: scene %s has no sequence yet; passing over it."), *Scene.Id);
		OnSceneUnavailable.Broadcast(Scene.Id);
		bPlaying = false;
		CommitScene();
		return false;
	}

	Cutscenes->OnCutsceneFinished.AddUniqueDynamic(this, &UCinemafyStorySubsystem::HandleCutsceneFinished);
	PlayingSequence = Sequence;
	bPlaying = true;
	ULevelSequencePlayer* Player = Cutscenes->PlayCutscene(Sequence, CutsceneSettings);
	if (Player)
	{
		BindPlayer(Player, Scene);
	}
	return Player != nullptr;
}

void UCinemafyStorySubsystem::ContinueScene()
{
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(Project, CurrentSceneId);
	if (!bAwaitingGameplay || !Scene)
	{
		return;
	}
	bAwaitingGameplay = false;
	StartPresentation(*Scene, CurrentInsert + 1);
}

void UCinemafyStorySubsystem::PresentationDone()
{
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(Project, CurrentSceneId);
	if (Scene && CurrentInsert > 0 && CurrentInsert < Scene->Inserts.Num())
	{
		// Back to play until the next insert.
		bAwaitingGameplay = true;
		const FString Who = !Scene->Game.ControlBefore.IsEmpty() && !Scene->Game.ControlBefore.Equals(TEXT("None"), ESearchCase::IgnoreCase)
			? Scene->Game.ControlBefore : Scene->Game.ControlAfter;
		OnControlReturned.Broadcast(Scene->Id, CurrentInsert + 1, Who);
		return;
	}
	CommitScene();
}

void UCinemafyStorySubsystem::CommitScene()
{
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(Project, CurrentSceneId);
	CurrentInsert = 0;
	bAwaitingGameplay = false;
	if (Scene && Scene->bFixedId && bCommitSceneFlags)
	{
		SetFlag(FName(*Scene->Id), true);
	}
	OnSceneFinished.Broadcast(CurrentSceneId);
	if (Scene && (!Scene->Game.ControlBefore.IsEmpty() || !Scene->Game.ControlAfter.IsEmpty()))
	{
		OnControlHandoff.Broadcast(Scene->Id, Scene->Game.ControlBefore, Scene->Game.ControlAfter);
	}
	if (bFollowStory)
	{
		Advance();
	}
}

void UCinemafyStorySubsystem::BindPlayer(ULevelSequencePlayer* Player, const FCinemafyScriptScene& Scene)
{
	const UCinemafyScriptProject* P = Project;
	// Who the player is controlling going in plays in the scene; else the cast member marked Is Player.
	const FString Controlled = Scene.Game.ControlBefore;
	const FCinemafyCastMember* Hero = nullptr;
	if (P && bBindControlledCharacter && !Controlled.IsEmpty() && !Controlled.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		Hero = P->CastMembers.FindByPredicate([&Controlled](const FCinemafyCastMember& M)
		{
			return M.ScriptName.Contains(Controlled, ESearchCase::IgnoreCase) || Controlled.Contains(M.ScriptName, ESearchCase::IgnoreCase);
		});
	}
	if (!Hero && P)
	{
		Hero = P->CastMembers.FindByPredicate([](const FCinemafyCastMember& M) { return M.bIsPlayer; });
	}
	APlayerController* Controller = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	ALevelSequenceActor* SequenceActor = Player ? Cast<ALevelSequenceActor>(Player->GetOuter()) : nullptr;
	const ULevelSequence* Sequence = PlayingSequence.Get();
	if (!Hero || !Pawn || !SequenceActor || !Sequence)
	{
		return;
	}
	// The sequence was shot with a stand-in labelled like the cast entry: the player's own pawn takes its place.
	const FString Label = Hero->ActorLabel.IsEmpty() ? Hero->ScriptName : Hero->ActorLabel;
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(Index);
		if (!Possessable.GetParent().IsValid() && Possessable.GetName().Equals(Label, ESearchCase::IgnoreCase))
		{
			SequenceActor->SetBinding(UE::MovieScene::FRelativeObjectBindingID(Possessable.GetGuid()), { Pawn }, false);
			UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Story: the player plays %s in scene %s."), *Hero->ScriptName, *Scene.Id);
			return;
		}
	}
}

void UCinemafyStorySubsystem::HandleCutsceneFinished(ULevelSequence* Sequence)
{
	if (!bPlaying || Sequence != PlayingSequence.Get())
	{
		return;
	}
	bPlaying = false;
	PresentationDone();
}

void UCinemafyStorySubsystem::Advance()
{
	const FString Next = NextSceneAfter(Project, CurrentSceneId, Flags, OfferedChoices);
	if (OfferedChoices.Num() > 0)
	{
		bAwaitingChoice = true;
		OnChoices.Broadcast(CurrentSceneId, OfferedChoices);
		if (bBuiltInChoiceUI)
		{
			ShowChoices();
		}
		return;
	}
	if (Next.IsEmpty() || Next == TEXT("END"))
	{
		OnStoryFinished.Broadcast();
		return;
	}
	PlayScene(Project, Next, true);
}

void UCinemafyStorySubsystem::Choose(int32 ChoiceIndex)
{
	if (!bAwaitingChoice || !OfferedChoices.IsValidIndex(ChoiceIndex))
	{
		return;
	}
	const FCinemafyScriptChoice Choice = OfferedChoices[ChoiceIndex];
	HideChoices();
	bAwaitingChoice = false;
	ApplyFlags(Choice.SetFlags);
	const FString Target = Choice.TargetSceneId.IsEmpty() ? CinemafyStoryFlow::NextInOrder(Project, CurrentSceneId, Flags) : Choice.TargetSceneId;
	if (Target == TEXT("END"))
	{
		OnStoryFinished.Broadcast();
		return;
	}
	PlayScene(Project, Target, true);
}

void UCinemafyStorySubsystem::SkipScene()
{
	// Some presentations can't be skipped (the bible says so); their state commits as they play.
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(Project, CurrentSceneId);
	if (Scene && !Scene->Game.bSkippable)
	{
		return;
	}
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (UCinemafyCutsceneSubsystem* Cutscenes = World ? World->GetSubsystem<UCinemafyCutsceneSubsystem>() : nullptr)
	{
		Cutscenes->SkipCutscene();
	}
}

void UCinemafyStorySubsystem::StopStory()
{
	bFollowStory = false;
	bAwaitingChoice = false;
	bAwaitingGameplay = false;
	CurrentInsert = 0;
	HideChoices();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (UCinemafyCutsceneSubsystem* Cutscenes = World ? World->GetSubsystem<UCinemafyCutsceneSubsystem>() : nullptr)
	{
		Cutscenes->StopCutscene();
	}
	bPlaying = false;
}

void UCinemafyStorySubsystem::SetFlag(FName Flag, bool bSet)
{
	if (bSet)
	{
		Flags.Add(Flag);
	}
	else
	{
		Flags.Remove(Flag);
	}
}

void UCinemafyStorySubsystem::ApplyFlags(const TArray<FString>& Changes)
{
	for (const FString& Change : Changes)
	{
		const bool bClear = Change.StartsWith(TEXT("!"));
		SetFlag(FName(*(bClear ? Change.Mid(1) : Change).TrimStartAndEnd()), !bClear);
	}
}

void UCinemafyStorySubsystem::ShowChoices()
{
	HideChoices();
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		return;
	}
	TArray<FText> Texts;
	for (const FCinemafyScriptChoice& Choice : OfferedChoices)
	{
		Texts.Add(FText::FromString(Choice.Text));
	}
	CinemafyStoryFlow::PendingChoice = -1;
	ChoiceWidget = SNew(SCinemafyChoices)
		.Choices(Texts)
		.OnChosen([](int32 Index) { CinemafyStoryFlow::PendingChoice = Index; });
	Viewport->AddViewportWidgetContent(ChoiceWidget.ToSharedRef(), 50);
	FSlateApplication::Get().SetKeyboardFocus(ChoiceWidget);
	if (APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController())
	{
		Controller->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(ChoiceWidget);
		Controller->SetInputMode(Mode);
	}
}

void UCinemafyStorySubsystem::HideChoices()
{
	if (!ChoiceWidget.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(ChoiceWidget.ToSharedRef());
	}
	ChoiceWidget.Reset();
	if (APlayerController* Controller = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
	{
		Controller->SetShowMouseCursor(false);
		Controller->SetInputMode(FInputModeGameOnly());
	}
}

void UCinemafyStorySubsystem::Tick(float DeltaTime)
{
	if (bAwaitingChoice && CinemafyStoryFlow::PendingChoice >= 0)
	{
		const int32 Index = CinemafyStoryFlow::PendingChoice;
		CinemafyStoryFlow::PendingChoice = -1;
		Choose(Index);
		return;
	}
	APlayerController* Controller = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (bPlaying && bAllowSkip && Controller)
	{
		for (const FKey& Key : SkipKeys)
		{
			if (Controller->WasInputKeyJustPressed(Key))
			{
				SkipScene();
				break;
			}
		}
	}
}

TStatId UCinemafyStorySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCinemafyStorySubsystem, STATGROUP_Tickables);
}

void UCinemafyStorySubsystem::Deinitialize()
{
	HideChoices();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------------

ACinemafySceneTrigger::ACinemafySceneTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	Volume->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Volume;
}

void ACinemafySceneTrigger::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		Fire();
	}
}

void ACinemafySceneTrigger::Fire()
{
	UGameInstance* GameInstance = GetGameInstance();
	UCinemafyStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<UCinemafyStorySubsystem>() : nullptr;
	if (!Story || !Project || (bOnlyOnce && bFired))
	{
		return;
	}
	// The next stretch of a gameplay bridge reached: continue it. Anything else waits for the story to be free.
	int32 Insert = 0;
	const FString BaseId = UCinemafyScriptLibrary::SplitInsertId(SceneId, Insert);
	const bool bContinues = Insert > 0 && Story->IsAwaitingGameplay() && Story->GetCurrentSceneId() == BaseId;
	if (Story->IsPlaying() && !bContinues)
	{
		return;
	}
	if (!UCinemafyScriptLibrary::FlagsHold(RequireFlags, TSet<FName>(Story->GetFlags())))
	{
		return;
	}
	bFired = true;
	Story->PlayScene(Project, SceneId, bFollowStory);
}
