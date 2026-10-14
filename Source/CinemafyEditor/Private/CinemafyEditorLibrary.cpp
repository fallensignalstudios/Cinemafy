#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafySequencerUtils.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyCameraMoveComponent.h"
#include "Sound/SoundWave.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Animation/AnimSequenceBase.h"
#include "Tracks/MovieSceneIntegerTrack.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Engine/SkeletalMesh.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyGazeComponent.h"
#include "CinemafyLightRig.h"
#include "CinemafyListenerComponent.h"
#include "CinemafyFaceComponent.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafySceneDirector.h"
#include "SCinemafyShotBrowser.h"
#include "EngineUtils.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneEnumSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneEnumTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "ISequencer.h"
#include "MovieSceneSequenceID.h"
#include "Camera/CameraComponent.h"
#include "CinemafyCameraMoveComponent.h"
#include "Sound/SoundWave.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Animation/AnimSequenceBase.h"
#include "Tracks/MovieSceneIntegerTrack.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Engine/SkeletalMesh.h"
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
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	FCinemafyShotSolution Solution;
	bool bClearView = false;
	if (!World || !UCinemafyFramingLibrary::SolveShotAvoidingObstacles(World, Request, Solution, bClearView))
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: couldn't solve that shot. Two-shots and over-the-shoulders need a subject and a counterpart."));
		return nullptr;
	}
	if (!bClearView)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: every placement for this shot is blocked by level geometry. Placed the best guess; nudge it by hand."));
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
	for (int32 Index = 2; Index < Selected.Num(); ++Index)
	{
		Request.Group.Add(Selected[Index]);
	}
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

int32 UCinemafyEditorLibrary::ApplyFaceEmotionToSelection(ECinemafyEmotion Emotion)
{
	const FScopedTransaction Transaction(LOCTEXT("FaceEmotion", "Cinemafy: Face Emotion"));
	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			UCinemafyFaceComponent* Face = FindOrAddComponent<UCinemafyFaceComponent>(Actor, TEXT("CinemafyFace"));
			Face->Emotion = Emotion;
			++Count;
		}
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("character (with a skeletal mesh)"));
	}
	return Count;
}

namespace
{
	/** Removes camera cut sections bound to any camera carrying the director's tag. */
	int32 RemoveDirectorCuts(ACinemafySceneDirector* Director, ULevelSequence* Sequence, UMovieSceneCameraCutTrack* CutTrack)
	{
		const FName Tag = Director->GetDirectorTag();
		TSet<FGuid> DirectorBindings;
		for (TActorIterator<ACineCameraActor> It(Director->GetWorld()); It; ++It)
		{
			if (It->Tags.Contains(Tag))
			{
				const FGuid Binding = CinemafySequencer::FindBinding(Sequence, *It);
				if (Binding.IsValid())
				{
					DirectorBindings.Add(Binding);
				}
			}
		}

		TArray<UMovieSceneSection*> ToRemove;
		for (UMovieSceneSection* Section : CutTrack->GetAllSections())
		{
			const UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Section);
			if (Cut && DirectorBindings.Contains(Cut->GetCameraBindingID().GetGuid()))
			{
				ToRemove.Add(Section);
			}
		}

