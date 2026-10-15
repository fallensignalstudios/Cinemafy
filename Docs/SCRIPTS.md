# Scripts: breakdown, mapping and the continuity checklist

Cinemafy can read a whole script for a game or a film and turn it into a **Script Project**. The project
breaks the script into acts, chapters and scenes, and tracks how far along each scene is in production.
The scenes you shoot are mapped back onto it, so it doubles as the project's checklist.

## Importing

- **Hub > Import Script**: pick a `.fountain`, `.fdx` (Final Draft) or `.txt` file. A `SP_<name>` asset is made in `/Game/Cinemafy/Scripts`, opened, and the breakdown report is shown.
- On the asset: set **Script File**, then click **Import Script**.
- Re-importing a revised script keeps each scene's status, director, sequence, level and notes, as long
  as the scene still matches (same heading and first line, or the same heading).
- From Python / MCP: `import_script(file_path, name)`.

Try it with [samples/TheBridge.fountain](samples/TheBridge.fountain).

## What it reads

| Script style | What Cinemafy picks up |
| --- | --- |
| Screenplay / Fountain | Scene headings (`INT.`, `EXT.`, `INT./EXT.`, `I/E`, forced `.HEADING`), character cues (`KADE`, `THANE (V.O.)`, `@Name`), parentheticals, transitions (`CUT TO:`, `FADE OUT.`), `# act` / `## chapter` sections, `=` synopses, the title page. It ignores boneyard and `[[notes]]`. |
| Final Draft (`.fdx`) | Scene Heading, Character, Parenthetical, Dialogue, Action and Transition paragraphs, and act breaks. |
| Game scripts | `KADE: line` dialogue, `[Scene: Hangar]` / `SCENE 12: Hangar` markers, and `MISSION 3: Blackout`, `CHAPTER 2`, `LEVEL`, `QUEST` or `EPISODE` markers, even with no blank lines. |
| Act markers | `ACT ONE`, `ACT 2`, `PART III`, `PROLOGUE`, `EPILOGUE`, `COLD OPEN`, or `#` sections. |

## PDF screenplays

A screenplay PDF imports straight away. Cinemafy runs `pdftotext` on it, which comes with poppler
(`brew install poppler` on a Mac, `poppler-utils` on Linux). The text is kept in `Saved/Cinemafy` so you can check
it. A printed page's furniture is cleaned off before the script is read:

- page numbers, `(MORE)` and `CONTINUED`
- `(CONT'D)`: the speech carries on as one line
- scene numbers either side of the heading (`12   INT. BRIDGE - NIGHT   12`)
- a heading wrapped onto a second line
- a paragraph broken by the page, which joins back up
- a title page without `Title:`, whose first line is taken as the title

Writers' conventions it reads:

- **A scene id on the line under a heading** (`SCOR-01-CS01`): that becomes the scene's permanent id, exactly as
  in a script bible.
- **A coded chapter header** (`SCOR 01   THE MANTLE`): a chapter with id `SCOR-01`, titled "The Mantle".
- **`PROLOGUE` / `EPILOGUE`**: bookends. The story proper starts at the first chapter after the prologue.

When the scenes carry a script bible's ids and the bible's Markdown sits beside the screenplay (or is set as
**Act Map File**), the screenplay takes the bible's acts. **Compare Scripts** (`compare_scripts`) then cross-checks
them by scene id: scenes only in one, scenes out of order, and scenes whose spoken words or speakers differ. It
works just as well for two drafts of the same script.

## Game script bibles

A game's cutscene bible imports as it is, no screenplay needed. Two formats are read:

- **The JSON companion:** `{ "title", "scenes": [ { "scene_id", "mission_id", "title", "mode", "trigger",
  "entry_state", "location", "cast", "control_before", "control_after", "camera_staging", "required_beats",
  "exit_state", "handoff", "continuity_notes", "planning_seconds", "skip_policy", "script": [ { "type":
  "dialogue" | "action" | "display", "speaker", "origin", "source_chapter", "text" } ] } ] }`
- **The Markdown bible:**
  - a scene map: `## 2. ... scene map`, with `### Act 1: Title` over tables of `[SCOR-01-CS01 · Title](...)`
  - then each scene: `#### SCOR-01-CS01`, a bold title, `**Key:** value` lines, `**Performance script**`,
    italic action, dialogue as `**TARRIK [M · Chapter 1]:** line`, and `- ` bullets for the completion state

