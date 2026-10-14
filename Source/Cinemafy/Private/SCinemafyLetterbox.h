#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateColorBrush.h"

/** Full-screen overlay that draws animated black bars to letterbox the game view to a cinema aspect ratio. */
class SCinemafyLetterbox : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyLetterbox)
		: _AspectRatio(2.39f)
	{}
		SLATE_ARGUMENT(float, AspectRatio)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 0 = no bars, 1 = fully letterboxed. */
	void SetAlpha(float InAlpha);
	void SetAspectRatio(float InAspectRatio);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }

private:
	float AspectRatio = 2.39f;
	float Alpha = 0.f;
	FSlateColorBrush BarBrush = FSlateColorBrush(FLinearColor::Black);
};
