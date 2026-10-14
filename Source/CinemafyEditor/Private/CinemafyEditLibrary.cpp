#include "CinemafyEditLibrary.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyEditDecisions.h"
#include "CinemafyModule.h"
#include "CinemafySequencerUtils.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneSequenceID.h"
#include "ScopedTransaction.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Selection.h"
#include "Sound/SoundWave.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneIntegerTrack.h"

#define LOCTEXT_NAMESPACE "CinemafyEdit"

namespace CinemafyEdit
{
	const FText FadeTrackName = LOCTEXT("EditFades", "Cinemafy Edit Fades");

	/** The open sequence and everything an edit needs to know about it. */
	struct FContext
	{
		ULevelSequence* Sequence = nullptr;
		UMovieScene* MovieScene = nullptr;
		TSharedPtr<ISequencer> Sequencer;
		UMovieSceneCameraCutTrack* CutTrack = nullptr;
		FFrameNumber Playhead;
		FFrameRate TickResolution;
		FFrameRate DisplayRate;
		/** Camera cuts in time order. */
		TArray<UMovieSceneCameraCutSection*> Cuts;

		FFrameNumber StartOf(const UMovieSceneSection* Section) const
		{
			return Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : MovieScene->GetPlaybackRange().GetLowerBoundValue();
		}
		FFrameNumber EndOf(const UMovieSceneSection* Section) const
		{
			return Section->HasEndFrame() ? Section->GetExclusiveEndFrame() : MovieScene->GetPlaybackRange().GetUpperBoundValue();
		}
		FFrameNumber DisplayFramesToTicks(int32 Frames) const
		{
			return FFrameRate::TransformTime(FFrameTime(Frames), DisplayRate, TickResolution).RoundToFrame();
		}

		/** The cut whose span contains Frame. */
		int32 CutAt(FFrameNumber Frame) const
		{
			for (int32 Index = 0; Index < Cuts.Num(); ++Index)
			{
				if (StartOf(Cuts[Index]) <= Frame && Frame < EndOf(Cuts[Index]))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		/** The cut point (between Cuts[i-1] and Cuts[i]) nearest Frame, as i. */
		int32 NearestCutPoint(FFrameNumber Frame) const
		{
			int32 Best = INDEX_NONE;
			int64 BestDistance = TNumericLimits<int64>::Max();
			for (int32 Index = 1; Index < Cuts.Num(); ++Index)
			{
				const int64 Distance = FMath::Abs(static_cast<int64>(StartOf(Cuts[Index]).Value) - Frame.Value);
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					Best = Index;
				}
			}
			return Best;
		}

		AActor* CameraOf(const UMovieSceneCameraCutSection* Cut) const
		{
			if (!Sequencer.IsValid() || !Cut)
			{
				return nullptr;
			}
			for (const TWeakObjectPtr<UObject>& Bound : Sequencer->FindBoundObjects(Cut->GetCameraBindingID().GetGuid(), MovieSceneSequenceID::Root))
			{
				if (AActor* Actor = Cast<AActor>(Bound.Get()))
				{
					return Actor;
				}
			}
			return nullptr;
		}
	};

	bool GetContext(FContext& Context, bool bNeedCuts, bool bQuiet = false)
	{
		Context.Sequence = CinemafySequencer::GetOpenSequence();
		Context.MovieScene = Context.Sequence ? Context.Sequence->GetMovieScene() : nullptr;
		Context.Sequencer = CinemafySequencer::GetOpenSequencer();
		if (!Context.MovieScene)
		{
			if (!bQuiet)
			{
				UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Edit: open a Level Sequence in Sequencer first."));
			}
			return false;
		}
		Context.TickResolution = Context.MovieScene->GetTickResolution();
		Context.DisplayRate = Context.MovieScene->GetDisplayRate();
		Context.Playhead = CinemafySequencer::GetPlayheadTick(Context.MovieScene);
		Context.CutTrack = Cast<UMovieSceneCameraCutTrack>(Context.MovieScene->GetCameraCutTrack());
		if (Context.CutTrack)
		{
			for (UMovieSceneSection* Section : Context.CutTrack->GetAllSections())
			{
				if (UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Section))
				{
					Context.Cuts.Add(Cut);
				}
			}
			Context.Cuts.Sort([&Context](const UMovieSceneCameraCutSection& A, const UMovieSceneCameraCutSection& B)
			{
				return Context.StartOf(&A) < Context.StartOf(&B);
			});
		}
		if (bNeedCuts && Context.Cuts.Num() == 0)
		{
			if (!bQuiet)
			{
				UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Edit: the open sequence has no camera cuts."));
			}
			return false;
		}
		return true;
	}

	/** Every section in the sequence: root tracks, the camera cut track and every binding's tracks. */
	void ForEachSection(UMovieScene* MovieScene, TFunctionRef<void(UMovieSceneSection*)> Visit)
	{
		TArray<UMovieSceneTrack*> Tracks(MovieScene->GetTracks());
		if (UMovieSceneTrack* CutTrack = MovieScene->GetCameraCutTrack())
		{
			Tracks.Add(CutTrack);
		}
		const UMovieScene* ConstMovieScene = MovieScene;
		for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
		{
			Tracks.Append(Binding.GetTracks());
		}
		for (UMovieSceneTrack* Track : Tracks)
		{
			if (!Track)
			{
				continue;
			}
			// Copy: visitors may add or remove sections.
			const TArray<UMovieSceneSection*> Sections = Track->GetAllSections();
			for (UMovieSceneSection* Section : Sections)
			{
				if (Section)
				{
					Visit(Section);
				}
			}
		}
	}

