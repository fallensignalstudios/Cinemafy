#include "SCinemafyShotBrowser.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafySceneDirector.h"
#include "EngineUtils.h"
#include "Selection.h"
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
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "CinemafyShotBrowser"

const FName SCinemafyShotBrowser::TabName(TEXT("CinemafyShotBrowser"));
TWeakPtr<SCinemafyShotBrowser> SCinemafyShotBrowser::ActiveInstance;

void SCinemafyShotBrowser::RefreshActive()
{
	if (const TSharedPtr<SCinemafyShotBrowser> Browser = ActiveInstance.Pin())
	{
		Browser->Refresh();
	}
}

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
			.AutoWidth()
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Check", "Check Continuity"))
				.ToolTipText(LOCTEXT("CheckTip", "Review every cut: line crossings, jump cuts, flash cuts, overlong shots."))
				.OnClicked_Lambda([]() { UCinemafyEditorLibrary::CheckSequenceContinuity(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SCinemafyShotBrowser::GetSummaryText)
				.ToolTipText_Lambda([this]()
				{
					const ACinemafySceneDirector* Director = GetActiveDirector();
					return Director && !Director->LastDecisionSummary.IsEmpty()
						? FText::FromString(Director->LastDecisionSummary)
						: LOCTEXT("NoDecisions", "Direct a scene to see how the director read it.");
				})
				.ColorAndOpacity(FSlateColor(DimTextColor))
			]
			// Director controls.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SCinemafyShotBrowser::GetDirectorText)
				.ColorAndOpacity(FSlateColor(DimTextColor))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Direct", "Direct"))
				.ToolTipText(LOCTEXT("DirectTip", "Cut the director's scene into this sequence at the playhead."))
				.IsEnabled_Lambda([this]() { return GetActiveDirector() != nullptr; })
				.OnClicked_Lambda([this]()
				{
					UCinemafyEditorLibrary::DirectSceneInSequencer(GetActiveDirector());
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Reroll", "Re-roll"))
				.ToolTipText(LOCTEXT("RerollTip", "Same scene, another take."))
				.IsEnabled_Lambda([this]() { return GetActiveDirector() != nullptr; })
				.OnClicked_Lambda([this]()
				{
					if (ACinemafySceneDirector* Director = GetActiveDirector())
					{
						Director->Modify();
						Director->Seed = FMath::Rand();
						UCinemafyEditorLibrary::DirectSceneInSequencer(Director);
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveTake", "Save Take"))
				.ToolTipText(LOCTEXT("SaveTakeTip", "Keep this cut as a take you can come back to."))
				.IsEnabled_Lambda([this]() { return GetActiveDirector() != nullptr; })
				.OnClicked_Lambda([this]()
				{
					UCinemafyEditorLibrary::SaveDirectorTake(GetActiveDirector(), FString());
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("TakesTip", "Flip between saved takes."))
				.IsEnabled_Lambda([this]() { const ACinemafySceneDirector* Director = GetActiveDirector(); return Director && Director->Takes.Num() > 0; })
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("Takes", "Takes"))
				]
				.OnGetMenuContent(this, &SCinemafyShotBrowser::MakeTakesMenu)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Learn", "Learn Style"))
				.ToolTipText(LOCTEXT("LearnTip", "Study this sequence's edit and make it the director's Custom style."))
				.IsEnabled_Lambda([this]() { return GetActiveDirector() != nullptr; })
				.OnClicked_Lambda([this]()
				{
					UCinemafyEditorLibrary::LearnDirectorStyleFromSequence(GetActiveDirector());
					return FReply::Handled();
				})
			]
		]

		// Pacing: every shot as a bar, width = duration, color = wide (cool) to tight (warm).
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 0.f, 6.f, 6.f)
		[
			SNew(SBox)
			.HeightOverride(12.f)
			[
				SAssignNew(PacingStrip, SHorizontalBox)
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

	ActiveInstance = StaticCastSharedRef<SCinemafyShotBrowser>(AsShared());
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

	AttachDirectorNotes();
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
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 0.f, 4.f, 4.f)
					[
						MakeEditControls(Shot)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 0.f, 4.f, 4.f)
					[
						SNew(STextBlock)
						.Visibility(Shot->Reason.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						.Text(FText::FromString(Shot->Reason))
						.AutoWrapText(true)
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
						.ColorAndOpacity(FSlateColor(DimTextColor))
					]
				]
			]
		];
}