		CutTrack->Modify();
		for (UMovieSceneSection* Section : ToRemove)
		{
			CutTrack->RemoveSection(*Section);
		}
		return ToRemove.Num();
	}

	/** Removes any existing track for Property on a binding, then adds a fresh one. */
	template <typename TTrack>
	TTrack* ReplacePropertyTrack(UMovieScene* MovieScene, const FGuid& Binding, FName Property)
	{
		const UMovieScene* ConstMovieScene = MovieScene;
		if (const FMovieSceneBinding* Existing = ConstMovieScene->FindBinding(Binding))
		{
			TArray<UMovieSceneTrack*> ToRemove;
			for (UMovieSceneTrack* Track : Existing->GetTracks())
			{
				const UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track);
				if (PropertyTrack && PropertyTrack->GetPropertyName() == Property)
				{
					ToRemove.Add(Track);
				}
			}
			for (UMovieSceneTrack* Track : ToRemove)
			{
				MovieScene->RemoveTrack(*Track);
			}
		}

		TTrack* Track = MovieScene->AddTrack<TTrack>(Binding);
		Track->SetPropertyNameAndPath(Property, Property.ToString());
		return Track;
	}

	/** One section covering all time, ready for keys. */
	template <typename TSection>
	TSection* AddInfiniteSection(UMovieSceneTrack* Track)
	{
		TSection* Section = Cast<TSection>(Track->CreateNewSection());
		if (Section)
		{
			Section->SetRange(TRange<FFrameNumber>::All());
			Track->AddSection(*Section);
		}
		return Section;
	}

	struct FCameraShotWindow
	{
		FFrameNumber Start;
		FFrameNumber End;
		const FCinemafyPlannedShot* Shot = nullptr;
	};

	/** Keys a camera's Move Progress: 0 -> 1 across each moving shot, held at 0 on the others. */
	void BakeCameraMoves(ULevelSequence* Sequence, UMovieScene* MovieScene, ACineCameraActor* Camera, const TArray<FCameraShotWindow>& Windows)
	{
		const FCameraShotWindow* FirstMoving = Windows.FindByPredicate([](const FCameraShotWindow& W) { return W.Shot->bMove; });
		UCinemafyCameraMoveComponent* Move = Camera->FindComponentByClass<UCinemafyCameraMoveComponent>();
		if (!FirstMoving && !Move)
		{
			return;
		}

		if (!Move)
		{
			Move = FindOrAddComponent<UCinemafyCameraMoveComponent>(Camera, TEXT("CinemafyMove"));
		}
		Move->Modify();
		Move->Progress = 0.f;
		if (FirstMoving)
		{
			Move->Move = FirstMoving->Shot->Move;
			Move->Distance = FirstMoving->Shot->MoveDistance;
		}

		const FGuid Binding = CinemafySequencer::BindObject(Sequence, Move);
		if (!Binding.IsValid())
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Director: open the sequence in Sequencer to bake camera moves."));
			return;
		}

		UMovieSceneFloatTrack* Track = ReplacePropertyTrack<UMovieSceneFloatTrack>(MovieScene, Binding, GET_MEMBER_NAME_CHECKED(UCinemafyCameraMoveComponent, Progress));
		UMovieSceneFloatSection* Section = AddInfiniteSection<UMovieSceneFloatSection>(Track);
		FMovieSceneFloatChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0) : nullptr;
		if (!Channel)
		{
			return;
		}

		// Linear keys: the component's own easing (operator settle) shapes the move.
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		for (const FCameraShotWindow& Window : Windows)
		{
			Channel->AddLinearKey(Window.Start, 0.f);
			if (Window.Shot->bMove)
			{
				// Hold, then travel across the move's window (timed to land on the emphasis when there is one).
				const FFrameNumber MoveStart = Window.Start + TickResolution.AsFrameNumber(Window.Shot->MoveStart);
				const FFrameNumber MoveEnd = FMath::Min(Window.End, Window.Start + TickResolution.AsFrameNumber(FMath::Max(Window.Shot->MoveEnd, Window.Shot->MoveStart + 0.2f)));
				if (MoveStart > Window.Start)
				{
					Channel->AddLinearKey(MoveStart, 0.f);
				}
				Channel->AddLinearKey(MoveEnd, 1.f);
				if (MoveEnd < Window.End)
				{
					Channel->AddLinearKey(Window.End, 1.f);
				}
			}
			else
			{
				Channel->AddLinearKey(Window.End, 0.f);
			}
		}
	}

	/** Keys each speaker's Face emotion at the start of lines that set one. */
	void BakeEmotions(ULevelSequence* Sequence, UMovieScene* MovieScene, const TArray<FCinemafyPlannedShot>& Plan, FFrameNumber SceneStart)
	{
		TMap<AActor*, TArray<const FCinemafyPlannedShot*>> ByCharacter;
		for (const FCinemafyPlannedShot& Shot : Plan)
		{
			if (Shot.bSetEmotion && Shot.Subject)
			{
				ByCharacter.FindOrAdd(Shot.Subject).Add(&Shot);
			}
		}

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		for (const TPair<AActor*, TArray<const FCinemafyPlannedShot*>>& Pair : ByCharacter)
		{
			UCinemafyFaceComponent* Face = FindOrAddComponent<UCinemafyFaceComponent>(Pair.Key, TEXT("CinemafyFace"));
			const FGuid Binding = CinemafySequencer::BindObject(Sequence, Face);
			if (!Binding.IsValid())
			{
				continue;
			}

			UMovieSceneEnumTrack* Track = ReplacePropertyTrack<UMovieSceneEnumTrack>(MovieScene, Binding, GET_MEMBER_NAME_CHECKED(UCinemafyFaceComponent, Emotion));
			Track->SetEnum(StaticEnum<ECinemafyEmotion>());
			UMovieSceneEnumSection* Section = AddInfiniteSection<UMovieSceneEnumSection>(Track);
			FMovieSceneByteChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneByteChannel>(0) : nullptr;
			if (!Channel)
			{
				continue;
			}

			for (const FCinemafyPlannedShot* Shot : Pair.Value)
			{
				Channel->GetData().AddKey(SceneStart + TickResolution.AsFrameNumber(Shot->StartTime), static_cast<uint8>(Shot->Emotion));
			}
		}
	}

	const FText DialogueTrackName = NSLOCTEXT("CinemafyEditor", "DialogueTrack", "Cinemafy Dialogue");

	/** Lays each line's audio into a "Cinemafy Dialogue" track (one row per speaker), replacing the last take's. */
	void PlaceDialogueAudio(UMovieScene* MovieScene, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart)
	{
		if (!Lines.ContainsByPredicate([](const FCinemafyLine& Line) { return Line.Audio != nullptr; }))
		{
			return;
		}

		TArray<UMovieSceneTrack*> OldTracks;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (Track && Track->IsA<UMovieSceneAudioTrack>() && Track->GetDisplayName().EqualTo(DialogueTrackName))
			{
				OldTracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : OldTracks)
		{
			MovieScene->RemoveTrack(*Track);
		}

		UMovieSceneAudioTrack* AudioTrack = MovieScene->AddTrack<UMovieSceneAudioTrack>();
		AudioTrack->SetDisplayName(DialogueTrackName);

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		TArray<AActor*> Speakers;
		float Time = 0.f;
		for (const FCinemafyLine& Line : Lines)
		{
			if (Line.Audio)
			{
				const int32 Row = Speakers.AddUnique(Line.Speaker);
				AudioTrack->AddNewSoundOnRow(Line.Audio, SceneStart + TickResolution.AsFrameNumber(Time), Row);
			}
			Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
		}
	}

	const FText PerformanceTrackName = NSLOCTEXT("CinemafyEditor", "PerformanceTrack", "Cinemafy Performance");

	/** The speaker's skeletal mesh that can play this clip (matching skeleton): body or face. */
	USkeletalMeshComponent* FindMeshForClip(AActor* Actor, const UAnimSequenceBase* Clip)
	{
		if (!Actor || !Clip)
		{
			return nullptr;
		}
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
			if (Asset && Asset->GetSkeleton() == Clip->GetSkeleton())
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	/**
	 * Lays each line's body / face clips onto the speaker, on a "Cinemafy Performance" animation track
	 * per mesh (replacing the last take's).
	 */
	void PlacePerformances(ULevelSequence* Sequence, UMovieScene* MovieScene, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart)
	{
		struct FPlacement { USkeletalMeshComponent* Mesh; UAnimSequenceBase* Clip; FFrameNumber Start; };
		TArray<FPlacement> Placements;

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		float Time = 0.f;
		for (const FCinemafyLine& Line : Lines)
		{
			const FFrameNumber Start = SceneStart + TickResolution.AsFrameNumber(Time);
			for (UAnimSequenceBase* Clip : { Line.BodyAnimation.Get(), Line.FaceAnimation.Get() })
			{
				if (!Clip)
				{
					continue;
				}
				if (USkeletalMeshComponent* Mesh = FindMeshForClip(Line.Speaker, Clip))
				{
					Placements.Add({ Mesh, Clip, Start });
				}
				else
				{
					UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Director: %s doesn't match any mesh on %s (skeleton mismatch)."),
						*Clip->GetName(), *GetNameSafe(Line.Speaker));
				}
			}
			Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
		}

		TMap<USkeletalMeshComponent*, UMovieSceneSkeletalAnimationTrack*> Tracks;
		for (const FPlacement& Placement : Placements)
		{
			UMovieSceneSkeletalAnimationTrack** Found = Tracks.Find(Placement.Mesh);
			if (!Found)
			{
				const FGuid Binding = CinemafySequencer::BindObject(Sequence, Placement.Mesh);
				if (!Binding.IsValid())
				{
					continue;
				}

				// Replace the previous take's performance track on this mesh, leave any others alone.
				const UMovieScene* ConstMovieScene = MovieScene;
				if (const FMovieSceneBinding* Existing = ConstMovieScene->FindBinding(Binding))
				{
					TArray<UMovieSceneTrack*> Old;
					for (UMovieSceneTrack* Track : Existing->GetTracks())
					{
						if (Track && Track->IsA<UMovieSceneSkeletalAnimationTrack>() && Track->GetDisplayName().EqualTo(PerformanceTrackName))
						{
							Old.Add(Track);
						}
					}
					for (UMovieSceneTrack* Track : Old)
					{
						MovieScene->RemoveTrack(*Track);
					}
				}

				UMovieSceneSkeletalAnimationTrack* Track = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(Binding);
				Track->SetDisplayName(PerformanceTrackName);
				Found = &Tracks.Add(Placement.Mesh, Track);
			}
			(*Found)->AddNewAnimation(Placement.Start, Placement.Clip);
		}
	}

	/** Keys the director's SubtitleLineIndex: each line's index while it's spoken, -1 in between. */
	void BakeSubtitles(ULevelSequence* Sequence, UMovieScene* MovieScene, ACinemafySceneDirector* Director, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart)
	{
		if (!Lines.ContainsByPredicate([](const FCinemafyLine& Line) { return !Line.Text.IsEmpty(); }))
		{
			return;
		}

		const FGuid Binding = CinemafySequencer::BindActor(Sequence, Director);
		UMovieSceneIntegerTrack* Track = ReplacePropertyTrack<UMovieSceneIntegerTrack>(MovieScene, Binding, GET_MEMBER_NAME_CHECKED(ACinemafySceneDirector, SubtitleLineIndex));
		UMovieSceneIntegerSection* Section = AddInfiniteSection<UMovieSceneIntegerSection>(Track);
		FMovieSceneIntegerChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneIntegerChannel>(0) : nullptr;
		if (!Channel)
		{
			return;
		}

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		float Time = 0.f;
		Channel->GetData().AddKey(SceneStart - 1, -1);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			const FCinemafyLine& Line = Lines[Index];
			Channel->GetData().AddKey(SceneStart + TickResolution.AsFrameNumber(Time), Line.Text.IsEmpty() ? -1 : Index);
			Channel->GetData().AddKey(SceneStart + TickResolution.AsFrameNumber(Time + Line.Duration), -1);
			Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
		}
	}

	UMovieSceneCameraCutTrack* GetOrAddCutTrack(UMovieScene* MovieScene)
	{
		UMovieSceneTrack* Track = MovieScene->GetCameraCutTrack();
		if (!Track)
		{
			Track = MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
		}
		return Cast<UMovieSceneCameraCutTrack>(Track);
	}
}

