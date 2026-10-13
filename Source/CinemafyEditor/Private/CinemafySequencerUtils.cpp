#include "CinemafySequencerUtils.h"
#include "Editor.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "MovieScene.h"
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

	void NotifyChanged()
	{
		ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
	}
}
