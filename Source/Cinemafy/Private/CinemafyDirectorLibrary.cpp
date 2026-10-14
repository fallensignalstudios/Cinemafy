#include "CinemafyDirectorLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyChoreographyLibrary.h"
#include "GameFramework/Actor.h"

namespace
{
	const ECinemafyShotType AllShotTypes[] = {
		ECinemafyShotType::WideTwoShot, ECinemafyShotType::MediumTwoShot, ECinemafyShotType::OverTheShoulder,
		ECinemafyShotType::MediumShot, ECinemafyShotType::MediumCloseUp, ECinemafyShotType::CloseUp,
		ECinemafyShotType::ExtremeCloseUp, ECinemafyShotType::LowHero, ECinemafyShotType::GroupShot,
		ECinemafyShotType::TrackingLead, ECinemafyShotType::TrackingFollow };

	const ECinemafyPrime AllPrimes[] = {
		ECinemafyPrime::Prime18, ECinemafyPrime::Prime24, ECinemafyPrime::Prime35,
		ECinemafyPrime::Prime50, ECinemafyPrime::Prime85, ECinemafyPrime::Prime135 };

	bool IsTwoShot(ECinemafyShotType Type)
	{
		return Type == ECinemafyShotType::WideTwoShot || Type == ECinemafyShotType::MediumTwoShot || Type == ECinemafyShotType::GroupShot;
	}

	FString ShotName(ECinemafyShotType Type)
	{
		return StaticEnum<ECinemafyShotType>()->GetDisplayNameTextByValue(static_cast<int64>(Type)).ToString();
	}

	FString ActorName(const AActor* Actor)
	{
		return Actor ? Actor->GetActorNameOrLabel() : FString(TEXT("?"));
	}

	ECinemafyPrime BiasLens(ECinemafyPrime Lens, int32 Bias)
	{
		const int32 Index = FMath::Clamp(static_cast<int32>(Lens) + Bias, 0, static_cast<int32>(UE_ARRAY_COUNT(AllPrimes)) - 1);
		return AllPrimes[Index];
	}

	/** The move that suits each shot, and how far it travels (smaller for tighter shots). */
	bool GetMoveForShot(ECinemafyShotType Type, ECinemafyCameraMove& OutMove, float& OutDistance)
	{
		switch (Type)
		{
		case ECinemafyShotType::WideTwoShot:   OutMove = ECinemafyCameraMove::PushIn;  OutDistance = 45.f; return true;
		case ECinemafyShotType::MediumTwoShot: OutMove = ECinemafyCameraMove::PushIn;  OutDistance = 35.f; return true;
		case ECinemafyShotType::MediumShot:    OutMove = ECinemafyCameraMove::PushIn;  OutDistance = 30.f; return true;
		case ECinemafyShotType::MediumCloseUp: OutMove = ECinemafyCameraMove::PushIn;  OutDistance = 20.f; return true;
		case ECinemafyShotType::CloseUp:       OutMove = ECinemafyCameraMove::PushIn;  OutDistance = 12.f; return true;
		case ECinemafyShotType::LowHero:       OutMove = ECinemafyCameraMove::CraneUp; OutDistance = 45.f; return true;
		default:                               return false; // OTS and ECU look best locked off.
		}
	}

	ECinemafyShotType PickShotType(const FCinemafyDirectorStyle& Style, FRandomStream& Random, float Intensity,
		const AActor* Subject, const AActor* Counterpart, bool bReaction, const FCinemafyPlannedShot* Previous, bool bGroupScene)
	{
		// Where on the wide..tight scale this moment wants to be.
		const float NeutralSize = 2.5f;
		const float TargetSize = FMath::Lerp(NeutralSize, FMath::Lerp(1.5f, 4.5f, Intensity), Style.TightenWithIntensity);

		TArray<TPair<ECinemafyShotType, float>, TInlineAllocator<8>> Candidates;
		float Total = 0.f;

		for (ECinemafyShotType Type : AllShotTypes)
		{
			if (UCinemafyFramingLibrary::ShotNeedsCounterpart(Type) && !Counterpart)
			{
				continue;
			}
			if (Type == ECinemafyShotType::GroupShot && !bGroupScene)
			{
				continue;
			}
			// Walk-and-talk coverage only when people are walking (handled by the planner directly).
			if (Type == ECinemafyShotType::TrackingLead || Type == ECinemafyShotType::TrackingFollow)
			{
				continue;
			}

			const float* Preference = Style.ShotPreference.Find(Type);
			float Weight = Preference ? *Preference : 1.f;
			if (Weight <= 0.f)
			{
				continue;
			}

			const float Size = UCinemafyDirectorLibrary::GetShotSize(Type);
			Weight *= FMath::Exp(-FMath::Square(Size - TargetSize) / (2.f * 1.1f * 1.1f));

			// Moment-specific craft.
			if (Type == ECinemafyShotType::LowHero)        { Weight *= Intensity > 0.5f ? Style.LowAngleBias * 2.f : 0.1f; }
			if (Type == ECinemafyShotType::ExtremeCloseUp) { Weight *= Intensity > 0.7f ? 1.f : 0.1f; }
			if (Type == ECinemafyShotType::WideTwoShot)    { Weight *= Intensity > 0.6f ? 0.3f : 1.f; }
			if (Type == ECinemafyShotType::GroupShot)      { Weight *= Intensity > 0.5f ? 0.2f : 0.8f; }
			if (bReaction && IsTwoShot(Type))              { Weight *= 0.3f; }

			if (Previous)
			{
				const float PreviousSize = UCinemafyDirectorLibrary::GetShotSize(Previous->ShotType);
				if (Previous->Subject == Subject)
				{
					// Same person again: never the same shot, and change size enough to avoid a jump cut
					// (pushing in tighter is the one move that reads as intentional).
					if (Previous->ShotType == Type)
					{
						// Same framing on the same person is a jump cut. Never.
						continue;
					}
					if (FMath::Abs(Size - PreviousSize) < 1.f && Size <= PreviousSize)
					{
						Weight *= 0.2f;
					}
				}
				else if (!IsTwoShot(Previous->ShotType) && !IsTwoShot(Type))
				{
					// Shot-reverse-shot: match the other person's framing, so the cut reads as one conversation.
					if (Previous->ShotType == Type)
					{
						Weight *= 2.5f;
					}
					else if (FMath::Abs(Size - PreviousSize) > 1.f)
					{
						Weight *= 0.5f;
					}
				}
			}

			if (Weight > 0.f)
			{
				Candidates.Emplace(Type, Weight);
				Total += Weight;
			}
		}

		if (Candidates.Num() == 0)
		{
			return ECinemafyShotType::MediumCloseUp;
		}

		float Roll = Random.FRand() * Total;
		for (const TPair<ECinemafyShotType, float>& Candidate : Candidates)
		{
			Roll -= Candidate.Value;
			if (Roll <= 0.f)
			{
				return Candidate.Key;
			}
		}
		return Candidates.Last().Key;
	}
}

FCinemafyDirectorStyle::FCinemafyDirectorStyle()
{
	for (ECinemafyShotType Type : AllShotTypes)
	{
		ShotPreference.Add(Type, 1.f);
	}
}

float UCinemafyDirectorLibrary::GetShotSize(ECinemafyShotType ShotType)
{
	switch (ShotType)
	{
	case ECinemafyShotType::GroupShot:       return -0.5f;
	case ECinemafyShotType::TrackingLead:    return 1.5f;
	case ECinemafyShotType::TrackingFollow:  return 2.f;
	case ECinemafyShotType::WideTwoShot:     return 0.f;
	case ECinemafyShotType::MediumTwoShot:   return 1.f;
	case ECinemafyShotType::MediumShot:      return 2.f;
	case ECinemafyShotType::LowHero:         return 2.f;
	case ECinemafyShotType::OverTheShoulder: return 2.5f;
	case ECinemafyShotType::MediumCloseUp:   return 3.f;
	case ECinemafyShotType::CloseUp:         return 4.f;
	case ECinemafyShotType::ExtremeCloseUp:  return 5.f;
	}
	return 3.f;
}

