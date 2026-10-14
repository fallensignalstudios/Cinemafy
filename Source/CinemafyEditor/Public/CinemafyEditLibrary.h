#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyEditDecisions.h"
#include "CinemafyEditLibrary.generated.h"

/**
 * Cinemafy Edit: video-editor tools for the Level Sequence open in Sequencer. Everything works at
 * the playhead (or the cut nearest it), is undoable, and is callable from Python / Blueprints.
 *
 * Ripple edits move everything after the edit on every track (sync-locked, like a full-track
 * selection in Premiere), including keys inside long sections such as the director's cue tracks.
 */
UCLASS()
class CINEMAFYEDITOR_API UCinemafyEditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --- Timeline -------------------------------------------------------------------------------

	/** Splits the shot under the playhead in two (then swap a camera in, or trim). All Tracks splits every section there. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool RazorAtPlayhead(bool bAllTracks = false);

	/** Removes the shot under the playhead and closes the gap on every track. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool RippleDeleteShotAtPlayhead();

	/** Premiere's Q: cuts the shot's head off up to the playhead and pulls everything after it left. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool RippleTrimStartToPlayhead();

	/** Premiere's W: cuts the shot's tail off from the playhead and pulls everything after it left. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool RippleTrimEndToPlayhead();

	/**
	 * Roll edit: moves the cut nearest the playhead by Frames (display frames, + later / - earlier),
	 * picture only. Later = a J-cut (the next line's sound starts under the old picture); earlier = an L-cut.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool RollNearestCut(int32 Frames);

	/** Rolls the cut nearest the playhead to the playhead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool RollNearestCutToPlayhead();

	/** Swaps the shot under the playhead with the next one (cameras and lengths trade places). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool SwapShotWithNext();

	/** Jumps the playhead to the next (or previous) cut. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool GoToCut(bool bNext = true);

	// --- Transitions & titles -------------------------------------------------------------------

	/**
	 * Puts a transition on the cut nearest the playhead. Fades through black are native Fade keys
	 * (they show everywhere); wipes, dissolves, dips to white and whip pans play over the game view.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool AddTransitionAtNearestCut(ECinemafyTransition Type, float Duration = 0.6f, ECinemafyWipeDirection Direction = ECinemafyWipeDirection::LeftToRight);

	/** Takes transitions off the cut nearest the playhead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 RemoveTransitionsAtNearestCut();

	/** Adds a title card / lower third / crawl starting at the playhead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool AddTitleAtPlayhead(const FCinemafyTitleCard& Card);

	// --- Markers & music ------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool AddMarkerAtPlayhead(const FString& Label);

	/** Finds the music in the sequence, hears its beat and drops a marker on every beat (labels every bar). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 AddMusicBeatMarkers();

	/** Rolls every cut within ToleranceFrames of a marker onto it. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 SnapCutsToMarkers(int32 ToleranceFrames = 6);

	// --- Audio ----------------------------------------------------------------------------------

	/** Dips the music under every piece of dialogue (keys the music section's volume, with ramps). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 DuckMusicUnderDialogue(float DuckDecibels = -10.f, float RampSeconds = 0.3f);

	// --- Grade ----------------------------------------------------------------------------------

	/** Copies the grade (post process, minus depth of field) of the selected camera, or the shot under the playhead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static bool CopyGrade();

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 PasteGradeToSelection();

	/** Pastes the copied grade onto every camera cut in the open sequence. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 ApplyGradeToAllShots();

	// --- Multicam -------------------------------------------------------------------------------

	/**
	 * Replaces the camera cuts between the first time and EndSeconds with a live-switched edit:
	 * Cameras[i] is on screen from Times[i] (seconds, sequence time) until the next switch.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static int32 ApplyMulticamCuts(const TArray<AActor*>& Cameras, const TArray<float>& Times, float EndSeconds);

	/** The cameras the open sequence cuts to, in order of first use. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static TArray<AActor*> GetSequenceCameras();

	/** The camera on screen at the playhead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Edit")
	static AActor* GetCameraAtPlayhead();
};
