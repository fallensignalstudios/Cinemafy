#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "CinemafyTypes.h"
#include "CinemafyEditDecisions.h"

/** Cinemafy Edit: video-editor tools for the open Level Sequence (edits, transitions, titles, audio, markers, grade). */
class SCinemafyEditPanel : public SCompoundWidget
{
public:
	static const FName TabName;

	SLATE_BEGIN_ARGS(SCinemafyEditPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeSection(const FText& Title, const TArray<TSharedRef<SWidget>>& Rows);
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& Tooltip, TFunction<void()> OnClick);
	TSharedRef<SWidget> MakeLabeled(const FText& Label, TSharedRef<SWidget> Content);
	template <typename TEnum>
	TSharedRef<SWidget> MakeEnumPicker(TEnum* Value);

	ECinemafyTransition TransitionType = ECinemafyTransition::Wipe;
	ECinemafyWipeDirection WipeDirection = ECinemafyWipeDirection::Alternate;
	float TransitionSeconds = 0.7f;

	FCinemafyTitleCard Title;

	float DuckDecibels = -10.f;
	int32 SplitFrames = 6;
	FString MarkerLabel;
};
