#include "CinemafyFramingLibrary.h"
#include "Components/CapsuleComponent.h"
#include "CinemafyCameraLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

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
		case ECinemafyShotType::GroupShot:       return { 230.f, 0.f, 10.f, 200.f };
		case ECinemafyShotType::TrackingLead:    return { 120.f, 10.f, -5.f, 80.f };
		case ECinemafyShotType::TrackingFollow:  return { 100.f, 0.f, 5.f, 0.f };
		case ECinemafyShotType::POV:             return { 90.f, 0.f, 0.f, 0.f };
		case ECinemafyShotType::Insert:          return { 30.f, 20.f, 0.f, 0.f };
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
	case ECinemafyShotType::GroupShot:       return ECinemafyPrime::Prime24;
	case ECinemafyShotType::TrackingLead:    return ECinemafyPrime::Prime35;
	case ECinemafyShotType::TrackingFollow:  return ECinemafyPrime::Prime35;
	case ECinemafyShotType::POV:             return ECinemafyPrime::Prime35;
	case ECinemafyShotType::Insert:          return ECinemafyPrime::Prime85;
	}
	return ECinemafyPrime::Prime50;
}

bool UCinemafyFramingLibrary::ShotNeedsCounterpart(ECinemafyShotType ShotType)
{
	return ShotType == ECinemafyShotType::WideTwoShot
		|| ShotType == ECinemafyShotType::MediumTwoShot
		|| ShotType == ECinemafyShotType::OverTheShoulder
		|| ShotType == ECinemafyShotType::POV;
}

namespace CinemafyEyes
{
	/** The eyes from an actor's own skeletal meshes: real eye bones, else the head bone nudged to eye level. */
	bool FromMeshes(const AActor* Actor, FVector& OutEye)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);

		// Real eye bones (MetaHuman face mesh) are the gold standard.
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetBoneIndex(Cinemafy::LeftEyeBone) != INDEX_NONE && Mesh->GetBoneIndex(Cinemafy::RightEyeBone) != INDEX_NONE)
			{
				OutEye = 0.5f * (Mesh->GetSocketLocation(Cinemafy::LeftEyeBone) + Mesh->GetSocketLocation(Cinemafy::RightEyeBone));
				return true;
			}
		}

		// The head bone sits at the base of the skull, so nudge up and forward to roughly eye level. "Forward" is the
		// way the mesh faces: skeletal meshes are authored facing +Y, the component's right vector.
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetBoneIndex(Cinemafy::HeadBone) != INDEX_NONE)
			{
				OutEye = Mesh->GetSocketLocation(Cinemafy::HeadBone) + FVector::UpVector * 9.f + Mesh->GetRightVector() * 8.f;
				return true;
			}
		}
		return false;
	}
}

FVector UCinemafyFramingLibrary::GetEyePoint(const AActor* Actor)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}
	FVector Eye;
	if (CinemafyEyes::FromMeshes(Actor, Eye))
	{
		return Eye;
	}
	// A visual stand-in: a body attached to the character (some character systems build the visible body as a
	// separate actor at runtime, so in the editor the character is an empty capsule: attach a stand-in mesh to it).
	TArray<AActor*> Attached;
	Actor->GetAttachedActors(Attached, true, true);
	for (const AActor* Proxy : Attached)
	{
		if (CinemafyEyes::FromMeshes(Proxy, Eye))
		{
			return Eye;
		}
	}

	// Guessing: say so once per actor, since everything framed on them is framed on a guess.
	static TSet<FString> Warned;
	if (Actor->IsA<APawn>() && !Warned.Contains(Actor->GetPathName()))
	{
		Warned.Add(Actor->GetPathName());
		UE_LOG(LogTemp, Warning, TEXT("Cinemafy: %s has no head or eye bone (no skeletal mesh with one, on it or attached to it): shots of them are framed on an estimate from its bounds. Attach a stand-in mesh to it to frame on a real head."), *Actor->GetActorNameOrLabel());
	}
	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	return FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z - 12.f);
}

FVector UCinemafyFramingLibrary::GetCanonicalLineDirection(const AActor* CharacterA, const AActor* CharacterB)
{
	if (!CharacterA || !CharacterB)
	{
		return FVector::ForwardVector;
	}

	// Order the pair by name so the line (and therefore "camera side") is stable.
	const bool bSwap = CharacterB->GetName() < CharacterA->GetName();
	const AActor* First = bSwap ? CharacterB : CharacterA;
	const AActor* Second = bSwap ? CharacterA : CharacterB;

	const FVector Dir = (GetEyePoint(Second) - GetEyePoint(First)).GetSafeNormal2D();
	return Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
}