ACinemafySceneDirector* SCinemafyShotBrowser::GetActiveDirector() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	// The selected director wins; otherwise the first one in the level.
	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (ACinemafySceneDirector* Director = Cast<ACinemafySceneDirector>(*It))
			{
				return Director;
			}
		}
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		if (TActorIterator<ACinemafySceneDirector> It(World); It)
		{
			return *It;
		}
	}
	return nullptr;
}

FText SCinemafyShotBrowser::GetDirectorText() const
{
	const ACinemafySceneDirector* Director = GetActiveDirector();
	return Director
		? FText::Format(LOCTEXT("DirectorLabel", "Director: {0}"), FText::FromString(Director->GetActorLabel()))
		: LOCTEXT("NoDirector", "No Scene Director in the level");
}

void SCinemafyShotBrowser::AttachDirectorNotes()
{
	const ACinemafySceneDirector* Director = GetActiveDirector();
	ULevelSequence* OpenSequence = Sequence.Get();
	if (!Director || !OpenSequence || Director->LastPlan.Num() == 0)
	{
		return;
	}

	const FFrameRate TickResolution = OpenSequence->GetMovieScene()->GetTickResolution();
	const FFrameNumber SceneStart(Director->LastSceneStartTick);
	const int32 Tolerance = FMath::Max(1, TickResolution.AsFrameNumber(0.05).Value);
	// (Not "Tag": SWidget has a member of that name, and UE 5.8 treats shadowing it as an error.)
	const FName DirectorTag = Director->GetDirectorTag();

	for (const TSharedRef<FCinemafyShot>& Shot : Shots)
	{
		const AActor* Camera = Shot->Camera.Get();
		if (!Camera || !Camera->Tags.Contains(DirectorTag))
		{
			continue;
		}
		for (const FCinemafyPlannedShot& Planned : Director->LastPlan)
		{
			const FFrameNumber PlannedStart = SceneStart + TickResolution.AsFrameNumber(Planned.StartTime);
			if (FMath::Abs((PlannedStart - Shot->StartTick).Value) <= Tolerance)
			{
				Shot->PlanIndex = static_cast<int32>(&Planned - Director->LastPlan.GetData());
				Shot->bLocked = Planned.bLocked;
				Shot->Reason = Planned.Reason;
				Shot->bHasShotType = true;
				Shot->ShotType = static_cast<uint8>(Planned.ShotType);
				break;
			}
		}
	}
}

