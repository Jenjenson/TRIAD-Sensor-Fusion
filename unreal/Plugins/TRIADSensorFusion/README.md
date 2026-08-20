# TRIAD Singapore Sensor Fusion (UE 5.5 runtime plugin)

This plugin provides a non-weaponized simulation and data-generation layer for a Cesium Singapore world. It creates visible, labeled geodetic sensor nodes, discovers drone-like actors, models the configured wideband RF channels (including 2.4/5.8 GHz), evaluates Unreal collision line-of-sight, simulates a geometry-only search-radar and radar-cued EO/thermal PTZ layer, and records bounded telemetry and camera frames.

## Activation

The `UTRIADSensorFusionWorldSubsystem` is instantiated only for `EWorldType::PIE` and `EWorldType::Game`. It starts a scenario manager only when this file exists and deserializes with `bEnabled: true`:

`<Project>/Config/SingaporeSensorFusion.json`

The active project configuration is `Config/SingaporeSensorFusion.json`; `Resources/SingaporeSensorFusion.example.json` is a smaller reference. Property names in the JSON intentionally match the reflected C++ property names. The map must already contain a `CesiumGeoreference`; the plugin logs an error and stops instead of creating or changing one.

Because this is a new C++ module, close the editor, build the editor target, and reopen the project before testing PIE. This project has an explicit enabled `TRIADSensorFusion` entry in `TRIAD.uproject`; the plugin descriptor declares its `CesiumForUnreal` and enabled `AirSimTriadRuntime` dependencies.

## Runtime behavior

- Each `ATRIADSensorNodeActor` uses `UCesiumGlobeAnchorComponent` and a WGS84 longitude/latitude/ellipsoid-height definition. A large state label, tangent-plane full/half range rings, and a tall beacon make nodes visible in PIE/Game. Cyan is online/idle, yellow is a single-node RF candidate, and orange is a corroborated multi-node preliminary RF cue. No color represents an inferred hostile classification.
- Targets match `TargetNameRegex`, `TargetActorTag`, or the `Drone1` fallback. If a discovered actor has no `UTRIADRFEmitterComponent`, the manager can attach a configured runtime component.
- Optional `ATRIADDemoDroneActor` instances use the AirSimTriadRuntime quadrotor body by default (with an Engine cube only as the final unavailable-asset fallback), the `DroneTarget` tag, an RF emitter, and stationary, circular, or linear ENU trajectories. `VisualMeshPath` may point to a project/plugin mesh; failure safely retains the default. `SpawnCount`, `FormationColumns`, and `FormationSpacingMeters` expand one definition into a centered ENU grid with unique `_01`, `_02`, ... names. Expansion is hard-capped at 64 demo actors across the session.
- The active 24-target scenario uses four named linear ingress corridors. Targets start beyond a documented rectangular simulation perimeter, fly through the sensor network at 22--28 m/s, cross the opposite edge, and reverse only through explicit ping-pong motion (no teleport loop). The four definitions retain 18 hostile-truth and 6 friendly-truth targets strictly for offline scoring; that authored truth never gates an operator cue. Their labels include the corridor ID.
- RF received power uses free-space path loss: `32.44 + 20 log10(distance_km) + 20 log10(frequency_MHz)`, plus configured antenna gains, receiver system loss, optional non-line-of-sight loss, and the active weather profile's explicit scenario attenuation. This is a deterministic approximation, not a full electromagnetic solver.
- LOS uses an Unreal line trace on `LineOfSightTraceChannel` (`0` is Visibility in a standard project). Terrain, Cesium tiles, buildings, and target collision settings therefore determine occlusion.
- A link is marked detected when its frequency is supported, it is within node range, received power exceeds sensitivity, and (when configured) LOS is present. This is RF-link detection only; RGB/depth or neuromorphic ML inference can consume the exported frames/telemetry but is not silently claimed here.
- Detected channels in the 2.4 GHz family are cyan, channels in the 5.8 GHz family are magenta, and other configured wideband channels are white. The world label explicitly says `RF WIDEBAND`, with configured channel count and frequency span. A stable on-screen line for each node enumerates **every detected channel** in compact MHz/GHz notation, then reports detected/drawn link counts, preliminary-cue evidence count, maximum SNR, and corroboration state. `bVisualizeRFLinks` and `bShowNodeRFStatusOnScreen` independently control these overlays; `MaxRFLinksVisualizedPerNode` bounds debug-draw cost without limiting RF calculation or telemetry.

## Singapore weather profiles

