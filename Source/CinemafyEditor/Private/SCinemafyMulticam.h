#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"

class SUniformGridPanel;
class UTextureRenderTarget2D;
struct FSlateBrush;

/**
 * Multicam switcher: a live grid of the sequence's cameras. Arm it, press play, and tap 1-9 (or click
 * an angle) to cut - when playback stops, your switching replaces the camera cuts you played over.
 * Unarmed, picking an angle cuts to it at the playhead.
 */
class SCinemafyMulticam : public SCompoundWidget
{
public:
	static const FName TabName;

	SLATE_BEGIN_ARGS(SCinemafyMulticam) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCinemafyMulticam() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	struct FAngle
	{
		TWeakObjectPtr<AActor> Camera;
		TStrongObjectPtr<UTextureRenderTarget2D> Target;
		TSharedPtr<FSlateBrush> Brush;
	};

	void UseSequenceCameras();
	void UseSelectedCameras();
	void SetCameras(const TArray<AActor*>& Cameras);
	void RebuildGrid();
	void CaptureAngles();
	void SwitchTo(int32 Index);
	void Commit();
	FText GetStatusText() const;

	TArray<FAngle> Angles;
	TSharedPtr<SUniformGridPanel> Grid;

	bool bArmed = false;
	bool bRecording = false;
	bool bWasPlaying = false;
	float LastTime = 0.f;
	TArray<TWeakObjectPtr<AActor>> RecordedCameras;
	TArray<float> RecordedTimes;
	int32 LiveIndex = INDEX_NONE;
	float RefreshCountdown = 0.f;
};
