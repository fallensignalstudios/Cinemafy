#include "CinemafySceneDirector.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyFaceComponent.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafyDirectorStyleAsset.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyFollowComponent.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyListenerComponent.h"
#include "CinemafyModule.h"
#include "CinemafySubtitleSubsystem.h"
#include "CinemafyTransitionSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
FCinemafyDirectorEditorRequest ACinemafySceneDirector::OnEditorRequest;
#endif

namespace
{
	FString ShotKey(const FCinemafyPlannedShot& Shot, bool bFlipSide)
	{
		return FString::Printf(TEXT("%d|%s|%s|%d"), static_cast<int32>(Shot.ShotType),
			Shot.Subject ? *Shot.Subject->GetName() : TEXT("-"),
			Shot.Counterpart ? *Shot.Counterpart->GetName() : TEXT("-"),
			bFlipSide ? 1 : 0);
	}

	template <typename TComponent>
	TComponent* FindOrAddRuntimeComponent(AActor* Actor)
	{
		if (TComponent* Existing = Actor->FindComponentByClass<TComponent>())
		{
			return Existing;
		}
		TComponent* Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

ACinemafySceneDirector::ACinemafySceneDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACinemafySceneDirector::ImportScriptFromCSV()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::ImportScript);
#endif
}

void ACinemafySceneDirector::AutoAssignPerformances()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::AssignPerformances);
#endif
}

void ACinemafySceneDirector::SaveStyleAsAsset()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::SaveStyleAsset);
#endif
}

FCinemafyDirectorStyle ACinemafySceneDirector::GetStyle() const
{
	if (StyleAsset)
	{
		return StyleAsset->Style;
	}
	return Style == ECinemafyDirectorPreset::Custom ? CustomStyle : UCinemafyDirectorLibrary::GetPresetStyle(Style);
}

FName ACinemafySceneDirector::GetDirectorTag() const
{
	return FName(*FString::Printf(TEXT("Cinemafy.Director.%s"), *GetName()));
}

void ACinemafySceneDirector::Direct()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::Direct);
#endif
}

void ACinemafySceneDirector::Reroll()
{
	Modify();
	Seed = FMath::Rand();
	Direct();
}

void ACinemafySceneDirector::ClearShots()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::Clear);
#endif
}

void ACinemafySceneDirector::AnalyzeDialogueAudio()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::AnalyzeAudio);
#endif
}

TArray<FCinemafyLine> ACinemafySceneDirector::GetEffectiveLines() const
{
	TArray<FCinemafyLine> Effective = Lines;
	for (FCinemafyLine& Line : Effective)
	{
		if (Line.bAutoIntensity && Line.AudioIntensity >= 0.f)
		{
			Line.Intensity = FMath::Lerp(Line.Intensity, Line.AudioIntensity, FMath::Clamp(AudioIntensityWeight, 0.f, 1.f));
		}
	}
	return Effective;
}

TArray<FCinemafyLine> ACinemafySceneDirector::GetDirectedLines() const
{
	TArray<FCinemafyLine> Directed = GatherDirectedLines();
	UCinemafyDirectorLibrary::ReadMoments(Directed);
	return Directed;
}

TArray<FCinemafyLine> ACinemafySceneDirector::GatherDirectedLines() const
{
	TArray<FCinemafyLine> Mine = GetEffectiveLines();
	if (!ParallelDirector || ParallelDirector == this)
	{
		return Mine;
	}

	TArray<FCinemafyLine> Theirs = ParallelDirector->GetEffectiveLines();
	for (FCinemafyLine& Line : Theirs)
	{
		Line.SceneId = 1;
	}

	// Interleave in blocks; the blocks shrink as we go, so the cutting accelerates into the climax.
	const int32 Block = FMath::Max(1, GetStyle().CrossCutLinesPerBlock);
	const int32 Total = Mine.Num() + Theirs.Num();
	TArray<FCinemafyLine> Merged;
	int32 MineIndex = 0;
	int32 TheirIndex = 0;
	bool bMyTurn = true;
	while (MineIndex < Mine.Num() || TheirIndex < Theirs.Num())
	{
		if ((bMyTurn && MineIndex >= Mine.Num()) || (!bMyTurn && TheirIndex >= Theirs.Num()))
		{
			bMyTurn = !bMyTurn;
		}

		const float Progress = static_cast<float>(Merged.Num()) / FMath::Max(Total, 1);
		const int32 BlockSize = bAccelerateCrossCutting ? FMath::Max(1, FMath::RoundToInt(Block * (1.f - 0.7f * Progress))) : Block;

		TArray<FCinemafyLine>& Source = bMyTurn ? Mine : Theirs;
		int32& SourceIndex = bMyTurn ? MineIndex : TheirIndex;
		for (int32 Taken = 0; Taken < BlockSize && SourceIndex < Source.Num(); ++Taken)
		{
			FCinemafyLine Line = Source[SourceIndex++];
			if (Taken == 0 && Merged.Num() > 0)
			{
				// Every switch between the scenes is a scene break.
				Line.bSceneBreak = true;
			}
			Merged.Add(Line);
		}
		bMyTurn = !bMyTurn;
	}
	return Merged;
}

