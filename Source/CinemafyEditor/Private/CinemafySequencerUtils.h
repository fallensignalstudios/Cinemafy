#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameNumber.h"

class ISequencer;
class ULevelSequence;
class UMovieScene;
class AActor;

/** Shared helpers for talking to the Level Sequence that's open in Sequencer. */
namespace CinemafySequencer
{
	/** The Level Sequence currently open in Sequencer, if any. */
	ULevelSequence* GetOpenSequence();

	/** The Sequencer editing that sequence, if any. */
	TSharedPtr<ISequencer> GetOpenSequencer();

	/** The playhead in the movie scene's tick resolution (playback start if Sequencer isn't open). */
	FFrameNumber GetPlayheadTick(const UMovieScene* MovieScene);

	/** Finds or creates a possessable binding for an actor in the open sequence. */
	FGuid BindActor(ULevelSequence* Sequence, AActor* Actor);

	/** Tells Sequencer the sequence's structure changed so its UI refreshes. */
	void NotifyChanged();
}
