#include "CinemafyChoreographyLibrary.h"
#include "CinemafySettings.h"
#include "CinemafyModule.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	/** Synonym groups. The first word of each group is its canonical name. */
	const TArray<TArray<FString>>& SynonymGroups()
	{
		static const TArray<TArray<FString>> Groups = {
			{ TEXT("attack"), TEXT("atk"), TEXT("slash"), TEXT("strike"), TEXT("swing"), TEXT("combo"), TEXT("cleave"), TEXT("chop"), TEXT("smash"), TEXT("thrust"), TEXT("stab"), TEXT("lunge") },
			{ TEXT("punch"), TEXT("jab"), TEXT("hook"), TEXT("uppercut"), TEXT("fist"), TEXT("unarmed"), TEXT("handtohand"), TEXT("hand") },
			{ TEXT("kick"), TEXT("roundhouse") },
			{ TEXT("shoot"), TEXT("fire"), TEXT("shot"), TEXT("blast"), TEXT("snipe") },
			{ TEXT("cast"), TEXT("spell"), TEXT("magic"), TEXT("mage"), TEXT("channel"), TEXT("buff") },
			{ TEXT("taunt"), TEXT("provoke"), TEXT("mock"), TEXT("gloat"), TEXT("challenge"), TEXT("roar"), TEXT("bravado") },
			{ TEXT("wave"), TEXT("greet"), TEXT("hello"), TEXT("hi"), TEXT("goodbye") },
			{ TEXT("point"), TEXT("gesture"), TEXT("indicate") },
			{ TEXT("salute") },
			{ TEXT("cheer"), TEXT("celebrate"), TEXT("victory"), TEXT("win"), TEXT("triumph") },
			{ TEXT("bow"), TEXT("curtsy") },
			{ TEXT("laugh"), TEXT("chuckle") },
			{ TEXT("shrug") },
			{ TEXT("kneel"), TEXT("knee") },
			{ TEXT("sit"), TEXT("seat") },
			{ TEXT("die"), TEXT("death"), TEXT("dead"), TEXT("dying"), TEXT("deaths"), TEXT("dies") },
			{ TEXT("dodge"), TEXT("evade"), TEXT("roll"), TEXT("sidestep"), TEXT("dash") },
			{ TEXT("block"), TEXT("parry"), TEXT("guard"), TEXT("defend") },
			{ TEXT("hit"), TEXT("react"), TEXT("reaction"), TEXT("stagger"), TEXT("flinch"), TEXT("hurt") },
			{ TEXT("draw"), TEXT("equip"), TEXT("unsheathe"), TEXT("unholster"), TEXT("ready") },
			{ TEXT("holster"), TEXT("sheathe"), TEXT("unequip"), TEXT("stow") },
			{ TEXT("traversal"), TEXT("vault"), TEXT("hurdle"), TEXT("mantle"), TEXT("climb") },
			{ TEXT("look"), TEXT("glance"), TEXT("search"), TEXT("scan") },
			{ TEXT("idle"), TEXT("wait"), TEXT("stand"), TEXT("breathe") },
			{ TEXT("heavy"), TEXT("power"), TEXT("strong") },
			{ TEXT("light"), TEXT("quick") },
			{ TEXT("spin"), TEXT("spinning"), TEXT("whirl") },
			{ TEXT("jump"), TEXT("leap"), TEXT("hop") },
		};
		return Groups;
	}

	/** Weapon words -> what animation names / overlay states for it tend to contain (best first). */
	const TMap<FString, TArray<FString>>& WeaponWords()
	{
		static const TMap<FString, TArray<FString>> Words = {
			{ TEXT("sword"),      { TEXT("sword"), TEXT("katana"), TEXT("blade"), TEXT("saber"), TEXT("onehanded"), TEXT("1h"), TEXT("melee"), TEXT("one") } },
			{ TEXT("greatsword"), { TEXT("greatsword"), TEXT("gs"), TEXT("twohanded"), TEXT("2h"), TEXT("claymore"), TEXT("heavy"), TEXT("two") } },
			{ TEXT("twinblades"), { TEXT("twinblades"), TEXT("twin"), TEXT("dual"), TEXT("blades"), TEXT("dsas") } },
			{ TEXT("dagger"),     { TEXT("dagger"), TEXT("knife"), TEXT("onehanded") } },
			{ TEXT("pistol"),     { TEXT("pistol"), TEXT("pistolonehanded"), TEXT("handgun"), TEXT("1911"), TEXT("onehanded"), TEXT("gun") } },
			{ TEXT("pistols"),    { TEXT("pistoltwohanded"), TEXT("pistols"), TEXT("dual"), TEXT("akimbo"), TEXT("pistol"), TEXT("twohanded") } },
			{ TEXT("rifle"),      { TEXT("rifle"), TEXT("gun"), TEXT("carbine"), TEXT("w1"), TEXT("w2") } },
			{ TEXT("bow"),        { TEXT("bow"), TEXT("arrow"), TEXT("archer") } },
			{ TEXT("wand"),       { TEXT("wand"), TEXT("staff"), TEXT("caster"), TEXT("magic"), TEXT("mage"), TEXT("spell") } },
			{ TEXT("unarmed"),    { TEXT("unarmed"), TEXT("default"), TEXT("fist"), TEXT("handtohand"), TEXT("none"), TEXT("masculine") } },
		};
		return Words;
	}

	FString WeaponForWord(const FString& Word)
	{
		static const TMap<FString, FString> Aliases = {
			{ TEXT("sword"), TEXT("sword") }, { TEXT("katana"), TEXT("sword") }, { TEXT("blade"), TEXT("sword") }, { TEXT("saber"), TEXT("sword") },
			{ TEXT("sabre"), TEXT("sword") }, { TEXT("lightsaber"), TEXT("sword") }, { TEXT("longsword"), TEXT("sword") },
			{ TEXT("greatsword"), TEXT("greatsword") }, { TEXT("claymore"), TEXT("greatsword") }, { TEXT("zweihander"), TEXT("greatsword") },
			{ TEXT("twinblades"), TEXT("twinblades") }, { TEXT("blades"), TEXT("twinblades") }, { TEXT("swords"), TEXT("twinblades") },
			{ TEXT("dagger"), TEXT("dagger") }, { TEXT("knife"), TEXT("dagger") },
			{ TEXT("pistol"), TEXT("pistol") }, { TEXT("handgun"), TEXT("pistol") }, { TEXT("blaster"), TEXT("pistol") }, { TEXT("revolver"), TEXT("pistol") },
			{ TEXT("pistols"), TEXT("pistols") }, { TEXT("blasters"), TEXT("pistols") }, { TEXT("akimbo"), TEXT("pistols") },
			{ TEXT("rifle"), TEXT("rifle") }, { TEXT("gun"), TEXT("rifle") }, { TEXT("carbine"), TEXT("rifle") }, { TEXT("shotgun"), TEXT("rifle") },
			{ TEXT("bow"), TEXT("bow") },
			{ TEXT("wand"), TEXT("wand") }, { TEXT("staff"), TEXT("wand") }, { TEXT("magic"), TEXT("wand") },
			{ TEXT("unarmed"), TEXT("unarmed") }, { TEXT("fists"), TEXT("unarmed") }, { TEXT("hands"), TEXT("unarmed") }, { TEXT("barehanded"), TEXT("unarmed") },
		};
		const FString* Found = Aliases.Find(Word);
		return Found ? *Found : FString();
	}

	bool IsOneOf(const FString& Word, std::initializer_list<const TCHAR*> Options)
	{
		for (const TCHAR* Option : Options)
		{
			if (Word.Equals(Option))
			{
				return true;
			}
		}
		return false;
	}

	int32 NumberWord(const FString& Word)
	{
		if (Word.IsNumeric())
		{
			return FMath::RoundToInt(FCString::Atof(*Word));
		}
		static const TMap<FString, int32> Words = {
			{ TEXT("once"), 1 }, { TEXT("one"), 1 }, { TEXT("twice"), 2 }, { TEXT("two"), 2 }, { TEXT("three"), 3 },
			{ TEXT("four"), 4 }, { TEXT("five"), 5 }, { TEXT("six"), 6 } };
		const int32* Found = Words.Find(Word);
		return Found ? *Found : 0;
	}

	/** "attack" -> "attack|atk|slash|...". Words without a group stay as they are. */
	FString Concept(const FString& Word)
	{
		const TArray<FString> Synonyms = UCinemafyChoreographyLibrary::GetSynonyms(Word);
		return FString::Join(Synonyms, TEXT("|"));
	}

	/** Tokens that mean "this isn't a one-shot action" for attacks and gestures. */
	bool IsLocomotionToken(const FString& Token)
	{
		return IsOneOf(Token, { TEXT("walk"), TEXT("run"), TEXT("jog"), TEXT("sprint"), TEXT("loop"), TEXT("locomotion"), TEXT("locomotions"),
			TEXT("strafe"), TEXT("pivot"), TEXT("start"), TEXT("stop"), TEXT("turn"), TEXT("lands"), TEXT("land"), TEXT("ipc"),
			TEXT("additive"), TEXT("pose"), TEXT("poses"), TEXT("ao"), TEXT("aimoffset"), TEXT("crouchwalk") });
	}

	FString StripTrailingNumber(const FString& Name, FString& OutNumber)
	{
		int32 End = Name.Len();
		while (End > 0 && (FChar::IsDigit(Name[End - 1]) || Name[End - 1] == TEXT('_')))
		{
			--End;
		}
		OutNumber = Name.Mid(End);
		return Name.Left(End);
	}
}

