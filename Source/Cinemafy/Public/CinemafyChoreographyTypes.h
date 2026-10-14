#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinemafyChoreographyTypes.generated.h"

class AActor;
class UAnimSequenceBase;

/** What a choreography beat asks the performer to do. */
UENUM(BlueprintType)
enum class ECinemafyActionKind : uint8
{
	/** Stand there for a moment (motion matching keeps the idle alive). */
	Wait      UMETA(DisplayName = "Wait"),
	/** Walk / run / sprint / sneak to the point. Motion matching does the starts, pivots and stops. */
	Move      UMETA(DisplayName = "Move"),
	/** Vault, hurdle, mantle or climb the obstacle at the point (the character's own traversal system). */
	Traverse  UMETA(DisplayName = "Traverse"),
	/** Close in on the target and play an attack (or a whole combo). */
	Attack    UMETA(DisplayName = "Attack"),
	/** Play a one-off animation: taunt, wave, salute, hit react, dodge... found by name. */
	Perform   UMETA(DisplayName = "Perform"),
	/** Turn to face the point / target. */
	Look      UMETA(DisplayName = "Look"),
	/** Draw or switch weapons (the overlay), e.g. "sword", "pistols", "holster". */
	Equip     UMETA(DisplayName = "Equip")
};

/** How fast the performer moves. */
UENUM(BlueprintType)
enum class ECinemafyGait : uint8
{
	Walk    UMETA(DisplayName = "Walk"),
	Run     UMETA(DisplayName = "Run"),
	Sprint  UMETA(DisplayName = "Sprint"),
	Crouch  UMETA(DisplayName = "Crouch")
};

/** How the director brain read a one- or two-word prompt. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyPromptReading
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	ECinemafyActionKind Kind = ECinemafyActionKind::Wait;

	/** Only meaningful when bHasGait. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	ECinemafyGait Gait = ECinemafyGait::Walk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	bool bHasGait = false;

	/** Weapon / overlay asked for ("sword", "pistols", "rifle", "wand", "unarmed"), empty if none. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	FString Weapon;

	/** Ranged attack (shoot, fire, cast): don't run up to the target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	bool bRanged = false;

	/** Words used to find the animation, already expanded with synonyms. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	TArray<FString> Keywords;

	/** "wait 2" -> 2 seconds. 0 = default. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	float Seconds = 0.f;

	/** "combo" / "attack x3" -> how many animations to chain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	int32 Repeat = 1;

	/** Human readable, e.g. "Attack with sword (x3)". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	FString Summary;
};

/** One click + prompt in a choreography. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyChoreoBeat
{
	GENERATED_BODY()

	/** One or two words: "walk", "vault", "sprint", "climb", "sword attack", "taunt", "wait 2". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat")
	FString Prompt;

	/** Where it happens (drag the widget to adjust). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat", meta = (MakeEditWidget = true))
	FVector Location = FVector::ZeroVector;

	/** What was clicked: the enemy to attack, the box to vault, the wall to climb (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat")
	TObjectPtr<AActor> Target = nullptr;

	/** Use this animation instead of the one the director picked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat")
	TObjectPtr<UAnimSequenceBase> AnimationOverride = nullptr;

	/** Play-rate for the beat's animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float PlayRate = 1.f;

	/** How the prompt was read (refreshed on every edit). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Director")
	FCinemafyPromptReading Reading;

	/** The animations the director chose (in order, for combos). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Director")
	TArray<TObjectPtr<UAnimSequenceBase>> ChosenAnimations;

	/** Seconds into the last recorded take when the beat started / ended. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	float StartTime = -1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	float EndTime = -1.f;

	/** Seconds into the take when hits landed / feet touched down. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	TArray<float> ImpactTimes;

	/** The beat ran out of time or found nothing to play (see the Output Log). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	bool bFailed = false;
};

/** One animation the director can reach for, with the words that describe it. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyMoveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	TObjectPtr<UAnimSequenceBase> Animation = nullptr;

	/** Words this animation answers to (lower case). Built from its name and folders; add your own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	TArray<FString> Keywords;

	/** Pick this one more (or less) often than its name alone would suggest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "-5.0", ClampMax = "5.0"))
	float Bias = 0.f;
};

/**
 * The director's animation vocabulary: every montage / sequence it may use for
 * attacks, taunts, reactions and so on. Optional: without one, Cinemafy reads your
 * whole project by asset name. With one, you control exactly what's in play.
 * Tools > Cinemafy > Build Move Library fills one from a folder.
 */
UCLASS(BlueprintType)
class CINEMAFY_API UCinemafyMoveLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moves")
	TArray<FCinemafyMoveEntry> Moves;
};
