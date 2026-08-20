# TRIAD live snapshot v3: search radar and radar-cued PTZ contract

`Saved/SingaporeSensorFusion/latest_rf_snapshot.json` now reports
`schemaVersion: triad.live_rf_snapshot.v3`. All v2 root fields and RF/track
semantics remain present. The two arrays below are additive and bounded; a v2
consumer may ignore them.

## `searchRadarDetections[]`

Each object is positive current-sample evidence from an explicitly simulated,
geometry-only analytic radar. It does not require or inspect an RF emitter.

| Field | Type / semantics |
|---|---|
| `timestampUtc`, `simulationSeconds` | Observation time. |
| `kind`, `source`, `simulated`, `calibratedDetector` | `SIMULATED_SENSOR_DETECTION`, `ANALYTIC_SEARCH_RADAR`, `true`, `false`. |
| `nodeId`, `sensorId`, `sensorType` | Site, `<node>:SEARCH_RADAR`, `SEARCH_RADAR`. |
| `id`, `trackId`, `targetActor` | Stable node observation id, opaque radar track, simulation-only actor cross-reference. |
| `rangeMeters`, `slantRangeMeters` | Noisy 3D sensor-to-target slant range. |
| `bearingDegrees`, `azimuthDegrees`, `elevationDegrees` | Noisy local bearing clockwise from north and elevation. |
| `radialVelocityMetersPerSecond` | Noisy range-rate estimate derived from target motion between samples. |
| `confidence` | Uncalibrated analytic score from range, LOS, authored RCS, and weather. |
| `radarCrossSectionSquareMeters` | Authored simulation input; never represented as measured RCS. |
| `lineOfSight`, `blockingActor` | Unreal visibility trace result. NLOS degrades rather than necessarily removes radar evidence. |
| `weatherProfile`, `weatherVisibilityMeters`, `weatherRainRateMillimetersPerHour` | Environment inputs used by the score. |
| `rangeEnvelopeMeters` | At least 5,000 m; 8,000 m for every active node. |
| `azimuthFieldOfRegardDegrees`, `elevationFieldOfRegardDegrees` | Configured simulated field of regard. |

## `ptzConfirmations[]`

Each enabled node exposes EO and synthetic-thermal records after a radar cue.
Records can be lightweight `SLEWING`, `SETTLING`, or `SETTLED` status without a
frame. A frame is captured only after settling and only inside that modality's
confirmation envelope.

| Field | Type / semantics |
|---|---|
| `kind`, `source`, `confirmationMethod`, `boxSource` | `SIMULATED_SENSOR_CONFIRMATION`, `SIMULATION_PROJECTION`, `SIMULATION_PROJECTION_TRUTH`, `DEBUG_PROJECTION`. Never learned-model output. |
| `simulated`, `calibratedDetector` | Always `true`, `false`. |
| `id`, `nodeId`, `sensorId`, `sensorType`, `modality`, `label` | Stable C2 identification. Sensor types are `EO_PTZ` / `THERMAL_PTZ`; exact modality values are `EO_VISIBLE` / `THERMAL_SYNTHETIC`, with an explicit SIM EO / SYNTHETIC THERMAL label. |
| `trackId`, `targetActor`, `radarCueSensorId`, `cueAgeSeconds` | Radar-to-PTZ linkage and cue age. |
| `slewState`, `confirmed`, `hasFrame` | Mechanical simulation/status and positive confirmation/frame availability. |
| `rangeMeters`, `confidence` | Radar range and projection/LOS/weather-informed simulated confirmation score. |
| `azimuthDeg` / `bearingDegrees`, `elevationDeg` / `elevationDegrees` | Cued sensor orientation. |
| `horizontalFovDeg` / `fovDegrees` | Narrow configured horizontal FOV. |
| `width`, `height`, `imageWidthPixels`, `imageHeightPixels` | Frame dimensions or configured dimensions for lightweight status. |
| `pixelExtentWidth`, `pixelExtentHeight`, `boundingBoxPixels` | Projected physical-primitive extent; empty/zero without a frame intersection. |
| `reticle` | Image-centre radar cue reticle in pixel coordinates. |
| `boxes[]` | `xyxyPixels` and `xyxyNormalized`, explicitly labelled `SIMULATED SENSOR CONFIRMATION`. |
| `lineOfSight`, `blockingActor`, `occlusionSemantics` | Fail-closed PTZ trace from capture origin to model-bounds centre/corners. Confirmation requires LOS. |
| `weatherProfile`, `weatherVisibilityMeters`, `weatherRainRateMillimetersPerHour`, `weatherConfidenceFactor`, `weatherConfidenceSemantics` | Exact environment context explaining simulated confidence changes. |
| `frameRelativePath` / `relativePath`, `metadataRelativePath` | Empty without a frame; otherwise allowlisted paths below `RadarPtzFrames/eo/` or `RadarPtzFrames/thermal/`. |
| `syntheticThermal`, `thermalSemantics` | Makes the false-colour proxy unambiguous. |

## Stable node capability records

Every `sensorNodes[]` item retains its v2 fields and adds configured/range/status
fields for `searchRadar`, `eoPtz`, and `thermalPtz`, plus
`longRangeSensorStack[]`. These health records remain present even when there is
no positive detection.

## Latest-frame paths

Paths are relative to `Saved/SingaporeSensorFusion`, use forward slashes, reject
`..`, and are atomically replaced:

```text
RadarPtzFrames/eo/<node>/latest.png
RadarPtzFrames/eo/<node>/latest.json
RadarPtzFrames/thermal/<node>/latest.png
RadarPtzFrames/thermal/<node>/latest.json
```

Only a currently confirmed capture may publish or expose these paths.
Unconfirmed, occluded, or off-boresight status records have `hasFrame: false`
and empty frame/metadata paths; they do not overwrite the retained last
positive-confirmation files. Consumers must follow the current snapshot paths
rather than inferring a current observation from a file that remains on disk.

EO is raw Unreal visible SceneCapture imagery with operator/debug aids excluded.
Thermal is a synthetic false-colour postprocess and contains a burned-in
`SIM THERMAL SYNTHETIC` banner.

## Acceptance geometry and tests

The four-drone `RADAR_PTZ_ACCEPTANCE_INBOUND` route starts outside the east
simulation perimeter, emits 2.437/5.795 GHz, reaches 500 m from East_Sector in
about 2.6--2.8 minutes, and passes at approximately 158--179 m slant. A separate
one-drone `RADAR_PTZ_CLOSE_RANGE_ACCEPTANCE` fixture begins inside the east
sector, travels at 8 m/s with an approximately 25 m cross-track offset, reaches
500 m from East_Sector in about 35 seconds, and is used only for repeatable
short-range validation. Its visual mesh is uniformly normalized and rolled to an
approximately 100 x 84 x 19 cm horizontal body. The separate 0.5 x 0.2 m analytic
pinhole acceptance fixture is intentionally conservative rather than an exact
rendered-mesh replica.
Total scenario population is
29: 28 outside-in contacts plus this close-pass fixture.

Run the engine-independent contract suite while the editor remains open:

```powershell
python -m unittest discover -s Plugins/TRIADSensorFusion/Tests -p "test_*.py" -v
```

After a coordinated Unreal build, run automation test
`TRIAD.SensorFusion.LongRange.SearchRadar.CloseThrough500Meters` for exact
25/100/250/500 m math with measurement noise disabled.
