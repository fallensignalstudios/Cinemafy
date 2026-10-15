#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyProduction.h"
#include "CinemafyTypes.h"
#include "CinemafyScript.generated.h"

class ULevelSequence;
class UWorld;
class USoundBase;
class UAnimSequenceBase;
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

/** What the last re-import did to a scene. */
UENUM(BlueprintType)
enum class ECinemafySceneRevision : uint8
{
	Unchanged  UMETA(DisplayName = "Unchanged"),
	Rewritten  UMETA(DisplayName = "Rewritten"),
	New        UMETA(DisplayName = "New")
};

/** What kind of script paragraph a line is. */
UENUM(BlueprintType)
enum class ECinemafyScriptLineKind : uint8
{
	Dialogue    UMETA(DisplayName = "Dialogue"),
	Action      UMETA(DisplayName = "Action"),
	Transition  UMETA(DisplayName = "Transition"),
	/** A camera direction: "CLOSE ON THANE", "KADE'S POV", "INSERT - THE DATAPAD", "SLOW PUSH IN". */
	Shot        UMETA(DisplayName = "Shot"),
	/** Text shown on screen, not spoken: "MOMENTS EARLIER", "TARGET: EMPEROR CAELUS VIREL". */
	Display     UMETA(DisplayName = "On-Screen Display")
};

/** How a game presents a scene (from a script bible). */
UENUM(BlueprintType)
enum class ECinemafyPresentation : uint8
{
	/** A screenplay scene: no game contract. */
	Screenplay      UMETA(DisplayName = "Screenplay"),
	/** Play stops; the sequence plays through. */
	Locked          UMETA(DisplayName = "Locked Cinematic"),
	/** A dedicated conversation scene. */
	Dialogue        UMETA(DisplayName = "Staged Dialogue"),
	/** Short inserts around playable stretches: control comes and goes. */
	GameplayBridge  UMETA(DisplayName = "Gameplay Bridge"),
	/** A recording or vision the characters watch. */
	Record          UMETA(DisplayName = "Record / Witness"),
	Montage         UMETA(DisplayName = "Montage"),
	/** Radio or hologram: the player keeps control. */
	Radio           UMETA(DisplayName = "Radio / Holo (in control)")
};

