#include "CinemafyEditDecisions.h"
#include "CinemafyTransitionSubsystem.h"
#include "SCinemafyTitles.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

ACinemafyEditDecisions::ACinemafyEditDecisions()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// After Sequencer has evaluated the cues for this frame.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetHidden(true);
}

void ACinemafyEditDecisions::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Transitions fire once when their cue is reached.
	if (TransitionCue < 0)
	{
		TriggeredTransitionCue = -1;
	}
	else if (TransitionCue != TriggeredTransitionCue && Transitions.IsValidIndex(TransitionCue))
	{
		TriggeredTransitionCue = TransitionCue;
		const FCinemafyEditTransition& Transition = Transitions[TransitionCue];
		if (UCinemafyTransitionSubsystem* Overlay = World->GetSubsystem<UCinemafyTransitionSubsystem>())
		{
			Overlay->PlayTransition(Transition.Type, Transition.Duration, Transition.Direction, Transition.OutgoingCamera);
		}
	}

	// Titles follow the keyed progress every frame (so they scrub and pause with the sequence).
	if (UCinemafyTitleSubsystem* TitlesOverlay = World->GetSubsystem<UCinemafyTitleSubsystem>())
	{
		if (Titles.IsValidIndex(TitleCue))
		{
			TitlesOverlay->ShowTitle(Titles[TitleCue], TitleProgress);
		}
		else
		{
			TitlesOverlay->HideTitle();
		}
	}
}

bool UCinemafyTitleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCinemafyTitleSubsystem::ShowTitle(const FCinemafyTitleCard& Card, float Progress)
{
	if (!Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Widget = SNew(SCinemafyTitles);
			// Over the letterbox (100) and subtitles (110), under transitions.
			Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 115);
		}
	}
	if (Widget.IsValid())
	{
		Widget->SetTitle(Card, Progress);
	}
}

void UCinemafyTitleSubsystem::HideTitle()
{
	if (Widget.IsValid())
	{
		Widget->Clear();
	}
}

void UCinemafyTitleSubsystem::Deinitialize()
{
	if (Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
		Widget.Reset();
	}
	Super::Deinitialize();
}
