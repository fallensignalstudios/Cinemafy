#pragma once

#include "CoreMinimal.h"
#include "CinemafyTypes.generated.h"

class USoundBase;
class UAnimSequenceBase;

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
	LowHero         UMETA(DisplayName = "Low Hero"),
	/** Frames everyone in a 3+ character scene. */
	GroupShot       UMETA(DisplayName = "Group Shot"),
	/** Walk-and-talk: camera backs up in front of the characters as they walk. */
	TrackingLead    UMETA(DisplayName = "Walk & Talk: Lead"),
	/** Walk-and-talk: camera follows behind, over the shoulder. */
	TrackingFollow  UMETA(DisplayName = "Walk & Talk: Follow")
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

	/** Everyone else a Group Shot should include (beyond Subject and Counterpart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TArray<TObjectPtr<AActor>> Group;

	/** Swing the camera further around the subject (degrees), staying on the same side of the line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", AdvancedDisplay)
	float AngleOffset = 0.f;

	/** Raise (+) or lower (-) the camera, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", AdvancedDisplay)
	float HeightOffset = 0.f;
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
	OrbitRight   UMETA(DisplayName = "Orbit Right"),
	/** A crash zoom on the lens itself (focal length), not a dolly. Distance = how far: 100 = 2x. */
	SnapZoomIn   UMETA(DisplayName = "Snap Zoom In (crash zoom)")
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

/** Emotion poses layered on top of a facial performance. */
UENUM(BlueprintType)
enum class ECinemafyEmotion : uint8
{
	Neutral,
	Happy,
	Sad,
	Angry,
	Fear,
	Surprise,
	Disgust,
	Contempt,
	Determined,
	Worried,
	Count UMETA(Hidden)
};

/** Face regions, for per-region control of a facial performance. */
UENUM(BlueprintType)
enum class ECinemafyFaceRegion : uint8
{
	Brows,
	Eyes,
	Cheeks,
	Nose,
	Mouth,
	Jaw,
	Tongue,
	Other,
	Count UMETA(Hidden)
};

/** How one face region's curves are processed. */
USTRUCT(BlueprintType)
struct FCinemafyFaceRegionSettings
{
	GENERATED_BODY()

	/** Multiplies the performance. > 1 boosts, < 1 calms it down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (ClampMin = "0", UIMax = "3"))
	float Gain = 1.f;

	/** Added after gain. Use small values to bias a region (e.g. a slight resting brow raise). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (UIMin = "-0.5", UIMax = "0.5"))
	float Offset = 0.f;

	/** 0 = raw, 1 = heavily smoothed. Adaptive: slow jitter is removed, fast moves still get through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (ClampMin = "0", ClampMax = "1"))
	float JitterReduction = 0.f;

	/** 0 = none, 1 = jelly. A touch of spring gives flesh a little overshoot and settle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (ClampMin = "0", ClampMax = "1"))
	float Springiness = 0.f;
};

/** Gain/offset for one specific curve, applied on top of its region. */
USTRUCT(BlueprintType)
struct FCinemafyCurveOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (ClampMin = "0", UIMax = "3"))
	float Gain = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (UIMin = "-0.5", UIMax = "0.5"))
	float Offset = 0.f;
};

/** A custom emotion pose: curve name -> additive value. */
USTRUCT(BlueprintType)
struct FCinemafyEmotionPose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	TMap<FName, float> Curves;
};