	/** Moves every key at or after From by Delta, in every channel of the section; optionally deletes keys in [DeleteFrom, From). */
	void ShiftKeys(UMovieSceneSection* Section, FFrameNumber From, FFrameNumber Delta, TOptional<FFrameNumber> DeleteFrom = TOptional<FFrameNumber>())
	{
		for (const FMovieSceneChannelEntry& Entry : Section->GetChannelProxy().GetAllEntries())
		{
			for (FMovieSceneChannel* Channel : Entry.GetChannels())
			{
				if (!Channel)
				{
					continue;
				}
				if (DeleteFrom.IsSet())
				{
					TArray<FKeyHandle> Doomed;
					Channel->GetKeys(TRange<FFrameNumber>(DeleteFrom.GetValue(), From), nullptr, &Doomed);
					Channel->DeleteKeys(Doomed);
				}
				TArray<FFrameNumber> Times;
				TArray<FKeyHandle> Handles;
				Channel->GetKeys(TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(From), TRangeBound<FFrameNumber>::Open()), &Times, &Handles);
				for (FFrameNumber& Time : Times)
				{
					Time += Delta;
				}
				Channel->SetKeyTimes(Handles, Times);
			}
		}
	}

	/** Removes [A, B) from the whole sequence and closes the gap (a full-track ripple delete). */
	void RippleRemove(FContext& Context, FFrameNumber A, FFrameNumber B)
	{
		if (B <= A)
		{
			return;
		}
		const FFrameNumber Delta = B - A;
		const FFrameRate Tick = Context.TickResolution;
		TArray<UMovieSceneSection*> Doomed;

		ForEachSection(Context.MovieScene, [&](UMovieSceneSection* Section)
		{
			const bool bHasStart = Section->HasStartFrame();
			const bool bHasEnd = Section->HasEndFrame();
			const FFrameNumber Start = bHasStart ? Section->GetInclusiveStartFrame() : FFrameNumber(TNumericLimits<int32>::Lowest());
			const FFrameNumber End = bHasEnd ? Section->GetExclusiveEndFrame() : FFrameNumber(TNumericLimits<int32>::Max());

			if (bHasEnd && End <= A)
			{
				return; // entirely before: untouched
			}
			Section->Modify();
			if (bHasStart && bHasEnd && Start >= A && End <= B)
			{
				Doomed.Add(Section);
			}
			else if (bHasStart && Start >= B)
			{
				Section->MoveSection(-Delta);
			}
			else if (bHasStart && Start >= A)
			{
				// Starts inside the cut, runs past it: lose the head, then slide left.
				Section->TrimSection(FQualifiedFrameTime(FFrameTime(B), Tick), true, true);
				Section->MoveSection(-Delta);
			}
			else if (bHasEnd && End <= B)
			{
				// Starts before, ends inside: lose the tail.
				Section->TrimSection(FQualifiedFrameTime(FFrameTime(A), Tick), false, true);
			}
			else
			{
				// Spans the whole cut (music, infinite cue tracks): drop what was in it and pull the rest left.
				ShiftKeys(Section, B, -Delta, A);
				if (bHasEnd)
				{
					Section->SetRange(TRange<FFrameNumber>(bHasStart ? TRangeBound<FFrameNumber>::Inclusive(Start) : TRangeBound<FFrameNumber>::Open(),
						TRangeBound<FFrameNumber>::Exclusive(End - Delta)));
				}
			}
		});

		for (UMovieSceneSection* Section : Doomed)
		{
			if (UMovieSceneTrack* Track = Section->GetTypedOuter<UMovieSceneTrack>())
			{
				Track->Modify();
				Track->RemoveSection(*Section);
			}
		}

		// Markers ride along too.
		for (int32 Index = Context.MovieScene->GetMarkedFrames().Num() - 1; Index >= 0; --Index)
		{
			const FFrameNumber Frame = Context.MovieScene->GetMarkedFrames()[Index].FrameNumber;
			if (Frame >= B)
			{
				Context.MovieScene->SetMarkedFrame(Index, Frame - Delta);
			}
			else if (Frame >= A)
			{
				Context.MovieScene->DeleteMarkedFrame(Index);
			}
		}

		const TRange<FFrameNumber> Playback = Context.MovieScene->GetPlaybackRange();
		if (Playback.GetUpperBoundValue() > B)
		{
			Context.MovieScene->SetPlaybackRange(Playback.GetLowerBoundValue(), (Playback.GetUpperBoundValue() - Delta - Playback.GetLowerBoundValue()).Value);
		}
	}

	void Touch(FContext& Context)
	{
		Context.Sequence->Modify();
		Context.MovieScene->Modify();
	}

	void SetPlayhead(FContext& Context, FFrameNumber Frame)
	{
		if (Context.Sequencer.IsValid())
		{
			Context.Sequencer->SetLocalTimeDirectly(FFrameTime(Frame));
			Context.Sequencer->ForceEvaluate();
		}
	}

	/** Moves the cut between Cuts[Index-1] and Cuts[Index] to Frame (kept at least a frame inside each shot). */
	bool RollCut(FContext& Context, int32 Index, FFrameNumber Frame)
	{
		if (!Context.Cuts.IsValidIndex(Index) || Index == 0)
		{
			return false;
		}
		UMovieSceneCameraCutSection* Before = Context.Cuts[Index - 1];
		UMovieSceneCameraCutSection* After = Context.Cuts[Index];
		const FFrameNumber MinGap = Context.DisplayFramesToTicks(1);
		const FFrameNumber Clamped = FMath::Clamp(Frame, Context.StartOf(Before) + MinGap, Context.EndOf(After) - MinGap);
		Before->Modify();
		After->Modify();
		Before->SetRange(TRange<FFrameNumber>(Context.StartOf(Before), Clamped));
		After->SetRange(TRange<FFrameNumber>(Clamped, Context.EndOf(After)));
		return true;
	}

	ACinemafyEditDecisions* FindOrSpawnDecisions()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return nullptr;
		}
		if (TActorIterator<ACinemafyEditDecisions> It(World); It)
		{
			return *It;
		}
		FActorSpawnParameters Params;
		ACinemafyEditDecisions* Decisions = World->SpawnActor<ACinemafyEditDecisions>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Decisions)
		{
			Decisions->SetActorLabel(TEXT("CinemafyEdit"));
		}
		return Decisions;
	}

	/** The (single, all-time) section of a property track on a binding, made if missing. */
	template <typename TTrack, typename TSection>
	TSection* FindOrAddPropertySection(UMovieScene* MovieScene, const FGuid& Binding, FName Property)
	{
		const UMovieScene* ConstMovieScene = MovieScene;
		TTrack* Track = nullptr;
		if (const FMovieSceneBinding* Existing = ConstMovieScene->FindBinding(Binding))
		{
			for (UMovieSceneTrack* Candidate : Existing->GetTracks())
			{
				TTrack* Typed = Cast<TTrack>(Candidate);
				if (Typed && Typed->GetPropertyName() == Property)
				{
					Track = Typed;
					break;
				}
			}
		}
		if (!Track)
		{
			Track = MovieScene->AddTrack<TTrack>(Binding);
			Track->SetPropertyNameAndPath(Property, Property.ToString());
		}
		Track->Modify();
		if (Track->GetAllSections().Num() == 0)
		{
			UMovieSceneSection* Section = Track->CreateNewSection();
			Section->SetRange(TRange<FFrameNumber>::All());
			Track->AddSection(*Section);
		}
		TSection* Section = Cast<TSection>(Track->GetAllSections()[0]);
		if (Section)
		{
			Section->Modify();
		}
		return Section;
	}

	/** Int cue channel on the edit-decisions actor (starts at -1 the first time). */
	FMovieSceneIntegerChannel* CueChannel(FContext& Context, ACinemafyEditDecisions* Decisions, FName Property)
	{
		const FGuid Binding = CinemafySequencer::BindActor(Context.Sequence, Decisions);
		UMovieSceneIntegerSection* Section = FindOrAddPropertySection<UMovieSceneIntegerTrack, UMovieSceneIntegerSection>(Context.MovieScene, Binding, Property);
		FMovieSceneIntegerChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneIntegerChannel>(0) : nullptr;
		if (Channel && Channel->GetNumKeys() == 0)
		{
			Channel->SetDefault(-1);
		}
		return Channel;
	}

	/** The music: the "Cinemafy Music" track if there is one, else the longest sound on a root audio track. */
	UMovieSceneAudioSection* FindMusic(UMovieScene* MovieScene)
	{
		UMovieSceneAudioSection* Best = nullptr;
		int64 BestLength = -1;
		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			UMovieSceneAudioTrack* Audio = Cast<UMovieSceneAudioTrack>(Track);
			if (!Audio)
			{
				continue;
			}
			const bool bNamedMusic = Audio->GetDisplayName().ToString().Contains(TEXT("Music"));
			for (UMovieSceneSection* Section : Audio->GetAllSections())
			{
				UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(Section);
				if (!AudioSection || !AudioSection->GetSound())
				{
					continue;
				}
				const int64 Length = (AudioSection->HasStartFrame() && AudioSection->HasEndFrame())
					? static_cast<int64>(AudioSection->GetExclusiveEndFrame().Value) - AudioSection->GetInclusiveStartFrame().Value : 0;
				const int64 Score = Length + (bNamedMusic ? (1ll << 40) : 0);
				if (Score > BestLength)
				{
					BestLength = Score;
					Best = AudioSection;
				}
			}
		}
		return Best;
	}

	/** The grade on the clipboard (post process settings minus depth of field). */
	struct FGradeClipboard
	{
		bool bValid = false;
		FPostProcessSettings Settings;
		float Weight = 1.f;
	};
	FGradeClipboard GradeClipboard;

	void PasteGrade(UCineCameraComponent* Camera)
	{
		Camera->Modify();
		// Everything except focus / depth of field, which belongs to each shot.
		for (TFieldIterator<FProperty> It(FPostProcessSettings::StaticStruct()); It; ++It)
		{
			if (It->GetName().Contains(TEXT("DepthOfField")))
			{
				continue;
			}
			It->CopyCompleteValue_InContainer(&Camera->PostProcessSettings, &GradeClipboard.Settings);
		}
		Camera->PostProcessBlendWeight = GradeClipboard.Weight;
	}
}


