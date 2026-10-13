#include "CinemafyCameraLibrary.h"
#include "CinemafyLookPreset.h"
#include "CineCameraComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	struct FLookParams
	{
		float Saturation;
		float Contrast;
		float WhiteTemp;
		FLinearColor ShadowGain;
		FLinearColor HighlightGain;
		float Grain;
		float Vignette;
		float Fringe;
		float Bloom;
	};

	const FLookParams NeutralLook = { 1.f, 1.f, 6500.f, FLinearColor(1.f, 1.f, 1.f), FLinearColor(1.f, 1.f, 1.f), 0.f, 0.4f, 0.f, 0.675f };

	FLookParams GetLookParams(ECinemafyLook Look)
	{
		switch (Look)
		{
		case ECinemafyLook::CleanCinema:
			return { 0.95f, 1.05f, 6400.f, FLinearColor(0.99f, 1.f, 1.02f), FLinearColor(1.02f, 1.f, 0.98f), 0.08f, 0.45f, 0.15f, 0.6f };
		case ECinemafyLook::WarmEpic:
			return { 1.05f, 1.10f, 5600.f, FLinearColor(1.f, 0.98f, 0.97f), FLinearColor(1.06f, 1.f, 0.92f), 0.12f, 0.55f, 0.2f, 0.9f };
		case ECinemafyLook::TealOrange:
			return { 1.08f, 1.12f, 6300.f, FLinearColor(0.9f, 1.f, 1.08f), FLinearColor(1.08f, 1.f, 0.9f), 0.10f, 0.5f, 0.2f, 0.7f };
		case ECinemafyLook::BleachBypass:
			return { 0.55f, 1.30f, 6500.f, FLinearColor(1.f, 1.f, 1.f), FLinearColor(1.f, 1.f, 0.98f), 0.20f, 0.6f, 0.1f, 0.5f };
		case ECinemafyLook::Noir:
			return { 0.0f, 1.35f, 6500.f, FLinearColor(1.f, 1.f, 1.f), FLinearColor(1.f, 1.f, 1.f), 0.30f, 0.7f, 0.f, 0.4f };
		case ECinemafyLook::ColdSteel:
			return { 0.85f, 1.15f, 7800.f, FLinearColor(0.94f, 1.f, 1.06f), FLinearColor(0.98f, 1.f, 1.04f), 0.12f, 0.55f, 0.15f, 0.6f };
		}
		return NeutralLook;
	}

	FVector4 ToGain(const FLinearColor& A, const FLinearColor& B, float Alpha)
	{
		const FLinearColor C = FMath::Lerp(A, B, Alpha);
		return FVector4(C.R, C.G, C.B, 1.f);
	}
}

UCineCameraComponent* UCinemafyCameraLibrary::FindCineCamera(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UCineCameraComponent>() : nullptr;
}

float UCinemafyCameraLibrary::GetPrimeFocalLength(ECinemafyPrime Prime)
{
	switch (Prime)
	{
	case ECinemafyPrime::Prime18:  return 18.f;
	case ECinemafyPrime::Prime24:  return 24.f;
	case ECinemafyPrime::Prime35:  return 35.f;
	case ECinemafyPrime::Prime50:  return 50.f;
	case ECinemafyPrime::Prime85:  return 85.f;
	case ECinemafyPrime::Prime135: return 135.f;
	}
	return 50.f;
}

ECinemafyPrime UCinemafyCameraLibrary::SnapToPrime(float FocalLengthMM)
{
	const ECinemafyPrime Primes[] = { ECinemafyPrime::Prime18, ECinemafyPrime::Prime24, ECinemafyPrime::Prime35,
		ECinemafyPrime::Prime50, ECinemafyPrime::Prime85, ECinemafyPrime::Prime135 };

	const float Target = FMath::Loge(FMath::Max(FocalLengthMM, 1.f));
	ECinemafyPrime Best = ECinemafyPrime::Prime50;
	float BestError = TNumericLimits<float>::Max();
	for (ECinemafyPrime Prime : Primes)
	{
		const float Error = FMath::Abs(FMath::Loge(GetPrimeFocalLength(Prime)) - Target);
		if (Error < BestError)
		{
			BestError = Error;
			Best = Prime;
		}
	}
	return Best;
}

FVector2D UCinemafyCameraLibrary::GetSensorSize(ECinemafySensor Sensor)
{
	switch (Sensor)
	{
	case ECinemafySensor::Super35_16x9:   return FVector2D(24.89, 14.0);
	case ECinemafySensor::Super35_4x3:    return FVector2D(24.89, 18.66);
	case ECinemafySensor::FullFrame_16x9: return FVector2D(36.0, 20.25);
	}
	return FVector2D(24.89, 14.0);
}

