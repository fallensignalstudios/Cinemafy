#include "CinemafyStoryLibrary.h"
#include "CinemafyModule.h"
#include "CinemafyPrevis.h"
#include "CinemafyProduction.h"
#include "CinemafyScript.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "CinemafyStoryDressing"

namespace CinemafySetDressing
{
	/** A believable size (cm) and shape for a placeholder, from what it's called. */
	struct FBlock { FVector Size; bool bRound; };

	FBlock BlockFor(const FString& Name)
	{
		struct FRule { const TCHAR* Word; FVector Size; bool bRound; };
		static const FRule Rules[] = {
			{ TEXT("console"), FVector(160.f, 80.f, 105.f), false }, { TEXT("terminal"), FVector(80.f, 60.f, 140.f), false },
			{ TEXT("panel"), FVector(120.f, 30.f, 160.f), false }, { TEXT("desk"), FVector(160.f, 80.f, 75.f), false },
			{ TEXT("table"), FVector(180.f, 90.f, 75.f), false }, { TEXT("workbench"), FVector(200.f, 80.f, 90.f), false },
			{ TEXT("crate"), FVector(100.f, 160.f, 100.f), false }, { TEXT("box"), FVector(60.f, 60.f, 50.f), false },
			{ TEXT("container"), FVector(250.f, 600.f, 260.f), false }, { TEXT("chair"), FVector(60.f, 60.f, 110.f), false },
			{ TEXT("seat"), FVector(60.f, 60.f, 100.f), false }, { TEXT("throne"), FVector(100.f, 90.f, 180.f), false },
			{ TEXT("stool"), FVector(40.f, 40.f, 70.f), true }, { TEXT("rack"), FVector(180.f, 50.f, 200.f), false },
			{ TEXT("shelf"), FVector(150.f, 40.f, 200.f), false }, { TEXT("locker"), FVector(60.f, 50.f, 200.f), false },
			{ TEXT("cabinet"), FVector(100.f, 50.f, 180.f), false }, { TEXT("hatch"), FVector(120.f, 20.f, 200.f), false },
			{ TEXT("lift"), FVector(250.f, 250.f, 10.f), false }, { TEXT("elevator"), FVector(250.f, 250.f, 10.f), false },
			{ TEXT("platform"), FVector(400.f, 400.f, 30.f), false }, { TEXT("railing"), FVector(400.f, 10.f, 105.f), false },
			{ TEXT("rail"), FVector(400.f, 10.f, 105.f), false }, { TEXT("barrier"), FVector(300.f, 40.f, 110.f), false },
			{ TEXT("fence"), FVector(400.f, 10.f, 180.f), false }, { TEXT("wall"), FVector(400.f, 30.f, 300.f), false },
			{ TEXT("bed"), FVector(200.f, 100.f, 60.f), false }, { TEXT("cot"), FVector(190.f, 80.f, 50.f), false },
			{ TEXT("stretcher"), FVector(200.f, 70.f, 90.f), false }, { TEXT("window"), FVector(200.f, 15.f, 150.f), false },
			{ TEXT("screen"), FVector(250.f, 15.f, 150.f), false }, { TEXT("pillar"), FVector(70.f, 70.f, 400.f), true },
			{ TEXT("column"), FVector(70.f, 70.f, 400.f), true }, { TEXT("barrel"), FVector(60.f, 60.f, 90.f), true },
			{ TEXT("drum"), FVector(60.f, 60.f, 90.f), true }, { TEXT("canister"), FVector(40.f, 40.f, 80.f), true },
			{ TEXT("tank"), FVector(150.f, 150.f, 250.f), true }, { TEXT("statue"), FVector(80.f, 80.f, 220.f), false },
			{ TEXT("car"), FVector(450.f, 190.f, 150.f), false }, { TEXT("truck"), FVector(700.f, 250.f, 300.f), false },
			{ TEXT("shuttle"), FVector(1200.f, 800.f, 400.f), false }, { TEXT("ship"), FVector(1500.f, 900.f, 500.f), false },
			{ TEXT("tree"), FVector(80.f, 80.f, 600.f), true }, { TEXT("rock"), FVector(150.f, 150.f, 100.f), false },
			{ TEXT("bar"), FVector(400.f, 70.f, 110.f), false }, { TEXT("counter"), FVector(300.f, 70.f, 100.f), false },
			{ TEXT("sofa"), FVector(220.f, 90.f, 85.f), false }, { TEXT("couch"), FVector(220.f, 90.f, 85.f), false },
			{ TEXT("altar"), FVector(200.f, 100.f, 110.f), false }, { TEXT("generator"), FVector(200.f, 120.f, 160.f), false } };
		TArray<FString> Words;
		Name.ToLower().ParseIntoArrayWS(Words);
		// The last word names the thing ("weapons rack" is a rack).
		for (int32 Index = Words.Num() - 1; Index >= 0; --Index)
		{
			for (const FRule& Rule : Rules)
			{
				if (Words[Index].StartsWith(Rule.Word))
				{
					return { Rule.Size, Rule.bRound };
				}
			}
		}
		return { FVector(100.f, 100.f, 100.f), false };
	}