TArray<FCinemafyPlannedShot> ACinemafySceneDirector::PlanDirectedLines(const TArray<FCinemafyLine>& InLines) const
{
	return UCinemafyDirectorLibrary::PlanScene(InLines, GetStyle(), Seed);
}

void ACinemafySceneDirector::LearnStyleFromSequence()
{
#if WITH_EDITOR
	OnEditorRequest.Broadcast(this, ECinemafyDirectorRequest::LearnStyle);
#endif
}

// --- Live directing -------------------------------------------------------------------

void ACinemafySceneDirector::QueueShots(const TArray<FCinemafyPlannedShot>& Shots, float TimeOffset)
{
	// A new line replaces whatever was still pending from the last one.
	LiveQueue.Reset();
	NextShot = 0;
	for (FCinemafyPlannedShot Shot : Shots)
	{
		Shot.StartTime += TimeOffset;
		LiveQueue.Add(Shot);
		LiveEndTime = FMath::Max(LiveEndTime, Shot.StartTime + Shot.Duration);
	}
}

void ACinemafySceneDirector::StartConversation()
{
	LastDirectedLines = GetDirectedLines();
	LastPlan = PlanDirectedLines(LastDirectedLines);
	if (LastPlan.Num() == 0)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Director: the script has no lines with speakers."));
		return;
	}

	bLive = true;
	bAutoEnd = true;
	bFirstCut = true;
	LiveTime = 0.f;
	LiveEndTime = 0.f;
	QueueShots(LastPlan, 0.f);
	SetActorTickEnabled(true);
}

void ACinemafySceneDirector::ShowLineSubtitle(const FCinemafyLine& Line)
{
	UCinemafySubtitleSubsystem* Subtitles = GetWorld() ? GetWorld()->GetSubsystem<UCinemafySubtitleSubsystem>() : nullptr;
	if (!Subtitles || !bShowSubtitles)
	{
		return;
	}

	const FText Speaker = !Line.SpeakerName.IsEmpty() ? Line.SpeakerName
		: (Line.Speaker ? FText::FromString(Line.Speaker->GetActorNameOrLabel()) : FText::GetEmpty());
	Subtitles->ShowSubtitle(Speaker, Line.Text, Line.Duration);
}

void ACinemafySceneDirector::UpdateSequencedSubtitle()
{
	// A sequence the director cut keys SubtitleLineIndex; follow it.
	if (SubtitleLineIndex == ShownSubtitleIndex || bLive)
	{
		return;
	}
	ShownSubtitleIndex = SubtitleLineIndex;

	const TArray<FCinemafyLine>& SubtitleLines = LastDirectedLines.Num() > 0 ? LastDirectedLines : Lines;
	if (SubtitleLines.IsValidIndex(SubtitleLineIndex))
	{
		ShowLineSubtitle(SubtitleLines[SubtitleLineIndex]);
	}
	else if (UCinemafySubtitleSubsystem* Subtitles = GetWorld() ? GetWorld()->GetSubsystem<UCinemafySubtitleSubsystem>() : nullptr)
	{
		Subtitles->ClearSubtitle();
	}
}

