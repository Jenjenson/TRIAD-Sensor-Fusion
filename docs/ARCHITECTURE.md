# Architecture

## Components

### Unreal producer

`unreal/Plugins/TRIADSensorFusion` runs in UE 5.5 Play in Editor or Game worlds. It creates Cesium-anchored sensor nodes, discovers or spawns simulated drone actors, evaluates RF links and Unreal line of sight, simulates search-radar measurements, cues EO/thermal PTZ captures, and writes bounded telemetry.

Its live handoff is `Saved/SingaporeSensorFusion/latest_rf_snapshot.json`, schema `triad.live_rf_snapshot.v3`. This producer snapshot contains sensor-node status, detected RF links, simulated search-radar returns, PTZ confirmations, track geometry, and evaluator-only scenario data.

### Python fusion core

`core/src/singapore_sensor_fusion/layered_runtime.py` validates the Unreal snapshot and model reports, associates observations to a track, applies freshness and provenance gates, and writes `latest_layered_snapshot.json`, schema `triad.layered_detection_snapshot.v1`.

Key modules:

| Area | Modules |
| --- | --- |
| Coordinates and records | `geodesy.py`, `observations.py`, `config.py` |
| Passive RF | `rf.py`, `synthetic.py`, `wideband_rf.py` |
| Active radar | `mmwave.py`, Unreal `TRIADLongRangeSensorModel` |
| Visual evidence | `oak_rgbd.py`, `event_camera.py`, `event_yolo.py` |
| Fusion policies | `fusion_v3.py` (active layered runtime); `fusion.py` and `fusion_v2.py` (legacy/auxiliary experiments) |
| Runtime and validation | `layered_runtime.py`, `validation.py`, audit modules |

### Globe-C2 adapter

`integrations/globe-c2/bridge/triad_bridge.py` validates a fresh layered snapshot, pseudonymizes scenario-bearing identifiers, maps sensors/tracks to the C2 REST contract, retries transient failures, and removes detections that are no longer active. It never regenerates observations or uses scenario truth as evidence.

## Runtime order

On the Unreal side, `UTRIADSensorFusionWorldSubsystem` starts only in PIE/Game when the scenario config loads with `bEnabled: true`. The scenario manager finds the existing Cesium georeference, initializes bounded telemetry, spawns the configured sensor nodes/demo targets, applies weather, and samples at the configured cadence. Search radar is sampled before the RF-emitter gate, RGB/depth tracking follows a positive same-node RF cue, and PTZ capture follows a current radar cue through slewing, settling, and settled states. The complete live snapshot is replaced atomically.

The Python runtime then:

1. Requires a complete supported Unreal schema with simulation-only, detection-only safety fields.
2. Validates RF, search-radar, PTZ, and optional visual-model observations fail closed.
3. Seeds contacts only from accepted current observations, never `scenarioTargets`.
4. Builds target-associated `LayeredEvidence` and fuses one track at a time.
5. Derives operational position only from accepted measurement geometry.
6. Writes the layered snapshot atomically.

## Fusion-v3 policy

The fusion layer groups modalities into three independent families:

| Family | Modalities | Current live behavior |
| --- | --- | --- |
| `passive_rf` | wideband RF | Consumes validated Unreal RF links. Multiple nodes can form only a same-family preliminary cue. |
| `active_radar` | mmWave, `SEARCH_RADAR` | Search radar is live. mmWave math/audits exist, but live mmWave stays offline until an observation-authoritative stream is supplied. |
| `visual` | RGB, event camera, `EO_PTZ`, `THERMAL_PTZ` | Radar-cued PTZ can contribute. Current projection-IoU RGB/event associations are diagnostic and not fusion-eligible. |

For each target track, the runtime:

1. Rejects evidence outside the configured age window.
2. Keeps the strongest observation in each correlation group.
3. Keeps the strongest surviving observation per modality.
4. Marks a modality active only when its discounted score reaches the corroboration threshold.
5. Uses the strongest active evidence as the displayed numeric score.
6. Counts distinct families only for policy gates; it never numerically stacks family scores.

The default detection threshold is `0.55`, the corroboration threshold is `0.35`, and the maximum evidence age is two seconds. Default decision tiers are:

- `PRELIMINARY`: the dominant RF evidence reaches the detection threshold, at least two independent RF receiver groups agree, and no second family is active;
- `CORROBORATED`: the dominant evidence reaches threshold and at least two independent families are active;
- `CONFIRMED`: all three independent families are active;
- `UNCONFIRMED`: current evidence does not satisfy one of those gates.

EO and thermal remain separately visible modalities but are one visual family. One radar-cued PTZ confirmation is one evidence item; its projection box and radar range are not counted separately.

Source reliability and weather multipliers are declared simulation assumptions, not measured sensor accuracy. Unreal RF/radar/PTZ records already include their modeled environmental effects and are not weather-discounted a second time.

## Evidence admission gates

- RF requires an enabled online node, a fresh positive detection, supported frequency, configured range/sensitivity, finite internally consistent power/noise/SNR values, explicit LOS state, and deduplication.
- Search radar requires exact simulated provenance, a configured online node, fresh same-sample geometry, valid range/field-of-regard/kinematics/confidence, and no expansion beyond the configured envelope.
- PTZ requires exact simulation-projection provenance, settled/confirmed/LOS state, a fresh same-target radar cue, a current frame within the modality range, a valid box, and an allow-listed `RadarPtzFrames/...` relative path. Thermal must remain explicitly synthetic.
- RGB/event frames require debug visuals to be excluded and model artifacts to pass their hash contract. RGB-derived event proxies and projection-truth associations remain diagnostic-only.

## Truth and provenance boundary

`scenarioTargets`, actor names, ingress labels, and hostile/friendly flags are synthetic evaluator inputs. They may appear in evaluator-only output but must never:

- seed an operator track;
- provide a fallback track position;
- regenerate RF/radar/visual evidence;
- change a fusion tier;
- infer hostility or trigger an action.

Operational map position requires accepted measurement-derived localization, currently supplied by validated search-radar polar geometry. RF-only cues without localization stay unlocated.

Visual detections are accepted only from debug-clean Unreal frames marked `debugVisualsExcludedFromSensorCapture: true`. Learned boxes must declare `kind: MODEL_DETECTION` and `source: MODEL_OUTPUT`. Radar/PTZ projection confirmations retain `SIMULATED_SENSOR_CONFIRMATION` / `SIMULATION_PROJECTION` provenance and cannot masquerade as learned detections.

## Failure behavior

The runtime and bridge fail closed when schemas, safety flags, timestamps, localization provenance, PTZ paths, frame metadata, or modality health are invalid. Stale snapshots cause existing C2 cues to be cleared rather than held indefinitely.

All paths received from snapshots are allow-listed and bounded. Generated telemetry, image sequences, and model artifacts remain outside version control.
