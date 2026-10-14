#include "CinemafySequencerUtils.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyFollowComponent.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Editor.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace CinemafySequencer
{
	ULevelSequence* GetOpenSequence()
	{
		return ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
	}

	TSharedPtr<ISequencer> GetOpenSequencer()
	{
		ULevelSequence* Sequence = GetOpenSequence();
		UAssetEditorSubsystem* AssetEditors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
		IAssetEditorInstance* Editor = (Sequence && AssetEditors) ? AssetEditors->FindEditorForAsset(Sequence, false) : nullptr;
		if (!Editor || Editor->GetEditorName() != FName(TEXT("LevelSequenceEditor")))
		{
			return nullptr;
		}
		return static_cast<ILevelSequenceEditorToolkit*>(Editor)->GetSequencer();
	}

	FFrameNumber GetPlayheadTick(const UMovieScene* MovieScene)
	{
		if (const TSharedPtr<ISequencer> Sequencer = GetOpenSequencer())
		{
			const FQualifiedFrameTime Now = Sequencer->GetLocalTime();
			return ConvertFrameTime(Now.Time, Now.Rate, MovieScene->GetTickResolution()).FloorToFrame();
		}
		return MovieScene->GetPlaybackRange().GetLowerBoundValue();
	}

	FGuid BindActor(ULevelSequence* Sequence, AActor* Actor)
	{
		// Sequencer finds an existing binding or makes a new one, with all the bookkeeping done for us.
		const TSharedPtr<ISequencer> Sequencer = GetOpenSequencer();
		if (Sequencer.IsValid() && Sequencer->GetFocusedMovieSceneSequence() == Sequence)
		{
			return Sequencer->GetHandleToObject(Actor, true);
		}

		// Sequencer isn't focused on this sequence: add a fresh possessable by hand.
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		Sequence->Modify();
		MovieScene->Modify();
		const FGuid Binding = MovieScene->AddPossessable(Actor->GetActorLabel(), Actor->GetClass());
		Sequence->BindPossessableObject(Binding, *Actor, Actor->GetWorld());
		return Binding;
	}

	FGuid BindObject(ULevelSequence* Sequence, UObject* Object)
	{
		const TSharedPtr<ISequencer> Sequencer = GetOpenSequencer();
		if (Sequencer.IsValid() && Sequencer->GetFocusedMovieSceneSequence() == Sequence)
		{
			return Sequencer->GetHandleToObject(Object, true);
		}
		return FGuid();
	}

	FGuid FindBinding(ULevelSequence* Sequence, AActor* Actor)
	{
		const TSharedPtr<ISequencer> Sequencer = GetOpenSequencer();
		if (Sequencer.IsValid() && Sequencer->GetFocusedMovieSceneSequence() == Sequence)
		{
			return Sequencer->GetHandleToObject(Actor, false);
		}
		return FGuid();
	}

	void NotifyChanged()
	{
		ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
	}

	int32 MuteBindingTracks(UMovieScene* MovieScene, const FGuid& Binding, FName TrackClass, const UMovieSceneTrack* Keep)
	{
		const UMovieScene* ConstMovieScene = MovieScene;
		const FMovieSceneBinding* Found = ConstMovieScene ? ConstMovieScene->FindBinding(Binding) : nullptr;
		if (!Found)
		{
			return 0;
		}
		int32 Muted = 0;
		for (UMovieSceneTrack* Track : Found->GetTracks())
		{
			if (Track && Track != Keep && !Track->IsEvalDisabled() && Track->GetClass()->GetFName() == TrackClass)
			{
				Track->Modify();
				Track->SetEvalDisabled(true);
				++Muted;
			}
		}
		return Muted;
	}

	int32 MuteChildPropertyTracks(UMovieScene* MovieScene, const FGuid& Binding, FName PropertyName)
	{
		if (!MovieScene)
		{
			return 0;
		}
		int32 Muted = 0;
		const UMovieScene* ConstMovieScene = MovieScene;
		for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
		{
			const FMovieScenePossessable& Child = MovieScene->GetPossessable(Index);
			const FMovieSceneBinding* ChildBinding = Child.GetParent() == Binding ? ConstMovieScene->FindBinding(Child.GetGuid()) : nullptr;
			if (!ChildBinding)
			{
				continue;
			}
			for (UMovieSceneTrack* Track : ChildBinding->GetTracks())
			{
				const UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track);
				if (PropertyTrack && !Track->IsEvalDisabled() && PropertyTrack->GetPropertyName() == PropertyName)
				{
					Track->Modify();
					Track->SetEvalDisabled(true);
					++Muted;
				}
			}
		}
		return Muted;
	}

	int32 LetCinemafyDriveCamera(UMovieScene* MovieScene, const FGuid& Binding, const AActor* Camera)
	{
		if (!Camera)
		{
			return 0;
		}
		int32 Muted = 0;
		const UCinemafyFollowComponent* Follow = Camera->FindComponentByClass<UCinemafyFollowComponent>();
		if (Follow && Follow->Target)
		{
			Muted += MuteBindingTracks(MovieScene, Binding, TEXT("MovieScene3DTransformTrack"));
		}
		const UCinemafyFocusComponent* Focus = Camera->FindComponentByClass<UCinemafyFocusComponent>();
		if (Focus && Focus->FocusTarget)
		{
			Muted += MuteChildPropertyTracks(MovieScene, Binding, TEXT("ManualFocusDistance"));
		}
		return Muted;
	}
}
