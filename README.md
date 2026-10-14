# Cinemafy 🎬

**Premium, film-grade real-time cutscenes for Unreal Engine 5.8, with less fiddling.**

Cinemafy packages the rules real DPs, focus pullers and colorists follow into one-click tools:
cinema lens kits, coverage shots that frame themselves, human camera feel, eye-tracked focus,
living MetaHuman eyes and grading looks. It was built for *Sovereign Call*'s in-engine cutscenes.

> Status: **v0.7 (beta)**. New here? Start with **[Getting Started](Docs/GETTING_STARTED.md)** (15 minutes to a
> directed scene). Also see the [changelog](CHANGELOG.md), [architecture](Docs/ARCHITECTURE.md),
> [roadmap](Docs/ROADMAP.md) and [build notes](Docs/BUILD_NOTES.md).

---

## Install

1. Clone into your project's `Plugins/` folder:
   ```
   <YourProject>/Plugins/Cinemafy/Cinemafy.uplugin
   ```
2. Regenerate project files and build (or just open the `.uproject` and let it compile).
3. Enable **Cinemafy** under *Edit → Plugins* if it isn't already enabled.

You'll get a **Tools → Cinemafy** menu in the level editor. Its first entry, **Cinemafy Hub**, opens
one dockable panel with every tool grouped by job: Scene, Shots, Camera, Characters, Review.

---

## What's in the box

