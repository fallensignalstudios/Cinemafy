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

namespace
{
	bool IsMostlyCJK(const FString& Text)
	{
		int32 CJK = 0;
		int32 Letters = 0;
		for (const TCHAR C : Text)
		{
			if (FChar::IsWhitespace(C) || FChar::IsPunct(C))
			{
				continue;
			}
			++Letters;
			const uint32 Code = static_cast<uint32>(C);
			const bool bCJK = (Code >= 0x3040 && Code <= 0x30FF) || (Code >= 0x3400 && Code <= 0x9FFF) || (Code >= 0xAC00 && Code <= 0xD7AF);
			CJK += bCJK ? 1 : 0;
		}
		return Letters > 0 && CJK * 2 > Letters;
	}

	/** Splits on any of the given break characters, keeping each break with the text before it. */
	TArray<FString> SplitAfter(const FString& Text, const TCHAR* Breaks)
	{
		TArray<FString> Parts;
		FString Current;
		for (const TCHAR C : Text)
		{
			Current.AppendChar(C);
			if (FCString::Strchr(Breaks, C))
			{
				Parts.Add(Current.TrimStartAndEnd());
				Current.Reset();
			}
		}
		if (!Current.TrimStartAndEnd().IsEmpty())
		{
			Parts.Add(Current.TrimStartAndEnd());
		}
		return Parts;
	}
}

TArray<FString> UCinemafySubtitleSubsystem::SplitIntoChunks(const FString& Text, int32 MaxCharacters)
{
	const FString Clean = Text.TrimStartAndEnd();
	if (Clean.Len() <= MaxCharacters)
	{
		return { Clean };
	}

	// Break into the smallest natural pieces: sentences, then clauses, then words.
	TArray<FString> Pieces;
	for (const FString& Sentence : SplitAfter(Clean, TEXT(".!?\u2026\u3002\uFF01\uFF1F")))
	{
		if (Sentence.Len() <= MaxCharacters)
		{
			Pieces.Add(Sentence);
			continue;
		}
		for (const FString& Clause : SplitAfter(Sentence, TEXT(",;:\u2014\u3001\uFF0C")))
		{
			if (Clause.Len() <= MaxCharacters)
			{
				Pieces.Add(Clause);
				continue;
			}
			TArray<FString> Words;
			Clause.ParseIntoArrayWS(Words);
			if (Words.Num() <= 1)
			{
				// No spaces (e.g. CJK): cut by length.
				for (int32 Start = 0; Start < Clause.Len(); Start += MaxCharacters)
				{
					Pieces.Add(Clause.Mid(Start, MaxCharacters));
				}
				continue;
			}
			FString Current;
			for (const FString& Word : Words)
			{
				if (!Current.IsEmpty() && Current.Len() + 1 + Word.Len() > MaxCharacters)
				{
					Pieces.Add(Current);
					Current.Reset();
				}
				Current += Current.IsEmpty() ? Word : TEXT(" ") + Word;
			}
			if (!Current.IsEmpty())
			{
				Pieces.Add(Current);
			}
		}
	}

	// Pack the pieces back into chunks as full as possible.
	TArray<FString> Result;
	FString Chunk;
	for (const FString& Piece : Pieces)
	{
		if (!Chunk.IsEmpty() && Chunk.Len() + 1 + Piece.Len() > MaxCharacters)
		{
			Result.Add(Chunk);
			Chunk.Reset();
		}
		Chunk += Chunk.IsEmpty() ? Piece : TEXT(" ") + Piece;
	}
	if (!Chunk.IsEmpty())
	{
		Result.Add(Chunk);
	}
	return Result;
}

void UCinemafySubtitleSubsystem::ShowSubtitle(const FText& Speaker, const FText& Text, float Duration)
{
	if (!bSubtitlesEnabled || Text.IsEmpty())
	{
		ClearSubtitle();
		return;
	}

	EnsureWidget();
	if (!Widget.IsValid())
	{
		return;
	}

	// The FText resolves to the player's language here, so chunking and timing fit the translation.
	const FString Localized = Text.ToString();
	const bool bCJK = IsMostlyCJK(Localized);
	const int32 MaxChars = bCJK ? FMath::Min(MaxCharactersPerChunk, 32) : MaxCharactersPerChunk;
	const float ReadingSpeed = bCJK ? FMath::Min(CharactersPerSecond, 8.f) : CharactersPerSecond;

	CurrentSpeaker = Speaker;
	Chunks = SplitIntoChunks(Localized, MaxChars);

	// Long enough to read, never shorter than the line; split across chunks by length.
	int32 TotalChars = 0;
	for (const FString& Chunk : Chunks)
	{
		TotalChars += Chunk.Len();
	}
	const float ReadingTime = TotalChars / FMath::Max(ReadingSpeed, 1.f);
	const float Total = FMath::Max(Duration, ReadingTime);
	ChunkSeconds.Reset();
	for (const FString& Chunk : Chunks)
	{
		ChunkSeconds.Add(FMath::Max(MinChunkSeconds, Total * Chunk.Len() / FMath::Max(TotalChars, 1)));
	}

	ShowChunk(0);
}

void UCinemafySubtitleSubsystem::ShowChunk(int32 Index)
{
	if (!Widget.IsValid() || !Chunks.IsValidIndex(Index))
	{
		ClearSubtitle();
		return;
	}
	ChunkIndex = Index;
	// Name the speaker on their first chunk only, like broadcast subtitles.
	Widget->SetLine(Index == 0 ? CurrentSpeaker : FText::GetEmpty(), FText::FromString(Chunks[Index]));
	TimeLeft = ChunkSeconds[Index];
}

void UCinemafySubtitleSubsystem::ClearSubtitle()
{
	if (Widget.IsValid())
	{
		Widget->Clear();
	}
	TimeLeft = 0.f;
	Chunks.Reset();
	ChunkSeconds.Reset();
	ChunkIndex = INDEX_NONE;
}

void UCinemafySubtitleSubsystem::Tick(float DeltaTime)
{
	if (TimeLeft > 0.f)
	{
		TimeLeft -= DeltaTime;
		if (TimeLeft <= 0.f)
		{
			if (Chunks.IsValidIndex(ChunkIndex + 1))
			{
				ShowChunk(ChunkIndex + 1);
			}
			else
			{
				ClearSubtitle();
			}
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
