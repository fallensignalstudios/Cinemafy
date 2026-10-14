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

namespace
{
	/** Draws Brush clipped to a convex quad given in local space (Slate stencil-clips arbitrary quads). */
	void DrawInQuad(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FSlateBrush* Brush,
		const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		// FSlateClippingZone wants top-left, top-right, bottom-left, bottom-right (a convex quad, in window space).
		auto ToWindow = [&Geometry](const FVector2D& Local)
		{
			const FVector2D Absolute = Geometry.LocalToAbsolute(Local);
			return FVector2f(static_cast<float>(Absolute.X), static_cast<float>(Absolute.Y));
		};
		const FVector2f P0 = ToWindow(A);
		const FVector2f P1 = ToWindow(B);
		const FVector2f P2 = ToWindow(D);
		const FVector2f P3 = ToWindow(C);
		OutDrawElements.PushClip(FSlateClippingZone(P0, P1, P2, P3));
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, FLinearColor::White);
		OutDrawElements.PopClip();
	}
}

void SCinemafyTransition::PaintShapedWipe(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FSlateBrush* Outgoing) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const float Far = Size.Size();          // beyond every corner
	const float Eased = Progress * Progress * (3.f - 2.f * Progress);
	constexpr int32 Segments = 72;

	auto OnCircle = [&Center](float Radius, float Angle) { return Center + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius; };

	if (Direction == ECinemafyWipeDirection::Clock)
	{
		// The old shot remains on the part of the dial the hand hasn't swept yet (12 o'clock, clockwise).
		const float Swept = Eased * 2.f * PI;
		const float Step = (2.f * PI - Swept) / Segments;
		if (Step <= 0.f)
		{
			return;
		}
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float A0 = Swept + Step * Index;
			const float A1 = A0 + Step;
			DrawInQuad(OutDrawElements, LayerId, Geometry, Outgoing, Center, Center, OnCircle(Far, A0), OnCircle(Far, A1));
		}
		return;
	}

	// Iris out: the new shot opens in a growing circle, so the old one is the ring outside it.
	// Iris in: the old shot shrinks into a closing circle.
	const bool bOut = Direction == ECinemafyWipeDirection::IrisOut;
	const float Step = 2.f * PI / Segments;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float A0 = Step * Index;
		const float A1 = A0 + Step;
		if (bOut)
		{
			const float Radius = Eased * Far * 0.6f;
			DrawInQuad(OutDrawElements, LayerId, Geometry, Outgoing, OnCircle(Radius, A0), OnCircle(Radius, A1), OnCircle(Far, A1), OnCircle(Far, A0));
		}
		else
		{
			const float Radius = (1.f - Eased) * Far * 0.6f;
			if (Radius <= 1.f)
			{
				return;
			}
			DrawInQuad(OutDrawElements, LayerId, Geometry, Outgoing, Center, Center, OnCircle(Radius, A0), OnCircle(Radius, A1));
		}
	}
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
		if (Direction == ECinemafyWipeDirection::IrisOut || Direction == ECinemafyWipeDirection::IrisIn || Direction == ECinemafyWipeDirection::Clock)
		{
			PaintShapedWipe(AllottedGeometry, OutDrawElements, LayerId, Outgoing);
			break;
		}
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

	case ECinemafyTransition::DipToWhite:
	{
		if (Progress < 0.5f)
		{
			DrawFull(Outgoing, FLinearColor::White);
			DrawFull(&WhiteBrush, FLinearColor(1.f, 1.f, 1.f, Progress * 2.f));
		}
		else
		{
			DrawFull(&WhiteBrush, FLinearColor(1.f, 1.f, 1.f, (1.f - Progress) * 2.f));
		}
		break;
	}

	case ECinemafyTransition::WhipPan:
	{
		// The old shot accelerates off-screen, smeared by a trail of fading copies; the new shot is underneath.
		const float T = FMath::Clamp(Progress * 1.6f, 0.f, 1.f);
		const float Travel = T * T * (Size.X * 1.15f);
		const float Sign = Direction == ECinemafyWipeDirection::RightToLeft ? 1.f : -1.f;
		for (int32 Copy = 5; Copy >= 0; --Copy)
		{
			const float Lag = Copy * 0.06f * Size.X * T;
			const float Offset = Sign * FMath::Max(Travel - Lag, 0.f);
			const float Alpha = (Copy == 0 ? 1.f : 0.35f / Copy) * (1.f - T * 0.5f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D(Offset, 0.f))),
				Outgoing, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Alpha));
		}
		break;
	}

	default:
		break;
	}
	return LayerId;
}
