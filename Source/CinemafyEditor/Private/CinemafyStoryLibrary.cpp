#include "CinemafyStoryLibrary.h"
#include "CinemafyChoreography.h"
#include "CinemafyChoreoTools.h"
#include "CinemafyPrevis.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "SCinemafyStoryBoard.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/PackageName.h"
#include "CinemafyMood.h"
#include "CinemafySettings.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "AutomatedAssetImportData.h"
#include "CinemafyFaceComponent.h"
#include "CinemafySequencerUtils.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sound/SoundWave.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "MovieSceneTimeHelpers.h"
#include "Widgets/Docking/SDockTab.h"
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

	// What each set needs dressing with, from its scenes' action (art's meshes and choices are kept).
	for (FCinemafySet& Set : Project->Sets)
	{
		for (FCinemafySetPiece& Piece : Set.Dressing)
		{
			Piece.Scenes.Reset();
			Piece.UsedBy.Reset();
		}
	}
	for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		FCinemafySet* Set = Project->Sets.FindByPredicate([Scene](const FCinemafySet& S) { return S.ScriptLocation.Equals(Scene->Location, ESearchCase::IgnoreCase); });
		if (!Set || Scene->Status == ECinemafySceneStatus::Cut)
		{
			continue;
		}
		for (const FCinemafySetPiece& Found : UCinemafyPrevisLibrary::FindSetDressing(*Scene))
		{
			FCinemafySetPiece* Piece = Set->Dressing.FindByPredicate([&Found](const FCinemafySetPiece& P) { return P.Name.Equals(Found.Name, ESearchCase::IgnoreCase); });
			if (!Piece)
			{
				Piece = &Set->Dressing.Add_GetRef(Found);
				Piece->Scenes.Reset();
				Piece->UsedBy.Reset();
			}
			Piece->Scenes.AddUnique(Scene->Id);
			for (const FString& Who : Found.UsedBy)
			{
				Piece->UsedBy.AddUnique(Who);
			}
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

namespace CinemafyRevision
{
	/** A fingerprint of a choreography's beats as staged (what, where, at whom, which line): changes mean hand edits. */
	FString BeatPrint(const ACinemafyChoreography* Choreo)
	{
		FString Words;
		for (const FCinemafyChoreoBeat& Beat : Choreo->Beats)
		{
			const FIntVector Rounded(FMath::RoundToInt(Beat.Location.X / 10.f), FMath::RoundToInt(Beat.Location.Y / 10.f), FMath::RoundToInt(Beat.Location.Z / 10.f));
			Words += FString::Printf(TEXT("|%s|%s|%s|%s|%d|%s"), *Beat.Prompt, *Rounded.ToString(), Beat.Target ? *Beat.Target->GetName() : TEXT(""),
				*Beat.Dialogue.ToString(), static_cast<int32>(Beat.Arrival), Beat.AnimationOverride ? *Beat.AnimationOverride->GetName() : TEXT(""));
		}
		return FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Words));
	}
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

	// Last pass's previs for this scene goes; the new one is built fresh. Choreographies changed by hand since
	// they were generated are kept (renamed Edited_..., sitting out takes) so no work is lost.
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
		const FString Label = Choreo->GetActorLabel();
		const FString* Generated = Scene->GeneratedPrints.Find(Label);
		if (Generated && *Generated != CinemafyRevision::BeatPrint(Choreo))
		{
			Choreo->Modify();
			Choreo->SetActorLabel(TEXT("Edited_") + Label);
			Choreo->bIncludeInTakes = false;
			Report.Add(FString::Printf(TEXT("Kept your edits: %s is now Edited_%s (out of takes). Copy beats across, or delete it."), *Label, *Label));
			continue;
		}
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
					if (Beat.Prompt == TEXT("vault") || Beat.Prompt == TEXT("climb") || Beat.Prompt == TEXT("jump"))
					{
						return It->GetActorLocation(); // Over it: the traversal plans its own line-up.
					}
					// Up to it, not into it: the near edge on their side, on the floor.
					FVector Center, Extent;
					It->GetActorBounds(true, Center, Extent);
					FVector Side = (Performer->GetActorLocation() - Center).GetSafeNormal2D();
					Side = Side.IsNearlyZero() ? FVector::ForwardVector : Side;
					return FVector(Center.X, Center.Y, Center.Z - Extent.Z) + Side * (FMath::Max(Extent.X, Extent.Y) + 60.f);
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
				Added.DialogueAudio = Beat.LineAudio.LoadSynchronous();
				Added.LineDirection = Beat.LineDirection;
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
	Project->Modify();
	for (const TPair<FString, ACinemafyChoreography*>& Pair : Choreos)
	{
		Pair.Value->RefreshLabels();
		Scene->GeneratedPrints.Add(Pair.Value->GetActorLabel(), CinemafyRevision::BeatPrint(Pair.Value));
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
	else if (Scene->IsOutOfDate())
	{
		const int32 Changed = UCinemafyEditorLibrary::RefreshDirectorFromScript(Director);
		Report.Add(FString::Printf(TEXT("The script changed: %d of the director's line(s) updated (unchanged lines keep your tweaks)."), FMath::Max(0, Changed)));
	}
	Scene->StagedHash = Scene->ContentHash;

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
	// The scene's mood picks the director's style, temp music and ambience (the level's lighting is left alone).
	const FString MoodReport = ApplySceneMood(Project, SceneId, false);
	if (!MoodReport.IsEmpty())
	{
		Report.Add(MoodReport);
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

FString UCinemafyStoryLibrary::UpdateChangedScenes(UCinemafyScriptProject* Project)
{
	UWorld* World = CinemafyStory::EditorWorld();
	if (!Project || !World)
	{
		return TEXT("No script project, or no level open.");
	}
	if (GEditor->PlayWorld)
	{
		return TEXT("Stop the play session first.");
	}
	TArray<FString> Report;
	TArray<FString> Ids;
	for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		if (Scene->IsOutOfDate() && Scene->Status != ECinemafySceneStatus::Cut)
		{
			Ids.Add(Scene->Id);
		}
	}
	int32 Updated = 0;
	for (const FString& Id : Ids)
	{
		FCinemafyScriptScene* Scene = Project->FindScene(Id);
		if (!Scene->Level.IsNull() && Scene->Level.ToSoftObjectPath() != FSoftObjectPath(World))
		{
			Report.Add(FString::Printf(TEXT("%s %s: staged in %s; open it and update again."), *Scene->Id, *Scene->Heading, *Scene->Level.GetAssetName()));
			continue;
		}
		const FString Prefix = FString::Printf(TEXT("Previs_%s_"), *Scene->Id.Replace(TEXT("."), TEXT("_")));
		bool bPrevisd = false;
		for (TActorIterator<ACinemafyChoreography> It(World); It && !bPrevisd; ++It)
		{
			bPrevisd = It->GetActorLabel().StartsWith(Prefix);
		}
		if (bPrevisd)
		{
			Report.Add(PrevisScene(Project, Id, false));
			Report.Add(TEXT("  Re-staged: Perform & Record (or Previs) to shoot the new version."));
			++Updated;
			continue;
		}
		ACinemafySceneDirector* Director = nullptr;
		for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
		{
			if (It->ScriptProject == Project && It->ScriptSceneId == Scene->Id)
			{
				Director = *It;
				break;
			}
		}
		if (Director)
		{
			const int32 Changed = UCinemafyEditorLibrary::RefreshDirectorFromScript(Director);
			Report.Add(FString::Printf(TEXT("%s %s: %d director line(s) updated. Direct again to re-cut."), *Scene->Id, *Scene->Heading, FMath::Max(0, Changed)));
			++Updated;
		}
		else
		{
			Report.Add(FString::Printf(TEXT("%s %s: nothing of it in this level; Previs it again."), *Scene->Id, *Scene->Heading));
		}
	}

	// The booth's list.
	for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.bNeedsRerecord)
			{
				Report.Add(FString::Printf(TEXT("Re-record %s (%s): \"%s\" (was \"%s\")"), *Line.LineId, *Line.Speaker, *Line.Text, *Line.PreviousText));
			}
		}
	}
	Report.Insert(Ids.Num() == 0 ? FString(TEXT("Every staged scene matches the script.")) :
		FString::Printf(TEXT("Updated %d of %d out-of-date scene(s)."), Updated, Ids.Num()), 0);
	Project->MarkPackageDirty();
	const FString Text = FString::Join(Report, TEXT("\n"));
	UE_LOG(LogCinemafy, Log, TEXT("%s"), *Text);
	return Text;
}

