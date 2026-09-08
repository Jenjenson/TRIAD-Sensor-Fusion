# Istana end-to-end simulation and placement guide

This is a detection-only, study-mode workflow. It demonstrates simulated UAS
ingress, sensor placement, and synthetic detections. It does not authorize a
physical installation, infer permission to use a mount, provide calibrated
probability of detection, or perform an engagement action.

## What to open

| Purpose | Exact item |
| --- | --- |
| Separate placement platform | `scripts/Show-IstanaPlacementDashboard.ps1` |
| Four-class recommendation replay | `core/reports/istana_four_class_dashboard_replay.v2.json` |
| Legacy Unreal-layout replay | `core/reports/istana_dashboard_replay_clear_rf.v2.json` |
| Four-class solver study | `core/reports/istana_four_class_demo_study.v2.json` |
| Unreal project | `D:\triad\TRIAD\TRIAD.uproject` |
| Unreal visual level | `/Game/Maps/Istana_PublicView_Explore_v5d_hybrid` |
| Live Unreal RF snapshot | `D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json` |
| Current R30-19 map/binary receipt | `D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json` |
| Certified recovery of failed R30 capture run 04 | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\recovery\r30-capture-20260908-04-recovery-02-certification\receipt.json` |
| Latest R30 capture rollback, run 09 (no pending/accepted images) | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\rollback.json` |
| Earlier clean R30 capture rollback, run 08 | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-08\rollback.json` |
| Historical R30 capture rollback, run 05 | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-05\rollback.json` |
| R29 predecessor map/binary receipt | `D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DR29CopernicusTerrainFallbackV1\r29terrainlive_20260906_11\commit.json` |
| R29 vegetation receipt | `D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DR29TropicalVegetationV1\r29veglive_20260906_06\commit.json` |
| R29 surrounding-façade receipt | `D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR29V1\r29facadelive_20260906_04\commit.json` |
| Last successful native raster set (R27 predecessor) | `D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_vegetation_provider_evidence_r27_grass_visual_20260906T0710SGT.json` |
| V5D migration evidence | `D:\triad\TRIAD\Saved\TRIAD\MigrationEvidence` |
| V5D visual-evidence output | `D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D` |
| Vegetation evidence wrapper | `scripts/Capture-IstanaExploreV5DVegetationProviderEvidence.ps1` |
| MacDonald House evidence wrapper | `scripts/Capture-IstanaExploreV5DMacDonaldHouseR24Evidence.ps1` |
| Temasek Shophouse evidence wrapper | `scripts/Capture-IstanaExploreV5DTemasekShophouseR24Evidence.ps1` |

The absolute `C:\...` and `D:\...` paths in this manual are host-specific to
the audited workstation. If the installation moves, substitute only the local
filesystem roots; keep the exact Unreal package and evidence artifact names.
Run every repository command below from:

```text
C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo
```

## Choose the right view

| View | What it proves and shows | What it does not do |
| --- | --- | --- |
| Separate placement dashboard, replay mode | Opens a deterministic offline replay of the two-site four-class study, explains why each study candidate was selected, and animates four simulated ingress tracks and their precompute-backed cues. | It is not live or field-calibrated. |
| Separate placement dashboard, live-overlay mode | Polls the GET-only loopback `/api/snapshot`, draws current runtime nodes/tracks/cues, and keeps the selected replay visible as a hollow, separately labelled recommendation reference. It compares the complete node-ID roster and compatible WGS84 3D coordinates before displaying **REFERENCE MATCHED**. | It does not make a mismatched runtime scenario into the solver recommendation, show stale tracks, mutate Unreal, or authorize operational use. |
| Unreal V5D level | Shows the current native environment, the separate four-node Unreal scenario, a moving demo quadcopter, and current simulated RF/search-radar contacts in the operator panel. | It is not the two-site four-class solver replay, and its run is not synchronized with the desktop dashboard. |
| Layered snapshot plus loopback API | Reads current Unreal snapshot files, applies freshness/fusion rules, and exposes a GET-only local JSON view consumed by live-overlay mode. | It remains detection-only and is not a command, engagement, or calibration interface. |

The recommended presentation uses the dashboard first to explain placement,
then Unreal to show the environment and interactive simulated detections. Keep
both labels visible: **study-mode replay** in the dashboard and **simulation**
in Unreal.

## Resource check before Unreal

Free disk and Windows free commit are different. Run this read-only check before
opening Unreal or starting a guarded maintainer wrapper:

```powershell
$os = Get-CimInstance Win32_OperatingSystem
[pscustomobject]@{
  DDriveFreeGiB = [math]::Round((Get-PSDrive -Name D).Free / 1GB, 2)
  FreeCommitGiB = [math]::Round(([int64]$os.FreeVirtualMemory * 1KB) / 1GB, 2)
}
```

The current R30 capture harness has no fixed process-RAM ceiling, keeps a 2 GiB
emergency system-commit reserve at launch and continuously, and uses eight
shader workers. Those are CPU/system-memory controls, not a GPU-VRAM setting.
The completed R30 transaction and the R31--R33 transaction/capture wrappers
retain their 10 GiB launch, 6 GiB continuous, and 12 GiB private-memory limits.
None is a disk threshold. Isolated cooks and rollback journals also use
substantial disk: one rollback is roughly 15 GiB on this workstation, and
accumulated R30 retries can exceed 120 GiB. Those sizes are observations, not
deletion authority or future space requirements.

Stop Unreal and Python normally before investigating pressure. Do not delete or
edit `Saved\TRIAD\NativeTransactions`, any `TRIAD_R*Evidence` tree, rollback or
recovery data, or certification receipts. Do not clear `Binaries`,
`Intermediate`, cooked output, or derived-data caches while a wrapper is active.
Use only a maintainer-approved exact cleanup or archive target. If free commit is
low, closing approved applications or an administrator-managed page-file change
and restart may help; deleting files alone will not.

## Fastest complete showcase

Use two clearly labelled PowerShell windows:

- **Terminal A — placement platform:** validate and open the frozen two-site
  recommendation replay in step 1. Leave the dashboard open.
- **Terminal B — Unreal simulation:** create the separate four-node scenario in
  step 2 and launch the exact V5D level in step 3.

The two windows show different, unsynchronised study packages. Do not imply that
the Unreal detection run proves the dashboard's two-site solver result.

### 1. Terminal A: validate and open the separate dashboard

In a fresh Windows PowerShell terminal, set the repository root and run the
headless check first:

```powershell
Set-Location 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo'
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -ValidateOnly
```

The checked-in artifact should report replay digest
`6949355f49492490752b453b49231db420446637818a2461e544254976524dab`,
two displayed sites, four simulated ingress tracks, and four eventually
detected tracks.

Open the window at double speed:

```powershell
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -Speed 2
```

In the dashboard:

1. Point out the persistent banner: **STUDY-MODE REPLAY · NOT LIVE · SYNTHETIC
   DEMO_STUDY_ONLY · NO DEPLOYMENT · NOT FIELD-CALIBRATED**.
2. Point out **Solver proof · v2**. It displays the exact
   `SEARCH_EXHAUSTED_OPTIMAL` termination reason, `4.4` incumbent and
   conservative lower-bound costs, zero absolute/relative gap, and **Node limit:
   NOT REACHED** (17 explored of 100,000). These prove only the frozen synthetic
   binary model.
3. Click the visible `demo-site-south` and `demo-site-west` sensor nodes. They
   correspond to candidate IDs `demo-south` and `demo-west`. The right panel
   shows each site, coordinates, failure domain, all four sensor classes, and
   the exact branch-and-bound selection rationale.
4. Click **Run simulated ingress**. At reset there are zero detections. The
   first precompute-backed synthetic cue occurs at `T+1.0 s` after a clearly
   labelled display-only timeline offset.
5. Watch the UAS diamond, sensor-to-track cue lines, and detection feed. The
   feed exposes range, an uncalibrated evidence index, and the display-timeline
   timestamp.
6. Use **Pause**, **Reset**, the timeline, or the `0.5x`--`16x` selector. Space
   toggles playback and `R` resets.

