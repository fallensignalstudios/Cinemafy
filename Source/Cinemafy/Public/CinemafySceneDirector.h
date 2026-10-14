#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinemafyTypes.h"
#include "CinemafySceneDirector.generated.h"

class ACineCameraActor;
class UCinemafyDirectorStyleAsset;
class ACinemafySceneDirector;

/** What the editor should do for a director (the runtime module can't talk to Sequencer itself). */
enum class ECinemafyDirectorRequest : uint8
{
	Direct,
	Clear,
	LearnStyle,
	AnalyzeAudio,
	SaveStyleAsset
};

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_TwoParams(FCinemafyDirectorEditorRequest, ACinemafySceneDirector*, ECinemafyDirectorRequest);
#endif

/**
 * An AI director for dialogue scenes. Write the scene as lines (who speaks to whom, for how long,
 * how intense), pick a style, and hit Direct: Cinemafy plans the coverage and the edit and bakes
 * cameras and camera cuts into the open Level Sequence. Re-roll for another take.
 *
 * At runtime it directs live: call DirectLine as your dialogue system plays each line (or
 * StartConversation to play the script) and it cuts between cameras like an editor would.
 */
UCLASS(meta = (DisplayName = "Cinemafy Scene Director"))
class CINEMAFY_API ACinemafySceneDirector : public AActor
{
	GENERATED_BODY()

public:
	ACinemafySceneDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	ECinemafyDirectorPreset Style = ECinemafyDirectorPreset::ClassicHollywood;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director", meta = (EditCondition = "Style == ECinemafyDirectorPreset::Custom"))
	FCinemafyDirectorStyle CustomStyle;

	/** A saved style. When set, it's used instead of Style / Custom Style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	TObjectPtr<UCinemafyDirectorStyleAsset> StyleAsset = nullptr;

	/** The scene, line by line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Script", meta = (TitleProperty = "Note"))
	TArray<FCinemafyLine> Lines;

	/** Same seed = same edit. Re-roll picks a new one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	int32 Seed = 1;

	/** How much each line's heard intensity overrides its manual Intensity (lines with audio and Auto Intensity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float AudioIntensityWeight = 0.8f;

	/** Lay each line's body / face animation clips onto the speaker when directing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Audio")
	bool bPlacePerformances = true;

	/** Lay each line's audio into the sequence when directing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Audio")
	bool bPlaceDialogueAudio = true;

	/** Show each line's Text as a subtitle (live, and in sequences the director has cut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Subtitles")
	bool bShowSubtitles = true;

	/** Which line's subtitle is showing (-1 = none). Keyed by Direct so sequences show subtitles too. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Director|Subtitles", AdvancedDisplay)
	int32 SubtitleLineIndex = -1;

	/** Which shot's transition is playing (keyed by Direct; wipes and dissolves render in PIE / game). */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Director|Transitions", AdvancedDisplay)
	int32 TransitionCue = -1;

	/** Which side of the line of action every camera lives on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	bool bFlipSide = false;

	/** Live directing: blend from the gameplay camera into the first shot, and back out at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Live", meta = (ClampMin = "0"))
	float BlendInTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Live", meta = (ClampMin = "0"))
	float BlendOutTime = 1.f;

	/** Live directing: point each listener's Cinemafy Listener at the current speaker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Live")
	bool bUpdateListeners = true;

	/** Where (in the sequence's tick resolution) the last Direct started the scene. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Director|Plan", AdvancedDisplay)
	int32 LastSceneStartTick = 0;

	/** The lines exactly as last directed (audio-blended intensities, cross-cut order). Subtitles index into these. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Director|Plan", AdvancedDisplay)
	TArray<FCinemafyLine> LastDirectedLines;

	/** The camera used for each shot of the last plan (same order as LastPlan). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Director|Plan", AdvancedDisplay)
	TArray<TObjectPtr<ACineCameraActor>> LastPlanCameras;

	/** The last plan, with the director's reasoning for each shot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Director|Plan")
	TArray<FCinemafyPlannedShot> LastPlan;

	// --- Editor ---------------------------------------------------------------------

	/** Plan the scene and bake cameras + camera cuts into the open Level Sequence, starting at the playhead. */
	UFUNCTION(CallInEditor, Category = "Director")
	void Direct();

