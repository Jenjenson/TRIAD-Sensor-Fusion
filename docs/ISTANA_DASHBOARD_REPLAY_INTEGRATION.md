# Istana dashboard replay and live-view integration

This note records only source-verified controls and paths. The replay artifact
is deterministic simulation evidence for a dashboard demonstration. It is not
live Unreal telemetry, calibrated detection performance, site authorisation, or
an operational C2 feed.

## Pick one data path deliberately

| Path | Display | Placement shown | Detection source |
| --- | --- | --- | --- |
| Preferred separate platform | Tk desktop dashboard opened by `scripts/Show-IstanaPlacementDashboard.ps1` | Two study candidates, `demo-south` and `demo-west` | Offline, deterministic, versioned synthetic precompute replay |
| Graphical live overlay | The same Tk dashboard with `-LiveUrl http://127.0.0.1:8765/api/snapshot` and an explicit replay reference | Frozen recommendation nodes as hollow reference markers; current runtime nodes as separate solid markers | Fresh, bounded, GET-only loopback snapshots; stale or invalid detections are suppressed |
| Legacy separate-platform replay | The same Tk dashboard with `istana_dashboard_replay_clear_rf.v2.json` | Four-node legacy layout matching the current Unreal scenario family | Offline deterministic range/weather gates |
| Interactive Unreal | `/Game/Maps/Istana_PublicView_Explore_v5d_hybrid` during Play-In-Editor | Four nodes from a newly generated enabled scenario | Current simulated RF/search-radar evidence |
| Loopback JSON | PowerShell or another GET-only client at `127.0.0.1:8765` | Whatever the current Unreal snapshot reports | Layered runtime output from fresh Unreal files |

The graphical client now has two deliberately distinct modes. Without
`-LiveUrl`, it is the original deterministic replay viewer. With `-LiveUrl`,
the replay becomes a frozen, clearly labelled recommendation reference while
current runtime nodes, tracks, cue links, source freshness, and provenance are
polled from the loopback bridge. A **REFERENCE MATCHED** claim is enabled only
for a fresh `live-local` snapshot whose complete node-ID roster and compatible
`WGS84_ELLIPSOID` 3D coordinates match the reference within one metre
horizontally and vertically.
Mismatch, delayed, stale, unavailable, and malformed states fail closed.

## Preferred four-class placement replay

The `.v2.json` suffixes below are deterministic artifact revisions. Their
outer dashboard and study schemas intentionally remain
`triad.dashboard_replay.v1` and
`triad.four_class_placement_demo_study.v1`; only the nested solver proof uses
`triad.robust_static_solver_result.v2`.

The primary separate-platform artifact is:

```text
core/reports/istana_four_class_dashboard_replay.v2.json
```

It adapts only the exact robust branch-and-bound result from
`istana_four_class_demo_study.v2.json`: two selected candidates
(`demo-south` and `demo-west`), four simulated ingress trajectories, and the
four allowed classes (`radar`, `passive_rf`, `rgb`, and `event_camera`). Event
times and evidence indexes are bound to versioned precompute rows. The adapter
does not invent measured calibration, RF propagation, or simultaneous
cross-sensor fusion. Consequently, all four tracks are eventually detected,
but the replay deliberately claims zero corroborated tracks.

Preflight and open it from a fresh Windows PowerShell terminal:

```powershell
Set-Location 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo'
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -ValidateOnly

& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -Speed 2
```

The checked-in digest is
`6949355f49492490752b453b49231db420446637818a2461e544254976524dab`.
At reset it has zero detections; the first precompute-backed cue is displayed
at `T+1.0 s`. Coverage is labelled **precomputed-model worst coverage** and is
valid only for the frozen synthetic binary precompute.