`Weather.Profile` accepts `Clear`, `LightRain`, `Monsoon`, `Haze`, or `Custom`. When `bApplyAirSimVisualWeather` is true, the manager initializes the enabled `AirSimTriadRuntime` weather assets around every sensor node and player pawn, sets rain/wetness/fog/dust/wind, and reads the material-collection values back. Telemetry records `airSimVisualWeatherApplied: true` only after that verification; otherwise it explicitly reports metadata/RF-only weather.

Each profile also records rain rate, visibility, band-interpolated specific RF attenuation, and total weather RF link loss in JSONL/CSV. These are declared scenario parameters for repeatability, not a complete atmospheric solver. For a timed smoke sweep, set `bCycleProfiles: true`, choose `CycleIntervalSeconds`, and list `CycleProfiles`. `SetWeatherProfile` is also Blueprint-callable for controlled test harnesses.

## RGB and depth frames

Every node owns RGB and scene-depth `USceneCaptureComponent2D` instances. FOV, resolution, rotation, low-rate cadence, and optional nearest-target tracking are configured per node. Files are written under:

`<Project>/Saved/SingaporeSensorFusion/frames/<node>/`

`bTrackNearestTarget` is fail-closed with respect to RF evidence: a node retargets its cameras only after that same node reports an RF detection, and it chooses the nearest RF-detected target in the current sample. With no current cue it does not retarget. For this simulation, cue-to-slew uses the detected actor's exact Unreal scene position as an explicitly idealized pointing location. RF bearing estimation, angular uncertainty, sensor/track association error, mechanical slew dynamics, and cue-to-capture latency are not modeled; camera pointing must not be treated as measured operational bearing performance.

Each capture contains a timestamped RGB PNG, an 8-bit normalized depth PNG, a lossless `*_depth_u32_mm.bin` metric sidecar, and JSON metadata (the legacy filename still ends in `_depth.json`). `SCS_SceneDepth` is read from R in Unreal centimeters. The raw sidecar stores row-major uint32 millimetres in explicit little-endian order, reserves `0` for invalid and `0xFFFFFFFF` for saturated/clamped values, and retains 0.001 m quantization beyond the configured 10 km camera range. Metadata declares the file name, encoding, byte order, units, dimensions, quantization, sentinel values, byte count, and simulation-only semantics. The PNG remains a diagnostic fallback; neither depth product claims calibrated physical OAK-camera accuracy.

Operator-only visuals are separated from sensor pixels. Drone/node text labels, sensor-site marker spheres, RF arrows, detection spheres, and range rings remain visible in the player's main view, but both scene-capture components hide them before every RGB/depth capture. Because event input is derived from these clean RGB frames, the same exclusion applies to the event-camera path. A saved frame contains `debugVisualsExcludedFromSensorCapture: true`; consumers should reject older metadata without that exact flag when contamination-free input is required. The physical drone mesh and ordinary Singapore scene geometry remain visible to the sensors.

## Simulated search radar and radar-cued PTZ

Every enabled node in the current project configuration explicitly contains three additional **simulated** capabilities:

- `SEARCH_RADAR`: 8 km configured slant-range envelope (runtime minimum 5 km), 360-degree azimuth field of regard, and an elevation field of regard. It samples target geometry even when the target has no RF emitter. Confidence is a deterministic, uncalibrated analytic score informed by range, Unreal LOS, an authored simulated RCS, visibility, and rain. Range, bearing, elevation, and radial velocity receive deterministic seeded Gaussian measurement noise. NLOS degrades radar confidence rather than automatically suppressing the return.
- `EO_PTZ`: 4-degree visible-light Unreal SceneCapture, configured for confirmation through 3 km. Its telemetry modality is `EO_VISIBLE`.
- `THERMAL_PTZ`: 6-degree synthetic false-colour proxy, configured for confirmation through 2.5 km. Its telemetry modality is `THERMAL_SYNTHETIC`; it is not radiometric or physical thermal imagery.

All current radar returns are exported. To keep rendering and the PIE display bounded, each node cues only its highest-confidence/nearest current radar track. PTZ orientation uses the noisy radar bearing/elevation and advances through `SLEWING`, `SETTLING`, and `SETTLED` at the configured slew rate and settle delay. A settled cue outside a modality's confirmation range emits lightweight unconfirmed status only—no SceneCapture, GPU readback, PNG compression, or publish. Once inside range, capture replaces one latest frame per modality at the configured cadence; it does not accumulate an unbounded video sequence.

