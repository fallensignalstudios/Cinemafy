#include "CinemafyEditorLibrary.h"
#include "CinemafyChoreoTools.h"
#include "CinemafyChoreography.h"
#include "CinemafyChoreographyLibrary.h"
#include "GameFramework/Character.h"
#include "CinemafyModule.h"
#include "CinemafySequencerUtils.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyFollowComponent.h"
#include "Sound/SoundWave.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Animation/AnimSequenceBase.h"
#include "Tracks/MovieSceneIntegerTrack.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Tracks/MovieSceneSlomoTrack.h"
#include "Sections/MovieSceneSlomoSection.h"
#include "Engine/SkeletalMesh.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyGazeComponent.h"
#include "CinemafyLightRig.h"
#include "CinemafyListenerComponent.h"
#include "CinemafyFaceComponent.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafySceneDirector.h"
#include "CinemafyDirectorStyleAsset.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Misc/FileHelper.h"
#include "IAssetTools.h"
#include "SCinemafyShotBrowser.h"
#include "EngineUtils.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneByteSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneByteTrack.h"
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
#include "Tracks/MovieSceneFadeTrack.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Tracks/MovieSceneSlomoTrack.h"
#include "Sections/MovieSceneSlomoSection.h"
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
		// One camera has one kind of move; shots that wanted another kind stay still on this camera.
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		for (const FCameraShotWindow& Window : Windows)
		{
			Channel->AddLinearKey(Window.Start, 0.f);
			if (Window.Shot->bMove && Window.Shot->Move == Move->Move)
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

	/**
	 * Focus for shots with two people in them: the camera's Eye Focus holds Subject (RackAlpha 0) and
	 * Counterpart (RackAlpha 1), and RackAlpha is keyed to whoever's talking, racking at each speaker change.
	 */
	void BakeRackFocus(ULevelSequence* Sequence, UMovieScene* MovieScene, ACineCameraActor* Camera, const TArray<FCameraShotWindow>& Windows)
	{
		const bool bAnyTwoShot = Windows.ContainsByPredicate([](const FCameraShotWindow& W)
		{
			return W.Shot->ShotType == ECinemafyShotType::WideTwoShot || W.Shot->ShotType == ECinemafyShotType::MediumTwoShot;
		});
		if (!bAnyTwoShot)
		{
			return;
		}

		UCinemafyFocusComponent* Focus = FindOrAddComponent<UCinemafyFocusComponent>(Camera, TEXT("CinemafyFocus"));
		const FCinemafyPlannedShot& Reference = *Windows[0].Shot;
		Focus->FocusTarget = Reference.Subject;
		Focus->RackTarget = Reference.Counterpart;

		const FGuid Binding = CinemafySequencer::BindObject(Sequence, Focus);
		if (!Binding.IsValid())
		{
			return;
		}

		UMovieSceneFloatTrack* Track = ReplacePropertyTrack<UMovieSceneFloatTrack>(MovieScene, Binding, GET_MEMBER_NAME_CHECKED(UCinemafyFocusComponent, RackAlpha));
		UMovieSceneFloatSection* Section = AddInfiniteSection<UMovieSceneFloatSection>(Track);
		FMovieSceneFloatChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0) : nullptr;
		if (!Channel)
		{
			return;
		}

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		auto AlphaFor = [&Reference](const AActor* Who) { return Who == Reference.Counterpart ? 1.f : 0.f; };
		for (const FCameraShotWindow& Window : Windows)
		{
			float Current = AlphaFor(Window.Shot->FocusOn);
			Channel->AddLinearKey(Window.Start, Current);
			for (const FCinemafyRackEvent& Rack : Window.Shot->RackEvents)
			{
				// A focus puller starts the pull just as the new voice comes in.
				const FFrameNumber PullStart = Window.Start + TickResolution.AsFrameNumber(FMath::Max(Rack.Time - 0.1f, 0.f));
				const FFrameNumber PullEnd = PullStart + TickResolution.AsFrameNumber(0.6);
				const float Target = AlphaFor(Rack.FocusOn);
				Channel->AddLinearKey(PullStart, Current);
				Channel->AddLinearKey(FMath::Min(PullEnd, Window.End), Target);
				Current = Target;
			}
			Channel->AddLinearKey(Window.End, Current);
		}
	}

	/** Keys each speaker's Face emotion at the start of lines that set one. */
	void BakeEmotions(ULevelSequence* Sequence, UMovieScene* MovieScene, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart)
	{
		struct FEmotionKey { float StartTime; ECinemafyEmotion Emotion; };
		TMap<AActor*, TArray<FEmotionKey>> ByCharacter;
		float Time = 0.f;
		for (const FCinemafyLine& Line : Lines)
		{
			if (Line.bSetEmotion && Line.Speaker)
			{
				ByCharacter.FindOrAdd(Line.Speaker).Add({ Time, Line.Emotion });
			}
			Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
		}

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		for (const TPair<AActor*, TArray<FEmotionKey>>& Pair : ByCharacter)
		{
			UCinemafyFaceComponent* Face = FindOrAddComponent<UCinemafyFaceComponent>(Pair.Key, TEXT("CinemafyFace"));
			const FGuid Binding = CinemafySequencer::BindObject(Sequence, Face);
			if (!Binding.IsValid())
			{
				continue;
			}

			UMovieSceneByteTrack* Track = ReplacePropertyTrack<UMovieSceneByteTrack>(MovieScene, Binding, GET_MEMBER_NAME_CHECKED(UCinemafyFaceComponent, Emotion));
			Track->SetEnum(StaticEnum<ECinemafyEmotion>());
			UMovieSceneByteSection* Section = AddInfiniteSection<UMovieSceneByteSection>(Track);
			FMovieSceneByteChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneByteChannel>(0) : nullptr;
			if (!Channel)
			{
				continue;
			}

			for (const FEmotionKey& Key : Pair.Value)
			{
				Channel->GetData().AddKey(SceneStart + TickResolution.AsFrameNumber(Key.StartTime), static_cast<uint8>(Key.Emotion));
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

	const FText TransitionTrackName = NSLOCTEXT("CinemafyEditor", "TransitionTrack", "Cinemafy Transitions");

	bool IsOverlayTransition(ECinemafyTransition Type)
	{
		return Type == ECinemafyTransition::Wipe || Type == ECinemafyTransition::Dissolve
			|| Type == ECinemafyTransition::DipToWhite || Type == ECinemafyTransition::WhipPan;
	}

	/**
	 * Fades through black become a native Fade track (visible in the editor and in renders); wipes and
	 * dissolves become cues on the director, which plays them over the game view.
	 */
	void BakeTransitions(ULevelSequence* Sequence, UMovieScene* MovieScene, ACinemafySceneDirector* Director,
		const TArray<FCinemafyPlannedShot>& Plan, FFrameNumber SceneStart, FFrameNumber SceneEnd)
	{
		const FFrameRate TickResolution = MovieScene->GetTickResolution();

		// Replace last take's fade track.
		TArray<UMovieSceneTrack*> OldTracks;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (Track && Track->IsA<UMovieSceneFadeTrack>() && Track->GetDisplayName().EqualTo(TransitionTrackName))
			{
				OldTracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : OldTracks)
		{
			MovieScene->RemoveTrack(*Track);
		}

		FMovieSceneFloatChannel* FadeChannel = nullptr;
		auto GetFadeChannel = [&]()
		{
			if (!FadeChannel)
			{
				UMovieSceneFadeTrack* FadeTrack = MovieScene->AddTrack<UMovieSceneFadeTrack>();
				FadeTrack->SetDisplayName(TransitionTrackName);
				if (UMovieSceneFadeSection* Section = AddInfiniteSection<UMovieSceneFadeSection>(FadeTrack))
				{
					FadeChannel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
					if (FadeChannel)
					{
						FadeChannel->AddLinearKey(SceneStart - 1, 0.f);
					}
				}
			}
			return FadeChannel;
		};

		TArray<TPair<FFrameNumber, int32>> Cues;
		for (int32 Index = 0; Index < Plan.Num(); ++Index)
		{
			const FCinemafyPlannedShot& Shot = Plan[Index];
			const FFrameNumber Cut = SceneStart + TickResolution.AsFrameNumber(Shot.StartTime);
			const FFrameNumber Half = TickResolution.AsFrameNumber(Shot.TransitionDuration * 0.5f);

			if (Shot.TransitionIn == ECinemafyTransition::FadeThroughBlack)
			{
				if (FMovieSceneFloatChannel* Channel = GetFadeChannel())
				{
					if (Index == 0)
					{
						// Fade in from black.
						Channel->AddLinearKey(Cut, 1.f);
						Channel->AddLinearKey(Cut + Half + Half, 0.f);
					}
					else
					{
						Channel->AddLinearKey(Cut - Half, 0.f);
						Channel->AddLinearKey(Cut, 1.f);
						Channel->AddLinearKey(Cut + Half, 0.f);
					}
				}
			}
			else if (IsOverlayTransition(Shot.TransitionIn))
			{
				Cues.Emplace(Cut, Index);
			}
		}

		const FCinemafyPlannedShot& Last = Plan.Last();
		if (Last.TransitionOut == ECinemafyTransition::FadeThroughBlack)
		{
			if (FMovieSceneFloatChannel* Channel = GetFadeChannel())
			{
				// Fade out to black at the end of the scene.
				Channel->AddLinearKey(SceneEnd - TickResolution.AsFrameNumber(Last.TransitionDuration), 0.f);
				Channel->AddLinearKey(SceneEnd, 1.f);
			}
		}
		else if (IsOverlayTransition(Last.TransitionOut))
		{
			// Fire just before the end so the last shot is still there to capture.
			Cues.Emplace(SceneEnd - TickResolution.AsFrameNumber(1.0 / 30.0), Plan.Num());
		}

		const FGuid Binding = CinemafySequencer::BindActor(Sequence, Director);
		UMovieSceneIntegerTrack* CueTrack = ReplacePropertyTrack<UMovieSceneIntegerTrack>(MovieScene, Binding, GET_MEMBER_NAME_CHECKED(ACinemafySceneDirector, TransitionCue));
		UMovieSceneIntegerSection* CueSection = AddInfiniteSection<UMovieSceneIntegerSection>(CueTrack);
		if (FMovieSceneIntegerChannel* CueChannel = CueSection ? CueSection->GetChannelProxy().GetChannel<FMovieSceneIntegerChannel>(0) : nullptr)
		{
			CueChannel->GetData().AddKey(SceneStart - 1, -1);
			for (const TPair<FFrameNumber, int32>& Cue : Cues)
			{
				CueChannel->GetData().AddKey(Cue.Key, Cue.Value);
			}
		}
	}

	const FText MusicTrackName = NSLOCTEXT("CinemafyEditor", "MusicTrack", "Cinemafy Music");

	void PlaceMusic(UMovieScene* MovieScene, USoundBase* Music, FFrameNumber Start)
	{
		TArray<UMovieSceneTrack*> OldTracks;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (Track && Track->IsA<UMovieSceneAudioTrack>() && Track->GetDisplayName().EqualTo(MusicTrackName))
			{
				OldTracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : OldTracks)
		{
			MovieScene->RemoveTrack(*Track);
		}
		if (Music)
		{
			UMovieSceneAudioTrack* Track = MovieScene->AddTrack<UMovieSceneAudioTrack>();
			Track->SetDisplayName(MusicTrackName);
			Track->AddNewSoundOnRow(Music, Start, 0);
		}
	}

	const FText SpeedRampTrackName = NSLOCTEXT("CinemafyEditor", "SpeedRampTrack", "Cinemafy Speed Ramps");

	/** Speed ramps around slowed impacts, as a native Play Rate track: ramp down into the hit, hold, ramp back. */
	void BakeSpeedRamps(UMovieScene* MovieScene, const TArray<FCinemafyPlannedShot>& Plan, FFrameNumber SceneStart)
	{
		TArray<UMovieSceneTrack*> OldTracks;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (Track && Track->IsA<UMovieSceneSlomoTrack>() && Track->GetDisplayName().EqualTo(SpeedRampTrackName))
			{
				OldTracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : OldTracks)
		{
			MovieScene->RemoveTrack(*Track);
		}

		if (!Plan.ContainsByPredicate([](const FCinemafyPlannedShot& Shot) { return Shot.bSlowMo; }))
		{
			return;
		}

		UMovieSceneSlomoTrack* Track = MovieScene->AddTrack<UMovieSceneSlomoTrack>();
		Track->SetDisplayName(SpeedRampTrackName);
		UMovieSceneSlomoSection* Section = AddInfiniteSection<UMovieSceneSlomoSection>(Track);
		FMovieSceneFloatChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0) : nullptr;
		if (!Channel)
		{
			return;
		}

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		Channel->AddLinearKey(SceneStart, 1.f);
		for (const FCinemafyPlannedShot& Shot : Plan)
		{
			if (!Shot.bSlowMo)
			{
				continue;
			}
			const float Hit = Shot.StartTime + Shot.SlowMoTime;
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(FMath::Max(Hit - 0.15f, 0.f)), 1.f);
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(Hit), 0.3f);
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(Hit + 0.5f), 0.3f);
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(Hit + 0.8f), 1.f);
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

	TArray<FCinemafyLine> DirectedLines = Director->GetDirectedLines();

	// Who's walking? Play through the sequence and see how far each speaker travels during their line.
	if (const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer())
	{
		const FQualifiedFrameTime OriginalTime = Sequencer->GetLocalTime();
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const FFrameNumber SceneStart = CinemafySequencer::GetPlayheadTick(MovieScene);
		float Time = 0.f;
		for (FCinemafyLine& Line : DirectedLines)
		{
			if (Line.Speaker && !Line.bMoving)
			{
				Sequencer->SetLocalTimeDirectly(FFrameTime(SceneStart + TickResolution.AsFrameNumber(Time)));
				Sequencer->ForceEvaluate();
				const FVector From = Line.Speaker->GetActorLocation();
				Sequencer->SetLocalTimeDirectly(FFrameTime(SceneStart + TickResolution.AsFrameNumber(Time + Line.Duration)));
				Sequencer->ForceEvaluate();
				Line.bMoving = FVector::Dist2D(From, Line.Speaker->GetActorLocation()) > 150.f;
			}
			Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
		}
		Sequencer->SetLocalTimeDirectly(ConvertFrameTime(OriginalTime.Time, OriginalTime.Rate, TickResolution));
		Sequencer->ForceEvaluate();
	}

	TArray<FCinemafyPlannedShot> NewPlan = Director->PlanDirectedLines(DirectedLines);

	// Shots locked by hand survive the new take; the director plans around them.
	TArray<FCinemafyPlannedShot> Locked = Director->LastPlan.FilterByPredicate([](const FCinemafyPlannedShot& Shot) { return Shot.bLocked; });
	if (Locked.Num() > 0)
	{
		UCinemafyDirectorLibrary::ApplyLockedShots(NewPlan, Locked);
	}

	// Music: find the beat, then let cuts land on it.
	if (Director->Music)
	{
		if (!Director->MusicAnalysis.bValid || Director->MusicAnalyzedFrom != Director->Music)
		{
			Director->Modify();
			Director->MusicAnalysis = AnalyzeMusic(Cast<USoundWave>(Director->Music));
			Director->MusicAnalyzedFrom = Director->Music;
		}
		if (Director->bCutToMusic && Director->MusicAnalysis.bValid)
		{
			TArray<float> SceneBeats;
			for (const float Beat : Director->MusicAnalysis.Beats)
			{
				SceneBeats.Add(Beat + Director->MusicOffset);
			}
			UCinemafyDirectorLibrary::SnapPlanToBeats(NewPlan, DirectedLines, SceneBeats);
		}
	}
	if (NewPlan.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DirectNoLines", "Add some lines to the director's script (each needs a Speaker)."));
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("DirectScene", "Cinemafy: Direct Scene"));
	Director->Modify();
	Director->LastPlan = NewPlan;
	Director->LastDirectedLines = DirectedLines;
	UCinemafyDirectorLibrary::ReadMoments(Director->LastDirectedLines);
	Director->LastDecisionSummary = UCinemafyDirectorLibrary::SummarizeDecisions(NewPlan, Director->LastDirectedLines);
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Director: %s"), *Director->LastDecisionSummary);
	return BakeDirectorPlan(Director, false);
}

