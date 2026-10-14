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