void ACinemafySceneDirector::UpdateSequencedTransition()
{
	if (TransitionCue < 0)
	{
		TriggeredTransitionCue = -1;
		return;
	}
	if (TransitionCue == TriggeredTransitionCue || bLive)
	{
		return;
	}
	TriggeredTransitionCue = TransitionCue;

	UCinemafyTransitionSubsystem* Transitions = GetWorld() ? GetWorld()->GetSubsystem<UCinemafyTransitionSubsystem>() : nullptr;
	if (!Transitions || LastPlan.Num() == 0)
	{
		return;
	}

	// Cue N = the transition into shot N; cue == Num = the closing transition out of the last shot.
	const bool bClosing = TransitionCue >= LastPlan.Num();
	const FCinemafyPlannedShot& Shot = bClosing ? LastPlan.Last() : LastPlan[TransitionCue];
	const ECinemafyTransition Type = bClosing ? Shot.TransitionOut : Shot.TransitionIn;
	const int32 OutgoingIndex = bClosing ? LastPlan.Num() - 1 : TransitionCue - 1;
	AActor* Outgoing = LastPlanCameras.IsValidIndex(OutgoingIndex) ? LastPlanCameras[OutgoingIndex].Get() : nullptr;
	if (!bClosing && TransitionCue == 0 && Type != ECinemafyTransition::DipToWhite)
	{
		return; // Nothing to dissolve, wipe or whip from (the scene's first shot): capturing it would flash white.
	}

	// Fades through black are baked as a Sequencer Fade track; only wipes and dissolves need the overlay.
	if (Type == ECinemafyTransition::Wipe || Type == ECinemafyTransition::Dissolve
		|| Type == ECinemafyTransition::DipToWhite || Type == ECinemafyTransition::WhipPan)
	{
		Transitions->PlayTransition(Type, Shot.TransitionDuration, Shot.WipeDirection, Outgoing);
	}
}

void ACinemafySceneDirector::PlayLiveTransition(ECinemafyTransition Type, float Duration, ECinemafyWipeDirection Direction, AActor* Outgoing, bool bFromGameplayView)
{
	UCinemafyTransitionSubsystem* Transitions = GetWorld() ? GetWorld()->GetSubsystem<UCinemafyTransitionSubsystem>() : nullptr;
	if (!Transitions || Type == ECinemafyTransition::Cut || Type == ECinemafyTransition::MatchCut)
	{
		return;
	}

	if (bFromGameplayView)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			Transitions->PlayTransitionFromView(Type, Duration, Direction, PC->PlayerCameraManager->GetCameraCacheView());
			return;
		}
	}
	Transitions->PlayTransition(Type, Duration, Direction, Outgoing);
}

void ACinemafySceneDirector::DirectLine(AActor* Speaker, AActor* Listener, float Duration, float Intensity, const FText& Subtitle)
{
	if (!Speaker)
	{
		return;
	}

	if (!bLive)
	{
		bLive = true;
		bFirstCut = true;
		LiveTime = 0.f;
		LiveEndTime = 0.f;
		LiveLineIndex = 0;
		bHasLastLiveShot = false;
		LiveCast.Reset();
		LiveRandom.Initialize(Seed);
	}
	// Lines driven by a dialogue system end when it says so.
	bAutoEnd = false;

	FCinemafyLine Line;
	Line.Speaker = Speaker;
	Line.Listener = Listener;
	Line.Duration = Duration;
	Line.PauseAfter = 0.f;
	Line.Intensity = Intensity;
	Line.Text = Subtitle;
	// Walking speakers get walk-and-talk coverage.
	Line.bMoving = Speaker->GetVelocity().Size2D() > 60.f;
	CurrentLiveLine = Line;

	LiveCast.AddUnique(Speaker);
	if (Listener)
	{
		LiveCast.AddUnique(Listener);
	}

	const TArray<FCinemafyPlannedShot> Shots = UCinemafyDirectorLibrary::PlanLine(
		Line, LiveLineIndex++, GetStyle(), LiveRandom, bHasLastLiveShot ? &LastLiveShot : nullptr, bFirstCut, 0.f, LiveCast);
	QueueShots(Shots, LiveTime);
	SetActorTickEnabled(true);
}