int32 UCinemafyEditorLibrary::BakeDirectorPlan(ACinemafySceneDirector* Director, bool bAtLastSceneStart)
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!Director || !MovieScene || Director->LastPlan.Num() == 0)
	{
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("BakeScene", "Cinemafy: Bake Directed Scene"));
	Director->Modify();
	MovieScene->Modify();

	const TArray<FCinemafyPlannedShot>& Plan = Director->LastPlan;
	const TArray<FCinemafyLine>& EffectiveLines = Director->LastDirectedLines;
	const FCinemafyDirectorStyle DirectorStyle = Director->GetStyle();
	Director->LastPlanCameras.Reset();
	Director->LastPlanCameras.SetNum(Plan.Num());

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
	const FFrameNumber SceneStart = bAtLastSceneStart ? FFrameNumber(Director->LastSceneStartTick) : CinemafySequencer::GetPlayheadTick(MovieScene);
	FFrameNumber SceneEnd = SceneStart;
	TMap<ACineCameraActor*, TArray<FCameraShotWindow>> CameraWindows;
	Director->LastSceneStartTick = SceneStart.Value;

	for (int32 ShotIndex = 0; ShotIndex < Plan.Num(); ++ShotIndex)
	{
		const FCinemafyPlannedShot& Shot = Plan[ShotIndex];
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

		Director->LastPlanCameras[ShotIndex] = Camera;

		// Walk-and-talk cameras ride a follow rig, locked to the subject at the offset they were framed at.
		if (Shot.ShotType == ECinemafyShotType::TrackingLead || Shot.ShotType == ECinemafyShotType::TrackingFollow)
		{
			UCinemafyFollowComponent* Follow = FindOrAddComponent<UCinemafyFollowComponent>(Camera, TEXT("CinemafyFollow"));
			if (!Follow->Target)
			{
				Follow->Target = Shot.Subject;
				Follow->CaptureOffset();
			}
		}

		const FCinemafyDirectorStyle& Style = DirectorStyle;
		if (Style.bApplyLook)
		{
			UCineCameraComponent* Cine = Camera->GetCineCameraComponent();
			Cine->Modify();
			UCinemafyCameraLibrary::ApplyLook(Cine, Style.Look, Style.LookStrength);
			UCinemafyCameraLibrary::SetLetterbox(Cine, Style.LetterboxAspect);
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
		BakeRackFocus(Sequence, MovieScene, Pair.Key, Pair.Value);
	}
	BakeEmotions(Sequence, MovieScene, EffectiveLines, SceneStart);

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

	BakeTransitions(Sequence, MovieScene, Director, Plan, SceneStart, SceneEnd);
	PlaceMusic(MovieScene, Director->Music, SceneStart + MovieScene->GetTickResolution().AsFrameNumber(Director->MusicOffset));
	BakeSpeedRamps(MovieScene, Plan, SceneStart);

	// Light the cast in the style's look: restyle their rigs, or give them one.
	if (DirectorStyle.bLightCharacters)
	{
		for (AActor* Member : UCinemafyDirectorLibrary::GetCast(EffectiveLines))
		{
			ACinemafyLightRig* Rig = nullptr;
			for (TActorIterator<ACinemafyLightRig> It(Director->GetWorld()); It; ++It)
			{
				if (It->Subject == Member)
				{
					Rig = *It;
					break;
				}
			}
			if (Rig)
			{
				Rig->Modify();
				Rig->Style = DirectorStyle.LightingStyle;
				Rig->SolveRig();
			}
			else
			{
				CreateLightRig(Member, nullptr, DirectorStyle.LightingStyle);
			}
		}
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

bool UCinemafyEditorLibrary::ToggleShotLock(ACinemafySceneDirector* Director, int32 ShotIndex)
{
	if (!Director || !Director->LastPlan.IsValidIndex(ShotIndex))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("ToggleLock", "Cinemafy: Lock Shot"));
	Director->Modify();
	FCinemafyPlannedShot& Shot = Director->LastPlan[ShotIndex];
	Shot.bLocked = !Shot.bLocked;
	SCinemafyShotBrowser::RefreshActive();
	return Shot.bLocked;
}

bool UCinemafyEditorLibrary::SwapShotForAlternate(ACinemafySceneDirector* Director, int32 ShotIndex)
{
	if (!Director)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("SwapShot", "Cinemafy: Alternate Shot"));
	Director->Modify();
	if (!UCinemafyDirectorLibrary::SwapForAlternate(Director->LastPlan, ShotIndex, Director->LastDirectedLines, Director->GetStyle()))
	{
		return false;
	}
	BakeDirectorPlan(Director, true);
	return true;
}

