#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTypes.h"
#include "CinemafyScript.h"
#include "CinemafyMood.generated.h"

UENUM(BlueprintType)
enum class ECinemafyTimeOfDay : uint8
{
	Day,
	Dawn,
	Dusk,
	Night,
	/** Interiors with no time given: lit by the set, not the sky. */
	Unknown
};

/** How a scene should feel, read from its heading and words. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafySceneMood
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	ECinemafyTimeOfDay TimeOfDay = ECinemafyTimeOfDay::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	bool bExterior = false;

	/** calm, tense, action, sad, warm, romantic, triumphant, eerie, alarm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FString Mood = TEXT("calm");

	/** Rain, storm, snow, fog, fire noticed in the action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	TArray<FString> Weather;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	ECinemafyDirectorPreset DirectorStyle = ECinemafyDirectorPreset::ClassicHollywood;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	ECinemafyLook Look = ECinemafyLook::CleanCinema;

	/** Sun pitch in degrees (negative = above the horizon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	float SunPitch = -45.f;

	/** Sun brightness relative to full day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	float SunScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FLinearColor SunColor = FLinearColor::White;

	/** Plain words: why it reads this way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FString Why;
};

UCLASS()
class CINEMAFY_API UCinemafyMoodLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Reads a scene's mood: time of day from the heading (CONTINUOUS / SAME inherit PreviousTime), interior or
	 * exterior, the feeling from its words (fights read as action, alarms as alarm, grief as sad...), weather,
	 * and the director style, look and sun that suit it.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Mood")
	static FCinemafySceneMood ReadSceneMood(const FCinemafyScriptScene& Scene, ECinemafyTimeOfDay PreviousTime);
};
