#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyEditorLibrary.generated.h"

class ACineCameraActor;
class ACinemafyLightRig;

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

	/** Adds a Gaze component to each selected character. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AddGazeToSelection();

	static TArray<AActor*> GetSelectedActorsInOrder();
};
