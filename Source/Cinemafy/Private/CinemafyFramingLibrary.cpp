#include "CinemafyFramingLibrary.h"
#include "CinemafyCameraLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	struct FShotSpec
	{
		/** How much of the world (cm, vertically) the frame should cover at the subject. */
		float FrameHeight;
		/** How far off the line of action the camera swings for singles, in degrees. */
		float SideAngleDeg;
		/** Camera height relative to the subject's eyes, in cm. */
		float HeightOffset;
		/** Extra horizontal room around both characters for two-shots, in cm. */
		float TwoShotPadding;
	};

	FShotSpec GetShotSpec(ECinemafyShotType ShotType)
	{
		switch (ShotType)
		{
		case ECinemafyShotType::WideTwoShot:     return { 230.f, 0.f, 0.f, 160.f };
		case ECinemafyShotType::MediumTwoShot:   return { 110.f, 0.f, 0.f, 80.f };
		case ECinemafyShotType::OverTheShoulder: return { 75.f, 0.f, -4.f, 0.f };
		case ECinemafyShotType::MediumShot:      return { 100.f, 20.f, -5.f, 0.f };
		case ECinemafyShotType::MediumCloseUp:   return { 60.f, 18.f, -2.f, 0.f };
		case ECinemafyShotType::CloseUp:         return { 38.f, 15.f, 0.f, 0.f };
		case ECinemafyShotType::ExtremeCloseUp:  return { 20.f, 12.f, 0.f, 0.f };
		case ECinemafyShotType::LowHero:         return { 110.f, 25.f, -75.f, 0.f };
		}
		return { 60.f, 18.f, 0.f, 0.f };
	}

	/** Pinhole camera: frame height / distance == sensor height / focal length. Returns cm. */
	float DistanceForFrame(float FrameHeightCm, float FocalMM, float SensorHeightMM)
	{
		return FrameHeightCm * FocalMM / FMath::Max(SensorHeightMM, KINDA_SMALL_NUMBER);
	}

	/** Shifts the aim point so the subject sits on the opposite third, leaving room in the direction they look. */
	FVector AddLeadRoom(const FVector& CamPos, const FVector& AimPoint, const FVector& LookDir, float FrameWidth)
	{
		const FVector ViewDir = (AimPoint - CamPos).GetSafeNormal();
		const FVector Lateral = (LookDir - ViewDir * FVector::DotProduct(LookDir, ViewDir)).GetSafeNormal();
		return AimPoint + Lateral * (FrameWidth / 6.f);
	}
}

ECinemafyPrime UCinemafyFramingLibrary::GetRecommendedPrime(ECinemafyShotType ShotType)
{
	switch (ShotType)
	{
	case ECinemafyShotType::WideTwoShot:     return ECinemafyPrime::Prime24;
	case ECinemafyShotType::MediumTwoShot:   return ECinemafyPrime::Prime35;
	case ECinemafyShotType::OverTheShoulder: return ECinemafyPrime::Prime50;
	case ECinemafyShotType::MediumShot:      return ECinemafyPrime::Prime50;
	case ECinemafyShotType::MediumCloseUp:   return ECinemafyPrime::Prime50;
	case ECinemafyShotType::CloseUp:         return ECinemafyPrime::Prime85;
	case ECinemafyShotType::ExtremeCloseUp:  return ECinemafyPrime::Prime135;
	case ECinemafyShotType::LowHero:         return ECinemafyPrime::Prime24;
	}
	return ECinemafyPrime::Prime50;
}

bool UCinemafyFramingLibrary::ShotNeedsCounterpart(ECinemafyShotType ShotType)
{
	return ShotType == ECinemafyShotType::WideTwoShot
		|| ShotType == ECinemafyShotType::MediumTwoShot
		|| ShotType == ECinemafyShotType::OverTheShoulder;
}

FVector UCinemafyFramingLibrary::GetEyePoint(const AActor* Actor)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}

	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);

	// Real eye bones (MetaHuman face mesh) are the gold standard.
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetBoneIndex(Cinemafy::LeftEyeBone) != INDEX_NONE && Mesh->GetBoneIndex(Cinemafy::RightEyeBone) != INDEX_NONE)
		{
			return 0.5f * (Mesh->GetSocketLocation(Cinemafy::LeftEyeBone) + Mesh->GetSocketLocation(Cinemafy::RightEyeBone));
		}
	}

	// The head bone sits at the base of the skull, so nudge up and forward to roughly eye level.
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetBoneIndex(Cinemafy::HeadBone) != INDEX_NONE)
		{
			return Mesh->GetSocketLocation(Cinemafy::HeadBone) + FVector::UpVector * 9.f + Actor->GetActorForwardVector() * 8.f;
		}
	}

	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	return FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z - 12.f);
}

