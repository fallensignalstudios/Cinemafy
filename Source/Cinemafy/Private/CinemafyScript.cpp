#include "CinemafyScript.h"
#include "CinemafyModule.h"
#include "LevelSequence.h"
#include "Sound/SoundBase.h"
#include "Misc/Crc.h"
#include "Engine/World.h"

FCinemafyScriptEditorRequest UCinemafyScriptProject::OnEditorRequest;

// ---------------------------------------------------------------------------------------------------------------------
// Reading the page
// ---------------------------------------------------------------------------------------------------------------------

namespace CinemafyScriptText
{
	bool IsAnyOf(const FString& Word, std::initializer_list<const TCHAR*> Options)
	{
		for (const TCHAR* Option : Options)
		{
			if (Word.Equals(Option, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Needles)
	{
		for (const TCHAR* Needle : Needles)
		{
			if (Text.Contains(Needle, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** No lowercase letters and at least one letter. */
	bool IsAllCaps(const FString& Text)
	{
		bool bLetter = false;
		for (const TCHAR C : Text)
		{
			if (FChar::IsLower(C))
			{
				return false;
			}
			bLetter |= FChar::IsAlpha(C);
		}
		return bLetter;
	}

	FString FirstWord(const FString& Text)
	{
		FString Word;
		for (const TCHAR C : Text)
		{
			if (FChar::IsWhitespace(C) || C == TEXT(':') || C == TEXT('.'))
			{
				break;
			}
			Word.AppendChar(C);
		}
		return Word;
	}

	int32 CountWords(const FString& Text)
	{
		TArray<FString> Words;
		Text.ParseIntoArrayWS(Words);
		return Words.Num();
	}

	/** Lowercase letters, digits and single spaces: for comparing lines however they were punctuated. */
	FString Normalize(const FString& Text)
	{
		FString Out;
		bool bSpace = false;
		for (const TCHAR C : Text.ToLower())
		{
			if (FChar::IsAlnum(C))
			{
				Out.AppendChar(C);
				bSpace = false;
			}
			else if (!bSpace && Out.Len() > 0)
			{
				Out.AppendChar(TEXT(' '));
				bSpace = true;
			}
		}
		return Out.TrimEnd();
	}

	FString TitleCase(const FString& Text)
	{
		FString Out = Text.ToLower();
		bool bStart = true;
		for (int32 Index = 0; Index < Out.Len(); ++Index)
		{
			if (bStart && FChar::IsAlpha(Out[Index]))
			{
				Out[Index] = FChar::ToUpper(Out[Index]);
			}
			bStart = !FChar::IsAlnum(Out[Index]) && Out[Index] != TEXT('\'');
		}
		return Out;
	}

	/** "KADE (V.O.)", "THANE (CONT'D)", "VOSS ^" -> "KADE", "THANE", "VOSS". */
	FString CleanCue(const FString& Cue, FString* OutExtension = nullptr)
	{
		FString Name = Cue.TrimStartAndEnd();
		Name.RemoveFromStart(TEXT("@"));
		Name.RemoveFromEnd(TEXT("^"));
		int32 Paren = INDEX_NONE;
		if (Name.FindChar(TEXT('('), Paren))
		{
			if (OutExtension)
			{
				*OutExtension = Name.Mid(Paren).TrimStartAndEnd();
			}
			Name = Name.Left(Paren);
		}
		return Name.TrimStartAndEnd();
	}

	bool IsTransition(const FString& Trim)
	{
		if (Trim.StartsWith(TEXT(">")) && !Trim.EndsWith(TEXT("<")))
		{
			return true;
		}
		if (!IsAllCaps(Trim))
		{
			return false;
		}
		return Trim.EndsWith(TEXT("TO:")) || IsAnyOf(Trim, { TEXT("FADE OUT."), TEXT("FADE OUT"), TEXT("FADE IN:"), TEXT("FADE IN"), TEXT("FADE TO BLACK."),
			TEXT("FADE TO BLACK"), TEXT("CUT TO BLACK."), TEXT("CUT TO BLACK"), TEXT("THE END"), TEXT("THE END."), TEXT("BLACKOUT."), TEXT("END CREDITS") });
	}

	/** A transition that closes a movement of the story (a strong place for an act or chapter break). */
	bool IsHardTransition(const FString& Trim)
	{
		return ContainsAny(Trim, { TEXT("FADE OUT"), TEXT("TO BLACK"), TEXT("THE END"), TEXT("BLACKOUT"), TEXT("END OF ACT") });
	}

	bool IsTimeJump(const FString& Text)
	{
		return ContainsAny(Text, { TEXT("LATER"), TEXT("NEXT DAY"), TEXT("NEXT MORNING"), TEXT("YEARS"), TEXT("MONTHS"), TEXT("WEEKS"),
			TEXT("FLASHBACK"), TEXT("PRESENT DAY"), TEXT("THE FOLLOWING"), TEXT("DAYS AGO"), TEXT("YEARS AGO") })
			&& !ContainsAny(Text, { TEXT("MOMENTS LATER"), TEXT("SECONDS LATER") });
	}

	bool IsContinuous(const FString& Text)
	{
		return ContainsAny(Text, { TEXT("CONTINUOUS"), TEXT("SAME TIME"), TEXT("MOMENTS LATER"), TEXT("SECONDS LATER"), TEXT("- SAME") });
	}

	/** "ACT TWO", "PART 1: THE FALL", "PROLOGUE". */
	bool IsActMarker(const FString& Trim, FString& OutTitle)
	{
		if (Trim.Len() > 60 || Trim.StartsWith(TEXT("END OF"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		const FString First = FirstWord(Trim);
		if (IsAnyOf(First, { TEXT("ACT"), TEXT("PART"), TEXT("BOOK") }) && Trim.Len() > First.Len() + 1 && (IsAllCaps(Trim) || FChar::IsUpper(Trim[0])))
		{
			// "Act" alone at the start of a sentence isn't a marker: it needs a number or a numbering word next.
			const FString Rest = Trim.Mid(First.Len()).TrimStartAndEnd();
			const FString Next = FirstWord(Rest);
			const bool bNumbered = Next.IsNumeric() || IsAnyOf(Next, { TEXT("one"), TEXT("two"), TEXT("three"), TEXT("four"), TEXT("five"), TEXT("six"),
				TEXT("seven"), TEXT("i"), TEXT("ii"), TEXT("iii"), TEXT("iv"), TEXT("v"), TEXT("vi"), TEXT("first"), TEXT("second"), TEXT("third"), TEXT("final"), TEXT("last") });
			if (bNumbered)
			{
				OutTitle = Trim;
				return true;
			}
		}
		if (IsAllCaps(Trim) && (IsAnyOf(Trim, { TEXT("PROLOGUE"), TEXT("EPILOGUE"), TEXT("TEASER"), TEXT("COLD OPEN"), TEXT("TAG") })
			|| ((First == TEXT("PROLOGUE") || First == TEXT("EPILOGUE")) && Trim.Contains(TEXT(":")))))
		{
			OutTitle = TitleCase(Trim);
			return true;
		}
		return false;
	}

	/** "CHAPTER 3", "Mission 4: Blackout", "LEVEL 2 - THE DOCKS", "EPISODE 1". */
	bool IsChapterMarker(const FString& Trim, FString& OutTitle)
	{
		if (Trim.Len() > 80)
		{
			return false;
		}
		const FString First = FirstWord(Trim);
		if (!IsAnyOf(First, { TEXT("CHAPTER"), TEXT("MISSION"), TEXT("LEVEL"), TEXT("QUEST"), TEXT("EPISODE"), TEXT("SEQUENCE"), TEXT("STAGE") }))
		{
			return false;
		}
		const FString Rest = Trim.Mid(First.Len()).TrimStartAndEnd();
		if (Rest.IsEmpty() || !(FChar::IsDigit(Rest[0]) || FChar::IsUpper(Rest[0]) || Rest[0] == TEXT(':')))
		{
			return false;
		}
		OutTitle = IsAllCaps(Trim) ? TitleCase(Trim) : Trim;
		return true;
	}

	/** Game-script scene markers: "[Scene: Bridge]", "SCENE 12: The Bridge", "Scene 3 - Hangar". */
	bool IsGameSceneMarker(const FString& Trim, FString& OutLocation)
	{
		FString Body = Trim;
		if (Body.StartsWith(TEXT("[")) && Body.EndsWith(TEXT("]")))
		{
			Body = Body.Mid(1, Body.Len() - 2).TrimStartAndEnd();
		}
		const FString First = FirstWord(Body);
		if (!First.Equals(TEXT("SCENE"), ESearchCase::IgnoreCase) || Body.Len() > 80)
		{
			return false;
		}
		FString Rest = Body.Mid(First.Len()).TrimStartAndEnd();
		// "Scene 12", "Scene: Hangar" - not "Scene after scene...".
		if (Rest.IsEmpty() || !(FChar::IsDigit(Rest[0]) || Rest[0] == TEXT(':') || Rest[0] == TEXT('-')))
		{
			return false;
		}
		// Skip the number, then any separator.
		while (Rest.Len() > 0 && (FChar::IsDigit(Rest[0]) || Rest[0] == TEXT('.')))
		{
			Rest.RightChopInline(1);
		}
		Rest = Rest.TrimStartAndEnd();
		while (Rest.Len() > 0 && (Rest[0] == TEXT(':') || Rest[0] == TEXT('-')))
		{
			Rest.RightChopInline(1);
			Rest = Rest.TrimStartAndEnd();
		}
		OutLocation = Rest.IsEmpty() ? Body : Rest;
		return true;
	}

	/** A character cue: an all-caps name, optionally with (V.O.) etc., not a heading or transition. */
	bool IsCue(const FString& Trim)
	{
		if (Trim.StartsWith(TEXT("@")))
		{
			return Trim.Len() > 1;
		}
		const FString Name = CleanCue(Trim);
		if (Name.IsEmpty() || Name.Len() > 38 || !IsAllCaps(Name) || Name.EndsWith(TEXT(":")) || (Name.EndsWith(TEXT(".")) && !Name.EndsWith(TEXT("DR."))))
		{
			return false;
		}
		for (const TCHAR C : Name)
		{
			if (!(FChar::IsAlnum(C) || C == TEXT(' ') || C == TEXT('.') || C == TEXT('\'') || C == TEXT('-') || C == TEXT('#') || C == TEXT('&')))
			{
				return false;
			}
		}
		FString Setting, Location, Time;
		if (UCinemafyScriptLibrary::ParseHeading(Trim, Setting, Location, Time) || IsTransition(Trim))
		{
			return false;
		}
		FString Ignored;
		return !IsActMarker(Trim, Ignored) && !IsChapterMarker(Trim, Ignored) && CountWords(Name) <= 4;
	}

	/** "KADE: Stand down." / "Thane: Make me." (game scripts). */
	bool IsColonDialogue(const FString& Trim, FString& OutSpeaker, FString& OutText, FString& OutDirection)
	{
		int32 Colon = INDEX_NONE;
		if (!Trim.FindChar(TEXT(':'), Colon) || Colon < 1 || Colon > 32 || Colon >= Trim.Len() - 1)
		{
			return false;
		}
		const FString Name = Trim.Left(Colon).TrimStartAndEnd();
		const FString Text = Trim.Mid(Colon + 1).TrimStartAndEnd();
		if (Name.IsEmpty() || Text.IsEmpty() || !FChar::IsUpper(Name[0]) || CountWords(Name) > 3)
		{
			return false;
		}
		// "Kade draws: pistol" is action; spoken lines start like sentences (or the name is in caps).
		if (FChar::IsLower(Text[0]) && !IsAllCaps(Name))
		{
			return false;
		}
		for (const TCHAR C : Name)
		{
			if (!(FChar::IsAlnum(C) || C == TEXT(' ') || C == TEXT('.') || C == TEXT('\'') || C == TEXT('-') || C == TEXT('(') || C == TEXT(')')))
			{
				return false;
			}
		}
		if (IsAnyOf(FirstWord(Name), { TEXT("Note"), TEXT("Notes"), TEXT("Setting"), TEXT("Location"), TEXT("Time"), TEXT("Scene"), TEXT("Chapter"), TEXT("Act"),
			TEXT("Mission"), TEXT("Objective"), TEXT("Title"), TEXT("Author"), TEXT("Draft"), TEXT("Level"), TEXT("Quest"), TEXT("Episode"), TEXT("Summary"),
			TEXT("Credit"), TEXT("Source"), TEXT("Contact"), TEXT("Cut"), TEXT("Fade"), TEXT("Smash"), TEXT("Dissolve") }))
		{
			return false;
		}
		OutSpeaker = CleanCue(Name, &OutDirection);
		OutText = Text;
		return true;
	}

	/** Strips Fountain boneyard (/* */) and notes ([[ ]]). */
	FString StripComments(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len());
		int32 Index = 0;
		while (Index < Text.Len())
		{
			if (Text[Index] == TEXT('/') && Index + 1 < Text.Len() && Text[Index + 1] == TEXT('*'))
			{
				const int32 End = Text.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 2);
				Index = End == INDEX_NONE ? Text.Len() : End + 2;
				continue;
			}
			if (Text[Index] == TEXT('[') && Index + 1 < Text.Len() && Text[Index + 1] == TEXT('['))
			{
				const int32 End = Text.Find(TEXT("]]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 2);
				Index = End == INDEX_NONE ? Text.Len() : End + 2;
				continue;
			}
			Out.AppendChar(Text[Index++]);
		}
		return Out;
	}

	/** The first couple of sentences, for a synopsis. */
	FString FirstSentences(const FString& Text, int32 MaxChars)
	{
		if (Text.Len() <= MaxChars)
		{
			return Text;
		}
		int32 Cut = MaxChars;
		for (int32 Index = MaxChars; Index > MaxChars / 2; --Index)
		{
			if (Text[Index] == TEXT('.') || Text[Index] == TEXT('!') || Text[Index] == TEXT('?'))
			{
				Cut = Index + 1;
				break;
			}
		}
		return Text.Left(Cut).TrimEnd() + (Cut == MaxChars ? TEXT("...") : TEXT(""));
	}

	bool DescribesAction(const FString& Text)
	{
		return ContainsAny(Text, { TEXT("fight"), TEXT("punch"), TEXT("kick"), TEXT("slash"), TEXT("stab"), TEXT("shoots"), TEXT("gunfire"), TEXT("fires"),
			TEXT("explosion"), TEXT("explodes"), TEXT("chase"), TEXT("sprints"), TEXT("vaults"), TEXT("leaps"), TEXT("attacks"), TEXT("duel"),
			TEXT("struggle"), TEXT("crashes"), TEXT("blade"), TEXT("sword"), TEXT("swings"), TEXT("dodges"), TEXT("tackles"), TEXT("brawl") });
	}
}

bool UCinemafyScriptLibrary::ParseHeading(const FString& Line, FString& OutSetting, FString& OutLocation, FString& OutTime)
{
	FString Text = Line.TrimStartAndEnd().Replace(TEXT("\u2013"), TEXT("-")).Replace(TEXT("\u2014"), TEXT("-"));
	// Scene numbers: "INT. BRIDGE - NIGHT #12#".
	int32 Hash = INDEX_NONE;
	if (Text.EndsWith(TEXT("#")) && Text.LeftChop(1).FindLastChar(TEXT('#'), Hash))
	{
		Text = Text.Left(Hash).TrimEnd();
	}

	FString Rest;
	if (Text.StartsWith(TEXT(".")) && Text.Len() > 1 && FChar::IsAlnum(Text[1]))
	{
		// Fountain's forced heading.
		OutSetting.Reset();
		Rest = Text.Mid(1);
	}
	else
	{
		const FString Upper = Text.ToUpper();
		static const TCHAR* Prefixes[] = { TEXT("INT./EXT."), TEXT("INT/EXT."), TEXT("INT/EXT"), TEXT("EXT./INT."), TEXT("EXT/INT"), TEXT("I/E."), TEXT("I/E"),
			TEXT("INT."), TEXT("EXT."), TEXT("EST."), TEXT("INT "), TEXT("EXT "), TEXT("EST ") };
		bool bFound = false;
		for (const TCHAR* Prefix : Prefixes)
		{
			if (Upper.StartsWith(Prefix))
			{
				const FString P(Prefix);
				OutSetting = P.Contains(TEXT("/")) ? TEXT("INT/EXT") : P.Left(3);
				Rest = Text.Mid(P.Len());
				bFound = true;
				break;
			}
		}
		// The prefix is written in caps ("INT."), even when the rest isn't.
		if (!bFound || !CinemafyScriptText::IsAllCaps(Text.Left(3)))
		{
			return false;
		}
	}

	Rest = Rest.TrimStartAndEnd();
	Rest.RemoveFromStart(TEXT("."));
	Rest = Rest.TrimStartAndEnd();
	int32 Dash = Rest.Find(TEXT(" - "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (Dash != INDEX_NONE)
	{
		OutLocation = Rest.Left(Dash).TrimStartAndEnd();
		OutTime = Rest.Mid(Dash + 3).TrimStartAndEnd();
	}
	else
	{
		OutLocation = Rest;
		OutTime.Reset();
	}
	return !OutLocation.IsEmpty();
}

FString UCinemafyScriptLibrary::FinalDraftToText(const FString& Xml)
{
	// Each <Paragraph Type="..."> becomes a Fountain-style paragraph; <Text> runs are joined.
	FString Out;
	int32 Cursor = 0;
	while (true)
	{
		const int32 Start = Xml.Find(TEXT("<Paragraph"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (Start == INDEX_NONE)
		{
			break;
		}
		const int32 End = Xml.Find(TEXT("</Paragraph>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			break;
		}
		const FString Para = Xml.Mid(Start, End - Start);
		Cursor = End + 12;

		FString Type;
		const int32 TypeAt = Para.Find(TEXT("Type=\""));
		if (TypeAt != INDEX_NONE)
		{
			const int32 TypeEnd = Para.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, TypeAt + 6);
			Type = Para.Mid(TypeAt + 6, TypeEnd - TypeAt - 6);
		}

		FString Text;
		int32 TextCursor = 0;
		while (true)
		{
			const int32 TextStart = Para.Find(TEXT("<Text"), ESearchCase::CaseSensitive, ESearchDir::FromStart, TextCursor);
			if (TextStart == INDEX_NONE)
			{
				break;
			}
			const int32 Open = Para.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, TextStart);
			const int32 Close = Para.Find(TEXT("</Text>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
			if (Open == INDEX_NONE || Close == INDEX_NONE)
			{
				break;
			}
			Text += Para.Mid(Open + 1, Close - Open - 1);
			TextCursor = Close + 7;
		}
		Text = Text.Replace(TEXT("&amp;"), TEXT("&")).Replace(TEXT("&lt;"), TEXT("<")).Replace(TEXT("&gt;"), TEXT(">"))
			.Replace(TEXT("&quot;"), TEXT("\"")).Replace(TEXT("&apos;"), TEXT("'")).TrimStartAndEnd();
		if (Text.IsEmpty())
		{
			continue;
		}

		if (Type == TEXT("Scene Heading"))
		{
			FString Setting, Location, Time;
			Out += TEXT("\n") + (UCinemafyScriptLibrary::ParseHeading(Text, Setting, Location, Time) ? Text.ToUpper() : TEXT(".") + Text) + TEXT("\n");
		}
		else if (Type == TEXT("Character"))
		{
			Out += TEXT("\n@") + Text + TEXT("\n");
		}
		else if (Type == TEXT("Parenthetical"))
		{
			Out += (Text.StartsWith(TEXT("(")) ? Text : TEXT("(") + Text + TEXT(")")) + TEXT("\n");
		}
		else if (Type == TEXT("Dialogue"))
		{
			Out += Text + TEXT("\n");
		}
		else if (Type == TEXT("Transition"))
		{
			Out += TEXT("\n>") + Text.ToUpper() + TEXT("\n");
		}
		else if (Type.Contains(TEXT("Act")) || Type == TEXT("New Act") || Type == TEXT("End of Act"))
		{
			Out += TEXT("\n# ") + Text + TEXT("\n");
		}
		else
		{
			Out += TEXT("\n") + Text + TEXT("\n");
		}
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Breaking it down
// ---------------------------------------------------------------------------------------------------------------------

namespace CinemafyScriptBreakdown
{
	using namespace CinemafyScriptText;

	struct FScene
	{
		FCinemafyScriptScene Data;
		FString ActMark;
		FString ChapterMark;
		bool bActMarked = false;
		bool bChapterMarked = false;
		bool bHardBreakBefore = false;
		bool bTimeJump = false;
		bool bContinuous = false;
		float Seconds = 0.f;
	};

	struct FParse
	{
		TArray<FScene> Scenes;
		FString Title;
		int32 ActMarks = 0;
		int32 ChapterMarks = 0;
		// Markers waiting for the next scene.
		FString PendingAct;
		FString PendingChapter;
		bool bPendingAct = false;
		bool bPendingChapter = false;
		bool bPendingHardBreak = false;

		FScene& StartScene(const FString& Heading, const FString& Setting, const FString& Location, const FString& Time)
		{
			FScene& Scene = Scenes.AddDefaulted_GetRef();
			Scene.Data.Heading = Heading;
			Scene.Data.Setting = Setting;
			Scene.Data.Location = Location;
			Scene.Data.TimeOfDay = Time;
			Scene.bActMarked = bPendingAct;
			Scene.ActMark = PendingAct;
			Scene.bChapterMarked = bPendingChapter;
			Scene.ChapterMark = PendingChapter;
			Scene.bHardBreakBefore = bPendingHardBreak;
			Scene.bTimeJump = IsTimeJump(Heading);
			Scene.bContinuous = IsContinuous(Heading);
			bPendingAct = bPendingChapter = bPendingHardBreak = false;
			PendingAct.Reset();
			PendingChapter.Reset();
			return Scene;
		}

		/** Content with no heading yet (game scripts, or text before the first heading): an untitled scene. */
		FScene& CurrentOrNew()
		{
			if (Scenes.Num() == 0 || bPendingAct || bPendingChapter)
			{
				const FString Name = !PendingChapter.IsEmpty() ? PendingChapter : (!PendingAct.IsEmpty() ? PendingAct : FString(TEXT("Opening")));
				return StartScene(FString(), FString(), Name, FString());
			}
			return Scenes.Last();
		}
	};

	/** "Spare him -> 2.1.3 ~ spared ? !angry" -> text "Spare him", target, flags set, conditions. */
	void ParseFlowTail(FString& InOutText, FString& OutTarget, TArray<FString>& OutSet, TArray<FString>& OutRequire)
	{
		// Conditions in braces up front: "{trusts_kade} Ask for help".
		while (InOutText.StartsWith(TEXT("{")))
		{
			const int32 Close = InOutText.Find(TEXT("}"));
			if (Close == INDEX_NONE)
			{
				break;
			}
			OutRequire.Add(InOutText.Mid(1, Close - 1).TrimStartAndEnd());
			InOutText = InOutText.Mid(Close + 1).TrimStartAndEnd();
		}
		TArray<FString> Parts;
		int32 Cut = InOutText.Len();
		for (const TCHAR* Marker : { TEXT("->"), TEXT(" ~ "), TEXT(" ? ") })
		{
			const int32 At = InOutText.Find(Marker);
			if (At != INDEX_NONE)
			{
				Cut = FMath::Min(Cut, At);
			}
		}
		FString Tail = InOutText.Mid(Cut);
		InOutText = InOutText.Left(Cut).TrimStartAndEnd();
		// Walk the tail: each marker owns the words up to the next marker.
		Tail = Tail.Replace(TEXT("->"), TEXT("\x01>")).Replace(TEXT(" ~ "), TEXT("\x01~")).Replace(TEXT(" ? "), TEXT("\x01?"));
		Tail.ParseIntoArray(Parts, TEXT("\x01"), true);
		for (const FString& Part : Parts)
		{
			const FString Value = Part.Mid(1).TrimStartAndEnd();
			if (Value.IsEmpty())
			{
				continue;
			}
			switch (Part[0])
			{
			case TEXT('>'): OutTarget = Value; break;
			case TEXT('~'): OutSet.Add(Value); break;
			case TEXT('?'): OutRequire.Add(Value); break;
			default: break;
			}
		}
	}

	FParse Parse(const FString& RawText)
	{
		FParse P;
		FString Text = RawText;
		if (Text.Contains(TEXT("<FinalDraft")) || (Text.StartsWith(TEXT("<?xml")) && Text.Contains(TEXT("<Paragraph"))))
		{
			Text = UCinemafyScriptLibrary::FinalDraftToText(Text);
		}
		Text = StripComments(Text.Replace(TEXT("\r\n"), TEXT("\n")).Replace(TEXT("\r"), TEXT("\n")));

		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), false);

		int32 Index = 0;
		// Title page: "Title: ..." key/value lines up to the first blank line.
		while (Index < Lines.Num() && Lines[Index].TrimStartAndEnd().IsEmpty())
		{
			++Index;
		}
		if (Index < Lines.Num())
		{
			const FString First = Lines[Index].TrimStartAndEnd();
			int32 Colon = INDEX_NONE;
			if (First.FindChar(TEXT(':'), Colon) && IsAnyOf(First.Left(Colon).TrimStartAndEnd(), { TEXT("Title"), TEXT("Credit"), TEXT("Author"), TEXT("Authors"), TEXT("Source"), TEXT("Draft date"), TEXT("Contact") }))
			{
				for (; Index < Lines.Num() && !Lines[Index].TrimStartAndEnd().IsEmpty(); ++Index)
				{
					const FString Pair = Lines[Index].TrimStartAndEnd();
					if (Pair.StartsWith(TEXT("Title:"), ESearchCase::IgnoreCase))
					{
						P.Title = Pair.Mid(6).TrimStartAndEnd().Replace(TEXT("_"), TEXT("")).Replace(TEXT("*"), TEXT(""));
					}
				}
			}
		}

		FString Speaker, SpeakerDirection, PendingDirection;
		bool bInDialogue = false;
		bool bBlankBefore = true;
		FCinemafyScriptLine* OpenAction = nullptr;

		auto NextNonEmpty = [&Lines](int32 From) -> FString
		{
			return From < Lines.Num() ? Lines[From].TrimStartAndEnd() : FString();
		};

		for (; Index < Lines.Num(); ++Index)
		{
			const FString Trim = Lines[Index].TrimStartAndEnd();
			if (Trim.IsEmpty())
			{
				bInDialogue = false;
				bBlankBefore = true;
				OpenAction = nullptr;
				continue;
			}
			const bool bWasBlank = bBlankBefore;
			bBlankBefore = false;

			// Story flow (games): "* choice -> target ~ flag ? condition", "-> divert", "~ flag", "? condition".
			if ((Trim.StartsWith(TEXT("* ")) || Trim.StartsWith(TEXT("+ "))) && P.Scenes.Num() > 0)
			{
				FCinemafyScriptChoice& Choice = P.Scenes.Last().Data.Choices.AddDefaulted_GetRef();
				FString Rest = Trim.Mid(2).TrimStartAndEnd();
				ParseFlowTail(Rest, Choice.Target, Choice.SetFlags, Choice.RequireFlags);
				Choice.Text = Rest.Replace(TEXT("["), TEXT("")).Replace(TEXT("]"), TEXT("")).TrimStartAndEnd();
				OpenAction = nullptr;
				continue;
			}
			if (Trim.StartsWith(TEXT("->")) && P.Scenes.Num() > 0)
			{
				P.Scenes.Last().Data.Divert = Trim.Mid(2).TrimStartAndEnd();
				OpenAction = nullptr;
				continue;
			}
			if (Trim.StartsWith(TEXT("~ ")) && P.Scenes.Num() > 0)
			{
				P.Scenes.Last().Data.SetFlags.Add(Trim.Mid(2).TrimStartAndEnd());
				continue;
			}
			if (Trim.StartsWith(TEXT("? ")) && P.Scenes.Num() > 0)
			{
				P.Scenes.Last().Data.RequireFlags.Add(Trim.Mid(2).TrimStartAndEnd());
				continue;
			}

			// Fountain sections: # act, ## chapter (### and deeper count as chapters too).
			if (Trim.StartsWith(TEXT("#")))
			{
				int32 Depth = 0;
				while (Depth < Trim.Len() && Trim[Depth] == TEXT('#'))
				{
					++Depth;
				}
				const FString Name = Trim.Mid(Depth).TrimStartAndEnd();
				if (Depth == 1)
				{
					P.bPendingAct = true;
					P.PendingAct = Name;
					++P.ActMarks;
				}
				else
				{
					P.bPendingChapter = true;
					P.PendingChapter = Name;
					++P.ChapterMarks;
				}
				OpenAction = nullptr;
				continue;
			}
			if (Trim.StartsWith(TEXT("===")))
			{
				continue; // Page break.
			}
			if (Trim.StartsWith(TEXT("=")))
			{
				// Fountain synopsis: the writer's own summary of the scene.
				P.CurrentOrNew().Data.Synopsis = Trim.Mid(1).TrimStartAndEnd();
				continue;
			}

			FString Marker;
			// Game scripts often run markers straight into the text: caps lines don't need a blank before them.
			const bool bStandsAlone = bWasBlank || IsAllCaps(Trim) || (Trim.StartsWith(TEXT("[")) && Trim.EndsWith(TEXT("]")));
			if (bStandsAlone && IsActMarker(Trim, Marker))
			{
				P.bPendingAct = true;
				P.PendingAct = Marker;
				++P.ActMarks;
				OpenAction = nullptr;
				continue;
			}
			// Numbered chapter markers ("MISSION 2: Fallout") are unambiguous wherever they sit.
			const FString AfterFirst = Trim.Mid(FirstWord(Trim).Len()).TrimStart();
			const bool bNumbered = AfterFirst.Len() > 0 && FChar::IsDigit(AfterFirst[0]) && (IsAllCaps(FirstWord(Trim)) || Trim.Contains(TEXT(":")));
			if ((bStandsAlone || bNumbered) && IsChapterMarker(Trim, Marker))
			{
				P.bPendingChapter = true;
				P.PendingChapter = Marker;
				++P.ChapterMarks;
				OpenAction = nullptr;
				continue;
			}

			FString Setting, Location, Time;
			if (UCinemafyScriptLibrary::ParseHeading(Trim, Setting, Location, Time))
			{
				P.StartScene(Trim.StartsWith(TEXT(".")) ? Trim.Mid(1) : Trim, Setting, Location, Time);
				bInDialogue = false;
				OpenAction = nullptr;
				continue;
			}
			if (bStandsAlone && IsGameSceneMarker(Trim, Location))
			{
				P.StartScene(Trim, FString(), Location, FString());
				bInDialogue = false;
				OpenAction = nullptr;
				continue;
			}

			if (IsTransition(Trim))
			{
				const FString Clean = Trim.StartsWith(TEXT(">")) ? Trim.Mid(1).TrimStartAndEnd() : Trim;
				if (IsHardTransition(Clean))
				{
					P.bPendingHardBreak = true;
				}
				if (P.Scenes.Num() > 0 && !Clean.StartsWith(TEXT("FADE IN")))
				{
					FCinemafyScriptLine& Line = P.Scenes.Last().Data.Lines.AddDefaulted_GetRef();
					Line.Kind = ECinemafyScriptLineKind::Transition;
					Line.Text = Clean;
				}
				OpenAction = nullptr;
				continue;
			}

			if (bInDialogue)
			{
				if (Trim.StartsWith(TEXT("(")))
				{
					PendingDirection = Trim;
					continue;
				}
				FCinemafyScriptScene& Scene = P.CurrentOrNew().Data;
				FCinemafyScriptLine* Last = Scene.Lines.Num() > 0 ? &Scene.Lines.Last() : nullptr;
				if (Last && Last->Kind == ECinemafyScriptLineKind::Dialogue && Last->Speaker == Speaker && PendingDirection.IsEmpty() && Lines[Index - 1].TrimStartAndEnd().Len() > 0
					&& !Lines[Index - 1].TrimStartAndEnd().StartsWith(TEXT("(")) && !IsCue(Lines[Index - 1].TrimStartAndEnd()))
				{
					Last->Text += TEXT(" ") + Trim;
				}
				else
				{
					FCinemafyScriptLine& Line = Scene.Lines.AddDefaulted_GetRef();
					Line.Kind = ECinemafyScriptLineKind::Dialogue;
					Line.Speaker = Speaker;
					Line.Direction = !PendingDirection.IsEmpty() ? PendingDirection : SpeakerDirection;
					Line.Text = Trim;
					PendingDirection.Reset();
				}
				continue;
			}

			if (bWasBlank && IsCue(Trim) && !NextNonEmpty(Index + 1).IsEmpty())
			{
				FString Extension;
				Speaker = CleanCue(Trim, &Extension).ToUpper();
				SpeakerDirection = Extension;
				PendingDirection.Reset();
				bInDialogue = true;
				OpenAction = nullptr;
				continue;
			}

			FString ColonSpeaker, ColonText, ColonDirection;
			if (IsColonDialogue(Trim, ColonSpeaker, ColonText, ColonDirection))
			{
				FCinemafyScriptLine& Line = P.CurrentOrNew().Data.Lines.AddDefaulted_GetRef();
				Line.Kind = ECinemafyScriptLineKind::Dialogue;
				Line.Speaker = ColonSpeaker.ToUpper();
				Line.Text = ColonText;
				Line.Direction = ColonDirection;
				OpenAction = nullptr;
				continue;
			}

			// Action: a paragraph runs until the next blank line.
			const FString ActionText = Trim.StartsWith(TEXT("!")) ? Trim.Mid(1) : Trim;
			FCinemafyScriptScene& Scene = P.CurrentOrNew().Data;
			if (OpenAction && Scene.Lines.Num() > 0 && OpenAction == &Scene.Lines.Last())
			{
				OpenAction->Text += TEXT(" ") + ActionText;
			}
			else
			{
				FCinemafyScriptLine& Line = Scene.Lines.AddDefaulted_GetRef();
				Line.Kind = ECinemafyScriptLineKind::Action;
				Line.Text = ActionText;
				OpenAction = &Line;
			}
		}

		// Per-scene numbers.
		for (FScene& Scene : P.Scenes)
		{
			FCinemafyScriptScene& Data = Scene.Data;
			int32 DialogueWords = 0, ActionWords = 0;
			FString Action;
			for (const FCinemafyScriptLine& Line : Data.Lines)
			{
				if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
				{
					++Data.DialogueLines;
					DialogueWords += CountWords(Line.Text);
					Data.Characters.AddUnique(Line.Speaker);
				}
				else if (Line.Kind == ECinemafyScriptLineKind::Action)
				{
					ActionWords += CountWords(Line.Text);
					Action += (Action.IsEmpty() ? TEXT("") : TEXT(" ")) + Line.Text;
				}
			}
			Data.EstimatedSeconds = FMath::Max(5.f, DialogueWords / 2.6f + ActionWords / 3.f);
			Data.bHasAction = DescribesAction(Action);
			if (Data.Synopsis.IsEmpty())
			{
				Data.Synopsis = FirstSentences(Action.IsEmpty() && Data.Lines.Num() > 0 ? Data.Lines[0].Text : Action, 220);
			}
			if (!Scene.bTimeJump && !Action.IsEmpty())
			{
				// "Three years later." as the first line of action is a time jump too.
				Scene.bTimeJump = IsTimeJump(FirstSentences(Action, 60).ToUpper());
			}
			if (Data.Heading.IsEmpty())
			{
				Data.Heading = Data.Location;
			}
			Scene.Seconds = Data.EstimatedSeconds;
		}
		return P;
	}

	/** Chooses the scene index closest to Target (0..1 through the running time) that's also a turn in the story. */
	int32 PickBreak(const TArray<FScene>& Scenes, const TArray<float>& StartFraction, float Target, int32 MinIndex, int32 MaxIndex, FString& OutWhy)
	{
		TSet<FString> Seen;
		TArray<bool> NewPlace;
		NewPlace.SetNum(Scenes.Num());
		for (int32 Index = 0; Index < Scenes.Num(); ++Index)
		{
			const FString Place = Scenes[Index].Data.Location.ToUpper();
			NewPlace[Index] = !Place.IsEmpty() && !Seen.Contains(Place);
			Seen.Add(Place);
		}

		int32 Best = INDEX_NONE;
		float BestScore = -BIG_NUMBER;
		for (int32 Index = FMath::Max(MinIndex, 1); Index <= FMath::Min(MaxIndex, Scenes.Num() - 1); ++Index)
		{
			const FScene& Scene = Scenes[Index];
			const float Distance = FMath::Abs(StartFraction[Index] - Target);
			float Score = -Distance * 12.f;
			Score += Scene.bHardBreakBefore ? 3.f : 0.f;
			Score += Scene.bTimeJump ? 2.f : 0.f;
			Score += NewPlace[Index] ? 1.f : 0.f;
			Score += Scenes[Index - 1].Data.bHasAction ? 0.5f : 0.f;
			Score -= Scene.bContinuous ? 3.f : 0.f;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Index;
			}
		}
		if (Best != INDEX_NONE)
		{
			TArray<FString> Reasons;
			Reasons.Add(FString::Printf(TEXT("%.0f%% through"), StartFraction[Best] * 100.f));
			if (Scenes[Best].bHardBreakBefore) { Reasons.Add(TEXT("after a fade out")); }
			if (Scenes[Best].bTimeJump) { Reasons.Add(TEXT("a time jump")); }
			if (NewPlace[Best]) { Reasons.Add(TEXT("a new place")); }
			if (Scenes[Best - 1].Data.bHasAction) { Reasons.Add(TEXT("after an action scene")); }
			OutWhy = FString::Join(Reasons, TEXT(", "));
		}
		return Best;
	}

	FString MostCommonPlace(const TArray<FCinemafyScriptScene>& Scenes)
	{
		TMap<FString, int32> Counts;
		for (const FCinemafyScriptScene& Scene : Scenes)
		{
			if (!Scene.Location.IsEmpty())
			{
				Counts.FindOrAdd(Scene.Location.ToUpper()) += 1;
			}
		}
		FString Best;
		int32 BestCount = 0;
		for (const TPair<FString, int32>& Pair : Counts)
		{
			if (Pair.Value > BestCount)
			{
				Best = Pair.Key;
				BestCount = Pair.Value;
			}
		}
		return TitleCase(Best);
	}
}

TArray<FCinemafyScriptAct> UCinemafyScriptLibrary::BreakDownScript(const FString& ScriptText, bool bInferActs, int32 ScenesPerChapter, FString& OutTitle, FString& OutReport)
{
	using namespace CinemafyScriptBreakdown;
	FParse P = Parse(ScriptText);
	OutTitle = P.Title;
	TArray<FCinemafyScriptAct> Acts;
	TArray<FScene>& Scenes = P.Scenes;
	if (Scenes.Num() == 0)
	{
		OutReport = TEXT("No scenes found. Scene headings (INT. / EXT.), \"SCENE\" markers or NAME: dialogue lines are what Cinemafy looks for.");
		return Acts;
	}

	float Total = 0.f;
	TArray<float> StartFraction;
	for (const FScene& Scene : Scenes)
	{
		StartFraction.Add(Total);
		Total += Scene.Seconds;
	}
	for (float& Fraction : StartFraction)
	{
		Fraction /= FMath::Max(Total, 1.f);
	}

	// Acts: as marked; else three acts placed on the story's turns near the quarter marks.
	TArray<int32> ActStarts = { 0 };
	TArray<FString> ActTitles, ActWhy;
	TArray<bool> ActInferred;
	if (P.ActMarks > 0)
	{
		const bool bOpening = !Scenes[0].bActMarked;
		ActTitles.Add(bOpening ? FString(TEXT("Opening")) : Scenes[0].ActMark);
		ActInferred.Add(bOpening);
		ActWhy.Add(bOpening ? TEXT("scenes before the first act marker") : TEXT("marked in the script"));
		for (int32 Index = 1; Index < Scenes.Num(); ++Index)
		{
			if (Scenes[Index].bActMarked)
			{
				ActStarts.Add(Index);
				ActTitles.Add(Scenes[Index].ActMark);
				ActInferred.Add(false);
				ActWhy.Add(TEXT("marked in the script"));
			}
		}
	}
	else if (bInferActs && Scenes.Num() >= 6)
	{
		FString WhyOne, WhyTwo;
		const int32 One = PickBreak(Scenes, StartFraction, 0.25f, 1, Scenes.Num() - 3, WhyOne);
		const int32 Two = PickBreak(Scenes, StartFraction, 0.75f, One + 2, Scenes.Num() - 1, WhyTwo);
		ActTitles = { TEXT("Act One: Setup"), TEXT("Act Two: Confrontation"), TEXT("Act Three: Resolution") };
		ActWhy = { TEXT("the beginning"), TEXT("Act break near the quarter mark: ") + WhyOne, TEXT("Act break near the three-quarter mark: ") + WhyTwo };
		ActInferred = { true, true, true };
		ActStarts.Add(One);
		ActStarts.Add(Two);
	}
	else
	{
		ActTitles.Add(TEXT("Act One"));
		ActInferred.Add(true);
		ActWhy.Add(Scenes.Num() < 6 ? TEXT("short script: one act") : TEXT("no act markers"));
	}

	int32 InferredChapters = 0, MarkedChapters = 0;
	int32 Number = 0;
	for (int32 ActIndex = 0; ActIndex < ActStarts.Num(); ++ActIndex)
	{
		const int32 First = ActStarts[ActIndex];
		const int32 End = ActIndex + 1 < ActStarts.Num() ? ActStarts[ActIndex + 1] : Scenes.Num();
		FCinemafyScriptAct& Act = Acts.AddDefaulted_GetRef();
		Act.Id = FString::FromInt(ActIndex + 1);
		Act.Title = ActTitles[ActIndex];
		Act.bInferred = ActInferred[ActIndex];
		Act.Why = ActWhy[ActIndex];

		// Chapters: as marked; else runs of scenes in continuous time and place, broken on turns.
		TArray<int32> ChapterStarts = { First };
		TArray<FString> ChapterTitles, ChapterWhy;
		TArray<bool> ChapterInferred;
		if (P.ChapterMarks > 0)
		{
			const bool bMarked = Scenes[First].bChapterMarked;
			ChapterTitles.Add(bMarked ? Scenes[First].ChapterMark : FString());
			ChapterInferred.Add(!bMarked);
			ChapterWhy.Add(bMarked ? TEXT("marked in the script") : TEXT("scenes before the act's first chapter marker"));
			for (int32 Index = First + 1; Index < End; ++Index)
			{
				if (Scenes[Index].bChapterMarked)
				{
					ChapterStarts.Add(Index);
					ChapterTitles.Add(Scenes[Index].ChapterMark);
					ChapterInferred.Add(false);
					ChapterWhy.Add(TEXT("marked in the script"));
				}
			}
		}
		else
		{
			ChapterTitles.Add(FString());
			ChapterInferred.Add(true);
			ChapterWhy.Add(TEXT("start of the act"));
			TSet<FString> Places;
			Places.Add(Scenes[First].Data.Location.ToUpper());
			int32 Count = 1;
			const int32 Target = FMath::Max(ScenesPerChapter, 2);
			for (int32 Index = First + 1; Index < End; ++Index)
			{
				const FScene& Scene = Scenes[Index];
				const FString Place = Scene.Data.Location.ToUpper();
				const bool bNewPlace = !Places.Contains(Place);
				FString Why;
				if (!Scene.bContinuous)
				{
					if (Scene.bHardBreakBefore) { Why = TEXT("after a fade out"); }
					else if (Scene.bTimeJump && Count >= 2) { Why = TEXT("a time jump"); }
					else if (bNewPlace && Count >= FMath::Max(2, FMath::RoundToInt(Target * 0.6f))) { Why = TEXT("the story moves somewhere new"); }
				}
				if (Why.IsEmpty() && Count >= FMath::RoundToInt(Target * 1.6f))
				{
					Why = TEXT("long run of scenes, split at the next scene");
				}
				if (!Why.IsEmpty())
				{
					ChapterStarts.Add(Index);
					ChapterTitles.Add(FString());
					ChapterInferred.Add(true);
					ChapterWhy.Add(Why);
					Places.Reset();
					Count = 0;
				}
				Places.Add(Place);
				++Count;
			}
		}

		for (int32 ChapterIndex = 0; ChapterIndex < ChapterStarts.Num(); ++ChapterIndex)
		{
			const int32 ChapterFirst = ChapterStarts[ChapterIndex];
			const int32 ChapterEnd = ChapterIndex + 1 < ChapterStarts.Num() ? ChapterStarts[ChapterIndex + 1] : End;
			FCinemafyScriptChapter& Chapter = Act.Chapters.AddDefaulted_GetRef();
			Chapter.Id = FString::Printf(TEXT("%d.%d"), ActIndex + 1, ChapterIndex + 1);
			Chapter.bInferred = ChapterInferred[ChapterIndex];
			Chapter.Why = ChapterWhy[ChapterIndex];
			(Chapter.bInferred ? InferredChapters : MarkedChapters) += 1;
			for (int32 Index = ChapterFirst; Index < ChapterEnd; ++Index)
			{
				FCinemafyScriptScene Scene = Scenes[Index].Data;
				Scene.Number = ++Number;
				Scene.Id = FString::Printf(TEXT("%d.%d.%d"), ActIndex + 1, ChapterIndex + 1, Chapter.Scenes.Num() + 1);
				Chapter.Scenes.Add(MoveTemp(Scene));
			}
			const FString Place = MostCommonPlace(Chapter.Scenes);
			Chapter.Title = !ChapterTitles[ChapterIndex].IsEmpty() ? ChapterTitles[ChapterIndex]
				: FString::Printf(TEXT("Chapter %d%s%s"), ChapterIndex + 1, Place.IsEmpty() ? TEXT("") : TEXT(": "), *Place);
		}
	}

	TrackContinuity(Acts);
	ResolveStoryTargets(Acts);

	// The report: what was found, and every call Cinemafy made.
	TArray<FString> Report;
	Report.Add(FString::Printf(TEXT("%d scene(s), %d act(s), %d chapter(s), about %d min."), Scenes.Num(), Acts.Num(), MarkedChapters + InferredChapters,
		FMath::RoundToInt(Total / 60.f)));
	for (const FCinemafyScriptAct& Act : Acts)
	{
		Report.Add(FString::Printf(TEXT("%s %s: %s%s"), *Act.Id, *Act.Title, Act.bInferred ? TEXT("inferred, ") : TEXT(""), *Act.Why));
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			Report.Add(FString::Printf(TEXT("  %s %s (%d scene(s)): %s%s"), *Chapter.Id, *Chapter.Title, Chapter.Scenes.Num(),
				Chapter.bInferred ? TEXT("inferred, ") : TEXT(""), *Chapter.Why));
		}
	}
	OutReport = FString::Join(Report, TEXT("\n"));
	return Acts;
}

// ---------------------------------------------------------------------------------------------------------------------
// Continuity
// ---------------------------------------------------------------------------------------------------------------------

namespace CinemafyScriptContinuity
{
	using namespace CinemafyScriptText;

	struct FCast
	{
		/** lowercase alias -> canonical name ("thane" -> "ADMIRAL THANE"). */
		TMap<FString, FString> Aliases;

		void Build(const TArray<FString>& Names)
		{
			static const TSet<FString> Titles = { TEXT("the"), TEXT("admiral"), TEXT("captain"), TEXT("doctor"), TEXT("dr"), TEXT("mr"), TEXT("mrs"), TEXT("ms"),
				TEXT("sergeant"), TEXT("sgt"), TEXT("commander"), TEXT("lieutenant"), TEXT("lt"), TEXT("general"), TEXT("young"), TEXT("old"), TEXT("officer"),
				TEXT("agent"), TEXT("lord"), TEXT("lady"), TEXT("sir"), TEXT("king"), TEXT("queen"), TEXT("prince"), TEXT("princess"), TEXT("guard"), TEXT("man"), TEXT("woman") };
			TMap<FString, int32> WordUse;
			for (const FString& Name : Names)
			{
				TArray<FString> Words;
				Normalize(Name).ParseIntoArrayWS(Words);
				for (const FString& Word : Words)
				{
					WordUse.FindOrAdd(Word) += 1;
				}
			}
			for (const FString& Name : Names)
			{
				const FString Full = Normalize(Name);
				Aliases.Add(Full, Name);
				TArray<FString> Words;
				Full.ParseIntoArrayWS(Words);
				for (const FString& Word : Words)
				{
					if (Word.Len() >= 3 && !Titles.Contains(Word) && WordUse.FindRef(Word) == 1)
					{
						Aliases.Add(Word, Name);
					}
				}
			}
		}

		/** Characters named in a sentence (normalized words), in order. */
		TArray<FString> Find(const TArray<FString>& Words) const
		{
			TArray<FString> Found;
			for (int32 Index = 0; Index < Words.Num(); ++Index)
			{
				// Two-word names first ("admiral thane").
				if (Index + 1 < Words.Num())
				{
					if (const FString* Two = Aliases.Find(Words[Index] + TEXT(" ") + Words[Index + 1]))
					{
						Found.AddUnique(*Two);
						++Index;
						continue;
					}
				}
				if (const FString* One = Aliases.Find(Words[Index]))
				{
					Found.AddUnique(*One);
				}
			}
			return Found;
		}
	};

	/** The noun phrase after a verb: up to three words, past a determiner, stopping at punctuation-ish words. */
	FString ObjectAfter(const TArray<FString>& Words, int32 VerbEnd)
	{
		static const TSet<FString> Determiners = { TEXT("the"), TEXT("a"), TEXT("an"), TEXT("her"), TEXT("his"), TEXT("their"), TEXT("its"), TEXT("my"), TEXT("your"), TEXT("some") };
		static const TSet<FString> Stops = { TEXT("and"), TEXT("to"), TEXT("from"), TEXT("at"), TEXT("as"), TEXT("then"), TEXT("with"), TEXT("in"), TEXT("on"),
			TEXT("off"), TEXT("up"), TEXT("out"), TEXT("toward"), TEXT("towards"), TEXT("into"), TEXT("while"), TEXT("before"), TEXT("after"), TEXT("but") };
		static const TSet<FString> NotProps = { TEXT("breath"), TEXT("step"), TEXT("steps"), TEXT("moment"), TEXT("look"), TEXT("glance"), TEXT("aim"), TEXT("cover"),
			TEXT("hand"), TEXT("hands"), TEXT("seat"), TEXT("position"), TEXT("place"), TEXT("turn"), TEXT("beat"), TEXT("chance"), TEXT("time"), TEXT("it"),
			TEXT("them"), TEXT("him"), TEXT("her"), TEXT("herself"), TEXT("himself"), TEXT("themselves"), TEXT("breathe"), TEXT("deep"), TEXT("lead") };
		int32 Index = VerbEnd;
		while (Index < Words.Num() && Determiners.Contains(Words[Index]))
		{
			++Index;
		}
		TArray<FString> Phrase;
		for (; Index < Words.Num() && Phrase.Num() < 3; ++Index)
		{
			if (Stops.Contains(Words[Index]))
			{
				break;
			}
			Phrase.Add(Words[Index]);
		}
		if (Phrase.Num() == 0 || NotProps.Contains(Phrase[0]) || Phrase.Last().Len() < 3)
		{
			return FString();
		}
		return FString::Join(Phrase, TEXT(" "));
	}

	/** Index just past a multi-word verb in Words, or INDEX_NONE. */
	int32 FindVerb(const TArray<FString>& Words, std::initializer_list<const TCHAR*> Verbs)
	{
		for (const TCHAR* Verb : Verbs)
		{
			TArray<FString> Parts;
			FString(Verb).ParseIntoArrayWS(Parts);
			for (int32 Index = 0; Index + Parts.Num() <= Words.Num(); ++Index)
			{
				bool bMatch = true;
				for (int32 Part = 0; Part < Parts.Num() && bMatch; ++Part)
				{
					bMatch = Words[Index + Part] == Parts[Part];
				}
				if (bMatch)
				{
					return Index + Parts.Num();
				}
			}
		}
		return INDEX_NONE;
	}

	struct FState
	{
		TArray<FCinemafyContinuityFact> Facts;

		bool Has(const FString& Character, const FString& Category) const
		{
			return Facts.ContainsByPredicate([&](const FCinemafyContinuityFact& F) { return F.Character == Character && F.Category == Category; });
		}
		const FCinemafyContinuityFact* Get(const FString& Character, const FString& Category) const
		{
			return Facts.FindByPredicate([&](const FCinemafyContinuityFact& F) { return F.Character == Character && F.Category == Category; });
		}
	};
}

void UCinemafyScriptLibrary::TrackContinuity(TArray<FCinemafyScriptAct>& Acts)
{
	using namespace CinemafyScriptContinuity;

	// The cast: everyone who speaks.
	TArray<FString> Names;
	for (const FCinemafyScriptAct& Act : Acts)
	{
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				for (const FCinemafyScriptLine& Line : Scene.Lines)
				{
					if (Line.Kind == ECinemafyScriptLineKind::Dialogue && !Line.Speaker.IsEmpty())
					{
						Names.AddUnique(Line.Speaker);
					}
				}
			}
		}
	}
	FCast CastNames;
	CastNames.Build(Names);

	FState State;
	for (FCinemafyScriptAct& Act : Acts)
	{
		for (FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				Scene.CarriedIn.Reset();
				Scene.Changes.Reset();
				Scene.Warnings.Reset();

				// Who's here: speakers, plus anyone named in the action.
				for (const FCinemafyScriptLine& Line : Scene.Lines)
				{
					if (Line.Kind == ECinemafyScriptLineKind::Action)
					{
						TArray<FString> Words;
						Normalize(Line.Text).ParseIntoArrayWS(Words);
						for (const FString& Name : CastNames.Find(Words))
						{
							Scene.Characters.AddUnique(Name);
						}
					}
				}

				// Going in: what's still true for the people in this scene.
				for (const FCinemafyContinuityFact& Fact : State.Facts)
				{
					if (Scene.Characters.Contains(Fact.Character))
					{
						Scene.CarriedIn.Add(Fact);
					}
				}

				// The dead don't talk (flashbacks aside).
				const bool bFlashback = Scene.Heading.Contains(TEXT("FLASHBACK"), ESearchCase::IgnoreCase);
				for (const FCinemafyScriptLine& Line : Scene.Lines)
				{
					if (Line.Kind == ECinemafyScriptLineKind::Dialogue && !bFlashback && !Line.Direction.Contains(TEXT("V.O")))
					{
						if (const FCinemafyContinuityFact* Dead = State.Get(Line.Speaker, TEXT("Dead")))
						{
							Scene.Warnings.AddUnique(FString::Printf(TEXT("%s died in %s but speaks here (flashback? mark the heading FLASHBACK)"), *Line.Speaker, *Dead->SinceScene));
						}
					}
				}

				// What changes here, sentence by sentence.
				FString LastNamed;
				auto Add = [&](const FString& Who, const TCHAR* Category, const FString& Fact, const FString& Evidence, bool bReplaceCategory)
				{
					if (Who.IsEmpty() || Fact.IsEmpty())
					{
						return;
					}
					if (bReplaceCategory)
					{
						State.Facts.RemoveAll([&](const FCinemafyContinuityFact& F) { return F.Character == Who && F.Category == Category; });
					}
					else if (State.Facts.ContainsByPredicate([&](const FCinemafyContinuityFact& F) { return F.Character == Who && F.Category == Category && F.Fact == Fact; }))
					{
						return;
					}
					FCinemafyContinuityFact& New = State.Facts.AddDefaulted_GetRef();
					New.Character = Who;
					New.Category = Category;
					New.Fact = Fact;
					New.SinceScene = Scene.Id;
					New.Evidence = Evidence.Left(160);
					Scene.Changes.Add(New);
				};
				auto Clear = [&](const FString& Who, const TCHAR* Category, const FString& Item, const FString& Evidence)
				{
					if (Who.IsEmpty())
					{
						return;
					}
					const int32 Removed = State.Facts.RemoveAll([&](const FCinemafyContinuityFact& F)
					{
						return F.Character == Who && F.Category == Category && (Item.IsEmpty() || F.Fact.Contains(Item) || Item.Contains(F.Fact));
					});
					if (Removed > 0)
					{
						FCinemafyContinuityFact& Change = Scene.Changes.AddDefaulted_GetRef();
						Change.Character = Who;
						Change.Category = TEXT("Cleared");
						Change.Fact = FString::Printf(TEXT("%s: %s"), Category, Item.IsEmpty() ? TEXT("all") : *Item);
						Change.SinceScene = Scene.Id;
						Change.Evidence = Evidence.Left(160);
					}
				};

				for (const FCinemafyScriptLine& Line : Scene.Lines)
				{
					if (Line.Kind != ECinemafyScriptLineKind::Action)
					{
						continue;
					}
					TArray<FString> Sentences;
					Line.Text.Replace(TEXT("!"), TEXT(".")).Replace(TEXT("?"), TEXT(".")).ParseIntoArray(Sentences, TEXT(". "), true);
					for (const FString& Sentence : Sentences)
					{
						TArray<FString> Words;
						Normalize(Sentence).ParseIntoArrayWS(Words);
						if (Words.Num() == 0)
						{
							continue;
						}
						TArray<FString> Named = CastNames.Find(Words);
						FString Subject = Named.Num() > 0 ? Named[0] : FString();
						if (Subject.IsEmpty() && IsAnyOf(Words[0], { TEXT("she"), TEXT("he"), TEXT("they"), TEXT("her"), TEXT("his") }))
						{
							Subject = LastNamed;
						}
						if (!Named.IsEmpty())
						{
							LastNamed = Named.Last();
						}

						// Deaths: "X kills Y" (Y dies), "X dies / is killed / falls dead".
						const int32 Kills = FindVerb(Words, { TEXT("kills"), TEXT("executes"), TEXT("murders"), TEXT("slays") });
						if (Kills != INDEX_NONE && Named.Num() >= 2)
						{
							Add(Named[1], TEXT("Dead"), TEXT("dead"), Sentence, true);
						}
						else if (FindVerb(Words, { TEXT("dies"), TEXT("is killed"), TEXT("falls dead"), TEXT("lies dead"), TEXT("is dead"), TEXT("breathes his last"),
							TEXT("breathes her last"), TEXT("is slain") }) != INDEX_NONE)
						{
							Add(Subject, TEXT("Dead"), TEXT("dead"), Sentence, true);
						}

						// Injuries, and getting patched up.
						if (FindVerb(Words, { TEXT("patched up"), TEXT("bandages"), TEXT("bandaged"), TEXT("stitches"), TEXT("healed") }) != INDEX_NONE)
						{
							Add(Subject, TEXT("Injury"), TEXT("bandaged"), Sentence, true);
						}
						else
						{
							static const TCHAR* Injuries[] = { TEXT("wounded"), TEXT("bleeding"), TEXT("bleeds"), TEXT("limping"), TEXT("limps"), TEXT("injured"),
								TEXT("bloodied"), TEXT("stabbed"), TEXT("is shot"), TEXT("gets shot"), TEXT("burned"), TEXT("unconscious"), TEXT("knocked out"), TEXT("broken arm"),
								TEXT("broken leg"), TEXT("broken ribs"), TEXT("bruised"), TEXT("scarred") };
							for (const TCHAR* Injury : Injuries)
							{
								if (FindVerb(Words, { Injury }) != INDEX_NONE)
								{
									Add(Subject, TEXT("Injury"), Injury, Sentence, false);
									break;
								}
							}
						}

						// In hand.
						const int32 Picks = FindVerb(Words, { TEXT("picks up"), TEXT("draws"), TEXT("grabs"), TEXT("pockets"), TEXT("unholsters"), TEXT("unsheathes"),
							TEXT("wields"), TEXT("brandishes"), TEXT("carries"), TEXT("carrying"), TEXT("holding"), TEXT("holds up"), TEXT("snatches") });
						if (Picks != INDEX_NONE)
						{
							Add(Subject, TEXT("Carrying"), ObjectAfter(Words, Picks), Sentence, false);
						}
						const int32 Drops = FindVerb(Words, { TEXT("drops"), TEXT("holsters"), TEXT("sheathes"), TEXT("puts down"), TEXT("sets down"), TEXT("throws away"),
							TEXT("tosses"), TEXT("discards"), TEXT("loses"), TEXT("hands over") });
						if (Drops != INDEX_NONE)
						{
							Clear(Subject, TEXT("Carrying"), ObjectAfter(Words, Drops), Sentence);
						}

						// Wearing.
						const int32 Wears = FindVerb(Words, { TEXT("puts on"), TEXT("changes into"), TEXT("dressed in"), TEXT("wearing"), TEXT("wears"), TEXT("dons") });
						if (Wears != INDEX_NONE)
						{
							Add(Subject, TEXT("Wearing"), ObjectAfter(Words, Wears), Sentence, false);
						}
						const int32 Removes = FindVerb(Words, { TEXT("takes off"), TEXT("removes"), TEXT("strips off"), TEXT("sheds") });
						if (Removes != INDEX_NONE)
						{
							Clear(Subject, TEXT("Wearing"), ObjectAfter(Words, Removes), Sentence);
						}
					}
				}
			}
		}
	}
}

float UCinemafyScriptLibrary::ScoreLinesAgainstScene(const TArray<FString>& SpokenLines, const FCinemafyScriptScene& Scene)
{
	using namespace CinemafyScriptText;
	TArray<FString> SceneLines;
	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			SceneLines.Add(Normalize(Line.Text));
		}
	}
	int32 Counted = 0, Matched = 0;
	for (const FString& Spoken : SpokenLines)
	{
		const FString Said = Normalize(Spoken);
		if (Said.IsEmpty())
		{
			continue;
		}
		++Counted;
		TArray<FString> SaidWords;
		Said.ParseIntoArrayWS(SaidWords);
		for (const FString& Written : SceneLines)
		{
			if (Written == Said || (Said.Len() >= 12 && (Written.Contains(Said) || Said.Contains(Written))))
			{
				++Matched;
				break;
			}
			// Rewrites on the day: most of the same words still counts.
			TArray<FString> WrittenWords;
			Written.ParseIntoArrayWS(WrittenWords);
			int32 Shared = 0;
			for (const FString& Word : SaidWords)
			{
				Shared += WrittenWords.Contains(Word) ? 1 : 0;
			}
			const int32 Union = SaidWords.Num() + WrittenWords.Num() - Shared;
			if (Union > 0 && Shared / static_cast<float>(Union) >= 0.6f)
			{
				++Matched;
				break;
			}
		}
	}
	return Counted > 0 ? Matched / static_cast<float>(Counted) : 0.f;
}

// ---------------------------------------------------------------------------------------------------------------------
// The project asset
// ---------------------------------------------------------------------------------------------------------------------

TArray<FCinemafyScriptScene*> UCinemafyScriptProject::GetAllScenes()
{
	TArray<FCinemafyScriptScene*> All;
	for (FCinemafyScriptAct& Act : Acts)
	{
		for (FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				All.Add(&Scene);
			}
		}
	}
	return All;
}

TArray<const FCinemafyScriptScene*> UCinemafyScriptProject::GetAllScenes() const
{
	TArray<const FCinemafyScriptScene*> All;
	for (const FCinemafyScriptAct& Act : Acts)
	{
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				All.Add(&Scene);
			}
		}
	}
	return All;
}

FCinemafyScriptScene* UCinemafyScriptProject::FindScene(const FString& SceneId)
{
	for (FCinemafyScriptScene* Scene : GetAllScenes())
	{
		if (Scene->Id == SceneId || FString::FromInt(Scene->Number) == SceneId)
		{
			return Scene;
		}
	}
	return nullptr;
}

void UCinemafyScriptProject::BreakDown(const FString& ScriptText)
{
	// Keep production work for scenes that are still there: matched by heading and first line, then by heading alone.
	struct FKept { FString Key; FString Heading; FCinemafyScriptScene Scene; bool bUsed = false; };
	TArray<FKept> Kept;
	auto KeyFor = [](const FCinemafyScriptScene& Scene)
	{
		FString First;
		for (const FCinemafyScriptLine& Line : Scene.Lines)
		{
			if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
			{
				First = Line.Text;
				break;
			}
		}
		return CinemafyScriptText::Normalize(Scene.Heading) + TEXT("|") + CinemafyScriptText::Normalize(First);
	};
	for (const FCinemafyScriptScene* Scene : GetAllScenes())
	{
		if (Scene->Status != ECinemafySceneStatus::NotStarted || !Scene->Notes.IsEmpty() || !Scene->DirectorLabel.IsEmpty() || !Scene->Sequence.IsNull() || Scene->ReviewNotes.Num() > 0)
		{
			Kept.Add(FKept{ KeyFor(*Scene), CinemafyScriptText::Normalize(Scene->Heading), *Scene });
		}
	}

	// Voice-over follows its line (by id), so a re-import keeps recorded and scratch audio.
	TMap<FString, TPair<TSoftObjectPtr<USoundBase>, bool>> LineAudio;
	for (const FCinemafyScriptScene* Scene : GetAllScenes())
	{
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (!Line.LineId.IsEmpty() && !Line.Audio.IsNull())
			{
				LineAudio.Add(Line.LineId, TPair<TSoftObjectPtr<USoundBase>, bool>(Line.Audio, Line.bScratchAudio));
			}
		}
	}

	FString FoundTitle;
	Acts = UCinemafyScriptLibrary::BreakDownScript(ScriptText, bInferActs, ScenesPerChapter, FoundTitle, BreakdownReport);

	// Line ids: speaker initials + a hash of who says what; repeats get a suffix.
	TSet<FString> UsedIds;
	for (FCinemafyScriptScene* Scene : GetAllScenes())
	{
		for (FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.Kind != ECinemafyScriptLineKind::Dialogue)
			{
				continue;
			}
			FString Initials;
			TArray<FString> Words;
			Line.Speaker.ParseIntoArrayWS(Words);
			for (const FString& Word : Words)
			{
				Initials.AppendChar(FChar::ToUpper(Word[0]));
			}
			const uint32 Hash = FCrc::StrCrc32(*(CinemafyScriptText::Normalize(Line.Speaker) + TEXT("|") + CinemafyScriptText::Normalize(Line.Text)));
			FString Id = FString::Printf(TEXT("%s_%08X"), *Initials, Hash);
			for (int32 Repeat = 2; UsedIds.Contains(Id); ++Repeat)
			{
				Id = FString::Printf(TEXT("%s_%08X_%d"), *Initials, Hash, Repeat);
			}
			UsedIds.Add(Id);
			Line.LineId = Id;
			if (const TPair<TSoftObjectPtr<USoundBase>, bool>* Kept = LineAudio.Find(Id))
			{
				Line.Audio = Kept->Key;
				Line.bScratchAudio = Kept->Value;
			}
		}
	}
	if (Title.IsEmpty())
	{
		Title = FoundTitle.IsEmpty() ? GetName() : FoundTitle;
	}

	int32 Carried = 0;
	for (FCinemafyScriptScene* Scene : GetAllScenes())
	{
		const FString Key = KeyFor(*Scene);
		FKept* Match = Kept.FindByPredicate([&Key](const FKept& K) { return !K.bUsed && K.Key == Key; });
		if (!Match)
		{
			const FString Heading = CinemafyScriptText::Normalize(Scene->Heading);
			Match = Kept.FindByPredicate([&Heading](const FKept& K) { return !K.bUsed && !Heading.IsEmpty() && K.Heading == Heading; });
		}
		if (Match)
		{
			Match->bUsed = true;
			Scene->Status = Match->Scene.Status;
			Scene->Sequence = Match->Scene.Sequence;
			Scene->Level = Match->Scene.Level;
			Scene->DirectorLabel = Match->Scene.DirectorLabel;
			Scene->MatchScore = Match->Scene.MatchScore;
			Scene->Notes = Match->Scene.Notes;
			Scene->Takes = Match->Scene.Takes;
			Scene->ReviewNotes = Match->Scene.ReviewNotes;
			Scene->DailiesFolder = Match->Scene.DailiesFolder;
			++Carried;
		}
	}
	const int32 Lost = Kept.FilterByPredicate([](const FKept& K) { return !K.bUsed; }).Num();
	if (Kept.Num() > 0)
	{
		BreakdownReport += FString::Printf(TEXT("\nKept production status for %d scene(s)%s."), Carried,
			Lost > 0 ? *FString::Printf(TEXT("; %d scene(s) with status no longer match the script (check the old checklist)"), Lost) : TEXT(""));
	}

	// The cast, most lines first.
	TMap<FString, int32> Lines;
	for (const FCinemafyScriptScene* Scene : GetAllScenes())
	{
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
			{
				Lines.FindOrAdd(Line.Speaker) += 1;
			}
		}
	}
	Lines.ValueSort([](int32 A, int32 B) { return A > B; });
	Lines.GenerateKeyArray(CastList);
	MarkPackageDirty();
}

void UCinemafyScriptProject::RefreshContinuity()
{
	UCinemafyScriptLibrary::TrackContinuity(Acts);
	UCinemafyScriptLibrary::ResolveStoryTargets(Acts);
	MarkPackageDirty();
}

float UCinemafyScriptProject::GetProgress() const
{
	int32 Counted = 0, Done = 0;
	for (const FCinemafyScriptScene* Scene : GetAllScenes())
	{
		if (Scene->Status == ECinemafySceneStatus::Cut)
		{
			continue;
		}
		++Counted;
		Done += Scene->Status == ECinemafySceneStatus::Directed || Scene->Status == ECinemafySceneStatus::Final ? 1 : 0;
	}
	return Counted > 0 ? Done / static_cast<float>(Counted) : 0.f;
}

FString UCinemafyScriptProject::BuildChecklist() const
{
	auto IsDone = [](const FCinemafyScriptScene& Scene) { return Scene.Status == ECinemafySceneStatus::Directed || Scene.Status == ECinemafySceneStatus::Final; };
	auto Tally = [&IsDone](const TArray<const FCinemafyScriptScene*>& Scenes)
	{
		int32 Done = 0, Counted = 0;
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			if (Scene->Status != ECinemafySceneStatus::Cut)
			{
				++Counted;
				Done += IsDone(*Scene) ? 1 : 0;
			}
		}
		return FString::Printf(TEXT("%d/%d"), Done, Counted);
	};
	auto Clock = [](float Seconds)
	{
		const int32 Total = FMath::RoundToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	};
	const UEnum* StatusEnum = StaticEnum<ECinemafySceneStatus>();