void UCinemafyCameraLibrary::ApplyPrime(UCineCameraComponent* Camera, ECinemafyPrime Prime, float FStop)
{
	if (!Camera)
	{
		return;
	}

	const float Focal = GetPrimeFocalLength(Prime);

	FCameraLensSettings Lens = Camera->LensSettings;
	Lens.MinFocalLength = Focal;
	Lens.MaxFocalLength = Focal;
	Lens.MinFStop = 1.4f;
	Lens.MaxFStop = 22.f;
	// Lots of blades = round, creamy bokeh like modern cinema glass.
	Lens.DiaphragmBladeCount = 11;
	Camera->LensSettings = Lens;

	Camera->SetCurrentFocalLength(Focal);
	Camera->SetCurrentAperture(FMath::Clamp(FStop, Lens.MinFStop, Lens.MaxFStop));
}

void UCinemafyCameraLibrary::ApplySensor(UCineCameraComponent* Camera, ECinemafySensor Sensor)
{
	if (!Camera)
	{
		return;
	}

	const FVector2D Size = GetSensorSize(Sensor);
	Camera->Filmback.SensorWidth = Size.X;
	Camera->Filmback.SensorHeight = Size.Y;
	// Re-setting the focal length makes the camera recompute its derived FOV / aspect data.
	Camera->SetCurrentFocalLength(Camera->CurrentFocalLength);
}

void UCinemafyCameraLibrary::SetLetterbox(UCineCameraComponent* Camera, float AspectRatio)
{
	if (!Camera)
	{
		return;
	}

	if (AspectRatio > 0.f)
	{
		Camera->Filmback.SensorHeight = Camera->Filmback.SensorWidth / AspectRatio;
		Camera->SetConstraintAspectRatio(true);
	}
	else
	{
		Camera->Filmback.SensorHeight = Camera->Filmback.SensorWidth * 9.f / 16.f;
		Camera->SetConstraintAspectRatio(false);
	}
	Camera->SetCurrentFocalLength(Camera->CurrentFocalLength);
}

void UCinemafyCameraLibrary::ApplyLookToSettings(FPostProcessSettings& Settings, ECinemafyLook Look, float Strength)
{
	const FLookParams L = GetLookParams(Look);
	const FLookParams& N = NeutralLook;
	const float S = FMath::Clamp(Strength, 0.f, 2.f);

	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(1.f, 1.f, 1.f, FMath::Lerp(N.Saturation, L.Saturation, S));

	Settings.bOverride_ColorContrast = true;
	Settings.ColorContrast = FVector4(1.f, 1.f, 1.f, FMath::Lerp(N.Contrast, L.Contrast, S));

	Settings.bOverride_WhiteTemp = true;
	Settings.WhiteTemp = FMath::Lerp(N.WhiteTemp, L.WhiteTemp, S);

	Settings.bOverride_ColorGainShadows = true;
	Settings.ColorGainShadows = ToGain(N.ShadowGain, L.ShadowGain, S);

	Settings.bOverride_ColorGainHighlights = true;
	Settings.ColorGainHighlights = ToGain(N.HighlightGain, L.HighlightGain, S);

	Settings.bOverride_FilmGrainIntensity = true;
	Settings.FilmGrainIntensity = FMath::Lerp(N.Grain, L.Grain, S);

	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = FMath::Lerp(N.Vignette, L.Vignette, S);

	Settings.bOverride_SceneFringeIntensity = true;
	Settings.SceneFringeIntensity = FMath::Lerp(N.Fringe, L.Fringe, S);

	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = FMath::Lerp(N.Bloom, L.Bloom, S);
}

FPostProcessSettings UCinemafyCameraLibrary::MakeLookSettings(ECinemafyLook Look, float Strength)
{
	FPostProcessSettings Settings;
	ApplyLookToSettings(Settings, Look, Strength);
	return Settings;
}

void UCinemafyCameraLibrary::ApplyLook(UCineCameraComponent* Camera, ECinemafyLook Look, float Strength)
{
	if (!Camera)
	{
		return;
	}

	ApplyLookToSettings(Camera->PostProcessSettings, Look, Strength);
	Camera->PostProcessBlendWeight = 1.f;
}

void UCinemafyCameraLibrary::ApplyLookPreset(UCineCameraComponent* Camera, const UCinemafyLookPreset* Preset)
{
	if (!Camera || !Preset)
	{
		return;
	}

	Camera->PostProcessSettings = Preset->Settings;
	Camera->PostProcessBlendWeight = 1.f;

	if (Preset->LetterboxAspectRatio > 0.f)
	{
		SetLetterbox(Camera, Preset->LetterboxAspectRatio);
	}
}