namespace CinemafyStory
{
	ULevelSequence* MakeMaster(const FString& Name)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/Cinemafy/Masters/%s"), *Name);
		ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sequence)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Sequence = NewObject<ULevelSequence>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
			Sequence->Initialize();
			Sequence->GetMovieScene()->SetDisplayRate(FFrameRate(30, 1));
			FAssetRegistryModule::AssetCreated(Sequence);
		}
		// Rebuilt from scratch: the shot track is Cinemafy's.
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		if (UMovieSceneTrack* Old = MovieScene->FindTrack<UMovieSceneCinematicShotTrack>())
		{
			MovieScene->RemoveTrack(*Old);
		}
		Sequence->MarkPackageDirty();
		return Sequence;
	}

	/** Places scenes back to back; returns the end (master ticks). */
	FFrameNumber LayScenes(ULevelSequence* Master, const TArray<const FCinemafyScriptScene*>& Scenes, int32& OutMissing)
	{
		UMovieScene* MovieScene = Master->GetMovieScene();
		UMovieSceneCinematicShotTrack* Track = MovieScene->AddTrack<UMovieSceneCinematicShotTrack>();
		const FFrameRate Resolution = MovieScene->GetTickResolution();
		FFrameNumber Time(0);
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			ULevelSequence* Sub = Scene->Status == ECinemafySceneStatus::Cut ? nullptr : Scene->Sequence.LoadSynchronous();
			if (!Sub)
			{
				OutMissing += Scene->Status == ECinemafySceneStatus::Cut ? 0 : 1;
				continue;
			}
			const UMovieScene* SubScene = Sub->GetMovieScene();
			const TRange<FFrameNumber> Range = SubScene->GetPlaybackRange();
			const double Seconds = SubScene->GetTickResolution().AsSeconds(FFrameTime(UE::MovieScene::DiscreteSize(Range)));
			const int32 Duration = FMath::Max(1, Resolution.AsFrameNumber(Seconds).Value);
			if (UMovieSceneSubSection* Section = Track->AddSequence(Sub, Time, Duration))
			{
				Section->SetRowIndex(0);
			}
			Time += Duration;
		}
		MovieScene->SetPlaybackRange(0, FMath::Max(Time.Value, 1));
		return Time;
	}
}