// --- Timeline ----------------------------------------------------------------------------------------------------

bool UCinemafyEditLibrary::RazorAtPlayhead(bool bAllTracks)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, !bAllTracks))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("Razor", "Cinemafy: Razor"));
	CinemafyEdit::Touch(Context);

	const FQualifiedFrameTime At(FFrameTime(Context.Playhead), Context.TickResolution);
	int32 Splits = 0;
	if (bAllTracks)
	{
		CinemafyEdit::ForEachSection(Context.MovieScene, [&](UMovieSceneSection* Section)
		{
			if (Section->HasStartFrame() && Section->HasEndFrame() && Section->GetInclusiveStartFrame() < Context.Playhead && Context.Playhead < Section->GetExclusiveEndFrame())
			{
				Section->Modify();
				Splits += Section->SplitSection(At, false) ? 1 : 0;
			}
		});
	}
	else
	{
		const int32 Index = Context.CutAt(Context.Playhead);
		if (Index != INDEX_NONE && Context.StartOf(Context.Cuts[Index]) < Context.Playhead)
		{
			Context.Cuts[Index]->Modify();
			Splits += Context.Cuts[Index]->SplitSection(At, false) ? 1 : 0;
		}
	}
	CinemafySequencer::NotifyChanged();
	return Splits > 0;
}