EO/thermal confirmation fails closed on occlusion. At capture time the node traces from the actual PTZ capture origin to the target's visible primitive-bounds centre and eight corners. The node actor is ignored; an unobstructed ray or a hit on the target is visible. If every ray is blocked, `confirmed` is false and metadata reports `lineOfSight: false`, `blockingActor`, and `occlusionSemantics`. Radar may still retain a degraded NLOS observation.

Latest files are atomically replaced under the only supported relative-path roots:

```
Saved/SingaporeSensorFusion/RadarPtzFrames/eo/<node>/latest.png
Saved/SingaporeSensorFusion/RadarPtzFrames/eo/<node>/latest.json
Saved/SingaporeSensorFusion/RadarPtzFrames/thermal/<node>/latest.png
Saved/SingaporeSensorFusion/RadarPtzFrames/thermal/<node>/latest.json
```

The EO PNG contains raw Unreal visible SceneCapture pixels. The thermal PNG is a deliberately synthetic luminance/false-colour transform with analytic target heat gain only when LOS is present; `SIM THERMAL SYNTHETIC` is burned into its top banner. Both metadata documents say `kind: SIMULATED_SENSOR_CONFIRMATION`, `source: SIMULATION_PROJECTION`, `confirmationMethod: SIMULATION_PROJECTION_TRUTH`, `boxSource: DEBUG_PROJECTION`, `simulated: true`, and `calibratedDetector: false`. Their projected boxes are simulation/debug ground truth for display and validation—not RGB, thermal, YOLO, or other learned-model output.

PIE shows only one selected long-range cue per node to avoid 28-target clutter. The cyan cue line/marker reads `[SIM DEBUG RADAR CUE]`; a successful PTZ projection/LOS confirmation gets a distinct green `[SIM DEBUG PTZ CONFIRMED]` bounds marker. The node label names SEARCH RADAR, EO PTZ, THERMAL PTZ, range/FOV, and state. These line-batcher/debug aids are refreshed into every RGB, depth, EO, and thermal capture exclusion list. Pressing **F8 only ejects the Unreal player camera**; it does not enable detections or outlines. Use the labelled SIM DEBUG markers in the main PIE view, or the latest PTZ tiles in C2.

The active scenario has 28 drones. Four are an explicitly labelled acceptance flight, `RADAR_PTZ_ACCEPTANCE_INBOUND`: it begins east of the simulation perimeter at 104.0400 E, travels west at 32 m/s, emits 2.437/5.795 GHz, and passes the East_Sector node at approximately 158--179 m slant range without changing the physical drone mesh scale. It should cross the 500 m slant-range point after roughly 2.6--2.8 minutes. This route makes a live 500 m radar-to-PTZ check practical; it does not replace the exact deterministic automation test.

Every PTZ confirmation and frame metadata record carries `weatherProfile`, `weatherVisibilityMeters`, `weatherRainRateMillimetersPerHour`, `weatherConfidenceFactor`, and `weatherConfidenceSemantics`. EO confidence uses a clamped visibility/range factor; the synthetic thermal proxy uses a rain-rate factor. These are transparent simulation inputs, not calibrated performance curves.

For isolated acceptance runs, `-TRIADScenarioConfig=<absolute-json-path>` selects a scenario JSON without modifying the project's normal configuration, and `-TRIADTelemetryDir=<absolute-directory>` keeps snapshots and camera frames out of the live `Saved/SingaporeSensorFusion` path. These overrides are local simulation controls; they do not change the detection-only safety boundary.

The metadata also contains capture-camera ground truth for every actor tagged `DroneTarget`. `targets` is an array of objects with this schema:

```json
{
  "actorName": "HostileSwarm_01",
  "hostileScenarioTruth": true,
  "bboxXyxyPixels": [120.5, 44.0, 168.2, 91.7],
  "distanceMeters": 842.3,
  "inFront": true,
  "intersectsFrame": true,
  "componentBoundsValid": true,
  "projectedCornerCount": 8
}
```

The bounding box is the union of visible, model-facing physical primitive-component bounds, excluding every component tagged `TRIADHumanOnlyOverlay`, projected through that node's `USceneCaptureComponent2D`, then clipped to the RGB image. It encloses the simulated drone geometry; it is not one of the colored debug blocks/markers shown in the operator view. `bboxXyxyPixels` is an empty array when the bounds do not intersect the frame. `inFront` means at least one bounds corner is beyond the one-centimeter projection near plane. `intersectsFrame` is frustum overlap only; the metadata records `targetProjectionSemantics: model_facing_physical_primitive_bounds_frustum_only_no_occlusion_test` and `targetBoundsSemantics: visible_primitive_component_bounds_excluding_TRIAD_human_only_overlays`, and does not claim that the target is unoccluded or visually classified. Top-level `taggedTargetCount` and `visibleTargetCount` make frame filtering inexpensive.