bool UCinemafyEditorLibrary::NudgeShotCut(ACinemafySceneDirector* Director, int32 ShotIndex, float DeltaSeconds)
{
	if (!Director)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("NudgeCut", "Cinemafy: Nudge Cut"));
	Director->Modify();
	if (!UCinemafyDirectorLibrary::NudgeCut(Director->LastPlan, ShotIndex, DeltaSeconds))
	{
		return false;
	}
	BakeDirectorPlan(Director, true);
	return true;
}

int32 UCinemafyEditorLibrary::SaveDirectorTake(ACinemafySceneDirector* Director, const FString& Name)
{
	if (!Director || Director->LastPlan.Num() == 0)
	{
		return -1;
	}
	const FScopedTransaction Transaction(LOCTEXT("SaveTake", "Cinemafy: Save Take"));
	Director->Modify();
	FCinemafyTake Take;
	Take.Name = Name.IsEmpty() ? FString::Printf(TEXT("Take %d"), Director->Takes.Num() + 1) : Name;
	Take.Seed = Director->Seed;
	Take.Plan = Director->LastPlan;
	Take.Lines = Director->LastDirectedLines;
	return Director->Takes.Add(Take);
}

bool UCinemafyEditorLibrary::LoadDirectorTake(ACinemafySceneDirector* Director, int32 TakeIndex)
{
	if (!Director || !Director->Takes.IsValidIndex(TakeIndex))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("LoadTake", "Cinemafy: Load Take"));
	Director->Modify();
	const FCinemafyTake& Take = Director->Takes[TakeIndex];
	Director->Seed = Take.Seed;
	Director->LastPlan = Take.Plan;
	Director->LastDirectedLines = Take.Lines;
	return BakeDirectorPlan(Director, true) > 0;
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
		const AActor* Subject = nullptr;
		const AActor* Camera = nullptr;
		FVector CameraLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		float StartSeconds = 0.f;
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

		struct FSeen { FVector Eye; float Depth; float X; const AActor* Actor; };
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
				InFrame.Add({ Eye, Depth, X, Character });
			}
			else if (FMath::Abs(X) < 1.6f && Depth < 150.f)
			{
				// Just off the edge and very close: a foreground shoulder.
				Foreground.Add({ Eye, Depth, X, Character });
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
		Out.Subject = Subject->Actor;
		Out.CameraLocation = View.Location;
		Out.ViewDirection = Forward;

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

	// An edit tells us about cutting, not grading or lighting: keep the director's current look and light.
	{
		const FCinemafyDirectorStyle Current = Director->GetStyle();
		Style.bApplyLook = Current.bApplyLook;
		Style.Look = Current.Look;
		Style.LookStrength = Current.LookStrength;
		Style.LetterboxAspect = Current.LetterboxAspect;
		Style.bLightCharacters = Current.bLightCharacters;
		Style.LightingStyle = Current.LightingStyle;
	}

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

FCinemafyMusicAnalysis UCinemafyEditorLibrary::AnalyzeMusic(USoundWave* Sound)
{
	FCinemafyMusicAnalysis Result;
	TArray<uint8> RawPCM;
	uint32 SampleRate = 0;
	uint16 NumChannels = 0;
	if (!Sound || !Sound->GetImportedSoundWaveData(RawPCM, SampleRate, NumChannels) || SampleRate == 0 || NumChannels == 0)
	{
		return Result;
	}

	const int16* Samples = reinterpret_cast<const int16*>(RawPCM.GetData());
	const int32 NumFrames = RawPCM.Num() / (sizeof(int16) * NumChannels);
	const int32 Hop = FMath::Max(1, static_cast<int32>(SampleRate / 100)); // 10 ms
	const float HopSeconds = static_cast<float>(Hop) / SampleRate;

	// Onset strength: how much louder each 10 ms gets than the one before (notes and drums starting).
	TArray<float> Onsets;
	float PreviousDb = -100.f;
	for (int32 Start = 0; Start + Hop <= NumFrames; Start += Hop)
	{
		double SumSquares = 0.0;
		for (int32 Frame = Start; Frame < Start + Hop; ++Frame)
		{
			float Mono = 0.f;
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				Mono += Samples[Frame * NumChannels + Channel] / 32768.f;
			}
			Mono /= NumChannels;
			SumSquares += Mono * Mono;
		}
		const float Db = 10.f * FMath::LogX(10.f, static_cast<float>(SumSquares / Hop) + 1e-12f);
		Onsets.Add(FMath::Max(0.f, Db - PreviousDb));
		PreviousDb = Db;
	}
	if (Onsets.Num() < 400)
	{
		return Result; // Under ~4 s: not enough to find a tempo.
	}

	// Tempo: the lag at which the onset pattern best repeats, gently preferring 80-160 BPM.
	const int32 MinLag = FMath::RoundToInt(60.f / 180.f / HopSeconds);
	const int32 MaxLag = FMath::RoundToInt(60.f / 60.f / HopSeconds);
	int32 BestLag = MinLag;
	double BestScore = -1.0;
	for (int32 Lag = MinLag; Lag <= MaxLag; ++Lag)
	{
		double Score = 0.0;
		for (int32 Index = 0; Index + Lag < Onsets.Num(); ++Index)
		{
			Score += Onsets[Index] * Onsets[Index + Lag];
		}
		const float Bpm = 60.f / (Lag * HopSeconds);
		const float Preference = FMath::Exp(-FMath::Square(FMath::Log2(Bpm / 120.f)) / (2.f * 0.5f * 0.5f));
		Score *= Preference;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestLag = Lag;
		}
	}

	// Phase: the offset where onsets line up best with that grid.
	int32 BestPhase = 0;
	double BestPhaseScore = -1.0;
	for (int32 Phase = 0; Phase < BestLag; ++Phase)
	{
		double Score = 0.0;
		for (int32 Index = Phase; Index < Onsets.Num(); Index += BestLag)
		{
			Score += Onsets[Index];
		}
		if (Score > BestPhaseScore)
		{
			BestPhaseScore = Score;
			BestPhase = Phase;
		}
	}

	Result.BeatsPerMinute = 60.f / (BestLag * HopSeconds);
	for (int32 Index = BestPhase; Index < Onsets.Num(); Index += BestLag)
	{
		Result.Beats.Add(Index * HopSeconds);
	}
	Result.bValid = Result.Beats.Num() >= 2;
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: %s is about %.0f BPM (%d beats)."), *Sound->GetName(), Result.BeatsPerMinute, Result.Beats.Num());
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

