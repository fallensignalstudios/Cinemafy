#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinemafyExtras.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UBillboardComponent;

UENUM(BlueprintType)
enum class ECinemafyExtrasBehavior : uint8
{
	/** At stations (marks named like the direction: "consoles", "desks"), looping work. */
	Work,
	/** Walking between points, pausing. */
	Patrol,
	/** In pairs, facing each other, gesturing. */
	Talk,
	/** Drifting around the area. */
	Wander,
	Sit,
	/** Standing about (guards, onlookers). */
	Idle
};

/**
 * Background life you direct in a sentence: "crew works at consoles", "marines patrol", "two officers talk",
 * "crowd mills about". Populate puts the extras on set with fitting loops from your animation library; in play
 * (and in takes) patrols and wanderers walk on the navmesh.
 */
UCLASS(meta = (DisplayName = "Cinemafy Extras"))
class CINEMAFY_API ACinemafyExtras : public AActor
{
	GENERATED_BODY()

public:
	ACinemafyExtras();

	/** What they're doing, in plain words. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extras")
	FString Direction = TEXT("crew works at consoles");

	/** Who they are (cycled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extras")
	TArray<TSoftClassPtr<ACharacter>> ExtraClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extras", meta = (ClampMin = "1", ClampMax = "64"))
	int32 Count = 4;

	/** The area they use around this actor (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extras", meta = (ClampMin = "100"))
	float Radius = 1200.f;

	/** How it read the direction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Extras")
	ECinemafyExtrasBehavior Behavior = ECinemafyExtrasBehavior::Work;

	/** The extras it put on set. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Extras")
	TArray<TObjectPtr<ACharacter>> Extras;

	/** Their loops (same order as Extras; empty = their own Anim Blueprint). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Extras")
	TArray<TObjectPtr<UAnimSequenceBase>> Loops;

	/** Puts the extras on set (replacing the last ones). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Extras")
	void Populate();

	/** Takes them off set. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Extras")
	void Clear();

	/** "crew works at consoles" -> Work (place word "consoles"); "marines patrol" -> Patrol... */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Extras")
	static ECinemafyExtrasBehavior ReadDirection(const FString& InDirection, FString& OutPlace);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	FVector RandomSpot(const FVector& Around, float InRadius) const;
	void NextStop(int32 Index);

	TArray<float> WaitLeft;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
