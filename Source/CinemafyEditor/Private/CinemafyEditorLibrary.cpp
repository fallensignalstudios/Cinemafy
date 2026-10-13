#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafySequencerUtils.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyGazeComponent.h"
#include "CinemafyLightRig.h"
#include "CinemafyListenerComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Selection.h"
#include "ScopedTransaction.h"
#include "Misc/MessageDialog.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

#define LOCTEXT_NAMESPACE "CinemafyEditor"

namespace
{
	template <typename TComponent>
	TComponent* FindOrAddComponent(AActor* Actor, const TCHAR* BaseName)
	{
		if (TComponent* Existing = Actor->FindComponentByClass<TComponent>())
		{
			Existing->Modify();
			return Existing;
		}

		Actor->Modify();
		TComponent* Component = NewObject<TComponent>(Actor, MakeUniqueObjectName(Actor, TComponent::StaticClass(), BaseName), RF_Transactional);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	TArray<UCineCameraComponent*> GetSelectedCameras()
	{
		TArray<UCineCameraComponent*> Cameras;
		for (AActor* Actor : UCinemafyEditorLibrary::GetSelectedActorsInOrder())
		{
			if (UCineCameraComponent* Camera = UCinemafyCameraLibrary::FindCineCamera(Actor))
			{
				Cameras.Add(Camera);
			}
		}
		return Cameras;
	}

	void WarnNothingSelected(const TCHAR* What)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: select at least one %s first."), What);
	}

	/** Sets manual ease-in on the earliest section and ease-out on the latest, clamped to half a section. */
	template <typename TSectionArray>
	void EaseFirstAndLastSections(const TSectionArray& Sections, int32 EaseInTicks, int32 EaseOutTicks)
	{
		UMovieSceneSection* First = nullptr;
		UMovieSceneSection* Last = nullptr;
		for (UMovieSceneSection* Section : Sections)
		{
			if (!Section || !Section->HasStartFrame() || !Section->HasEndFrame())
			{
				continue;
			}
			if (!First || Section->GetInclusiveStartFrame() < First->GetInclusiveStartFrame())
			{
				First = Section;
			}
			if (!Last || Section->GetExclusiveEndFrame() > Last->GetExclusiveEndFrame())
			{
				Last = Section;
			}
		}

		auto HalfLength = [](const UMovieSceneSection* Section)
		{
			return (Section->GetExclusiveEndFrame() - Section->GetInclusiveStartFrame()).Value / 2;
		};

		if (First)
		{
			First->Modify();
			First->Easing.bManualEaseIn = true;
			First->Easing.ManualEaseInDuration = FMath::Min(EaseInTicks, HalfLength(First));
		}
		if (Last)
		{
			Last->Modify();
			Last->Easing.bManualEaseOut = true;
			Last->Easing.ManualEaseOutDuration = FMath::Min(EaseOutTicks, HalfLength(Last));
		}
	}

	/** A move size that reads well on screen without being showy. */
	void ApplyDefaultMoveSize(UCinemafyCameraMoveComponent* MoveComp)
	{
		switch (MoveComp->Move)
		{
		case ECinemafyCameraMove::PushIn:
		case ECinemafyCameraMove::PullOut:      MoveComp->Distance = 80.f; break;
		case ECinemafyCameraMove::TruckLeft:
		case ECinemafyCameraMove::TruckRight:   MoveComp->Distance = 120.f; break;
		case ECinemafyCameraMove::PedestalUp:
		case ECinemafyCameraMove::PedestalDown: MoveComp->Distance = 60.f; break;
		case ECinemafyCameraMove::CraneUp:
		case ECinemafyCameraMove::CraneDown:    MoveComp->Distance = 150.f; break;
		default:                                MoveComp->OrbitAngle = 20.f; break;
		}
	}
}

TArray<AActor*> UCinemafyEditorLibrary::GetSelectedActorsInOrder()
{
	TArray<AActor*> Actors;
	if (GEditor)
	{
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Actors);
	}
	return Actors;
}

