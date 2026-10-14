#include "CinemafyChoreoTools.h"
#include "CinemafyChoreography.h"
#include "CinemafyChoreographyLibrary.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafySceneDirector.h"
#include "CinemafySequencerUtils.h"
#include "Algo/BinarySearch.h"
#include "AnimationRecorder.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Character.h"
#include "HitProxies.h"
#include "IAssetTools.h"
#include "ISequencer.h"
#include "LevelEditorViewport.h"
#include "LevelSequence.h"
#include "Misc/MessageDialog.h"
#include "MovieScene.h"
#include "PlayInEditorDataTypes.h"
#include "SceneView.h"
#include "ScopedTransaction.h"
#include "SEditorViewport.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinemafyChoreo"

TSharedPtr<FCinemafyChoreoMode> FCinemafyChoreoMode::Instance;

namespace
{
	const FText TakeTrackName = LOCTEXT("TakeTrack", "Cinemafy Take");
	const TCHAR* TakeFolder = TEXT("/Game/Cinemafy/Takes");

	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	void Toast(const FText& Text, bool bSuccess = true)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 5.f;
		Info.bUseSuccessFailIcons = true;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	FString PerformerLabel(const ACharacter* Performer)
	{
		return Performer ? Performer->GetActorLabel() : FString(TEXT("nobody yet"));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Click mode
// ---------------------------------------------------------------------------------------------------------------------

bool FCinemafyChoreoMode::IsActive()
{
	return Instance.IsValid();
}

void FCinemafyChoreoMode::Start(ACinemafyChoreography* InChoreography)
{
	Stop();
	if (!InChoreography || !FSlateApplication::IsInitialized())
	{
		return;
	}

	Instance = MakeShared<FCinemafyChoreoMode>();
	Instance->Choreography = InChoreography;
	FSlateApplication::Get().RegisterInputPreProcessor(Instance);

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(InChoreography, true, true);

	TWeakObjectPtr<ACinemafyChoreography> Weak = InChoreography;
	FNotificationInfo Info(FText::GetEmpty());
	Info.bFireAndForget = false;
	Info.FadeOutDuration = 0.5f;
	Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("PerformRecord", "Perform & Record"),
		LOCTEXT("PerformRecordTip", "Play it, record it and bake it into Sequencer."),
		FSimpleDelegate::CreateLambda([Weak]()
		{
			FCinemafyChoreoMode::Stop();
			if (ACinemafyChoreography* Choreo = Weak.Get())
			{
				CinemafyChoreoTake::PerformAndRecord(Choreo);
			}
		}), SNotificationItem::CS_Pending));
	Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("Done", "Done"), LOCTEXT("DoneTip", "Leave choreograph mode (Esc)."),
		FSimpleDelegate::CreateStatic(&FCinemafyChoreoMode::Stop), SNotificationItem::CS_Pending));
	Instance->Banner = FSlateNotificationManager::Get().AddNotification(Info);
	if (Instance->Banner.IsValid())
	{
		Instance->Banner->SetCompletionState(SNotificationItem::CS_Pending);
	}
	Instance->UpdateBanner();
}

void FCinemafyChoreoMode::Stop()
{
	if (!Instance.IsValid())
	{
		return;
	}
	TSharedPtr<FCinemafyChoreoMode> Old = Instance;
	Instance.Reset();
	Old->ClosePrompt();
	if (Old->Banner.IsValid())
	{
		Old->Banner->SetCompletionState(SNotificationItem::CS_None);
		Old->Banner->ExpireAndFadeout();
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Old);
	}
}

void FCinemafyChoreoMode::UpdateBanner()
{
	if (!Banner.IsValid())
	{
		return;
	}
	const ACinemafyChoreography* Choreo = Choreography.Get();
	if (!Choreo)
	{
		return;
	}
	if (!Choreo->Performer)
	{
		Banner->SetText(LOCTEXT("BannerCast", "Cinemafy Choreograph: click the character who performs."));
		return;
	}
	Banner->SetText(FText::Format(LOCTEXT("BannerBeats",
		"Choreographing {0}: {1} beat(s).\nClick where something happens, then type it (walk, vault, sprint, climb, sword attack, taunt, or a line in quotes). Esc when done."),
		FText::FromString(PerformerLabel(Choreo->Performer)), Choreo->Beats.Num()));
}

void FCinemafyChoreoMode::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FCinemafyChoreoMode::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Escape)
	{
		return false;
	}
	if (PromptMenu.IsValid())
	{
		ClosePrompt();
		return true;
	}
	// Leaving from inside an input handler: defer so Slate isn't iterating us.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FCinemafyChoreoMode::Stop(); return false; }));
	return true;
}

