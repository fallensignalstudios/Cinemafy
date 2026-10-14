# Telling a story with Cinemafy

Cinemafy goes from a script to a cut scene, then lets you shape every part of it. This page walks the whole
workflow. On a project's first editor session, the same walk-through opens as a tutorial. You can get it
back any time from **Hub > Tutorial**.

```
Script ─► Story Board ─► Cast & Sets ─► Scratch voices ─► Previs ─► Refine ─► Cut together ─► Dailies & paperwork
                                                                                   └─► Story Player (games)
```

## 1. Bring the script

**Hub > Import Script**, or the **Story Board**'s Import button, accepts:

- Fountain
- Final Draft (`.fdx`)
- plain screenplay text
- game scripts (`KADE: line`, `[Scene: Hangar]`, `MISSION 3: Blackout`)

Cinemafy breaks the script into acts, chapters and scenes, inferring any the script doesn't mark. It then
tracks continuity across scenes: injuries, props in hand, costumes and deaths. See [SCRIPTS](SCRIPTS.md).

Try it with `Docs/samples/TheBridge.fountain` (a film scene), or `Docs/samples/Mutiny_Branching.txt` (a
branching game script). The tutorial's **Try the sample story** imports the first one.

## 2. The Story Board

Every scene appears as a card, grouped by act and chapter.

- **Colors** show progress: grey not started, blue blocking, purple recorded, green directed, gold final.
- **Hover** a card for its synopsis, what each character carries in from earlier scenes, continuity
  warnings and open review notes.
