#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinemafyChoreographyTypes.h"
#include "CinemafyChoreographyLibrary.h"
#include "CinemafyChoreography.generated.h"

class ACharacter;
class AController;
class UAnimMontage;
class UTextRenderComponent;
class ULevelSequence;
class ACinemafySceneDirector;

/** Buttons on the choreography that the editor module answers. */
UENUM()
enum class ECinemafyChoreoRequest : uint8
{
	/** Enter click-to-place mode for this choreography. */
	Choreograph,
	/** Play in editor, perform every choreography, record the takes and bake them into the open Level Sequence. */
	PerformAndRecord,
	/** Hand the recorded take to a Scene Director and cut shots around it. */
	DirectShots
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FCinemafyChoreoEditorRequest, class ACinemafyChoreography*, ECinemafyChoreoRequest);
DECLARE_MULTICAST_DELEGATE_TwoParams(FCinemafyChoreoPerformanceEvent, class ACinemafyChoreography*, bool /*bStarted*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCinemafyChoreographyFinished);

/** Hooks into a performer (gait, traversal, overlay, crouch), found by reflection once per performance. */
struct FCinemafyPerformerRig
{
	TFunction<void(bool)> SetSprint;
	TFunction<void(bool)> SetWalk;
	TFunction<bool()> Traverse;
	TFunction<bool(const FString&)> SetOverlay;
	TFunction<void(bool)> SetCrouch;
};

/**
 * A choreographed performance for one character: click points in the level, give each a
 * word or two ("walk", "vault", "sprint", "climb", "sword attack", "taunt"), and the
 * character performs it for real - motion matching does the locomotion, the character's
 * own traversal does the vaults and climbs, and attacks / gestures are picked from your
 * animation library by name. Perform & Record bakes the take into Sequencer; Direct Shots
 * cuts coverage around it.
 *
 * Works with any ACharacter. GASP / GASPALS / SignalForge-style characters are wired up
 * automatically (SetWantsToSprint/Walk, TryTraversalAction, overlay state); anything else
 * falls back to speed control, Jump and slot montages.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Cinemafy Choreography"))
class CINEMAFY_API ACinemafyChoreography : public AActor
{
	GENERATED_BODY()

public:
	ACinemafyChoreography();

	/** Who performs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography")
	TObjectPtr<ACharacter> Performer = nullptr;

	/** The performance, in order. Each beat's Location is relative to this actor (move it to move the whole routine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography", meta = (TitleProperty = "Prompt"))
	TArray<FCinemafyChoreoBeat> Beats;

	/** Animations the director may use. Empty = every montage / sequence in Search Folders that fits the performer's skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Animation")
	TObjectPtr<UCinemafyMoveLibrary> MoveLibrary = nullptr;

	/** Where to look for animations when there's no Move Library ("/Game" if empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Animation", meta = (ContentDir))
	TArray<FDirectoryPath> SearchFolders;

	/** Include this performer when Perform & Record is pressed on any choreography (multi-character scenes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Take")
	bool bIncludeInTakes = true;

	/** After baking a take, let the Scene Director cut shots around it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Take")
	bool bDirectAfterBake = true;

	/** The Scene Director that cuts this performance (made by Direct Shots if empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Take")
	TObjectPtr<ACinemafySceneDirector> SceneDirector = nullptr;

	/** Style for a director made by Direct Shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Take")
	ECinemafyDirectorPreset DirectorStyle = ECinemafyDirectorPreset::SpaceOpera;

	/** Perform automatically when play begins (in game, without recording). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Take")
	bool bPerformOnBeginPlay = false;

	/** Idle before the first beat so the pose settles (and the camera has something to cut in on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Timing", meta = (ClampMin = "0.0"))
	float LeadInSeconds = 0.5f;

	/** Idle after the last beat before the take ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Timing", meta = (ClampMin = "0.0"))
	float TailSeconds = 1.f;

	/** "wait" with no number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Timing", meta = (ClampMin = "0.0"))
	float DefaultWaitSeconds = 1.5f;

	/** Give up on a beat after this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Timing", meta = (ClampMin = "1.0"))
	float BeatTimeout = 15.f;

	/** How close counts as arrived when the performer stops at a point (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Movement", meta = (ClampMin = "10.0"))
	float ArrivalRadius = 50.f;

	/** Moves that flow into another move turn this early (cm): motion matching rounds the corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Movement", meta = (ClampMin = "10.0"))
	float CornerRadius = 140.f;

	/** Melee attacks stop this far from the target's capsule (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Movement", meta = (ClampMin = "0.0"))
	float MeleeRange = 110.f;

	/** Start asking the character to traverse inside this distance of the obstacle point (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Movement", meta = (ClampMin = "50.0"))
	float TraversalRange = 350.f;

	/** Degrees per second when turning to face a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Movement", meta = (ClampMin = "30.0"))
	float TurnSpeed = 300.f;

	/** Follow the navmesh between points when there is one (moves only; traversal always goes straight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Movement")
	bool bUseNavMesh = true;

	/** Function overrides when auto-wiring guesses wrong (None = auto). E.g. SetWantsToSprint, TryTraversalAction, SetOverlayState. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Wiring", AdvancedDisplay)
	FName SprintFunction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Wiring", AdvancedDisplay)
	FName WalkFunction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Wiring", AdvancedDisplay)
	FName TraversalFunction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Wiring", AdvancedDisplay)
	FName OverlayFunction;

	/** Max walk speed per gait when the character has no gait wiring of its own (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Wiring", AdvancedDisplay)
	FVector FallbackSpeeds = FVector(180.f, 400.f, 650.f);

	/** Draw the route and beat labels in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Display")
	bool bShowPath = true;

	/** Length of the last recorded take (seconds). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Choreography|Take")
	float TakeDuration = 0.f;

	/** Where in the Level Sequence the last take was baked (seconds). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Choreography|Take")
	float TakeStartSeconds = 0.f;

	/** The sequence the last take was baked into. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Choreography|Take")
	TSoftObjectPtr<ULevelSequence> TakeSequence;

	/** What the auto-wiring found on the performer (refreshed when performing). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Choreography|Wiring", AdvancedDisplay)
	FString WiringReport;

	/** Set by Perform & Record; the play-in-editor copy performs and records, then the editor clears it. */
	UPROPERTY()
	bool bPerformOnNextPlay = false;

	UPROPERTY(BlueprintAssignable, Category = "Choreography")
	FCinemafyChoreographyFinished OnPerformanceFinished;

	/** Button presses for the editor module. */
	static FCinemafyChoreoEditorRequest OnEditorRequest;

	/** Fired when any choreography starts / finishes performing (the editor records takes off this). */
	static FCinemafyChoreoPerformanceEvent OnPerformanceEvent;

	/** Click points in the viewport and type what happens there. */
	UFUNCTION(CallInEditor, Category = "Choreography", meta = (DisplayPriority = 0))
	void Choreograph();

	/** Plays in editor, performs, records every choreography in the level and bakes the take into the open Level Sequence. */
	UFUNCTION(CallInEditor, Category = "Choreography", meta = (DisplayPriority = 1))
	void PerformAndRecord();

	/** Cuts shots around the recorded take with a Scene Director. */
	UFUNCTION(CallInEditor, Category = "Choreography", meta = (DisplayPriority = 2))
	void DirectShots();

	/** Re-reads every prompt and re-picks animations (e.g. after adding animations to the project). */
	UFUNCTION(CallInEditor, Category = "Choreography", meta = (DisplayPriority = 3))
	void ReadPrompts();

	/** Start performing now (in game / PIE). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	void Perform();

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	void StopPerforming();

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	bool IsPerforming() const { return bPerforming; }

	/** Adds a beat at a world location. Returns its index. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	int32 AddBeat(const FString& Prompt, FVector WorldLocation, AActor* Target);

	/** A beat's location in the world. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Choreography")
	FVector GetBeatWorldLocation(int32 BeatIndex) const;

	/** The beats as Scene Director lines (needs a recorded take). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Choreography")
	TArray<FCinemafyLine> BuildDirectorLines() const;

	/** Reads one beat's prompt and picks its animations. */
	void ResolveBeat(int32 BeatIndex);

	/** Recorded while performing: the performer's transform over time (seconds since the take started). */
	TArray<float> RecordedTimes;
	TArray<FTransform> RecordedTransforms;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	enum class EPhase : uint8 { Start, Approach, Face, Act, Done };

	void BuildRig();
	void ApplyGait(ECinemafyGait Gait);
	bool ApplyOverlay(const FString& Weapon);
	bool TryTraverse();

	void StartBeat();
	void FinishBeat(bool bFailed);
	void TickBeat(float DeltaSeconds);
	void FinishPerformance();

	/** Steers toward a point; true once within Radius. */
	bool SteerTo(const FVector& Destination, float Radius, float InputScale = 1.f);
	/** Turns toward a point; true once facing it. */
	bool FaceToward(const FVector& Point, float DeltaSeconds);
	/** Plays the next queued animation; false when there's nothing (left) to play. */
	bool PlayNextAnimation();
	bool IsAnimationBusy() const;
	bool IsTraversing() const;
	FVector GetTargetPoint(const FCinemafyChoreoBeat& Beat, int32 BeatIndex) const;

	void RefreshLabels();
	void DrawPath() const;

	TArray<FCinemafyMoveCandidate> Candidates;
	FString CandidatesSkeleton;
	void EnsureCandidates();

	FCinemafyPerformerRig Rig;
	bool bPerforming = false;
	float PerformTime = 0.f;
	float LeadInLeft = 0.f;
	float TailLeft = -1.f;
	int32 CurrentBeat = 0;
	EPhase Phase = EPhase::Start;
	float PhaseTime = 0.f;
	float BeatTime = 0.f;
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	FVector StuckCheckLocation = FVector::ZeroVector;
	float StuckTimer = 0.f;
	float TraverseRetry = 0.f;
	bool bTraversalStarted = false;
	bool bSawTraversal = false;
	float ActDoneDelay = 0.f;
	TArray<TObjectPtr<UAnimSequenceBase>> AnimQueue;
	TWeakObjectPtr<UAnimMontage> ActiveMontage;
	float ActivePlayRate = 1.f;
	ECinemafyGait CurrentGait = ECinemafyGait::Walk;
	bool bCrouched = false;
	TWeakObjectPtr<AController> SpawnedController;
	TSet<FSoftObjectPath> UsedAnimations;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Labels;
};
