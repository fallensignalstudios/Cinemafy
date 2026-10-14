#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyChoreographyTypes.h"
#include "CinemafyTypes.h"
#include "CinemafyPerformerProfile.h"
#include "CinemafyChoreographyLibrary.generated.h"

class USkeleton;

/** An animation the director could pick, described by the words in its name and folders. */
struct CINEMAFY_API FCinemafyMoveCandidate
{
	FSoftObjectPath Path;
	FString Name;
	TArray<FString> Tokens;
	float Bias = 0.f;
};

/**
 * The choreographer's brain: reads one- or two-word prompts, picks animations by name,
 * and turns a recorded performance into lines the Scene Director can shoot.
 */
UCLASS()
class CINEMAFY_API UCinemafyChoreographyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** "sprint" -> Move/Sprint, "sword attack" -> Attack with a sword, "taunt" -> Perform "taunt", "wait 2" -> Wait 2s. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static FCinemafyPromptReading ReadPrompt(const FString& Prompt);

	/** "AM_Katana_Attack_Combo01" -> katana, attack, combo, 01. Splits on separators, CamelCase and digits; lower case. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static TArray<FString> TokenizeName(const FString& NameOrPath);

	/** How well an animation (described by its tokens) fits the prompt. Below zero = don't use it. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static float ScoreAnimation(const FCinemafyPromptReading& Reading, const TArray<FString>& AnimationTokens);

	/** The words a prompt word also answers to ("slash" -> attack, strike, swing...). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static TArray<FString> GetSynonyms(const FString& Word);

	/** What a beat's Arrival means in context: Auto passes through before more travel, enters vaults moving, and stops before anything else. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static ECinemafyArrival ResolveArrival(const TArray<FCinemafyChoreoBeat>& Beats, int32 BeatIndex);

	/**
	 * What a defender should look for to answer a strike: a hit react from the side it came from, a knockdown,
	 * a death, a block or a dodge. RelativeYaw is where the attacker is, seen from the defender (0 = in front, + = right).
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static FCinemafyPromptReading MakeReactionReading(ECinemafyHitReaction Reaction, float RelativeYaw);

	/** "front", "back", "left" or "right" for where a blow comes from. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	static FString ReactionDirection(float RelativeYaw);

	/**
	 * Measures the obstacle at Toward, approached from From (the performer's feet), against what the performer can
	 * do: height, depth, landing room, headroom and approach angle, with a verdict and a plain-words reason.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography", meta = (WorldContext = "WorldContextObject"))
	static FCinemafyTraversalCheck CheckTraversal(const UObject* WorldContextObject, FVector From, FVector Toward,
		const FCinemafyTraversalEnvelope& Envelope, float CapsuleRadius, float CapsuleHalfHeight, const TArray<AActor*>& IgnoreActors);

	static FCinemafyTraversalCheck CheckTraversalImpl(const UObject* WorldContextObject, FVector From, FVector Toward,
		const FCinemafyTraversalEnvelope& Envelope, float CapsuleRadius, float CapsuleHalfHeight, const TArray<AActor*>& IgnoreActors, bool bAllowLineUp);

	/** Turns measured numbers into a verdict (the pure part of CheckTraversal, testable without a level). */
	static void JudgeTraversal(FCinemafyTraversalCheck& Check, const FCinemafyTraversalEnvelope& Envelope, bool bLandingClear, float CapsuleRadius);

	/** Same for a gap or a ledge: GapWidth (0 = no far side, a plain drop) and DropHeight against the envelope. */
	static void JudgeGap(FCinemafyTraversalCheck& Check, const FCinemafyTraversalEnvelope& Envelope);

	/**
	 * Where the route goes once the verdict is in: an approach point squared up to the face with room for a
	 * run-up (never behind the start), the takeoff, and the exit (landing, top, or far side).
	 */
	static void PlanTraversalRoute(FCinemafyTraversalCheck& Check, const FVector& From, const FCinemafyTraversalEnvelope& Envelope);

	/**
	 * Plans the in-between moves for a whole choreography by walking it in order with the performer's state
	 * (weapon in hand, gait): draws before strikes that need a weapon not in hand, a brake out of a sprint
	 * before jumps, climbs and melee, and a roll or recovery after big landings.
	 * StartingWeapon: what's in hand when the take starts (empty = nothing drawn).
	 */
	static void PlanTransitions(TArray<FCinemafyChoreoBeat>& Beats, const FString& StartingWeapon, const FCinemafyTraversalEnvelope& Envelope);

	/** True for weapons that shoot (pistol, pistols, rifle, bow, wand). */
	static bool IsRangedWeapon(const FString& Weapon);

	/** Builds the lines the Scene Director cuts from: one per recorded beat, timed to the take. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	static TArray<FCinemafyLine> BuildDirectorLines(const TArray<FCinemafyChoreoBeat>& Beats, AActor* Performer, float TakeDuration);

	/**
	 * Picks up to Count animations for a reading. Combos come out as a numbered chain
	 * (Attack_01, _02, _03) when the library has one. Recently used picks are avoided.
	 */
	static TArray<int32> PickAnimations(const FCinemafyPromptReading& Reading, const TArray<FCinemafyMoveCandidate>& Candidates,
		int32 Count, const TSet<FSoftObjectPath>& RecentlyUsed);

	/** Every montage / sequence in the library (or, without one, in the given folders) the skeleton can play. */
	static TArray<FCinemafyMoveCandidate> GatherCandidates(const UCinemafyMoveLibrary* Library, const TArray<FString>& Folders, const USkeleton* Skeleton);

	/** Scores an enum value name ("PistolTwoHanded") for a weapon word ("pistols"). */
	static float ScoreOverlayName(const FString& Weapon, const FString& EnumValueName);
};
