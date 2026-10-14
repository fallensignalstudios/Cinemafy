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
		FRandomStream& Random, const FCinemafyPlannedShot* Previous, bool bOpensScene, float StartTime,
		const TArray<AActor*>& SceneCast = TArray<AActor*>());

	/**
	 * Slides cuts onto the nearest beat, keeping every shot at least
	 * MinShotLength long. Cuts that start a spoken line (lines with audio) stay put so dialogue stays in sync.
	 * Beats are in seconds from the start of the scene.
	 */
	static void SnapPlanToBeats(TArray<FCinemafyPlannedShot>& Plan, const TArray<FCinemafyLine>& Lines, const TArray<float>& Beats, float MinShotLength = 0.5f);

	/** Puts locked shots back into a fresh plan, trimming or splitting whatever overlaps them. */
	static void ApplyLockedShots(TArray<FCinemafyPlannedShot>& Plan, const TArray<FCinemafyPlannedShot>& Locked);

	/**
	 * Swaps one planned shot for the director's next-best option (different framing, still legal next
	 * to its neighbours) and locks it. Returns false if there's no alternative.
	 */
	static bool SwapForAlternate(TArray<FCinemafyPlannedShot>& Plan, int32 Index, const TArray<FCinemafyLine>& Lines, const FCinemafyDirectorStyle& Style);

	/** Moves the cut at the start of shot Index by DeltaSeconds (keeping both shots >= MinShotLength) and locks both shots. */
	static bool NudgeCut(TArray<FCinemafyPlannedShot>& Plan, int32 Index, float DeltaSeconds, float MinShotLength = 0.3f);

	/**
	 * Reads the scene before cutting it: decides each line's Moment (reveal, silent beat, new exchange,
	 * action, walk & talk, dialogue), and flags action lines whose audio is all hits and no talk.
	 */
	static void ReadMoments(TArray<FCinemafyLine>& Lines);

	/** A plain-language account of the tools a plan uses ("2 wipes, 3 push-ins on emphasis..."). */
	static FString SummarizeDecisions(const TArray<FCinemafyPlannedShot>& Plan, const TArray<FCinemafyLine>& Lines);

	/** Everyone who speaks or is spoken to in a script. */
	static TArray<AActor*> GetCast(const TArray<FCinemafyLine>& Lines);

	/** How tight a shot type is: 0 = wide two-shot ... 5 = extreme close-up. */
	static float GetShotSize(ECinemafyShotType ShotType);
};
