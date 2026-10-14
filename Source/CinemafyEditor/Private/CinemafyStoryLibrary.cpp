#include "CinemafyStoryLibrary.h"
#include "CinemafyChoreography.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafyProduction.h"
#include "CinemafySceneDirector.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "CinemafyStory"

namespace CinemafyStory
{
	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	/** Script names and labels compared loosely: "ADMIRAL THANE" finds "Thane", "BP_Thane" or "Admiral_Thane". */
	bool NameMatchesLabel(const FString& ScriptName, const FString& Label)
	{
		const FString Name = ScriptName.ToLower();
		const FString Lower = Label.ToLower().Replace(TEXT("_"), TEXT(" "));
		if (Lower == Name || Lower.Contains(Name))
		{
			return true;
		}
		int32 Space = INDEX_NONE;
		if (Name.FindLastChar(TEXT(' '), Space))
		{
			const FString Last = Name.Mid(Space + 1);
			return Last.Len() >= 3 && Lower.Contains(Last);
		}
		return false;
	}
}

AActor* UCinemafyStoryLibrary::FindCastActor(const UCinemafyScriptProject* Project, const FString& ScriptName, UWorld* World)
{
	if (!World || ScriptName.IsEmpty())
	{
		return nullptr;
	}
	const FCinemafyCastMember* Member = Project ? Project->FindCastMember(ScriptName) : nullptr;
	const FString WantedLabel = Member && !Member->ActorLabel.IsEmpty() ? Member->ActorLabel : FString();
	ACharacter* Loose = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!WantedLabel.IsEmpty() && It->GetActorLabel().Equals(WantedLabel, ESearchCase::IgnoreCase))
		{
			return *It;
		}
		ACharacter* Character = Cast<ACharacter>(*It);
		if (!Loose && Character && CinemafyStory::NameMatchesLabel(ScriptName, Character->GetActorLabel()))
		{
			Loose = Character;
		}
	}
	return WantedLabel.IsEmpty() ? Loose : nullptr;
}

int32 UCinemafyStoryLibrary::BuildCastAndSets(UCinemafyScriptProject* Project)
{
	UWorld* World = CinemafyStory::EditorWorld();
	if (!Project)
	{
		return 0;
	}
	const FScopedTransaction Transaction(LOCTEXT("BuildCastAndSets", "Cinemafy: Build Cast & Sets"));
	Project->Modify();
	int32 Added = 0;

	for (const FString& Name : Project->CastList)
	{
		FCinemafyCastMember* Member = Project->CastMembers.FindByPredicate([&Name](const FCinemafyCastMember& M) { return M.ScriptName.Equals(Name, ESearchCase::IgnoreCase); });
		if (!Member)
		{
			Member = &Project->CastMembers.AddDefaulted_GetRef();
			Member->ScriptName = Name;
			++Added;
		}
		if (Member->ActorLabel.IsEmpty() && World)
		{
			if (AActor* Actor = FindCastActor(Project, Name, World))
			{
				Member->ActorLabel = Actor->GetActorLabel();
				if (Member->CharacterClass.IsNull() && Actor->GetClass()->IsChildOf(ACharacter::StaticClass()))
				{
					Member->CharacterClass = TSoftClassPtr<ACharacter>(Actor->GetClass());
				}
			}
		}
	}

	TArray<FString> Locations;
	for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		if (!Scene->Location.IsEmpty())
		{
			Locations.AddUnique(Scene->Location);
		}
	}
	for (const FString& Location : Locations)
	{
		FCinemafySet* Set = Project->Sets.FindByPredicate([&Location](const FCinemafySet& S) { return S.ScriptLocation.Equals(Location, ESearchCase::IgnoreCase); });
		if (!Set)
		{
			Set = &Project->Sets.AddDefaulted_GetRef();
			Set->ScriptLocation = Location;
			++Added;
		}
		if (!World)
		{
			continue;
		}
		// Marks placed for this set name it; that also tells us which level it's built in.
		Set->Marks.Reset();
		for (TActorIterator<ACinemafyMark> It(World); It; ++It)
		{
			if (It->SetName.Equals(Location, ESearchCase::IgnoreCase))
			{
				Set->Marks.AddUnique(It->MarkName);
			}
		}
		if (Set->Marks.Num() > 0 && Set->Level.IsNull())
		{
			Set->Level = World;
		}
	}
	Project->MarkPackageDirty();
	return Added;
}

ACinemafyMark* UCinemafyStoryLibrary::PlaceMark(const FString& MarkName, const FString& SetName)
{
	UWorld* World = CinemafyStory::EditorWorld();
	if (!World)
	{
		return nullptr;
	}
	FVector Location = FVector::ZeroVector;
	if (FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient)
	{
		const FVector Eye = Client->GetViewLocation();
		const FVector Forward = Client->GetViewRotation().Vector();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyPlaceMark), true);
		Location = World->LineTraceSingleByChannel(Hit, Eye, Eye + Forward * 5000.f, ECC_Visibility, Params) ? Hit.ImpactPoint : Eye + Forward * 500.f;
	}
	const FScopedTransaction Transaction(LOCTEXT("PlaceMark", "Cinemafy: Place Mark"));
	FActorSpawnParameters Params;
	ACinemafyMark* Mark = World->SpawnActor<ACinemafyMark>(Location, FRotator::ZeroRotator, Params);
	if (Mark)
	{
		Mark->MarkName = MarkName.IsEmpty() ? FString(TEXT("mark")) : MarkName;
		Mark->SetName = SetName;
		Mark->SetActorLabel(FString::Printf(TEXT("Mark_%s"), *Mark->MarkName.Replace(TEXT(" "), TEXT("_"))));
		Mark->RerunConstructionScripts();
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Mark, true, true);
	}
	return Mark;
}

int32 UCinemafyStoryLibrary::ApplySceneContinuity(UCinemafyScriptProject* Project, const FString& SceneId)
{
	UWorld* World = CinemafyStory::EditorWorld();
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!World || !Scene)
	{
		return 0;
	}
	int32 Updated = 0;
	for (const FString& Name : Scene->Characters)
	{
		AActor* Actor = FindCastActor(Project, Name, World);
		if (!Actor)
		{
			continue;
		}
		TArray<FCinemafyContinuityFact> Facts;
		for (const FCinemafyContinuityFact& Fact : Scene->CarriedIn)
		{
			if (Fact.Character == Name)
			{
				Facts.Add(Fact);
			}
		}
		static const FCinemafyCastMember NoLooks;
		const FCinemafyCastMember* Member = Project->FindCastMember(Name);
		Actor->Modify();
		UCinemafyProductionLibrary::ApplyContinuityToActor(Actor, Member ? *Member : NoLooks, Facts);
		++Updated;
	}
	return Updated;
}

#undef LOCTEXT_NAMESPACE
