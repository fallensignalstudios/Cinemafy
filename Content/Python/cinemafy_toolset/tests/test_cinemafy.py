"""Tests for CinemafyTools. Run against a live editor with a level loaded."""

import unreal

from toolset_registry.tests.toolset_testcase import ToolCallTestCase
from cinemafy_toolset.toolsets.cinemafy import CinemafyTools

_FOLDER = '/Game/__CinemafyToolsetTests'
_MESH = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple'
_ORIGIN = unreal.Vector(0, 0, 20000)  # far above anything in the level


class CinemafyToolsTestCase(ToolCallTestCase):
    """Test CinemafyTools against two characters and a scratch Level Sequence."""

    def setUp(self):
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.before = set(a.get_path_name() for a in self.actors.get_all_level_actors())
        self.kara = self._character(-85, -90)
        self.voss = self._character(85, 90)
        self.sequence = None

    def tearDown(self):
        unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
        for actor in self.actors.get_all_level_actors():
            if actor.get_path_name() not in self.before:
                self.actors.destroy_actor(actor)
        if unreal.EditorAssetLibrary.does_directory_exist(_FOLDER):
            unreal.EditorAssetLibrary.delete_directory(_FOLDER)

    # --- Scene Director ---------------------------------------------------------------

    def test_create_scene_director_returns_director_with_script(self):
        """Creates a director with a starter script for the characters."""
        director = CinemafyTools.create_scene_director([self.kara, self.voss], unreal.CinemafyDirectorPreset.INTIMATE_DRAMA)
        self.assertIsInstance(director, unreal.CinemafySceneDirector)
        self.assertGreater(len(director.lines), 0)

    def test_create_scene_director_raises_with_one_character(self):
        """Raises when fewer than two characters are given."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.create_scene_director([self.kara], unreal.CinemafyDirectorPreset.EPIC)

    def test_direct_scene_bakes_cuts(self):
        """Plans shots and adds camera cuts to the open sequence."""
        director = self._directed()
        self.assertGreater(len(director.last_plan), 0)
        self.assertTrue(self.sequence.find_tracks_by_type(unreal.MovieSceneCameraCutTrack))

    def test_direct_scene_raises_without_open_sequence(self):
        """Raises when no Level Sequence is open."""
        director = self._director()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.direct_scene(director)

    def test_direct_scene_raises_without_speakers(self):
        """Raises when no line has a speaker."""
        director = self._director()
        director.set_editor_property('lines', [unreal.CinemafyLine()])
        self._open_sequence()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.direct_scene(director)

    def test_reroll_scene_changes_seed(self):
        """Re-directs with a new seed."""
        director = self._directed()
        seed = director.seed
        shots = CinemafyTools.reroll_scene(director)
        self.assertNotEqual(director.seed, seed)
        self.assertGreater(len(shots), 0)

    def test_clear_directed_shots_removes_cuts(self):
        """Removes the director's cuts from the sequence."""
        director = self._directed()
        CinemafyTools.clear_directed_shots(director)
        track = self.sequence.find_tracks_by_type(unreal.MovieSceneCameraCutTrack)
        self.assertEqual(len(track[0].get_sections()) if track else 0, 0)

    def test_clear_directed_shots_raises_without_open_sequence(self):
        """Raises when no Level Sequence is open."""
        director = self._director()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.clear_directed_shots(director)

    def test_toggle_shot_lock_toggles(self):
        """Locks, then unlocks, a shot."""
        director = self._directed()
        self.assertTrue(CinemafyTools.toggle_shot_lock(director, 0))
        self.assertFalse(CinemafyTools.toggle_shot_lock(director, 0))

    def test_toggle_shot_lock_raises_out_of_range(self):
        """Raises for a shot index outside the plan."""
        director = self._directed()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.toggle_shot_lock(director, len(director.last_plan))

    def test_swap_shot_for_alternate_locks_shot(self):
        """Swaps a shot and locks it."""
        director = self._directed()
        shots = CinemafyTools.swap_shot_for_alternate(director, 1)
        self.assertTrue(shots[1].locked)

    def test_swap_shot_for_alternate_raises_out_of_range(self):
        """Raises for a shot index outside the plan."""
        director = self._directed()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.swap_shot_for_alternate(director, -1)

    def test_nudge_shot_cut_moves_cut(self):
        """Moves the cut before a shot."""
        director = self._directed()
        start = director.last_plan[1].start_time
        shots = CinemafyTools.nudge_shot_cut(director, 1, 0.25)
        self.assertAlmostEqual(shots[1].start_time, start + 0.25, places=2)

    def test_nudge_shot_cut_raises_for_first_shot(self):
        """Raises for the first shot, which has no cut before it."""
        director = self._directed()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.nudge_shot_cut(director, 0, 0.25)

    def test_save_and_load_take_round_trip(self):
        """Saves a take and restores it."""
        director = self._directed()
        planned = [(s.shot_type, s.start_time) for s in director.last_plan]
        index = CinemafyTools.save_take(director, 'A')
        CinemafyTools.reroll_scene(director)
        restored = CinemafyTools.load_take(director, index)
        self.assertEqual([(s.shot_type, s.start_time) for s in restored], planned)

    def test_save_take_raises_without_cut(self):
        """Raises when the director hasn't directed anything yet."""
        director = self._director()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.save_take(director, 'A')

    def test_load_take_raises_out_of_range(self):
        """Raises for a take index that doesn't exist."""
        director = self._directed()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.load_take(director, 5)

    # --- Cameras ----------------------------------------------------------------------

    def test_create_shot_returns_camera(self):
        """Creates a camera aimed at the subject."""
        camera = self._camera()
        self.assertIsInstance(camera, unreal.CineCameraActor)

    def test_create_shot_adds_to_open_sequence(self):
        """Adds the camera to the open sequence with a cut."""
        self._open_sequence()
        CinemafyTools.create_shot(self.kara, unreal.CinemafyShotType.CLOSE_UP, self.voss)
        self.assertTrue(self.sequence.find_tracks_by_type(unreal.MovieSceneCameraCutTrack))

    def test_create_shot_raises_without_subject(self):
        """Raises when subject is missing."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.create_shot(None, unreal.CinemafyShotType.CLOSE_UP, None, False, False)

    def test_create_shot_raises_without_sequence_when_adding(self):
        """Raises when asked to add to a sequence but none is open."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.create_shot(self.kara, unreal.CinemafyShotType.CLOSE_UP, self.voss, False, True)

    def test_apply_look_keeps_selection(self):
        """Grades the camera and leaves the editor selection as it was."""
        camera = self._camera()
        self.actors.set_selected_level_actors([self.kara])
        CinemafyTools.apply_look([camera], unreal.CinemafyLook.NOIR, 1.0)
        self.assertEqual(self.actors.get_selected_level_actors(), [self.kara])

    def test_apply_look_raises_for_non_camera(self):
        """Rejects an entry that isn't a Cine Camera."""
        with self.assertRaises(TypeError):
            CinemafyTools.apply_look([self.kara], unreal.CinemafyLook.NOIR, 1.0)

    def test_apply_look_raises_for_empty_list(self):
        """Raises when no cameras are given."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.apply_look([], unreal.CinemafyLook.NOIR, 1.0)

    def test_set_letterbox_constrains_aspect(self):
        """Constrains the camera to the aspect ratio."""
        camera = self._camera()
        CinemafyTools.set_letterbox([camera], 2.39)
        component = camera.get_cine_camera_component()
        self.assertTrue(component.constrain_aspect_ratio)
        self.assertAlmostEqual(component.aspect_ratio, 2.39, places=2)

    def test_set_letterbox_raises_for_bad_ratio(self):
        """Raises for a non-positive aspect ratio."""
        camera = self._camera()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.set_letterbox([camera], 0.0)

    def test_apply_prime_sets_focal_length(self):
        """Sets the lens to the prime's focal length."""
        camera = self._camera()
        CinemafyTools.apply_prime([camera], unreal.CinemafyPrime.PRIME85, 2.0)
        component = camera.get_cine_camera_component()
        self.assertAlmostEqual(component.current_focal_length, 85.0, places=1)
        self.assertAlmostEqual(component.current_aperture, 2.0, places=2)

    def test_apply_prime_raises_for_bad_f_stop(self):
        """Raises for a non-positive f-stop."""
        camera = self._camera()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.apply_prime([camera], unreal.CinemafyPrime.PRIME85, 0.0)

    def test_apply_camera_feel_adds_component(self):
        """Adds a Camera Feel component."""
        camera = self._camera()
        CinemafyTools.apply_camera_feel([camera], unreal.CinemafyCameraFeel.HANDHELD)
        feel = camera.get_component_by_class(unreal.CinemafyCameraFeelComponent)
        self.assertEqual(feel.feel, unreal.CinemafyCameraFeel.HANDHELD)

    def test_apply_camera_feel_raises_for_non_camera(self):
        """Rejects an entry that isn't a Cine Camera."""
        with self.assertRaises(TypeError):
            CinemafyTools.apply_camera_feel([self.voss], unreal.CinemafyCameraFeel.HANDHELD)

    def test_apply_camera_move_adds_component(self):
        """Adds a Camera Move component."""
        camera = self._camera()
        CinemafyTools.apply_camera_move([camera], unreal.CinemafyCameraMove.PUSH_IN)
        self.assertIsNotNone(camera.get_component_by_class(unreal.CinemafyCameraMoveComponent))

    def test_apply_camera_move_raises_for_non_camera(self):
        """Rejects an entry that isn't a Cine Camera."""
        with self.assertRaises(TypeError):
            CinemafyTools.apply_camera_move([self.voss], unreal.CinemafyCameraMove.PUSH_IN)

    # --- Characters and lighting ------------------------------------------------------

    def test_add_gaze_adds_component(self):
        """Adds a Gaze component to each character."""
        CinemafyTools.add_gaze([self.kara, self.voss])
        self.assertIsNotNone(self.kara.get_component_by_class(unreal.CinemafyGazeComponent))
        self.assertIsNotNone(self.voss.get_component_by_class(unreal.CinemafyGazeComponent))

    def test_add_gaze_raises_for_empty_list(self):
        """Raises when no characters are given."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.add_gaze([])

    def test_add_listening_adds_component(self):
        """Adds a Listener component to each character."""
        CinemafyTools.add_listening([self.kara, self.voss], unreal.CinemafyTemperament.SKEPTICAL)
        self.assertIsNotNone(self.kara.get_component_by_class(unreal.CinemafyListenerComponent))

    def test_add_listening_raises_for_empty_list(self):
        """Raises when no characters are given."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.add_listening([], unreal.CinemafyTemperament.SKEPTICAL)

    def test_set_face_emotion_adds_component(self):
        """Adds a Face component to the character."""
        CinemafyTools.set_face_emotion([self.kara], unreal.CinemafyEmotion.ANGRY)
        self.assertIsNotNone(self.kara.get_component_by_class(unreal.CinemafyFaceComponent))

    def test_set_face_emotion_raises_for_empty_list(self):
        """Raises when no characters are given."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.set_face_emotion([], unreal.CinemafyEmotion.ANGRY)

    def test_create_light_rig_returns_rig(self):
        """Creates a light rig for a subject and camera."""
        rig = CinemafyTools.create_light_rig(self.kara, unreal.CinemafyLightingStyle.REMBRANDT, self._camera())
        self.assertIsInstance(rig, unreal.CinemafyLightRig)

    def test_create_light_rig_raises_without_subject(self):
        """Raises when subject is missing."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.create_light_rig(None, unreal.CinemafyLightingStyle.REMBRANDT, None)

    # --- Sequence ---------------------------------------------------------------------

    def test_set_up_gameplay_blend_succeeds_with_cuts(self):
        """Sets up blending on a sequence that has camera cuts."""
        self._directed()
        CinemafyTools.set_up_gameplay_blend(1.0, 1.0, False)

    def test_set_up_gameplay_blend_raises_without_cuts(self):
        """Raises when the open sequence has no camera cuts."""
        self._open_sequence()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.set_up_gameplay_blend(1.0, 1.0, False)

    def test_set_up_gameplay_blend_raises_without_open_sequence(self):
        """Raises when no Level Sequence is open."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.set_up_gameplay_blend(1.0, 1.0, False)

    def test_render_camera_frame_writes_png(self):
        """Writes a PNG at the requested width with the camera's aspect ratio."""
        import os, tempfile
        camera = self._camera()
        CinemafyTools.set_letterbox([camera], 2.0)
        self._open_sequence()
        path = os.path.join(tempfile.mkdtemp(), 'frame.png')
        self.assertEqual(CinemafyTools.render_camera_frame(camera, 0, path, 200), path)
        with open(path, 'rb') as f:
            header = f.read(24)
        self.assertEqual(header[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(int.from_bytes(header[16:20], 'big'), 200)
        self.assertEqual(int.from_bytes(header[20:24], 'big'), 100)

    def test_eye_focus_distance_is_on_the_subject(self):
        """Solves Eye Focus to the subject's focal-plane distance without waiting for a tick."""
        camera = self._camera()
        to_subject = (self.kara.get_actor_location() - camera.get_actor_location()).length()
        distance = CinemafyTools._eye_focus_distance(camera)
        self.assertGreater(distance, 0.5 * to_subject)
        self.assertLess(distance, 1.5 * to_subject)

    def test_render_camera_frame_raises_without_open_sequence(self):
        """Raises when no Level Sequence is open."""
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.render_camera_frame(self._camera(), 0, '/tmp/frame.png', 200)

    def test_render_camera_frame_raises_for_relative_path(self):
        """Raises when file_path isn't an absolute .png path."""
        camera = self._camera()
        self._open_sequence()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.render_camera_frame(camera, 0, 'frame.png', 200)

    def test_render_camera_frame_raises_for_tiny_width(self):
        """Raises for a width below 16 pixels."""
        camera = self._camera()
        self._open_sequence()
        with self.assertToolRaisesRuntimeError():
            CinemafyTools.render_camera_frame(camera, 0, '/tmp/frame.png', 4)

    def test_direct_scene_keys_emotions_on_a_byte_track(self):
        """Emotion changes in the script are keyed on a byte track that Sequencer can display."""
        director = self._director()
        lines = list(director.lines)
        lines[0].set_editor_property('set_emotion', True)
        lines[0].set_editor_property('emotion', unreal.CinemafyEmotion.ANGRY)
        director.set_editor_property('lines', lines)
        self._open_sequence()
        CinemafyTools.direct_scene(director)
        tracks = [t for b in self.sequence.get_bindings() for t in b.get_tracks()]
        self.assertTrue(any(isinstance(t, unreal.MovieSceneByteTrack) for t in tracks))
        self.assertFalse(any(isinstance(t, unreal.MovieSceneEnumTrack) for t in tracks))

    # --- Helpers ----------------------------------------------------------------------

    def _character(self, x: float, yaw: float) -> unreal.SkeletalMeshActor:
        location = _ORIGIN + unreal.Vector(x, 0, 0)
        actor = self.actors.spawn_actor_from_class(unreal.SkeletalMeshActor, location, unreal.Rotator(0, 0, yaw))
        actor.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset(_MESH))
        return actor

    def _camera(self) -> unreal.CineCameraActor:
        return CinemafyTools.create_shot(self.kara, unreal.CinemafyShotType.MEDIUM_CLOSE_UP, self.voss, False, False)

    def _open_sequence(self) -> None:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        self.sequence = tools.create_asset('LS_Test', _FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
        self.sequence.set_playback_start(0)
        self.sequence.set_playback_end(24 * 30)
        unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(self.sequence)

    def _director(self) -> unreal.CinemafySceneDirector:
        return CinemafyTools.create_scene_director([self.kara, self.voss], unreal.CinemafyDirectorPreset.CLASSIC_HOLLYWOOD)

    def _directed(self) -> unreal.CinemafySceneDirector:
        director = self._director()
        self._open_sequence()
        CinemafyTools.direct_scene(director)
        return director
