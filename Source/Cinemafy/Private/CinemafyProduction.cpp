#include "CinemafyProduction.h"
#include "CinemafyScript.h"
#include "Components/BillboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"

const FName UCinemafyProductionLibrary::WalkOnlyTag(TEXT("Cinemafy.WalkOnly"));
const FName UCinemafyProductionLibrary::ContinuityTag(TEXT("Cinemafy.Continuity"));

ACinemafyMark::ACinemafyMark()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	bIsEditorOnlyActor = true;
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
		Sprite->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	}
	Label = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (Label)
	{
		Label->SetupAttachment(RootComponent);
		Label->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(24.f);
		Label->SetTextRenderColor(FColor(255, 220, 90));
		Label->SetHiddenInGame(true);
		Label->bIsEditorOnly = true;
	}
#endif
}

void ACinemafyMark::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	if (Label)
	{
		Label->SetText(FText::FromString(SetName.IsEmpty() ? MarkName : FString::Printf(TEXT("%s\n(%s)"), *MarkName, *SetName)));
	}
#endif
}

void UCinemafyProductionLibrary::ClearContinuityFromActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	TInlineComponentArray<UStaticMeshComponent*> Props(Actor);
	for (UStaticMeshComponent* Prop : Props)
	{
		if (Prop && Prop->ComponentHasTag(ContinuityTag))
		{
			Prop->DestroyComponent();
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (Mesh->ComponentHasTag(ContinuityTag))
			{
				Mesh->EmptyOverrideMaterials();
				Mesh->ComponentTags.Remove(ContinuityTag);
			}
		}
	}
	// Tags we added are recorded next to the marker tag as "Cinemafy.Continuity:<tag>".
	TArray<FName> Added;
	for (const FName& Tag : Actor->Tags)
	{
		const FString Text = Tag.ToString();
		if (Text.StartsWith(TEXT("Cinemafy.Continuity:")))
		{
			Added.Add(Tag);
			Added.Add(FName(*Text.Mid(20)));
		}
	}
	Actor->Tags.RemoveAll([&Added](const FName& Tag) { return Added.Contains(Tag) || Tag == WalkOnlyTag; });
	if (Actor->GetClass()->ImplementsInterface(UCinemafyContinuityReceiver::StaticClass()))
	{
		ICinemafyContinuityReceiver::Execute_ClearContinuity(Actor);
	}
}

void UCinemafyProductionLibrary::ApplyContinuityToActor(AActor* Actor, const FCinemafyCastMember& Member, const TArray<FCinemafyContinuityFact>& Facts)
{
	if (!Actor)
	{
		return;
	}
	ClearContinuityFromActor(Actor);
	ACharacter* Character = Cast<ACharacter>(Actor);
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : Actor->FindComponentByClass<USkeletalMeshComponent>();
	const bool bReceiver = Actor->GetClass()->ImplementsInterface(UCinemafyContinuityReceiver::StaticClass());

	for (const FCinemafyContinuityFact& Fact : Facts)
	{
		if (bReceiver)
		{
			ICinemafyContinuityReceiver::Execute_ApplyContinuity(Actor, Fact.Category, Fact.Fact, Fact.SinceScene);
		}
		for (const FCinemafyContinuityLook& Look : Member.Looks)
		{
			const bool bCategory = Look.Category.IsEmpty() || Look.Category.Equals(Fact.Category, ESearchCase::IgnoreCase);
			const bool bFact = Look.FactContains.IsEmpty() || Fact.Fact.Contains(Look.FactContains, ESearchCase::IgnoreCase);
			if (!bCategory || !bFact)
			{
				continue;
			}
			if (UStaticMesh* Mesh = Look.AttachMesh.LoadSynchronous())
			{
				UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
				Prop->SetStaticMesh(Mesh);
				Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Prop->ComponentTags.Add(ContinuityTag);
				if (Body)
				{
					Prop->SetupAttachment(Body, Look.Socket);
				}
				else
				{
					Prop->SetupAttachment(Actor->GetRootComponent());
				}
				Prop->RegisterComponent();
				Actor->AddInstanceComponent(Prop);
			}
			if (Body && !Look.Material.IsNull())
			{
				if (UMaterialInterface* Material = Look.Material.LoadSynchronous())
				{
					const int32 Slots = Body->GetNumMaterials();
					for (int32 Slot = 0; Slot < Slots; ++Slot)
					{
						if (Look.MaterialSlot < 0 || Look.MaterialSlot == Slot)
						{
							Body->SetMaterial(Slot, Material);
						}
					}
					Body->ComponentTags.AddUnique(ContinuityTag);
				}
			}
			if (!Look.ActorTag.IsNone() && !Actor->Tags.Contains(Look.ActorTag))
			{
				Actor->Tags.Add(Look.ActorTag);
				Actor->Tags.Add(FName(*(TEXT("Cinemafy.Continuity:") + Look.ActorTag.ToString())));
			}
			if (Look.bWalkOnly)
			{
				Actor->Tags.AddUnique(WalkOnlyTag);
			}
		}
	}
}

ACinemafyMark* UCinemafyProductionLibrary::FindMark(const UObject* WorldContextObject, const FString& Phrase, const FString& SetName)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || Phrase.IsEmpty())
	{
		return nullptr;
	}
	FString Wanted = Phrase.ToLower().TrimStartAndEnd();
	for (const TCHAR* Article : { TEXT("the "), TEXT("a "), TEXT("an "), TEXT("her "), TEXT("his "), TEXT("their ") })
	{
		Wanted.RemoveFromStart(Article);
	}

	ACinemafyMark* Best = nullptr;
	int32 BestRank = 0;
	for (TActorIterator<ACinemafyMark> It(World); It; ++It)
	{
		ACinemafyMark* Mark = *It;
		if (!SetName.IsEmpty() && !Mark->SetName.IsEmpty() && !Mark->SetName.Equals(SetName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FString Name = Mark->MarkName.ToLower();
		const int32 Rank = Name == Wanted ? 3 : (Name.Contains(Wanted) ? 2 : (Wanted.Contains(Name) && Name.Len() >= 3 ? 1 : 0));
		if (Rank > BestRank)
		{
			BestRank = Rank;
			Best = Mark;
		}
	}
	return Best;
}
