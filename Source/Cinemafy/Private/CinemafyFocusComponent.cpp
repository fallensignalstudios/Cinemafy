#include "CinemafyFocusComponent.h"
#include "CinemafyFramingLibrary.h"
#include "CineCameraComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr float MinFocusDistance = 15.f;
}

UCinemafyFocusComponent::UCinemafyFocusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After animation, so we read this frame's eye bone positions.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bTickInEditor = true;
}

void UCinemafyFocusComponent::RackFocusTo(AActor* NewTarget, float Duration)
{
	if (!NewTarget || NewTarget == FocusTarget)
	{
		return;
	}

	if (!FocusTarget || Duration <= 0.f)
	{
		FocusTarget = NewTarget;
		RackTarget = nullptr;
		RackAlpha = 0.f;
		bRacking = false;
		return;
	}

	RackTarget = NewTarget;
	RackAlpha = 0.f;
	RackElapsed = 0.f;
	RackDuration = Duration;
	bRacking = true;
}

float UCinemafyFocusComponent::GetPlaneDistance(const AActor* Target, const UCineCameraComponent* Camera) const
{
	FVector Point;
	if (bFocusOnEyes)
	{
		Point = UCinemafyFramingLibrary::GetEyePoint(Target);
	}
	else
	{
		FVector Extent;
		Target->GetActorBounds(true, Point, Extent);
	}

	// Focus distance is measured to the focal plane, not along a straight line to the point.
	const float Dist = FVector::DotProduct(Point - Camera->GetComponentLocation(), Camera->GetForwardVector());
	return FMath::Max(Dist, MinFocusDistance);
}

void UCinemafyFocusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UCineCameraComponent* Camera = GetOwner() ? GetOwner()->FindComponentByClass<UCineCameraComponent>() : nullptr;
	if (!Camera || !FocusTarget)
	{
		return;
	}

	if (bRacking)
	{
		RackElapsed += DeltaTime;
		const float Linear = FMath::Clamp(RackElapsed / FMath::Max(RackDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		RackAlpha = FMath::InterpEaseInOut(0.f, 1.f, Linear, 2.f);
		if (Linear >= 1.f)
		{
			FocusTarget = RackTarget;
			RackTarget = nullptr;
			RackAlpha = 0.f;
			bRacking = false;
		}
	}

	if (!FocusTarget)
	{
		return;
	}

	float Desired = GetPlaneDistance(FocusTarget, Camera);
	if (RackTarget && RackAlpha > 0.f)
	{
		// Blend in diopters (1 / distance), like a real follow-focus: the pull feels even across near and far.
		const float To = GetPlaneDistance(RackTarget, Camera);
		Desired = 1.f / FMath::Lerp(1.f / Desired, 1.f / To, RackAlpha);
	}
	Desired = FMath::Max(Desired + FocusOffset, MinFocusDistance);

	CurrentDistance = (SmoothingSpeed > 0.f && CurrentDistance > 0.f)
		? FMath::FInterpTo(CurrentDistance, Desired, DeltaTime, SmoothingSpeed)
		: Desired;

	FCameraFocusSettings Focus = Camera->FocusSettings;
	Focus.FocusMethod = ECameraFocusMethod::Manual;
	Focus.ManualFocusDistance = CurrentDistance;
	Focus.bSmoothFocusChanges = false;
	Camera->FocusSettings = Focus;
}
