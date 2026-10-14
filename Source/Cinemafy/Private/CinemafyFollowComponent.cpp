#include "CinemafyFollowComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/** Bigger steps than this are scrubs or cuts: snap instead of lagging across them. */
	constexpr float MaxLagDeltaTime = 0.2f;
}

UCinemafyFollowComponent::UCinemafyFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the character has moved and animated this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bTickInEditor = true;
}

FTransform UCinemafyFollowComponent::GetTargetFrame() const
{
	// Heading only: walking characters bob and lean, the camera shouldn't.
	return FTransform(FRotator(0.f, Target->GetActorRotation().Yaw, 0.f), Target->GetActorLocation());
}

void UCinemafyFollowComponent::CaptureOffset()
{
	if (Target && GetOwner())
	{
		Offset = GetOwner()->GetActorTransform().GetRelativeTransform(GetTargetFrame());
	}
}

void UCinemafyFollowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Target || Weight <= 0.f)
	{
		return;
	}

	const FTransform Desired = Offset * GetTargetFrame();
	const FTransform Current = Owner->GetActorTransform();
	const bool bSnap = DeltaTime <= 0.f || DeltaTime > MaxLagDeltaTime;

	FVector Location = bSnap ? Desired.GetLocation() : FMath::VInterpTo(Current.GetLocation(), Desired.GetLocation(), DeltaTime, PositionLag);
	FRotator Rotation = bSnap ? Desired.Rotator() : FMath::RInterpTo(Current.Rotator(), Desired.Rotator(), DeltaTime, RotationLag);

	const float W = FMath::Clamp(Weight, 0.f, 1.f);
	Location = FMath::Lerp(Current.GetLocation(), Location, W);
	Rotation = FQuat::Slerp(Current.GetRotation(), Rotation.Quaternion(), W).Rotator();
	Owner->SetActorLocationAndRotation(Location, Rotation);
}