int32 UCinemafyEditorLibrary::DirectSceneInSequencer(ACinemafySceneDirector* Director)
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!Director || !MovieScene)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DirectNoSequence", "Open the Level Sequence you want the scene cut into, then hit Direct."));
		return 0;
	}

	// Listen first if there's audio nobody has analyzed yet.
	const bool bNeedsAnalysis = Director->Lines.ContainsByPredicate([](const FCinemafyLine& Line) { return Line.Audio && !Line.Analysis.bValid; });
	if (bNeedsAnalysis)
	{
		AnalyzeDirectorAudio(Director);
	}

	const TArray<FCinemafyLine> EffectiveLines = Director->GetEffectiveLines();
	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(EffectiveLines, Director->GetStyle(), Director->Seed);
	if (Plan.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DirectNoLines", "Add some lines to the director's script (each needs a Speaker)."));
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("DirectScene", "Cinemafy: Direct Scene"));
	Director->Modify();
	Director->LastPlan = Plan;
	MovieScene->Modify();

	UMovieSceneCameraCutTrack* CutTrack = GetOrAddCutTrack(MovieScene);
	if (!CutTrack)
	{
		return 0;
	}
	RemoveDirectorCuts(Director, Sequence, CutTrack);

	// Cameras this director made on earlier takes, by shot. Reusing them keeps any hand tweaks.
	const FName DirectorTag = Director->GetDirectorTag();
	TMap<FName, ACineCameraActor*> ExistingCameras;
	for (TActorIterator<ACineCameraActor> It(Director->GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(DirectorTag))
		{
			for (const FName& Tag : It->Tags)
			{
				if (Tag.ToString().StartsWith(TEXT("Cinemafy.Shot.")))
				{
					ExistingCameras.Add(Tag, *It);
				}
			}
		}
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber SceneStart = CinemafySequencer::GetPlayheadTick(MovieScene);
	FFrameNumber SceneEnd = SceneStart;
	TMap<ACineCameraActor*, TArray<FCameraShotWindow>> CameraWindows;

	for (const FCinemafyPlannedShot& Shot : Plan)
	{
		const FName ShotTag(*FString::Printf(TEXT("Cinemafy.Shot.%s.%d.%s.%s.%d"), *Director->GetName(), static_cast<int32>(Shot.ShotType),
			Shot.Subject ? *Shot.Subject->GetName() : TEXT("-"), Shot.Counterpart ? *Shot.Counterpart->GetName() : TEXT("-"), Director->bFlipSide ? 1 : 0));

		ACineCameraActor* Camera = ExistingCameras.FindRef(ShotTag);
		if (!Camera)
		{
			FCinemafyShotRequest Request;
			Request.ShotType = Shot.ShotType;
			Request.Subject = Shot.Subject;
			Request.Counterpart = Shot.Counterpart;
			Request.Group = Shot.Group;
			Request.bUseRecommendedLens = false;
			Request.Lens = Shot.Lens;
			Request.bFlipSide = Director->bFlipSide;
			Camera = CreateShot(Request, false);
			if (!Camera)
			{
				continue;
			}
			Camera->Tags.Add(DirectorTag);
			Camera->Tags.Add(ShotTag);
			ExistingCameras.Add(ShotTag, Camera);
		}

		UCinemafyCameraFeelComponent* Feel = FindOrAddComponent<UCinemafyCameraFeelComponent>(Camera, TEXT("CinemafyFeel"));
		Feel->Feel = Shot.Feel;
		Feel->Seed = static_cast<int32>(GetTypeHash(Camera->GetFName()) % 1000);

		const FGuid Binding = CinemafySequencer::BindActor(Sequence, Camera);
		const FFrameNumber Start = SceneStart + TickResolution.AsFrameNumber(Shot.StartTime);
		const FFrameNumber End = SceneStart + TickResolution.AsFrameNumber(Shot.StartTime + Shot.Duration);
		if (UMovieSceneCameraCutSection* Cut = CutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(Binding), Start))
		{
			Cut->SetRange(TRange<FFrameNumber>(Start, End));
		}
		SceneEnd = FMath::Max(SceneEnd, End);
		CameraWindows.FindOrAdd(Camera).Add({ Start, End, &Shot });

		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Director: %s"), *Shot.Reason);
	}

	for (const TPair<ACineCameraActor*, TArray<FCameraShotWindow>>& Pair : CameraWindows)
	{
		BakeCameraMoves(Sequence, MovieScene, Pair.Key, Pair.Value);
	}
	BakeEmotions(Sequence, MovieScene, Plan, SceneStart);

	if (Director->bPlaceDialogueAudio)
	{
		PlaceDialogueAudio(MovieScene, EffectiveLines, SceneStart);
	}
	if (Director->bPlacePerformances)
	{
		PlacePerformances(Sequence, MovieScene, EffectiveLines, SceneStart);
	}
	if (Director->bShowSubtitles)
	{
		BakeSubtitles(Sequence, MovieScene, Director, EffectiveLines, SceneStart);
	}

	// Make sure the whole scene fits in the playback range.
	const TRange<FFrameNumber> Playback = MovieScene->GetPlaybackRange();
	if (Playback.GetUpperBoundValue() < SceneEnd)
	{
		MovieScene->SetPlaybackRange(TRange<FFrameNumber>(Playback.GetLowerBoundValue(), SceneEnd));
	}

	CinemafySequencer::NotifyChanged();
	SCinemafyShotBrowser::RefreshActive();

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Director, true, true);
	return Plan.Num();
}

