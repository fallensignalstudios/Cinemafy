#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinemafyTypes.h"
#include "CinemafySceneDirector.generated.h"

class ACineCameraActor;
class ACinemafySceneDirector;

/** What the editor should do for a director (the runtime module can't talk to Sequencer itself). */
enum class ECinemafyDirectorRequest : uint8
{
	Direct,
	Clear
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

	/** The scene, line by line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Script", meta = (TitleProperty = "Note"))
	TArray<FCinemafyLine> Lines;

	/** Same seed = same edit. Re-roll picks a new one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	int32 Seed = 1;

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

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Director")
	FCinemafyDirectorStyle GetStyle() const;

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
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Director")
	void DirectLine(AActor* Speaker, AActor* Listener, float Duration = 3.f, float Intensity = 0.5f);

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
