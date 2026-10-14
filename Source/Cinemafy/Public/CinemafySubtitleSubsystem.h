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
	/**
	 * Show a line. Long lines are split into readable chunks, and the whole thing stays up for at least
	 * Duration (the length of the spoken line) and long enough to read. A new line replaces it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Subtitles")
	void ShowSubtitle(const FText& Speaker, const FText& Text, float Duration = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Subtitles")
	void ClearSubtitle();

	/** Global switch, e.g. from your game's accessibility settings. */
	UPROPERTY(BlueprintReadWrite, Category = "Cinemafy|Subtitles")
	bool bSubtitlesEnabled = true;

	/** Reading speed for alphabetic scripts (characters per second). CJK text automatically uses a slower pace. */
	UPROPERTY(BlueprintReadWrite, Category = "Cinemafy|Subtitles")
	float CharactersPerSecond = 17.f;

	/** Longest chunk shown at once (about two lines). CJK text automatically uses shorter chunks. */
	UPROPERTY(BlueprintReadWrite, Category = "Cinemafy|Subtitles")
	int32 MaxCharactersPerChunk = 84;

	/** No chunk is shown for less than this, however short. */
	UPROPERTY(BlueprintReadWrite, Category = "Cinemafy|Subtitles")
	float MinChunkSeconds = 1.2f;

	/** Splits text into readable chunks: sentences first, then clauses, then words. */
	static TArray<FString> SplitIntoChunks(const FString& Text, int32 MaxCharacters);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureWidget();

	void ShowChunk(int32 Index);

	TSharedPtr<SCinemafySubtitles> Widget;
	float TimeLeft = 0.f;

	FText CurrentSpeaker;
	TArray<FString> Chunks;
	TArray<float> ChunkSeconds;
	int32 ChunkIndex = INDEX_NONE;
};