int32 UCinemafyEditorLibrary::CheckSequenceContinuity()
{
	ULevelSequence* Sequence = CinemafySequencer::GetOpenSequence();
	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	const UMovieSceneCameraCutTrack* CutTrack = MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!Sequencer.IsValid() || !CutTrack || !World)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ContinuityNoSequence", "Open a Level Sequence with camera cuts in Sequencer to check its continuity."));
		return -1;
	}

	TArray<AActor*> Characters;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<USkeletalMeshComponent>() && !It->FindComponentByClass<UCameraComponent>())
		{
			Characters.Add(*It);
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
	TArray<FString> Issues;
	FObservedShot Previous;
	bool bHasPrevious = false;

	for (int32 Index = 0; Index < Cuts.Num(); ++Index)
	{
		const UMovieSceneCameraCutSection* Cut = Cuts[Index];
		const FFrameNumber Start = Cut->GetInclusiveStartFrame();
		const float StartSeconds = static_cast<float>(TickResolution.AsSeconds(FFrameTime(Start)));
		const float Duration = static_cast<float>(TickResolution.AsSeconds(FFrameTime(Cut->GetExclusiveEndFrame() - Start)));
		const FString Where = FString::Printf(TEXT("Shot %d (%.1fs)"), Index + 1, StartSeconds);

		if (Duration < 0.5f)
		{
			Issues.Add(FString::Printf(TEXT("%s: flash cut, only %.2fs long."), *Where, Duration));
		}
		else if (Duration > 15.f)
		{
			Issues.Add(FString::Printf(TEXT("%s: %.0fs is a long time to hold one shot in a game. Consider a cutaway or a move."), *Where, Duration));
		}

		Sequencer->SetLocalTimeDirectly(FFrameTime(Start));
		Sequencer->ForceEvaluate();

		const AActor* CameraActor = nullptr;
		for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(Cut->GetCameraBindingID().GetGuid(), MovieSceneSequenceID::Root))
		{
			if ((CameraActor = Cast<AActor>(Bound.Get())) != nullptr)
			{
				break;
			}
		}
		const UCameraComponent* Camera = CameraActor ? CameraActor->FindComponentByClass<UCameraComponent>() : nullptr;

		FObservedShot Shot;
		Shot.Camera = CameraActor;
		Shot.StartSeconds = StartSeconds;
		const bool bClassified = Camera && ClassifyShot(Camera, Characters, Shot);

		if (bHasPrevious && CameraActor && CameraActor == Previous.Camera)
		{
			Issues.Add(FString::Printf(TEXT("%s: cuts from %s to itself, which isn't really a cut."), *Where, *CameraActor->GetActorLabel()));
		}

		if (bClassified && bHasPrevious && Previous.Subject && Shot.Subject)
		{
			if (Previous.Subject == Shot.Subject && Previous.Type == Shot.Type && Previous.Camera != Shot.Camera)
			{
				const float AngleChange = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Previous.ViewDirection, Shot.ViewDirection), -1.f, 1.f)));
				if (AngleChange < 30.f)
				{
					Issues.Add(FString::Printf(TEXT("%s: jump cut on %s (same framing, only %.0f degrees apart). Change the size or the angle."),
						*Where, *Shot.Subject->GetActorLabel(), AngleChange));
				}
			}
			else if (Previous.Subject != Shot.Subject)
			{
				// A reverse: both cameras should be on the same side of the line between these two.
				const float SideBefore = UCinemafyFramingLibrary::GetSideOfLine(Previous.CameraLocation, Previous.Subject, Shot.Subject);
				const float SideAfter = UCinemafyFramingLibrary::GetSideOfLine(Shot.CameraLocation, Previous.Subject, Shot.Subject);
				if (FMath::Abs(SideBefore) > 15.f && FMath::Abs(SideAfter) > 15.f && FMath::Sign(SideBefore) != FMath::Sign(SideAfter))
				{
					Issues.Add(FString::Printf(TEXT("%s: crosses the line between %s and %s. Screen direction flips on this cut."),
						*Where, *Previous.Subject->GetActorLabel(), *Shot.Subject->GetActorLabel()));
				}
			}
		}

		if (bClassified)
		{
			Previous = Shot;
			bHasPrevious = true;
		}
		else if (CameraActor)
		{
			Previous = FObservedShot();
			Previous.Camera = CameraActor;
			bHasPrevious = true;
		}
	}

	Sequencer->SetLocalTimeDirectly(ConvertFrameTime(OriginalTime.Time, OriginalTime.Rate, TickResolution));
	Sequencer->ForceEvaluate();

	FString Report;
	if (Issues.Num() == 0)
	{
		Report = FString::Printf(TEXT("%d cuts checked: no continuity problems found. Clean edit!"), Cuts.Num());
	}
	else
	{
		Report = FString::Printf(TEXT("%d cuts checked, %d note(s):\n\n"), Cuts.Num(), Issues.Num());
		for (const FString& Issue : Issues)
		{
			Report += TEXT("- ") + Issue + TEXT("\n");
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy continuity: %s"), *Issue);
		}
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report));
	return Issues.Num();
}