/** A game's contract for a scene: when it plays, who has control before and after, what it commits. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyGameContract
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	ECinemafyPresentation Presentation = ECinemafyPresentation::Screenplay;

	/** What starts it in play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (MultiLine = true))
	FString Trigger;

	/** What's true when it starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (MultiLine = true))
	FString EntryState;

	/** Who the player controls going in and coming out ("Tarrik", "Selene", "None"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	FString ControlBefore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	FString ControlAfter;

	/** What happens next in play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (MultiLine = true))
	FString Handoff;

	/** The canonical state it commits, played or skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	TArray<FString> ExitState;

	/** Beats the scene has to land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	TArray<FString> RequiredBeats;

	/** The bible's camera and performance notes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (MultiLine = true))
	FString CameraNotes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (MultiLine = true))
	FString ContinuityNotes;

	/** The full location as written, and the cast as listed (full names). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	FString LocationDetail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	TArray<FString> CastNotes;

	/** Where it comes from in the source ("Chapter 1; printed p. 4, 5, 8"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	FString SourceRef;

	/** The planned length (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	float TargetSeconds = 0.f;

	/** The player can skip its presentation (its state still commits). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	bool bSkippable = true;

	/** Gameplay happens between its inserts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	bool bInterleavedGameplay = false;
};

/** One playable-to-playable piece of a gameplay bridge: its own short sequence, control back to the player after it. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafySceneInsert
{
	GENERATED_BODY()

	/** The script lines it covers (indices into the scene's Lines, inclusive). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Insert")
	int32 FirstLine = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Insert")
	int32 LastLine = 0;

	/** The direction that hands control to the player at its end ("rises into player control"), if any. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Insert", meta = (MultiLine = true))
	FString HandsControl;

	/** Its sequence (SC_<scene>_I<n>), made by Previs or Open on "<scene>#<n>". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insert")
	TSoftObjectPtr<ULevelSequence> Sequence;
};

/** A camera direction read from the page. It applies to the next line spoken. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyShotCall
{
	GENERATED_BODY()

	/** It names a framing (close, wide, POV, insert...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bFraming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	ECinemafyShotType ShotType = ECinemafyShotType::CloseUp;

	/** Who or what it's on, as written ("THANE", "THE DATAPAD", "KADE'S HAND"). For a POV, whose eyes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FString Subject;

	/** "OVER KADE'S SHOULDER": the shot is on whoever Kade faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bOverShoulderOfSubject = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bMove = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	ECinemafyCameraMove Move = ECinemafyCameraMove::PushIn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bFeel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	ECinemafyCameraFeel Feel = ECinemafyCameraFeel::Handheld;

	/** "BACK TO SCENE": the director takes over again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bBackToScene = false;
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

	/** A camera direction (Kind = Shot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FCinemafyShotCall Shot;

	/** Where the line comes from in a script bible: "M" (manuscript) or "A" (adapted), and its source chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FString Origin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FString SourceRef;

	/** The face's performance of this line: MetaHuman's audio-driven lip-sync, or a captured take. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	TSoftObjectPtr<UAnimSequenceBase> FaceAnimation;

	/** The audio the face was made from: a new recording makes it stale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line", AdvancedDisplay)
	FSoftObjectPath FaceMadeFrom;

	/** It has a face performance made from its current audio. */
	bool HasCurrentFace() const { return !FaceAnimation.IsNull() && !Audio.IsNull() && FaceMadeFrom == Audio.ToSoftObjectPath(); }

	/** The line was rewritten after its voice-over was recorded: it needs recording again. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line")
	bool bNeedsRerecord = false;

	/** What it said before the rewrite (for the booth). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Line")
	FString PreviousText;
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

/** How a line is delivered, read from its parenthetical ("(coldly)", "(shouting)", "(through tears)"). */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyDelivery
{
	GENERATED_BODY()

	/** The direction names a feeling (otherwise Emotion is left to the director). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bHasEmotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	ECinemafyEmotion Emotion = ECinemafyEmotion::Neutral;

	/** 0 = barely there, 1 = all out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	float Intensity = 0.5f;

	/** Speaking pace for scratch reads (1 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	float Rate = 1.f;

	/** Loudness for scratch reads (1 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	float Volume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bWhisper = false;

	/** The words that set it ("coldly"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	FString Cue;
};

/** A review note on a scene (from dailies or a screening). */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyReviewNote
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	FString Author;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	FString Date;

	/** Seconds into the scene it's about (-1 = the whole scene). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	float AtSeconds = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review", meta = (MultiLine = true))
	FString Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	bool bResolved = false;
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

	/** Act.Chapter.Scene, e.g. "2.3.1"; or the script bible's own id ("SCOR-01-CS03"), which never changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
	FString Id;

	/** The id is the script's own: moving the scene doesn't renumber it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene")
	bool bFixedId = false;

	/** The game's contract for it (from a script bible): presentation, trigger, control handoff, exit state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	FCinemafyGameContract Game;

	/** A gameplay bridge's inserts: each its own sequence, with control back to the player between them. Address one as "<scene id>#<n>" (1-based). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	TArray<FCinemafySceneInsert> Inserts;

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

	/**
	 * Your staging, in plain words, kept apart from the script's text: "Tarrik and Lyessa lie on the bunk, cuddled,
	 * for the whole scene." Previs puts people where and how it says (it wins over the action lines). From a Fountain
	 * note ([[BLOCKING: ...]]), a bible's staging field, or the Story Board; kept across re-imports.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene", meta = (MultiLine = true))
	FString Blocking;

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

	/** Notes from dailies and screenings; open ones show on the Story Board and in the checklist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production", meta = (TitleProperty = "{Author}: {Text}"))
	TArray<FCinemafyReviewNote> ReviewNotes;

	// --- Revisions ------------------------------------------------------------------------------------------------

	/** A fingerprint of the scene's words (heading, action, dialogue, directions). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Revision")
	FString ContentHash;

	/** The fingerprint when it was last staged or given a director: differs = the script changed since. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Revision")
	FString StagedHash;

	/** What the last re-import did to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Revision")
	ECinemafySceneRevision Revision = ECinemafySceneRevision::New;

	/** Fingerprints of what was generated for it (choreography / director label -> beats), to spot hand edits. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Revision", AdvancedDisplay)
	TMap<FString, FString> GeneratedPrints;

	/** Staged (or directed) from an older version of the script. */
	bool IsOutOfDate() const { return !StagedHash.IsEmpty() && StagedHash != ContentHash; }

	// --- Camera ---------------------------------------------------------------------------------------------------

	/** How the scene comes in, from the transition that ended the one before ("SMASH CUT TO:", "DISSOLVE TO:"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (InlineEditConditionToggle))
	bool bTransitionIn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (EditCondition = "bTransitionIn"))
	ECinemafyTransition TransitionIn = ECinemafyTransition::Cut;

	/** Rendered dailies for the current take (folder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Production")
	FString DailiesFolder;
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

	/** For a JSON script bible: the Markdown bible to take acts and mission titles from (found beside the JSON when empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script", meta = (FilePathFilter = "md"))
	FFilePath ActMapFile;

	/** What the last re-import changed: rewritten, new and removed scenes, lines to re-record. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Script", meta = (MultiLine = true))
	FString RevisionReport;

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

	/** A scene by id or running number. "<id>#<n>" (an insert of a gameplay bridge) finds its scene. */
	FCinemafyScriptScene* FindScene(const FString& SceneId);

	/** The insert "<id>#<n>" names (1-based), or null for a plain scene id. */
	FCinemafySceneInsert* FindInsert(const FString& SceneId);
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

	/**
	 * How a line should be delivered: its parenthetical ("(coldly)", "(shouting)", "(whispering)", "(through tears)",
	 * "(laughing)", "(terrified)"...) gives a feeling, an intensity, and a pace and loudness for scratch reads;
	 * a shouted line in capitals or with "!!" reads as angry and loud even without one.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static FCinemafyDelivery ReadDelivery(const FString& Direction, const FString& Text);

	/**
	 * ReadDelivery, and when the page gives no direction and the words settle nothing ("I love you" does; "Yes." doesn't),
	 * the scene's mood sets a gentle baseline: a romantic scene plays tender, a sad one sad, a tense one uneasy.
	 * Mood is a Scene Mood's word (calm, tense, action, sad, warm, romantic, triumphant, eerie, alarm).
	 */
	/** The scene's mood after a line: a warm scene whose words turn dark (fear, grief, anger) plays tense from there. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static FString MoodAfterLine(const FString& Mood, const FCinemafyDelivery& Line);

	/** The direction a scene's mood stands for when a line has none ("romantic" -> "tender"); empty for calm. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static FString MoodDirection(const FString& Mood);

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static FCinemafyDelivery ReadDeliveryInMood(const FString& Direction, const FString& Text, const FString& Mood);

	/**
	 * Reads a camera direction: framings ("CLOSE ON THANE", "WIDE", "TWO SHOT", "OVER KADE'S SHOULDER", "LOW ANGLE ON",
	 * "KADE'S POV", "INSERT - THE DATAPAD", "ANGLE ON"), moves ("SLOW PUSH IN ON", "PULL BACK TO REVEAL", "CRANE UP",
	 * "ARC AROUND", "CRASH ZOOM"), the camera's feel ("HANDHELD", "LOCKED OFF") and "BACK TO SCENE". False when the
	 * words aren't a camera direction.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static bool ReadShotCall(const FString& Text, FCinemafyShotCall& OutCall);

	/**
	 * Reads a transition into the next scene: SMASH CUT (a hard cut, whatever the style), MATCH CUT, DISSOLVE,
	 * FADE TO BLACK / FADE OUT, FADE TO WHITE, WIPE, WHIP PAN. False for a plain "CUT TO:" (the style decides).
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static bool ReadTransition(const FString& Text, ECinemafyTransition& OutTransition);

	/**
	 * Reads camera intent from prose ("Warm cramped two-shot on gunship", "Tarrik's point of view sinks into mud",
	 * "Close on the seal.", "Slow push in as she signs"): a direction at the start of a sentence, or an unmistakable
	 * camera phrase anywhere in it (two-shot, point of view, over-the-shoulder, extreme close, push in, handheld).
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static bool ReadCameraProse(const FString& Sentence, FCinemafyShotCall& OutCall);

	/** A game script bible: JSON with a "scenes" list of scene_id / script entries, or the Markdown bible ("#### <id>" scenes with a "Performance script"). */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static bool IsScriptBible(const FString& Text);

	/**
	 * Breaks down a script bible. Scenes keep their ids; missions become chapters; acts come from the bible's scene map
	 * (the Markdown's "### Act N: ..." tables). JSON has no acts: pass the Markdown bible as ActMapText to borrow its
	 * acts and mission titles (else Prologue/Epilogue get their own acts and the rest is one). Each scene gets its game
	 * contract, on-screen display lines, line origins, camera directions read from its notes and staging prose, and a
	 * gameplay bridge is split into inserts where control goes back to the player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Script")
	static TArray<FCinemafyScriptAct> BreakDownScriptBible(const FString& Text, const FString& ActMapText, FString& OutTitle, FString& OutReport);

	/**
	 * Regroups a breakdown's scenes into the acts of a script bible's scene map (the Markdown's "### Act N: ..."
	 * tables), by scene id; chapters stay as they were. For a screenplay whose scenes carry the bible's ids.
	 * False (nothing changed) when the map has no acts or names none of the scenes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Script")
	static bool GroupActsByMap(UPARAM(ref) TArray<FCinemafyScriptAct>& Acts, const FString& ActMapText);

	/**
	 * Cross-checks two versions of a story by scene id (a screenplay and its script bible, or two drafts): scenes only
	 * in one, scenes out of order, and scenes whose spoken words or speakers differ. Returns a report; OutMatching
	 * counts the scenes that agree.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Script")
	static FString CompareScripts(const TArray<FCinemafyScriptAct>& A, const TArray<FCinemafyScriptAct>& B, int32& OutMatching);

	/**
	 * A voice with no body on set: (V.O.) lines, and speakers that are announcements, systems, intercoms, radios,
	 * narrators or unseen voices ("SHIP ANNOUNCEMENT", "UNIDENTIFIED VOICE"). They're heard and subtitled, never cast,
	 * marked, walked or cut to.
	 */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static bool IsVoiceOnly(const FString& Speaker, const FString& Direction);

	/** The people behind a cue: "TARRIK AND SELENE" -> TARRIK, SELENE; "KADE" -> KADE; "ALL" -> nobody in particular. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static TArray<FString> PeopleInCue(const FString& Speaker);

	/** "SCOR-01-CS01#2" -> "SCOR-01-CS01" and 2 (1-based); a plain id -> itself and 0. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static FString SplitInsertId(const FString& SceneId, int32& OutInsert);

	/** One insert of a gameplay bridge as a scene of its own (its lines only, id "<scene>#<n>"); 0 or a bad index = the whole scene. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Script")
	static FCinemafyScriptScene SceneForInsert(const FCinemafyScriptScene& Scene, int32 Insert);

	/** A fingerprint of a scene's words (what Revision and Out of Date compare). */
	static FString ComputeContentHash(const FCinemafyScriptScene& Scene);

	/** Points choice and divert targets at scene ids (after a breakdown or an edit). */
	static void ResolveStoryTargets(TArray<FCinemafyScriptAct>& Acts);

	/** True if every flag condition holds ("flag" set, "!flag" not set). */
	static bool FlagsHold(const TArray<FString>& Conditions, const TSet<FName>& Flags);

	/** "INT. BRIDGE - NIGHT" -> setting INT, location BRIDGE, time NIGHT. False if it isn't a scene heading. */
	static bool ParseHeading(const FString& Line, FString& OutSetting, FString& OutLocation, FString& OutTime);
};
