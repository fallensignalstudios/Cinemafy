#include "CinemafyCameraMoveComponent.h"
#include "CineCameraComponent.h"

namespace
{
	constexpr float DefaultPivotDistance = 300.f;
}

float UCinemafyCameraMoveComponent::ApplyEase(ECinemafyEase InEase, float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.f, 1.f);
	switch (InEase)
	{
	case ECinemafyEase::EaseInOut:
		return FMath::InterpEaseInOut(0.f, 1.f, T, 2.f);

	case ECinemafyEase::OperatorSettle:
	{
		// A soft start, then an "ease out back" with a small overshoot that settles,
		// like an operator landing a move by hand.
		const float U = FMath::Pow(T, 1.3f) - 1.f;
		constexpr float C1 = 0.8f;
		constexpr float C3 = C1 + 1.f;
		return 1.f + C3 * U * U * U + C1 * U * U;
	}

	default:
		return T;
	}
}

void UCinemafyCameraMoveComponent::Play(float Duration, float Delay)
{
	bPlaying = true;
	// Negative elapsed time = waiting out the delay (Progress clamps to 0 meanwhile).
	PlayElapsed = -FMath::Max(Delay, 0.f);
	PlayDuration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	Progress = 0.f;
}

void UCinemafyCameraMoveComponent::Stop()
{
	bPlaying = false;
}

float UCinemafyCameraMoveComponent::ResolvePivotDistance()
{
	// Re-sample at the start of the move; hold it for the rest.
	if (Progress <= 0.f || CachedPivotDistance < 0.f)
	{
		CachedPivotDistance = PivotDistance;
		if (CachedPivotDistance <= 0.f)
		{
			const UCineCameraComponent* Cine = Cast<UCineCameraComponent>(GetCamera());
			CachedPivotDistance = (Cine && Cine->FocusSettings.FocusMethod == ECameraFocusMethod::Manual)
				? Cine->FocusSettings.ManualFocusDistance
				: DefaultPivotDistance;
		}
		CachedPivotDistance = FMath::Max(CachedPivotDistance, 10.f);
	}
	return CachedPivotDistance;
}

FTransform UCinemafyCameraMoveComponent::EvaluateOffset(float DeltaTime)
{
	if (bPlaying)
	{
		PlayElapsed += DeltaTime;
		Progress = FMath::Clamp(PlayElapsed / PlayDuration, 0.f, 1.f);
		bPlaying = Progress < 1.f;
	}

	const float A = ApplyEase(Ease, Progress);
	const float Pivot = ResolvePivotDistance();

	// Crash zoom: drive the lens, not the camera position.
	if (UCineCameraComponent* Cine = Cast<UCineCameraComponent>(GetCamera()))
	{
		if (Move == ECinemafyCameraMove::SnapZoomIn)
		{
			// Remember the lens at rest. If the zoom snapped back to 0 from mid-zoom, the lens is still
			// zoomed right now, so keep the remembered base instead of re-reading it.
			if (BaseFocalLength < 0.f || (Progress <= 0.f && LastZoomAlpha <= 0.f))
			{
				BaseFocalLength = Cine->CurrentFocalLength;
			}
			LastZoomAlpha = A;
			const float Target = BaseFocalLength * (1.f + Distance / 100.f);
			// Primes are locked to one focal length; open the range up for the zoom.
			Cine->LensSettings.MaxFocalLength = FMath::Max(Cine->LensSettings.MaxFocalLength, Target);
			Cine->SetCurrentFocalLength(FMath::Lerp(BaseFocalLength, Target, A));
			return FTransform::Identity;
		}
		if (BaseFocalLength > 0.f)
		{
			// Switched away from a zoom: put the lens back.
			Cine->SetCurrentFocalLength(BaseFocalLength);
			BaseFocalLength = -1.f;
		}
	}

	// Local camera space: X forward, Y right, Z up.
	switch (Move)
	{
	case ECinemafyCameraMove::PushIn:       return FTransform(FVector(Distance * A, 0.f, 0.f));
	case ECinemafyCameraMove::PullOut:      return FTransform(FVector(-Distance * A, 0.f, 0.f));
	case ECinemafyCameraMove::TruckLeft:    return FTransform(FVector(0.f, -Distance * A, 0.f));
	case ECinemafyCameraMove::TruckRight:   return FTransform(FVector(0.f, Distance * A, 0.f));
	case ECinemafyCameraMove::PedestalUp:   return FTransform(FVector(0.f, 0.f, Distance * A));
	case ECinemafyCameraMove::PedestalDown: return FTransform(FVector(0.f, 0.f, -Distance * A));

	case ECinemafyCameraMove::CraneUp:
	case ECinemafyCameraMove::CraneDown:
	{
		// Rise / drop while tilting to keep the pivot point framed.
		const float Height = (Move == ECinemafyCameraMove::CraneUp ? Distance : -Distance) * A;
		const float Tilt = -FMath::RadiansToDegrees(FMath::Atan2(Height, Pivot));
		return FTransform(FRotator(Tilt, 0.f, 0.f), FVector(0.f, 0.f, Height));
	}

	case ECinemafyCameraMove::OrbitLeft:
	case ECinemafyCameraMove::OrbitRight:
	{
		// Swing around a point Pivot cm ahead, turning to keep looking at it.
		// Positive yaw swings the camera to its left.
		const float Sign = (Move == ECinemafyCameraMove::OrbitLeft) ? 1.f : -1.f;
		const float Theta = FMath::DegreesToRadians(OrbitAngle * A * Sign);
		const FVector Location(Pivot - Pivot * FMath::Cos(Theta), -Pivot * FMath::Sin(Theta), 0.f);
		return FTransform(FRotator(0.f, FMath::RadiansToDegrees(Theta), 0.f), Location);
	}
	}

	return FTransform::Identity;
}
