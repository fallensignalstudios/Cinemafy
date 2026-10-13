#include "SCinemafyShotBrowser.h"
#include "CinemafySequencerUtils.h"
#include "CineCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneSequenceID.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyShotBrowser"

const FName SCinemafyShotBrowser::TabName(TEXT("CinemafyShotBrowser"));

namespace
{
	constexpr float ThumbWidth = 240.f;
	constexpr float DefaultThumbHeight = ThumbWidth * 9.f / 16.f;

	/** Shots shorter than this tend to read as flash cuts; longer ones risk dragging in a game. */
	constexpr float ShortShotSeconds = 0.75f;
	constexpr float LongShotSeconds = 12.f;

	const FLinearColor ActiveColor(1.f, 0.62f, 0.15f);
	const FLinearColor IdleColor(0.08f, 0.08f, 0.08f);
	const FLinearColor WarningColor(1.f, 0.55f, 0.2f);
	const FLinearColor DimTextColor(0.65f, 0.65f, 0.65f);

	FText FormatDuration(float Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%.1fs"), Seconds));
	}
}

void SCinemafyShotBrowser::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTip", "Re-read the camera cuts of the sequence open in Sequencer."))
				.OnClicked_Lambda([this]() { Refresh(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Capture", "Capture Thumbnails"))
				.ToolTipText(LOCTEXT("CaptureTip", "Render each shot through its camera at the first frame of its cut."))
				.OnClicked_Lambda([this]() { CaptureThumbnails(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SCinemafyShotBrowser::GetSummaryText)
				.ColorAndOpacity(FSlateColor(DimTextColor))
			]
		]

		// Film strip
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SAssignNew(Strip, SHorizontalBox)
			]
		]
	];

	Refresh();
}

FText SCinemafyShotBrowser::GetSummaryText() const
{
	if (!Sequence.IsValid())
	{
		return LOCTEXT("NoSequence", "Open a Level Sequence in Sequencer, then hit Refresh.");
	}

	float Total = 0.f;
	for (const TSharedRef<FCinemafyShot>& Shot : Shots)
	{
		Total += Shot->DurationSeconds;
	}

	const float Average = Shots.Num() > 0 ? Total / Shots.Num() : 0.f;
	return FText::Format(LOCTEXT("Summary", "{0}  |  {1} shots  |  {2} total  |  {3} average shot"),
		FText::FromString(Sequence->GetName()), Shots.Num(), FormatDuration(Total), FormatDuration(Average));
}

void SCinemafyShotBrowser::Refresh()
{
	Shots.Reset();

	ULevelSequence* OpenSequence = CinemafySequencer::GetOpenSequence();
	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	Sequence = OpenSequence;

	UMovieScene* MovieScene = OpenSequence ? OpenSequence->GetMovieScene() : nullptr;
	const UMovieSceneCameraCutTrack* CutTrack = MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	if (!CutTrack)
	{
		RebuildStrip();
		return;
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

	for (UMovieSceneSection* Section : CutTrack->GetAllSections())
	{
		const UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Section);
		if (!Cut)
		{
			continue;
		}

		TSharedRef<FCinemafyShot> Shot = MakeShared<FCinemafyShot>();
		Shot->StartTick = Cut->HasStartFrame() ? Cut->GetInclusiveStartFrame() : PlaybackRange.GetLowerBoundValue();
		Shot->EndTick = Cut->HasEndFrame() ? Cut->GetExclusiveEndFrame() : PlaybackRange.GetUpperBoundValue();
		Shot->DurationSeconds = static_cast<float>(TickResolution.AsSeconds(FFrameTime(Shot->EndTick - Shot->StartTick)));

		const FGuid CameraGuid = Cut->GetCameraBindingID().GetGuid();
		Shot->CameraName = MovieScene->GetObjectDisplayName(CameraGuid);

		if (Sequencer.IsValid())
		{
			for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(CameraGuid, MovieSceneSequenceID::Root))
			{
				if (AActor* Actor = Cast<AActor>(Bound.Get()))
				{
					Shot->Camera = Actor;
					if (const UCineCameraComponent* Cine = Actor->FindComponentByClass<UCineCameraComponent>())
					{
						Shot->FocalLength = Cine->CurrentFocalLength;
						Shot->Aperture = Cine->CurrentAperture;
					}
					break;
				}
			}
		}

		Shots.Add(Shot);
	}

	Shots.Sort([](const TSharedRef<FCinemafyShot>& A, const TSharedRef<FCinemafyShot>& B) { return A->StartTick < B->StartTick; });
	for (int32 Index = 0; Index < Shots.Num(); ++Index)
	{
		Shots[Index]->Number = Index + 1;
	}

	RebuildStrip();
}