bool FCinemafyChoreoMode::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || MouseEvent.IsAltDown() || MouseEvent.IsControlDown() || PromptMenu.IsValid())
	{
		return false;
	}
	ACinemafyChoreography* Choreo = Choreography.Get();
	UWorld* World = GetEditorWorld();
	if (!Choreo || !World || GEditor->PlayWorld)
	{
		return false;
	}

	// Only clicks on the 3D view itself (not its toolbars or other panels).
	const FVector2D Screen = MouseEvent.GetScreenSpacePosition();
	FWidgetPath Path = SlateApp.LocateWindowUnderMouse(Screen, SlateApp.GetInteractiveTopLevelWindows());
	if (!Path.IsValid() || Path.Widgets.Num() == 0 || Path.Widgets.Last().Widget->GetType() != FName(TEXT("SViewport")))
	{
		return false;
	}
	const FGeometry& ViewportGeometry = Path.Widgets.Last().Geometry;

	FLevelEditorViewportClient* Client = nullptr;
	for (FLevelEditorViewportClient* Candidate : GEditor->GetLevelViewportClients())
	{
		if (Candidate && Candidate->Viewport && Candidate->GetEditorViewportWidget().IsValid()
			&& Candidate->GetEditorViewportWidget()->GetCachedGeometry().IsUnderLocation(Screen))
		{
			Client = Candidate;
			break;
		}
	}
	if (!Client)
	{
		return false;
	}

	const FVector2D Local = ViewportGeometry.AbsoluteToLocal(Screen);
	const FVector2D LocalSize = ViewportGeometry.GetLocalSize();
	const FIntPoint ViewportSize = Client->Viewport->GetSizeXY();
	if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
	{
		return false;
	}
	const int32 X = FMath::Clamp(FMath::RoundToInt(Local.X / LocalSize.X * ViewportSize.X), 0, ViewportSize.X - 1);
	const int32 Y = FMath::Clamp(FMath::RoundToInt(Local.Y / LocalSize.Y * ViewportSize.Y), 0, ViewportSize.Y - 1);

	// What's under the cursor (the same way the editor selects things)...
	AActor* Clicked = nullptr;
	if (HHitProxy* Proxy = Client->Viewport->GetHitProxy(X, Y))
	{
		if (HActor* ActorProxy = HitProxyCast<HActor>(Proxy))
		{
			Clicked = ActorProxy->Actor;
		}
	}

	// ...and exactly where.
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Client->Viewport, Client->GetScene(), Client->EngineShowFlags)
		.SetRealtimeUpdate(Client->IsRealtime()));
	FSceneView* View = Client->CalcSceneView(&ViewFamily);
	const FViewportCursorLocation Cursor(View, Client, X, Y);
	const FVector Origin = Cursor.GetOrigin();
	const FVector Direction = Cursor.GetDirection();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CinemafyChoreoClick), true, Choreo);
	FHitResult Hit;
	FVector Location;
	if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 1000000.f, ECC_Visibility, Params))
	{
		Location = Hit.ImpactPoint;
		if (!Clicked)
		{
			Clicked = Hit.GetActor();
		}
	}
	else
	{
		// Nothing solid: the plane the performer stands on.
		const float PlaneZ = Choreo->Performer ? Choreo->Performer->GetActorLocation().Z - Choreo->Performer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		if (FMath::Abs(Direction.Z) < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		Location = Origin + Direction * ((PlaneZ - Origin.Z) / Direction.Z);
	}

	// Characters stand where their feet are.
	if (ACharacter* Character = Cast<ACharacter>(Clicked))
	{
		Location = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	// No performer yet: the first character clicked is cast.
	if (!Choreo->Performer)
	{
		if (ACharacter* Character = Cast<ACharacter>(Clicked))
		{
			const FScopedTransaction Transaction(LOCTEXT("CastPerformer", "Cast Performer"));
			Choreo->Modify();
			Choreo->Performer = Character;
			if (Choreo->Beats.Num() == 0)
			{
				Choreo->SetActorLocation(Character->GetActorLocation());
			}
			Choreo->ReadPrompts();
			UpdateBanner();
		}
		return true;
	}

	OpenPrompt(Screen, Location, Clicked, Client->GetEditorViewportWidget().ToSharedRef());
	return true;
}

void FCinemafyChoreoMode::OpenPrompt(const FVector2D& ScreenPosition, const FVector& WorldLocation, AActor* ClickedActor, TSharedRef<SWidget> Parent)
{
	PendingLocation = WorldLocation;
	PendingActor = ClickedActor;

	TWeakPtr<FCinemafyChoreoMode> WeakSelf = AsShared();
	const ACinemafyChoreography* Choreo = Choreography.Get();
	const FText Title = FText::Format(LOCTEXT("PromptTitle", "Beat {0}{1}"), (Choreo ? Choreo->Beats.Num() : 0) + 1,
		Cast<APawn>(ClickedActor) && ClickedActor != (Choreo ? Choreo->Performer : nullptr)
			? FText::Format(LOCTEXT("PromptTarget", " at {0}"), FText::FromString(ClickedActor->GetActorLabel()))
			: FText::GetEmpty());

	TSharedRef<SEditableTextBox> TextBox = SNew(SEditableTextBox)
		.MinDesiredWidth(280.f)
		.HintText(LOCTEXT("PromptHint", "walk, vault, sword attack, wait 2, walk \"Stand down, Admiral\", answer \"Make me\"..."))
		.OnTextCommitted_Lambda([WeakSelf](const FText& Text, ETextCommit::Type Type)
		{
			if (Type == ETextCommit::OnEnter)
			{
				if (TSharedPtr<FCinemafyChoreoMode> Self = WeakSelf.Pin())
				{
					Self->CommitPrompt(Text.ToString());
				}
			}
		});

	TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (const TCHAR* Word : { TEXT("walk"), TEXT("run"), TEXT("sprint"), TEXT("sneak"), TEXT("vault"), TEXT("climb"), TEXT("attack"),
		TEXT("combo"), TEXT("shoot"), TEXT("taunt"), TEXT("wave"), TEXT("look"), TEXT("wait"), TEXT("say"), TEXT("answer") })
	{
		const FString Chip = Word;
		Chips->AddSlot()
		[
			SNew(SButton)
			.Text(FText::FromString(Chip))
			.OnClicked_Lambda([WeakSelf, Chip, TextBox]()
			{
				// A chip completes what's typed ("sword" + attack) or stands alone.
				const FString Typed = TextBox->GetText().ToString().TrimStartAndEnd();
				if (TSharedPtr<FCinemafyChoreoMode> Self = WeakSelf.Pin())
				{
					Self->CommitPrompt(Typed.IsEmpty() ? Chip : Typed + TEXT(" ") + Chip);
				}
				return FReply::Handled();
			})
		];
	}

	TSharedRef<SWidget> Content =
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(8.f)
		[
			SNew(SBox)
			.WidthOverride(320.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					TextBox
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Chips
				]
			]
		];

	PromptMenu = FSlateApplication::Get().PushMenu(Parent, FWidgetPath(), Content, ScreenPosition, FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup), true);
	if (PromptMenu.IsValid())
	{
		PromptMenu->GetOnMenuDismissed().AddLambda([WeakSelf](TSharedRef<IMenu>)
		{
			if (TSharedPtr<FCinemafyChoreoMode> Self = WeakSelf.Pin())
			{
				Self->PromptMenu.Reset();
			}
		});
	}
	FSlateApplication::Get().SetKeyboardFocus(TextBox, EFocusCause::SetDirectly);
}