float UCinemafyFramingLibrary::GetSideOfLine(const FVector& Point, const AActor* CharacterA, const AActor* CharacterB)
{
	if (!CharacterA || !CharacterB)
	{
		return 0.f;
	}

	const FVector SideDir = FVector::CrossProduct(FVector::UpVector, GetCanonicalLineDirection(CharacterA, CharacterB));
	const FVector Mid = 0.5f * (GetEyePoint(CharacterA) + GetEyePoint(CharacterB));
	return FVector::DotProduct(Point - Mid, SideDir);
}

FVector UCinemafyFramingLibrary::GetSubjectTrackPoint(const AActor* Actor)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}
	const FVector Base = Actor->GetActorLocation();
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		for (const TCHAR* Bone : { TEXT("pelvis"), TEXT("Pelvis"), TEXT("hips"), TEXT("Hips"), TEXT("mixamorig:Hips") })
		{
			if (Mesh->GetBoneIndex(Bone) != INDEX_NONE)
			{
				const FVector Hips = Mesh->GetBoneLocation(Bone);
				return FVector(Hips.X, Hips.Y, Base.Z); // Floor-plan position: no bob from the walk.
			}
		}
	}
	return Base;
}

bool UCinemafyFramingLibrary::IsLyingDown(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}
	// Blocked lying down (Previs tags them "Cinemafy.Pose.Lie..."), whatever the mesh shows right now.
	if (Actor->Tags.ContainsByPredicate([](const FName& Tag) { return Tag.ToString().StartsWith(TEXT("Cinemafy.Pose.Lie")); }))
	{
		return true;
	}
	// Eyes within 70 cm of the feet: lying on a bed, a floor, a couch.
	float Feet = Actor->GetActorLocation().Z;
	if (const UCapsuleComponent* Capsule = Actor->FindComponentByClass<UCapsuleComponent>())
	{
		Feet -= Capsule->GetScaledCapsuleHalfHeight();
	}
	else
	{
		FVector Origin, Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		Feet = Origin.Z - Extent.Z;
	}
	return GetEyePoint(Actor).Z - Feet < 70.f;
}