The displayed coverage is explicitly labelled **precomputed-model worst
coverage**. Optimality and coverage apply only to the frozen synthetic binary
precompute. The aggregate rows do not prove simultaneous fusion, so this replay
deliberately reports no corroborated tracks. The Stage-0 warning is part of the
evidence, not an error to hide.

To show the older four-node layout that matches the current Unreal scenario,
open `core/reports/istana_dashboard_replay_clear_rf.v2.json` instead. That
legacy replay has digest
`667def08d038407a88d7a674dc22041e7f3e3f0f5edcd76e021b0ca69cb8c34a`,
first cue at `T+1.0 s`, and synthetic two-family corroboration at `T+2.0 s`.
Its `95.2%` is correctly labelled **legacy-reported coverage** because thermal
was filtered and event-camera coverage is absent.

### 2. Terminal B: create a new enabled Unreal scenario

Do not enable or edit `unreal/Config/IstanaSensorPlacement.base.json` in place.
Create a new file under Unreal's `Saved` tree. The timestamp makes the command
repeatable because the builder correctly refuses to overwrite an existing
scenario. Keep this terminal open so `$scenarioPath` is available in step 3:

```powershell
Set-Location 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo'
$showcaseStamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$scenarioPath = "D:\triad\TRIAD\Saved\TRIAD\Configs\IstanaSensorPlacement.showcase_$showcaseStamp.json"
& .\scripts\New-IstanaScenarioConfig.ps1 `
  -PatchPath '.\core\reports\istana_1km_unreal_sensor_nodes_patch.json' `
  -OutputPath $scenarioPath `
  -Enable `
  -EnableOperatorObserver
$scenarioPath
```

`-EnableOperatorObserver` enables the in-Unreal evidence panel in this new
copy. It does not change sensor decisions.

An existing file was also observed at
`D:\triad\TRIAD\Saved\TRIAD\Configs\IstanaSensorPlacement.recommended.v2.json`.
It is enabled and contains four nodes plus one demo target, but its operator
observer is disabled.

### 3. Open the correct Unreal level

Launch the exact V5D map with the new config:

```powershell
& .\scripts\Start-IstanaSimulation.ps1 `
  -ScenarioConfigPath $scenarioPath `
  -MapPackage '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
```

Do not omit, shorten, or substitute the `-MapPackage` value. In particular, the
rejected `Istana_1km_Context_v2` prototype is not a valid fallback.

The launcher checks that the map exists, the scenario is enabled, sensor nodes
are present, the AOI is the exact 1,000 m circle, and any dedicated-RF package
binding matches the selected map. When Unreal finishes loading, press the green
**Play** button, then click once inside the game viewport so it owns keyboard and
mouse input.

The package name did not change as the guarded visual transactions produced
their successors. Do not infer which revision is open from the level name. For
a current R30 evidence claim, compare the native `.umap` and both plugin DLLs
with the exact R30-19 receipt and pins below. It must report `COMMITTED`. If any
identity or validation is missing, describe the level only as the current native
V5D map—not as the committed R30 artifact set.