ACineCameraActor* UCinemafyEditorLibrary::CreateShot(const FCinemafyShotRequest& Request, bool bAddToOpenSequence)
{
	FCinemafyShotSolution Solution;
	if (!GEditor || !UCinemafyFramingLibrary::SolveShot(Request, Solution))
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: couldn't solve that shot. Two-shots and over-the-shoulders need a subject and a counterpart."));
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateShot", "Cinemafy: Create Shot"));

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transactional;
	ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(
		Solution.CameraTransform.GetLocation(), Solution.CameraTransform.Rotator(), Params);
	if (!CameraActor)
	{
		return nullptr;
	}

	const FString ShotName = StaticEnum<ECinemafyShotType>()->GetNameStringByValue(static_cast<int64>(Request.ShotType));
	CameraActor->SetActorLabel(FString::Printf(TEXT("CAM_%s_%s"), *ShotName, *Request.Subject->GetActorLabel()));

	UCineCameraComponent* Camera = CameraActor->GetCineCameraComponent();
	UCinemafyCameraLibrary::ApplySensor(Camera, Request.Sensor);
	UCinemafyCameraLibrary::ApplyPrime(Camera, Solution.Lens, 2.8f);

	UCinemafyFocusComponent* Focus = FindOrAddComponent<UCinemafyFocusComponent>(CameraActor, TEXT("CinemafyFocus"));
	Focus->FocusTarget = Request.Subject;

	if (bAddToOpenSequence)
	{
		AddCameraToOpenSequence(CameraActor, true);
	}

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(CameraActor, true, true);
	return CameraActor;
}

ACineCameraActor* UCinemafyEditorLibrary::CreateShotFromSelection(ECinemafyShotType ShotType, bool bFlipSide)
{
	const TArray<AActor*> Selected = GetSelectedActorsInOrder();
	if (Selected.Num() == 0)
	{
		WarnNothingSelected(TEXT("character"));
		return nullptr;
	}

	FCinemafyShotRequest Request;
	Request.ShotType = ShotType;
	Request.Subject = Selected[0];
	Request.Counterpart = Selected.Num() > 1 ? Selected[1] : nullptr;
	Request.bFlipSide = bFlipSide;
	return CreateShot(Request);
}

int32 UCinemafyEditorLibrary::ApplyCameraFeelToSelection(ECinemafyCameraFeel Feel)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyFeel", "Cinemafy: Camera Feel"));
	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (!UCinemafyCameraLibrary::FindCineCamera(Actor))
		{
			continue;
		}

		UCinemafyCameraFeelComponent* FeelComp = FindOrAddComponent<UCinemafyCameraFeelComponent>(Actor, TEXT("CinemafyFeel"));
		FeelComp->Feel = Feel;
		// Different cameras shouldn't wobble identically.
		FeelComp->Seed = static_cast<int32>(GetTypeHash(Actor->GetFName()) % 1000);
		++Count;
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Count;
}

int32 UCinemafyEditorLibrary::ApplyLookToSelection(ECinemafyLook Look, float Strength)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyLook", "Cinemafy: Apply Look"));
	const TArray<UCineCameraComponent*> Cameras = GetSelectedCameras();
	for (UCineCameraComponent* Camera : Cameras)
	{
		Camera->Modify();
		UCinemafyCameraLibrary::ApplyLook(Camera, Look, Strength);
	}

	if (Cameras.Num() == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Cameras.Num();
}

int32 UCinemafyEditorLibrary::SetLetterboxOnSelection(float AspectRatio)
{
	const FScopedTransaction Transaction(LOCTEXT("Letterbox", "Cinemafy: Letterbox"));
	const TArray<UCineCameraComponent*> Cameras = GetSelectedCameras();
	for (UCineCameraComponent* Camera : Cameras)
	{
		Camera->Modify();
		UCinemafyCameraLibrary::SetLetterbox(Camera, AspectRatio);
	}

	if (Cameras.Num() == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Cameras.Num();
}

int32 UCinemafyEditorLibrary::ApplyPrimeToSelection(ECinemafyPrime Prime, float FStop)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyPrime", "Cinemafy: Lens"));
	const TArray<UCineCameraComponent*> Cameras = GetSelectedCameras();
	for (UCineCameraComponent* Camera : Cameras)
	{
		Camera->Modify();
		UCinemafyCameraLibrary::ApplyPrime(Camera, Prime, FStop);
	}

	if (Cameras.Num() == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Cameras.Num();
}

int32 UCinemafyEditorLibrary::ApplyCameraMoveToSelection(ECinemafyCameraMove Move)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyMove", "Cinemafy: Camera Move"));
	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (!UCinemafyCameraLibrary::FindCineCamera(Actor))
		{
			continue;
		}

		UCinemafyCameraMoveComponent* MoveComp = FindOrAddComponent<UCinemafyCameraMoveComponent>(Actor, TEXT("CinemafyMove"));
		MoveComp->Move = Move;
		ApplyDefaultMoveSize(MoveComp);
		++Count;
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Count;
}

bool UCinemafyEditorLibrary::AddCameraToOpenSequence(ACineCameraActor* CameraActor, bool bAddCameraCut)
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!CameraActor || !MovieScene)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddToSequence", "Cinemafy: Add Camera to Sequence"));
	Sequence->Modify();
	MovieScene->Modify();

	const FGuid Binding = CinemafySequencer::BindActor(Sequence, CameraActor);

	if (bAddCameraCut)
	{
		UMovieSceneTrack* Track = MovieScene->GetCameraCutTrack();
		if (!Track)
		{
			Track = MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
		}

		if (UMovieSceneCameraCutTrack* CutTrack = Cast<UMovieSceneCameraCutTrack>(Track))
		{
			CutTrack->Modify();
			CutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(Binding), CinemafySequencer::GetPlayheadTick(MovieScene));
		}
	}

	CinemafySequencer::NotifyChanged();
	return true;
}