bool UCinemafyFramingLibrary::SolveShot(const FCinemafyShotRequest& InRequest, FCinemafyShotSolution& OutSolution)
{
	// People lying down are seen from above (an operator standing at the bed), not from eye level across the sheets.
	FCinemafyShotRequest Request = InRequest;
	if (IsLyingDown(Request.Subject) && Request.ShotType != ECinemafyShotType::POV)
	{
		Request.HeightOffset += 100.f;
	}
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
	// "camera side" would flip between the A and B shots.
	const FVector LineDir = Counterpart ? GetCanonicalLineDirection(Subject, Counterpart) : LookDir;
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
		const float Separation = FVector::Dist2D(SubjectEye, OtherEye);
		// The two-shot favours its subject: swung toward their face (a three-quarter on them), so "the two-shot on A"
		// and "on B" are different setups. People standing well apart swing further, so they aren't flat profiles
		// pinned to the frame's edges, and get more room.
		const float Favour = Separation > 200.f ? 34.f : 22.f;
		const float Padding = Spec.TwoShotPadding * (Separation > 200.f ? 1.3f : 1.f);
		const FVector Square = SideDir.RotateAngleAxis(Request.AngleOffset * (Request.bFlipSide ? -1.f : 1.f), Up);
		const FVector TowardA = Square.RotateAngleAxis(Favour, Up);
		const FVector TowardB = Square.RotateAngleAxis(-Favour, Up);
		const FVector Dir = FVector::DotProduct(TowardA, LookDir) >= FVector::DotProduct(TowardB, LookDir) ? TowardA : TowardB;
		// Seen at an angle, the pair takes less of the frame's width.
		const float Across = Separation * FMath::Abs(FVector::DotProduct(FVector::CrossProduct(Up, Dir).GetSafeNormal(), LookDir));
		const float NeededWidth = FMath::Max(Across, Separation * 0.6f) + Padding;
		const float FrameH = FMath::Max(Spec.FrameHeight, NeededWidth / Aspect);
		CamPos = Mid + Dir * DistanceForFrame(FrameH, Focal, SensorH) + Up * (Spec.HeightOffset + Request.HeightOffset);
		AimPoint = FMath::Lerp(Mid, SubjectEye, 0.15f) - Up * (FrameH / 6.f);
		FocusPoint = Mid;
		break;
	}

	case ECinemafyShotType::GroupShot:
	{
		// Everyone in: center on the group, wide enough for its spread, from the camera side of the main pair.
		TArray<FVector, TInlineAllocator<8>> Eyes;
		Eyes.Add(SubjectEye);
		if (Counterpart)
		{
			Eyes.Add(OtherEye);
		}
		for (const AActor* Member : Request.Group)
		{
			if (Member && Member != Subject && Member != Counterpart)
			{
				Eyes.Add(GetEyePoint(Member));
			}
		}

		FVector Center = FVector::ZeroVector;
		for (const FVector& Eye : Eyes)
		{
			Center += Eye / Eyes.Num();
		}
		float Spread = 0.f;
		for (const FVector& Eye : Eyes)
		{
			Spread = FMath::Max(Spread, static_cast<float>(FVector::Dist2D(Eye, Center)));
		}

		const float FrameH = FMath::Max(Spec.FrameHeight, (2.f * Spread + Spec.TwoShotPadding) / Aspect);
		const FVector Dir = SideDir.RotateAngleAxis(Request.AngleOffset * (Request.bFlipSide ? -1.f : 1.f), Up);
		// A touch above eye level helps separate people standing in depth.
		CamPos = Center + Dir * (DistanceForFrame(FrameH, Focal, SensorH) + Spread) + Up * (Spec.HeightOffset + Request.HeightOffset);
		AimPoint = Center - Up * (FrameH / 6.f);
		FocusPoint = Center;
		break;
	}

	case ECinemafyShotType::TrackingLead:
	{
		// In front of the walkers, backing up with them: the pair (or the subject) framed medium-wide.
		FVector Walk = Subject->GetActorForwardVector().GetSafeNormal2D();
		if (Counterpart)
		{
			Walk = (Walk + Counterpart->GetActorForwardVector().GetSafeNormal2D()).GetSafeNormal2D();
		}
		if (Walk.IsNearlyZero())
		{
			Walk = LookDir;
		}
		const FVector Center = Counterpart ? 0.5f * (SubjectEye + OtherEye) : SubjectEye;
		const float Width = Counterpart ? FVector::Dist2D(SubjectEye, OtherEye) + Spec.TwoShotPadding : 0.f;
		const float FrameH = FMath::Max(Spec.FrameHeight, Width / Aspect);
		const FVector Dir = Walk.RotateAngleAxis((Spec.SideAngleDeg + Request.AngleOffset) * (Request.bFlipSide ? -1.f : 1.f), Up);
		CamPos = Center + Dir * DistanceForFrame(FrameH, Focal, SensorH) + Up * (Spec.HeightOffset + Request.HeightOffset);
		AimPoint = Center - Up * (FrameH / 6.f);
		FocusPoint = Center;
		break;
	}

	case ECinemafyShotType::TrackingFollow:
	{
		// Behind and just off the subject's shoulder, looking where they're headed.
		FVector Walk = Subject->GetActorForwardVector().GetSafeNormal2D();
		if (Walk.IsNearlyZero())
		{
			Walk = LookDir;
		}
		const FVector Side = FVector::CrossProduct(Up, Walk) * (Request.bFlipSide ? -1.f : 1.f);
		CamPos = SubjectEye - Walk * 170.f + Side * (40.f + Request.AngleOffset * 1.5f) + Up * (Spec.HeightOffset + Request.HeightOffset);
		AimPoint = SubjectEye + Walk * 250.f - Up * 20.f;
		FocusPoint = SubjectEye;
		Lens = UCinemafyCameraLibrary::SnapToPrime(Focal);
		break;
	}

	case ECinemafyShotType::POV:
	{
		// Through the counterpart's eyes: just in front of their face, looking at the subject.
		const FVector Toward = (SubjectEye - OtherEye).GetSafeNormal();
		CamPos = OtherEye + (Toward.IsNearlyZero() ? LookDir : Toward) * 12.f + Up * Request.HeightOffset;
		AimPoint = SubjectEye - Up * 8.f;
		FocusPoint = SubjectEye;
		break;
	}

	case ECinemafyShotType::Insert:
	{
		// Tight on a thing: a character's hand, or a prop's bounds, seen from above at an angle from the viewer's side.
		FVector Center;
		float Size = Spec.FrameHeight;
		const USkeletalMeshComponent* Hands = nullptr;
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Subject);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetBoneIndex(Cinemafy::RightHandBone) != INDEX_NONE)
			{
				Hands = Mesh;
				break;
			}
		}
		if (Hands)
		{
			Center = Hands->GetSocketLocation(Cinemafy::RightHandBone);
		}
		else
		{
			FVector Extent;
			Subject->GetActorBounds(true, Center, Extent);
			Size = FMath::Max(Spec.FrameHeight, static_cast<float>(Extent.Size()) * 2.2f);
		}
		const FVector Side = Counterpart ? (OtherEye - Center).GetSafeNormal2D() : Subject->GetActorForwardVector().GetSafeNormal2D();
		const FVector Flat = (Side.IsNearlyZero() ? FVector::ForwardVector : Side).RotateAngleAxis((Spec.SideAngleDeg + Request.AngleOffset) * (Request.bFlipSide ? -1.f : 1.f), Up);
		const float Elevation = FMath::DegreesToRadians(35.f);
		const FVector Dir = (Flat * FMath::Cos(Elevation) + Up * FMath::Sin(Elevation)).GetSafeNormal();
		CamPos = Center + Dir * DistanceForFrame(Size, Focal, SensorH) + Up * Request.HeightOffset;
		AimPoint = Center;
		FocusPoint = Center;
		break;
	}

	case ECinemafyShotType::OverTheShoulder:
	{
		// Sit just behind the counterpart's shoulder (camera side of the line) and look back at the subject.
		CamPos = OtherEye + LookDir * 45.f + SideDir * (35.f + Request.AngleOffset * 1.5f) + Up * (Spec.HeightOffset + Request.HeightOffset);
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
		const float Angle = FMath::DegreesToRadians(FMath::Clamp(Spec.SideAngleDeg + Request.AngleOffset, 2.f, 70.f));
		const FVector Dir = (LookDir * FMath::Cos(Angle) + SideDir * FMath::Sin(Angle)).GetSafeNormal();
		CamPos = SubjectEye + Dir * DistanceForFrame(Spec.FrameHeight, Focal, SensorH) + Up * (Spec.HeightOffset + Request.HeightOffset);
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

bool UCinemafyFramingLibrary::IsShotClear(const UWorld* World, const FCinemafyShotRequest& Request, const FCinemafyShotSolution& Solution)
{
	if (!World)
	{
		return true;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyShotClear), false);
	if (Request.Subject)
	{
		Params.AddIgnoredActor(Request.Subject);
	}
	if (Request.Counterpart)
	{
		// The counterpart is *meant* to be in the way for OTS shots.
		Params.AddIgnoredActor(Request.Counterpart);
	}

	const FVector Camera = Solution.CameraTransform.GetLocation();

	// Room for the lens: not inside a wall or a prop.
	if (World->OverlapAnyTestByChannel(Camera, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
	{
		return false;
	}

	// A clear view of everyone the shot is about.
	TArray<FVector, TInlineAllocator<2>> Targets;
	Targets.Add(GetEyePoint(Request.Subject));
	if (Request.Counterpart && Request.ShotType != ECinemafyShotType::OverTheShoulder && ShotNeedsCounterpart(Request.ShotType))
	{
		Targets.Add(GetEyePoint(Request.Counterpart));
	}
	if (Request.ShotType == ECinemafyShotType::GroupShot)
	{
		for (const AActor* Member : Request.Group)
		{
			if (Member)
			{
				Params.AddIgnoredActor(Member);
				Targets.Add(GetEyePoint(Member));
			}
		}
	}
	for (const FVector& Target : Targets)
	{
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Camera, Target, ECC_Visibility, Params))
		{
			return false;
		}
	}

	// The counterpart is ignored by the traces above (it's meant to be in an OTS), so check it separately:
	// a shoulder in the foreground is framing, a shoulder over the subject's face is a ruined shot.
	if (Request.Subject && Request.Counterpart && Request.ShotType != ECinemafyShotType::GroupShot
		&& Request.ShotType != ECinemafyShotType::POV && Request.ShotType != ECinemafyShotType::Insert)
	{
		if (IsFaceCoveredByForeground(Camera, GetEyePoint(Request.Subject), GetEyePoint(Request.Counterpart), Request.Counterpart->GetActorRightVector()))
		{
			return false;
		}
	}
	return true;
}

