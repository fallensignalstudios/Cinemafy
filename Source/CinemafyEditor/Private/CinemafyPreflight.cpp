#include "CinemafyPreflight.h"
#include "CinemafyChoreography.h"
#include "CinemafyModule.h"
#include "Misc/MessageDialog.h"
#include "CinemafyPrevis.h"
#include "CinemafyProduction.h"
#include "CinemafyScript.h"
#include "CinemafyStoryLibrary.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "LevelEditorViewport.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyPreflight"

namespace CinemafyPreflight
{
	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	FCinemafyCheck MakeCheck(const FString& SceneId, ECinemafyCheckSeverity Severity, const FString& Problem, ECinemafyFix Fix = ECinemafyFix::None,
		const FString& FixArg = FString(), const FString& FixLabel = FString())
	{
		FCinemafyCheck Check;
		Check.SceneId = SceneId;
		Check.Severity = Severity;
		Check.Problem = Problem;
		Check.Fix = Fix;
		Check.FixArg = FixArg;
		Check.FixLabel = FixLabel;
		return Check;
	}

	/** Is there a computer voice to read scratch lines with? */
	bool HasSpeechEngine()
	{
#if PLATFORM_MAC
		return FPaths::FileExists(TEXT("/usr/bin/say"));
#elif PLATFORM_WINDOWS
		return true;
#else
		return FPaths::FileExists(TEXT("/usr/bin/espeak-ng")) || FPaths::FileExists(TEXT("/usr/bin/espeak"));
#endif
	}

	/** The usual template characters (GASP sandbox, Third Person), whichever the project has. */
	UClass* FindTemplateCharacter()
	{
		static const TCHAR* Candidates[] = {
			TEXT("/Game/Blueprints/CBP_SandboxCharacter.CBP_SandboxCharacter_C"),
			TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"),
			TEXT("/Game/ThirdPersonBP/Blueprints/ThirdPersonCharacter.ThirdPersonCharacter_C"),
			TEXT("/Game/Characters/Mannequins/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C") };
		for (const TCHAR* Candidate : Candidates)
		{
			if (UClass* Found = LoadClass<ACharacter>(nullptr, Candidate, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** A mark, or a prop with that name, the way Previs looks for places. */
	bool PlaceExists(UWorld* World, const FString& Place, const FString& SetName)
	{
		if (UCinemafyProductionLibrary::FindMark(World, Place, SetName))
		{
			return true;
		}
		const FString Wanted = Place.ToLower();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString Label = It->GetActorLabel().ToLower().Replace(TEXT("_"), TEXT(" "));
			if (!It->IsA<ACharacter>() && (Label.Contains(Wanted) || (Wanted.Len() > 4 && Wanted.Contains(Label) && Label.Len() >= 4)))
			{
				return true;
			}
		}
		return false;
	}

	bool HasNavMeshBounds(UWorld* World)
	{
		return static_cast<bool>(TActorIterator<ANavMeshBoundsVolume>(World));
	}

	/** A nav mesh bounds volume over everyone and every mark in the level (or in front of the camera). */
	bool AddNavMesh(UWorld* World)
	{
		FBox Area(ForceInit);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<ACharacter>() || It->IsA<ACinemafyMark>())
			{
				Area += It->GetActorLocation();
			}
		}
		if (!Area.IsValid)
		{
			FVector Center = FVector::ZeroVector;
			if (FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient)
			{
				Center = Client->GetViewLocation() + Client->GetViewRotation().Vector() * 1000.f;
			}
			Area = FBox::BuildAABB(Center, FVector(1000.f, 1000.f, 200.f));
		}
		Area = Area.ExpandBy(FVector(600.f, 600.f, 300.f));
		const FScopedTransaction Transaction(LOCTEXT("AddNavMesh", "Cinemafy: Add Nav Mesh"));
		FActorSpawnParameters Params;
		ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(Area.GetCenter(), FRotator::ZeroRotator, Params);
		if (!Volume)
		{
			return false;
		}
		UCubeBuilder* Builder = NewObject<UCubeBuilder>();
		const FVector Size = Area.GetSize();
		Builder->X = static_cast<float>(Size.X);
		Builder->Y = static_cast<float>(Size.Y);
		Builder->Z = static_cast<float>(FMath::Max(Size.Z, 600.0));
		UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
		Volume->SetActorLabel(TEXT("NavMeshBounds_Cinemafy"));
		Volume->PostEditChange();
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			Nav->OnNavigationBoundsUpdated(Volume);
		}
		return true;
	}