The recommendation panel also displays the bound robust-solver v2 proof:
termination reason `SEARCH_EXHAUSTED_OPTIMAL`, incumbent cost `4.4`,
conservative lower bound `4.4`, zero absolute and relative gaps, and
`nodeLimitReached=false` after 17 explored nodes against a 100,000-node limit.
These values prove only this synthetic precomputed binary model. The persistent
`DEMO_STUDY_ONLY` and **NO DEPLOYMENT** labels remain authoritative.

The visible site labels are `demo-site-south` and `demo-site-west`; their
underlying candidate IDs are `demo-south` and `demo-west`. Click each site to
show location, failure domain, all four sensor classes, and the exact-selection
rationale. Click **Run simulated ingress** to animate the four tracks. A clean
replay ends with four of four tracks detected and 18 cue events, but zero
corroborated tracks because aggregate precompute rows do not prove simultaneous
multi-sensor fusion. That is the intended result, not a failed demonstration.

For a hands-off loop, add `-Autoplay`. Space toggles playback and `R` resets;
the buttons provide **Pause**, **Reset**, timeline scrubbing, and `0.5x`--`16x`
speed control.

To create a new version without overwriting the checked-in artifact:

```powershell
Push-Location .\core
$previousPythonPath = $env:PYTHONPATH
try {
  $env:PYTHONPATH = 'src'
  python -m singapore_sensor_fusion.placement.dashboard_demo_adapter `
    '.\reports\istana_four_class_demo_study.v2.json' `
    --output '.\reports\istana_four_class_dashboard_replay.v3.json'
}
finally {
  $env:PYTHONPATH = $previousPythonPath
  Pop-Location
}
```

## Generate the legacy Unreal-layout replay

From the repository root, run:

```powershell
Push-Location .\core
$env:PYTHONPATH = 'src'
python -m singapore_sensor_fusion.placement.dashboard_replay `
  '.\examples\istana_1km_placement_request.json' `
  '.\reports\istana_1km_recommendation.json' `
  '..\unreal\Config\IstanaSensorPlacement.base.json' `
  --scenario-id 'clear-rf' `
  --duration-seconds 150 `
  --cadence-seconds 1 `
  --map-package '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid' `
  --output '.\reports\istana_dashboard_replay_clear_rf.v3.json'
Pop-Location
```

The checked-in legacy artifact is
`core/reports/istana_dashboard_replay_clear_rf.v2.json`; the example writes a
fresh V3 because the writer refuses to overwrite an existing artifact. The
output uses schema `triad.dashboard_replay.v1` and contains:

- the exact selected placement rows and their source recommendation digest;
- display rationale without inventing unavailable per-site marginal metrics;
- the recommendation's network coverage metrics verbatim;
- map/AOI/scenario metadata;
- deterministic relative-time drone positions matching the Unreal linear,
  circular, stationary, ping-pong, and formation-expansion rules;
- per-node/per-modality positive detection samples with range, an explicitly
  uncalibrated confidence index, and deterministic display latency;
- an explicit Stage-0 scope audit: the legacy package's prohibited
  `THERMAL_PTZ` modality is filtered, while its missing `event_camera` class is
  reported as a warning rather than fabricated;
- first detection/corroboration time and an all-tracks-detected summary; and
- a whole-document SHA-256 `replayDigest` plus fail-closed safety fields. The
  digest detects accidental or unreviewed byte changes; it is not a signature
  or authenticity proof.

The checked-in V2 replay has no detections at `T+0`. Its first synthetic
range-gate cue appears at `T+1.0 s` and its first two-family corroboration at
`T+2.0 s`, so the viewer demonstrates acquisition before continuing to track.

## Open the legacy Unreal-layout replay

From the repository root, first run the headless preflight:

```powershell
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_dashboard_replay_clear_rf.v2.json' `
  -ValidateOnly
```

Then open the window:

```powershell
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_dashboard_replay_clear_rf.v2.json' `
  -Speed 2
