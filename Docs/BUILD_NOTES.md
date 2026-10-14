# Build notes: engine APIs most likely to drift

Cinemafy is written against UE 5.8. Everything up to `448b540` (v0.7 plus the Mac fixes) has compiled cleanly. Later work
was written without a compiler. These are the calls most likely to need a small fix on a newer
(or older) engine, with the fallback if one breaks.

| Where | Call | If it doesn't compile |
|---|---|---|
| `AnimNode_CinemafyFace.cpp` | `FBlendedCurve::ForEachElement`, `Set(FName, float)` | 5.3+ name-based curve API. On older engines, use `UE::Anim::FCurveUtils` / `FBlendedCurve::Get(FName)`. |
| `CinemafyEditorLibrary.cpp` | `USoundWave::GetImportedSoundWaveData(TArray<uint8>&, uint32&, uint16&)` | Editor-only. Fallback: read `Sound->RawData` and parse it with `FWaveModInfo::ReadWaveInfo`. |
| `CinemafyEditorLibrary.cpp` | `UMovieScene::GetTracks()` / `AddTrack<T>()` (root tracks) | Pre-5.2 names: `GetMasterTracks()` / `AddMasterTrack<T>()`. |
| `CinemafyEditorLibrary.cpp` | `FMovieSceneFloatChannel::AddLinearKey` | Use `Channel->GetData().AddKey(Time, FMovieSceneFloatValue(Value))` and set the interp mode. |
| `CinemafyEditorLibrary.cpp` | `UMovieSceneAudioTrack::AddNewSoundOnRow` | Older: `AddNewSound(Sound, Time)`. |
| `CinemafyChoreographyLibrary.cpp` | `USkeleton::IsCompatibleForEditor(const FString&)` | Editor-only. Fallback: compare the `Skeleton` asset tag to `Skeleton->GetCompatibleSkeletons()` paths, or drop to exact-match only. |
| `CinemafyChoreoTools.cpp` | `FAnimationRecorderManager::RecordAnimation(Comp, Path, Name, Settings)` / `StopRecordingAnimation(Comp, bShowMessage)` | Older overload takes `UAnimSequence*`: create it with `AssetTools.CreateAsset` first. |
| `CinemafyChoreoTools.cpp` | `FAnimationRecordingSettings::SampleFrameRate` | Pre-5.1: `SampleRate` (float). |
| `CinemafyChoreoTools.cpp` | `FMovieSceneDoubleChannel::AddLinearKey` | Use `GetData().AddKey(Time, FMovieSceneDoubleValue(Value))`. |
| `CinemafyChoreoTools.cpp` | `FViewportCursorLocation`, `Viewport->GetHitProxy`, `IInputProcessor` | Stable since 4.x. If the click lands in the wrong spot, check DPI scaling in `HandleMouseButtonDownEvent`. |
| `CinemafyChoreography.cpp` | `UAnimMontage::GetDefaultBlendOutTime()` | Older: `Montage->BlendOut.GetBlendTime()`. |
| `CinemafyEditorModule.cpp` | `ISequencerModule::GetCommandBindings(ESequencerCommandBindings::Sequencer)` | If missing, delete the binding block; the panel and toolbar dropdown still work. |
| `CinemafyEditorModule.cpp` | ToolMenus `"Sequencer.MainToolBar"` | If the dropdown doesn't appear, the toolbar has another menu name. It's harmless; use the panel. |
| `CinemafyEditLibrary.cpp` | `UMovieSceneSection::SplitSection` / `TrimSection(FQualifiedFrameTime, bool, bool)` | Older signatures take `FFrameNumber`. |
| `CinemafyEditLibrary.cpp` | `FMovieSceneChannel::GetKeys` / `SetKeyTimes` / `DeleteKeys` (generic key shifting) | Stable since 4.22. |
| `CinemafyEditLibrary.cpp` | `UMovieSceneAudioSection::GetSoundVolumeChannel()` | Fallback: `GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0)`. |
| `CinemafyEditLibrary.cpp` | `UMovieScene::SetMarkedFrame` / `DeleteMarkedFrame` / `AddMarkedFrame` | 4.26+. |
| `SCinemafyTransition.cpp` | `FSlateClippingZone(TopLeft, TopRight, BottomLeft, BottomRight)` with `FVector2f` | Older engines take `FVector2D`. |
| `CinemafyChoreoTools.cpp` | `UMovieSceneTrack::IsEvalDisabled` / `SetEvalDisabled` | Editor-only data. Fallback: remove the mute and let the user disable the old track. |
| `CinemafyChoreography.cpp` | `UMotionWarpingComponent::RemoveWarpTarget(FName)` | Older: `RemoveWarpTargets`, or skip (the targets are harmless). |
| `SCinemafyShotBrowser.cpp` / Learn Style | `ISequencer::FindBoundObjects` | Compiled fine in 5.8 (Shot Browser). If removed later, resolve through the sequence's shared playback state. |

Behavioral things to check in-engine (not compile issues):
- **Face emotion and pupil curve names:** `eyePupilNarrow`, `jawClench` and `mouthLowerLipBite` are the least
  certain MetaHuman control names. Fix them in the Face Profile (lip-sync curves) or as custom emotions.
- **Shortcut conflicts:** if a Cinemafy Edit shortcut clashes with one of yours, rebind it under Editor
  Preferences > Keyboard Shortcuts > Cinemafy Edit.
- **Overlay transitions and titles in Movie Render Queue:** these draw as Slate over the game view, so
  enable MRQ's UI renderer (or use native fades) for final renders.
- **Choreography traversal:** GASP's `TryTraversalAction` reads movement input and speed. If vaults don't
  trigger, raise `TraversalRange` or set `TraversalFunction` explicitly. The Wiring Report in the
  Details panel lists what was found.
- **Recorded takes and MetaHumans:** the Face mesh is skipped on purpose (Cinemafy Face drives faces).
  Body, hidden mannequin and retargeted body meshes are all recorded.
- **Listener `SpeechThreshold`:** tune it while watching `HeardEnergy` on real performances.
