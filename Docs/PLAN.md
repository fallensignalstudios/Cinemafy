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
| 11 | **Cinemafy Hub:** one dockable panel for every tool | The Tools menu is getting long | ⏳ |
| 12 | **Docs:** getting-started guide, changelog, architecture notes | So future-you (and teammates) can pick it up fast | ⏳ |