```

Click a blue placement node to see its exact location, modalities, failure
domain, and why the source solver selected it. Click **Run simulated ingress**
to start; use **Pause**, **Reset**, the timeline, or the `0.5x`--`16x` speed
selector. Detection links appear only at or after their event timestamp. The
feed shows range, the uncalibrated evidence index, and synthetic display
latency. The dashboard is a wall-clock animated offline replay, not a client
for Unreal's live `/api/snapshot` endpoint unless it is explicitly reopened
with `-LiveUrl` as described below.

`IstanaSensorPlacement.base.json` deliberately has `bEnabled: false`; therefore
the exported replay correctly says `runtimeConfigEnabledInSource: false`. That
is valid for the offline replay generator. It is **not** permission to edit or
enable the base file in place. An Unreal run requires a separate, reviewed,
enabled scenario created with the existing non-overwriting workflow, for
example:

```powershell
$showcaseStamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$scenarioPath = "D:\triad\TRIAD\Saved\TRIAD\Configs\IstanaSensorPlacement.showcase_$showcaseStamp.json"
& .\scripts\New-IstanaScenarioConfig.ps1 `
  -PatchPath '.\core\reports\istana_1km_unreal_sensor_nodes_patch.json' `
  -OutputPath $scenarioPath `
  -Enable `
  -EnableOperatorObserver
```

Keep the same terminal open for the launch below. The timestamp makes this
repeatable because the config builder refuses to overwrite an existing file.

## Open the exact V5D visual map with the scenario

The source-validated level package is:

```text
/Game/Maps/Istana_PublicView_Explore_v5d_hybrid
```

The current native paths and authoritative R30-19 transaction receipt were
verified read-only on 2026-09-08:

```text
D:\triad\TRIAD\TRIAD.uproject
D:\triad\TRIAD\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap
C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe
D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json
```

That receipt is `COMMITTED`, 65,411 bytes, with SHA-256
`F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F`.
It pins the current map at 37,468,415 bytes / SHA-256
`126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7`,
the runtime editor DLL at 5,076,992 bytes /
`100B061CC5677508A67D644687923CA58F68F072305B6FF62B6BCD363D472028`,
and the editor DLL at 8,448,000 bytes /
`3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553`.
The R29 receipts remain immutable predecessor evidence and content dependencies;
they are not the current native identity.

R30-19 promotes the texture-backed façade-lookdev boundary together with
TreeRealism v3's 13 response materials, five rebound meshes and 26 MIDs. It is
native, but its transaction explicitly records that visual capture is not
accepted. The latest capture attempt is the fail-closed run09 rollback:

```text
D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\rollback.json
```

It is 658 bytes with SHA-256
`5827225197FE3BC4F57B28D36947F6C3642713410BD768B674C2F5D122144B87`.
Run09 proved the eight-worker shader path and exact Columnar-tree DDC hit, then
failed closed on an `AirSimTriadRuntime` optical-flow material compile error. It
reports `ROLLED_BACK` with no rollback errors and emitted neither pending nor
accepted visual evidence. Run 05 is retained as historical failure evidence.
The R30 capture harness has no fixed process-RAM cap and reserves 2 GiB of
emergency system commit; this is not a GPU-VRAM limit. The completed R30
transaction and R31--R33 stages retain their 10/6/12-GiB limits. Do not rerun
the R30 transaction or claim its appearance as accepted. The surrounding-
building coverage is extensive, but the broad shells remain simplified and
procedural rather than hyperreal.

The checked-in R30 offline CPU comparison at
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R30FacadeLookdev/OfflineSourceLookdev/r30_facade_cpu_source_lookdev.png`
is source-material diagnostics only. It is not Unreal/provider evidence and
cannot accept R30. The aligned visual delta is small (`0.43/255` mean absolute;
`0.98%` above eight levels), confirming that R30 affects the separate facade
cue rather than the broad shells. A guarded five-pose native capture path now
exists at `scripts/Capture-IstanaExploreV5DR30Player0Evidence.ps1`, but it is
admissible only after the exact R30 committed receipt and before R31. It creates
fresh cooked output with a run-local isolated, hash-pinned seeded and prewarmed
DDC. There is still no accepted native R30 capture.