int32 UCinemafyEditorLibrary::ClearDirectedShots(ACinemafySceneDirector* Director)
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	UMovieSceneCameraCutTrack* CutTrack = MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	if (!Director || !CutTrack)
	{
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("ClearDirected", "Cinemafy: Clear Directed Shots"));
	MovieScene->Modify();
	const int32 Removed = RemoveDirectorCuts(Director, Sequence, CutTrack);
	CinemafySequencer::NotifyChanged();
	SCinemafyShotBrowser::RefreshActive();
	return Removed;
}

namespace
{
	struct FObservedShot
	{
		ECinemafyShotType Type = ECinemafyShotType::MediumShot;
		float Duration = 0.f;
		float FocalLength = 0.f;
		bool bLowAngle = false;
		bool bMoving = false;
		ECinemafyCameraFeel Feel = ECinemafyCameraFeel::Locked;
	};

	/**
	 * Works out what kind of shot a camera is from what it actually sees: who's in frame, how much of
	 * them (frame height at the subject), whether a foreground shoulder is in the way, and the angle.
	 */
	bool ClassifyShot(const UCameraComponent* Camera, const TArray<AActor*>& Characters, FObservedShot& Out)
	{
		FMinimalViewInfo View;
		const_cast<UCameraComponent*>(Camera)->GetCameraView(0.f, View);

		const float Aspect = View.AspectRatio > 0.f ? View.AspectRatio : 16.f / 9.f;
		const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(View.FOV * 0.5f));
		const float TanHalfV = TanHalfH / Aspect;
		const FRotationMatrix Basis(View.Rotation);
		const FVector Forward = Basis.GetUnitAxis(EAxis::X);
		const FVector Right = Basis.GetUnitAxis(EAxis::Y);
		const FVector Up = Basis.GetUnitAxis(EAxis::Z);

