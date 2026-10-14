#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyPerformerProfile.h"
#include "CinemafyEditorLibrary.generated.h"

class ACineCameraActor;
class ACinemafyLightRig;
class ACinemafySceneDirector;
class USoundWave;
class UCinemafyDirectorStyleAsset;
class ACinemafyChoreography;
class UCinemafyMoveLibrary;
class UCinemafyPerformerProfile;

/**
 * Editor-time Cinemafy actions. Everything here is undoable and callable from
 * Editor Utility Widgets / Python, and powers the Tools > Cinemafy menu.
 */
UCLASS()
class CINEMAFYEDITOR_API UCinemafyEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spawns a framed, lensed, eye-focused Cine Camera for a coverage shot. If a Level Sequence is
	 * open in Sequencer, the camera is added to it with a camera cut at the playhead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static ACineCameraActor* CreateShot(const FCinemafyShotRequest& Request, bool bAddToOpenSequence = true);

	/**
	 * Creates a shot from the current selection: first selected actor = subject,
	 * second = counterpart (who they're talking to).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static ACineCameraActor* CreateShotFromSelection(ECinemafyShotType ShotType, bool bFlipSide = false);

	/** Adds/updates a Camera Feel component on each selected camera. Returns how many were changed. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ApplyCameraFeelToSelection(ECinemafyCameraFeel Feel);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ApplyLookToSelection(ECinemafyLook Look, float Strength = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 SetLetterboxOnSelection(float AspectRatio = 2.39f);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ApplyPrimeToSelection(ECinemafyPrime Prime, float FStop = 2.8f);

	/** Adds/updates a Camera Move on each selected camera, with a sensible size for that move. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ApplyCameraMoveToSelection(ECinemafyCameraMove Move);

	/**
	 * Adds a camera to the Level Sequence open in Sequencer (reusing its binding if it's already
	 * there) and optionally cuts to it at the playhead. Returns false if no sequence is open.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static bool AddCameraToOpenSequence(ACineCameraActor* CameraActor, bool bAddCameraCut = true);

	/** Adds every selected camera to the open sequence, cutting to each at the playhead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AddSelectedCamerasToSequence();

	/**
	 * Select two characters and any number of cameras. Reports (and selects) cameras on the
	 * minority side of the line of action: cutting to those will flip screen direction.
	 * Returns the number of offending cameras, or -1 if the selection wasn't usable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 CheckLineOfActionForSelection();

	/**
	 * Light a shot. Select a character (and optionally the camera the shot is for), then pick a style.
	 * If Light Rigs are selected instead, they're switched to the new style.
	 * Returns the number of rigs created or restyled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ApplyLightingStyleToSelection(ECinemafyLightingStyle Style);

	/** Spawns a light rig on a character, designed for a camera (optional). Added to the open sequence so its Intensity can be keyed. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static ACinemafyLightRig* CreateLightRig(AActor* Subject, AActor* TargetCamera, ECinemafyLightingStyle Style);

	/**
	 * Makes the open Level Sequence hand off smoothly with gameplay: turns on camera cut blending,
	 * eases the first cut in from the gameplay camera and the last cut back out to it, and (optionally)
	 * eases each character's first/last animation section so poses blend with their Anim Blueprint.
	 * Returns false if no sequence is open or it has no camera cuts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static bool SetUpGameplayBlendForOpenSequence(float BlendInSeconds = 1.f, float BlendOutSeconds = 1.f, bool bIncludeAnimations = true);

	/**
	 * Adds a Listener (and Gaze, if missing) to each selected character. Select exactly two and
	 * they're set up as a conversation: each listens to the other. Returns how many were set up.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AddListeningToSelection(ECinemafyTemperament Temperament);

	/** Adds a Cinemafy Face component (if missing) to each selected character and sets its emotion. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ApplyFaceEmotionToSelection(ECinemafyEmotion Emotion);

	/**
	 * Plans the director's scene and bakes it into the open Level Sequence from the playhead:
	 * cameras (reused across takes, so your tweaks survive), lenses, camera feel and timed cuts.
	 * Replaces this director's previous cuts. Returns the number of shots.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 DirectSceneInSequencer(ACinemafySceneDirector* Director);

	/**
	 * Bakes the director's current LastPlan (as planned, or as edited in the Shot Browser) into the open
	 * sequence, without re-planning. bAtLastSceneStart keeps the scene where it was last placed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 BakeDirectorPlan(ACinemafySceneDirector* Director, bool bAtLastSceneStart = true);

	/** Lock or unlock a shot of the director's current cut (locked shots survive re-rolls). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static bool ToggleShotLock(ACinemafySceneDirector* Director, int32 ShotIndex);

	/** Swap a shot for the director's next-best alternative, lock it and re-bake. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static bool SwapShotForAlternate(ACinemafySceneDirector* Director, int32 ShotIndex);

	/** Move the cut at the start of a shot by DeltaSeconds, lock both sides and re-bake. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static bool NudgeShotCut(ACinemafySceneDirector* Director, int32 ShotIndex, float DeltaSeconds);

	/** Save the director's current cut as a take. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 SaveDirectorTake(ACinemafySceneDirector* Director, const FString& Name);

	/** Restore a saved take and re-bake it where the scene was. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static bool LoadDirectorTake(ACinemafySceneDirector* Director, int32 TakeIndex);

	/** Removes this director's camera cuts from the open sequence (cameras are kept). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ClearDirectedShots(ACinemafySceneDirector* Director);

	/**
	 * Watches every cut in the open Level Sequence, classifies each shot from its actual framing,
	 * and writes the result into the director's Custom style. Returns the number of shots analyzed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 LearnDirectorStyleFromSequence(ACinemafySceneDirector* Director);

	/** Finds a piece of music's tempo and beat grid. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static FCinemafyMusicAnalysis AnalyzeMusic(USoundWave* Sound);

	/** Measures a dialogue recording: speech loudness, peaks, speech rate and how much of it is talking. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static FCinemafyAudioAnalysis AnalyzeDialogueSound(USoundWave* Sound);

	/**
	 * Analyzes every line's audio in a director's script, sets durations from the audio and derives
	 * each line's AudioIntensity relative to the rest of the scene. Returns the number of lines analyzed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AnalyzeDirectorAudio(ACinemafySceneDirector* Director);

	/**
	 * Reviews every cut in the open Level Sequence: crossing the 180-degree line, jump cuts, flash cuts,
	 * overlong shots, cutting a camera to itself. Shows a report. Returns the number of notes (-1 if no sequence).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 CheckSequenceContinuity();

	/** Saves the style a director is using as a reusable Director Style asset (/Game/Cinemafy/Styles) and assigns it. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static UCinemafyDirectorStyleAsset* SaveDirectorStyleAsAsset(ACinemafySceneDirector* Director);

	/**
	 * Replaces a director's script with a CSV. Columns (any order, case-insensitive): Speaker, Listener,
	 * Text, Intensity, Emotion, SceneBreak, Action, Duration. Speakers are matched to actors in the level
	 * by label. Missing durations are estimated from the text. Returns the number of lines imported (-1 on error).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 ImportDirectorScript(ACinemafySceneDirector* Director, const FString& CsvPath);

	/**
	 * Fills each line's Audio, Face Animation and Body Animation from a content folder, matching assets
	 * by line number in their name (Line03, Line_03, L03). Animations go to face or body by skeleton.
	 * Returns the number of slots filled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AssignDirectorPerformances(ACinemafySceneDirector* Director, const FString& ContentFolder);

	/** Spawns a Scene Director for the selected characters with a starter script (alternating lines, rising intensity). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static ACinemafySceneDirector* CreateSceneDirectorFromSelection(ECinemafyDirectorPreset Style);

	/** Adds a Gaze component to each selected character. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AddGazeToSelection();

	/**
	 * Choreograph mode for the selected character (or choreography): click points in the viewport,
	 * type what happens there. Makes a Cinemafy Choreography for the character if it has none.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static ACinemafyChoreography* ChoreographSelection();

	/** Plays in editor, performs every choreography in the level, records and bakes the take into Sequencer. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static void PerformAndRecordChoreography(ACinemafyChoreography* Choreography);

	/** Cuts shots around a choreography's recorded take with a Scene Director. Returns the shot count. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static int32 DirectChoreography(ACinemafyChoreography* Choreography);

	/**
	 * Makes a Move Library asset from every one-shot animation (attacks, gestures, reactions...) in a
	 * content folder, with keywords from their names, for you to curate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static UCinemafyMoveLibrary* CreateMoveLibraryFromFolder(const FString& ContentFolder);

	/** Makes a Performer Profile asset from a preset (GASP, GASPALS, SignalForge, MetaHuman, generic) in /Game/Cinemafy/Performers. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static UCinemafyPerformerProfile* CreatePerformerProfile(ECinemafyPerformerPreset Preset, const FString& Name);

	/** Saves what a choreography's auto-wiring found on its performer as a Performer Profile, and assigns it. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static UCinemafyPerformerProfile* SaveWiringAsProfile(ACinemafyChoreography* Choreography);

	/** The wiring report for a choreography's performer (and its profile's check), as text. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static FString GetWiringReport(ACinemafyChoreography* Choreography);

	static TArray<AActor*> GetSelectedActorsInOrder();
};