See `Docs/samples/ScriptBible_Sample.md` / `.json` for a small example of each.

What happens on import:

- **Ids are permanent.** Scenes keep the bible's ids (`SCOR-01-CS03`). Moving a scene never renumbers it, and a
  re-import matches scenes by id, so their production work stays.
- **Structure:** missions are chapters. Acts and mission titles come from the scene map. JSON has no acts, so a
  JSON bible borrows them from a Markdown bible in the same folder, or the project's **Act Map File**. Without
  one, it gets Prologue / Campaign / Epilogue.
- **The game contract** on each scene (Game, on the scene):
  - presentation: locked cinematic, staged dialogue, gameplay bridge, record / Witness, montage, radio / holo
  - trigger, entry state, and who the player controls before and after
  - exit state, required beats, handoff, camera and continuity notes, planned length, skippable
- **Lines:**
  - Each line keeps its origin (`M` manuscript, `A` adapted) and source chapter. The voice-over CSVs show it.
  - `ON-SCREEN DISPLAY` lines are on-screen text, with `OSD_` ids in the localization table.
  - `(RECORD)` after a name is a direction (a recording), not a different character.
- **Camera:** the camera notes and the italic staging are read for camera intent ("Warm cramped two-shot",
  "Tarrik's point of view sinks into mud", "Slow push in on the seal", "handheld"). The reader is conservative:
  ordinary prose ("Following his gaze", "teeth close on a breastplate") is left alone.
