# Changelog

## 0.18.0: "The fine print"
Fixes from staging a 420-page game screenplay end to end.
- **Reading:**
  - `ON SCREEN:` (and `SUPER:`, `CHYRON:`, `CAPTION:`, `TITLE CARD:`, `HUD:`, `READOUT:`) blocks are display lines
    up to the next blank line, not speakers, chapters or scenes.
  - Chapter markers need a number: `MISSION ASSESSMENT: OPERATIONAL SUCCESS` no longer starts a chapter.
  - A screenplay that writes speech under cues no longer reads `The contrast settles: ...` in its action as dialogue.
  - Cast in a scene: speakers, plus whole names (or a titled name's own name) written with a capital. Single words
    of longer names ("hand", "dead", "and") no longer pull characters into scenes.
  - Group cues (`TARRIK AND SELENE`) credit each speaker and aren't cast members of their own.
  - Deaths need a character as the subject: "her channel dies", "his scream dies", "the dead console" kill nobody.
  - Between a prologue and an epilogue, the story gets three inferred acts on chapter starts (if the script marks none).
- **Voices:** announcements, systems, consoles, narrators, unseen voices and `(V.O.)` lines are voice-only: a
  **Voice Only** cast flag, no template character, mark or walk, and nobody waits on them for a cue. The cut watches
  whoever hears the line instead of going black.
- **Takes:**
  - A recorded take keeps when each line was said, so its director lines keep their text, audio and tone
    (they were all empty, 0.3 Neutral).
  - A cue that never comes no longer takes the waiting character's own line with it: after the timeout they go ahead.
- **Set dressing:** body parts, picture words ("foreground") and fragments ("heart instead of") aren't props.
- **Pre-flight:** Fix All places a start mark for every character it brings in (including the ones it casts as the
  template), not one per run. Spawned cast stand on their marks instead of falling through the floor. Fix All builds
  the cast before casting it.
- **Re-staging:** Previs drops bindings from earlier takes whose actors are gone before it spawns anyone, so a new
  character given an old object name doesn't inherit the old take. (Only bindings with Cinemafy Take tracks.)
- **Talking characters meet in the middle** instead of walking into each other.
- **The cut is the playback range:** the sequence ends at the last shot instead of playing the take's black tail.
- **Pre-flight:** warns when the level has no Player Start (the game's pawn would spawn at the origin during the
  take), with a fix that places one off set.
- A possessive isn't a death: "kills Selene's Frost" kills the Frost.
- Choreography wiring is logged once per performer, not on every beat.
- Mac (clang) build fix in the director.
- New test: `Cinemafy.Script.ReadsTheFinePrint`.

## 0.17.0: "Printed pages"
- **PDF screenplays:**
  - Import Script takes a `.pdf`. Its text comes from `pdftotext` (poppler: `brew install poppler` on a Mac), and
    a copy is kept in `Saved/Cinemafy`.
  - Text from a printed screenplay is cleaned before reading:
    - page numbers and `(MORE)` / `CONTINUED` are dropped, and `(CONT'D)` speeches rejoin across the page
    - scene numbers on either side of a heading are removed, and headings wrapped onto a second line are joined
    - paragraphs broken by the page join back up
    - a title page without `Title:` gives its first line as the title
- **Scene ids in screenplays:** a tag on the line under a heading (`SCOR-01-CS01`) becomes the scene's permanent
  id, as in a script bible.
- **Coded chapter headers:** `SCOR 01   THE MANTLE` is a chapter with id `SCOR-01` and title "The Mantle".
- **Bookend prologues:** a `PROLOGUE` (teaser, cold open) no longer runs on to the epilogue; the story starts at
  the next chapter after it.
- **Acts from the bible:** a screenplay whose scenes carry a bible's ids takes its acts from the bible's scene
  map (`GroupActsByMap`; found beside the screenplay, or set **Act Map File**).
- **Compare Scripts:** cross-checks two versions by scene id: scenes only in one, order, spoken words, speakers.
  Use it for a screenplay against its bible, or one draft against another (`compare_scripts` over MCP).
- Script bibles: `SELENE — INTERNAL` reads as Selene with an "(INTERNAL)" direction, and curly and straight
  apostrophes in names are the same.
- New sample: `Docs/samples/PrintedScreenplay_Sample.txt`.
- New test: `Cinemafy.Script.ReadsPrintedScreenplays`. The golden bible test also checks a screenplay (`.fountain`
  or `.txt`) in `CINEMAFY_BIBLE_DIR` against the bible.

## 0.16.0: "The bible"
- **Game script bibles:**
  - A game's cutscene bible imports directly: the JSON companion, or the Markdown it's written in.
  - Scenes keep the bible's ids (`SCOR-01-CS03`), which are never renumbered and are matched on re-import.
  - Missions become chapters. Acts and mission titles come from the bible's scene map; a JSON bible finds
    the Markdown beside it (or set **Act Map File**).
  - Lines keep their origin (`M` manuscript / `A` adapted) and source chapter, and the voice-over CSVs gain an
    Origin column.
  - `ON-SCREEN DISPLAY` lines are on-screen text, not a character: they get `OSD_` line ids and go into the
    localization table.
  - `(RECORD)` cues are a direction on the speaker.
  - Camera intent is read from the camera notes and the staging prose: two-shots, extreme close, someone's
    point of view, push in, handheld.
- **The game contract on every scene:**
  - what it carries: presentation (locked, staged dialogue, gameplay bridge, record, montage, radio/holo),
    trigger, entry state, control before and after, exit state, beats, handoff, camera and continuity notes,
    planned length, skippable
  - the Story Board shows it on each card and in its tooltip
  - `Scene_Contracts.csv` in the paperwork, and the `scene_contract` MCP tool
- **Gameplay-bridge inserts:**
  - Bridges split where the script hands control to the player. Any scene id can name an insert as
    `<scene>#<n>`.
  - Previs, Open and a Scene Director all work on one insert, with its own `SC_<scene>_I<n>` sequence.
- **Story Player for games:**
  - plays a bridge insert by insert, returning control between them (`OnControlReturned`) until
    `ContinueScene` or a Scene Trigger set to `<scene>#<n>`
  - binds the player's pawn to the character they control going in
  - respects unskippable scenes
  - sets a flag of the scene's id when it commits, played or skipped
  - tells the game who has control afterwards (`OnControlHandoff`)
- New samples: `Docs/samples/ScriptBible_Sample.md` / `.json`.
- New tests:
  - `Cinemafy.Script.ReadsScriptBibles`
  - `Cinemafy.Golden.ReadsTheGameBible`: point `CINEMAFY_BIBLE_DIR` at your own bible

## 0.15.0: "Off the page"
- **Camera language from the page:**
  - Screenplay camera directions now drive the director. They can sit on their own line, or open an action
    paragraph ("CLOSE ON KADE'S HAND. It trembles."). It reads:
    - framings: CLOSE ON, EXTREME CLOSE-UP, WIDE, TWO SHOT, OVER KADE'S SHOULDER, LOW ANGLE, ANGLE ON
    - POV and INSERT shots
    - moves: SLOW PUSH IN, PULL BACK TO REVEAL, CRANE UP, ARC AROUND, CRASH ZOOM
    - the camera's feel: HANDHELD, LOCKED OFF
    - BACK TO SCENE, which hands the camera back to the director
  - Each direction applies to the next line spoken. The director plays that framing on that subject, with
    that move, and never folds it into a held two-shot.
  - New **POV** and **Insert** shot types: through a character's eyes, and tight on a prop or a hand.
  - Transitions set how the next scene comes in: SMASH CUT (a hard cut whatever the style), MATCH CUT,
    DISSOLVE, FADE, WIPE and WHIP PAN.
  - Final Draft "Shot" paragraphs are read too.
- **Set dressing from the script:**
  - The action tells the art department what it needs:
    - set pieces where things happen ("vaults a cargo crate", "from the weapons rack")
    - hand props ("picks up a saber", "grabs the DATAPAD")
    - wardrobe ("wears his dress uniform")
  - Cast & Sets keeps a **Dressing** list per set, where art assigns real meshes.
  - **Dress the Set** places every missing set piece, labelled so Previs finds it:
    - in front of its mark, or in a row across the set
    - as the assigned mesh, or a placeholder block of a believable size
  - Previs walks up to a prop's near edge instead of into it.
  - Pre-flight offers **Dress the set**, the paperwork adds `Props_and_Dressing.md/.csv`, and there is a
    `dress_set` MCP tool.
- **Real lip-sync:**
  - **Lip-Sync** solves a face performance for each voiced line with MetaHuman's audio-driven animation. It
    runs in the background, one line at a time.
  - Each face is stored on its line with the audio it was made from: it follows the line across re-imports,
    goes stale with a new recording, and replaces the Talk curves when the scene is directed again.
  - It needs the MetaHuman plugin and the Python Editor Script Plugin. Without them, faces keep the Talk
    curves.
  - MCP tools: `generate_lip_sync` and `lip_sync_status`.
- New tests:
  - `Cinemafy.Script.CameraLanguageFromThePage`
  - set dressing checks in the golden sample test
  - lip-sync staleness checks in the revision test

## 0.14.0: "Rewrites and delivery"
- **Parentheticals drive the performance:** `(furious)`, `(whispering)`, `(through tears)`, `(coldly)` and friends set
  each line's emotion and intensity. That in turn drives the face, the body casting and the cut, and the
  scratch voice's pace, volume and whisper.
- **Script revisions:**
  - Re-importing a revised script fingerprints every scene and reports what was rewritten, added and removed
    (`Revision Report` on the project).
  - Rewritten lines that had recorded voice-over are flagged **re-record**, with the old words for the booth.
    The paperwork lists them, and Assign Recorded Voices clears the flag.
  - Scenes staged from older pages show **Out of date** on the Story Board.
  - **Update Changed** re-stages them. Choreographies you edited by hand are kept as `Edited_...`, and
    directors keep the tweaks on lines whose words didn't change.
- **Pre-flight check:**
  - Before Previs, the board says what it will change in the level and checks the scene: the cast on set
    or castable, marks or props for the places the script names, start marks, a nav mesh, voices, the
    right level, and stale pages.
  - Fix buttons place the marks, add a nav mesh, read scratch voices, cast a template character, build Cast
    & Sets or open the right level. **Fix All** does the lot.
  - A project-wide **Pre-flight** button, and the `check_scene` MCP tool.
- **Final Render:**
  - The delivery render for a scene, an act or the film: 1080p or 4K, cinematic quality, temporal
    anti-aliasing, PNG (or EXR) frames and the mixed audio as WAV.
  - Optionally a movie through the command-line encoder.
  - Output in `Saved/Cinemafy/Final`, and the `render_final` MCP tool.
- **Confirmations:** Previs, Dailies, Final Render and Light the set say what they'll change before they do
  it, in the editor UI. MCP tools never ask.
- **Golden test:** `Cinemafy.Golden.StagesTheSampleScript` breaks down and stages the tutorial's sample script
  end to end.
- New MCP tools: `script_revision`, `update_changed_scenes`, `check_scene`, `render_final`.

## 0.13.0: "Storytellers"
The whole workflow is in [STORYTELLING](Docs/STORYTELLING.md).

- **First-run tutorial:**
  - A step-by-step walk from script to cut scene, with do-it-for-me buttons. It opens on a project's first
    session and lives in the Hub afterwards.
  - It stays off in unattended, commandlet and test runs, and as soon as any Cinemafy MCP tool is called
    (the toolset reports each call).
  - Sample story, sample set builder, and a branching game-script sample.
- **Cast & Sets:**
  - Script characters map to level actors or Blueprints, with performer profiles, starting weapons, a player
    flag and scratch voices.
  - Continuity **Looks** attach props, swap materials, add tags and keep a limp to a walk; Blueprints can also
    react through `CinemafyContinuityReceiver`.
  - Locations map to levels and ambience.
  - **Cinemafy Marks** name the places the script talks about.
- **Previs This Scene:** stages a scene straight from the script.
  - Cast on set and dressed for continuity.
  - A choreography per character from the action verbs, marks and targets.
  - Lines on cue: beats can now wait for a specific line of another character.
  - Mood, sequence and director set up, then recorded and cut.
  - A report of what couldn't be staged.
- **Story Board:** every scene as a card by act and chapter, colored by progress.
  - Previs, Open and Direct on each card.
  - Takes kept side by side, and scenes can be reordered.
  - Master sequences per act and for the whole story.
- **Voices:**
  - Scratch voice-over from the platform's speech (macOS, Windows, Linux).
  - Recorded voice-over swapped in by line id.
  - Talking faces: audio-driven `Talk` curves baked onto Cinemafy Face.
- **Branching stories for games:**
  - Choices, diverts, flags and conditional scenes in the script.
  - `UCinemafyStorySubsystem` plays them in game with choice UI, skipping, save/restore, and the player's own
    pawn in their scenes.
  - `Cinemafy Scene Trigger`.
- **Mood from the script:** time of day, feeling and weather pick the director style, temp music, ambience
  and, optionally, the sun and sky.
- **Foley** under recorded takes, by event.
- **Cinemafy Extras:** background life directed in a sentence.
- **Dailies** with Movie Render Queue, and **review notes** on scenes.
- **Paperwork:** voice-over scripts per actor, subtitles, a localization string table, shot lists and an HTML
  storyboard.
- Line ids on script dialogue; new MCP tools for all of the above.
- New dependencies:
  - Plugin: MovieRenderPipeline.
  - Runtime modules: InputCore, AIModule.
  - Editor modules: DesktopPlatform, the MovieRenderPipeline modules, ImageWrapper, Projects.

## 0.12.0: "Productions"
- **Planned traversal routes:**
  - Vault and climb beats square up to the obstacle's face when the approach is oblique. They used to
    refuse it.
  - The route plans a run-up (vaults and gaps want speed, so a walking performer picks up to a run), a
    takeoff and an exit: the landing, the top of a mantle, or the far side of a gap.
  - The take follows the route: navmesh to the run-up start, then straight across the face. The next beat
    measures from where the route comes out. The route is drawn in the viewport.
- **Gaps and ledges:** the floor is walked for edges.
  - A gap is jumped from its edge, and a ledge is dropped from. Each is judged against new limits: Max Gap
    Width, Max Drop Height, Recovery Drop Height and Vault Run Up.
  - Too wide, too high, or a far side out of reach is refused with the numbers.
- **Transitions between actions:** each beat's in-between moves are planned from the beats around it.
  - **Draw before the strike:** `sword attack` draws the sword if it isn't in hand. `shoot` with nothing
    that shoots in hand draws a sidearm. From far off, the weapon comes out on the move; close in, a draw
    clip plays before the strike, with no contact scheduled on it.
  - **Brake out of a sprint:** before a jump, a climb or a melee charge, the performer gathers by dropping
    to a run, 2.4 to 3.2 m before committing.
  - **Roll or recovery:** a big drop or a long jump ends in a roll if the performer moves on, or a hard
    landing and recovery if it stops. A fall seen during the take adds one even when it wasn't planned.
  - The transitions show on beat labels ("+ draws the sword first, brakes out of the sprint") and are
    logged as Transition events. Switch them off per beat with **Auto Transitions**. Set what's in hand at
    the start with **Starting Weapon**.
- **Script breakdown** ([SCRIPTS](Docs/SCRIPTS.md)): import a full script (Fountain, Final Draft or game
  script) into a **Script Project**.
  - The script is broken into acts, chapters and scenes. Where the script doesn't mark them, acts and
    chapters are inferred, and the report says why each break is where it is.
  - Per scene: cast, lines, estimated time, and continuity (injuries, props in hand, costumes and deaths
    carried between scenes, with warnings).
  - Finished work maps back:
    - **Create Director For Scene** spawns a director with the scene's dialogue.
    - Directing a linked scene marks it Directed.
    - **Map Scenes In Level** matches directors by their lines.
  - **Export Checklist** writes a Markdown production checklist.
  - New MCP tools: `import_script`, `get_script_checklist`, `map_script_scenes`,
    `create_director_for_scene`, `set_script_scene_status`.
  - Sample: `Docs/samples/TheBridge.fountain`.

## 0.11.0: "Scenes"
- **Dialogue and choreography in one scene:**
  - Quoted words in a prompt are a line: `walk "Stand down, Admiral"` says it while walking.
  - A bare quote, `say`, or `answer "Make me."` is a **Say** beat: it faces its listener and holds for the line.
  - `answer` / `after` (or the beat's **Wait For Line From**) holds a beat until the other character's line
    ends, so Thane answers Kade, then she draws.
  - Beats have **Dialogue**, **Dialogue Audio**, **Listener** and **Wait For Line From**. During the take the
    audio plays on the performer with a subtitle, and `LineStart` / `LineEnd` events are logged.
  - The director cuts the take as one scene: lines become dialogue or walk-and-talk lines that start on
    their first word, the audio is laid into the cut, and strikes stay action.
- **Performer profiles** (`UCinemafyPerformerProfile` data asset): gait, crouch, traversal and overlay wiring,
  climb limits, move library and recording skips, written down once per character type.
  - Presets: GASP, GASPALS, SignalForge, MetaHuman on GASP, generic Character.
  - Map character classes to profiles in **Project Settings > Plugins > Cinemafy**, or set one per choreography.
  - **Save Wiring As Profile** and **Check Wiring** buttons; missing hooks are named, not guessed around.
- **Measured traversal:** each vault / climb beat measures its obstacle (height, depth, headroom, landing
  room, approach angle) against the performer's limits.
  - It says why before you record: "too tall (climbs up to 275 cm)", "no room to land on the far side",
    "approaching at 70 degrees".
  - The measured shape is drawn in the viewport (green works, red doesn't) and the label turns red.
  - Perform & Record lists impossible beats and asks first. During the take they fail honestly (no fake
    jump), and ankle-high obstacles are simply walked over.
- **The director watches the fight:**
  - Shots that land a hit frame both fighters: over the attacker's shoulder onto the hit, then over the
    defender's shoulder into the strike.
  - The killing blow gets a two-shot and a speed ramp, scaled to the style (gentle for calm styles, a hard
    drop for action ones; none for styles that never ramp). Focus goes to the one who falls.
  - Over-the-shoulders (and any shot with the counterpart in front) are rejected when the foreground
    head or shoulders cover the subject's face; the framing tries nearby angles instead.
- Scripted Perform & Record logs impossible beats instead of asking. New MCP tools:
  `set_choreography_beat_line`, `check_choreography_traversal`, `create_performer_profile`,
  `get_choreography_wiring`.

## 0.10.0: "Fights"
- **Synchronized attacker and defender:**
  - An attack on a character makes the target react, its impact frame timed to the strike's contact.
  - Hit reacts come from the side the blow lands on (front, back, left, right).
  - Knockdown (`heavy`, `knock down`), death (`kill`, `finish`), block (`blocked`), dodge (`dodged`), or none (`feint`).
  - In combos, every strike lands and the finisher gets the big reaction.
  - Defenders turn into blows they can see, stop moving for the exchange, and hold their final pose when downed.
  - A cut-off swing causes no reaction, and a busy defender isn't overridden (both are reported).
- **Defenders are recorded and baked** with the attacker's take when they have no choreography of their own.
- Per-beat interaction controls: `bTargetReacts`, reaction override, reaction clip override.
- Fix: modifiers like "heavy" no longer replace the action when picking clips ("heavy attack" looks for attacks).
- Contextual Animation adapter planned for paired content (see [CHOREOGRAPHY](Docs/CHOREOGRAPHY.md)).

## 0.9.2: fixes from the first v0.9 test (bridge scene)
- **Tracking shots stay out of walls:** the follow rig works like a spring arm. It pulls in in front of a
  blocker, or swings around the subject when pulling in would get too close (`bAvoidWalls`, `MinDistance`).
- **Blocked static shots:** wider fallback angles, then a pull-in in front of the blocker instead of a camera
  left behind a wall. These are still flagged.
- **Animation picks:** folder words now count half as much as the asset's own name. A folder-only match is
  rejected when the name says a different action (no more `MM_Dash` for "jump").
- **Wipe diagnostics:** `cinemafy.Transition.Dump 1` saves the outgoing capture to
  `Saved/Cinemafy/TransitionOutgoing_N.png` and logs its setup. `cinemafy.Transition.ManualExposure <EV>`
  and `cinemafy.Transition.CaptureSource <n>` test exposure and source overrides live. The capture now
  adapts exposure instantly.

## 0.9.1: "Honest performances" (from Vesryn's review)
- **Beat outcomes:** Succeeded, Fallback, Blocked, Failed or TimedOut, each with a reason, shown on
  the beat's viewport label and in the bake toast.
  - A jump standing in for a vault is now a labeled Fallback, not a success.
  - Blocked moves say so.
- **Arrival policies** per beat:
  - Pass through before more travel or a charge (no idle at waypoints).
  - Enter vaults moving.
  - Stop and settle before gestures and looks (no taunting mid-skid).
- **Typed performance events:** beat start, arrived, action start, takeoff, landing, contact, action end.
  - Contacts are authored (hit/impact/contact notify) or flagged estimated.
  - A contact is only published if the swing is still playing when it lands.
  - The director's cut points come from these events.
- **Bake fixes:**
  - The root bone is kept, preserving GASP's root offset in turns; no double counting.
  - Recording uses the sequence's display rate, sampled in whole frames.
  - Only the previous take's track is replaced; hand-made transform tracks are muted, never deleted.
  - `SkipRecordingComponents` replaces the "Face" name guess.
- **Melee warp target** is now where the attacker should stand, facing the target, not the target's center.
- **The performer is restored** after a take: possession, gait flags, crouch, speed, warp targets.
- [Choreography design contract](Docs/CHOREOGRAPHY.md) and roadmap items for adapters, traversal
  queries, paired interactions and bake validation.

## 0.9.0: "Cinemafy Edit"
- **Premiere-style editing in Sequencer:**
  - Razor (shot or all tracks), sync-locked **ripple delete**, ripple trim start/end (Q/W).
  - **Roll** edits for J/L cuts, swap with next, jump between cuts.
  - Ripples carry keys inside long sections, markers and the playback range.
- **Transitions on any cut:** iris and clock wipes (stencil-clipped, no materials), dip to white, whip
  pan, plus wipes, dissolves and native fades. The Space Opera style's alternating wipes now include
  clock and iris.
- **Titles:**
  - Lower thirds, typed-on location cards, chapter cards and centered titles.
  - A perspective **opening crawl** with an intro line and a falling title.
  - Keyed progress, so titles scrub and pause with the sequence.
- **Audio:** duck music under dialogue; J-cut and L-cut buttons.
- **Markers:** marker at playhead, beat markers from the music, snap cuts to markers.
- **Grade:** copy, paste, and apply to all shots (depth of field stays per shot).
- **Multicam switcher:** live preview grid of the sequence's cameras. Arm, play, tap 1-9; stopping
  commits the switched edit over the range you played.
- Keyboard shortcuts bound into Sequencer (rebindable), a Cinemafy Edit dropdown on Sequencer's
  toolbar, and Edit and Multicam buttons in the Hub.
- MCP toolset: `edit_at_playhead`, `add_transition_at_nearest_cut`, `add_title_at_playhead`,
  `duck_music_under_dialogue`.

## 0.8.0: "Choreographer"
- **Click-to-choreograph:** pick a character, click points in the level, type one or two words per
  point (`walk`, `vault`, `sprint`, `climb`, `sword attack`, `taunt`...). A popup with quick chips
  shows, plus labeled route lines in the viewport. Undoable.
- **The performer brain** reads prompts (gaits, traversal, attacks, combos, ranged, weapons, waits,
  looks, gestures) and picks animations by name and folder from the project or a curated **Move
  Library**. It filters by skeleton compatibility, chains numbered combos, avoids repeats, and never
  uses locomotion or hit reacts as an attack.
- **Real performances, not splices:** motion matching drives the locomotion live. Traversal uses the
  character's own system (GASP `TryTraversalAction`). Overlays switch by enum display name (GASPALS).
  Attacks get motion-warp targets. Everything is auto-wired by reflection, with overrides.
- **Perform & Record:** play in editor, record every choreographed character, auto-stop, and bake a
  transform track plus recorded body animation into the open Level Sequence.
- **Direct Shots:** the take becomes Scene Director lines. Impacts cut on the hit, sprints track,
  targets become reverses. Multi-character takes merge onto one timeline.
- MCP toolset: `create_choreography`, `add_choreography_beat`, `read_choreography_prompt`,
  `perform_and_record_choreography`, `direct_choreography`.
- Tests for prompt reading, name scoring, combo chains, overlay matching and take-to-lines.

## 0.7.0: "The Director grows up"
- **Director style assets** and six archetype presets. **Space Opera Classic** is modeled on classic-trilogy
  craft: locked-off classical coverage, two-shots held through exchanges, wipes, brisk action cutting,
  almost no slow-mo, anamorphic.
- **Transitions:** live wipes (the outgoing shot keeps playing), dissolves, fades through black (native Fade
  track), match cuts, scene breaks.
- **Hold-through with rack focus**, **walk-and-talk** tracking on a follow rig, and **action coverage** (cut on
  impact, crash zooms, speed ramps).
- **Cross-cutting** between two directors, and **music-aware cutting** (beat detection plus snapping).
- **Timeline editing** in the Shot Browser (lock, alternate, nudge) and **takes**.
- **Performance import:** CSV scripts and take auto-assignment.
- **Subtitles v2:** reading-speed timing, smart chunking, localization-aware.
- **The brain:** moment reading (reveal, silent beat, new exchange, action, walk & talk) drives tool choice,
  with a plain-language decision report.
- Plan/bake split. More automation tests (transitions, action, walking, beats, locks, subtitles, moments).

## 0.6.0: overnight build
- **Smart camera placement:** avoids walls and occluders while keeping the 180° side.
- **Cut on the breath:** in-line cuts snap to pauses, and push-ins land on stressed words.
- **Performance clips per line:** body and face animation baked onto the right mesh.
- **Subtitles:** live and in directed sequences, with an accessibility switch.
- **Group scenes:** group shot, bystander reactions, re-establishing.
- **Styles drive look and light:** each preset grades, frames and lights the scene.
- **Shot Browser v2:** director controls, pacing strip, reasoning on cards.
- **Continuity checker:** line crossings, jump cuts, flash cuts and redundant cuts in any sequence.
- **Cinemafy Hub:** one panel for every tool.
- **Automation tests** for lenses, easing, shot sizes and planner rules. Jump cuts are now impossible.
- Lip-sync seals only trigger mid-speech. Build notes for engine-API drift.

## 0.5.0
- **Scene Director:** style presets, coverage planner, Sequencer bake, re-roll, live directing.
- **Director v2:** camera moves, emotion per line, Learn Style from an edit, reverse-shot size matching.
- **Intensity from audio:** loudness, peaks and speech rate, scored relative to the scene. Dialogue track placement.

## 0.4.0
- **Cinemafy Face:** region gain, One Euro jitter cleanup, flesh springs, emotions, breathing, life, pupils.
- **Lip sync:** P/B/M seals with hold, F/V tuck, vowel boost, jaw lift, silence gate.
- **Listener:** hears the speaker's face, with backchannel nods, emphasis reactions and temperaments.

## 0.3.0
- **Shot Browser.** **Gameplay ↔ cutscene handoff** (letterbox, anchoring, final-camera alignment, skip).
- **Light Rigs:** camera-relative, lux-based, character-only.
- Fixes for UE 5.8 Sequencer deprecations.

## 0.2.0
- Camera moves, offset stack, Sequencer shot integration, 180° line check.

## 0.1.0
- Lens kit, shot solver and Create Shot, Camera Feel, Eye Focus, Gaze, looks and letterbox.
