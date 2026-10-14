#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyFollowComponent.generated.h"

/**
 * A Steadicam operator in a component: keeps the camera actor at a fixed offset from a walking
 * character (in their heading, ignoring pitch and roll), with a soft positional and rotational lag.
 * Put it on a camera, set Target, frame the shot, then CaptureOffset (the Director does all of this).
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Follow Rig"))
class CINEMAFY_API UCinemafyFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyFollowComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow")
	TObjectPtr<AActor> Target = nullptr;

	/** The camera's transform relative to the target's heading frame. Set by CaptureOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow")
	FTransform Offset;

	/** How quickly the camera catches up (higher = tighter). ~3 feels like a Steadicam operator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (ClampMin = "0", UIMax = "20"))
	float PositionLag = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (ClampMin = "0", UIMax = "20"))
	float RotationLag = 4.f;

	/** Keyable: 0 = rig off (camera stays put), 1 = following. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	/** Store where the camera is now, relative to the target, as the offset to hold. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Follow")
	void CaptureOffset();

	/**
	 * Keep the camera out of walls, like a spring arm: when the spot ahead of (or behind) the subject is
	 * blocked, swing around them a little, or pull in in front of the blocker.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow")
	bool bAvoidWalls = true;

	/** Never pull in closer to the subject's eyes than this (cm); swing around instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (ClampMin = "20", EditCondition = "bAvoidWalls"))
	float MinDistance = 90.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FTransform GetTargetFrame() const;
	/** Where the camera can actually go: Desired, swung or pulled in so it sees the subject. */
	FTransform ResolveWalls(const FTransform& Desired);

	/** The swing that worked last time (kept while it still works, so the camera doesn't hunt). */
	float CurrentSwing = 0.f;
};
