#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Scene.h"
#include "CinemafyTypes.h"
#include "CinemafyCameraLibrary.generated.h"

class UCineCameraComponent;
class UCinemafyLookPreset;

/** One-click lens, sensor, letterbox and grading helpers for Cine Cameras. Usable at runtime and in editor. */
UCLASS()
class CINEMAFY_API UCinemafyCameraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Finds the Cine Camera component on an actor (e.g. a CineCameraActor). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Camera")
	static UCineCameraComponent* FindCineCamera(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Lens")
	static float GetPrimeFocalLength(ECinemafyPrime Prime);

	/** Returns the prime closest to the given focal length (compared in log space, the way lenses "feel"). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Lens")
	static ECinemafyPrime SnapToPrime(float FocalLengthMM);

	/** Sensor size in mm (X = width, Y = height). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Lens")
	static FVector2D GetSensorSize(ECinemafySensor Sensor);

	/** Turns the camera into a fixed prime with a sensible aperture range and round bokeh. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Lens")
	static void ApplyPrime(UCineCameraComponent* Camera, ECinemafyPrime Prime, float FStop = 2.8f);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Lens")
	static void ApplySensor(UCineCameraComponent* Camera, ECinemafySensor Sensor);

	/**
	 * Crops the sensor to a cinema aspect ratio and constrains the view so the game shows real letterbox bars.
	 * 2.39 = scope, 1.85 = flat. Pass 0 to go back to 16:9 with no bars.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Look")
	static void SetLetterbox(UCineCameraComponent* Camera, float AspectRatio = 2.39f);

	/** Builds post process overrides for a built-in look. Strength 0 = neutral, 1 = full look. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Look")
	static FPostProcessSettings MakeLookSettings(ECinemafyLook Look, float Strength = 1.f);

	/** Applies a built-in look on top of the camera's existing post process settings. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Look")
	static void ApplyLook(UCineCameraComponent* Camera, ECinemafyLook Look, float Strength = 1.f);

	/** Applies a saved look preset asset (and its letterbox, if it has one). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Look")
	static void ApplyLookPreset(UCineCameraComponent* Camera, const UCinemafyLookPreset* Preset);

	/** Writes a built-in look's overrides into an existing settings struct (leaves other overrides alone). */
	static void ApplyLookToSettings(FPostProcessSettings& Settings, ECinemafyLook Look, float Strength);
};
