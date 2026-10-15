#include "CinemafyPrevis.h"
#include "CinemafySettings.h"
#include "CinemafyChoreographyLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"

namespace CinemafyStaging
{
	enum class EVerb : uint8 { Move, Traverse, Draw, Holster, Attack, Look, Gesture, Wait, Dodge };

	struct FVerb
	{
		const TCHAR* Word;
		const TCHAR* Prompt;
		EVerb Kind;
	};

	const TArray<FVerb>& Verbs()
	{
		static const TArray<FVerb> Table = {
			// Moving.
			{ TEXT("walks"), TEXT("walk"), EVerb::Move }, { TEXT("steps"), TEXT("walk"), EVerb::Move }, { TEXT("crosses"), TEXT("walk"), EVerb::Move },
			{ TEXT("moves"), TEXT("walk"), EVerb::Move }, { TEXT("heads"), TEXT("walk"), EVerb::Move }, { TEXT("approaches"), TEXT("walk"), EVerb::Move },
			{ TEXT("strides"), TEXT("walk"), EVerb::Move }, { TEXT("paces"), TEXT("walk"), EVerb::Move }, { TEXT("wanders"), TEXT("walk"), EVerb::Move },
			{ TEXT("backs"), TEXT("walk"), EVerb::Move }, { TEXT("goes"), TEXT("walk"), EVerb::Move }, { TEXT("enters"), TEXT("walk"), EVerb::Move },
			{ TEXT("exits"), TEXT("walk"), EVerb::Move }, { TEXT("returns"), TEXT("walk"), EVerb::Move }, { TEXT("follows"), TEXT("walk"), EVerb::Move },
			{ TEXT("advances"), TEXT("walk"), EVerb::Move }, { TEXT("retreats"), TEXT("walk"), EVerb::Move },
			{ TEXT("runs"), TEXT("run"), EVerb::Move }, { TEXT("races"), TEXT("run"), EVerb::Move }, { TEXT("rushes"), TEXT("run"), EVerb::Move },
			{ TEXT("hurries"), TEXT("run"), EVerb::Move }, { TEXT("dashes"), TEXT("run"), EVerb::Move }, { TEXT("jogs"), TEXT("run"), EVerb::Move },
			{ TEXT("sprints"), TEXT("sprint"), EVerb::Move }, { TEXT("charges"), TEXT("sprint"), EVerb::Move }, { TEXT("bolts"), TEXT("sprint"), EVerb::Move },
			{ TEXT("sneaks"), TEXT("sneak"), EVerb::Move }, { TEXT("creeps"), TEXT("sneak"), EVerb::Move }, { TEXT("tiptoes"), TEXT("sneak"), EVerb::Move },
			// Over and up.
			{ TEXT("vaults"), TEXT("vault"), EVerb::Traverse }, { TEXT("hurdles"), TEXT("vault"), EVerb::Traverse }, { TEXT("leaps"), TEXT("jump"), EVerb::Traverse },
			{ TEXT("jumps"), TEXT("jump"), EVerb::Traverse }, { TEXT("climbs"), TEXT("climb"), EVerb::Traverse }, { TEXT("mantles"), TEXT("climb"), EVerb::Traverse },
			{ TEXT("clambers"), TEXT("climb"), EVerb::Traverse }, { TEXT("scrambles"), TEXT("climb"), EVerb::Traverse },
			// Weapons.
			{ TEXT("draws"), TEXT(""), EVerb::Draw }, { TEXT("unholsters"), TEXT(""), EVerb::Draw }, { TEXT("unsheathes"), TEXT(""), EVerb::Draw },
			{ TEXT("readies"), TEXT(""), EVerb::Draw }, { TEXT("picks"), TEXT(""), EVerb::Draw }, { TEXT("grabs"), TEXT(""), EVerb::Draw }, { TEXT("raises"), TEXT(""), EVerb::Draw }, { TEXT("brandishes"), TEXT(""), EVerb::Draw },
			{ TEXT("holsters"), TEXT("holster"), EVerb::Holster }, { TEXT("sheathes"), TEXT("holster"), EVerb::Holster },
			// Violence.
			{ TEXT("attacks"), TEXT("attack"), EVerb::Attack }, { TEXT("strikes"), TEXT("attack"), EVerb::Attack }, { TEXT("slashes"), TEXT("attack"), EVerb::Attack },
			{ TEXT("stabs"), TEXT("attack"), EVerb::Attack }, { TEXT("swings"), TEXT("attack"), EVerb::Attack }, { TEXT("lunges"), TEXT("attack"), EVerb::Attack },
			{ TEXT("hits"), TEXT("attack"), EVerb::Attack }, { TEXT("slams"), TEXT("attack heavy"), EVerb::Attack }, { TEXT("punches"), TEXT("punch"), EVerb::Attack },
			{ TEXT("kicks"), TEXT("kick"), EVerb::Attack }, { TEXT("shoots"), TEXT("shoot"), EVerb::Attack }, { TEXT("fires"), TEXT("shoot"), EVerb::Attack },
			{ TEXT("kills"), TEXT("attack kill"), EVerb::Attack }, { TEXT("executes"), TEXT("attack kill"), EVerb::Attack },
			// Looking.
			{ TEXT("looks"), TEXT("look"), EVerb::Look }, { TEXT("glances"), TEXT("look"), EVerb::Look }, { TEXT("stares"), TEXT("look"), EVerb::Look },
			{ TEXT("turns"), TEXT("look"), EVerb::Look }, { TEXT("faces"), TEXT("look"), EVerb::Look }, { TEXT("watches"), TEXT("look"), EVerb::Look },
			{ TEXT("eyes"), TEXT("look"), EVerb::Look }, { TEXT("studies"), TEXT("look"), EVerb::Look },
			// Gestures.
			{ TEXT("nods"), TEXT("nod"), EVerb::Gesture }, { TEXT("shrugs"), TEXT("shrug"), EVerb::Gesture }, { TEXT("waves"), TEXT("wave"), EVerb::Gesture },
			{ TEXT("salutes"), TEXT("salute"), EVerb::Gesture }, { TEXT("kneels"), TEXT("kneel"), EVerb::Gesture }, { TEXT("sits"), TEXT("sit"), EVerb::Gesture },
			{ TEXT("laughs"), TEXT("laugh"), EVerb::Gesture }, { TEXT("points"), TEXT("point"), EVerb::Gesture }, { TEXT("bows"), TEXT("bow"), EVerb::Gesture },
			{ TEXT("sighs"), TEXT("sigh"), EVerb::Gesture }, { TEXT("cheers"), TEXT("cheer"), EVerb::Gesture }, { TEXT("taunts"), TEXT("taunt"), EVerb::Gesture },
			// Stillness and evasion.
			{ TEXT("waits"), TEXT("wait 2"), EVerb::Wait }, { TEXT("pauses"), TEXT("wait 1"), EVerb::Wait }, { TEXT("freezes"), TEXT("wait 2"), EVerb::Wait },
			{ TEXT("hesitates"), TEXT("wait 1"), EVerb::Wait },
			{ TEXT("rolls"), TEXT("dodge"), EVerb::Dodge }, { TEXT("dodges"), TEXT("dodge"), EVerb::Dodge }, { TEXT("ducks"), TEXT("dodge"), EVerb::Dodge },
			{ TEXT("sidesteps"), TEXT("dodge"), EVerb::Dodge },
		};
		return Table;
	}

