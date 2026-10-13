#include "SCinemafyLetterbox.h"
#include "Rendering/DrawElements.h"

void SCinemafyLetterbox::Construct(const FArguments& InArgs)
{
	AspectRatio = InArgs._AspectRatio;
	SetVisibility(EVisibility::HitTestInvisible);
}

void SCinemafyLetterbox::SetAlpha(float InAlpha)
{
	const float NewAlpha = FMath::Clamp(InAlpha, 0.f, 1.f);
	if (!FMath::IsNearlyEqual(NewAlpha, Alpha))
	{
		Alpha = NewAlpha;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SCinemafyLetterbox::SetAspectRatio(float InAspectRatio)
{
	AspectRatio = FMath::Max(InAspectRatio, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SCinemafyLetterbox::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (Alpha <= 0.f)
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float FullBar = FMath::Max(0.f, static_cast<float>(Size.Y - Size.X / AspectRatio) * 0.5f);
	const float Bar = FullBar * Alpha;
	if (Bar <= 0.f)
	{
		return LayerId;
	}

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, Bar), FSlateLayoutTransform(FVector2D(0.f, 0.f))),
		&BarBrush, ESlateDrawEffect::None, FLinearColor::Black);

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, Bar), FSlateLayoutTransform(FVector2D(0.f, Size.Y - Bar))),
		&BarBrush, ESlateDrawEffect::None, FLinearColor::Black);

	return LayerId;
}