bool UCinemafyFramingLibrary::SolveShot(const FCinemafyShotRequest& Request, FCinemafyShotSolution& OutSolution)
{
	const AActor* Subject = Request.Subject;
	const AActor* Counterpart = (Request.Counterpart != Request.Subject) ? Request.Counterpart.Get() : nullptr;
	if (!Subject || (ShotNeedsCounterpart(Request.ShotType) && !Counterpart))
	{
		return false;
	}

	const FVector Up = FVector::UpVector;
	const FShotSpec Spec = GetShotSpec(Request.ShotType);
	const FVector2D Sensor = UCinemafyCameraLibrary::GetSensorSize(Request.Sensor);
	const float SensorH = Sensor.Y;
	const float Aspect = Sensor.X / Sensor.Y;

	const FVector SubjectEye = GetEyePoint(Subject);
	const FVector OtherEye = Counterpart ? GetEyePoint(Counterpart) : SubjectEye + Subject->GetActorForwardVector() * 200.f;

	// Direction the subject is looking (toward the counterpart), flattened.
	FVector LookDir = (OtherEye - SubjectEye).GetSafeNormal2D();
	if (LookDir.IsNearlyZero())
	{
		LookDir = Subject->GetActorForwardVector().GetSafeNormal2D();
	}
	if (LookDir.IsNearlyZero())
	{
		LookDir = FVector::ForwardVector;
	}

	// The line of action must be the same no matter which character the shot is on, or the
	// "camera side" would flip between the A and B shots. Order the pair by name to make it stable.
	FVector LineDir = LookDir;
	if (Counterpart && Counterpart->GetName() < Subject->GetName())
	{
		LineDir = -LookDir;
	}
	const FVector SideDir = FVector::CrossProduct(Up, LineDir) * (Request.bFlipSide ? -1.f : 1.f);

	ECinemafyPrime Lens = Request.bUseRecommendedLens ? GetRecommendedPrime(Request.ShotType) : Request.Lens;
	float Focal = UCinemafyCameraLibrary::GetPrimeFocalLength(Lens);

	FVector CamPos;
	FVector AimPoint;
	FVector FocusPoint = SubjectEye;

	switch (Request.ShotType)
	{
	case ECinemafyShotType::WideTwoShot:
	case ECinemafyShotType::MediumTwoShot:
	{
		const FVector Mid = 0.5f * (SubjectEye + OtherEye);
		const float NeededWidth = FVector::Dist2D(SubjectEye, OtherEye) + Spec.TwoShotPadding;
		const float FrameH = FMath::Max(Spec.FrameHeight, NeededWidth / Aspect);
		CamPos = Mid + SideDir * DistanceForFrame(FrameH, Focal, SensorH) + Up * Spec.HeightOffset;
		AimPoint = Mid - Up * (FrameH / 6.f);
		FocusPoint = Mid;
		break;
	}

	case ECinemafyShotType::OverTheShoulder:
	{
		// Sit just behind the counterpart's shoulder (camera side of the line) and look back at the subject.
		CamPos = OtherEye + LookDir * 45.f + SideDir * 35.f + Up * Spec.HeightOffset;
		const float Dist = FVector::Dist(CamPos, SubjectEye);

		// Distance is fixed by the blocking, so pick the lens that gives the right frame size.
		Lens = UCinemafyCameraLibrary::SnapToPrime(SensorH * Dist / Spec.FrameHeight);
		Focal = UCinemafyCameraLibrary::GetPrimeFocalLength(Lens);
		const float FrameH = Dist * SensorH / Focal;

		AimPoint = SubjectEye - Up * (FrameH / 6.f);
		// Push the subject away from the foreground shoulder.
		AimPoint = AddLeadRoom(CamPos, AimPoint, -SideDir, FrameH * Aspect);
		break;
	}

	default:
	{
		const float Angle = FMath::DegreesToRadians(Spec.SideAngleDeg);
		const FVector Dir = (LookDir * FMath::Cos(Angle) + SideDir * FMath::Sin(Angle)).GetSafeNormal();
		CamPos = SubjectEye + Dir * DistanceForFrame(Spec.FrameHeight, Focal, SensorH) + Up * Spec.HeightOffset;
		AimPoint = SubjectEye - Up * (Spec.FrameHeight / 6.f);
		if (Counterpart)
		{
			AimPoint = AddLeadRoom(CamPos, AimPoint, LookDir, Spec.FrameHeight * Aspect);
		}
		break;
	}
	}

	const FRotator CamRot = (AimPoint - CamPos).Rotation();

	OutSolution.CameraTransform = FTransform(CamRot, CamPos);
	OutSolution.Lens = Lens;
	OutSolution.FocalLength = Focal;
	OutSolution.FocusDistance = FMath::Max(FVector::DotProduct(FocusPoint - CamPos, CamRot.Vector()), 15.f);
	return true;
}