	FString WeaponFor(const FString& Word)
	{
		static const TMap<FString, FString> Weapons = {
			{ TEXT("sword"), TEXT("sword") }, { TEXT("blade"), TEXT("sword") }, { TEXT("saber"), TEXT("sword") }, { TEXT("sabre"), TEXT("sword") },
			{ TEXT("katana"), TEXT("sword") }, { TEXT("pistol"), TEXT("pistol") }, { TEXT("gun"), TEXT("pistol") }, { TEXT("blaster"), TEXT("pistol") },
			{ TEXT("revolver"), TEXT("pistol") }, { TEXT("handgun"), TEXT("pistol") }, { TEXT("sidearm"), TEXT("pistol") }, { TEXT("weapon"), TEXT("pistol") },
			{ TEXT("rifle"), TEXT("rifle") }, { TEXT("carbine"), TEXT("rifle") }, { TEXT("shotgun"), TEXT("rifle") }, { TEXT("knife"), TEXT("dagger") },
			{ TEXT("dagger"), TEXT("dagger") }, { TEXT("bow"), TEXT("bow") }, { TEXT("staff"), TEXT("wand") }, { TEXT("wand"), TEXT("wand") } };
		const FString* Found = Weapons.Find(Word);
		return Found ? *Found : FString();
	}

	FString Normalize(const FString& Text)
	{
		FString Out;
		bool bSpace = false;
		for (const TCHAR C : Text.ToLower())
		{
			if (FChar::IsAlnum(C) || C == TEXT('\''))
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

	struct FCastAliases
	{
		TMap<FString, FString> Map;

		void Build(const TArray<FString>& Names)
		{
			static const TSet<FString> Titles = { TEXT("the"), TEXT("admiral"), TEXT("captain"), TEXT("doctor"), TEXT("dr"), TEXT("mr"), TEXT("mrs"), TEXT("ms"),
				TEXT("sergeant"), TEXT("commander"), TEXT("lieutenant"), TEXT("general"), TEXT("officer"), TEXT("agent"), TEXT("lord"), TEXT("lady"), TEXT("sir") };
			// Group cues are their people ("TARRIK AND SELENE" -> TARRIK, SELENE), and "and" is nobody's name.
			TArray<FString> People;
			for (const FString& Name : Names)
			{
				People.Append(UCinemafyScriptLibrary::PeopleInCue(Name));
			}
			TMap<FString, int32> Use;
			for (const FString& Name : People)
			{
				TArray<FString> Words;
				Normalize(Name).ParseIntoArrayWS(Words);
				for (const FString& Word : Words)
				{
					Use.FindOrAdd(Word) += 1;
				}
			}
			for (const FString& Name : People)
			{
				const FString Full = Normalize(Name);
				Map.Add(Full, Name);
				TArray<FString> Words;
				Full.ParseIntoArrayWS(Words);
				// "ADMIRAL THANE" is also "thane"; "DEAD HAND" is never just "hand".
				TArray<FString> Own = Words.FilterByPredicate([](const FString& Word) { return !Titles.Contains(Word); });
				if (Words.Num() > 1 && Own.Num() == 1 && Own[0].Len() >= 3 && Use.FindRef(Own[0]) == 1)
				{
					Map.Add(Own[0], Name);
					Map.Add(Own[0] + TEXT("'s"), Name);
				}
			}
		}

		/** The character a word (or a two-word run starting at Index) names, and how many words it took. */
		FString At(const TArray<FString>& Words, int32 Index, int32& OutLength) const
		{
			if (Index + 1 < Words.Num())
			{
				if (const FString* Two = Map.Find(Words[Index] + TEXT(" ") + Words[Index + 1]))
				{
					OutLength = 2;
					return *Two;
				}
			}
			OutLength = 1;
			const FString* One = Map.Find(Words[Index]);
			return One ? *One : FString();
		}
	};

	bool IsDeterminer(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("the"), TEXT("a"), TEXT("an"), TEXT("her"), TEXT("his"), TEXT("their"), TEXT("its"), TEXT("my"), TEXT("your") };
		return Set.Contains(Word);
	}

	bool IsPreposition(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("to"), TEXT("toward"), TEXT("towards"), TEXT("into"), TEXT("onto"), TEXT("over"), TEXT("across"), TEXT("through"),
			TEXT("at"), TEXT("up"), TEXT("past"), TEXT("behind"), TEXT("beside"), TEXT("near"), TEXT("by"), TEXT("on"), TEXT("off"), TEXT("down"), TEXT("around") };
		return Set.Contains(Word);
	}

	bool IsStop(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("and"), TEXT("then"), TEXT("as"), TEXT("while"), TEXT("before"), TEXT("after"), TEXT("but"), TEXT("with"),
			TEXT("from"), TEXT("away"), TEXT("again"), TEXT("slowly"), TEXT("quickly"), TEXT("clear"), TEXT("instead"), TEXT("of"), TEXT("than"),
			TEXT("because"), TEXT("until"), TEXT("once"), TEXT("so"), TEXT("which"), TEXT("that"), TEXT("who"), TEXT("where"), TEXT("when"), TEXT("if"),
			TEXT("for"), TEXT("like"), TEXT("now"), TEXT("still"), TEXT("too"), TEXT("only"), TEXT("just"), TEXT("even"), TEXT("almost"), TEXT("already"),
			TEXT("is"), TEXT("are"), TEXT("was"), TEXT("were"), TEXT("has"), TEXT("have"), TEXT("not"), TEXT("never"), TEXT("together"), TEXT("alone") };
		return Set.Contains(Word);
	}

	/** Up to three words of noun phrase from Index, past determiners. */
	FString NounPhrase(const TArray<FString>& Words, int32 Index)
	{
		while (Index < Words.Num() && IsDeterminer(Words[Index]))
		{
			++Index;
		}
		TArray<FString> Phrase;
		for (; Index < Words.Num() && Phrase.Num() < 3; ++Index)
		{
			if (IsStop(Words[Index]) || IsPreposition(Words[Index]))
			{
				break;
			}
			Phrase.Add(Words[Index]);
		}
		return FString::Join(Phrase, TEXT(" "));
	}

	/** Splits a stretch of action into clauses: sentences, then ", " / "; " / " and " / " then " runs. */
	TArray<FString> Clauses(const FString& Action)
	{
		TArray<FString> Sentences;
		Action.Replace(TEXT("!"), TEXT(".")).Replace(TEXT("?"), TEXT(".")).ParseIntoArray(Sentences, TEXT(". "), true);
		TArray<FString> Out;
		for (FString Sentence : Sentences)
		{
			Sentence = Sentence.Replace(TEXT(";"), TEXT(",")).Replace(TEXT(" and then "), TEXT(",")).Replace(TEXT(" then "), TEXT(","))
				.Replace(TEXT(" and "), TEXT(","));
			TArray<FString> Parts;
			Sentence.ParseIntoArray(Parts, TEXT(","), true);
			for (const FString& Part : Parts)
			{
				const FString Trimmed = Part.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					Out.Add(Trimmed);
				}
			}
		}
		return Out;
	}

	bool IsMovePrompt(const FString& Prompt)
	{
		return Prompt == TEXT("walk") || Prompt == TEXT("run") || Prompt == TEXT("sprint") || Prompt == TEXT("sneak");
	}
}