After the editor opens, use **Window > Cesium** for any authorised provider
sign-in. Keep credentials inside the plugin's supported account/server flow;
never paste or expose a token. Select the existing Google Photorealistic
tileset and verify only source **From Cesium ion**, asset ID `2275207`, and a
non-null server reference. Visible refinement and the absence of `401`, `403`,
or repeated `429` messages are useful diagnostics, not provider-readiness or
terrain-accuracy proof. The full boundary is under
[Cesium and terrain accuracy](#cesium-and-terrain-accuracy-boundary).

To open it manually instead, double-click
`D:\triad\TRIAD\TRIAD.uproject`, find `Content/Maps`, and open
`Istana_PublicView_Explore_v5d_hybrid`. Use the scripted launch for the demo so
the intended scenario-config override is present.

This is the interactive simulation path. The visual-evidence wrappers below
launch and close their own unattended UE 5.5 helper. Do not leave this editor
open while running those wrappers.

### 4. Move around in Unreal

On first open, wait until shader/asset loading settles, press **Play**, and click
the viewport. If keys do nothing, click inside it again. `Shift+F1` releases the
mouse back to the editor.

- `W` / `S`: move forward / backward.
- `A` / `D`: strafe left / right.
- `E` / `Q`: rise / descend.
- Mouse: look.
- `Shift`: 4x movement boost.
- `Ctrl`: 0.25x precision movement.
- `O`: hide/show the operator panel when it is enabled.
- `N`: select the next current contact.
- `Shift+F1`: release the mouse to the editor.
- `Esc`: stop Play-In-Editor.

The free-roam base speed is 12 m/s. Movement is collision-swept, bounded to
950 m horizontally, and limited to 1.5--300 m local altitude.

### 5. Show the Unreal simulated-ingress-and-detection sequence

The scenario spawns one simulated quadcopter on a 1,800 m east-to-west linear
path, moving at 12 m/s and ping-ponging at the endpoints. Sampling runs every
0.25 s.

With the operator observer enabled, positive current RF or search-radar
evidence creates a contact. The panel then shows the reporting node, detected
range, radar confidence and/or RF SNR, and EO/PTZ status. `O` toggles the panel;
`N` moves between active contacts. World-space cue arrows and markers are
presentation aids and do not create detections.

Use the Unreal view to show the environment and the current four-node runtime
sensor evidence. Use the separate desktop dashboard to explain the new exact
two-site four-class study result and deterministic ingress replay. They use
different study packages, are two separate runs, and are not time-synchronized.

For a clean presentation, wait for the demo target to move into range, point
out the reporting node and range/SNR or radar evidence in the panel, then press
`N` to cycle the current contact. A visible drone or route marker alone is not
a detection. If the panel remains empty, use the checks in **Troubleshooting**
instead of describing the target as detected.

Verify that the raw snapshot belongs to this exact scenario and advances. Run
this while Play is active in Terminal B:

```powershell
$snapshotPath = 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json'
$config = Get-Content -Raw -LiteralPath $scenarioPath | ConvertFrom-Json
$first = Get-Content -Raw -LiteralPath $snapshotPath | ConvertFrom-Json
Start-Sleep -Seconds 1
$second = Get-Content -Raw -LiteralPath $snapshotPath | ConvertFrom-Json
$expectedNodeIds = @($config.SensorNodes.NodeId | Sort-Object)
$actualNodeIds = @($second.sensorNodes.nodeId | Sort-Object)
[pscustomobject]@{
  Schema = $second.schemaVersion
  Advanced = ([datetime]$second.timestampUtc -gt [datetime]$first.timestampUtc)
  SampleComplete = $second.sampleComplete
  DetectionOnly = $second.detectionOnly
  ActionsTaken = $second.actionsTaken
  NodeIdsMatch = @(Compare-Object $expectedNodeIds $actualNodeIds).Count -eq 0
  AllNodesEnabledAndSpawned = @(
    $second.sensorNodes | Where-Object { $_.enabled -ne $true -or $_.spawned -ne $true }
  ).Count -eq 0
  ScenarioTargetCount = $second.counts.scenarioTargets
  NonHostileDemoTruth = @(
    $second.scenarioTargets | Where-Object hostileScenarioTruth
  ).Count -eq 0
  CurrentPositiveEvidence = (
    $second.counts.retainedDetectedRFLinks -gt 0 -or
    $second.counts.retainedSearchRadarDetections -gt 0)
}
```

The pass state is schema `triad.live_rf_snapshot.v3`, an advancing timestamp,
complete detection-only samples, `actionsTaken=none`, exact node-ID match, all
four nodes enabled and spawned, one non-hostile scenario target, and no authored
hostile truth. Positive RF or radar evidence may be false while the target is
out of range; wait until it becomes true and the operator panel agrees before
claiming a detection.

## V5D current R30 native scene status

The guarded chain keeps the same
`/Game/Maps/Istana_PublicView_Explore_v5d_hybrid` package. The latest native
transaction is R30-19; it retains the validated R29 vegetation, façade, and
visual-fallback predecessors while adding R30 façade-distance and TreeRealism
v3 content. Workspace source, timestamps, or the familiar level name are not
publication evidence.

### Current R30 committed native identities — committed and reverified 8 September 2026

| Evidence | Status | Path | SHA-256 |
| --- | --- | --- | --- |
| R30-19 transaction | `COMMITTED` | `D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json` (65,411 bytes) | `F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F` |
| Run-04 recovery certification | `CERTIFIED_RECOVERED_AFTER_FAILED_RECEIPT` | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\recovery\r30-capture-20260908-04-recovery-02-certification\receipt.json` (8,224 bytes) | `2CBAEFE6AB2B268B21788D2BDA7239B44817897C36F392BDC6E95A7B613AE014` |
| Run-05 historical capture rollback | `ROLLED_BACK`; no pending/accepted capture | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-05\rollback.json` (571 bytes) | `9AEEFCA65ABBC3ACE2109AEA24C5DB6CB2966017591D4DB6783C1D5CFEFE0178` |
| Run-08 capture rollback | `ROLLED_BACK`; no pending/accepted capture | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-08\rollback.json` (618 bytes) | `067A5A1BB9B709A63D7E35DC7C375657FA048387405F94E181FE1C241CD08A2B` |
| Run-09 latest capture rollback | `ROLLED_BACK`; no pending/accepted capture | `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\rollback.json` (658 bytes) | `5827225197FE3BC4F57B28D36947F6C3642713410BD768B674C2F5D122144B87` |

The R30-19 receipt pins the currently committed map and editor plugins:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap` | `37,468,415` | `126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusion.dll` | `5,076,992` | `100B061CC5677508A67D644687923CA58F68F072305B6FF62B6BCD363D472028` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusionEditor.dll` | `8,448,000` | `3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553` |

The transaction explicitly records `VisualCaptureAccepted=false` and
`CaptureRevalidationRequired=true`. It proves a committed, runnable native
artifact, not that the result is visually accepted, photoreal, or hyperreal.

### R30 status — native commit, visual acceptance outstanding

R30-19 is the current native promotion. The same committed boundary carries the
texture-backed façade-distance implementation and TreeRealism v3's 13 response
materials, five rebound meshes, and 26 runtime MIDs. The previous
`r30-lookdev-20260906a` memory-guard rollback remains valid historical evidence,
but it no longer describes the current native state and must not be used to
claim that R30 is absent.

Capture run 04 failed and initially left incomplete cleanup evidence. The
separate recovery certification listed above proves that the six-root native
closure was restored exactly: 3,332 files, 16,529,511,957 bytes, manifest
SHA-256
`C8872BCAE283B311EBB08F4863A4F908B59141744C592D6CD903F6B234F951AD`,
with all missing, added, content, timestamp, and directory deltas equal to zero.
That receipt is recovery evidence only.

The `r30-capture-20260908-05` attempt is retained as historical failure
evidence. Later work added a run-local, hash-pinned seeded and prewarmed DDC,
bounded per-mesh probes, faster binary-marker scanning, and an eight-worker
shader path. Run 08 then ended `ROLLED_BACK` after its owned first prewarm
exited. The latest attempt, `r30-capture-20260908-09`, proved the exact
Columnar-tree DDC hit and eight-worker path but failed closed when
`AirSimTriadRuntime` optical-flow materials did not compile. Its rollback has
an empty `RollbackErrors` array and records that R30 content and
`AirSimTriadRuntime` were preserved. It published neither
`pending-visual-review.json` nor capture `commit.json`, so R30 visual acceptance
is still outstanding and R31 admission remains false. Diagnose that material
failure before using a genuinely new capture token; never reuse a completed
token, rerun the committed R30 transaction, bypass the applicable stage guard,
or copy staged files manually.

A deterministic CPU source-lookdev comparison is available at
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R30FacadeLookdev/OfflineSourceLookdev/r30_facade_cpu_source_lookdev.png`.
It uses the exact R29 facade OBJ/UV/material IDs and the admitted R30 textures,
but it is not an Unreal screenshot, provider composition, native integration,
or visual-acceptance artifact. Its measured mean absolute aligned-pixel change
from the source baseline is only `0.43/255`, with `0.98%` of aligned pixels
changing by more than eight levels. It confirms that R30 adds surface relief to
the separate facade-cue mesh; it does not fix the broad flat building shells.

The repository also contains the guarded five-pose Player0 capture harness,
`scripts/Capture-IstanaExploreV5DR30Player0Evidence.ps1`. It captures the exact
75 m, 20 m, 8 m, 2 m and MacDonald House oblique views from fresh cooked output
in a Development Game sandbox. It uses a run-local isolated, hash-pinned seeded
and prewarmed DDC, fully decodes and hashes the PNGs, and preserves the map. It
must be run only after an exact R30 `COMMITTED` transaction receipt is available
and before any R31 successor is promoted. Source readiness and a clean rollback
are not capture results; no accepted R30 Player0 image set currently exists.

### R31 broad-shell lookdev status — source-frozen, not native

R31 addresses the separate broad surroundings shell rather than the R29
façade-cue mesh. It retains the exact suppression-V2 geography, identity
component transform, `43,448` triangles and `17` semantic slots, and changes
only the component's 17 material overrides. The isolated asset contract is
frozen at `13,386` bytes with SHA-256
`6F2F348AB1D6DED49D3A8350C19552C700A2647917D68C0E3D9F1A8519BD7617`.
It defines one opaque DefaultLit master and four role instances using the
admitted Plaster, Stone and Slate texture sets. Its exact source closure is 15
files: one source OBJ row, five other source pins and nine texture pins. The
reused Unreal textures must also retain their exact single-source provenance,
valid stored MD5 matching the current SHA-256-pinned PNG, source shape and
render settings, while the shared texture tree remains immutable. This admits
the existing texture provenance; it does not prove byte-for-byte equality
between PNG pixels and embedded Unreal source pixels.

The deterministic source-only comparison is at
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev/OfflineSourceLookdev/r31_broad_shell_cpu_source_lookdev.png`.
On its fixed crop, the aligned mean absolute change from the flat shell is
`8.45/255`, and `41.82%` of pixels change by more than eight levels. Those are
diagnostic contrast figures, not a photorealism score. The image is not an
Unreal screenshot, native R31 integration, Cesium/provider composition,
actual-site comparison or visual-acceptance artifact. Visual acceptance is
false.

The R31 PowerShell parser and repository-only static self-check pass, and the
combined R25/R30/capture/R31 source regression set completes `134/134`. No
Unreal process was launched and no native file was written for R31. The
required native order is strict: obtain an exact R30 `COMMITTED` receipt, run
and accept the five-pose R30 Player0 capture, and only then run the guarded R31
transaction and a new R31 capture. Until that sequence succeeds, the exact
R30-19 map and DLL identities above remain current. R30 is native but not
visually accepted; R31 must not be presented as visible in the level.

### R32 medium-distance turf status — source-only, not native

R32 closes the lawn-detail handoff between the dense near grass and the flatter
inherited medium-range presentation. Its actor reads a validated copy of the
four exact R23 grass-profile rosters, selects ordinal zero and every fourth
transform in each profile, and preserves 4,608 existing world translations and
rotations exactly: 3,115 manicured, 994 humid, 288 shade, and 211 dry-edge.
Those instances are distributed over 12 profile/mesh buckets using the three
existing R29 modeled-blade meshes and four existing R29 turf materials. Local
tip height is bounded to 5--9 cm and the render-only layer follows the reused
material's calibrated fade from 65--90 m. It is hidden from scene captures and
owns no collision, navigation, sensor, RF, terrain, or geospatial authority.

This R32 implementation is repository source only. It has not changed the
native project or target map and has no accepted Unreal capture. Promotion is
ordered after accepted R30 and R31 native transactions and captures. The exact
contract and required visual gates are in
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/R32MediumDistanceTurf/README.md`.

### R33 Cesium World Terrain readiness — source-ready, not native

Use the
[Istana R33 Cesium readiness audit — 2026-09-08](ISTANA_R33_CESIUM_READINESS_AUDIT_2026-09-08.md)
as the controlling readiness record. The R33 runtime, context, editor and
Player0 capture-state boundaries now require valid, non-null opaque Cesium Ion
server objects before accepting Google/CWT pointer identity; equal null
pointers fail closed. Focused R33 source/native/capture tests pass `49/49`, the
adjacent R29/R32/context/R33 regression passes `93/93`, and both of the two
repository-only R33 static self-checks (transaction and capture) pass.

Cesium for Unreal `2.18.0` (`Version 78`, UE `5.5.0`) is installed and exposes
the API declarations used by R33. That is API availability only, not proof of a
TRIAD compile, native map promotion, provider connection, entitlement,
streaming, visual acceptance or runtime readiness. The current native map and
DLLs remain the exact R30-19 identities above: map `37,468,415` bytes / SHA-256
`126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7`,
runtime DLL `5,076,992` bytes /
`100B061CC5677508A67D644687923CA58F68F072305B6FF62B6BCD363D472028`,
and editor DLL `8,448,000` bytes /
`3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553`.
There is no accepted R30 capture and no R31/R32/R33 native transaction. Do not
rerun R30-19, lower the guards, or skip the mandatory human-accepted R30, R31,
and R32 capture sequence before R33.

Do not inspect, display or record provider token values or fingerprints. A
valid non-null shared server object does not prove a token exists or is valid,
does not prove entitlement to Google asset `2275207` or CWT asset `1`, and does
not prove provider terms, readiness or live streaming. No CWT height has been
sampled or persisted, no vertical datum or independent checkpoint accuracy is
resolved, and R33 is not live or simulation terrain. It remains an optional
visual reference with no collision, navigation, line-of-sight, sensor or RF
authority.

### Post-R33 tropical umbrella hero — source-only, not live

The [candidate](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroCandidate/README.md)
and [integration scaffold](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/README.md)
are repository source only. There is nothing to activate or showcase in the
current R30 level. Eligibility is limited to six source HISM instances already
native-classified as `umbrella`: four heritage and two R29 landmarks. All 720
unresolved V4-main raster rows are excluded. Their ordered route is
`C/B/B/B/A/B`, giving A1/B4/C1; this classification is not native candidate
integration or visual acceptance.

The private source path snapshots the exact 4+2 counts, all six ordered native
transforms, visibility and hidden-in-game state. Caller transform copies must
match bit-for-bit and the canonical before/after transform SHA-256 must remain
identical. Suppression is limited to the two presentation fields with
propagation disabled. On every later failure, the source path clears the
candidate, restores both source states, revalidates counts/transforms, and
attempts physical cleanup of both proven-empty-at-entry namespaces. This is
source-implemented fail-closed rollback, not native rollback evidence.

Editor materialize/swap, runtime selection and runtime activation are three
independent compile-time `false` gates. Their trust anchors remain invalid; the
sole reflected endpoint provides read-only receipt inspection, the private
materializer has no caller, and no package write, activation, map save or
numbered successor is authorized. The [sealed integration contract](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/tropical_umbrella_hero_post_r33_integration.source_contract.v1.json)
is 21,792 bytes with SHA-256
`C3CAD6385FD35AB648A2A7064B8BD14A9430227728282F367F942D9B1FCF9714`;
all 27 pins are exact. Focused tests pass `29/29`; independent adjacent
verification reports 67 passed and two expected native-only skips. Candidate
check and self-test both pass with byte-identical clean-build output. The prior
sole stale OuterContextVegetation R33 receipt chain is repaired through an exact
transitive reseal. The combined R33/outer/rim/promotion focused-plus-adjacent
suite is green at `83/83`. The outer candidate unittest passes `9/9`;
`--check` is `PASS`; and `--self-test` is `PASS`, with all five outputs
byte-identical.

Do not try to operate this scaffold. First complete the human-accepted R30
capture, R31 commit plus human capture, R32 commit plus human capture and R33's
eight-image human acceptance. A separate reviewed source change, recompile and
explicit successor are then required. Native compile/link, import/save/cold
reload, injected rollback after both suppression branches, fixed-view
comparison, explicit human visual acceptance, wind/alpha, LOD/culling,
shadow/subsurface and equal-condition frame-time/memory proof remain
outstanding. The candidate
carries no Rain Tree/current-inventory or survey claim and has no geography,
collision, navigation, line-of-sight, RF, sensor or terrain authority.

### Post-R33 graded turf presentation V2 — source-only, not live

The newer `Vegetation/GradedTurfPresentationIntegrationV2` does not replace or
change either placement owner. R29 still owns exactly 6,144 modeled-grass
placements and R32 exactly 4,608. The scaffold clones the exact accepted R23B
masked lawn overlay—including `BLEND_Masked`, the `0.5` clip and its 64-point
estate-boundary/core-feather OpacityMask graph—then substitutes only Base Color,
Roughness, Normal and Ambient Occlusion with the existing dual-phase Grass001
response. Specular and OpacityMask remain inherited, with no WPO, displacement
or PDO route. Its design keeps the ground response present through the 65--90 m
blade fade and the 95 m review probe.

This is not visible in the level. Runtime selection and activation are separate
compiled `false` gates; the exact `V5DGroundMacroVariationOverlay` material
snapshot/apply/restore path is private and uncalled. The one-material editor
materializer is private, non-reflected and blocked by distinct invalid
accepted-R33 and future-authorization anchors. A separate upstream Grass001
package byte/SHA-256 pin is also deliberately unset because no accepted native
OrdinaryDistance material exists yet. A future reviewed source change must pin
its clean cold-reloaded package; V2 then independently checks the complete
25-node payload/topology and allows only the four grafted plus exact inherited
OpacityMask/Specular roots. The focused source contract
passes `14/14`, but no material has been created, saved, cold-reloaded, bound,
captured or accepted. Continue to describe the open R30 level as the current
smooth ordinary-distance lawn, not as this 0--95 m V2 presentation.

The R29 predecessors and current R30 transaction explicitly withhold visual
acceptance. R30-19 records `VisualCaptureAccepted: false` and
`CaptureRevalidationRequired: true`. They prove native promotion and validation,
not that the scene is photorealistic or hyperreal. The last successful R27
raster set remains useful as predecessor-only grass/tree evidence, but it does
not visually certify the current R30 façade, tree-response, or terrain
composition.

The earlier guarded actual-file RF receipt remains at
`D:\triad\TRIAD\Saved\TRIAD\RFActualFileNativeTransactions\rf_actual_fix_20260905T184017Z\receipt.json`.
It records `PASS` with exactly one success for
`TRIAD.RF.IndexedGeometryQuery.OneKilometreV2ActualFile`. Exact deterministic
offline replay of the same hash-pinned actual-file workload performs 29,904,936
containment triangle checks under its explicit 30,000,000-check cap; the
ordinary loader default remains 20,000,000. The completed native load produces
exactly 16,383 BVH nodes. Its runtime-DLL identity predates and is superseded by
the R30 build, so it is not a validation of the current R30 DLL. It remains
native geometry-loader evidence only—not field calibration, live deployed-
scenario proof, or RF performance validation against measurements. R30 leaves
the RF inputs and authority unchanged.

The successor serializes `MaximumSimultaneousTileLoads=12` and
`PreloadSiblings=false`; ancestor preloading remains enabled. This is a bounded
provider-request policy, not evidence that the provider finished loading or
that it will not return HTTP `429` responses.

### Cesium and terrain accuracy boundary

For an authorised operator setup, open Unreal's **Window > Cesium** panel and
use the plugin's account/server sign-in. Never paste a token into PowerShell,
the scenario file, source control, a screenshot, a chat, or a log. Select the
existing Google Photorealistic tileset in the World Outliner and verify only
non-secret state: source **From Cesium ion**, asset ID `2275207`, and a non-null
Cesium ion server reference. Do not expand, copy, print, hash, or record a token
field or fingerprint.

In the viewport, provider-backed city context should refine when network access
and entitlement work. Use the Output Log only to check non-secret outcomes:
HTTP `401`/`403` indicates authorization/entitlement failure, while repeated
`429` indicates throttling. A visible local fallback, a non-null server object,
or global load progress alone is not proof of a successful provider stream.

The current level owns exactly one `CesiumGeoreference`, centred at longitude
`103.84288055`, latitude `1.30709615`, ellipsoidal height `47 m`, and exactly
one streamed `Cesium3DTileset`. That tileset is **Google Photorealistic 3D
Tiles**, Cesium ion asset `2275207`; it is not a separately instantiated Cesium
World Terrain tileset. Treat it as georeferenced visual context only.

The provider-handoff state is a presentation heuristic. `ProviderReady`
requires the tileset's global load progress to remain at or above `98%` for
three consecutive `0.5 s` samples, and restores the local fallback below
`90%`. It does not prove local ground coverage, elevation accuracy, a vertical
datum, collision quality, or suitability for line-of-sight or RF analysis.

The R29 local terrain mesh is a `2 km`, `129 x 129` visual fallback derived
from the roughly `30.9 m` Copernicus DEM GLO-30 product. It is smoothed and
centre-relative; it has no geoid-to-ellipsoid conversion, only `59.8576%`
valid height-error coverage in the admitted source, no surveyed control
points, no checkpoint residuals, and no absolute placement authority. The
pre-existing source terrain remains the collision owner. Neither the Google
tiles nor the R29 fallback has collision, navigation, terrain, sensor, RF, or
absolute-height authority.

Do not describe this composition as Cesium World Terrain, surveyed terrain, or
simulation-grade elevation. To admit terrain for physics, occlusion, or sensor
analysis, obtain an authorised local DTM or survey surface, record its
horizontal and vertical coordinate reference systems, perform the explicit
vertical-datum conversion, validate independent checkpoints against declared
residual thresholds, and version a separate deterministic collision/LOS
surface. Cesium World Terrain may be added as another visual source if desired,
but its presence alone would not satisfy those accuracy gates.

A strict `ProviderReady` vegetation attempt with run token
`throttle12_ready_20260905T1618Z` reached only about `64%` observed load
progress before repeated HTTP `429` responses prevented the ready gate. It was
stopped cleanly and did not produce accepted `ProviderReady` proof. The exact
failure-evidence log is:

```text
D:\triad\TRIAD\Saved\Logs\Codex_V5D_VegetationProvider_throttle12_ready_20260905T1618Z.log
```

It is `1,947,349` bytes with SHA-256
`15AA867A78D28C2636A4EF378E22F75CBD21F2F000A10DAC3ACE55918A6C33E6`.
Do not present that attempt as a provider-ready screenshot run. A new strict
run must satisfy the gate and publish a passing manifest before making that
claim.

A subsequent bounded retry run used token
`ready_retry_20260905T1643Z`. Its first attempt received zero HTTP `429`
responses, briefly reached about `60.6%`, and finished the full readiness
timeout at `47.254%`. Because the failure was incomplete provider refinement
rather than explicit rate limiting, the fail-closed orchestrator correctly did
not launch its second attempt. It published no PNG, leaf manifest, or retry
receipt. The cleanly closed attempt log is
`D:\triad\TRIAD\Saved\Logs\Codex_V5D_VegetationProvider_ready_retry_20260905T1643Z_a1.log`
(`3,190,486` bytes, SHA-256
`A29DF7E8E03841960D6F683939CFC3F23E1B0C754B0D831AEAEA1E7B5E20CC57`).
After shutdown there were zero Unreal editor processes and zero listeners on
port `30010`. This second run is also diagnostic failure evidence, not a
provider-ready visual acceptance result. Both attempts predate R29, and Cesium
`ProviderReady` remains unproven for the current successor.

Do not start another strict ProviderReady retry on the current machine state.
The bounded first-pose diagnostic now has an authoritative live receipt at:

```text
D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_cesium_first_pose_memory_queue_diagnostic_v1_r27_cesium_diag_20260906T0730SGT.json
```

It is `ABORTED_BY_MEMORY_GUARD_NON_PROOF`, with SHA-256
`B91A4F9F1EAFFEB783F09A03BE974B9696DC377A5E533E8FD39032ECDFDD9940`.
The independent 100 ms watchdog contained the exact owned UE5.5 helper when
system free virtual/commit crossed the 6 GiB safety floor: the trigger sample
was `6,435,196,928` bytes with `1,920,385,024` helper-private bytes, and the
minimum observed free virtual/commit was `6,254,166,016` bytes. This happened
before map validation, PIE, first-pose acknowledgement, Cesium resume, or any
selection-telemetry sample, so the run measured startup headroom only. It did
not test whether the scheduling-only
`EditorWorldSuspendedPoseBeforeResumeV1` profile improves provider refinement.
The R27 map and DLL pins and the protected UE5.4 CAPSTONE identity remained
unchanged; no provider setting or map was saved; no PNG was published; and no
UE5.5 helper or Remote Control listener remained after containment.

Treat the receipt as decisive evidence that available system commit headroom
was insufficient for this bounded startup while the protected CAPSTONE session
was open—not as provider readiness or visual proof. Do not weaken the guard or
blindly rerun it. First create materially more commit headroom (for example, by
having the CAPSTONE operator close that session when safe, or by increasing the
Windows page file and restarting), then rerun the same exact-pin diagnostic.
Following that receipt, the wrapper was strengthened with a fixed 10 GiB
free-virtual/commit admission floor before `Start-Process`; the independent
continuous watchdog still enforces its original 6 GiB floor after launch. A
preflight refusal is therefore expected while baseline headroom remains below
10 GiB and does not launch Unreal.
Only a safe completed diagnostic can justify a subsequent strict
`ProviderReady` attempt.

The current authored visual composition is:

- the retained tree-realism layer at all 729 source-tree positions, with five
  isolated high-resolution derivatives, automatic screen-size LOD, source LOD0
  available near camera, and original PBR/wind materials; plus R29's seven
  landmark tree anchors spanning spreading, dome, high-fork, columnar, and palm
  silhouettes;
- the inherited 18,432-instance broad turf layer plus R29's 6,144 modeled
  landmark grass instances, three blade-mesh variants, 12 profile buckets, and
  four material responses designed to remain readable through 90 m;
- a dedicated MacDonald House study proxy with red-brick/white-trim massing,
  checked from façade-close, front-corner-oblique, and streetscape-context
  views;
- a dedicated Temasek Shophouse study proxy with a three-storey row,
  individual façade and roofline articulation, and the same three-view check;
- broader streamed city context outside the clipped authored core, with the
  local dated LoD2 surroundings shown while the provider is not ready. R29 adds
  a 256,850-triangle façade overlay across 1,174 selected building parts, with
  20,011 four-sided apertures and rails, 7,881 sills, 46 awnings, and 28 balcony
  proxies;
- Google Photorealistic 3D Tiles through Cesium ion asset `2275207` as the
  preferred visible provider, with a local 2 km, 129-by-129 Copernicus DEM
  mesh used only as a complementary visual fallback;
- a render-only 1,000--1,250 m outer annulus that covers the inherited terrain
  rim during loading. It is a visual continuity surface, not terrain data.

### Main Istana grounds versus surrounding buildings

Treat these as two different quality tiers when presenting the scene:

- **Main Istana grounds:** this is the intended visual focus. The authored
  terrain, lawn, mature-tree silhouettes, shadows, palace composition, and
  dedicated landmark landscaping receive the closest presentation work. R29
  increases modeled grass density and medium-range material response and gives
  the seven landmark trees more varied tropical silhouettes; R30-19 adds its
  TreeRealism v3 response closure. Manual native-resolution review of the
  current R30 bundle is still required; none of its receipts certifies
  photorealism or hyperrealism.
- **Surrounding buildings:** buildings are present around the grounds, but
  their base shells are still simplified, dated LoD2 context meshes. R29 makes
  many more parts read as buildings through physical aperture/frame/sill and
  roofline cues, while MacDonald House and the Temasek Shophouse remain the two
  more articulated dedicated study proxies. R30-19 adds texture-backed distance
  cues to the separate inherited façade mesh, but the latest run09 capture
  failed closed before review, so this is native-state truth rather than
  accepted appearance evidence. It does not turn the broad shells into measured, weathered, or
  as-built buildings. The source-frozen R31 pass extends texture, weathering and
  procedural façade-depth intent to the broad 17-slot shell, but its checked-in
  CPU comparison is diagnostic only and it is not visible in the native map.

Post-R33 source candidates and dormant, fail-closed integration scaffolds now
exist for building optics/contact composition and public-road normal
continuity. `BuildingOpticsContactCompositionIntegration` resolves the earlier
either/or material choice in the exact order R31 → all seven optics channels →
bounded foot contact. Contact changes only Base Color, Roughness and Ambient
Occlusion; tangent Normal, Metallic, Clear Coat and Clear Coat Roughness pass
through unchanged. The source keeps the R31 43,448-triangle, seventeen-slot,
1,388-group identity geometry and prepares one isolated master plus seventeen
instances. Both runtime gates remain compiled `false`, and its private
materializer is uncalled behind invalid trust anchors, so no composed material
is live. Its exact normalized 42-node validation compares every unchanged R31
node class, payload and input edge plus every root selector, including Specular
and the original `R31.SurfaceRoughnessA -> MP_Roughness` route. All 42 nodes
must be root-reachable; orphan and custom-output substitutions are rejected.
Custom Surface inputs 23--25 require exact names, source/output selection,
independent `FExpressionInput::InputName` values and masks. All seven Surface
outputs require exact ordered names, zero masks and
`bShowOutputNameOnPin=true`. The eleven presentation override families are
three-way exact across candidate, optics and R31. Editor checks deny direct
physical material, the separate `PhysMaterialMask` and physical-material maps
through that chain; runtime checks also deny direct `PhysMaterialMask` and
effective `GetPhysicalMaterialMask()` for candidate and preferred-optics Clear
Coat rosters.

The independently rerun source results are `17/17` focused and `68/68`
combined, including `51/51` adjacent optics/contact/R31 checks. The sealed
integration contract is exactly 12,632 bytes with SHA-256
`021C445DC42BD8461FE2741792C5E013D3B55F86DBA6FFC050C17760AEA09E6A`.
These counts are source-contract evidence, not live-render acceptance. No
Unreal launch, native UBT/UHT compile/link or automation run, shader compile,
materialize/save/cold reload, map or component binding, source-plane alignment,
capture, performance proof, Nanite/raster parity or human visual acceptance
has occurred, and there is no visible building change to operate or showcase.

The junction scaffold prepares two isolated meshes with only 355 indexed
normal rows changed; runtime selection/activation remain compiled off and
nothing is bound to the map. The separate 950--1,000 m rim recipe is still
explicitly fail-closed: its current direct-source pins are repaired, but native
topmost-renderer ownership and historical cooked-provenance admission remain
open. The `VQSP20260905` PublicRealm closure now reproducibly supplies its ten
evolved historical promotion inputs from a verified 52-file completed snapshot,
but that is replay isolation only and does not promote current content. None of
these candidates or closures is a new native asset or visible change in the
current map, so do not use them when describing the live R30 appearance.

For the clearest existing provider-backed city view, open
`D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_r23_managed_turf_provider_skyline_01_r23_final_c_20260903_0007.png`.
It is validated R23 skyline evidence, not a current R30 beauty-capture claim.
Compare it with the source-only R31 façade intent at
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev/OfflineSourceLookdev/r31_broad_shell_cpu_source_lookdev.png`;
the latter must not be presented as an Unreal or Cesium composite.

The active V2 locally suppressed surroundings derivative contains 1,386
visible features, 1,388 polygon parts, and 43,448 triangles. It removes exactly
96 triangles belonging to the two known coarse landmark shells
(`OSM:way:46521250` and `OSM:way:1551538490`) and the confirmed floating block
(`OSM:way:429681826`). The dedicated MacDonald House and Temasek overlays are
therefore the sole local fallback renderers for those landmarks. V2 does not
convert the remaining surroundings into survey-grade building models. R29
adds its separate façade mesh over selected context parts and does not grant
the renderer physical-material, collision, sensor-occlusion, or RF authority.

These changes are presentation-only. The trees are not a botanical inventory,
the buildings are not survey or as-built models, and the materials are not
field calibrated. The successor and its screenshots establish no collision,
navigation, terrain, sensor-occlusion, sensor, or RF geometry/material
authority. Cesium content is used only as visible raster background pixels; no
provider geometry is exported, traced, analysed, derived, or baked.

The main grounds are therefore the stronger part of the scene. The completed
R27 technical capture gives archival predecessor-only grass/tree files for
visual review, but its classification is
`R27_TECHNICAL_RASTER_CAPTURE_COMPLETE_MANUAL_VISUAL_ACCEPTANCE_PENDING`.
It cannot certify the R30 map. Close lawn views can still look flatter or more
repetitive than a measured blade-level surface. The wider buildings have more
façade geometry and the separate R30 distance-cue material closure, but broad
generic shells, sparse street dressing, procedural vegetation, terrain-contact
artefacts, and distant culling can remain visible.
Do not describe either tier as a photogrammetric, surveyed, or hyperreal
digital twin.

## Review archived R27 raster evidence

This section documents the last completed R27 predecessor capture; it is not a
current R30 visual-acceptance workflow. Review the already-produced files below.
Do **not** run the historical R27 commands against the current R30-19 map and
DLLs: their exact R27 pins must fail closed. Reproducing R27 requires an
independently restored, exact R27 checkout/native closure and explicit
maintainer authorization; it is not part of the beginner showcase.

The historical workflow required all six R27 identities and the exact commit
receipt rather than wrapper defaults.

All three wrappers also fail closed around the V2 suppressed-surroundings
boundary asset at
`LocalFallbackSuppressionV2/SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset`.
Their static/runtime contract requires
`currentContextSuppressionContract=local_fallback_suppression_v2`, and their
evidence manifests record the exact file identity under
`SuppressedFallbackMeshAssetPin`. A V1 asset or V1 contract marker is not valid
successor evidence.

Historical reference only — do not execute on the current R30 native state:

```text
$capturePins = @{
  ExpectedMapBytes = [int64]36335930
  ExpectedMapSha256 = '9C9660B02F3B9FBF8C679182E8EB39B8FECD0A618AECA6ED546AAD39E0C895E5'
  ExpectedRuntimeDllBytes = [int64]4676096
  ExpectedRuntimeDllSha256 = 'C341A9F2583248824911EBB5904EC357E0DCB840304C637D87FD0CE87E67B910'
  ExpectedEditorDllBytes = [int64]7781888
  ExpectedEditorDllSha256 = '30569F04A956FDCC24F5AE8C0A99D1B70D86C03208E9497EE9BBD8480ABCC2B6'
}

$r27Receipt =
  'D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DLandmarkVegetationR27V1\r27_visual_20260906T0701SGT\commit.json'
$r27ReceiptSha =
  '913206E0A914425145D71E973018B7807F4E1DAB9CE1D500B30AB30BD081995E'

function Invoke-V5DVisualEvidenceBatch {
  param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('ProviderFallback', 'ProviderReady')]
    [string] $Mode
  )

  $runToken = 'postmig_' + $Mode.ToLowerInvariant() + '_' +
    [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')

  & '.\scripts\Capture-IstanaExploreV5DVegetationProviderEvidence.ps1' `
    @capturePins -RunToken $runToken -ProviderEvidenceMode $Mode `
    -TelemetryDwellSeconds 12 -RequireLandmarkVegetationR27 `
    -R27CommitReceiptPath $r27Receipt `
    -ExpectedR27CommitReceiptSha256 $r27ReceiptSha

  foreach ($wrapper in @(
    '.\scripts\Capture-IstanaExploreV5DMacDonaldHouseR24Evidence.ps1'
    '.\scripts\Capture-IstanaExploreV5DTemasekShophouseR24Evidence.ps1'
  )) {
    & $wrapper @capturePins -RunToken $runToken `
      -ProviderEvidenceMode $Mode -TelemetryDwellSeconds 12
  }
}
```

### Completed R27 technical capture

The completed `TelemetryOnly` run used token
`r27_grass_visual_20260906T0710SGT`. Its manifest is:

```text
D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_vegetation_provider_evidence_r27_grass_visual_20260906T0710SGT.json
```

It is `441,373` bytes with SHA-256
`0BC8E95310A3BDB85FC963D570227335970B45B7B4EFEC3408CD1446EB2899D5`,
reports technical `PASS`, and contains seven distinct 2560 x 1440 captures.
Open these four first:

```text
D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_diagnostic_vegetation_range_050m_r27_grass_visual_20260906T0710SGT.png
D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_diagnostic_vegetation_range_012m_ground_grazing_r27_r27_grass_visual_20260906T0710SGT.png
D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_diagnostic_vegetation_range_008m_r27_grass_visual_20260906T0710SGT.png
D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_diagnostic_vegetation_range_002m_r27_grass_visual_20260906T0710SGT.png
```

Use `050m` as the mid-range grounds/context view,
`012m_ground_grazing_r27` as the primary grass-readability review, `008m` as
the close transition view, and `002m` only as a material/mesh inspection. The
manifest explicitly says manual visual acceptance is pending. A technical
`PASS` means the exact R27 artifacts were captured cleanly; it is not a
photorealism, hyperrealism, `ProviderReady`, or field-calibration claim.

For the historical record, a complete R27 acceptance would have invoked one
full batch under each state, sequentially, from an exact R27 restoration:

```text
Invoke-V5DVisualEvidenceBatch -Mode ProviderFallback
Invoke-V5DVisualEvidenceBatch -Mode ProviderReady
```

`ProviderFallback` requires consecutive exact-pose readbacks with
`providerReadyForProof=false` and the local fallback visible. It is classified
`STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF`.
`ProviderReady` requires the strict global ready gate and the local fallback
hidden; it is classified
`PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED`. A provider-state
timeout means the requested state was not evidenced. Do not relabel the run or
switch to `TelemetryOnly` merely to obtain a pass.

Even a `ProviderReady` pass proves only the wrapper's strict global provider
gate for that run. It does not prove landmark-specific readiness, dedicated
provider exclusion, or provider geometry quality.

Each successful R27 mode produces seven vegetation views, including the
dedicated 12 m ground-grazing review pose, plus three MacDonald House views,
three Temasek Shophouse views, and one JSON evidence manifest per wrapper under
`D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D`. Use the returned
`EvidenceManifest.Path` and each manifest's `Captures[].Png.Path` to open the
actual files; do not guess names. Before accepting a batch, require:

- manifest `Status: PASS`, the requested `ProviderEvidenceMode`, and its exact
  evidence-mode classification;
- map and both DLL pins equal to the R27 commit receipt, with the receipt's own
  path and SHA-256 pinned by the vegetation wrapper;
- `SuppressedFallbackMeshAssetPin.Path` names the V2 suppressed-surroundings
  asset and its byte count/SHA-256 remain stable throughout the live run;
- seven vegetation captures and three captures for each landmark, all with
  distinct PNG hashes;
- `GracefulQuitRequested: true`, `ForcedContainment: false`, editor exit code
  `0`, and final non-protected-editor and RC-listener counts both `0`; and
- the manifest's visual-only authority fields and all provider non-export,
  non-trace, non-analysis, and non-bake fields preserved.

That archived capture suite was visual QA, not the simulated-ingress
demonstration. Do not rerun it on the current R30 state. Use the interactive
level and desktop dashboard sections above for drone tracks and detections.

## Placement-study workflow and results

Regenerate the four-class study to a new filename:

```powershell
Push-Location .\core
$previousPythonPath = $env:PYTHONPATH
try {
  $env:PYTHONPATH = 'src'
  python -m singapore_sensor_fusion.placement.demo_study `
    --output '.\reports\istana_four_class_demo_study.v3.json'
}
finally {
  $env:PYTHONPATH = $previousPythonPath
  Pop-Location
}
```

The writer is create-new and refuses overwrite. The checked-in V2 study has:

- all 29 Stage-0 evidence artifacts unresolved, all 10 readiness gates
  blocked, one mount region ineligible, and five approvals incomplete;
- actual-file hash/count validation for OneKilometreV2: 1,094 solids, 13,936
  surfaces, 23,496 vertices, and 42,700 triangles;
- explicitly synthetic radar, passive-RF, RGB, and event-camera calibration
  records;
- 80 versioned candidate x trajectory x sensor-class rows;
- deterministic greedy/local-search and robust branch-and-bound results;
- an exact study optimum of `demo-south + demo-west`, cost `4.4`, with the
  branch-and-bound proof completed in 17 explored nodes; and
- equal 48-evaluation budgets for bounded derivative-free pose search and a
  seeded evolutionary challenger.

The study artifact digest is
`6c6cad94da9ec4578227bfc596b3ea1748243a67d16779c0014772a4ce2e3975`;
its precompute digest is
`fa974546812886cc81bfc920686d7c2a863d4d4e5bcb04c810b214f5d573b78a`.

Adapt that exact study result into a create-new dashboard artifact with:

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

The checked-in V2 adapter output maps only the proven `demo-south` and
`demo-west` candidates, exposes radar/passive-RF/RGB/event-camera at both
sites, and binds event time and evidence index to the versioned precompute
rows. Its digest is
`6949355f49492490752b453b49231db420446637818a2461e544254976524dab`.

This completes a deterministic demo vertical slice, not Stage-0 approval.
OneKilometreV2 files load and cross-check, but the demo precompute remains a
kinematic surrogate and does not query that RF geometry for propagation.
Measured model calibration, approved mounts/hardware/privacy/RF constraints,
exact simulator replay, and independent review remain required before a
production solve or field recommendation.

### Status of the six-step placement plan

| Plan item | Current demonstrable status | Remaining production work |
| --- | --- | --- |
| 1. Resolve Stage-0 inputs | A DRAFT contract and fail-closed readiness receipt exist. | All 29 artifact bindings remain unverified, all 10 gates are blocked, one mount region is ineligible, and five approval roles are incomplete. |
| 2. Actual-file RF test and sensor calibration | The hash-pinned OneKilometreV2 geometry/material/scene files load and cross-check; a guarded native actual-file geometry-query receipt exists. | The demo does not use the geometry for RF propagation. Radar, passive-RF, RGB, and event-camera records are synthetic assumptions, not measured calibrations. |
| 3. Versioned precompute | The checked-in demo contains 80 versioned candidate-by-trajectory-by-class rows. | Recompute against approved candidates, frozen trajectories, measured models, and exact simulator/RF runs. |
| 4. Deterministic baseline | Greedy/local search returns the two-site demo layout. | Re-run against the frozen production corpus and constraints. |
| 5. Primary robust static solver | Deterministic branch-and-bound proves the tiny frozen demo optimum in 17 explored nodes. | This is not a production-scale MILP/CP-SAT solve or a field recommendation. |
| 6. Continuous refinement and challenger | The demo gives derivative-free and seeded evolutionary paths the same 48-evaluation budget. | Current refinement is a bounded scalar-pose demonstration with a synthetic objective; replace it with approved continuous pose variables and simulator-backed scoring. |

Do not tell an audience that Stage-0 is complete, that the actual-file RF
geometry is field-calibrated, or that the two displayed sites are approved
mounts. The correct description is **deterministic demo recommendation over a
frozen synthetic precompute**.

## Live telemetry path

Replay mode stays offline. For the separately labelled graphical live overlay,
start Unreal Play first, then run these in separate terminals.

Terminal 1 -- validate and fuse the latest Unreal snapshot:

```powershell
Push-Location .\core
$env:PYTHONPATH = 'src'
python -m singapore_sensor_fusion.layered_runtime `
  --input 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json' `
  --output 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_layered_snapshot.json' `
  --interval 0.25
```

Terminal 2 -- expose the newest bounded snapshot over loopback GET only:

```powershell
Push-Location .\core
$env:PYTHONPATH = 'src'
python -m singapore_sensor_fusion.c3_bridge `
  --saved-dir 'D:\triad\TRIAD\Saved\SingaporeSensorFusion' `
  --host 127.0.0.1 `
  --port 8765
```

Inspect it from a third terminal:

```powershell
Invoke-RestMethod 'http://127.0.0.1:8765/health'
Invoke-RestMethod 'http://127.0.0.1:8765/api/snapshot' |
  ConvertTo-Json -Depth 12
```

Then open the graphical live overlay. The replay path is required here as the
frozen recommendation reference; it does not drive the live tracks:

```powershell
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -LiveUrl 'http://127.0.0.1:8765/api/snapshot' `
  -LivePollSeconds 0.25 `
  -LiveTimeoutSeconds 0.75
```

Hollow blue squares are the frozen recommendation. Solid nodes, current tracks,
and cue links come only from a fresh loopback snapshot. The preferred two-site
study and the current four-node Unreal scenario normally produce a visible
**REFERENCE MISMATCH**; that is correct and prevents a false claim that live
detections prove the two-site recommendation. A match is shown only when all
node IDs and coordinates agree, including a compatible `WGS84_ELLIPSOID`
height reference. Delayed or stale data retains only muted node context and
suppresses tracks/cues; unavailable or malformed data clears the live layer.
The client is GET-only, accepts only the loopback `/api/snapshot` path, rejects
redirects, ignores all environment/system proxy settings, caps each response at
2 MiB, and bounds both timeout and poll rate. Bridge-reported source and track
ages continue advancing on a local monotonic clock after receipt. A stalled or
overlong poll therefore expires tracks, cue links, and **REFERENCE MATCHED**
authorization without waiting for another response. Exact repeated bodies do
not reset that clock; malformed, old, future, changed-at-the-same-timestamp, or
retrogressing `generatedAtUtc` envelopes clear the live layer.

Use the same checks without opening Tk by adding `-ValidateOnly`. The JSON
summary reports `liveState`, `placementComparison`,
`referenceDeploymentClaimAuthorized`, and `detectionRenderingEnabled`.

## What a detection means

- In the primary four-class replay, a cue exists only when its versioned
  precompute row has a non-null first-detection time. The viewer adds a
  one-second display-only timeline offset; it is not sensor dwell or measured
  latency.
- In the legacy replay, detection is a deterministic distance/weather range
  gate with explicit synthetic per-modality acquisition delays.
- Both replay models omit terrain/foliage occlusion, multipath, antenna
  patterns, camera FOV, and PTZ slew. Their `confidence` values are evidence
  indexes, not calibrated probabilities.
- In Unreal, RF and search-radar results come from the simulation models and
  current sensor state. The layered runtime admits only validated, fresh
  observations and requires independent sensor families for corroboration.
- Scenario route, actor name, and authored role may be displayed as exercise
  context but cannot create or rank a contact.
- Every path is detection-only. No response or engagement action is generated.

## Troubleshooting

- **A script or replay path is not found:** reset the terminal to the repository
  root before using any relative command:

  ```powershell
  Set-Location 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo'
  Test-Path '.\scripts\Show-IstanaPlacementDashboard.ps1'
  Test-Path '.\core\reports\istana_four_class_dashboard_replay.v2.json'
  ```

- **PowerShell blocks a checked-in script:** for this terminal only, use
  `Set-ExecutionPolicy -Scope Process Bypass`, then rerun the command. Do not
  change machine-wide policy for the showcase.
- **Dashboard reports no usable Python:** pass a Python 3.11+ executable with
  both `tkinter` and `numpy` using `-PythonExecutable`. The launcher probes each
  candidate and skips stale virtual environments.
- **The dashboard closes during `-ValidateOnly`:** that is expected. Validation
  is headless. Run the second command without `-ValidateOnly` to open the GUI.
- **The scenario builder refuses to overwrite:** this is an evidence safeguard.
  Generate a new `$showcaseStamp` and `$scenarioPath`; do not delete or edit the
  existing scenario just to reuse its name.
- **Scenario is disabled:** create a new config with `-Enable`; do not edit the
  base file.
- **No Unreal operator panel:** the chosen config needs
  `OperatorObserver.bEnabled: true`; generate a new copy with
  `-EnableOperatorObserver`.
- **Unreal opened but nothing moves:** the launcher opens the editor; it does
  not start Play-In-Editor. Wait for the map to finish loading, then press the
  green **Play** button.
- **No Unreal detections:** confirm Play is running, the config has
  `bSpawnDemoTargets: true`, at least one `DemoTargets[].bEnabled` value is
  true, all four
  sensor nodes are enabled, and the snapshot is fresh. Inspect the generated
  config and snapshot time without modifying either file:

  ```powershell
  $config = Get-Content -Raw -LiteralPath $scenarioPath | ConvertFrom-Json
  [pscustomobject]@{
    Enabled = $config.bEnabled
    SpawnDemoTargets = $config.bSpawnDemoTargets
    EnabledDemoTargetCount = @($config.DemoTargets | Where-Object bEnabled).Count
    ObserverEnabled = $config.OperatorObserver.bEnabled
    SensorNodeCount = @($config.SensorNodes).Count
  }
  $snapshot = Get-Item 'D:\triad\TRIAD\Saved\SingaporeSensorFusion\latest_rf_snapshot.json'
  [pscustomobject]@{
    LastWriteTime = $snapshot.LastWriteTime
    AgeSeconds = [math]::Round(((Get-Date) - $snapshot.LastWriteTime).TotalSeconds, 1)
  }
  ```

  Never infer a detection from the drone mesh alone.
- **Tree/grass, landmark, or context-façade presentation looks unchanged:**
  compare the native map byte count/SHA-256 and both DLL identities with the
  current R30-19 receipt and the exact pins in
  [V5D current R30 native scene status](#v5d-current-r30-native-scene-status).
  Do not use the R27 raster manifest to certify R30, and do not infer promotion
  from the package name or timestamps. Even matching pins prove a committed
  artifact, not visual acceptance.
- **A capture wrapper refuses to start:** close non-protected Unreal Editor
  processes and verify port `30010` has no listener. The wrappers must own the
  helper and Remote Control endpoint exclusively.
- **A capture output already exists:** keep the existing evidence immutable and
  run the complete batch with a fresh `RunToken`.
- **A map or DLL pin is rejected:** stop. Resolve the receipt/build mismatch;
  do not copy the current file's hash into the command merely to bypass the
  check.
- **The R30 lookdev is requested or appears absent:** R30-19 is already the
  current committed native state; do not rerun its transaction. Its visual
  acceptance remains outstanding. Run 05 is historical; the latest run09
  attempt proved eight shader workers and the exact Columnar-tree DDC hit, then
  failed closed on AirSim optical-flow material compilation. It rolled back
  cleanly and emitted no pending or accepted capture receipt. Preserve that
  evidence, diagnose the material failure, and use a genuinely new capture
  token only after the exact R30-19 and run-04 recovery-certification pins pass.
  The R30 capture harness has no fixed process-RAM cap but keeps a 2 GiB
  emergency system-commit reserve; R31--R33 retain 10/6/12-GiB guards. Never
  bypass the applicable guard or copy staged files manually.
- **`ProviderFallback` or `ProviderReady` times out:** the requested exact
  provider state was not held. Preserve the failed log, correct the provider
  condition, and rerun all three wrappers with a fresh token in the intended
  mode. `TelemetryOnly` is diagnostic, not substitute acceptance evidence.
- **A capture has a black outer foreground or duplicate landmark shell:**
  inspect the manifest's suppressed-surroundings and outer-ground asset pins,
  its provider-state readbacks, and the hybrid validation report. Global load
  progress alone is not landmark-specific readiness proof.
- **Dedicated RF config is rejected:** its exact expected world package must be
  `/Game/Maps/Istana_PublicView_Explore_v5d_hybrid`; never bypass the launcher
  check.
- **Loopback health fails:** make sure the bridge terminal is still running,
  then check who owns the port:

  ```powershell
  Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue |
    Select-Object LocalAddress, LocalPort, OwningProcess
  Invoke-RestMethod 'http://127.0.0.1:8765/health'
  ```

  A healthy API is necessary but not sufficient for a current graphical track.
  Check the dashboard's freshness, provenance, and reference-comparison labels;
  stale/unavailable tracks are intentionally suppressed.

Stop Unreal with `Esc`, stop the Python live processes with `Ctrl+C`, and close
the desktop dashboard normally.
