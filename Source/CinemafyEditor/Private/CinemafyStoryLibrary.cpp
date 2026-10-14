#include "CinemafyStoryLibrary.h"
#include "CinemafyChoreography.h"
#include "CinemafyChoreoTools.h"
#include "CinemafyPrevis.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Subsystems/AssetEditorSubsystem.h"
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

ULevelSequence* UCinemafyStoryLibrary::OpenSceneSequence(UCinemafyScriptProject* Project, const FString& SceneId)
{
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!Scene)
	{
		return nullptr;
	}
	ULevelSequence* Sequence = Scene->Sequence.LoadSynchronous();
	if (!Sequence)
	{
		const FString Place = FPaths::MakeValidFileName(Scene->Location.IsEmpty() ? FString(TEXT("Scene")) : Scene->Location).Replace(TEXT(" "), TEXT("_"));
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		FString PackageName, AssetName;
		AssetTools.CreateUniqueAssetName(FString::Printf(TEXT("/Game/Cinemafy/Scenes/SC_%s_%s"), *Scene->Id.Replace(TEXT("."), TEXT("_")), *Place), TEXT(""), PackageName, AssetName);
		UPackage* Package = CreatePackage(*PackageName);
		Sequence = NewObject<ULevelSequence>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		Sequence->Initialize();
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		MovieScene->SetDisplayRate(FFrameRate(30, 1));
		MovieScene->SetPlaybackRange(0, MovieScene->GetTickResolution().AsFrameNumber(FMath::Max(Scene->EstimatedSeconds, 5.f)).Value);
		FAssetRegistryModule::AssetCreated(Sequence);
		Package->MarkPackageDirty();
		Project->Modify();
		Scene->Sequence = Sequence;
	}
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Sequence);
	return Sequence;
}