/** How the lip-sync stage of Cinemafy Face sharpens a dialogue performance. */
USTRUCT(BlueprintType)
struct FCinemafyLipSyncSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
	bool bEnabled = true;

	/** P / B / M: how firmly detected lip closures are sealed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", ClampMax = "1"))
	float ClosureAssist = 0.8f;

	/** Minimum time a closure stays sealed, so it can't fall between frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", UIMax = "150", Units = "ms"))
	float ClosureHoldMs = 70.f;

	/** F / V: how firmly the lower lip tucks under the teeth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", ClampMax = "1"))
	float LabiodentalAssist = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", UIMax = "150", Units = "ms"))
	float LabiodentalHoldMs = 60.f;

	/** OO / W: extra lip rounding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", UIMax = "1"))
	float RoundingBoost = 0.3f;

	/** EE / S: extra lip stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", UIMax = "1"))
	float WideBoost = 0.2f;

	/** Lifts small jaw openings so quiet speech still reads, without blowing out big ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", UIMax = "1"))
	float JawLift = 0.3f;

	/** Damps small mouth twitches while nobody's talking. Real expressions pass through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync", meta = (ClampMin = "0", ClampMax = "1"))
	float SilenceGate = 0.5f;
};

/** Which MetaHuman curves the lip-sync stage reads and shapes. Editable in the Face Profile. */
USTRUCT(BlueprintType)
struct FCinemafyLipSyncCurves
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
	FName Jaw = TEXT("CTRL_expressions_jawOpen");

	/** Lips pressed together (P / B / M). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
	TArray<FName> Closure = {
		TEXT("CTRL_expressions_mouthLipsTogetherUL"), TEXT("CTRL_expressions_mouthLipsTogetherUR"),
		TEXT("CTRL_expressions_mouthLipsTogetherDL"), TEXT("CTRL_expressions_mouthLipsTogetherDR") };

	/** Lower lip under the upper teeth (F / V). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
	TArray<FName> Labiodental = {
		TEXT("CTRL_expressions_mouthLowerLipBiteL"), TEXT("CTRL_expressions_mouthLowerLipBiteR") };

	/** Rounded lips (OO / W). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
	TArray<FName> Rounding = {
		TEXT("CTRL_expressions_mouthFunnelUL"), TEXT("CTRL_expressions_mouthFunnelUR"),
		TEXT("CTRL_expressions_mouthFunnelDL"), TEXT("CTRL_expressions_mouthFunnelDR"),
		TEXT("CTRL_expressions_mouthPurseUL"), TEXT("CTRL_expressions_mouthPurseUR"),
		TEXT("CTRL_expressions_mouthPurseDL"), TEXT("CTRL_expressions_mouthPurseDR") };

	/** Stretched lips (EE / S). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
	TArray<FName> Wide = {
		TEXT("CTRL_expressions_mouthStretchL"), TEXT("CTRL_expressions_mouthStretchR") };
};

/** Built-in directing styles: archetypes, not imitations of real directors. */
UENUM(BlueprintType)
enum class ECinemafyDirectorPreset : uint8
{
	ClassicHollywood UMETA(DisplayName = "Classic Hollywood (clean coverage)"),
	IntimateDrama    UMETA(DisplayName = "Intimate Drama (close, lingering)"),
	KineticAction    UMETA(DisplayName = "Kinetic Action (fast, handheld, low)"),
	SlowBurn         UMETA(DisplayName = "Slow Burn (wide, patient)"),
	NoirThriller     UMETA(DisplayName = "Noir Thriller (low angles, tight)"),
	Epic             UMETA(DisplayName = "Epic (wide, heroic)"),
	SpaceOpera       UMETA(DisplayName = "Space Opera Classic (wipes, locked-off, two-shots)"),
	MindBender       UMETA(DisplayName = "Mind-Bender Epic (huge wides, tight singles, cross-cutting)"),
	SymmetryStorybook UMETA(DisplayName = "Symmetry Storybook (centered, front-on, deadpan)"),
	LongTakeTension  UMETA(DisplayName = "Long-Take Tension (few cuts, creeping moves)"),
	SnapZoomGrindhouse UMETA(DisplayName = "Snap Zoom Grindhouse (crash zooms, extreme close-ups)"),
	GoldenHourLyrical UMETA(DisplayName = "Golden Hour Lyrical (drifting, warm, lingering)"),
	Custom           UMETA(DisplayName = "Custom")
};