	void SortWorstFirst(TArray<FCinemafyCheck>& Checks)
	{
		Checks.StableSort([](const FCinemafyCheck& A, const FCinemafyCheck& B) { return A.Severity > B.Severity; });
	}
}

TArray<FCinemafyCheck> UCinemafyPreflightLibrary::CheckScene(UCinemafyScriptProject* Project, const FString& SceneId)
{
	using namespace CinemafyPreflight;
	TArray<FCinemafyCheck> Checks;
	UWorld* World = EditorWorld();
	const FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!World || !Scene)
	{
		Checks.Add(MakeCheck(SceneId, ECinemafyCheckSeverity::Blocker, TEXT("No such scene, or no level open.")));
		return Checks;
	}
	const FString Id = Scene->Id;
	if (GEditor->PlayWorld)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Blocker, TEXT("A play session is running: stop it first.")));
	}
	if (Scene->Status == ECinemafySceneStatus::Cut)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, TEXT("This scene is marked Cut.")));
	}

	// Cast & Sets, and the right level.
	if (Project->CastMembers.Num() == 0)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, TEXT("No cast yet: Previs will build Cast & Sets first, matching characters to actors by name."),
			ECinemafyFix::BuildCastAndSets, FString(), TEXT("Build Cast & Sets")));
	}
	const FCinemafySet* Set = Project->FindSet(Scene->Location);
	if (!Set && !Scene->Location.IsEmpty() && Project->CastMembers.Num() > 0)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, FString::Printf(TEXT("No set for %s yet."), *Scene->Location),
			ECinemafyFix::BuildCastAndSets, FString(), TEXT("Build Cast & Sets")));
	}
	else if (Set && !Set->Level.IsNull() && Set->Level.ToSoftObjectPath() != FSoftObjectPath(World))
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("%s is set in %s, but %s is open."), *Scene->Location, *Set->Level.GetAssetName(), *World->GetMapName()),
			ECinemafyFix::OpenLevel, Set->Level.ToSoftObjectPath().GetLongPackageName(), FString::Printf(TEXT("Open %s"), *Set->Level.GetAssetName())));
	}

	// Who's in it, and where they go.
	TArray<FString> NotStaged;
	const TArray<FCinemafyStagedBeat> Staged = UCinemafyPrevisLibrary::StageScene(*Scene, NotStaged);
	TArray<FString> Characters;
	TArray<FString> Places;
	bool bMoves = false;
	for (const FCinemafyStagedBeat& Beat : Staged)
	{
		Characters.AddUnique(Beat.Character);
		if (!Beat.Target.IsEmpty())
		{
			Characters.AddUnique(Beat.Target);
			bMoves = true;
		}
		if (!Beat.Place.IsEmpty())
		{
			Places.AddUnique(Beat.Place);
			bMoves = true;
		}
	}
	UClass* Template = nullptr;
	bool bLookedForTemplate = false;
	int32 ToSpawn = 0;
	for (const FString& Name : Characters)
	{
		AActor* Actor = UCinemafyStoryLibrary::FindCastActor(Project, Name, World);
		const FCinemafyCastMember* Member = Project->FindCastMember(Name);
		if (Actor)
		{
			if (!Actor->IsA<ACharacter>())
			{
				Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Blocker, FString::Printf(TEXT("%s is played by %s, which isn't a Character: point their cast entry at a character."), *Name, *Actor->GetActorLabel()),
					ECinemafyFix::OpenProject, FString(), TEXT("Open Cast & Sets")));
			}
			continue;
		}
		if (Member && !Member->CharacterClass.IsNull())
		{
			++ToSpawn;
			continue;
		}
		if (!bLookedForTemplate)
		{
			Template = FindTemplateCharacter();
			bLookedForTemplate = true;
		}
		Checks.Add(Template
			? MakeCheck(Id, ECinemafyCheckSeverity::Blocker, FString::Printf(TEXT("%s isn't on set and has no Character Class."), *Name),
				ECinemafyFix::CastTemplateCharacter, Name, FString::Printf(TEXT("Cast as %s"), *Template->GetName()))
			: MakeCheck(Id, ECinemafyCheckSeverity::Blocker, FString::Printf(TEXT("%s isn't on set and has no Character Class: place them in the level (labelled %s), or give their cast entry a Character Class."), *Name, *Name),
				ECinemafyFix::OpenProject, FString(), TEXT("Open Cast & Sets")));
	}
	if (ToSpawn > 0)
	{
		int32 StartMarks = 0;
		for (TActorIterator<ACinemafyMark> It(World); It; ++It)
		{
			StartMarks += It->bStartMark && (It->SetName.IsEmpty() || It->SetName.Equals(Scene->Location, ESearchCase::IgnoreCase)) ? 1 : 0;
		}
		if (StartMarks < ToSpawn)
		{
			Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("%d character(s) will be brought in, but there are %d start mark(s): the rest line up at the world origin."), ToSpawn, StartMarks),
				ECinemafyFix::PlaceMark, TEXT("start"), TEXT("Place a start mark")));
		}
	}
	// Set pieces the script mentions that the level doesn't have yet: one fix dresses them all.
	TArray<FString> Undressed;
	for (const FCinemafySetPiece& Piece : UCinemafyPrevisLibrary::FindSetDressing(*Scene))
	{
		if (Piece.Kind == ECinemafyDressingKind::SetPiece && !PlaceExists(World, Piece.Name, Scene->Location))
		{
			Undressed.Add(Piece.Name);
		}
	}
	if (Undressed.Num() > 0)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("The script mentions set pieces the level doesn't have: %s."), *FString::Join(Undressed, TEXT(", "))),
			ECinemafyFix::DressSet, FString(), TEXT("Dress the set")));
	}
	for (const FString& Place : Places)
	{
		if (!PlaceExists(World, Place, Scene->Location) && !Undressed.Contains(Place))
		{
			Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("No mark or prop for \"%s\": whoever goes there will stay put."), *Place),
				ECinemafyFix::PlaceMark, Place, FString::Printf(TEXT("Place \"%s\""), *Place)));
		}
	}
	if (bMoves && !HasNavMeshBounds(World))
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, TEXT("No nav mesh in the level: moves can't path around things."),
			ECinemafyFix::AddNavMesh, FString(), TEXT("Add a Nav Mesh")));
	}

	// Voices.
	int32 Silent = 0;
	int32 Rerecord = 0;
	for (const FCinemafyScriptLine& Line : Scene->Lines)
	{
		Silent += Line.Kind == ECinemafyScriptLineKind::Dialogue && Line.Audio.IsNull() ? 1 : 0;
		Rerecord += Line.bNeedsRerecord ? 1 : 0;
	}
	if (Silent > 0)
	{
		Checks.Add(HasSpeechEngine()
			? MakeCheck(Id, ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("%d line(s) have no voice: their timing is guessed and faces won't talk."), Silent),
				ECinemafyFix::ScratchVoices, FString(), TEXT("Scratch voices"))
			: MakeCheck(Id, ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d line(s) have no voice, and there's no speech engine for scratch reads (install espeak-ng)."), Silent)));
	}
	if (Rerecord > 0)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d line(s) were rewritten after recording: they need re-recording (see Paperwork)."), Rerecord)));
	}
	if (UCinemafyStoryLibrary::IsLipSyncAvailable())
	{
		const int32 Faceless = UCinemafyStoryLibrary::GetLipSyncJobs(Project, Id, false).Num();
		if (Faceless > 0)
		{
			Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d voiced line(s) could have real MetaHuman lip-sync (they use the Talk curves now)."), Faceless),
				ECinemafyFix::LipSync, FString(), TEXT("Lip-sync")));
		}
	}

	// The pages, and what the script asks for that Previs can't do.
	if (Scene->IsOutOfDate())
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Warning, TEXT("The script changed since this scene was staged."),
			ECinemafyFix::UpdateChanged, FString(), TEXT("Update changed scenes")));
	}
	for (int32 Index = 0; Index < NotStaged.Num() && Index < 6; ++Index)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, FString::Printf(TEXT("Won't be staged (stage it by hand): \"%s\""), *NotStaged[Index])));
	}
	for (const FString& Warning : Scene->Warnings)
	{
		Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, TEXT("Continuity: ") + Warning));
	}
	for (const FCinemafyReviewNote& Note : Scene->ReviewNotes)
	{
		if (!Note.bResolved)
		{
			Checks.Add(MakeCheck(Id, ECinemafyCheckSeverity::Info, FString::Printf(TEXT("Review note (%s): %s"), *Note.Author, *Note.Text)));
		}
	}
	SortWorstFirst(Checks);
	return Checks;
}

