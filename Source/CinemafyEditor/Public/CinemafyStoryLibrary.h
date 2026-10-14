#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyScript.h"
#include "CinemafyStoryLibrary.generated.h"

class ACinemafyMark;
class ACinemafySceneDirector;
class ACinemafyChoreography;
class ULevelSequence;

/**
 * The storyteller's tools: casting and sets, staging a scene straight from the script, the story board,
 * scratch voices, mood, dailies and paperwork. Everything works from a Script Project.
 */
UCLASS()
class CINEMAFYEDITOR_API UCinemafyStoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --- Cast & Sets -----------------------------------------------------------------------------------------------

	/**
	 * Adds a cast entry for every speaking character and a set for every location (existing entries are kept).
	 * Characters are matched to actors in the open level by label, sets to the open level when its marks name
	 * them. Returns how many entries were added.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static int32 BuildCastAndSets(UCinemafyScriptProject* Project);

	/** Drops a named mark where the viewport is looking (on the floor). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static ACinemafyMark* PlaceMark(const FString& MarkName, const FString& SetName);

	/** Puts each cast member in the scene in the state the script says they arrive in (props, injuries, costume). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static int32 ApplySceneContinuity(UCinemafyScriptProject* Project, const FString& SceneId);

	// --- Previs ----------------------------------------------------------------------------------------------------

	/**
	 * Stages a scene straight from the script: puts the cast on set (spawning from Cast & Sets where needed),
	 * dresses them for continuity, turns the action and dialogue into a choreography per character (marks
	 * for places, cues between lines), makes the scene's Level Sequence and Scene Director, and (bRecord)
	 * performs, records and cuts it. Returns a report: what was staged, and what wasn't.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static FString PrevisScene(UCinemafyScriptProject* Project, const FString& SceneId, bool bRecord = true);

	/** The scene's Level Sequence (/Game/Cinemafy/Scenes/SC_<id>_<place>), made if it doesn't exist yet, opened in Sequencer. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static ULevelSequence* OpenSceneSequence(UCinemafyScriptProject* Project, const FString& SceneId);

	// --- Story board -----------------------------------------------------------------------------------------------

	/**
	 * Cuts the story together: a master sequence per act and one for the whole film (/Game/Cinemafy/Masters),
	 * each scene's sequence placed back to back on a shot track in script order. Returns the film master.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static ULevelSequence* BuildMasterSequences(UCinemafyScriptProject* Project);

	/** Keeps the scene's current take and starts a new one from a copy of it (_T02, _T03...). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static ULevelSequence* NewTake(UCinemafyScriptProject* Project, const FString& SceneId);

	/** Makes an earlier take the scene's current one (the current one joins the takes). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static bool UseTake(UCinemafyScriptProject* Project, const FString& SceneId, int32 TakeIndex);

	/**
	 * Moves a scene earlier (-1) or later (+1) within its chapter; scene ids are renumbered and Scene Directors in
	 * the open level that point at moved scenes follow them.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static bool MoveScene(UCinemafyScriptProject* Project, const FString& SceneId, int32 Delta);

	/** Opens the Story Board on a project. */
	static void OpenStoryBoard(UCinemafyScriptProject* Project);

	/** The actor playing a script character in a world: the cast entry's label, else a matching character label. */
	static AActor* FindCastActor(const UCinemafyScriptProject* Project, const FString& ScriptName, UWorld* World);
};