/** How one shot hands over to the next at a scene break. */
UENUM(BlueprintType)
enum class ECinemafyTransition : uint8
{
	Cut              UMETA(DisplayName = "Cut"),
	Wipe             UMETA(DisplayName = "Wipe (new shot pushes the old one off)"),
	Dissolve         UMETA(DisplayName = "Dissolve"),
	FadeThroughBlack UMETA(DisplayName = "Fade Through Black"),
	MatchCut         UMETA(DisplayName = "Match Cut (same framing across the break)")
};

/** Which way wipes travel. */
UENUM(BlueprintType)
enum class ECinemafyWipeDirection : uint8
{
	LeftToRight,
	RightToLeft,
	TopToBottom,
	BottomToTop,
	/** Vary it every time, like the classic serials did. */
	Alternate
};

/**
 * A directing style: the shot vocabulary a director favors and how pacing reacts to drama.
 * Shot choice = preference x how well the shot size fits the moment's intensity x continuity rules.
 */
USTRUCT(BlueprintType)
struct FCinemafyDirectorStyle
{
	GENERATED_BODY()

	FCinemafyDirectorStyle();

	/** Relative preference for each shot type (0 = never, 1 = neutral, 2 = loves it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	TMap<ECinemafyShotType, float> ShotPreference;

	/** How much intensity tightens the framing (wide when calm, close-ups at the peak). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0", ClampMax = "1"))
	float TightenWithIntensity = 0.6f;

	/** Shot length range (seconds) at intensity 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pacing")
	FVector2D CalmShotLength = FVector2D(3.5, 6.0);

	/** Shot length range (seconds) at intensity 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pacing")
	FVector2D IntenseShotLength = FVector2D(1.8, 3.5);

	/** Chance to cut to the listener during a line (higher in the pause after it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pacing", meta = (ClampMin = "0", ClampMax = "1"))
	float ReactionShotChance = 0.35f;

	/** Appetite for low hero angles when things heat up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0", ClampMax = "1"))
	float LowAngleBias = 0.2f;

	/** Shift every lens choice wider (-) or longer (+), in prime steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "-2", ClampMax = "2"))
	int32 LensBias = 0;

	/** Open the scene on a wide two-shot so the audience knows where everyone is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bEstablishingShot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	ECinemafyCameraFeel CalmFeel = ECinemafyCameraFeel::Tripod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	ECinemafyCameraFeel IntenseFeel = ECinemafyCameraFeel::Shoulder;

	/** Appetite for camera moves (push-ins, crane-ups). Rises with intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float MoveChance = 0.3f;

	/** Grade every camera with this style's look when directing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	bool bApplyLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (EditCondition = "bApplyLook"))
	ECinemafyLook Look = ECinemafyLook::CleanCinema;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (EditCondition = "bApplyLook", ClampMin = "0", ClampMax = "1.5"))
	float LookStrength = 0.8f;

	/** Frame shape for this style (0 = full 16:9). Applied to the cameras. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (EditCondition = "bApplyLook", ClampMin = "0", UIMax = "2.76"))
	float LetterboxAspect = 2.39f;

	/** Give every character a Light Rig in this style when directing (existing rigs are restyled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bLightCharacters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting", meta = (EditCondition = "bLightCharacters"))
	ECinemafyLightingStyle LightingStyle = ECinemafyLightingStyle::ThreePoint;

	// --- Transitions ---------------------------------------------------------------

	/** How the scene opens (Fade Through Black = fade in from black). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	ECinemafyTransition OpeningTransition = ECinemafyTransition::Cut;

	/** Used on lines marked as a Scene Break (new place, jump in time, cross-cut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	ECinemafyTransition SceneBreakTransition = ECinemafyTransition::Cut;

	/** How the scene ends (Fade Through Black = fade out to black). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	ECinemafyTransition ClosingTransition = ECinemafyTransition::Cut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions", meta = (ClampMin = "0.1", UIMax = "3"))
	float TransitionDuration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	ECinemafyWipeDirection WipeDirection = ECinemafyWipeDirection::Alternate;

	// --- Coverage habits -------------------------------------------------------------

	/** Chance a two-shot or group shot holds across a change of speaker (focus racks instead of cutting). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0", ClampMax = "1"))
	float HoldThroughChance = 0.2f;

	/** 0 = classic angled coverage with lead room, 1 = centered, front-on, symmetrical framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0", ClampMax = "1"))
	float FrontalFraming = 0.f;

	/** Walk-and-talk: 1 = lead the characters (camera backs up in front), 0 = follow behind them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0", ClampMax = "1"))
	float TrackingLeadPreference = 0.7f;

	// --- Action ------------------------------------------------------------------------

	/** Appetite for crash zooms on impacts and big emphasis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", ClampMax = "1"))
	float CrashZoomChance = 0.1f;

	/** Appetite for speed ramps (slow motion) on the biggest impacts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", ClampMax = "1"))
	float SlowMoChance = 0.15f;

	/** Interleave with the director's Parallel Director by default (cross-cutting). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross-Cutting", meta = (ClampMin = "1", UIMax = "6"))
	int32 CrossCutLinesPerBlock = 3;
};

/** What Cinemafy heard in a line's dialogue audio. */
USTRUCT(BlueprintType)
struct FCinemafyAudioAnalysis
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	bool bValid = false;