bool UCinemafyEditLibrary::RippleDeleteShotAtPlayhead()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return false;
	}
	const int32 Index = Context.CutAt(Context.Playhead);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("RippleDelete", "Cinemafy: Ripple Delete Shot"));
	CinemafyEdit::Touch(Context);
	const FFrameNumber Start = Context.StartOf(Context.Cuts[Index]);
	CinemafyEdit::RippleRemove(Context, Start, Context.EndOf(Context.Cuts[Index]));
	CinemafySequencer::NotifyChanged();
	CinemafyEdit::SetPlayhead(Context, Start);
	return true;
}

bool UCinemafyEditLibrary::RippleTrimStartToPlayhead()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return false;
	}
	const int32 Index = Context.CutAt(Context.Playhead);
	if (Index == INDEX_NONE || Context.StartOf(Context.Cuts[Index]) >= Context.Playhead)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("RippleTrimStart", "Cinemafy: Ripple Trim Start"));
	CinemafyEdit::Touch(Context);
	const FFrameNumber Start = Context.StartOf(Context.Cuts[Index]);
	CinemafyEdit::RippleRemove(Context, Start, Context.Playhead);
	CinemafySequencer::NotifyChanged();
	CinemafyEdit::SetPlayhead(Context, Start);
	return true;
}

bool UCinemafyEditLibrary::RippleTrimEndToPlayhead()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return false;
	}
	const int32 Index = Context.CutAt(Context.Playhead);
	if (Index == INDEX_NONE || Context.Playhead <= Context.StartOf(Context.Cuts[Index]))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("RippleTrimEnd", "Cinemafy: Ripple Trim End"));
	CinemafyEdit::Touch(Context);
	CinemafyEdit::RippleRemove(Context, Context.Playhead, Context.EndOf(Context.Cuts[Index]));
	CinemafySequencer::NotifyChanged();
	return true;
}

bool UCinemafyEditLibrary::RollNearestCut(int32 Frames)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true) || Frames == 0)
	{
		return false;
	}
	const int32 Index = Context.NearestCutPoint(Context.Playhead);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("Roll", "Cinemafy: Roll Edit"));
	CinemafyEdit::Touch(Context);
	const FFrameNumber NewCut = Context.StartOf(Context.Cuts[Index]) + Context.DisplayFramesToTicks(Frames);
	const bool bRolled = CinemafyEdit::RollCut(Context, Index, NewCut);
	CinemafySequencer::NotifyChanged();
	CinemafyEdit::SetPlayhead(Context, Context.StartOf(Context.Cuts[Index]));
	return bRolled;
}

bool UCinemafyEditLibrary::RollNearestCutToPlayhead()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return false;
	}
	const int32 Index = Context.NearestCutPoint(Context.Playhead);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("RollToPlayhead", "Cinemafy: Roll Cut to Playhead"));
	CinemafyEdit::Touch(Context);
	const bool bRolled = CinemafyEdit::RollCut(Context, Index, Context.Playhead);
	CinemafySequencer::NotifyChanged();
	return bRolled;
}

bool UCinemafyEditLibrary::SwapShotWithNext()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return false;
	}
	const int32 Index = Context.CutAt(Context.Playhead);
	if (Index == INDEX_NONE || !Context.Cuts.IsValidIndex(Index + 1))
	{
		return false;
	}
	UMovieSceneCameraCutSection* First = Context.Cuts[Index];
	UMovieSceneCameraCutSection* Second = Context.Cuts[Index + 1];
	const FFrameNumber Start = Context.StartOf(First);
	const FFrameNumber End = Context.EndOf(Second);
	const FFrameNumber SecondLength = End - Context.StartOf(Second);

	const FScopedTransaction Transaction(LOCTEXT("Swap", "Cinemafy: Swap Shots"));
	CinemafyEdit::Touch(Context);
	First->Modify();
	Second->Modify();
	const FMovieSceneObjectBindingID FirstCamera = First->GetCameraBindingID();
	First->SetCameraBindingID(Second->GetCameraBindingID());
	Second->SetCameraBindingID(FirstCamera);
	First->SetRange(TRange<FFrameNumber>(Start, Start + SecondLength));
	Second->SetRange(TRange<FFrameNumber>(Start + SecondLength, End));
	CinemafySequencer::NotifyChanged();
	return true;
}

bool UCinemafyEditLibrary::GoToCut(bool bNext)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true, true))
	{
		return false;
	}
	TArray<FFrameNumber> Points;
	for (const UMovieSceneCameraCutSection* Cut : Context.Cuts)
	{
		Points.AddUnique(Context.StartOf(Cut));
		Points.AddUnique(Context.EndOf(Cut));
	}
	Points.Sort();
	if (bNext)
	{
		for (const FFrameNumber& Point : Points)
		{
			if (Point > Context.Playhead)
			{
				CinemafyEdit::SetPlayhead(Context, Point);
				return true;
			}
		}
	}
	else
	{
		for (int32 Index = Points.Num() - 1; Index >= 0; --Index)
		{
			if (Points[Index] < Context.Playhead)
			{
				CinemafyEdit::SetPlayhead(Context, Points[Index]);
				return true;
			}
		}
	}
	return false;
}

