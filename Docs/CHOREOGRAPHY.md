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

## Interactions: attacker and defender

An attack beat on a character makes that character **answer the strike, synchronized on the contact**:

- **Timing:** for every strike, Cinemafy knows when it lands (authored contact notify, or a labeled estimate).
  The defender's reaction starts early enough that **its own impact frame lands on that moment**:
  - Its own hit notify sets the lead when there is one.
  - Otherwise: about 0.06s for hits, knockdowns and deaths, 0.25s for blocks (the guard comes up first),
    0.3s for dodges.
- **Choice:**
  - The prompt decides the reaction: hit by default, `kill` / `finish` for death, `heavy` / `knock down`
    for a knockdown, `blocked` / `parried` for a block, `dodged` / `missed` for a dodge, `feint` for none.
  - Earlier strikes in a combo get hit reacts, and the last one gets the big reaction.
  - Clips are picked by name from the defender's own skeleton, preferring the side the blow comes from
    (front, back, left or right). A missing knockdown or death falls back to a hit react (logged). A block or
    dodge never quietly becomes a hit.
- **Alignment:**
  - A defender facing roughly toward the blow (within `DefenderTurnLimit`) turns into it during the wind-up.
    Otherwise it takes the hit from the side or back.
  - The defender stops moving for the exchange.
  - The attacker's warp target is the stand-off spot in front of the defender.
- **Honesty:**
  - A swing cut off before contact causes no reaction.
  - A defender busy with its own action isn't overridden. That's recorded as an event, and a missing
    reaction makes the beat a Fallback.
  - Deaths and knockdowns hold their final pose and don't react again.
- **Recording:** defenders without their own choreography are recorded with the attacker's take and baked
  onto their own bindings. A defender with its own choreography performs it, and reacts only when it isn't
  busy.
- **Per-beat controls:** `bTargetReacts`, `Reaction` (override) and `ReactionOverride` (a specific clip).

### Contextual Animation (later, as an adapter)

Epic's **Contextual Animation** plugin is designed for exactly this: clips authored as a pair, roles, and
alignment sections with motion warping. Cinemafy doesn't depend on it yet because:
- It's marked experimental, and its API has moved between engine versions.
- It needs **paired** content, while most combat packs ship independent attacks and hit reacts.

The interaction model above (roles, a shared contact moment, an alignment target) maps one-to-one onto a
Contextual Animation scene. When paired scene assets are present, an adapter can play the scene for
both roles instead of picking two independent clips, keeping the same events, outcomes and recording.

## Performers: profiles

A `UCinemafyPerformerProfile` is the explicit adapter for one kind of character. Resolution order for each
hook: the choreography's own *Wiring* override, then the profile, then name-based discovery (only when
there is no profile or it has *Auto Detect Missing* on). A hook the profile names but the character
lacks is reported as `MISSING ...` in the wiring report, never silently replaced. Profiles also carry the
**traversal envelope** (`MinHeight`, `MaxVaultHeight`, `MaxVaultDepth`, `MaxMantleHeight`, `MinLandingDepth`,
`MinHeadroom`, `MaxApproachAngle`, or `bCanTraverse = false` with `MaxJumpHeight`). The profile is chosen
from the choreography's *Profile*, else the class map in Project Settings (walking up the class hierarchy).

## Traversal: measured before committing

`UCinemafyChoreographyLibrary::CheckTraversal` traces the obstacle between the previous mark and the
beat's mark: floor, front face (and approach angle), top, depth across the top, headroom above it, and a
capsule overlap at the landing spot. `JudgeTraversal` turns the numbers into a typed verdict
(`StepOver`, `Vault`, `Mantle`, `Jump`, `TooTall`, `NoRoomToLand`, `NoHeadroom`, `TooNarrow`, `BadAngle`,
`NoObstacle`, `CannotTraverse`) and a plain-words reason. It is measured when the beat is read, again
before the take, and again from the performer's real position when the beat starts. An impossible beat
finishes `Failed` with the reason. It is never replaced by a jump. `NoObstacle` keeps the old behavior: the
character's own traversal decides.

## Dialogue beats

A beat's line comes from its **Dialogue** (or the prompt's quoted words) and optional **Dialogue Audio**.

- **Timing:** the line starts with the beat, or, when the beat waits for a cue (`answer` / `after` /
  **Wait For Line From**), when the other character's line ends. The cue registry is shared by every
  choreography in the take.
- **Duration:** the audio's length, else the words at **Words Per Second**.
- **Say beats:** they hold until their line ends. Other beats don't, so a line can run under a walk.
- **Logging:** the take records `LineStartTime` / `LineDuration` and `LineStart` / `LineEnd` events.

`BuildDirectorLines` makes each spoken line its own director line, starting on its first word. Beats that
start under someone's line ride along with it, except strikes, which still cut. `MergeLines` never drops a
spoken line in favor of an unspoken one.

## Fight coverage

In the planner, a shot that contains an impact on a line with a defender (the line's Listener) must frame
both fighters. It becomes an over-the-shoulder that alternates sides, or a two-shot. Lines whose last
impact kills (`FCinemafyLine::bLethal`, set from Death reactions) give that impact a two-shot focused on the
defender, and a speed ramp whose depth (`SlowMoRate`) and length (`SlowMoHold`) scale with the style's
`SlowMoChance`. A style with no slow motion gets no ramp. `UCinemafyFramingLibrary::IsFaceCoveredByForeground`
rejects framings where the counterpart's head or shoulders cover the subject's face. The obstacle-avoiding
solver then tries its other angles.

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

- **Planned trajectories:** time-parameterized routes that plan vault entry speed and turn distance,
  fed through the performer's own trajectory inputs.
- **Validated move definitions:** promote name-matched candidates into curated entries with contact
  markers, entry windows, body masks and root-motion policy (Move Library is the start of this).
- **Contextual Animation adapter** for paired content (the native synchronized interactions are in).
- **Weapon-path contact checks:** verify the blade actually reaches the defender through the swing,
  not just at one instant.
- **Hand occupancy and stow/draw performances;** action-aware gaze and IK reduction during contact
  windows.
- **Fixed-time contact constraints** ("the hit lands at 4.2s"), resolved explicitly rather than by
  speeding clips up.
- **Bake validation:** compare reconstructed world-space root/hands/feet against the capture and
  report drift; MRQ subframe tests.
- **Versioned takes** kept side by side, rather than replaced.
- **Preview isolation:** suppress gameplay side effects (damage, quests, saves) during Perform &
  Record without suppressing animation notifies.
- **Composition checks:** subject cropping across a move (shoulder occlusion and both-sides coverage are in).

## Licensing note

Cinemafy doesn't ship any third-party animation. GASPALS's content is **UE-only** licensed, so keep it
in the project, not in this plugin.
