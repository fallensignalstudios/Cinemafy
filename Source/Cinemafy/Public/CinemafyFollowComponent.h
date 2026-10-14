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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FTransform GetTargetFrame() const;
};
