# Unreal integration

## What to use in Unreal Engine

Use the C++ runtime plugin at `Plugins/TRIADSensorFusion` and the scenario file at `Config/SingaporeSensorFusion.json`. No sensor-fusion Blueprint asset is required: the plugin dynamically creates its scenario manager, sensor nodes, demo targets, cameras, radar/PTZ state, and telemetry writers.

Blueprint projects can extend actors with:

- `UTRIADRFEmitterComponent` for an RF-emitting target;
- `UTRIADSensorNodeComponent` for sensor-node configuration.

The host map must already contain a `CesiumGeoreference` actor.

## Prerequisites

- Unreal Engine 5.5
- Cesium for Unreal
- a sibling `Plugins/AirSimTriadRuntime` plugin

`AirSimTriadRuntime` currently provides the weather API, weather content, and default quadcopter mesh. Do not enable the original `AirSim` plugin at the same time.

## Install into a host project

1. Copy `Plugins/TRIADSensorFusion` into `<PROJECT>/Plugins/TRIADSensorFusion`.
2. Copy `Config/SingaporeSensorFusion.json` into `<PROJECT>/Config/SingaporeSensorFusion.json`.
3. Enable `TRIADSensorFusion`, `CesiumForUnreal`, and `AirSimTriadRuntime` in the `.uproject`.
4. Close the editor and build the editor target.
5. Reopen the map containing `CesiumGeoreference` and press Play.

The world subsystem activates only for PIE/Game and only when the scenario JSON exists with `bEnabled: true`.

## Contract tests

The Python tests inspect the plugin source and configuration contracts:

```powershell
python -m unittest discover -s Plugins\TRIADSensorFusion\Tests -p "test_*.py" -v
```

Run the C++ automation test after building the host editor target:

```powershell
& "<UE5_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "<PROJECT>\TRIAD.uproject" `
  -unattended -nop4 -NullRHI `
  -ExecCmds="Automation RunTests TRIAD.SensorFusion.LongRange.SearchRadar.CloseThrough500Meters;Quit" `
  -TestExit="Automation Test Queue Empty" -log
```

See the plugin's own `README.md` and `Docs/LiveSnapshotV3RadarPtzContract.md` for the detailed telemetry and PTZ contracts.