TArray<FCinemafyStagedBeat> UCinemafyPrevisLibrary::StageScene(const FCinemafyScriptScene& Scene, TArray<FString>& OutNotStaged)
{
	using namespace CinemafyStaging;
	TArray<FCinemafyStagedBeat> Beats;
	OutNotStaged.Reset();

	// Who's who: everyone the breakdown found in the scene, plus anyone who speaks.
	TArray<FString> Names = Scene.Characters;
	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			Names.AddUnique(Line.Speaker);
		}
	}
	// Voices with no body ("SHIP ANNOUNCEMENT") never walk, turn or get walked to.
	Names.RemoveAll([](const FString& Name) { return UCinemafyScriptLibrary::IsVoiceOnly(Name, FString()); });
	FCastAliases Aliases;
	Aliases.Build(Names);

	// People the blocking places (lying on the bunk, seated at the console) hold it: they don't walk or sit down again.
	TSet<FString> Holding;
	for (const FCinemafyBlock& Block : BlockScene(Scene))
	{
		Holding.Append(Block.Characters);
	}

	// What each character holds coming in.
	TMap<FString, FString> InHand;
	for (const FCinemafyContinuityFact& Fact : Scene.CarriedIn)
	{
		if (Fact.Category == TEXT("Carrying"))
		{
			TArray<FString> Words;
			Normalize(Fact.Fact).ParseIntoArrayWS(Words);
			for (const FString& Word : Words)
			{
				const FString Weapon = WeaponFor(Word);
				if (!Weapon.IsEmpty())
				{
					InHand.Add(Fact.Character, Weapon);
				}
			}
		}
	}

	TMap<FString, int32> LinesSaid;
	int32 TotalLines = 0;
	TMap<FString, int32> LastBeatOf;  // Each character's latest beat (index into Beats).
	TArray<int32> LinesAtBeat;        // Lines spoken in the scene when each beat was staged.
	FString LastSpeaker;       // Who spoke most recently.
	FString PendingCueFrom;    // Someone just finished a line: the next action by anyone else waits for it.
	FString LastNamed;

	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue && UCinemafyScriptLibrary::IsVoiceOnly(Line.Speaker, Line.Direction))
		{
			// Heard, not seen: no body walks it or says it. Whoever is there to hear it (the last to speak, else the
			// next, else anyone in the scene) carries it in the take, so it plays in time and gives its cue.
			FString Hearer = LastSpeaker;
			if (Hearer.IsEmpty())
			{
				bool bAfter = false;
				for (const FCinemafyScriptLine& Later : Scene.Lines)
				{
					bAfter |= &Later == &Line;
					if (bAfter && &Later != &Line && Later.Kind == ECinemafyScriptLineKind::Dialogue && !UCinemafyScriptLibrary::IsVoiceOnly(Later.Speaker, Later.Direction))
					{
						Hearer = Later.Speaker;
						break;
					}
				}
			}
			if (Hearer.IsEmpty() && Names.Num() > 0)
			{
				Hearer = Names[0];
			}
			if (Hearer.IsEmpty())
			{
				continue; // Nobody on set: the cut still plays it.
			}
			LastBeatOf.Add(Hearer, Beats.Num());
			LinesAtBeat.Add(TotalLines + 1);
			FCinemafyStagedBeat& Heard = Beats.AddDefaulted_GetRef();
			Heard.Character = Hearer;
			Heard.Prompt = TEXT("say");
			Heard.Line = Line.Text;
			Heard.LineAudio = Line.Audio;
			Heard.LineDirection = Line.Direction;
			Heard.HeardFrom = Line.Speaker;
			Heard.Source = Line.Text;
			LinesSaid.FindOrAdd(Hearer) += 1;
			++TotalLines;
			LastSpeaker = Hearer;
			PendingCueFrom = Hearer;
			continue;
		}
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			// "TARRIK AND SELENE": the first of them carries it on set (there's no actor called that).
			const TArray<FString> People = UCinemafyScriptLibrary::PeopleInCue(Line.Speaker);
			const FString Speaker = People.Num() > 0 ? People[0] : Line.Speaker;
			// Said to the last other speaker, or the next one to speak.
			FString Listener = LastSpeaker != Speaker ? LastSpeaker : FString();
			if (Listener.IsEmpty())
			{
				bool bAfter = false;
				for (const FCinemafyScriptLine& Later : Scene.Lines)
				{
					if (&Later == &Line)
					{
						bAfter = true;
						continue;
					}
					if (bAfter && Later.Kind == ECinemafyScriptLineKind::Dialogue && Later.Speaker != Speaker && !UCinemafyScriptLibrary::IsVoiceOnly(Later.Speaker, Later.Direction))
					{
						Listener = Later.Speaker;
						break;
					}
				}
			}

			// The speaker's own last beat, if nobody has spoken since it started.
			const int32* LastOwn = LastBeatOf.Find(Speaker);
			FCinemafyStagedBeat* Previous = LastOwn && LinesAtBeat[*LastOwn] == TotalLines ? &Beats[*LastOwn] : nullptr;
			if (Previous && IsMovePrompt(Previous->Prompt) && Previous->Line.IsEmpty())
			{
				// Walk and talk: the line rides on the move it follows.
				Previous->Line = Line.Text;
				Previous->LineAudio = Line.Audio;
				Previous->LineDirection = Line.Direction;
				if (Previous->Target.IsEmpty())
				{
					Previous->Target = Listener;
				}
			}
			else
			{
				LastBeatOf.Add(Speaker, Beats.Num());
				LinesAtBeat.Add(TotalLines + 1);
				FCinemafyStagedBeat& Say = Beats.AddDefaulted_GetRef();
				Say.Character = Speaker;
				Say.Prompt = TEXT("say");
				Say.Line = Line.Text;
				Say.LineAudio = Line.Audio;
				Say.LineDirection = Line.Direction;
				Say.Target = Listener;
				Say.Source = Line.Text;
				if (!LastSpeaker.IsEmpty() && LastSpeaker != Speaker)
				{
					Say.WaitFor = LastSpeaker;
					Say.WaitForLineNumber = LinesSaid.FindRef(LastSpeaker);
				}
			}
			LinesSaid.FindOrAdd(Speaker) += 1;
			++TotalLines;
			LastSpeaker = Speaker;
			PendingCueFrom = Speaker;
			continue;
		}
		if (Line.Kind != ECinemafyScriptLineKind::Action)
		{
			continue;
		}

		FString Subject;
		for (const FString& Clause : Clauses(Line.Text))
		{
			TArray<FString> Words;
			Normalize(Clause).ParseIntoArrayWS(Words);
			if (Words.Num() == 0)
			{
				continue;
			}

			// The subject: the first character named before the verb, a pronoun, or the last clause's subject.
			int32 VerbIndex = INDEX_NONE;
			const FVerb* Verb = nullptr;
			FString ClauseSubject;
			for (int32 Index = 0; Index < Words.Num(); ++Index)
			{
				int32 Length = 1;
				const FString Named = Aliases.At(Words, Index, Length);
				if (!Named.IsEmpty())
				{
					if (ClauseSubject.IsEmpty() && !Verb)
					{
						ClauseSubject = Named;
					}
					LastNamed = Named;
					Index += Length - 1;
					continue;
				}
				if (!Verb)
				{
					Verb = Verbs().FindByPredicate([&Words, Index](const FVerb& V) { return Words[Index] == V.Word; });
					if (Verb)
					{
						VerbIndex = Index;
					}
				}
			}
			if (ClauseSubject.IsEmpty())
			{
				const bool bPronoun = Words[0] == TEXT("she") || Words[0] == TEXT("he") || Words[0] == TEXT("her") || Words[0] == TEXT("his");
				ClauseSubject = bPronoun ? LastNamed : Subject;
			}
			Subject = ClauseSubject;
			if (!Verb || Subject.IsEmpty())
			{
				if (Verb || Words.Num() > 2)
				{
					OutNotStaged.AddUnique(Clause);
				}
				continue;
			}
			const bool bChangesPlace = Verb->Kind == EVerb::Move || Verb->Kind == EVerb::Traverse || Verb->Kind == EVerb::Dodge
				|| (Verb->Kind == EVerb::Gesture && (FCString::Strcmp(Verb->Prompt, TEXT("sit")) == 0 || FCString::Strcmp(Verb->Prompt, TEXT("kneel")) == 0));
			if (bChangesPlace && Holding.Contains(Subject))
			{
				OutNotStaged.AddUnique(FString::Printf(TEXT("%s holds the blocking, so not: \"%s\""), *Subject, *Clause));
				continue;
			}

			// The object: a character (target) or a place, after a preposition or straight after the verb.
			FString Target, Place;
			for (int32 Index = VerbIndex + 1; Index < Words.Num(); ++Index)
			{
				int32 Length = 1;
				const FString Named = Aliases.At(Words, Index, Length);
				if (!Named.IsEmpty() && Named != Subject)
				{
					Target = Named;
					break;
				}
				if (Place.IsEmpty() && (IsPreposition(Words[Index]) || Index == VerbIndex + 1))
				{
					const FString Phrase = NounPhrase(Words, IsPreposition(Words[Index]) ? Index + 1 : Index);
					if (!Phrase.IsEmpty() && Aliases.At(Words, IsPreposition(Words[Index]) ? Index + 1 : Index, Length).IsEmpty())
					{
						Place = Phrase;
					}
				}
			}

			FCinemafyStagedBeat Beat;
			Beat.Character = Subject;
			Beat.Source = Clause;
			Beat.Target = Target;
			switch (Verb->Kind)
			{
			case EVerb::Move:
			case EVerb::Traverse:
				Beat.Prompt = Verb->Prompt;
				Beat.Place = Target.IsEmpty() ? Place : FString();
				break;
			case EVerb::Draw:
			{
				FString Weapon;
				for (int32 Index = VerbIndex + 1; Index < Words.Num() && Weapon.IsEmpty(); ++Index)
				{
					Weapon = WeaponFor(Words[Index]);
				}
				if (Weapon.IsEmpty())
				{
					OutNotStaged.AddUnique(Clause);
					continue;
				}
				Beat.Prompt = Weapon;
				InHand.Add(Subject, Weapon);
				break;
			}
			case EVerb::Holster:
				Beat.Prompt = Verb->Prompt;
				InHand.Remove(Subject);
				break;
			case EVerb::Attack:
			{
				FString Prompt = Verb->Prompt;
				const FString* Held = InHand.Find(Subject);
				if (Held && (Prompt == TEXT("attack") || Prompt.StartsWith(TEXT("attack "))))
				{
					Prompt = *Held + TEXT(" ") + Prompt;
				}
				Beat.Prompt = Prompt;
				Beat.Place = Target.IsEmpty() ? Place : FString();
				break;
			}
			case EVerb::Look:
				Beat.Prompt = Verb->Prompt;
				Beat.Place = Target.IsEmpty() ? Place : FString();
				break;
			default:
				Beat.Prompt = Verb->Prompt;
				break;
			}

			// Right after someone else's line, an action waits for it to finish.
			if (!PendingCueFrom.IsEmpty() && PendingCueFrom != Subject)
			{
				Beat.WaitFor = PendingCueFrom;
				Beat.WaitForLineNumber = LinesSaid.FindRef(PendingCueFrom);
			}
			PendingCueFrom.Reset();
			LastBeatOf.Add(Subject, Beats.Num());
			LinesAtBeat.Add(TotalLines);
			Beats.Add(Beat);
		}
	}
	return Beats;
}