void SCinemafyShotBrowser::CaptureThumbnails()
{
	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!Sequencer.IsValid() || !World || Shots.Num() == 0)
	{
		return;
	}

	// A throwaway, owner-less capture component: nothing gets added to the level or dirties it.
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->RegisterComponentWithWorld(World);

	const FQualifiedFrameTime OriginalTime = Sequencer->GetLocalTime();
	const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

	for (const TSharedRef<FCinemafyShot>& Shot : Shots)
	{
		// Evaluate the sequence at the cut so animated cameras and characters are where they'll be on screen.
		Sequencer->SetLocalTimeDirectly(FFrameTime(Shot->StartTick));
		Sequencer->ForceEvaluate();

		const AActor* CameraActor = Shot->Camera.Get();
		UCameraComponent* Camera = CameraActor ? CameraActor->FindComponentByClass<UCameraComponent>() : nullptr;
		if (!Camera)
		{
			continue;
		}

		FMinimalViewInfo View;
		Camera->GetCameraView(0.f, View);

		const float Aspect = (View.bConstrainAspectRatio && View.AspectRatio > 0.f) ? View.AspectRatio : 16.f / 9.f;
		const int32 Width = FMath::RoundToInt(ThumbWidth);
		const int32 Height = FMath::Max(1, FMath::RoundToInt(ThumbWidth / Aspect));

		UTextureRenderTarget2D* Target = Shot->Thumbnail.Get();
		if (!Target)
		{
			Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
			Shot->Thumbnail.Reset(Target);
		}
		Target->ClearColor = FLinearColor::Black;
		Target->InitAutoFormat(Width, Height);
		Target->UpdateResourceImmediate(true);

		Capture->TextureTarget = Target;
		Capture->SetWorldLocationAndRotation(View.Location, View.Rotation);
		Capture->FOVAngle = View.FOV;
		Capture->PostProcessSettings = View.PostProcessSettings;
		Capture->PostProcessBlendWeight = View.PostProcessBlendWeight;

		// One-off captures have no exposure history, so let auto exposure settle instantly.
		Capture->PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
		Capture->PostProcessSettings.AutoExposureSpeedUp = 100.f;
		Capture->PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
		Capture->PostProcessSettings.AutoExposureSpeedDown = 100.f;

		Capture->CaptureScene();
		Capture->CaptureScene();

		TSharedRef<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Target);
		Brush->ImageSize = FVector2D(Width, Height);
		Brush->DrawAs = ESlateBrushDrawType::Image;
		Shot->ThumbnailBrush = Brush;
	}

	Capture->UnregisterComponent();
	Capture->MarkAsGarbage();

	Sequencer->SetLocalTimeDirectly(ConvertFrameTime(OriginalTime.Time, OriginalTime.Rate, TickResolution));
	Sequencer->ForceEvaluate();

	RebuildStrip();
}

bool SCinemafyShotBrowser::IsShotUnderPlayhead(const FCinemafyShot& Shot) const
{
	const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
	if (!Sequencer.IsValid())
	{
		return false;
	}

	const FQualifiedFrameTime Now = Sequencer->GetLocalTime();
	const FFrameNumber Tick = ConvertFrameTime(Now.Time, Now.Rate, Sequencer->GetFocusedTickResolution()).FloorToFrame();
	return Tick >= Shot.StartTick && Tick < Shot.EndTick;
}

