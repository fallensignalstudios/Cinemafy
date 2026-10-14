# Choreography: design contract

Adapted from Vesryn's technical review of the v0.8 Choreographer. The guiding principle:

> Direct the intent, execute through validated character capabilities, record what actually
> happened, and cut around that performance.

## Who owns what

| Responsibility | Owner in Cinemafy | Boundary |
|---|---|---|
| Meaning, order, targets, arrival policy | `ACinemafyChoreography` beats + `UCinemafyChoreographyLibrary::ReadPrompt` | Structured beats, never bone motion |
| Routes | Navmesh path (moves), straight line (traversal) | Where, not how it looks |
| Capsule movement | The performer's CharacterMovement, driven only by movement input and gait intents | Cinemafy never sets location while moving; it only sets rotation when stopped and facing |
| Locomotion pose | The performer's Motion Matching | Gets real input/velocity, so its own trajectory prediction stays authoritative |
| Discrete actions | Montages / slot animations picked by name, or the performer's own traversal | Committed once started; nothing re-picks mid-swing |
| Traversal | The performer's traversal (`TryTraversalAction`) | Cinemafy never runs a competing solver; a stand-in is a labeled **Fallback** |
| Weapon presentation | The performer's overlay state | An overlay switch is not a draw animation |
| Recording | `FAnimationRecorderManager` + actor transform samples, one clock | Records the evaluated result |
| Coverage | The Scene Director | Reads the take's events; rerolling shots never reruns the performance |

## Beats: authored, resolved, observed

- **Authored:** `Prompt`, `Location` (relative to the choreography actor), `Target`, `Arrival`,
  `AnimationOverride`, `PlayRate`, and a stable `Id`.
- **Resolved:** `Reading` (how the prompt was read) and `ChosenAnimations`.
- **Observed** (after a take):
  - `Outcome` and `OutcomeReason`.
  - `Events`: BeatStart, Arrived, ActionStart, Takeoff, Landing, Contact and ActionEnd, each timed
    and located, and flagged when estimated.
  - `ImpactTimes`: the cut points derived from those events.

### Outcomes

**Succeeded**, **Fallback** (done, but not as asked, with the reason), **Blocked**, **Failed**,
**TimedOut**, **NotRun**. Problems show as red and amber labels on the beat in the viewport and in a
toast after the bake. Nothing is reported as a success just because the timeline moved on:
- A jump standing in for a vault is a Fallback.
- A move stopped by geometry is Blocked.
- An attack with no fitting animation is Failed.

### Arrival

`Auto` resolves to:
- **Pass Through** before more travel or a melee charge: no idle at the mark.
- **Enter Action** for vaults: arrive moving.
- **Stop** before gestures, looks, waits and ranged attacks: the stop plays out until speed settles
  before the next beat starts.

### Contacts

A strike's contact time is:
- **Authored** when the clip has a `hit`, `impact` or `contact` notify.
- **Estimated** inside a damage/trace window, or at 40% of the clip when there is no notify. It is
  flagged as estimated either way.

It is only published if the montage is **still playing** when that time arrives. An interrupted
swing doesn't hit.

## Bake-space contract

- **Actor (capsule) transform** goes on the performer's transform track, sampled at the open
  sequence's display rate in whole frames.
- **Body animation** is recorded relative to its mesh component, **root bone included**
  (`bRemoveRootAnimation = false`). This keeps pose-relative root offsets (GASP's Offset Root Bone
  in turns and pivots).
- **No double counting:** a CharacterMovement character's travel lives only on the capsule, so it is
  never counted twice.
- **Reconstruction rule:** actor transform × mesh-relative transform × recorded bones = the world
  skeleton seen in play.
- **Track ownership:** the take only replaces tracks named *Cinemafy Take*. Hand-made transform
  tracks are **muted, never deleted**.
- **Recorded components:** body meshes are recorded. Follower (leader-pose) meshes are skipped.
  `SkipRecordingComponents` (default: `Face`) lists the rest.
- **Restore after the take:** possession, gait flags, crouch, max walk speed and warp targets are put
  back on finish, stop, or end of play.

## Not done yet (roadmap)

These are real and worth doing. They're bigger than a patch:

- **Performer adapter interface:** an explicit capability profile per character family (GASPALS,
  Narrative, custom) instead of reflection discovery, with validated bindings.
- **Geometry-aware traversal queries before committing:** obstacle front/top/far edge, landing
  clearance, supported height envelopes, typed refusal reasons (`UnsupportedHeight`,
  `LandingBlocked`...).
- **Planned trajectories:** time-parameterized routes that plan vault entry speed and turn distance,
  fed through the performer's own trajectory inputs.
- **Validated move definitions:** promote name-matched candidates into curated entries with contact
  markers, entry windows, body masks and root-motion policy (Move Library is the start of this).
- **Synchronized interactions:** attacker/defender solved together on a shared contact marker,
  paired content, reserved space; evaluate Contextual Animation behind an adapter.
- **Hand occupancy and stow/draw performances;** action-aware gaze and IK reduction during contact
  windows.
- **Fixed-time contact constraints** ("the hit lands at 4.2s"), resolved explicitly rather than by
  speeding clips up.
- **Bake validation:** compare reconstructed world-space root/hands/feet against the capture and
  report drift; MRQ subframe tests.
- **Versioned takes** kept side by side, rather than replaced.
- **Preview isolation:** suppress gameplay side effects (damage, quests, saves) during Perform &
  Record without suppressing animation notifies.
- **Composition checks:** foreground shoulder occupancy in over-the-shoulders, subject cropping
  across a move, and protecting both sides of an interaction in action coverage.

## Licensing note

Cinemafy doesn't ship any third-party animation. GASPALS's content is **UE-only** licensed, so keep it
in the project, not in this plugin.