ULevelSequence* UCinemafyStoryLibrary::BuildMasterSequences(UCinemafyScriptProject* Project)
{
	if (!Project)
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(LOCTEXT("BuildMasters", "Cinemafy: Build Master Sequences"));
	Project->Modify();
	const FString Title = FPaths::MakeValidFileName(Project->Title.IsEmpty() ? Project->GetName() : Project->Title).Replace(TEXT(" "), TEXT("_"));
	int32 Missing = 0;
	TArray<const FCinemafyScriptScene*> Everything;
	for (FCinemafyScriptAct& Act : Project->Acts)
	{
		TArray<const FCinemafyScriptScene*> ActScenes;
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				ActScenes.Add(&Scene);
				Everything.Add(&Scene);
			}
		}
		ULevelSequence* ActMaster = CinemafyStory::MakeMaster(FString::Printf(TEXT("LS_%s_Act%s"), *Title, *Act.Id));
		int32 ActMissing = 0;
		CinemafyStory::LayScenes(ActMaster, ActScenes, ActMissing);
		Act.MasterSequence = ActMaster;
	}
	ULevelSequence* Film = CinemafyStory::MakeMaster(FString::Printf(TEXT("LS_%s_Film"), *Title));
	CinemafyStory::LayScenes(Film, Everything, Missing);
	Project->FilmSequence = Film;
	Project->MarkPackageDirty();
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: built master sequences for %s (%d scene(s) have no sequence yet)."), *Title, Missing);
	return Film;
}

ULevelSequence* UCinemafyStoryLibrary::NewTake(UCinemafyScriptProject* Project, const FString& SceneId)
{
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	ULevelSequence* Current = Scene ? Scene->Sequence.LoadSynchronous() : nullptr;
	if (!Current)
	{
		return nullptr;
	}
	FString Base = Current->GetName();
	int32 Underscore = INDEX_NONE;
	if (Base.FindLastChar(TEXT('_'), Underscore) && Base.Mid(Underscore + 1).StartsWith(TEXT("T")) && Base.Mid(Underscore + 2).IsNumeric())
	{
		Base = Base.Left(Underscore);
	}
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	const FString Folder = FPackageName::GetLongPackagePath(Current->GetOutermost()->GetName());
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(FString::Printf(TEXT("%s/%s_T%02d"), *Folder, *Base, Scene->Takes.Num() + 2), TEXT(""), PackageName, AssetName);
	ULevelSequence* Copy = Cast<ULevelSequence>(AssetTools.DuplicateAsset(AssetName, Folder, Current));
	if (!Copy)
	{
		return nullptr;
	}
	Project->Modify();
	Scene->Takes.Add(Current);
	Scene->Sequence = Copy;
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Copy);
	return Copy;
}

bool UCinemafyStoryLibrary::UseTake(UCinemafyScriptProject* Project, const FString& SceneId, int32 TakeIndex)
{
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!Scene || !Scene->Takes.IsValidIndex(TakeIndex))
	{
		return false;
	}
	Project->Modify();
	const TSoftObjectPtr<ULevelSequence> Chosen = Scene->Takes[TakeIndex];
	Scene->Takes.RemoveAt(TakeIndex);
	if (!Scene->Sequence.IsNull())
	{
		Scene->Takes.Add(Scene->Sequence);
	}
	Scene->Sequence = Chosen;
	return true;
}