TArray<FCinemafyCheck> UCinemafyPreflightLibrary::CheckProject(UCinemafyScriptProject* Project)
{
	using namespace CinemafyPreflight;
	TArray<FCinemafyCheck> Checks;
	UWorld* World = EditorWorld();
	if (!Project || !World)
	{
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Blocker, TEXT("No script project, or no level open.")));
		return Checks;
	}
	if (Project->CastMembers.Num() == 0)
	{
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Warning, TEXT("No cast or sets yet."), ECinemafyFix::BuildCastAndSets, FString(), TEXT("Build Cast & Sets")));
	}
	TArray<FString> Uncast;
	for (const FCinemafyCastMember& Member : Project->CastMembers)
	{
		if (Member.CharacterClass.IsNull() && !UCinemafyStoryLibrary::FindCastActor(Project, Member.ScriptName, World))
		{
			Uncast.Add(Member.ScriptName);
		}
	}
	if (Uncast.Num() > 0)
	{
		UClass* Template = FindTemplateCharacter();
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Warning,
			FString::Printf(TEXT("Not in this level and no Character Class: %s."), *FString::Join(Uncast, TEXT(", "))),
			Template ? ECinemafyFix::CastTemplateCharacter : ECinemafyFix::OpenProject, FString(),
			Template ? FString::Printf(TEXT("Cast them as %s"), *Template->GetName()) : FString(TEXT("Open Cast & Sets"))));
	}
	int32 Silent = 0, Rerecord = 0, Stale = 0, Notes = 0, Warnings = 0;
	for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		if (Scene->Status == ECinemafySceneStatus::Cut)
		{
			continue;
		}
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			Silent += Line.Kind == ECinemafyScriptLineKind::Dialogue && Line.Audio.IsNull() ? 1 : 0;
			Rerecord += Line.bNeedsRerecord ? 1 : 0;
		}
		Stale += Scene->IsOutOfDate() ? 1 : 0;
		Notes += Scene->ReviewNotes.FilterByPredicate([](const FCinemafyReviewNote& Note) { return !Note.bResolved; }).Num();
		Warnings += Scene->Warnings.Num();

		// Scenes staged in this level get the full check (its warnings and blockers).
		if (Scene->Level.ToSoftObjectPath() == FSoftObjectPath(World))
		{
			for (const FCinemafyCheck& Check : CheckScene(Project, Scene->Id))
			{
				if (Check.Severity != ECinemafyCheckSeverity::Info && Check.Fix != ECinemafyFix::UpdateChanged && Check.Fix != ECinemafyFix::ScratchVoices)
				{
					FCinemafyCheck Copy = Check;
					Copy.Problem = FString::Printf(TEXT("%s: %s"), *Scene->Id, *Check.Problem);
					Checks.Add(Copy);
				}
			}
		}
	}
	if (Silent > 0)
	{
		Checks.Add(HasSpeechEngine()
			? MakeCheck(FString(), ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("%d line(s) have no voice yet."), Silent), ECinemafyFix::ScratchVoices, FString(), TEXT("Scratch voices"))
			: MakeCheck(FString(), ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d line(s) have no voice yet (no speech engine for scratch reads: install espeak-ng)."), Silent)));
	}
	if (Rerecord > 0)
	{
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d line(s) need re-recording after rewrites."), Rerecord)));
	}
	if (Stale > 0)
	{
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Warning, FString::Printf(TEXT("%d scene(s) were staged from older pages."), Stale), ECinemafyFix::UpdateChanged, FString(), TEXT("Update changed scenes")));
	}
	if (Notes > 0)
	{
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d open review note(s)."), Notes)));
	}
	if (Warnings > 0)
	{
		Checks.Add(MakeCheck(FString(), ECinemafyCheckSeverity::Info, FString::Printf(TEXT("%d continuity warning(s): see the checklist."), Warnings)));
	}
	SortWorstFirst(Checks);
	return Checks;
}

