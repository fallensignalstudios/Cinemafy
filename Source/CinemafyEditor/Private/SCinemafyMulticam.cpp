#include "SCinemafyMulticam.h"
#include "CinemafyEditLibrary.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafySequencerUtils.h"
#include "CineCameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ISequencer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyMulticam"

const FName SCinemafyMulticam::TabName(TEXT("CinemafyMulticam"));

namespace
{
	constexpr int32 MulticamThumbWidth = 320;
	constexpr int32 MulticamThumbHeight = 180;
	constexpr int32 MulticamMaxAngles = 9;
}

void SCinemafyMulticam::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SequenceCams", "Sequence Cameras"))
				.ToolTipText(LOCTEXT("SequenceCamsTip", "Every camera the open sequence cuts to."))
				.OnClicked_Lambda([this]() { UseSequenceCameras(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SelectedCams", "Selected Cameras"))
				.ToolTipText(LOCTEXT("SelectedCamsTip", "The cameras selected in the level, in selection order."))
				.OnClicked_Lambda([this]() { UseSelectedCameras(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bArmed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bArmed = State == ECheckBoxState::Checked; })
				.ToolTipText(LOCTEXT("ArmTip", "Armed: play the sequence and switch; your cuts replace what you played over when you stop."))
				[
					SNew(STextBlock).Text(LOCTEXT("Arm", "Record switching"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 0.f, 6.f, 6.f)
		[
			SNew(STextBlock).Text_Lambda([this]() { return GetStatusText(); }).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f)
		[
			SAssignNew(Grid, SUniformGridPanel).SlotPadding(FMargin(4.f))
		]
	];

	UseSequenceCameras();
}

SCinemafyMulticam::~SCinemafyMulticam()
{
}

FText SCinemafyMulticam::GetStatusText() const
{
	if (Angles.Num() == 0)
	{
		return LOCTEXT("NoAngles", "No cameras. Open a sequence with camera cuts, or select cameras and press Selected Cameras.");
	}
	if (bRecording)
	{
		return FText::Format(LOCTEXT("Recording", "RECORDING - {0} switch(es). Tap 1-{1}. Stop playback to commit."), RecordedTimes.Num(), Angles.Num());
	}
	if (bArmed)
	{
		return LOCTEXT("Armed", "Armed: press Space (or play in Sequencer), then tap 1-9 to cut live.");
	}
	return LOCTEXT("Idle", "Click an angle (or press 1-9 here) to cut to it at the playhead. Tick Record switching to cut live.");
}

void SCinemafyMulticam::UseSequenceCameras()
{
	SetCameras(UCinemafyEditLibrary::GetSequenceCameras());
}

void SCinemafyMulticam::UseSelectedCameras()
{
	TArray<AActor*> Cameras;
	for (AActor* Actor : UCinemafyEditorLibrary::GetSelectedActorsInOrder())
	{
		if (Actor && Actor->FindComponentByClass<UCameraComponent>())
		{
			Cameras.Add(Actor);
		}
	}
	SetCameras(Cameras);
}

void SCinemafyMulticam::SetCameras(const TArray<AActor*>& Cameras)
{
	Angles.Reset();
	for (AActor* Camera : Cameras)
	{
		if (Angles.Num() >= MulticamMaxAngles)
		{
			break;
		}
		FAngle& Angle = Angles.AddDefaulted_GetRef();
		Angle.Camera = Camera;
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		Target->ClearColor = FLinearColor::Black;
		Target->InitAutoFormat(MulticamThumbWidth, MulticamThumbHeight);
		Target->UpdateResourceImmediate(true);
		Angle.Target.Reset(Target);
		Angle.Brush = MakeShared<FSlateBrush>();
		Angle.Brush->SetResourceObject(Target);
		Angle.Brush->ImageSize = FVector2D(MulticamThumbWidth, MulticamThumbHeight);
		Angle.Brush->DrawAs = ESlateBrushDrawType::Image;
	}
	RebuildGrid();
	CaptureAngles();
}

void SCinemafyMulticam::RebuildGrid()
{
	if (!Grid.IsValid())
	{
		return;
	}
	Grid->ClearChildren();
	const int32 Columns = Angles.Num() <= 4 ? 2 : 3;
	for (int32 Index = 0; Index < Angles.Num(); ++Index)
	{
		const FAngle& Angle = Angles[Index];
		const FString Name = Angle.Camera.IsValid() ? Angle.Camera->GetActorLabel() : FString(TEXT("(gone)"));
		Grid->AddSlot(Index % Columns, Index / Columns)
		[
			SNew(SBorder)
			.Padding(3.f)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this, Index]() -> FSlateColor
			{
				// Program (on air) in red, everything else dark.
				return Index == LiveIndex ? FLinearColor(0.9f, 0.1f, 0.1f) : FLinearColor(0.05f, 0.05f, 0.05f);
			})
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked_Lambda([this, Index]() { SwitchTo(Index); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(MulticamThumbWidth).HeightOverride(MulticamThumbHeight)
						[
							SNew(SImage).Image(Angle.Brush.Get())
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Index + 1, *Name)))
					]
				]
			]
		];
	}
}

