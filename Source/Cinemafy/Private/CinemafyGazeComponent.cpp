#include "CinemafyGazeComponent.h"
#include "CinemafyFramingLibrary.h"
#include "GameFramework/Actor.h"

namespace
{
	/** Fraction of a blink spent closing; the rest is the slower re-open. */
	constexpr float BlinkClosePortion = 0.35f;

	FRotator SlerpRot(const FRotator& A, const FRotator& B, float Alpha)
	{
		return FQuat::Slerp(A.Quaternion(), B.Quaternion(), Alpha).Rotator();
	}
}

UCinemafyGazeComponent::UCinemafyGazeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Before animation evaluates, so the Anim Blueprint reads this frame's values.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bTickInEditor = true;
}

void UCinemafyGazeComponent::OnRegister()
{
	Super::OnRegister();

	Random.Initialize(Seed);
	LocalTime = 0.f;
	bInitialized = false;
	NextSaccadeTime = 0.f;
	NextBlinkTime = Random.FRandRange(BlinkInterval.X, BlinkInterval.Y);
	BlinkStartTime = -100.f;
	PendingDoubleBlinkTime = -1.f;
}

void UCinemafyGazeComponent::SetLookAtTarget(AActor* NewTarget, FVector Offset)
{
	LookAtTarget = NewTarget;
	LookAtOffset = Offset;
}

void UCinemafyGazeComponent::Blink()
{
	// Don't restart a blink that's already in progress.
	if (LocalTime - BlinkStartTime < BlinkDuration)
	{
		return;
	}

	BlinkStartTime = LocalTime;
	NextBlinkTime = LocalTime + Random.FRandRange(BlinkInterval.X, BlinkInterval.Y);
	if (Random.FRand() < DoubleBlinkChance)
	{
		PendingDoubleBlinkTime = LocalTime + BlinkDuration + 0.08f;
	}
}

FVector UCinemafyGazeComponent::ResolveLookPoint(const FVector& EyeOrigin) const
{
	if (LookAtTarget)
	{
		return UCinemafyFramingLibrary::GetEyePoint(LookAtTarget) + LookAtOffset;
	}
	return EyeOrigin + GetOwner()->GetActorForwardVector() * 300.f + LookAtOffset;
}

void UCinemafyGazeComponent::UpdateBlink()
{
	if (bAutoBlink && LocalTime >= NextBlinkTime)
	{
		Blink();
	}

	if (PendingDoubleBlinkTime > 0.f && LocalTime >= PendingDoubleBlinkTime)
	{
		PendingDoubleBlinkTime = -1.f;
		BlinkStartTime = LocalTime;
	}

	const float T = (LocalTime - BlinkStartTime) / BlinkDuration;
	if (T < 0.f || T >= 1.f)
	{
		BlinkAlpha = 0.f;
	}
	else if (T < BlinkClosePortion)
	{
		// Lids snap shut...
		BlinkAlpha = FMath::InterpEaseIn(0.f, 1.f, T / BlinkClosePortion, 2.f);
	}
	else
	{
		// ...and drift back open.
		BlinkAlpha = FMath::InterpEaseOut(1.f, 0.f, (T - BlinkClosePortion) / (1.f - BlinkClosePortion), 2.f);
	}
}

void UCinemafyGazeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	LocalTime += DeltaTime;

	const FVector EyeOrigin = UCinemafyFramingLibrary::GetEyePoint(Owner);
	const FVector LookPoint = ResolveLookPoint(EyeOrigin);
	const FVector ToLook = LookPoint - EyeOrigin;
	const FVector LookDir = ToLook.GetSafeNormal();
	const float LookDistance = FMath::Max(ToLook.Size(), 50.f);
	const FRotator BodyRot = Owner->GetActorRotation();
	const FRotator TargetRot = LookDir.Rotation();

	if (!bInitialized)
	{
		CurrentEyeRot = TargetRot;
		CurrentHeadRot = BodyRot;
		LastLookDir = LookDir;
		bInitialized = true;
	}

	// Big gaze shifts tend to come with a blink.
	const float ShiftDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(LastLookDir, LookDir), -1.f, 1.f)));
	if (ShiftDeg > BlinkOnGazeShiftAngle && Random.FRand() < 0.6f)
	{
		Blink();
	}
	LastLookDir = LookDir;

	// Micro-saccades: the eyes never sit perfectly still.
	if (LocalTime >= NextSaccadeTime)
	{
		SaccadeOffset = FRotator(
			Random.FRandRange(-SaccadeAmplitude, SaccadeAmplitude) * 0.6f,
			Random.FRandRange(-SaccadeAmplitude, SaccadeAmplitude),
			0.f);
		NextSaccadeTime = LocalTime + Random.FRandRange(SaccadeInterval.X, SaccadeInterval.Y);
	}

	// Eyes are fast...
	const FRotator DesiredEye = (TargetRot + SaccadeOffset).GetNormalized();
	CurrentEyeRot = FMath::RInterpTo(CurrentEyeRot, DesiredEye, DeltaTime, EyeSpeed);

	// ...the head is lazy and only carries part of the turn.
	FRotator HeadDelta = (TargetRot - BodyRot).GetNormalized();
	HeadDelta.Yaw = FMath::Clamp(HeadDelta.Yaw * HeadFollowAmount, -MaxHeadYaw, MaxHeadYaw);
	HeadDelta.Pitch = FMath::Clamp(HeadDelta.Pitch * HeadFollowAmount, -MaxHeadPitch, MaxHeadPitch);
	HeadDelta.Roll = 0.f;
	CurrentHeadRot = FMath::RInterpTo(CurrentHeadRot, (BodyRot + HeadDelta).GetNormalized(), DeltaTime, HeadSpeed);

	UpdateBlink();

	// Outputs, blended toward rest by GazeWeight.
	const float W = FMath::Clamp(GazeWeight, 0.f, 1.f);
	const FRotator EyeOut = SlerpRot(BodyRot, CurrentEyeRot, W);
	const FRotator HeadOut = SlerpRot(BodyRot, CurrentHeadRot, W);

	EyeTargetLocation = EyeOrigin + EyeOut.Vector() * LookDistance;
	HeadLookAtLocation = EyeOrigin + HeadOut.Vector() * LookDistance;

	const FRotator EyeInHead = (EyeOut - HeadOut).GetNormalized();
	EyeLookNormalized = FVector2D(
		FMath::Clamp(EyeInHead.Yaw / FMath::Max(MaxEyeYaw, 1.f), -1.f, 1.f),
		FMath::Clamp(EyeInHead.Pitch / FMath::Max(MaxEyePitch, 1.f), -1.f, 1.f));
}