int32 UCinemafyEditorLibrary::AddSelectedCamerasToSequence()
{
	if (!CinemafySequencer::GetOpenSequence())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSequence", "Open a Level Sequence in Sequencer first."));
		return 0;
	}

	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (ACineCameraActor* CameraActor = Cast<ACineCameraActor>(Actor))
		{
			Count += AddCameraToOpenSequence(CameraActor, true) ? 1 : 0;
		}
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Count;
}

int32 UCinemafyEditorLibrary::CheckLineOfActionForSelection()
{
	// Cameras that sit (almost) on the line are neutral: cutting through them is a legit way to cross.
	constexpr float OnTheLineTolerance = 15.f;

	TArray<AActor*> Characters;
	TArray<AActor*> Cameras;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (UCinemafyCameraLibrary::FindCineCamera(Actor))
		{
			Cameras.Add(Actor);
		}
		else if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			Characters.Add(Actor);
		}
	}

	if (Characters.Num() != 2 || Cameras.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LineCheckSelection",
			"Select exactly two characters (the line of action runs between them) and the cameras you want to check."));
		return -1;
	}

	TArray<AActor*> PositiveSide;
	TArray<AActor*> NegativeSide;
	for (AActor* Camera : Cameras)
	{
		const FVector CameraLocation = UCinemafyCameraLibrary::FindCineCamera(Camera)->GetComponentLocation();
		const float Side = UCinemafyFramingLibrary::GetSideOfLine(CameraLocation, Characters[0], Characters[1]);
		if (Side > OnTheLineTolerance)
		{
			PositiveSide.Add(Camera);
		}
		else if (Side < -OnTheLineTolerance)
		{
			NegativeSide.Add(Camera);
		}
	}

	const TArray<AActor*>& Offenders = (NegativeSide.Num() <= PositiveSide.Num()) ? NegativeSide : PositiveSide;
	if (Offenders.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("LineCheckClean",
			"All {0} cameras are on the same side of the line between {1} and {2}. Cut away!"),
			Cameras.Num(), FText::FromString(Characters[0]->GetActorLabel()), FText::FromString(Characters[1]->GetActorLabel())));
		return 0;
	}

	FString Names;
	for (const AActor* Offender : Offenders)
	{
		Names += FString::Printf(TEXT("\n  - %s"), *Offender->GetActorLabel());
	}

	GEditor->SelectNone(false, true);
	for (AActor* Offender : Offenders)
	{
		GEditor->SelectActor(Offender, true, false);
	}
	GEditor->NoteSelectionChange();

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("LineCheckCrossed",
		"{0} camera(s) are across the line from the rest (now selected). Cutting between the two sides will flip screen direction:{1}\n\nMove them across, or bridge the jump with a shot on the line or a camera move that crosses it on screen."),
		Offenders.Num(), FText::FromString(Names)));
	return Offenders.Num();
}

ACinemafyLightRig* UCinemafyEditorLibrary::CreateLightRig(AActor* Subject, AActor* TargetCamera, ECinemafyLightingStyle Style)
{
	UWorld* World = (GEditor && Subject) ? Subject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateLightRig", "Cinemafy: Create Light Rig"));

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transactional;
	ACinemafyLightRig* Rig = World->SpawnActor<ACinemafyLightRig>(UCinemafyFramingLibrary::GetEyePoint(Subject), FRotator::ZeroRotator, Params);
	if (!Rig)
	{
		return nullptr;
	}

	const FString StyleName = StaticEnum<ECinemafyLightingStyle>()->GetNameStringByValue(static_cast<int64>(Style));
	Rig->SetActorLabel(FString::Printf(TEXT("LIGHT_%s_%s"), *StyleName, *Subject->GetActorLabel()));
	Rig->Subject = Subject;
	Rig->TargetCamera = TargetCamera;
	Rig->Style = Style;

	// The rig turns on a lighting channel on the character's meshes, so record them for undo.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Subject);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->Modify();
	}
	Rig->ApplyCharacterChannels();
	Rig->SolveRig();

	if (ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence())
	{
		CinemafySequencer::BindActor(Sequence, Rig);
		CinemafySequencer::NotifyChanged();
	}

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Rig, true, true);
	return Rig;
}

