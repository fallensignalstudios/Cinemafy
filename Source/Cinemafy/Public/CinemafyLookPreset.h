#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Scene.h"
#include "CinemafyLookPreset.generated.h"

/** A shareable "film stock": a grade plus framing. Create one per mood and reuse it across every cutscene. */
UCLASS(BlueprintType)
class CINEMAFY_API UCinemafyLookPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ShowOnlyInnerProperties))
	FPostProcessSettings Settings;

	/** Letterbox aspect ratio to apply along with the grade. 0 = leave the camera's framing alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", UIMax = "2.76"))
	float LetterboxAspectRatio = 0.f;
};