TArray<FString> UCinemafyChoreographyLibrary::GetSynonyms(const FString& Word)
{
	const FString Lower = Word.ToLower();
	for (const TArray<FString>& Group : SynonymGroups())
	{
		if (Group.Contains(Lower))
		{
			// The word itself first so an exact match can score a little higher.
			TArray<FString> Result = { Lower };
			for (const FString& Other : Group)
			{
				Result.AddUnique(Other);
			}
			return Result;
		}
	}
	return { Lower };
}

namespace
{
	/** One name's words: split on separators, CamelCase and digits, plus joined CamelCase pairs ("OneHanded"). */
	TArray<FString> TokenizeSegment(const FString& Text)
	{
		TArray<FString> Tokens;
		FString Current;
		auto Flush = [&Tokens, &Current]()
		{
			if (!Current.IsEmpty())
			{
				Tokens.Add(Current.ToLower());
				Current.Reset();
			}
		};

		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			if (!FChar::IsAlnum(Char))
			{
				Flush();
				continue;
			}
			if (!Current.IsEmpty())
			{
				const TCHAR Prev = Current[Current.Len() - 1];
				const bool bCaseBreak = FChar::IsLower(Prev) && FChar::IsUpper(Char);
				const bool bDigitBreak = FChar::IsDigit(Prev) != FChar::IsDigit(Char);
				// "HTTPServer" -> "HTTP", "Server": an upper followed by upper+lower starts a new word.
				const bool bAcronymBreak = FChar::IsUpper(Prev) && FChar::IsUpper(Char) && Index + 1 < Text.Len() && FChar::IsLower(Text[Index + 1]);
				// Keep short alpha-digit combos like "1h", "2h", "w1" together.
				const bool bKeepCombo = bDigitBreak && Current.Len() == 1;
				if ((bCaseBreak || bDigitBreak || bAcronymBreak) && !bKeepCombo)
				{
					Flush();
				}
			}
			Current.AppendChar(Char);
		}
		Flush();

		const int32 Count = Tokens.Num();
		for (int32 Index = 0; Index + 1 < Count; ++Index)
		{
			if (!Tokens[Index].IsNumeric() && !Tokens[Index + 1].IsNumeric())
			{
				Tokens.Add(Tokens[Index] + Tokens[Index + 1]);
			}
		}
		return Tokens;
	}

	/** Groups that name an action (as opposed to modifiers like heavy / light / spin). */
	bool IsActionGroup(const TArray<FString>& Group)
	{
		static const TSet<FString> Actions = { TEXT("attack"), TEXT("punch"), TEXT("kick"), TEXT("shoot"), TEXT("cast"), TEXT("taunt"),
			TEXT("wave"), TEXT("point"), TEXT("salute"), TEXT("cheer"), TEXT("bow"), TEXT("laugh"), TEXT("shrug"), TEXT("kneel"), TEXT("sit"),
			TEXT("die"), TEXT("dodge"), TEXT("block"), TEXT("hit"), TEXT("draw"), TEXT("holster"), TEXT("traversal"), TEXT("jump") };
		return Group.Num() > 0 && Actions.Contains(Group[0]);
	}
}

TArray<FString> UCinemafyChoreographyLibrary::TokenizeName(const FString& NameOrPath)
{
	// The asset's own name counts fully; folder words are kept with an "@" prefix and count for less.
	TArray<FString> Segments;
	NameOrPath.ParseIntoArray(Segments, TEXT("/"));
	TArray<FString> Tokens;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const bool bName = Index == Segments.Num() - 1;
		FString Segment = Segments[Index];
		if (bName)
		{
			// "Name.Name" object paths: just the name.
			int32 Dot = INDEX_NONE;
			if (Segment.FindChar(TEXT('.'), Dot))
			{
				Segment.LeftInline(Dot);
			}
		}
		for (const FString& Token : TokenizeSegment(Segment))
		{
			Tokens.Add(bName ? Token : TEXT("@") + Token);
		}
	}
	return Tokens;
}