int32 UCinemafyEditorLibrary::ApplyLightingStyleToSelection(ECinemafyLightingStyle Style)
{
	const TArray<AActor*> Selected = GetSelectedActorsInOrder();

	// Restyle any selected rigs.
	int32 Restyled = 0;
	{
		const FScopedTransaction Transaction(LOCTEXT("RestyleLightRig", "Cinemafy: Lighting Style"));
		for (AActor* Actor : Selected)
		{
			if (ACinemafyLightRig* Rig = Cast<ACinemafyLightRig>(Actor))
			{
				Rig->Modify();
				Rig->Style = Style;
				Rig->SolveRig();
				++Restyled;
			}
		}
	}
	if (Restyled > 0)
	{
		return Restyled;
	}

	// Otherwise light the selected character(s), designed for the selected camera if there is one.
	AActor* Camera = nullptr;
	TArray<AActor*> Characters;
	for (AActor* Actor : Selected)
	{
		if (UCinemafyCameraLibrary::FindCineCamera(Actor))
		{
			Camera = Camera ? Camera : Actor;
		}
		else if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			Characters.Add(Actor);
		}
	}

	int32 Created = 0;
	for (AActor* Character : Characters)
	{
		Created += CreateLightRig(Character, Camera, Style) ? 1 : 0;
	}

	if (Created == 0)
	{
		WarnNothingSelected(TEXT("character or Light Rig"));
	}
	return Created;
}

bool UCinemafyEditorLibrary::SetUpGameplayBlendForOpenSequence(float BlendInSeconds, float BlendOutSeconds, bool bIncludeAnimations)
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	UMovieSceneCameraCutTrack* CutTrack = MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	if (!CutTrack || CutTrack->GetAllSections().Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoCuts", "Open a Level Sequence with a camera cut track first (Create Shot adds one for you)."));
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("GameplayBlend", "Cinemafy: Set Up Gameplay Blend"));
	MovieScene->Modify();

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const int32 EaseInTicks = TickResolution.AsFrameNumber(FMath::Max(BlendInSeconds, 0.f)).Value;
	const int32 EaseOutTicks = TickResolution.AsFrameNumber(FMath::Max(BlendOutSeconds, 0.f)).Value;

	// Camera: blend from the gameplay camera into the first cut, and from the last cut back out.
	CutTrack->Modify();
	CutTrack->bCanBlend = true;
	EaseFirstAndLastSections(CutTrack->GetAllSections(), EaseInTicks, EaseOutTicks);

	// Characters: ease their animation in and out so the pose blends with the Anim Blueprint.
	int32 AnimTracks = 0;
	if (bIncludeAnimations)
	{
		// Read-only access to the bindings; we only modify the tracks they own.
		const UMovieScene* ConstMovieScene = MovieScene;
		for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
		{
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				if (UMovieSceneSkeletalAnimationTrack* AnimTrack = Cast<UMovieSceneSkeletalAnimationTrack>(Track))
				{
					AnimTrack->Modify();
					EaseFirstAndLastSections(AnimTrack->GetAllSections(), EaseInTicks, EaseOutTicks);
					++AnimTracks;
				}
			}
		}
	}

	CinemafySequencer::NotifyChanged();
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: gameplay blend set up (%.2fs in, %.2fs out, %d animation tracks)."), BlendInSeconds, BlendOutSeconds, AnimTracks);
	return true;
}

int32 UCinemafyEditorLibrary::AddListeningToSelection(ECinemafyTemperament Temperament)
{
	TArray<AActor*> Characters;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			Characters.Add(Actor);
		}
	}

	if (Characters.Num() == 0)
	{
		WarnNothingSelected(TEXT("character (with a skeletal mesh)"));
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddListening", "Cinemafy: Add Listening"));
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		AActor* Character = Characters[Index];
		FindOrAddComponent<UCinemafyGazeComponent>(Character, TEXT("CinemafyGaze"));

		UCinemafyListenerComponent* Listener = FindOrAddComponent<UCinemafyListenerComponent>(Character, TEXT("CinemafyListener"));
		Listener->Temperament = Temperament;
		// Different seeds so two listeners never react in lockstep.
		Listener->Seed = static_cast<int32>(GetTypeHash(Character->GetFName()) % 1000);

		// A pair becomes a conversation: each listens to the other.
		if (Characters.Num() == 2)
		{
			Listener->SetSpeaker(Characters[1 - Index]);
		}
	}
	return Characters.Num();
}

int32 UCinemafyEditorLibrary::AddGazeToSelection()
{
	const FScopedTransaction Transaction(LOCTEXT("AddGaze", "Cinemafy: Add Gaze"));
	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			FindOrAddComponent<UCinemafyGazeComponent>(Actor, TEXT("CinemafyGaze"));
			++Count;
		}
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("character (with a skeletal mesh)"));
	}
	return Count;
}

#undef LOCTEXT_NAMESPACE