bool UCinemafyStoryLibrary::MoveScene(UCinemafyScriptProject* Project, const FString& SceneId, int32 Delta)
{
	if (!Project || Delta == 0)
	{
		return false;
	}
	for (FCinemafyScriptAct& Act : Project->Acts)
	{
		for (FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			const int32 Index = Chapter.Scenes.IndexOfByPredicate([&SceneId](const FCinemafyScriptScene& S) { return S.Id == SceneId; });
			const int32 To = Index + FMath::Sign(Delta);
			if (Index == INDEX_NONE || !Chapter.Scenes.IsValidIndex(To))
			{
				continue;
			}
			const FScopedTransaction Transaction(LOCTEXT("MoveScene", "Cinemafy: Move Scene"));
			Project->Modify();
			Chapter.Scenes.Swap(Index, To);

			// Renumber the whole script, and keep directors pointing at the scenes they shoot.
			TMap<FString, FString> Renamed;
			int32 Number = 0;
			for (int32 A = 0; A < Project->Acts.Num(); ++A)
			{
				for (int32 C = 0; C < Project->Acts[A].Chapters.Num(); ++C)
				{
					TArray<FCinemafyScriptScene>& Scenes = Project->Acts[A].Chapters[C].Scenes;
					for (int32 S = 0; S < Scenes.Num(); ++S)
					{
						const FString NewId = FString::Printf(TEXT("%d.%d.%d"), A + 1, C + 1, S + 1);
						if (Scenes[S].Id != NewId)
						{
							Renamed.Add(Scenes[S].Id, NewId);
							Scenes[S].Id = NewId;
						}
						Scenes[S].Number = ++Number;
					}
				}
			}
			if (UWorld* World = CinemafyStory::EditorWorld())
			{
				for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
				{
					if (It->ScriptProject == Project)
					{
						if (const FString* NewId = Renamed.Find(It->ScriptSceneId))
						{
							It->Modify();
							It->ScriptSceneId = *NewId;
						}
					}
				}
			}
			Project->RefreshContinuity();
			return true;
		}
	}
	return false;
}

void UCinemafyStoryLibrary::OpenStoryBoard(UCinemafyScriptProject* Project)
{
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(SCinemafyStoryBoard::TabName);
	if (Project)
	{
		SCinemafyStoryBoard::SetProject(Project);
	}
}

namespace CinemafyVoices
{
	/** Speaks Text into a .wav with the platform's voices. */
	bool Synthesize(const FString& Text, const FString& InVoice, float Rate, float Volume, bool bWhisper, const FString& WavPath)
	{
		const FString TextPath = FPaths::ChangeExtension(WavPath, TEXT("txt"));
		FString Voice = InVoice;
		FString Spoken = Text;
#if PLATFORM_MAC
		// macOS speech takes loudness inline; a whisper uses the Whisper voice unless the cast picked one.
		Spoken = FString::Printf(TEXT("[[volm %.2f]] %s"), FMath::Clamp(Volume, 0.1f, 1.f), *Text);
		if (bWhisper && Voice.IsEmpty())
		{
			Voice = TEXT("Whisper");
		}
#endif
		FFileHelper::SaveStringToFile(Spoken, *TextPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		IFileManager::Get().Delete(*WavPath, false, true, true);
		int32 Code = -1;
		FString Out, Err;
#if PLATFORM_MAC
		const int32 WordsPerMinute = FMath::RoundToInt(175.f * Rate);
		const FString Params = FString::Printf(TEXT("%s-r %d -o \"%s\" --file-format=WAVE --data-format=LEI16@22050 -f \"%s\""),
			Voice.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("-v \"%s\" "), *Voice), WordsPerMinute, *WavPath, *TextPath);
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/say"), *Params, &Code, &Out, &Err);
#elif PLATFORM_WINDOWS
		const int32 SpeechRate = FMath::Clamp(FMath::RoundToInt((Rate - 1.f) * 10.f), -10, 10);
		const FString Script = FString::Printf(
			TEXT("Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; %s$s.Rate = %d; $s.Volume = %d; ")
			TEXT("$s.SetOutputToWaveFile('%s'); $s.Speak([IO.File]::ReadAllText('%s')); $s.Dispose()"),
			Voice.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("try { $s.SelectVoice('%s') } catch {}; "), *Voice.Replace(TEXT("'"), TEXT("''"))),
			SpeechRate, FMath::Clamp(FMath::RoundToInt(Volume * 100.f), 5, 100), *WavPath.Replace(TEXT("'"), TEXT("''")), *TextPath.Replace(TEXT("'"), TEXT("''")));
		FPlatformProcess::ExecProcess(TEXT("powershell.exe"), *FString::Printf(TEXT("-NoProfile -NonInteractive -Command \"%s\""), *Script), &Code, &Out, &Err);
#else
		const int32 WordsPerMinute = FMath::RoundToInt(165.f * Rate);
		const FString Params = FString::Printf(TEXT("%s-s %d -a %d -w \"%s\" -f \"%s\""),
			Voice.IsEmpty() ? (bWhisper ? TEXT("-v en+whisper ") : TEXT("")) : *FString::Printf(TEXT("-v %s "), *Voice), WordsPerMinute,
			FMath::Clamp(FMath::RoundToInt(Volume * 100.f), 10, 200), *WavPath, *TextPath);
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/espeak-ng"), *Params, &Code, &Out, &Err);
		if (!FPaths::FileExists(WavPath))
		{
			FPlatformProcess::ExecProcess(TEXT("/usr/bin/espeak"), *Params, &Code, &Out, &Err);
		}
#endif
		IFileManager::Get().Delete(*TextPath, false, true, true);
		if (!FPaths::FileExists(WavPath))
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: the scratch voice didn't speak (%d): %s %s"), Code, *Out, *Err);
			return false;
		}
		return true;
	}

	/** Gives every Scene Director line with these words the audio. */
	void ShareWithDirectors(UWorld* World, const FString& Text, USoundBase* Audio)
	{
		if (!World || !Audio)
		{
			return;
		}
		for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
		{
			for (FCinemafyLine& Line : It->Lines)
			{
				if (Line.Text.ToString().Equals(Text, ESearchCase::IgnoreCase))
				{
					It->Modify();
					Line.Audio = Audio;
					Line.bUseAudioDuration = true;
					Line.Analysis = FCinemafyAudioAnalysis();
				}
			}
		}
	}
}

