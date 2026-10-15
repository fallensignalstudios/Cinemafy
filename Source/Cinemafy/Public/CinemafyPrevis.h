#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyScript.h"
#include "CinemafyPrevis.generated.h"

class USoundBase;
class USkeleton;
class UAnimSequenceBase;

/** One piece of blocking: who, where, and how ("Tarrik and Lyessa lie on the bunk, cuddled"). */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	TArray<FString> Characters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	ECinemafyPose Pose = ECinemafyPose::Stand;

	/** The words named a pose (else the place's mark decides). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	bool bPoseGiven = false;

	/** Where ("bunk", "window"): a mark or a piece of the set with that name. Empty = their start mark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	FString Place;

	/** Close together (cuddled, side by side, in each other's arms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	bool bTogether = false;

	/** From the blocking notes (whole scene) rather than the action lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	bool bFromNotes = false;

	/** The words it came from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking")
	FString Source;
};

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

	/** From this beat on, Character holds this pose (lies down on the bunk mid-scene). Stand = no change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	ECinemafyPose Pose = ECinemafyPose::Stand;

	/** Set when Character hears this line rather than says it: the voice with no body it comes from ("SHIP ANNOUNCEMENT"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FString HeardFrom;

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

	/**
	 * Reads staging in plain words: "Tarrik and Lyessa lie on the bunk, cuddled." / "Kade sits at the console; Thane
	 * leans against the window." CastNames = the names to look for. One block per sentence that says who, and how or where.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Previs")
	static TArray<FCinemafyBlock> ReadBlocking(const FString& Text, const TArray<FString>& CastNames);

	/**
	 * A scene's blocking: its Blocking notes (whole scene, they win), plus poses its opening action gives the others
	 * ("Kade sits at the console." before anyone speaks). Those people hold their pose: they don't walk.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Previs")
	static TArray<FCinemafyBlock> BlockScene(const FCinemafyScriptScene& Scene);

	/**
	 * The clip for holding a pose on this skeleton: Project Settings' Together Animations (for two people together;
	 * Index 0 = the lead, 1 = the partner), then Pose Animations, then one found by name. Null = none.
	 */
	static UAnimSequenceBase* FindPoseAnimation(ECinemafyPose Pose, const USkeleton* Skeleton, bool bTogether, int32 Index);
};
