#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyProduction.h"
#include "CinemafyScript.generated.h"

class ULevelSequence;
class UWorld;
class USoundBase;
class UCinemafyScriptProject;

/** How far along a scene is in production. */
UENUM(BlueprintType)
enum class ECinemafySceneStatus : uint8
{
	NotStarted  UMETA(DisplayName = "Not Started"),
	/** A director or choreography exists for it (blocking, lines in). */
	Blocking    UMETA(DisplayName = "Blocking"),
	/** A performance take is recorded. */
	Recorded    UMETA(DisplayName = "Recorded"),
	/** Shots are cut. */
	Directed    UMETA(DisplayName = "Directed"),
	/** Signed off. */
	Final       UMETA(DisplayName = "Final"),
	/** Dropped from the project (kept for the record). */
	Cut         UMETA(DisplayName = "Cut")
};

/** What kind of script paragraph a line is. */
UENUM(BlueprintType)
enum class ECinemafyScriptLineKind : uint8
{
	Dialogue    UMETA(DisplayName = "Dialogue"),
	Action      UMETA(DisplayName = "Action"),
	Transition  UMETA(DisplayName = "Transition")
};

/** One paragraph of a scene: a spoken line, a stretch of action, or a transition. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyScriptLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	ECinemafyScriptLineKind Kind = ECinemafyScriptLineKind::Action;

	/** Who says it (dialogue only), as written in the script ("KADE"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FString Speaker;

	/** How it's said: "(quietly)", "V.O.". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FString Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line", meta = (MultiLine = true))
	FString Text;

	/** Stable id for dialogue (speaker + words): voice-over files, subtitles and localization key on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line")
	FString LineId;

	/** The spoken line: scratch (generated) or the recorded voice-over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	TSoftObjectPtr<USoundBase> Audio;

	/** Audio is a generated scratch read, waiting for the real performance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line")
	bool bScratchAudio = false;
};

/** Something that has to stay true across scenes: an injury, a weapon in hand, a costume, a death. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyContinuityFact
{
	GENERATED_BODY()

	/** Whose it is (empty = the scene's world: time of day, weather). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuity")
	FString Character;

	/** Injury, Carrying, Wearing, Dead, Time... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuity")
	FString Category;

	/** What's true: "wounded", "pistol", "officer's uniform". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuity")
	FString Fact;

	/** The scene it became true in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuity")
	FString SinceScene;

	/** The script's words for it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuity")
	FString Evidence;
};

/**
 * A choice at the end of a scene (games): "* Spare him -> 2.1.3 ~ spared_thane". Shown when its Require Flags
 * hold; picking it sets Set Flags and plays Target.
 */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyScriptChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Text;

	/** Where it goes, as written: a scene id ("2.1.3"), "#14", a heading or place ("MED BAY"), or END. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Target;

	/** The scene id Target resolved to (empty = the next scene; "END" = the story ends). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Choice")
	FString TargetSceneId;

	/** Flags set by picking it ("!flag" clears one). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	TArray<FString> SetFlags;

	/** Only offered when these flags are set ("!flag" = when it isn't). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	TArray<FString> RequireFlags;
};

/** One scene: a place and time, who's in it, what's said, and where it is in production. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyScriptScene
{
	GENERATED_BODY()

	/** Act.Chapter.Scene, e.g. "2.3.1". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString Id;

	/** Running scene number across the whole script (1-based). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	int32 Number = 0;

	/** The scene heading as written: "INT. BRIDGE - NIGHT". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString Heading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString TimeOfDay;

	/** INT, EXT or INT/EXT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString Setting;

	/** Who speaks or is named in the action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	TArray<FString> Characters;

	/** The first beats of the action, for the checklist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene", meta = (MultiLine = true))
	FString Synopsis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	TArray<FCinemafyScriptLine> Lines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene")
	int32 DialogueLines = 0;

	/** Rough screen time from the words (dialogue at speaking pace, action at reading pace). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene")
	float EstimatedSeconds = 0.f;

	/** A fight, chase or stunt is described. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene")
	bool bHasAction = false;

	// --- Story flow (games) ----------------------------------------------------------------------------------------

	/** Offered when the scene ends ("* text -> target" lines). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	TArray<FCinemafyScriptChoice> Choices;

	/** Where the story goes after this scene with no choice ("-> target"); empty = the next scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	FString Divert;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow")
	FString DivertSceneId;

	/** Flags set when the scene plays ("~ flag", "~ !flag"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	TArray<FString> SetFlags;

	/** The scene only plays when these hold ("? flag"); otherwise the story skips it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	TArray<FString> RequireFlags;

	/** What has to be true going into this scene (carried from earlier scenes), for set, wardrobe and blocking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Continuity")
	TArray<FCinemafyContinuityFact> CarriedIn;

	/** What changes in this scene. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Continuity")
	TArray<FCinemafyContinuityFact> Changes;

	/** Things that look wrong ("Thane died in 2.1.3 but speaks here"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Continuity")
	TArray<FString> Warnings;

	// --- Production ---------------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
	ECinemafySceneStatus Status = ECinemafySceneStatus::NotStarted;

	/** The Level Sequence that holds this scene (the current take). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
	TSoftObjectPtr<ULevelSequence> Sequence;

	/** Earlier takes, kept side by side (New Take copies the current one here first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
	TArray<TSoftObjectPtr<ULevelSequence>> Takes;

	/** The level it's shot in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
	TSoftObjectPtr<UWorld> Level;

	/** The Scene Director that cuts it (actor label in Level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production")
	FString DirectorLabel;

	/** How much of the script's dialogue the linked director has (1 = every line). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Production")
	float MatchScore = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production", meta = (MultiLine = true))
	FString Notes;
};

/** A run of scenes that belong together (a sequence, a mission, a chapter of the game). */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyScriptChapter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	FString Title;

	/** Cinemafy grouped these scenes (the script didn't mark a chapter here). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chapter")
	bool bInferred = false;

	/** Why the chapter starts where it does. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chapter")
	FString Why;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter", meta = (TitleProperty = "{Id} {Heading}"))
	TArray<FCinemafyScriptScene> Scenes;
};

/** A movement of the story. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyScriptAct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Act")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Act")
	FString Title;

	/** Cinemafy placed this act break (the script didn't mark one). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act")
	bool bInferred = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act")
	FString Why;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Act", meta = (TitleProperty = "{Id} {Title}"))
	TArray<FCinemafyScriptChapter> Chapters;

	/** Every scene of the act cut together (Build Master Sequences). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act")
	TSoftObjectPtr<ULevelSequence> MasterSequence;
};

/** What the editor should do for a script project (the runtime module can't touch levels or Sequencer). */
UENUM()
enum class ECinemafyScriptRequest : uint8
{
	Import,
	MapScenes,
	ExportChecklist,
	BuildCastAndSets,
	OpenStoryBoard
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FCinemafyScriptEditorRequest, class UCinemafyScriptProject*, ECinemafyScriptRequest);