void SCinemafyMulticam::CaptureAngles()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || Angles.Num() == 0)
	{
		return;
	}
	// A throwaway, owner-less capture (like the Shot Browser's): nothing is added to the level.
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->RegisterComponentWithWorld(World);

	for (FAngle& Angle : Angles)
	{
		const AActor* CameraActor = Angle.Camera.Get();
		UCameraComponent* Camera = CameraActor ? CameraActor->FindComponentByClass<UCameraComponent>() : nullptr;
		if (!Camera || !Angle.Target.IsValid())
		{
			continue;
		}
		FMinimalViewInfo View;
		Camera->GetCameraView(0.f, View);
		Capture->TextureTarget = Angle.Target.Get();
		Capture->SetWorldLocationAndRotation(View.Location, View.Rotation);
		Capture->FOVAngle = View.FOV;
		Capture->PostProcessSettings = View.PostProcessSettings;
		Capture->PostProcessBlendWeight = View.PostProcessBlendWeight;
		Capture->PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
		Capture->PostProcessSettings.AutoExposureSpeedUp = 100.f;
		Capture->PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
		Capture->PostProcessSettings.AutoExposureSpeedDown = 100.f;
		Capture->CaptureScene();
	}

	Capture->UnregisterComponent();
	Capture->MarkAsGarbage();
}

void SCinemafyMulticam::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	const bool bPlaying = Sequencer.IsValid() && Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing;
	const float Now = Sequencer.IsValid() ? static_cast<float>(Sequencer->GetLocalTime().AsSeconds()) : 0.f;

	// Program monitor: what's on air right now.
	if (!bRecording)
	{
		const AActor* OnAir = UCinemafyEditLibrary::GetCameraAtPlayhead();
		LiveIndex = Angles.IndexOfByPredicate([OnAir](const FAngle& Angle) { return OnAir && Angle.Camera.Get() == OnAir; });
	}

	if (bPlaying && !bWasPlaying && bArmed && Angles.Num() > 0)
	{
		// Rolling: start from whatever's on air (or angle 1).
		bRecording = true;
		RecordedCameras.Reset();
		RecordedTimes.Reset();
		const int32 Start = LiveIndex != INDEX_NONE ? LiveIndex : 0;
		RecordedCameras.Add(Angles[Start].Camera);
		RecordedTimes.Add(Now);
		LiveIndex = Start;
	}
	if (!bPlaying && bWasPlaying && bRecording)
	{
		Commit();
	}
	if (bPlaying)
	{
		LastTime = Now;
	}
	bWasPlaying = bPlaying;

	RefreshCountdown -= InDeltaTime;
	if (RefreshCountdown <= 0.f)
	{
		RefreshCountdown = bPlaying ? 0.15f : 0.75f;
		CaptureAngles();
	}
}

FReply SCinemafyMulticam::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	static const FKey PadDigits[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 Index = 0; Index < 9; ++Index)
	{
		if (InKeyEvent.GetKey() == Digits[Index] || InKeyEvent.GetKey() == PadDigits[Index])
		{
			SwitchTo(Index);
			return FReply::Handled();
		}
	}
	if (InKeyEvent.GetKey() == EKeys::SpaceBar)
	{
		if (const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer())
		{
			const bool bPlaying = Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing;
			Sequencer->SetPlaybackStatus(bPlaying ? EMovieScenePlayerStatus::Stopped : EMovieScenePlayerStatus::Playing);
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SCinemafyMulticam::SwitchTo(int32 Index)
{
	if (!Angles.IsValidIndex(Index) || !Angles[Index].Camera.IsValid())
	{
		return;
	}

	if (bRecording)
	{
		const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
		const float Now = Sequencer.IsValid() ? static_cast<float>(Sequencer->GetLocalTime().AsSeconds()) : LastTime;
		if (Index != LiveIndex && Now > RecordedTimes.Last())
		{
			RecordedCameras.Add(Angles[Index].Camera);
			RecordedTimes.Add(Now);
		}
		LiveIndex = Index;
		return;
	}

	// Not rolling: cut to this angle at the playhead.
	if (ACineCameraActor* Cine = Cast<ACineCameraActor>(Angles[Index].Camera.Get()))
	{
		UCinemafyEditorLibrary::AddCameraToOpenSequence(Cine, true);
		LiveIndex = Index;
	}
}

void SCinemafyMulticam::Commit()
{
	bRecording = false;
	TArray<AActor*> Cameras;
	TArray<float> Times;
	for (int32 Index = 0; Index < RecordedCameras.Num(); ++Index)
	{
		if (AActor* Camera = RecordedCameras[Index].Get())
		{
			Cameras.Add(Camera);
			Times.Add(RecordedTimes[Index]);
		}
	}
	if (Cameras.Num() > 0 && LastTime > Times[0])
	{
		UCinemafyEditLibrary::ApplyMulticamCuts(Cameras, Times, LastTime);
	}
	RecordedCameras.Reset();
	RecordedTimes.Reset();
}

#undef LOCTEXT_NAMESPACE