int32 UCinemafyStoryLibrary::GenerateScratchVoices(UCinemafyScriptProject* Project, const FString& SceneId, bool bReplaceExistingScratch)
{
	if (!Project)
	{
		return 0;
	}
	const FString ProjectName = FPaths::MakeValidFileName(Project->GetName());
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Cinemafy") / TEXT("ScratchVO") / ProjectName);
	IFileManager::Get().MakeDirectory(*Folder, true);

	struct FPending { FCinemafyScriptLine* Line; FString Wav; };
	TArray<FPending> Spoken;
	for (FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		if (!SceneId.IsEmpty() && Scene->Id != SceneId)
		{
			continue;
		}
		for (FCinemafyScriptLine& Line : Scene->Lines)
		{
			const bool bHasReal = !Line.Audio.IsNull() && !Line.bScratchAudio;
			const bool bHasScratch = !Line.Audio.IsNull() && Line.bScratchAudio;
			if (Line.Kind != ECinemafyScriptLineKind::Dialogue || Line.LineId.IsEmpty() || bHasReal || (bHasScratch && !bReplaceExistingScratch))
			{
				continue;
			}
			const FCinemafyCastMember* Member = Project->FindCastMember(Line.Speaker);
			const FString Wav = Folder / (Line.LineId + TEXT(".wav"));
			// Read the way the script directs it: "(whispering)" quiet and slow, "(shouting)" loud and quick.
			const FCinemafyDelivery Delivery = UCinemafyScriptLibrary::ReadDelivery(Line.Direction, Line.Text);
			if (CinemafyVoices::Synthesize(Line.Text, Member ? Member->ScratchVoice : FString(), (Member ? Member->ScratchRate : 1.f) * Delivery.Rate,
				Delivery.Volume, Delivery.bWhisper, Wav))
			{
				Spoken.Add({ &Line, Wav });
			}
		}
	}
	if (Spoken.Num() == 0)
	{
		return 0;
	}

	UAutomatedAssetImportData* Import = NewObject<UAutomatedAssetImportData>();
	Import->DestinationPath = FString::Printf(TEXT("/Game/Cinemafy/ScratchVO/%s"), *ProjectName);
	Import->bReplaceExisting = true;
	for (const FPending& Pending : Spoken)
	{
		Import->Filenames.Add(Pending.Wav);
	}
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	const TArray<UObject*> Imported = AssetTools.ImportAssetsAutomated(Import);

	const FScopedTransaction Transaction(LOCTEXT("ScratchVO", "Cinemafy: Scratch Voices"));
	Project->Modify();
	UWorld* World = CinemafyStory::EditorWorld();
	int32 Voiced = 0;
	for (const FPending& Pending : Spoken)
	{
		const FString Name = FPaths::GetBaseFilename(Pending.Wav);
		UObject* const* Found = Imported.FindByPredicate([&Name](const UObject* Object) { return Object && Object->GetName() == Name; });
		USoundWave* Sound = Found ? Cast<USoundWave>(*Found) : nullptr;
		if (!Sound)
		{
			continue;
		}
		Pending.Line->Audio = Sound;
		Pending.Line->bScratchAudio = true;
		CinemafyVoices::ShareWithDirectors(World, Pending.Line->Text, Sound);
		++Voiced;
	}
	Project->MarkPackageDirty();
	return Voiced;
}

