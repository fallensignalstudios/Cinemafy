#include "CinemafyEditorLibrary.h"
#include "CinemafyChoreoTools.h"
#include "CinemafyChoreography.h"
#include "CinemafyScript.h"
#include "CinemafyStoryLibrary.h"
#include "CinemafyChoreographyLibrary.h"
#include "CinemafyPerformerProfile.h"
#include "GameFramework/Character.h"
#include "CinemafyModule.h"
#include "CinemafySequencerUtils.h"
#include "CinemafyMood.h"
#include "CinemafyPrevis.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyFollowComponent.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
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
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Misc/FileHelper.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "IAssetTools.h"
#include "SCinemafyShotBrowser.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
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
#include "Sections/MovieSceneSkeletalAnimationSection.h"

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
	CinemafySequencer::LetCinemafyDriveCamera(MovieScene, Binding, CameraActor);

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

		// Cuts to Cinemafy cameras ("CAM_...") that no longer exist: an earlier staging's director made them and its
		// cameras are gone. Left in, they pile up under the new cut and drop the view to the player wherever they win.
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
		const bool bCanResolve = Sequencer.IsValid() && CinemafySequencer::GetOpenSequence() == Sequence;
		TSet<FGuid> DeadCameras;
		auto IsDeadCamera = [&](const FGuid& Guid)
		{
			if (!bCanResolve || !MovieScene)
			{
				return false;
			}
			const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Guid);
			if (!Possessable || !Possessable->GetName().StartsWith(TEXT("CAM_")))
			{
				return false;
			}
			for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(Guid, MovieSceneSequenceID::Root))
			{
				if (Bound.IsValid())
				{
					return false;
				}
			}
			return true;
		};

		TArray<UMovieSceneSection*> ToRemove;
		for (UMovieSceneSection* Section : CutTrack->GetAllSections())
		{
			const UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Section);
			if (!Cut)
			{
				continue;
			}
			const FGuid Guid = Cut->GetCameraBindingID().GetGuid();
			if (DirectorBindings.Contains(Guid))
			{
				ToRemove.Add(Section);
			}
			else if (IsDeadCamera(Guid))
			{
				ToRemove.Add(Section);
				DeadCameras.Add(Guid);
			}
		}

		CutTrack->Modify();
		for (UMovieSceneSection* Section : ToRemove)
		{
			CutTrack->RemoveSection(*Section);
		}
		if (DeadCameras.Num() > 0)
		{
			Sequence->Modify();
			MovieScene->Modify();
			for (const FGuid& Guid : DeadCameras)
			{
				// The camera's components and tracks go with it.
				TArray<FGuid> Children;
				for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
				{
					if (MovieScene->GetPossessable(Index).GetParent() == Guid)
					{
						Children.Add(MovieScene->GetPossessable(Index).GetGuid());
					}
				}
				for (const FGuid& Child : Children)
				{
					Sequence->UnbindPossessableObjects(Child);
					MovieScene->RemovePossessable(Child);
				}
				Sequence->UnbindPossessableObjects(Guid);
				MovieScene->RemovePossessable(Guid);
			}
			UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Director: dropped %d camera cut(s) to %d deleted camera(s) from earlier stagings."), ToRemove.Num(), DeadCameras.Num());
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

	const TCHAR* const CameraTrackName = TEXT("Cinemafy Camera Track");

	/** The camera's own transform tracks (Cinemafy's, or Sequencer's default Transform): the camera's position is keyed by us. */
	TArray<UMovieScene3DTransformTrack*> CameraTransformTracks(UMovieScene* MovieScene, const FGuid& Binding)
	{
		TArray<UMovieScene3DTransformTrack*> Found;
		const UMovieScene* ConstMovieScene = MovieScene;
		if (const FMovieSceneBinding* Existing = Binding.IsValid() ? ConstMovieScene->FindBinding(Binding) : nullptr)
		{
			for (UMovieSceneTrack* Track : Existing->GetTracks())
			{
				if (UMovieScene3DTransformTrack* Transform = Cast<UMovieScene3DTransformTrack>(Track))
				{
					Found.Add(Transform);
				}
			}
		}
		return Found;
	}

	/**
	 * Takes a camera off its earlier tracking keys: back to where it was framed (the first key of the last bake, which
	 * is always the framed position) with the track gone, so it can be reused or re-framed cleanly.
	 */
	void ResetCameraTracking(ULevelSequence* Sequence, UMovieScene* MovieScene, ACineCameraActor* Camera)
	{
		const FGuid Binding = CinemafySequencer::FindBinding(Sequence, Camera);
		for (UMovieScene3DTransformTrack* Track : CameraTransformTracks(MovieScene, Binding))
		{
			if (Track->GetDisplayName().ToString() != CameraTrackName)
			{
				continue;
			}
			const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
			if (Sections.Num() > 0)
			{
				const TArrayView<FMovieSceneDoubleChannel*> Channels = Sections[0]->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
				if (Channels.Num() >= 6 && Channels[0]->GetNumKeys() > 0)
				{
					auto First = [&Channels](int32 Index) { return Channels[Index]->GetValues()[0].Value; };
					Camera->Modify();
					Camera->SetActorLocationAndRotation(FVector(First(0), First(1), First(2)), FRotator(First(4), First(5), First(3)));
				}
			}
			MovieScene->RemoveTrack(*Track);
		}
	}

	/**
	 * Cameras keep people in frame when they move during the shot, whether they walk (the take moves the actor) or an
	 * animation carries the body (root motion on a clip): the sequence is played through each shot and the camera is
	 * keyed. Walk-and-talk shots ride along at the offset they were framed at. Other shots hold their spot and pan and
	 * tilt to keep the face where the shot put it (a small dead zone, eased like an operator would), and dolly after
	 * someone who walks more than a metre or so away. A shot nobody moves in gets no keys. Returns how many shots moved.
	 */
	int32 BakeCameraTracking(ULevelSequence* Sequence, UMovieScene* MovieScene, ACineCameraActor* Camera, const TArray<FCameraShotWindow>& Windows,
		const TSharedPtr<ISequencer>& Sequencer)
	{
		if (!Camera || !Sequencer.IsValid() || Sequencer->GetFocusedMovieSceneSequence() != Sequence || Windows.Num() == 0)
		{
			return 0;
		}
		const FGuid Binding = CinemafySequencer::FindBinding(Sequence, Camera);
		if (!Binding.IsValid())
		{
			return 0;
		}
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const FTransform Rest = Camera->GetActorTransform();
		const float Step = 0.2f;
		struct FCameraKey { FFrameNumber Frame; FVector Location; FRotator Rotation; };
		TArray<FCameraKey> Keys;
		int32 Moved = 0;
		TArray<FCameraShotWindow> Sorted = Windows;
		Sorted.Sort([](const FCameraShotWindow& A, const FCameraShotWindow& B) { return A.Start < B.Start; });
		for (const FCameraShotWindow& Window : Sorted)
		{
			const FCinemafyPlannedShot* Shot = Window.Shot;
			AActor* Subject = Shot ? Shot->Subject.Get() : nullptr;
			if (Keys.Num() > 0 && Keys.Last().Frame >= Window.Start)
			{
				continue; // Overlapping use of one camera: the earlier shot's keys stand.
			}
			Keys.Add({ Window.Start, Rest.GetLocation(), Rest.Rotator() });
			if (!Subject)
			{
				continue;
			}
			const bool bTracking = Shot->ShotType == ECinemafyShotType::TrackingLead || Shot->ShotType == ECinemafyShotType::TrackingFollow;
			const double Ticks = static_cast<double>((Window.End - Window.Start).Value);
			const int32 Samples = FMath::Clamp(FMath::CeilToInt(TickResolution.AsSeconds(FFrameTime(Window.End - Window.Start)) / Step), 1, 900);
			FVector Body0 = FVector::ZeroVector;
			FTransform Offset = FTransform::Identity;
			FRotator Aim0 = FRotator::ZeroRotator;
			FVector Carried = FVector::ZeroVector;
			FVector Location = Rest.GetLocation();
			FRotator Rotation = Rest.Rotator();
			bool bShotMoved = false;
			for (int32 Sample = 0; Sample <= Samples; ++Sample)
			{
				const FFrameNumber Frame = Window.Start + FFrameNumber(static_cast<int32>(FMath::RoundToDouble(Ticks * Sample / Samples)));
				Sequencer->SetLocalTimeDirectly(FFrameTime(Frame));
				Sequencer->ForceEvaluate();
				const FVector Body = UCinemafyFramingLibrary::GetSubjectTrackPoint(Subject);
				const FVector Eye = UCinemafyFramingLibrary::GetEyePoint(Subject);
				const FTransform SubjectFrame(FRotator(0.f, Subject->GetActorRotation().Yaw, 0.f), Body);
				if (Sample == 0)
				{
					Body0 = Body;
					Offset = Rest.GetRelativeTransform(SubjectFrame);
					Aim0 = (Eye - Rest.GetLocation()).Rotation();
					continue;
				}
				FVector WantLocation;
				FRotator WantRotation;
				if (bTracking)
				{
					// Riding along at the framed offset.
					const FTransform Along = Offset * SubjectFrame;
					WantLocation = Along.GetLocation();
					WantRotation = Along.Rotator();
				}
				else
				{
					// Dolly after them once they've gone more than 1.2 m from where the shot was framed.
					const FVector Travel = FVector(Body.X - Body0.X, Body.Y - Body0.Y, 0.f);
					const float Distance = Travel.Size();
					const FVector Want = Distance > 120.f ? Travel / Distance * (Distance - 120.f) : FVector::ZeroVector;
					Carried = FMath::VInterpTo(Carried, Want, Step, 2.5f);
					WantLocation = Rest.GetLocation() + Carried;
					// Pan and tilt to keep their eyes where the shot put them, past a 2 degree dead zone.
					const FRotator Aim = (Eye - WantLocation).Rotation();
					auto Beyond = [](float Degrees) { return FMath::Abs(Degrees) < 2.f ? 0.f : Degrees - FMath::Sign(Degrees) * 2.f; };
					WantRotation = Rest.Rotator();
					WantRotation.Yaw += Beyond(FRotator::NormalizeAxis(Aim.Yaw - Aim0.Yaw));
					WantRotation.Pitch = FMath::Clamp(WantRotation.Pitch + Beyond(FRotator::NormalizeAxis(Aim.Pitch - Aim0.Pitch)), -85.f, 85.f);
				}
				// Eased toward it, like an operator: no snapping on a step.
				const float Ease = bTracking ? 6.f : 4.f;
				Location = FMath::VInterpTo(Location, WantLocation, Step, Ease);
				Rotation = FMath::RInterpTo(Rotation, WantRotation, Step, Ease);
				bShotMoved |= !Location.Equals(Rest.GetLocation(), 3.f) || !Rotation.Equals(Rest.Rotator(), 0.5f);
				Keys.Add({ Frame, Location, Rotation });
			}
			Moved += bShotMoved ? 1 : 0;
		}
		Camera->SetActorTransform(Rest);
		for (UMovieScene3DTransformTrack* Track : CameraTransformTracks(MovieScene, Binding))
		{
			if (Track->GetDisplayName().ToString() == CameraTrackName)
			{
				MovieScene->RemoveTrack(*Track);
			}
		}
		if (Moved == 0)
		{
			return 0; // Nobody moved: the camera holds where it was framed.
		}

		// Sequencer's own Transform track would pin it: ours drives the camera now.
		CinemafySequencer::MuteBindingTracks(MovieScene, Binding, TEXT("MovieScene3DTransformTrack"));
		UMovieScene3DTransformTrack* Track = MovieScene->AddTrack<UMovieScene3DTransformTrack>(Binding);
		Track->SetDisplayName(FText::FromString(CameraTrackName));
		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
		if (!Section)
		{
			return 0;
		}
		Section->SetRange(TRange<FFrameNumber>::All());
		Track->AddSection(*Section);
		const TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		if (Channels.Num() < 9)
		{
			return 0;
		}
		for (int32 Index = 6; Index < 9; ++Index)
		{
			Channels[Index]->SetDefault(1.0);
		}
		// Unwound angles, so a pan across 180 degrees doesn't spin the long way round between keys.
		FRotator Previous = Keys[0].Rotation;
		for (const FCameraKey& Key : Keys)
		{
			FRotator Rotation = Key.Rotation;
			Rotation.Yaw = Previous.Yaw + FRotator::NormalizeAxis(Rotation.Yaw - Previous.Yaw);
			Rotation.Pitch = Previous.Pitch + FRotator::NormalizeAxis(Rotation.Pitch - Previous.Pitch);
			Rotation.Roll = Previous.Roll + FRotator::NormalizeAxis(Rotation.Roll - Previous.Roll);
			Previous = Rotation;
			Channels[0]->AddCubicKey(Key.Frame, Key.Location.X);
			Channels[1]->AddCubicKey(Key.Frame, Key.Location.Y);
			Channels[2]->AddCubicKey(Key.Frame, Key.Location.Z);
			Channels[3]->AddCubicKey(Key.Frame, Rotation.Roll);
			Channels[4]->AddCubicKey(Key.Frame, Rotation.Pitch);
			Channels[5]->AddCubicKey(Key.Frame, Rotation.Yaw);
		}
		// The keys drive it now; the live follow rig (for the Story Player's live scenes) would fight them here.
		if (UCinemafyFollowComponent* Follow = Camera->FindComponentByClass<UCinemafyFollowComponent>())
		{
			Follow->Modify();
			Follow->Weight = 0.f;
		}
		return Moved;
	}

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

	/** The actor's skeletal mesh that can play this clip (same or compatible skeleton): body or face. */
	USkeletalMeshComponent* FindMeshForClip(AActor* Actor, const UAnimSequenceBase* Clip)
	{
		if (!Actor || !Clip || !Clip->GetSkeleton())
		{
			return nullptr;
		}
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
			const USkeleton* Skeleton = Asset ? Asset->GetSkeleton() : nullptr;
			if (Skeleton && (Skeleton == Clip->GetSkeleton() || Skeleton->IsCompatibleForEditor(Clip->GetSkeleton())))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	/** The actor's main body mesh: the character mesh, else its first skeletal mesh. */
	USkeletalMeshComponent* FindBodyMesh(AActor* Actor)
	{
		if (const ACharacter* Character = Cast<ACharacter>(Actor))
		{
			if (Character->GetMesh() && Character->GetMesh()->GetSkeletalMeshAsset())
			{
				return Character->GetMesh();
			}
		}
		return Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	}

	/**
	 * How far a clip moves the body around (cm): the widest horizontal spread of the hips over the clip. In-place
	 * talking stays under ~30; clips that pace or walk go well past it. 0 when it can't be read.
	 */
	float BodyTravel(const UAnimSequenceBase* Clip)
	{
		const UAnimSequence* Sequence = Cast<UAnimSequence>(Clip);
		const USkeleton* Skeleton = Clip ? Clip->GetSkeleton() : nullptr;
		if (!Sequence || !Skeleton || !Sequence->GetDataModel())
		{
			return 0.f;
		}
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		FName Hips = TEXT("pelvis");
		if (!Model->IsValidBoneTrackName(Hips) && Ref.GetNum() > 1)
		{
			Hips = Ref.GetBoneName(1);
		}
		if (!Model->IsValidBoneTrackName(Hips))
		{
			return 0.f;
		}
		const int32 Frames = FMath::Max(Model->GetNumberOfFrames(), 1);
		FBox2D Box(ForceInit);
		for (int32 Sample = 0; Sample <= 16; ++Sample)
		{
			const FVector Location = Model->EvaluateBoneTrackTransform(Hips, FFrameTime(Frames * Sample / 16), EAnimInterpolationType::Linear).GetLocation();
			Box += FVector2D(Location.X, Location.Y);
		}
		return Box.GetSize().GetMax();
	}

	/** True when a Choreographer take is baked onto the actor or one of its components in this sequence. */
	bool HasChoreographyTake(ULevelSequence* Sequence, UMovieScene* MovieScene, AActor* Actor)
	{
		const FGuid ActorBinding = CinemafySequencer::FindBinding(Sequence, Actor);
		if (!ActorBinding.IsValid())
		{
			return false;
		}
		static const FString TakeName = TEXT("Cinemafy Take");
		const UMovieScene* ConstMovieScene = MovieScene;
		for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
		{
			const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
			const bool bOurs = Binding.GetObjectGuid() == ActorBinding || (Possessable && Possessable->GetParent() == ActorBinding);
			if (!bOurs)
			{
				continue;
			}
			for (const UMovieSceneTrack* Track : Binding.GetTracks())
			{
				if (Track && Track->GetDisplayName().ToString() == TakeName)
				{
					return true;
				}
			}
		}
		return false;
	}

	/**
	 * Lays each line's body / face clips onto the speaker, on a "Cinemafy Performance" animation track per mesh
	 * (replacing the last take's). With bCast, lines without a body clip get one cast from the project's
	 * animations for their tone: a talking clip on the speaker, a listening one on the listener.
	 */
	void PlacePerformances(ULevelSequence* Sequence, UMovieScene* MovieScene, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart,
		bool bCast, int32 Seed)
	{
		struct FPlacement { USkeletalMeshComponent* Mesh; UAnimSequenceBase* Clip; FFrameNumber Start; FFrameNumber End; };
		TArray<FPlacement> Placements;
		// Cast clips over a recorded take: where the take's own body animation steps aside for them, per mesh.
		TMap<USkeletalMeshComponent*, TArray<TRange<FFrameNumber>>> OverTake;

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const float Blend = 0.35f;
		const float MaxTravel = 40.f;

		// Casting: each body skeleton's candidates, once; who's already performing a recorded take.
		TMap<const USkeleton*, TArray<FCinemafyMoveCandidate>> CandidatesBySkeleton;
		TMap<AActor*, bool> Recorded;
		TMap<USkeletalMeshComponent*, TArray<FSoftObjectPath>> Recent;
		static const FName PosedTag(TEXT("Cinemafy.Posed"));
		auto CastClip = [&](AActor* Actor, const FCinemafyLine& Line, int32 LineIndex, bool bSpeaking, float From, float To)
		{
			if (Actor && Actor->Tags.Contains(PosedTag))
			{
				return; // Lying on the bed, seated: a standing gesture would pull them out of the pose.
			}
			// Or posed partway through the take (lay down beside him): from that beat on, no standing gestures.
			UWorld* ActorWorld = Actor ? Actor->GetWorld() : nullptr;
			for (TActorIterator<ACinemafyChoreography> It(ActorWorld); ActorWorld && It; ++It)
			{
				if (It->Performer != Actor)
				{
					continue;
				}
				for (const FCinemafyChoreoBeat& Beat : It->Beats)
				{
					if (Beat.PoseHere != ECinemafyPose::Stand && Beat.StartTime >= 0.f && Beat.StartTime <= To)
					{
						return;
					}
				}
			}
			USkeletalMeshComponent* Mesh = FindBodyMesh(Actor);
			const USkeleton* Skeleton = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
			if (!Skeleton)
			{
				return;
			}
			bool* bRecorded = Recorded.Find(Actor);
			if (!bRecorded)
			{
				bRecorded = &Recorded.Add(Actor, HasChoreographyTake(Sequence, MovieScene, Actor));
			}
			TArray<FCinemafyMoveCandidate>* Candidates = CandidatesBySkeleton.Find(Skeleton);
			if (!Candidates)
			{
				Candidates = &CandidatesBySkeleton.Add(Skeleton, UCinemafyChoreographyLibrary::GatherCandidates(nullptr, {}, Skeleton));
			}
			TArray<FSoftObjectPath>& Used = Recent.FindOrAdd(Mesh);
			UAnimSequenceBase* Clip = nullptr;
			FSoftObjectPath ClipPath;
			for (int32 Attempt = 0; Attempt < 8 && !Clip; ++Attempt)
			{
				const int32 Pick = UCinemafyDirectorLibrary::PickDialogueAnimation(Line, bSpeaking, *Candidates,
					Seed + LineIndex * 7919 + (bSpeaking ? 0 : 104729) + Attempt, Used);
				if (Pick == INDEX_NONE)
				{
					return;
				}
				ClipPath = (*Candidates)[Pick].Path;
				Clip = Cast<UAnimSequenceBase>(ClipPath.TryLoad());
				// A clip that walks the actor off their mark leaves the shot framed on nobody: never cast it again.
				if (!Clip || BodyTravel(Clip) > MaxTravel)
				{
					UE_LOG(LogCinemafy, Verbose, TEXT("Cinemafy Director: skipped %s for casting (it moves the body around)."), *ClipPath.ToString());
					Candidates->RemoveAt(Pick);
					Clip = nullptr;
				}
			}
			if (!Clip)
			{
				return;
			}
			Used.Add(ClipPath);
			if (Used.Num() > 4)
			{
				Used.RemoveAt(0);
			}
			Placements.Add({ Mesh, Clip, SceneStart + TickResolution.AsFrameNumber(From), SceneStart + TickResolution.AsFrameNumber(To) });
			if (*bRecorded)
			{
				// A still line in a recorded take: the take is standing there, so the line's clip plays over it.
				OverTake.FindOrAdd(Mesh).Add(TRange<FFrameNumber>(Placements.Last().Start, Placements.Last().End));
			}
			UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Director: line %d cast %s as %s (%s, %s)."), LineIndex, *Clip->GetName(),
				*GetNameSafe(Actor), bSpeaking ? TEXT("speaking") : TEXT("listening"),
				*StaticEnum<ECinemafyEmotion>()->GetNameStringByValue(static_cast<int64>(UCinemafyDirectorLibrary::ReadLineTone(Line))));
		};

		float Time = 0.f;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FCinemafyLine& Line = Lines[LineIndex];
			const float Length = FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
			const FFrameNumber Start = SceneStart + TickResolution.AsFrameNumber(Time);
			for (UAnimSequenceBase* Clip : { Line.BodyAnimation.Get(), Line.FaceAnimation.Get() })
			{
				if (!Clip)
				{
					continue;
				}
				if (USkeletalMeshComponent* Mesh = FindMeshForClip(Line.Speaker, Clip))
				{
					Placements.Add({ Mesh, Clip, Start, FFrameNumber() });
				}
				else
				{
					UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Director: %s doesn't match any mesh on %s (skeleton mismatch)."),
						*Clip->GetName(), *GetNameSafe(Line.Speaker));
				}
			}

			// Fights and walks have their own bodies; everyone else gets a performance for the line's tone. Sections
			// overlap by the blend so one clip eases into the next.
			const bool bStill = !Line.bAction && !Line.bMoving && Line.Moment != ECinemafyMoment::Action && Line.Moment != ECinemafyMoment::WalkAndTalk;
			if (bCast && bStill)
			{
				const float From = FMath::Max(Time - Blend, 0.f);
				const float To = Time + Length + Blend;
				if (!Line.BodyAnimation && Line.Speaker)
				{
					CastClip(Line.Speaker, Line, LineIndex, true, From, To);
				}
				if (Line.Listener && Line.Listener != Line.Speaker)
				{
					CastClip(Line.Listener, Line, LineIndex, false, From, To);
				}
			}
			Time += Length;
		}

		// People the blocking posed hold their pose clip the whole scene (Previs tagged them "Cinemafy.Pose.Lie.Together.0").
		TSet<AActor*> Posed;
		for (const FCinemafyLine& Line : Lines)
		{
			for (AActor* Actor : { Line.Speaker.Get(), Line.Listener.Get() })
			{
				if (Actor && Actor->Tags.Contains(PosedTag))
				{
					Posed.Add(Actor);
				}
			}
		}
		for (AActor* Actor : Posed)
		{
			for (const FName& Tag : Actor->Tags)
			{
				TArray<FString> Parts;
				Tag.ToString().ParseIntoArray(Parts, TEXT("."), true);
				if (Parts.Num() != 5 || Parts[1] != TEXT("Pose"))
				{
					continue;
				}
				const int64 PoseValue = StaticEnum<ECinemafyPose>()->GetValueByNameString(Parts[2]);
				USkeletalMeshComponent* Mesh = FindBodyMesh(Actor);
				const USkeleton* Skeleton = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
				UAnimSequenceBase* Clip = PoseValue == INDEX_NONE ? nullptr
					: UCinemafyPrevisLibrary::FindPoseAnimation(static_cast<ECinemafyPose>(PoseValue), Skeleton, Parts[3] == TEXT("Together"), FCString::Atoi(*Parts[4]));
				if (!Clip || !Mesh)
				{
					continue; // A lying stand-in is laid flat on the actor itself; standing needs nothing.
				}
				// From the first frame to the last ("for the whole scene"), not from the first line.
				const TRange<FFrameNumber> Playback = MovieScene->GetPlaybackRange();
				const FFrameNumber Begin = FMath::Min(SceneStart, Playback.GetLowerBoundValue());
				const FFrameNumber End = FMath::Max(SceneStart + TickResolution.AsFrameNumber(FMath::Max(Time, 0.2f)), Playback.GetUpperBoundValue());
				Placements.Add({ Mesh, Clip, Begin, End });
				bool* bRecorded = Recorded.Find(Actor);
				if (!bRecorded)
				{
					bRecorded = &Recorded.Add(Actor, HasChoreographyTake(Sequence, MovieScene, Actor));
				}
				if (*bRecorded)
				{
					OverTake.FindOrAdd(Mesh).Add(TRange<FFrameNumber>(Begin, End));
				}
			}
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
				// Binding a mesh can auto-add a Control Rig track, which would override the performance.
				CinemafySequencer::MuteBindingTracks(MovieScene, Binding, TEXT("MovieSceneControlRigParameterTrack"), Track);
				Found = &Tracks.Add(Placement.Mesh, Track);
			}
			UMovieSceneSection* Section = (*Found)->AddNewAnimation(Placement.Start, Placement.Clip);
			if (Section && Placement.End > Placement.Start)
			{
				// A cast clip covers its line: looped if it's short, cut if it's long, eased in and out.
				Section->SetRange(TRange<FFrameNumber>(Placement.Start, Placement.End));
				const int32 Ease = TickResolution.AsFrameNumber(Blend).Value;
				Section->Easing.bManualEaseIn = true;
				Section->Easing.ManualEaseInDuration = Ease;
				Section->Easing.bManualEaseOut = true;
				Section->Easing.ManualEaseOutDuration = Ease;
			}
		}

		// Under those clips, the recorded take fades out and back (its weight), so the two don't play half and half.
		const int32 EaseTicks = TickResolution.AsFrameNumber(Blend).Value;
		for (TPair<USkeletalMeshComponent*, TArray<TRange<FFrameNumber>>>& Pair : OverTake)
		{
			const FGuid Binding = CinemafySequencer::FindBinding(Sequence, Pair.Key->GetOwner());
			const FGuid MeshBinding = CinemafySequencer::BindObject(Sequence, Pair.Key);
			// Back-to-back lines are one stretch, so the take doesn't pop back in between them.
			TArray<TRange<FFrameNumber>>& Ranges = Pair.Value;
			Ranges.Sort([](const TRange<FFrameNumber>& A, const TRange<FFrameNumber>& B) { return A.GetLowerBoundValue() < B.GetLowerBoundValue(); });
			TArray<TRange<FFrameNumber>> Merged;
			for (const TRange<FFrameNumber>& Range : Ranges)
			{
				if (Merged.Num() > 0 && Range.GetLowerBoundValue() <= Merged.Last().GetUpperBoundValue() + EaseTicks * 2)
				{
					Merged.Last() = TRange<FFrameNumber>(Merged.Last().GetLowerBoundValue(), FMath::Max(Merged.Last().GetUpperBoundValue(), Range.GetUpperBoundValue()));
				}
				else
				{
					Merged.Add(Range);
				}
			}
			const UMovieScene* ConstMovieScene = MovieScene;
			for (const FGuid& Owner : { Binding, MeshBinding })
			{
				const FMovieSceneBinding* Found = Owner.IsValid() ? ConstMovieScene->FindBinding(Owner) : nullptr;
				if (!Found)
				{
					continue;
				}
				for (UMovieSceneTrack* Track : Found->GetTracks())
				{
					if (!Track || !Track->IsA<UMovieSceneSkeletalAnimationTrack>() || Track->GetDisplayName().ToString() != TEXT("Cinemafy Take"))
					{
						continue;
					}
					for (UMovieSceneSection* Section : Track->GetAllSections())
					{
						UMovieSceneSkeletalAnimationSection* Take = Cast<UMovieSceneSkeletalAnimationSection>(Section);
						if (!Take)
						{
							continue;
						}
						Take->Modify();
						FMovieSceneFloatChannel& Weight = Take->Params.Weight;
						Weight.Reset();
						Weight.SetDefault(1.f);
						for (const TRange<FFrameNumber>& Range : Merged)
						{
							const FFrameNumber In = Range.GetLowerBoundValue();
							const FFrameNumber Out = Range.GetUpperBoundValue();
							Weight.AddLinearKey(In, 1.f);
							Weight.AddLinearKey(In + EaseTicks, 0.f);
							Weight.AddLinearKey(FMath::Max(Out - EaseTicks, In + EaseTicks), 0.f);
							Weight.AddLinearKey(Out, 1.f);
						}
					}
				}
			}
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

			// The first shot has nothing to dissolve, wipe or whip from: the overlay would capture an empty frame (a
			// white flash). It fades in from black instead; a dip to white stays a dip.
			const bool bNothingBefore = Index == 0 && IsOverlayTransition(Shot.TransitionIn) && Shot.TransitionIn != ECinemafyTransition::DipToWhite;
			if (Shot.TransitionIn == ECinemafyTransition::FadeThroughBlack || bNothingBefore)
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
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(Hit), Shot.SlowMoRate);
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(Hit + Shot.SlowMoHold), Shot.SlowMoRate);
			Channel->AddLinearKey(SceneStart + TickResolution.AsFrameNumber(Hit + Shot.SlowMoHold + 0.3f), 1.f);
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

	// Who's walking? Play through the sequence and see how far the people in each line travel during it: the speaker
	// and the listener, by their bodies (an animation can carry the mesh away from the actor), sampled all the way
	// through (a walk out and back, or a walk in the pause after the words, counts).
	if (const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer())
	{
		const FQualifiedFrameTime OriginalTime = Sequencer->GetLocalTime();
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const FFrameNumber SceneStart = CinemafySequencer::GetPlayheadTick(MovieScene);
		float Time = 0.f;
		for (FCinemafyLine& Line : DirectedLines)
		{
			const float Length = FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
			if (!Line.bMoving && (Line.Speaker || Line.Listener))
			{
				TMap<AActor*, FVector> From;
				float Farthest = 0.f;
				const int32 Samples = FMath::Clamp(FMath::CeilToInt(Length / 0.25f), 1, 120);
				for (int32 Sample = 0; Sample <= Samples; ++Sample)
				{
					Sequencer->SetLocalTimeDirectly(FFrameTime(SceneStart + TickResolution.AsFrameNumber(Time + Length * Sample / Samples)));
					Sequencer->ForceEvaluate();
					for (AActor* Person : { Line.Speaker.Get(), Line.Listener.Get() })
					{
						if (!Person)
						{
							continue;
						}
						const FVector Here = UCinemafyFramingLibrary::GetSubjectTrackPoint(Person);
						const FVector& Start = From.FindOrAdd(Person, Here);
						Farthest = FMath::Max(Farthest, FVector::Dist2D(Start, Here));
					}
				}
				Line.bMoving = Farthest > 150.f;
			}
			Time += Length;
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
	const int32 Shots = BakeDirectorPlan(Director, false);
	if (Shots > 0)
	{
		// The script's checklist hears about it: this scene is cut, in this sequence.
		Director->LastSequence = Sequence;
		if (Director->ScriptProject)
		{
			if (FCinemafyScriptScene* Scene = Director->ScriptProject->FindScene(Director->ScriptSceneId))
			{
				Director->ScriptProject->Modify();
				// An insert of a gameplay bridge keeps its own sequence; the scene counts as directed once any of it is.
				if (FCinemafySceneInsert* Insert = Director->ScriptProject->FindInsert(Director->ScriptSceneId))
				{
					Insert->Sequence = Sequence;
				}
				else
				{
					Scene->Sequence = Sequence;
				}
				Scene->Level = Director->GetWorld();
				Scene->DirectorLabel = Director->GetActorLabel();
				if (Scene->Status < ECinemafySceneStatus::Directed)
				{
					Scene->Status = ECinemafySceneStatus::Directed;
				}
			}
		}
	}
	return Shots;
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

	// Last cut's tracking keys go first: each camera back where it was framed.
	for (const TPair<FName, ACineCameraActor*>& Pair : ExistingCameras)
	{
		ResetCameraTracking(Sequence, MovieScene, Pair.Value);
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber SceneStart = bAtLastSceneStart ? FFrameNumber(Director->LastSceneStartTick) : CinemafySequencer::GetPlayheadTick(MovieScene);
	FFrameNumber SceneEnd = SceneStart;
	TMap<ACineCameraActor*, TArray<FCameraShotWindow>> CameraWindows;
	Director->LastSceneStartTick = SceneStart.Value;

	// New cameras are framed on where the characters are when their shot starts (a baked choreography take moves
	// them), so evaluate the sequence at each shot before framing it, then put the playhead back.
	const TSharedPtr<ISequencer> FramingSequencer = CinemafySequencer::GetOpenSequencer();
	const bool bFrameAtShotTime = FramingSequencer.IsValid() && FramingSequencer->GetFocusedMovieSceneSequence() == Sequence;
	const FQualifiedFrameTime FramingRestoreTime = bFrameAtShotTime ? FramingSequencer->GetLocalTime() : FQualifiedFrameTime();

	for (int32 ShotIndex = 0; ShotIndex < Plan.Num(); ++ShotIndex)
	{
		const FCinemafyPlannedShot& Shot = Plan[ShotIndex];
		const FName ShotTag(*FString::Printf(TEXT("Cinemafy.Shot.%s.%d.%s.%s.%d"), *Director->GetName(), static_cast<int32>(Shot.ShotType),
			Shot.Subject ? *Shot.Subject->GetName() : TEXT("-"), Shot.Counterpart ? *Shot.Counterpart->GetName() : TEXT("-"), Director->bFlipSide ? 1 : 0));

		ACineCameraActor* Camera = ExistingCameras.FindRef(ShotTag);
		if (bFrameAtShotTime)
		{
			FramingSequencer->SetLocalTimeDirectly(FFrameTime(SceneStart + TickResolution.AsFrameNumber(Shot.StartTime)));
			FramingSequencer->ForceEvaluate();
		}
		FCinemafyShotRequest Request;
		Request.ShotType = Shot.ShotType;
		Request.Subject = Shot.Subject;
		Request.Counterpart = Shot.Counterpart;
		Request.Group = Shot.Group;
		Request.bUseRecommendedLens = false;
		Request.Lens = Shot.Lens;
		Request.bFlipSide = Director->bFlipSide;
		// Where the subject stood when the camera was framed: a reused camera is re-framed if they've moved since
		// (a new take put them elsewhere), and keeps its hand tweaks if they haven't.
		const FVector SubjectAt = Shot.Subject ? UCinemafyFramingLibrary::GetSubjectTrackPoint(Shot.Subject) : FVector::ZeroVector;
		const FName AtTag(*FString::Printf(TEXT("Cinemafy.FramedAt.%d_%d_%d"), FMath::RoundToInt(SubjectAt.X), FMath::RoundToInt(SubjectAt.Y), FMath::RoundToInt(SubjectAt.Z)));
		auto FramedFar = [&SubjectAt](const ACineCameraActor* Existing)
		{
			for (const FName& Tag : Existing->Tags)
			{
				TArray<FString> Parts;
				const FString Text = Tag.ToString();
				if (Text.StartsWith(TEXT("Cinemafy.FramedAt.")) && Text.Mid(18).ParseIntoArray(Parts, TEXT("_"), true) == 3)
				{
					return FVector::Dist(SubjectAt, FVector(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2]))) > 60.f;
				}
			}
			return true; // Framed before Cinemafy remembered where: frame it again.
		};
		if (Camera && bFrameAtShotTime && Shot.Subject && FramedFar(Camera))
		{
			FCinemafyShotSolution Solution;
			bool bClearView = false;
			if (UCinemafyFramingLibrary::SolveShotAvoidingObstacles(Camera->GetWorld(), Request, Solution, bClearView))
			{
				Camera->Modify();
				Camera->SetActorTransform(Solution.CameraTransform);
				UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Director: re-framed %s (its subject moved since it was framed)."), *Camera->GetActorLabel());
			}
		}
		if (!Camera)
		{
			Camera = CreateShot(Request, false);
			if (!Camera)
			{
				continue;
			}
			Camera->Tags.Add(DirectorTag);
			Camera->Tags.Add(ShotTag);
			ExistingCameras.Add(ShotTag, Camera);
		}
		if (Shot.Subject)
		{
			Camera->Tags.RemoveAll([](const FName& Tag) { return Tag.ToString().StartsWith(TEXT("Cinemafy.FramedAt.")); });
			Camera->Tags.Add(AtTag);
		}

		Director->LastPlanCameras[ShotIndex] = Camera;

		// Walk-and-talk cameras ride a follow rig, locked to the subject at the offset they were framed at.
		if (Shot.ShotType == ECinemafyShotType::TrackingLead || Shot.ShotType == ECinemafyShotType::TrackingFollow)
		{
			UCinemafyFollowComponent* Follow = FindOrAddComponent<UCinemafyFollowComponent>(Camera, TEXT("CinemafyFollow"));
			// Captured fresh each cut (the subject may stand somewhere else in this take); baked keys switch it off below.
			Follow->Modify();
			Follow->Target = Shot.Subject;
			Follow->Weight = 1.f;
			Follow->CaptureOffset();
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

		// Don't let the tracks Sequencer just added pin walk-and-talk cameras in place or freeze Eye Focus.
		CinemafySequencer::LetCinemafyDriveCamera(MovieScene, Binding, Camera);

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

	int32 Tracked = 0;
	for (const TPair<ACineCameraActor*, TArray<FCameraShotWindow>>& Pair : CameraWindows)
	{
		BakeCameraMoves(Sequence, MovieScene, Pair.Key, Pair.Value);
		BakeRackFocus(Sequence, MovieScene, Pair.Key, Pair.Value);
		Tracked += BakeCameraTracking(Sequence, MovieScene, Pair.Key, Pair.Value, FramingSequencer);
	}
	if (Tracked > 0)
	{
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Director: %d shot(s) follow someone who moves during them."), Tracked);
	}
	BakeEmotions(Sequence, MovieScene, EffectiveLines, SceneStart);

	if (Director->bPlaceDialogueAudio)
	{
		PlaceDialogueAudio(MovieScene, EffectiveLines, SceneStart);
		UCinemafyStoryLibrary::BakeTalkCurves(Sequence, EffectiveLines, SceneStart);
	}
	if (Director->bPlacePerformances)
	{
		PlacePerformances(Sequence, MovieScene, EffectiveLines, SceneStart, Director->bCastDialogueAnimations, Director->Seed);
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

	if (bFrameAtShotTime)
	{
		FramingSequencer->SetLocalTimeDirectly(ConvertFrameTime(FramingRestoreTime.Time, FramingRestoreTime.Rate, TickResolution));
		FramingSequencer->ForceEvaluate();
	}

	// The playback range is the cut: the whole scene fits, and nothing plays on past the last shot (a take keeps
	// recording after its last line, and with no camera that tail plays black).
	const TRange<FFrameNumber> Playback = MovieScene->GetPlaybackRange();
	if (Playback.GetUpperBoundValue() != SceneEnd && SceneEnd > Playback.GetLowerBoundValue())
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
	TArray<float> WindowCrossings;
	for (int32 Start = 0; Start + Window <= NumFrames; Start += Window)
	{
		double SumSquares = 0.0;
		int32 Crossings = 0;
		float Previous = 0.f;
		for (int32 Frame = Start; Frame < Start + Window; ++Frame)
		{
			float Mono = 0.f;
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				Mono += Samples[Frame * NumChannels + Channel] / 32768.f;
			}
			Mono /= NumChannels;
			SumSquares += Mono * Mono;
			Crossings += (Mono >= 0.f) != (Previous >= 0.f) ? 1 : 0;
			Previous = Mono;
		}
		WindowCrossings.Add(static_cast<float>(Crossings) / Window);
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

	// Mouth curves for faces with no captured performance (Cinemafy Face's Talk): how open (loudness above
	// the speech floor) and how narrow (hissy, high zero-crossing sounds: s, sh, f) at 50 Hz.
	Result.TalkOpen.Reset();
	Result.TalkNarrow.Reset();
	const float Range = FMath::Max(Result.PeakDb - SpeechThreshold + 6.f, 6.f);
	for (int32 Index = 0; Index < WindowDb.Num(); ++Index)
	{
		const float Open = FMath::Clamp((WindowDb[Index] - SpeechThreshold + 6.f) / Range, 0.f, 1.f);
		Result.TalkOpen.Add(Open);
		Result.TalkNarrow.Add(Open > 0.f ? FMath::Clamp((WindowCrossings[Index] - 0.06f) / 0.2f, 0.f, 1.f) : 0.f);
	}

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
	CinemafyChoreoTake::PerformAndRecord(Choreography, /*bAskAboutProblems*/ false);
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

namespace
{
	UCinemafyPerformerProfile* NewProfileAsset(const FString& Name)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		FString PackageName, AssetName;
		AssetTools.CreateUniqueAssetName(FString::Printf(TEXT("/Game/Cinemafy/Performers/PP_%s"), *Name), TEXT(""), PackageName, AssetName);
		UPackage* Package = CreatePackage(*PackageName);
		UCinemafyPerformerProfile* Profile = NewObject<UCinemafyPerformerProfile>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Profile);
		Package->MarkPackageDirty();
		return Profile;
	}
}

UCinemafyPerformerProfile* UCinemafyEditorLibrary::CreatePerformerProfile(ECinemafyPerformerPreset Preset, const FString& Name)
{
	UCinemafyPerformerProfile* Profile = NewProfileAsset(Name.IsEmpty() ? StaticEnum<ECinemafyPerformerPreset>()->GetNameStringByValue(static_cast<int64>(Preset)) : Name);
	Profile->ApplyPreset(Preset);
	return Profile;
}

UCinemafyPerformerProfile* UCinemafyEditorLibrary::SaveWiringAsProfile(ACinemafyChoreography* Choreography)
{
	if (!Choreography || !Choreography->Performer)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: pick a performer first: a profile is made from what's found on it."));
		return nullptr;
	}

	// Detect with no profile in the way, so everything gets looked for.
	UCinemafyPerformerProfile* Previous = Choreography->Profile;
	Choreography->Profile = nullptr;
	Choreography->RefreshWiring();
	const FCinemafyDetectedWiring Found = Choreography->DetectedWiring;
	Choreography->Profile = Previous;

	const FString ClassName = Choreography->Performer->GetClass()->GetName().Replace(TEXT("_C"), TEXT(""));
	UCinemafyPerformerProfile* Profile = NewProfileAsset(ClassName);
	Profile->SprintFunction = Found.SprintFunction;
	Profile->SprintProperty = Found.SprintFunction.IsNone() ? Found.SprintProperty : FString();
	Profile->WalkFunction = Found.WalkFunction;
	Profile->WalkProperty = Found.WalkFunction.IsNone() ? Found.WalkProperty : FString();
	Profile->CrouchFunction = Found.CrouchFunction;
	Profile->UncrouchFunction = Found.UncrouchFunction;
	Profile->TraversalFunction = Found.TraversalFunction;
	Profile->Traversal.bCanTraverse = !Found.TraversalFunction.IsNone();
	Profile->OverlayFunction = Found.OverlayFunction;
	Profile->OverlayProperty = Found.OverlayFunction.IsNone() ? Found.OverlayProperty : FString();
	Profile->SkipRecordingComponents = Choreography->SkipRecordingComponents;
	Profile->Notes = FString::Printf(TEXT("Saved from what was found on %s (%s). Check the traversal limits against your character."),
		*Choreography->Performer->GetActorLabel(), *Choreography->Performer->GetClass()->GetName());

	const FScopedTransaction Transaction(LOCTEXT("AssignProfile", "Assign Performer Profile"));
	Choreography->Modify();
	Choreography->Profile = Profile;
	Choreography->RefreshWiring();

	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: saved %s from %s and assigned it."), *Profile->GetName(), *ClassName);
	return Profile;
}

FString UCinemafyEditorLibrary::GetWiringReport(ACinemafyChoreography* Choreography)
{
	if (!Choreography || !Choreography->Performer)
	{
		return TEXT("No performer.");
	}
	Choreography->RefreshWiring();
	FString Report = Choreography->WiringReport.Replace(TEXT("; "), TEXT("\n"));
	if (const UCinemafyPerformerProfile* Profile = Choreography->GetProfile())
	{
		Report += FString::Printf(TEXT("\n\nProfile %s against %s:\n%s"), *Profile->GetName(), *Choreography->Performer->GetActorLabel(), *Profile->Validate(Choreography->Performer));
	}
	const FCinemafyTraversalEnvelope Envelope = Choreography->GetTraversalEnvelope();
	Report += Envelope.bCanTraverse
		? FString::Printf(TEXT("\n\nClimbs: vaults up to %.0f cm (%.0f cm deep), mantles up to %.0f cm."), Envelope.MaxVaultHeight, Envelope.MaxVaultDepth, Envelope.MaxMantleHeight)
		: FString::Printf(TEXT("\n\nClimbs: nothing (jumps up to %.0f cm)."), Envelope.MaxJumpHeight);
	return Report;
}

#undef LOCTEXT_NAMESPACE

FCinemafyTraversalCheck UCinemafyEditorLibrary::CheckChoreographyTraversal(ACinemafyChoreography* Choreography, int32 BeatIndex)
{
	if (!Choreography || !Choreography->Beats.IsValidIndex(BeatIndex))
	{
		return FCinemafyTraversalCheck();
	}
	FCinemafyChoreoBeat& Beat = Choreography->Beats[BeatIndex];
	if (Beat.Reading.Summary.IsEmpty())
	{
		Choreography->ResolveBeat(BeatIndex);
	}
	Beat.TraversalCheck = Choreography->MeasureTraversal(BeatIndex);
	Choreography->RefreshLabels();
	return Beat.TraversalCheck;
}

bool UCinemafyEditorLibrary::SetChoreographyBeatLine(ACinemafyChoreography* Choreography, int32 BeatIndex, const FText& Text, USoundBase* Audio,
	AActor* Listener, AActor* WaitForLineFrom)
{
	if (!Choreography || !Choreography->Beats.IsValidIndex(BeatIndex))
	{
		return false;
	}
	Choreography->Modify();
	FCinemafyChoreoBeat& Beat = Choreography->Beats[BeatIndex];
	Beat.Dialogue = Text;
	Beat.DialogueAudio = Audio;
	Beat.Listener = Listener;
	Beat.WaitForLineFrom = WaitForLineFrom;
	Choreography->RefreshLabels();
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Script breakdown
// ---------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "CinemafyEditor"

namespace CinemafyScriptEditor
{
	UCinemafyScriptProject* FindOrCreateProject(const FString& Name)
	{
		const FString Clean = FPaths::MakeValidFileName(Name.IsEmpty() ? FString(TEXT("Script")) : Name).Replace(TEXT(" "), TEXT("_"));
		const FString PackageName = FString::Printf(TEXT("/Game/Cinemafy/Scripts/SP_%s"), *Clean);
		if (UCinemafyScriptProject* Existing = LoadObject<UCinemafyScriptProject>(nullptr, *(PackageName + TEXT(".SP_") + Clean), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		UCinemafyScriptProject* Project = NewObject<UCinemafyScriptProject>(Package, *(TEXT("SP_") + Clean), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Project);
		Package->MarkPackageDirty();
		return Project;
	}

	/**
	 * A script file's text. A PDF (a printed screenplay) goes through pdftotext, from poppler (Homebrew's
	 * "brew install poppler" on a Mac, "poppler-utils" on Linux, or on the PATH on Windows).
	 */
	bool LoadScriptText(const FString& FilePath, FString& OutText)
	{
		if (!FPaths::GetExtension(FilePath).Equals(TEXT("pdf"), ESearchCase::IgnoreCase))
		{
			return FFileHelper::LoadFileToString(OutText, *FilePath);
		}
		TArray<FString> Tools = { TEXT("/opt/homebrew/bin/pdftotext"), TEXT("/usr/local/bin/pdftotext"), TEXT("/usr/bin/pdftotext") };
		const FString Out = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Cinemafy") / (FPaths::GetBaseFilename(FilePath) + TEXT(".txt")));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out), true);
		const FString Args = FString::Printf(TEXT("-layout -enc UTF-8 \"%s\" \"%s\""), *FilePath, *Out);
		int32 Code = -1;
		FString StdOut, StdErr;
		for (const FString& Tool : Tools)
		{
			if (FPaths::FileExists(Tool) && FPlatformProcess::ExecProcess(*Tool, *Args, &Code, &StdOut, &StdErr) && Code == 0)
			{
				break;
			}
		}
		if (Code != 0)
		{
			// Last try: whatever pdftotext is on the PATH.
			FPlatformProcess::ExecProcess(TEXT("pdftotext"), *Args, &Code, &StdOut, &StdErr);
		}
		if (Code != 0 || !FFileHelper::LoadFileToString(OutText, *Out))
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: couldn't read the PDF %s. Install poppler for pdftotext (Mac: brew install poppler), or export the screenplay as Fountain or Final Draft (.fdx)."), *FilePath);
			return false;
		}
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: read the PDF screenplay %s (text kept at %s)."), *FilePath, *Out);
		return true;
	}

	/** An actor in the level for a script name: label equal, then a character whose label contains the name (or its last word). */
	AActor* FindActorForName(UWorld* World, const FString& ScriptName)
	{
		if (!World || ScriptName.IsEmpty())
		{
			return nullptr;
		}
		const FString Name = ScriptName.ToLower();
		FString LastWord = Name;
		int32 Space = INDEX_NONE;
		if (Name.FindLastChar(TEXT(' '), Space))
		{
			LastWord = Name.Mid(Space + 1);
		}
		AActor* Partial = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString Label = It->GetActorLabel().ToLower();
			if (Label == Name)
			{
				return *It;
			}
			if (!Partial && It->IsA<APawn>() && (Label.Contains(Name) || (LastWord.Len() >= 3 && Label.Contains(LastWord))))
			{
				Partial = *It;
			}
		}
		return Partial;
	}
}

UCinemafyScriptProject* UCinemafyEditorLibrary::ImportScriptFile(const FString& FilePath, const FString& Name)
{
	FString Text;
	if (!CinemafyScriptEditor::LoadScriptText(FilePath, Text))
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: couldn't read a script from %s."), *FilePath);
		return nullptr;
	}
	UCinemafyScriptProject* Project = CinemafyScriptEditor::FindOrCreateProject(Name.IsEmpty() ? FPaths::GetBaseFilename(FilePath) : Name);
	const FScopedTransaction Transaction(LOCTEXT("ImportScriptProject", "Cinemafy: Import Script"));
	Project->Modify();
	Project->ScriptFile.FilePath = FilePath;
	Project->BreakDown(Text);
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: broke down %s:\n%s"), *FilePath, *Project->BreakdownReport);
	return Project;
}

bool UCinemafyEditorLibrary::ReimportScriptProject(UCinemafyScriptProject* Project)
{
	FString Text;
	if (!Project || !CinemafyScriptEditor::LoadScriptText(Project->ScriptFile.FilePath, Text))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("ReimportScriptProject", "Cinemafy: Re-import Script"));
	Project->Modify();
	Project->BreakDown(Text);
	return true;
}

