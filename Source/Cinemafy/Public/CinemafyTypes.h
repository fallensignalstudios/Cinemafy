#pragma once

#include "CoreMinimal.h"
#include "CinemafyTypes.generated.h"

/** Bone names Cinemafy uses to find a character's eyes and head. Defaults match the MetaHuman skeletons. */
namespace Cinemafy
{
	inline const FName LeftEyeBone(TEXT("FACIAL_L_Eye"));
	inline const FName RightEyeBone(TEXT("FACIAL_R_Eye"));
	inline const FName HeadBone(TEXT("head"));
}

/** A cinema prime lens kit. Real productions shoot on a handful of primes, not arbitrary zoom values. */
UENUM(BlueprintType)
enum class ECinemafyPrime : uint8
{
	Prime18  UMETA(DisplayName = "18mm (ultra wide)"),
	Prime24  UMETA(DisplayName = "24mm (wide)"),
	Prime35  UMETA(DisplayName = "35mm (storyteller)"),
	Prime50  UMETA(DisplayName = "50mm (natural)"),
	Prime85  UMETA(DisplayName = "85mm (portrait)"),
	Prime135 UMETA(DisplayName = "135mm (tele)")
};

/** Sensor / filmback presets. */
UENUM(BlueprintType)
enum class ECinemafySensor : uint8
{
	Super35_16x9   UMETA(DisplayName = "Super 35 (16:9)"),
	Super35_4x3    UMETA(DisplayName = "Super 35 (4:3 open gate)"),
	FullFrame_16x9 UMETA(DisplayName = "Full Frame (16:9)")
};

/** Classic coverage shot types. "Subject" is the character the shot is about; "Counterpart" is who they're talking to. */
UENUM(BlueprintType)
enum class ECinemafyShotType : uint8
{
	WideTwoShot     UMETA(DisplayName = "Wide Two-Shot"),
	MediumTwoShot   UMETA(DisplayName = "Medium Two-Shot"),
	OverTheShoulder UMETA(DisplayName = "Over-the-Shoulder"),
	MediumShot      UMETA(DisplayName = "Medium Shot"),
	MediumCloseUp   UMETA(DisplayName = "Medium Close-Up"),
	CloseUp         UMETA(DisplayName = "Close-Up"),
	ExtremeCloseUp  UMETA(DisplayName = "Extreme Close-Up"),
	LowHero         UMETA(DisplayName = "Low Hero")
};

/** How a camera operator would be holding the camera. */
UENUM(BlueprintType)
enum class ECinemafyCameraFeel : uint8
{
	Locked    UMETA(DisplayName = "Locked Off"),
	Tripod    UMETA(DisplayName = "Tripod (fluid head drift)"),
	Shoulder  UMETA(DisplayName = "Shoulder Rig"),
	Handheld  UMETA(DisplayName = "Handheld"),
	RunAndGun UMETA(DisplayName = "Run & Gun"),
	Custom    UMETA(DisplayName = "Custom", Hidden)
};

/** Built-in grading looks. */
UENUM(BlueprintType)
enum class ECinemafyLook : uint8
{
	CleanCinema  UMETA(DisplayName = "Clean Cinema"),
	WarmEpic     UMETA(DisplayName = "Warm Epic"),
	TealOrange   UMETA(DisplayName = "Teal & Orange"),
	BleachBypass UMETA(DisplayName = "Bleach Bypass"),
	Noir         UMETA(DisplayName = "Noir"),
	ColdSteel    UMETA(DisplayName = "Cold Steel")
};

/** Layered procedural noise settings for camera feel. Rotation vectors are (Pitch, Yaw, Roll) in degrees. */
USTRUCT(BlueprintType)
struct FCinemafyFeelSettings
{
	GENERATED_BODY()