UCinemafyDirectorStyleAsset* UCinemafyEditorLibrary::SaveDirectorStyleAsAsset(ACinemafySceneDirector* Director)
{
	if (!Director)
	{
		return nullptr;
	}

	const FString StyleName = Director->StyleAsset ? Director->StyleAsset->GetName()
		: StaticEnum<ECinemafyDirectorPreset>()->GetNameStringByValue(static_cast<int64>(Director->Style));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName;
	FString AssetName;
	AssetTools.CreateUniqueAssetName(FString::Printf(TEXT("/Game/Cinemafy/Styles/DS_%s"), *StyleName), TEXT(""), PackageName, AssetName);

	UCinemafyDirectorStyleAsset* Asset = Cast<UCinemafyDirectorStyleAsset>(
		AssetTools.CreateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), UCinemafyDirectorStyleAsset::StaticClass(), nullptr));
	if (!Asset)
	{
		return nullptr;
	}

	Asset->Style = Director->GetStyle();
	Asset->Description = FText::Format(LOCTEXT("StyleAssetDescription", "Saved from {0} ({1})."),
		FText::FromString(Director->GetActorLabel()), FText::FromString(StyleName));
	Asset->MarkPackageDirty();

	const FScopedTransaction Transaction(LOCTEXT("SaveStyle", "Cinemafy: Save Director Style"));
	Director->Modify();
	Director->StyleAsset = Asset;

	TArray<UObject*> ToSync = { Asset };
	GEditor->SyncBrowserToObjects(ToSync);
	return Asset;
}