namespace CinemafyDressing
{
	using namespace CinemafyStaging;

	bool IsPlacePreposition(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("to"), TEXT("toward"), TEXT("towards"), TEXT("into"), TEXT("onto"), TEXT("over"), TEXT("across"),
			TEXT("at"), TEXT("past"), TEXT("behind"), TEXT("beside"), TEXT("near"), TEXT("by"), TEXT("on"), TEXT("off"), TEXT("around"), TEXT("from"),
			TEXT("against"), TEXT("under"), TEXT("beneath"), TEXT("inside"), TEXT("atop"), TEXT("upon") };
		return Set.Contains(Word);
	}

	/** Not dressing: architecture, bodies, people, light and air. */
	bool IsGeneric(const FString& Noun)
	{
		static const TSet<FString> Set = { TEXT("wall"), TEXT("walls"), TEXT("floor"), TEXT("ground"), TEXT("ceiling"), TEXT("room"), TEXT("air"),
			TEXT("side"), TEXT("way"), TEXT("corner"), TEXT("hand"), TEXT("hands"), TEXT("head"), TEXT("face"), TEXT("eyes"), TEXT("eye"), TEXT("back"),
			TEXT("arm"), TEXT("arms"), TEXT("leg"), TEXT("legs"), TEXT("feet"), TEXT("foot"), TEXT("shoulder"), TEXT("chest"), TEXT("body"), TEXT("mouth"),
			TEXT("lips"), TEXT("breath"), TEXT("moment"), TEXT("beat"), TEXT("silence"), TEXT("light"), TEXT("lights"), TEXT("shadow"), TEXT("shadows"),
			TEXT("darkness"), TEXT("distance"), TEXT("edge"), TEXT("middle"), TEXT("center"), TEXT("centre"), TEXT("end"), TEXT("top"), TEXT("bottom"),
			TEXT("front"), TEXT("rear"), TEXT("him"), TEXT("her"), TEXT("them"), TEXT("it"), TEXT("himself"), TEXT("herself"), TEXT("themselves"),
			TEXT("everyone"), TEXT("everybody"), TEXT("someone"), TEXT("something"), TEXT("nothing"), TEXT("view"), TEXT("sight"), TEXT("place"),
			TEXT("spot"), TEXT("area"), TEXT("space"), TEXT("cover"), TEXT("gap"), TEXT("tears"), TEXT("pain"), TEXT("trigger"), TEXT("grip"),
			TEXT("hilt"), TEXT("knees"), TEXT("knee"), TEXT("feet"), TEXT("sky"), TEXT("horizon"), TEXT("crowd"), TEXT("one"), TEXT("other"),
			TEXT("others"), TEXT("us"), TEXT("me"), TEXT("you"), TEXT("this"), TEXT("that"), TEXT("there"), TEXT("here"), TEXT("door"), TEXT("doorway"),
			// The rest of the body, and what it does.
			TEXT("faces"), TEXT("heads"), TEXT("neck"), TEXT("necks"), TEXT("throat"), TEXT("collarbone"), TEXT("heart"), TEXT("hair"), TEXT("skin"),
			TEXT("cheek"), TEXT("cheeks"), TEXT("jaw"), TEXT("brow"), TEXT("forehead"), TEXT("temple"), TEXT("temples"), TEXT("finger"), TEXT("fingers"),
			TEXT("fingertips"), TEXT("thumb"), TEXT("wrist"), TEXT("wrists"), TEXT("palm"), TEXT("palms"), TEXT("fist"), TEXT("fists"), TEXT("knuckles"),
			TEXT("elbow"), TEXT("spine"), TEXT("ribs"), TEXT("rib"), TEXT("hip"), TEXT("hips"), TEXT("waist"), TEXT("lap"), TEXT("stomach"), TEXT("gut"),
			TEXT("shoulders"), TEXT("blood"), TEXT("sweat"), TEXT("voice"), TEXT("gaze"), TEXT("glance"), TEXT("expression"), TEXT("smile"), TEXT("teeth"),
			TEXT("tongue"), TEXT("nose"), TEXT("ear"), TEXT("ears"), TEXT("eyelids"), TEXT("pulse"), TEXT("wound"), TEXT("wounds"), TEXT("scar"), TEXT("ankle"),
			TEXT("thigh"), TEXT("heel"), TEXT("heels"), TEXT("toes"), TEXT("mind"), TEXT("thoughts"), TEXT("memory"), TEXT("name"), TEXT("words"),
			// Picture-making words, not things on set.
			TEXT("foreground"), TEXT("background"), TEXT("frame"), TEXT("shot"), TEXT("scene"), TEXT("focus"), TEXT("cut"),
			TEXT("glow"), TEXT("dark"), TEXT("quiet"), TEXT("noise"), TEXT("sound"), TEXT("echo"), TEXT("time"), TEXT("seconds"), TEXT("instant"),
			TEXT("way"), TEXT("direction"), TEXT("line"), TEXT("lines"), TEXT("course"), TEXT("signal"), TEXT("channel"), TEXT("order"), TEXT("orders") };
		return Noun.IsEmpty() || Set.Contains(Noun);
	}

	bool IsAdjective(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("empty"), TEXT("old"), TEXT("new"), TEXT("broken"), TEXT("small"), TEXT("large"), TEXT("big"),
			TEXT("huge"), TEXT("tiny"), TEXT("heavy"), TEXT("open"), TEXT("closed"), TEXT("dark"), TEXT("bright"), TEXT("nearby"), TEXT("metal"),
			TEXT("wooden"), TEXT("steel"), TEXT("rusty"), TEXT("dusty"), TEXT("abandoned"), TEXT("battered"), TEXT("ancient"), TEXT("makeshift"),
			TEXT("ornate"), TEXT("damaged"), TEXT("smashed"), TEXT("overturned"), TEXT("long"), TEXT("short"), TEXT("tall"), TEXT("low"), TEXT("high"),
			TEXT("massive"), TEXT("giant"), TEXT("little"), TEXT("same"), TEXT("other"), TEXT("last"), TEXT("first"), TEXT("next"), TEXT("lone") };
		return Set.Contains(Word);
	}

	bool IsHandVerb(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("picks"), TEXT("grabs"), TEXT("holds"), TEXT("takes"), TEXT("carries"), TEXT("pockets"), TEXT("drops"),
			TEXT("draws"), TEXT("unholsters"), TEXT("holsters"), TEXT("sheathes"), TEXT("unsheathes"), TEXT("raises"), TEXT("brandishes"),
			TEXT("clutches"), TEXT("tosses"), TEXT("throws"), TEXT("catches"), TEXT("loads"), TEXT("reloads"), TEXT("pulls"), TEXT("lifts"),
			TEXT("snatches"), TEXT("wields"), TEXT("readies"), TEXT("hands"), TEXT("offers"), TEXT("reads"), TEXT("checks") };
		return Set.Contains(Word);
	}

	bool IsWearVerb(const FString& Word)
	{
		return Word == TEXT("wears") || Word == TEXT("dons") || Word == TEXT("sports") || Word == TEXT("wearing");
	}

	bool IsTraverseVerb(const FString& Word)
	{
		static const TSet<FString> Set = { TEXT("vaults"), TEXT("hurdles"), TEXT("climbs"), TEXT("mantles"), TEXT("clambers"), TEXT("scrambles"),
			TEXT("jumps"), TEXT("leaps") };
		return Set.Contains(Word);
	}

	/** "empty command chair" -> "command chair"; nothing if it's not a thing. */
	FString Thing(const FString& Phrase)
	{
		TArray<FString> Words;
		Phrase.ParseIntoArrayWS(Words);
		while (Words.Num() > 1 && IsAdjective(Words[0]))
		{
			Words.RemoveAt(0);
		}
		if (Words.Num() == 0)
		{
			return FString();
		}
		if (Words.Last().EndsWith(TEXT("'s")))
		{
			return FString(); // "Thane's" is a person, not a thing.
		}
		if (IsGeneric(Words.Last()) || (Words.Num() == 1 && IsAdjective(Words[0])))
		{
			return FString();
		}
		return FString::Join(Words, TEXT(" "));
	}

	/** Skips "up", "out" and determiners after a verb: "picks up a saber" -> the index of "saber". */
	int32 ObjectStart(const TArray<FString>& Words, int32 Index)
	{
		while (Index < Words.Num() && (Words[Index] == TEXT("up") || Words[Index] == TEXT("out") || Words[Index] == TEXT("off")
			|| Words[Index] == TEXT("down") || Words[Index] == TEXT("over") || Words[Index] == TEXT("onto") || IsDeterminer(Words[Index])))
		{
			++Index;
		}
		return Index;
	}
}

