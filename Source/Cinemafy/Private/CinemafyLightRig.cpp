#include "CinemafyLightRig.h"
#include "CinemafyFramingLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/**
	 * Angles are in degrees around the subject. Azimuth 0 = right next to the camera, 90 = side-on,
	 * 180 = straight behind. Elevation is above (+) or below (-) eye level.
	 */
	struct FStyleParams
	{
		float KeyAzimuth;
		float KeyElevation;
		float KeyMultiplier;
		/** Key-to-fill ratio. 0 = no fill. */
		float FillRatio;
		float FillAzimuth;
		float FillElevation;
		float RimAzimuth;
		float RimElevation;
		float RimMultiplier;
		/** 0 = hard, 1 = soft. */
		float Softness;
		float KeyTemperature;
		float FillTemperature;
		float RimTemperature;
	};

	FStyleParams GetStyleParams(ECinemafyLightingStyle Style)
	{
		switch (Style)
		{
		case ECinemafyLightingStyle::ThreePoint: return { 35.f, 25.f, 1.f,    2.f, 40.f,  10.f, 145.f, 35.f, 1.0f, 0.7f,  5400.f, 5800.f, 6500.f };
		case ECinemafyLightingStyle::Loop:       return { 30.f, 30.f, 1.f,    3.f, 40.f,   5.f, 150.f, 35.f, 0.8f, 0.6f,  5000.f, 5600.f, 6500.f };
		case ECinemafyLightingStyle::Rembrandt:  return { 50.f, 40.f, 1.f,    5.f, 45.f,   0.f, 150.f, 35.f, 0.7f, 0.5f,  4600.f, 5600.f, 6800.f };
		// Butterfly fills from below, like a bounce board under the chin.
		case ECinemafyLightingStyle::Butterfly:  return {  0.f, 45.f, 1.f,  2.5f,  0.f, -20.f, 165.f, 30.f, 0.8f, 0.7f,  5400.f, 5400.f, 6000.f };
		case ECinemafyLightingStyle::Split:      return { 90.f, 10.f, 1.f,   10.f, 60.f,   0.f, 160.f, 30.f, 0.6f, 0.4f,  5000.f, 5600.f, 6500.f };
		case ECinemafyLightingStyle::Noir:       return { 60.f, 50.f, 1.f,   15.f, 40.f,   0.f, 150.f, 40.f, 0.6f, 0.08f, 4800.f, 5000.f, 5600.f };
		case ECinemafyLightingStyle::Silhouette: return { 35.f, 25.f, 0.05f, 0.f,  0.f,   0.f, 160.f, 25.f, 3.0f, 0.4f,  5600.f, 5600.f, 7000.f };
		}
		return GetStyleParams(ECinemafyLightingStyle::Loop);
	}

	/** Direction from the subject to a light. Sign picks camera-right (+1) or camera-left (-1). */
	FVector LightDirection(const FVector& ToCamera, const FVector& CameraRight, float AzimuthDeg, float ElevationDeg, float Sign)
	{
		const float Az = FMath::DegreesToRadians(AzimuthDeg);
		const float El = FMath::DegreesToRadians(ElevationDeg);
		const FVector Flat = ToCamera * FMath::Cos(Az) + CameraRight * (Sign * FMath::Sin(Az));
		return (Flat * FMath::Cos(El) + FVector::UpVector * FMath::Sin(El)).GetSafeNormal();
	}

	/** Candela needed to put Lux on a face DistanceCm away (inverse square law). */
	float LuxToCandela(float Lux, float DistanceCm)
	{
		const float Meters = DistanceCm / 100.f;
		return Lux * Meters * Meters;
	}

	/** Constructor-time defaults. Set directly: light setters can no-op before the component is registered. */
	void SetupLight(ULocalLightComponent* Light, bool bCastShadows)
	{
		// The rig places every light in world space itself.
		Light->SetUsingAbsoluteLocation(true);
		Light->SetUsingAbsoluteRotation(true);
		Light->Mobility = EComponentMobility::Movable;
		Light->IntensityUnits = ELightUnits::Candelas;
		Light->bUseTemperature = true;
		Light->CastShadows = bCastShadows;
	}

	void PlaceLight(ULocalLightComponent* Light, const FVector& Eye, const FVector& Direction, float Distance)
	{
		const FVector Location = Eye + Direction * Distance;
		Light->SetWorldLocationAndRotation(Location, (Eye - Location).Rotation());
		Light->SetAttenuationRadius(Distance * 3.f);
	}
}

ACinemafyLightRig::ACinemafyLightRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	KeyLight = CreateDefaultSubobject<URectLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	SetupLight(KeyLight, true);

	FillLight = CreateDefaultSubobject<URectLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	SetupLight(FillLight, false);

	RimLight = CreateDefaultSubobject<URectLightComponent>(TEXT("RimLight"));
	RimLight->SetupAttachment(Root);
	SetupLight(RimLight, false);

	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(Root);
	SetupLight(EyeLight, false);
	EyeLight->SourceRadius = 2.f;
}

void ACinemafyLightRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCharacterChannels();
	SolveRig();
}

void ACinemafyLightRig::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterChannels();
	SolveRig();
}

void ACinemafyLightRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bLiveUpdate)
	{
		SolveRig();
	}
}