namespace
{
	/** Splits one CSV row, honoring quoted fields ("Well, then," she said). */
	TArray<FString> ParseCsvRow(const FString& Row)
	{
		TArray<FString> Fields;
		FString Current;
		bool bQuoted = false;
		for (int32 Index = 0; Index < Row.Len(); ++Index)
		{
			const TCHAR C = Row[Index];
			if (C == TEXT('"'))
			{
				if (bQuoted && Index + 1 < Row.Len() && Row[Index + 1] == TEXT('"'))
				{
					Current.AppendChar(TEXT('"'));
					++Index;
				}
				else
				{
					bQuoted = !bQuoted;
				}
			}
			else if (C == TEXT(',') && !bQuoted)
			{
				Fields.Add(Current.TrimStartAndEnd());
				Current.Reset();
			}
			else
			{
				Current.AppendChar(C);
			}
		}
		Fields.Add(Current.TrimStartAndEnd());
		return Fields;
	}

	bool ParseBool(const FString& Value)
	{
		return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("y"), ESearchCase::IgnoreCase) || Value == TEXT("1") || Value.Equals(TEXT("x"), ESearchCase::IgnoreCase);
	}
}

int32 UCinemafyEditorLibrary::ImportDirectorScript(ACinemafySceneDirector* Director, const FString& CsvPath)
{
	TArray<FString> Rows;
	if (!Director || !FFileHelper::LoadFileToStringArray(Rows, *CsvPath) || Rows.Num() < 2)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CsvMissing", "Couldn't read a script from {0}. It needs a header row and at least one line."), FText::FromString(CsvPath)));
		return -1;
	}

	// Columns by name, in any order.
	const TArray<FString> Header = ParseCsvRow(Rows[0]);
	auto Column = [&Header](const TCHAR* Name) { return Header.IndexOfByPredicate([Name](const FString& H) { return H.Equals(Name, ESearchCase::IgnoreCase); }); };
	const int32 SpeakerCol = Column(TEXT("Speaker"));
	const int32 ListenerCol = Column(TEXT("Listener"));
	const int32 TextCol = Column(TEXT("Text"));
	const int32 IntensityCol = Column(TEXT("Intensity"));
	const int32 EmotionCol = Column(TEXT("Emotion"));
	const int32 BreakCol = Column(TEXT("SceneBreak"));
	const int32 ActionCol = Column(TEXT("Action"));
	const int32 DurationCol = Column(TEXT("Duration"));
	if (SpeakerCol == INDEX_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CsvNoSpeaker", "The script needs a Speaker column."));
		return -1;
	}

	// Actors in the level, by label, for matching speaker names.
	TMap<FString, AActor*> ActorsByLabel;
	for (TActorIterator<AActor> It(Director->GetWorld()); It; ++It)
	{
		ActorsByLabel.Add(It->GetActorLabel().ToLower(), *It);
	}
	auto FindActor = [&ActorsByLabel](const FString& Name) -> AActor*
	{
		AActor* const* Found = ActorsByLabel.Find(Name.ToLower());
		return Found ? *Found : nullptr;
	};

	const FScopedTransaction Transaction(LOCTEXT("ImportScript", "Cinemafy: Import Script"));
	Director->Modify();
	Director->Lines.Reset();

	int32 Unmatched = 0;
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		if (Rows[RowIndex].TrimStartAndEnd().IsEmpty())
		{
			continue;
		}
		const TArray<FString> Fields = ParseCsvRow(Rows[RowIndex]);
		auto Field = [&Fields](int32 Col) { return Fields.IsValidIndex(Col) ? Fields[Col] : FString(); };

		FCinemafyLine Line;
		const FString SpeakerName = Field(SpeakerCol);
		Line.Speaker = FindActor(SpeakerName);
		Line.SpeakerName = FText::FromString(SpeakerName.ToUpper());
		Line.Listener = FindActor(Field(ListenerCol));
		Line.Text = FText::FromString(Field(TextCol));
		Line.Note = FString::Printf(TEXT("%d. %s: %s"), Director->Lines.Num() + 1, *SpeakerName, *Field(TextCol).Left(40));
		Unmatched += Line.Speaker ? 0 : 1;

		if (!Field(IntensityCol).IsEmpty())
		{
			Line.Intensity = FMath::Clamp(FCString::Atof(*Field(IntensityCol)), 0.f, 1.f);
		}
		if (!Field(EmotionCol).IsEmpty())
		{
			const int64 Value = StaticEnum<ECinemafyEmotion>()->GetValueByNameString(Field(EmotionCol));
			if (Value != INDEX_NONE)
			{
				Line.bSetEmotion = true;
				Line.Emotion = static_cast<ECinemafyEmotion>(Value);
			}
		}
		Line.bSceneBreak = ParseBool(Field(BreakCol));
		Line.bAction = ParseBool(Field(ActionCol));

		// Duration: given, or about 2.6 words a second plus a breath.
		const FString DurationText = Field(DurationCol);
		if (!DurationText.IsEmpty())
		{
			Line.Duration = FMath::Max(FCString::Atof(*DurationText), 0.2f);
		}
		else
		{
			TArray<FString> Words;
			Field(TextCol).ParseIntoArrayWS(Words);
			Line.Duration = FMath::Max(1.f, Words.Num() / 2.6f + 0.4f);
		}
		Director->Lines.Add(Line);
	}

	if (Unmatched > 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CsvUnmatched",
			"Imported {0} lines. {1} speaker name(s) didn't match an actor label in the level; set those Speakers by hand."),
			Director->Lines.Num(), Unmatched));
	}
	return Director->Lines.Num();
}

