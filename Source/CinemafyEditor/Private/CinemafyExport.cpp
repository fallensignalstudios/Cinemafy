#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafySceneDirector.h"
#include "CinemafyFocusComponent.h"
#include "CinemafySequencerUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Editor.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorSubsystem.h"
#include "MovieScene.h"
#include "MovieSceneBindingProxy.h"
#include "Misc/PackageName.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"

namespace CinemafyExport
{
	bool IsCinemafyClass(const UClass* Class)
	{
		for (const UClass* Each = Class; Each; Each = Each->GetSuperClass())
		{
			if (Each->GetName().StartsWith(TEXT("Cinemafy")))
			{
				return true;
			}
		}
		return false;
	}

	struct FCameraBake
	{
		ACineCameraActor* Camera = nullptr;
		FGuid Binding;
		FTransform ActorRest;
		TArray<FTransform> Transforms;
		TArray<float> Focus;
		TArray<float> Focal;
	};

	/** A binding's tracks and its children's bindings, gone (the binding itself stays unless bWhole). */
	int32 ClearBinding(ULevelSequence* Sequence, UMovieScene* MovieScene, const FGuid& Guid, bool bWhole)
	{
		int32 Removed = 0;
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
			++Removed;
		}
		const UMovieScene* ConstMovieScene = MovieScene;
		if (const FMovieSceneBinding* Binding = ConstMovieScene->FindBinding(Guid))
		{
			const TArray<UMovieSceneTrack*> Tracks = Binding->GetTracks();
			for (UMovieSceneTrack* Track : Tracks)
			{
				MovieScene->RemoveTrack(*Track);
				++Removed;
			}
		}
		if (bWhole)
		{
			Sequence->UnbindPossessableObjects(Guid);
			MovieScene->RemovePossessable(Guid);
		}
		return Removed;
	}

	template <typename TSection>
	TSection* AddSection(UMovieSceneTrack* Track)
	{
		TSection* Section = Cast<TSection>(Track->CreateNewSection());
		if (Section)
		{
			Section->SetRange(TRange<FFrameNumber>::All());
			Section->SetCompletionMode(EMovieSceneCompletionMode::RestoreState);
			Track->AddSection(*Section);
		}
		return Section;
	}
}

