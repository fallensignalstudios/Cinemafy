#include "CinemafyMood.h"

namespace CinemafyMoodWords
{
	int32 Count(const FString& Text, std::initializer_list<const TCHAR*> Words)
	{
		int32 Hits = 0;
		for (const TCHAR* Word : Words)
		{
			int32 From = 0;
			while (true)
			{
				const int32 At = Text.Find(Word, ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
				if (At == INDEX_NONE)
				{
					break;
				}
				++Hits;
				From = At + 1;
			}
		}
		return Hits;
	}
}

FCinemafySceneMood UCinemafyMoodLibrary::ReadSceneMood(const FCinemafyScriptScene& Scene, ECinemafyTimeOfDay PreviousTime)
{
	using namespace CinemafyMoodWords;
	FCinemafySceneMood Mood;
	TArray<FString> Why;

	// When and where.
	const FString Time = Scene.TimeOfDay.ToUpper();
	if (Time.Contains(TEXT("NIGHT")) || Time.Contains(TEXT("MIDNIGHT")))
	{
		Mood.TimeOfDay = ECinemafyTimeOfDay::Night;
	}
	else if (Time.Contains(TEXT("DUSK")) || Time.Contains(TEXT("SUNSET")) || Time.Contains(TEXT("EVENING")) || Time.Contains(TEXT("GOLDEN")))
	{
		Mood.TimeOfDay = ECinemafyTimeOfDay::Dusk;
	}
	else if (Time.Contains(TEXT("DAWN")) || Time.Contains(TEXT("SUNRISE")) || Time.Contains(TEXT("FIRST LIGHT")))
	{
		Mood.TimeOfDay = ECinemafyTimeOfDay::Dawn;
	}
	else if (Time.Contains(TEXT("DAY")) || Time.Contains(TEXT("MORNING")) || Time.Contains(TEXT("AFTERNOON")) || Time.Contains(TEXT("NOON")))
	{
		Mood.TimeOfDay = ECinemafyTimeOfDay::Day;
	}
	else if (Time.Contains(TEXT("CONTINUOUS")) || Time.Contains(TEXT("SAME")) || Time.Contains(TEXT("LATER")))
	{
		Mood.TimeOfDay = PreviousTime;
	}
	Mood.bExterior = Scene.Setting.Contains(TEXT("EXT"));
	if (Mood.TimeOfDay != ECinemafyTimeOfDay::Unknown)
	{
		Why.Add(StaticEnum<ECinemafyTimeOfDay>()->GetNameStringByValue(static_cast<int64>(Mood.TimeOfDay)).ToLower());
	}

	// The feeling, from what happens and what's said.
	FString Words = Scene.Heading + TEXT(" ") + Scene.Synopsis;
	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		Words += TEXT(" ") + Line.Text + TEXT(" ") + Line.Direction;
	}
	TMap<FString, int32> Scores;
	Scores.Add(TEXT("action"), (Scene.bHasAction ? 3 : 0) + Count(Words, { TEXT("fight"), TEXT("shoots"), TEXT("explod"), TEXT("chase"), TEXT("slash"), TEXT("attack") }));
	Scores.Add(TEXT("alarm"), 2 * Count(Words, { TEXT("red alert"), TEXT("alarm"), TEXT("siren"), TEXT("klaxon"), TEXT("evacuat") }));
	Scores.Add(TEXT("sad"), Count(Words, { TEXT("tears"), TEXT("cries"), TEXT("weeps"), TEXT("grief"), TEXT("funeral"), TEXT("grave"), TEXT("mourn"), TEXT("sobs"), TEXT("dying") }));
	Scores.Add(TEXT("tense"), Count(Words, { TEXT("whisper"), TEXT("shadow"), TEXT("silence"), TEXT("creeps"), TEXT("stand down"), TEXT("threat"), TEXT("glares"), TEXT("cold") }));
	Scores.Add(TEXT("eerie"), Count(Words, { TEXT("eerie"), TEXT("ghost"), TEXT("flicker"), TEXT("empty"), TEXT("abandoned"), TEXT("strange") }));
	Scores.Add(TEXT("warm"), Count(Words, { TEXT("laugh"), TEXT("smile"), TEXT("hug"), TEXT("toast"), TEXT("home"), TEXT("together") }));
	Scores.Add(TEXT("romantic"), 2 * Count(Words, { TEXT("kiss"), TEXT("embrace"), TEXT("love") }));
	Scores.Add(TEXT("triumphant"), Count(Words, { TEXT("victory"), TEXT("cheer"), TEXT("triumph"), TEXT("we won"), TEXT("celebrat") }));
	int32 Best = 1;
	for (const TPair<FString, int32>& Score : Scores)
	{
		if (Score.Value > Best)
		{
			Best = Score.Value;
			Mood.Mood = Score.Key;
		}
	}
	Why.Add(Mood.Mood);