FCinemafyDirectorStyle UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset Preset)
{
	FCinemafyDirectorStyle S;
	auto Prefs = [&S](float Wide, float MedTwo, float OTS, float Medium, float MCU, float CU, float ECU, float Low)
	{
		S.ShotPreference.Add(ECinemafyShotType::WideTwoShot, Wide);
		S.ShotPreference.Add(ECinemafyShotType::MediumTwoShot, MedTwo);
		S.ShotPreference.Add(ECinemafyShotType::OverTheShoulder, OTS);
		S.ShotPreference.Add(ECinemafyShotType::MediumShot, Medium);
		S.ShotPreference.Add(ECinemafyShotType::MediumCloseUp, MCU);
		S.ShotPreference.Add(ECinemafyShotType::CloseUp, CU);
		S.ShotPreference.Add(ECinemafyShotType::ExtremeCloseUp, ECU);
		S.ShotPreference.Add(ECinemafyShotType::LowHero, Low);
	};

	switch (Preset)
	{
	case ECinemafyDirectorPreset::IntimateDrama:
		//    Wide MedTwo OTS  Med  MCU  CU   ECU  Low
		Prefs(0.5f, 0.6f, 1.2f, 0.6f, 1.4f, 1.5f, 0.7f, 0.1f);
		S.TightenWithIntensity = 0.8f;
		S.CalmShotLength = FVector2D(4.0, 7.0);
		S.IntenseShotLength = FVector2D(2.5, 4.5);
		S.ReactionShotChance = 0.5f;
		S.LowAngleBias = 0.05f;
		S.LensBias = 1;
		S.MoveChance = 0.5f;
		S.Look = ECinemafyLook::WarmEpic;
		S.LookStrength = 0.6f;
		S.LightingStyle = ECinemafyLightingStyle::Loop;
		break;

	case ECinemafyDirectorPreset::KineticAction:
		Prefs(0.6f, 0.5f, 0.8f, 1.0f, 1.0f, 1.2f, 0.8f, 1.2f);
		S.TightenWithIntensity = 0.7f;
		S.CalmShotLength = FVector2D(2.0, 3.5);
		S.IntenseShotLength = FVector2D(0.8, 1.6);
		S.ReactionShotChance = 0.3f;
		S.LowAngleBias = 0.8f;
		S.LensBias = -1;
		S.bEstablishingShot = false;
		S.CalmFeel = ECinemafyCameraFeel::Handheld;
		S.IntenseFeel = ECinemafyCameraFeel::RunAndGun;
		S.MoveChance = 0.6f;
		S.Look = ECinemafyLook::TealOrange;
		S.LightingStyle = ECinemafyLightingStyle::ThreePoint;
		break;

	case ECinemafyDirectorPreset::SlowBurn:
		Prefs(1.4f, 1.2f, 0.8f, 1.0f, 0.7f, 0.6f, 0.4f, 0.2f);
		S.TightenWithIntensity = 0.5f;
		S.CalmShotLength = FVector2D(6.0, 10.0);
		S.IntenseShotLength = FVector2D(3.0, 5.0);
		S.ReactionShotChance = 0.2f;
		S.LowAngleBias = 0.1f;
		S.IntenseFeel = ECinemafyCameraFeel::Tripod;
		S.MoveChance = 0.7f;
		S.Look = ECinemafyLook::CleanCinema;
		S.LookStrength = 1.f;
		S.LightingStyle = ECinemafyLightingStyle::Rembrandt;
		break;

	case ECinemafyDirectorPreset::NoirThriller:
		Prefs(0.7f, 0.5f, 1.1f, 0.8f, 1.2f, 1.2f, 0.6f, 1.0f);
		S.TightenWithIntensity = 0.7f;
		S.CalmShotLength = FVector2D(3.5, 6.0);
		S.IntenseShotLength = FVector2D(1.5, 3.0);
		S.ReactionShotChance = 0.4f;
		S.LowAngleBias = 0.6f;
		S.MoveChance = 0.4f;
		S.Look = ECinemafyLook::Noir;
		S.LookStrength = 1.f;
		S.LetterboxAspect = 1.85f;
		S.LightingStyle = ECinemafyLightingStyle::Noir;
		break;

	case ECinemafyDirectorPreset::Epic:
		Prefs(1.5f, 1.0f, 0.8f, 1.0f, 0.8f, 0.7f, 0.3f, 1.2f);
		S.TightenWithIntensity = 0.5f;
		S.CalmShotLength = FVector2D(4.0, 7.0);
		S.IntenseShotLength = FVector2D(2.0, 3.5);
		S.ReactionShotChance = 0.25f;
		S.LowAngleBias = 0.7f;
		S.LensBias = -1;
		S.MoveChance = 0.6f;
		S.Look = ECinemafyLook::WarmEpic;
		S.LightingStyle = ECinemafyLightingStyle::Rembrandt;
		break;

	case ECinemafyDirectorPreset::SpaceOpera:
		// Modeled on classic-trilogy space opera craft: classical, mostly locked-off coverage that
		// favors medium shots and two-shots held through exchanges over tight close-ups; brisk cutting
		// that gets much faster in action; wipes between scenes; almost no slow motion; anamorphic frame.
		//    Wide MedTwo OTS  Med  MCU  CU   ECU   Low
		Prefs(1.3f, 1.5f, 0.9f, 1.4f, 1.0f, 0.6f, 0.15f, 0.3f);
		S.ShotPreference.Add(ECinemafyShotType::GroupShot, 1.3f);
		S.TightenWithIntensity = 0.4f;
		S.CalmShotLength = FVector2D(3.0, 5.5);
		S.IntenseShotLength = FVector2D(1.2, 2.5);
		S.ReactionShotChance = 0.4f;
		S.LowAngleBias = 0.3f;
		S.CalmFeel = ECinemafyCameraFeel::Locked;
		S.IntenseFeel = ECinemafyCameraFeel::Tripod;
		S.MoveChance = 0.12f;
		S.HoldThroughChance = 0.45f;
		S.Look = ECinemafyLook::WarmEpic;
		S.LookStrength = 0.45f;
		S.LetterboxAspect = 2.39f;
		S.LightingStyle = ECinemafyLightingStyle::ThreePoint;
		S.OpeningTransition = ECinemafyTransition::Wipe;
		S.SceneBreakTransition = ECinemafyTransition::Wipe;
		S.ClosingTransition = ECinemafyTransition::Wipe;
		S.TransitionDuration = 0.8f;
		S.WipeDirection = ECinemafyWipeDirection::Alternate;
		S.CrashZoomChance = 0.f;
		S.SlowMoChance = 0.02f;
		S.TrackingLeadPreference = 0.4f;
		break;

	case ECinemafyDirectorPreset::MindBender:
		// Large-format blockbuster craft: enormous establishing wides against tight, intense singles;
		// slow push-ins; cold grade; relentless cross-cutting that accelerates toward the climax.
		Prefs(1.5f, 0.5f, 0.9f, 0.6f, 1.3f, 1.4f, 0.4f, 0.6f);
		S.TightenWithIntensity = 0.7f;
		S.CalmShotLength = FVector2D(3.0, 5.0);
		S.IntenseShotLength = FVector2D(1.2, 2.5);
		S.ReactionShotChance = 0.3f;
		S.LowAngleBias = 0.5f;
		S.LensBias = 0;
		S.IntenseFeel = ECinemafyCameraFeel::Shoulder;
		S.MoveChance = 0.6f;
		S.HoldThroughChance = 0.1f;
		S.Look = ECinemafyLook::ColdSteel;
		S.LightingStyle = ECinemafyLightingStyle::Rembrandt;
		S.SceneBreakTransition = ECinemafyTransition::MatchCut;
		S.ClosingTransition = ECinemafyTransition::FadeThroughBlack;
		S.CrashZoomChance = 0.f;
		S.SlowMoChance = 0.25f;
		S.CrossCutLinesPerBlock = 2;
		break;

	case ECinemafyDirectorPreset::SymmetryStorybook:
		// Storybook craft: centered, front-on, symmetrical framing; locked-off camera; deadpan holds.
		Prefs(1.3f, 1.2f, 0.3f, 1.4f, 1.0f, 0.7f, 0.2f, 0.1f);
		S.TightenWithIntensity = 0.3f;
		S.CalmShotLength = FVector2D(4.0, 7.0);
		S.IntenseShotLength = FVector2D(2.5, 4.0);
		S.ReactionShotChance = 0.45f;
		S.LowAngleBias = 0.05f;
		S.CalmFeel = ECinemafyCameraFeel::Locked;
		S.IntenseFeel = ECinemafyCameraFeel::Locked;
		S.MoveChance = 0.15f;
		S.FrontalFraming = 1.f;
		S.HoldThroughChance = 0.3f;
		S.Look = ECinemafyLook::WarmEpic;
		S.LookStrength = 0.35f;
		S.LetterboxAspect = 1.85f;
		S.LightingStyle = ECinemafyLightingStyle::Butterfly;
		S.SceneBreakTransition = ECinemafyTransition::Cut;
		S.TrackingLeadPreference = 0.f;
		break;

	case ECinemafyDirectorPreset::LongTakeTension:
		// Prestige-thriller craft: few cuts, slow creeping moves, cold light, cuts held until they hurt.
		Prefs(1.1f, 1.3f, 1.0f, 1.2f, 1.0f, 0.8f, 0.3f, 0.2f);
		S.TightenWithIntensity = 0.5f;
		S.CalmShotLength = FVector2D(8.0, 14.0);
		S.IntenseShotLength = FVector2D(4.0, 7.0);
		S.ReactionShotChance = 0.15f;
		S.LowAngleBias = 0.1f;
		S.CalmFeel = ECinemafyCameraFeel::Tripod;
		S.IntenseFeel = ECinemafyCameraFeel::Tripod;
		S.MoveChance = 0.8f;
		S.HoldThroughChance = 0.6f;
		S.Look = ECinemafyLook::ColdSteel;
		S.LightingStyle = ECinemafyLightingStyle::Rembrandt;
		S.OpeningTransition = ECinemafyTransition::FadeThroughBlack;
		S.ClosingTransition = ECinemafyTransition::FadeThroughBlack;
		S.TransitionDuration = 1.2f;
		S.SlowMoChance = 0.f;
		break;

	case ECinemafyDirectorPreset::SnapZoomGrindhouse:
		// 70s exploitation homage: crash zooms, extreme close-ups on eyes, punchy short shots.
		Prefs(0.6f, 0.5f, 0.6f, 0.8f, 1.0f, 1.4f, 1.2f, 0.6f);
		S.TightenWithIntensity = 0.8f;
		S.CalmShotLength = FVector2D(2.0, 3.5);
		S.IntenseShotLength = FVector2D(0.8, 1.6);
		S.ReactionShotChance = 0.45f;
		S.LowAngleBias = 0.6f;
		S.CalmFeel = ECinemafyCameraFeel::Shoulder;
		S.IntenseFeel = ECinemafyCameraFeel::Handheld;
		S.MoveChance = 0.3f;
		S.Look = ECinemafyLook::BleachBypass;
		S.LightingStyle = ECinemafyLightingStyle::Split;
		S.SceneBreakTransition = ECinemafyTransition::Cut;
		S.CrashZoomChance = 0.7f;
		S.SlowMoChance = 0.3f;
		break;

	case ECinemafyDirectorPreset::GoldenHourLyrical:
		// Naturalistic poetic craft: drifting handheld, magic-hour warmth, lingering reactions.
		Prefs(1.2f, 0.9f, 0.8f, 1.0f, 1.1f, 1.2f, 0.4f, 0.2f);
		S.TightenWithIntensity = 0.5f;
		S.CalmShotLength = FVector2D(5.0, 9.0);
		S.IntenseShotLength = FVector2D(3.0, 5.0);
		S.ReactionShotChance = 0.6f;
		S.LowAngleBias = 0.2f;
		S.LensBias = 1;
		S.CalmFeel = ECinemafyCameraFeel::Shoulder;
		S.IntenseFeel = ECinemafyCameraFeel::Handheld;
		S.MoveChance = 0.4f;
		S.Look = ECinemafyLook::WarmEpic;
		S.LookStrength = 1.f;
		S.LightingStyle = ECinemafyLightingStyle::Loop;
		S.OpeningTransition = ECinemafyTransition::Dissolve;
		S.SceneBreakTransition = ECinemafyTransition::Dissolve;
		S.ClosingTransition = ECinemafyTransition::FadeThroughBlack;
		S.TransitionDuration = 1.5f;
		S.TrackingLeadPreference = 0.3f;
		break;

	case ECinemafyDirectorPreset::ClassicHollywood:
	default:
		Prefs(1.0f, 1.0f, 1.4f, 0.8f, 1.2f, 1.0f, 0.3f, 0.3f);
		S.Look = ECinemafyLook::CleanCinema;
		S.LetterboxAspect = 1.85f;
		S.LightingStyle = ECinemafyLightingStyle::ThreePoint;
		break;
	}
	return S;
}

