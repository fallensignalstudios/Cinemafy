#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinemafyChoreographyTypes.generated.h"

class USoundBase;

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
	Equip     UMETA(DisplayName = "Equip"),
	/** Say a line on the spot, facing whoever it's said to: say "Stand down, Admiral". The next beat waits for it. */
	Say       UMETA(DisplayName = "Say")
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

/** What actually happened to a beat. A requested action, a chosen animation and a completed action are different things. */
UENUM(BlueprintType)
enum class ECinemafyBeatOutcome : uint8
{
	NotRun     UMETA(DisplayName = "Not Run"),
	Succeeded  UMETA(DisplayName = "Succeeded"),
	/** Done, but not the way it was asked (e.g. a jump instead of a vault). The reason says how. */
	Fallback   UMETA(DisplayName = "Fallback"),
	/** Geometry stopped the performer short of the mark. */
	Blocked    UMETA(DisplayName = "Blocked"),
	/** Nothing could perform it (no animation, no capability). */
	Failed     UMETA(DisplayName = "Failed"),
	TimedOut   UMETA(DisplayName = "Timed Out")
};

/** How a performer comes out of a big landing. */
UENUM(BlueprintType)
enum class ECinemafyRecovery : uint8
{
	None     UMETA(DisplayName = "None"),
	/** Moving on: a roll keeps the momentum. */
	Roll     UMETA(DisplayName = "Roll"),
	/** Stopping: a hard landing and a recovery to standing. */
	Recover  UMETA(DisplayName = "Recover")
};

/** The in-between moves a real performer adds around a beat, planned from what comes before and after it. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyBeatTransitions
{
	GENERATED_BODY()

	/** Draw this weapon first (the attack needs it and it isn't in hand). Empty = nothing to draw. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	FString DrawWeapon;

	/** Ease off from a sprint before committing (a gather step before a jump, a check before a strike). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	bool bBrakeIn = false;

	/** How far out the brake starts (cm before the takeoff / target). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	float BrakeDistance = 0.f;

	/** How the landing ends (planned from the measured drop; a big fall in the take adds one anyway). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	ECinemafyRecovery Recovery = ECinemafyRecovery::None;

	/** Plain words, e.g. "draws the sword first, brakes out of the sprint". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	FString Summary;
};

/** How a movement beat treats its mark. */
UENUM(BlueprintType)
enum class ECinemafyArrival : uint8
{
	/** Pass through before more travel, stop before anything else. */
	Auto         UMETA(DisplayName = "Auto"),
	/** A waypoint: round the corner at speed, no idle. */
	PassThrough  UMETA(DisplayName = "Pass Through"),
	/** Come to a real stop (speed settles) before the next beat starts. */
	StopAndFace  UMETA(DisplayName = "Stop"),
	/** Arrive moving, straight into the action (vaults). */
	EnterAction  UMETA(DisplayName = "Enter Action")
};

/** Kinds of moment in a recorded take, for the camera director. */
UENUM(BlueprintType)
enum class ECinemafyPerformanceEventKind : uint8
{
	BeatStart   UMETA(DisplayName = "Beat Start"),
	Arrived     UMETA(DisplayName = "Arrived"),
	ActionStart UMETA(DisplayName = "Action Start"),
	Takeoff     UMETA(DisplayName = "Takeoff"),
	Landing     UMETA(DisplayName = "Landing"),
	/** A strike landing. Estimated unless the clip has a hit/impact/contact notify. */
	Contact     UMETA(DisplayName = "Contact"),
	ActionEnd   UMETA(DisplayName = "Action End"),
	/** The target started reacting to a strike (synchronized so its impact meets the contact). */
	Reaction    UMETA(DisplayName = "Reaction"),
	/** A line of dialogue started / finished. */
	LineStart   UMETA(DisplayName = "Line Start"),
	LineEnd     UMETA(DisplayName = "Line End"),
	/** An in-between move: a draw before a strike, a brake out of a sprint, a roll out of a landing. */
	Transition  UMETA(DisplayName = "Transition")
};

