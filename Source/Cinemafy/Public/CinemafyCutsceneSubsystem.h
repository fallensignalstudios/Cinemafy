#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CinemafyTypes.h"
#include "CinemafyCutsceneSubsystem.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class SCinemafyLetterbox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCinemafyCutsceneEvent, ULevelSequence*, Sequence);

/**
 * Plays cutscenes that hand off cleanly with gameplay: camera blend in and out (from the sequence's
 * camera cut easing), animated letterbox bars in sync with the blend, optional re-anchoring to the
 * player so nothing pops, and a seamless return with the gameplay camera facing where the cutscene ended.
 *
 *   GetWorld()->GetSubsystem<UCinemafyCutsceneSubsystem>()->PlayCutscene(Sequence, Settings);
 */
UCLASS()
class CINEMAFY_API UCinemafyCutsceneSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Plays a cutscene. Any cutscene already playing is stopped first. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Cutscene")
	ULevelSequencePlayer* PlayCutscene(ULevelSequence* Sequence, const FCinemafyCutsceneSettings& Settings);

	/** Jumps to the start of the blend-out, so skipping still hands back to gameplay smoothly. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Cutscene")
	void SkipCutscene();

	/** Stops immediately, with no blend. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Cutscene")
	void StopCutscene();

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Cutscene")
	bool IsCutscenePlaying() const;

	/** Fired when a cutscene starts. Hide your UMG HUD here. */
	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Cutscene")
	FCinemafyCutsceneEvent OnCutsceneStarted;

	/** Fired when a cutscene finishes (or is stopped). Bring your HUD back here. */
	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Cutscene")
	FCinemafyCutsceneEvent OnCutsceneFinished;

	/**
	 * Reads blend times from a sequence's camera cut track: the first cut's ease-in and the last
	 * cut's ease-out, in seconds. Returns false if the track doesn't blend.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Cutscene")
	static bool GetSequenceBlendTimes(const ULevelSequence* Sequence, float& OutBlendIn, float& OutBlendOut);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleFinished();

	FTransform ResolveAnchor(const FCinemafyCutsceneSettings& Settings) const;
	void AlignPlayerToCurrentCamera() const;
	void AddLetterbox(float AspectRatio);
	void RemoveLetterbox();
	void Cleanup();

	UPROPERTY()
	TObjectPtr<ULevelSequencePlayer> ActivePlayer;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> ActiveActor;

	UPROPERTY()
	TObjectPtr<ULevelSequence> ActiveSequence;

	FCinemafyCutsceneSettings ActiveSettings;
	float BlendIn = 0.f;
	float BlendOut = 0.f;
	bool bAligned = false;

	TSharedPtr<SCinemafyLetterbox> Letterbox;
};
