#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyDirectorLibrary.generated.h"

/**
 * The director's brain. Turns dialogue lines into an edit: which shot, on whom, for how long, with
 * which lens and camera feel. Follows editing craft: shot-reverse-shot with matched sizes,
 * tightening as drama builds, no jump cuts, reaction shots in the pauses, an establishing shot up top.
 * Deterministic for a given seed, so "re-roll" is just a new seed.
 */
UCLASS()
class CINEMAFY_API UCinemafyDirectorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Director")
	static FCinemafyDirectorStyle GetPresetStyle(ECinemafyDirectorPreset Preset);

	/** Plans a whole scene. Shot times are seconds from the start of the scene. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Director")
	static TArray<FCinemafyPlannedShot> PlanScene(const TArray<FCinemafyLine>& Lines, const FCinemafyDirectorStyle& Style, int32 Seed = 0);

	/**
	 * Plans the shots for a single line, continuing from the previous shot (if any).
	 * Used for live directing, where lines arrive one at a time.
	 */
	static TArray<FCinemafyPlannedShot> PlanLine(const FCinemafyLine& Line, int32 LineIndex, const FCinemafyDirectorStyle& Style,
		FRandomStream& Random, const FCinemafyPlannedShot* Previous, bool bOpensScene, float StartTime);

	/** How tight a shot type is: 0 = wide two-shot ... 5 = extreme close-up. */
	static float GetShotSize(ECinemafyShotType ShotType);
};
