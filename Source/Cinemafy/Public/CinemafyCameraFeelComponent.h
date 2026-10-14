#pragma once

#include "CoreMinimal.h"
#include "CinemafyCameraOffsetComponent.h"
#include "CinemafyTypes.h"
#include "CinemafyCameraFeelComponent.generated.h"

/**
 * Makes a camera feel operated by a human: layered organic drift, sway and breathing.
 * Drop it on a CineCameraActor. Key "Intensity" in Sequencer to ramp the shake with the drama.
 *
 * Stacks on top of Sequencer keys, rails, cranes and Cinemafy Camera Moves.
 */
UCLASS(ClassGroup = (Cinemafy), meta = (BlueprintSpawnableComponent, DisplayName = "Cinemafy Camera Feel"))
class CINEMAFY_API UCinemafyCameraFeelComponent : public UCinemafyCameraOffsetComponent
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

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Camera Feel")
	static FCinemafyFeelSettings GetPresetSettings(ECinemafyCameraFeel InFeel);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Camera Feel")
	void CopyPresetToCustom();

	/** Shake rides on top of moves. */
	virtual int32 GetStackOrder() const override { return 100; }

protected:
	virtual FTransform EvaluateOffset(float DeltaTime) override;

private:
	float ElapsedTime = 0.f;
};