TArray<FCinemafyPlannedShot> UCinemafyDirectorLibrary::PlanLine(const FCinemafyLine& Line, int32 LineIndex, const FCinemafyDirectorStyle& Style,
	FRandomStream& Random, const FCinemafyPlannedShot* Previous, bool bOpensScene, float StartTime, const TArray<AActor*>& SceneCast)
{
	const bool bGroupScene = SceneCast.Num() >= 3;
	auto OthersInGroup = [&SceneCast](const AActor* A, const AActor* B)
	{
		TArray<TObjectPtr<AActor>> Others;
		for (AActor* Member : SceneCast)
		{
			if (Member != A && Member != B)
			{
				Others.Add(Member);
			}
		}
		return Others;
	};

	constexpr float MinShotLength = 0.8f;

	TArray<FCinemafyPlannedShot> Shots;
	if (!Line.Speaker)
	{
		return Shots;
	}

	// Action plays hot regardless of the line's dialogue intensity; a reveal plays a notch tighter.
	float Intensity = FMath::Clamp(Line.Intensity, 0.f, 1.f);
	if (Line.bAction)
	{
		Intensity = FMath::Max(Intensity, 0.8f);
	}
	else if (Line.Moment == ECinemafyMoment::Reveal)
	{
		Intensity = FMath::Min(Intensity + 0.15f, 1.f);
	}
	const FVector2D LengthRange = FMath::Lerp(Style.CalmShotLength, Style.IntenseShotLength, Intensity);
	float Remaining = FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);

	// Cut on action: every impact becomes a cut a few frames early, so the hit lands in the new shot.
	TArray<float> ActionCuts;
	TArray<float> Impacts;
	if (Line.bAction)
	{
		Impacts = Line.ImpactTimes.Num() > 0 ? Line.ImpactTimes : Line.Analysis.EmphasisTimes;
		Impacts.Sort();
		float LastCut = 0.f;
		for (const float Impact : Impacts)
		{
			const float CutTime = Impact - 0.08f;
			if (CutTime - LastCut >= 0.4f && (Line.Duration + Line.PauseAfter) - CutTime >= 0.4f)
			{
				ActionCuts.Add(CutTime);
				LastCut = CutTime;
			}
		}
	}
	int32 SlowMoBudget = 1;
	float Time = StartTime;
	FCinemafyPlannedShot Last = Previous ? *Previous : FCinemafyPlannedShot();
	bool bHasLast = Previous != nullptr;

	for (int32 Segment = 0; Remaining > 0.01f; ++Segment)
	{
		float Length = Random.FRandRange(LengthRange.X, FMath::Max(LengthRange.X, LengthRange.Y));
		const float SegmentStartInLine = Time - StartTime;
		const float* NextActionCut = ActionCuts.FindByPredicate([SegmentStartInLine](float CutTime) { return CutTime > SegmentStartInLine + 0.05f; });
		if (Line.bAction && ActionCuts.Num() > 0)
		{
			// Action is cut by its impacts, not by a clock.
			Length = NextActionCut ? *NextActionCut - SegmentStartInLine : Remaining;
		}
		else if (Remaining - Length < MinShotLength)
		{
			Length = Remaining;
		}
		else
		{
			// Cut on the breath: slide the cut to the nearest pause in the delivery, if one is close.
			const float SegmentStart = Time - StartTime;
			float BestPause = -1.f;
			for (const float Pause : Line.Analysis.PauseTimes)
			{
				const bool bLongEnough = Pause - SegmentStart >= MinShotLength && Remaining - (Pause - SegmentStart) >= MinShotLength;
				if (bLongEnough && FMath::Abs(Pause - (SegmentStart + Length)) < 0.7f &&
					(BestPause < 0.f || FMath::Abs(Pause - (SegmentStart + Length)) < FMath::Abs(BestPause - (SegmentStart + Length))))
				{
					BestPause = Pause;
				}
			}
			if (BestPause >= 0.f)
			{
				Length = BestPause - SegmentStart;
			}
		}

		FCinemafyPlannedShot Shot;
		Shot.StartTime = Time;
		Shot.Duration = Length;
		Shot.LineIndex = LineIndex;
		Shot.bLineStart = Segment == 0;
		Shot.Subject = Line.Speaker;
		Shot.Counterpart = Line.Listener;

		// Later in a line (and especially in the pause after it), cut to the listener's reaction.
		const bool bInPause = (Time - StartTime) >= Line.Duration;
		// A silent beat is one held reaction: the whole pause in a single shot.
		const bool bBeat = bInPause && Line.Listener && (Line.Moment == ECinemafyMoment::SilentBeat || Line.Moment == ECinemafyMoment::Reveal);
		if (bBeat && Segment > 0)
		{
			Length = Remaining;
			Shot.Duration = Length;
		}

		FString Why;
		if (Line.Listener && Segment > 0 && (bBeat || Random.FRand() < Style.ReactionShotChance * (bInPause ? 1.6f : 1.f)))
		{
			Shot.Subject = Line.Listener;
			Shot.Counterpart = Line.Speaker;
			Shot.bIsReaction = true;

			// In a group, sometimes it's a bystander's face that tells the story.
			if (bGroupScene && Random.FRand() < 0.3f)
			{
				const TArray<TObjectPtr<AActor>> Bystanders = OthersInGroup(Line.Speaker, Line.Listener);
				if (Bystanders.Num() > 0)
				{
					Shot.Subject = Bystanders[Random.RandRange(0, Bystanders.Num() - 1)];
					Why = TEXT("bystander reaction");
				}
			}
		}

		// A new pair talking in a group: sometimes step back so the audience knows where everyone is.
		const bool bNewExchange = bGroupScene && Segment == 0 && bHasLast &&
			!((Last.Subject == Line.Speaker && Last.Counterpart == Line.Listener) || (Last.Subject == Line.Listener && Last.Counterpart == Line.Speaker));

		if (Line.bMoving && !Line.bForceShot)
		{
			// Walking: lead them (camera backs up in front) or follow behind; alternate within the line so
			// consecutive shots change.
			const bool bLeadFirst = Random.FRand() < Style.TrackingLeadPreference;
			const bool bLead = (Segment % 2 == 0) ? bLeadFirst : !bLeadFirst;
			Shot.ShotType = bLead ? ECinemafyShotType::TrackingLead : ECinemafyShotType::TrackingFollow;
			if (bHasLast && Last.ShotType == Shot.ShotType && Last.Subject == Shot.Subject)
			{
				Shot.ShotType = bLead ? ECinemafyShotType::TrackingFollow : ECinemafyShotType::TrackingLead;
			}
			Why = bLead ? TEXT("walk-and-talk, leading them") : TEXT("walk-and-talk, following");
		}
		else if (bOpensScene && Segment == 0 && Style.bEstablishingShot && Shot.Counterpart)
		{
			Shot.ShotType = bGroupScene ? ECinemafyShotType::GroupShot : ECinemafyShotType::WideTwoShot;
			Why = TEXT("establishing the space");
		}
		else if (Segment == 0 && Line.Moment == ECinemafyMoment::NewExchange && Line.Listener && !bGroupScene
			&& Intensity < 0.7f && Random.FRand() < 0.6f && !(bHasLast && Last.ShotType == ECinemafyShotType::MediumTwoShot))
		{
			Shot.ShotType = ECinemafyShotType::MediumTwoShot;
			Why = TEXT("new exchange, re-establishing");
		}
		else if (bNewExchange && Random.FRand() < 0.35f * (1.f - Intensity) && Last.ShotType != ECinemafyShotType::GroupShot)
		{
			Shot.ShotType = ECinemafyShotType::GroupShot;
			Why = TEXT("re-establishing for the new exchange");
		}
		else if (Line.bForceShot && Segment == 0)
		{
			Shot.ShotType = Line.ForcedShot;
			Why = TEXT("forced by the script");
		}
		else
		{
			Shot.ShotType = PickShotType(Style, Random, Intensity, Shot.Subject, Shot.Counterpart, Shot.bIsReaction, bHasLast ? &Last : nullptr, bGroupScene);
			if (Why.IsEmpty())
			{
				Why = Line.bAction ? TEXT("action, cut on the impact")
					: (Shot.bIsReaction ? TEXT("reaction") : (Intensity > 0.7f ? TEXT("tightening for drama") : TEXT("coverage")));
			}
		}

		if (Shot.ShotType == ECinemafyShotType::GroupShot)
		{
			Shot.Group = OthersInGroup(Shot.Subject, Shot.Counterpart);
		}

		// Singles focus on their subject; shots with both people in them focus on whoever's talking.
		Shot.FocusOn = IsTwoShot(Shot.ShotType) ? Line.Speaker : Shot.Subject;

		Shot.Lens = BiasLens(UCinemafyFramingLibrary::GetRecommendedPrime(Shot.ShotType), Style.LensBias);

		// Moves: a creep-in on the establishing shot, push-ins as drama builds.
		{
			const bool bEstablishing = Why == TEXT("establishing the space");

			// The strongest stressed word inside this shot, if the audio told us.
			const float SegmentStart = Time - StartTime;
			float Emphasis = -1.f;
			for (const float E : Line.Analysis.EmphasisTimes)
			{
				if (E > SegmentStart + 0.4f && E < SegmentStart + Length - 0.1f)
				{
					Emphasis = E - SegmentStart;
					break; // Loudest first.
				}
			}

			float MoveChance = Style.MoveChance * (bEstablishing ? 1.5f : FMath::Lerp(0.6f, 1.4f, Intensity));
			if (Line.Moment == ECinemafyMoment::Reveal && Segment == 0)
			{
				// Reveals are where a director spends their camera moves.
				MoveChance = FMath::Max(MoveChance * 2.f, 0.5f);
			}
			if (Emphasis >= 0.f && !Shot.bIsReaction)
			{
				MoveChance *= 1.5f;
			}
			ECinemafyCameraMove Move;
			float Distance;
			if (!Line.bMoving && !Line.bAction && Random.FRand() < MoveChance && GetMoveForShot(Shot.ShotType, Move, Distance))
			{
				Shot.bMove = true;
				Shot.Move = Move;
				Shot.MoveDistance = Distance;
				Shot.MoveStart = 0.f;
				Shot.MoveEnd = Length;
				if (Emphasis >= 0.f && !Shot.bIsReaction)
				{
					// Land the push on the stressed word.
					Shot.MoveStart = FMath::Max(0.f, Emphasis - 1.4f);
					Shot.MoveEnd = FMath::Min(Length, Emphasis + 0.25f);
					Why += TEXT(", pushing in on the emphasis");
				}
				else
				{
					Why += TEXT(", moving");
				}
			}
		}

		// Action: a crash zoom or slow motion on an impact that lands in this shot.
		if (Line.bAction)
		{
			const float ShotStart = Time - StartTime;
			const float* Impact = Impacts.FindByPredicate([ShotStart, Length](float I) { return I >= ShotStart && I < ShotStart + Length; });
			if (Impact)
			{
				const float ImpactInShot = *Impact - ShotStart;
				if (SlowMoBudget > 0 && Random.FRand() < Style.SlowMoChance)
				{
					Shot.bSlowMo = true;
					Shot.SlowMoTime = ImpactInShot;
					--SlowMoBudget;
					Why += TEXT(", speed ramp on the hit");
				}
				else if (!Shot.bMove && Random.FRand() < Style.CrashZoomChance)
				{
					Shot.bMove = true;
					Shot.Move = ECinemafyCameraMove::SnapZoomIn;
					Shot.MoveDistance = 100.f;
					Shot.MoveStart = FMath::Max(0.f, ImpactInShot - 0.05f);
					Shot.MoveEnd = FMath::Min(Length, Shot.MoveStart + 0.2f);
					Why += TEXT(", crash zoom on the hit");
				}
			}
		}

		if (Segment == 0 && Line.bSetEmotion)
		{
			Shot.bSetEmotion = true;
			Shot.Emotion = Line.Emotion;
		}

		// Calm moments get the calm camera; heated ones get the operator's adrenaline.
		const float FeelRoll = FMath::Clamp((Intensity - 0.4f) / 0.3f, 0.f, 1.f);
		Shot.Feel = Random.FRand() < FeelRoll ? Style.IntenseFeel : Style.CalmFeel;

		const FString MomentTag = Line.Moment == ECinemafyMoment::Dialogue ? FString()
			: FString::Printf(TEXT("[%s] "), *StaticEnum<ECinemafyMoment>()->GetDisplayNameTextByValue(static_cast<int64>(Line.Moment)).ToString());
		Shot.Reason = FString::Printf(TEXT("%sLine %d: %s on %s (%s, intensity %.1f)"),
			*MomentTag, LineIndex + 1, *ShotName(Shot.ShotType), *ActorName(Shot.Subject), *Why, Intensity);

		Shots.Add(Shot);
		Last = Shot;
		bHasLast = true;
		Time += Length;
		Remaining -= Length;
	}
	return Shots;
}