	TArray<FString> Out;
	const TArray<const FCinemafyScriptScene*> All = GetAllScenes();
	float Runtime = 0.f;
	int32 Warnings = 0;
	for (const FCinemafyScriptScene* Scene : All)
	{
		Runtime += Scene->EstimatedSeconds;
		Warnings += Scene->Warnings.Num();
	}
	Out.Add(FString::Printf(TEXT("# %s: production checklist"), *Title));
	Out.Add(FString());
	Out.Add(FString::Printf(TEXT("**%s scenes directed (%.0f%%)**, about %s of screen time, %d continuity warning(s)."),
		*Tally(All), GetProgress() * 100.f, *Clock(Runtime), Warnings));
	if (CastList.Num() > 0)
	{
		Out.Add(FString::Printf(TEXT("Cast: %s"), *FString::Join(CastList, TEXT(", "))));
	}

	for (const FCinemafyScriptAct& Act : Acts)
	{
		TArray<const FCinemafyScriptScene*> ActScenes;
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				ActScenes.Add(&Scene);
			}
		}
		Out.Add(FString());
		Out.Add(FString::Printf(TEXT("## %s %s (%s)"), *Act.Id, *Act.Title, *Tally(ActScenes)));
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			TArray<const FCinemafyScriptScene*> ChapterScenes;
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				ChapterScenes.Add(&Scene);
			}
			Out.Add(FString());
			Out.Add(FString::Printf(TEXT("### %s %s (%s)"), *Chapter.Id, *Chapter.Title, *Tally(ChapterScenes)));
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				const FString Status = StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(Scene.Status)).ToString();
				FString Line = FString::Printf(TEXT("- [%s] **%s** %s (%s, ~%s%s)"), IsDone(Scene) ? TEXT("x") : TEXT(" "), *Scene.Id, *Scene.Heading, *Status,
					*Clock(Scene.EstimatedSeconds), Scene.bHasAction ? TEXT(", action") : TEXT(""));
				if (!Scene.DirectorLabel.IsEmpty())
				{
					Line += FString::Printf(TEXT(" | %s"), *Scene.DirectorLabel);
				}
				if (!Scene.Sequence.IsNull())
				{
					Line += FString::Printf(TEXT(" | %s"), *Scene.Sequence.GetAssetName());
				}
				Out.Add(Line);
				if (Scene.Characters.Num() > 0)
				{
					Out.Add(FString::Printf(TEXT("  - Cast: %s"), *FString::Join(Scene.Characters, TEXT(", "))));
				}
				if (Scene.CarriedIn.Num() > 0)
				{
					TArray<FString> Facts;
					for (const FCinemafyContinuityFact& Fact : Scene.CarriedIn)
					{
						Facts.Add(FString::Printf(TEXT("%s %s %s (since %s)"), *Fact.Character, *Fact.Category.ToLower(), *Fact.Fact, *Fact.SinceScene));
					}
					Out.Add(FString::Printf(TEXT("  - Carry in: %s"), *FString::Join(Facts, TEXT("; "))));
				}
				for (const FString& Warning : Scene.Warnings)
				{
					Out.Add(FString::Printf(TEXT("  - WARNING: %s"), *Warning));
				}
				for (const FCinemafyReviewNote& Note : Scene.ReviewNotes)
				{
					Out.Add(FString::Printf(TEXT("  - [%s] Review (%s, %s%s): %s"), Note.bResolved ? TEXT("x") : TEXT(" "), *Note.Author, *Note.Date,
						Note.AtSeconds >= 0.f ? *FString::Printf(TEXT(" at %.1fs"), Note.AtSeconds) : TEXT(""), *Note.Text));
				}
				if (!Scene.Notes.IsEmpty())
				{
					Out.Add(FString::Printf(TEXT("  - Notes: %s"), *Scene.Notes.Replace(TEXT("\n"), TEXT(" "))));
				}
			}
		}
	}
	return FString::Join(Out, TEXT("\n"));
}