FCinemafyPromptReading UCinemafyChoreographyLibrary::ReadPrompt(const FString& Prompt)
{
	FCinemafyPromptReading Reading;

	// Quoted words are a line to say: walk "Stand down, Admiral". Straight or curly quotes.
	FString Actions = Prompt;
	{
		FString Normalized = Prompt.Replace(TEXT("\u201C"), TEXT("\"")).Replace(TEXT("\u201D"), TEXT("\""));
		int32 Open = INDEX_NONE;
		if (Normalized.FindChar(TEXT('"'), Open))
		{
			int32 Close = Normalized.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Close == INDEX_NONE)
			{
				Close = Normalized.Len();
			}
			Reading.Line = Normalized.Mid(Open + 1, Close - Open - 1).TrimStartAndEnd();
			Actions = Normalized.Left(Open) + TEXT(" ") + Normalized.Mid(FMath::Min(Close + 1, Normalized.Len()));
		}
	}

	TArray<FString> Words;
	Actions.ToLower().ParseIntoArrayWS(Words);

	// Speech words make it a line, not an animation search.
	bool bSay = false;
	for (int32 Index = Words.Num() - 1; Index >= 0; --Index)
	{
		FString Word = Words[Index];
		Word.TrimCharInline(TEXT(':'), nullptr);
		Word.TrimCharInline(TEXT(','), nullptr);
		if (IsOneOf(Word, { TEXT("answer"), TEXT("answers"), TEXT("reply"), TEXT("replies"), TEXT("respond"), TEXT("responds"), TEXT("after") }))
		{
			Reading.bAfterLine = true;
			bSay |= Word != TEXT("after");
			Words.RemoveAt(Index);
		}
		else if (IsOneOf(Word, { TEXT("say"), TEXT("says"), TEXT("tell"), TEXT("tells"), TEXT("speak"), TEXT("speaks"), TEXT("line"), TEXT("ask"), TEXT("asks"), TEXT("shout"), TEXT("shouts") }))
		{
			bSay = true;
			Words.RemoveAt(Index);
		}
	}

	// "x3" / "3x" -> 3, and a crude singular ("attacks" -> "attack") except where the plural means something.
	for (FString& Word : Words)
	{
		if (Word.Len() > 1 && (Word.StartsWith(TEXT("x")) || Word.EndsWith(TEXT("x"))))
		{
			const FString Digits = Word.Replace(TEXT("x"), TEXT(""));
			if (Digits.IsNumeric())
			{
				Word = Digits;
				continue;
			}
		}
		if (Word.Len() > 3 && Word.EndsWith(TEXT("s")) && !Word.EndsWith(TEXT("ss")) && WeaponForWord(Word).IsEmpty()
			&& !IsOneOf(Word, { TEXT("dies"), TEXT("bows"), TEXT("times"), TEXT("seconds"), TEXT("towards"), TEXT("always") }))
		{
			Word.LeftChopInline(1);
		}
	}

	bool bAttack = false, bTraverse = false, bWait = false, bLook = false, bEquip = false, bHolster = false, bMove = false;
	int32 Number = 0;
	bool bPairedWeapon = false;
	TArray<FString> Others;

	for (int32 Index = 0; Index < Words.Num(); ++Index)
	{
		const FString& Word = Words[Index];
		if (const int32 N = NumberWord(Word))
		{
			// "two pistols" is a weapon, not a count.
			if (N == 2 && Index + 1 < Words.Num() && WeaponForWord(Words[Index + 1]).StartsWith(TEXT("pistol")))
			{
				bPairedWeapon = true;
				continue;
			}
			Number = N;
			continue;
		}
		if (IsOneOf(Word, { TEXT("dual"), TEXT("double"), TEXT("twin"), TEXT("both") }))
		{
			bPairedWeapon = true;
			continue;
		}
		if (IsOneOf(Word, { TEXT("time"), TEXT("times"), TEXT("second"), TEXT("sec"), TEXT("the"), TEXT("a"), TEXT("an"), TEXT("at"), TEXT("to"),
			TEXT("with"), TEXT("on"), TEXT("over"), TEXT("up"), TEXT("then"), TEXT("and"), TEXT("his"), TEXT("her"), TEXT("their"), TEXT("it"), TEXT("them"),
			TEXT("around"), TEXT("back"), TEXT("forward"), TEXT("away"), TEXT("here"), TEXT("there"), TEXT("toward"), TEXT("towards"), TEXT("into"),
			TEXT("onto"), TEXT("off"), TEXT("down"), TEXT("out"), TEXT("of"), TEXT("him"), TEXT("enemy"), TEXT("target"), TEXT("wall"), TEXT("box"),
			TEXT("ledge"), TEXT("crate"), TEXT("seconds"), TEXT("s") }))
		{
			continue;
		}

		// How the target answers. These words shape the reaction; they never search animation names.
		if (IsOneOf(Word, { TEXT("kill"), TEXT("finish"), TEXT("slay"), TEXT("execute"), TEXT("finisher") }))
		{
			bAttack = true;
			Reading.Reaction = ECinemafyHitReaction::Death;
			continue;
		}
		if (IsOneOf(Word, { TEXT("knock"), TEXT("knockdown"), TEXT("launch"), TEXT("floor"), TEXT("flatten"), TEXT("sweep") }))
		{
			bAttack = true;
			Reading.Reaction = ECinemafyHitReaction::Knockdown;
			continue;
		}
		if (IsOneOf(Word, { TEXT("blocked"), TEXT("parried"), TEXT("deflected"), TEXT("guarded") }))
		{
			Reading.Reaction = ECinemafyHitReaction::Block;
			continue;
		}
		if (IsOneOf(Word, { TEXT("dodged"), TEXT("evaded"), TEXT("missed"), TEXT("whiffed"), TEXT("sidestepped") }))
		{
			Reading.Reaction = ECinemafyHitReaction::Dodge;
			continue;
		}
		if (IsOneOf(Word, { TEXT("feint"), TEXT("fake") }))
		{
			bAttack = true;
			Reading.Reaction = ECinemafyHitReaction::None;
			continue;
		}

		const FString Weapon = WeaponForWord(Word);
		if (!Weapon.IsEmpty())
		{
			Reading.Weapon = Weapon;
			continue;
		}

		if (IsOneOf(Word, { TEXT("walk"), TEXT("stroll"), TEXT("step"), TEXT("amble"), TEXT("approach"), TEXT("go"), TEXT("move"), TEXT("head"), TEXT("slowly"), TEXT("calmly"), TEXT("pace") }))
		{
			Reading.Gait = ECinemafyGait::Walk; Reading.bHasGait = true; bMove = true;
		}
		else if (IsOneOf(Word, { TEXT("jog"), TEXT("run"), TEXT("hurry"), TEXT("rush"), TEXT("quickly"), TEXT("fast"), TEXT("trot") }))
		{
			Reading.Gait = ECinemafyGait::Run; Reading.bHasGait = true; bMove = true;
		}
		else if (IsOneOf(Word, { TEXT("sprint"), TEXT("charge"), TEXT("bolt"), TEXT("race"), TEXT("flee"), TEXT("escape"), TEXT("book") }))
		{
			Reading.Gait = ECinemafyGait::Sprint; Reading.bHasGait = true; bMove = true;
		}
		else if (IsOneOf(Word, { TEXT("sneak"), TEXT("crouch"), TEXT("creep"), TEXT("stalk"), TEXT("tiptoe"), TEXT("prowl") }))
		{
			Reading.Gait = ECinemafyGait::Crouch; Reading.bHasGait = true; bMove = true;
		}
		else if (IsOneOf(Word, { TEXT("vault"), TEXT("hurdle"), TEXT("mantle"), TEXT("climb"), TEXT("scale"), TEXT("clamber"), TEXT("jump"), TEXT("leap"), TEXT("hop"), TEXT("parkour") }))
		{
			bTraverse = true;
			Others.Add(Word);
		}
		else if (IsOneOf(Word, { TEXT("attack"), TEXT("atk"), TEXT("strike"), TEXT("slash"), TEXT("swing"), TEXT("stab"), TEXT("thrust"), TEXT("cleave"),
			TEXT("smash"), TEXT("punch"), TEXT("kick"), TEXT("combo"), TEXT("chop"), TEXT("lunge"), TEXT("kill"), TEXT("finish"), TEXT("fight"),
			TEXT("hit"), TEXT("shoot"), TEXT("fire"), TEXT("blast"), TEXT("snipe"), TEXT("cast"), TEXT("spell") }))
		{
			bAttack = true;
			if (IsOneOf(Word, { TEXT("shoot"), TEXT("fire"), TEXT("blast"), TEXT("snipe"), TEXT("cast"), TEXT("spell") }))
			{
				Reading.bRanged = true;
			}
			if (Word == TEXT("combo"))
			{
				Number = FMath::Max(Number, 3);
			}
			if (!IsOneOf(Word, { TEXT("kill"), TEXT("finish"), TEXT("fight"), TEXT("hit") }))
			{
				Others.Add(Word);
			}
		}
		else if (IsOneOf(Word, { TEXT("wait"), TEXT("pause"), TEXT("hold"), TEXT("idle"), TEXT("stand"), TEXT("stay"), TEXT("beat"), TEXT("breathe"), TEXT("still") }))
		{
			bWait = true;
		}
		else if (IsOneOf(Word, { TEXT("look"), TEXT("face"), TEXT("turn"), TEXT("watch"), TEXT("glance"), TEXT("stare"), TEXT("notice"), TEXT("eye") }))
		{
			bLook = true;
		}
		else if (IsOneOf(Word, { TEXT("draw"), TEXT("equip"), TEXT("unsheathe"), TEXT("unholster"), TEXT("ready"), TEXT("arm"), TEXT("switch"), TEXT("wield") }))
		{
			bEquip = true;
		}
		else if (IsOneOf(Word, { TEXT("holster"), TEXT("sheathe"), TEXT("unequip"), TEXT("stow"), TEXT("disarm") }))
		{
			bEquip = true; bHolster = true;
		}
		else
		{
			Others.Add(Word);
		}
	}

	if (bPairedWeapon && Reading.Weapon == TEXT("pistol"))
	{
		Reading.Weapon = TEXT("pistols");
	}
	else if (bPairedWeapon && Reading.Weapon == TEXT("sword"))
	{
		Reading.Weapon = TEXT("twinblades");
	}

	// Decide the action. Attacks and traversal win over gait words ("sprint vault", "charge attack").
	if (bAttack || (Reading.Reaction != ECinemafyHitReaction::Hit && Others.Num() == 0 && !bTraverse))
	{
		// "blocked" / "dodged" alone still means an attack (that the target answers).
		Reading.Kind = ECinemafyActionKind::Attack;
		Reading.Repeat = FMath::Clamp(Number > 0 ? Number : 1, 1, 6);
		if (Reading.Reaction == ECinemafyHitReaction::Hit && Others.Contains(TEXT("heavy")))
		{
			Reading.Reaction = ECinemafyHitReaction::Knockdown;
		}
	}
	else if (bTraverse)
	{
		Reading.Kind = ECinemafyActionKind::Traverse;
	}
	else if (bEquip || (!Reading.Weapon.IsEmpty() && Others.Num() == 0 && !bMove))
	{
		Reading.Kind = ECinemafyActionKind::Equip;
		if (bHolster)
		{
			Reading.Weapon = TEXT("unarmed");
		}
	}
	else if (Others.Num() > 0)
	{
		Reading.Kind = ECinemafyActionKind::Perform;
		Reading.Repeat = FMath::Clamp(Number > 0 ? Number : 1, 1, 4);
	}
	else if (bLook)
	{
		Reading.Kind = ECinemafyActionKind::Look;
		Reading.Seconds = static_cast<float>(Number);
	}
	else if (bMove)
	{
		Reading.Kind = ECinemafyActionKind::Move;
	}
	else if ((bSay || !Reading.Line.IsEmpty()) && !bWait)
	{
		// Nothing but a line: say it on the spot.
		Reading.Kind = ECinemafyActionKind::Say;
	}
	else
	{
		Reading.Kind = ECinemafyActionKind::Wait;
		Reading.Seconds = static_cast<float>(Number);
	}

	// What to look for in animation names.
	switch (Reading.Kind)
	{
	case ECinemafyActionKind::Attack:
	{
		bool bHasSpecific = false;
		TArray<FString> Modifiers;
		for (const FString& Word : Others)
		{
			// Specific verbs ("kick", "punch", "shoot", "cast") narrow the search; generic ones fold into "attack".
			if (IsOneOf(Word, { TEXT("attack"), TEXT("atk"), TEXT("strike"), TEXT("swing"), TEXT("combo") }))
			{
				continue;
			}
			// Modifiers ("heavy", "light", "spin") prefer matching clips; they never replace the action itself.
			if (IsOneOf(Word, { TEXT("heavy"), TEXT("power"), TEXT("strong"), TEXT("light"), TEXT("quick"), TEXT("spin"), TEXT("spinning"), TEXT("whirl") }))
			{
				Modifiers.Add(Concept(Word));
				continue;
			}
			Reading.Keywords.Add(Concept(Word));
			bHasSpecific = true;
		}
		if (!bHasSpecific)
		{
			Reading.Keywords.Insert(Concept(TEXT("attack")), 0);
		}
		Reading.Keywords.Append(Modifiers);
		break;
	}
	case ECinemafyActionKind::Traverse:
		Reading.Keywords.Add(Concept(Others.Num() > 0 ? Others[0] : TEXT("vault")));
		break;
	case ECinemafyActionKind::Equip:
		Reading.Keywords.Add(Concept(bHolster ? TEXT("holster") : TEXT("draw")));
		break;
	case ECinemafyActionKind::Perform:
		for (const FString& Word : Others)
		{
			Reading.Keywords.Add(Concept(Word));
		}
		break;
	default:
		break;
	}

	// Summary.
	const UEnum* KindEnum = StaticEnum<ECinemafyActionKind>();
	FString Summary = KindEnum->GetDisplayNameTextByValue(static_cast<int64>(Reading.Kind)).ToString();
	if (Reading.Kind == ECinemafyActionKind::Perform && Others.Num() > 0)
	{
		Summary = FString::Printf(TEXT("Perform \"%s\""), *FString::Join(Others, TEXT(" ")));
	}
	if (Reading.bHasGait)
	{
		Summary += FString::Printf(TEXT(" (%s)"), *StaticEnum<ECinemafyGait>()->GetDisplayNameTextByValue(static_cast<int64>(Reading.Gait)).ToString());
	}
	if (!Reading.Weapon.IsEmpty())
	{
		Summary += FString::Printf(TEXT(" with %s"), *Reading.Weapon);
	}
	if (Reading.Repeat > 1)
	{
		Summary += FString::Printf(TEXT(" x%d"), Reading.Repeat);
	}
	if (Reading.Kind == ECinemafyActionKind::Attack && Reading.Reaction != ECinemafyHitReaction::Hit)
	{
		Summary += FString::Printf(TEXT(" -> target %s"), *StaticEnum<ECinemafyHitReaction>()->GetDisplayNameTextByValue(static_cast<int64>(Reading.Reaction)).ToString());
	}
	if (Reading.Seconds > 0.f)
	{
		Summary += FString::Printf(TEXT(" %.1fs"), Reading.Seconds);
	}
	if (Reading.bAfterLine)
	{
		Summary = TEXT("After their line: ") + Summary;
	}
	if (!Reading.Line.IsEmpty())
	{
		const FString Quote = Reading.Line.Len() > 28 ? Reading.Line.Left(26) + TEXT("...") : Reading.Line;
		Summary += Reading.Kind == ECinemafyActionKind::Say ? FString::Printf(TEXT(" \"%s\""), *Quote) : FString::Printf(TEXT(", saying \"%s\""), *Quote);
	}
	Reading.Summary = Summary;
	return Reading;
}

