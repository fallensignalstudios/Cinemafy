#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinemafyTypes.h"
#include "CinemafyLightRig.generated.h"

class URectLightComponent;
class UPointLightComponent;
class USceneComponent;

/**
 * A gaffer in a box. Key, fill, rim and eye light that place themselves around a character
 * relative to the camera, the way film lighting is set per shot.
 *
 * - Intensities are in lux at the subject, so pushing lights further out never changes exposure.
 * - "Character Only" uses a lighting channel so cutscene cheats never touch the environment.
 * - Follows the character and camera live, or freeze it once you like the look.
 */
UCLASS(meta = (DisplayName = "Cinemafy Light Rig"))
class CINEMAFY_API ACinemafyLightRig : public AActor
{
	GENERATED_BODY()

public:
	ACinemafyLightRig();

	// --- Who and from where -----------------------------------------------------

	/** The character being lit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig")
	TObjectPtr<AActor> Subject = nullptr;

	/** The camera the lighting is designed for. Leave empty to use the active game camera (or the subject's front). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig")
	TObjectPtr<AActor> TargetCamera = nullptr;

	/** In game, with no Target Camera set, re-light for whatever camera is live, so every cut gets lit "per setup". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig")
	bool bFollowActiveCamera = true;

	/** Keep re-solving as things move. Turn off to freeze the lights where they are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig")
	bool bLiveUpdate = true;

	// --- Look -------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look")
	ECinemafyLightingStyle Style = ECinemafyLightingStyle::Loop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look")
	ECinemafyKeySide KeySide = ECinemafyKeySide::ShortSide;

	/** Key light brightness at the subject's face, in lux. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "0", UIMax = "2000"))
	float KeyIlluminance = 150.f;

	/** Master dimmer for the whole rig. Keyable: fade the rig in and out in Sequencer. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "0", UIMax = "2"))
	float Intensity = 1.f;

	/** Multiplies the style's key-to-fill ratio. > 1 = moodier, < 1 = flatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "0.1", UIMax = "4"))
	float Contrast = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "0", UIMax = "3"))
	float RimStrength = 1.f;

	/** Catchlight in the eyes. Tiny, but it's what makes a face feel alive on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "0", UIMax = "3"))
	float EyeLightStrength = 1.f;

	/** 0 = hard little source (crisp shadows), 1 = big soft box. Multiplies the style's softness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "0", UIMax = "2"))
	float Softness = 1.f;

	/** How far the key, fill and rim sit from the face, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (ClampMin = "30", UIMax = "600"))
	float LightDistance = 200.f;

	/** Add this many kelvin to every light: negative = warmer, positive = cooler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Look", meta = (UIMin = "-2000", UIMax = "2000"))
	float TemperatureShift = 0.f;

	// --- Isolation ----------------------------------------------------------------

	/**
	 * Only light the subject (lighting channel 1). Cinemafy enables channel 1 on the subject's meshes
	 * and leaves their normal world lighting alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Rig|Isolation")
	bool bCharacterOnly = true;

	// --- Lights -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light Rig|Lights")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light Rig|Lights")
	TObjectPtr<URectLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light Rig|Lights")
	TObjectPtr<URectLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light Rig|Lights")
	TObjectPtr<URectLightComponent> RimLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light Rig|Lights")
	TObjectPtr<UPointLightComponent> EyeLight;

	/** Re-place and re-balance every light now. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Light Rig")
	void SolveRig();

	/** Enable lighting channel 1 on the subject's meshes so Character Only lights can reach them. */
	UFUNCTION(BlueprintCallable, Category = "Light Rig")
	void ApplyCharacterChannels();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return bLiveUpdate; }

private:
	FVector ResolveCameraLocation(const FVector& Eye) const;
};