bool UCinemafyPreflightLibrary::ApplyFix(UCinemafyScriptProject* Project, const FCinemafyCheck& Check)
{
	using namespace CinemafyPreflight;
	UWorld* World = EditorWorld();
	if (!Project || !World)
	{
		return false;
	}
	const FCinemafyScriptScene* Scene = Project->FindScene(Check.SceneId);
	switch (Check.Fix)
	{
	case ECinemafyFix::BuildCastAndSets:
		UCinemafyStoryLibrary::BuildCastAndSets(Project);
		return true;
	case ECinemafyFix::OpenProject:
		return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Project);
	case ECinemafyFix::OpenLevel:
	{
		if (!FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave*/ true, /*bSaveMapPackages*/ true, /*bSaveContentPackages*/ false))
		{
			return false;
		}
		FString File;
		if (!FPackageName::TryConvertLongPackageNameToFilename(Check.FixArg, File, FPackageName::GetMapPackageExtension()))
		{
			return false;
		}
		FEditorFileUtils::LoadMap(File);
		return true;
	}
	case ECinemafyFix::PlaceMark:
	{
		const bool bStart = Check.FixArg == TEXT("start");
		ACinemafyMark* Mark = UCinemafyStoryLibrary::PlaceMark(Check.FixArg, Scene ? Scene->Location : FString());
		if (!Mark)
		{
			return false;
		}
		// Side by side, not stacked, when several are placed in a row.
		for (int32 Step = 0; Step < 20; ++Step)
		{
			bool bCrowded = false;
			for (TActorIterator<ACinemafyMark> It(World); It; ++It)
			{
				bCrowded |= *It != Mark && FVector::Dist2D(It->GetActorLocation(), Mark->GetActorLocation()) < 80.f;
			}
			if (!bCrowded)
			{
				break;
			}
			Mark->SetActorLocation(Mark->GetActorLocation() + FVector(0.f, 150.f, 0.f));
		}
		if (bStart)
		{
			Mark->bStartMark = true;
			Mark->RerunConstructionScripts();
		}
		return true;
	}
	case ECinemafyFix::AddNavMesh:
		return HasNavMeshBounds(World) || AddNavMesh(World);
	case ECinemafyFix::ScratchVoices:
		return UCinemafyStoryLibrary::GenerateScratchVoices(Project, Check.SceneId, false) > 0;
	case ECinemafyFix::LipSync:
		UE_LOG(LogCinemafy, Log, TEXT("%s"), *UCinemafyStoryLibrary::GenerateLipSync(Project, Check.SceneId, false));
		return true;
	case ECinemafyFix::DressSet:
		UCinemafyStoryLibrary::DressSet(Project, Scene ? Scene->Id : Check.FixArg);
		return true;
	case ECinemafyFix::UpdateChanged:
		UCinemafyStoryLibrary::UpdateChangedScenes(Project);
		return true;
	case ECinemafyFix::CastTemplateCharacter:
	{
		UClass* Template = FindTemplateCharacter();
		if (!Template)
		{
			return false;
		}
		const FScopedTransaction Transaction(LOCTEXT("CastTemplate", "Cinemafy: Cast Template Character"));
		Project->Modify();
		int32 Given = 0;
		for (FCinemafyCastMember& Member : Project->CastMembers)
		{
			const bool bWanted = Check.FixArg.IsEmpty() || Member.ScriptName.Equals(Check.FixArg, ESearchCase::IgnoreCase);
			if (bWanted && Member.CharacterClass.IsNull() && !UCinemafyStoryLibrary::FindCastActor(Project, Member.ScriptName, World))
			{
				Member.CharacterClass = TSoftClassPtr<ACharacter>(Template);
				++Given;
			}
		}
		Project->MarkPackageDirty();
		return Given > 0;
	}
	default:
		return false;
	}
}