TArray<AActor*> UCinemafyDirectorLibrary::GetCast(const TArray<FCinemafyLine>& Lines)
{
	TArray<AActor*> SceneCast;
	for (const FCinemafyLine& Line : Lines)
	{
		if (Line.Speaker) { SceneCast.AddUnique(Line.Speaker); }
		if (Line.Listener) { SceneCast.AddUnique(Line.Listener); }
	}
	return SceneCast;
}

TArray<FCinemafyPlannedShot> UCinemafyDirectorLibrary::PlanScene(const TArray<FCinemafyLine>& InLines, const FCinemafyDirectorStyle& Style, int32 Seed)
{
	// Read the scene first: every tool choice below starts from what kind of moment each line is.
	TArray<FCinemafyLine> Lines = InLines;
	ReadMoments(Lines);

	FRandomStream Random(Seed);

	// Each scene (cross-cut scenes are separate) has its own cast.
	TMap<int32, TArray<AActor*>> CastByScene;
	for (const FCinemafyLine& Line : Lines)
	{
		TArray<AActor*>& SceneCast = CastByScene.FindOrAdd(Line.SceneId);
		if (Line.Speaker) { SceneCast.AddUnique(Line.Speaker); }
		if (Line.Listener) { SceneCast.AddUnique(Line.Listener); }
	}
	TArray<FCinemafyPlannedShot> Plan;
	float Time = 0.f;

	int32 WipeCount = 0;
	auto ResolveWipe = [&Style, &WipeCount]()
	{
		if (Style.WipeDirection != ECinemafyWipeDirection::Alternate)
		{
			return Style.WipeDirection;
		}
		// Vary it, like the serials did: across, back, a clock sweep, down, an iris, up.
		const ECinemafyWipeDirection Cycle[] = { ECinemafyWipeDirection::LeftToRight, ECinemafyWipeDirection::RightToLeft,
			ECinemafyWipeDirection::Clock, ECinemafyWipeDirection::TopToBottom, ECinemafyWipeDirection::IrisOut, ECinemafyWipeDirection::BottomToTop };
		return Cycle[WipeCount++ % UE_ARRAY_COUNT(Cycle)];
	};

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FCinemafyLine& Line = Lines[Index];
		const bool bOpening = Plan.Num() == 0;
		const bool bBreak = !bOpening && Line.bSceneBreak;
		const ECinemafyTransition Transition = Line.bOverrideTransition ? Line.Transition
			: (bOpening ? Style.OpeningTransition : (bBreak ? Style.SceneBreakTransition : ECinemafyTransition::Cut));

		const FCinemafyPlannedShot* Previous = Plan.Num() > 0 ? &Plan.Last() : nullptr;
		const FCinemafyPlannedShot PreviousCopy = Previous ? *Previous : FCinemafyPlannedShot();

		// A scene break re-establishes the new place, unless it's a match cut (which keeps the framing).
		FCinemafyLine PlannedLine = Line;
		if (Transition == ECinemafyTransition::MatchCut && Previous &&
			(!UCinemafyFramingLibrary::ShotNeedsCounterpart(Previous->ShotType) || Line.Listener))
		{
			PlannedLine.bForceShot = true;
			PlannedLine.ForcedShot = Previous->ShotType;
		}
		const bool bEstablish = bOpening || (bBreak && Transition != ECinemafyTransition::MatchCut);

		TArray<FCinemafyPlannedShot> LineShots = PlanLine(PlannedLine, Index, Style, Random,
			(Previous && !bBreak) ? &PreviousCopy : nullptr, bEstablish, Time, CastByScene.FindRef(Line.SceneId));

		if (LineShots.Num() > 0 && (bOpening || bBreak || Line.bOverrideTransition))
		{
			FCinemafyPlannedShot& First = LineShots[0];
			First.TransitionIn = Transition;
			First.TransitionDuration = Style.TransitionDuration;
			First.WipeDirection = ResolveWipe();
			if (Transition != ECinemafyTransition::Cut)
			{
				First.Reason += FString::Printf(TEXT(" [%s in]"), *StaticEnum<ECinemafyTransition>()->GetDisplayNameTextByValue(static_cast<int64>(Transition)).ToString());
			}
		}

		Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
		Plan.Append(LineShots);
	}

	// Hold-through: a two-shot that already shows both speakers can stay up across a change of speaker,
	// racking focus instead of cutting.
	for (int32 Index = 0; Index + 1 < Plan.Num(); ++Index)
	{
		FCinemafyPlannedShot& Held = Plan[Index];
		const FCinemafyPlannedShot& Next = Plan[Index + 1];
		if (!Next.bLineStart || Next.TransitionIn != ECinemafyTransition::Cut || !IsTwoShot(Held.ShotType)
			|| Held.ShotType == ECinemafyShotType::GroupShot || !Lines.IsValidIndex(Next.LineIndex))
		{
			continue;
		}

		const FCinemafyLine& NextLine = Lines[Next.LineIndex];
		const bool bSamePair = (NextLine.Speaker == Held.Subject && NextLine.Listener == Held.Counterpart)
			|| (NextLine.Speaker == Held.Counterpart && NextLine.Listener == Held.Subject);
		if (!bSamePair || Random.FRand() >= Style.HoldThroughChance)
		{
			continue;
		}

		// Don't create a jump cut with whatever follows the absorbed shot.
		if (Plan.IsValidIndex(Index + 2) && Plan[Index + 2].ShotType == Held.ShotType && Plan[Index + 2].Subject == Held.Subject)
		{
			continue;
		}

		FCinemafyRackEvent Rack;
		Rack.Time = Next.StartTime - Held.StartTime;
		Rack.FocusOn = NextLine.Speaker;
		Held.RackEvents.Add(Rack);
		Held.Duration = (Next.StartTime + Next.Duration) - Held.StartTime;
		Held.Reason += TEXT(", holding through the exchange (rack focus)");
		Plan.RemoveAt(Index + 1);
		--Index; // It may hold through the next exchange too.
	}

	if (Plan.Num() > 0 && Style.ClosingTransition != ECinemafyTransition::Cut && Style.ClosingTransition != ECinemafyTransition::MatchCut)
	{
		FCinemafyPlannedShot& Last = Plan.Last();
		Last.TransitionOut = Style.ClosingTransition;
		Last.TransitionDuration = Style.TransitionDuration;
		if (Last.TransitionIn == ECinemafyTransition::Cut)
		{
			Last.WipeDirection = ResolveWipe();
		}
	}
	return Plan;
}

