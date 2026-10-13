# Cinemafy 🎬

**Premium, film-grade real-time cutscenes for Unreal Engine 5.8, with less fiddling.**

Cinemafy packages the rules real DPs, focus pullers and colorists follow into one-click tools:
cinema lens kits, coverage shots that frame themselves, human camera feel, eye-tracked focus,
living MetaHuman eyes and grading looks. It was built for *Sovereign Call*'s in-engine cutscenes.

> Status: **v0.1 scaffold (beta)**. The core systems are in. See the [roadmap](Docs/ROADMAP.md) for what's next.

---

## Install

1. Clone into your project's `Plugins/` folder:
   ```
   <YourProject>/Plugins/Cinemafy/Cinemafy.uplugin
   ```
2. Regenerate project files and build (or just open the `.uproject` and let it compile).
3. Enable **Cinemafy** under *Edit → Plugins* if it isn't already enabled.

You'll get a **Tools → Cinemafy** menu in the level editor.

---

## What's in the box

| Tool | What it does | Where |
|---|---|---|
| **Create Shot** | Select the subject, then who they're talking to, and pick a shot. You get a CineCamera that's framed (eyes on the upper third, lead room), lensed with the right prime, focused on the eyes, and **always on the same side of the 180° line**. If a Level Sequence is open, the camera is **added to it with a camera cut at the playhead**. | Tools → Cinemafy → Create Shot |
| **Camera Move** | Push in, pull out, truck, pedestal, crane (it tilts to hold the subject) and orbit (around the focus point). Key `Progress` 0 → 1 in Sequencer; the easing is built in, including an "operator settle" with a tiny overshoot. Or call `Play()` from gameplay. | `Cinemafy Camera Move` component / Tools → Cinemafy → Camera Move |
| **180° Line Check** | Select two characters and your cameras. Cameras across the line from the rest get flagged and selected. | Tools → Cinemafy → Check 180-Degree Line |
| **Add Cameras to Sequence** | Adds the selected cameras to the open Level Sequence and cuts to them at the playhead. | Tools → Cinemafy → Add Cameras to Sequence |
| **Lens Kit** | Real primes (18/24/35/50/85/135) with 11-blade round bokeh. No more random zoom values. | Tools → Cinemafy → Lens Kit |
| **Camera Feel** | Tripod drift, shoulder rig, handheld or run-and-gun. Organic layered noise plus operator breathing. Seeded and repeatable. `Intensity` is **keyable in Sequencer**. | `Cinemafy Camera Feel` component |
| **Eye Focus** | Focuses on MetaHuman **eye bones** using true focal-plane distance. Rack focus between two characters by keying `RackAlpha`, or call `RackFocusTo` from gameplay. Pulls blend in diopters, like a real follow-focus. | `Cinemafy Eye Focus` component |
| **Gaze** | Living eyes: micro-saccades, a lazy head that carries part of the turn, natural blinks, double blinks and blink-on-gaze-shift. | `Cinemafy Gaze` component |
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
8. Select both characters plus all the cameras, then run *Check 180-Degree Line* for peace of mind.

> **Tip:** keep `bFlipSide` the same for every shot in a scene. That's what keeps you on one side of the line.

---

## Hooking Gaze into a MetaHuman

The Gaze component **computes**, and your Anim Blueprint **applies**. That way it plays nicely with
whatever face and body animation you already have.

In the **Face** Anim BP (or a post-process ABP on the face mesh):

1. `Event Blueprint Update Animation` → `Try Get Pawn Owner` → `Get Component by Class (Cinemafy Gaze)`.
2. Cache `EyeLookNormalized`, `HeadLookAtLocation` and `BlinkAlpha` into variables.
3. Drive the eyes:
   - `EyeLookNormalized.X` → left/right eye look controls (`CTRL_L_eye` / `CTRL_R_eye` X)
   - `EyeLookNormalized.Y` → up/down eye look controls (Y)
   - `BlinkAlpha` → blink controls (`CTRL_L_eye_blink` / `CTRL_R_eye_blink`)

   You can do this with a Control Rig node or with *Modify Curve*, depending on your face setup.
4. In the **Body** Anim BP, aim the neck and head at `HeadLookAtLocation` (a *Look At* node, or a
   Control Rig aim), with a blend weight around 0.5–0.8.

To switch who a character is looking at mid-cutscene, add a Sequencer **Event Track** that calls
`SetLookAtTarget`. Key `GazeWeight` to fade the system in and out around hand-animated beats.

---

## Notes and gotchas

- **Camera Feel** and **Camera Move** offset the *camera component*, not the actor, so they stack
  with Sequencer transform keys, rails and cranes (and with each other: the shake rides on the move).
  If you key the camera *component's* transform directly, they'll fight.
- **Eye Focus** owns the camera's manual focus distance while it's active. To hand-key focus
  instead, disable the component.
- For perfectly repeatable shake in renders, enable `bUseTimeOverride` on Camera Feel and key
  `TimeOverride` linearly across the shot.
- Bone names default to MetaHuman (`FACIAL_L_Eye`, `FACIAL_R_Eye`, `head`) and fall back
  gracefully for other characters.

---

## Layout

```
Cinemafy.uplugin
Source/
  Cinemafy/            Runtime: components, lens/look/framing libraries
  CinemafyEditor/      Editor: Tools menu, undoable actions
Docs/ROADMAP.md
```
