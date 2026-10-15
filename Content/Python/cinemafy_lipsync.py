"""Real lip-sync for Cinemafy: MetaHuman's audio-driven animation, one face performance per voiced line.

Cinemafy lists the lines that need a face (UCinemafyStoryLibrary.get_lip_sync_jobs), this module solves each one
with a MetaHuman Performance asset set to audio input, exports the face animation, and hands it back
(UCinemafyStoryLibrary.assign_lip_sync). Solving runs one line at a time on the editor's tick, so the editor stays
responsive. MetaHuman's Python API has moved between engine versions, so every call here is probed, and a missing
piece is reported instead of raised.
"""

import unreal

_queue = []
_current = None
_tick_handle = None
_done = []
_failed = []


def available():
    """MetaHuman's audio-driven animation is there to use."""
    return (getattr(unreal, 'MetaHumanPerformance', None) is not None
            and getattr(unreal, 'MetaHumanPerformanceExportUtils', None) is not None)


def generate(project, scene_id='', replace=False):
    """Queues every voiced line (of one scene, or the whole script) that has no face performance yet.

    Returns:
        What was queued, or why nothing was.
    """
    if project is None:
        return 'Cinemafy lip-sync: no script project.'
    if not available():
        return ("Cinemafy lip-sync: MetaHuman's audio-driven animation isn't available (enable the MetaHuman plugin). "
                'Faces keep the Talk curves.')
    jobs = list(unreal.CinemafyStoryLibrary.get_lip_sync_jobs(project, scene_id or '', bool(replace)))
    queued = {job.line_id for _, job in _queue}
    if _current is not None:
        queued.add(_current[1].line_id)
    fresh = [job for job in jobs if job.line_id not in queued]
    if not fresh:
        return 'Cinemafy lip-sync: nothing to solve (every voiced line with a MetaHuman face has one, or is queued).'
    for job in fresh:
        _queue.append((project, job))
    _start()
    return f'Cinemafy lip-sync: solving {len(fresh)} line(s) with MetaHuman ({len(_queue)} queued).'


def status():
    """How the lip-sync queue is doing."""
    working = f'working on {_current[1].line_id}, ' if _current is not None else ''
    lines = [f'Cinemafy lip-sync: {working}{len(_queue)} queued, {len(_done)} done, {len(_failed)} failed.']
    lines += [f'  failed {line_id}: {why}' for line_id, why in _failed[-10:]]
    return '\n'.join(lines)


# --- The queue ------------------------------------------------------------------------------------------------------

def _start():
    global _tick_handle
    if _tick_handle is None:
        _tick_handle = unreal.register_slate_post_tick_callback(_on_tick)


def _stop():
    global _tick_handle
    if _tick_handle is not None:
        unreal.unregister_slate_post_tick_callback(_tick_handle)
        _tick_handle = None
    unreal.log(status())


def _fail(job, error):
    _failed.append((job.line_id, str(error)))
    unreal.log_warning(f'Cinemafy lip-sync: {job.line_id} failed: {error}')


def _on_tick(_delta):
    """One step a tick: start the next line, or wait for the current solve and export it."""
    global _current
    if _current is None:
        if not _queue:
            _stop()
            return
        project, job = _queue.pop(0)
        try:
            _current = (project, job, _begin(job))
        except Exception as error:  # pylint: disable=broad-except
            _fail(job, error)
        return
    project, job, performance = _current
    if _is_processing(performance):
        return
    try:
        _finish(project, job, performance)
    except Exception as error:  # pylint: disable=broad-except
        _fail(job, error)
    _current = None


# --- One line -------------------------------------------------------------------------------------------------------

def _set(obj, names, value):
    """Sets the first property name that exists (MetaHuman's names vary by version)."""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:  # pylint: disable=broad-except
            continue
    return False


def _enum_value(type_names, member_names):
    for type_name in type_names:
        enum = getattr(unreal, type_name, None)
        if enum is None:
            continue
        for member in member_names:
            value = getattr(enum, member, None)
            if value is not None:
                return value
    return None


def _is_processing(performance):
    probe = getattr(performance, 'is_processing', None)
    if probe is None:
        return False
    try:
        return bool(probe())
    except Exception:  # pylint: disable=broad-except
        return False


def _begin(job):
    """Makes the Performance asset for a line and starts the audio solve."""
    sound = unreal.load_asset(job.audio_path)
    face = unreal.load_asset(job.face_mesh_path)
    if sound is None:
        raise RuntimeError(f'no audio at {job.audio_path}')
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory_class = getattr(unreal, 'MetaHumanPerformanceFactoryNew', None)
    name = job.asset_name + '_Performance'
    folder = job.package_path + '/Performances'
    existing = unreal.load_asset(f'{folder}/{name}.{name}')
    performance = existing or tools.create_asset(name, folder, unreal.MetaHumanPerformance,
                                                 factory_class() if factory_class else None)
    if performance is None:
        raise RuntimeError('could not make a MetaHuman Performance asset')
    audio_input = _enum_value(['DataInputType'], ['AUDIO', 'Audio'])
    if audio_input is None or not _set(performance, ['input_type'], audio_input):
        raise RuntimeError("this MetaHuman version has no audio input on its Performance asset")
    if not _set(performance, ['audio'], sound):
        raise RuntimeError("couldn't give the Performance its audio")
    if face is not None:
        _set(performance, ['visualization_mesh'], face)
    starter = getattr(performance, 'start_pipeline', None)
    if starter is None:
        raise RuntimeError('this MetaHuman version has no start_pipeline')
    try:
        starter(True)
    except TypeError:
        starter()
    return performance


def _finish(project, job, performance):
    """Exports the solved face animation and gives it to the line."""
    settings_class = getattr(unreal, 'MetaHumanPerformanceExportAnimationSettings', None)
    if settings_class is None:
        raise RuntimeError('this MetaHuman version has no animation export settings')
    settings = settings_class()
    _set(settings, ['show_export_dialog'], False)
    _set(settings, ['package_path'], job.package_path)
    _set(settings, ['asset_name'], job.asset_name)
    _set(settings, ['enable_head_movement'], False)
    _set(settings, ['auto_save_anim_sequence'], True)
    whole = _enum_value(['PerformanceExportRange'], ['WHOLE_SEQUENCE', 'WholeSequence'])
    if whole is not None:
        _set(settings, ['export_range'], whole)
    face = unreal.load_asset(job.face_mesh_path)
    if face is not None:
        _set(settings, ['target_skeleton_or_skeletal_mesh'], face)
    animation = unreal.MetaHumanPerformanceExportUtils.export_animation_sequence(performance, settings)
    if animation is None:
        animation = unreal.load_asset(f'{job.package_path}/{job.asset_name}.{job.asset_name}')
    if animation is None:
        raise RuntimeError('the solve finished but no animation was exported')
    if not unreal.CinemafyStoryLibrary.assign_lip_sync(project, job.line_id, animation):
        raise RuntimeError('the line is gone from the script')
    _done.append(job.line_id)
    unreal.log(f'Cinemafy lip-sync: {job.speaker} "{job.text[:40]}" -> {animation.get_path_name()}')