// --- Transitions & titles ----------------------------------------------------------------------------------------

bool UCinemafyEditLibrary::AddTransitionAtNearestCut(ECinemafyTransition Type, float Duration, ECinemafyWipeDirection Direction)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return false;
	}
	if (Type == ECinemafyTransition::Cut || Type == ECinemafyTransition::MatchCut)
	{
		return false;
	}

	// The opening shot counts as a cut from black when the playhead is nearer its start than any other cut.
	int32 Index = Context.NearestCutPoint(Context.Playhead);
	const FFrameNumber FirstStart = Context.StartOf(Context.Cuts[0]);
	if (Index == INDEX_NONE || FMath::Abs(Context.Playhead.Value - FirstStart.Value) < FMath::Abs(Context.Playhead.Value - Context.StartOf(Context.Cuts[Index]).Value))
	{
		Index = 0;
	}
	const FFrameNumber Cut = Context.StartOf(Context.Cuts[Index]);
	const FFrameNumber Half = Context.TickResolution.AsFrameNumber(Duration * 0.5f);

	const FScopedTransaction Transaction(LOCTEXT("AddTransition", "Cinemafy: Add Transition"));
	CinemafyEdit::Touch(Context);

	if (Type == ECinemafyTransition::FadeThroughBlack)
	{
		// Native Fade keys: visible in the editor, in play and in renders.
		UMovieSceneFadeTrack* FadeTrack = nullptr;
		for (UMovieSceneTrack* Track : Context.MovieScene->GetTracks())
		{
			if (Track && Track->IsA<UMovieSceneFadeTrack>() && Track->GetDisplayName().EqualTo(CinemafyEdit::FadeTrackName))
			{
				FadeTrack = Cast<UMovieSceneFadeTrack>(Track);
			}
		}
		if (!FadeTrack)
		{
			FadeTrack = Context.MovieScene->AddTrack<UMovieSceneFadeTrack>();
			FadeTrack->SetDisplayName(CinemafyEdit::FadeTrackName);
			UMovieSceneSection* Section = FadeTrack->CreateNewSection();
			Section->SetRange(TRange<FFrameNumber>::All());
			FadeTrack->AddSection(*Section);
		}
		FadeTrack->Modify();
		UMovieSceneFadeSection* Section = Cast<UMovieSceneFadeSection>(FadeTrack->GetAllSections()[0]);
		FMovieSceneFloatChannel* Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0) : nullptr;
		if (!Channel)
		{
			return false;
		}
		Section->Modify();
		if (Index == 0)
		{
			Channel->AddLinearKey(Cut, 1.f);
			Channel->AddLinearKey(Cut + Half + Half, 0.f);
		}
		else
		{
			Channel->AddLinearKey(Cut - Half, 0.f);
			Channel->AddLinearKey(Cut, 1.f);
			Channel->AddLinearKey(Cut + Half, 0.f);
		}
		CinemafySequencer::NotifyChanged();
		return true;
	}

	ACinemafyEditDecisions* Decisions = CinemafyEdit::FindOrSpawnDecisions();
	if (!Decisions)
	{
		return false;
	}
	Decisions->Modify();
	FCinemafyEditTransition& Transition = Decisions->Transitions.AddDefaulted_GetRef();
	Transition.Type = Type;
	Transition.Duration = Duration;
	Transition.Direction = Direction;
	if (Direction == ECinemafyWipeDirection::Alternate)
	{
		const ECinemafyWipeDirection Cycle[] = { ECinemafyWipeDirection::LeftToRight, ECinemafyWipeDirection::RightToLeft, ECinemafyWipeDirection::Clock,
			ECinemafyWipeDirection::TopToBottom, ECinemafyWipeDirection::IrisOut, ECinemafyWipeDirection::BottomToTop };
		Transition.Direction = Cycle[(Decisions->Transitions.Num() - 1) % UE_ARRAY_COUNT(Cycle)];
	}
	Transition.OutgoingCamera = Index > 0 ? Context.CameraOf(Context.Cuts[Index - 1]) : nullptr;
	const int32 Cue = Decisions->Transitions.Num() - 1;

	if (FMovieSceneIntegerChannel* Channel = CinemafyEdit::CueChannel(Context, Decisions, GET_MEMBER_NAME_CHECKED(ACinemafyEditDecisions, TransitionCue)))
	{
		Channel->GetData().UpdateOrAddKey(Cut, Cue);
		Channel->GetData().UpdateOrAddKey(Cut + Half + Half + Context.DisplayFramesToTicks(1), -1);
	}
	CinemafySequencer::NotifyChanged();
	return true;
}