float UCinemafyChoreographyLibrary::ScoreAnimation(const FCinemafyPromptReading& Reading, const TArray<FString>& AnimationTokens)
{
	if (Reading.Keywords.Num() == 0)
	{
		return -1.f;
	}

	// 1 = in the asset's name, 0.5 = only in a folder name ("@word"), 0 = absent.
	auto Match = [&AnimationTokens](const FString& Word) -> float
	{
		if (AnimationTokens.Contains(Word))
		{
			return 1.f;
		}
		return AnimationTokens.Contains(TEXT("@") + Word) ? 0.5f : 0.f;
	};
	auto HasToken = [&Match](const FString& Word) { return Match(Word) > 0.f; };

	float Score = 0.f;
	TArray<FString> Primary;
	float PrimaryWeight = 0.f;
	for (int32 Index = 0; Index < Reading.Keywords.Num(); ++Index)
	{
		TArray<FString> Synonyms;
		Reading.Keywords[Index].ParseIntoArray(Synonyms, TEXT("|"));
		float Best = 0.f;
		for (int32 S = 0; S < Synonyms.Num(); ++S)
		{
			Best = FMath::Max(Best, Match(Synonyms[S]) * (S == 0 ? 2.5f : 2.f));
		}
		if (Index == 0)
		{
			// The first concept is the action itself: it has to be there.
			if (Best <= 0.f)
			{
				return -1.f;
			}
			Primary = Synonyms;
			PrimaryWeight = Best;
		}
		Score += Best;
	}

	// A death is only ever a death: "Deaths_Hit_Chest" isn't a hit reaction someone gets back up from.
	static const TArray<FString> DeathWords = { TEXT("die"), TEXT("death"), TEXT("dead"), TEXT("dying"), TEXT("deaths"), TEXT("dies") };
	const bool bDeathReading = Primary.ContainsByPredicate([](const FString& Word) { return DeathWords.Contains(Word); });
	if (!bDeathReading && DeathWords.ContainsByPredicate([&AnimationTokens](const FString& Word) { return AnimationTokens.Contains(Word); }))
	{
		return -1.f;
	}

	// Only the folder says the action, and the name says a different one (MM_Dash in a Jump folder): not this.
	const bool bPrimaryInName = Primary.ContainsByPredicate([&AnimationTokens](const FString& Word) { return AnimationTokens.Contains(Word); });
	if (!bPrimaryInName)
	{
		for (const TArray<FString>& Group : SynonymGroups())
		{
			if (!IsActionGroup(Group) || Group.ContainsByPredicate([&Primary](const FString& Word) { return Primary.Contains(Word); }))
			{
				continue;
			}
			if (Group.ContainsByPredicate([&AnimationTokens](const FString& Word) { return AnimationTokens.Contains(Word); }))
			{
				return -1.f;
			}
		}
	}
	(void)PrimaryWeight;

	// The weapon: reward the right one, punish the wrong one.
	if (!Reading.Weapon.IsEmpty())
	{
		const TArray<FString>* Mine = WeaponWords().Find(Reading.Weapon);
		float Right = 0.f;
		if (Mine)
		{
			for (int32 S = 0; S < Mine->Num(); ++S)
			{
				Right = FMath::Max(Right, Match((*Mine)[S]) * (S == 0 ? 2.f : 1.5f));
			}
		}
		Score += Right;
		if (Right <= 0.f)
		{
			for (const TPair<FString, TArray<FString>>& Other : WeaponWords())
			{
				if (Other.Key != Reading.Weapon && HasToken(Other.Key))
				{
					Score -= 2.5f;
					break;
				}
			}
		}
	}

	// One-shot actions shouldn't come from locomotion, poses or loops (judged by the asset's own name).
	if (Reading.Kind == ECinemafyActionKind::Attack || Reading.Kind == ECinemafyActionKind::Perform || Reading.Kind == ECinemafyActionKind::Equip)
	{
		for (const FString& Token : AnimationTokens)
		{
			if (!Token.StartsWith(TEXT("@")) && IsLocomotionToken(Token))
			{
				Score -= 1.5f;
			}
		}
	}
	if (Reading.Kind == ECinemafyActionKind::Attack)
	{
		// An attack isn't a hit reaction, a block or a death.
		for (const TCHAR* Word : { TEXT("react"), TEXT("die"), TEXT("death"), TEXT("block"), TEXT("parry"), TEXT("dodge"), TEXT("evade"), TEXT("equip"), TEXT("unequip"), TEXT("idle") })
		{
			if (AnimationTokens.Contains(Word))
			{
				Score -= 2.f;
			}
		}
		if (HasToken(TEXT("root")) || HasToken(TEXT("rootmotion")))
		{
			Score += 0.4f;
		}
	}
	if (HasToken(TEXT("inplace")) || (HasToken(TEXT("in")) && HasToken(TEXT("place"))))
	{
		Score -= 0.4f;
	}
	if (HasToken(TEXT("montage")) || AnimationTokens.Contains(TEXT("am")))
	{
		Score += 0.3f;
	}
	if (HasToken(TEXT("ue4")))
	{
		Score -= 0.2f;
	}
	return Score;
}

TArray<int32> UCinemafyChoreographyLibrary::PickAnimations(const FCinemafyPromptReading& Reading, const TArray<FCinemafyMoveCandidate>& Candidates,
	int32 Count, const TSet<FSoftObjectPath>& RecentlyUsed)
{
	TArray<TPair<float, int32>> Scored;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		float Score = ScoreAnimation(Reading, Candidates[Index].Tokens);
		if (Score <= 0.f)
		{
			continue;
		}
		Score += Candidates[Index].Bias;
		if (RecentlyUsed.Contains(Candidates[Index].Path))
		{
			Score -= 1.f;
		}
		Scored.Add({ Score, Index });
	}
	Scored.Sort([&Candidates](const TPair<float, int32>& A, const TPair<float, int32>& B)
	{
		return A.Key != B.Key ? A.Key > B.Key : Candidates[A.Value].Name < Candidates[B.Value].Name;
	});

	TArray<int32> Picks;
	if (Scored.Num() == 0)
	{
		return Picks;
	}

	const int32 Best = Scored[0].Value;
	Picks.Add(Best);

	if (Count > 1)
	{
		// A numbered family (Combo_01_01, Combo_01_02...) makes a real combo: take the chain from the start.
		FString Number;
		const FString Stem = StripTrailingNumber(Candidates[Best].Name, Number);
		if (!Number.IsEmpty())
		{
			TArray<int32> Family;
			for (const TPair<float, int32>& Entry : Scored)
			{
				FString OtherNumber;
				if (StripTrailingNumber(Candidates[Entry.Value].Name, OtherNumber) == Stem && !OtherNumber.IsEmpty())
				{
					Family.Add(Entry.Value);
				}
			}
			if (Family.Num() > 1)
			{
				Family.Sort([&Candidates](int32 A, int32 B) { return Candidates[A].Name < Candidates[B].Name; });
				Picks.Reset();
				for (int32 Index = 0; Index < Family.Num() && Picks.Num() < Count; ++Index)
				{
					Picks.Add(Family[Index]);
				}
			}
		}

		// Otherwise (or if the family is short) the next best distinct picks.
		for (int32 Index = 1; Index < Scored.Num() && Picks.Num() < Count; ++Index)
		{
			Picks.AddUnique(Scored[Index].Value);
		}
	}
	return Picks;
}

TArray<FCinemafyMoveCandidate> UCinemafyChoreographyLibrary::GatherCandidates(const UCinemafyMoveLibrary* Library, const TArray<FString>& Folders, const USkeleton* Skeleton)
{
	TArray<FCinemafyMoveCandidate> Candidates;

	// No library of its own: use the project's default one (Project Settings > Plugins > Cinemafy), if set.
	if (!Library)
	{
		Library = UCinemafySettings::GetDefaultMoveLibrary();
	}

	if (Library && Library->Moves.Num() > 0)
	{
		for (const FCinemafyMoveEntry& Entry : Library->Moves)
		{
			if (!Entry.Animation)
			{
				continue;
			}
			FCinemafyMoveCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Path = FSoftObjectPath(Entry.Animation);
			Candidate.Name = Entry.Animation->GetName();
			Candidate.Tokens = TokenizeName(Entry.Animation->GetPathName());
			for (const FString& Keyword : Entry.Keywords)
			{
				Candidate.Tokens.AddUnique(Keyword.ToLower());
			}
			Candidate.Bias = Entry.Bias;
		}
		return Candidates;
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	for (const FString& Folder : Folders)
	{
		if (!Folder.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*Folder));
		}
	}
	if (Filter.PackagePaths.Num() == 0)
	{
		Filter.PackagePaths.Add(TEXT("/Game"));
	}

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	const FString SkeletonPath = Skeleton ? FSoftObjectPath(Skeleton).ToString() : FString();
	for (const FAssetData& Asset : Assets)
	{
		if (Skeleton)
		{
			FString AssetSkeleton;
			if (!Asset.GetTagValue(TEXT("Skeleton"), AssetSkeleton))
			{
				continue;
			}
			const bool bSame = AssetSkeleton.Contains(SkeletonPath);
#if WITH_EDITOR
			if (!bSame && !Skeleton->IsCompatibleForEditor(AssetSkeleton))
			{
				continue;
			}
#else
			if (!bSame)
			{
				continue;
			}
#endif
		}

		FCinemafyMoveCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Path = Asset.GetSoftObjectPath();
		Candidate.Name = Asset.AssetName.ToString();
		Candidate.Tokens = TokenizeName(Asset.PackageName.ToString());
		Candidate.Bias = UCinemafySettings::GetFolderBias(Asset.PackageName.ToString());
		if (Asset.AssetClassPath == UAnimMontage::StaticClass()->GetClassPathName())
		{
			Candidate.Tokens.AddUnique(TEXT("montage"));
		}
	}
	return Candidates;
}

