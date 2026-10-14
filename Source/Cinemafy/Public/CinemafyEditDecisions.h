#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "CinemafyTypes.h"
#include "CinemafyEditDecisions.generated.h"

class SCinemafyTitles;

/** How a title card looks. */
UENUM(BlueprintType)
enum class ECinemafyTitleStyle : uint8
{
	/** Name and role, bottom left, sliding in on an accent bar. */
	LowerThird     UMETA(DisplayName = "Lower Third"),
	/** "DAGOBAH - DAY": spaced capitals, typed on, bottom left. */
	LocationCard   UMETA(DisplayName = "Location Card"),
	/** Black card, centered title and subtitle. */
	ChapterCard    UMETA(DisplayName = "Chapter Card"),
	/** Big centered title over the picture. */
	CenteredTitle  UMETA(DisplayName = "Centered Title"),
	/** Intro line, a title that falls away, then a crawl receding into the distance. Put it over a starfield. */
	OpeningCrawl   UMETA(DisplayName = "Opening Crawl")
};

/** A transition someone placed on a cut by hand (Cinemafy Edit). */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyEditTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	ECinemafyTransition Type = ECinemafyTransition::Dissolve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float Duration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	ECinemafyWipeDirection Direction = ECinemafyWipeDirection::LeftToRight;

	/** The camera before the cut (what the overlay captures and transitions away from). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	TObjectPtr<AActor> OutgoingCamera = nullptr;
};

/** A title, lower third or crawl. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyTitleCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title")
	ECinemafyTitleStyle Style = ECinemafyTitleStyle::LowerThird;

	/** Name / place / chapter / title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title")
	FText Title;

	/** Role / time of day / chapter subtitle. For a crawl: the intro line shown first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title")
	FText Subtitle;

	/** The crawl's paragraphs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title", meta = (MultiLine = true))
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title", meta = (ClampMin = "0.5"))
	float Duration = 4.f;

	/** Accent / text color. Black = the style's own (gold crawl, white titles, amber bar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title")
	FLinearColor Color = FLinearColor::Black;
};

/**
 * Hand-made edit decisions for a Level Sequence: transitions on any cut and title cards, placed
 * from the Cinemafy Edit panel. The cues are keyed in Sequencer; this actor plays them over the game
 * view (wipes, dissolves, dips, whips and titles show in play and in game; fades through black use
 * Sequencer's Fade track so they show everywhere).
 */
UCLASS(meta = (DisplayName = "Cinemafy Edit Decisions"))
class CINEMAFY_API ACinemafyEditDecisions : public AActor
{
	GENERATED_BODY()

public:
	ACinemafyEditDecisions();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit")
	TArray<FCinemafyEditTransition> Transitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit")
	TArray<FCinemafyTitleCard> Titles;

	/** Keyed by Cinemafy Edit: the transition to play now (index into Transitions), -1 between. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Edit", AdvancedDisplay)
	int32 TransitionCue = -1;

	/** Keyed by Cinemafy Edit: the title on screen (index into Titles), -1 for none. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Edit", AdvancedDisplay)
	int32 TitleCue = -1;

	/** Keyed 0 -> 1 across each title, so titles scrub and render deterministically. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Edit", AdvancedDisplay)
	float TitleProgress = 0.f;

	virtual void Tick(float DeltaSeconds) override;

private:
	int32 TriggeredTransitionCue = -1;
};

/** Draws title cards over the game view. Cinemafy Edit Decisions drives it; call it from anywhere. */
UCLASS()
class CINEMAFY_API UCinemafyTitleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Shows a card at a point in its life (0 = just appearing, 1 = gone). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Titles")
	void ShowTitle(const FCinemafyTitleCard& Card, float Progress);

	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Titles")
	void HideTitle();

	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TSharedPtr<SCinemafyTitles> Widget;
};
