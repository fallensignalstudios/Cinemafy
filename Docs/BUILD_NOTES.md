# Build notes: engine APIs most likely to drift

Cinemafy is written against UE 5.8. Everything up to `56dcd81` has compiled cleanly. Later work
was written without a compiler. These are the calls most likely to need a small fix on a newer
(or older) engine, with the fallback if one breaks.

| Where | Call | If it doesn't compile |
|---|---|---|
| `AnimNode_CinemafyFace.cpp` | `FBlendedCurve::ForEachElement`, `Set(FName, float)` | 5.3+ name-based curve API. On older engines, use `UE::Anim::FCurveUtils` / `FBlendedCurve::Get(FName)`. |
| `CinemafyEditorLibrary.cpp` | `USoundWave::GetImportedSoundWaveData(TArray<uint8>&, uint32&, uint16&)` | Editor-only. Fallback: read `Sound->RawData` and parse it with `FWaveModInfo::ReadWaveInfo`. |
| `CinemafyEditorLibrary.cpp` | `UMovieScene::GetTracks()` / `AddTrack<T>()` (root tracks) | Pre-5.2 names: `GetMasterTracks()` / `AddMasterTrack<T>()`. |
| `CinemafyEditorLibrary.cpp` | `FMovieSceneFloatChannel::AddLinearKey` | Use `Channel->GetData().AddKey(Time, FMovieSceneFloatValue(Value))` and set the interp mode. |
| `CinemafyEditorLibrary.cpp` | `UMovieSceneAudioTrack::AddNewSoundOnRow` | Older: `AddNewSound(Sound, Time)`. |
| `SCinemafyShotBrowser.cpp` / Learn Style | `ISequencer::FindBoundObjects` | Compiled fine in 5.8 (Shot Browser). If removed later, resolve through the sequence's shared playback state. |

Behavioral things to check in-engine (not compile issues):
- **Face emotion and pupil curve names:** `eyePupilNarrow`, `jawClench` and `mouthLowerLipBite` are the least
  certain MetaHuman control names. Fix them in the Face Profile (lip-sync curves) or as custom emotions.
- **Listener `SpeechThreshold`:** tune it while watching `HeardEnergy` on real performances.