void UCinemafyDirectorLibrary::SnapPlanToBeats(TArray<FCinemafyPlannedShot>& Plan, const TArray<FCinemafyLine>& Lines, const TArray<float>& Beats, float MinShotLength)
{
	if (Beats.Num() < 2 || Plan.Num() < 2)
	{
		return;
	}

	// Any cut is at most half a beat from a beat, so "nearest beat" always finds one.
	const float BeatPeriod = (Beats.Last() - Beats[0]) / (Beats.Num() - 1);
	const float Tolerance = BeatPeriod * 0.5f + KINDA_SMALL_NUMBER;

	for (int32 Index = 1; Index < Plan.Num(); ++Index)
	{
		FCinemafyPlannedShot& Before = Plan[Index - 1];
		FCinemafyPlannedShot& After = Plan[Index];

		// Dialogue owns its own timing: never move the cut that starts a spoken line.
		if (After.bLineStart && Lines.IsValidIndex(After.LineIndex) && Lines[After.LineIndex].Audio)
		{
			continue;
		}

		const float Cut = After.StartTime;
		float Best = Cut;
		float BestDistance = Tolerance;
		for (const float Beat : Beats)
		{
			const float Distance = FMath::Abs(Beat - Cut);
			if (Distance < BestDistance && Beat - Before.StartTime >= MinShotLength && (After.StartTime + After.Duration) - Beat >= MinShotLength)
			{
				Best = Beat;
				BestDistance = Distance;
			}
		}
		if (Best == Cut)
		{
			continue;
		}

		const float Shift = Best - Cut;
		Before.Duration += Shift;
		After.StartTime = Best;
		After.Duration -= Shift;

		// The second shot now starts at a different time: its internal timings shift with it.
		for (FCinemafyRackEvent& Rack : After.RackEvents)
		{
			Rack.Time -= Shift;
		}
		After.MoveStart -= Shift;
		After.MoveEnd -= Shift;
		After.SlowMoTime -= Shift;

		// Keep everything timed inside the shots inside them.
		for (FCinemafyPlannedShot* Shot : { &Before, &After })
		{
			Shot->MoveStart = FMath::Clamp(Shot->MoveStart, 0.f, Shot->Duration);
			Shot->MoveEnd = FMath::Clamp(Shot->MoveEnd, Shot->MoveStart, Shot->Duration);
			Shot->SlowMoTime = FMath::Clamp(Shot->SlowMoTime, 0.f, Shot->Duration);
			Shot->RackEvents.RemoveAll([Shot](const FCinemafyRackEvent& Rack) { return Rack.Time <= 0.f || Rack.Time >= Shot->Duration; });
		}
		Before.Reason += TEXT(", cut on the beat");
	}
}

