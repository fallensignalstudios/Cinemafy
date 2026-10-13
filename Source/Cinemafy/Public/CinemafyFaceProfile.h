#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinemafyTypes.h"
#include "CinemafyFaceProfile.generated.h"

/**
 * How a character's face performance gets processed: per-region gain, jitter cleanup and flesh
 * dynamics, per-curve overrides and custom emotion poses. Make one per character (or per capture
 * setup) and assign it to their Cinemafy Face component.
 */
UCLASS(BlueprintType)
class CINEMAFY_API UCinemafyFaceProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UCinemafyFaceProfile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Brows;

	/** Lids, blinks, squints and eye looks. Keep smoothing and springs low so blinks stay crisp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Eyes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Cheeks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Nose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Mouth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Jaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Tongue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regions")
	FCinemafyFaceRegionSettings Other;

	/** Fine control of individual curves, by full name (e.g. CTRL_expressions_mouthCornerPullL). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curves")
	TMap<FName, FCinemafyCurveOverride> CurveOverrides;

	/** Replace a built-in emotion with your own pose (full curve names). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emotions")
	TMap<ECinemafyEmotion, FCinemafyEmotionPose> CustomEmotions;

	/** Only curves starting with this are processed. MetaHuman face controls use CTRL_expressions_. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curves")
	FString CurvePrefix = TEXT("CTRL_expressions_");

	const FCinemafyFaceRegionSettings& GetRegion(ECinemafyFaceRegion Region) const;

	/** Settings used when no profile is assigned: light cleanup, a touch of flesh on the mouth. */
	static const UCinemafyFaceProfile* GetDefaultProfile();
};