		struct FSeen { FVector Eye; float Depth; float X; };
		TArray<FSeen> InFrame;
		TArray<FSeen> Foreground;
		for (const AActor* Character : Characters)
		{
			const FVector Eye = UCinemafyFramingLibrary::GetEyePoint(Character);
			const FVector Dir = Eye - View.Location;
			const float Depth = FVector::DotProduct(Dir, Forward);
			if (Depth <= 10.f)
			{
				continue;
			}
			const float X = FVector::DotProduct(Dir, Right) / (Depth * TanHalfH);
			const float Y = FVector::DotProduct(Dir, Up) / (Depth * TanHalfV);
			if (FMath::Abs(X) < 1.1f && FMath::Abs(Y) < 1.3f)
			{
				InFrame.Add({ Eye, Depth, X });
			}
			else if (FMath::Abs(X) < 1.6f && Depth < 150.f)
			{
				// Just off the edge and very close: a foreground shoulder.
				Foreground.Add({ Eye, Depth, X });
			}
		}

		if (InFrame.Num() == 0)
		{
			return false;
		}

		// The subject is whoever's nearest the center of frame (ignoring anything right on top of the lens).
		InFrame.Sort([](const FSeen& A, const FSeen& B) { return FMath::Abs(A.X) < FMath::Abs(B.X); });
		const FSeen* Subject = InFrame.FindByPredicate([](const FSeen& S) { return S.Depth > 80.f; });
		if (!Subject)
		{
			Subject = &InFrame[0];
		}
		const float FrameHeight = 2.f * Subject->Depth * TanHalfV;

		bool bOverShoulder = false;
		int32 PeersInFrame = 0;
		for (const FSeen& Other : InFrame)
		{
			if (&Other == Subject)
			{
				continue;
			}
			if (Other.Depth < Subject->Depth * 0.6f && Other.Depth < 150.f)
			{
				bOverShoulder = true;
			}
			else if (Other.Depth > Subject->Depth * 0.6f && Other.Depth < Subject->Depth * 1.6f)
			{
				++PeersInFrame;
			}
		}
		bOverShoulder |= Foreground.Num() > 0;

		Out.bLowAngle = View.Location.Z < Subject->Eye.Z - 50.f && FrameHeight > 70.f && FrameHeight < 220.f;

		if (PeersInFrame >= 2)                   { Out.Type = ECinemafyShotType::GroupShot; }
		else if (PeersInFrame > 0)               { Out.Type = FrameHeight > 180.f ? ECinemafyShotType::WideTwoShot : ECinemafyShotType::MediumTwoShot; }
		else if (bOverShoulder)                  { Out.Type = ECinemafyShotType::OverTheShoulder; }
		else if (Out.bLowAngle)                  { Out.Type = ECinemafyShotType::LowHero; }
		else if (FrameHeight > 80.f)             { Out.Type = ECinemafyShotType::MediumShot; }
		else if (FrameHeight > 49.f)             { Out.Type = ECinemafyShotType::MediumCloseUp; }
		else if (FrameHeight > 29.f)             { Out.Type = ECinemafyShotType::CloseUp; }
		else                                     { Out.Type = ECinemafyShotType::ExtremeCloseUp; }
		return true;
	}

	float Percentile(TArray<float> Values, float P)
	{
		if (Values.Num() == 0)
		{
			return 0.f;
		}
		Values.Sort();
		const int32 Index = FMath::Clamp(FMath::RoundToInt(P * (Values.Num() - 1)), 0, Values.Num() - 1);
		return Values[Index];
	}
}

