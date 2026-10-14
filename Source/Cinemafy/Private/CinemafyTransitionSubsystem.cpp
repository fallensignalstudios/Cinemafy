#include "CinemafyTransitionSubsystem.h"
#include "SCinemafyTransition.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"
#include "CinemafyModule.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace CinemafyTransitionCVars
{
	// Diagnostics for the outgoing-shot capture (wipes and dissolves draw a live capture of the old camera).
	static TAutoConsoleVariable<int32> Dump(TEXT("cinemafy.Transition.Dump"), 0,
		TEXT("1 = save the outgoing capture to Saved/Cinemafy/TransitionOutgoing_N.png a few frames into each transition, and log its setup."));
	static TAutoConsoleVariable<float> ManualExposure(TEXT("cinemafy.Transition.ManualExposure"), -100.f,
		TEXT("Force manual exposure on the outgoing capture with this exposure bias (EV). Default (-100) = inherit the camera's settings."));
	static TAutoConsoleVariable<int32> CaptureSource(TEXT("cinemafy.Transition.CaptureSource"), -1,
		TEXT("Override the outgoing capture's ESceneCaptureSource (e.g. 0 SceneColorHDR, 2 FinalColorLDR, 7 FinalToneCurveHDR). -1 = default."));
}

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
	FramesSinceStart = 0;
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

	// A capture that starts mid-scene has no exposure history: let it adapt instantly (as the Shot Browser does).
	Capture->PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
	Capture->PostProcessSettings.AutoExposureSpeedUp = 100.f;
	Capture->PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
	Capture->PostProcessSettings.AutoExposureSpeedDown = 100.f;

	const float ManualBias = CinemafyTransitionCVars::ManualExposure.GetValueOnGameThread();
	if (ManualBias > -99.f)
	{
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
		Capture->PostProcessSettings.AutoExposureBias = ManualBias;
	}
	const int32 Source = CinemafyTransitionCVars::CaptureSource.GetValueOnGameThread();
	Capture->CaptureSource = Source >= 0 ? static_cast<ESceneCaptureSource>(Source) : ESceneCaptureSource::SCS_FinalColorLDR;
}

void UCinemafyTransitionSubsystem::DumpOutgoing()
{
	const AActor* Camera = OutgoingCamera.Get();
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Transition dump: outgoing=%s capture=%s at %s fov=%.1f source=%d target=%s %dx%d format=%d brush=%s"),
		*GetNameSafe(Camera), Capture ? TEXT("yes") : TEXT("no"),
		Capture ? *Capture->GetComponentLocation().ToString() : TEXT("-"), Capture ? Capture->FOVAngle : 0.f,
		Capture ? static_cast<int32>(Capture->CaptureSource.GetValue()) : -1,
		*GetNameSafe(RenderTarget), RenderTarget ? RenderTarget->SizeX : 0, RenderTarget ? RenderTarget->SizeY : 0,
		RenderTarget ? static_cast<int32>(RenderTarget->GetFormat()) : -1, OutgoingBrush.IsValid() ? TEXT("render target") : TEXT("black fallback"));
	if (!RenderTarget)
	{
		return;
	}
	const FString Path = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Cinemafy/TransitionOutgoing_%d.png"), DumpCount++);
	if (TUniquePtr<FArchive> File = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*Path)))
	{
		FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *File);
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Transition dump: wrote %s"), *Path);
	}
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
	if (++FramesSinceStart == 3 && CinemafyTransitionCVars::Dump.GetValueOnGameThread() > 0)
	{
		DumpOutgoing();
	}
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