UCinemafyScriptProject* UCinemafyEditorLibrary::ImportScriptWithDialog()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		return nullptr;
	}
	TArray<FString> Files;
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (!Desktop->OpenFileDialog(Parent, TEXT("Import a script"), FPaths::ProjectDir(), FString(),
		TEXT("Scripts (*.fountain;*.fdx;*.txt;*.md;*.json;*.pdf)|*.fountain;*.fdx;*.txt;*.md;*.json;*.pdf"), EFileDialogFlags::None, Files) || Files.Num() == 0)
	{
		return nullptr;
	}
	UCinemafyScriptProject* Project = ImportScriptFile(Files[0], FString());
	if (Project)
	{
		TArray<UObject*> ToSync = { Project };
		GEditor->SyncBrowserToObjects(ToSync);
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Project);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Project->BreakdownReport.Left(2000)));
	}
	return Project;
}

int32 UCinemafyEditorLibrary::MapScriptScenesInLevel(UCinemafyScriptProject* Project)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!Project || !World)
	{
		return 0;
	}
	const FScopedTransaction Transaction(LOCTEXT("MapScriptScenes", "Cinemafy: Map Script Scenes"));
	Project->Modify();

	// Directors with a recorded take handed to them (choreographies).
	TSet<const ACinemafySceneDirector*> Recorded;
	for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
	{
		if (It->TakeDuration > 0.f && It->SceneDirector)
		{
			Recorded.Add(It->SceneDirector);
		}
	}

	int32 Mapped = 0;
	TArray<FCinemafyScriptScene*> Scenes = Project->GetAllScenes();
	for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
	{
		ACinemafySceneDirector* Director = *It;
		FCinemafyScriptScene* Scene = nullptr;
		float Score = 1.f;
		if (Director->ScriptProject == Project && !Director->ScriptSceneId.IsEmpty())
		{
			Scene = Project->FindScene(Director->ScriptSceneId);
		}
		if (!Scene)
		{
			// Match by what's said.
			TArray<FString> Spoken;
			for (const FCinemafyLine& Line : Director->Lines)
			{
				if (!Line.Text.IsEmpty())
				{
					Spoken.Add(Line.Text.ToString());
				}
			}
			float Best = 0.f;
			for (FCinemafyScriptScene* Candidate : Scenes)
			{
				const float CandidateScore = UCinemafyScriptLibrary::ScoreLinesAgainstScene(Spoken, *Candidate);
				if (CandidateScore > Best)
				{
					Best = CandidateScore;
					Scene = Candidate;
				}
			}
			Score = Best;
			if (Best < 0.5f)
			{
				Scene = nullptr;
			}
		}
		if (!Scene)
		{
			continue;
		}

		Director->Modify();
		Director->ScriptProject = Project;
		Director->ScriptSceneId = Scene->Id;
		Scene->DirectorLabel = Director->GetActorLabel();
		Scene->Level = World;
		Scene->MatchScore = Score;
		if (!Director->LastSequence.IsNull())
		{
			Scene->Sequence = Director->LastSequence;
		}
		ECinemafySceneStatus Reached = ECinemafySceneStatus::Blocking;
		if (Recorded.Contains(Director))
		{
			Reached = ECinemafySceneStatus::Recorded;
		}
		if (Director->LastPlan.Num() > 0)
		{
			Reached = ECinemafySceneStatus::Directed;
		}
		if (Scene->Status < Reached && Scene->Status != ECinemafySceneStatus::Cut)
		{
			Scene->Status = Reached;
		}
		++Mapped;
	}
	Project->MarkPackageDirty();
	return Mapped;
}

