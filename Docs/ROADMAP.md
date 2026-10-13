# Cinemafy Roadmap

## ✅ v0.1: Foundation
- [x] Lens kit (cinema primes, sensors, round bokeh)
- [x] Shot solver + Create Shot menu (8 coverage types, 180° safe, rule of thirds, lead room)
- [x] Camera Feel (tripod / shoulder / handheld / run & gun, keyable intensity, deterministic mode)
- [x] Eye Focus (eye bones, focal-plane distance, diopter rack focus)
- [x] Gaze (saccades, head follow, blinks) for MetaHumans
- [x] Looks + letterbox + Look Preset assets

## 🎯 v0.2: Sequencer-native
- [x] "Create Shot" adds the camera straight into the open Level Sequence with a Camera Cut
- [x] 180° line checker (selected cameras vs. two characters)
- [x] Operator moves: push, pull, truck, pedestal, crane, orbit, with eased / overshoot-and-settle curves
- [ ] Shot browser: a thumbnail strip of every camera in the sequence
- [ ] Line checker v2: walk the camera cut track and check each actual cut, with animated cameras

## 💡 v0.3: Lighting
- [x] Camera-relative light rigs: Three-Point, Loop, Rembrandt, Butterfly, Split, Noir, Silhouette
- [x] Auto short/broad-side keying; lux-at-subject intensities
- [x] Eye light (catchlight) built into every rig
- [x] Character-only lighting via lighting channels
- [ ] Practical-light matching (key from the nearest lamp or window direction)
- [ ] Per-shot rig snapshots driven by camera cuts in Sequencer

## 🎭 v0.4: Performance and flow
- [ ] Gameplay ↔ cutscene camera and character blend
- [ ] Listening-reaction idles during dialogue
- [ ] Animated letterbox bars in/out (UMG) for gameplay transitions
- [ ] Per-shot grade overrides + "match to reference shot"

## 🚀 Stretch
- [ ] Director assistant: describe a beat ("tense standoff") and get proposed coverage
- [ ] Anamorphic mode (squeeze, oval bokeh, streak flares)
- [ ] Movie Render Graph presets for trailer captures
