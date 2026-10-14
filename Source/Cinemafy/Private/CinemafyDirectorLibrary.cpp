#include "CinemafyDirectorLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "GameFramework/Actor.h"

namespace
{
	const ECinemafyShotType AllShotTypes[] = {
		ECinemafyShotType::WideTwoShot, ECinemafyShotType::MediumTwoShot, ECinemafyShotType::OverTheShoulder,
		ECinemafyShotType::MediumShot, ECinemafyShotType::MediumCloseUp, ECinemafyShotType::CloseUp,
		ECinemafyShotType::ExtremeCloseUp, ECinemafyShotType::LowHero };

	const ECinemafyPrime AllPrimes[] = {
		ECinemafyPrime::Prime18, ECinemafyPrime::Prime24, ECinemafyPrime::Prime35,
		ECinemafyPrime::Prime50, ECinemafyPrime::Prime85, ECinemafyPrime::Prime135 };

	bool IsTwoShot(ECinemafyShotType Type)
	{
		return Type == ECinemafyShotType::WideTwoShot || Type == ECinemafyShotType::MediumTwoShot;
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
		const AActor* Subject, const AActor* Counterpart, bool bReaction, const FCinemafyPlannedShot* Previous)
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
		break;

	case ECinemafyDirectorPreset::NoirThriller:
		Prefs(0.7f, 0.5f, 1.1f, 0.8f, 1.2f, 1.2f, 0.6f, 1.0f);
		S.TightenWithIntensity = 0.7f;
		S.CalmShotLength = FVector2D(3.5, 6.0);
		S.IntenseShotLength = FVector2D(1.5, 3.0);
		S.ReactionShotChance = 0.4f;
		S.LowAngleBias = 0.6f;
		S.MoveChance = 0.4f;
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
		break;

	case ECinemafyDirectorPreset::ClassicHollywood:
	default:
		Prefs(1.0f, 1.0f, 1.4f, 0.8f, 1.2f, 1.0f, 0.3f, 0.3f);
		break;
	}
	return S;
}

TArray<FCinemafyPlannedShot> UCinemafyDirectorLibrary::PlanLine(const FCinemafyLine& Line, int32 LineIndex, const FCinemafyDirectorStyle& Style,
	FRandomStream& Random, const FCinemafyPlannedShot* Previous, bool bOpensScene, float StartTime)
{
	constexpr float MinShotLength = 0.8f;

	TArray<FCinemafyPlannedShot> Shots;
	if (!Line.Speaker)
	{
		return Shots;
	}

	const float Intensity = FMath::Clamp(Line.Intensity, 0.f, 1.f);
	const FVector2D LengthRange = FMath::Lerp(Style.CalmShotLength, Style.IntenseShotLength, Intensity);
	float Remaining = FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
	float Time = StartTime;
	FCinemafyPlannedShot Last = Previous ? *Previous : FCinemafyPlannedShot();
	bool bHasLast = Previous != nullptr;

	for (int32 Segment = 0; Remaining > 0.01f; ++Segment)
	{
		float Length = Random.FRandRange(LengthRange.X, FMath::Max(LengthRange.X, LengthRange.Y));
		if (Remaining - Length < MinShotLength)
		{
			Length = Remaining;
		}

		FCinemafyPlannedShot Shot;
		Shot.StartTime = Time;
		Shot.Duration = Length;
		Shot.LineIndex = LineIndex;
		Shot.Subject = Line.Speaker;
		Shot.Counterpart = Line.Listener;

		// Later in a line (and especially in the pause after it), cut to the listener's reaction.
		const bool bInPause = (Time - StartTime) >= Line.Duration;
		if (Line.Listener && Segment > 0 && Random.FRand() < Style.ReactionShotChance * (bInPause ? 1.6f : 1.f))
		{
			Shot.Subject = Line.Listener;
			Shot.Counterpart = Line.Speaker;
			Shot.bIsReaction = true;
		}

		FString Why;
		if (bOpensScene && Segment == 0 && Style.bEstablishingShot && Shot.Counterpart)
		{
			Shot.ShotType = ECinemafyShotType::WideTwoShot;
			Why = TEXT("establishing the space");
		}
		else if (Line.bForceShot && Segment == 0)
		{
			Shot.ShotType = Line.ForcedShot;
			Why = TEXT("forced by the script");
		}
		else
		{
			Shot.ShotType = PickShotType(Style, Random, Intensity, Shot.Subject, Shot.Counterpart, Shot.bIsReaction, bHasLast ? &Last : nullptr);
			Why = Shot.bIsReaction ? TEXT("reaction") : (Intensity > 0.7f ? TEXT("tightening for drama") : TEXT("coverage"));
		}

		Shot.Lens = BiasLens(UCinemafyFramingLibrary::GetRecommendedPrime(Shot.ShotType), Style.LensBias);

		// Moves: a creep-in on the establishing shot, push-ins as drama builds.
		{
			const bool bEstablishing = Why == TEXT("establishing the space");
			const float MoveChance = Style.MoveChance * (bEstablishing ? 1.5f : FMath::Lerp(0.6f, 1.4f, Intensity));
			ECinemafyCameraMove Move;
			float Distance;
			if (Random.FRand() < MoveChance && GetMoveForShot(Shot.ShotType, Move, Distance))
			{
				Shot.bMove = true;
				Shot.Move = Move;
				Shot.MoveDistance = Distance;
				Why += TEXT(", moving");
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

		Shot.Reason = FString::Printf(TEXT("Line %d: %s on %s (%s, intensity %.1f)"),
			LineIndex + 1, *ShotName(Shot.ShotType), *ActorName(Shot.Subject), *Why, Intensity);

		Shots.Add(Shot);
		Last = Shot;
		bHasLast = true;
		Time += Length;
		Remaining -= Length;
	}
	return Shots;
}

TArray<FCinemafyPlannedShot> UCinemafyDirectorLibrary::PlanScene(const TArray<FCinemafyLine>& Lines, const FCinemafyDirectorStyle& Style, int32 Seed)
{
	FRandomStream Random(Seed);
	TArray<FCinemafyPlannedShot> Plan;
	float Time = 0.f;

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FCinemafyPlannedShot* Previous = Plan.Num() > 0 ? &Plan.Last() : nullptr;
		const FCinemafyPlannedShot PreviousCopy = Previous ? *Previous : FCinemafyPlannedShot();
		TArray<FCinemafyPlannedShot> LineShots = PlanLine(Lines[Index], Index, Style, Random, Previous ? &PreviousCopy : nullptr, Plan.Num() == 0, Time);
		Time += FMath::Max(Lines[Index].Duration + Lines[Index].PauseAfter, 0.2f);
		Plan.Append(LineShots);
	}
	return Plan;
}