namespace CinemafyScriptEditor
{
	/** Who or what a camera direction names: one of the scene's characters ("THANE", "KADE'S HAND"), else a prop by label. */
	AActor* FindShotSubject(UWorld* World, const FCinemafyScriptScene& Scene, const FString& Subject)
	{
		FString Clean = Subject.ToUpper().TrimStartAndEnd();
		if (Clean.StartsWith(TEXT("THE ")))
		{
			Clean = Clean.Mid(4);
		}
		if (Clean.IsEmpty())
		{
			return nullptr;
		}
		const int32 Possessive = Clean.Find(TEXT("'S"));
		const FString Owner = Possessive != INDEX_NONE ? Clean.Left(Possessive).TrimEnd() : Clean;
		TArray<FString> OwnerWords;
		Owner.ParseIntoArrayWS(OwnerWords);
		for (const FString& Character : Scene.Characters)
		{
			TArray<FString> Words;
			Character.ParseIntoArrayWS(Words);
			if (Character == Owner || (OwnerWords.Num() > 0 && Words.Contains(OwnerWords.Last())))
			{
				if (AActor* Actor = FindActorForName(World, Character))
				{
					return Actor;
				}
			}
		}
		// A prop or piece of the set, by its label ("Prop_datapad", "Console").
		const FString Wanted = Clean.ToLower();
		const FString LastWord = OwnerWords.Num() > 0 ? OwnerWords.Last().ToLower() : Wanted;
		AActor* Partial = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<APawn>() || It->IsA<ACinemafySceneDirector>())
			{
				continue;
			}
			const FString Label = It->GetActorLabel().ToLower().Replace(TEXT("_"), TEXT(" "));
			if (Label.Contains(Wanted))
			{
				return *It;
			}
			if (!Partial && LastWord.Len() >= 4 && Label.Contains(LastWord))
			{
				Partial = *It;
			}
		}
		return Partial ? Partial : FindActorForName(World, Owner);
	}

	/** The director's lines for a script scene: speakers matched to actor labels, delivery read from the parentheticals,
	 * camera directions ("CLOSE ON THANE", "KADE'S POV", "SLOW PUSH IN", "HANDHELD") on the line they come before, and
	 * the scene's transition in on its first line. */
	TArray<FCinemafyLine> BuildDirectorLines(UWorld* World, const FCinemafyScriptScene& Scene, TArray<AActor*>& OutPerformers, int32& OutUnmatched)
	{
		// With no parenthetical, a line plays its own words, else the scene's mood (a love scene isn't Neutral).
		FString SceneMood = UCinemafyMoodLibrary::ReadSceneMood(Scene, ECinemafyTimeOfDay::Unknown).Mood;
		TArray<FCinemafyLine> Lines;
		FString PreviousSpeaker;
		AActor* PreviousActor = nullptr;
		FCinemafyShotCall PendingShot;
		FString PendingWords;
		bool bPendingShot = false;
		bool bFeel = false;
		ECinemafyCameraFeel Feel = ECinemafyCameraFeel::Handheld;
		for (const FCinemafyScriptLine& ScriptLine : Scene.Lines)
		{
			if (ScriptLine.Kind == ECinemafyScriptLineKind::Shot)
			{
				const FCinemafyShotCall& Call = ScriptLine.Shot;
				if (Call.bFeel)
				{
					bFeel = true;
					Feel = Call.Feel;
				}
				if (Call.bBackToScene && !Call.bFraming)
				{
					bPendingShot = false;
					continue;
				}
				if (Call.bFraming || Call.bMove)
				{
					// The last framing called before a line wins; a move called on its own rides along with it.
					const bool bKeepMove = PendingShot.bMove && !Call.bMove;
					const ECinemafyCameraMove KeptMove = PendingShot.Move;
					PendingShot = Call;
					if (bKeepMove)
					{
						PendingShot.bMove = true;
						PendingShot.Move = KeptMove;
					}
					PendingWords = bPendingShot && !PendingWords.IsEmpty() ? PendingWords + TEXT(" / ") + ScriptLine.Text : ScriptLine.Text;
					bPendingShot = true;
				}
				continue;
			}
			if (ScriptLine.Kind != ECinemafyScriptLineKind::Dialogue)
			{
				continue;
			}
			FCinemafyLine& Line = Lines.AddDefaulted_GetRef();
			Line.Speaker = FindActorForName(World, ScriptLine.Speaker);
			Line.SpeakerName = FText::FromString(ScriptLine.Speaker);
			Line.Text = FText::FromString(ScriptLine.Text);
			Line.Audio = ScriptLine.Audio.LoadSynchronous();
			// A face performance made from this audio (MetaHuman lip-sync) plays instead of the Talk curves.
			Line.FaceAnimation = ScriptLine.HasCurrentFace() ? ScriptLine.FaceAnimation.LoadSynchronous() : nullptr;
			// How it's played, from the parenthetical: feeling, intensity (and so the face, body casting and cut).
			const FCinemafyDelivery Delivery = UCinemafyScriptLibrary::ReadDeliveryInMood(ScriptLine.Direction, ScriptLine.Text, SceneMood);
			SceneMood = UCinemafyScriptLibrary::MoodAfterLine(SceneMood, Delivery);
			if (Delivery.bHasEmotion)
			{
				Line.bSetEmotion = true;
				Line.Emotion = Delivery.Emotion;
			}
			if (!Delivery.Cue.IsEmpty())
			{
				Line.Intensity = Delivery.Intensity;
				Line.bAutoIntensity = false;
			}
			// Said to whoever spoke last (the usual back-and-forth); the first line goes to the next speaker.
			Line.Listener = PreviousSpeaker != ScriptLine.Speaker ? PreviousActor : nullptr;
			Line.Note = FString::Printf(TEXT("%s %d. %s: %s"), *Scene.Id, Lines.Num(), *ScriptLine.Speaker, *ScriptLine.Text.Left(40));

			// The page's camera.
			if (Lines.Num() == 1 && Scene.bTransitionIn)
			{
				Line.bOverrideTransition = true;
				Line.Transition = Scene.TransitionIn;
			}
			if (bFeel)
			{
				Line.bForceFeel = true;
				Line.ForcedFeel = Feel;
			}
			if (bPendingShot)
			{
				Line.bForceShot = true;
				Line.ForcedShot = PendingShot.ShotType;
				Line.ShotCall = PendingWords;
				AActor* On = FindShotSubject(World, Scene, PendingShot.Subject);
				if (PendingShot.bOverShoulderOfSubject && On)
				{
					// Over their shoulder: the shot is on whoever they face.
					On = On == Line.Speaker ? Line.Listener.Get() : Line.Speaker.Get();
				}
				Line.ShotOn = On;
				if (PendingShot.bMove)
				{
					Line.bForceMove = true;
					Line.ForcedMove = PendingShot.Move;
				}
				bPendingShot = false;
				PendingShot = FCinemafyShotCall();
				PendingWords.Reset();
			}
			TArray<FString> Words;
			ScriptLine.Text.ParseIntoArrayWS(Words);
			Line.Duration = FMath::Max(1.f, Words.Num() / 2.6f + 0.4f);
			OutUnmatched += Line.Speaker ? 0 : 1;
			if (Line.Speaker)
			{
				OutPerformers.AddUnique(Line.Speaker);
			}
			if (PreviousSpeaker != ScriptLine.Speaker)
			{
				PreviousSpeaker = ScriptLine.Speaker;
				PreviousActor = Line.Speaker;
			}
		}
		if (Lines.Num() > 1 && !Lines[0].Listener)
		{
			for (int32 Index = 1; Index < Lines.Num(); ++Index)
			{
				if (Lines[Index].Speaker && Lines[Index].Speaker != Lines[0].Speaker)
				{
					Lines[0].Listener = Lines[Index].Speaker;
					break;
				}
			}
		}
		return Lines;
	}
}