float UCinemafyChoreographyLibrary::ScoreOverlayName(const FString& Weapon, const FString& EnumValueName)
{
	const TArray<FString>* Words = WeaponWords().Find(Weapon);
	if (!Words)
	{
		return 0.f;
	}

	// "E_OverlayState::PistolTwoHanded" -> "PistolTwoHanded".
	FString Short = EnumValueName;
	int32 Colon = INDEX_NONE;
	if (Short.FindLastChar(TEXT(':'), Colon))
	{
		Short.RightChopInline(Colon + 1);
	}
	const FString Whole = Short.ToLower();
	const TArray<FString> Tokens = TokenizeName(Short);

	float Best = 0.f;
	for (int32 Index = 0; Index < Words->Num(); ++Index)
	{
		const FString& Word = (*Words)[Index];
		float Score = 0.f;
		if (Whole == Word)
		{
			Score = 10.f;
		}
		else if (Tokens.Contains(Word))
		{
			Score = 6.f;
		}
		else if (Word.Len() > 3 && Whole.Contains(Word))
		{
			Score = 4.f;
		}
		Best = FMath::Max(Best, Score - Index * 0.5f);
	}
	return Best;
}

FString UCinemafyChoreographyLibrary::ReactionDirection(float RelativeYaw)
{
	const float Yaw = FRotator::NormalizeAxis(RelativeYaw);
	if (FMath::Abs(Yaw) <= 45.f)
	{
		return TEXT("front");
	}
	if (FMath::Abs(Yaw) >= 135.f)
	{
		return TEXT("back");
	}
	return Yaw > 0.f ? TEXT("right") : TEXT("left");
}

FCinemafyPromptReading UCinemafyChoreographyLibrary::MakeReactionReading(ECinemafyHitReaction Reaction, float RelativeYaw)
{
	FCinemafyPromptReading Reading;
	Reading.Kind = ECinemafyActionKind::Perform;
	switch (Reaction)
	{
	case ECinemafyHitReaction::Knockdown:
		Reading.Keywords.Add(TEXT("knockdown|knock|knocked|fall|falldown|down|launch|launched"));
		break;
	case ECinemafyHitReaction::Death:
		Reading.Keywords.Add(Concept(TEXT("die")));
		break;
	case ECinemafyHitReaction::Block:
		Reading.Keywords.Add(Concept(TEXT("block")));
		break;
	case ECinemafyHitReaction::Dodge:
		Reading.Keywords.Add(Concept(TEXT("dodge")));
		break;
	default:
		Reading.Keywords.Add(Concept(TEXT("hit")));
		break;
	}

	// Where the blow comes from is a preference, not a requirement (it's scored, never required).
	const FString Side = ReactionDirection(RelativeYaw);
	static const TMap<FString, FString> Sides = {
		{ TEXT("front"), TEXT("front|forward|fwd|chest|gut|guts") }, { TEXT("back"), TEXT("back|backward|behind|bwd|rear") },
		{ TEXT("left"), TEXT("left|lft") }, { TEXT("right"), TEXT("right|rgt") } };
	if (Reaction != ECinemafyHitReaction::Block)
	{
		Reading.Keywords.Add(Sides.FindRef(Side));
	}
	Reading.Summary = FString::Printf(TEXT("%s from the %s"),
		*StaticEnum<ECinemafyHitReaction>()->GetDisplayNameTextByValue(static_cast<int64>(Reaction)).ToString(), *Side);
	return Reading;
}

ECinemafyArrival UCinemafyChoreographyLibrary::ResolveArrival(const TArray<FCinemafyChoreoBeat>& Beats, int32 BeatIndex)
{
	if (!Beats.IsValidIndex(BeatIndex))
	{
		return ECinemafyArrival::StopAndFace;
	}
	const FCinemafyChoreoBeat& Beat = Beats[BeatIndex];
	if (Beat.Arrival != ECinemafyArrival::Auto)
	{
		return Beat.Arrival;
	}
	if (Beat.Reading.Kind == ECinemafyActionKind::Traverse)
	{
		return ECinemafyArrival::EnterAction;
	}
	if (Beat.Reading.Kind != ECinemafyActionKind::Move || !Beats.IsValidIndex(BeatIndex + 1))
	{
		return ECinemafyArrival::StopAndFace;
	}
	// Keep momentum into more travel, a vault, or a charge at someone; stop before gestures, looks and waits.
	const FCinemafyChoreoBeat& Next = Beats[BeatIndex + 1];
	switch (Next.Reading.Kind)
	{
	case ECinemafyActionKind::Move:
	case ECinemafyActionKind::Traverse:
		return ECinemafyArrival::PassThrough;
	case ECinemafyActionKind::Attack:
		return Next.Reading.bRanged ? ECinemafyArrival::StopAndFace : ECinemafyArrival::PassThrough;
	default:
		return ECinemafyArrival::StopAndFace;
	}
}

void UCinemafyChoreographyLibrary::JudgeTraversal(FCinemafyTraversalCheck& Check, const FCinemafyTraversalEnvelope& Envelope, bool bLandingClear, float CapsuleRadius)
{
	auto Set = [&Check](ECinemafyTraversalVerdict Verdict, bool bPossible, const FString& Reason)
	{
		Check.Verdict = Verdict;
		Check.bPossible = bPossible;
		Check.Reason = Reason;
	};
	const bool bPlatform = Check.Depth >= 390.f;
	const FString Size = bPlatform ? FString::Printf(TEXT("%.0f cm high (a platform)"), Check.Height)
		: FString::Printf(TEXT("%.0f cm high, %.0f cm deep"), Check.Height, Check.Depth);

	if (Check.Height < Envelope.MinHeight)
	{
		Set(ECinemafyTraversalVerdict::StepOver, true, FString::Printf(TEXT("Only %.0f cm: walks over it, no vault needed"), Check.Height));
		return;
	}
	if (!Envelope.bCanTraverse)
	{
		if (Check.Height <= Envelope.MaxJumpHeight)
		{
			Set(ECinemafyTraversalVerdict::Jump, true, FString::Printf(TEXT("%s: jumps it (no traversal system)"), *Size));
		}
		else
		{
			Set(ECinemafyTraversalVerdict::CannotTraverse, false, FString::Printf(TEXT("%s: too tall to jump (%.0f cm max) and the performer has no traversal system"),
				*Size, Envelope.MaxJumpHeight));
		}
		return;
	}
	if (Check.ApproachAngle > Envelope.MaxApproachAngle)
	{
		Set(ECinemafyTraversalVerdict::BadAngle, false, FString::Printf(TEXT("Approaching at %.0f degrees to the face (max %.0f): put the mark before it straighter on"),
			Check.ApproachAngle, Envelope.MaxApproachAngle));
		return;
	}
	if (Check.Height > Envelope.MaxMantleHeight)
	{
		Set(ECinemafyTraversalVerdict::TooTall, false, FString::Printf(TEXT("%s: too tall (climbs up to %.0f cm)"), *Size, Envelope.MaxMantleHeight));
		return;
	}

	const bool bVaultSized = Check.Height <= Envelope.MaxVaultHeight && Check.Depth <= Envelope.MaxVaultDepth;
	const bool bCanStandOnTop = Check.Depth >= CapsuleRadius * 2.f && Check.Headroom >= Envelope.MinHeadroom;
	if (bVaultSized && bLandingClear && Check.Headroom >= Envelope.MinHeadroom * 0.5f)
	{
		Set(ECinemafyTraversalVerdict::Vault, true, FString::Printf(TEXT("%s: vaults over it"), *Size));
		return;
	}
	if (bCanStandOnTop)
	{
		Set(ECinemafyTraversalVerdict::Mantle, true, bVaultSized
			? FString::Printf(TEXT("%s: no room to land beyond, so climbs onto it instead"), *Size)
			: FString::Printf(TEXT("%s: climbs onto it"), *Size));
		return;
	}
	if (bVaultSized)
	{
		Set(ECinemafyTraversalVerdict::NoRoomToLand, false, FString::Printf(TEXT("%s: no room to land on the far side (needs %.0f cm clear)"), *Size, Envelope.MinLandingDepth));
		return;
	}
	if (Check.Headroom < Envelope.MinHeadroom)
	{
		Set(ECinemafyTraversalVerdict::NoHeadroom, false, FString::Printf(TEXT("%s: only %.0f cm of headroom above it (needs %.0f)"), *Size, Check.Headroom, Envelope.MinHeadroom));
		return;
	}
	Set(ECinemafyTraversalVerdict::TooNarrow, false, FString::Printf(TEXT("%s: too deep to vault (%.0f cm max) and too narrow to stand on"), *Size, Envelope.MaxVaultDepth));
}

