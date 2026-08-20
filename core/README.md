# Singapore Sensor Fusion Simulation Core

This package contains simulation-only sensing primitives for the Singapore
Cesium/Unreal environment. It intentionally contains no engagement, targeting,
or weapon-control logic.

The core provides:

- validated JSON/TOML configuration for sensor nodes and simulated drone RF
  emitters;
- WGS84 geodetic, ECEF, and local ENU coordinate conversion;
- free-space and log-distance path loss, `kTB` receiver noise, linear RF power
  summation, a pluggable LOS/NLOS loss hook, and reproducible smooth shadowing;
- deterministic synthetic complex IQ plus Welch PSD primitives and a model-free
  configurable 169.4 MHz-6.2 GHz channelized radiometer;
- a false-alarm-calibrated **analytic fallback** energy-detector estimate;
- a bounded RGB-to-event proxy and hash-pinned Event-YOLO adapter;
- a hash-pinned OAK YOLO RGB adapter with post-detection Unreal depth-Z/slant
  ranging and two-of-three-frame persistence;
- read-only readiness checks for the supplied RGB-D and event artifacts; the
  supplied RF checkpoints are excluded from the default manifest and runtime;
- timezone-aware observation records, conservative dependence-safe fusion, and
  a one-shot Unreal export experiment report.
- an auditable fusion-v2 track API that separates raw detector confidence,
  declared source reliability, and weather-adjusted evidence; includes explicit
  2.4/5.8 GHz, RGB-D, and neuromorphic contribution records; and never stacks
  corroborating scores without a validated dependence model;
- deterministic clear, light-rain, monsoon/heavy-rain, and haze/fog scenario
  profiles whose reliability values are prominently labelled as simulation
  assumptions rather than measured accuracy.
- a three-independent-family layered runtime: passive RF; active radar
  containing mmWave plus long-range search radar; and one visual family
  containing RGB, event camera, radar-cued EO PTZ, and synthetic thermal PTZ.
  Modalities remain separately visible, but co-family evidence cannot
  corroborate itself. Accepted search-radar polar geometry provides the v3 map
  position while kinematics and uncertainty remain uncalibrated simulation
  surrogates; approach state is measured against an explicit simulation perimeter.

## Important model boundary

The two supplied RF checkpoints are not opened, hashed by the default model
doctor, or used by the layered runtime. Wideband RF uses a declared link budget,
per-cell false-alarm setting, temporal occupancy gate, and analytic radiometer
estimate. Its output is a simulation evidence index, not a learned-model score
or a calibrated field probability.

Likewise, `proxy_event_stack_v1` is derived from adjacent RGB frames and is not
equivalent to the unavailable custom event-stack projector used to train the
supplied Event-YOLO checkpoint. Its detections are reported as candidates, not
as calibrated true positives. The incompatible TensorRT engine is never
deserialized on this host.

## Quick start

```powershell
cd <CLONE>\core
py -m pip install -e .
py -m unittest discover -s tests -v
py -m singapore_sensor_fusion.model_doctor --json
```

Build a single layered snapshot for Globe-C2:

```powershell
py -m singapore_sensor_fusion.layered_runtime --once
```

Or keep the snapshot current while Unreal is running:

```powershell
py -m singapore_sensor_fusion.layered_runtime --interval 0.25
```

The output is
`<TRIAD_PROJECT>\Saved\SingaporeSensorFusion\latest_layered_snapshot.json`.
An outside target observed by at least two RF nodes is shown as a preliminary
approach cue. It becomes a corroborated detection only when a second sensor
family agrees. Numeric confidence scores are never added across modalities.
Exact scene coordinates and hostile/friendly scenario labels remain in an
evaluator-only object; the operational track uses a deterministic noisy estimate
and never infers hostility from scenario truth.

New visual evidence is admitted only when its Unreal metadata contains
`debugVisualsExcludedFromSensorCapture: true`. Legacy frames containing magenta
labels or other operator graphics are rejected. A box shown in C2 therefore
means a model detector box on a debug-clean sensor frame; projection truth and
human-only markers are not detector boxes.

### Long-range radar-cued PTZ path

Unreal snapshot v3 additionally accepts `searchRadarDetections[]` and
`ptzConfirmations[]`. A PTZ record contributes only when it is fresh, settled,
confirmed, line-of-sight, linked to a fresh accepted radar cue, within its
configured envelope, and points at an allow-listed `RadarPtzFrames/...` path.
Its box is published as `SIMULATED_SENSOR_CONFIRMATION` from
`SIMULATION_PROJECTION`, never as learned-model output. The EO projection box
and radar-cued range form one evidence record and are never double-counted.