TArray<FCinemafySetPiece> UCinemafyPrevisLibrary::FindSetDressing(const FCinemafyScriptScene& Scene)
{
	using namespace CinemafyDressing;
	TArray<FCinemafySetPiece> Found;

	TArray<FString> Names = Scene.Characters;
	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			Names.AddUnique(Line.Speaker);
		}
	}
	FCastAliases Aliases;
	Aliases.Build(Names);
	const FString Location = Normalize(Scene.Location);
	TArray<FString> LocationWords;
	Location.ParseIntoArrayWS(LocationWords);

	auto Add = [&](const FString& Name, ECinemafyDressingKind Kind, const FString& Who, const FString& Source)
	{
		if (Name.IsEmpty() || Location.Contains(Name) || LocationWords.Contains(Name))
		{
			return;
		}
		FCinemafySetPiece* Piece = Found.FindByPredicate([&Name](const FCinemafySetPiece& P) { return P.Name == Name; });
		if (!Piece)
		{
			Piece = &Found.AddDefaulted_GetRef();
			Piece->Name = Name;
			Piece->Kind = Kind;
			Piece->Source = Source;
			Piece->Scenes.Add(Scene.Id);
		}
		if (!Who.IsEmpty() && Kind != ECinemafyDressingKind::SetPiece)
		{
			Piece->UsedBy.AddUnique(Who);
		}
	};

	FString LastNamed;
	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		if (Line.Kind != ECinemafyScriptLineKind::Action)
		{
			continue;
		}
		FString Subject;
		for (const FString& Clause : Clauses(Line.Text))
		{
			TArray<FString> Words;
			Normalize(Clause).ParseIntoArrayWS(Words);
			if (Words.Num() == 0)
			{
				continue;
			}
			// Who's doing it: the first character named, a pronoun, or the last clause's subject.
			FString ClauseSubject;
			for (int32 Index = 0; Index < Words.Num(); ++Index)
			{
				int32 Length = 1;
				const FString Named = Aliases.At(Words, Index, Length);
				if (!Named.IsEmpty())
				{
					ClauseSubject = ClauseSubject.IsEmpty() ? Named : ClauseSubject;
					LastNamed = Named;
					Index += Length - 1;
				}
			}
			if (ClauseSubject.IsEmpty())
			{
				const bool bPronoun = Words[0] == TEXT("she") || Words[0] == TEXT("he") || Words[0] == TEXT("they");
				ClauseSubject = bPronoun ? LastNamed : Subject;
			}
			Subject = ClauseSubject;

			for (int32 Index = 0; Index < Words.Num(); ++Index)
			{
				const FString& Word = Words[Index];
				const bool bHand = IsHandVerb(Word);
				const bool bWear = IsWearVerb(Word);
				const bool bOver = IsTraverseVerb(Word);
				const bool bAt = Index > 0 && IsPlacePreposition(Word);
				if (!bHand && !bWear && !bOver && !bAt)
				{
					continue;
				}
				const int32 Start = ObjectStart(Words, Index + 1);
				int32 Length = 1;
				if (Start >= Words.Num() || !Aliases.At(Words, Start, Length).IsEmpty())
				{
					continue; // A person, not a thing.
				}
				const FString Name = Thing(NounPhrase(Words, Start));
				if (Name.IsEmpty())
				{
					continue;
				}
				if (bHand)
				{
					Add(Name, ECinemafyDressingKind::HandProp, Subject, Clause);
				}
				else if (bWear)
				{
					Add(Name, ECinemafyDressingKind::Wardrobe, Subject, Clause);
				}
				else
				{
					Add(Name, ECinemafyDressingKind::SetPiece, Subject, Clause);
				}
			}

			// Production drafts capitalize props: "She grabs the DATAPAD."
			TArray<FString> Raw;
			Clause.ParseIntoArrayWS(Raw);
			for (int32 Index = 1; Index < Raw.Num(); ++Index)
			{
				if (!IsDeterminer(Normalize(Raw[Index - 1])))
				{
					continue;
				}
				TArray<FString> Caps;
				for (int32 Run = Index; Run < Raw.Num() && Caps.Num() < 3; ++Run)
				{
					FString Token = Raw[Run];
					Token.TrimCharInline(TEXT('.'), nullptr);
					Token.TrimCharInline(TEXT(','), nullptr);
					Token.TrimCharInline(TEXT('!'), nullptr);
					bool bCaps = Token.Len() >= 2;
					for (const TCHAR C : Token)
					{
						bCaps &= FChar::IsUpper(C) || C == TEXT('-');
					}
					if (!bCaps)
					{
						break;
					}
					Caps.Add(Token.ToLower());
				}
				int32 Length = 1;
				if (Caps.Num() > 0 && Aliases.At(Caps, 0, Length).IsEmpty())
				{
					Add(Thing(FString::Join(Caps, TEXT(" "))), ECinemafyDressingKind::HandProp, Subject, Clause);
				}
			}
		}
	}
	return Found;
}

