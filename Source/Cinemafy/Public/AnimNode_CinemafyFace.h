#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "CinemafyTypes.h"
#include "AnimNode_CinemafyFace.generated.h"

class UCinemafyFaceProfile;

/**
 * Processes a MetaHuman facial performance before RigLogic: per-region gain, adaptive jitter
 * cleanup (One Euro filter), spring "flesh" dynamics, emotion poses, breathing, idle face life and
 * blink-driven pupil response. It also applies the character's Cinemafy Gaze (eye look + blinks)
 * and Cinemafy Listener (brows, smile, squint), so no manual curve wiring is needed.
 *
 * Settings come from the owning actor's Cinemafy Face component (so they're keyable in Sequencer).
 * Place the node in the Face Anim Blueprint, right before the Output Pose.
 */
USTRUCT(BlueprintInternalUseOnly)
struct CINEMAFY_API FAnimNode_CinemafyFace : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	/** Blend between the untouched performance (0) and the processed one (1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PinShownByDefault, ClampMin = "0", ClampMax = "1"))
	float Alpha = 1.f;

	/** Process the performance with the Face component's settings (gain, smoothing, springs, emotion, life). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bApplyFace = true;

	/** Drive eye look and blinks from the character's Cinemafy Gaze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bApplyGaze = true;

	/** How much Gaze replaces the performance's own eye look (0 = keep the performer's eyes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bApplyGaze"))
	float GazeEyeLookBlend = 1.f;

	/** Add the character's Cinemafy Listener expressions (brows, smile, squint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bApplyListener = true;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

private:
	/** A snapshot of the game-thread components, so evaluation never touches UObjects on worker threads. */
	struct FFrameInput
	{
		bool bHasFace = false;
		float PerformanceIntensity = 1.f;
		float EmotionIntensity = 1.f;
		float FaceLife = 1.f;
		bool bBreathing = true;
		float BreathRate = 0.25f;
		bool bPupilResponse = true;
		TArray<float> EmotionWeights;

		bool bHasGaze = false;
		FVector2D EyeLook = FVector2D::ZeroVector;
		float GazeBlink = 0.f;
		float GazeWeight = 0.f;

		bool bHasListener = false;
		float BrowRaise = 0.f;
		float BrowFurrow = 0.f;
		float Smile = 0.f;
		float Squint = 0.f;
	};

	struct FCurveState
	{
		bool bInitialized = false;
		float Filtered = 0.f;
		float FilteredSpeed = 0.f;
		float SpringPos = 0.f;
		float SpringVel = 0.f;
	};

	ECinemafyFaceRegion ClassifyCurve(FName Curve, const FString& Prefix);
	float ProcessCurve(FName Curve, float Value, const UCinemafyFaceProfile& Profile, float DeltaTime);
	void AddEmotions(TMap<FName, float>& Additive) const;
	void AddLife(TMap<FName, float>& Additive) const;

	FFrameInput Input;
	/** Owned by the Face component (a UPROPERTY), so it outlives this node's use of it. */
	const UCinemafyFaceProfile* Profile = nullptr;
	const UCinemafyFaceProfile* EmotionPoseSource = nullptr;
	TMap<FName, FCurveState> CurveStates;
	TMap<FName, ECinemafyFaceRegion> RegionCache;
	/** Whether each curve name is a face control (has the profile's prefix). Avoids string work per frame. */
	TMap<FName, bool> FaceCurveCache;
	TArray<TMap<FName, float>> EmotionPoses;

	float PendingDeltaTime = 0.f;
	float Time = 0.f;
	float PreviousBlink = 0.f;
	float PupilConstriction = 0.f;
};