int32 UCinemafyPreflightLibrary::FixAll(UCinemafyScriptProject* Project, const TArray<FCinemafyCheck>& Checks)
{
	int32 Fixed = 0;
	bool bVoiced = false;
	bool bUpdated = false;
	for (const FCinemafyCheck& Check : Checks)
	{
		// Opening another level is a decision; the rest is safe to do in one go (once each for the big ones).
		if (Check.Fix == ECinemafyFix::None || Check.Fix == ECinemafyFix::OpenLevel || Check.Fix == ECinemafyFix::OpenProject
			|| (Check.Fix == ECinemafyFix::ScratchVoices && bVoiced) || (Check.Fix == ECinemafyFix::UpdateChanged && bUpdated))
		{
			continue;
		}
		bVoiced |= Check.Fix == ECinemafyFix::ScratchVoices && Check.SceneId.IsEmpty();
		bUpdated |= Check.Fix == ECinemafyFix::UpdateChanged;
		Fixed += ApplyFix(Project, Check) ? 1 : 0;
	}
	return Fixed;
}

FString UCinemafyPreflightLibrary::FormatChecks(const TArray<FCinemafyCheck>& Checks)
{
	if (Checks.Num() == 0)
	{
		return TEXT("All clear.");
	}
	TArray<FString> Lines;
	const UEnum* Fixes = StaticEnum<ECinemafyFix>();
	for (const FCinemafyCheck& Check : Checks)
	{
		const TCHAR* Tag = Check.Severity == ECinemafyCheckSeverity::Blocker ? TEXT("BLOCKER") : Check.Severity == ECinemafyCheckSeverity::Warning ? TEXT("warning") : TEXT("info");
		FString Line = FString::Printf(TEXT("[%s] %s"), Tag, *Check.Problem);
		if (Check.Fix != ECinemafyFix::None)
		{
			Line += FString::Printf(TEXT(" (fix: %s)"), *Fixes->GetNameStringByValue(static_cast<int64>(Check.Fix)));
		}
		Lines.Add(Line);
	}
	return FString::Join(Lines, TEXT("\n"));
}

