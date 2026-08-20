# TRIAD Sensor Fusion

Simulation-only sensor fusion for the TRIAD Unreal/Cesium environment, packaged for collaborative development. This repository contains the complete Python fusion core, the Unreal Engine producer plugin and scenario configuration, and the adapter that publishes fused tracks into Globe-C2.

It contains no engagement, targeting, or weapon-control logic. Scores are simulation evidence indexes, not calibrated real-world probabilities.

## Repository layout

```text
core/                         Python sensing, association, fusion, audits, and tests
unreal/                       UE 5.5 TRIADSensorFusion plugin and active scenario config
integrations/globe-c2/        Layered-snapshot-to-C2 adapter and contract tests
docs/ARCHITECTURE.md          Data flow, fusion rules, provenance, and safety boundaries
```

Generated Unreal frames, telemetry, caches, build products, virtual environments, credentials, and model binaries are intentionally excluded.

## Runtime flow

```text
TRIAD Unreal plugin
  -> latest_rf_snapshot.json
  -> singapore_sensor_fusion.layered_runtime
  -> latest_layered_snapshot.json
  -> integrations/globe-c2/bridge/triad_bridge.py
  -> Globe-C2 REST API and dashboard
```

The fusion policy has three independent evidence families:

- passive RF: wideband RF;
- active radar: mmWave and simulated search radar;
- visual: RGB, event camera, radar-cued EO PTZ, and synthetic thermal PTZ.

Evidence inside one family cannot corroborate itself. The displayed score is the strongest reliability- and weather-discounted evidence item; modality scores are never added or multiplied.

## Quick start: Python core

Requires Python 3.11 or newer.

```powershell
cd core
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e ".[test]"
python -m unittest discover -s tests -p "test_*.py" -v
```

Optional visual tooling:

```powershell
python -m pip install -e ".[test,event,rgbd]"
```

## Connect it to TRIAD Unreal

The Unreal plugin requires:

- Unreal Engine 5.5;
- Cesium for Unreal;
- the existing sibling `AirSimTriadRuntime` plugin;
- a map containing an existing `CesiumGeoreference` actor.

Copy `unreal/Plugins/TRIADSensorFusion` into the host project's `Plugins` directory and `unreal/Config/SingaporeSensorFusion.json` into its `Config` directory. Enable `TRIADSensorFusion`, `CesiumForUnreal`, and `AirSimTriadRuntime`, then build the editor target before opening Play in Editor.

Set the host project once per shell:

```powershell
$env:TRIAD_UNREAL_PROJECT = "D:\triad\TRIAD"
```

With Unreal running in Play mode, start the layered runtime:

```powershell
cd core
.\.venv\Scripts\python.exe -m singapore_sensor_fusion.layered_runtime --interval 0.25
```

You can also pass `--input` and `--output` explicitly. `TRIAD_SCENARIO_CONFIG`, `TRIAD_FUSION_OUTPUT_DIR`, and `TRIAD_MODEL_ROOT` override the portable configuration, audit-output, and model defaults.

## Publish tracks to Globe-C2

Start Globe-C2 on loopback only, then run:

```powershell
py integrations\globe-c2\bridge\triad_bridge.py `
  --snapshot "D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_layered_snapshot.json" `
  --c2 http://127.0.0.1:3000 `
  --api-profile globe
```

The bridge rejects stale or unsafe snapshots and removes stale C2 cues. The current Globe-C2 mutation endpoints have no authentication, so do not expose them beyond `127.0.0.1`.

## Verification

Run all source-level suites from the repository root:

```powershell
python -m pip install -e ".\core[test]"
python -m unittest discover -s core\tests -p "test_*.py" -v
python -m unittest discover -s unreal\Plugins\TRIADSensorFusion\Tests -p "test_*.py" -v
Push-Location integrations\globe-c2
python -m unittest bridge.test_triad_bridge -v
Pop-Location
```

The C++ search-radar automation test additionally runs inside a built Unreal editor target. See [unreal/README.md](unreal/README.md).

## Important exclusions

- `AirSimTriadRuntime` is an external prerequisite and is not vendored here.
- The OAK YOLO checkpoint is not committed. Its manifest and trusted SHA-256 are included under `core/models`; redistribution requires a license decision.
- `Saved/SingaporeSensorFusion` is runtime data and can grow to tens of gigabytes.
- No repository-wide license has been selected yet. See [LICENSE-NOTICE.md](LICENSE-NOTICE.md).

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) before changing fusion decisions, provenance rules, or C2 mapping behavior.