| Tool | What it does | Where |
|---|---|---|
| **Create Shot** | Select the subject, then who they're talking to, and pick a shot. You get a CineCamera that's framed (eyes on the upper third, lead room), lensed with the right prime, focused on the eyes, and **always on the same side of the 180° line**. If a Level Sequence is open, the camera is **added to it with a camera cut at the playhead**. | Tools → Cinemafy → Create Shot |
| **Scene Director** 🎬 | An AI director for dialogue scenes. Write lines (speaker, listener, duration, intensity), pick a style (Classic Hollywood, Intimate Drama, Kinetic Action, Slow Burn, Noir Thriller, Epic or custom), and hit **Direct**. It plans the coverage and the edit with real editing craft and bakes cameras, lenses, camera feel and timed cuts into the open sequence. **Re-roll** for another take; tweaks you make to its cameras survive. At runtime, `DirectLine()` cuts gameplay conversations live. | Tools → Cinemafy → Scene Director / `Cinemafy Scene Director` actor |
| **Shot Browser** | The director's control room. Edit the cut right on the cards: **Lock** shots you love (re-rolls keep them and plan around them), **Alt** swaps in the director's next-best shot, **< >** nudge a cut by a quarter second. **Save Take** and the **Takes** menu flip between versions. Also: **Direct / Re-roll / Learn Style** buttons, a **pacing strip** showing every shot as a bar (width = duration, color = wide to tight), and the director's reasoning on each card. Plus the film strip of every camera cut in the open sequence. Each card shows a thumbnail rendered through that shot's camera at the first frame of its cut, the lens and aperture, and the duration with pacing warnings for flash cuts and overlong shots. The shot under the playhead lights up live, and clicking a card jumps there and selects its camera. | Tools → Cinemafy → Shot Browser |
| **Camera Move** | Push in, pull out, truck, pedestal, crane (it tilts to hold the subject) and orbit (around the focus point). Key `Progress` 0 → 1 in Sequencer; the easing is built in, including an "operator settle" with a tiny overshoot. Or call `Play()` from gameplay. | `Cinemafy Camera Move` component / Tools → Cinemafy → Camera Move |
| **Continuity Check** | Reviews every cut in the open sequence, hand-cut or directed: **line crossings** between the characters in consecutive shots, **jump cuts** (same person, same framing, under 30° apart), **flash cuts**, **overlong shots** and **cutting a camera to itself**. Shows a report with shot numbers and times. | Tools → Cinemafy → Check Sequence Continuity / Shot Browser |
| **180° Line Check** | Select two characters and your cameras. Cameras across the line from the rest get flagged and selected. | Tools → Cinemafy → Check 180-Degree Line |
| **Add Cameras to Sequence** | Adds the selected cameras to the open Level Sequence and cuts to them at the playhead. | Tools → Cinemafy → Add Cameras to Sequence |
| **Lens Kit** | Real primes (18/24/35/50/85/135) with 11-blade round bokeh. No more random zoom values. | Tools → Cinemafy → Lens Kit |
| **Camera Feel** | Tripod drift, shoulder rig, handheld or run-and-gun. Organic layered noise plus operator breathing. Seeded and repeatable. `Intensity` is **keyable in Sequencer**. | `Cinemafy Camera Feel` component |
| **Eye Focus** | Focuses on MetaHuman **eye bones** using true focal-plane distance. Rack focus between two characters by keying `RackAlpha`, or call `RackFocusTo` from gameplay. Pulls blend in diopters, like a real follow-focus. | `Cinemafy Eye Focus` component |
| **Gaze** | Living eyes: micro-saccades, a lazy head that carries part of the turn, natural blinks, double blinks and blink-on-gaze-shift. | `Cinemafy Gaze` component |
| **Light Rig** | A gaffer in a box: key, fill, rim and eye light that place themselves around a character **relative to the shot's camera**. Styles: Three-Point, Loop, Rembrandt, Butterfly, Split, Noir and Silhouette. Short- or broad-side keying is picked automatically from where the face is turned. Intensities are in **lux at the face**, so moving lights never changes exposure. **Character Only** mode uses lighting channel 1, so cutscene cheats never touch the environment. In game it can re-light for every camera cut. `Intensity` is keyable. | Tools → Cinemafy → Light Rig / `Cinemafy Light Rig` actor |
| **Gameplay ↔ Cutscene** | `UCinemafyCutsceneSubsystem::PlayCutscene` blends from the gameplay camera into the cutscene and back. Animated letterbox bars follow the blend, the cutscene can re-anchor to the player (no teleport pop), and the gameplay camera lands facing where the cutscene ended. Skips still blend out. | Tools → Cinemafy → Set Up Gameplay Blend + `Cinemafy Cutscene` subsystem |
| **Listener** | Characters who actually listen. It reads the speaker's MetaHuman jaw curve to find phrase endings, emphasis and long stretches of talk. Listeners nod in the pauses, blink and raise brows on stressed words, tilt or glance away while processing, and idle quietly in silence. Six temperaments: Attentive, Warm, Stoic, Skeptical, Nervous, Hostile. `Engagement` is keyable, and `React()` places a reaction on an exact line. Goes quiet while its own character talks. | Tools → Cinemafy → Add Listening / `Cinemafy Listener` component |
| **Cinemafy Face** | Facial performance control for MetaHumans, via one anim node in the face Anim BP. Per-region gain to boost or calm a performance, adaptive jitter cleanup (One Euro filter) for mocap and LiveLink, spring "flesh" dynamics, 10 crossfading emotion poses, breathing, idle face life and blink-driven pupil constriction. It also applies Gaze and Listener to the face automatically. | `Cinemafy Face` anim node + component / Tools → Cinemafy → Face Emotion |
| **Looks** | Clean Cinema, Warm Epic, Teal & Orange, Bleach Bypass, Noir, Cold Steel. Applied on top of your existing post settings, with a strength slider. | Tools → Cinemafy → Look & Letterbox |
| **Letterbox** | 2.39 scope or 1.85 flat. Real in-game bars through the camera's aspect constraint. | Tools → Cinemafy → Look & Letterbox |
| **Look Presets** | `CinemafyLookPreset` data asset: save a grade and letterbox as a reusable "film stock". | Content Browser → Miscellaneous → Data Asset |

Everything is also exposed to **Blueprint**, **Editor Utility Widgets** and **Python**:
`UCinemafyCameraLibrary`, `UCinemafyFramingLibrary` (runtime) and `UCinemafyEditorLibrary` (editor).

---

## A 2-minute dialogue scene

