#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyTypes.h"
#include "CinemafyFaceComponent.generated.h"

class UCinemafyFaceProfile;

/**
 * Facial performance controls for a character, applied by the "Cinemafy Face" node in the face
 * Anim Blueprint: boost or calm the performance, clean jitter, add flesh dynamics, layer
 * emotions, breathing, idle life and pupil response. Everything here is keyable in Sequencer.
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Face"))
class CINEMAFY_API UCinemafyFaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyFaceComponent();

	/** Per-region processing. Leave empty for sensible defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	TObjectPtr<UCinemafyFaceProfile> Profile = nullptr;

	/** Scales the whole performance around neutral. 1 = as captured. Keyable. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (ClampMin = "0", UIMax = "2"))
	float PerformanceIntensity = 1.f;

	// --- Lip sync ----------------------------------------------------------------

	/** Overall lip-sync sharpening: 0 = off, 1 = as configured, 2 = extra crisp. Keyable per line. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Face|Lip Sync", meta = (ClampMin = "0", UIMax = "2"))
	float Articulation = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Lip Sync")
	FCinemafyLipSyncSettings LipSync;

	// --- Emotion ---------------------------------------------------------------

	/** Emotion layered on top of the performance. Crossfades over EmotionBlendTime when changed. Keyable. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Face|Emotion")
	ECinemafyEmotion Emotion = ECinemafyEmotion::Neutral;

	/** How strongly the emotion shows. Keyable: build it up over a line. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Face|Emotion", meta = (ClampMin = "0", UIMax = "1.5"))
	float EmotionIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Emotion", meta = (ClampMin = "0"))
	float EmotionBlendTime = 0.6f;

	// --- Life --------------------------------------------------------------------

	/** Tiny idle movement in brows, cheeks and mouth corners, so a face is never frozen. Keyable. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Face|Life", meta = (ClampMin = "0", UIMax = "2"))
	float FaceLife = 1.f;

	/** Subtle nostril and jaw motion with each breath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Life")
	bool bBreathing = true;

	/** Breaths per second (~0.25 calm, ~0.5 after exertion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Life", meta = (ClampMin = "0.05", UIMax = "1"))
	float BreathRate = 0.25f;

	/** Pupils briefly constrict after each blink, like real eyes re-adapting to light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Life")
	bool bPupilResponse = true;

	/** Crossfade to a new emotion. Great as a Sequencer event on a line. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Face")
	void SetEmotion(ECinemafyEmotion NewEmotion, float BlendTime = 0.6f);

	/** Current crossfade weight of each emotion (indexed by ECinemafyEmotion). */
	const TArray<float>& GetEmotionWeights() const { return EmotionWeights; }

	/** The built-in MetaHuman pose for an emotion (full curve name -> additive value). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Face")
	static TMap<FName, float> GetBuiltInEmotionPose(ECinemafyEmotion InEmotion);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TArray<float> EmotionWeights;
};
