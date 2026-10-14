#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "CinemafyEditDecisions.h"

/** Full-screen title overlay: lower thirds, location and chapter cards, centered titles and the opening crawl. */
class SCinemafyTitles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyTitles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetTitle(const FCinemafyTitleCard& InCard, float InProgress);
	void Clear();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }

private:
	void PaintCrawl(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, float Alpha) const;

	bool bVisible = false;
	FCinemafyTitleCard Card;
	float Progress = 0.f;

	/** The crawl body, wrapped for the screen width it was last laid out for. */
	mutable TArray<FString> CrawlLines;
	mutable float CrawlLayoutWidth = 0.f;
	mutable FString CrawlLayoutSource;

	FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor::White);
};
