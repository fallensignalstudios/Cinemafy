#include "SCinemafyTitles.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** 0 -> 1 -> 0 over a card's life, with In/Out as fractions of it. */
	float TitleEnvelope(float Progress, float In, float Out)
	{
		const float Rise = In > 0.f ? FMath::SmoothStep(0.f, In, Progress) : 1.f;
		const float Fall = Out > 0.f ? 1.f - FMath::SmoothStep(1.f - Out, 1.f, Progress) : 1.f;
		return FMath::Clamp(Rise * Fall, 0.f, 1.f);
	}

	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	void PaintLabel(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry, const FString& Text, const FSlateFontInfo& Font,
		const FVector2D& Position, const FLinearColor& Color, float Scale = 1.f)
	{
		const FVector2D Size = Measure(Text, Font);
		FSlateDrawElement::MakeText(Out, LayerId, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Scale, Position)), Text, Font,
			ESlateDrawEffect::None, Color);
	}

	/** "Dagobah" -> "D A G O B A H". */
	FString Spaced(const FString& Text)
	{
		FString Result;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			Result.AppendChar(FChar::ToUpper(Text[Index]));
			if (Index + 1 < Text.Len())
			{
				Result.AppendChar(TEXT(' '));
			}
		}
		return Result;
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}
}

