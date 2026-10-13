#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyGazeComponent.generated.h"

/**
 * Living eyes for MetaHumans (or any character). Add this to the character, give it something to
 * look at, and it produces natural gaze: fast eyes with micro-saccades, a lazier head that only
 * partly follows, and blinks (including the blink people do on a big gaze shift).
 *
 * It doesn't pose bones itself. Read the outputs in your Anim Blueprint:
 *  - EyeLookNormalized  -> MetaHuman face eye controls (X = left/right, Y = up/down, -1..1)
 *  - HeadLookAtLocation -> a LookAt / Control Rig aim on the head and neck
 *  - BlinkAlpha         -> the blink curve / eyelid controls
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Gaze"))
class CINEMAFY_API UCinemafyGazeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyGazeComponent();

	// --- Target ---------------------------------------------------------------

	/** Who to look at. If it's a character, Cinemafy finds their eyes. Set from Sequencer events with SetLookAtTarget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze")
	TObjectPtr<AActor> LookAtTarget = nullptr;

	/** World-space offset added to the target point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze")
	FVector LookAtOffset = FVector::ZeroVector;

	/** 0 = gaze system off (rest pose), 1 = fully driving. Keyable in Sequencer. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Gaze", meta = (ClampMin = "0", ClampMax = "1"))
	float GazeWeight = 1.f;

	// --- Eyes -----------------------------------------------------------------

	/** Random interval between micro eye darts, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Eyes")
	FVector2D SaccadeInterval = FVector2D(0.4, 1.8);

	/** Size of micro eye darts, in degrees. ~1-2 reads as alive; more reads as nervous. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Eyes", meta = (ClampMin = "0", UIMax = "6"))
	float SaccadeAmplitude = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Eyes", meta = (ClampMin = "0"))
	float EyeSpeed = 30.f;

	/** Eye rotation (relative to head) that maps to +-1 in EyeLookNormalized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Eyes")
	float MaxEyeYaw = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Eyes")
	float MaxEyePitch = 25.f;

	// --- Head -----------------------------------------------------------------

	/** How much of the gaze the head carries (eyes do the rest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Head", meta = (ClampMin = "0", ClampMax = "1"))
	float HeadFollowAmount = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Head", meta = (ClampMin = "0"))
	float HeadSpeed = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Head")
	float MaxHeadYaw = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Head")
	float MaxHeadPitch = 35.f;

	// --- Blinks ---------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Blink")
	bool bAutoBlink = true;

	/** Random time between blinks, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Blink")
	FVector2D BlinkInterval = FVector2D(2.5, 6.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Blink", meta = (ClampMin = "0.05"))
	float BlinkDuration = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Blink", meta = (ClampMin = "0", ClampMax = "1"))
	float DoubleBlinkChance = 0.15f;

	/** A gaze shift bigger than this (degrees) usually comes with a blink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze|Blink")
	float BlinkOnGazeShiftAngle = 20.f;

	/** Same seed = same performance every playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze")
	int32 Seed = 0;

	// --- Outputs (read these in the Anim Blueprint) ---------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Gaze|Output")
	FVector EyeTargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Gaze|Output")
	FVector HeadLookAtLocation = FVector::ZeroVector;

	/** Eye direction relative to the head, -1..1. X = right, Y = up. */
	UPROPERTY(BlueprintReadOnly, Category = "Gaze|Output")
	FVector2D EyeLookNormalized = FVector2D::ZeroVector;

	/** 0 = eyes open, 1 = closed. */
	UPROPERTY(BlueprintReadOnly, Category = "Gaze|Output")
	float BlinkAlpha = 0.f;

	// --- Functions ------------------------------------------------------------

	/** Great as a Sequencer event: "now look at Voss". Nullptr = look ahead. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Gaze")
	void SetLookAtTarget(AActor* NewTarget, FVector Offset = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Gaze")
	void Blink();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;

private:
	FVector ResolveLookPoint(const FVector& EyeOrigin) const;
	void UpdateBlink();

	FRandomStream Random;
	float LocalTime = 0.f;
	bool bInitialized = false;

	FRotator CurrentEyeRot = FRotator::ZeroRotator;
	FRotator CurrentHeadRot = FRotator::ZeroRotator;
	FRotator SaccadeOffset = FRotator::ZeroRotator;
	FVector LastLookDir = FVector::ForwardVector;

	float NextSaccadeTime = 0.f;
	float NextBlinkTime = 0.f;
	float BlinkStartTime = -100.f;
	float PendingDoubleBlinkTime = -1.f;
};