Capture stops for each node at `MaxFramesPerNode` or `MaxFrameBytesPerNode`, and the raw sidecar bytes count toward that cap. Synchronous render-target readback can hitch. The active configuration enables 640x360 capture at all eight nodes: City and South use a 0.5-second cadence, while West, Jurong, North, North-East, East, and Central use 1.0 second. Every node stops after at most 120 frames, subject to the byte cap. Raw depth uses 921,600 bytes per frame (110,592,000 bytes for 120 frames); the active 256 MiB per-node cap leaves the remainder for the RGB/preview PNGs and metadata. This all-node capture configuration is a bounded simulation stress setup, not a qualified real-time throughput claim.

## RF telemetry

Session-specific JSONL and CSV files are written under:

`<Project>/Saved/SingaporeSensorFusion/`

Records contain UTC and simulation timestamps, node/target geodetic positions, distance, local azimuth/elevation, LOS and blocker, frequency, FSPL, obstruction loss, received power, noise floor, SNR, range/sensitivity checks, and the final simulated RF detection flag. `hostileScenarioTruth` is copied from authored scenario truth for offline scoring only. `preliminaryCueEvidence` is the sensor-derived RF detection result and is independent of scenario truth. The older `alertEvidence` JSON/CSV name remains as a compatibility alias with those same truth-independent semantics. Each RF output format stops at `MaxTelemetryRecords` or `MaxTelemetryFileBytes`; RF calculation, camera tracking, and multi-node cue corroboration continue after telemetry stops.

For a live local dashboard, every sample also atomically replaces the bounded current-state document:

`<Project>/Saved/SingaporeSensorFusion/latest_rf_snapshot.json`

This snapshot is independent of the capped session JSONL/CSV files. It contains all configured sensor-node definitions and current status, verified/unverified visual-weather state, the strongest currently detected RF links (limited by `MaxLiveSnapshotDetectedLinks`, default 512), and target summaries. Links are ordered by descending SNR and then nearest slant range. Schema `triad.live_rf_snapshot.v3` retains every v2 field and adds optional bounded `searchRadarDetections` and `ptzConfirmations` arrays. Every `sensorNodes` item also advertises the stable SEARCH_RADAR/EO_PTZ/THERMAL_PTZ configured range and runtime status. `frameRelativePath`/`metadataRelativePath` are accepted only below `RadarPtzFrames/eo/` or `RadarPtzFrames/thermal/`, contain no parent traversal, and are relative to `Saved/SingaporeSensorFusion`. Existing `preliminaryCueEvidenceNodeIds`, `rfMultinodePreliminaryCueRuleSatisfied`, and compatibility `alertEvidence*` fields retain their v2 meanings. The document labels `hostileScenarioTruth` as authored synthetic truth that never gates cues, and declares every sensor model uncalibrated and detection-only.

### Inbound approach telemetry

`SimulationPerimeter` is an axis-aligned WGS84 rectangle. The active bounds are longitude 103.6200--104.0200 and latitude 1.2200--1.4700, which contain all eight configured nodes. This is a deliberately simple scenario reference, **not** a Singapore national, territorial, FIR, legal, or restricted-airspace boundary. Horizontal distance uses a local equirectangular WGS84 approximation. The boundary is inclusive.

Every detected entry in `tracks` contains `airspaceState`, `distanceToPerimeterMeters`, `approachRateMetersPerSecond`, `headingDegrees`, `speedMetersPerSecond`, `outsideSimulationPerimeter`, and `ingressCorridorId`. `APPROACHING`, `DEPARTING`, and `OUTSIDE` apply beyond the reference rectangle; boundary/interior tracks use `INSIDE`. Perimeter distance is signed (positive outside, zero on the boundary, negative inside). Approach rate is positive while that signed distance is decreasing. Heading is clockwise from north (`0=N`, `90=E`, `180=S`, `270=W`).

The separate `scenarioTargets` array includes all authored/discovered target geometry, including targets not currently RF-detected. Its `detectedThisSample` flag prevents scenario truth from being mistaken for sensor evidence. `counts.outsideDetectedTracks` makes the intended outside-before-entry detection behavior directly auditable. With the active starts just 1.3--2.8 km outside the rectangle and near the outer sensor nodes' 20 km rings, targets can be RF-detected while still `APPROACHING`; configured entry times are roughly 52--126 seconds depending on corridor.

## Detection-only preliminary RF early-warning cues

