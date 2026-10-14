#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UCinemafyScriptProject;
class FAssetThumbnailPool;
class SVerticalBox;
struct FCinemafyScriptScene;

/**
 * The storyteller's home screen: every scene of a script as a card, grouped by act and chapter, colored by how
 * far along it is, with previs / open / direct / takes one click away.
 */
class SCinemafyStoryBoard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCinemafyStoryBoard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCinemafyStoryBoard() override;

	static const FName TabName;

	/** Shows a project on the open board (and remembers it for next time). */
	static void SetProject(UCinemafyScriptProject* Project);

private:
	void Rebuild();
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeCard(const FCinemafyScriptScene& Scene);
	TSharedRef<SWidget> MakeProjectPicker();
	void OnObjectChanged(UObject* Object, struct FPropertyChangedEvent& Event);

	TWeakObjectPtr<UCinemafyScriptProject> Project;
	TSharedPtr<SVerticalBox> Body;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	FDelegateHandle ChangedHandle;

	static TWeakPtr<SCinemafyStoryBoard> Open;
};
