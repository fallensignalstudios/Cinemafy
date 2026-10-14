#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinemafyPerformerProfile.generated.h"

class ACharacter;
class UCinemafyMoveLibrary;

/** Starting points for a performer profile. */
UENUM(BlueprintType)
enum class ECinemafyPerformerPreset : uint8
{
	/** Epic's Game Animation Sample character (CBP_SandboxCharacter): input-state gait flags, TryTraversalAction. */
	GASP              UMETA(DisplayName = "Game Animation Sample"),
	/** GASP with the ALS overlay layer (GASPALS): GASP plus an overlay pose state. */
	GASPALS           UMETA(DisplayName = "GASPALS (GASP + overlays)"),
	/** SignalForge-style C++ base: SetWantsToSprint/Walk, StartCrouch/StopCrouch, GASP traversal. */
	SignalForge       UMETA(DisplayName = "SignalForge character"),
	/** A MetaHuman driven by a GASP character (retargeted body); its face is left to Cinemafy Face. */
	MetaHumanGASP     UMETA(DisplayName = "MetaHuman on GASP"),
	/** Any ACharacter: speed-controlled gait, jump for obstacles, no overlays. */
	GenericCharacter  UMETA(DisplayName = "Generic Character")
};

/** What a performer's traversal can do, in centimetres. Cinemafy measures obstacles against this before recording. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyTraversalEnvelope
{
	GENERATED_BODY()

	/** Below this, it isn't an obstacle: just walk over it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MinHeight = 35.f;

	/** Tallest thing the character can vault or hurdle (pass over and land beyond). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MaxVaultHeight = 125.f;

	/** Deepest thing it can vault over; deeper obstacles are mantled (climbed onto). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MaxVaultDepth = 70.f;

	/** Tallest ledge it can mantle / climb onto. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MaxMantleHeight = 275.f;

	/** Clear floor needed beyond a vault to land on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MinLandingDepth = 60.f;

	/** Clear space needed above the top to pass over it (or to stand on it after a mantle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MinHeadroom = 90.f;

	/** Steepest approach angle to the obstacle's face that still works (degrees from straight on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MaxApproachAngle = 50.f;

	/** False = no traversal system: only things under MinHeight + a jump are passable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool bCanTraverse = true;

	/** Jump height when there's no traversal system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal", meta = (EditCondition = "!bCanTraverse"))
	float MaxJumpHeight = 60.f;
};

/**
 * How to drive one kind of character, written down once: which functions or properties set its gait,
 * crouch, traversal and weapon overlay, what it can climb, and what to leave out of recordings.
 * Choreographies use it instead of guessing by name every take, and say exactly what's missing when a
 * character doesn't match it. Make one from a preset, or with "Save Wiring As Profile" on a choreography.
 */
UCLASS(BlueprintType)
class CINEMAFY_API UCinemafyPerformerProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Fill in anything left empty by looking for likely names (as choreographies do without a profile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	bool bAutoDetectMissing = false;

	// --- Gait --------------------------------------------------------------------------------------------------

	/** A function taking one bool, e.g. SetWantsToSprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FName SprintFunction;

	/** Or a bool property, optionally inside a struct: "CharacterInputState.WantsToSprint". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FString SprintProperty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FName WalkFunction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FString WalkProperty;

	/** Max walk speed per gait (walk, run, sprint) when there's no gait function or property. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FVector FallbackSpeeds = FVector(180.f, 400.f, 650.f);

	/** No-argument crouch functions (e.g. StartCrouch / StopCrouch). Empty = ACharacter::Crouch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FName CrouchFunction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FName UncrouchFunction;

	// --- Traversal ---------------------------------------------------------------------------------------------

	/** The character's own traversal entry point, e.g. TryTraversalAction (inputs filled from Get...Inputs functions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FName TraversalFunction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FCinemafyTraversalEnvelope Traversal;

	// --- Weapons -----------------------------------------------------------------------------------------------

	/** A function taking one enum that sets the weapon overlay (e.g. SetOverlayState). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
	FName OverlayFunction;

	/** Or an enum property that holds it (e.g. OverlayPose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
	FString OverlayProperty;

	/** Weapon word -> enum value (name or display name), e.g. "pistols" -> PistolTwoHanded. Unlisted words are matched by name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
	TMap<FString, FString> OverlayValues;

	// --- Performance -------------------------------------------------------------------------------------------

	/** Animations for this kind of character (overrides the project default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	TSoftObjectPtr<UCinemafyMoveLibrary> MoveLibrary;

	/** Mesh components (by name) Perform & Record leaves alone, e.g. Face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	TArray<FName> SkipRecordingComponents = { FName(TEXT("Face")) };

	/** Notes for whoever maintains the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (MultiLine = true))
	FString Notes;

	/** Fills this profile from a preset (keeps nothing that was there). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Performer")
	void ApplyPreset(ECinemafyPerformerPreset Preset);

	/** Checks a character against the profile: one line per hook, "ok" or what's missing. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Performer")
	FString Validate(const ACharacter* Character) const;

	/** The profile for a character: its choreography's, else Project Settings' map by class, else null. */
	static const UCinemafyPerformerProfile* ResolveFor(const ACharacter* Character, const UCinemafyPerformerProfile* Explicit);
};