	/** Something in the level already goes by this name (as Previs would find it). */
	AActor* FindInLevel(UWorld* World, const FString& Name)
	{
		const FString Wanted = Name.ToLower();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<APawn>() || It->IsA<ACinemafyMark>())
			{
				continue;
			}
			const FString Label = It->GetActorLabel().ToLower().Replace(TEXT("_"), TEXT(" "));
			if (Label.Contains(Wanted))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The floor under a point (or the point itself). */
	float FloorAt(UWorld* World, const FVector& Point)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyDressFloor), true);
		return World->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 400.f), Point - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params)
			? Hit.ImpactPoint.Z : Point.Z;
	}
}

FString UCinemafyStoryLibrary::DressSet(UCinemafyScriptProject* Project, const FString& SceneIdOrLocation)
{
	using namespace CinemafySetDressing;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!Project || !World)
	{
		return TEXT("No script project, or no level open.");
	}
	const FCinemafyScriptScene* Scene = Project->FindScene(SceneIdOrLocation);
	const FString Location = Scene ? Scene->Location : SceneIdOrLocation;
	BuildCastAndSets(Project);
	FCinemafySet* Set = Project->Sets.FindByPredicate([&Location](const FCinemafySet& S) { return S.ScriptLocation.Equals(Location, ESearchCase::IgnoreCase); });
	if (!Set)
	{
		return FString::Printf(TEXT("No set called %s in the script."), *Location);
	}

	const FScopedTransaction Transaction(LOCTEXT("DressSet", "Cinemafy: Dress the Set"));
	TArray<FString> Report;

	// Where to lay things out: around the set's marks, else where the camera looks.
	FVector Anchor = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	int32 MarkCount = 0;
	for (TActorIterator<ACinemafyMark> It(World); It; ++It)
	{
		if (It->SetName.IsEmpty() || It->SetName.Equals(Location, ESearchCase::IgnoreCase))
		{
			Anchor += It->GetActorLocation();
			++MarkCount;
		}
	}
	if (FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient)
	{
		Forward = Client->GetViewRotation().Vector().GetSafeNormal2D();
		if (MarkCount == 0)
		{
			const FVector Eye = Client->GetViewLocation();
			FHitResult Hit;
			Anchor = World->LineTraceSingleByChannel(Hit, Eye, Eye + Client->GetViewRotation().Vector() * 5000.f, ECC_Visibility) ? Hit.ImpactPoint : Eye + Forward * 1200.f;
		}
	}
	Anchor = MarkCount > 0 ? Anchor / MarkCount : Anchor;
	Forward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Forward);

	TArray<FString> Placed, Already, Props, Wardrobe;
	float RowRight = 0.f, RowLeft = 0.f;
	for (const FCinemafySetPiece& Piece : Set->Dressing)
	{
		if (Piece.Kind == ECinemafyDressingKind::HandProp)
		{
			Props.Add(Piece.UsedBy.Num() > 0 ? FString::Printf(TEXT("%s (%s)"), *Piece.Name, *FString::Join(Piece.UsedBy, TEXT(", "))) : Piece.Name);
			continue;
		}
		if (Piece.Kind == ECinemafyDressingKind::Wardrobe)
		{
			Wardrobe.Add(Piece.UsedBy.Num() > 0 ? FString::Printf(TEXT("%s (%s)"), *Piece.Name, *FString::Join(Piece.UsedBy, TEXT(", "))) : Piece.Name);
			continue;
		}
		if (FindInLevel(World, Piece.Name))
		{
			Already.Add(Piece.Name);
			continue;
		}

		const FBlock Block = BlockFor(Piece.Name);
		UStaticMesh* Mesh = Piece.Mesh.LoadSynchronous();
		const bool bPlaceholder = Mesh == nullptr;
		if (bPlaceholder)
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, Block.bRound ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		const FVector Size = bPlaceholder ? Block.Size : (Mesh ? Mesh->GetBounds().BoxExtent * 2.f : Block.Size);

		// In front of its mark (they stand at the mark, facing it), else the next spot in a row across the set.
		FVector Where;
		FRotator Facing;
		if (ACinemafyMark* Mark = UCinemafyProductionLibrary::FindMark(World, Piece.Name, Location))
		{
			const FVector MarkForward = Mark->GetActorForwardVector().GetSafeNormal2D();
			Where = Mark->GetActorLocation() + MarkForward * (Size.Y * 0.5f + 70.f);
			Facing = (-MarkForward).Rotation();
		}
		else
		{
			const float Width = FMath::Max(Size.X, Size.Y);
			const bool bRight = RowRight <= RowLeft;
			float& Row = bRight ? RowRight : RowLeft;
			const float Offset = (Row + Width * 0.5f + 75.f) * (bRight ? 1.f : -1.f);
			Row += Width + 150.f;
			Where = Anchor + Forward * 500.f + Across * Offset;
			Facing = (-Forward).Rotation();
		}
		Facing.Pitch = 0.f;
		Facing.Roll = 0.f;
		const float Floor = FloorAt(World, Where);
		const float Lift = bPlaceholder ? Size.Z * 0.5f : (Mesh ? Mesh->GetBounds().BoxExtent.Z - Mesh->GetBounds().Origin.Z : 0.f);
		Where.Z = Floor + Lift;

		FActorSpawnParameters Params;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Where, Facing, Params);
		if (!Actor)
		{
			continue;
		}
		Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		if (bPlaceholder)
		{
			Actor->SetActorScale3D(Size / 100.f);
		}
		Actor->SetActorLabel(TEXT("Set_") + Piece.Name.Replace(TEXT(" "), TEXT("_")));
		Actor->Tags.AddUnique(FName(TEXT("Cinemafy.Dressing")));
		Actor->SetFolderPath(*FString::Printf(TEXT("Cinemafy/Dressing/%s"), *Location.Replace(TEXT("/"), TEXT("-"))));
		Placed.Add(bPlaceholder ? Piece.Name : Piece.Name + TEXT(" (art)"));
	}

	if (Set->Level.IsNull())
	{
		Project->Modify();
		Set->Level = World;
	}
	Report.Add(FString::Printf(TEXT("Dressed %s: placed %d, %d already there."), *Location, Placed.Num(), Already.Num()));
	if (Placed.Num() > 0)
	{
		Report.Add(TEXT("  Placed: ") + FString::Join(Placed, TEXT(", ")) + TEXT(". Move them where they belong; give the set's Dressing entries a Mesh to swap placeholders for art."));
	}
	if (Props.Num() > 0)
	{
		Report.Add(TEXT("  Hand props to source: ") + FString::Join(Props, TEXT(", ")) + TEXT(" (attach them with cast Looks)."));
	}
	if (Wardrobe.Num() > 0)
	{
		Report.Add(TEXT("  Wardrobe: ") + FString::Join(Wardrobe, TEXT(", ")) + TEXT("."));
	}
	if (Set->Dressing.Num() == 0)
	{
		Report.Add(TEXT("  The script doesn't mention any set pieces here."));
	}
	const FString Text = FString::Join(Report, TEXT("\n"));
	UE_LOG(LogCinemafy, Log, TEXT("%s"), *Text);
	return Text;
}

#undef LOCTEXT_NAMESPACE