R31 is now source-frozen for the broad 17-slot surroundings shell. It preserves
the exact suppression-V2 mesh, transform, geography and `43,448` triangles and
changes only the 17 material overrides through one master and four role
instances. Its contract is `13,386` bytes, SHA-256
`6F2F348AB1D6DED49D3A8350C19552C700A2647917D68C0E3D9F1A8519BD7617`,
with an exact 15-file source/texture closure and provenance checks for the nine
reused texture assets. The repository-only parser, static self-check and
combined R25/R30/capture/R31 source regression set pass (`134/134`). No Unreal
process or native mutation was used to establish that result.

The R31 CPU comparison at
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev/OfflineSourceLookdev/r31_broad_shell_cpu_source_lookdev.png`
shows a fixed aligned delta of `8.45/255` mean absolute and `41.82%` above eight
levels versus the flat source shell. It is source-material diagnostics only:
not Unreal, not a native R31 integration, not Cesium/provider composition, not
actual-site or embedded-pixel truth, and not visual acceptance. R31 cannot run
until R30 has both its exact committed transaction and an accepted five-pose
Player0 capture. The native level therefore remains exact R30-19, with no
accepted R30 image and no native R31 image to claim.

The V5D map's Cesium integration provides an exact shared cartographic frame,
not an admitted terrain-analysis surface. It contains one georeference at
longitude `103.84288055`, latitude `1.30709615`, ellipsoidal height `47 m`, and
one Google Photorealistic 3D Tiles provider (Cesium ion asset `2275207`). It is
not a dedicated Cesium World Terrain actor. Provider readiness is based on the
tileset's global load percentage (`>=98%` for three `0.5 s` samples, restoring
fallback below `90%`); that signal does not prove local coverage or elevation
accuracy.

R29's `2 km`, `129 x 129` Copernicus GLO-30 mesh is visual-only. The admitted
source is roughly `30.9 m`, smoothed and centre-relative, with no
geoid-to-ellipsoid conversion, only `59.8576%` valid height-error coverage, no
survey control, and no absolute-height authority. Neither provider may be used
for dashboard collision, occlusion, sensor, RF, or terrain-accuracy claims.
Simulation-grade terrain requires an authorised local DTM/survey surface,
explicit horizontal and vertical datum handling, independent checkpoint
residuals and a separately versioned deterministic collision/LOS authority.

`Start-IstanaSimulation.ps1` now retains `/Game/Maps/Istana_1km` as its
default and accepts the V5D map only through an exact allowlist. It verifies the
selected `.umap`, enabled config, non-empty `SensorNodes`, exact 1,000 m circle,
and any dedicated-RF world-package binding before it starts Unreal:

```powershell
& .\scripts\Start-IstanaSimulation.ps1 `
  -ScenarioConfigPath $scenarioPath `
  -MapPackage '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