ACinemafySceneDirector* UCinemafyEditorLibrary::CreateDirectorForScene(UCinemafyScriptProject* Project, const FString& SceneId)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!World || !Scene)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("DirectorForScene", "Cinemafy: Director for Scene"));
	TArray<AActor*> Performers;
	int32 Unmatched = 0;
	// "<scene>#<n>": a director for one insert of a gameplay bridge (its lines only).
	int32 InsertIndex = 0;
	UCinemafyScriptLibrary::SplitInsertId(SceneId, InsertIndex);
	const bool bInsert = Project->FindInsert(SceneId) != nullptr;
	const FString TargetId = bInsert ? FString::Printf(TEXT("%s#%d"), *Scene->Id, InsertIndex) : Scene->Id;
	const TArray<FCinemafyLine> Lines = CinemafyScriptEditor::BuildDirectorLines(World, UCinemafyScriptLibrary::SceneForInsert(*Scene, bInsert ? InsertIndex : 0), Performers, Unmatched);

	FVector Location = FVector::ZeroVector;
	for (AActor* Member : Performers)
	{
		Location += Member->GetActorLocation() / Performers.Num();
	}
	FActorSpawnParameters Params;
	ACinemafySceneDirector* Director = World->SpawnActor<ACinemafySceneDirector>(Location, FRotator::ZeroRotator, Params);
	if (!Director)
	{
		return nullptr;
	}
	Director->SetActorLabel(FString::Printf(TEXT("Director_%s"), *TargetId.Replace(TEXT("."), TEXT("_")).Replace(TEXT("#"), TEXT("_I"))));
	Director->Lines = Lines;
	Director->ScriptProject = Project;
	Director->ScriptSceneId = TargetId;

	Project->Modify();
	if (!bInsert)
	{
		Scene->DirectorLabel = Director->GetActorLabel();
	}
	Scene->Level = World;
	Scene->MatchScore = 1.f;
	Scene->StagedHash = Scene->ContentHash;
	if (Scene->Status == ECinemafySceneStatus::NotStarted)
	{
		Scene->Status = ECinemafySceneStatus::Blocking;
	}
	if (Unmatched > 0)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: %d line(s) in scene %s have a speaker with no matching actor label; set those Speakers by hand."), Unmatched, *Scene->Id);
	}
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Director, true, true);
	return Director;
}