- **Gameplay bridges** split into inserts wherever the staging gives the player control ("rises into player
  control", "Playable walking continues", "Gameplay resumes", "Restore Selene's camera and interaction control").
  Address one as `SCOR-01-CS01#2` anywhere a scene id goes: Previs, Open, Scene Directors, MCP. Each insert gets
  its own `SC_<scene>_I<n>` sequence.

`Scene_Contracts.csv` in the paperwork lists every scene's contract and production status. `scene_contract`
(MCP) reads one.

## Branching (games)

Choices, diverts and flags go right in the script:

- `* [Choice] -> target ~ flag`
- `{condition}` before a choice
- `-> target` to divert
- `~ flag` to set a flag, and `? flag` to make a scene conditional

They are played in game by the Story Player. See [STORYTELLING](STORYTELLING.md#8-games-the-story-player)
and `samples/Mutiny_Branching.txt`.

## Line ids and voice-over

Every dialogue line gets a stable **line id**: speaker initials plus a hash of who says what. Voice-over
files, subtitles and localization key on it, and recorded or scratch audio stays attached to its line
across re-imports.

## Parentheticals

A line's parenthetical is how it's played. Cinemafy reads it for a feeling, how strongly, and how it
sounds:

| Parenthetical | Played as |
| --- | --- |
| `(shouting)`, `(furious)`, `(angrily)`, `(roaring)` | angry, strong, louder and quicker |
| `(whispering)`, `(quietly)`, `(under her breath)` | soft and low (scratch voices whisper) |
| `(through tears)`, `(sobbing)`, `(sadly)` | sad |
| `(terrified)`, `(scared)`, `(trembling)` | afraid |
| `(shocked)`, `(stunned)` | surprised |
| `(disgusted)`, `(revolted)` | disgusted |
| `(coldly)`, `(flatly)`, `(sarcastic)` | cold and restrained |
| `(smiling)`, `(laughing)`, `(warmly)` | happy |
| `(nervous)`, `(hesitant)`, `(uneasy)` | worried, slower |
| `(firmly)`, `(determined)`, `(defiant)` | determined |

Directors made from the script set each line's emotion and intensity from it. That steers the face, the body
casting and the cut. Scratch voices use the pace, volume and whisper. A line with no parenthetical that is
written in capitals, or has `!!`, is shouted. `(V.O.)` and `(O.S.)` are left alone.

## Camera directions

Write the camera the way screenplays do and the director follows it. A direction can sit on its own line in
capitals, or open an action paragraph ("CLOSE ON KADE'S HAND. It trembles."). It applies to the next line spoken:

| On the page | On screen |
| --- | --- |
| `CLOSE ON THANE`, `TIGHT ON`, `CU -` | a close-up on Thane (a reaction shot if Thane is listening) |
| `EXTREME CLOSE-UP - THANE'S EYES`, `ECU` | an extreme close-up |
| `MEDIUM CLOSE`, `MCU`, `ANGLE ON` | a medium close-up |
| `MEDIUM SHOT`, `MS -` | a medium shot |
| `WIDE`, `WIDE SHOT`, `ESTABLISHING`, `LONG SHOT` | a wide two-shot (a group shot with three or more) |
| `TWO SHOT` | a medium two-shot |
| `OVER KADE'S SHOULDER`, `OTS`, `REVERSE ANGLE` | over Kade's shoulder, onto whoever she faces |
| `LOW ANGLE ON THANE` | a low hero angle |
| `KADE'S POV`, `POV - KADE` | through Kade's eyes, at whoever else is in the exchange |
| `INSERT - THE DATAPAD` | tight on the prop labelled datapad (or a character's hand: `INSERT - KADE'S HAND`) |
| `TRACKING WITH`, `FOLLOWING` | walk-and-talk coverage |
| `SLOW PUSH IN ON KADE`, `DOLLY IN` | a push in over the shot |
| `PULL BACK TO REVEAL THE FLEET` | a wide that pulls out |
| `CRANE UP`, `CRANE DOWN`, `ARC AROUND`, `CRASH ZOOM`, `TRUCK LEFT` | that move |
| `HANDHELD`, `STEADICAM`, `LOCKED OFF` | the operator's feel, from this line on |
| `BACK TO SCENE` | the director's choice again |

Moves and framings combine: `PUSH IN TO A CLOSE ON THANE`. The subject is matched to the scene's characters
(the last word of a name is enough), then to a prop by its label.

Transitions set how the **next** scene comes in: `SMASH CUT TO:` is a hard cut even in a style that dissolves,
and `MATCH CUT TO:` keeps the framing across the break. `DISSOLVE TO:`, `FADE OUT.` / `FADE TO BLACK.`,
`FADE TO WHITE`, `WIPE TO:` and `WHIP PAN TO:` are also read. A plain `CUT TO:` leaves it to the style.

Directors made from the script carry the directions on their lines (*Force Shot*, *Shot On*, *Force Move*,
*Force Feel*, *Shot Call*). When a revised script refreshes a director, these fields are rewritten from the
page. Your other tweaks on unchanged lines stay.

## Set dressing

Cinemafy reads the action for what the art department has to provide. The **Dressing** list on each set (in
Cast & Sets) and the `Props_and_Dressing` paperwork hold:

- **Set pieces** where something happens: "vaults a **cargo crate**", "picks up a saber from the **weapons
  rack**", "looks at the empty **command chair**"
- **Hand props**, with who handles them: "picks up a **saber**", "draws her **pistol**", "grabs the **DATAPAD**"
  (capitalized props count)
- **Wardrobe**, with who wears it: "wears his **dress uniform**"

Walls, floors, bodies ("pressed to her side"), people and the location itself are left out.

**Dress the Set** (a card's **...** menu, pre-flight, or `dress_set`) places every set piece the level doesn't
have yet:

- in front of the mark with its name, or in a row across the set
- as the **Mesh** you set on its Dressing entry, or a placeholder block sized like the real thing (a console is
  waist-high, a rack is tall, a lift is a platform)

Pieces are labelled `Set_<name>` so Previs finds them.

## Revisions

Re-import a revised script (the asset's **Import** button, or `reimport_script_project`) and Cinemafy compares
it with what you had. Every scene keeps its production work (status, sequence, takes, notes, director) when its
heading or first line still match. The project's **Revision Report** lists:

- scenes **rewritten**, **new** and **removed** (removed scenes' sequences and directors stay in the project)
- lines to **re-record:** rewritten lines that had real voice-over. The old words are kept beside the new ones
  for the booth, the paperwork marks them, and **Assign Recorded Voices** clears the flag once a new take is in.

A scene staged (Previs, or a director made from it) from older pages is **Out of date** on the Story Board.
**Update Changed** (board toolbar, or `update_changed_scenes`) brings the open level's out-of-date scenes up to
the new pages:

- previs'd scenes are re-staged without recording. Choreographies changed by hand since Previs made them are
  kept, renamed `Edited_...` and out of takes, so you can copy your beats across.
- directors get their lines refilled. Lines whose words didn't change keep every tweak (speaker, listener,
  audio, emotion).

`script_revision` reports the same over MCP.

## Structure, marked or inferred

**Acts** come from the script's own markers when it has them.

If it has none (and **Infer Acts** is on), Cinemafy places three-act breaks using the estimated running
time, near the 25% and 75% marks. Within that window, each break lands on the strongest turn in the story:

- right after a fade out, or a time jump (`LATER`, `YEARS LATER`, `NEXT MORNING`, `FLASHBACK`)
- a place the story hasn't been before
- the end of an action scene

It never breaks into a `CONTINUOUS` scene. Scripts under six scenes are one act.

**Chapters** also come from the script's markers when it has them (missions, chapters, `##` sections).
Otherwise Cinemafy groups runs of scenes that share time and place. A new chapter starts after a fade out,
on a time jump, or when the story moves somewhere new once a chapter has a few scenes. Very long runs are
split at about 1.6x **Scenes Per Chapter**. Inferred chapters are named after their main location.

Every act and chapter records whether it was marked or **inferred**, and **Why** it starts where it does.
The **Breakdown Report** lists all of these decisions.

**Scenes** are numbered `act.chapter.scene` (e.g. `2.3.1`) and also carry a running number. Each scene has:

- its heading, place, time of day, cast, lines and synopsis
- an estimated screen time (dialogue at speaking pace, action at reading pace)
- whether it describes action (a fight, a chase, a stunt)

## Continuity

Cinemafy walks the scenes in order and keeps what has to stay true for each speaking character:

| Category | Starts with | Ends with |
| --- | --- | --- |
| Injury | wounded, bleeding, limps, stabbed, knocked out... | patched up / bandaged (becomes "bandaged") |
| Carrying | draws, picks up, grabs, wields, carries... | drops, holsters, sheathes, hands over... |
| Wearing | wears, puts on, changes into, dressed in | takes off, removes |
| Dead | dies, is killed, `X kills Y` | (permanent) |

Pronouns (*she*, *he*) refer to the last character named in the paragraph.

Each scene shows:

- **Carried In:** what's true going in, for the people in the scene. This is the wardrobe, props, makeup
  and blocking checklist.
- **Changes:** what happens to change it in this scene.
- **Warnings:** for example, a character who died earlier speaking again. Lines marked `V.O.` and
  scenes marked `FLASHBACK` are allowed.

These are heuristics over the script's words. They're there to catch things, not to replace a script
supervisor.

## Mapping finished work back

Each scene has **Status** (Not Started, Blocking, Recorded, Directed, Final, Cut), **Sequence**,
**Level**, **Director Label**, **Match Score** and **Notes**. Status gets set in four ways:

- **Create a director from a scene:** `create_director_for_scene(project, "2.1.1")`, or
  `UCinemafyEditorLibrary::CreateDirectorForScene`. This spawns a Scene Director with the scene's
  dialogue as its lines and links it to the scene. Speakers are matched to actor labels: an exact label
  first, then a character whose label contains the name or its last word. Each line's Listener is whoever
  spoke last. The scene moves to Blocking.
- **Direct it:** when a linked director cuts the scene, its scene becomes **Directed**, with the sequence
  and level recorded.
- **Map Scenes In Level** (asset button, or `map_script_scenes`): every Scene Director in the open level
  is matched to a scene. It uses the director's *Script Scene Id* when that is set. Otherwise it matches
  by what the director's lines say, and the match survives rewording on the day. Each mapped scene
  moves up to:
  - **Blocking** when it has lines
  - **Recorded** when a choreography take was handed to its director
  - **Directed** when it has been cut
- **By hand:** set Status on the asset, or call `set_script_scene_status`.

A director's **Script Project** and **Script Scene Id** fields (Director > Script) make the link explicit.

## The checklist

**Export Checklist** (asset button) writes `Saved/Cinemafy/<Title>_Checklist.md` and opens it.
`get_script_checklist` returns the same Markdown. It contains:

- overall progress (scenes directed out of scenes not cut), estimated runtime, cast and the warning count
- a progress tally per act and per chapter
- one checkbox per scene, with its status, estimated length, director and sequence, cast, carry-ins,
  warnings and notes
