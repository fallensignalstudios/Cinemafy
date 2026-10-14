#include "SCinemafyTransition.h"
#include "Rendering/DrawElements.h"

void SCinemafyTransition::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SCinemafyTransition::Begin(ECinemafyTransition InType, ECinemafyWipeDirection InDirection, const FSlateBrush* InOutgoingImage)
{
	bActive = true;
	Type = InType;
	Direction = InDirection == ECinemafyWipeDirection::Alternate ? ECinemafyWipeDirection::LeftToRight : InDirection;
	OutgoingImage = InOutgoingImage;
	Progress = 0.f;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SCinemafyTransition::SetProgress(float InProgress)
{
	Progress = FMath::Clamp(InProgress, 0.f, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SCinemafyTransition::Finish()
{
	bActive = false;
	OutgoingImage = nullptr;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SCinemafyTransition::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!bActive || Type == ECinemafyTransition::Cut || Type == ECinemafyTransition::MatchCut)
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FSlateBrush* Outgoing = OutgoingImage ? OutgoingImage : &BlackBrush;
	auto DrawFull = [&](const FSlateBrush* Brush, const FLinearColor& Tint)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, Tint);
	};

	switch (Type)
	{
	case ECinemafyTransition::Dissolve:
		DrawFull(Outgoing, FLinearColor(1.f, 1.f, 1.f, 1.f - Progress));
		break;

	case ECinemafyTransition::FadeThroughBlack:
	{
		// First half: the old shot sinks into black. Second half: the new shot rises out of it.
		if (Progress < 0.5f)
		{
			DrawFull(Outgoing, FLinearColor::White);
			DrawFull(&BlackBrush, FLinearColor(0.f, 0.f, 0.f, Progress * 2.f));
		}
		else
		{
			DrawFull(&BlackBrush, FLinearColor(0.f, 0.f, 0.f, (1.f - Progress) * 2.f));
		}
		break;
	}

	case ECinemafyTransition::Wipe:
	{
		// The old shot keeps playing on the part of the screen the wipe hasn't reached yet.
		FVector2D Min(0.f, 0.f);
		FVector2D Max = Size;
		switch (Direction)
		{
		case ECinemafyWipeDirection::LeftToRight: Min.X = Size.X * Progress; break;
		case ECinemafyWipeDirection::RightToLeft: Max.X = Size.X * (1.f - Progress); break;
		case ECinemafyWipeDirection::TopToBottom: Min.Y = Size.Y * Progress; break;
		case ECinemafyWipeDirection::BottomToTop: Max.Y = Size.Y * (1.f - Progress); break;
		default: break;
		}
		if (Max.X <= Min.X || Max.Y <= Min.Y)
		{
			break;
		}

		const FVector2D AbsMin = AllottedGeometry.LocalToAbsolute(Min);
		const FVector2D AbsMax = AllottedGeometry.LocalToAbsolute(Max);
		OutDrawElements.PushClip(FSlateClippingZone(FSlateRect(AbsMin.X, AbsMin.Y, AbsMax.X, AbsMax.Y)));
		DrawFull(Outgoing, FLinearColor::White);
		OutDrawElements.PopClip();
		break;
	}

	default:
		break;
	}
	return LayerId;
}
