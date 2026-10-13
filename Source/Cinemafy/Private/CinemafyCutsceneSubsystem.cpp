#include "CinemafyCutsceneSubsystem.h"
#include "CinemafyModule.h"
#include "SCinemafyLetterbox.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

namespace
{
	/** Pretty much how the camera blend eases, so the bars and the move feel like one gesture. */
	float SmoothAlpha(float X)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f));
	}

	APlayerController* GetFirstPlayerController(const UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

bool UCinemafyCutsceneSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCinemafyCutsceneSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCinemafyCutsceneSubsystem, STATGROUP_Tickables);
}

void UCinemafyCutsceneSubsystem::Deinitialize()
{
	Cleanup();
	Super::Deinitialize();
}

bool UCinemafyCutsceneSubsystem::GetSequenceBlendTimes(const ULevelSequence* Sequence, float& OutBlendIn, float& OutBlendOut)
{
	OutBlendIn = 0.f;
	OutBlendOut = 0.f;

	const UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	const UMovieSceneCameraCutTrack* CutTrack = MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	if (!CutTrack || !CutTrack->bCanBlend)
	{
		return false;
	}

	const UMovieSceneSection* First = nullptr;
	const UMovieSceneSection* Last = nullptr;
	for (const UMovieSceneSection* Section : CutTrack->GetAllSections())
	{
		if (!Section || !Section->HasStartFrame())
		{
			continue;
		}
		if (!First || Section->GetInclusiveStartFrame() < First->GetInclusiveStartFrame())
		{
			First = Section;
		}
		if (!Last || Section->GetInclusiveStartFrame() > Last->GetInclusiveStartFrame())
		{
			Last = Section;
		}
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	if (First)
	{
		OutBlendIn = static_cast<float>(TickResolution.AsSeconds(FFrameTime(First->Easing.GetEaseInDuration())));
	}
	if (Last)
	{
		OutBlendOut = static_cast<float>(TickResolution.AsSeconds(FFrameTime(Last->Easing.GetEaseOutDuration())));
	}
	return true;
}

FTransform UCinemafyCutsceneSubsystem::ResolveAnchor(const FCinemafyCutsceneSettings& Settings) const
{
	if (Settings.Anchor == ECinemafyCutsceneAnchor::AnchorActor && Settings.AnchorActor)
	{
		return Settings.AnchorActor->GetActorTransform();
	}

	if (Settings.Anchor == ECinemafyCutsceneAnchor::Player)
	{
		const APlayerController* PC = GetFirstPlayerController(GetWorld());
		if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			// The player's feet, facing where they face (yaw only, so the scene stays level).
			const FVector Feet = Pawn->GetActorLocation() - FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight());
			return FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Feet);
		}
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: no player pawn to anchor the cutscene to; playing as authored."));
	}

	return FTransform::Identity;
}

ULevelSequencePlayer* UCinemafyCutsceneSubsystem::PlayCutscene(ULevelSequence* Sequence, const FCinemafyCutsceneSettings& Settings)
{
	UWorld* World = GetWorld();
	if (!Sequence || !World)
	{
		return nullptr;
	}

	StopCutscene();

	FMovieSceneSequencePlaybackSettings Playback;
	Playback.bDisableMovementInput = Settings.bDisableMovementInput;
	Playback.bDisableLookAtInput = Settings.bDisableLookInput;
	Playback.bHidePlayer = Settings.bHidePlayer;
	Playback.bHideHud = Settings.bHideHUD;

	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, Playback, SequenceActor);
	if (!Player || !SequenceActor)
	{
		return nullptr;
	}

	if (Settings.Anchor != ECinemafyCutsceneAnchor::World)
	{
		SequenceActor->bOverrideInstanceData = true;
		if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
		{
			InstanceData->TransformOrigin = ResolveAnchor(Settings);
		}
	}

	ActivePlayer = Player;
	ActiveActor = SequenceActor;
	ActiveSequence = Sequence;
	ActiveSettings = Settings;
	bAligned = false;

	if (!Settings.bUseSequenceBlendTimes || !GetSequenceBlendTimes(Sequence, BlendIn, BlendOut))
	{
		BlendIn = Settings.BlendInTime;
		BlendOut = Settings.BlendOutTime;
	}

	if (Settings.bAnimatedLetterbox)
	{
		AddLetterbox(Settings.LetterboxAspectRatio);
	}

	Player->OnFinished.AddDynamic(this, &UCinemafyCutsceneSubsystem::HandleFinished);
	Player->Play();

	OnCutsceneStarted.Broadcast(Sequence);
	return Player;
}