/** One moment in a take: what, when (take seconds), where, and whether it was observed or estimated. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyPerformanceEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
	ECinemafyPerformanceEventKind Kind = ECinemafyPerformanceEventKind::BeatStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
	float Time = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
	FVector Location = FVector::ZeroVector;

	/** A guess (e.g. a strike timed at 40% of a clip with no hit notify), not something seen to happen. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
	bool bEstimated = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
	FString Note;
};

/** How the target of an attack answers it. */
UENUM(BlueprintType)
enum class ECinemafyHitReaction : uint8
{
	/** Flinch from the hit, from the side it came from. */
	Hit        UMETA(DisplayName = "Hit React"),
	/** Off their feet ("heavy attack", "smash", "knock down"). */
	Knockdown  UMETA(DisplayName = "Knockdown"),
	/** "kill", "finish": dies and stays down. */
	Death      UMETA(DisplayName = "Death"),
	/** "attack blocked", "parried". */
	Block      UMETA(DisplayName = "Block"),
	/** "attack dodged", "missed". */
	Dodge      UMETA(DisplayName = "Dodge"),
	/** No reaction (the target does its own thing). */
	None       UMETA(DisplayName = "None")
};

/** What a measured obstacle allows. */
UENUM(BlueprintType)
enum class ECinemafyTraversalVerdict : uint8
{
	NotChecked     UMETA(DisplayName = "Not Checked"),
	/** Too low to matter: just walk over it. */
	StepOver       UMETA(DisplayName = "Step Over"),
	Vault          UMETA(DisplayName = "Vault"),
	/** Climb up onto it (deep or tall). */
	Mantle         UMETA(DisplayName = "Mantle"),
	/** No traversal system, but low enough to jump. */
	Jump           UMETA(DisplayName = "Jump"),
	TooTall        UMETA(DisplayName = "Too Tall"),
	NoRoomToLand   UMETA(DisplayName = "No Room to Land"),
	NoHeadroom     UMETA(DisplayName = "No Headroom"),
	TooNarrow      UMETA(DisplayName = "Too Narrow to Stand On"),
	BadAngle       UMETA(DisplayName = "Approach Too Steep"),
	NoObstacle     UMETA(DisplayName = "Nothing There"),
	CannotTraverse UMETA(DisplayName = "Can't Traverse"),
	/** A gap in the floor: run and jump it from the edge. */
	Gap            UMETA(DisplayName = "Jump the Gap"),
	/** The floor drops away: go off the ledge (with a recovery if it's high). */
	Drop           UMETA(DisplayName = "Drop Down"),
	/** A gap wider than the performer can jump. */
	TooFar         UMETA(DisplayName = "Gap Too Wide"),
	/** A drop higher than the performer can survive gracefully. */
	TooHighToDrop  UMETA(DisplayName = "Drop Too High")
};