// ---------------------------------------------------------------------------------------------------------------------
// Blocking: the director's own staging, in plain words
// ---------------------------------------------------------------------------------------------------------------------

namespace CinemafyBlocking
{
	using namespace CinemafyStaging;

	bool PoseWord(const FString& Word, ECinemafyPose& OutPose)
	{
		static const TMap<FString, ECinemafyPose> Words = {
			{ TEXT("lie"), ECinemafyPose::Lie }, { TEXT("lies"), ECinemafyPose::Lie }, { TEXT("lying"), ECinemafyPose::Lie },
			{ TEXT("lay"), ECinemafyPose::Lie }, { TEXT("lays"), ECinemafyPose::Lie }, { TEXT("laying"), ECinemafyPose::Lie },
			{ TEXT("laid"), ECinemafyPose::Lie }, { TEXT("sprawled"), ECinemafyPose::Lie }, { TEXT("sprawls"), ECinemafyPose::Lie },
			{ TEXT("reclines"), ECinemafyPose::Lie }, { TEXT("reclining"), ECinemafyPose::Lie }, { TEXT("sleeps"), ECinemafyPose::Lie },
			{ TEXT("sleeping"), ECinemafyPose::Lie }, { TEXT("asleep"), ECinemafyPose::Lie }, { TEXT("cuddle"), ECinemafyPose::Lie },
			{ TEXT("sit"), ECinemafyPose::Sit }, { TEXT("sits"), ECinemafyPose::Sit }, { TEXT("sitting"), ECinemafyPose::Sit },
			{ TEXT("seated"), ECinemafyPose::Sit }, { TEXT("sat"), ECinemafyPose::Sit }, { TEXT("perched"), ECinemafyPose::Sit },
			{ TEXT("perches"), ECinemafyPose::Sit },
			{ TEXT("kneel"), ECinemafyPose::Kneel }, { TEXT("kneels"), ECinemafyPose::Kneel }, { TEXT("kneeling"), ECinemafyPose::Kneel },
			{ TEXT("knelt"), ECinemafyPose::Kneel },
			{ TEXT("lean"), ECinemafyPose::Lean }, { TEXT("leans"), ECinemafyPose::Lean }, { TEXT("leaning"), ECinemafyPose::Lean },
			{ TEXT("leaned"), ECinemafyPose::Lean }, { TEXT("slumped"), ECinemafyPose::Lean }, { TEXT("slumps"), ECinemafyPose::Lean },
			{ TEXT("stand"), ECinemafyPose::Stand }, { TEXT("stands"), ECinemafyPose::Stand }, { TEXT("standing"), ECinemafyPose::Stand },
			{ TEXT("stood"), ECinemafyPose::Stand },
		};
		const ECinemafyPose* Found = Words.Find(Word);
		if (Found)
		{
			OutPose = *Found;
		}
		return Found != nullptr;
	}

