#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CinemafySubtitleSubsystem.generated.h"

class SCinemafySubtitles;

/**
 * Shows dialogue subtitles over the game view. The Scene Director drives it (live and in baked
 * sequences), and you can call it yourself from any dialogue system.
 */
UCLASS()
class CINEMAFY_API UCinemafySubtitleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Show a line. Duration <= 0 keeps it up until the next line or ClearSubtitle. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Subtitles")
	void ShowSubtitle(const FText& Speaker, const FText& Text, float Duration = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Subtitles")
	void ClearSubtitle();

	/** Global switch, e.g. from your game's accessibility settings. */
	UPROPERTY(BlueprintReadWrite, Category = "Cinemafy|Subtitles")
	bool bSubtitlesEnabled = true;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureWidget();

	TSharedPtr<SCinemafySubtitles> Widget;
	float TimeLeft = 0.f;
};
