#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyTypes.h"
#include "CinemafyCameraFeelComponent.generated.h"

class UCameraComponent;

/**
 * Makes a camera feel operated by a human: layered organic drift, sway and breathing.
 * Drop it on a CineCameraActor. Key "Intensity" in Sequencer to ramp the shake with the drama.
 *
 * Works on the camera component's relative transform, so it stacks on top of whatever
 * Sequencer does to the actor (rails, cranes, keyframed moves).
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Camera Feel"))
class CINEMAFY_API UCinemafyCameraFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyCameraFeelComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	ECinemafyCameraFeel Feel = ECinemafyCameraFeel::Shoulder;

	/** Overall multiplier. Keyable in Sequencer. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Camera Feel", meta = (ClampMin = "0", UIMax = "3"))
	float Intensity = 1.f;

	/** Used when Feel is Custom. Copy a preset into it with "Copy Preset To Custom" and tweak from there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	FCinemafyFeelSettings CustomSettings;

	/** Different seeds give different (but repeatable) wobble. Give each camera its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	int32 Seed = 0;

	/**
	 * Drive the noise from a keyed time value instead of the world clock. Key this linearly in Sequencer
	 * and scrubbing / renders will show exactly the same shake every time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Timing")
	bool bUseTimeOverride = false;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Timing", meta = (EditCondition = "bUseTimeOverride"))
	float TimeOverride = 0.f;

	/** Preview the feel in the editor viewport (while Sequencer is scrubbing, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	bool bPreviewInEditor = true;

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Camera Feel")
	static FCinemafyFeelSettings GetPresetSettings(ECinemafyCameraFeel InFeel);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Camera Feel")
	void CopyPresetToCustom();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	void RestoreBaseTransform();

	TWeakObjectPtr<UCameraComponent> Camera;
	FTransform BaseRelativeTransform;
	float ElapsedTime = 0.f;
};
