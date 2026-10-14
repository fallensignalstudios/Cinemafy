#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/FrameNumber.h"

class ISequencer;
class ULevelSequence;
class UTextureRenderTarget2D;
class SHorizontalBox;
class STextBlock;
class ACinemafySceneDirector;
struct FSlateBrush;

/** One camera cut in the open sequence. */
struct FCinemafyShot
{
	int32 Number = 0;
	FText CameraName;
	FFrameNumber StartTick;
	FFrameNumber EndTick;
	float DurationSeconds = 0.f;
	TWeakObjectPtr<AActor> Camera;
	float FocalLength = 0.f;
	float Aperture = 0.f;

	/** Filled in when a Scene Director cut this shot. */
	FString Reason;
	bool bHasShotType = false;
	uint8 ShotType = 0;

	TStrongObjectPtr<UTextureRenderTarget2D> Thumbnail;
	TSharedPtr<FSlateBrush> ThumbnailBrush;
};

/**
 * A film strip of every camera cut in the Level Sequence open in Sequencer: thumbnail, lens,
 * duration (with pacing warnings) and a live highlight on the current shot. Click to jump there.
 */
class SCinemafyShotBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyShotBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Re-reads the camera cut track. */
	void Refresh();

	/** Renders a thumbnail through each shot's camera at the first frame of its cut. */
	void CaptureThumbnails();

	static const FName TabName;

	/** Refreshes the open Shot Browser, if there is one (e.g. after the Director re-cuts a scene). */
	static void RefreshActive();

private:
	void RebuildStrip();
	void RebuildPacingStrip();
	void AttachDirectorNotes();
	ACinemafySceneDirector* GetActiveDirector() const;
	FText GetDirectorText() const;
	TSharedRef<SWidget> MakeShotCard(const TSharedRef<FCinemafyShot>& Shot);
	FReply OnShotClicked(TSharedRef<FCinemafyShot> Shot);
	bool IsShotUnderPlayhead(const FCinemafyShot& Shot) const;
	FText GetSummaryText() const;

	TArray<TSharedRef<FCinemafyShot>> Shots;
	TWeakObjectPtr<ULevelSequence> Sequence;
	TSharedPtr<SHorizontalBox> Strip;
	TSharedPtr<SHorizontalBox> PacingStrip;

	static TWeakPtr<SCinemafyShotBrowser> ActiveInstance;
};