int32 UCinemafyStoryLibrary::AssignRecordedVoices(UCinemafyScriptProject* Project, const FString& ContentFolder)
{
	if (!Project || ContentFolder.IsEmpty())
	{
		return 0;
	}
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*ContentFolder), Assets, true);
	TMap<FString, FAssetData> ByName;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsInstanceOf(USoundBase::StaticClass()))
		{
			ByName.Add(Asset.AssetName.ToString().ToUpper(), Asset);
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("RecordedVO", "Cinemafy: Assign Recorded Voices"));
	Project->Modify();
	UWorld* World = CinemafyStory::EditorWorld();
	int32 Matched = 0;
	for (FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		int32 Number = 0;
		for (FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.Kind != ECinemafyScriptLineKind::Dialogue)
			{
				continue;
			}
			++Number;
			const FString ByLine = FString::Printf(TEXT("%s_%02d"), *Scene->Id.Replace(TEXT("."), TEXT("_")), Number);
			const FAssetData* Asset = ByName.Find(Line.LineId.ToUpper());
			if (!Asset)
			{
				Asset = ByName.Find(ByLine);
			}
			if (!Asset)
			{
				// Prefixed names ("VO_2_1_3_04", "SC2_1_3_04_Kade") still count.
				for (const TPair<FString, FAssetData>& Pair : ByName)
				{
					if (Pair.Key.Contains(Line.LineId.ToUpper()) || Pair.Key.Contains(ByLine))
					{
						Asset = &Pair.Value;
						break;
					}
				}
			}
			USoundBase* Sound = Asset ? Cast<USoundBase>(Asset->GetAsset()) : nullptr;
			if (!Sound)
			{
				continue;
			}
			Line.Audio = Sound;
			Line.bScratchAudio = false;
			Line.bNeedsRerecord = false;
			Line.PreviousText.Reset();
			CinemafyVoices::ShareWithDirectors(World, Line.Text, Sound);
			++Matched;
		}
	}
	Project->MarkPackageDirty();
	return Matched;
}

void UCinemafyStoryLibrary::BakeTalkCurves(ULevelSequence* Sequence, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart)
{
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	struct FFaceChannels { FMovieSceneFloatChannel* Open = nullptr; FMovieSceneFloatChannel* Narrow = nullptr; };
	TMap<UCinemafyFaceComponent*, FFaceChannels> Faces;

	auto MakeChannel = [MovieScene](const FGuid& Binding, const FName Property) -> FMovieSceneFloatChannel*
	{
		// Rebuilt each time: the old Talk track goes.
		if (UMovieSceneTrack* Old = MovieScene->FindTrack(UMovieSceneFloatTrack::StaticClass(), Binding, Property))
		{
			MovieScene->RemoveTrack(*Old);
		}
		UMovieSceneFloatTrack* Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(Binding);
		Track->SetPropertyNameAndPath(Property, Property.ToString());
		UMovieSceneSection* Section = Track->CreateNewSection();
		Section->SetRange(TRange<FFrameNumber>::All());
		Track->AddSection(*Section);
		return Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
	};

	float Time = 0.f;
	for (const FCinemafyLine& Line : Lines)
	{
		const TArray<float>& Open = Line.Analysis.TalkOpen;
		UCinemafyFaceComponent* Face = Line.Speaker && Line.Audio ? Line.Speaker->FindComponentByClass<UCinemafyFaceComponent>() : nullptr;
		if (Face && Open.Num() > 0)
		{
			FFaceChannels* Channels = Faces.Find(Face);
			if (!Channels)
			{
				const FGuid Binding = CinemafySequencer::BindObject(Sequence, Face);
				if (Binding.IsValid())
				{
					Channels = &Faces.Add(Face, FFaceChannels{ MakeChannel(Binding, TEXT("Talk")), MakeChannel(Binding, TEXT("TalkNarrow")) });
				}
			}
			if (Channels && Channels->Open && Channels->Narrow)
			{
				// 25 keys a second is plenty for a jaw; close the mouth after the last word.
				for (int32 Index = 0; Index < Open.Num(); Index += 2)
				{
					const FFrameNumber Frame = SceneStart + Resolution.AsFrameNumber(Time + Index * 0.02f);
					Channels->Open->AddLinearKey(Frame, Open[Index]);
					Channels->Narrow->AddLinearKey(Frame, Line.Analysis.TalkNarrow.IsValidIndex(Index) ? Line.Analysis.TalkNarrow[Index] : 0.f);
				}
				const FFrameNumber End = SceneStart + Resolution.AsFrameNumber(Time + Open.Num() * 0.02f + 0.05f);
				Channels->Open->AddLinearKey(End, 0.f);
				Channels->Narrow->AddLinearKey(End, 0.f);
			}
		}
		Time += FMath::Max(Line.Duration + Line.PauseAfter, 0.2f);
	}
}

