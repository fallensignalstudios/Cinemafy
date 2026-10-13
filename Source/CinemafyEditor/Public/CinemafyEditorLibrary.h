#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyEditorLibrary.generated.h"

class ACineCameraActor;

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

	/** Adds a Gaze component to each selected character. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AddGazeToSelection();

	static TArray<AActor*> GetSelectedActorsInOrder();
};