	/** Same scene, new take. */
	UFUNCTION(CallInEditor, Category = "Director")
	void Reroll();

	/** Remove this director's camera cuts from the open sequence. */
	UFUNCTION(CallInEditor, Category = "Director")
	void ClearShots();

	/**
	 * Watch the edit in the open Level Sequence (one you cut by hand, or any you like) and turn it
	 * into this director's Custom style: shot preferences, pacing, lenses, angles, movement and feel.
	 */
	UFUNCTION(CallInEditor, Category = "Director")
	void LearnStyleFromSequence();

	/** Save the style this director is using (preset, custom or learned) as a reusable asset. */
	UFUNCTION(CallInEditor, Category = "Director")
	void SaveStyleAsAsset();

	/** Listen to every line's audio: measure the delivery, set durations and audio intensity. */
	UFUNCTION(CallInEditor, Category = "Director")
	void AnalyzeDialogueAudio();

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Director")
	FCinemafyDirectorStyle GetStyle() const;

	/** The script as the director will cut it: intensities blended with what the audio says. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Director")
	TArray<FCinemafyLine> GetEffectiveLines() const;

	/** Everything that will be cut, in order (this director's lines, interleaved with a parallel scene if cross-cutting). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Director")
	TArray<FCinemafyLine> GetDirectedLines() const;

	/** Plans the given lines with this director's style and seed. */
	TArray<FCinemafyPlannedShot> PlanDirectedLines(const TArray<FCinemafyLine>& InLines) const;

	/** Tag on every camera this director creates. */
	FName GetDirectorTag() const;

#if WITH_EDITOR
	static FCinemafyDirectorEditorRequest OnEditorRequest;
#endif

	// --- Live -----------------------------------------------------------------------

	/** Plays the whole script live, cutting between cameras, then hands back to gameplay. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Director")
	void StartConversation();

	/** Direct one line as it plays (for dialogue systems). Starts a conversation if none is running. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Director", meta = (AdvancedDisplay = "Subtitle"))
	void DirectLine(AActor* Speaker, AActor* Listener, float Duration = 3.f, float Intensity = 0.5f, const FText& Subtitle = FText::GetEmpty());

	/** Blend back to the player's camera. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Director")
	void EndConversation();

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Director")
	bool IsDirecting() const { return bLive; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void QueueShots(const TArray<FCinemafyPlannedShot>& Shots, float TimeOffset);
	void CutTo(const FCinemafyPlannedShot& Shot);
	ACineCameraActor* GetLiveCamera(const FCinemafyPlannedShot& Shot);

	void ShowLineSubtitle(const FCinemafyLine& Line);
	void UpdateSequencedSubtitle();
	void UpdateSequencedTransition();
	void PlayLiveTransition(ECinemafyTransition Type, float Duration, ECinemafyWipeDirection Direction, AActor* Outgoing, bool bFromGameplayView);

	int32 TriggeredTransitionCue = -1;
	TWeakObjectPtr<AActor> LastLiveCamera;

	int32 ShownSubtitleIndex = -1;
	FCinemafyLine CurrentLiveLine;
	TArray<AActor*> LiveCast;
	bool bLive = false;
	bool bAutoEnd = false;
	bool bFirstCut = true;
	float LiveTime = 0.f;
	float LiveEndTime = 0.f;
	int32 NextShot = 0;
	int32 LiveLineIndex = 0;
	TArray<FCinemafyPlannedShot> LiveQueue;
	FCinemafyPlannedShot LastLiveShot;
	bool bHasLastLiveShot = false;
	FRandomStream LiveRandom;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<ACineCameraActor>> LiveCameras;
};