int32 UCinemafyEditLibrary::RemoveTransitionsAtNearestCut()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return 0;
	}
	int32 Index = Context.NearestCutPoint(Context.Playhead);
	if (Index == INDEX_NONE)
	{
		Index = 0;
	}
	const FFrameNumber Cut = Context.StartOf(Context.Cuts[Index]);
	const FFrameNumber Window = Context.TickResolution.AsFrameNumber(2.0);
	const TRange<FFrameNumber> Around(Cut - Window, Cut + Window);

	const FScopedTransaction Transaction(LOCTEXT("RemoveTransition", "Cinemafy: Remove Transition"));
	CinemafyEdit::Touch(Context);
	int32 Removed = 0;

	auto Strip = [&](UMovieSceneSection* Section, int32 ChannelIndex, bool bIntChannel)
	{
		if (!Section)
		{
			return;
		}
		FMovieSceneChannel* Channel = bIntChannel
			? static_cast<FMovieSceneChannel*>(Section->GetChannelProxy().GetChannel<FMovieSceneIntegerChannel>(ChannelIndex))
			: static_cast<FMovieSceneChannel*>(Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(ChannelIndex));
		if (!Channel)
		{
			return;
		}
		Section->Modify();
		TArray<FKeyHandle> Keys;
		Channel->GetKeys(Around, nullptr, &Keys);
		Channel->DeleteKeys(Keys);
		Removed += Keys.Num();
	};

	for (UMovieSceneTrack* Track : Context.MovieScene->GetTracks())
	{
		if (Track && Track->IsA<UMovieSceneFadeTrack>() && Track->GetDisplayName().EqualTo(CinemafyEdit::FadeTrackName) && Track->GetAllSections().Num() > 0)
		{
			Strip(Track->GetAllSections()[0], 0, false);
		}
	}
	if (ACinemafyEditDecisions* Decisions = CinemafyEdit::FindOrSpawnDecisions())
	{
		if (FMovieSceneIntegerChannel* Channel = CinemafyEdit::CueChannel(Context, Decisions, GET_MEMBER_NAME_CHECKED(ACinemafyEditDecisions, TransitionCue)))
		{
			TArray<FKeyHandle> Keys;
			Channel->GetKeys(Around, nullptr, &Keys);
			Channel->DeleteKeys(Keys);
			Removed += Keys.Num();
		}
	}
	CinemafySequencer::NotifyChanged();
	return Removed;
}

bool UCinemafyEditLibrary::AddTitleAtPlayhead(const FCinemafyTitleCard& Card)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, false))
	{
		return false;
	}
	ACinemafyEditDecisions* Decisions = CinemafyEdit::FindOrSpawnDecisions();
	if (!Decisions)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTitle", "Cinemafy: Add Title"));
	CinemafyEdit::Touch(Context);
	Decisions->Modify();
	Decisions->Titles.Add(Card);
	const int32 Cue = Decisions->Titles.Num() - 1;
	const FFrameNumber Start = Context.Playhead;
	const FFrameNumber End = Start + Context.TickResolution.AsFrameNumber(FMath::Max(Card.Duration, 0.5f));

	if (FMovieSceneIntegerChannel* Channel = CinemafyEdit::CueChannel(Context, Decisions, GET_MEMBER_NAME_CHECKED(ACinemafyEditDecisions, TitleCue)))
	{
		Channel->GetData().UpdateOrAddKey(Start, Cue);
		Channel->GetData().UpdateOrAddKey(End, -1);
	}

	const FGuid Binding = CinemafySequencer::BindActor(Context.Sequence, Decisions);
	if (UMovieSceneFloatSection* Section = CinemafyEdit::FindOrAddPropertySection<UMovieSceneFloatTrack, UMovieSceneFloatSection>(
		Context.MovieScene, Binding, GET_MEMBER_NAME_CHECKED(ACinemafyEditDecisions, TitleProgress)))
	{
		if (FMovieSceneFloatChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0))
		{
			Channel->AddLinearKey(Start, 0.f);
			Channel->AddLinearKey(End, 1.f);
		}
	}

	// Make sure the sequence is long enough to show it.
	const TRange<FFrameNumber> Playback = Context.MovieScene->GetPlaybackRange();
	if (Playback.GetUpperBoundValue() < End)
	{
		Context.MovieScene->SetPlaybackRange(Playback.GetLowerBoundValue(), (End - Playback.GetLowerBoundValue()).Value);
	}
	CinemafySequencer::NotifyChanged();
	return true;
}

// --- Markers & music ---------------------------------------------------------------------------------------------

bool UCinemafyEditLibrary::AddMarkerAtPlayhead(const FString& Label)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, false))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddMarker", "Cinemafy: Add Marker"));
	CinemafyEdit::Touch(Context);
	FMovieSceneMarkedFrame Marker(Context.Playhead);
	Marker.Label = Label;
	Context.MovieScene->AddMarkedFrame(Marker);
	return true;
}

int32 UCinemafyEditLibrary::AddMusicBeatMarkers()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, false))
	{
		return 0;
	}
	UMovieSceneAudioSection* Music = CinemafyEdit::FindMusic(Context.MovieScene);
	USoundWave* Wave = Music ? Cast<USoundWave>(Music->GetSound()) : nullptr;
	if (!Wave)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Edit: no music (a Sound Wave on a root audio track) in the open sequence."));
		return 0;
	}
	const FCinemafyMusicAnalysis Analysis = UCinemafyEditorLibrary::AnalyzeMusic(Wave);
	if (!Analysis.bValid)
	{
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("BeatMarkers", "Cinemafy: Beat Markers"));
	CinemafyEdit::Touch(Context);
	const FFrameNumber Start = Context.StartOf(Music);
	const FFrameNumber End = Context.EndOf(Music);
	int32 Added = 0;
	for (int32 Index = 0; Index < Analysis.Beats.Num(); ++Index)
	{
		const FFrameNumber Frame = Start + Context.TickResolution.AsFrameNumber(Analysis.Beats[Index]);
		if (Frame >= End)
		{
			break;
		}
		FMovieSceneMarkedFrame Marker(Frame);
		Marker.Label = Index % 4 == 0 ? FString::Printf(TEXT("Bar %d"), Index / 4 + 1) : FString();
		Context.MovieScene->AddMarkedFrame(Marker);
		++Added;
	}
	return Added;
}