	/** Close together: "cuddled", "side by side", "in each other's arms". */
	bool SaysTogether(const TArray<FString>& Words)
	{
		static const TSet<FString> Single = { TEXT("cuddle"), TEXT("cuddles"), TEXT("cuddling"), TEXT("cuddled"), TEXT("snuggle"),
			TEXT("snuggles"), TEXT("snuggled"), TEXT("snuggling"), TEXT("spooning"), TEXT("spooned"), TEXT("embrace"), TEXT("embraces"),
			TEXT("embracing"), TEXT("embraced"), TEXT("entwined"), TEXT("intertwined"), TEXT("together"), TEXT("huddled"), TEXT("nestled") };
		const FString Joined = TEXT(" ") + FString::Join(Words, TEXT(" ")) + TEXT(" ");
		for (const FString& Word : Words)
		{
			if (Single.Contains(Word))
			{
				return true;
			}
		}
		for (const TCHAR* Phrase : { TEXT(" each other "), TEXT(" side by side "), TEXT(" one another "), TEXT(" in his arms "), TEXT(" in her arms "),
			TEXT(" in their arms "), TEXT(" arm in arm "), TEXT(" holding hands "), TEXT(" hold hands "), TEXT(" close together ") })
		{
			if (Joined.Contains(Phrase))
			{
				return true;
			}
		}
		return false;
	}

	bool IsBedLike(const FString& Place)
	{
		for (const TCHAR* Word : { TEXT("bed"), TEXT("bunk"), TEXT("cot"), TEXT("mattress"), TEXT("hammock"), TEXT("berth"), TEXT("futon"),
			TEXT("bedroll"), TEXT("pallet"), TEXT("blanket"), TEXT("sheets") })
		{
			if (Place == Word || Place.EndsWith(FString(TEXT(" ")) + Word))
			{
				return true;
			}
		}
		return false;
	}

	bool IsSeatLike(const FString& Place)
	{
		for (const TCHAR* Word : { TEXT("chair"), TEXT("couch"), TEXT("sofa"), TEXT("bench"), TEXT("stool"), TEXT("throne"), TEXT("seat"),
			TEXT("armchair"), TEXT("booth"), TEXT("pew"), TEXT("settee") })
		{
			if (Place == Word || Place.EndsWith(FString(TEXT(" ")) + Word))
			{
				return true;
			}
		}
		return false;
	}

	/** The place in a clause: the first thing after "on / in / at / against / by..." that isn't a person or a body. */
	FString PlaceIn(const TArray<FString>& Words, const FCastAliases& Aliases)
	{
		static const TSet<FString> Places = { TEXT("on"), TEXT("in"), TEXT("onto"), TEXT("into"), TEXT("across"), TEXT("against"), TEXT("at"),
			TEXT("by"), TEXT("beside"), TEXT("near"), TEXT("upon"), TEXT("atop"), TEXT("under"), TEXT("beneath"), TEXT("inside"), TEXT("to") };
		for (int32 Index = 0; Index + 1 < Words.Num(); ++Index)
		{
			if (!Places.Contains(Words[Index]))
			{
				continue;
			}
			int32 Start = Index + 1;
			while (Start < Words.Num() && IsDeterminer(Words[Start]))
			{
				++Start;
			}
			int32 Length = 1;
			if (Start >= Words.Num() || !Aliases.At(Words, Start, Length).IsEmpty())
			{
				continue; // "on Tarrik's chest": a person, not a place.
			}
			TArray<FString> Phrase;
			for (int32 Word = Start; Word < Words.Num() && Phrase.Num() < 3; ++Word)
			{
				ECinemafyPose Ignored;
				if (IsStop(Words[Word]) || IsPreposition(Words[Word]) || Places.Contains(Words[Word]) || PoseWord(Words[Word], Ignored)
					|| SaysTogether({ Words[Word] }) || IsDeterminer(Words[Word]))
				{
					break;
				}
				Phrase.Add(Words[Word]);
			}
			const FString Place = CinemafyDressing::Thing(FString::Join(Phrase, TEXT(" ")));
			if (!Place.IsEmpty() && Place != TEXT("whole scene") && Place != TEXT("scene"))
			{
				return Place;
			}
		}
		return FString();
	}
}

TArray<FCinemafyBlock> UCinemafyPrevisLibrary::ReadBlocking(const FString& Text, const TArray<FString>& CastNames)
{
	using namespace CinemafyBlocking;
	TArray<FCinemafyBlock> Blocks;
	FCastAliases Aliases;
	Aliases.Build(CastNames);

	TArray<FString> Sentences;
	Text.Replace(TEXT(";"), TEXT(".")).Replace(TEXT("!"), TEXT(".")).Replace(TEXT("?"), TEXT(".")).Replace(TEXT("\n"), TEXT(". "))
		.ParseIntoArray(Sentences, TEXT(". "), true);
	TArray<FString> LastNamed;
	for (FString Sentence : Sentences)
	{
		Sentence.TrimStartAndEndInline();
		Sentence.RemoveFromEnd(TEXT("."));
		TArray<FString> Words;
		Normalize(Sentence).ParseIntoArrayWS(Words);
		if (Words.Num() == 0)
		{
			continue;
		}
		FCinemafyBlock Block;
		Block.Source = Sentence;
		TSet<int32> NameWords;
		for (int32 Index = 0; Index < Words.Num(); ++Index)
		{
			int32 Length = 1;
			const FString Named = Aliases.At(Words, Index, Length);
			if (!Named.IsEmpty())
			{
				Block.Characters.AddUnique(Named);
				for (int32 Word = Index; Word < Index + Length; ++Word)
				{
					NameWords.Add(Word);
				}
				Index += Length - 1;
			}
		}
		// "They", "both", "the two": whoever the last sentence named; "she" / "he": the last one of them.
		const FString Joined = TEXT(" ") + FString::Join(Words, TEXT(" ")) + TEXT(" ");
		if (Block.Characters.Num() == 0)
		{
			if (Joined.Contains(TEXT(" they ")) || Joined.Contains(TEXT(" both ")) || Joined.Contains(TEXT(" the two ")) || Joined.Contains(TEXT(" the pair ")))
			{
				Block.Characters = LastNamed;
			}
			else if ((Words[0] == TEXT("she") || Words[0] == TEXT("he")) && LastNamed.Num() > 0)
			{
				Block.Characters = { LastNamed.Last() };
			}
		}
		if (Block.Characters.Num() == 0)
		{
			continue;
		}
		LastNamed = Block.Characters;

		for (int32 Index = 0; Index < Words.Num(); ++Index)
		{
			if (!NameWords.Contains(Index) && PoseWord(Words[Index], Block.Pose))
			{
				Block.bPoseGiven = true;
				break;
			}
		}
		Block.bTogether = Block.Characters.Num() > 1 && SaysTogether(Words);
		// The place, clause by clause (commas keep "on the bunk, cuddled" from reading as "bunk cuddled").
		TArray<FString> Clauses;
		Sentence.ParseIntoArray(Clauses, TEXT(","), true);
		for (const FString& Clause : Clauses)
		{
			TArray<FString> ClauseWords;
			Normalize(Clause).ParseIntoArrayWS(ClauseWords);
			Block.Place = PlaceIn(ClauseWords, Aliases);
			if (!Block.Place.IsEmpty())
			{
				break;
			}
		}
		if (!Block.bPoseGiven && Block.Place.IsEmpty() && !Block.bTogether)
		{
			continue; // Names, but no staging.
		}
		// "Cuddled on the bunk", "in bed": the place says how.
		if (!Block.bPoseGiven && (Block.bTogether || Joined.Contains(TEXT(" in bed "))) && IsBedLike(Block.Place.IsEmpty() ? FString(TEXT("bed")) : Block.Place))
		{
			Block.Pose = ECinemafyPose::Lie;
			Block.bPoseGiven = Block.bTogether || Joined.Contains(TEXT(" in bed "));
			if (Block.Place.IsEmpty())
			{
				Block.Place = TEXT("bed");
			}
		}
		else if (!Block.bPoseGiven && Block.bTogether && IsSeatLike(Block.Place))
		{
			Block.Pose = ECinemafyPose::Sit;
			Block.bPoseGiven = true;
		}
		Blocks.Add(Block);
	}
	// A later sentence about someone wins over an earlier one.
	for (int32 Index = Blocks.Num() - 1; Index > 0; --Index)
	{
		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
		{
			for (const FString& Name : Blocks[Index].Characters)
			{
				Blocks[Earlier].Characters.Remove(Name);
			}
		}
	}
	Blocks.RemoveAll([](const FCinemafyBlock& Block) { return Block.Characters.Num() == 0; });
	return Blocks;
}