void UCinemafyDirectorLibrary::ApplyLockedShots(TArray<FCinemafyPlannedShot>& Plan, const TArray<FCinemafyPlannedShot>& Locked)
{
	constexpr float MinRemainder = 0.3f;

	for (const FCinemafyPlannedShot& Lock : Locked)
	{
		const float LockStart = Lock.StartTime;
		const float LockEnd = Lock.StartTime + Lock.Duration;

		TArray<FCinemafyPlannedShot> Result;
		for (const FCinemafyPlannedShot& Shot : Plan)
		{
			const float Start = Shot.StartTime;
			const float End = Shot.StartTime + Shot.Duration;
			if (End <= LockStart || Start >= LockEnd)
			{
				Result.Add(Shot);
				continue;
			}
			// Keep whatever part sticks out on either side of the locked shot.
			if (Start < LockStart && LockStart - Start >= MinRemainder)
			{
				FCinemafyPlannedShot Left = Shot;
				Left.Duration = LockStart - Start;
				Left.MoveEnd = FMath::Min(Left.MoveEnd, Left.Duration);
				Left.MoveStart = FMath::Min(Left.MoveStart, Left.MoveEnd);
				Left.RackEvents.RemoveAll([&Left](const FCinemafyRackEvent& Rack) { return Rack.Time >= Left.Duration; });
				Result.Add(Left);
			}
			if (End > LockEnd && End - LockEnd >= MinRemainder)
			{
				FCinemafyPlannedShot Right = Shot;
				const float Shift = LockEnd - Start;
				Right.StartTime = LockEnd;
				Right.Duration = End - LockEnd;
				Right.bLineStart = false;
				Right.TransitionIn = ECinemafyTransition::Cut;
				Right.bMove = false;
				Right.bSlowMo = false;
				for (FCinemafyRackEvent& Rack : Right.RackEvents)
				{
					Rack.Time -= Shift;
				}
				Right.RackEvents.RemoveAll([&Right](const FCinemafyRackEvent& Rack) { return Rack.Time <= 0.f || Rack.Time >= Right.Duration; });
				Result.Add(Right);
			}
		}
		Result.Add(Lock);
		Result.Sort([](const FCinemafyPlannedShot& A, const FCinemafyPlannedShot& B) { return A.StartTime < B.StartTime; });

		// Close any slivers left between pieces by stretching the shot before them.
		for (int32 Index = 1; Index < Result.Num(); ++Index)
		{
			FCinemafyPlannedShot& Before = Result[Index - 1];
			FCinemafyPlannedShot& After = Result[Index];
			const float Gap = After.StartTime - (Before.StartTime + Before.Duration);
			if (Gap <= 0.f)
			{
				continue;
			}
			if (!Before.bLocked)
			{
				Before.Duration += Gap;
			}
			else if (!After.bLocked)
			{
				// The shot after a locked one reaches back to meet it.
				After.StartTime -= Gap;
				After.Duration += Gap;
				for (FCinemafyRackEvent& Rack : After.RackEvents)
				{
					Rack.Time += Gap;
				}
				After.MoveStart += Gap;
				After.MoveEnd += Gap;
				After.SlowMoTime += Gap;
			}
		}
		Plan = MoveTemp(Result);
	}
}

