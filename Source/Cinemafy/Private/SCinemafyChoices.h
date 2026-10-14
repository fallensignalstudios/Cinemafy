#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** The story's choice buttons, shown over the game when a scene ends on a choice. Mouse, or keys 1-9. */
class SCinemafyChoices : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyChoices) {}
		SLATE_ARGUMENT(TArray<FText>, Choices)
		SLATE_ARGUMENT(TFunction<void(int32)>, OnChosen)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Pick(int32 Index);

	TArray<FText> Choices;
	TFunction<void(int32)> OnChosen;
	bool bPicked = false;
};
