#include "CinemafyExtras.h"
#include "CinemafyChoreographyLibrary.h"
#include "CinemafyModule.h"
#include "CinemafyProduction.h"
#include "CinemafySettings.h"
#include "AIController.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

ACinemafyExtras::ACinemafyExtras()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
	}
#endif
}

ECinemafyExtrasBehavior ACinemafyExtras::ReadDirection(const FString& InDirection, FString& OutPlace)
{
	const FString Text = InDirection.ToLower();
	OutPlace.Reset();
	// The place: whatever follows "at" / "by" / "on" / "near" ("works at consoles" -> "console").
	for (const TCHAR* Preposition : { TEXT(" at "), TEXT(" by "), TEXT(" near "), TEXT(" on "), TEXT(" around ") })
	{
		const int32 At = Text.Find(Preposition);
		if (At != INDEX_NONE)
		{
			FString Rest = Text.Mid(At + FCString::Strlen(Preposition)).TrimStartAndEnd();
			Rest.RemoveFromStart(TEXT("the "));
			Rest.RemoveFromStart(TEXT("their "));
			TArray<FString> Words;
			Rest.ParseIntoArrayWS(Words);
			if (Words.Num() > 0)
			{
				OutPlace = Words[0];
				if (OutPlace.Len() > 3 && OutPlace.EndsWith(TEXT("s")))
				{
					OutPlace.LeftChopInline(1);
				}
			}
			break;
		}
	}
	auto Has = [&Text](std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* Word : Words)
		{
			if (Text.Contains(Word))
			{
				return true;
			}
		}
		return false;
	};
	if (Has({ TEXT("patrol"), TEXT("march"), TEXT("guard the"), TEXT("sweep") }))
	{
		return ECinemafyExtrasBehavior::Patrol;
	}
	if (Has({ TEXT("talk"), TEXT("chat"), TEXT("convers"), TEXT("argue"), TEXT("gossip") }))
	{
		return ECinemafyExtrasBehavior::Talk;
	}
	if (Has({ TEXT("mill"), TEXT("wander"), TEXT("stroll"), TEXT("crowd"), TEXT("bustle") }))
	{
		return ECinemafyExtrasBehavior::Wander;
	}
	if (Has({ TEXT("sit"), TEXT("seated"), TEXT("dine"), TEXT("eat") }))
	{
		return ECinemafyExtrasBehavior::Sit;
	}
	if (Has({ TEXT("work"), TEXT("type"), TEXT("operat"), TEXT("repair"), TEXT("console"), TEXT("station"), TEXT("monitor") }))
	{
		return ECinemafyExtrasBehavior::Work;
	}
	return ECinemafyExtrasBehavior::Idle;
}

FVector ACinemafyExtras::RandomSpot(const FVector& Around, float InRadius) const
{
	UWorld* World = GetWorld();
	if (UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr)
	{
		FNavLocation Spot;
		if (Nav->GetRandomReachablePointInRadius(Around, InRadius, Spot))
		{
			return Spot.Location;
		}
	}
	const FVector2D Offset = FMath::RandPointInCircle(InRadius);
	FVector Spot = Around + FVector(Offset.X, Offset.Y, 0.f);
	FHitResult Hit;
	if (World && World->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 300.f), Spot - FVector(0.f, 0.f, 1000.f), ECC_Visibility))
	{
		Spot = Hit.ImpactPoint;
	}
	return Spot;
}

void ACinemafyExtras::Clear()
{
	for (ACharacter* Extra : Extras)
	{
		if (Extra)
		{
			Extra->Destroy();
		}
	}
	Extras.Reset();
	Loops.Reset();
}

void ACinemafyExtras::Populate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	Modify();
	Clear();

	TArray<UClass*> Classes;
	for (const TSoftClassPtr<ACharacter>& Class : ExtraClasses)
	{
		if (UClass* Loaded = Class.LoadSynchronous())
		{
			Classes.Add(Loaded);
		}
	}
	if (Classes.Num() == 0)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Extras %s: add at least one character class to Extra Classes."), *GetName());
		return;
	}

	FString Place;
	Behavior = ReadDirection(Direction, Place);

	// Stations: marks named like the place ("console"), nearest first.
	TArray<FTransform> Stations;
	if (Behavior == ECinemafyExtrasBehavior::Work || Behavior == ECinemafyExtrasBehavior::Sit || Behavior == ECinemafyExtrasBehavior::Idle)
	{
		for (TActorIterator<ACinemafyMark> It(World); It; ++It)
		{
			const bool bNamed = !Place.IsEmpty() && It->MarkName.Contains(Place, ESearchCase::IgnoreCase);
			if (bNamed && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < Radius * 3.f)
			{
				Stations.Add(It->GetActorTransform());
			}
		}
	}

	// Loops from the animation library that fit the behaviour.
	static const TMap<ECinemafyExtrasBehavior, FString> LoopWords = {
		{ ECinemafyExtrasBehavior::Work, TEXT("work|type|typing|console|interact|repair|operate") },
		{ ECinemafyExtrasBehavior::Talk, TEXT("talk|conversation|chat|gesture|explain") },
		{ ECinemafyExtrasBehavior::Sit, TEXT("sit|seated|sitting") },
		{ ECinemafyExtrasBehavior::Idle, TEXT("idle|stand|guard|wait") } };
	TArray<FCinemafyMoveCandidate> Candidates;
	FCinemafyPromptReading Reading;
	Reading.Kind = ECinemafyActionKind::Traverse; // No locomotion penalty: loops are fine here.
	if (const FString* Words = LoopWords.Find(Behavior))
	{
		Reading.Keywords.Add(*Words);
	}
	TSet<FSoftObjectPath> Used;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FTransform Spot;
		if (Stations.IsValidIndex(Index))
		{
			Spot = Stations[Index];
		}
		else if (Behavior == ECinemafyExtrasBehavior::Talk && Index % 2 == 1 && Extras.Num() > 0 && Extras.Last())
		{
			// The other half of a pair: across from the last one, facing them.
			const FVector Partner = Extras.Last()->GetActorLocation();
			const FVector Here = Partner + Extras.Last()->GetActorForwardVector() * 120.f;
			Spot = FTransform((Partner - Here).GetSafeNormal2D().Rotation(), Here);
		}
		else
		{
			const FVector Location = RandomSpot(GetActorLocation(), Radius);
			Spot = FTransform(FRotator(0.f, FMath::FRandRange(-180.f, 180.f), 0.f), Location);
		}
		UClass* Class = Classes[Index % Classes.Num()];
		const ACharacter* Defaults = Class->GetDefaultObject<ACharacter>();
		const float HalfHeight = Defaults && Defaults->GetCapsuleComponent() ? Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		Spot.AddToTranslation(FVector(0.f, 0.f, HalfHeight));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ACharacter* Extra = World->SpawnActor<ACharacter>(Class, Spot, Params);
		if (!Extra)
		{
			continue;
		}