	/** Average level of the speech (dBFS, silence excluded). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	float LoudnessDb = -60.f;

	/** How hard the loudest moments hit (95th percentile, dBFS). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	float PeakDb = -60.f;

	/** Speech rate estimate from loudness bursts (roughly syllables per second). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	float SyllablesPerSecond = 0.f;

	/** Fraction of the clip that's speech rather than silence. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	float ActiveRatio = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	float Duration = 0.f;

	/** Breaths and pauses inside the line (seconds from its start): where editors hide cuts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<float> PauseTimes;

	/** The most stressed moments (seconds from its start), loudest first: where camera moves should land. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<float> EmphasisTimes;
};

/** One line of dialogue in a scene script. */
USTRUCT(BlueprintType)
struct FCinemafyLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	TObjectPtr<AActor> Speaker = nullptr;

	/** Who they're talking to (needed for reaction shots, two-shots and over-the-shoulders). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	TObjectPtr<AActor> Listener = nullptr;

	/** How long the line takes to say, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (ClampMin = "0.2"))
	float Duration = 3.f;

	/** Silence after the line, in seconds. Reaction shots love this space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (ClampMin = "0"))
	float PauseAfter = 0.5f;

	/** Dramatic weight of the line: 0 = small talk, 1 = the big reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (ClampMin = "0", ClampMax = "1"))
	float Intensity = 0.5f;

	/** The recorded line. Drives intensity and timing, and is laid into the sequence when directing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Audio")
	TObjectPtr<USoundBase> Audio = nullptr;

	/** A fight, chase or stunt beat: cut on impacts, crash zooms and speed ramps instead of dialogue coverage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Action")
	bool bAction = false;

	/** Moments of impact (seconds from the line's start). Empty = taken from the audio's loudest hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Action", meta = (EditCondition = "bAction"))
	TArray<float> ImpactTimes;

	/** Which scene this line belongs to when cross-cutting (set automatically). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line", AdvancedDisplay)
	int32 SceneId = 0;

	/** The characters are walking during this line (auto-detected from the sequence when directing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	bool bMoving = false;

	/** This line starts somewhere (or somewhen) new: use the style's scene-break transition and re-establish. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	bool bSceneBreak = false;

	/** Use a specific transition into this line instead of the style's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (InlineEditConditionToggle))
	bool bOverrideTransition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (EditCondition = "bOverrideTransition"))
	ECinemafyTransition Transition = ECinemafyTransition::Cut;

	/** What's said, for subtitles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (MultiLine = true))
	FText Text;

	/** Name shown in subtitles. Empty = the speaker actor's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FText SpeakerName;

	/** The speaker's body performance for this line (any clip on the body skeleton). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Performance")
	TObjectPtr<UAnimSequenceBase> BodyAnimation = nullptr;

	/** The speaker's face performance for this line (e.g. a MetaHuman Animator take on the face skeleton). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Performance")
	TObjectPtr<UAnimSequenceBase> FaceAnimation = nullptr;

	/** Let the delivery decide how intense the line is (blended by the director's Audio Intensity Weight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Audio")
	bool bAutoIntensity = true;

	/** Set Duration from the audio's length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line|Audio")
	bool bUseAudioDuration = true;

	/** Intensity heard in the audio, relative to the rest of the scene. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line|Audio")
	float AudioIntensity = -1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line|Audio")
	FCinemafyAudioAnalysis Analysis;

	/** Override the director for the start of this line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	bool bForceShot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (EditCondition = "bForceShot"))
	ECinemafyShotType ForcedShot = ECinemafyShotType::CloseUp;

	/** Change the speaker's Cinemafy Face emotion as this line starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	bool bSetEmotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (EditCondition = "bSetEmotion"))
	ECinemafyEmotion Emotion = ECinemafyEmotion::Neutral;

	/** The line itself, or a note. Shown in the list and the plan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FString Note;
};

/** A focus pull inside a shot, e.g. a held two-shot racking to whoever starts talking. */
USTRUCT(BlueprintType)
struct FCinemafyRackEvent
{
	GENERATED_BODY()