int32 UCinemafyEditorLibrary::LearnDirectorStyleFromSequence(ACinemafySceneDirector* Director)
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	const UMovieSceneCameraCutTrack* CutTrack = MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	if (!Director || !Sequencer.IsValid() || !CutTrack)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LearnNoSequence", "Open a Level Sequence with camera cuts in Sequencer: the director will study its edit."));
		return 0;
	}

	// The cast: the director's characters, or every skeletal character in the level.
	TArray<AActor*> Characters;
	for (const FCinemafyLine& Line : Director->Lines)
	{
		if (Line.Speaker) { Characters.AddUnique(Line.Speaker); }
		if (Line.Listener) { Characters.AddUnique(Line.Listener); }
	}
	if (Characters.Num() == 0)
	{
		for (TActorIterator<AActor> It(Director->GetWorld()); It; ++It)
		{
			if (It->FindComponentByClass<USkeletalMeshComponent>() && !It->FindComponentByClass<UCameraComponent>())
			{
				Characters.Add(*It);
			}
		}
	}

	TArray<const UMovieSceneCameraCutSection*> Cuts;
	for (const UMovieSceneSection* Section : CutTrack->GetAllSections())
	{
		const UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Section);
		if (Cut && Cut->HasStartFrame() && Cut->HasEndFrame())
		{
			Cuts.Add(Cut);
		}
	}
	Cuts.Sort([](const UMovieSceneCameraCutSection& A, const UMovieSceneCameraCutSection& B) { return A.GetInclusiveStartFrame() < B.GetInclusiveStartFrame(); });

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FQualifiedFrameTime OriginalTime = Sequencer->GetLocalTime();
	TArray<FObservedShot> Observed;

	for (const UMovieSceneCameraCutSection* Cut : Cuts)
	{
		// Look at each shot as it actually plays (animated cameras and characters included).
		const FFrameNumber Start = Cut->GetInclusiveStartFrame();
		Sequencer->SetLocalTimeDirectly(FFrameTime(Start));
		Sequencer->ForceEvaluate();

		const AActor* CameraActor = nullptr;
		for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(Cut->GetCameraBindingID().GetGuid(), MovieSceneSequenceID::Root))
		{
			CameraActor = Cast<AActor>(Bound.Get());
			if (CameraActor)
			{
				break;
			}
		}
		const UCameraComponent* Camera = CameraActor ? CameraActor->FindComponentByClass<UCameraComponent>() : nullptr;

		FObservedShot Shot;
		if (!Camera || !ClassifyShot(Camera, Characters, Shot))
		{
			continue;
		}

		Shot.Duration = static_cast<float>(TickResolution.AsSeconds(FFrameTime(Cut->GetExclusiveEndFrame() - Start)));
		if (const UCineCameraComponent* Cine = Cast<UCineCameraComponent>(Camera))
		{
			Shot.FocalLength = Cine->CurrentFocalLength;
		}
		if (const UCinemafyCameraFeelComponent* Feel = CameraActor->FindComponentByClass<UCinemafyCameraFeelComponent>())
		{
			Shot.Feel = Feel->Feel;
		}
		Shot.bMoving = CameraActor->FindComponentByClass<UCinemafyCameraMoveComponent>() != nullptr;
		Observed.Add(Shot);
	}

	Sequencer->SetLocalTimeDirectly(ConvertFrameTime(OriginalTime.Time, OriginalTime.Rate, TickResolution));
	Sequencer->ForceEvaluate();

	if (Observed.Num() < 3)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LearnTooFew", "Not enough recognizable shots to learn from (need at least 3 cuts with characters in frame)."));
		return Observed.Num();
	}

	FCinemafyDirectorStyle Style = UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood);

	// Shot preferences: how much more (or less) often each type appears than an even spread would.
	TMap<ECinemafyShotType, int32> Counts;
	TArray<float> Durations;
	int32 LowAngle = 0;
	int32 Moving = 0;
	float LensLogSum = 0.f;
	int32 LensSamples = 0;
	TMap<ECinemafyCameraFeel, int32> Feels;
	for (const FObservedShot& Shot : Observed)
	{
		Counts.FindOrAdd(Shot.Type)++;
		Durations.Add(Shot.Duration);
		LowAngle += Shot.bLowAngle ? 1 : 0;
		Moving += Shot.bMoving ? 1 : 0;
		Feels.FindOrAdd(Shot.Feel)++;
		if (Shot.FocalLength > 0.f)
		{
			const float Recommended = UCinemafyCameraLibrary::GetPrimeFocalLength(UCinemafyFramingLibrary::GetRecommendedPrime(Shot.Type));
			LensLogSum += FMath::Log2(Shot.FocalLength / Recommended);
			++LensSamples;
		}
	}

	const float EvenShare = 1.f / Style.ShotPreference.Num();
	for (TPair<ECinemafyShotType, float>& Pref : Style.ShotPreference)
	{
		const float Share = static_cast<float>(Counts.FindRef(Pref.Key)) / Observed.Num();
		Pref.Value = FMath::Clamp(Share / EvenShare, 0.1f, 2.5f);
	}

	// Pacing: the slower half of shots defines "calm", the quicker half "intense".
	const float P10 = Percentile(Durations, 0.1f);
	const float P50 = Percentile(Durations, 0.5f);
	const float P90 = Percentile(Durations, 0.9f);
	Style.CalmShotLength = FVector2D(FMath::Max(P50, 0.6f), FMath::Max(P90, P50 + 0.5f));
	Style.IntenseShotLength = FVector2D(FMath::Max(P10, 0.6f), FMath::Max(P50, P10 + 0.3f));

	// Quick cutaways are mostly reactions.
	const int32 QuickCuts = Durations.FilterByPredicate([P50](float D) { return D < P50 * 0.6f; }).Num();
	Style.ReactionShotChance = FMath::Clamp(0.15f + 0.6f * QuickCuts / Observed.Num(), 0.f, 1.f);

	Style.LowAngleBias = FMath::Clamp(4.f * LowAngle / Observed.Num(), 0.05f, 1.f);
	Style.MoveChance = FMath::Clamp(static_cast<float>(Moving) / Observed.Num(), 0.f, 1.f);
	Style.LensBias = LensSamples > 0 ? FMath::Clamp(FMath::RoundToInt((LensLogSum / LensSamples) / 0.5f), -2, 2) : 0;
	Style.bEstablishingShot = Observed[0].Type == ECinemafyShotType::WideTwoShot || Observed[0].Type == ECinemafyShotType::MediumTwoShot
		|| Observed[0].Type == ECinemafyShotType::GroupShot;

	// Feel: the most common one is "calm"; the wildest one seen is "intense".
	ECinemafyCameraFeel MostCommon = ECinemafyCameraFeel::Tripod;
	int32 MostCount = 0;
	ECinemafyCameraFeel Wildest = ECinemafyCameraFeel::Locked;
	for (const TPair<ECinemafyCameraFeel, int32>& Pair : Feels)
	{
		if (Pair.Value > MostCount) { MostCount = Pair.Value; MostCommon = Pair.Key; }
		if (Pair.Key != ECinemafyCameraFeel::Custom && Pair.Key > Wildest) { Wildest = Pair.Key; }
	}
	Style.CalmFeel = MostCommon;
	Style.IntenseFeel = FMath::Max(Wildest, MostCommon);

	const FScopedTransaction Transaction(LOCTEXT("LearnStyle", "Cinemafy: Learn Director Style"));
	Director->Modify();
	Director->CustomStyle = Style;
	Director->Style = ECinemafyDirectorPreset::Custom;

	// Show what it learned, in plain words.
	FString Summary = FString::Printf(TEXT("Learned from %d shots in %s:\n\n"), Observed.Num(), *Sequence->GetName());
	for (const TPair<ECinemafyShotType, int32>& Pair : Counts)
	{
		Summary += FString::Printf(TEXT("  %s: %d\n"), *StaticEnum<ECinemafyShotType>()->GetDisplayNameTextByValue(static_cast<int64>(Pair.Key)).ToString(), Pair.Value);
	}
	Summary += FString::Printf(TEXT("\nShot length: median %.1fs (quick %.1fs, long %.1fs)\nLens bias: %+d  |  Low angles: %d  |  Moving shots: %d\nOpens wide: %s\n\nThe director's style is now Custom. Hit Direct to cut a scene your way."),
		P50, P10, P90, Style.LensBias, LowAngle, Moving, Style.bEstablishingShot ? TEXT("yes") : TEXT("no"));
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Summary));
	return Observed.Num();
}