void ACinemafyLightRig::ApplyCharacterChannels()
{
	const bool bChannel0 = !bCharacterOnly;
	const bool bChannel1 = bCharacterOnly;
	ULocalLightComponent* const Lights[] = { KeyLight.Get(), FillLight.Get(), RimLight.Get(), EyeLight.Get() };
	for (ULocalLightComponent* Light : Lights)
	{
		if (Light)
		{
			Light->SetLightingChannels(bChannel0, bChannel1, false);
		}
	}

	if (!bCharacterOnly || !Subject)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(Subject.Get());
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive->LightingChannels.bChannel1)
		{
			// Keep whatever world lighting the character already had; just let the rig reach it too.
			Primitive->SetLightingChannels(Primitive->LightingChannels.bChannel0, true, Primitive->LightingChannels.bChannel2);
		}
	}
}

FVector ACinemafyLightRig::ResolveCameraLocation(const FVector& Eye) const
{
	if (TargetCamera)
	{
		if (const UCameraComponent* Camera = TargetCamera->FindComponentByClass<UCameraComponent>())
		{
			return Camera->GetComponentLocation();
		}
		return TargetCamera->GetActorLocation();
	}

	const UWorld* World = GetWorld();
	if (bFollowActiveCamera && World && World->IsGameWorld())
	{
		if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			return CameraManager->GetCameraLocation();
		}
	}

	// No camera to design for: light for someone standing in front of the character.
	return Eye + Subject->GetActorForwardVector() * 300.f;
}

void ACinemafyLightRig::SolveRig()
{
	if (!Subject || !KeyLight || !FillLight || !RimLight || !EyeLight)
	{
		return;
	}

	const FStyleParams P = GetStyleParams(Style);
	const FVector Eye = UCinemafyFramingLibrary::GetEyePoint(Subject);

	FVector ToCamera = (ResolveCameraLocation(Eye) - Eye).GetSafeNormal2D();
	if (ToCamera.IsNearlyZero())
	{
		ToCamera = Subject->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector CameraRight = FVector::CrossProduct(FVector::UpVector, -ToCamera);

	// Which side of the frame the key lives on.
	float KeySign = 1.f;
	switch (KeySide)
	{
	case ECinemafyKeySide::CameraLeft:  KeySign = -1.f; break;
	case ECinemafyKeySide::CameraRight: KeySign = 1.f; break;
	default:
	{
		// The side the face is turned toward is the "short" side (the one turned away from camera).
		const float FacingLateral = FVector::DotProduct(Subject->GetActorForwardVector(), CameraRight);
		const float ShortSign = FacingLateral >= 0.f ? 1.f : -1.f;
		KeySign = (KeySide == ECinemafyKeySide::ShortSide) ? ShortSign : -ShortSign;
		break;
	}
	}

	const float Master = FMath::Max(Intensity, 0.f);
	const float Dist = LightDistance;
	const float Soft = FMath::Clamp(P.Softness * Softness, 0.f, 1.f);

	// Key
	const float KeyLux = KeyIlluminance * P.KeyMultiplier * Master;
	PlaceLight(KeyLight, Eye, LightDirection(ToCamera, CameraRight, P.KeyAzimuth, P.KeyElevation, KeySign), Dist);
	KeyLight->SetIntensity(LuxToCandela(KeyLux, Dist));
	const float KeySize = FMath::Lerp(15.f, 160.f, Soft);
	KeyLight->SetSourceWidth(KeySize);
	KeyLight->SetSourceHeight(KeySize);
	KeyLight->SetTemperature(P.KeyTemperature + TemperatureShift);

	// Fill: opposite side, bigger and softer.
	const bool bHasFill = P.FillRatio > 0.f;
	FillLight->SetVisibility(bHasFill);
	if (bHasFill)
	{
		const float FillLux = KeyIlluminance * Master / (P.FillRatio * FMath::Max(Contrast, 0.1f));
		PlaceLight(FillLight, Eye, LightDirection(ToCamera, CameraRight, P.FillAzimuth, P.FillElevation, -KeySign), Dist);
		FillLight->SetIntensity(LuxToCandela(FillLux, Dist));
		const float FillSize = FMath::Lerp(60.f, 220.f, FMath::Clamp(Soft + 0.3f, 0.f, 1.f));
		FillLight->SetSourceWidth(FillSize);
		FillLight->SetSourceHeight(FillSize);
		FillLight->SetTemperature(P.FillTemperature + TemperatureShift);
	}

	// Rim: behind, opposite the key, as a tall strip to trace the silhouette. Grazing angles need extra punch.
	const float RimLux = KeyIlluminance * P.RimMultiplier * RimStrength * Master * 1.5f;
	PlaceLight(RimLight, Eye, LightDirection(ToCamera, CameraRight, P.RimAzimuth, P.RimElevation, -KeySign), Dist);
	RimLight->SetIntensity(LuxToCandela(RimLux, Dist));
	RimLight->SetSourceWidth(30.f);
	RimLight->SetSourceHeight(120.f);
	RimLight->SetTemperature(P.RimTemperature + TemperatureShift);

	// Eye light: small, dim and right above the lens, so it shows up as a catchlight without visibly lighting the face.
	constexpr float EyeLightDistance = 90.f;
	const FVector EyeLightLocation = Eye + ToCamera * EyeLightDistance + FVector::UpVector * 12.f;
	EyeLight->SetWorldLocation(EyeLightLocation);
	EyeLight->SetAttenuationRadius(EyeLightDistance * 2.f);
	EyeLight->SetIntensity(LuxToCandela(KeyIlluminance * 0.06f * EyeLightStrength * Master, EyeLightDistance));
	EyeLight->SetTemperature(P.KeyTemperature + TemperatureShift);
}