bool UCinemafyFramingLibrary::IsFaceCoveredByForeground(const FVector& Camera, const FVector& SubjectEye, const FVector& ForegroundEye, const FVector& ForegroundRight)
{
	const float SubjectDistance = FVector::Dist(Camera, SubjectEye);
	if (SubjectDistance < 1.f)
	{
		return false;
	}
	const FVector ToFace = (SubjectEye - Camera) / SubjectDistance;
	// Half the face (about 9 cm) may be overlapped before it reads as covered.
	const float FaceAllowance = FMath::Asin(FMath::Clamp(9.f / SubjectDistance, 0.f, 1.f)) * 0.5f;

	const FVector Right = ForegroundRight.GetSafeNormal2D();
	struct FBlob { FVector Center; float Radius; };
	const FBlob Blobs[] =
	{
		{ ForegroundEye + FVector(0.f, 0.f, 4.f), 12.f },                      // head
		{ ForegroundEye - FVector(0.f, 0.f, 24.f) + Right * 18.f, 13.f },      // shoulders
		{ ForegroundEye - FVector(0.f, 0.f, 24.f) - Right * 18.f, 13.f },
	};
	for (const FBlob& Blob : Blobs)
	{
		const float BlobDistance = FVector::Dist(Camera, Blob.Center);
		if (BlobDistance < 1.f || BlobDistance >= SubjectDistance - 10.f)
		{
			continue; // Behind (or beside) the subject: can't cover them.
		}
		const FVector ToBlob = (Blob.Center - Camera) / BlobDistance;
		const float Separation = FMath::Acos(FMath::Clamp(FVector::DotProduct(ToFace, ToBlob), -1.f, 1.f));
		const float BlobRadius = FMath::Asin(FMath::Clamp(Blob.Radius / BlobDistance, 0.f, 1.f));
		if (Separation < BlobRadius - FaceAllowance + FMath::Asin(FMath::Clamp(9.f / SubjectDistance, 0.f, 1.f)))
		{
			return true;
		}
	}
	return false;
}

