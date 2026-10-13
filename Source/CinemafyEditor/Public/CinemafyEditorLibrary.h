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
	/** Spawns a framed, lensed, eye-focused Cine Camera for a coverage shot. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static ACineCameraActor* CreateShot(const FCinemafyShotRequest& Request);

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

	/** Adds a Gaze component to each selected character. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Editor")
	static int32 AddGazeToSelection();

	static TArray<AActor*> GetSelectedActorsInOrder();
};
