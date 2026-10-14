#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

/** Bottom-of-screen subtitle line: "SPEAKER: text", with a soft shadow for readability on any shot. */
class SCinemafySubtitles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafySubtitles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetLine(const FText& Speaker, const FText& Text);
	void Clear();

private:
	TSharedPtr<STextBlock> TextBlock;
};