void ACinemafySceneDirector::EndConversation()
{
	if (!bLive)
	{
		return;
	}

	bLive = false;
	LiveQueue.Reset();
	if (SlowMoDelay >= 0.f || SlowMoRemaining > 0.f)
	{
		SlowMoDelay = -1.f;
		SlowMoRemaining = 0.f;
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}

	// Closing transition out of the last shot, straight into gameplay.
	const FCinemafyDirectorStyle DirectorStyle = GetStyle();
	const bool bClosingTransition = DirectorStyle.ClosingTransition != ECinemafyTransition::Cut && DirectorStyle.ClosingTransition != ECinemafyTransition::MatchCut;
	if (bClosingTransition)
	{
		PlayLiveTransition(DirectorStyle.ClosingTransition, DirectorStyle.TransitionDuration, ECinemafyWipeDirection::LeftToRight, LastLiveCamera.Get(), false);
	}
	if (UCinemafySubtitleSubsystem* Subtitles = GetWorld() ? GetWorld()->GetSubsystem<UCinemafySubtitleSubsystem>() : nullptr)
	{
		Subtitles->ClearSubtitle();
	}
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), bClosingTransition ? 0.f : BlendOutTime, VTBlend_Cubic);
	}
}

void ACinemafySceneDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		UpdateSequencedSubtitle();
		UpdateSequencedTransition();
	}

	if (!bLive)
	{
		return;
	}

	LiveTime += DeltaSeconds;

	// Speed ramps run on real time, since game time is what they slow down.
	if (SlowMoDelay >= 0.f || SlowMoRemaining > 0.f)
	{
		const float RealDelta = static_cast<float>(FApp::GetDeltaTime());
		if (SlowMoDelay >= 0.f)
		{
			SlowMoDelay -= RealDelta;
			if (SlowMoDelay < 0.f)
			{
				UGameplayStatics::SetGlobalTimeDilation(this, SlowMoRate);
			}
		}
		else
		{
			SlowMoRemaining -= RealDelta;
			if (SlowMoRemaining <= 0.f)
			{
				UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
			}
		}
	}
	while (NextShot < LiveQueue.Num() && LiveQueue[NextShot].StartTime <= LiveTime)
	{
		CutTo(LiveQueue[NextShot++]);
	}

	if (bAutoEnd && NextShot >= LiveQueue.Num() && LiveTime >= LiveEndTime)
	{
		EndConversation();
	}
}

