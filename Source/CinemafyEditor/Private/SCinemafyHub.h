#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SUniformGridPanel;

/** One dockable panel with every Cinemafy tool, grouped by job. */
class SCinemafyHub : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyHub) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName TabName;

private:
	TSharedRef<SWidget> MakeSection(const FText& Title, const TArray<TSharedRef<SWidget>>& Buttons);
};