int32 UCinemafyEditorLibrary::AssignDirectorPerformances(ACinemafySceneDirector* Director, const FString& ContentFolder)
{
	if (!Director || ContentFolder.IsEmpty())
	{
		return 0;
	}

	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*ContentFolder), Assets, true);

	const FScopedTransaction Transaction(LOCTEXT("AssignTakes", "Cinemafy: Assign Performances"));
	Director->Modify();

	int32 Filled = 0;
	for (int32 Index = 0; Index < Director->Lines.Num(); ++Index)
	{
		FCinemafyLine& Line = Director->Lines[Index];
		const int32 Number = Index + 1;
		const FString Tags[] = {
			FString::Printf(TEXT("line%02d"), Number), FString::Printf(TEXT("line_%02d"), Number),
			FString::Printf(TEXT("line%d_"), Number), FString::Printf(TEXT("l%02d_"), Number) };

		for (const FAssetData& Asset : Assets)
		{
			const FString Name = Asset.AssetName.ToString().ToLower() + TEXT("_");
			bool bMatches = false;
			for (const FString& Tag : Tags)
			{
				// "Line1_" must not match "Line10_": tags end at a separator or a non-digit.
				const int32 At = Name.Find(Tag);
				if (At != INDEX_NONE && !FChar::IsDigit(Name[FMath::Min(At + Tag.Len(), Name.Len() - 1)]))
				{
					bMatches = true;
					break;
				}
			}
			if (!bMatches)
			{
				continue;
			}

			UObject* Object = Asset.GetAsset();
			if (USoundWave* Sound = Cast<USoundWave>(Object))
			{
				Line.Audio = Sound;
				Line.Analysis = FCinemafyAudioAnalysis();
				++Filled;
			}
			else if (UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(Object))
			{
				USkeletalMeshComponent* Mesh = FindMeshForClip(Line.Speaker, Clip);
				if (!Mesh)
				{
					continue;
				}
				// The face mesh is the one with eye bones.
				if (Mesh->GetBoneIndex(Cinemafy::LeftEyeBone) != INDEX_NONE)
				{
					Line.FaceAnimation = Clip;
				}
				else
				{
					Line.BodyAnimation = Clip;
				}
				++Filled;
			}
		}
	}
	return Filled;
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

