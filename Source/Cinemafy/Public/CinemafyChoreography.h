#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinemafyChoreographyTypes.h"
#include "CinemafyChoreographyLibrary.h"
#include "CinemafyChoreography.generated.h"

class ACharacter;
class AController;
class APlayerController;
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

/** A strike that should land at Time if its montage is still playing then. */
struct FCinemafyPendingContact
{
	int32 BeatIndex = INDEX_NONE;
	float Time = 0.f;
	bool bEstimated = true;
	TWeakObjectPtr<UAnimMontage> Montage;
	FString Note;
};

/** A target's reaction, timed so its impact lands on the attacker's contact. */
struct FCinemafyPendingReaction
{
	int32 BeatIndex = INDEX_NONE;
	TWeakObjectPtr<ACharacter> Defender;
	/** Take seconds: when the reaction clip starts, and when the strike lands. */
	float StartTime = 0.f;
	float ContactTime = 0.f;
	TWeakObjectPtr<UAnimSequenceBase> Clip;
	TWeakObjectPtr<UAnimMontage> AttackMontage;
	ECinemafyHitReaction Reaction = ECinemafyHitReaction::Hit;
};

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

	/** Defenders turn to face an incoming strike when it comes from within this many degrees of their front; beyond it they take it from behind or the side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Interaction", meta = (ClampMin = "0", ClampMax = "180"))
	float DefenderTurnLimit = 120.f;

	/** Max walk speed per gait when the character has no gait wiring of its own (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Wiring", AdvancedDisplay)
	FVector FallbackSpeeds = FVector(180.f, 400.f, 650.f);

	/** Draw the route and beat labels in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Display")
	bool bShowPath = true;

	/**
	 * Skeletal mesh components (by component name) that Perform & Record leaves alone. Faces are
	 * Cinemafy Face's job by default; follower meshes (leader pose) are always skipped, since they copy their leader.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choreography|Take")
	TArray<FName> SkipRecordingComponents = { FName(TEXT("Face")) };

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

	/** Rebuilds the beat labels in the viewport (prompts, plus outcomes after a take). */
	void RefreshLabels();

	/** Reads one beat's prompt and picks its animations. */
	void ResolveBeat(int32 BeatIndex);

	/** Recorded while performing: the performer's transform over time (seconds since the take started). */
	TArray<float> RecordedTimes;
	TArray<FTransform> RecordedTransforms;

	/** Characters this performance acts on that have no choreography of their own (they're recorded with this take). */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Choreography|Take")
	TArray<TObjectPtr<ACharacter>> Partners;

	/** Each partner's transform at every RecordedTimes sample. */
	TArray<TArray<FTransform>> PartnerTransforms;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	enum class EPhase : uint8 { Start, Approach, Settle, Face, Act, Done };

	void BuildRig();
	void ApplyGait(ECinemafyGait Gait);
	bool ApplyOverlay(const FString& Weapon);
	bool TryTraverse();

	void StartBeat();
	void FinishBeat(ECinemafyBeatOutcome Outcome, const FString& Reason = FString());
	void AddEvent(ECinemafyPerformanceEventKind Kind, bool bEstimated = false, const FString& Note = FString(), float AtTime = -1.f);
	/** Confirms contacts whose time has come (only while their montage is still playing: an interrupted swing doesn't hit). */
	void UpdatePendingContacts();
	/** Puts the performer back the way it was found (possession, gait flags, crouch, speed, warp targets). */
	void RestorePerformer();
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
	TWeakObjectPtr<APlayerController> SavedPlayer;
	float SavedMaxWalkSpeed = -1.f;
	bool bStartedCrouched = false;
	bool bWorldEnding = false;
	ECinemafyArrival CurrentArrival = ECinemafyArrival::StopAndFace;
	/** Set when the approach ended short of the mark (geometry); the beat can still act, but it isn't a clean success. */
	FString ShortfallReason;
	FString FallbackReason;
	TArray<FCinemafyPendingContact> PendingContacts;
	TArray<FCinemafyPendingReaction> PendingReactions;
	/** Montages we started on defenders (so a combo's next reaction may cut in on the last one). */
	TArray<TWeakObjectPtr<UAnimMontage>> ReactionMontages;
	/** Defenders who died or went down, with the montage whose last pose they hold; they don't react again. */
	TArray<TPair<TWeakObjectPtr<ACharacter>, TWeakObjectPtr<UAnimMontage>>> HeldPoses;
	bool IsDowned(const ACharacter* Character) const;
	TMap<FString, TArray<FCinemafyMoveCandidate>> PartnerCandidates;

	/** Schedules the target's answer to a strike that lands at ContactTime (take seconds). */
	void ScheduleReaction(int32 BeatIndex, UAnimMontage* AttackMontage, float ContactTime, bool bFinalStrike);
	void UpdateReactions(float DeltaSeconds);
	/** Picks a reaction clip for a defender; OutLead = how long before contact it has to start. */
	UAnimSequenceBase* PickReaction(ACharacter* Defender, ECinemafyHitReaction Reaction, float RelativeYaw, float& OutLead);
	TSet<FSoftObjectPath> UsedAnimations;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Labels;
};
