#include "CinemafyPerformerProfile.h"
#include "CinemafyChoreographyTypes.h"
#include "CinemafySettings.h"
#include "GameFramework/Character.h"

void UCinemafyPerformerProfile::ApplyPreset(ECinemafyPerformerPreset Preset)
{
	Modify();
	bAutoDetectMissing = false;
	SprintFunction = WalkFunction = CrouchFunction = UncrouchFunction = TraversalFunction = OverlayFunction = NAME_None;
	SprintProperty.Reset();
	WalkProperty.Reset();
	OverlayProperty.Reset();
	OverlayValues.Reset();
	Traversal = FCinemafyTraversalEnvelope();
	SkipRecordingComponents = { FName(TEXT("Face")) };

	switch (Preset)
	{
	case ECinemafyPerformerPreset::GASP:
	case ECinemafyPerformerPreset::GASPALS:
	case ECinemafyPerformerPreset::MetaHumanGASP:
		// The sample character keeps its input intent in a struct and traverses through TryTraversalAction
		// (hurdles and vaults up to about 1.25 m, mantles up to about 2.75 m).
		SprintProperty = TEXT("CharacterInputState.WantsToSprint");
		WalkProperty = TEXT("CharacterInputState.WantsToWalk");
		TraversalFunction = TEXT("TryTraversalAction");
		if (Preset == ECinemafyPerformerPreset::GASPALS)
		{
			// The overlay pose lives in a Blueprint enum whose name varies by version: let it be found by name.
			bAutoDetectMissing = true;
			Notes = TEXT("GASPALS: overlay found by name (an enum with 'Overlay' in it). Set Overlay Property once you know it.");
		}
		if (Preset == ECinemafyPerformerPreset::MetaHumanGASP)
		{
			Notes = TEXT("MetaHuman body retargeted from the GASP mannequin. The face is left to Cinemafy Face.");
		}
		break;

	case ECinemafyPerformerPreset::SignalForge:
		SprintFunction = TEXT("SetWantsToSprint");
		WalkFunction = TEXT("SetWantsToWalk");
		CrouchFunction = TEXT("StartCrouch");
		UncrouchFunction = TEXT("StopCrouch");
		TraversalFunction = TEXT("TryTraversalAction");
		bAutoDetectMissing = true; // overlays switch through equipment; found by name if exposed
		Notes = TEXT("SignalForge ASFCharacterBase: intent setters in C++, GASP traversal in the character Blueprint.");
		break;

	case ECinemafyPerformerPreset::GenericCharacter:
		Traversal.bCanTraverse = false;
		Notes = TEXT("No gait or traversal system: speeds from Fallback Speeds, obstacles only by jumping.");
		break;
	}
}

const UCinemafyPerformerProfile* UCinemafyPerformerProfile::ResolveFor(const ACharacter* Character, const UCinemafyPerformerProfile* Explicit)
{
	if (Explicit || !Character)
	{
		return Explicit;
	}
	const UCinemafySettings* Settings = GetDefault<UCinemafySettings>();
	if (!Settings)
	{
		return nullptr;
	}
	// The most specific class wins: walk up from the character's own class.
	for (const UClass* Class = Character->GetClass(); Class; Class = Class->GetSuperClass())
	{
		for (const TPair<TSoftClassPtr<ACharacter>, TSoftObjectPtr<UCinemafyPerformerProfile>>& Entry : Settings->PerformerProfiles)
		{
			if (Entry.Key.ToSoftObjectPath() == FSoftObjectPath(Class))
			{
				return Entry.Value.LoadSynchronous();
			}
		}
	}
	return nullptr;
}
