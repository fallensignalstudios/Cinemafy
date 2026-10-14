# Changelog

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
