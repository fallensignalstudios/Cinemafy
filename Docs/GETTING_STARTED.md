# Getting started: your first Cinemafy scene in 15 minutes

You'll go from two characters standing in a level to a lit, graded, edited and subtitled
dialogue scene, and then play it in game.

## 0. Setup (once)
1. Put the plugin in `<Project>/Plugins/Cinemafy`, build, and open the editor.
2. Open **Tools → Cinemafy → Cinemafy Hub** and dock it next to the Details panel.
3. *MetaHumans:* add the **Cinemafy Face** node to each MetaHuman's Face Anim Blueprint, right
   before *Output Pose*. That one node applies face processing, lip sync, Gaze and Listener.
   See the README's "MetaHuman face setup" section.

## 1. Cast the scene (1 min)
Place your characters facing each other. Select them all.
- **Characters → Add Listening → Attentive**: they react while the other talks.
- **Characters → Face Emotion → Neutral**: gives them a Cinemafy Face to drive later.

## 2. Create a director (2 min)
With the characters still selected: **Scene → Scene Director → Intimate Drama**.
Select the new *CinemafyDirector* and open **Lines** in the Details panel. For each line:
- **Speaker / Listener**
- **Text** (it becomes the subtitle)
- **Audio**, your recorded line (optional, but it makes everything smarter)
- **Face Animation / Body Animation**, performance clips (optional)
- **Set Emotion** if the line changes how they feel

No audio yet? Set **Duration** and **Intensity** (0 = small talk, 1 = the big reveal) by hand.

## 3. Direct (1 min)
1. Create a Level Sequence, open it, and put the playhead where the scene should start.
2. Select the director and click **Direct**. If the lines have audio, it listens to it first.
3. Open **Scene → Shot Browser** and click **Capture Thumbnails**. That's your storyboard.

Every shot card says *why* the director chose it. The pacing strip shows the rhythm of the edit.

## 4. Iterate (5 min)
- **Re-roll** for another take. Cameras you've nudged by hand keep your tweaks.
- Try another style: change *Style* on the director and Direct again.
- Cut a version you like by hand? **Learn Style** turns your edit into the director's style.
- **Check Continuity** reviews every cut for line crossings, jump cuts and flash cuts.

## 5. Play it in game (5 min)
1. With the sequence open: **Scene → Gameplay Blend** (it eases in from and back out to gameplay).
2. In your level Blueprint or trigger: **Get Cinemafy Cutscene Subsystem → Play Cutscene**.
   Set *Anchor = Relative to Player* if the scene should happen wherever the player is.
3. Bind **On Cutscene Started / Finished** to hide and restore your HUD.

## Bonus: live, unscripted conversations
For NPC chats driven by your dialogue system, skip Sequencer entirely. Call the director's
**Direct Line** (speaker, listener, duration, intensity, subtitle) as each line plays, then
**End Conversation**. It cuts between live-solved cameras, retargets listeners and shows subtitles.
