#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyTypes.h"
#include "CinemafyListenerComponent.generated.h"

class UCinemafyGazeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCinemafyReactionEvent, ECinemafyReaction, Reaction, float, Strength);

/**
 * Makes a character actually listen. It reads the speaker's face (the MetaHuman jaw curve by default),
 * finds phrase endings, emphasis and long stretches of talk, and responds the way people do:
 * nods in the pauses, blinks and brow raises on stressed words, tilts and glances away while
 * processing, and quiet idle life in between. Shaped by a temperament and a keyable Engagement.
 *
 * It suppresses itself while its own character is talking, so put one on each person in a
 * conversation and point their Speakers at each other.
 *
 * Like Gaze, it computes and the Anim Blueprint applies:
 *  - HeadRotation                          -> additive rotation on the head / neck
 *  - BrowRaise, BrowFurrow, Smile, Squint  -> face expression curves (0..1)
 *  - BreathAlpha, WeightShift              -> body additive / idle blend
 * If the character has a Cinemafy Gaze component, it also drives that: look at the speaker,
 * avert while thinking, blink on emphasis.
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Listener"))
class CINEMAFY_API UCinemafyListenerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyListenerComponent();

	// --- Who --------------------------------------------------------------------

	/** Who this character is listening to. Change it mid-scene with SetSpeaker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener")
	TObjectPtr<AActor> Speaker = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener")
	ECinemafyTemperament Temperament = ECinemafyTemperament::Attentive;

	/** 0 = zoned out, 1 = hanging on every word. Scales how often and how strongly they react. Keyable. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Listener", meta = (ClampMin = "0", ClampMax = "1.5"))
	float Engagement = 1.f;

	/** Overall output weight (0 = rest pose). Keyable, for fading the system around hand-animated beats. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Listener", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	/** Same seed = same performance on every playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener")
	int32 Seed = 0;

	// --- Hearing ----------------------------------------------------------------

	/**
	 * Anim curves that move when someone talks. The loudest one counts. MetaHuman faces expose
	 * CTRL_expressions_jawOpen, which follows MetaHuman Animator / audio-driven performances.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener|Hearing")
	TArray<FName> SpeechCurves = { FName(TEXT("CTRL_expressions_jawOpen")) };

	/** Curve level that counts as talking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener|Hearing", meta = (ClampMin = "0", ClampMax = "1"))
	float SpeechThreshold = 0.06f;

	/** Silence (seconds) that ends a phrase. Listeners react in these gaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener|Hearing", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float PhrasePause = 0.35f;

	/** No face curves? Turn this on and key ManualSpeechEnergy instead (0 = silent, 1 = talking hard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener|Hearing")
	bool bUseManualSpeechEnergy = false;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Listener|Hearing", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bUseManualSpeechEnergy"))
	float ManualSpeechEnergy = 0.f;

	/** Go quiet while this character is the one talking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener|Hearing")
	bool bSuppressWhileSpeaking = true;

	/** Drive this character's Cinemafy Gaze (look at speaker, avert while thinking, blink on emphasis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Listener")
	bool bDriveGaze = true;

	// --- Outputs (read these in the Anim Blueprints) -----------------------------

	/** Additive head rotation in degrees. Pitch + = chin up, Yaw + = turn right, Roll + = tilt right. */
	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	FRotator HeadRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float BrowRaise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float BrowFurrow = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float Smile = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float Squint = 0.f;

	/** 0..1 breathing cycle, for a chest/shoulder additive. */
	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float BreathAlpha = 0.f;

	/** -1..1 slow side-to-side weight shift, for blending idle poses. */
	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float WeightShift = 0.f;

	/** Is the speaker talking right now (as heard by this listener)? */
	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	bool bSpeakerTalking = false;

	/** Smoothed speech level being heard, 0..1. Handy for debugging thresholds. */
	UPROPERTY(BlueprintReadOnly, Category = "Listener|Output")
	float HeardEnergy = 0.f;

	/** Fired for every gesture, automatic or triggered. Hook body montages ("acknowledge", "shrug") here. */
	UPROPERTY(BlueprintAssignable, Category = "Listener")
	FCinemafyReactionEvent OnReaction;

	// --- Functions ----------------------------------------------------------------

	/** Perform a gesture now. Use from Sequencer event tracks to place a reaction on an exact line. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Listener")
	void React(ECinemafyReaction Reaction, float Strength = 1.f);

	/** Switch who's being listened to (also re-targets Gaze if driving it). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Listener")
	void SetSpeaker(AActor* NewSpeaker);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;

private:
	struct FGesture
	{
		ECinemafyReaction Type = ECinemafyReaction::Nod;
		float StartTime = 0.f;
		float Duration = 0.5f;
		float Strength = 1.f;
		float Sign = 1.f;
	};

	float ReadSpeechLevel(const AActor* Actor) const;
	void UpdateHearing(float DeltaTime);
	void OnPhraseEnd(float PhraseLength);
	void OnEmphasis();
	void OnLongListen();
	void OnIdleSilence();
	bool TryGesture(ECinemafyReaction Reaction, float Chance, float Strength);
	float RollStrength();
	void EvaluateOutputs(float DeltaTime);
	UCinemafyGazeComponent* GetGaze() const;

	FRandomStream Random;
	float LocalTime = 0.f;
	TArray<FGesture> Gestures;

	// Hearing state
	float FastEnvelope = 0.f;
	float SlowEnvelope = 0.f;
	bool bInPhrase = false;
	float PhraseStartTime = 0.f;
	float LastVoiceTime = -100.f;
	float LastEmphasisTime = -100.f;
	float LastGestureTime = -100.f;
	float NextLongListenTime = 0.f;
	float NextIdleTime = 0.f;
	float SelfSpeakingAlpha = 0.f;
};