namespace CinemafyMoodApply
{
	/** A light's intensity as it was before Cinemafy touched it (remembered in a tag), so moods don't compound. */
	float BaseIntensity(AActor* Light, float Current)
	{
		const FString Prefix = TEXT("Cinemafy.BaseIntensity:");
		for (const FName& Tag : Light->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(Prefix))
			{
				return FCString::Atof(*Text.Mid(Prefix.Len()));
			}
		}
		Light->Tags.Add(FName(*FString::Printf(TEXT("%s%f"), *Prefix, Current)));
		return Current;
	}

	UMovieSceneAudioTrack* FreshAudioTrack(UMovieScene* MovieScene, const FText& Name)
	{
		TArray<UMovieSceneTrack*> Old;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (Track && Track->IsA<UMovieSceneAudioTrack>() && Track->GetDisplayName().EqualTo(Name))
			{
				Old.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : Old)
		{
			MovieScene->RemoveTrack(*Track);
		}
		UMovieSceneAudioTrack* Track = MovieScene->AddTrack<UMovieSceneAudioTrack>();
		Track->SetDisplayName(Name);
		return Track;
	}
}

FString UCinemafyStoryLibrary::ApplySceneMood(UCinemafyScriptProject* Project, const FString& SceneId, bool bLightTheLevel)
{
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	UWorld* World = CinemafyStory::EditorWorld();
	if (!Scene || !World)
	{
		return FString();
	}
	// CONTINUOUS / LATER carry the time of day of the scene before.
	ECinemafyTimeOfDay Previous = ECinemafyTimeOfDay::Unknown;
	for (const FCinemafyScriptScene* Earlier : Project->GetAllScenes())
	{
		if (Earlier == Scene)
		{
			break;
		}
		Previous = UCinemafyMoodLibrary::ReadSceneMood(*Earlier, Previous).TimeOfDay;
	}
	const FCinemafySceneMood Mood = UCinemafyMoodLibrary::ReadSceneMood(*Scene, Previous);
	TArray<FString> Report;
	Report.Add(FString::Printf(TEXT("%s reads as %s."), *Scene->Id, *Mood.Why));

	const FScopedTransaction Transaction(LOCTEXT("SceneMood", "Cinemafy: Scene Mood"));
	const UCinemafySettings* Settings = GetDefault<UCinemafySettings>();
	for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
	{
		if (It->ScriptProject != Project || It->ScriptSceneId != Scene->Id)
		{
			continue;
		}
		It->Modify();
		It->Style = Mood.DirectorStyle;
		Report.Add(FString::Printf(TEXT("Director style: %s."), *StaticEnum<ECinemafyDirectorPreset>()->GetDisplayNameTextByValue(static_cast<int64>(Mood.DirectorStyle)).ToString()));
		if (!It->Music)
		{
			if (const TSoftObjectPtr<USoundBase>* Music = Settings->MoodMusic.Find(Mood.Mood))
			{
				It->Music = Music->LoadSynchronous();
				Report.Add(FString::Printf(TEXT("Temp music: %s."), *Music->GetAssetName()));
			}
		}
	}
	for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
	{
		if (It->GetActorLabel().StartsWith(FString::Printf(TEXT("Previs_%s_"), *Scene->Id.Replace(TEXT("."), TEXT("_")))))
		{
			It->Modify();
			It->DirectorStyle = Mood.DirectorStyle;
		}
	}

	// The set's room tone under the whole scene.
	const FCinemafySet* Set = Project->FindSet(Scene->Location);
	ULevelSequence* Sequence = Scene->Sequence.LoadSynchronous();
	if (Set && Sequence && !Set->Ambience.IsNull())
	{
		if (USoundBase* Ambience = Set->Ambience.LoadSynchronous())
		{
			UMovieScene* MovieScene = Sequence->GetMovieScene();
			UMovieSceneAudioTrack* Track = CinemafyMoodApply::FreshAudioTrack(MovieScene, LOCTEXT("AmbienceTrack", "Cinemafy Ambience"));
			if (UMovieSceneSection* Section = Track->AddNewSoundOnRow(Ambience, UE::MovieScene::DiscreteInclusiveLower(MovieScene->GetPlaybackRange()), 0))
			{
				Section->SetRange(MovieScene->GetPlaybackRange());
			}
			Report.Add(FString::Printf(TEXT("Ambience: %s."), *Ambience->GetName()));
		}
	}

	if (bLightTheLevel)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			ULightComponent* Light = It->GetLightComponent();
			if (!Light)
			{
				continue;
			}
			It->Modify();
			Light->Modify();
			const float Base = CinemafyMoodApply::BaseIntensity(*It, Light->Intensity);
			FRotator Rotation = It->GetActorRotation();
			Rotation.Pitch = Mood.SunPitch;
			It->SetActorRotation(Rotation);
			Light->SetIntensity(Base * Mood.SunScale);
			Light->SetLightColor(Mood.SunColor);
			Report.Add(FString::Printf(TEXT("Sun: %.0f degrees, %.0f%% bright."), -Mood.SunPitch, Mood.SunScale * 100.f));
			break;
		}
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			USkyLightComponent* Sky = It->GetLightComponent();
			if (!Sky)
			{
				continue;
			}
			It->Modify();
			Sky->Modify();
			const float Base = CinemafyMoodApply::BaseIntensity(*It, Sky->Intensity);
			const float Scale = Mood.TimeOfDay == ECinemafyTimeOfDay::Night ? 0.2f : (Mood.TimeOfDay == ECinemafyTimeOfDay::Dawn || Mood.TimeOfDay == ECinemafyTimeOfDay::Dusk ? 0.6f : 1.f);
			Sky->SetIntensity(Base * Scale);
			Sky->RecaptureSky();
			break;
		}
	}
	return FString::Join(Report, TEXT("\n"));
}