int32 UCinemafyEditLibrary::SnapCutsToMarkers(int32 ToleranceFrames)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true))
	{
		return 0;
	}
	const TArray<FMovieSceneMarkedFrame>& Markers = Context.MovieScene->GetMarkedFrames();
	if (Markers.Num() == 0)
	{
		return 0;
	}
	const FFrameNumber Tolerance = Context.DisplayFramesToTicks(ToleranceFrames);

	const FScopedTransaction Transaction(LOCTEXT("SnapCuts", "Cinemafy: Snap Cuts to Markers"));
	CinemafyEdit::Touch(Context);
	int32 Snapped = 0;
	for (int32 Index = 1; Index < Context.Cuts.Num(); ++Index)
	{
		const FFrameNumber Cut = Context.StartOf(Context.Cuts[Index]);
		const FMovieSceneMarkedFrame* Best = nullptr;
		for (const FMovieSceneMarkedFrame& Marker : Markers)
		{
			if (FMath::Abs(Marker.FrameNumber.Value - Cut.Value) <= Tolerance.Value
				&& (!Best || FMath::Abs(Marker.FrameNumber.Value - Cut.Value) < FMath::Abs(Best->FrameNumber.Value - Cut.Value)))
			{
				Best = &Marker;
			}
		}
		if (Best && Best->FrameNumber != Cut && CinemafyEdit::RollCut(Context, Index, Best->FrameNumber))
		{
			++Snapped;
		}
	}
	CinemafySequencer::NotifyChanged();
	return Snapped;
}

// --- Audio -------------------------------------------------------------------------------------------------------

int32 UCinemafyEditLibrary::DuckMusicUnderDialogue(float DuckDecibels, float RampSeconds)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, false))
	{
		return 0;
	}
	UMovieSceneAudioSection* Music = CinemafyEdit::FindMusic(Context.MovieScene);
	if (!Music)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Edit: no music track to duck."));
		return 0;
	}

	// Every other sound in the sequence is dialogue (or effects worth hearing).
	TArray<TRange<FFrameNumber>> Speech;
	CinemafyEdit::ForEachSection(Context.MovieScene, [&](UMovieSceneSection* Section)
	{
		UMovieSceneAudioSection* Audio = Cast<UMovieSceneAudioSection>(Section);
		if (Audio && Audio != Music && Audio->GetSound() && Audio->GetSound() != Music->GetSound() && Audio->HasStartFrame() && Audio->HasEndFrame())
		{
			Speech.Add(TRange<FFrameNumber>(Audio->GetInclusiveStartFrame(), Audio->GetExclusiveEndFrame()));
		}
	});
	if (Speech.Num() == 0)
	{
		return 0;
	}

	// Merge lines close enough that bouncing the music between them would pump.
	const FFrameNumber Ramp = Context.TickResolution.AsFrameNumber(RampSeconds);
	const FFrameNumber Bridge = Ramp + Ramp + Context.TickResolution.AsFrameNumber(0.4);
	Speech.Sort([](const TRange<FFrameNumber>& A, const TRange<FFrameNumber>& B) { return A.GetLowerBoundValue() < B.GetLowerBoundValue(); });
	TArray<TRange<FFrameNumber>> Merged;
	for (const TRange<FFrameNumber>& Range : Speech)
	{
		if (Merged.Num() > 0 && Range.GetLowerBoundValue() <= Merged.Last().GetUpperBoundValue() + Bridge)
		{
			Merged.Last() = TRange<FFrameNumber>(Merged.Last().GetLowerBoundValue(), FMath::Max(Merged.Last().GetUpperBoundValue(), Range.GetUpperBoundValue()));
		}
		else
		{
			Merged.Add(Range);
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("Duck", "Cinemafy: Duck Music"));
	CinemafyEdit::Touch(Context);
	Music->Modify();
	FMovieSceneFloatChannel& Volume = const_cast<FMovieSceneFloatChannel&>(Music->GetSoundVolumeChannel());
	const float Full = Volume.GetDefault().Get(1.f);
	const float Ducked = Full * FMath::Pow(10.f, DuckDecibels / 20.f);
	Volume.Reset();
	Volume.SetDefault(Full);
	for (const TRange<FFrameNumber>& Range : Merged)
	{
		Volume.AddLinearKey(Range.GetLowerBoundValue() - Ramp, Full);
		Volume.AddLinearKey(Range.GetLowerBoundValue(), Ducked);
		Volume.AddLinearKey(Range.GetUpperBoundValue(), Ducked);
		Volume.AddLinearKey(Range.GetUpperBoundValue() + Ramp, Full);
	}
	CinemafySequencer::NotifyChanged();
	return Merged.Num();
}

// --- Grade -------------------------------------------------------------------------------------------------------

bool UCinemafyEditLibrary::CopyGrade()
{
	UCineCameraComponent* Source = nullptr;
	for (AActor* Actor : UCinemafyEditorLibrary::GetSelectedActorsInOrder())
	{
		if (UCineCameraComponent* Camera = Actor ? Actor->FindComponentByClass<UCineCameraComponent>() : nullptr)
		{
			Source = Camera;
			break;
		}
	}
	if (!Source)
	{
		if (AActor* OnScreen = GetCameraAtPlayhead())
		{
			Source = OnScreen->FindComponentByClass<UCineCameraComponent>();
		}
	}
	if (!Source)
	{
		return false;
	}
	CinemafyEdit::GradeClipboard.bValid = true;
	CinemafyEdit::GradeClipboard.Settings = Source->PostProcessSettings;
	CinemafyEdit::GradeClipboard.Weight = Source->PostProcessBlendWeight;
	return true;
}