FCinemafyAudioAnalysis UCinemafyEditorLibrary::AnalyzeDialogueSound(USoundWave* Sound)
{
	FCinemafyAudioAnalysis Result;
	if (!Sound)
	{
		return Result;
	}

	TArray<uint8> RawPCM;
	uint32 SampleRate = 0;
	uint16 NumChannels = 0;
	if (!Sound->GetImportedSoundWaveData(RawPCM, SampleRate, NumChannels) || SampleRate == 0 || NumChannels == 0)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: couldn't read audio data from %s."), *Sound->GetName());
		return Result;
	}

	// 16-bit interleaved PCM -> mono floats.
	const int16* Samples = reinterpret_cast<const int16*>(RawPCM.GetData());
	const int32 NumFrames = RawPCM.Num() / (sizeof(int16) * NumChannels);
	Result.Duration = static_cast<float>(NumFrames) / SampleRate;

	// Loudness in 20 ms windows.
	const int32 Window = FMath::Max(1, static_cast<int32>(SampleRate / 50));
	TArray<float> WindowDb;
	TArray<float> WindowPower;
	for (int32 Start = 0; Start + Window <= NumFrames; Start += Window)
	{
		double SumSquares = 0.0;
		for (int32 Frame = Start; Frame < Start + Window; ++Frame)
		{
			float Mono = 0.f;
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				Mono += Samples[Frame * NumChannels + Channel] / 32768.f;
			}
			Mono /= NumChannels;
			SumSquares += Mono * Mono;
		}
		const float Power = static_cast<float>(SumSquares / Window);
		WindowPower.Add(Power);
		WindowDb.Add(10.f * FMath::LogX(10.f, Power + 1e-12f));
	}
	if (WindowDb.Num() < 3)
	{
		return Result;
	}

	// Speech = clearly above the room tone.
	TArray<float> SortedDb = WindowDb;
	SortedDb.Sort();
	const float NoiseFloor = SortedDb[SortedDb.Num() / 10];
	const float SpeechThreshold = FMath::Max(NoiseFloor + 10.f, -50.f);

	double SpeechPower = 0.0;
	TArray<float> SpeechDb;
	for (int32 Index = 0; Index < WindowDb.Num(); ++Index)
	{
		if (WindowDb[Index] > SpeechThreshold)
		{
			SpeechPower += WindowPower[Index];
			SpeechDb.Add(WindowDb[Index]);
		}
	}
	if (SpeechDb.Num() == 0)
	{
		return Result;
	}

	SpeechDb.Sort();
	Result.LoudnessDb = 10.f * FMath::LogX(10.f, static_cast<float>(SpeechPower / SpeechDb.Num()) + 1e-12f);
	Result.PeakDb = SpeechDb[FMath::Min(SpeechDb.Num() - 1, FMath::RoundToInt(SpeechDb.Num() * 0.95f))];
	Result.ActiveRatio = static_cast<float>(SpeechDb.Num()) / WindowDb.Num();

	// Speech rate: count loudness bursts (syllable nuclei) at least 80 ms apart that rise 4 dB above the valley before them.
	int32 Bursts = 0;
	int32 LastBurst = -100;
	float Valley = WindowDb[0];
	for (int32 Index = 2; Index < WindowDb.Num() - 2; ++Index)
	{
		const float V = WindowDb[Index];
		Valley = FMath::Min(Valley, V);
		const bool bLocalPeak = V >= WindowDb[Index - 1] && V >= WindowDb[Index + 1] && V >= WindowDb[Index - 2] && V >= WindowDb[Index + 2];
		if (bLocalPeak && V > SpeechThreshold && V - Valley > 4.f && Index - LastBurst >= 4)
		{
			++Bursts;
			LastBurst = Index;
			Valley = V;
		}
	}
	// Pauses: runs of 120 ms+ below the speech threshold, with talking on both sides.
	{
		int32 RunStart = -1;
		bool bSpokeBefore = false;
		for (int32 Index = 0; Index < WindowDb.Num(); ++Index)
		{
			const bool bSilent = WindowDb[Index] <= SpeechThreshold;
			if (bSilent && RunStart < 0 && bSpokeBefore)
			{
				RunStart = Index;
			}
			else if (!bSilent)
			{
				if (RunStart >= 0 && Index - RunStart >= 6)
				{
					Result.PauseTimes.Add((RunStart + Index) * 0.5f * 0.02f);
				}
				RunStart = -1;
				bSpokeBefore = true;
			}
		}
	}

	// Emphasis: the loudest bursts, clearly above the line's average, loudest first.
	{
		TArray<TPair<float, float>> Peaks; // (dB, time)
		for (int32 Index = 2; Index < WindowDb.Num() - 2; ++Index)
		{
			const float V = WindowDb[Index];
			if (V >= WindowDb[Index - 1] && V >= WindowDb[Index + 1] && V >= WindowDb[Index - 2] && V >= WindowDb[Index + 2] && V > Result.LoudnessDb + 4.f)
			{
				Peaks.Emplace(V, Index * 0.02f);
			}
		}
		Peaks.Sort([](const TPair<float, float>& A, const TPair<float, float>& B) { return A.Key > B.Key; });
		for (const TPair<float, float>& Peak : Peaks)
		{
			// Keep distinct moments, not neighbouring syllables of the same word.
			if (!Result.EmphasisTimes.ContainsByPredicate([&Peak](float T) { return FMath::Abs(T - Peak.Value) < 0.4f; }))
			{
				Result.EmphasisTimes.Add(Peak.Value);
			}
			if (Result.EmphasisTimes.Num() >= 3)
			{
				break;
			}
		}
	}

	const float SpeechSeconds = SpeechDb.Num() * 0.02f;
	Result.SyllablesPerSecond = SpeechSeconds > 0.f ? Bursts / SpeechSeconds : 0.f;
	Result.bValid = true;
	return Result;
}

