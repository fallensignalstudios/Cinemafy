#include "CinemafyChoreographyLibrary.h"
#include "CinemafyModule.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameFramework/Pawn.h"

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
			{ TEXT("die"), TEXT("death"), TEXT("dead"), TEXT("dying") },
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

	TArray<FString> Words;
	Prompt.ToLower().ParseIntoArrayWS(Words);

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
	if (bAttack)
	{
		Reading.Kind = ECinemafyActionKind::Attack;
		Reading.Repeat = FMath::Clamp(Number > 0 ? Number : 1, 1, 6);
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
	else
	{
		Reading.Kind = ECinemafyActionKind::Wait;
		Reading.Seconds = static_cast<float>(Number);
	}
	(void)bWait;

	// What to look for in animation names.
	switch (Reading.Kind)
	{
	case ECinemafyActionKind::Attack:
	{
		bool bHasSpecific = false;
		for (const FString& Word : Others)
		{
			// Specific verbs ("kick", "punch", "shoot", "cast") narrow the search; generic ones fold into "attack".
			if (IsOneOf(Word, { TEXT("attack"), TEXT("atk"), TEXT("strike"), TEXT("swing"), TEXT("combo") }))
			{
				continue;
			}
			Reading.Keywords.Add(Concept(Word));
			bHasSpecific = true;
		}
		if (!bHasSpecific)
		{
			Reading.Keywords.Insert(Concept(TEXT("attack")), 0);
		}
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
	if (Reading.Seconds > 0.f)
	{
		Summary += FString::Printf(TEXT(" %.1fs"), Reading.Seconds);
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

TArray<FCinemafyLine> UCinemafyChoreographyLibrary::BuildDirectorLines(const TArray<FCinemafyChoreoBeat>& Beats, AActor* Performer, float TakeDuration)
{
	TArray<const FCinemafyChoreoBeat*> Timed;
	for (const FCinemafyChoreoBeat& Beat : Beats)
	{
		if (Beat.StartTime >= 0.f)
		{
			Timed.Add(&Beat);
		}
	}
	Timed.StableSort([](const FCinemafyChoreoBeat& A, const FCinemafyChoreoBeat& B) { return A.StartTime < B.StartTime; });

	TArray<FCinemafyLine> Lines;
	TArray<float> LineStarts;
	for (int32 Index = 0; Index < Timed.Num(); ++Index)
	{
		const FCinemafyChoreoBeat& Beat = *Timed[Index];
		const float Start = Index == 0 ? 0.f : Beat.StartTime;
		const float End = Index + 1 < Timed.Num() ? Timed[Index + 1]->StartTime : FMath::Max(TakeDuration, Beat.EndTime);
		const float Duration = End - Start;

		const ECinemafyActionKind Kind = Beat.Reading.Kind;
		const bool bAction = Kind == ECinemafyActionKind::Attack || Kind == ECinemafyActionKind::Traverse;
		const bool bMoving = Kind == ECinemafyActionKind::Move || Kind == ECinemafyActionKind::Traverse
			|| (Kind == ECinemafyActionKind::Attack && !Beat.Reading.bRanged);

		// Very short beats ride along with the one before (a cut every half second is noise).
		if (Lines.Num() > 0 && Duration < 0.8f)
		{
			FCinemafyLine& Previous = Lines.Last();
			for (float Impact : Beat.ImpactTimes)
			{
				Previous.ImpactTimes.Add(Impact - LineStarts.Last());
			}
			Previous.Duration += Duration;
			Previous.bAction |= bAction;
			Previous.Note += TEXT(", ") + Beat.Prompt;
			continue;
		}

		LineStarts.Add(Start);
		FCinemafyLine& Line = Lines.AddDefaulted_GetRef();
		Line.Speaker = Performer;
		APawn* TargetPawn = Cast<APawn>(Beat.Target.Get());
		Line.Listener = TargetPawn && TargetPawn != Performer ? TargetPawn : nullptr;
		Line.Duration = FMath::Max(Duration, 0.2f);
		Line.PauseAfter = 0.f;
		Line.bUseAudioDuration = false;
		Line.bAutoIntensity = false;
		Line.bAction = bAction;
		Line.bMoving = bMoving;
		Line.Note = FString::Printf(TEXT("%s (%s)"), *Beat.Prompt, *Beat.Reading.Summary);
		for (float Impact : Beat.ImpactTimes)
		{
			Line.ImpactTimes.Add(Impact - Start);
		}

		switch (Kind)
		{
		case ECinemafyActionKind::Attack:   Line.Intensity = 0.85f; Line.Moment = ECinemafyMoment::Action; break;
		case ECinemafyActionKind::Traverse: Line.Intensity = 0.65f; Line.Moment = ECinemafyMoment::Action; break;
		case ECinemafyActionKind::Perform:  Line.Intensity = 0.55f; break;
		case ECinemafyActionKind::Equip:    Line.Intensity = 0.5f;  break;
		case ECinemafyActionKind::Look:     Line.Intensity = 0.35f; break;
		case ECinemafyActionKind::Wait:     Line.Intensity = 0.2f;  Line.Moment = ECinemafyMoment::SilentBeat; break;
		case ECinemafyActionKind::Move:
			Line.Intensity = Beat.Reading.Gait == ECinemafyGait::Sprint ? 0.7f : Beat.Reading.Gait == ECinemafyGait::Run ? 0.45f : 0.25f;
			Line.Moment = Beat.Reading.Gait == ECinemafyGait::Sprint ? ECinemafyMoment::Action : ECinemafyMoment::WalkAndTalk;
			break;
		}
	}
	return Lines;
}