ACineCameraActor* ACinemafySceneDirector::GetLiveCamera(const FCinemafyPlannedShot& Shot)
{
	const FString Key = ShotKey(Shot, bFlipSide);
	if (TObjectPtr<ACineCameraActor>* Existing = LiveCameras.Find(Key))
	{
		if (IsValid(*Existing))
		{
			return *Existing;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACineCameraActor* Camera = GetWorld()->SpawnActor<ACineCameraActor>(GetActorLocation(), FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->Tags.Add(GetDirectorTag());
		LiveCameras.Add(Key, Camera);
	}
	return Camera;
}

void ACinemafySceneDirector::CutTo(const FCinemafyPlannedShot& Shot)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ACineCameraActor* Camera = PC ? GetLiveCamera(Shot) : nullptr;
	if (!Camera)
	{
		return;
	}

	// Re-solve every cut: characters move during gameplay conversations.
	FCinemafyShotRequest Request;
	Request.ShotType = Shot.ShotType;
	Request.Subject = Shot.Subject;
	Request.Counterpart = Shot.Counterpart;
	Request.Group = Shot.Group;
	Request.bUseRecommendedLens = false;
	Request.Lens = Shot.Lens;
	Request.bFlipSide = bFlipSide;

	FCinemafyShotSolution Solution;
	bool bClearView = false;
	if (!UCinemafyFramingLibrary::SolveShotAvoidingObstacles(this, Request, Solution, bClearView))
	{
		return;
	}

	Camera->SetActorTransform(Solution.CameraTransform);

	// Walk-and-talk shots ride a follow rig; everything else stays put.
	const bool bTracking = Shot.ShotType == ECinemafyShotType::TrackingLead || Shot.ShotType == ECinemafyShotType::TrackingFollow;
	UCinemafyFollowComponent* Follow = Camera->FindComponentByClass<UCinemafyFollowComponent>();
	if (bTracking)
	{
		Follow = Follow ? Follow : FindOrAddRuntimeComponent<UCinemafyFollowComponent>(Camera);
		Follow->Target = Shot.Subject;
		Follow->Weight = 1.f;
		Follow->CaptureOffset();
	}
	else if (Follow)
	{
		Follow->Weight = 0.f;
	}
	UCineCameraComponent* Cine = Camera->GetCineCameraComponent();
	UCinemafyCameraLibrary::ApplySensor(Cine, Request.Sensor);
	UCinemafyCameraLibrary::ApplyPrime(Cine, Solution.Lens, 2.8f);

	const FCinemafyDirectorStyle DirectorStyle = GetStyle();
	if (DirectorStyle.bApplyLook)
	{
		UCinemafyCameraLibrary::ApplyLook(Cine, DirectorStyle.Look, DirectorStyle.LookStrength);
		UCinemafyCameraLibrary::SetLetterbox(Cine, DirectorStyle.LetterboxAspect);
	}

	FindOrAddRuntimeComponent<UCinemafyCameraFeelComponent>(Camera)->Feel = Shot.Feel;
	UCinemafyFocusComponent* Focus = FindOrAddRuntimeComponent<UCinemafyFocusComponent>(Camera);
	Focus->FocusTarget = Shot.FocusOn ? Shot.FocusOn.Get() : Shot.Subject.Get();
	Focus->RackTarget = nullptr;
	Focus->RackAlpha = 0.f;

	UCinemafyCameraMoveComponent* Move = FindOrAddRuntimeComponent<UCinemafyCameraMoveComponent>(Camera);
	if (Shot.bMove)
	{
		Move->Move = Shot.Move;
		Move->Distance = Shot.MoveDistance;
		Move->Play(FMath::Max(Shot.MoveEnd - Shot.MoveStart, 0.2f), Shot.MoveStart);
	}
	else
	{
		Move->Stop();
		Move->Progress = 0.f;
	}

	if (Shot.bSlowMo)
	{
		// Live speed ramp: slow the world around the hit, restored in Tick (on real time).
		SlowMoDelay = Shot.SlowMoTime;
		SlowMoRate = FMath::Clamp(Shot.SlowMoRate, 0.05f, 1.f);
		SlowMoRemaining = Shot.SlowMoHold + 0.1f;
	}

	if (Shot.bSetEmotion && !Shot.bIsReaction && Shot.Subject)
	{
		if (UCinemafyFaceComponent* Face = Shot.Subject->FindComponentByClass<UCinemafyFaceComponent>())
		{
			Face->SetEmotion(Shot.Emotion, 0.6f);
		}
	}

	if (Shot.bLineStart)
	{
		// Scripted conversations look the line up; dialogue-system lines carry their own.
		if (bAutoEnd && LastDirectedLines.IsValidIndex(Shot.LineIndex))
		{
			ShowLineSubtitle(LastDirectedLines[Shot.LineIndex]);
		}
		else if (!bAutoEnd)
		{
			ShowLineSubtitle(CurrentLiveLine);
		}
	}

	if (bUpdateListeners && !Shot.bIsReaction && Shot.Counterpart)
	{
		if (UCinemafyListenerComponent* Listener = Shot.Counterpart->FindComponentByClass<UCinemafyListenerComponent>())
		{
			Listener->SetSpeaker(Shot.Subject);
		}
	}

	// Transitions: the shot's own (scene breaks), or the style's opening on the first shot.
	ECinemafyTransition Transition = Shot.TransitionIn;
	if (bFirstCut && Transition == ECinemafyTransition::Cut)
	{
		Transition = DirectorStyle.OpeningTransition;
	}
	const bool bTransition = Transition != ECinemafyTransition::Cut && Transition != ECinemafyTransition::MatchCut;
	if (bTransition)
	{
		PlayLiveTransition(Transition, DirectorStyle.TransitionDuration, Shot.WipeDirection, LastLiveCamera.Get(), bFirstCut);
	}

	// Blend in from gameplay on the first shot (unless a transition does the job); hard cuts after that.
	PC->SetViewTargetWithBlend(Camera, (bFirstCut && !bTransition) ? BlendInTime : 0.f, VTBlend_Cubic);
	bFirstCut = false;
	LastLiveCamera = Camera;
	LastLiveShot = Shot;
	bHasLastLiveShot = true;
}