void FCinemafyChoreoMode::CommitPrompt(const FString& Prompt)
{
	const FString Trimmed = Prompt.TrimStartAndEnd();
	ACinemafyChoreography* Choreo = Choreography.Get();
	if (Choreo && !Trimmed.IsEmpty())
	{
		const FScopedTransaction Transaction(LOCTEXT("AddBeat", "Add Choreography Beat"));
		Choreo->Modify();
		AActor* Target = PendingActor.Get();
		// Only characters are targets (an attack on "the floor" shouldn't walk to the floor's pivot).
		if (!Cast<APawn>(Target) || Target == Choreo->Performer)
		{
			Target = nullptr;
		}
		const int32 Index = Choreo->AddBeat(Trimmed, PendingLocation, Target);
		const FCinemafyChoreoBeat& Beat = Choreo->Beats[Index];
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreograph: beat %d \"%s\" -> %s%s"), Index + 1, *Beat.Prompt, *Beat.Reading.Summary,
			Beat.ChosenAnimations.Num() > 0 ? *FString::Printf(TEXT(" using %s"), *GetNameSafe(Beat.ChosenAnimations[0])) : TEXT(""));
	}
	ClosePrompt();
	UpdateBanner();
}

void FCinemafyChoreoMode::ClosePrompt()
{
	if (PromptMenu.IsValid())
	{
		TSharedPtr<IMenu> Menu = PromptMenu;
		PromptMenu.Reset();
		Menu->Dismiss();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Takes
// ---------------------------------------------------------------------------------------------------------------------

namespace CinemafyChoreoTake
{
	struct FRecordedComponent
	{
		FName Name;
		TWeakObjectPtr<USkeletalMeshComponent> Live;
		TWeakObjectPtr<UAnimSequence> Animation;
	};

	struct FRecording
	{
		FName ChoreographyName;
		TWeakObjectPtr<ACinemafyChoreography> Live;
		TArray<FRecordedComponent> Components;
		TArray<float> Times;
		TArray<FTransform> Transforms;
		TArray<FCinemafyChoreoBeat> Beats;
		float Duration = 0.f;
		bool bFinished = false;
		/** Body and transform samples share this rate: the open sequence's display rate when there is one. */
		FFrameRate SampleRate = FFrameRate(30, 1);
		/** A character this choreography acted on (a defender with no choreography of its own), not the performer. */
		bool bPartner = false;
		int32 PartnerIndex = INDEX_NONE;
		FName ActorName;
	};

	TArray<FRecording> Recordings;
	FName PrimaryName;
	bool bSessionActive = false;
	FDelegateHandle PerformanceHandle;
	FDelegateHandle EndPieHandle;

	void StopComponents(FRecording& Recording)
	{
		for (FRecordedComponent& Component : Recording.Components)
		{
			if (USkeletalMeshComponent* Mesh = Component.Live.Get())
			{
				// StopRecordingAnimation returns nothing in 5.8: take the sequence before stopping.
				FAnimationRecorderManager& Recorder = FAnimationRecorderManager::Get();
				Component.Animation = Recorder.GetCurrentlyRecordingSequence(Mesh);
				Recorder.StopRecordingAnimation(Mesh, false);
			}
			Component.Live.Reset();
		}
	}

	void CopyResults(FRecording& Recording, const ACinemafyChoreography* Choreo)
	{
		if (!Choreo)
		{
			return;
		}
		Recording.Times = Choreo->RecordedTimes;
		if (Recording.bPartner)
		{
			Recording.Transforms = Choreo->PartnerTransforms.IsValidIndex(Recording.PartnerIndex) ? Choreo->PartnerTransforms[Recording.PartnerIndex] : TArray<FTransform>();
		}
		else
		{
			Recording.Transforms = Choreo->RecordedTransforms;
			Recording.Beats = Choreo->Beats;
		}
		Recording.Duration = Choreo->RecordedTimes.Num() > 0 ? Choreo->RecordedTimes.Last() : 0.f;
	}

	void StartRecording(ACinemafyChoreography* Choreo)
	{
		FRecording& Recording = Recordings.AddDefaulted_GetRef();
		Recording.ChoreographyName = Choreo->GetFName();
		Recording.Live = Choreo;

		ACharacter* Performer = Choreo->Performer;
		if (!Performer)
		{
			return;
		}
		Recording.ActorName = Performer->GetFName();

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		FAnimationRecordingSettings Settings;
		const ULevelSequence* OpenSequence = CinemafySequencer::GetOpenSequence();
		Recording.SampleRate = OpenSequence && OpenSequence->GetMovieScene() ? OpenSequence->GetMovieScene()->GetDisplayRate() : FFrameRate(30, 1);
		Settings.SampleFrameRate = Recording.SampleRate;
		Settings.Length = 0.f;
		// Bake-space contract: the actor (capsule) transform goes on a transform track; the body is recorded relative to
		// its mesh component, root bone included. Keeping the root keeps pose-relative root offsets (GASP's Offset Root
		// Bone during turns and pivots) that removing it would throw away. Travel lives only on the actor track, since
		// a character's root bone doesn't carry it, so nothing is counted twice.
		Settings.bRecordInWorldSpace = false;
		Settings.bRemoveRootAnimation = false;
		Settings.bAutoSaveAsset = false;

		auto RecordMeshes = [&](FRecording& Into, ACharacter* Subject)
		{
			TArray<USkeletalMeshComponent*> Meshes;
			Subject->GetComponents(Meshes);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				// Followers copy their leader's pose and faces are Cinemafy Face's job: record the bodies that animate.
				if (!Mesh || !Mesh->GetSkeletalMeshAsset() || Mesh->LeaderPoseComponent.IsValid() || Choreo->GetSkipRecordingComponents().Contains(Mesh->GetFName()))
				{
					continue;
				}

				// Each take is its own package. (Takes aren't saved automatically, so an earlier take this session may only
				// exist in memory, where CreateUniqueAssetName doesn't look; step past loaded names too. A clash makes the
				// recorder show a modal "asset with same name exists" and record nothing.)
				const FString BaseName = FString::Printf(TEXT("%s/%s_%s_Take"), TakeFolder, *PerformerLabel(Subject), *Mesh->GetName());
				FString PackageName, AssetName;
				AssetTools.CreateUniqueAssetName(BaseName, TEXT(""), PackageName, AssetName);
				for (int32 Suffix = 2; FindPackage(nullptr, *PackageName) != nullptr && Suffix < 1000; ++Suffix)
				{
					AssetTools.CreateUniqueAssetName(FString::Printf(TEXT("%s_%d"), *BaseName, Suffix), TEXT(""), PackageName, AssetName);
				}
				// RecordAnimation's "AssetPath" is the package it creates, not a folder: passing the folder put every take
				// inside one package named after it ("/Game/Cinemafy/Takes"), so the second take of a name always clashed.
				if (FAnimationRecorderManager::Get().RecordAnimation(Mesh, PackageName, AssetName, Settings))
				{
					Into.Components.Add({ Mesh->GetFName(), Mesh, nullptr });
				}
			}
			UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Take: recording %s (%d mesh(es))."), *PerformerLabel(Subject), Into.Components.Num());
		};

		RecordMeshes(Recording, Performer);

		// Defenders this performance acts on (with no choreography of their own) are recorded with it, once.
		const FFrameRate Rate = Recording.SampleRate;
		for (int32 Index = 0; Index < Choreo->Partners.Num(); ++Index)
		{
			ACharacter* Partner = Choreo->Partners[Index];
			if (!Partner || Recordings.ContainsByPredicate([Partner](const FRecording& R) { return R.ActorName == Partner->GetFName(); }))
			{
				continue;
			}
			// (Adding to Recordings can move it: don't touch the performer's Recording reference after this.)
			FRecording PartnerRecording;
			PartnerRecording.ChoreographyName = Choreo->GetFName();
			PartnerRecording.Live = Choreo;
			PartnerRecording.bPartner = true;
			PartnerRecording.PartnerIndex = Index;
			PartnerRecording.ActorName = Partner->GetFName();
			PartnerRecording.SampleRate = Rate;
			RecordMeshes(PartnerRecording, Partner);
			Recordings.Add(MoveTemp(PartnerRecording));
		}
	}

	void HandlePerformance(ACinemafyChoreography* Choreo, bool bStarted)
	{
		UWorld* World = Choreo ? Choreo->GetWorld() : nullptr;
		if (!bSessionActive || !World || World->WorldType != EWorldType::PIE)
		{
			return;
		}

		if (bStarted)
		{
			StartRecording(Choreo);
			return;
		}

		// The performer and any defenders it recorded.
		for (FRecording& Recording : Recordings)
		{
			if (Recording.Live.Get() == Choreo && !Recording.bFinished)
			{
				CopyResults(Recording, Choreo);
				StopComponents(Recording);
				Recording.bFinished = true;
			}
		}

		if (!Recordings.ContainsByPredicate([](const FRecording& R) { return !R.bFinished; }))
		{
			GEditor->RequestEndPlayMap();
		}
	}

	ULevelSequence* GetOrCreateSequence(const FString& Label, float Duration)
	{
		if (ULevelSequence* Open = CinemafySequencer::GetOpenSequence())
		{
			return Open;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		FString PackageName, AssetName;
		AssetTools.CreateUniqueAssetName(FString::Printf(TEXT("%s/LS_%s"), TakeFolder, *Label), TEXT(""), PackageName, AssetName);
		UPackage* Package = CreatePackage(*PackageName);
		ULevelSequence* Sequence = NewObject<ULevelSequence>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		Sequence->Initialize();
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		MovieScene->SetDisplayRate(FFrameRate(30, 1));
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		MovieScene->SetPlaybackRange(0, TickResolution.AsFrameNumber(FMath::Max(Duration, 1.f)).Value);
		FAssetRegistryModule::AssetCreated(Sequence);
		Package->MarkPackageDirty();
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Sequence);
		return Sequence;
	}

	FTransform SampleTransform(const TArray<float>& Times, const TArray<FTransform>& Transforms, float Time)
	{
		if (Times.Num() == 0)
		{
			return FTransform::Identity;
		}
		const int32 Upper = Algo::LowerBound(Times, Time);
		if (Upper <= 0)
		{
			return Transforms[0];
		}
		if (Upper >= Times.Num())
		{
			return Transforms.Last();
		}
		const float Span = Times[Upper] - Times[Upper - 1];
		const float Alpha = Span > KINDA_SMALL_NUMBER ? (Time - Times[Upper - 1]) / Span : 0.f;
		FTransform Result;
		Result.Blend(Transforms[Upper - 1], Transforms[Upper], Alpha);
		return Result;
	}

	template <typename TrackType>
	TrackType* ReplaceBindingTrack(UMovieScene* MovieScene, const FGuid& Binding, bool bOnlyTakeTracks)
	{
		const UMovieScene* ConstMovieScene = MovieScene;
		if (const FMovieSceneBinding* Existing = ConstMovieScene->FindBinding(Binding))
		{
			TArray<UMovieSceneTrack*> Old;
			for (UMovieSceneTrack* Track : Existing->GetTracks())
			{
				if (Track && Track->IsA<TrackType>() && (!bOnlyTakeTracks || Track->GetDisplayName().EqualTo(TakeTrackName)))
				{
					Old.Add(Track);
				}
			}
			for (UMovieSceneTrack* Track : Old)
			{
				MovieScene->RemoveTrack(*Track);
			}
		}
		TrackType* Track = MovieScene->AddTrack<TrackType>(Binding);
		return Track;
	}

	void BakeTransform(ULevelSequence* Sequence, UMovieScene* MovieScene, AActor* Performer, const FRecording& Recording, FFrameNumber Start)
	{
		const FGuid Binding = CinemafySequencer::BindActor(Sequence, Performer);
		if (!Binding.IsValid() || Recording.Times.Num() < 2)
		{
			return;
		}

		// Replace only the previous take's track. Hand-made transform tracks are never deleted: they're muted (eval
		// disabled) so the take plays, and can be switched back on in Sequencer.
		UMovieScene3DTransformTrack* Track = ReplaceBindingTrack<UMovieScene3DTransformTrack>(MovieScene, Binding, true);
		Track->SetDisplayName(TakeTrackName);
		const UMovieScene* ConstMovieScene = MovieScene;
		if (const FMovieSceneBinding* Existing = ConstMovieScene->FindBinding(Binding))
		{
			for (UMovieSceneTrack* Other : Existing->GetTracks())
			{
				if (Other && Other != Track && Other->IsA<UMovieScene3DTransformTrack>() && !Other->IsEvalDisabled())
				{
					Other->Modify();
					Other->SetEvalDisabled(true);
					UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Take: muted an existing transform track on %s so the take plays (not deleted)."), *Performer->GetActorLabel());
				}
			}
		}
		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
		Track->AddSection(*Section);

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const FFrameNumber End = Start + TickResolution.AsFrameNumber(Recording.Duration);
		Section->SetRange(TRange<FFrameNumber>(Start, End + 1));

		TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		if (Channels.Num() < 6)
		{
			return;
		}

		FRotator Previous = Recording.Transforms[0].Rotator();
		// Step in whole frames of the capture rate (no drifting float accumulation), converted exactly to ticks.
		const FFrameRate Rate = Recording.SampleRate;
		const int32 Frames = Rate.AsFrameTime(Recording.Duration).FloorToFrame().Value;
		for (int32 FrameIndex = 0; FrameIndex <= Frames; ++FrameIndex)
		{
			const float Time = static_cast<float>(Rate.AsSeconds(FFrameTime(FrameIndex)));
			const FTransform Sample = SampleTransform(Recording.Times, Recording.Transforms, Time);
			const FVector Location = Sample.GetLocation();
			FRotator Rotation = Sample.Rotator();
			// Unwind so yaw goes 179 -> 181, not 179 -> -179.
			Rotation.Roll = Previous.Roll + FRotator::NormalizeAxis(Rotation.Roll - Previous.Roll);
			Rotation.Pitch = Previous.Pitch + FRotator::NormalizeAxis(Rotation.Pitch - Previous.Pitch);
			Rotation.Yaw = Previous.Yaw + FRotator::NormalizeAxis(Rotation.Yaw - Previous.Yaw);
			Previous = Rotation;

			const FFrameNumber Frame = Start + FFrameRate::TransformTime(FFrameTime(FrameIndex), Rate, TickResolution).RoundToFrame();
			Channels[0]->AddLinearKey(Frame, Location.X);
			Channels[1]->AddLinearKey(Frame, Location.Y);
			Channels[2]->AddLinearKey(Frame, Location.Z);
			Channels[3]->AddLinearKey(Frame, Rotation.Roll);
			Channels[4]->AddLinearKey(Frame, Rotation.Pitch);
			Channels[5]->AddLinearKey(Frame, Rotation.Yaw);
		}
	}

	void BakeAnimations(ULevelSequence* Sequence, UMovieScene* MovieScene, ACharacter* Performer, const FRecording& Recording, FFrameNumber Start)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Performer->GetComponents(Meshes);
		for (const FRecordedComponent& Recorded : Recording.Components)
		{
			UAnimSequence* Animation = Recorded.Animation.Get();
			USkeletalMeshComponent* const* Mesh = Meshes.FindByPredicate([&Recorded](const USkeletalMeshComponent* M) { return M && M->GetFName() == Recorded.Name; });
			if (!Animation || !Mesh)
			{
				continue;
			}
			const FGuid Binding = CinemafySequencer::BindObject(Sequence, *Mesh);
			if (!Binding.IsValid())
			{
				continue;
			}
			UMovieSceneSkeletalAnimationTrack* Track = ReplaceBindingTrack<UMovieSceneSkeletalAnimationTrack>(MovieScene, Binding, true);
			Track->SetDisplayName(TakeTrackName);
			Track->AddNewAnimation(Start, Animation);
			Animation->MarkPackageDirty();

			// Binding a skeletal mesh through Sequencer auto-adds an FK Control Rig track, which overrides the
			// recorded animation with its default pose. Mute it (never delete) so the take plays.
			if (CinemafySequencer::MuteBindingTracks(MovieScene, Binding, TEXT("MovieSceneControlRigParameterTrack"), Track) > 0)
			{
				UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Take: muted a Control Rig track on %s so the recorded body animation plays (not deleted)."), *(*Mesh)->GetName());
			}
		}
	}

	void Bake()
	{
		bSessionActive = false;
		UWorld* World = GetEditorWorld();
		if (!World || Recordings.Num() == 0)
		{
			Recordings.Reset();
			return;
		}

		float LongestTake = 0.f;
		for (const FRecording& Recording : Recordings)
		{
			LongestTake = FMath::Max(LongestTake, Recording.Duration);
		}

		ACinemafyChoreography* Primary = nullptr;
		TArray<TPair<ACinemafyChoreography*, const FRecording*>> Matched;
		for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
		{
			It->bPerformOnNextPlay = false;
			if (const FRecording* Recording = Recordings.FindByPredicate([&It](const FRecording& R) { return !R.bPartner && R.ChoreographyName == It->GetFName(); }))
			{
				Matched.Add({ *It, Recording });
				if (It->GetFName() == PrimaryName)
				{
					Primary = *It;
				}
			}
		}
		if (Matched.Num() == 0)
		{
			Recordings.Reset();
			return;
		}

		ULevelSequence* Sequence = GetOrCreateSequence(PerformerLabel(Primary ? Primary->Performer.Get() : Matched[0].Key->Performer.Get()), LongestTake);
		UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
		if (!MovieScene)
		{
			Recordings.Reset();
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("BakeTake", "Bake Choreography Take"));
		Sequence->Modify();
		MovieScene->Modify();

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const FFrameNumber Start = CinemafySequencer::GetPlayheadTick(MovieScene);
		const float StartSeconds = static_cast<float>(TickResolution.AsSeconds(FFrameTime(Start)));

		int32 Baked = 0;
		int32 NeedsAttention = 0;
		for (const TPair<ACinemafyChoreography*, const FRecording*>& Pair : Matched)
		{
			ACinemafyChoreography* Choreo = Pair.Key;
			const FRecording& Recording = *Pair.Value;
			ACharacter* Performer = Choreo->Performer;
			if (!Performer || Recording.Times.Num() < 2)
			{
				continue;
			}

			BakeTransform(Sequence, MovieScene, Performer, Recording, Start);
			BakeAnimations(Sequence, MovieScene, Performer, Recording, Start);

			// The editor copy learns how the take went.
			Choreo->Modify();
			for (int32 Index = 0; Index < Choreo->Beats.Num() && Index < Recording.Beats.Num(); ++Index)
			{
				FCinemafyChoreoBeat& Beat = Choreo->Beats[Index];
				const FCinemafyChoreoBeat& Took = Recording.Beats[Index];
				Beat.StartTime = Took.StartTime;
				Beat.EndTime = Took.EndTime;
				Beat.ImpactTimes = Took.ImpactTimes;
				Beat.Events = Took.Events;
				Beat.Outcome = Took.Outcome;
				Beat.OutcomeReason = Took.OutcomeReason;
				if (Took.Outcome != ECinemafyBeatOutcome::Succeeded)
				{
					++NeedsAttention;
				}
			}
			Choreo->TakeDuration = Recording.Duration;
			Choreo->TakeStartSeconds = StartSeconds;
			Choreo->TakeSequence = Sequence;
			Choreo->RefreshLabels();
			++Baked;
		}

		// Defenders recorded alongside: their motion and reactions go on their own bindings.
		for (const FRecording& Recording : Recordings)
		{
			if (!Recording.bPartner || Recording.Times.Num() < 2)
			{
				continue;
			}
			for (TActorIterator<ACharacter> It(World); It; ++It)
			{
				if (It->GetFName() == Recording.ActorName)
				{
					BakeTransform(Sequence, MovieScene, *It, Recording, Start);
					BakeAnimations(Sequence, MovieScene, *It, Recording, Start);
					break;
				}
			}
		}

		// Make room for the take.
		const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
		const FFrameNumber TakeEnd = Start + TickResolution.AsFrameNumber(LongestTake);
		if (Range.GetUpperBoundValue() < TakeEnd)
		{
			MovieScene->SetPlaybackRange(Range.GetLowerBoundValue(), (TakeEnd - Range.GetLowerBoundValue()).Value);
		}
		CinemafySequencer::NotifyChanged();
		Recordings.Reset();

		Toast(FText::Format(LOCTEXT("Baked", "Baked {0} performance(s) into {1} ({2}s).{3}"), Baked, FText::FromString(Sequence->GetName()),
			FText::AsNumber(FMath::RoundToFloat(LongestTake * 10.f) / 10.f),
			NeedsAttention > 0 ? FText::Format(LOCTEXT("BakedProblems", " {0} beat(s) need attention: see the red and amber labels."), NeedsAttention) : FText::GetEmpty()),
			NeedsAttention == 0);

		if (Primary && Primary->bDirectAfterBake && Baked > 0)
		{
			DirectShots(Primary);
		}
	}

	void HandleEndPie(bool bIsSimulating)
	{
		if (!bSessionActive)
		{
			return;
		}
		// Stopped early: keep whatever was performed.
		for (FRecording& Recording : Recordings)
		{
			if (!Recording.bFinished)
			{
				CopyResults(Recording, Recording.Live.Get());
				StopComponents(Recording);
				Recording.bFinished = true;
			}
		}
		// The editor world is back next frame.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { Bake(); return false; }), 0.1f);
	}

	void Initialize()
	{
		PerformanceHandle = ACinemafyChoreography::OnPerformanceEvent.AddStatic(&HandlePerformance);
		EndPieHandle = FEditorDelegates::EndPIE.AddStatic(&HandleEndPie);
	}

	void Shutdown()
	{
		ACinemafyChoreography::OnPerformanceEvent.Remove(PerformanceHandle);
		FEditorDelegates::EndPIE.Remove(EndPieHandle);
		Recordings.Reset();
		bSessionActive = false;
	}

	void PerformAndRecord(ACinemafyChoreography* Primary, bool bAskAboutProblems)
	{
		if (!Primary || !GEditor)
		{
			return;
		}
		if (GEditor->PlayWorld)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AlreadyPlaying", "Stop the current play session first, then Perform & Record."));
			return;
		}
		if (!Primary->Performer || Primary->Beats.Num() == 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NothingToPerform", "Choreograph first: pick a performer and click some beats."));
			return;
		}

		FCinemafyChoreoMode::Stop();

		int32 Performers = 0;
		TArray<FString> WontWork;
		for (TActorIterator<ACinemafyChoreography> It(GetEditorWorld()); It; ++It)
		{
			ACinemafyChoreography* Choreo = *It;
			const bool bInTake = (Choreo == Primary || Choreo->bIncludeInTakes) && Choreo->Performer && Choreo->Beats.Num() > 0;
			Choreo->bPerformOnNextPlay = bInTake;
			if (bInTake)
			{
				for (int32 Index = 0; Index < Choreo->Beats.Num(); ++Index)
				{
					FCinemafyChoreoBeat& Beat = Choreo->Beats[Index];
					if (Beat.Reading.Summary.IsEmpty())
					{
						Choreo->ResolveBeat(Index);
					}
					else if (Beat.Reading.Kind == ECinemafyActionKind::Traverse)
					{
						// The level may have changed since the beat was placed: measure again.
						Beat.TraversalCheck = Choreo->MeasureTraversal(Index);
					}
					const FCinemafyTraversalCheck& Check = Beat.TraversalCheck;
					// Same rule as the take: only beats it will refuse are listed (NoObstacle still gets tried).
					if (Beat.Reading.Kind == ECinemafyActionKind::Traverse && Check.Verdict != ECinemafyTraversalVerdict::NotChecked
						&& Check.Verdict != ECinemafyTraversalVerdict::NoObstacle && !Check.bPossible)
					{
						WontWork.Add(FString::Printf(TEXT("%s, beat %d \"%s\": %s"), *GetNameSafe(Choreo->Performer), Index + 1, *Beat.Prompt, *Check.Reason));
					}
				}
				Choreo->RefreshLabels();
				++Performers;
			}
		}

		for (const FString& Problem : WontWork)
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: won't work as placed: %s"), *Problem);
		}
		if (WontWork.Num() > 0 && bAskAboutProblems)
		{
			const FText Message = FText::Format(LOCTEXT("TraversalWontWork",
				"These won't work as placed:\n\n{0}\n\nRecord anyway? (They'll be marked Failed in the take, not faked.)"),
				FText::FromString(FString::Join(WontWork, TEXT("\n"))));
			if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
			{
				for (TActorIterator<ACinemafyChoreography> It(GetEditorWorld()); It; ++It)
				{
					It->bPerformOnNextPlay = false;
				}
				return;
			}
		}

		Recordings.Reset();
		PrimaryName = Primary->GetFName();
		bSessionActive = true;

		FRequestPlaySessionParams Params;
		GEditor->RequestPlaySession(Params);

		Toast(FText::Format(LOCTEXT("Recording", "Recording a take with {0} performer(s). Play stops by itself when they're done."), Performers));
	}

	/** Lines from every choreography in the take, merged onto one timeline. */
	TArray<FCinemafyLine> MergeLines(const TArray<ACinemafyChoreography*>& Ensemble)
	{
		struct FTimedLine { float Start; FCinemafyLine Line; };
		TArray<FTimedLine> Timed;
		TArray<TPair<float, float>> Impacts; // absolute time, intensity
		TArray<float> Kills; // absolute times of killing blows
		float TakeEnd = 0.f;
		for (const ACinemafyChoreography* Choreo : Ensemble)
		{
			float Time = 0.f;
			for (const FCinemafyLine& Line : Choreo->BuildDirectorLines())
			{
				Timed.Add({ Time, Line });
				for (float Impact : Line.ImpactTimes)
				{
					Impacts.Add({ Time + Impact, 0.f });
				}
				if (Line.bLethal && Line.ImpactTimes.Num() > 0)
				{
					float LastImpact = Line.ImpactTimes[0];
					for (float Impact : Line.ImpactTimes)
					{
						LastImpact = FMath::Max(LastImpact, Impact);
					}
					Kills.Add(Time + LastImpact);
				}
				Time += Line.Duration + Line.PauseAfter;
			}
			TakeEnd = FMath::Max(TakeEnd, Time);
		}
		Timed.StableSort([](const FTimedLine& A, const FTimedLine& B) { return A.Start < B.Start; });

		// Lines that start together: whoever speaks leads (their words and audio must survive), else the more intense one.
		auto Speaks = [](const FTimedLine& T) { return !T.Line.Text.IsEmpty() || T.Line.Audio != nullptr; };
		for (int32 Index = Timed.Num() - 1; Index > 0; --Index)
		{
			if (Timed[Index].Start - Timed[Index - 1].Start < 0.8f)
			{
				const bool bLaterSpeaks = Speaks(Timed[Index]);
				const bool bEarlierSpeaks = Speaks(Timed[Index - 1]);
				if (bLaterSpeaks && (bEarlierSpeaks || (Index == 1 && Timed[Index].Start > 0.05f)))
				{
					// Two people talking over each other, or a line just after the take starts: keep both,
					// so every line's audio still lands on its first word.
					continue;
				}
				if (bLaterSpeaks || (!bEarlierSpeaks && Timed[Index].Line.Intensity > Timed[Index - 1].Line.Intensity))
				{
					// A spoken line keeps its own start, so its audio lands on its first word.
					if (!bLaterSpeaks)
					{
						Timed[Index].Start = Timed[Index - 1].Start;
					}
					Timed[Index - 1] = Timed[Index];
				}
				Timed.RemoveAt(Index);
			}
		}

		TArray<FCinemafyLine> Lines;
		for (int32 Index = 0; Index < Timed.Num(); ++Index)
		{
			FCinemafyLine Line = Timed[Index].Line;
			const float Start = Index == 0 ? 0.f : Timed[Index].Start;
			const float End = Index + 1 < Timed.Num() ? Timed[Index + 1].Start : TakeEnd;
			Line.Duration = FMath::Max(End - Start, 0.2f);
			Line.PauseAfter = 0.f;
			Line.ImpactTimes.Reset();
			for (const TPair<float, float>& Impact : Impacts)
			{
				if (Impact.Key >= Start && Impact.Key < End)
				{
					Line.ImpactTimes.Add(Impact.Key - Start);
				}
			}
			Line.bAction |= Line.ImpactTimes.Num() > 0;
			// Whichever line the killing blow lands in carries it (lines get re-cut when performers are merged).
			Line.bLethal = Kills.ContainsByPredicate([Start, End](float Kill) { return Kill >= Start && Kill < End; });
			Lines.Add(Line);
		}
		return Lines;
	}

	int32 DirectShots(ACinemafyChoreography* Choreo)
	{
		if (!Choreo)
		{
			return 0;
		}
		if (Choreo->TakeDuration <= 0.f)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoTake", "Record a take first (Perform & Record), then the director can cut around it."));
			return 0;
		}

		// Cut in the sequence the take lives in.
		ULevelSequence* Sequence = Choreo->TakeSequence.LoadSynchronous();
		if (Sequence && CinemafySequencer::GetOpenSequence() != Sequence)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Sequence);
		}
		TSharedPtr<ISequencer> Sequencer = CinemafySequencer::GetOpenSequencer();
		ULevelSequence* Open = CinemafySequencer::GetOpenSequence();
		if (!Sequencer.IsValid() || !Open)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSequence", "Open the Level Sequence the take was baked into, then Direct Shots."));
			return 0;
		}

		// Everyone who performed in the same take is in the scene.
		TArray<ACinemafyChoreography*> Ensemble;
		for (TActorIterator<ACinemafyChoreography> It(Choreo->GetWorld()); It; ++It)
		{
			if (*It == Choreo || (It->TakeDuration > 0.f && It->TakeSequence == Choreo->TakeSequence
				&& FMath::IsNearlyEqual(It->TakeStartSeconds, Choreo->TakeStartSeconds, 0.05f)))
			{
				Ensemble.Add(*It);
			}
		}

		ACinemafySceneDirector* Director = Choreo->SceneDirector;
		if (!Director)
		{
			FActorSpawnParameters Params;
			Director = Choreo->GetWorld()->SpawnActor<ACinemafySceneDirector>(Choreo->GetActorLocation(), FRotator::ZeroRotator, Params);
			if (!Director)
			{
				return 0;
			}
			Director->SetActorLabel(FString::Printf(TEXT("CinemafyDirector_%s"), *PerformerLabel(Choreo->Performer)));
			Director->Style = Choreo->DirectorStyle;
			Choreo->Modify();
			Choreo->SceneDirector = Director;
		}

		Director->Modify();
		Director->Lines = MergeLines(Ensemble);
		Director->bPlacePerformances = false;
		// The take's bodies are already baked; its spoken lines still need their audio in the cut.
		Director->bPlaceDialogueAudio = Director->Lines.ContainsByPredicate([](const FCinemafyLine& Line) { return Line.Audio != nullptr; });

		const UMovieScene* MovieScene = Open->GetMovieScene();
		Sequencer->SetLocalTimeDirectly(FFrameTime(MovieScene->GetTickResolution().AsFrameNumber(Choreo->TakeStartSeconds)));
		return UCinemafyEditorLibrary::DirectSceneInSequencer(Director);
	}
}

#undef LOCTEXT_NAMESPACE