ULevelSequence* UCinemafyEditorLibrary::ExportSequenceForPlayback(ULevelSequence* Source, const FString& Destination, bool bSpawnableCameras, FString& OutReport)
{
	using namespace CinemafyExport;
	TArray<FString> Report;
	if (!Source || !OpenSequenceForEditing(Source))
	{
		OutReport = TEXT("Couldn't open the sequence in Sequencer to play it through.");
		return nullptr;
	}
	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UMovieScene* MovieScene = Source->GetMovieScene();
	if (!Sequencer.IsValid() || !World || !MovieScene)
	{
		OutReport = TEXT("No Sequencer or editor world to bake in.");
		return nullptr;
	}

	// What's in it: cameras to bake, directors to drop, Cinemafy components bound on anyone (talk curves, gaze).
	TArray<FCameraBake> Cameras;
	TArray<FGuid> Directors;
	TArray<FGuid> CinemafyChildren;
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(Index);
		if (Possessable.GetParent().IsValid())
		{
			for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(Possessable.GetGuid(), MovieSceneSequenceID::Root))
			{
				if (Bound.IsValid() && IsCinemafyClass(Bound->GetClass()))
				{
					CinemafyChildren.Add(Possessable.GetGuid());
					break;
				}
			}
			continue;
		}
		for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(Possessable.GetGuid(), MovieSceneSequenceID::Root))
		{
			if (ACineCameraActor* Camera = Cast<ACineCameraActor>(Bound.Get()))
			{
				FCameraBake& Bake = Cameras.AddDefaulted_GetRef();
				Bake.Camera = Camera;
				Bake.Binding = Possessable.GetGuid();
				Bake.ActorRest = Camera->GetActorTransform();
				break;
			}
			if (Bound.IsValid() && Bound->IsA<ACinemafySceneDirector>())
			{
				Directors.Add(Possessable.GetGuid());
				break;
			}
		}
	}

	// Play it through, a frame at a time, running each camera's Cinemafy rig as it would in play (moves and feel,
	// then focus), and record what the lens actually does.
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
	const FFrameNumber First = FFrameRate::TransformTime(FFrameTime(Range.GetLowerBoundValue()), TickResolution, DisplayRate).FloorToFrame();
	const FFrameNumber Last = FFrameRate::TransformTime(FFrameTime(Range.GetUpperBoundValue()), TickResolution, DisplayRate).CeilToFrame();
	const float Step = static_cast<float>(DisplayRate.AsInterval());
	const FQualifiedFrameTime Restore = Sequencer->GetLocalTime();
	TArray<FFrameNumber> Frames;
	for (FFrameNumber Frame = First; Frame <= Last; ++Frame)
	{
		Frames.Add(FFrameRate::TransformTime(FFrameTime(Frame), DisplayRate, TickResolution).RoundToFrame());
		Sequencer->SetLocalTimeDirectly(FFrameTime(Frames.Last()));
		Sequencer->ForceEvaluate();
		for (FCameraBake& Bake : Cameras)
		{
			TInlineComponentArray<UActorComponent*> Components(Bake.Camera);
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (UActorComponent* Component : Components)
				{
					if (IsCinemafyClass(Component->GetClass()) && (Pass == 1) == Component->IsA<UCinemafyFocusComponent>())
					{
						Component->TickComponent(Step, LEVELTICK_All, nullptr);
					}
				}
			}
			const UCineCameraComponent* Cine = Bake.Camera->GetCineCameraComponent();
			Bake.Transforms.Add(Cine->GetComponentTransform());
			Bake.Focus.Add(Cine->FocusSettings.ManualFocusDistance);
			Bake.Focal.Add(Cine->CurrentFocalLength);
		}
	}
	Sequencer->SetLocalTimeDirectly(ConvertFrameTime(Restore.Time, Restore.Rate, TickResolution));
	Sequencer->ForceEvaluate();
	for (FCameraBake& Bake : Cameras)
	{
		Bake.Camera->SetActorTransform(Bake.ActorRest); // The previs cameras stay as they were.
	}
	Report.Add(FString::Printf(TEXT("Played %d frame(s) through for %d camera(s)."), Frames.Num(), Cameras.Num()));

	// The copy.
	FString PackagePath = FPackageName::GetLongPackagePath(Source->GetPackage()->GetName());
	FString AssetName = Source->GetName() + TEXT("_Playback");
	if (!Destination.IsEmpty())
	{
		PackagePath = FPackageName::GetLongPackagePath(Destination);
		AssetName = FPackageName::GetLongPackageAssetName(Destination);
	}
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	FString UniquePackage, UniqueName;
	AssetTools.CreateUniqueAssetName(PackagePath / AssetName, TEXT(""), UniquePackage, UniqueName);
	ULevelSequence* Dest = Cast<ULevelSequence>(AssetTools.DuplicateAsset(UniqueName, PackagePath, Source));
	UMovieScene* Out = Dest ? Dest->GetMovieScene() : nullptr;
	if (!Out)
	{
		OutReport = FString::Join(Report, TEXT("\n")) + TEXT("\nCouldn't make the copy.");
		return nullptr;
	}
	Dest->Modify();
	Out->Modify();

	// No director: its binding goes, with its transition cues.
	for (const FGuid& Guid : Directors)
	{
		ClearBinding(Dest, Out, Guid, true);
	}
	// No Cinemafy components bound on anyone (talk curves on a face component, gaze): their tracks would do nothing.
	int32 ComponentBindings = 0;
	for (const FGuid& Guid : CinemafyChildren)
	{
		if (Out->FindPossessable(Guid))
		{
			ClearBinding(Dest, Out, Guid, true);
			++ComponentBindings;
		}
	}

	// Plain cameras: a copy of each, no Cinemafy components, its lens where the rig left it, bound in place of the
	// original (which stays for the previs).
	for (FCameraBake& Bake : Cameras)
	{
		ClearBinding(Dest, Out, Bake.Binding, false);
		FActorSpawnParameters Params;
		Params.Template = Bake.Camera;
		Params.ObjectFlags |= RF_Transactional;
		ACineCameraActor* Copy = World->SpawnActor<ACineCameraActor>(Bake.Camera->GetClass(), Bake.ActorRest, Params);
		if (!Copy)
		{
			continue;
		}
		Copy->SetActorLabel(Bake.Camera->GetActorLabel() + TEXT("_Playback"));
		Copy->Tags.Reset();
		TInlineComponentArray<UActorComponent*> Components(Copy);
		for (UActorComponent* Component : Components)
		{
			if (IsCinemafyClass(Component->GetClass()))
			{
				Component->DestroyComponent();
			}
		}
		UCineCameraComponent* Cine = Copy->GetCineCameraComponent();
		Cine->SetRelativeTransform(FTransform::Identity); // The baked transform is the lens's own.
		Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		Dest->UnbindPossessableObjects(Bake.Binding);
		Dest->BindPossessableObject(Bake.Binding, *Copy, World);
		Bake.Camera = Copy;
	}

	// The keys, on the copy (opened in Sequencer so the camera component gets a binding of its own).
	if (!OpenSequenceForEditing(Dest))
	{
		OutReport = FString::Join(Report, TEXT("\n")) + TEXT("\nCouldn't open the copy to key it.");
		return Dest;
	}
	for (const FCameraBake& Bake : Cameras)
	{
		UMovieScene3DTransformTrack* Track = Out->AddTrack<UMovieScene3DTransformTrack>(Bake.Binding);
		UMovieScene3DTransformSection* Section = Track ? AddSection<UMovieScene3DTransformSection>(Track) : nullptr;
		const TArrayView<FMovieSceneDoubleChannel*> Channels = Section ? Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>() : TArrayView<FMovieSceneDoubleChannel*>();
		if (Channels.Num() >= 9)
		{
			FRotator Previous = Bake.Transforms.Num() > 0 ? Bake.Transforms[0].Rotator() : FRotator::ZeroRotator;
			for (int32 Index = 0; Index < Bake.Transforms.Num() && Index < Frames.Num(); ++Index)
			{
				const FVector Location = Bake.Transforms[Index].GetLocation();
				FRotator Rotation = Bake.Transforms[Index].Rotator();
				Rotation.Yaw = Previous.Yaw + FRotator::NormalizeAxis(Rotation.Yaw - Previous.Yaw);
				Rotation.Pitch = Previous.Pitch + FRotator::NormalizeAxis(Rotation.Pitch - Previous.Pitch);
				Rotation.Roll = Previous.Roll + FRotator::NormalizeAxis(Rotation.Roll - Previous.Roll);
				Previous = Rotation;
				Channels[0]->AddLinearKey(Frames[Index], Location.X);
				Channels[1]->AddLinearKey(Frames[Index], Location.Y);
				Channels[2]->AddLinearKey(Frames[Index], Location.Z);
				Channels[3]->AddLinearKey(Frames[Index], Rotation.Roll);
				Channels[4]->AddLinearKey(Frames[Index], Rotation.Pitch);
				Channels[5]->AddLinearKey(Frames[Index], Rotation.Yaw);
			}
			for (int32 Index = 6; Index < 9; ++Index)
			{
				Channels[Index]->SetDefault(1.0);
			}
		}

		// Focus (and the focal length, when a move zooms) on the camera component.
		const FGuid Lens = CinemafySequencer::BindObject(Dest, Bake.Camera->GetCineCameraComponent());
		if (!Lens.IsValid())
		{
			continue;
		}
		auto KeyFloat = [&](FName Property, const FString& Path, const TArray<float>& Values)
		{
			UMovieSceneFloatTrack* FloatTrack = Out->AddTrack<UMovieSceneFloatTrack>(Lens);
			FloatTrack->SetPropertyNameAndPath(Property, Path);
			UMovieSceneFloatSection* FloatSection = AddSection<UMovieSceneFloatSection>(FloatTrack);
			FMovieSceneFloatChannel* Channel = FloatSection ? FloatSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0) : nullptr;
			for (int32 Index = 0; Channel && Index < Values.Num() && Index < Frames.Num(); ++Index)
			{
				Channel->AddLinearKey(Frames[Index], Values[Index]);
			}
		};
		KeyFloat(TEXT("ManualFocusDistance"), TEXT("FocusSettings.ManualFocusDistance"), Bake.Focus);
		float FocalLow = TNumericLimits<float>::Max(), FocalHigh = 0.f;
		for (const float Focal : Bake.Focal)
		{
			FocalLow = FMath::Min(FocalLow, Focal);
			FocalHigh = FMath::Max(FocalHigh, Focal);
		}
		if (FocalHigh - FocalLow > 0.1f)
		{
			KeyFloat(TEXT("CurrentFocalLength"), TEXT("CurrentFocalLength"), Bake.Focal);
		}
		else if (Bake.Focal.Num() > 0)
		{
			Bake.Camera->GetCineCameraComponent()->SetCurrentFocalLength(Bake.Focal[0]);
		}
	}
	Report.Add(FString::Printf(TEXT("Baked tracking, moves, feel and focus to plain keys on %d camera(s); removed %d director binding(s) and %d Cinemafy component binding(s) (talk curves, gaze: use your own lip-sync and look-at)."),
		Cameras.Num(), Directors.Num(), ComponentBindings));

	// Owned by the sequence: the cameras move in, so the map needn't be saved.
	if (bSpawnableCameras)
	{
		int32 Converted = 0;
		if (ULevelSequenceEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULevelSequenceEditorSubsystem>())
		{
			for (const FCameraBake& Bake : Cameras)
			{
				Subsystem->ConvertToSpawnable(FMovieSceneBindingProxy(Bake.Binding, Dest));
				++Converted;
			}
		}
		Report.Add(FString::Printf(TEXT("%d camera(s) are now spawnables owned by the sequence."), Converted));
	}
	Dest->MarkPackageDirty();
	Report.Add(FString::Printf(TEXT("Wrote %s (unsaved: save it with the rest)."), *Dest->GetPathName()));
	OutReport = FString::Join(Report, TEXT("\n"));
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy export:\n%s"), *OutReport);
	return Dest;
}