/**
 * A whole script, broken down: acts, chapters and scenes, with who's in each, what's said, what has to stay
 * true between scenes, and how far each scene has got. Scenes you shoot map back onto it (by their Scene
 * Director's lines), so it doubles as the project's checklist.
 */
UCLASS(BlueprintType)
class CINEMAFY_API UCinemafyScriptProject : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script")
	FString Title;

	/** The script to read: Fountain (.fountain), Final Draft (.fdx) or plain text / game script (.txt). */
	UPROPERTY(EditAnywhere, Category = "Script", meta = (FilePathFilter = "Scripts (*.fountain;*.fdx;*.txt;*.md)|*.fountain;*.fdx;*.txt;*.md"))
	FFilePath ScriptFile;

	/** No act markers in the script: place three-act breaks (else everything is one act). */
	UPROPERTY(EditAnywhere, Category = "Script")
	bool bInferActs = true;

	/** Chapters aim for about this many scenes when the script doesn't mark them. */
	UPROPERTY(EditAnywhere, Category = "Script", meta = (ClampMin = "2", ClampMax = "20"))
	int32 ScenesPerChapter = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script", meta = (TitleProperty = "{Id} {Title}"))
	TArray<FCinemafyScriptAct> Acts;

	/** The whole story cut together, scene after scene (Build Master Sequences). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Script")
	TSoftObjectPtr<ULevelSequence> FilmSequence;

	/** Everyone with a line, most lines first. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Script")
	TArray<FString> CastList;

	/** Who plays whom: script names to characters, with voices and continuity looks. Build Cast & Sets fills it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast & Sets", meta = (TitleProperty = "{ScriptName} -> {ActorLabel}"))
	TArray<FCinemafyCastMember> CastMembers;

	/** Where scenes are shot: script locations to levels, with ambience and marks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast & Sets", meta = (TitleProperty = "{ScriptLocation}"))
	TArray<FCinemafySet> Sets;

	/** Adds a cast entry for every speaking character and a set for every location (keeps what's filled in). */
	UFUNCTION(CallInEditor, Category = "Cast & Sets")
	void BuildCastAndSets() { OnEditorRequest.Broadcast(this, ECinemafyScriptRequest::BuildCastAndSets); }

	/** Opens the Story Board: every scene as a card, by act and chapter. */
	UFUNCTION(CallInEditor, Category = "Script")
	void OpenStoryBoard() { OnEditorRequest.Broadcast(this, ECinemafyScriptRequest::OpenStoryBoard); }

	const FCinemafyCastMember* FindCastMember(const FString& ScriptName) const;
	const FCinemafySet* FindSet(const FString& ScriptLocation) const;

	/** What the breakdown found and decided. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Script", meta = (MultiLine = true))
	FString BreakdownReport;

	/** Reads Script File and breaks it down. Production fields (status, links, notes) survive for scenes that still match. */
	UFUNCTION(CallInEditor, Category = "Script")
	void ImportScript() { OnEditorRequest.Broadcast(this, ECinemafyScriptRequest::Import); }

	/** Finds the Scene Directors in the open level that shoot these scenes (by their lines) and updates progress. */
	UFUNCTION(CallInEditor, Category = "Script")
	void MapScenesInLevel() { OnEditorRequest.Broadcast(this, ECinemafyScriptRequest::MapScenes); }

	/** Writes the checklist (progress + continuity) to Saved/Cinemafy and opens it. */
	UFUNCTION(CallInEditor, Category = "Script")
	void ExportChecklist() { OnEditorRequest.Broadcast(this, ECinemafyScriptRequest::ExportChecklist); }

	/** Replaces the breakdown with one read from text, keeping production fields for scenes that still match. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Script")
	void BreakDown(const FString& ScriptText);

	/** Re-runs the continuity pass (after editing scenes by hand). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Script")
	void RefreshContinuity();

	/** The checklist as Markdown: progress per act and chapter, each scene's status and carry-ins, warnings. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Script")
	FString BuildChecklist() const;

	/** Share of scenes Directed or Final (Cut scenes don't count). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	float GetProgress() const;

	FCinemafyScriptScene* FindScene(const FString& SceneId);
	TArray<FCinemafyScriptScene*> GetAllScenes();
	TArray<const FCinemafyScriptScene*> GetAllScenes() const;

	static FCinemafyScriptEditorRequest OnEditorRequest;
};

/** Reading scripts: screenplay and game-script parsing, structure inference, continuity. */
UCLASS()
class CINEMAFY_API UCinemafyScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Breaks a script into acts, chapters and scenes. Reads Fountain, Final Draft XML and plain screenplay or
	 * game-script text ("KADE: Stand down." lines, "MISSION 3" / "CHAPTER 2" markers). Where the script doesn't
	 * mark acts or chapters they're inferred: three-act breaks near the quarter marks, landing on turns (a fade
	 * out, a time jump, a new place); chapters grouped by continuous time and place.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Script")
	static TArray<FCinemafyScriptAct> BreakDownScript(const FString& ScriptText, bool bInferActs, int32 ScenesPerChapter, FString& OutTitle, FString& OutReport);

	/** Final Draft XML to Fountain-style text (headings, cues, dialogue, action). */
	static FString FinalDraftToText(const FString& Xml);

	/** Walks the scenes in order and fills each one's CarriedIn / Changes / Warnings. */
	static void TrackContinuity(TArray<FCinemafyScriptAct>& Acts);

	/** How well a set of spoken lines matches a scene's dialogue: share of the lines found in it (0..1). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static float ScoreLinesAgainstScene(const TArray<FString>& SpokenLines, const FCinemafyScriptScene& Scene);

	/** Points choice and divert targets at scene ids (after a breakdown or an edit). */
	static void ResolveStoryTargets(TArray<FCinemafyScriptAct>& Acts);

	/** True if every flag condition holds ("flag" set, "!flag" not set). */
	static bool FlagsHold(const TArray<FString>& Conditions, const TSet<FName>& Flags);

	/** "INT. BRIDGE - NIGHT" -> setting INT, location BRIDGE, time NIGHT. False if it isn't a scene heading. */
	static bool ParseHeading(const FString& Line, FString& OutSetting, FString& OutLocation, FString& OutTime);
};