1. Drop your two MetaHumans in the level, facing each other.
2. Select **Kara**, then shift-select **Voss**.
3. *Tools → Cinemafy → Create Shot → Medium Two-Shot*, then *Over-the-Shoulder* and *Close-Up*.
4. Select **Voss**, then **Kara**, and make the same singles again. You now have matched reverse coverage, all on one side of the line.
5. Select all the cameras: *Camera Feel → Shoulder Rig*, then *Look → Warm Epic*, then *Letterbox → 2.39*.
6. Select both characters: *Add Gaze to Characters*. Set each one's `LookAtTarget` to the other.
7. Before step 3, open your Level Sequence in Sequencer and move the playhead before each shot: every **Create Shot** drops a camera cut right there. 🎬
8. Open *Tools → Cinemafy → Shot Browser* and hit **Capture Thumbnails** to see your edit as a film strip.
9. Select both characters plus all the cameras, then run *Check 180-Degree Line* for peace of mind.
10. Select both characters: *Add Listening → Attentive* (or *Skeptical* for Voss 😏). Each now reacts while the other talks.
11. Select **Kara** plus her close-up camera: *Light Rig → Rembrandt*. Do the same for **Voss** with *Loop*. 💡

> **Tip:** keep `bFlipSide` the same for every shot in a scene. That's what keeps you on one side of the line.

---

## Directing a scene with the Scene Director

1. Select the characters and pick *Tools → Cinemafy → Scene Director → Intimate Drama* (or any style).
   You get a director with a starter script of six alternating lines, building in intensity.
2. Edit the **Lines** to match your real dialogue: speaker, listener, how long it takes to say,
   the pause after it, and how intense it is (0 = small talk, 1 = the big reveal).
3. Open your Level Sequence, put the playhead where the scene starts, select the director and
   click **Direct** in its Details panel. Cameras and cuts appear, and the Shot Browser updates.
4. Not feeling it? Click **Re-roll** for a new take, or switch styles and Direct again.
   Moved a camera by hand? The director reuses its cameras, so your tweak survives.

**How it reads the scene first 🧠:** before choosing a single shot, the director decides what *kind* of
moment each line is, and that picks its tools:

| Moment | Detected from | Tools it reaches for |
|---|---|---|
| Reveal | a jump in intensity of 0.3 or more, or a peak line (0.85+) | a notch tighter, a push-in landing on the stressed word, a held reaction in the pause |
| Silent Beat | 1.5 s or more of pause after the line | one held reaction shot, no busy cutting |
| New Exchange | someone new starts talking | re-establish with a two-shot |
| Action | the Action flag, or audio that's all hits and no talk | cut on impact, crash zoom, speed ramp |
| Walk & Talk | movement in the sequence (or velocity, live) | lead / follow tracking on a follow rig |
| Dialogue | everything else | coverage, shot-reverse-shot, hold-through with rack focus |