FString UCinemafyStoryLibrary::PrevisScene(UCinemafyScriptProject* Project, const FString& SceneId, bool bRecord)
{
	UWorld* World = CinemafyStory::EditorWorld();
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!World || !Scene)
	{
		return TEXT("No such scene, or no level open.");
	}
	if (GEditor->PlayWorld)
	{
		return TEXT("Stop the play session first.");
	}
	TArray<FString> Report;
	Report.Add(FString::Printf(TEXT("Previs %s %s"), *Scene->Id, *Scene->Heading));

	const FScopedTransaction Transaction(LOCTEXT("PrevisScene", "Cinemafy: Previs Scene"));
	if (Project->CastMembers.Num() == 0)
	{
		BuildCastAndSets(Project);
	}
	const FCinemafySet* Set = Project->FindSet(Scene->Location);
	if (Set && !Set->Level.IsNull() && Set->Level.ToSoftObjectPath() != FSoftObjectPath(World))
	{
		Report.Add(FString::Printf(TEXT("Note: %s is set in %s, but %s is open. Staging here anyway."), *Scene->Location, *Set->Level.GetAssetName(), *World->GetMapName()));
	}

	// The staging, from the script.
	TArray<FString> NotStaged;
	const TArray<FCinemafyStagedBeat> Staged = UCinemafyPrevisLibrary::StageScene(*Scene, NotStaged);

	// The cast on set: who's here, who has to be brought in.
	TArray<FString> Characters;
	for (const FCinemafyStagedBeat& Beat : Staged)
	{
		Characters.AddUnique(Beat.Character);
		if (!Beat.Target.IsEmpty())
		{
			Characters.AddUnique(Beat.Target);
		}
	}
	TArray<ACinemafyMark*> StartMarks;
	for (TActorIterator<ACinemafyMark> It(World); It; ++It)
	{
		if (It->bStartMark && (It->SetName.IsEmpty() || It->SetName.Equals(Scene->Location, ESearchCase::IgnoreCase)))
		{
			StartMarks.Add(*It);
		}
	}
	TMap<FString, AActor*> Actors;
	int32 Spawned = 0;
	for (const FString& Name : Characters)
	{
		AActor* Actor = FindCastActor(Project, Name, World);
		const FCinemafyCastMember* Member = Project->FindCastMember(Name);
		if (!Actor && Member && !Member->CharacterClass.IsNull())
		{
			if (UClass* Class = Member->CharacterClass.LoadSynchronous())
			{
				const FTransform Start = StartMarks.IsValidIndex(Spawned) ? StartMarks[Spawned]->GetActorTransform()
					: FTransform(FVector(0.f, 200.f * Spawned, 100.f));
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				Actor = World->SpawnActor<AActor>(Class, Start, Params);
				if (Actor)
				{
					const FString Label = Member->ActorLabel.IsEmpty() ? Name.Replace(TEXT(" "), TEXT("_")) : Member->ActorLabel;
					Actor->SetActorLabel(Label);
					++Spawned;
					Report.Add(FString::Printf(TEXT("Brought in %s (%s)."), *Name, *Class->GetName()));
				}
			}
		}
		if (Actor)
		{
			Actors.Add(Name, Actor);
		}
		else
		{
			Report.Add(FString::Printf(TEXT("Not on set: %s (put them in the level, or give their Cast entry a Character Class)."), *Name));
		}
	}
	ApplySceneContinuity(Project, SceneId);

	// Last pass's previs for this scene goes; the new one is built fresh.
	const FString Prefix = FString::Printf(TEXT("Previs_%s_"), *Scene->Id.Replace(TEXT("."), TEXT("_")));
	TArray<ACinemafyChoreography*> Old;
	for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
	{
		if (It->GetActorLabel().StartsWith(Prefix))
		{
			Old.Add(*It);
		}
	}
	for (ACinemafyChoreography* Choreo : Old)
	{
		World->EditorDestroyActor(Choreo, true);
	}
	// Other choreographies in the level sit this take out (Include In Takes is turned off on them; turn it back on to mix them in).
	int32 SatOut = 0;
	for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
	{
		if (It->bIncludeInTakes)
		{
			It->Modify();
			It->bIncludeInTakes = false;
			++SatOut;
		}
	}
	if (SatOut > 0)
	{
		Report.Add(FString::Printf(TEXT("%d other choreograph(ies) in the level sit this take out (Include In Takes turned off)."), SatOut));
	}

	// A choreography per character, beats in script order.
	TMap<FString, ACinemafyChoreography*> Choreos;
	auto PlaceFor = [&](const FCinemafyStagedBeat& Beat, AActor* Performer, AActor* Target, bool& bOutResolved) -> FVector
	{
		bOutResolved = true;
		if (!Beat.Place.IsEmpty())
		{
			if (ACinemafyMark* Mark = UCinemafyProductionLibrary::FindMark(World, Beat.Place, Scene->Location))
			{
				return Mark->GetActorLocation();
			}
			// A prop or piece of set dressing with that name.
			const FString Wanted = Beat.Place.ToLower();
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const FString Label = It->GetActorLabel().ToLower().Replace(TEXT("_"), TEXT(" "));
				if (*It != Performer && !It->IsA<ACinemafyChoreography>() && (Label.Contains(Wanted) || (Wanted.Len() > 4 && Wanted.Contains(Label) && Label.Len() >= 4)))
				{
					return It->GetActorLocation();
				}
			}
			bOutResolved = false;
		}
		if (Target)
		{
			// Toward them, stopping short.
			const FVector From = Performer->GetActorLocation();
			const FVector To = Target->GetActorLocation();
			return To + (From - To).GetSafeNormal2D() * 150.f;
		}
		return Performer->GetActorLocation();
	};

	int32 BeatCount = 0;
	for (const FCinemafyStagedBeat& Beat : Staged)
	{
		AActor* const* PerformerActor = Actors.Find(Beat.Character);
		ACharacter* Performer = PerformerActor ? Cast<ACharacter>(*PerformerActor) : nullptr;
		if (!Performer)
		{
			continue;
		}
		ACinemafyChoreography*& Choreo = Choreos.FindOrAdd(Beat.Character);
		if (!Choreo)
		{
			FActorSpawnParameters Params;
			Choreo = World->SpawnActor<ACinemafyChoreography>(Performer->GetActorLocation(), FRotator::ZeroRotator, Params);
			Choreo->SetActorLabel(Prefix + Beat.Character.Replace(TEXT(" "), TEXT("_")));
			Choreo->Performer = Performer;
			Choreo->bIncludeInTakes = true;
			if (const FCinemafyCastMember* Member = Project->FindCastMember(Beat.Character))
			{
				Choreo->Profile = Member->Profile.LoadSynchronous();
				Choreo->StartingWeapon = Member->StartingWeapon;
			}
			// What they carry in from earlier scenes wins over the cast default.
			for (const FCinemafyContinuityFact& Fact : Scene->CarriedIn)
			{
				if (Fact.Character == Beat.Character && Fact.Category == TEXT("Carrying"))
				{
					for (const TCHAR* Weapon : { TEXT("sword"), TEXT("pistol"), TEXT("rifle"), TEXT("dagger"), TEXT("bow") })
					{
						if (Fact.Fact.Contains(Weapon))
						{
							Choreo->StartingWeapon = Weapon;
						}
					}
				}
			}
		}
		AActor* const* TargetActor = Actors.Find(Beat.Target);
		AActor* Target = TargetActor ? *TargetActor : nullptr;
		bool bResolved = true;
		const FVector Location = PlaceFor(Beat, Performer, Target, bResolved);
		if (!bResolved)
		{
			Report.Add(FString::Printf(TEXT("No mark for \"%s\" (%s): place a Cinemafy Mark named that. Stayed put."), *Beat.Place, *Beat.Source));
		}
		const int32 Index = Choreo->AddBeat(Beat.Prompt, Location, Target);
		if (Choreo->Beats.IsValidIndex(Index))
		{
			FCinemafyChoreoBeat& Added = Choreo->Beats[Index];
			if (!Beat.Line.IsEmpty())
			{
				Added.Dialogue = FText::FromString(Beat.Line);
				Added.Listener = Target;
			}
			if (AActor* const* Cue = Actors.Find(Beat.WaitFor))
			{
				Added.WaitForLineFrom = *Cue;
				Added.WaitForLineNumber = Beat.WaitForLineNumber;
			}
		}
		++BeatCount;
	}
	for (const TPair<FString, ACinemafyChoreography*>& Pair : Choreos)
	{
		Pair.Value->RefreshLabels();
	}
	Report.Add(FString::Printf(TEXT("Staged %d beat(s) for %d character(s)."), BeatCount, Choreos.Num()));
	for (const FString& Clause : NotStaged)
	{
		Report.Add(FString::Printf(TEXT("Not staged: \"%s\""), *Clause));
	}

	// The scene's sequence, and its director (linked back to the script).
	OpenSceneSequence(Project, SceneId);
	ACinemafySceneDirector* Director = nullptr;
	for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
	{
		if (It->ScriptProject == Project && It->ScriptSceneId == Scene->Id)
		{
			Director = *It;
			break;
		}
	}
	if (!Director)
	{
		Director = UCinemafyEditorLibrary::CreateDirectorForScene(Project, Scene->Id);
	}

	ACinemafyChoreography* Primary = nullptr;
	for (const TPair<FString, ACinemafyChoreography*>& Pair : Choreos)
	{
		// The scene's first speaker leads (their director cuts the take).
		if (!Primary || (Scene->Characters.Num() > 0 && Pair.Key == Scene->Characters[0]))
		{
			Primary = Pair.Value;
		}
	}
	if (Primary && Director)
	{
		Primary->SceneDirector = Director;
		Primary->bDirectAfterBake = true;
	}
	Project->Modify();
	if (Scene->Status == ECinemafySceneStatus::NotStarted)
	{
		Scene->Status = ECinemafySceneStatus::Blocking;
	}

	if (bRecord && Primary)
	{
		CinemafyChoreoTake::PerformAndRecord(Primary, /*bAskAboutProblems*/ false);
		Report.Add(TEXT("Recording: play stops by itself, then the take is baked and cut into the scene's sequence."));
	}
	const FString Text = FString::Join(Report, TEXT("\n"));
	UE_LOG(LogCinemafy, Log, TEXT("%s"), *Text);
	return Text;
}

#undef LOCTEXT_NAMESPACE
