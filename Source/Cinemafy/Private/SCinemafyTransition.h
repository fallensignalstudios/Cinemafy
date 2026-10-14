#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "CinemafyTypes.h"

/**
 * Full-screen transition overlay. Draws the outgoing shot (a live capture) over the incoming one and
 * reveals the new shot with a wipe (straight, iris or clock), a dissolve, a dip, or a whip pan.
 */
class SCinemafyTransition : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyTransition) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Start a transition. OutgoingImage may be null (then the outgoing "shot" is black). */
	void Begin(ECinemafyTransition InType, ECinemafyWipeDirection InDirection, const FSlateBrush* InOutgoingImage);

	/** 0 = all outgoing, 1 = all incoming. */
	void SetProgress(float InProgress);
	void Finish();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }

private:
	/** Iris and clock wipes: the outgoing shot drawn through a fan of stencil-clipped quads. */
	void PaintShapedWipe(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FSlateBrush* Outgoing) const;

	bool bActive = false;
	float Progress = 0.f;
	ECinemafyTransition Type = ECinemafyTransition::Cut;
	ECinemafyWipeDirection Direction = ECinemafyWipeDirection::LeftToRight;
	const FSlateBrush* OutgoingImage = nullptr;
	FSlateColorBrush BlackBrush = FSlateColorBrush(FLinearColor::Black);
	FSlateColorBrush WhiteBrush = FSlateColorBrush(FLinearColor::White);
};