int32 UCinemafyEditLibrary::PasteGradeToSelection()
{
	if (!CinemafyEdit::GradeClipboard.bValid)
	{
		return 0;
	}
	const FScopedTransaction Transaction(LOCTEXT("CinemafyEdit::PasteGrade", "Cinemafy: Paste Grade"));
	int32 Pasted = 0;
	for (AActor* Actor : UCinemafyEditorLibrary::GetSelectedActorsInOrder())
	{
		if (UCineCameraComponent* Camera = Actor ? Actor->FindComponentByClass<UCineCameraComponent>() : nullptr)
		{
			CinemafyEdit::PasteGrade(Camera);
			++Pasted;
		}
	}
	return Pasted;
}

int32 UCinemafyEditLibrary::ApplyGradeToAllShots()
{
	if (!CinemafyEdit::GradeClipboard.bValid && !CopyGrade())
	{
		return 0;
	}
	const FScopedTransaction Transaction(LOCTEXT("GradeAll", "Cinemafy: Grade All Shots"));
	int32 Pasted = 0;
	for (AActor* Camera : GetSequenceCameras())
	{
		if (UCineCameraComponent* Cine = Camera ? Camera->FindComponentByClass<UCineCameraComponent>() : nullptr)
		{
			CinemafyEdit::PasteGrade(Cine);
			++Pasted;
		}
	}
	return Pasted;
}

// --- Multicam ----------------------------------------------------------------------------------------------------

TArray<AActor*> UCinemafyEditLibrary::GetSequenceCameras()
{
	TArray<AActor*> Cameras;
	CinemafyEdit::FContext Context;
	if (CinemafyEdit::GetContext(Context, true, true))
	{
		for (const UMovieSceneCameraCutSection* Cut : Context.Cuts)
		{
			if (AActor* Camera = Context.CameraOf(Cut))
			{
				Cameras.AddUnique(Camera);
			}
		}
	}
	return Cameras;
}

AActor* UCinemafyEditLibrary::GetCameraAtPlayhead()
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, true, true))
	{
		return nullptr;
	}
	const int32 Index = Context.CutAt(Context.Playhead);
	return Index != INDEX_NONE ? Context.CameraOf(Context.Cuts[Index]) : nullptr;
}

int32 UCinemafyEditLibrary::ApplyMulticamCuts(const TArray<AActor*>& Cameras, const TArray<float>& Times, float EndSeconds)
{
	CinemafyEdit::FContext Context;
	if (!CinemafyEdit::GetContext(Context, false) || Cameras.Num() == 0 || Cameras.Num() != Times.Num())
	{
		return 0;
	}

	// Sequencer's local time is in the movie scene's tick resolution, so seconds map straight across.
	auto ToTick = [&Context](float Seconds) { return Context.TickResolution.AsFrameNumber(Seconds); };
	const FFrameNumber RangeStart = ToTick(Times[0]);
	const FFrameNumber RangeEnd = ToTick(EndSeconds);
	if (RangeEnd <= RangeStart)
	{
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("Multicam", "Cinemafy: Multicam Edit"));
	CinemafyEdit::Touch(Context);

	UMovieSceneCameraCutTrack* CutTrack = Context.CutTrack;
	if (!CutTrack)
	{
		CutTrack = Cast<UMovieSceneCameraCutTrack>(Context.MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
	}
	CutTrack->Modify();

	// Clear the switched range: drop cuts inside it, trim the ones that overlap its edges.
	for (UMovieSceneCameraCutSection* Cut : Context.Cuts)
	{
		const FFrameNumber Start = Context.StartOf(Cut);
		const FFrameNumber End = Context.EndOf(Cut);
		if (End <= RangeStart || Start >= RangeEnd)
		{
			continue;
		}
		Cut->Modify();
		if (Start >= RangeStart && End <= RangeEnd)
		{
			CutTrack->RemoveSection(*Cut);
		}
		else if (Start < RangeStart && End > RangeEnd)
		{
			// The take sits inside one long shot: keep its head, and its tail as a copy.
			if (UMovieSceneSection* Tail = Cut->SplitSection(FQualifiedFrameTime(FFrameTime(RangeEnd), Context.TickResolution), false))
			{
				Tail->Modify();
			}
			Cut->SetRange(TRange<FFrameNumber>(Start, RangeStart));
		}
		else if (Start < RangeStart)
		{
			Cut->SetRange(TRange<FFrameNumber>(Start, RangeStart));
		}
		else
		{
			Cut->SetRange(TRange<FFrameNumber>(RangeEnd, End));
		}
	}

	int32 Added = 0;
	for (int32 Index = 0; Index < Cameras.Num(); ++Index)
	{
		AActor* Camera = Cameras[Index];
		const FFrameNumber Start = ToTick(Times[Index]);
		const FFrameNumber End = Index + 1 < Times.Num() ? ToTick(Times[Index + 1]) : RangeEnd;
		if (!Camera || End <= Start)
		{
			continue;
		}
		const FGuid Binding = CinemafySequencer::BindActor(Context.Sequence, Camera);
		CinemafySequencer::LetCinemafyDriveCamera(Context.MovieScene, Binding, Camera);
		if (UMovieSceneCameraCutSection* Cut = CutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(Binding), Start))
		{
			Cut->SetRange(TRange<FFrameNumber>(Start, End));
			++Added;
		}
	}
	CinemafySequencer::NotifyChanged();
	return Added;
}

#undef LOCTEXT_NAMESPACE
