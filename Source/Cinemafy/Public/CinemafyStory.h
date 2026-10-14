#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "CinemafyTypes.h"
#include "CinemafyScript.h"
#include "CinemafyStory.generated.h"

class UBoxComponent;
class ULevelSequence;
class SCinemafyChoices;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCinemafyStorySceneEvent, const FString&, SceneId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCinemafyStoryChoicesEvent, const FString&, SceneId, const TArray<FCinemafyScriptChoice>&, Choices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCinemafyStoryFinishedEvent);

/**
 * Plays a Script Project as a story in the game: each scene's sequence (with the gameplay handoff of
 * Cinemafy cutscenes), its flags, its choices, and where it leads. The player's own pawn stands in for the
 * cast member marked Is Player, gear and all. Skippable. Flags and position can be saved and restored.
 */
UCLASS()
class CINEMAFY_API UCinemafyStorySubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Starts (or jumps) the story at a scene. bFollowStory = carry on through choices and diverts afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	bool PlayScene(UCinemafyScriptProject* Project, const FString& SceneId, bool bFollowStory = true);

	/** Picks one of the offered choices (index into the choices broadcast by OnChoices). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	void Choose(int32 ChoiceIndex);

	/** Skips to the end of the playing scene (the cutscene still hands back smoothly). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	void SkipScene();

	/** Stops the story (the playing scene stops, nothing follows). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	void StopStory();

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Story")
	bool IsPlaying() const { return bPlaying || bAwaitingChoice; }

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Story")
	FString GetCurrentSceneId() const { return CurrentSceneId; }

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	void SetFlag(FName Flag, bool bSet);

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Story")
	bool HasFlag(FName Flag) const { return Flags.Contains(Flag); }

	/** For your save game: the story's flags and the scenes seen. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Story")
	TArray<FName> GetFlags() const { return Flags.Array(); }

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	void RestoreFlags(const TArray<FName>& SavedFlags) { Flags = TSet<FName>(SavedFlags); }

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Story")
	TArray<FString> GetScenesSeen() const { return ScenesSeen; }

	/** How scenes play (blend, letterbox, anchoring). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinemafy|Story")
	FCinemafyCutsceneSettings CutsceneSettings;

	/** Let the player skip scenes (Escape / gamepad Special Right by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinemafy|Story")
	bool bAllowSkip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinemafy|Story")
	TArray<FKey> SkipKeys = { EKeys::Escape, EKeys::Gamepad_Special_Right };

	/** Show Cinemafy's own choice buttons. Turn off to draw your own UI from OnChoices and call Choose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinemafy|Story")
	bool bBuiltInChoiceUI = true;

	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Story")
	FCinemafyStorySceneEvent OnSceneStarted;

	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Story")
	FCinemafyStorySceneEvent OnSceneFinished;

	/** A scene ended on a choice: show the options, then call Choose. */
	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Story")
	FCinemafyStoryChoicesEvent OnChoices;

	/** A scene has no sequence yet (or its level isn't loaded): load it, or skip on with Choose / PlayScene. */
	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Story")
	FCinemafyStorySceneEvent OnSceneUnavailable;

	UPROPERTY(BlueprintAssignable, Category = "Cinemafy|Story")
	FCinemafyStoryFinishedEvent OnStoryFinished;

	/** Where the story goes after a scene with these flags (pure: the choices on offer, else the divert or the next scene). */
	static FString NextSceneAfter(const UCinemafyScriptProject* Project, const FString& SceneId, const TSet<FName>& Flags, TArray<FCinemafyScriptChoice>& OutChoices);

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bPlaying || bAwaitingChoice; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableWhenPaused() const override { return true; }

	virtual void Deinitialize() override;

private:
	UFUNCTION()
	void HandleCutsceneFinished(ULevelSequence* Sequence);

	void Advance();
	void ShowChoices();
	void HideChoices();
	void BindPlayer(class ULevelSequencePlayer* Player, const FCinemafyScriptScene& Scene);
	void ApplyFlags(const TArray<FString>& Changes);

	UPROPERTY()
	TObjectPtr<UCinemafyScriptProject> Project;

	FString CurrentSceneId;
	TArray<FCinemafyScriptChoice> OfferedChoices;
	TSet<FName> Flags;
	TArray<FString> ScenesSeen;
	bool bPlaying = false;
	bool bAwaitingChoice = false;
	bool bFollowStory = true;
	TWeakObjectPtr<ULevelSequence> PlayingSequence;
	TSharedPtr<SCinemafyChoices> ChoiceWidget;
	TWeakObjectPtr<UWorld> ChoiceWorld;
};

/** Plays a scene of a script when the player walks in (once, by default). */
UCLASS(meta = (DisplayName = "Cinemafy Scene Trigger"))
class CINEMAFY_API ACinemafySceneTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACinemafySceneTrigger();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	TObjectPtr<UCinemafyScriptProject> Project;

	/** The scene to play ("2.1.3"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString SceneId;

	/** Carry on through the story's choices and diverts after this scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	bool bFollowStory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	bool bOnlyOnce = true;

	/** Only fire when these story flags hold ("flag", "!flag"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	TArray<FString> RequireFlags;

	UPROPERTY(VisibleAnywhere, Category = "Scene")
	TObjectPtr<UBoxComponent> Volume;

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	void Fire();

protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

private:
	bool bFired = false;
};