	/** Seconds from the start of the shot. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float Time = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TObjectPtr<AActor> FocusOn = nullptr;
};

/** A shot the director decided on. */
USTRUCT(BlueprintType)
struct FCinemafyPlannedShot
{
	GENERATED_BODY()

	/** Seconds from the start of the scene. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float StartTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyShotType ShotType = ECinemafyShotType::MediumCloseUp;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TObjectPtr<AActor> Subject = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TObjectPtr<AActor> Counterpart = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyPrime Lens = ECinemafyPrime::Prime50;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyCameraFeel Feel = ECinemafyCameraFeel::Tripod;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bIsReaction = false;

	/** Does the camera move during this shot? */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyCameraMove Move = ECinemafyCameraMove::PushIn;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float MoveDistance = 0.f;

	/** When the move starts and lands, in seconds from the start of the shot (lands on emphasis when known). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float MoveStart = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float MoveEnd = 0.f;

	/** Set the speaker's emotion as this shot starts (first shot of a line). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bSetEmotion = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyEmotion Emotion = ECinemafyEmotion::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	int32 LineIndex = 0;

	/** Everyone else in frame for a Group Shot. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TArray<TObjectPtr<AActor>> Group;

	/** Who's in focus as the shot starts (the speaker, for two-shots). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TObjectPtr<AActor> FocusOn = nullptr;

	/** Focus pulls during the shot (held two-shots rack to each new speaker). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	TArray<FCinemafyRackEvent> RackEvents;

	/** Slow motion around an impact in this shot. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bSlowMo = false;

	/** When the slowed impact happens, in seconds from the start of the shot. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float SlowMoTime = 0.f;

	/** How this shot arrives (opening / scene break). Anything but Cut is a transition. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyTransition TransitionIn = ECinemafyTransition::Cut;

	/** How the scene leaves this shot (only on the last shot). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyTransition TransitionOut = ECinemafyTransition::Cut;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float TransitionDuration = 0.7f;

	/** Resolved wipe direction (never Alternate). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	ECinemafyWipeDirection WipeDirection = ECinemafyWipeDirection::LeftToRight;

	/** First shot of its line (where the line, its subtitle and its emotion start). */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool bLineStart = false;

	/** Why the director chose it, in plain words. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FString Reason;
};
