#include "CinemafySceneDirector.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyFaceComponent.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyListenerComponent.h"
#include "CinemafyModule.h"
#include "CinemafySubtitleSubsystem.h"
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

FCinemafyDirectorStyle ACinemafySceneDirector::GetStyle() const
{
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
	LastPlan = UCinemafyDirectorLibrary::PlanScene(GetEffectiveLines(), GetStyle(), Seed);
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

	if (Lines.IsValidIndex(SubtitleLineIndex))
	{
		ShowLineSubtitle(Lines[SubtitleLineIndex]);
	}
	else if (UCinemafySubtitleSubsystem* Subtitles = GetWorld() ? GetWorld()->GetSubsystem<UCinemafySubtitleSubsystem>() : nullptr)
	{
		Subtitles->ClearSubtitle();
	}
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
	CurrentLiveLine = Line;

	const TArray<FCinemafyPlannedShot> Shots = UCinemafyDirectorLibrary::PlanLine(
		Line, LiveLineIndex++, GetStyle(), LiveRandom, bHasLastLiveShot ? &LastLiveShot : nullptr, bFirstCut, 0.f);
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
	if (UCinemafySubtitleSubsystem* Subtitles = GetWorld() ? GetWorld()->GetSubsystem<UCinemafySubtitleSubsystem>() : nullptr)
	{
		Subtitles->ClearSubtitle();
	}
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), BlendOutTime, VTBlend_Cubic);
	}
}

void ACinemafySceneDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		UpdateSequencedSubtitle();
	}

	if (!bLive)
	{
		return;
	}

	LiveTime += DeltaSeconds;
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
	UCineCameraComponent* Cine = Camera->GetCineCameraComponent();
	UCinemafyCameraLibrary::ApplySensor(Cine, Request.Sensor);
	UCinemafyCameraLibrary::ApplyPrime(Cine, Solution.Lens, 2.8f);

	FindOrAddRuntimeComponent<UCinemafyCameraFeelComponent>(Camera)->Feel = Shot.Feel;
	FindOrAddRuntimeComponent<UCinemafyFocusComponent>(Camera)->FocusTarget = Shot.Subject;

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
		if (bAutoEnd && Lines.IsValidIndex(Shot.LineIndex))
		{
			ShowLineSubtitle(GetEffectiveLines()[Shot.LineIndex]);
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

	// Blend in from gameplay on the first shot; hard cuts after that, like a real edit.
	PC->SetViewTargetWithBlend(Camera, bFirstCut ? BlendInTime : 0.f, VTBlend_Cubic);
	bFirstCut = false;
	LastLiveShot = Shot;
	bHasLastLiveShot = true;
}