- **Card buttons:** Previs, Open (the scene's sequence) and Direct. The **...** menu has status, staging
  without recording, dressing the cast, mood, lighting, scratch voices, review notes, dailies, takes, and
  moving the scene earlier or later.
- **Toolbar:** Import, Cast & Sets, Scratch Voices, Map Scenes, Cut Together, Dailies, Paperwork, Checklist.

## 3. Cast & Sets

**Cast & Sets** fills the Script Project with every speaking character and every location.

For each **cast member**:

- point them at a character already in your level (**Actor Label**), or a Blueprint to spawn
  (**Character Class**)
- give them a **Performer Profile**, a **Starting Weapon**, a **Scratch Voice**, and **Is Player** (games)
- add **Looks** for continuity

Looks are how continuity shows up on screen. Facts carried into a scene:

- attach props (a pistol in `hand_r`)
- swap materials (blood, dirt, a different uniform)
- add actor tags your Blueprint can read
- keep a limping character to a walk (**Walk Only**)

Characters that implement `CinemafyContinuityReceiver` get `ApplyContinuity` / `ClearContinuity` calls.

**Sets** map script locations to levels, with their ambience. Place **Cinemafy Marks** on the set and name
them the way the script talks: "console", "lift", "window". **Start** marks are where spawned cast members
begin. The tutorial's **Build the sample set** lays out a floor, a crate and marks for the sample, and casts
a template character (GASP or Third Person) if your project has one.

## 4. Scratch voices

**Scratch Voices** reads lines aloud with the computer's voices, so timing, pacing and lip motion work
before anyone records:

| Platform | Engine |
| --- | --- |
| macOS | `say` |
| Windows | System.Speech |
| Linux | `espeak-ng` |

Each character uses their cast entry's voice and rate. The reads land in `/Game/Cinemafy/ScratchVO` and are
attached to the script lines and to the directors that speak them.

**Talking faces:** when a director lays a line's audio into a sequence, it also keys the speaker's
**Cinemafy Face** `Talk` / `Talk Narrow` from the audio's loudness and hiss. The face node adds jaw and lip
motion on top of whatever the face is doing. Lower **Talk Strength** for characters that already have
captured lip-sync. This is a stand-in, not MetaHuman Animator quality: swap in a captured performance for
hero shots.

**Real voice-over:** name the files by **line id** (the paperwork's voice-over scripts list them) or
`<scene>_<line>` (`2_1_3_04`). Then **Assign Recorded Voices** swaps them in. Re-Direct a scene and the cut
re-times to the real performances.

## 5. Previs

**Previs** on a card stages the scene from the script:

1. The cast is put on set: found by label, or spawned at start marks.
2. Each character is dressed for continuity.
3. A choreography is built per character:
   - **Moves:** "walks to the console" sends them to the console mark, or a prop with that name.
   - **Traversal:** vaults, climbs and jumps.
   - **Weapons:** draws and holsters.
   - **Violence:** strikes, shoots and kills (aimed at whoever is named).
   - **Looks and gestures:** looks, turns to someone, gestures, waits and dodges.
   - **Lines:** said to the last speaker, each waiting for the line it answers. A line said right after
     the speaker's own move becomes walk-and-talk.
4. The scene's mood picks the director style, temp music and ambience.
5. The scene's sequence (`/Game/Cinemafy/Scenes/SC_<id>_<place>`) and Scene Director are made.
6. The take is performed, recorded and cut.

The report lists what it staged and what it couldn't: a sentence it didn't understand, a missing mark, a
character not on set. **Stage without recording** (in the card's **...** menu) builds everything but the
take, so you can adjust beats first.

## 6. Refine

Everything Previs builds is ordinary Cinemafy and Unreal:

- **Choreographies:** click beats to change them, or **Choreograph** more ("sword attack", "vault",
  `walk "a line"`).
- **Perform & Record** re-shoots.
- **Scene Directors** re-cut in other styles.
- **Cinemafy Edit** trims, ripples, and adds transitions and titles in Sequencer.

**New Take** keeps the current version and starts a copy; **Use take** switches between them.

**Extras:** a **Cinemafy Extras** actor directed in a sentence:

- "crew works at consoles": at marks named console
- "marines patrol"
- "two officers talk"
- "crowd mills about"

**Populate** spawns them with varied loops from your animation library. Patrols and wanderers walk the navmesh
in play. In edited sequences they hold their loops, because takes don't record extras.

**Mood:** **Set the mood** applies the director style, music and ambience for the scene's time of day and
feeling. **Light the set** also turns the level's sun and sky to the time of day.

**Foley:** set sounds for impact, takeoff, land, draw, holster, roll and body_fall under **Project Settings
> Cinemafy > Foley Sounds**. Directed takes then get a **Cinemafy Foley** track at those moments.

## 7. Cut together, review, paperwork

- **Cut Together:** a master sequence per act, plus one for the whole story, scenes back to back
  (`/Game/Cinemafy/Masters`).
- **Map Scenes:** finds Scene Directors already in the level by their lines, and updates progress.
- **Dailies:** Movie Render Queue, 720p frames per scene, to `Saved/Cinemafy/Dailies`. This replaces the
  jobs in the editor's render queue.
- **Review notes:** add them on cards. Open notes show on the board and in the checklist.
- **Paperwork** (`Saved/Cinemafy/Paperwork`):
  - voice-over recording scripts per actor (Markdown + CSV, with cues, directions, line-id file names and
    recording status)
  - story subtitles (SRT)
  - a localization String Table CSV keyed by line id
  - shot lists
  - an HTML storyboard with a frame from each shot's camera
- **Checklist:** progress, carry-ins, warnings and notes, per act, chapter and scene.

## 8. Games: the Story Player

Scripts can branch:

```
* [Spare him] -> Brig ~ spared_thane       a choice: text, target, flags it sets
* {has_backup} Call the marines -> #4      only offered when has_backup is set
-> END                                     where the story goes after this scene
~ thane_alive                              set a flag when the scene plays (~ !flag clears it)
? spared_thane                             only play this scene when the flag is set
```

Targets can be a scene id, a running number (`#4`), a heading or place, or `END`.

**`UCinemafyStorySubsystem`** (Game Instance subsystem) plays the story:

- each scene's sequence, through Cinemafy's cutscene handoff (blend, letterbox, camera alignment)
- flags, choices (built-in buttons, keys 1-9, or your UI via `OnChoices` / `Choose`), diverts, and scenes
  passed over when their conditions don't hold
- skippable with Escape or the gamepad's Special Right button
- flags you can save and restore

The cast member marked **Is Player** is played by the player's own pawn, gear and all.

**Cinemafy Scene Trigger** starts a scene when the player walks in: once, optionally gated on flags, and
optionally following the story from there.

## The tutorial and automation

The tutorial opens about eight seconds after the editor starts, on a project's first session. It stays
closed:

- once it has been finished, or unticked
- in unattended, commandlet or automation-test runs
- with `-NoCinemafyTutorial` or the `CINEMAFY_NO_TUTORIAL` environment variable
- as soon as any Cinemafy MCP tool is called: the toolset reports the call, the tutorial closes, and it
  won't open on its own in that project again

Reopen it from **Hub > Tutorial**.
