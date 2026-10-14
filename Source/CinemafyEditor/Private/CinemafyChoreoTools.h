#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

class ACinemafyChoreography;
class FEditorViewportClient;
class IMenu;
class SNotificationItem;

/**
 * Click-to-choreograph: while active, a left click in a level viewport places a beat where you
 * clicked (or casts the performer, if there isn't one yet) and pops up a box for the prompt.
 * Esc (or Done on the banner) leaves the mode; Ctrl+Z undoes beats like any other edit.
 */
class FCinemafyChoreoMode : public IInputProcessor, public TSharedFromThis<FCinemafyChoreoMode>
{
public:
	static void Start(ACinemafyChoreography* Choreography);
	static void Stop();
	static bool IsActive();

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("CinemafyChoreograph"); }

private:
	void OpenPrompt(const FVector2D& ScreenPosition, const FVector& WorldLocation, AActor* ClickedActor, TSharedRef<class SWidget> Parent);
	void CommitPrompt(const FString& Prompt);
	void ClosePrompt();
	void UpdateBanner();

	TWeakObjectPtr<ACinemafyChoreography> Choreography;
	TSharedPtr<SNotificationItem> Banner;
	TSharedPtr<IMenu> PromptMenu;
	FVector PendingLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> PendingActor;

	static TSharedPtr<FCinemafyChoreoMode> Instance;
};

/** Perform & Record: plays in editor, records every flagged choreography, then bakes the takes into Sequencer. */
namespace CinemafyChoreoTake
{
	void Initialize();
	void Shutdown();

	/** Starts the take. Primary is the choreography whose button was pressed (it gets directed afterwards). */
	/** bAskAboutProblems: show beats that can't work (e.g. an impossible vault) and ask before recording; scripts log them instead. */
	void PerformAndRecord(ACinemafyChoreography* Primary, bool bAskAboutProblems = true);

	/** Cuts shots around the last take of this choreography (and anyone who performed with it). */
	int32 DirectShots(ACinemafyChoreography* Choreography);
}