bool UCinemafyPreflightLibrary::ShowPreflight(UCinemafyScriptProject* Project, const FString& SceneId, const FText& Title, const FText& WhatHappens, const FText& GoLabel)
{
	if (!Project || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	struct FState
	{
		bool bGo = false;
		FCinemafyCheck PendingLevel;
		TArray<FCinemafyCheck> Checks;
		TSharedPtr<SVerticalBox> List;
		TFunction<void()> Refill;
	};
	TSharedRef<FState> State = MakeShared<FState>();
	TWeakObjectPtr<UCinemafyScriptProject> WeakProject = Project;
	auto Run = [WeakProject, SceneId]()
	{
		UCinemafyScriptProject* P = WeakProject.Get();
		return SceneId.IsEmpty() ? UCinemafyPreflightLibrary::CheckProject(P) : UCinemafyPreflightLibrary::CheckScene(P, SceneId);
	};

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(640.f, 480.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);
	TWeakPtr<SWindow> WeakWindow = Window;
	TWeakPtr<FState> WeakState = State;

	State->Refill = [WeakState, WeakProject, WeakWindow, Run]()
	{
		TSharedPtr<FState> S = WeakState.Pin();
		if (!S.IsValid() || !S->List.IsValid())
		{
			return;
		}
		S->Checks = Run();
		S->List->ClearChildren();
		if (S->Checks.Num() == 0)
		{
			S->List->AddSlot().AutoHeight().Padding(4.f)
			[
				SNew(STextBlock).Text(LOCTEXT("AllClear", "All clear: nothing missing.")).ColorAndOpacity(FLinearColor(0.4f, 0.9f, 0.4f))
			];
		}
		for (const FCinemafyCheck& Check : S->Checks)
		{
			const FLinearColor Color = Check.Severity == ECinemafyCheckSeverity::Blocker ? FLinearColor(1.f, 0.3f, 0.25f)
				: Check.Severity == ECinemafyCheckSeverity::Warning ? FLinearColor(1.f, 0.7f, 0.2f) : FLinearColor(0.6f, 0.75f, 0.9f);
			const FText Tag = Check.Severity == ECinemafyCheckSeverity::Blocker ? LOCTEXT("Blocker", "Blocker")
				: Check.Severity == ECinemafyCheckSeverity::Warning ? LOCTEXT("Warning", "Warning") : LOCTEXT("Info", "Info");
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SBox).WidthOverride(64.f)
					[
						SNew(STextBlock).Text(Tag).ColorAndOpacity(Color).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Check.Problem)).AutoWrapText(true)
				];
			if (Check.Fix != ECinemafyFix::None)
			{
				const FCinemafyCheck Copy = Check;
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(FText::FromString(Check.FixLabel.IsEmpty() ? FString(TEXT("Fix")) : Check.FixLabel))
					.OnClicked_Lambda([WeakState, WeakProject, WeakWindow, Copy]()
					{
						TSharedPtr<FState> Inner = WeakState.Pin();
						if (!Inner.IsValid())
						{
							return FReply::Handled();
						}
						if (Copy.Fix == ECinemafyFix::OpenLevel)
						{
							// Levels change after the window closes.
							Inner->PendingLevel = Copy;
							if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
							{
								Pinned->RequestDestroyWindow();
							}
							return FReply::Handled();
						}
						UCinemafyPreflightLibrary::ApplyFix(WeakProject.Get(), Copy);
						Inner->Refill();
						return FReply::Handled();
					})
				];
			}
			S->List->AddSlot().AutoHeight().Padding(4.f, 3.f)[ Row ];
		}
	};

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("FixAll", "Fix All"))
			.ToolTipText(LOCTEXT("FixAllTip", "Run every fix that needs no decision (everything but opening another level)."))
			.OnClicked_Lambda([WeakState, WeakProject]()
			{
				if (TSharedPtr<FState> S = WeakState.Pin())
				{
					UCinemafyPreflightLibrary::FixAll(WeakProject.Get(), S->Checks);
					S->Refill();
				}
				return FReply::Handled();
			})
		];
	if (!GoLabel.IsEmpty())
	{
		Buttons->AddSlot().AutoWidth().Padding(4.f, 0.f)
		[
			SNew(SButton)
			.Text(GoLabel)
			.OnClicked_Lambda([WeakState, WeakWindow]()
			{
				if (TSharedPtr<FState> S = WeakState.Pin())
				{
					S->bGo = true;
				}
				if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
				{
					Pinned->RequestDestroyWindow();
				}
				return FReply::Handled();
			})
		];
	}
	Buttons->AddSlot().AutoWidth().Padding(4.f, 0.f)
	[
		SNew(SButton)
		.Text(GoLabel.IsEmpty() ? LOCTEXT("Close", "Close") : LOCTEXT("Cancel", "Cancel"))
		.OnClicked_Lambda([WeakWindow]()
		{
			if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
			{
				Pinned->RequestDestroyWindow();
			}
			return FReply::Handled();
		})
	];

	Window->SetContent(
		SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(12.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(WhatHappens).AutoWrapText(true).Visibility(WhatHappens.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[ SAssignNew(State->List, SVerticalBox) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				Buttons
			]
		]);
	State->Refill();
	GEditor->EditorAddModalWindow(Window);

	if (State->PendingLevel.Fix == ECinemafyFix::OpenLevel)
	{
		ApplyFix(WeakProject.Get(), State->PendingLevel);
		return false;
	}
	return State->bGo;
}

