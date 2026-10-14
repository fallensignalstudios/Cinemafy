# Changelog

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
