#pragma once

#include "CoreMinimal.h"
#include "CinemafyCameraOffsetComponent.h"
#include "CinemafyTypes.h"
#include "CinemafyCameraMoveComponent.generated.h"

/**
 * One-knob operator moves. Pick a move and a size, then key Progress 0 -> 1 across the shot in
 * Sequencer (a straight linear key is fine: the easing is built in). For gameplay, call Play().
 *
 * Moves are relative to wherever the camera actor already is, so you can still key or rail
 * the actor and the move rides on top. Camera Feel shake rides on top of the move.
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Camera Move"))
class CINEMAFY_API UCinemafyCameraMoveComponent : public UCinemafyCameraOffsetComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Move")
	ECinemafyCameraMove Move = ECinemafyCameraMove::PushIn;

	/** Travel distance in cm (push, truck, pedestal, crane). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Move", meta = (ClampMin = "0", UIMax = "1000"))
	float Distance = 80.f;

	/** Orbit sweep in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Move", meta = (ClampMin = "0", UIMax = "180"))
	float OrbitAngle = 20.f;

	/**
	 * Distance in cm to the point the camera orbits around / holds aim on while craning.
	 * 0 = use the camera's current focus distance (perfect with Cinemafy Eye Focus).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Move", meta = (ClampMin = "0"))
	float PivotDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Move")
	ECinemafyEase Ease = ECinemafyEase::OperatorSettle;

	/** 0 = start of the move, 1 = end. Key this in Sequencer. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Camera Move", meta = (ClampMin = "0", ClampMax = "1"))
	float Progress = 0.f;

	/** Plays the move over Duration seconds, optionally after a delay (for gameplay-triggered moments). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Camera Move")
	void Play(float Duration = 3.f, float Delay = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Camera Move")
	void Stop();

	/** The eased curve, exposed so tools and UI can draw it. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Camera Move")
	static float ApplyEase(ECinemafyEase InEase, float Alpha);

	virtual int32 GetStackOrder() const override { return 0; }

protected:
	virtual FTransform EvaluateOffset(float DeltaTime) override;

private:
	float ResolvePivotDistance();

	bool bPlaying = false;
	float PlayElapsed = 0.f;
	float PlayDuration = 3.f;

	/** Pivot distance captured at the start of the move so focus changes mid-move don't wobble the path. */
	float CachedPivotDistance = -1.f;
};