const FCinemafyCastMember* UCinemafyScriptProject::FindCastMember(const FString& ScriptName) const
{
	return CastMembers.FindByPredicate([&ScriptName](const FCinemafyCastMember& Member) { return Member.ScriptName.Equals(ScriptName, ESearchCase::IgnoreCase); });
}

const FCinemafySet* UCinemafyScriptProject::FindSet(const FString& ScriptLocation) const
{
	return Sets.FindByPredicate([&ScriptLocation](const FCinemafySet& Set) { return Set.ScriptLocation.Equals(ScriptLocation, ESearchCase::IgnoreCase); });
}

void UCinemafyScriptLibrary::ResolveStoryTargets(TArray<FCinemafyScriptAct>& Acts)
{
	TArray<FCinemafyScriptScene*> Scenes;
	for (FCinemafyScriptAct& Act : Acts)
	{
		for (FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				Scenes.Add(&Scene);
			}
		}
	}
	auto Resolve = [&Scenes](const FString& Target, FCinemafyScriptScene& From) -> FString
	{
		const FString Trimmed = Target.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}
		if (Trimmed.Equals(TEXT("END"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("DONE"), ESearchCase::IgnoreCase))
		{
			return TEXT("END");
		}
		const FString Number = Trimmed.StartsWith(TEXT("#")) ? Trimmed.Mid(1) : FString();
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			if (Scene->Id == Trimmed || (!Number.IsEmpty() && FString::FromInt(Scene->Number) == Number))
			{
				return Scene->Id;
			}
		}
		// By heading or place: the first match after this scene, else anywhere.
		const FString Wanted = CinemafyScriptText::Normalize(Trimmed);
		const FCinemafyScriptScene* Fallback = nullptr;
		bool bAfter = false;
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			if (Scene == &From)
			{
				bAfter = true;
				continue;
			}
			const bool bMatch = CinemafyScriptText::Normalize(Scene->Heading).Contains(Wanted) || CinemafyScriptText::Normalize(Scene->Location) == Wanted;
			if (bMatch && bAfter)
			{
				return Scene->Id;
			}
			if (bMatch && !Fallback)
			{
				Fallback = Scene;
			}
		}
		return Fallback ? Fallback->Id : FString();
	};
	for (FCinemafyScriptScene* Scene : Scenes)
	{
		Scene->DivertSceneId = Resolve(Scene->Divert, *Scene);
		for (FCinemafyScriptChoice& Choice : Scene->Choices)
		{
			Choice.TargetSceneId = Resolve(Choice.Target, *Scene);
		}
	}
}

bool UCinemafyScriptLibrary::FlagsHold(const TArray<FString>& Conditions, const TSet<FName>& Flags)
{
	for (const FString& Condition : Conditions)
	{
		const bool bNot = Condition.StartsWith(TEXT("!"));
		const FName Flag(*(bNot ? Condition.Mid(1) : Condition).TrimStartAndEnd());
		if (Flags.Contains(Flag) == bNot)
		{
			return false;
		}
	}
	return true;
}