void UCinemafyPreflightLibrary::PrevisWithPreflight(UCinemafyScriptProject* Project, const FString& SceneId, bool bRecord)
{
	UWorld* World = CinemafyPreflight::EditorWorld();
	const FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!World || !Scene)
	{
		return;
	}
	// What it's about to change in the level, in plain words.
	const FString Prefix = FString::Printf(TEXT("Previs_%s_"), *Scene->Id.Replace(TEXT("."), TEXT("_")));
	int32 Replaced = 0;
	int32 SitOut = 0;
	for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
	{
		if (It->GetActorLabel().StartsWith(Prefix))
		{
			++Replaced;
		}
		else if (It->bIncludeInTakes)
		{
			++SitOut;
		}
	}
	TArray<FString> Changes;
	Changes.Add(FString::Printf(TEXT("Stage %s %s in %s from the script (cast, continuity, a choreography per character, the scene's sequence, director and mood)."),
		*Scene->Id, *Scene->Heading, *World->GetMapName()));
	if (Replaced > 0)
	{
		Changes.Add(FString::Printf(TEXT("Replace this scene's %d previs choreograph(ies); any you edited by hand are kept as Edited_..."), Replaced));
	}
	if (SitOut > 0)
	{
		Changes.Add(FString::Printf(TEXT("Turn off Include In Takes on %d other choreograph(ies) in the level."), SitOut));
	}
	if (bRecord)
	{
		Changes.Add(TEXT("Then play in the editor to perform and record the take, and cut it (play stops by itself)."));
	}
	Changes.Add(TEXT("Everything can be undone with Ctrl+Z, except the recording."));
	const FText Title = FText::Format(LOCTEXT("PrevisTitle", "Previs {0}"), FText::FromString(Scene->Id));
	if (!ShowPreflight(Project, SceneId, Title, FText::FromString(TEXT("This will:\n- ") + FString::Join(Changes, TEXT("\n- "))),
		bRecord ? LOCTEXT("GoPrevis", "Previs & Record") : LOCTEXT("GoStage", "Stage")))
	{
		return;
	}
	const FString Report = UCinemafyStoryLibrary::PrevisScene(Project, SceneId, bRecord);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report.Left(3000)));
}

#undef LOCTEXT_NAMESPACE