	for (const TCHAR* Weather : { TEXT("rain"), TEXT("storm"), TEXT("snow"), TEXT("fog"), TEXT("fire"), TEXT("smoke") })
	{
		if (Count(Words, { Weather }) > 0)
		{
			Mood.Weather.Add(Weather);
		}
	}

	// Camera, grade and sun to suit.
	const bool bNight = Mood.TimeOfDay == ECinemafyTimeOfDay::Night;
	const bool bGolden = Mood.TimeOfDay == ECinemafyTimeOfDay::Dusk || Mood.TimeOfDay == ECinemafyTimeOfDay::Dawn;
	if (Mood.Mood == TEXT("action") || Mood.Mood == TEXT("alarm"))
	{
		Mood.DirectorStyle = ECinemafyDirectorPreset::KineticAction;
		Mood.Look = bNight ? ECinemafyLook::ColdSteel : ECinemafyLook::TealOrange;
	}
	else if (Mood.Mood == TEXT("sad") || Mood.Mood == TEXT("romantic"))
	{
		Mood.DirectorStyle = bGolden ? ECinemafyDirectorPreset::GoldenHourLyrical : ECinemafyDirectorPreset::IntimateDrama;
		Mood.Look = Mood.Mood == TEXT("sad") ? ECinemafyLook::BleachBypass : ECinemafyLook::WarmEpic;
	}
	else if (Mood.Mood == TEXT("tense") || Mood.Mood == TEXT("eerie"))
	{
		Mood.DirectorStyle = bNight ? ECinemafyDirectorPreset::NoirThriller : ECinemafyDirectorPreset::SlowBurn;
		Mood.Look = bNight ? ECinemafyLook::Noir : ECinemafyLook::ColdSteel;
	}
	else if (Mood.Mood == TEXT("triumphant"))
	{
		Mood.DirectorStyle = ECinemafyDirectorPreset::Epic;
		Mood.Look = ECinemafyLook::WarmEpic;
	}
	else
	{
		Mood.DirectorStyle = bGolden ? ECinemafyDirectorPreset::GoldenHourLyrical : ECinemafyDirectorPreset::ClassicHollywood;
		Mood.Look = bGolden || Mood.Mood == TEXT("warm") ? ECinemafyLook::WarmEpic : (bNight ? ECinemafyLook::ColdSteel : ECinemafyLook::CleanCinema);
	}

	switch (Mood.TimeOfDay)
	{
	case ECinemafyTimeOfDay::Dawn:  Mood.SunPitch = -6.f;  Mood.SunScale = 0.35f; Mood.SunColor = FLinearColor(1.f, 0.72f, 0.55f); break;
	case ECinemafyTimeOfDay::Dusk:  Mood.SunPitch = -5.f;  Mood.SunScale = 0.3f;  Mood.SunColor = FLinearColor(1.f, 0.6f, 0.35f); break;
	case ECinemafyTimeOfDay::Night: Mood.SunPitch = -35.f; Mood.SunScale = 0.02f; Mood.SunColor = FLinearColor(0.55f, 0.65f, 1.f); break; // moonlight
	default:                        Mood.SunPitch = -50.f; Mood.SunScale = 1.f;   Mood.SunColor = FLinearColor::White; break;
	}
	if (Mood.Weather.Contains(TEXT("storm")) || Mood.Weather.Contains(TEXT("rain")))
	{
		Mood.SunScale *= 0.4f;
		Why.Add(TEXT("overcast"));
	}

	Mood.Why = FString::Join(Why, TEXT(", "));
	return Mood;
}
