#include "CinemafyPrevis.h"

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
			TMap<FString, int32> Use;
			for (const FString& Name : Names)
			{
				TArray<FString> Words;
				Normalize(Name).ParseIntoArrayWS(Words);
				for (const FString& Word : Words)
				{
					Use.FindOrAdd(Word) += 1;
				}
			}
			for (const FString& Name : Names)
			{
				const FString Full = Normalize(Name);
				Map.Add(Full, Name);
				TArray<FString> Words;
				Full.ParseIntoArrayWS(Words);
				for (const FString& Word : Words)
				{
					if (Word.Len() >= 3 && !Titles.Contains(Word) && Use.FindRef(Word) == 1)
					{
						Map.Add(Word, Name);
						Map.Add(Word + TEXT("'s"), Name);
					}
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
			TEXT("from"), TEXT("away"), TEXT("again"), TEXT("slowly"), TEXT("quickly"), TEXT("clear") };
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
	FCastAliases Aliases;
	Aliases.Build(Names);

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
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			const FString& Speaker = Line.Speaker;
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
					if (bAfter && Later.Kind == ECinemafyScriptLineKind::Dialogue && Later.Speaker != Speaker)
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
			TEXT("others"), TEXT("us"), TEXT("me"), TEXT("you"), TEXT("this"), TEXT("that"), TEXT("there"), TEXT("here"), TEXT("door"), TEXT("doorway") };
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