/** An obstacle measured from where the performer approaches it. Distances in cm. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyTraversalCheck
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	ECinemafyTraversalVerdict Verdict = ECinemafyTraversalVerdict::NotChecked;

	/** The performer can get over / onto it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	bool bPossible = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	float Height = 0.f;

	/** Front face to far edge (very large = a platform). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	float Depth = 0.f;

	/** Clear space above the top. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	float Headroom = 0.f;

	/** Degrees between the approach and straight on to the face. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	float ApproachAngle = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	FVector FrontPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	FVector FarPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	FVector LandingPoint = FVector::ZeroVector;

	/** The obstacle face's outward normal (flat); crossing straight means moving along -FaceNormal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal|Route")
	FVector FaceNormal = FVector::ZeroVector;

	/** Where to be, squared up and at speed, before committing (the run-up starts here). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal|Route")
	FVector ApproachPoint = FVector::ZeroVector;

	/** Where the performer leaves the floor (the obstacle's face, or a gap's edge). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal|Route")
	FVector TakeoffPoint = FVector::ZeroVector;

	/** Where the performer ends up: the landing beyond a vault, the top of a mantle, the far side of a gap. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal|Route")
	FVector ExitPoint = FVector::ZeroVector;

	/** The approach was too oblique, so the route squares up to the face first. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal|Route")
	bool bLinedUp = false;

	/** Gaps: edge to edge, in cm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	float GapWidth = 0.f;

	/** How far below the starting floor the performer lands (negative = higher). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	float DropHeight = 0.f;

	/** In plain words: what was measured and why it will or won't work. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	FString Reason;
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

	/** Attacks: how the target answers the final strike (earlier strikes in a combo get hit reacts, or blocks). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	ECinemafyHitReaction Reaction = ECinemafyHitReaction::Hit;

	/** "combo" / "attack x3" -> how many animations to chain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	int32 Repeat = 1;

	/** Quoted words in the prompt: walk "Stand down, Admiral" -> the line said while walking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	FString Line;

	/** "answer", "reply", "after": wait for the other character's line to finish first. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prompt")
	bool bAfterLine = false;

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

	/** Movement beats: pass through the mark, stop on it, or enter the next action moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat")
	ECinemafyArrival Arrival = ECinemafyArrival::Auto;

	/** Stable identity (survives reordering and renaming). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat", AdvancedDisplay)
	FGuid Id;

	/** A line said during this beat (walk-and-talk, a line before a strike). Empty = the prompt's quoted words, if any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line", meta = (MultiLine = true))
	FText Dialogue;

	/** The recorded line. Played while performing, laid into the sequence when the director cuts the scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line")
	TObjectPtr<USoundBase> DialogueAudio = nullptr;

	/** Who it's said to. Empty = the beat's target if it's a character, else whoever this performer last spoke to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line")
	TObjectPtr<AActor> Listener = nullptr;

	/** Hold this beat until that character finishes their line (an answer, or a draw on the last word). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line")
	TObjectPtr<AActor> WaitForLineFrom = nullptr;

	/** How the line is said ("(coldly)", "(shouting)"): sets its feeling and intensity when the director cuts it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line")
	FString LineDirection;

	/**
	 * A line this performer hears rather than says: a voice with no body ("SHIP ANNOUNCEMENT", a radio, V.O.). It plays
	 * unattached with that name on the subtitle, the lips stay still, and the director watches this performer hearing it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line")
	FString HeardFrom;

	/** Wait for exactly this line of theirs (1 = their first line in the take); 0 = the next line they finish. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Line", meta = (ClampMin = "0"))
	int32 WaitForLineNumber = 0;

	/** Seconds into the last take when the line started, and how long it ran (-1 = no line). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	float LineStartTime = -1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	float LineDuration = 0.f;

	/** The words this beat says (Dialogue, else the prompt's quotes). */
	FText GetLineText() const { return Dialogue.IsEmpty() ? FText::FromString(Reading.Line) : Dialogue; }
	bool HasLine() const { return DialogueAudio != nullptr || !Dialogue.IsEmpty() || !Reading.Line.IsEmpty(); }

	/** Let the director add the in-between moves (draw before a strike, brake before a jump, roll out of a fall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Transitions")
	bool bAutoTransitions = true;

	/** The in-between moves planned for this beat (refreshed when beats change). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Transitions")
	FCinemafyBeatTransitions Transitions;

	/** Traversal beats: the obstacle as measured from the approach (refreshed when the beat is read). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Director")
	FCinemafyTraversalCheck TraversalCheck;

	/** Attacks on a character: make the target react, synchronized to the contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Interaction")
	bool bTargetReacts = true;

	/** Attacks: how the target answers, instead of what the prompt implies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Interaction", meta = (EditCondition = "bTargetReacts"))
	bool bOverrideReaction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Interaction", meta = (EditCondition = "bOverrideReaction"))
	ECinemafyHitReaction Reaction = ECinemafyHitReaction::Hit;

	/** Use this clip for the target's reaction instead of the one the director picks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat|Interaction", meta = (EditCondition = "bTargetReacts"))
	TObjectPtr<UAnimSequenceBase> ReactionOverride = nullptr;

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

	/** Cut points for the director: observed takeoffs and confirmed (or estimated) contacts, in take seconds. Derived from Events. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	TArray<float> ImpactTimes;

	/** Everything that happened in this beat, typed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	TArray<FCinemafyPerformanceEvent> Events;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	ECinemafyBeatOutcome Outcome = ECinemafyBeatOutcome::NotRun;

	/** Why the outcome isn't a plain success (shown on the beat's label too). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beat|Take")
	FString OutcomeReason;
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