ACinemafyChoreography* UCinemafyEditorLibrary::ChoreographSelection()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	ACinemafyChoreography* Choreography = nullptr;
	ACharacter* Character = nullptr;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (ACinemafyChoreography* Selected = Cast<ACinemafyChoreography>(Actor))
		{
			Choreography = Selected;
			break;
		}
		if (!Character)
		{
			Character = Cast<ACharacter>(Actor);
		}
	}

	// A character's existing choreography, if it has one.
	if (!Choreography && Character)
	{
		for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
		{
			if (It->Performer == Character)
			{
				Choreography = *It;
				break;
			}
		}
	}

	if (!Choreography)
	{
		const FScopedTransaction Transaction(LOCTEXT("NewChoreography", "Create Choreography"));
		FActorSpawnParameters Params;
		const FVector Location = Character ? Character->GetActorLocation() : FVector::ZeroVector;
		Choreography = World->SpawnActor<ACinemafyChoreography>(Location, FRotator::ZeroRotator, Params);
		if (!Choreography)
		{
			return nullptr;
		}
		Choreography->SetActorLabel(Character ? FString::Printf(TEXT("Choreography_%s"), *Character->GetActorLabel()) : FString(TEXT("Choreography")));
		Choreography->Performer = Character;
	}

	FCinemafyChoreoMode::Start(Choreography);
	return Choreography;
}

void UCinemafyEditorLibrary::PerformAndRecordChoreography(ACinemafyChoreography* Choreography)
{
	CinemafyChoreoTake::PerformAndRecord(Choreography);
}

int32 UCinemafyEditorLibrary::DirectChoreography(ACinemafyChoreography* Choreography)
{
	return CinemafyChoreoTake::DirectShots(Choreography);
}

UCinemafyMoveLibrary* UCinemafyEditorLibrary::CreateMoveLibraryFromFolder(const FString& ContentFolder)
{
	const TArray<FCinemafyMoveCandidate> Candidates = UCinemafyChoreographyLibrary::GatherCandidates(nullptr, { ContentFolder }, nullptr);

	// One-shots only: locomotion belongs to motion matching.
	static const TSet<FString> Locomotion = { TEXT("walk"), TEXT("run"), TEXT("jog"), TEXT("sprint"), TEXT("loop"), TEXT("locomotion"),
		TEXT("strafe"), TEXT("pivot"), TEXT("start"), TEXT("stop"), TEXT("turn"), TEXT("idle"), TEXT("pose"), TEXT("additive"), TEXT("ao") };

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(TEXT("/Game/Cinemafy/ML_Moves"), TEXT(""), PackageName, AssetName);
	UPackage* Package = CreatePackage(*PackageName);
	UCinemafyMoveLibrary* Library = NewObject<UCinemafyMoveLibrary>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

	TSet<FString> SeenNames;
	for (const FCinemafyMoveCandidate& Candidate : Candidates)
	{
		if (Candidate.Tokens.ContainsByPredicate([](const FString& Token) { return Locomotion.Contains(Token); }))
		{
			continue;
		}
		// AS_Attack and AS_Attack_Montage are the same move: keep the montage.
		FString BaseName = Candidate.Name;
		BaseName.RemoveFromEnd(TEXT("_Montage"));
		const bool bMontage = Candidate.Tokens.Contains(TEXT("montage"));
		if (SeenNames.Contains(BaseName) && !bMontage)
		{
			continue;
		}
		UAnimSequenceBase* Animation = Cast<UAnimSequenceBase>(Candidate.Path.TryLoad());
		if (!Animation)
		{
			continue;
		}
		if (SeenNames.Contains(BaseName))
		{
			Library->Moves.RemoveAll([&BaseName](const FCinemafyMoveEntry& Entry) { return Entry.Animation && Entry.Animation->GetName() == BaseName; });
		}
		SeenNames.Add(BaseName);

		FCinemafyMoveEntry& Entry = Library->Moves.AddDefaulted_GetRef();
		Entry.Animation = Animation;
		for (const FString& Token : UCinemafyChoreographyLibrary::TokenizeName(Candidate.Name))
		{
			if (Token.Len() > 1 && !Token.IsNumeric())
			{
				Entry.Keywords.AddUnique(Token);
			}
		}
	}

	FAssetRegistryModule::AssetCreated(Library);
	Package->MarkPackageDirty();
	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("MoveLibraryMade",
		"Made {0} with {1} move(s). Set it as the Move Library on your choreographies, and add your own keywords to any entry."),
		FText::FromString(AssetName), Library->Moves.Num()));
	return Library;
}

#undef LOCTEXT_NAMESPACE
