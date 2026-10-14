#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinemafyTypes.h"
#include "CinemafyDirectorStyleAsset.generated.h"

/**
 * A saved directing style: share it across scenes, tweak it, version it. Assign it to a Scene
 * Director's Style Asset to use it. "Save Style As Asset" on a director creates one from its
 * current (or learned) style.
 */
UCLASS(BlueprintType)
class CINEMAFY_API UCinemafyDirectorStyleAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (ShowOnlyInnerProperties))
	FCinemafyDirectorStyle Style;
};
