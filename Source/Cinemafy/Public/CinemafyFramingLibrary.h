#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyFramingLibrary.generated.h"

/**
 * Shot math. Turns "close-up on Kara, talking to Voss" into a camera transform and lens,
 * following the rules a DP would: eyes on the upper third, lead room in the look direction,
 * and every shot on the same side of the line of action.
 */
UCLASS()
class CINEMAFY_API UCinemafyFramingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Solves camera placement for a coverage shot. Returns false if the request is missing a needed actor. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Framing")
	static bool SolveShot(const FCinemafyShotRequest& Request, FCinemafyShotSolution& OutSolution);

	/** The lens most DPs would reach for on this shot type. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Framing")
	static ECinemafyPrime GetRecommendedPrime(ECinemafyShotType ShotType);

	/** True if the shot type needs a counterpart actor. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Framing")
	static bool ShotNeedsCounterpart(ECinemafyShotType ShotType);

	/**
	 * Best guess at where a character's eyes are: MetaHuman eye bones, then the head bone,
	 * then the top of the actor's bounds.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Framing")
	static FVector GetEyePoint(const AActor* Actor);

	/**
	 * Which side of the line of action between two characters a point is on: > 0 is the side
	 * SolveShot uses by default, < 0 is the flipped side. The line is ordered by actor name,
	 * so the answer doesn't depend on which character you pass first. Returns signed distance in cm.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Framing")
	static float GetSideOfLine(const FVector& Point, const AActor* CharacterA, const AActor* CharacterB);

	/** Horizontal line-of-action direction for a pair of characters, ordered by actor name. */
	static FVector GetCanonicalLineDirection(const AActor* CharacterA, const AActor* CharacterB);
};