void UCinemafyChoreographyLibrary::JudgeGap(FCinemafyTraversalCheck& Check, const FCinemafyTraversalEnvelope& Envelope)
{
	auto Set = [&Check](ECinemafyTraversalVerdict Verdict, bool bPossible, const FString& Reason)
	{
		Check.Verdict = Verdict;
		Check.bPossible = bPossible;
		Check.Reason = Reason;
	};
	const bool bRecovery = Check.DropHeight > Envelope.RecoveryDropHeight;
	const FString Landing = bRecovery ? TEXT(", lands into a roll") : FString();

	if (Check.GapWidth > 0.f)
	{
		const float Climb = -Check.DropHeight;
		const float MaxUp = Envelope.bCanTraverse ? 60.f : Envelope.MaxJumpHeight;
		if (Check.GapWidth > Envelope.MaxGapWidth)
		{
			Set(ECinemafyTraversalVerdict::TooFar, false, FString::Printf(TEXT("A %.0f cm gap: too wide to jump (%.0f cm max)"), Check.GapWidth, Envelope.MaxGapWidth));
		}
		else if (Climb > MaxUp)
		{
			Set(ECinemafyTraversalVerdict::TooTall, false, FString::Printf(TEXT("A %.0f cm gap, but the far side is %.0f cm higher (jumps up to %.0f)"), Check.GapWidth, Climb, MaxUp));
		}
		else if (Check.DropHeight > Envelope.MaxDropHeight)
		{
			Set(ECinemafyTraversalVerdict::TooHighToDrop, false, FString::Printf(TEXT("A %.0f cm gap down to %.0f cm below: too far to fall (%.0f cm max)"),
				Check.GapWidth, Check.DropHeight, Envelope.MaxDropHeight));
		}
		else
		{
			Set(ECinemafyTraversalVerdict::Gap, true, FString::Printf(TEXT("A %.0f cm gap: jumps it from the edge%s"), Check.GapWidth, *Landing));
		}
		return;
	}

	if (Check.DropHeight > Envelope.MaxDropHeight)
	{
		Set(ECinemafyTraversalVerdict::TooHighToDrop, false, FString::Printf(TEXT("A %.0f cm drop: too high (%.0f cm max)"), Check.DropHeight, Envelope.MaxDropHeight));
		return;
	}
	Set(ECinemafyTraversalVerdict::Drop, true, FString::Printf(TEXT("A %.0f cm drop: goes off the ledge%s"), Check.DropHeight, *Landing));
}

void UCinemafyChoreographyLibrary::PlanTraversalRoute(FCinemafyTraversalCheck& Check, const FVector& From, const FCinemafyTraversalEnvelope& Envelope)
{
	if (Check.TakeoffPoint.IsNearlyZero())
	{
		return;
	}
	const FVector Takeoff = Check.TakeoffPoint;
	const FVector Cross = (Takeoff - From).GetSafeNormal2D();

	// Speed matters for vaults and gaps; a mantle or a step down only needs to arrive square.
	float RunUp = 100.f;
	switch (Check.Verdict)
	{
	case ECinemafyTraversalVerdict::Vault:
	case ECinemafyTraversalVerdict::Gap:  RunUp = Envelope.VaultRunUp; break;
	case ECinemafyTraversalVerdict::Jump: RunUp = 150.f; break;
	case ECinemafyTraversalVerdict::Drop: RunUp = 60.f; break;
	default: break;
	}
	// Never plan a run-up behind where the performer starts.
	RunUp = FMath::Min(RunUp, FVector::Dist2D(From, Takeoff));
	Check.ApproachPoint = Takeoff - Cross * RunUp;
	Check.ApproachPoint.Z = From.Z;

	const FVector OffTheFar = Check.FarPoint + Cross * 60.f;
	switch (Check.Verdict)
	{
	case ECinemafyTraversalVerdict::Mantle:
		Check.ExitPoint = Check.FrontPoint + Cross * FMath::Min(Check.Depth * 0.5f, 60.f);
		Check.ExitPoint.Z = Check.FarPoint.Z;
		break;
	case ECinemafyTraversalVerdict::StepOver:
		Check.ExitPoint = Check.FarPoint + Cross * 40.f;
		break;
	default:
		Check.ExitPoint = !Check.LandingPoint.IsNearlyZero() ? Check.LandingPoint : OffTheFar;
		break;
	}
}

namespace CinemafyTraversalProbe
{
	/** Walks the floor toward the mark: the first edge where it drops away, and where (if anywhere) it comes back up. */
	bool FindEdge(const UWorld* World, const FVector& From, const FVector& Direction, float Reach, float FloorZ, float MinHeight,
		const FCollisionQueryParams& Params, FVector& OutEdge, FVector& OutBelow, FVector& OutFarSide, bool& bOutFarSide)
	{
		constexpr float Step = 15.f;
		constexpr float Abyss = 2000.f;
		FHitResult Hit;
		bool bEdge = false;
		bOutFarSide = false;
		// An edge is a sudden step down between neighbouring samples (a ramp going down is not an edge).
		float LastZ = FloorZ;
		float EdgeZ = FloorZ;
		for (float Along = 20.f; Along <= Reach; Along += Step)
		{
			const FVector Probe = From + Direction * Along;
			const bool bFloor = World->LineTraceSingleByChannel(Hit, FVector(Probe.X, Probe.Y, LastZ + 50.f), FVector(Probe.X, Probe.Y, LastZ - Abyss), ECC_Visibility, Params);
			const FVector Ground = bFloor ? Hit.ImpactPoint : FVector(Probe.X, Probe.Y, LastZ - Abyss);
			if (!bEdge)
			{
				if (LastZ - Ground.Z > MinHeight)
				{
					bEdge = true;
					EdgeZ = LastZ;
					OutEdge = From + Direction * (Along - Step);
					OutEdge.Z = LastZ;
					OutBelow = Ground;
				}
				else
				{
					LastZ = Ground.Z;
				}
			}
			else if (EdgeZ - Ground.Z <= MinHeight)
			{
				// The floor comes back near our level: the far side of a gap.
				bOutFarSide = true;
				OutFarSide = Ground;
				return true;
			}
		}
		return bEdge;
	}

	bool IsRouteClear(const UWorld* World, const FVector& A, const FVector& B, float Radius, float HalfHeight, const FCollisionQueryParams& Params)
	{
		const FVector Lift(0.f, 0.f, HalfHeight + 5.f);
		return !World->SweepTestByChannel(A + Lift, B + Lift, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.9f), Params);
	}
}

FCinemafyTraversalCheck UCinemafyChoreographyLibrary::CheckTraversal(const UObject* WorldContextObject, FVector From, FVector Toward,
	const FCinemafyTraversalEnvelope& Envelope, float CapsuleRadius, float CapsuleHalfHeight, const TArray<AActor*>& IgnoreActors)
{
	return CheckTraversalImpl(WorldContextObject, From, Toward, Envelope, CapsuleRadius, CapsuleHalfHeight, IgnoreActors, true);
}