```

This launches the new observer-enabled file created above. A separate existing
`recommended.v2.json` file was observed as enabled with four sensor nodes, one
demo target, and an exact 1,000 m circular perimeter, but its operator observer
is disabled. The generated file does not declare the dedicated-RF opt-in
fields, so Unreal uses their backward-compatible `false` defaults. The separate
`IstanaOneKilometreV2RF.example.json` is hash-bound to the exact V5D package,
but is an RF binding example rather than a complete placement scenario. Merge
such settings only into a new reviewed config; do not present the generic run
as dedicated-RF-calibrated evidence.

The sensor subsystem itself is map-independent for PIE/Game: when the selected
scenario deserializes with `bEnabled: true`,
`UTRIADSensorFusionWorldSubsystem` spawns one scenario manager in the active
world. The V5D contract supplies the required Cesium georeference and exact V5
game mode/pawn. This is the source evidence supporting the bounded launcher
extension; no Unreal process was started during this source-side change.

## Verified Explore controls

After the editor loads, press the green **Play** button. The V5 free-roam pawn
polls keys directly, so no project input mapping is required:

- `W` / `S`: horizontal forward / backward;
- `A` / `D`: horizontal strafe left / right;
- `E` / `Q`: rise / descend;
- mouse: yaw and pitch look;
- either `Shift`: 4x boost;
- either `Ctrl`: 0.25x precision movement (`Ctrl` wins if both are held);
- `Esc`: stop PIE; and
- `Shift+F1`: release the captured mouse to the editor.

The authored base speed is 12 m/s. Movement is collision-swept and bounded to
950 m horizontally from spawn and local altitude 1.5--300 m.

The source `istana_1km_recommendation.json` is a legacy prototype, not a
complete result for the frozen Stage-0 four-class scope. Its package contains
`THERMAL_PTZ` and has no `event_camera`. The replay exporter never turns that
thermal entry into a deployed sensor and never manufactures event-camera
detections. A dashboard must show `stage0Scope.warnings` and must not label the
legacy layout as complete radar/passive-RF/RGB/event-camera coverage.

## Live detection feed

Replay mode remains fully offline. Live-overlay mode consumes only the existing
GET-only loopback projection; it never reads or mutates Unreal directly:

1. Unreal writes `triad.live_rf_snapshot.v3` atomically to
   `D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json`, plus
   bounded `rf_links_<session>.jsonl`, `rf_links_<session>.csv`, optional
   `alerts_<session>.jsonl`, RGB/depth frames, and radar/PTZ frames.
2. The layered runtime converts accepted current observations to
   `triad.layered_detection_snapshot.v1` without regenerating RF from scenario
   truth:

   ```powershell
   Push-Location .\core
   $env:PYTHONPATH = 'src'
   python -m singapore_sensor_fusion.layered_runtime `
     --input 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json' `
     --output 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_layered_snapshot.json' `
     --interval 0.25
   ```

3. In another terminal, the read-only loopback bridge publishes the newest
   bounded view:

   ```powershell
   Push-Location .\core
   $env:PYTHONPATH = 'src'
   python -m singapore_sensor_fusion.c3_bridge `
     --saved-dir 'D:\triad\TRIAD\Saved\SingaporeSensorFusion' `
     --host 127.0.0.1 `
     --port 8765
   ```

   A local platform can poll `http://127.0.0.1:8765/api/snapshot`; health is at
   `http://127.0.0.1:8765/health`. The bridge exposes GET-only data and frames,
   never mutation or engagement endpoints.

