#include "CinemafyTransitionSubsystem.h"
#include "SCinemafyTransition.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"

bool UCinemafyTransitionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCinemafyTransitionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCinemafyTransitionSubsystem, STATGROUP_Tickables);
}

void UCinemafyTransitionSubsystem::EnsureWidget()
{
	if (Widget.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Widget = SNew(SCinemafyTransition);
		// Under the letterbox (100) and subtitles (110): the transition is part of the picture.
		Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 90);
	}
}

void UCinemafyTransitionSubsystem::PlayTransition(ECinemafyTransition Type, float InDuration, ECinemafyWipeDirection Direction, AActor* InOutgoingCamera)
{
	OutgoingCamera = InOutgoingCamera;
	FixedView.Reset();
	Start(Type, InDuration, Direction, InOutgoingCamera != nullptr);
}

void UCinemafyTransitionSubsystem::PlayTransitionFromView(ECinemafyTransition Type, float InDuration, ECinemafyWipeDirection Direction, const FMinimalViewInfo& View)
{
	OutgoingCamera.Reset();
	FixedView = View;
	Start(Type, InDuration, Direction, true);
}

void UCinemafyTransitionSubsystem::Start(ECinemafyTransition Type, float InDuration, ECinemafyWipeDirection Direction, bool bCaptureOutgoing)
{
	if (Type == ECinemafyTransition::Cut || Type == ECinemafyTransition::MatchCut)
	{
		return;
	}

	EnsureWidget();
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Widget.IsValid() || !Viewport)
	{
		return;
	}

	OutgoingBrush.Reset();
	if (bCaptureOutgoing)
	{
		// Half resolution is plenty for a moving transition and keeps the extra render cheap.
		FVector2D ViewportSize;
		Viewport->GetViewportSize(ViewportSize);
		const int32 Width = FMath::Max(64, FMath::RoundToInt(ViewportSize.X * 0.5f));
		const int32 Height = FMath::Max(36, FMath::RoundToInt(ViewportSize.Y * 0.5f));

		if (!RenderTarget)
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		}
		RenderTarget->InitAutoFormat(Width, Height);
		RenderTarget->UpdateResourceImmediate(true);

		if (!Capture)
		{
			Capture = NewObject<USceneCaptureComponent2D>(this);
			Capture->bCaptureOnMovement = false;
			Capture->bAlwaysPersistRenderingState = true;
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->RegisterComponentWithWorld(GetWorld());
		}
		Capture->TextureTarget = RenderTarget;
		Capture->bCaptureEveryFrame = true;
		UpdateCapture();

		OutgoingBrush = MakeShared<FSlateBrush>();
		OutgoingBrush->SetResourceObject(RenderTarget);
		OutgoingBrush->ImageSize = FVector2D(Width, Height);
		OutgoingBrush->DrawAs = ESlateBrushDrawType::Image;
	}

	Duration = FMath::Max(InDuration, 0.05f);
	TimeLeft = Duration;
	Widget->Begin(Type, Direction, OutgoingBrush.Get());
}

void UCinemafyTransitionSubsystem::UpdateCapture()
{
	if (!Capture)
	{
		return;
	}

	FMinimalViewInfo View;
	if (FixedView.IsSet())
	{
		View = FixedView.GetValue();
	}
	else if (const AActor* Camera = OutgoingCamera.Get())
	{
		UCameraComponent* CameraComponent = Camera->FindComponentByClass<UCameraComponent>();
		if (!CameraComponent)
		{
			return;
		}
		CameraComponent->GetCameraView(0.f, View);
	}
	else
	{
		return;
	}

	Capture->SetWorldLocationAndRotation(View.Location, View.Rotation);
	Capture->FOVAngle = View.FOV;
	Capture->PostProcessSettings = View.PostProcessSettings;
	Capture->PostProcessBlendWeight = View.PostProcessBlendWeight;
}

void UCinemafyTransitionSubsystem::StopCapture()
{
	if (Capture)
	{
		Capture->bCaptureEveryFrame = false;
	}
	OutgoingCamera.Reset();
	FixedView.Reset();
}

void UCinemafyTransitionSubsystem::Tick(float DeltaTime)
{
	if (TimeLeft <= 0.f || !Widget.IsValid())
	{
		return;
	}

	TimeLeft -= DeltaTime;
	UpdateCapture();
	Widget->SetProgress(1.f - FMath::Max(TimeLeft, 0.f) / Duration);

	if (TimeLeft <= 0.f)
	{
		Widget->Finish();
		StopCapture();
	}
}

void UCinemafyTransitionSubsystem::Deinitialize()
{
	StopCapture();
	if (Capture)
	{
		Capture->UnregisterComponent();
		Capture = nullptr;
	}
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