`FTRIADRFEmitterDefinition.bHostileScenarioTruth` defaults to `false`. It is authored synthetic truth for offline evaluation, not simulated IFF and not an inferred class. It does not promote or suppress RF detections, multi-node corroboration, operator cues, or alert records.

The reflected configuration names `bEnableThreatAlerts` and `AlertMinimumConfirmingNodes` are retained for existing JSON and Blueprint assets. Their current semantics are generic: when enabled, any emitter RF-detected by at least that many distinct sensor nodes in the same sample (default 2) produces a preliminary early-warning cue. Frequencies do not double-count a node. Cues are shown in orange on screen, logged by Unreal, and written in batches to a bounded session file:

`<Project>/Saved/SingaporeSensorFusion/alerts_<session>.jsonl`

Repeated cues have a per-target `AlertCooldownSeconds` (default 3 seconds). JSONL persistence stops at `MaxAlertRecords` (default 1000), and the telemetry byte bound also applies to the legacy-named alert file; reaching either storage cap does **not** silence subsequent on-screen or Unreal-log cues. The orange on-screen cue and Unreal log identify the nearest confirming node and show its distance in metres or kilometres.

A cue JSON object uses `alertType: "rf_multinode_preliminary_cue"` and contains `timestampUtc`, `simulationSeconds`, `cueLevel: "preliminary"`, `classification: "unclassified_emitter"`, `hostilityAssessment: "not_inferred"`, `evidenceBasis`, a deterministic opaque `trackId` such as `RF-CUE-1A2B3C4D-55667788`, `confirmingNodeCount`, sorted `confirmingNodeIds`, `minimumConfirmingNodes`, `maxSnrDb`, `detectionOnly: true`, and `actionsTaken: none`. It deliberately contains no `hostileScenarioTruth`, raw `targetActor`, raw `emitterId`, target longitude/latitude/height, or evaluator-derived approach state/heading/speed. Instead, `positionEstimateAvailable`, `approachEstimateAvailable`, and `bearingEstimateAvailable` are `false`, with `positionApproachSemantics: "unavailable_at_raw_rf_cue_stage"`. The on-screen cue uses the same pseudonym and limitation; location at this stage is only the nearest confirming sensor node plus simulated 3D slant range. Position/approach estimates belong to the separately audited layered tracker.

```json
{
  "distanceValid": true,
  "distanceSemantics": "sensor_to_target_3d_slant_range",
  "nearestConfirmingNodeId": "City_Sector",
  "nearestConfirmingNodeDistanceMeters": 842.3,
  "confirmingNodes": [
    {"nodeId": "City_Sector", "slantDistanceMeters": 842.3},
    {"nodeId": "South_Sector", "slantDistanceMeters": 2114.8}
  ]
}
```

`confirmingNodes` is sorted by `nodeId` and has at most one entry per confirming sensor node; frequencies never duplicate a node. The distance is the three-dimensional sensor-to-target slant range used by that same RF link-budget sample, not camera-estimated depth or ground distance. There are no engagement actions.

## Blueprint extension points

`UTRIADRFEmitterComponent` and `UTRIADSensorNodeComponent` are Blueprint-spawnable. Node and demo actors are Blueprintable, while the auto-spawned manager is intentionally not placeable. There are no effectors, interceptors, weapon controls, or damage behavior in this module.

## Regression and exact-range validation

The engine-independent source-contract suite can run while Unreal Editor remains open:

`python -m unittest discover -s Plugins/TRIADSensorFusion/Tests -p "test_*.py" -v`

It guards the generic any-emitter node threshold, pseudonymized truth-free operator cue payload (including omission of raw corridor identity labels), the `rf_multinode_preliminary_cue` type, storage-cap/display separation, the additive v3 live-snapshot contract, RF independence of search radar, per-node 5 km-or-greater radar configuration, radar-cue slew/settle behavior, fail-closed PTZ occlusion, allowlisted atomic latest-frame paths, synthetic-thermal labeling, debug-aid exclusion, projection-not-model provenance, and preservation of physical drone mesh scale.

The coordinated Unreal automation test is:

`TRIAD.SensorFusion.LongRange.SearchRadar.CloseThrough500Meters`

It evaluates exact 25 m (close), 100 m, 250 m, and 500 m inputs with measurement noise disabled, requires a radar detection at every range, requires measured range to equal the input, and checks that a 0.5 m target at 4-degree/960-pixel EO settings retains at least two horizontal pixels through 500 m. This is deterministic simulation validation, not a field-performance claim. A normal Unreal Editor target build and automation run are still required to validate C++ integration after source changes.
