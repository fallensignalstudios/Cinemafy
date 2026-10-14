# Architecture

## Modules
| Module | Type | Contents |
|---|---|---|
| `Cinemafy` | Runtime | Components, actors, subsystems, the face anim node, and the framing / lens / director libraries. Ships with the game. |
| `CinemafyEditor` | Editor | Tools menu, Hub, Shot Browser, undoable editor actions, everything that touches Sequencer. |
| `CinemafyAnimGraph` | UncookedOnly | The *Cinemafy Face* Anim Blueprint node (editor graph node for `FAnimNode_CinemafyFace`). |

## How the pieces talk
- **Pure logic, no world:** `UCinemafyFramingLibrary` (shot solving, eye points, line of action),
  `UCinemafyDirectorLibrary` (planning) and `UCinemafyCameraLibrary` (lenses, looks). They're deterministic
  and covered by automation tests.
- **Components compute, Anim Blueprints apply:** Gaze and Listener produce values. `FAnimNode_CinemafyFace`
  snapshots them on the game thread (`PreUpdate`) and writes face curves on the worker thread.
- **Camera offset stack:** `UCinemafyCameraOffsetComponent` subclasses (Move, Feel) layer local offsets
  on the camera component in `StackOrder`, so they never fight Sequencer's actor transforms.
- **Runtime ↔ editor bridge:** the Scene Director lives in the runtime module, but its Details-panel
  buttons broadcast `ACinemafySceneDirector::OnEditorRequest`, which the editor module answers (Sequencer
  is editor-only).
- **Sequencer access** goes through `CinemafySequencer` helpers (`ISequencer` from the open Level Sequence
  editor): playhead, bindings via `GetHandleToObject`, refresh.

## Director pipeline
```
Lines (+ audio analysis) ──► GetEffectiveLines ──► PlanScene ──► FCinemafyPlannedShot[]
                                                              │
                     ┌────────────────────────────────────────┼─────────────────────────┐
                     ▼                                        ▼                         ▼
            Editor bake (Direct)                     Live (DirectLine)          Shot Browser
  cameras · cuts · Progress / Emotion /     cameras solved per cut · moves ·    reasons · pacing
  SubtitleLineIndex keys · audio · anims    emotions · subtitles · listeners
```

## Conventions
- Characters' eyes come from MetaHuman bones (`FACIAL_L/R_Eye`, `head`), with graceful fallbacks.
- Face curves are `CTRL_expressions_*`. Names that might differ by rig are data (Face Profile), not code.
- Directed cameras carry `Cinemafy.Director.<name>` and `Cinemafy.Shot.<key>` tags, so re-takes reuse them.