int32 UCinemafyEditorLibrary::RefreshDirectorFromScript(ACinemafySceneDirector* Director)
{
	UCinemafyScriptProject* Project = Director ? Director->ScriptProject.Get() : nullptr;
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(Director->ScriptSceneId) : nullptr;
	if (!Scene || !Director->GetWorld())
	{
		return -1;
	}
	TArray<AActor*> Performers;
	int32 Unmatched = 0;
	int32 InsertIndex = 0;
	UCinemafyScriptLibrary::SplitInsertId(Director->ScriptSceneId, InsertIndex);
	TArray<FCinemafyLine> Fresh = CinemafyScriptEditor::BuildDirectorLines(Director->GetWorld(), UCinemafyScriptLibrary::SceneForInsert(*Scene, InsertIndex), Performers, Unmatched);

	// Unchanged words keep the line as it was tuned (first unused match, so repeated lines pair up in order).
	TArray<bool> Used;
	Used.Init(false, Director->Lines.Num());
	int32 Changed = 0;
	for (FCinemafyLine& Line : Fresh)
	{
		const FString Words = Line.Text.ToString();
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Director->Lines.Num(); ++Index)
		{
			if (!Used[Index] && Director->Lines[Index].Text.ToString().Equals(Words, ESearchCase::IgnoreCase))
			{
				Found = Index;
				break;
			}
		}
		if (Found != INDEX_NONE)
		{
			Used[Found] = true;
			const FCinemafyLine FromScript = Line;
			Line = Director->Lines[Found];
			Line.Note = FromScript.Note;
			// The page's camera directions are the script's to change: they follow it.
			if (FromScript.bForceShot || !Line.ShotCall.IsEmpty())
			{
				Line.bForceShot = FromScript.bForceShot;
				Line.ForcedShot = FromScript.ForcedShot;
				Line.ShotOn = FromScript.ShotOn;
				Line.ShotCall = FromScript.ShotCall;
				Line.bForceMove = FromScript.bForceMove;
				Line.ForcedMove = FromScript.ForcedMove;
			}
			if (FromScript.FaceAnimation || FromScript.Audio != Line.Audio)
			{
				Line.FaceAnimation = FromScript.FaceAnimation;
			}
			Line.bForceFeel = FromScript.bForceFeel || Line.bForceFeel;
			Line.ForcedFeel = FromScript.bForceFeel ? FromScript.ForcedFeel : Line.ForcedFeel;
			if (FromScript.bOverrideTransition)
			{
				Line.bOverrideTransition = true;
				Line.Transition = FromScript.Transition;
			}
		}
		else
		{
			++Changed;
		}
	}
	for (bool bUsed : Used)
	{
		Changed += bUsed ? 0 : 1;
	}
	const FScopedTransaction Transaction(LOCTEXT("RefreshDirector", "Cinemafy: Refresh Director from Script"));
	Director->Modify();
	Director->Lines = MoveTemp(Fresh);
	Project->Modify();
	Scene->StagedHash = Scene->ContentHash;
	Project->MarkPackageDirty();
	return Changed;
}

bool UCinemafyEditorLibrary::SetScriptSceneStatus(UCinemafyScriptProject* Project, const FString& SceneId, ECinemafySceneStatus Status)
{
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!Scene)
	{
		return false;
	}
	Project->Modify();
	Scene->Status = Status;
	Project->MarkPackageDirty();
	return true;
}

FString UCinemafyEditorLibrary::ExportScriptChecklist(UCinemafyScriptProject* Project)
{
	if (!Project)
	{
		return FString();
	}
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Cinemafy") /
		(FPaths::MakeValidFileName(Project->Title.IsEmpty() ? Project->GetName() : Project->Title) + TEXT("_Checklist.md")));
	FFileHelper::SaveStringToFile(Project->BuildChecklist(), *Path, FFileHelper::EEncodingOptions::ForceUTF8);
	return Path;
}

#undef LOCTEXT_NAMESPACE