TSharedRef<SWidget> SCinemafyShotBrowser::MakeTakesMenu()
{
	FMenuBuilder Menu(true, nullptr);
	ACinemafySceneDirector* Director = GetActiveDirector();
	if (Director)
	{
		for (int32 Index = 0; Index < Director->Takes.Num(); ++Index)
		{
			const FCinemafyTake& Take = Director->Takes[Index];
			const TWeakObjectPtr<ACinemafySceneDirector> WeakDirector = Director;
			Menu.AddMenuEntry(FText::FromString(Take.Name),
				FText::Format(LOCTEXT("TakeTip", "{0} shots. Restore this take and re-bake it."), Take.Plan.Num()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([WeakDirector, Index]()
				{
					UCinemafyEditorLibrary::LoadDirectorTake(WeakDirector.Get(), Index);
				})));
		}
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SCinemafyShotBrowser::MakeEditControls(const TSharedRef<FCinemafyShot>& Shot)
{
	if (Shot->PlanIndex == INDEX_NONE)
	{
		return SNullWidget::NullWidget;
	}

	const int32 PlanIndex = Shot->PlanIndex;
	auto SmallButton = [this](const FText& Label, const FText& Tooltip, TFunction<void(ACinemafySceneDirector*)> Action)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(4.f, 1.f))
			.OnClicked_Lambda([this, Action]()
			{
				if (ACinemafySceneDirector* Director = GetActiveDirector())
				{
					Action(Director);
				}
				return FReply::Handled();
			});
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 3.f, 0.f)
		[
			SmallButton(Shot->bLocked ? LOCTEXT("Unlock", "Unlock") : LOCTEXT("Lock", "Lock"),
				LOCTEXT("LockTip", "Locked shots survive re-rolls; the director plans around them."),
				[PlanIndex](ACinemafySceneDirector* Director) { UCinemafyEditorLibrary::ToggleShotLock(Director, PlanIndex); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 3.f, 0.f)
		[
			SmallButton(LOCTEXT("Alt", "Alt"), LOCTEXT("AltTip", "Swap for the director's next-best shot here (and lock it)."),
				[PlanIndex](ACinemafySceneDirector* Director) { UCinemafyEditorLibrary::SwapShotForAlternate(Director, PlanIndex); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 3.f, 0.f)
		[
			SmallButton(LOCTEXT("Earlier", "<"), LOCTEXT("EarlierTip", "Cut into this shot a quarter second earlier."),
				[PlanIndex](ACinemafySceneDirector* Director) { UCinemafyEditorLibrary::NudgeShotCut(Director, PlanIndex, -0.25f); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SmallButton(LOCTEXT("Later", ">"), LOCTEXT("LaterTip", "Cut into this shot a quarter second later."),
				[PlanIndex](ACinemafySceneDirector* Director) { UCinemafyEditorLibrary::NudgeShotCut(Director, PlanIndex, 0.25f); })
		];
}

void SCinemafyShotBrowser::RebuildPacingStrip()
{
	if (!PacingStrip.IsValid())
	{
		return;
	}
	PacingStrip->ClearChildren();

	const TWeakPtr<SCinemafyShotBrowser> WeakThis = StaticCastSharedRef<SCinemafyShotBrowser>(AsShared());
	for (const TSharedRef<FCinemafyShot>& Shot : Shots)
	{
		// Wide = cool blue, tight = warm orange; unknown framing = neutral.
		FLinearColor Color(0.35f, 0.35f, 0.35f);
		if (Shot->bHasShotType)
		{
			const float Size = UCinemafyDirectorLibrary::GetShotSize(static_cast<ECinemafyShotType>(Shot->ShotType));
			Color = FMath::Lerp(FLinearColor(0.2f, 0.45f, 0.85f), FLinearColor(1.f, 0.5f, 0.15f), FMath::Clamp(Size / 5.f, 0.f, 1.f));
		}

		const TWeakPtr<FCinemafyShot> WeakShot = Shot;
		PacingStrip->AddSlot()
		.FillWidth(FMath::Max(Shot->DurationSeconds, 0.05f))
		.Padding(1.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.ToolTipText(FText::Format(LOCTEXT("PacingTip", "{0}  {1}  ({2})"), FText::AsNumber(Shot->Number), Shot->CameraName, FormatDuration(Shot->DurationSeconds)))
			.BorderBackgroundColor_Lambda([WeakThis, WeakShot, Color]()
			{
				const TSharedPtr<SCinemafyShotBrowser> Browser = WeakThis.Pin();
				const TSharedPtr<FCinemafyShot> PinnedShot = WeakShot.Pin();
				const bool bActive = Browser && PinnedShot && Browser->IsShotUnderPlayhead(*PinnedShot);
				return FSlateColor(bActive ? FLinearColor::White : Color);
			})
		];
	}
}

void SCinemafyShotBrowser::RebuildStrip()
{
	RebuildPacingStrip();

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
