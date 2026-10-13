#include "CinemafyFaceProfile.h"

UCinemafyFaceProfile::UCinemafyFaceProfile()
{
	// Sensible starting point for MetaHuman Animator / LiveLink performances.
	Brows.JitterReduction = 0.3f;
	Brows.Springiness = 0.15f;

	Eyes.JitterReduction = 0.f;
	Eyes.Springiness = 0.f;

	Cheeks.JitterReduction = 0.3f;
	Cheeks.Springiness = 0.25f;

	Nose.JitterReduction = 0.3f;
	Nose.Springiness = 0.1f;

	Mouth.JitterReduction = 0.25f;
	Mouth.Springiness = 0.2f;

	Jaw.JitterReduction = 0.2f;
	Jaw.Springiness = 0.1f;

	Tongue.JitterReduction = 0.2f;
	Other.JitterReduction = 0.2f;
}

const FCinemafyFaceRegionSettings& UCinemafyFaceProfile::GetRegion(ECinemafyFaceRegion Region) const
{
	switch (Region)
	{
	case ECinemafyFaceRegion::Brows:  return Brows;
	case ECinemafyFaceRegion::Eyes:   return Eyes;
	case ECinemafyFaceRegion::Cheeks: return Cheeks;
	case ECinemafyFaceRegion::Nose:   return Nose;
	case ECinemafyFaceRegion::Mouth:  return Mouth;
	case ECinemafyFaceRegion::Jaw:    return Jaw;
	case ECinemafyFaceRegion::Tongue: return Tongue;
	default:                          return Other;
	}
}

const UCinemafyFaceProfile* UCinemafyFaceProfile::GetDefaultProfile()
{
	return ::GetDefault<UCinemafyFaceProfile>();
}