Run the deterministic eight-site, multi-range radar/PTZ contract audit with:

```powershell
py -m singapore_sensor_fusion.radar_ptz_audit
```

It covers 25, 100, 250, 500, and 1,000 m under clear conditions, weather at
500 m, a low-score false-alarm case, stale cue rejection, and PTZ occlusion.
Reports are written to `reports/radar_ptz_multirange_audit.json` and `.md`.
Passing this audit proves the software path and deterministic simulation
fixtures only; it is not physical range or probability-of-detection evidence.

Run the deterministic range, weather, speed, failure, and 1/8/24/64/100-target
scale audit with:

```powershell
py -m singapore_sensor_fusion.layered_audit
```

It writes `layered_sensor_audit.json` and `.md` into the shared `outputs`
directory. The audit distinguishes configured mathematical coverage and modeled
latency from field calibration; its visual fallback cases are post-association
fixtures and do not validate either visual model.

Run a bounded experiment over the newest Unreal export:

```powershell
py -m singapore_sensor_fusion.experiment `
  --saved-dir "<TRIAD_PROJECT>\Saved\SingaporeSensorFusion" `
  --output experiment_report.json `
  --max-records 10000 `
  --max-event-pairs 8
```

Run the supplied OAK detector over the latest Unreal RGB/depth session:

```powershell
py -m singapore_sensor_fusion.oak_rgbd `
  --frames-root "<TRIAD_PROJECT>\Saved\SingaporeSensorFusion\frames" `
  --node City_Sector `
  --checkpoint "<CLONE>\core\models\oak_drone_yolo11s_960_best.pt" `
  --trusted-sha256 14DD5B34C4A7CCC69E268F417CE6C3A410085E06B6CCDEE9F52597855D8AF4C5 `
  --output oak_rgbd_report.json `
  --max-frames 24
```

Use `python -m singapore_sensor_fusion.oak_rgbd_viewer watch ...` for a live
OpenCV sensor window, or replace `watch` with `video` to publish an annotated
MP4.  The learned network consumes RGB only; aligned depth is applied after a
detection. New Unreal captures prefer a lossless little-endian uint32 SceneDepth
sidecar in millimetres and retain the normalized 8-bit PNG as a debug/fallback
preview. Zero and the clamped `0xFFFFFFFF` sentinel are excluded; 0.001 m
quantization is retained across the simulation's 10 km configured range. Both
sources are simulation geometry and must not be presented as calibrated
OAK-camera range.

Write the deterministic fusion-v2 weather stress-test fixture used by the C3
dashboard:

```powershell
py -m singapore_sensor_fusion.weather_sweep `
  --output fusion_v2_weather_scenarios.json
```

Fusion-v2 requires upstream target association and refuses to mix different
track IDs. Its `fused_evidence_score` is an auditable evidence index, not a
calibrated probability. Corroborating modalities can satisfy the visible alert
policy but do not numerically inflate the score.

Serve the newest Unreal RF links, multi-node alerts, and City/South RGB frames
to the local C3 dashboard:

```powershell
python src\singapore_sensor_fusion\c3_bridge.py `
  --saved-dir "<TRIAD_PROJECT>\Saved\SingaporeSensorFusion" `
  --host 127.0.0.1 `
  --port 8765
```

The bridge is read-only and exposes `GET /api/snapshot`, `GET /health`, and
bounded `GET /frames/City_Sector` or `GET /frames/South_Sector` endpoints. It
reports source age and visual-weather verification explicitly. When a fresh
`latest_layered_snapshot.json` is available, it also projects long-range sensor
health, fusion-v3 decisions, modality evidence, and up to four radar-cued PTZ
confirmation frames per track. It never converts projection boxes into learned
model confidence; the dashboard keeps RF, simulated radar/PTZ, and learned
model provenance distinct.

```python
from singapore_sensor_fusion import (
    BAND_2_4_GHZ,
    analytic_energy_detector_probability,
    generate_synthetic_iq,
    welch_psd,
)

frame = generate_synthetic_iq(
    center_frequency_hz=BAND_2_4_GHZ,
    sample_rate_hz=20e6,
    num_samples=4096,
    signal_power_dbm=-88.0,
    noise_power_dbm=-96.0,
    seed=7,
)
psd = welch_psd(frame, bins=256)
estimate = analytic_energy_detector_probability(
    snr_db=8.0,
    num_complex_samples=4096,
    false_alarm_probability=1e-3,
)
```
