# Contributing

## Development setup

1. Create a Python 3.11+ virtual environment under `core/.venv`.
2. Install the core with `python -m pip install -e ".[test,event,rgbd]"` from `core`.
3. Set `TRIAD_UNREAL_PROJECT` when running against a local Unreal checkout.
4. Keep Globe-C2 bound to `127.0.0.1` during development.

## Required checks

From the repository root:

```powershell
python -m unittest discover -s core\tests -p "test_*.py" -v
python -m unittest discover -s unreal\Plugins\TRIADSensorFusion\Tests -p "test_*.py" -v
Push-Location integrations\globe-c2
python -m unittest bridge.test_triad_bridge -v
Pop-Location
```

Changes to `TRIADLongRangeSensorModel` also require the Unreal automation test documented in `unreal/README.md`.

## Invariants that changes must preserve

- Authored hostile/friendly scenario truth is evaluator-only and never creates, promotes, suppresses, positions, or classifies an operator track.
- Fusion accepts only fresh, target-associated observations with explicit provenance.
- Wideband RF, active radar, and visual are the only independent families. Two modalities in one family cannot satisfy a multi-family gate.
- Scores from separate observations are not added or multiplied. The strongest discounted evidence remains the numeric score.
- RGB-derived event proxies are diagnostic candidates and cannot count as an independent event-camera confirmation.
- Unreal projection boxes are debug/simulation truth, not learned-model detections.
- Missing, stale, unsafe, or ambiguous data fails closed.
- Outputs remain clearly labelled simulation-only and detection-only.

Add or update tests whenever an input contract, evidence gate, decision tier, safety field, or C2 payload changes.

## Data and model hygiene

Do not commit Unreal `Saved`, frames, telemetry sessions, caches, build output, secrets, local databases, virtual environments, or model binaries. Use small synthetic fixtures in tests. Before adding a model artifact, document its license, source, hash, preprocessing contract, and allowed redistribution path.