void SCinemafyTitles::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SCinemafyTitles::SetTitle(const FCinemafyTitleCard& InCard, float InProgress)
{
	bVisible = true;
	Card = InCard;
	Progress = FMath::Clamp(InProgress, 0.f, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SCinemafyTitles::Clear()
{
	if (bVisible)
	{
		bVisible = false;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 SCinemafyTitles::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!bVisible)
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float H = Size.Y;
	const bool bCustomColor = Card.Color != FLinearColor::Black;
	const float FadeFraction = FMath::Clamp(0.5f / FMath::Max(Card.Duration, 0.5f), 0.02f, 0.4f);
	const float Alpha = TitleEnvelope(Progress, FadeFraction, FadeFraction);
	const FString Title = Card.Title.ToString();
	const FString Subtitle = Card.Subtitle.ToString();

	switch (Card.Style)
	{
	case ECinemafyTitleStyle::LowerThird:
	{
		const FLinearColor Accent = bCustomColor ? Card.Color : FLinearColor(1.f, 0.62f, 0.12f);
		const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(H * 0.034f));
		const FSlateFontInfo RoleFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(H * 0.022f));
		const float Slide = (1.f - FMath::SmoothStep(0.f, FadeFraction * 1.5f, Progress)) * -40.f;
		const FVector2D Origin(Size.X * 0.07f + Slide, H * 0.76f);
		const FVector2D NameSize = Measure(Title, NameFont);
		const FVector2D RoleSize = Subtitle.IsEmpty() ? FVector2D::ZeroVector : Measure(Subtitle, RoleFont);
		const float BarHeight = NameSize.Y + RoleSize.Y + H * 0.012f;

		// Accent bar, then a soft plate behind the text.
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(H * 0.006f, BarHeight),
			FSlateLayoutTransform(Origin)), &SolidBrush, ESlateDrawEffect::None, WithAlpha(Accent, Alpha));
		const float PlateWidth = FMath::Max(NameSize.X, RoleSize.X) + H * 0.05f;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(PlateWidth, BarHeight),
			FSlateLayoutTransform(Origin + FVector2D(H * 0.006f, 0.f))), &SolidBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha));
		PaintLabel(OutDrawElements, LayerId + 1, AllottedGeometry, Title, NameFont, Origin + FVector2D(H * 0.022f, H * 0.004f), WithAlpha(FLinearColor::White, Alpha));
		if (!Subtitle.IsEmpty())
		{
			PaintLabel(OutDrawElements, LayerId + 1, AllottedGeometry, Subtitle, RoleFont, Origin + FVector2D(H * 0.022f, NameSize.Y + H * 0.004f),
				WithAlpha(FLinearColor(0.85f, 0.85f, 0.85f), Alpha));
		}
		return LayerId + 1;
	}

	case ECinemafyTitleStyle::LocationCard:
	{
		// Typed on over the first third, held, faded.
		const FString Full = Spaced(Title);
		const int32 Shown = FMath::Clamp(FMath::CeilToInt(Full.Len() * FMath::Clamp(Progress / 0.35f, 0.f, 1.f)), 0, Full.Len());
		const FLinearColor Color = bCustomColor ? Card.Color : FLinearColor::White;
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(H * 0.03f));
		const FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(H * 0.02f));
		const FVector2D Origin(Size.X * 0.07f, H * 0.8f);
		const float OutAlpha = TitleEnvelope(Progress, 0.f, FadeFraction);
		PaintLabel(OutDrawElements, LayerId, AllottedGeometry, Full.Left(Shown), Font, Origin, WithAlpha(Color, OutAlpha));
		if (!Subtitle.IsEmpty() && Progress > 0.35f)
		{
			const float SubAlpha = FMath::SmoothStep(0.35f, 0.5f, Progress) * OutAlpha;
			PaintLabel(OutDrawElements, LayerId, AllottedGeometry, Spaced(Subtitle), SubFont, Origin + FVector2D(0.f, H * 0.045f), WithAlpha(Color, SubAlpha * 0.85f));
		}
		return LayerId;
	}

	case ECinemafyTitleStyle::ChapterCard:
	case ECinemafyTitleStyle::CenteredTitle:
	{
		const bool bChapter = Card.Style == ECinemafyTitleStyle::ChapterCard;
		if (bChapter)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &SolidBrush, ESlateDrawEffect::None,
				FLinearColor(0.f, 0.f, 0.f, TitleEnvelope(Progress, FadeFraction * 0.5f, FadeFraction * 0.5f)));
		}
		const FLinearColor Color = bCustomColor ? Card.Color : FLinearColor::White;
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(H * (bChapter ? 0.05f : 0.08f)));
		const FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(H * 0.025f));
		const FString Shown = bChapter ? Spaced(Title) : Title;
		const FVector2D TitleSize = Measure(Shown, Font);
		// A slow push in while it's up.
		const float Scale = 1.f + Progress * 0.04f;
		const FVector2D Position = Size * 0.5f - TitleSize * Scale * 0.5f - FVector2D(0.f, Subtitle.IsEmpty() ? 0.f : H * 0.03f);
		PaintLabel(OutDrawElements, LayerId + 1, AllottedGeometry, Shown, Font, Position, WithAlpha(Color, Alpha), Scale);
		if (!Subtitle.IsEmpty())
		{
			const FVector2D SubSize = Measure(Subtitle, SubFont);
			PaintLabel(OutDrawElements, LayerId + 1, AllottedGeometry, Subtitle, SubFont,
				FVector2D((Size.X - SubSize.X) * 0.5f, Position.Y + TitleSize.Y * Scale + H * 0.02f), WithAlpha(Color, Alpha * 0.85f));
		}
		return LayerId + 1;
	}

	case ECinemafyTitleStyle::OpeningCrawl:
		PaintCrawl(AllottedGeometry, OutDrawElements, LayerId, Alpha);
		return LayerId + 1;
	}
	return LayerId;
}

