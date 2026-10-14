#include "CinemafySubtitleSubsystem.h"
#include "SCinemafySubtitles.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

bool UCinemafySubtitleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCinemafySubtitleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCinemafySubtitleSubsystem, STATGROUP_Tickables);
}

void UCinemafySubtitleSubsystem::EnsureWidget()
{
	if (Widget.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Widget = SNew(SCinemafySubtitles);
		// Above the letterbox bars (Z 100) so text can sit over them if needed.
		Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 110);
	}
}

void UCinemafySubtitleSubsystem::ShowSubtitle(const FText& Speaker, const FText& Text, float Duration)
{
	if (!bSubtitlesEnabled || Text.IsEmpty())
	{
		ClearSubtitle();
		return;
	}

	EnsureWidget();
	if (Widget.IsValid())
	{
		Widget->SetLine(Speaker, Text);
		TimeLeft = Duration;
	}
}

void UCinemafySubtitleSubsystem::ClearSubtitle()
{
	if (Widget.IsValid())
	{
		Widget->Clear();
	}
	TimeLeft = 0.f;
}

void UCinemafySubtitleSubsystem::Tick(float DeltaTime)
{
	if (TimeLeft > 0.f)
	{
		TimeLeft -= DeltaTime;
		if (TimeLeft <= 0.f)
		{
			ClearSubtitle();
		}
	}
}

void UCinemafySubtitleSubsystem::Deinitialize()
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
