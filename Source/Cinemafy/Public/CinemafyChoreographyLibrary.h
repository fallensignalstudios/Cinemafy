#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyChoreographyTypes.h"
#include "CinemafyTypes.h"
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