After each Direct, **Last Decision Summary** (and the Shot Browser's summary tooltip) explains it in plain
words, e.g. *"Read the scene as: 4 dialogue, 1 reveal, 1 silent beat. Cut it with: 11 shots over 31.5s,
3 reactions, 2 transitions, 1 held two-shot with rack focus, 2 camera moves."* Every shot's reason is
tagged with its moment.

How it decides, per shot:
- **Size follows drama:** wide when calm, tighter as intensity rises, with an extreme close-up only at the peak.
- **Shot-reverse-shot:** cutting between speakers favors matching shot sizes.
- **No jump cuts:** two shots in a row on the same person must change size meaningfully (pushing in is allowed).
- **Reaction shots:** sometimes during a line, more often in the pause after it.
- **Establishing shot:** the scene opens on a wide two-shot (style dependent).
- **Pacing:** each style has calm vs. intense shot-length ranges, a lens bias, an appetite for low angles and calm vs. intense camera feel.

- **Camera moves:** a creep-in on the establishing wide, push-ins on singles as drama builds, a crane-up on low hero shots. They're baked as keyed `Progress` on each camera's Camera Move, and each style has its own appetite for movement (`MoveChance`).
- **Emotion per line:** tick *Set Emotion* on a line and the speaker's Cinemafy Face emotion is keyed in Sequencer at the start of that line. It crossfades, and the same thing happens live.

- **Walk-and-talk:** when a speaker travels during their line (auto-detected from the sequence, or from
  velocity when live), coverage switches to **Lead** (the camera backs up in front of them) and **Follow**
  (behind, over the shoulder) shots. These ride a **Cinemafy Follow Rig** with Steadicam-style lag.
  `TrackingLeadPreference` per style picks which comes first.
- **Action coverage:** tick **Action** on a fight, chase or stunt line. Shots are cut **on the impacts**
  (a few frames early, so each hit lands in the new shot), with **crash zooms** on the lens and **speed
  ramps** on the biggest hit (a native Play Rate track, or global time dilation live). Impacts come from
  `ImpactTimes`, or from the audio's loudest hits if you leave it empty. `CrashZoomChance` and
  `SlowMoChance` per style (Space Opera barely touches slow-mo; Grindhouse crash zooms everything).
- **Hold-through and rack focus:** a two-shot can *hold* across a change of speaker, with focus racking
  to whoever starts talking instead of cutting (very classic-trilogy; `HoldThroughChance` per style).
  Shots with both characters in them always focus on the speaker.
- **Look and light:** each style also brings a grade, a frame shape and a lighting style. Classic
  Hollywood is clean, 1.85:1 and three-point. Intimate Drama is warm, 2.39:1 and Loop. Kinetic Action is
  teal & orange. Slow Burn uses Rembrandt. Noir Thriller uses the Noir grade and hard Noir light. Epic is
  warm and Rembrandt. Direct grades every camera and gives every character a Light Rig (or restyles
  theirs). Turn either off, or change them, in a Custom style.

Every decision is logged with a reason ("Line 4: Close-Up on Voss (tightening for drama, intensity 0.8)")
and kept in the director's **Last Plan**.

**Intensity from the audio 🎙️:** drop each line's recording into its **Audio** slot and click
**Analyze Dialogue Audio** (Direct also does this automatically for new audio). For each line, Cinemafy measures:
- speech loudness (silence excluded)
- how hard the peaks hit
- speech rate
- how much of the clip is talking

It then scores intensity **relative to the rest of the scene**, so recording gain doesn't matter. A shouted
line in a quiet conversation reads as intense, and a calm one in a shouting match reads as calm. Line durations come from
the audio, and the result shows on each line as **Audio Intensity**. The director blends it with your manual
Intensity using **Audio Intensity Weight** (0 = your call, 1 = the delivery decides). With
**Place Dialogue Audio** on, Direct also lays the recordings into a *Cinemafy Dialogue* track, one row
per speaker, so the cut plays with the real delivery.

The analysis also finds each line's **pauses** and **stressed words**. Cuts inside a line slide to the
nearest breath, like an editor would make them, and push-ins are timed to **land on the emphasis**,
starting about a second before the stressed word.

**Performance clips per line 🎭:** give a line a **Body Animation** and/or **Face Animation**
(MetaHuman Animator takes, mocap clips) and Direct lays them onto the speaker at the line's start.
Each clip goes on whichever mesh matches its skeleton (body or face), on a *Cinemafy Performance*
track that re-takes replace without touching your other animation tracks. Script in, full scene out:
cameras, audio and performance.

**Group scenes 👥:** put 3+ characters in the script (or select 3+ for the starter script) and the
director opens on a **Group Shot** framing everyone. When a new pair starts talking it sometimes
re-establishes, and some reaction cuts go to **bystanders**, whose faces often tell the story. Create
Shot also offers Group Shot: select the subject, their partner, then everyone else.

**Transitions 🌀:** each style has an opening, a scene-break and a closing transition: **Cut**,
**Wipe** (the new shot sweeps across while the old one keeps playing), **Dissolve**, **Fade Through
Black** or **Match Cut** (the new scene opens in the same framing as the last shot). Tick **Scene
Break** on a line that starts somewhere (or somewhen) new: the director uses the style's transition
and re-establishes. Any line can also override its transition. Fades are baked as a native Fade track,
so they show everywhere, including renders. Wipes and dissolves render live in PIE and the game (the
outgoing camera keeps rendering during the wipe). Space Opera Classic wipes, and alternates directions
like the serials did.

**Cross-cutting ✂️:** set a director's **Parallel Director** to another director and Direct the first.
Blocks of lines alternate between the two scenes (`CrossCutLinesPerBlock`), shrinking as the scenes
build so the cutting accelerates into the climax. Every switch is a scene break, so the style's
transition applies (Mind-Bender match-cuts, Space Opera wipes). Each scene keeps its own cast.

**Music 🎵:** give the director a **Music** track. Cinemafy finds its tempo and beat grid, lays it into
the sequence (*Cinemafy Music*, with `MusicOffset`), and with **Cut To Music** slides cuts onto the
nearest beat. Cuts that start a spoken line stay put, so dialogue stays in sync. Great for montages and trailers.

**Subtitles 💬:** fill in each line's **Text** (and optionally a **Speaker Name**). Subtitles show
live during `StartConversation` / `DirectLine` (pass the text as `Subtitle`), and Direct keys the
director's `SubtitleLineIndex`, so cut sequences show them in PIE and in the game. Anything else can
call **Cinemafy Subtitle Subsystem → Show Subtitle** too, and `bSubtitlesEnabled` hooks into your
accessibility settings. Subtitles stay up long enough to read (about 17 characters per second, never less
than the line), and long lines split into two-line chunks at sentence, then clause, then word breaks.
Text is resolved in the player's language at runtime, so translations are chunked and timed on their
own length, and CJK text automatically gets shorter chunks and a slower pace.

**Importing a script and takes 📥:**
- **Import Script From CSV:** set the director's *Script File* to a CSV with any of the columns `Speaker, Listener,
  Text, Intensity, Emotion, SceneBreak, Action, Duration` (any order). Speakers are matched to actor labels
  in the level, and missing durations are estimated from the text.
- **Auto-Assign Performances:** set *Performance Folder* to a content folder of takes named by line number
  (`Line03_Kara.wav`, `Line03_Kara_Face`, `Line03_Kara_Body`…). Audio, face and body clips land on the right
  lines; face vs. body is decided by skeleton.

Pair these with MetaHuman Animator (face from video) and a video-mocap tool's FBX export (body), and you
go from recorded performances to a directed scene in a couple of clicks.

**Learn a style from your own edit 🧠:** cut a scene by hand (or open one you love), select the
director and click **Learn Style From Sequence**. It plays through every cut and classifies each shot
from what the camera actually sees: frame size at the subject, two characters in frame, a foreground
shoulder for OTS, camera below the eyes for low angles. From that it measures shot-type preferences,
pacing (median, quick and long shot lengths), lens bias, low angles, movement, camera feel and whether
you open wide. It writes the result into the director's **Custom** style and shows a summary.
Hit **Direct** and new scenes get cut your way.

**Live, in gameplay:** call `DirectLine(Speaker, Listener, Duration, Intensity)` from your dialogue
system as each line starts, then `EndConversation()` when it's over. The director blends in from the
gameplay camera, cuts between live-solved cameras (so it follows characters who move), and points
everyone's Listener at whoever's talking. `StartConversation()` plays the director's own script live.

> Inspired by [Cine-AI](https://github.com/inanevin/Cine-AI) (Evin, Hämäläinen & Guckelsberger,
> CHI PLAY 2022): director styles as weighted shot vocabularies, steered by scene intensity, with
> each shot chosen in light of the last. Cinemafy's implementation is its own.

---

## Choreograph a performance (click, type, record)

Make stock motion-matching animation *act*. **Tools > Cinemafy > Choreograph Character** (or **Choreograph**
in the Hub) with your character selected, then:

1. **Click** where something happens in the viewport. A box pops up.
2. **Type** a word or two: `walk`, `run`, `sprint`, `sneak`, `vault`, `climb`, `jump`, `sword attack`,
   `combo`, `shoot`, `taunt`, `wave`, `look`, `wait 2`, `pistols`, `holster`... Click an enemy before
   `sword attack` and the attack is aimed at them. Repeat. **Esc** when done. **Ctrl+Z** undoes beats.
3. **Perform & Record** (banner button, or on the Choreography actor). Cinemafy plays in editor and the
   character performs it *for real*:
   - **Locomotion** is your motion matching: Cinemafy steers the character and sets its gait
     (`SetWantsToSprint` / `SetWantsToWalk`, GASP's `CharacterInputState`, or a speed fallback), so starts,
     stops, pivots and turns come from the database.
   - **Vaults and climbs** call the character's own traversal (GASP's `TryTraversalAction`, fed by
     `GetTraversalCheckInputs`), so the chooser, motion warping and the blend back out are GASP's own.
   - **Attacks, taunts, gestures and reactions** are picked from your animation library by name and
     folder (`AM_Katana_Attack_Combo_01` answers to *sword attack*), filtered to animations your
     skeleton can play. `combo` chains a numbered family in order. Melee closes the distance first,
     faces the target and sets motion-warp targets (`AttackTarget` / `Target`).
   - **Weapons** switch the overlay: `sword`, `pistols`, `rifle`, `wand` match GASPALS's overlay enum
     by display name (*pistols* → `PistolTwoHanded`).
   - Beats flow: moves round corners, the next action starts as a montage blends out.
4. Play stops by itself. The take is **baked into the open Level Sequence** (or a new one in
   `/Game/Cinemafy/Takes`): a transform track plus a recorded animation per body mesh.
5. **Direct Shots** (automatic after the bake by default) hands the take to a Scene Director: vaults
   and hits become action lines cut on impact, sprints become tracking shots, the enemy becomes the
   reverse for over-the-shoulders. Change **Director Style** on the choreography and re-direct as
   often as you like.

**Fights:** click an enemy before `sword attack` and the enemy **reacts on the hit**. Its reaction is
timed so its impact lands on the strike's contact frame, from the side the blow comes from. Use `kill` for
a death, `heavy attack` for a knockdown, `attack blocked` for a block, `attack dodged` for a dodge, and
`combo` for a hit on every strike. Enemies with no choreography of their own are recorded with the take.

Several characters? Give each a choreography. Perform & Record takes everyone (with **Include In
Takes** on) in one take, and the director cuts the whole fight.

After a take, each beat reports its **Outcome**: Succeeded, Fallback (done another way, with the reason),
Blocked, Failed or Timed Out. Problems turn the beat's label red or amber in the viewport. Movement
beats have an **Arrival** policy: pass through, stop, or enter the next action moving. Auto picks one
from what follows. See the [design contract](Docs/CHOREOGRAPHY.md).

**Dialogue in the same scene:** put the line in quotes. `walk "Stand down, Admiral"` says it while walking
to the mark. Click Thane and type `answer "Make me."`: his beat waits for her line, then he turns to her and
says it. Back on Kade, `sword after` draws once he's done, then `sword attack` on Thane. Give a beat
**Dialogue Audio** and it plays during the take and lands in the cut. The director cuts it all as one scene:
walk-and-talk, the exchange, then the fight.

**Before you record:** vault and climb beats measure their obstacle. A label like *"140 cm high, 40 cm deep:
no room to land on the far side"* turns red, and the measured shape is drawn in the viewport. Perform &
Record lists anything that can't work and asks first.

Each beat shows how it was read (**Reading**) and what it picked (**Chosen Animations**). Override any
beat with **Animation Override**. For full control, **Build Move Library** gathers the project's
one-shots into an asset you can curate (add keywords, bias favorites), and set it as the
choreography's **Move Library**. The **Wiring Report** says what Cinemafy found on the character. If it
guessed wrong, set the function names under *Wiring*.

**Performer profiles** write that down once per kind of character. **Save Wiring As Profile** turns what
was found into a `PP_` asset, or make one from a preset (GASP, GASPALS, SignalForge, MetaHuman on GASP,
generic). It holds the gait / crouch / traversal / overlay hooks, what the character can climb (vault
height and depth, mantle height, landing room), its move library, and which meshes not to record. Map
classes to profiles in **Project Settings > Plugins > Cinemafy**. **Check Wiring** names anything the
character is missing.

---

## Cinemafy Edit: a video editor inside Sequencer

**Tools > Cinemafy > Cinemafy Edit** (or the **Cinemafy Edit** dropdown on Sequencer's toolbar). Everything
works at the playhead or the cut nearest it, and everything is undoable.

| Tool | What it does | Shortcut (in Sequencer) |
|---|---|---|
| Razor / Razor All Tracks | Split the shot (or every section) at the playhead | Ctrl+K / Ctrl+Shift+K |
| Ripple Delete Shot | Remove the shot and close the gap on **every** track (sync-locked) | Shift+Delete |
| Ripple Trim Start / End | Premiere's Q / W | Alt+Q / Alt+W |
| Roll | Move a cut, picture only (J-cuts and L-cuts) | Alt+, / Alt+. |
| Swap With Next | Shots trade places | |
| Prev / Next Cut | Jump between edit points | Alt+Up / Alt+Down |
| Transitions | Wipes (straight, **iris**, **clock**, alternating like the serials), dissolve, **dip to white**, **whip pan**, fade through black | Ctrl+Alt+D (wipe) |
| Titles | Lower thirds, typed-on location cards, chapter cards, centered titles, and an **opening crawl** (intro line, falling title, receding crawl) | |
| Duck Music | Dips the music under every line with smooth ramps | |
| Markers | Add markers, drop **beat markers** from the music, **snap cuts to markers** | |
| Grade | Copy a camera's grade, paste to selected, or apply to every shot (focus stays per shot) | |
| **Multicam** | A live grid of every camera. Arm it, press play, tap 1-9 to cut; stop and your switching becomes the edit | |

Ripples move keys inside long sections too (the director's cue tracks, music volume), plus markers
and the playback range, so nothing drifts out of sync. Shortcuts are rebindable under *Editor
Preferences > Keyboard Shortcuts > Cinemafy Edit*.

Fades through black are native Fade keys, so they show in the editor, in play and in Movie Render
Queue. Wipes, dissolves, dips, whips and titles draw over the game view, so they show in PIE and in
game. A **Cinemafy Edit Decisions** actor holds them, keyed in the sequence.

---

## Gameplay ↔ cutscene handoff

1. Open the cutscene's Level Sequence and click *Tools → Cinemafy → Set Up Gameplay Blend*. This:
   - turns on camera cut blending,
   - eases the first cut in from the gameplay camera and the last cut back out to it (1s each), and
   - eases each character's first and last animation section so their pose blends with the Anim Blueprint.
     For that pose blend, the character binding needs to use the Anim Blueprint with a slot.
2. Play it from Blueprint: **Get Cinemafy Cutscene Subsystem → Play Cutscene**, or from C++:
   ```cpp
   FCinemafyCutsceneSettings Settings;
   Settings.Anchor = ECinemafyCutsceneAnchor::Player; // optional: no teleport pop
   GetWorld()->GetSubsystem<UCinemafyCutsceneSubsystem>()->PlayCutscene(MySequence, Settings);
   ```
3. Bind **On Cutscene Started / Finished** to hide and restore your UMG HUD. Wire your skip button to **Skip Cutscene**.

What you get out of the box:
- **Animated letterbox bars** that ease in and out in time with the camera blend.
- **Relative to Player:** the whole cutscene plays around wherever the player is standing.
  Author it around the world origin, with the player's start mark at `(0,0,0)` facing +X.
  This is best for cutscenes in open space; scenes tied to level geometry should stay *World*.
- **Seamless return:** as the blend back to gameplay starts, the player's control rotation turns to match
  the cutscene's last camera, so the gameplay camera picks up facing the same way.
- **Skip** jumps to the start of the blend-out, so even skipped cutscenes hand back smoothly.

> Tip: use the animated bars *or* camera letterboxing for a cutscene, not both. Camera
> letterboxing pops on at the first frame.

---

## MetaHuman face setup (one node)

1. Open the MetaHuman's **Face** Anim Blueprint and add the **Cinemafy Face** node (right-click → *Cinemafy*)
   right before **Output Pose**, so it runs before RigLogic.
2. Add a **Cinemafy Face** component to the character, or use *Tools → Cinemafy → Face Emotion*.

That one node then handles the whole face:

| Layer | What it does |
|---|---|
| **Performance control** | `PerformanceIntensity` (keyable) boosts or calms everything. A **Face Profile** asset sets gain and offset per region (brows, eyes, cheeks, nose, mouth, jaw, tongue) and per curve. |
| **Jitter cleanup** | A One Euro filter per curve. It smooths heavily when a curve is slow (where mocap jitter shows) and lightly when it moves fast, so blinks and plosives stay crisp. Set `JitterReduction` per region. |
| **Flesh dynamics** | A damped spring per curve adds a hint of overshoot and settle (`Springiness` per region). Eyes default to 0 so blinks stay snappy. |
| **Lip sync** | Reads mouth shapes from the raw performance (before smoothing rounds them off), then:<br>• **P/B/M:** seals the lips fully, with a minimum hold so closures never fall between frames<br>• **F/V:** firms up the lower-lip tuck<br>• **OO/EE:** boosts rounding and stretch<br>• **Jaw:** lifts small openings so quiet speech reads<br>• **Silence:** gates small mouth twitches when no one's talking<br>`Articulation` (keyable) scales it all; fine controls live under `LipSync`. |
| **Emotions** | Neutral, Happy, Sad, Angry, Fear, Surprise, Disgust, Contempt, Determined and Worried. They crossfade over `EmotionBlendTime`, and `EmotionIntensity` is keyable. Call `SetEmotion` from a Sequencer event to change one on a line. Override any pose in the profile's `CustomEmotions`. |
| **Life** | Slow, asymmetric micro-movement (`FaceLife`, keyable), nostril and jaw breathing, and pupils that briefly constrict after every blink, whether performed or procedural. |
| **Gaze** | Eye-look curves from the Gaze component (blended over the performer's eyes by `GazeEyeLookBlend`), plus procedural blinks. |
| **Listener** | Brow raise and furrow, smile and squint go onto the right MetaHuman curves automatically. |

The node's `Alpha` blends between the raw performance and the processed one. That's handy for A/B checks.

**Body Anim BP:** this part is still manual.
- Gaze: aim the neck and head at `HeadLookAtLocation` with a *Look At* node or a Control Rig aim (weight around 0.5–0.8).
- Listener: apply `HeadRotation` with *Transform (Modify) Bone* on `head` (*Add to Existing*). Use `BreathAlpha` and `WeightShift` for idle layers, and `OnReaction` for body montages.

**Lip-sync tips:** key `Articulation` up for shouted or important lines and down for mumbles.
If closures seal when they shouldn't (some performers rest with lips together), lower `ClosureAssist`
or raise `SilenceGate`. The mouth curves it reads are listed in the Face Profile under *Lip Sync*,
so you can fix them if your rig names them differently. The node's debug line (Anim BP debug /
`ShowDebug Animation`) shows live Seal, Tuck and Gate values.

**Not using MetaHumans?** Point the profile's `CurvePrefix` at your rig's curves. Gaze, Listener and Face
also expose all their values to Blueprint, so you can wire them by hand.

**How Listener hears:** by default it watches `CTRL_expressions_jawOpen` on the speaker. That curve
follows MetaHuman Animator and audio-driven performances, so no markup is needed. Other rigs can add
their own curves to `SpeechCurves`, or key `ManualSpeechEnergy`. Tune `SpeechThreshold` while watching `HeardEnergy`.

**Directing listeners:** key `Engagement`, call `React` from a Sequencer event track, and call
`SetSpeaker` to switch who they're listening to.

---

## Notes and gotchas

- **Camera Feel** and **Camera Move** offset the *camera component*, not the actor, so they stack
  with Sequencer transform keys, rails and cranes (and with each other: the shake rides on the move).
  If you key the camera *component's* transform directly, they'll fight.
- **Eye Focus** owns the camera's manual focus distance while it's active. To hand-key focus
  instead, disable the component.
- For perfectly repeatable shake in renders, enable `bUseTimeOverride` on Camera Feel and key
  `TimeOverride` linearly across the shot.
- **Light Rig** intensities assume a fairly neutral exposure. If faces come out too hot or dark,
  adjust `KeyIlluminance` once per scene (or lock your camera's exposure), and every style
  stays balanced relative to it.
- **Character Only** lighting turns on lighting channel 1 for the subject's meshes (body, face
  and groom). Their normal channel 0 world lighting is left as it was.
- With **Drive Gaze** on, the Listener owns its Gaze's `LookAtOffset` (for thinking glances).
  Use `SetLookAtTarget` to aim the gaze and leave the offset to the Listener.
- **Smart placement:** Create Shot and the Director check that each camera isn't inside geometry and has a
  clear view of its subject. If not, they try nearby alternatives on the same side of the line (swing
  the angle, step closer on a wider lens, raise or lower). The Output Log warns if nothing works.
- Bone names default to MetaHuman (`FACIAL_L_Eye`, `FACIAL_R_Eye`, `head`) and fall back
  gracefully for other characters.

---

## Layout

```
Cinemafy.uplugin
Source/
  Cinemafy/            Runtime: components, anim node, lens/look/framing libraries
  CinemafyEditor/      Editor: Tools menu, Hub, Shot Browser, undoable actions
  CinemafyAnimGraph/   Anim Blueprint node (UncookedOnly)
Docs/                  Getting started, architecture, roadmap, build notes, plan
Docs/ROADMAP.md
```