FCinemafyTraversalCheck UCinemafyChoreographyLibrary::CheckTraversalImpl(const UObject* WorldContextObject, FVector From, FVector Toward,
	const FCinemafyTraversalEnvelope& Envelope, float CapsuleRadius, float CapsuleHalfHeight, const TArray<AActor*>& IgnoreActors, bool bAllowLineUp)
{
	FCinemafyTraversalCheck Check;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	FVector Direction = (Toward - From) * FVector(1.f, 1.f, 0.f);
	if (!World || Direction.SizeSquared() < 1.f)
	{
		Check.Verdict = ECinemafyTraversalVerdict::NoObstacle;
		Check.Reason = TEXT("No approach to measure from");
		return Check;
	}
	Direction.Normalize();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyTraversalCheck), false);
	Params.AddIgnoredActors(IgnoreActors);
	FHitResult Hit;

	// The floor the performer approaches on.
	float FloorZ = From.Z;
	if (World->LineTraceSingleByChannel(Hit, From + FVector(0.f, 0.f, 100.f), From - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		FloorZ = Hit.ImpactPoint.Z;
	}
	const FVector Feet(From.X, From.Y, FloorZ);

	// The front face: the lowest probe height that hits something on the way to (and a little past) the mark.
	const float Reach = FVector::Dist2D(From, Toward) + 150.f;
	bool bFoundFront = false;
	FVector FrontNormal = -Direction;
	for (const float ProbeHeight : { 20.f, 45.f, 80.f, 120.f, 170.f, 230.f })
	{
		const FVector Start(From.X, From.Y, FloorZ + ProbeHeight);
		if (World->LineTraceSingleByChannel(Hit, Start, Start + Direction * Reach, ECC_Visibility, Params) && !Hit.bStartPenetrating)
		{
			Check.FrontPoint = Hit.ImpactPoint;
			FrontNormal = Hit.ImpactNormal.GetSafeNormal2D();
			bFoundFront = true;
			break;
		}
	}

	// No face, but maybe the floor ends: a gap to jump or a ledge to go off.
	FVector Edge, Below, FarSide;
	bool bFarSide = false;
	const bool bEdge = CinemafyTraversalProbe::FindEdge(World, Feet, Direction, Reach, FloorZ, Envelope.MinHeight, Params, Edge, Below, FarSide, bFarSide);
	const bool bEdgeFirst = bEdge && (!bFoundFront || FVector::Dist2D(Feet, Edge) < FVector::Dist2D(Feet, Check.FrontPoint));
	if (bEdgeFirst)
	{
		Check = FCinemafyTraversalCheck();
		Check.FaceNormal = -Direction;
		Check.TakeoffPoint = Edge;
		Check.FrontPoint = Edge;
		if (bFarSide)
		{
			Check.GapWidth = FVector::Dist2D(Edge, FarSide);
			Check.FarPoint = FarSide;
			Check.LandingPoint = FarSide + Direction * 60.f;
			if (World->LineTraceSingleByChannel(Hit, Check.LandingPoint + FVector(0.f, 0.f, 100.f), Check.LandingPoint - FVector(0.f, 0.f, 200.f), ECC_Visibility, Params))
			{
				Check.LandingPoint = Hit.ImpactPoint;
			}
			Check.DropHeight = FloorZ - Check.LandingPoint.Z;
		}
		else
		{
			Check.FarPoint = Edge;
			Check.LandingPoint = Below + Direction * 40.f;
			Check.DropHeight = FloorZ - Below.Z;
		}
		JudgeGap(Check, Envelope);
		PlanTraversalRoute(Check, Feet, Envelope);
		return Check;
	}

	if (!bFoundFront)
	{
		Check.Verdict = ECinemafyTraversalVerdict::NoObstacle;
		Check.Reason = TEXT("Nothing to climb between the last mark and this one (click on the obstacle itself)");
		return Check;
	}
	Check.FaceNormal = FrontNormal;
	Check.ApproachAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(-Direction, FrontNormal), -1.f, 1.f)));

	// Coming in at an angle: square up to the face first, if there's room for a run-up in front of it.
	if (bAllowLineUp && Check.ApproachAngle > Envelope.MaxApproachAngle * 0.5f)
	{
		FVector LineUp = Check.FrontPoint + FrontNormal * FMath::Max(Envelope.VaultRunUp, CapsuleRadius * 3.f);
		if (World->LineTraceSingleByChannel(Hit, FVector(LineUp.X, LineUp.Y, FloorZ + 100.f), FVector(LineUp.X, LineUp.Y, FloorZ - 150.f), ECC_Visibility, Params))
		{
			LineUp = Hit.ImpactPoint;
			const FVector FaceFoot(Check.FrontPoint.X, Check.FrontPoint.Y, LineUp.Z);
			const bool bReachable = CinemafyTraversalProbe::IsRouteClear(World, Feet, LineUp, CapsuleRadius, CapsuleHalfHeight, Params)
				&& CinemafyTraversalProbe::IsRouteClear(World, LineUp, FaceFoot + FrontNormal * (CapsuleRadius + 10.f), CapsuleRadius, CapsuleHalfHeight, Params);
			if (bReachable)
			{
				FCinemafyTraversalCheck Squared = CheckTraversalImpl(WorldContextObject, LineUp, Check.FrontPoint - FrontNormal * 60.f,
					Envelope, CapsuleRadius, CapsuleHalfHeight, IgnoreActors, false);
				if (Squared.Verdict != ECinemafyTraversalVerdict::NoObstacle)
				{
					Squared.bLinedUp = true;
					Squared.ApproachPoint = LineUp;
					Squared.Reason += FString::Printf(TEXT(" (squares up to it first: the approach was %.0f degrees off)"), Check.ApproachAngle);
					return Squared;
				}
			}
		}
	}

	// Its top: drop down onto it from above anything climbable.
	const float CeilingProbe = FloorZ + FMath::Max(Envelope.MaxMantleHeight, Envelope.MaxJumpHeight) + 150.f;
	const FVector InsideFront = Check.FrontPoint + Direction * 4.f;
	float TopZ = FloorZ;
	const bool bTopHit = World->LineTraceSingleByChannel(Hit, FVector(InsideFront.X, InsideFront.Y, CeilingProbe), FVector(InsideFront.X, InsideFront.Y, FloorZ - 5.f), ECC_Visibility, Params);
	// A line that starts inside a tall wall may not report the overlap: it just finds nothing, or the floor beyond.
	// The top can't be lower than where the face was hit, so either case means the wall is taller than the probe.
	if (!bTopHit || Hit.bStartPenetrating || Hit.ImpactPoint.Z < Check.FrontPoint.Z - 5.f)
	{
		// Solid all the way up past anything climbable: a wall (or something too thin to find a top on).
		Check.Height = CeilingProbe - FloorZ;
		Check.Verdict = ECinemafyTraversalVerdict::TooTall;
		Check.Reason = FString::Printf(TEXT("No top found below %.0f cm: a wall, or too thin to climb"), Check.Height);
		return Check;
	}
	TopZ = Hit.ImpactPoint.Z;
	Check.Height = TopZ - FloorZ;

	// Its depth: walk across the top until it drops away.
	Check.Depth = 400.f;
	for (float Along = 10.f; Along <= 400.f; Along += 10.f)
	{
		const FVector Probe = Check.FrontPoint + Direction * Along;
		const bool bStillTop = World->LineTraceSingleByChannel(Hit, FVector(Probe.X, Probe.Y, TopZ + 25.f), FVector(Probe.X, Probe.Y, TopZ - 25.f), ECC_Visibility, Params);
		if (!bStillTop)
		{
			Check.Depth = Along;
			break;
		}
	}
	Check.FarPoint = Check.FrontPoint + Direction * Check.Depth;
	Check.FarPoint.Z = TopZ;

	// Headroom above the top.
	const FVector OnTop = Check.FrontPoint + Direction * FMath::Min(Check.Depth * 0.5f, 30.f);
	const float HeadroomProbe = 250.f;
	Check.Headroom = HeadroomProbe;
	if (World->LineTraceSingleByChannel(Hit, FVector(OnTop.X, OnTop.Y, TopZ + 5.f), FVector(OnTop.X, OnTop.Y, TopZ + HeadroomProbe), ECC_Visibility, Params))
	{
		Check.Headroom = Hit.Distance + 5.f;
	}

	// The landing beyond: floor to stand on, and a capsule's worth of clear space there.
	bool bLandingClear = false;
	if (Check.Depth < 390.f)
	{
		const FVector Beyond = Check.FarPoint + Direction * (CapsuleRadius + Envelope.MinLandingDepth * 0.5f);
		if (World->LineTraceSingleByChannel(Hit, FVector(Beyond.X, Beyond.Y, TopZ + 50.f), FVector(Beyond.X, Beyond.Y, FloorZ - 200.f), ECC_Visibility, Params))
		{
			Check.LandingPoint = Hit.ImpactPoint;
			Check.DropHeight = FloorZ - Hit.ImpactPoint.Z;
			const FVector Standing = Hit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight + 3.f);
			bLandingClear = !World->OverlapAnyTestByChannel(Standing, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), Params);
		}
	}

	JudgeTraversal(Check, Envelope, bLandingClear, CapsuleRadius);
	Check.TakeoffPoint = FVector(Check.FrontPoint.X, Check.FrontPoint.Y, FloorZ);
	PlanTraversalRoute(Check, Feet, Envelope);
	return Check;
}

bool UCinemafyChoreographyLibrary::IsRangedWeapon(const FString& Weapon)
{
	return Weapon.StartsWith(TEXT("pistol")) || Weapon == TEXT("rifle") || Weapon == TEXT("bow") || Weapon == TEXT("wand");
}

void UCinemafyChoreographyLibrary::PlanTransitions(TArray<FCinemafyChoreoBeat>& Beats, const FString& StartingWeapon, const FCinemafyTraversalEnvelope& Envelope)
{
	auto Hand = [](const FString& Weapon) { return Weapon == TEXT("unarmed") ? FString() : Weapon; };
	FString InHand = Hand(StartingWeapon);
	ECinemafyGait Gait = ECinemafyGait::Walk;

	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		FCinemafyChoreoBeat& Beat = Beats[Index];
		const FCinemafyPromptReading& Reading = Beat.Reading;
		FCinemafyBeatTransitions Plan;
		TArray<FString> Notes;
		const ECinemafyGait Moving = Reading.bHasGait ? Reading.Gait : Gait;

		// Weapons: an attack that needs something not in hand draws it first.
		if (Reading.Kind == ECinemafyActionKind::Equip)
		{
			const bool bHolster = Reading.Keywords.ContainsByPredicate([](const FString& Word) { return Word.StartsWith(TEXT("holster")); });
			InHand = bHolster ? FString() : (Reading.Weapon.IsEmpty() ? InHand : Hand(Reading.Weapon));
		}
		else if (Reading.Kind == ECinemafyActionKind::Attack)
		{
			FString Needed = Hand(Reading.Weapon);
			if (Needed.IsEmpty() && Reading.bRanged && !IsRangedWeapon(InHand))
			{
				// "shoot" with nothing that shoots in hand: a sidearm.
				Needed = TEXT("pistol");
			}
			if (!Needed.IsEmpty() && Needed != InHand && Beat.bAutoTransitions)
			{
				Plan.DrawWeapon = Needed;
				Notes.Add(FString::Printf(TEXT("draws the %s first"), *Needed));
			}
			if (!Needed.IsEmpty())
			{
				InHand = Needed;
			}
		}
		else if (!Reading.Weapon.IsEmpty())
		{
			InHand = Hand(Reading.Weapon);
		}

		// Out of a full sprint into a jump, a climb or a strike: a gather step first.
		const FCinemafyTraversalCheck& Check = Beat.TraversalCheck;
		const bool bJumpWord = Reading.Keywords.ContainsByPredicate([](const FString& Word) { return Word.StartsWith(TEXT("jump")); });
		const bool bJumpLike = Reading.Kind == ECinemafyActionKind::Traverse
			&& (bJumpWord || Check.Verdict == ECinemafyTraversalVerdict::Jump || Check.Verdict == ECinemafyTraversalVerdict::Gap
				|| Check.Verdict == ECinemafyTraversalVerdict::Mantle);
		const bool bMeleeCharge = Reading.Kind == ECinemafyActionKind::Attack && !Reading.bRanged && Cast<APawn>(Beat.Target.Get());
		if (Beat.bAutoTransitions && Moving == ECinemafyGait::Sprint && (bJumpLike || bMeleeCharge))
		{
			Plan.bBrakeIn = true;
			Plan.BrakeDistance = Check.Verdict == ECinemafyTraversalVerdict::Mantle ? 320.f : bMeleeCharge ? 300.f : 240.f;
			Notes.Add(TEXT("brakes out of the sprint"));
			Gait = ECinemafyGait::Run;
		}
		else
		{
			Gait = Moving;
		}

		// Big landings end in a roll (moving on) or a recovery (stopping).
		if (Reading.Kind == ECinemafyActionKind::Traverse && Check.bPossible && Beat.bAutoTransitions)
		{
			const bool bBigDrop = Check.DropHeight > Envelope.RecoveryDropHeight;
			const bool bLongJump = Check.Verdict == ECinemafyTraversalVerdict::Gap && Check.GapWidth > Envelope.MaxGapWidth * 0.7f;
			if (bBigDrop || bLongJump)
			{
				bool bMovesOn = false;
				for (int32 Next = Index + 1; Next < Beats.Num(); ++Next)
				{
					const ECinemafyActionKind NextKind = Beats[Next].Reading.Kind;
					if (NextKind == ECinemafyActionKind::Look || NextKind == ECinemafyActionKind::Say)
					{
						continue;
					}
					bMovesOn = NextKind == ECinemafyActionKind::Move || NextKind == ECinemafyActionKind::Traverse
						|| (NextKind == ECinemafyActionKind::Attack && !Beats[Next].Reading.bRanged);
					break;
				}
				Plan.Recovery = bMovesOn ? ECinemafyRecovery::Roll : ECinemafyRecovery::Recover;
				Notes.Add(bMovesOn ? TEXT("rolls out of the landing") : TEXT("lands hard and recovers"));
			}
		}

		Plan.Summary = FString::Join(Notes, TEXT(", "));
		Beat.Transitions = Plan;
	}
}