#if WITH_EDITOR
		Extra->SetActorLabel(FString::Printf(TEXT("Extra_%s_%02d"), *GetActorLabel(), Index + 1));
#endif
		Extra->Tags.AddUnique(TEXT("Cinemafy.Extra"));
		Extra->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
		Extras.Add(Extra);

		UAnimSequenceBase* Loop = nullptr;
		USkeletalMeshComponent* Mesh = Extra->GetMesh();
		if (Reading.Keywords.Num() > 0 && Mesh && Mesh->GetSkeletalMeshAsset())
		{
			if (Candidates.Num() == 0)
			{
				Candidates = UCinemafyChoreographyLibrary::GatherCandidates(UCinemafySettings::GetDefaultMoveLibrary(), { TEXT("/Game") }, Mesh->GetSkeletalMeshAsset()->GetSkeleton());
			}
			// Varied: avoid the loops already handed out while others fit.
			const TArray<int32> Picks = UCinemafyChoreographyLibrary::PickAnimations(Reading, Candidates, 1, Used);
			if (Picks.Num() > 0)
			{
				Loop = Cast<UAnimSequenceBase>(Candidates[Picks[0]].Path.TryLoad());
				Used.Add(Candidates[Picks[0]].Path);
			}
		}
		Loops.Add(Loop);
		if (Loop && Mesh)
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Mesh->PlayAnimation(Loop, true);
			Mesh->SetPlayRate(FMath::FRandRange(0.9f, 1.1f));
		}
	}
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Extras %s: %d extra(s), %s%s."), *GetName(), Extras.Num(),
		*StaticEnum<ECinemafyExtrasBehavior>()->GetNameStringByValue(static_cast<int64>(Behavior)),
		Place.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" at \"%s\" (%d mark(s))"), *Place, Stations.Num()));
}

void ACinemafyExtras::BeginPlay()
{
	Super::BeginPlay();
	WaitLeft.Init(0.f, Extras.Num());
	if (Behavior != ECinemafyExtrasBehavior::Patrol && Behavior != ECinemafyExtrasBehavior::Wander)
	{
		SetActorTickEnabled(false);
		return;
	}
	for (int32 Index = 0; Index < Extras.Num(); ++Index)
	{
		if (ACharacter* Extra = Extras[Index])
		{
			if (!Extra->GetController())
			{
				Extra->SpawnDefaultController();
			}
			WaitLeft[Index] = FMath::FRandRange(0.f, 2.f);
		}
	}
}

void ACinemafyExtras::NextStop(int32 Index)
{
	ACharacter* Extra = Extras.IsValidIndex(Index) ? Extras[Index].Get() : nullptr;
	AAIController* Controller = Extra ? Cast<AAIController>(Extra->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}
	const float Range = Behavior == ECinemafyExtrasBehavior::Patrol ? Radius : Radius * 0.4f;
	Controller->MoveToLocation(RandomSpot(Behavior == ECinemafyExtrasBehavior::Patrol ? GetActorLocation() : Extra->GetActorLocation(), Range), 50.f);
}

void ACinemafyExtras::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 Index = 0; Index < Extras.Num(); ++Index)
	{
		ACharacter* Extra = Extras[Index];
		const AAIController* Controller = Extra ? Cast<AAIController>(Extra->GetController()) : nullptr;
		if (!Controller || !WaitLeft.IsValidIndex(Index))
		{
			continue;
		}
		if (Controller->GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			continue;
		}
		WaitLeft[Index] -= DeltaSeconds;
		if (WaitLeft[Index] <= 0.f)
		{
			// Pause a beat at each stop, longer when wandering.
			WaitLeft[Index] = Behavior == ECinemafyExtrasBehavior::Patrol ? FMath::FRandRange(1.f, 3.f) : FMath::FRandRange(2.f, 6.f);
			NextStop(Index);
		}
	}
}