void SCinemafyTitles::PaintCrawl(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, float Alpha) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const float W = Size.X;
	const float H = Size.Y;
	const FLinearColor Gold = Card.Color != FLinearColor::Black ? Card.Color : FLinearColor(1.f, 0.82f, 0.18f);
	const FLinearColor IntroBlue(0.29f, 0.75f, 0.95f);

	// Timeline (fractions of the card): intro line, then the title falling away, then the crawl.
	const bool bHasIntro = !Card.Subtitle.IsEmpty();
	const bool bHasTitle = !Card.Title.IsEmpty();
	const float IntroEnd = bHasIntro ? 0.12f : 0.f;
	const float TitleEnd = IntroEnd + (bHasTitle ? 0.16f : 0.f);
	const float CrawlStart = IntroEnd + (bHasTitle ? 0.06f : 0.f);

	if (bHasIntro && Progress < IntroEnd)
	{
		const float T = Progress / IntroEnd;
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(H * 0.035f));
		const FString Intro = Card.Subtitle.ToString();
		const FVector2D TextSize = Measure(Intro, Font);
		PaintLabel(OutDrawElements, LayerId, Geometry, Intro, Font, (Size - TextSize) * 0.5f, WithAlpha(IntroBlue, TitleEnvelope(T, 0.2f, 0.25f)));
	}

	if (bHasTitle && Progress >= IntroEnd && Progress < TitleEnd)
	{
		// The title starts huge and shrinks toward the horizon.
		const float T = (Progress - IntroEnd) / (TitleEnd - IntroEnd);
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(H * 0.12f));
		const FString Title = Card.Title.ToString().ToUpper();
		const FVector2D TextSize = Measure(Title, Font);
		const float Scale = FMath::Lerp(1.6f, 0.02f, FMath::Pow(T, 0.6f));
		PaintLabel(OutDrawElements, LayerId, Geometry, Title, Font, Size * 0.5f - TextSize * Scale * 0.5f, WithAlpha(Gold, 1.f - FMath::SmoothStep(0.75f, 1.f, T)), Scale);
	}

	if (Progress < CrawlStart || Card.Body.IsEmpty())
	{
		return;
	}

	// Lay the body out once per screen width: centered lines about half the screen wide at the near edge.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(H * 0.05f));
	const FString Source = Card.Body.ToString();
	if (!FMath::IsNearlyEqual(CrawlLayoutWidth, W) || CrawlLayoutSource != Source)
	{
		CrawlLayoutWidth = W;
		CrawlLayoutSource = Source;
		CrawlLines.Reset();
		const float MaxWidth = W * 0.55f;
		TArray<FString> Paragraphs;
		Source.ParseIntoArrayLines(Paragraphs, false);
		for (const FString& Paragraph : Paragraphs)
		{
			TArray<FString> Words;
			Paragraph.ParseIntoArrayWS(Words);
			FString Line;
			for (const FString& Word : Words)
			{
				const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				if (!Line.IsEmpty() && Measure(Candidate, Font).X > MaxWidth)
				{
					CrawlLines.Add(Line);
					Line = Word;
				}
				else
				{
					Line = Candidate;
				}
			}
			CrawlLines.Add(Line); // empty paragraphs become blank lines
		}
	}

	// The text lies on a plane tilted away from camera: further up the screen = further away = smaller.
	const float LineHeight = Measure(TEXT("Ag"), Font).Y * 1.15f;
	const float T = (Progress - CrawlStart) / FMath::Max(1.f - CrawlStart, KINDA_SMALL_NUMBER);
	const float TotalTravel = (CrawlLines.Num() + 14) * LineHeight;
	const float Scroll = T * TotalTravel;
	const float Horizon = H * 0.08f;
	const float Depth = LineHeight * 6.f;

	for (int32 Index = 0; Index < CrawlLines.Num(); ++Index)
	{
		const float Along = Scroll - Index * LineHeight; // distance up the plane from the bottom edge
		if (Along < 0.f)
		{
			continue;
		}
		const float Perspective = 1.f / (1.f + Along / Depth);
		if (Perspective < 0.05f)
		{
			continue;
		}
		const float ScreenY = Horizon + (H - Horizon) * Perspective - LineHeight * Perspective;
		const FVector2D TextSize = Measure(CrawlLines[Index], Font);
		const float Fade = FMath::SmoothStep(0.05f, 0.18f, Perspective);
		PaintLabel(OutDrawElements, LayerId, Geometry, CrawlLines[Index], Font, FVector2D(W * 0.5f - TextSize.X * Perspective * 0.5f, ScreenY),
			WithAlpha(Gold, Fade * Alpha), Perspective);
	}
}