TArray<FCinemafyLine> UCinemafyChoreographyLibrary::BuildDirectorLines(const TArray<FCinemafyChoreoBeat>& Beats, AActor* Performer, float TakeDuration)
{
	// Each beat is a piece of the scene; a spoken line is its own piece, starting on its first word
	// (an answer can start well after its beat did, once the other character stops talking).
	struct FPiece
	{
		float Start;
		const FCinemafyChoreoBeat* Beat;
		bool bLine;
	};
	TArray<FPiece> Pieces;
	for (const FCinemafyChoreoBeat& Beat : Beats)
	{
		if (Beat.StartTime < 0.f)
		{
			continue;
		}
		const bool bSpoke = Beat.LineStartTime >= 0.f;
		if (bSpoke && Beat.LineStartTime - Beat.StartTime > 0.5f)
		{
			Pieces.Add({ Beat.StartTime, &Beat, false });
			Pieces.Add({ Beat.LineStartTime, &Beat, true });
		}
		else
		{
			Pieces.Add({ bSpoke ? FMath::Min(Beat.StartTime, Beat.LineStartTime) : Beat.StartTime, &Beat, bSpoke });
		}
	}
	Pieces.StableSort([](const FPiece& A, const FPiece& B) { return A.Start < B.Start; });
	// The scene starts at 0; a line can't (its audio would land early), so a quiet moment leads into it.
	if (Pieces.Num() > 0 && Pieces[0].bLine && Pieces[0].Start > 0.05f)
	{
		Pieces.Insert(FPiece{ 0.f, Pieces[0].Beat, false }, 0);
	}

	// The last listener a line went to: who a later line without one is said to.
	AActor* LastListener = nullptr;

	TArray<FCinemafyLine> Lines;
	TArray<float> LineStarts;
	float SpeechEnd = -1.f;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		const FPiece& Piece = Pieces[Index];
		const FCinemafyChoreoBeat& Beat = *Piece.Beat;
		const float Start = Index == 0 ? 0.f : Piece.Start;
		const float End = Index + 1 < Pieces.Num() ? Pieces[Index + 1].Start : FMath::Max(TakeDuration, FMath::Max(Beat.EndTime, Beat.LineStartTime + Beat.LineDuration));
		const float Duration = End - Start;

		const ECinemafyActionKind Kind = Beat.Reading.Kind;
		const bool bAction = !Piece.bLine && (Kind == ECinemafyActionKind::Attack || Kind == ECinemafyActionKind::Traverse);
		const bool bMoving = Kind == ECinemafyActionKind::Move || Kind == ECinemafyActionKind::Traverse
			|| (Kind == ECinemafyActionKind::Attack && !Beat.Reading.bRanged);

		// Very short beats ride along with the one before (a cut every half second is noise), and so does anything
		// that isn't a strike while someone is still talking: stay on the line, cut on the action.
		const bool bUnderLine = !Piece.bLine && Start < SpeechEnd - 0.3f && Kind != ECinemafyActionKind::Attack;
		if (Lines.Num() > 0 && !Piece.bLine && (Duration < 0.8f || bUnderLine))
		{
			FCinemafyLine& Previous = Lines.Last();
			for (float Impact : Beat.ImpactTimes)
			{
				Previous.ImpactTimes.Add(Impact - LineStarts.Last());
			}
			Previous.Duration += Duration;
			Previous.bAction |= bAction;
			Previous.bMoving |= bMoving;
			const ECinemafyHitReaction RideAnswer = Beat.bOverrideReaction ? Beat.Reaction : Beat.Reading.Reaction;
			Previous.bLethal |= Kind == ECinemafyActionKind::Attack && Beat.bTargetReacts && Cast<APawn>(Beat.Target.Get()) && RideAnswer == ECinemafyHitReaction::Death && Beat.ImpactTimes.Num() > 0;
			Previous.Note += TEXT(", ") + Beat.Prompt;
			continue;
		}

		LineStarts.Add(Start);
		FCinemafyLine& Line = Lines.AddDefaulted_GetRef();
		Line.Speaker = Performer;
		APawn* TargetPawn = Cast<APawn>(Beat.Target.Get());
		AActor* Listener = Beat.Listener ? Beat.Listener.Get() : (TargetPawn && TargetPawn != Performer ? TargetPawn : nullptr);
		if (Piece.bLine && !Listener)
		{
			Listener = LastListener;
		}
		Line.Listener = Listener != Performer ? Listener : nullptr;
		Line.Duration = FMath::Max(Duration, 0.2f);
		Line.PauseAfter = 0.f;
		Line.bUseAudioDuration = false;
		Line.bAutoIntensity = false;
		Line.bAction = bAction;
		Line.bMoving = bMoving;
		Line.Note = FString::Printf(TEXT("%s (%s)"), *Beat.Prompt, *Beat.Reading.Summary);
		const ECinemafyHitReaction Answer = Beat.bOverrideReaction ? Beat.Reaction : Beat.Reading.Reaction;
		Line.bLethal = Kind == ECinemafyActionKind::Attack && Beat.bTargetReacts && TargetPawn && Answer == ECinemafyHitReaction::Death && Beat.ImpactTimes.Num() > 0;
		for (float Impact : Beat.ImpactTimes)
		{
			if (!Piece.bLine || Pieces.Num() == 1 || Beat.LineStartTime <= Beat.StartTime + 0.5f)
			{
				Line.ImpactTimes.Add(Impact - Start);
			}
		}

		if (Piece.bLine)
		{
			// A spoken line: the director covers it as dialogue (walk-and-talk while moving), with its words and audio.
			Line.Text = Beat.GetLineText();
			Line.Audio = Beat.DialogueAudio;
			Line.bAutoIntensity = Beat.DialogueAudio != nullptr;
			Line.Intensity = Beat.Reading.bAfterLine ? 0.6f : 0.5f;
			Line.Moment = bMoving ? ECinemafyMoment::WalkAndTalk : ECinemafyMoment::Dialogue;
			SpeechEnd = Beat.LineStartTime + Beat.LineDuration;
			if (Line.Listener)
			{
				LastListener = Line.Listener;
			}
			continue;
		}

		switch (Kind)
		{
		case ECinemafyActionKind::Attack:   Line.Intensity = 0.85f; Line.Moment = ECinemafyMoment::Action; break;
		case ECinemafyActionKind::Traverse: Line.Intensity = 0.65f; Line.Moment = ECinemafyMoment::Action; break;
		case ECinemafyActionKind::Perform:  Line.Intensity = 0.55f; break;
		case ECinemafyActionKind::Equip:    Line.Intensity = 0.5f;  break;
		case ECinemafyActionKind::Look:     Line.Intensity = 0.35f; break;
		case ECinemafyActionKind::Say:      Line.Intensity = 0.3f;  Line.Moment = ECinemafyMoment::SilentBeat; break; // waiting to answer
		case ECinemafyActionKind::Wait:     Line.Intensity = 0.2f;  Line.Moment = ECinemafyMoment::SilentBeat; break;
		case ECinemafyActionKind::Move:
			Line.Intensity = Beat.Reading.Gait == ECinemafyGait::Sprint ? 0.7f : Beat.Reading.Gait == ECinemafyGait::Run ? 0.45f : 0.25f;
			Line.Moment = Beat.Reading.Gait == ECinemafyGait::Sprint ? ECinemafyMoment::Action : ECinemafyMoment::WalkAndTalk;
			break;
		}
	}
	return Lines;
}
