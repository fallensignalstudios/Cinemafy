# Cinemafy: overnight build plan

Twelve updates, in order. Each one is its own commit on the PR, so they're easy to review,
test or revert one at a time. Status is updated as work lands.

| # | Update | Why it matters | Status |
|---|---|---|---|
| 1 | **API audit** of everything not yet compiled (Listener, Face, lip sync, Director v1/v2, audio) | Several rounds haven't been built yet. Catch risky API use before you do | ✅ |
| 2 | **Automation tests** for the shot solver, director planner and lens math | Something you can run in *Session Frontend → Automation* to prove the logic, not just the build | ✅ |
| 3 | **Smart camera placement:** occlusion and wall avoidance | Solved cameras can land inside walls or behind pillars. Try alternatives automatically | ✅ |
| 4 | **Cut on the breath:** snap cuts to pauses in the dialogue audio, and time push-ins to emphasis | Editors cut in breaths and land moves on stressed words | ✅ |
| 5 | **Performance clips per line:** body / face animation per line, baked into the sequence | Your script becomes a full scene: cameras, audio *and* performance | ✅ |
| 6 | **Subtitles:** runtime overlay driven by the Director (live and in sequences) | Accessibility, and great for reviewing cuts | ✅ |
| 7 | **Group scenes (3+ characters):** group shot, addressee-aware coverage | Real scenes aren't always two-handers | ✅ |
| 8 | **Styles drive look and light:** each director preset brings a grade, letterbox and light rigs | One click from "script" to "lit, graded, cut" | ✅ |
| 9 | **Shot Browser v2:** director reasons on cards, Direct / Re-roll / Learn buttons, pacing strip | The storyboard becomes the director's control room | ✅ |
| 10 | **Continuity checker:** 180° across the actual cuts, jump cuts, flash cuts, all in one report | Catch editing mistakes in any sequence, hand-cut or directed | ✅ |
| 11 | **Cinemafy Hub:** one dockable panel for every tool | The Tools menu is getting long | ✅ |
| 12 | **Docs:** getting-started guide, changelog, architecture notes | So future-you (and teammates) can pick it up fast | ✅ |

---

# Batch 2: "The Director grows up"

| # | Update | Status |
|---|---|---|
| 13 | **Style assets + archetype presets:** savable `CinemafyDirectorStyle` assets, plus Space Opera Classic (modeled on classic-trilogy craft), Mind-Bender Epic, Symmetry Storybook, Long-Take Tension, Snap Zoom Grindhouse and Golden Hour Lyrical | ✅ |
| 14 | **Plan / bake split:** re-bake an edited plan without re-planning (foundation for timeline editing) | ⏳ |
| 15 | **Transitions:** wipes (with the outgoing shot still playing), dissolves, fades through black, match cuts. Scene breaks per line | ⏳ |
| 16 | **Hold-through and rack focus:** two-shots that hold across an exchange, with focus racking to whoever's talking | ⏳ |
| 17 | **Walk-and-talk:** tracking-lead and follow shots on a Steadicam-style follow rig, with moving lines auto-detected | ⏳ |
| 18 | **Action coverage:** cut on impact, crash (snap) zooms, speed ramps; impacts auto-found from audio | ⏳ |
| 19 | **Cross-cutting:** interleave two directors' scenes, accelerating toward the climax | ⏳ |
| 20 | **Music-aware cutting:** beat detection, with cuts snapped to the beat | ⏳ |
| 21 | **Timeline editing:** lock shots, swap for an alternate, nudge cuts, save and compare takes, all from the Shot Browser | ⏳ |
| 22 | **Performance import:** auto-assign audio and anim takes from a folder, plus script import from CSV | ⏳ |
| 23 | **Subtitles v2:** reading-speed timing and smart splitting of long lines, working in any language | ⏳ |
| 24 | **The brain:** a decision layer that picks the right tool for each moment, with reasons and tests | ⏳ |
