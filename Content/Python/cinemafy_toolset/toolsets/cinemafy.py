"""Cinemafy toolset: AI-callable access to Cinemafy's editor tools."""

import random

import unreal

import toolset_registry

_lib = unreal.CinemafyEditorLibrary
_seq_lib = unreal.LevelSequenceEditorBlueprintLibrary


@unreal.uclass()
class CinemafyTools(unreal.ToolsetDefinition):
    """Builds film-grade cutscenes with the Cinemafy plugin: AI-directed dialogue scenes baked into the
    open Level Sequence, framed coverage cameras, lenses, camera feel and moves, grading looks,
    letterboxing, camera-relative light rigs, and character gaze, listening and facial emotion.
    Director and sequence tools act on the Level Sequence currently open in Sequencer."""

    # --- Scene Director ---------------------------------------------------------------

    @toolset_registry.tool_call
    @staticmethod
    def create_scene_director(characters: list[unreal.Actor],
                              style: unreal.CinemafyDirectorPreset) -> unreal.CinemafySceneDirector:
        """Spawns a Scene Director for a group of characters, with a starter script of alternating
        lines that rise in intensity. Edit its Lines property to write the real scene.

        Args:
            characters: The characters in the scene, at least two.
            style: The directing style.

        Returns:
            The new director.
        """
        if len(characters) < 2:
            raise ValueError('A scene needs at least two characters.')
        director = CinemafyTools._with_selection(
            characters, lambda: _lib.create_scene_director_from_selection(style))
        if director is None:
            raise RuntimeError('Cinemafy could not create a director for these characters.')
        return director

    @toolset_registry.tool_call
    @staticmethod
    def direct_scene(director: unreal.CinemafySceneDirector) -> list[unreal.CinemafyPlannedShot]:
        """Plans the director's scene and bakes cameras and timed camera cuts into the open Level
        Sequence from the playhead, replacing this director's previous cuts. Cameras are reused
        across takes, so hand tweaks survive.

        Args:
            director: The director to run.

        Returns:
            The planned shots in order, each with the director's reason for it.
        """
        CinemafyTools._require_open_sequence()
        if not any(line.speaker for line in director.lines):
            raise ValueError('The director has no lines with a Speaker.')
        if _lib.direct_scene_in_sequencer(director) <= 0:
            raise RuntimeError('The director planned no shots.')
        return list(director.last_plan)

    @toolset_registry.tool_call
    @staticmethod
    def reroll_scene(director: unreal.CinemafySceneDirector) -> list[unreal.CinemafyPlannedShot]:
        """Directs the same scene again with a new random seed for a different take. Locked shots
        are kept.

        Args:
            director: The director to re-roll.

        Returns:
            The new planned shots.
        """
        director.set_editor_property('seed', random.randint(1, 2 ** 31 - 1))
        return CinemafyTools.direct_scene(director)

    @toolset_registry.tool_call
    @staticmethod
    def clear_directed_shots(director: unreal.CinemafySceneDirector) -> None:
        """Removes the director's camera cuts from the open Level Sequence. Its cameras are kept.

        Args:
            director: The director whose cuts to remove.
        """
        CinemafyTools._require_open_sequence()
        _lib.clear_directed_shots(director)

    @toolset_registry.tool_call
    @staticmethod
    def toggle_shot_lock(director: unreal.CinemafySceneDirector, shot_index: int) -> bool:
        """Locks or unlocks one shot of the director's current cut. Locked shots survive re-rolls.

        Args:
            director: The director.
            shot_index: Index into the director's last plan.

        Returns:
            True if the shot is now locked.
        """
        CinemafyTools._require_shot(director, shot_index)
        return _lib.toggle_shot_lock(director, shot_index)

    @toolset_registry.tool_call
    @staticmethod
    def swap_shot_for_alternate(director: unreal.CinemafySceneDirector,
                                shot_index: int) -> list[unreal.CinemafyPlannedShot]:
        """Replaces one shot with the director's next-best alternative, locks it and re-bakes.

        Args:
            director: The director.
            shot_index: Index into the director's last plan.

        Returns:
            The updated planned shots.
        """
        CinemafyTools._require_open_sequence()
        CinemafyTools._require_shot(director, shot_index)
        if not _lib.swap_shot_for_alternate(director, shot_index):
            raise RuntimeError(f'Shot {shot_index} has no alternative.')
        return list(director.last_plan)

    @toolset_registry.tool_call
    @staticmethod
    def nudge_shot_cut(director: unreal.CinemafySceneDirector, shot_index: int,
                       delta_seconds: float) -> list[unreal.CinemafyPlannedShot]:
        """Moves the cut at the start of a shot, locks the shots on both sides and re-bakes.

        Args:
            director: The director.
            shot_index: Index into the director's last plan. Must not be the first shot.
            delta_seconds: How far to move the cut. Negative is earlier.

        Returns:
            The updated planned shots.
        """
        CinemafyTools._require_open_sequence()
        CinemafyTools._require_shot(director, shot_index)
        if shot_index == 0:
            raise ValueError('The first shot has no cut before it to move.')
        if not _lib.nudge_shot_cut(director, shot_index, delta_seconds):
            raise RuntimeError(f'Could not move the cut before shot {shot_index}.')
        return list(director.last_plan)

    @toolset_registry.tool_call
    @staticmethod
    def save_take(director: unreal.CinemafySceneDirector, name: str) -> int:
        """Saves the director's current cut as a named take.

        Args:
            director: The director.
            name: The take's name.

        Returns:
            The index of the new take.
        """
        if not director.last_plan:
            raise ValueError('The director has no cut to save. Direct the scene first.')
        return _lib.save_director_take(director, name)

    @toolset_registry.tool_call
    @staticmethod
    def load_take(director: unreal.CinemafySceneDirector,
                  take_index: int) -> list[unreal.CinemafyPlannedShot]:
        """Restores a saved take and re-bakes it where the scene was.

        Args:
            director: The director.
            take_index: Index into the director's Takes.

        Returns:
            The restored planned shots.
        """
        CinemafyTools._require_open_sequence()
        if not 0 <= take_index < len(director.takes):
            raise ValueError(f'take_index {take_index} is out of range (director has {len(director.takes)} takes).')
        if not _lib.load_director_take(director, take_index):
            raise RuntimeError(f'Could not load take {take_index}.')
        return list(director.last_plan)

    # --- Cameras ----------------------------------------------------------------------

    @toolset_registry.tool_call
    @staticmethod
    def create_shot(subject: unreal.Actor, shot_type: unreal.CinemafyShotType,
                    counterpart: unreal.Actor | None = None, flip_side: bool = False,
                    add_to_sequence: bool = True) -> unreal.CineCameraActor:
        """Spawns a Cine Camera framed, lensed and eye-focused for a coverage shot, always on the
        same side of the 180-degree line for a given flip_side.

        Args:
            subject: The character the shot is on.
            shot_type: The kind of shot.
            counterpart: Who the subject is talking to. Needed for two-shots and over-the-shoulders.
            flip_side: Which side of the line of action. Keep it the same for every shot in a scene.
            add_to_sequence: Also add the camera to the open Level Sequence with a cut at the playhead.

        Returns:
            The new camera.
        """
        if subject is None:
            raise ValueError('subject is required.')
        if add_to_sequence:
            CinemafyTools._require_open_sequence()
        request = unreal.CinemafyShotRequest()
        request.shot_type = shot_type
        request.subject = subject
        request.counterpart = counterpart
        request.flip_side = flip_side
        camera = _lib.create_shot(request, add_to_sequence)
        if camera is None:
            raise RuntimeError('Cinemafy could not create the shot.')
        return camera

    @toolset_registry.tool_call
    @staticmethod
    def apply_look(cameras: list[unreal.CineCameraActor], look: unreal.CinemafyLook,
                   strength: float = 1.0) -> None:
        """Applies a cinematic grade on top of each camera's existing post-process settings.

        Args:
            cameras: The cameras to grade.
            look: The grade.
            strength: Blend amount, 0 to 1.
        """
        CinemafyTools._apply_to_cameras(cameras, lambda: _lib.apply_look_to_selection(look, strength))

    @toolset_registry.tool_call
    @staticmethod
    def set_letterbox(cameras: list[unreal.CineCameraActor], aspect_ratio: float = 2.39) -> None:
        """Constrains each camera to a cinema aspect ratio with real letterbox bars.

        Args:
            cameras: The cameras to letterbox.
            aspect_ratio: Width over height, e.g. 2.39 (scope) or 1.85 (flat).
        """
        if aspect_ratio <= 0:
            raise ValueError('aspect_ratio must be positive.')
        CinemafyTools._apply_to_cameras(cameras, lambda: _lib.set_letterbox_on_selection(aspect_ratio))

    @toolset_registry.tool_call
    @staticmethod
    def apply_prime(cameras: list[unreal.CineCameraActor], prime: unreal.CinemafyPrime,
                    f_stop: float = 2.8) -> None:
        """Puts a cinema prime lens on each camera.

        Args:
            cameras: The cameras.
            prime: The lens.
            f_stop: Aperture. Lower is a shallower depth of field.
        """
        if f_stop <= 0:
            raise ValueError('f_stop must be positive.')
        CinemafyTools._apply_to_cameras(cameras, lambda: _lib.apply_prime_to_selection(prime, f_stop))

    @toolset_registry.tool_call
    @staticmethod
    def apply_camera_feel(cameras: list[unreal.CineCameraActor], feel: unreal.CinemafyCameraFeel) -> None:
        """Gives each camera procedural operator motion, from tripod drift to run-and-gun handheld.

        Args:
            cameras: The cameras.
            feel: How the operator holds the camera.
        """
        CinemafyTools._apply_to_cameras(cameras, lambda: _lib.apply_camera_feel_to_selection(feel))

    @toolset_registry.tool_call
    @staticmethod
    def apply_camera_move(cameras: list[unreal.CineCameraActor], move: unreal.CinemafyCameraMove) -> None:
        """Adds an eased camera move to each camera, sized for that move. Key its Progress 0 to 1 in
        Sequencer to play it.

        Args:
            cameras: The cameras.
            move: The move.
        """
        CinemafyTools._apply_to_cameras(cameras, lambda: _lib.apply_camera_move_to_selection(move))

    # --- Characters and lighting ------------------------------------------------------

    @toolset_registry.tool_call
    @staticmethod
    def add_gaze(characters: list[unreal.Actor]) -> None:
        """Gives each character living eyes: saccades, blinks and a head that follows the look.
        Set the Gaze component's LookAtTarget to aim it.

        Args:
            characters: The characters.
        """
        CinemafyTools._require_actors(characters)
        CinemafyTools._with_selection(characters, _lib.add_gaze_to_selection)

    @toolset_registry.tool_call
    @staticmethod
    def add_listening(characters: list[unreal.Actor], temperament: unreal.CinemafyTemperament) -> None:
        """Makes each character react while others talk (nods, blinks, glances). With exactly two
        characters, each listens to the other.

        Args:
            characters: The characters.
            temperament: How they react.
        """
        CinemafyTools._require_actors(characters)
        CinemafyTools._with_selection(characters, lambda: _lib.add_listening_to_selection(temperament))

    @toolset_registry.tool_call
    @staticmethod
    def set_face_emotion(characters: list[unreal.Actor], emotion: unreal.CinemafyEmotion) -> None:
        """Sets each character's facial emotion, adding a Cinemafy Face component if needed.
        Needs the Cinemafy Face anim node in the character's face Anim Blueprint to show.

        Args:
            characters: The characters.
            emotion: The emotion.
        """
        CinemafyTools._require_actors(characters)
        CinemafyTools._with_selection(characters, lambda: _lib.apply_face_emotion_to_selection(emotion))

    @toolset_registry.tool_call
    @staticmethod
    def create_light_rig(subject: unreal.Actor, style: unreal.CinemafyLightingStyle,
                         camera: unreal.Actor | None = None) -> unreal.CinemafyLightRig:
        """Spawns key, fill, rim and eye lights placed around a character for a shot. Set the rig's
        KeyIlluminance (lux at the face) to match the scene's exposure.

        Args:
            subject: The character to light.
            style: The lighting style.
            camera: The camera the lighting is designed for. None lights for the subject's front.

        Returns:
            The new light rig.
        """
        if subject is None:
            raise ValueError('subject is required.')
        rig = _lib.create_light_rig(subject, camera, style)
        if rig is None:
            raise RuntimeError('Cinemafy could not create the light rig.')
        return rig

    # --- Sequence ---------------------------------------------------------------------

    @toolset_registry.tool_call
    @staticmethod
    def set_up_gameplay_blend(blend_in_seconds: float = 1.0, blend_out_seconds: float = 1.0,
                              include_animations: bool = True) -> None:
        """Makes the open Level Sequence blend in from and back out to the gameplay camera.

        Args:
            blend_in_seconds: Ease from the gameplay camera into the first cut.
            blend_out_seconds: Ease from the last cut back to the gameplay camera.
            include_animations: Also ease characters' first and last animation sections.
        """
        sequence = CinemafyTools._require_open_sequence()
        if not sequence.find_tracks_by_type(unreal.MovieSceneCameraCutTrack):
            raise ValueError('The open Level Sequence has no camera cuts.')
        if not _lib.set_up_gameplay_blend_for_open_sequence(blend_in_seconds, blend_out_seconds, include_animations):
            raise RuntimeError('Could not set up the gameplay blend.')

    @toolset_registry.tool_call
    @staticmethod
    def render_camera_frame(camera: unreal.CineCameraActor, frame: int, file_path: str,
                            width: int = 1280) -> str:
        """Moves the open Level Sequence's playhead to a frame and renders what the camera sees there
        (its lens, letterbox and grade) to a PNG. Eye Focus is solved for that frame. The playhead
        stays at that frame.

        Args:
            camera: The camera to render through.
            frame: Frame in the sequence's display rate.
            file_path: Absolute path of the .png to write. Existing files are overwritten.
            width: Image width in pixels. Height follows the camera's aspect ratio.

        Returns:
            The path of the written image.
        """
        import os
        CinemafyTools._require_open_sequence()
        if width < 16:
            raise ValueError('width must be at least 16 pixels.')
        if not file_path.lower().endswith('.png') or not os.path.isabs(file_path):
            raise ValueError('file_path must be an absolute path ending in .png.')

        params = unreal.MovieSceneSequencePlaybackParams()
        params.frame = unreal.FrameTime(unreal.FrameNumber(frame))
        params.position_type = unreal.MovieScenePositionType.FRAME
        params.update_method = unreal.UpdatePositionMethod.SCRUB
        _seq_lib.set_global_position(params, unreal.MovieSceneTimeUnit.DISPLAY_RATE)

        view = camera.get_cine_camera_component().get_camera_view(0.0)
        aspect = view.aspect_ratio if view.constrain_aspect_ratio and view.aspect_ratio > 0 else 16.0 / 9.0
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        target = unreal.RenderingLibrary.create_render_target2d(
            world, width, max(1, round(width / aspect)), unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)

        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        capture_actor = actor_subsystem.spawn_actor_from_class(unreal.SceneCapture2D, view.location, view.rotation, True)
        try:
            capture = capture_actor.capture_component2d
            capture.texture_target = target
            capture.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
            capture.capture_every_frame = False
            capture.capture_on_movement = False
            capture.fov_angle = view.fov
            post = view.post_process_settings
            focus = CinemafyTools._eye_focus_distance(camera)
            if focus is not None:
                post.override_depth_of_field_focal_distance = True
                post.depth_of_field_focal_distance = focus
            # One-off captures have no exposure history, so let auto exposure settle instantly.
            post.override_auto_exposure_speed_up = True
            post.auto_exposure_speed_up = 100.0
            post.override_auto_exposure_speed_down = True
            post.auto_exposure_speed_down = 100.0
            capture.post_process_settings = post
            capture.post_process_blend_weight = view.post_process_blend_weight
            capture.capture_scene()
            capture.capture_scene()

            folder, name = os.path.split(file_path)
            os.makedirs(folder, exist_ok=True)
            stem = name[:-4]
            unreal.RenderingLibrary.export_render_target(world, target, folder, stem)
            exported = os.path.join(folder, stem)
            if os.path.exists(exported) and exported != file_path:
                os.replace(exported, file_path)
        finally:
            actor_subsystem.destroy_actor(capture_actor)
        if not os.path.exists(file_path):
            raise RuntimeError(f'The frame could not be written to {file_path}.')
        return file_path

    # --- Helpers ----------------------------------------------------------------------

    # --- Choreography -------------------------------------------------------------------

    @toolset_registry.tool_call
    @staticmethod
    def create_choreography(performer: unreal.Character) -> unreal.CinemafyChoreography:
        """Creates a Cinemafy Choreography for a character (or returns the one it already has).
        Add beats to it, then perform_and_record_choreography.

        Args:
            performer: The character who performs.

        Returns:
            The choreography.
        """
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            if isinstance(actor, unreal.CinemafyChoreography) and actor.get_editor_property('performer') == performer:
                return actor
        choreo = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
            unreal.CinemafyChoreography, performer.get_actor_location())
        if choreo is None:
            raise RuntimeError('Could not spawn a choreography.')
        choreo.set_actor_label('Choreography_' + performer.get_actor_label())
        choreo.set_editor_property('performer', performer)
        return choreo

    @toolset_registry.tool_call
    @staticmethod
    def add_choreography_beat(choreography: unreal.CinemafyChoreography, prompt: str, location: unreal.Vector,
                              target: unreal.Actor = None) -> str:
        """Appends a beat: the performer goes to the location and does what the prompt says. Prompts are
        one or two words: walk, run, sprint, sneak, vault, climb, jump, sword attack, combo, shoot, taunt,
        wave, look, wait 2, pistols, holster. Unknown words are searched for in animation names.

        Args:
            choreography: The choreography to add to.
            prompt: What happens at this point.
            location: Where it happens (world space).
            target: A character the action is aimed at (attacks, taunts), if any.

        Returns:
            How the director read the prompt and which animation(s) it picked.
        """
        index = choreography.add_beat(prompt, location, target)
        beat = choreography.get_editor_property('beats')[index]
        reading = beat.get_editor_property('reading')
        anims = [a.get_name() for a in beat.get_editor_property('chosen_animations') if a]
        return reading.get_editor_property('summary') + (' using ' + ', '.join(anims) if anims else '')

    @toolset_registry.tool_call
    @staticmethod
    def read_choreography_prompt(prompt: str) -> unreal.CinemafyPromptReading:
        """Shows how the choreographer reads a prompt, without adding it anywhere.

        Args:
            prompt: One or two words, e.g. "sword attack".

        Returns:
            The reading: action kind, gait, weapon, keywords, repeat count.
        """
        return unreal.CinemafyChoreographyLibrary.read_prompt(prompt)

    @toolset_registry.tool_call
    @staticmethod
    def perform_and_record_choreography(choreography: unreal.CinemafyChoreography) -> str:
        """Starts play-in-editor; every choreography in the level performs and is recorded. When the take
        ends, play stops and the performance is baked into the open Level Sequence (or a new one), then
        directed if the choreography's Direct After Bake is on. This returns immediately; the take runs
        in the background.

        Args:
            choreography: The main choreography (its director cuts the take).

        Returns:
            A status message.
        """
        if not choreography.get_editor_property('performer') or len(choreography.get_editor_property('beats')) == 0:
            raise ValueError('The choreography needs a performer and at least one beat.')
        _lib.perform_and_record_choreography(choreography)
        return 'Recording started; the take bakes into Sequencer when the performance finishes.'

    @toolset_registry.tool_call
    @staticmethod
    def direct_choreography(choreography: unreal.CinemafyChoreography) -> list[unreal.CinemafyPlannedShot]:
        """Cuts coverage around a recorded choreography take with a Scene Director.

        Args:
            choreography: A choreography with a recorded take.

        Returns:
            The planned shots.
        """
        if choreography.get_editor_property('take_duration') <= 0:
            raise ValueError('Record a take first (perform_and_record_choreography).')
        if _lib.direct_choreography(choreography) <= 0:
            raise RuntimeError('The director planned no shots.')
        director = choreography.get_editor_property('scene_director')
        return list(director.last_plan) if director else []

    # --- Edit (video-editor tools on the open sequence) ------------------------------------

    @toolset_registry.tool_call
    @staticmethod
    def edit_at_playhead(operation: str) -> bool:
        """Runs a Premiere-style edit on the open Level Sequence at the playhead. Ripple edits move every
        track after the edit.

        Args:
            operation: One of razor, razor_all, ripple_delete, trim_start, trim_end, roll_earlier,
                roll_later, roll_to_playhead, swap_next, next_cut, previous_cut.

        Returns:
            True if the edit was made.
        """
        CinemafyTools._require_open_sequence()
        edit = unreal.CinemafyEditLibrary
        operations = {
            'razor': lambda: edit.razor_at_playhead(False),
            'razor_all': lambda: edit.razor_at_playhead(True),
            'ripple_delete': edit.ripple_delete_shot_at_playhead,
            'trim_start': edit.ripple_trim_start_to_playhead,
            'trim_end': edit.ripple_trim_end_to_playhead,
            'roll_earlier': lambda: edit.roll_nearest_cut(-1),
            'roll_later': lambda: edit.roll_nearest_cut(1),
            'roll_to_playhead': edit.roll_nearest_cut_to_playhead,
            'swap_next': edit.swap_shot_with_next,
            'next_cut': lambda: edit.go_to_cut(True),
            'previous_cut': lambda: edit.go_to_cut(False),
        }
        if operation not in operations:
            raise ValueError('Unknown edit: ' + operation + '. Use one of ' + ', '.join(operations))
        return bool(operations[operation]())

    @toolset_registry.tool_call
    @staticmethod
    def add_transition_at_nearest_cut(transition: unreal.CinemafyTransition, seconds: float = 0.7,
                                      wipe_direction: unreal.CinemafyWipeDirection = unreal.CinemafyWipeDirection.ALTERNATE) -> bool:
        """Puts a transition on the cut nearest the playhead: wipe (straight, iris, clock), dissolve,
        fade through black, dip to white or whip pan.

        Args:
            transition: The transition type.
            seconds: Its length.
            wipe_direction: For wipes and whips. Alternate cycles like the classic serials.

        Returns:
            True if it was added.
        """
        CinemafyTools._require_open_sequence()
        return unreal.CinemafyEditLibrary.add_transition_at_nearest_cut(transition, seconds, wipe_direction)

    @toolset_registry.tool_call
    @staticmethod
    def add_title_at_playhead(style: unreal.CinemafyTitleStyle, title: str, subtitle: str = '', body: str = '',
                              seconds: float = 4.0) -> bool:
        """Adds a title card at the playhead: lower third, location card, chapter card, centered title or
        opening crawl (subtitle = intro line, body = the crawl's paragraphs).

        Args:
            style: The card style.
            title: Name / place / title.
            subtitle: Role / time of day / crawl intro line.
            body: Crawl text.
            seconds: How long it's up.

        Returns:
            True if it was added.
        """
        CinemafyTools._require_open_sequence()
        card = unreal.CinemafyTitleCard()
        card.set_editor_property('style', style)
        card.set_editor_property('title', title)
        card.set_editor_property('subtitle', subtitle)
        card.set_editor_property('body', body)
        card.set_editor_property('duration', seconds)
        return unreal.CinemafyEditLibrary.add_title_at_playhead(card)

    @toolset_registry.tool_call
    @staticmethod
    def duck_music_under_dialogue(duck_decibels: float = -10.0) -> int:
        """Dips the sequence's music under every other sound (dialogue), with smooth ramps.

        Args:
            duck_decibels: How far to dip (negative dB).

        Returns:
            How many dips were keyed.
        """
        CinemafyTools._require_open_sequence()
        return unreal.CinemafyEditLibrary.duck_music_under_dialogue(duck_decibels, 0.3)

    @staticmethod
    def _eye_focus_distance(camera: unreal.CineCameraActor):
        """The focal-plane distance the camera's Eye Focus wants right now (it only updates on ticks,
        which don't run while the editor viewport isn't realtime). None without Eye Focus."""
        focus = camera.get_component_by_class(unreal.CinemafyFocusComponent)
        if focus is None or focus.focus_target is None:
            return None
        component = camera.get_cine_camera_component()
        origin, forward = component.get_world_location(), component.get_forward_vector()

        def plane_distance(target):
            if focus.focus_on_eyes:
                point = unreal.CinemafyFramingLibrary.get_eye_point(target)
            else:
                point, _ = target.get_actor_bounds(True)
            return max((point - origin).dot(forward), 15.0)

        distance = plane_distance(focus.focus_target)
        if focus.rack_target is not None and focus.rack_alpha > 0:
            to = plane_distance(focus.rack_target)
            distance = 1.0 / (1.0 / distance + (1.0 / to - 1.0 / distance) * focus.rack_alpha)
        return max(distance + focus.focus_offset, 15.0)

    @staticmethod
    def _require_open_sequence() -> unreal.LevelSequence:
        sequence = _seq_lib.get_current_level_sequence()
        if sequence is None:
            raise RuntimeError('No Level Sequence is open in Sequencer. Open one first.')
        return sequence

    @staticmethod
    def _require_shot(director: unreal.CinemafySceneDirector, shot_index: int) -> None:
        count = len(director.last_plan)
        if not 0 <= shot_index < count:
            raise ValueError(f'shot_index {shot_index} is out of range (the cut has {count} shots).')

    @staticmethod
    def _require_actors(actors: list) -> None:
        if not actors or any(a is None for a in actors):
            raise ValueError('Pass at least one actor, and no empty entries.')

    @staticmethod
    def _apply_to_cameras(cameras: list, apply) -> None:
        CinemafyTools._require_actors(cameras)
        CinemafyTools._with_selection(cameras, apply)

    @staticmethod
    def _with_selection(actors: list, action):
        """Runs a selection-driven Cinemafy action on the given actors, then restores the selection."""
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        previous = actor_subsystem.get_selected_level_actors()
        actor_subsystem.set_selected_level_actors(actors)
        try:
            return action()
        finally:
            actor_subsystem.set_selected_level_actors(previous)
