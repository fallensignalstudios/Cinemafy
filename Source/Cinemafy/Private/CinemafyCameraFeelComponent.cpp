#include "CinemafyCameraFeelComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
	/** Two octaves of Perlin noise: smooth wander with a little texture on top. Roughly in [-1, 1]. */
	float OrganicNoise(float Time, float Frequency, float Offset)
	{
		const float X = Time * Frequency + Offset;
		return FMath::PerlinNoise1D(X) * 0.7f + FMath::PerlinNoise1D(X * 2.13f + 17.3f) * 0.3f;
	}
}

UCinemafyCameraFeelComponent::UCinemafyCameraFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bTickInEditor = true;
	CustomSettings = GetPresetSettings(ECinemafyCameraFeel::Shoulder);
}

FCinemafyFeelSettings UCinemafyCameraFeelComponent::GetPresetSettings(ECinemafyCameraFeel InFeel)
{
	FCinemafyFeelSettings S;
	switch (InFeel)
	{
	case ECinemafyCameraFeel::Tripod:
		S.RotationAmplitude = FVector(0.04, 0.04, 0.0);
		S.RotationFrequency = 0.3f;
		break;
	case ECinemafyCameraFeel::Shoulder:
		S.RotationAmplitude = FVector(0.35, 0.3, 0.25);
		S.RotationFrequency = 0.6f;
		S.LocationAmplitude = FVector(0.3, 0.3, 0.4);
		S.LocationFrequency = 0.5f;
		S.BreathingAmplitude = 0.15f;
		S.BreathingRate = 0.25f;
		break;
	case ECinemafyCameraFeel::Handheld:
		S.RotationAmplitude = FVector(0.8, 0.7, 0.5);
		S.RotationFrequency = 0.9f;
		S.LocationAmplitude = FVector(0.8, 0.8, 1.0);
		S.LocationFrequency = 0.8f;
		S.BreathingAmplitude = 0.25f;
		S.BreathingRate = 0.28f;
		break;
	case ECinemafyCameraFeel::RunAndGun:
		S.RotationAmplitude = FVector(1.8, 1.6, 1.2);
		S.RotationFrequency = 1.8f;
		S.LocationAmplitude = FVector(2.0, 2.0, 2.5);
		S.LocationFrequency = 1.5f;
		S.BreathingAmplitude = 0.4f;
		S.BreathingRate = 0.4f;
		break;
	default:
		break;
	}
	return S;
}

void UCinemafyCameraFeelComponent::CopyPresetToCustom()
{
	if (Feel != ECinemafyCameraFeel::Custom)
	{
		Modify();
		CustomSettings = GetPresetSettings(Feel);
		Feel = ECinemafyCameraFeel::Custom;
	}
}

void UCinemafyCameraFeelComponent::OnRegister()
{
	Super::OnRegister();

	Camera = GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
	if (Camera.IsValid())
	{
		BaseRelativeTransform = Camera->GetRelativeTransform();
	}
}

void UCinemafyCameraFeelComponent::OnUnregister()
{
	RestoreBaseTransform();
	Camera.Reset();
	Super::OnUnregister();
}

void UCinemafyCameraFeelComponent::RestoreBaseTransform()
{
	if (Camera.IsValid())
	{
		Camera->SetRelativeTransform(BaseRelativeTransform);
	}
}

void UCinemafyCameraFeelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Camera.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const bool bIsEditorPreview = World && !World->IsGameWorld();
	if (bIsEditorPreview && !bPreviewInEditor)
	{
		RestoreBaseTransform();
		return;
	}

	ElapsedTime += DeltaTime;
	const float T = bUseTimeOverride ? TimeOverride : ElapsedTime;
	const FCinemafyFeelSettings S = (Feel == ECinemafyCameraFeel::Custom) ? CustomSettings : GetPresetSettings(Feel);
	const float K = FMath::Max(Intensity, 0.f);

	// Every axis samples the noise field at a different seeded offset so they never move in lockstep.
	const float O = static_cast<float>(Seed) * 31.7f;

	const float Breath = FMath::Sin(T * S.BreathingRate * UE_TWO_PI) * S.BreathingAmplitude;

	const FRotator RotOffset(
		(OrganicNoise(T, S.RotationFrequency, O + 0.f) * S.RotationAmplitude.X + Breath) * K,
		OrganicNoise(T, S.RotationFrequency, O + 101.f) * S.RotationAmplitude.Y * K,
		OrganicNoise(T, S.RotationFrequency, O + 211.f) * S.RotationAmplitude.Z * K);

	const FVector LocOffset(
		OrganicNoise(T, S.LocationFrequency, O + 307.f) * S.LocationAmplitude.X * K,
		OrganicNoise(T, S.LocationFrequency, O + 401.f) * S.LocationAmplitude.Y * K,
		OrganicNoise(T, S.LocationFrequency, O + 503.f) * S.LocationAmplitude.Z * K + Breath * 0.5f * K);

	const FTransform Offset(RotOffset, LocOffset);
	Camera->SetRelativeTransform(Offset * BaseRelativeTransform);
}