	/** Peak rotation wobble in degrees (X = Pitch, Y = Yaw, Z = Roll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel")
	FVector RotationAmplitude = FVector::ZeroVector;

	/** How fast the rotation wobble wanders, in Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0"))
	float RotationFrequency = 0.5f;

	/** Peak positional sway in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel")
	FVector LocationAmplitude = FVector::ZeroVector;

	/** How fast the positional sway wanders, in Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0"))
	float LocationFrequency = 0.5f;

	/** Slow rhythmic pitch from the operator breathing, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0"))
	float BreathingAmplitude = 0.f;

	/** Breaths per second. ~0.25 is a calm operator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0"))
	float BreathingRate = 0.25f;
};

/** Input to the shot solver. */
USTRUCT(BlueprintType)
struct FCinemafyShotRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	ECinemafyShotType ShotType = ECinemafyShotType::MediumCloseUp;

	/** Who the shot is about. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<AActor> Subject = nullptr;

	/** Who the subject is talking to / looking at. Optional for singles, required for two-shots and OTS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<AActor> Counterpart = nullptr;

	/** Use the lens a DP would typically reach for on this shot type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bUseRecommendedLens = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (EditCondition = "!bUseRecommendedLens"))
	ECinemafyPrime Lens = ECinemafyPrime::Prime50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	ECinemafySensor Sensor = ECinemafySensor::Super35_16x9;

	/**
	 * Which side of the line of action the camera lives on. Keep this the same for every shot in a scene
	 * and your coverage will never break the 180-degree rule.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bFlipSide = false;
};

/** Output of the shot solver. */
USTRUCT(BlueprintType)
struct FCinemafyShotSolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FTransform CameraTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyPrime Lens = ECinemafyPrime::Prime50;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float FocalLength = 50.f;

	/** Distance (cm) from the camera's focal plane to the subject's eyes. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float FocusDistance = 300.f;
};

/** Classic operator moves, all relative to where the camera starts. */
UENUM(BlueprintType)
enum class ECinemafyCameraMove : uint8
{
	PushIn       UMETA(DisplayName = "Push In"),
	PullOut      UMETA(DisplayName = "Pull Out"),
	TruckLeft    UMETA(DisplayName = "Truck Left"),
	TruckRight   UMETA(DisplayName = "Truck Right"),
	PedestalUp   UMETA(DisplayName = "Pedestal Up"),
	PedestalDown UMETA(DisplayName = "Pedestal Down"),
	CraneUp      UMETA(DisplayName = "Crane Up (tilt to hold)"),
	CraneDown    UMETA(DisplayName = "Crane Down (tilt to hold)"),
	OrbitLeft    UMETA(DisplayName = "Orbit Left"),
	OrbitRight   UMETA(DisplayName = "Orbit Right")
};

/** How a move accelerates. */
UENUM(BlueprintType)
enum class ECinemafyEase : uint8
{
	Linear         UMETA(DisplayName = "Linear (mechanical)"),
	EaseInOut      UMETA(DisplayName = "Ease In/Out (dolly grip)"),
	OperatorSettle UMETA(DisplayName = "Operator Settle (tiny overshoot)")
};

/** Classic portrait lighting setups. */
UENUM(BlueprintType)
enum class ECinemafyLightingStyle : uint8
{
	ThreePoint UMETA(DisplayName = "Three-Point (clean, heroic)"),
	Loop       UMETA(DisplayName = "Loop (natural, flattering)"),
	Rembrandt  UMETA(DisplayName = "Rembrandt (dramatic triangle)"),
	Butterfly  UMETA(DisplayName = "Butterfly (glamour, front-top)"),
	Split      UMETA(DisplayName = "Split (half in shadow)"),
	Noir       UMETA(DisplayName = "Noir (hard, high contrast)"),
	Silhouette UMETA(DisplayName = "Silhouette (rim-lit)")
};

/** Which side of the face the key light sits on, as seen from the camera. */
UENUM(BlueprintType)
enum class ECinemafyKeySide : uint8
{
	/** Key on the side of the face turned away from camera. The cinematic default: moodier, more shape. */
	ShortSide  UMETA(DisplayName = "Short Side (auto, moody)"),
	/** Key on the side of the face turned toward camera. Brighter, more open. */
	BroadSide  UMETA(DisplayName = "Broad Side (auto, open)"),
	CameraLeft UMETA(DisplayName = "Camera Left"),
	CameraRight UMETA(DisplayName = "Camera Right")
};

/** Where a cutscene plays. */
UENUM(BlueprintType)
enum class ECinemafyCutsceneAnchor : uint8
{
	/** Plays exactly where it was authored. */
	World       UMETA(DisplayName = "World (as authored)"),
	/**
	 * Re-anchors the whole cutscene to the player's feet and facing, so there's no teleport pop.
	 * Author the sequence around the world origin, with the player's start mark at (0,0,0) facing +X.
	 */
	Player      UMETA(DisplayName = "Relative to Player"),
	/** Re-anchors the cutscene to AnchorActor's transform. Same authoring rule as Player. */
	AnchorActor UMETA(DisplayName = "Relative to Anchor Actor")
};

/** How a cutscene hands off with gameplay. */
USTRUCT(BlueprintType)
struct FCinemafyCutsceneSettings
{
	GENERATED_BODY()

