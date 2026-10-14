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
	const FCinemafyScriptScene* Scene = CinemafyStoryFlow::Find(InProject, SceneId);
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!Scene || !World)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Story: no scene %s to play."), *SceneId);
		return false;
	}
	HideChoices();
	Project = InProject;
	bFollowStory = bInFollowStory;
	CurrentSceneId = Scene->Id;
	bAwaitingChoice = false;

	// A scene whose conditions don't hold is passed over.
	if (!UCinemafyScriptLibrary::FlagsHold(Scene->RequireFlags, Flags))
	{
		Advance();
		return true;
	}

	ApplyFlags(Scene->SetFlags);
	ScenesSeen.AddUnique(Scene->Id);

	ULevelSequence* Sequence = Scene->Sequence.LoadSynchronous();
	UCinemafyCutsceneSubsystem* Cutscenes = World->GetSubsystem<UCinemafyCutsceneSubsystem>();
	if (!Sequence || !Cutscenes)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Story: scene %s has no sequence yet; passing over it."), *Scene->Id);
		OnSceneUnavailable.Broadcast(Scene->Id);
		bPlaying = false;
		if (bFollowStory)
		{
			Advance();
		}
		return false;
	}

	Cutscenes->OnCutsceneFinished.AddUniqueDynamic(this, &UCinemafyStorySubsystem::HandleCutsceneFinished);
	PlayingSequence = Sequence;
	bPlaying = true;
	ULevelSequencePlayer* Player = Cutscenes->PlayCutscene(Sequence, CutsceneSettings);
	if (Player)
	{
		BindPlayer(Player, *Scene);
	}
	OnSceneStarted.Broadcast(Scene->Id);
	return Player != nullptr;
}

void UCinemafyStorySubsystem::BindPlayer(ULevelSequencePlayer* Player, const FCinemafyScriptScene& Scene)
{
	const UCinemafyScriptProject* P = Project;
	const FCinemafyCastMember* Hero = P ? P->CastMembers.FindByPredicate([](const FCinemafyCastMember& M) { return M.bIsPlayer; }) : nullptr;
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
	const UMovieScene* MovieScene = Sequence->GetMovieScene();
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
	OnSceneFinished.Broadcast(CurrentSceneId);
	if (bFollowStory)
	{
		Advance();
	}
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
	if (!Story || !Project || (bOnlyOnce && bFired) || Story->IsPlaying())
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
