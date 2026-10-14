#include "SCinemafyChoices.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SCinemafyChoices::Construct(const FArguments& InArgs)
{
	Choices = InArgs._Choices;
	OnChosen = InArgs._OnChosen;

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		List->AddSlot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.65f))
			.ContentPadding(FMargin(18.f, 10.f))
			.OnClicked_Lambda([this, Index]() { Pick(Index); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::Format(NSLOCTEXT("CinemafyChoices", "Choice", "{0}.  {1}"), Index + 1, Choices[Index]))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.f, 1.f))
			]
		];
	}

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(40.f, 40.f, 40.f, 90.f))
	[
		SNew(SBox)
		.MinDesiredWidth(520.f)
		[
			List
		]
	];
}

FReply SCinemafyChoices::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Digits) && Index < Choices.Num(); ++Index)
	{
		if (InKeyEvent.GetKey() == Digits[Index])
		{
			Pick(Index);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

void SCinemafyChoices::Pick(int32 Index)
{
	if (bPicked || !Choices.IsValidIndex(Index))
	{
		return;
	}
	bPicked = true;
	if (OnChosen)
	{
		OnChosen(Index);
	}
}