4. Open the graphical live overlay from a fourth terminal. Keep the replay
   explicit because it is the frozen recommendation reference:

   ```powershell
   & .\scripts\Show-IstanaPlacementDashboard.ps1 `
     -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
     -LiveUrl 'http://127.0.0.1:8765/api/snapshot' `
     -LivePollSeconds 0.25 `
     -LiveTimeoutSeconds 0.75
   ```

   The client accepts only an exact HTTP loopback `/api/snapshot` URL, disables
   environment/system proxies, rejects redirects, limits each body to 2 MiB,
   bounds each request to 0.1--5 seconds, and bounds the poll interval to
   0.1--10 seconds. After receipt it advances bridge-reported source and track
   ages with a local monotonic clock; a stalled poll therefore expires tracks,
   cues, and reference authorization at the declared freshness threshold.
   Repeated bodies cannot reset that clock, and invalid, old, future, changed-
   at-the-same-time, or retrogressing `generatedAtUtc` envelopes fail closed.
   Solid markers and tracks are the current runtime scenario. Hollow markers
   remain the frozen solver reference. If those rosters differ—as the preferred
   two-site study and the current four-node Unreal scenario normally do—the
   dashboard prominently says **REFERENCE MISMATCH** and never attributes live
   detections to the frozen recommendation. Delayed or stale snapshots may
   retain muted node context, but live tracks and cue links are suppressed.

   A headless live preflight uses the same validation path:

   ```powershell
   & .\scripts\Show-IstanaPlacementDashboard.ps1 `
     -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
     -LiveUrl 'http://127.0.0.1:8765/api/snapshot' `
     -ValidateOnly
   ```

## What a detection means

- Unreal RF detections are simulated link-budget decisions. Search-radar and
  radar-cued EO/thermal PTZ are deterministic simulation records.
- The layered runtime keeps modalities visible and requires independent sensor
  families for corroboration; it does not add confidence numbers across
  dependent sensors.
- Projection boxes are labelled simulation projection, not learned-model
  output. Event-camera evidence currently uses a proxy path when no native
  event stream is available.
- The primary four-class replay exposes only detections backed by non-null
  first-detection times in the versioned precompute and adds a one-second
  display-only timeline offset. The legacy replay uses deterministic
  distance/weather range gates with synthetic per-modality acquisition delays.
- Neither offline model evaluates terrain/foliage occlusion, multipath,
  antenna patterns, camera FOV, or PTZ slew. Their `confidence` values are
  evidence indexes, not field probabilities; displayed timing is not measured
  hardware latency.
- All paths are detection-only. No engagement action is generated.

For a presentation, use the portable replay for deterministic scrub/play/reset
behavior and visibly label it **SIMULATION REPLAY**. Use live-overlay mode only
when Unreal, the layered runtime, and the loopback bridge are actually running.
Its live tracks disappear when evidence is not fresh; do not reinterpret muted
nodes or a reference overlay as current detections.

## Recommended showcase handoff

1. Open the preferred replay, click `demo-site-south` and
   `demo-site-west`, and read the exact-selection rationale.
2. Reset, then click **Run simulated ingress**. Point out that the first cue is
   absent until `T+1.0 s`, then show the cue line and detection feed.
3. End on four of four simulated tracks detected and explicitly say that zero
   tracks are corroborated in this aggregate-row replay.
4. Switch to Unreal, press **Play**, and use the operator overlay for the
   separate four-node runtime scenario. Do not imply that the desktop replay
   and Unreal clocks or placements are the same.
5. If live JSON is needed, start the layered runtime and loopback bridge, verify
   `/health`, and reopen the Tk dashboard with `-LiveUrl`. Read the visible
   freshness and reference-comparison label before describing any track as
   current or any runtime node as the recommended placement.

## Windows troubleshooting

- If relative paths fail, run
  `Set-Location 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo'`.
- If PowerShell blocks a checked-in script, use
  `Set-ExecutionPolicy -Scope Process Bypass` for that terminal only.
- `-ValidateOnly` intentionally exits without opening a window. Run the second
  dashboard command without that switch for the GUI.
- If Python probing fails, pass a Python 3.11+ executable that can import both
  `tkinter` and `numpy` with `-PythonExecutable`.
- If scenario creation reports that the output exists, create a new timestamped
  `$scenarioPath`. The builder is non-overwriting by design.
- The Unreal launcher opens the editor but does not press Play. Wait for the
  exact V5D map to load, then press the green **Play** button.
- If no Unreal contact appears, verify `bEnabled`, `bSpawnDemoTargets`, at least
  one enabled `DemoTargets[]` entry, four enabled `SensorNodes`, and
  `OperatorObserver.bEnabled` in the generated scenario. A visible drone mesh
  alone is not a detection.
- Check live-file freshness before claiming live data:

  ```powershell
  $snapshot = Get-Item 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json'
  [pscustomobject]@{
    LastWriteTime = $snapshot.LastWriteTime
    AgeSeconds = [math]::Round(((Get-Date) - $snapshot.LastWriteTime).TotalSeconds, 1)
  }
  ```

- If loopback health fails, keep the bridge terminal open and inspect the port:

  ```powershell
  Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue |
    Select-Object LocalAddress, LocalPort, OwningProcess
  Invoke-RestMethod 'http://127.0.0.1:8765/health'
  ```