int32 UCinemafyEditorLibrary::AnalyzeDirectorAudio(ACinemafySceneDirector* Director)
{
	if (!Director)
	{
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("AnalyzeAudio", "Cinemafy: Analyze Dialogue Audio"));
	Director->Modify();

	int32 Analyzed = 0;
	for (FCinemafyLine& Line : Director->Lines)
	{
		USoundWave* Wave = Cast<USoundWave>(Line.Audio);
		Line.Analysis = AnalyzeDialogueSound(Wave);
		Line.AudioIntensity = -1.f;
		if (Line.Analysis.bValid)
		{
			++Analyzed;
			if (Line.bUseAudioDuration)
			{
				Line.Duration = FMath::Max(Line.Analysis.Duration, 0.2f);
			}
		}
		else if (Line.Audio && !Wave)
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: %s isn't a Sound Wave; only Sound Waves can be analyzed."), *Line.Audio->GetName());
		}
	}

	if (Analyzed == 0)
	{
		return 0;
	}

	// Intensity is relative to the scene: what's loud, sharp and fast for *this* conversation.
	auto Stats = [&Director](TFunctionRef<float(const FCinemafyAudioAnalysis&)> Get, float MinStd, float& OutMean, float& OutStd)
	{
		TArray<float> Values;
		for (const FCinemafyLine& Line : Director->Lines)
		{
			if (Line.Analysis.bValid)
			{
				Values.Add(Get(Line.Analysis));
			}
		}
		OutMean = 0.f;
		for (float V : Values) { OutMean += V; }
		OutMean /= Values.Num();
		float Variance = 0.f;
		for (float V : Values) { Variance += FMath::Square(V - OutMean); }
		OutStd = FMath::Max(FMath::Sqrt(Variance / Values.Num()), MinStd);
	};

	float LoudMean, LoudStd, RateMean, RateStd, PunchMean, PunchStd;
	Stats([](const FCinemafyAudioAnalysis& A) { return A.LoudnessDb; }, 3.f, LoudMean, LoudStd);
	Stats([](const FCinemafyAudioAnalysis& A) { return A.SyllablesPerSecond; }, 0.5f, RateMean, RateStd);
	Stats([](const FCinemafyAudioAnalysis& A) { return A.PeakDb - A.LoudnessDb; }, 2.f, PunchMean, PunchStd);

	for (FCinemafyLine& Line : Director->Lines)
	{
		if (!Line.Analysis.bValid)
		{
			continue;
		}

		const FCinemafyAudioAnalysis& A = Line.Analysis;
		if (Analyzed == 1)
		{
			// Nothing to compare against: judge loudness on an absolute scale.
			Line.AudioIntensity = static_cast<float>(FMath::GetMappedRangeValueClamped(FVector2D(-30.0, -10.0), FVector2D(0.2, 0.8), static_cast<double>(A.LoudnessDb)));
			continue;
		}

		const float LoudZ = (A.LoudnessDb - LoudMean) / LoudStd;
		const float RateZ = (A.SyllablesPerSecond - RateMean) / RateStd;
		const float PunchZ = ((A.PeakDb - A.LoudnessDb) - PunchMean) / PunchStd;
		Line.AudioIntensity = FMath::Clamp(0.5f + 0.2f * LoudZ + 0.12f * RateZ + 0.08f * PunchZ, 0.05f, 0.98f);

		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: \"%s\" %.1f dB, %.1f syl/s -> audio intensity %.2f"),
			*Line.Note, A.LoudnessDb, A.SyllablesPerSecond, Line.AudioIntensity);
	}
	return Analyzed;
}

ACinemafySceneDirector* UCinemafyEditorLibrary::CreateSceneDirectorFromSelection(ECinemafyDirectorPreset Style)
{
	TArray<AActor*> Characters;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			Characters.Add(Actor);
		}
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	FVector Location = FVector::ZeroVector;
	for (const AActor* Character : Characters)
	{
		Location += Character->GetActorLocation() / Characters.Num();
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateDirector", "Cinemafy: Create Scene Director"));
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transactional;
	ACinemafySceneDirector* Director = World->SpawnActor<ACinemafySceneDirector>(Location, FRotator::ZeroRotator, Params);
	if (!Director)
	{
		return nullptr;
	}
	Director->SetActorLabel(TEXT("CinemafyDirector"));
	Director->Style = Style;

	// A starter script: two characters trading lines as the tension builds. Replace with your real lines.
	if (Characters.Num() >= 2)
	{
		const float Intensities[] = { 0.2f, 0.35f, 0.5f, 0.65f, 0.8f, 0.95f };
		const int32 CastSize = Characters.Num();
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Intensities)); ++Index)
		{
			// Round-robin: each character speaks to the next one around the group.
			FCinemafyLine Line;
			Line.Speaker = Characters[Index % CastSize];
			Line.Listener = Characters[(Index + 1) % CastSize];
			Line.Intensity = Intensities[Index];
			Line.Duration = FMath::Lerp(4.f, 2.5f, Line.Intensity);
			Line.PauseAfter = 0.6f;
			Line.Note = FString::Printf(TEXT("Line %d: %s"), Index + 1, *Line.Speaker->GetActorLabel());
			Director->Lines.Add(Line);
		}
	}
	else if (Characters.Num() == 1)
	{
		FCinemafyLine Line;
		Line.Speaker = Characters[0];
		Line.Note = TEXT("Monologue");
		Director->Lines.Add(Line);
	}

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Director, true, true);
	return Director;
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