bool UCinemafyFramingLibrary::SolveShotAvoidingObstacles(const UObject* WorldContextObject, const FCinemafyShotRequest& Request,
	FCinemafyShotSolution& OutSolution, bool& bOutClearView)
{
	bOutClearView = false;
	if (!SolveShot(Request, OutSolution))
	{
		return false;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || IsShotClear(World, Request, OutSolution))
	{
		bOutClearView = true;
		return true;
	}

	// Alternatives that keep the shot's intent, roughly in order of how little they change it.
	struct FVariant { float Angle; float Height; int32 LensStep; };
	static const FVariant Variants[] = {
		{  8.f,   0.f,  0 }, { -8.f,   0.f,  0 }, { 16.f,   0.f,  0 },
		{  0.f,   0.f, -1 }, { 10.f,   0.f, -1 }, {  0.f,  25.f,  0 },
		{  0.f, -20.f,  0 }, { 24.f,   0.f, -1 }, {  0.f,   0.f, -2 },
		{ 40.f,   0.f, -1 }, {-40.f,   0.f, -1 }, { 60.f,   0.f, -2 }, {-60.f,   0.f, -2 },
	};

	const ECinemafyPrime BaseLens = OutSolution.Lens;
	for (const FVariant& Variant : Variants)
	{
		FCinemafyShotRequest Attempt = Request;
		Attempt.AngleOffset += Variant.Angle;
		Attempt.HeightOffset += Variant.Height;
		if (Variant.LensStep != 0)
		{
			// A wider lens brings the camera closer for the same framing: in front of whatever was blocking.
			Attempt.bUseRecommendedLens = false;
			Attempt.Lens = static_cast<ECinemafyPrime>(FMath::Clamp(static_cast<int32>(BaseLens) + Variant.LensStep, 0, static_cast<int32>(ECinemafyPrime::Prime135)));
		}

		FCinemafyShotSolution Candidate;
		if (SolveShot(Attempt, Candidate) && IsShotClear(World, Attempt, Candidate))
		{
			OutSolution = Candidate;
			bOutClearView = true;
			return true;
		}
	}

	// Last resort, like a spring arm: keep the angle but bring the camera in front of whatever is between it
	// and the subject, instead of leaving it behind a wall. (Still reported as not clear, so it gets flagged.)
	if (Request.Subject)
	{
		const FVector Eye = GetEyePoint(Request.Subject);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyShotPullIn), false, Request.Subject);
		if (Request.Counterpart)
		{
			Params.AddIgnoredActor(Request.Counterpart);
		}
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Eye, OutSolution.CameraTransform.GetLocation(), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(15.f), Params)
			&& FVector::Dist(Eye, Hit.Location) > 60.f)
		{
			OutSolution.CameraTransform.SetLocation(Hit.Location);
		}
	}
	return true;
}
