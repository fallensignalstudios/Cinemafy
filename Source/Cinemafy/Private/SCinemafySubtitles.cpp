#include "SCinemafySubtitles.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SNullWidget.h"

void SCinemafySubtitles::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		// Sits just above where 2.39 letterbox bars end on a 16:9 screen.
		.Padding(FMargin(80.f, 0.f, 80.f, 0.f))
		[
			SNew(SBox)
			.Padding(FMargin(0.f, 0.f, 0.f, 70.f))
			[
				SAssignNew(TextBlock, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.5f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		]
	];
}

void SCinemafySubtitles::SetLine(const FText& Speaker, const FText& Text)
{
	if (TextBlock.IsValid())
	{
		TextBlock->SetText(Speaker.IsEmpty() ? Text : FText::Format(NSLOCTEXT("Cinemafy", "SubtitleLine", "{0}: {1}"), Speaker, Text));
	}
}

void SCinemafySubtitles::Clear()
{
	if (TextBlock.IsValid())
	{
		TextBlock->SetText(FText::GetEmpty());
	}
}