bool UCinemafyDirectorLibrary::SwapForAlternate(TArray<FCinemafyPlannedShot>& Plan, int32 Index, const TArray<FCinemafyLine>& Lines, const FCinemafyDirectorStyle& Style)
{
	if (!Plan.IsValidIndex(Index))
	{
		return false;
	}

	FCinemafyPlannedShot& Shot = Plan[Index];
	const FCinemafyPlannedShot* Before = Plan.IsValidIndex(Index - 1) ? &Plan[Index - 1] : nullptr;
	const FCinemafyPlannedShot* After = Plan.IsValidIndex(Index + 1) ? &Plan[Index + 1] : nullptr;
	const bool bGroupScene = Shot.Group.Num() > 0;

	// Rank every legal option by how much the style likes it, then take the best one that isn't the current shot.
	TArray<TPair<ECinemafyShotType, float>> Options;
	for (ECinemafyShotType Type : AllShotTypes)
	{
		if (Type == Shot.ShotType || Type == ECinemafyShotType::TrackingLead || Type == ECinemafyShotType::TrackingFollow)
		{
			continue;
		}
		if ((UCinemafyFramingLibrary::ShotNeedsCounterpart(Type) && !Shot.Counterpart) || (Type == ECinemafyShotType::GroupShot && !bGroupScene))
		{
			continue;
		}
		// Swapping must not create a jump cut with either neighbour.
		if ((Before && Before->Subject == Shot.Subject && Before->ShotType == Type) || (After && After->Subject == Shot.Subject && After->ShotType == Type))
		{
			continue;
		}
		const float* Preference = Style.ShotPreference.Find(Type);
		const float Closeness = FMath::Exp(-FMath::Square(GetShotSize(Type) - GetShotSize(Shot.ShotType)) / 4.f);
		Options.Emplace(Type, (Preference ? *Preference : 1.f) * Closeness);
	}
	if (Options.Num() == 0)
	{
		return false;
	}
	Options.Sort([](const TPair<ECinemafyShotType, float>& A, const TPair<ECinemafyShotType, float>& B) { return A.Value > B.Value; });

	Shot.ShotType = Options[0].Key;
	Shot.Lens = BiasLens(UCinemafyFramingLibrary::GetRecommendedPrime(Shot.ShotType), Style.LensBias);
	Shot.bMove = false;
	Shot.RackEvents.Reset();
	Shot.FocusOn = IsTwoShot(Shot.ShotType) && Lines.IsValidIndex(Shot.LineIndex) ? Lines[Shot.LineIndex].Speaker.Get() : Shot.Subject.Get();
	if (Shot.ShotType == ECinemafyShotType::GroupShot && Shot.Group.Num() == 0)
	{
		return false;
	}
	Shot.bLocked = true;
	Shot.Reason = FString::Printf(TEXT("Line %d: %s on %s (alternate, chosen by hand)"),
		Shot.LineIndex + 1, *ShotName(Shot.ShotType), *ActorName(Shot.Subject));
	return true;
}

bool UCinemafyDirectorLibrary::NudgeCut(TArray<FCinemafyPlannedShot>& Plan, int32 Index, float DeltaSeconds, float MinShotLength)
{
	if (!Plan.IsValidIndex(Index) || !Plan.IsValidIndex(Index - 1))
	{
		return false;
	}

	FCinemafyPlannedShot& Before = Plan[Index - 1];
	FCinemafyPlannedShot& After = Plan[Index];
	const float Delta = FMath::Clamp(DeltaSeconds, MinShotLength - Before.Duration, After.Duration - MinShotLength);
	if (FMath::IsNearlyZero(Delta))
	{
		return false;
	}

	Before.Duration += Delta;
	After.StartTime += Delta;
	After.Duration -= Delta;
	for (FCinemafyPlannedShot* Shot : { &Before, &After })
	{
		Shot->MoveStart = FMath::Clamp(Shot->MoveStart, 0.f, Shot->Duration);
		Shot->MoveEnd = FMath::Clamp(Shot->MoveEnd, Shot->MoveStart, Shot->Duration);
		Shot->RackEvents.RemoveAll([Shot](const FCinemafyRackEvent& Rack) { return Rack.Time <= 0.f || Rack.Time >= Shot->Duration; });
		Shot->bLocked = true;
	}
	return true;
}

void UCinemafyDirectorLibrary::ReadMoments(TArray<FCinemafyLine>& Lines)
{
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		FCinemafyLine& Line = Lines[Index];
		const FCinemafyLine* Previous = Lines.IsValidIndex(Index - 1) ? &Lines[Index - 1] : nullptr;

		// Audio that's all hits and no talk (and no written dialogue) is an action beat.
		const FCinemafyAudioAnalysis& A = Line.Analysis;
		if (!Line.bAction && A.bValid && Line.Text.IsEmpty() && A.ActiveRatio < 0.35f && (A.PeakDb - A.LoudnessDb) > 15.f)
		{
			Line.bAction = true;
		}

		if (Line.bAction)
		{
			Line.Moment = ECinemafyMoment::Action;
		}
		else if (Line.bMoving)
		{
			Line.Moment = ECinemafyMoment::WalkAndTalk;
		}
		else if (Line.Intensity >= 0.85f || (Previous && Line.Intensity - Previous->Intensity >= 0.3f))
		{
			Line.Moment = ECinemafyMoment::Reveal;
		}
		else if (Line.PauseAfter >= 1.5f)
		{
			Line.Moment = ECinemafyMoment::SilentBeat;
		}
		else if (Previous && !Line.bSceneBreak && Line.Speaker != Previous->Speaker && Line.Speaker != Previous->Listener)
		{
			// Someone new joins the conversation.
			Line.Moment = ECinemafyMoment::NewExchange;
		}
		else
		{
			Line.Moment = ECinemafyMoment::Dialogue;
		}
	}
}

FString UCinemafyDirectorLibrary::SummarizeDecisions(const TArray<FCinemafyPlannedShot>& Plan, const TArray<FCinemafyLine>& Lines)
{
	TMap<ECinemafyMoment, int32> Moments;
	for (const FCinemafyLine& Line : Lines)
	{
		Moments.FindOrAdd(Line.Moment)++;
	}

	int32 Transitions = 0, Holds = 0, PushIns = 0, CrashZooms = 0, SlowMo = 0, Tracking = 0, Reactions = 0, OnBeat = 0, Locked = 0;
	float Total = 0.f;
	for (const FCinemafyPlannedShot& Shot : Plan)
	{
		Transitions += (Shot.TransitionIn != ECinemafyTransition::Cut ? 1 : 0) + (Shot.TransitionOut != ECinemafyTransition::Cut ? 1 : 0);
		Holds += Shot.RackEvents.Num() > 0 ? 1 : 0;
		PushIns += (Shot.bMove && Shot.Move != ECinemafyCameraMove::SnapZoomIn) ? 1 : 0;
		CrashZooms += (Shot.bMove && Shot.Move == ECinemafyCameraMove::SnapZoomIn) ? 1 : 0;
		SlowMo += Shot.bSlowMo ? 1 : 0;
		Tracking += (Shot.ShotType == ECinemafyShotType::TrackingLead || Shot.ShotType == ECinemafyShotType::TrackingFollow) ? 1 : 0;
		Reactions += Shot.bIsReaction ? 1 : 0;
		OnBeat += Shot.Reason.Contains(TEXT("cut on the beat")) ? 1 : 0;
		Locked += Shot.bLocked ? 1 : 0;
		Total += Shot.Duration;
	}

	FString Moment;
	for (const TPair<ECinemafyMoment, int32>& Pair : Moments)
	{
		Moment += FString::Printf(TEXT("%s%d %s"), Moment.IsEmpty() ? TEXT("") : TEXT(", "), Pair.Value,
			*StaticEnum<ECinemafyMoment>()->GetDisplayNameTextByValue(static_cast<int64>(Pair.Key)).ToString().ToLower());
	}

	FString Tools = FString::Printf(TEXT("%d shots over %.1fs (avg %.1fs), %d reactions"),
		Plan.Num(), Total, Plan.Num() > 0 ? Total / Plan.Num() : 0.f, Reactions);
	auto Add = [&Tools](int32 Count, const TCHAR* What) { if (Count > 0) { Tools += FString::Printf(TEXT(", %d %s"), Count, What); } };
	Add(Transitions, TEXT("transitions"));
	Add(Holds, TEXT("held two-shots with rack focus"));
	Add(PushIns, TEXT("camera moves"));
	Add(CrashZooms, TEXT("crash zooms"));
	Add(SlowMo, TEXT("speed ramps"));
	Add(Tracking, TEXT("tracking shots"));
	Add(OnBeat, TEXT("cuts on the beat"));
	Add(Locked, TEXT("locked shots"));

	return FString::Printf(TEXT("Read the scene as: %s.\nCut it with: %s."), *Moment, *Tools);
}