TArray<FCinemafyBlock> UCinemafyPrevisLibrary::BlockScene(const FCinemafyScriptScene& Scene)
{
	TArray<FString> People;
	for (const FString& Name : Scene.Characters)
	{
		People.Append(UCinemafyScriptLibrary::PeopleInCue(Name));
	}
	FString Opening;
	bool bSpoken = false;
	for (const FCinemafyScriptLine& Line : Scene.Lines)
	{
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			People.Append(UCinemafyScriptLibrary::PeopleInCue(Line.Speaker));
			bSpoken = true;
		}
		else if (Line.Kind == ECinemafyScriptLineKind::Action && !bSpoken)
		{
			Opening += Line.Text + TEXT(" ");
		}
	}
	People.RemoveAll([](const FString& Name) { return UCinemafyScriptLibrary::IsVoiceOnly(Name, FString()); });

	// The notes: the whole scene, and they win.
	TArray<FCinemafyBlock> Blocks = ReadBlocking(Scene.Blocking, People);
	TSet<FString> Blocked;
	for (FCinemafyBlock& Block : Blocks)
	{
		Block.bFromNotes = true;
		Blocked.Append(Block.Characters);
	}
	// The page: a pose the opening action sets before anyone speaks ("Kade sits at the console.").
	for (FCinemafyBlock& Block : ReadBlocking(Opening, People))
	{
		Block.Characters.RemoveAll([&Blocked](const FString& Name) { return Blocked.Contains(Name); });
		if (Block.Characters.Num() > 0 && Block.bPoseGiven && Block.Pose != ECinemafyPose::Stand)
		{
			Blocked.Append(Block.Characters);
			Blocks.Add(Block);
		}
	}
	return Blocks;
}

UAnimSequenceBase* UCinemafyPrevisLibrary::FindPoseAnimation(ECinemafyPose Pose, const USkeleton* Skeleton, bool bTogether, int32 Index)
{
	if (Pose == ECinemafyPose::Stand)
	{
		return nullptr;
	}
	const UCinemafySettings* Settings = GetDefault<UCinemafySettings>();
	// Your clips first: made for two, then for the pose.
	if (bTogether)
	{
		for (const FCinemafyPosePair& Pair : Settings->TogetherAnimations)
		{
			if (Pair.Pose == Pose)
			{
				if (UAnimSequenceBase* Clip = (Index % 2 == 0 ? Pair.Lead : Pair.Partner).LoadSynchronous())
				{
					return Clip;
				}
			}
		}
	}
	if (const TSoftObjectPtr<UAnimSequenceBase>* Set = Settings->PoseAnimations.Find(Pose))
	{
		if (UAnimSequenceBase* Clip = Set->LoadSynchronous())
		{
			return Clip;
		}
	}
	if (!Skeleton)
	{
		return nullptr;
	}
	// Else by name, for this skeleton: a held loop ("Lying_Idle", "Sitting_Loop"), not a getting-up or sitting-down.
	TArray<const TCHAR*> Keys;
	switch (Pose)
	{
	case ECinemafyPose::Lie:   Keys = { TEXT("lie"), TEXT("lying"), TEXT("laying"), TEXT("lay"), TEXT("sleep"), TEXT("sleeping"), TEXT("supine"), TEXT("bed") }; break;
	case ECinemafyPose::Sit:   Keys = { TEXT("sit"), TEXT("sitting"), TEXT("seated"), TEXT("chair") }; break;
	case ECinemafyPose::Kneel: Keys = { TEXT("kneel"), TEXT("kneeling") }; break;
	case ECinemafyPose::Lean:  Keys = { TEXT("lean"), TEXT("leaning") }; break;
	default: break;
	}
	static const TSet<FString> Changes = { TEXT("to"), TEXT("getup"), TEXT("standup"), TEXT("start"), TEXT("stop"), TEXT("enter"), TEXT("exit"),
		TEXT("transition"), TEXT("down"), TEXT("up"), TEXT("death"), TEXT("die"), TEXT("dying"), TEXT("dead") };
	const TArray<FCinemafyMoveCandidate> Candidates = UCinemafyChoreographyLibrary::GatherCandidates(nullptr, {}, Skeleton);
	const FCinemafyMoveCandidate* Best = nullptr;
	float BestScore = 0.f;
	for (const FCinemafyMoveCandidate& Candidate : Candidates)
	{
		bool bPose = false, bChange = false;
		float Score = Candidate.Bias;
		for (const FString& Token : Candidate.Tokens)
		{
			for (const TCHAR* Key : Keys)
			{
				bPose |= Token == Key;
			}
			bChange |= Changes.Contains(Token);
			Score += (Token == TEXT("idle") || Token == TEXT("loop")) ? 2.f : 0.f;
			Score += bTogether && (Token == TEXT("cuddle") || Token == TEXT("couple") || Token == TEXT("hug") || Token == TEXT("embrace")) ? 3.f : 0.f;
		}
		if (!bPose || bChange)
		{
			continue;
		}
		Score += 1.f;
		if (!Best || Score > BestScore)
		{
			Best = &Candidate;
			BestScore = Score;
		}
	}
	return Best ? Cast<UAnimSequenceBase>(Best->Path.TryLoad()) : nullptr;
}