FReply SCinemafyShotBrowser::OnShotClicked(TSharedRef<FCinemafyShot> Shot)
{
	if (const TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer())
	{
		Sequencer->SetLocalTime(FFrameTime(Shot->StartTick));
	}

	if (AActor* Camera = Shot->Camera.Get())
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Camera, true, true);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SCinemafyShotBrowser::MakeShotCard(const TSharedRef<FCinemafyShot>& Shot)
{
	const float ThumbHeight = Shot->ThumbnailBrush.IsValid() ? Shot->ThumbnailBrush->ImageSize.Y : DefaultThumbHeight;

	TSharedRef<SWidget> Thumbnail = Shot->ThumbnailBrush.IsValid()
		? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Shot->ThumbnailBrush.Get()))
		: StaticCastSharedRef<SWidget>(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoThumb", "Capture Thumbnails to preview"))
				.ColorAndOpacity(FSlateColor(DimTextColor))
			]);

	// Lens and timing, with a pacing warning for shots that are likely too short or too long.
	FString LensInfo = Shot->FocalLength > 0.f
		? FString::Printf(TEXT("%.0fmm  f/%.1f  |  "), Shot->FocalLength, Shot->Aperture)
		: FString();
	LensInfo += FormatDuration(Shot->DurationSeconds).ToString();

	FText PacingNote;
	if (Shot->DurationSeconds < ShortShotSeconds)
	{
		PacingNote = LOCTEXT("ShortShot", "Very short: may read as a flash cut");
	}
	else if (Shot->DurationSeconds > LongShotSeconds)
	{
		PacingNote = LOCTEXT("LongShot", "Long: consider a cutaway or a camera move");
	}

	const TWeakPtr<SCinemafyShotBrowser> WeakThis = StaticCastSharedRef<SCinemafyShotBrowser>(AsShared());
	const TWeakPtr<FCinemafyShot> WeakShot = Shot;

	return SNew(SBox)
		.WidthOverride(ThumbWidth + 12.f)
		.Padding(4.f)
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
			.ContentPadding(0.f)
			.ToolTipText(LOCTEXT("CardTip", "Jump the playhead to this shot and select its camera."))
			.OnClicked(this, &SCinemafyShotBrowser::OnShotClicked, Shot)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.Padding(3.f)
				.BorderBackgroundColor_Lambda([WeakThis, WeakShot]()
				{
					const TSharedPtr<SCinemafyShotBrowser> Browser = WeakThis.Pin();
					const TSharedPtr<FCinemafyShot> PinnedShot = WeakShot.Pin();
					const bool bActive = Browser && PinnedShot && Browser->IsShotUnderPlayhead(*PinnedShot);
					return FSlateColor(bActive ? ActiveColor : IdleColor);
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(ThumbWidth)
						.HeightOverride(ThumbHeight)
						[
							Thumbnail
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 4.f, 4.f, 0.f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.Text(FText::Format(LOCTEXT("ShotTitle", "{0}  {1}"), FText::AsNumber(Shot->Number), Shot->CameraName))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 0.f, 4.f, PacingNote.IsEmpty() ? 4.f : 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(LensInfo))
						.ColorAndOpacity(FSlateColor(PacingNote.IsEmpty() ? DimTextColor : WarningColor))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 0.f, 4.f, 4.f)
					[
						SNew(STextBlock)
						.Visibility(PacingNote.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						.Text(PacingNote)
						.ColorAndOpacity(FSlateColor(WarningColor))
					]
				]
			]
		];
}

void SCinemafyShotBrowser::RebuildStrip()
{
	if (!Strip.IsValid())
	{
		return;
	}

	Strip->ClearChildren();

	if (Shots.Num() == 0)
	{
		Strip->AddSlot()
		.AutoWidth()
		.Padding(12.f)
		[
			SNew(STextBlock)
			.Text(Sequence.IsValid()
				? LOCTEXT("NoCuts", "No camera cuts yet. Tools > Cinemafy > Create Shot adds them at the playhead.")
				: LOCTEXT("NoSequenceStrip", "No Level Sequence open in Sequencer."))
			.ColorAndOpacity(FSlateColor(DimTextColor))
		];
		return;
	}

	for (const TSharedRef<FCinemafyShot>& Shot : Shots)
	{
		Strip->AddSlot()
		.AutoWidth()
		[
			MakeShotCard(Shot)
		];
	}
}

#undef LOCTEXT_NAMESPACE