ECinemafyEmotion UCinemafyDirectorLibrary::ReadLineTone(const FCinemafyLine& Line)
{
	if (Line.bSetEmotion)
	{
		return Line.Emotion;
	}
	const FString Text = Line.Text.ToString();
	int32 Exclamations = 0;
	for (const TCHAR Char : Text)
	{
		Exclamations += Char == TEXT('!') ? 1 : 0;
	}
	if (Line.Intensity >= 0.8f || (Exclamations > 0 && Line.Intensity >= 0.6f))
	{
		return ECinemafyEmotion::Angry;
	}
	if (Line.Intensity >= 0.6f)
	{
		return ECinemafyEmotion::Determined;
	}
	if (Text.TrimEnd().EndsWith(TEXT("...")) && Line.Intensity <= 0.4f)
	{
		return ECinemafyEmotion::Sad;
	}
	return ECinemafyEmotion::Neutral;
}

float UCinemafyDirectorLibrary::ScoreDialogueAnimation(const FCinemafyLine& Line, bool bSpeaking, const TArray<FString>& AnimationTokens)
{
	auto Has = [&AnimationTokens](std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* Word : Words)
		{
			if (AnimationTokens.Contains(Word) || AnimationTokens.Contains(FString(TEXT("@")) + Word))
			{
				return true;
			}
		}
		return false;
	};

	// Poses that need a set (seat, wall, curb), moving clips, fights and couples' clips: never under a line.
	if (Has({ TEXT("sit"), TEXT("siting"), TEXT("sitting"), TEXT("seated"), TEXT("bench"), TEXT("bar"), TEXT("chair"), TEXT("curb"),
			TEXT("lie"), TEXT("lying"), TEXT("wall"), TEXT("static"), TEXT("leaning"), TEXT("rm"), TEXT("walk"), TEXT("run"), TEXT("jog"),
			TEXT("sprint"), TEXT("hit"), TEXT("attack"), TEXT("death"), TEXT("deaths"), TEXT("die"), TEXT("punch"), TEXT("kick"),
			TEXT("combo"), TEXT("dodge"), TEXT("block"), TEXT("jump"), TEXT("fall"), TEXT("hug"), TEXT("hugging"), TEXT("kiss"),
			TEXT("kissing"), TEXT("romantic"), TEXT("ue4"), TEXT("calling"), TEXT("waving") }))
	{
		return -1.f;
	}

	// The role: what a talker does with their hands, what a listener does while they wait.
	const bool bTalk = Has({ TEXT("talk"), TEXT("explain"), TEXT("gesture"), TEXT("whisper"), TEXT("insist"), TEXT("emphatic") });
	const bool bArguing = Has({ TEXT("arguing"), TEXT("argue"), TEXT("argument") });
	const bool bListen = Has({ TEXT("listen"), TEXT("listening"), TEXT("nod"), TEXT("crossarms"), TEXT("shrug"), TEXT("sigh"),
		TEXT("shocked"), TEXT("surprise"), TEXT("embarrassed") });
	const bool bDialogueIdle = Has({ TEXT("idle") }) && Has({ TEXT("dialogue") });

	float Score = 0.f;
	if (bSpeaking)
	{
		Score = bTalk ? 2.f : bArguing ? 1.5f : 0.f;
	}
	else
	{
		Score = bListen ? 2.f : bDialogueIdle ? 1.5f : bArguing ? 1.f : 0.f;
		if (bTalk && !bListen)
		{
			return -1.f;
		}
	}
	if (Score <= 0.f)
	{
		return -1.f;
	}

	// The tone: words that fit it score, words of another strong tone count against.
	const ECinemafyEmotion Tone = ReadLineTone(Line);
	const bool bHigh = Line.Intensity >= 0.6f;
	// Fits: words that suit the tone, its signature words (the tone itself, "sad" for Sad) first.
	TArray<const TCHAR*> Fits;
	int32 Signature = 1;
	switch (Tone)
	{
	case ECinemafyEmotion::Angry:
	case ECinemafyEmotion::Contempt:
	case ECinemafyEmotion::Disgust:
		Fits = { TEXT("furious"), TEXT("arguing"), TEXT("emphatic"), TEXT("insist"), TEXT("crossarms"), TEXT("nono"), TEXT("no") };
		Signature = 2;
		break;
	case ECinemafyEmotion::Sad:
	case ECinemafyEmotion::Worried:
		Fits = { TEXT("sad"), TEXT("sigh"), TEXT("serious"), TEXT("whisper"), TEXT("embarrassed") };
		break;
	case ECinemafyEmotion::Happy:
		Fits = { TEXT("laugh"), TEXT("smile"), TEXT("light"), TEXT("yes"), TEXT("calm") };
		Signature = 2;
		break;
	case ECinemafyEmotion::Surprise:
	case ECinemafyEmotion::Fear:
		Fits = { TEXT("shocked"), TEXT("surprise"), TEXT("whisper"), TEXT("serious") };
		Signature = 2;
		break;
	case ECinemafyEmotion::Determined:
		Fits = { TEXT("serious"), TEXT("emphatic"), TEXT("insist"), TEXT("explain"), TEXT("up") };
		break;
	default:
		Fits = bHigh ? TArray<const TCHAR*>{ TEXT("serious"), TEXT("emphatic"), TEXT("explain") }
			: TArray<const TCHAR*>{ TEXT("calm"), TEXT("talk"), TEXT("explain"), TEXT("listen"), TEXT("neutral"), TEXT("nod"), TEXT("low") };
		break;
	}
	if (!bSpeaking && Line.Moment == ECinemafyMoment::Reveal)
	{
		Fits.Add(TEXT("shocked"));
		Fits.Add(TEXT("surprise"));
	}

	for (int32 Index = 0; Index < Fits.Num(); ++Index)
	{
		const float Weight = Index < Signature ? 2.5f : 1.5f;
		if (AnimationTokens.Contains(Fits[Index]))
		{
			Score += Weight;
		}
		else if (AnimationTokens.Contains(FString(TEXT("@")) + Fits[Index]))
		{
			Score += Weight * 0.5f;
		}
	}

	static const TCHAR* StrongTones[] = { TEXT("furious"), TEXT("arguing"), TEXT("sad"), TEXT("laugh"), TEXT("smile"), TEXT("shocked"),
		TEXT("surprise"), TEXT("whisper"), TEXT("emphatic"), TEXT("embarrassed") };
	for (const TCHAR* Word : StrongTones)
	{
		if (!Fits.Contains(Word) && Has({ Word }))
		{
			Score -= 2.f;
		}
	}
	return Score;
}

int32 UCinemafyDirectorLibrary::PickDialogueAnimation(const FCinemafyLine& Line, bool bSpeaking, const TArray<FCinemafyMoveCandidate>& Candidates,
	int32 Seed, const TArray<FSoftObjectPath>& Recent)
{
	TArray<TPair<float, int32>> Scored;
	float Best = 0.f;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		float Score = ScoreDialogueAnimation(Line, bSpeaking, Candidates[Index].Tokens);
		if (Score <= 0.f)
		{
			continue;
		}
		Score += Candidates[Index].Bias;
		const int32 Used = Recent.Find(Candidates[Index].Path);
		if (Used != INDEX_NONE)
		{
			// The more recent, the bigger the penalty.
			Score -= 1.f + float(Used + 1) / float(Recent.Num());
		}
		Scored.Add({ Score, Index });
		Best = FMath::Max(Best, Score);
	}
	if (Scored.Num() == 0)
	{
		return INDEX_NONE;
	}

	// One of the close-to-best fits, so a scene doesn't repeat one clip; stable for a seed.
	Scored.Sort([&Candidates](const TPair<float, int32>& A, const TPair<float, int32>& B)
	{
		return A.Key != B.Key ? A.Key > B.Key : Candidates[A.Value].Name < Candidates[B.Value].Name;
	});
	int32 Close = 0;
	while (Close < Scored.Num() && Scored[Close].Key >= Best - 0.5f)
	{
		++Close;
	}
	FRandomStream Stream(Seed);
	return Scored[Stream.RandRange(0, Close - 1)].Value;
}
