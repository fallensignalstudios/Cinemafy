#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyFocusComponent.generated.h"

class UCineCameraComponent;

/**
 * Keeps a Cine Camera focused on a character's eyes (MetaHuman eye bones), not their pivot.
 * Racks focus between two characters the way a real focus puller does (eased, in diopters),
 * either from gameplay via RackFocusTo or by keying RackAlpha in Sequencer.
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Eye Focus"))
class CINEMAFY_API UCinemafyFocusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyFocusComponent();

	/** The character to hold focus on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	TObjectPtr<AActor> FocusTarget = nullptr;

	/** The character to rack focus toward as RackAlpha goes from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	TObjectPtr<AActor> RackTarget = nullptr;

	/** 0 = focus on FocusTarget, 1 = focus on RackTarget. Key this in Sequencer for a focus pull. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Focus", meta = (ClampMin = "0", ClampMax = "1"))
	float RackAlpha = 0.f;

	/** Pushes focus further (+) or closer (-) in cm. Great for a subtle "soft" beat. Keyable. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Focus")
	float FocusOffset = 0.f;

	/** Focus on the eyes. If off, focuses on the actor's bounds center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	bool bFocusOnEyes = true;

	/** Lag on focus changes (0 = locked on). A little lag feels like a human puller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus", meta = (ClampMin = "0"))
	float SmoothingSpeed = 0.f;

	/** Eased focus pull to a new target over Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Focus")
	void RackFocusTo(AActor* NewTarget, float Duration = 1.f);

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Focus")
	float GetCurrentFocusDistance() const { return CurrentDistance; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float GetPlaneDistance(const AActor* Target, const UCineCameraComponent* Camera) const;

	float CurrentDistance = 0.f;
	bool bRacking = false;
	float RackElapsed = 0.f;
	float RackDuration = 1.f;
};