void UCinemafyCutsceneSubsystem::SkipCutscene()
{
	if (!ActivePlayer)
	{
		return;
	}

	const float Now = static_cast<float>(ActivePlayer->GetCurrentTime().AsSeconds());
	const float BlendOutStart = static_cast<float>(ActivePlayer->GetEndTime().AsSeconds()) - BlendOut;
	if (Now < BlendOutStart)
	{
		ActivePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(BlendOutStart, EUpdatePositionMethod::Jump));
	}
}

void UCinemafyCutsceneSubsystem::StopCutscene()
{
	if (ActivePlayer)
	{
		ActivePlayer->Stop();
		// Stop() may already have fired OnFinished; only clean up what's still around.
		if (ActivePlayer)
		{
			HandleFinished();
		}
	}
}

bool UCinemafyCutsceneSubsystem::IsCutscenePlaying() const
{
	return ActivePlayer && ActivePlayer->IsPlaying();
}

void UCinemafyCutsceneSubsystem::Tick(float DeltaTime)
{
	if (!ActivePlayer)
	{
		return;
	}

	const float Start = static_cast<float>(ActivePlayer->GetStartTime().AsSeconds());
	const float End = static_cast<float>(ActivePlayer->GetEndTime().AsSeconds());
	const float Now = static_cast<float>(ActivePlayer->GetCurrentTime().AsSeconds());

	if (Letterbox.IsValid())
	{
		const float In = BlendIn > 0.f ? SmoothAlpha((Now - Start) / BlendIn) : 1.f;
		const float Out = BlendOut > 0.f ? SmoothAlpha((End - Now) / BlendOut) : 1.f;
		Letterbox->SetAlpha(FMath::Min(In, Out));
	}

	if (ActiveSettings.bAlignPlayerToFinalCamera && !bAligned && Now >= End - BlendOut)
	{
		AlignPlayerToCurrentCamera();
		bAligned = true;
	}
}

void UCinemafyCutsceneSubsystem::AlignPlayerToCurrentCamera() const
{
	APlayerController* PC = GetFirstPlayerController(GetWorld());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	// The camera manager is looking through the cutscene camera right now. Hand its heading to gameplay.
	const FRotator CutsceneView = PC->PlayerCameraManager->GetCameraRotation();
	PC->SetControlRotation(FRotator(FMath::Clamp(CutsceneView.Pitch * 0.5f, -30.f, 30.f), CutsceneView.Yaw, 0.f));
}

void UCinemafyCutsceneSubsystem::AddLetterbox(float AspectRatio)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}

	RemoveLetterbox();
	Letterbox = SNew(SCinemafyLetterbox).AspectRatio(AspectRatio);
	// High Z-order so the bars sit over gameplay UI, like real bars would.
	Viewport->AddViewportWidgetContent(Letterbox.ToSharedRef(), 100);
}

void UCinemafyCutsceneSubsystem::RemoveLetterbox()
{
	if (!Letterbox.IsValid())
	{
		return;
	}

	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Letterbox.ToSharedRef());
	}
	Letterbox.Reset();
}

void UCinemafyCutsceneSubsystem::HandleFinished()
{
	ULevelSequence* FinishedSequence = ActiveSequence;
	Cleanup();
	if (FinishedSequence)
	{
		OnCutsceneFinished.Broadcast(FinishedSequence);
	}
}

void UCinemafyCutsceneSubsystem::Cleanup()
{
	RemoveLetterbox();

	if (ActivePlayer)
	{
		ActivePlayer->OnFinished.RemoveDynamic(this, &UCinemafyCutsceneSubsystem::HandleFinished);
	}
	if (ActiveActor)
	{
		// We can get here from inside the player's own OnFinished, so don't destroy it mid-callback.
		ActiveActor->SetLifeSpan(0.1f);
	}

	ActivePlayer = nullptr;
	ActiveActor = nullptr;
	ActiveSequence = nullptr;
}
