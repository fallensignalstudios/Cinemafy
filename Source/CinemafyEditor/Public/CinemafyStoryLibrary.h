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

	// --- Voices ----------------------------------------------------------------------------------------------------

	/**
	 * Reads every line without real voice-over aloud with the computer's voices (macOS say, Windows speech, or
	 * espeak-ng on Linux), each character in their Cast entry's Scratch Voice, and imports the reads under
	 * /Game/Cinemafy/ScratchVO. Lines get the audio, and so do the Scene Directors that speak them. SceneId empty =
	 * the whole script. Returns lines voiced.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static int32 GenerateScratchVoices(UCinemafyScriptProject* Project, const FString& SceneId, bool bReplaceExistingScratch = false);

	/**
	 * Swaps in recorded voice-over from a content folder: sound assets named by line id (KT_1A2B3C4D...), or
	 * <scene id>_<line number> ("2_1_3_04"). Scratch reads are replaced; directors get the new audio (re-Direct to
	 * re-time the cut to the real performance). Returns lines matched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static int32 AssignRecordedVoices(UCinemafyScriptProject* Project, const FString& ContentFolder);

	/** Keys Cinemafy Face's Talk / Talk Narrow on each speaker from their lines' audio, where the director laid it. */
	static void BakeTalkCurves(ULevelSequence* Sequence, const TArray<FCinemafyLine>& Lines, FFrameNumber SceneStart);

	// --- Mood and sound -------------------------------------------------------------------------------------------

	/**
	 * Sets a scene up for its mood (read from the heading and words): its Scene Director's style (camera, cut,
	 * grade, lighting), temp music for the mood (Project Settings > Cinemafy > Mood Music), and the set's ambience
	 * under the scene. bLightTheLevel also turns the sun and sky to the time of day. Returns what it read and did.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static FString ApplySceneMood(UCinemafyScriptProject* Project, const FString& SceneId, bool bLightTheLevel = false);

	/** Lays foley (Project Settings > Cinemafy > Foley Sounds) under a recorded take at its impacts, landings, draws and falls. */
	static int32 BakeFoley(ULevelSequence* Sequence, const TArray<ACinemafyChoreography*>& Ensemble, float TakeStartSeconds);

	// --- Dailies, review, paperwork ----------------------------------------------------------------------------------

	/**
	 * Renders dailies with Movie Render Queue (720p JPEG frames, in play-in-editor): one job per scene with a
	 * sequence. SceneId empty = every scene that's been recorded or directed. Output goes to
	 * Saved/Cinemafy/Dailies/<project>/<scene>. Returns the number of jobs queued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static int32 RenderDailies(UCinemafyScriptProject* Project, const FString& SceneId);

	/** Adds a review note to a scene (from you, now). AtSeconds = where in the scene (-1 = the whole scene). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static bool AddReviewNote(UCinemafyScriptProject* Project, const FString& SceneId, const FString& Text, float AtSeconds = -1.f);

	/**
	 * Writes the production paperwork to Saved/Cinemafy/Paperwork/<project>: voice-over recording scripts per actor
	 * (Markdown + CSV, with line ids as file names), subtitles (SRT, estimated or from audio), a localization string
	 * table (CSV: Key, SourceString, Comment), shot lists from the level's directors (CSV), and an HTML storyboard
	 * with a frame from each planned shot's camera. Returns the folder.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Story")
	static FString ExportPaperwork(UCinemafyScriptProject* Project);

	/** The actor playing a script character in a world: the cast entry's label, else a matching character label. */
	static AActor* FindCastActor(const UCinemafyScriptProject* Project, const FString& ScriptName, UWorld* World);
};
