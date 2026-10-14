#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameNumber.h"

class ISequencer;
class ULevelSequence;
class UMovieScene;
class UMovieSceneTrack;
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

	/** Finds or creates a binding for any object Sequencer can possess (e.g. a component). Needs Sequencer open on the sequence. */
	FGuid BindObject(ULevelSequence* Sequence, UObject* Object);

	/** The actor's existing binding in the open sequence, or an invalid guid (never creates one). */
	FGuid FindBinding(ULevelSequence* Sequence, AActor* Actor);

	/** Tells Sequencer the sequence's structure changed so its UI refreshes. */
	void NotifyChanged();

	/**
	 * Mutes (never deletes) the binding's tracks of the given class, except Keep. Used for the tracks Sequencer adds on its
	 * own when it binds an object (a transform track, an FK Control Rig track) that would override what Cinemafy drives.
	 * Returns how many tracks were muted.
	 */
	int32 MuteBindingTracks(UMovieScene* MovieScene, const FGuid& Binding, FName TrackClass, const UMovieSceneTrack* Keep = nullptr);

	/** Mutes (never deletes) property tracks animating PropertyName on the binding's child bindings (its components). */
	int32 MuteChildPropertyTracks(UMovieScene* MovieScene, const FGuid& Binding, FName PropertyName);

	/**
	 * For a camera just bound into a sequence: Sequencer auto-adds tracks keyed at its current values, which then override
	 * what Cinemafy drives every frame. Mutes the transform track if the camera rides a follow rig, and the manual focus
	 * distance track if Eye Focus is pulling focus. Returns how many tracks were muted.
	 */
	int32 LetCinemafyDriveCamera(UMovieScene* MovieScene, const FGuid& Binding, const AActor* Camera);
}
