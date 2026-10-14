#include "CinemafyFollowComponent.h"
#include "GameFramework/Actor.h"
#include "CinemafyFramingLibrary.h"
#include "Engine/World.h"

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

	const FTransform Desired = bAvoidWalls ? ResolveWalls(Offset * GetTargetFrame()) : Offset * GetTargetFrame();
	const FTransform Current = Owner->GetActorTransform();
	const bool bSnap = DeltaTime <= 0.f || DeltaTime > MaxLagDeltaTime;

	FVector Location = bSnap ? Desired.GetLocation() : FMath::VInterpTo(Current.GetLocation(), Desired.GetLocation(), DeltaTime, PositionLag);
	FRotator Rotation = bSnap ? Desired.Rotator() : FMath::RInterpTo(Current.Rotator(), Desired.Rotator(), DeltaTime, RotationLag);

	const float W = FMath::Clamp(Weight, 0.f, 1.f);
	Location = FMath::Lerp(Current.GetLocation(), Location, W);
	Rotation = FQuat::Slerp(Current.GetRotation(), Rotation.Quaternion(), W).Rotator();
	Owner->SetActorLocationAndRotation(Location, Rotation);
}

FTransform UCinemafyFollowComponent::ResolveWalls(const FTransform& Desired)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Desired;
	}

	const FVector Eye = UCinemafyFramingLibrary::GetEyePoint(Target);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyFollowWalls), false, GetOwner());
	Params.AddIgnoredActor(Target);
	const FCollisionShape Probe = FCollisionShape::MakeSphere(12.f);

	// Swings the desired camera around the subject's eyes by Yaw degrees (position and aim together).
	auto Swung = [&Desired, &Eye](float Yaw)
	{
		const FRotator Turn(0.f, Yaw, 0.f);
		return FTransform(Turn.Quaternion() * Desired.GetRotation(), Eye + Turn.RotateVector(Desired.GetLocation() - Eye), Desired.GetScale3D());
	};
	auto IsClear = [&](const FTransform& Candidate, FHitResult* OutHit)
	{
		FHitResult Hit;
		const bool bBlocked = World->SweepSingleByChannel(Hit, Eye, Candidate.GetLocation(), FQuat::Identity, ECC_Camera, Probe, Params);
		if (OutHit)
		{
			*OutHit = Hit;
		}
		return !bBlocked;
	};

	// Keep the last working swing while it still works; otherwise try the intended spot first.
	if (CurrentSwing != 0.f && IsClear(Swung(CurrentSwing), nullptr))
	{
		return Swung(CurrentSwing);
	}
	FHitResult Hit;
	if (IsClear(Desired, &Hit))
	{
		CurrentSwing = 0.f;
		return Desired;
	}

	// Pulling in keeps the angle; fine as long as it doesn't end up in the actor's face.
	const FVector PulledIn = Hit.Location;
	if (FVector::Dist(Eye, PulledIn) >= MinDistance)
	{
		CurrentSwing = 0.f;
		return FTransform(Desired.GetRotation(), PulledIn, Desired.GetScale3D());
	}

	// Swing around the subject for a clear angle, smallest change first.
	for (const float Yaw : { 30.f, -30.f, 60.f, -60.f, 90.f, -90.f })
	{
		const FTransform Candidate = Swung(Yaw);
		if (IsClear(Candidate, nullptr))
		{
			CurrentSwing = Yaw;
			return Candidate;
		}
	}

	// Nowhere clear: the closest unblocked spot on the intended line.
	return FTransform(Desired.GetRotation(), PulledIn, Desired.GetScale3D());
}
