#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyScript.h"
#include "CinemafyPrevis.generated.h"

class USoundBase;

/** One beat staged from the script: who does what, where, to whom, and what they wait for. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyStagedBeat
{
	GENERATED_BODY()

	/** The script name of who performs it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString Character;

	/** A choreography prompt: "walk", "run", "vault", "sword attack", "look", "say", "nod"... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString Prompt;

	/** Where: a phrase naming a mark or a prop ("the console"). Empty = toward the target, or where they are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString Place;

	/** Who it's aimed at, looked at, or said to (script name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString Target;

	/** Words said during the beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString Line;

	/** How it's said, as the script writes it ("(coldly)"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString LineDirection;

	/** The line's audio (scratch or recorded), if the script has it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	TSoftObjectPtr<USoundBase> LineAudio;

	/** Hold the beat until this character has finished their Nth line (WaitForLineNumber). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString WaitFor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	int32 WaitForLineNumber = 0;

	/** The script's words it came from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString Source;
};

/** Turning script scenes into staging. */
UCLASS()
class CINEMAFY_API UCinemafyPrevisLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Reads a scene the way a first AD would: who moves where ("Kade walks to the console" -> Kade: walk to
	 * "console"), who draws, strikes, vaults, looks, gestures, and who says what to whom, in script order.
	 * Lines answer the previous speaker (waiting for that line), and an action right after someone else's
	 * line waits for it. Sentences it can't stage are listed in OutNotStaged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Previs")
	static TArray<FCinemafyStagedBeat> StageScene(const FCinemafyScriptScene& Scene, TArray<FString>& OutNotStaged);

	/**
	 * What the scene's action mentions, for the art department: set pieces where things happen ("vaults a cargo crate",
	 * "from the weapons rack", "looks at the empty command chair"), hand props ("picks up a saber", "draws her pistol",
	 * "grabs the DATAPAD") and wardrobe ("wears his dress uniform"). Walls, floors, body parts and the location itself
	 * are left out.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Previs")
	static TArray<FCinemafySetPiece> FindSetDressing(const FCinemafyScriptScene& Scene);
};