int32 UCinemafyStoryLibrary::BakeFoley(ULevelSequence* Sequence, const TArray<ACinemafyChoreography*>& Ensemble, float TakeStartSeconds)
{
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	const UCinemafySettings* Settings = GetDefault<UCinemafySettings>();
	if (!MovieScene || Settings->FoleySounds.Num() == 0)
	{
		return 0;
	}
	struct FCue { float Time; FString Kind; };
	TArray<FCue> Cues;
	for (const ACinemafyChoreography* Choreo : Ensemble)
	{
		if (!Choreo)
		{
			continue;
		}
		for (const FCinemafyChoreoBeat& Beat : Choreo->Beats)
		{
			const ECinemafyHitReaction Answer = Beat.bOverrideReaction ? Beat.Reaction : Beat.Reading.Reaction;
			for (const FCinemafyPerformanceEvent& Event : Beat.Events)
			{
				const FString Note = Event.Note.ToLower();
				switch (Event.Kind)
				{
				case ECinemafyPerformanceEventKind::Contact:  Cues.Add(FCue{ Event.Time, TEXT("impact") }); break;
				case ECinemafyPerformanceEventKind::Takeoff:  Cues.Add(FCue{ Event.Time, TEXT("takeoff") }); break;
				case ECinemafyPerformanceEventKind::Landing:  Cues.Add(FCue{ Event.Time, TEXT("land") }); break;
				case ECinemafyPerformanceEventKind::Reaction:
					if (Answer == ECinemafyHitReaction::Death)
					{
						Cues.Add(FCue{ Event.Time + 0.9f, TEXT("body_fall") });
					}
					break;
				case ECinemafyPerformanceEventKind::Transition:
					if (Note.Contains(TEXT("draw")))
					{
						Cues.Add(FCue{ Event.Time, TEXT("draw") });
					}
					else if (Note.Contains(TEXT("roll")) || Note.Contains(TEXT("recover")))
					{
						Cues.Add(FCue{ Event.Time, TEXT("roll") });
					}
					break;
				case ECinemafyPerformanceEventKind::ActionStart:
					if (Beat.Reading.Kind == ECinemafyActionKind::Equip)
					{
						Cues.Add(FCue{ Event.Time, Beat.Reading.Keywords.ContainsByPredicate([](const FString& W) { return W.StartsWith(TEXT("holster")); }) ? TEXT("holster") : TEXT("draw") });
					}
					break;
				default:
					break;
				}
			}
		}
	}

	UMovieSceneAudioTrack* Track = CinemafyMoodApply::FreshAudioTrack(MovieScene, LOCTEXT("FoleyTrack", "Cinemafy Foley"));
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	int32 Placed = 0;
	for (const FCue& Cue : Cues)
	{
		const TSoftObjectPtr<USoundBase>* Sound = Settings->FoleySounds.Find(Cue.Kind);
		USoundBase* Loaded = Sound ? Sound->LoadSynchronous() : nullptr;
		if (Loaded)
		{
			Track->AddNewSoundOnRow(Loaded, Resolution.AsFrameNumber(TakeStartSeconds + Cue.Time), Placed % 4);
			++Placed;
		}
	}
	return Placed;
}

#undef LOCTEXT_NAMESPACE