	/**
	 * Read the blend times from the sequence's camera cut easing (set them with Tools > Cinemafy >
	 * Set Up Gameplay Blend). If off, BlendInTime / BlendOutTime are used for the letterbox and alignment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	bool bUseSequenceBlendTimes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (ClampMin = "0", EditCondition = "!bUseSequenceBlendTimes"))
	float BlendInTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (ClampMin = "0", EditCondition = "!bUseSequenceBlendTimes"))
	float BlendOutTime = 1.f;

	/** Animate letterbox bars in and out with the camera blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Letterbox")
	bool bAnimatedLetterbox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Letterbox", meta = (ClampMin = "1", UIMax = "2.76", EditCondition = "bAnimatedLetterbox"))
	float LetterboxAspectRatio = 2.39f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor")
	ECinemafyCutsceneAnchor Anchor = ECinemafyCutsceneAnchor::World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor", meta = (EditCondition = "Anchor == ECinemafyCutsceneAnchor::AnchorActor"))
	TObjectPtr<AActor> AnchorActor = nullptr;

	/**
	 * As the blend back to gameplay starts, turn the player's control rotation to match the cutscene's
	 * final camera, so the gameplay camera lands facing the same way instead of whipping around.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handoff")
	bool bAlignPlayerToFinalCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handoff")
	bool bDisableMovementInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handoff")
	bool bDisableLookInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handoff")
	bool bHidePlayer = false;

	/** Hides the AHUD. For UMG HUDs, listen to OnCutsceneStarted / OnCutsceneFinished. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handoff")
	bool bHideHUD = true;
};

/** A listener's personality: how often and how strongly they react. */
UENUM(BlueprintType)
enum class ECinemafyTemperament : uint8
{
	Attentive UMETA(DisplayName = "Attentive (engaged, nods)"),
	Warm      UMETA(DisplayName = "Warm (smiles, tilts)"),
	Stoic     UMETA(DisplayName = "Stoic (minimal, slow)"),
	Skeptical UMETA(DisplayName = "Skeptical (tilts, furrows)"),
	Nervous   UMETA(DisplayName = "Nervous (glances away, fidgets)"),
	Hostile   UMETA(DisplayName = "Hostile (chin up, stares)")
};

/** A single listening gesture. Trigger these from Sequencer events with React(). */
UENUM(BlueprintType)
enum class ECinemafyReaction : uint8
{
	Nod        UMETA(DisplayName = "Nod"),
	SmallNod   UMETA(DisplayName = "Small Nod (uh-huh)"),
	DoubleNod  UMETA(DisplayName = "Double Nod"),
	HeadShake  UMETA(DisplayName = "Head Shake"),
	Tilt       UMETA(DisplayName = "Head Tilt"),
	BrowRaise  UMETA(DisplayName = "Brow Raise"),
	Smile      UMETA(DisplayName = "Smile"),
	Frown      UMETA(DisplayName = "Frown"),
	LookAway   UMETA(DisplayName = "Look Away (thinking)"),
	Blink      UMETA(DisplayName = "Blink")
};
