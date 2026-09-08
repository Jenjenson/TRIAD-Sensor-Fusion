# Istana newcomer quickstart

Use this guide to install and open the delivered Istana environment, run its
simulated drone, see sensor evidence, and open the separate placement
dashboard. It covers both the audited workstation and a release-bundle install
on another Windows device. It is the short operator path; the
[full operator manual](ISTANA_OPERATOR_MANUAL.md) contains the evidence model,
limitations, and detailed troubleshooting.

This workflow is detection-only and simulation-only. It does not approve a
physical sensor mount, provide a field-calibrated probability of detection, or
perform an engagement action.

## Know what is actually installed

As verified on 8 September 2026, the runnable native project is the committed
**R30-19 content scene plus its UE 5.5 cooked-runtime compatibility hotfix**.
That is the environment a newcomer can run now. The hotfix preserves the R30
map, assets, geography, and simulation authority; it implements compatibility
handling for editor-only material-slot metadata and TreeRealism material
delegation in cooked builds. Final run 14 completed the full cook but later
failed closed in the packaged Game after detecting runtime tree-override
ownership drift. It produced no reviewable five-view set, pending-review
receipt, or human visual acceptance. A committed editor build is not a claim
that the scene is provider-ready, photoreal, or hyperreal.

| Revision | Current status | What that means when the map opens |
| --- | --- | --- |
| R29 | Committed predecessor retained by R30 | The R29 tropical vegetation, surrounding-façade geometry, and Copernicus-derived visual fallback remain part of the R30 successor. |
| R30 + UE 5.5 hotfix | Native, committed, and runnable in the editor; final packaged validation failed closed and human visual acceptance does not exist | R30-19 adds texture-backed façade-distance and TreeRealism v3 content. The code-only hotfix preserves that content. Run 14 cooked successfully but did not establish stable packaged-runtime ownership or complete the five required views. |
| R31 | Source-frozen, not native | The broad surrounding-building shell upgrade is **not** visible. |
| R32 | Source-only, not native | The medium-distance turf upgrade is **not** visible. |
| R33 | Source-ready, not native | Cesium World Terrain is **not** an active terrain layer in this map. Provider entitlement, streaming, native integration, and visual acceptance remain unproved. |
| Post-R33 tree, grass, and building candidates | Source-only, not native | Offline lookdev images and source tests are not features in the open Unreal level. |

The current level contains one `CesiumGeoreference` and a Google
Photorealistic 3D Tiles context actor with non-secret asset ID `2275207`.
The audited host has a configured Cesium server asset. Release bundles omit
`Content/CesiumSettings` because that asset contains credential-like per-device
state, so another device must configure its own server/account reference.
Configuration does not prove provider entitlement, availability, or a
successful live stream. The R29 local terrain is a visual fallback. Neither
source is surveyed collision, navigation, sensor, RF, or height authority. Do
not describe the current scene as survey-grade terrain or as the completed R33
environment.

What to expect visually: the main Istana grounds are the stronger part of the
scene. The lawn combines an inherited 18,432-instance broad turf layer with
6,144 modeled landmark-grass instances, three blade meshes, twelve profile
buckets, and four material responses. The preserved 729 tree positions use five
high-resolution tree forms, automatic screen-size LOD, PBR/wind response, and
seven landmark-tree anchors. They are designed to read as a varied tropical
canopy and defined turf at ordinary viewing distance, but they remain
procedural—not a surveyed botanical inventory.

Surrounding buildings are present. R29/R30 add many aperture, frame, sill,
roofline, and texture-distance cues; the broad base shells remain simplified,
dated LoD2 context. MacDonald House and the Temasek Shophouse are more
articulated but remain generic study proxies rather than measured as-built
models. The current bundle has not passed native-resolution human visual
acceptance and must not be presented as photoreal or hyperreal. See
[Main Istana grounds versus surrounding buildings](ISTANA_OPERATOR_MANUAL.md#main-istana-grounds-versus-surrounding-buildings)
for the detailed evidence boundary.

The package name stays the same across revisions, so the level name alone does
not prove the installed state. The current code-only hotfix receipt is
`D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DCookedRuntimeUE55V1\r30-cooked-runtime-ue55-20260908-01\commit.json`
(15,166 bytes, SHA-256
`94A94303468D6045137CC74241117CC8C8047C9D9388E5672955B138745453EE`).
It binds the immutable R30-19 content receipt, records all twelve R30 content
packages as unchanged, and pins these currently installed identities:

| Native file | Bytes | SHA-256 |
| --- | ---: | --- |
| `Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap` | `37,468,415` | `126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusion.dll` | `5,076,992` | `7BB3D5225D832900A6C675C4026BD2EF75588DB3B5FDFEE6A4EE2AB608BE00BE` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusionEditor.dll` | `8,448,000` | `3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553` |

Those receipt and DLL pins identify the audited workstation only. A fresh
device verifies its downloaded archives against the release manifest, then
builds new DLLs locally; its locally compiled DLL hashes are not expected to
match this table.

The original R30-19 content receipt remains immutable at
`D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json`
(65,411 bytes, SHA-256
`F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F`).
Its runtime-DLL identity is the hotfix preimage, so the table above—not the
older DLL row in the R30 receipt—is the correct check for the current install.

### Final validation status

Run `r30-capture-20260908-14` is the final packaged validation for this
delivery. Its full UE 5.5 build and cook succeeded; the cook completed
`1,151/1,151` packages, reported zero errors, and ended with zero shaders left.
The packaged Game initially logged
valid fountain, TreeRealism, and ground/vegetation states and saved one 75 m
Player0 PNG. It then reported that the V5D runtime tree-override ownership,
source, tag, or snapshot roster was incomplete. The fountain became invalid
through its inherited appearance dependency, and ground/hybrid presentation
failed closed. The exact owned Game was stopped before the remaining four
views.

The wrapper rolled back cleanly with the R30 map/content and
`AirSimTriadRuntime` preserved. There is no retained admissible five-image set,
no `pending-visual-review.json`, and no accepted visual-review receipt. The cook
and first frame must not be presented as packaged-runtime success or human
visual acceptance.

Use these immutable records when the final outcome must be audited:

- rollback:
  `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-14\rollback.json`
  (600 bytes, SHA-256
  `1B042C466B2B54E976634A9CD4DC37D3ADC5B2801BE4CDB91F2E27A7B49D18D5`);
- cook log:
  `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-14\logs\cook.runtime.log`
  (601,132 bytes, SHA-256
  `35AD2D0EC3DB456BDC794C473C3C71A366B9BB6599AD36EC7C63F6922936BA63`);
- packaged Game log:
  `D:\triad\TRIAD_R30Evidence\r30-capture-20260908-14\logs\game.runtime.log`
  (368,236 bytes, SHA-256
  `A1A152902E21A1EBE9FBD77759841FD90E454BFA811A77C211DB8D019475497A`).

Per the final delivery decision, do not repair or rerun the packaged capture.
The remainder of this guide opens the best installed R30-19-plus-hotfix scene
in the Unreal editor and runs the separate simulation/dashboard showcase.

Opening the showcase does not require a native transaction or capture wrapper.
Do not rerun the already committed R30 transaction, reuse any completed capture
token, bypass a stage's guards, or copy staged files into the native project.
The full boundary is in
[V5D current R30 native scene status](ISTANA_OPERATOR_MANUAL.md#v5d-current-r30-native-scene-status).

## Requirements

- Windows, Unreal Engine 5.5, and Visual Studio 2022 with **Game development
  with C++** and a Windows SDK.
- A DirectX 12 / Shader Model 6-capable GPU and current driver. No minimum VRAM
  configuration has been formally qualified for this high-detail scene.
- Git, Git LFS, PowerShell 7, GitHub CLI, and Python 3.11 or newer with
  `tkinter`; installing this repository's Python package supplies `numpy`.
- A clone of this repository plus its matching versioned GitHub Release assets.
  A clone alone does not contain `TRIAD.uproject`, the R30 map/content, or the
  multi-gigabyte `unreal/SourceAssets` tree.
- `AirSimTriadRuntime` and Cesium for Unreal compatible with UE 5.5, enabled in
  the reconstructed project. Cesium ion server/token state is configured
  locally and is never included in the release. Do not substitute the original
  `AirSim` plugin.
- The separately licensed/Fab host content at
  `<PROJECT>\Content\Scene_RoadsideConstruction`. The map directly references
  `Urb_Str_Shrub_Common_Set_01` mesh variants `A`, `D`, `F`, and `H`. Bundled
  material `M_IPV5D_PublicRealm_AsphaltDry_R2` also hard-references
  `Ground_Asphalt_Fresh_01` textures `D`, `N`, and `ORDp`. This licensed content
  is not redistributed by this repository or TRIAD bundles.
- Network access and the configured provider entitlement if streamed Cesium
  context is expected. A provider layer can be unavailable while the local
  R29 fallback retained by R30 remains visible.

Plan for roughly 15 GiB for cached release archives plus their extracted TRIAD
payload, in addition to Unreal Engine, licensed dependencies, builds, and
derived-data caches. Those additional components can require tens of gigabytes.

`AirSimTriadRuntime` is an external compile/runtime gate. The Unreal project can
open without `Scene_RoadsideConstruction`, but it will report missing-asset
warnings and use a fallback appearance; identical audited-host visuals require
the seven licensed mesh/texture assets above. Python tests and the offline
placement dashboard do not require those Unreal dependencies.

The audited native installation is already built. On that installation, do
**not** rebuild, copy plugin files, or regenerate the map as part of this
zero-to-demo path: doing so would invalidate the R30-19-plus-hotfix identities.
If an audited DLL is missing or does not match, stop instead of repairing that
installation in place. A new release-bundle installation must build its own
DLLs as described below.
Do not launch this interactive showcase while a guarded native transaction,
cook, or capture wrapper is active; wait for that owned Unreal process to close
and for its result to be recorded first.

## First-time installation on another device

Follow [Install on another Windows device](../README.md#install-on-another-windows-device).
In summary, the supported portable flow is:

1. Clone the repository and run `git lfs pull`.
2. Run `scripts/Install-GitHubFreeReleaseBundles.ps1` from PowerShell 7 into an
   empty `$projectRoot`. The installer verifies each release archive before
   extracting it and installs the tracked credential-free portable
   `DefaultEngine.ini`. The native host configuration and `Content/CesiumSettings`
   are explicitly excluded.
3. Confirm that the release manifest's `sourceCommit` exactly equals
   `git rev-parse HEAD` as shown in the repository README.
4. Install the authorised external `AirSimTriadRuntime` and Cesium for Unreal.
   Install the `Scene_RoadsideConstruction` content for matching visuals. Keep
   their expected plugin/content mount names unchanged.
5. With Unreal closed, build `TRIADSensorFusion` and
   `TRIADSensorFusionEditor` using the README command.

If the repository's Releases page has no release containing
`triad-release-manifest.json`, project-owned Unreal assets have not been
published yet. Do not treat the code-only clone as a complete Unreal product.
The offline Python dashboard remains usable.

## Set local paths and verify the installation

Open fresh PowerShell terminals. Set these three variables in **each** terminal
before using a relative path. The shown values are examples; only the exact map
package and checked-in artifact names are fixed:

```powershell
$repoRoot = 'C:\Work\TRIAD-Sensor-Fusion' # your clone
$projectRoot = 'D:\TRIAD-Work\TRIAD'      # release-bundle destination
$engineRoot = 'C:\Program Files\Epic Games\UE_5.5'
Set-Location $repoRoot
```

Confirm the essential files before launching anything:

```powershell
$projectFile = Join-Path $projectRoot 'TRIAD.uproject'
$requiredFiles = @(
  $projectFile
  (Join-Path $projectRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap')
  (Join-Path $projectRoot 'Plugins\TRIADSensorFusion\TRIADSensorFusion.uplugin')
  (Join-Path $projectRoot 'Plugins\AirSimTriadRuntime\AirSimTriadRuntime.uplugin')
  (Join-Path $repoRoot 'core\reports\istana_four_class_dashboard_replay.v2.json')
  (Join-Path $repoRoot 'core\reports\istana_1km_unreal_sensor_nodes_patch.json')
)
$missingFiles = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missingFiles.Count -gt 0) {
  $missingFiles
  throw 'TRIAD installation is incomplete; install the listed project or licensed prerequisites.'
}

$roadsideRoot = Join-Path $projectRoot 'Content\Scene_RoadsideConstruction\Assets\MS'
$licensedVisualFiles = @(
  (Join-Path $roadsideRoot '3D_Plants\Urb_Str_Shrub_Common_Set_01\SM_Urb_Str_Shrub_Common_Set_01_A.uasset')
  (Join-Path $roadsideRoot '3D_Plants\Urb_Str_Shrub_Common_Set_01\SM_Urb_Str_Shrub_Common_Set_01_D.uasset')
  (Join-Path $roadsideRoot '3D_Plants\Urb_Str_Shrub_Common_Set_01\SM_Urb_Str_Shrub_Common_Set_01_F.uasset')
  (Join-Path $roadsideRoot '3D_Plants\Urb_Str_Shrub_Common_Set_01\SM_Urb_Str_Shrub_Common_Set_01_H.uasset')
  (Join-Path $roadsideRoot 'Surfaces\Ground_Asphalt_Fresh_01\T_Ground_Asphalt_Fresh_01_D.uasset')
  (Join-Path $roadsideRoot 'Surfaces\Ground_Asphalt_Fresh_01\T_Ground_Asphalt_Fresh_01_N.uasset')
  (Join-Path $roadsideRoot 'Surfaces\Ground_Asphalt_Fresh_01\T_Ground_Asphalt_Fresh_01_ORDp.uasset')
)
$missingLicensedVisuals = @(
  $licensedVisualFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }
)
if ($missingLicensedVisuals.Count -gt 0) {
  Write-Warning 'Licensed roadside assets are absent; expect missing-asset warnings and fallback visuals.'
  $missingLicensedVisuals
}
```

The map and DLL hashes near the top of this document remain the historical
identity record for the original audited workstation. Do not apply its absolute
paths or locally compiled DLL hashes to a reconstructed device. The release
installer's manifest size/SHA-256 checks and the `sourceCommit` equality check
are the portable integrity gates.

Install the dashboard environment once per clone if it was not created during
the README setup:

```powershell
if (-not (Test-Path '.\core\.venv\Scripts\python.exe' -PathType Leaf)) {
  py -3.11 -m venv .\core\.venv
}
& .\core\.venv\Scripts\python.exe -m pip install -e '.\core'
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -PythonExecutable '.\core\.venv\Scripts\python.exe' `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -ValidateOnly
```

The optional local Globe-C2-compatible FastAPI service has additional pinned
runtime dependencies. Install them only if that service is needed:

```powershell
& .\core\.venv\Scripts\python.exe -m pip install `
  -r '.\integrations\globe-c2\service\requirements.txt'
```

Before opening Unreal, also run the read-only disk/commit check in
[Storage and memory check](#storage-and-memory-check). A low disk reading and a
low Windows commit reading require different remedies; never delete evidence or
clear an active wrapper's working files.

## Use two terminals

Keep the two views separate so their evidence is not confused:

- **Terminal A — placement platform:** validates and opens the frozen two-site,
  four-class recommendation replay. Leave its dashboard window open.
- **Terminal B — Unreal simulation:** creates a new four-node scenario and
  launches the exact V5D level. Its drone and detections are a separate run and
  are not synchronized with Terminal A.

Terminal A opens a native Tk desktop window. It is not a web server and has no
browser URL. The optional live overlay later still renders in that desktop
window; only its read-only loopback source has URLs:
`http://127.0.0.1:8765/health` and
`http://127.0.0.1:8765/api/snapshot`.

The optional live overlay later uses three additional terminals. It does not
turn the four-node Unreal scenario into the two-site solver recommendation.

## Cesium setup without exposing a secret

The map can run with its local visual fallback even when provider streaming is
unavailable. A release install deliberately starts without
`Content/CesiumSettings`; never copy that directory from the audited host.
Perform these steps after Part 3 has opened Unreal. If authorised Google
context is required:

1. Open Unreal's **Window > Cesium** panel and use its supported Cesium ion
   account/server sign-in. Do not put a token in PowerShell, a scenario JSON,
   this repository, a screenshot, a chat, or a log.
2. Create or select the local Cesium ion server/account through that panel. In
   the World Outliner, select the existing Google Photorealistic 3D Tiles actor
   and assign the local server reference if it is null. Verify only the
   non-secret facts: source **From Cesium ion**, asset ID `2275207`, and a
   non-null Cesium ion server reference. Do not expand, copy, print, or record
   any token field or fingerprint, and do not add the generated
   `Content/CesiumSettings` asset to a commit or release.
3. In the viewport, streamed city context should refine after network requests
   complete. In the Output Log, `401`/`403` means authorization or entitlement
   is not ready; repeated `429` means throttling. Do not mistake the local
   fallback for a successful stream.

These checks are operator diagnostics only. Visible pixels, a non-null server
object, and global load progress still do not prove formal provider readiness,
terrain accuracy, collision, line-of-sight, sensor, or RF authority. R33 Cesium
World Terrain is not native.

## Part 1 — Terminal A: open the simulated UAS ingress and detection dashboard

First validate the checked-in replay without opening a window:

```powershell
Set-Location $repoRoot
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -PythonExecutable '.\core\.venv\Scripts\python.exe' `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -ValidateOnly
```

The validated artifact reports digest
`6949355f49492490752b453b49231db420446637818a2461e544254976524dab`,
two displayed study sites, four simulated ingress tracks, and four eventually
detected tracks. Open it at double speed:

```powershell
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -PythonExecutable '.\core\.venv\Scripts\python.exe' `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -Speed 2
```

In the dashboard:

1. Click `demo-site-south` or `demo-site-west` to see its coordinates, sensor
   classes, failure domain, and selection rationale.
2. Click **Run simulated ingress**.
3. Watch the four UAS markers, cue lines, and detection feed. The first
   precompute-backed cue appears at display time `T+1.0 s`.
4. Use **Pause**, **Reset**, the timeline, or the speed selector. Space toggles
   playback and `R` resets.

This window is a deterministic offline replay of a frozen synthetic
recommendation. It is not live Unreal telemetry and its two sites are not
approved physical mounts.

## Part 2 — Terminal B: create a safe runnable Unreal scenario

Do not edit or enable the base config. Generate a new enabled copy under
Unreal's `Saved` directory. Keep this PowerShell window open because the next
command uses `$scenarioPath`.

```powershell
Set-Location $repoRoot
$showcaseStamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$scenarioDirectory = Join-Path $projectRoot 'Saved\TRIAD\Configs'
$scenarioPath = Join-Path $scenarioDirectory `
  "IstanaSensorPlacement.showcase_$showcaseStamp.json"
& .\scripts\New-IstanaScenarioConfig.ps1 `
  -PatchPath '.\core\reports\istana_1km_unreal_sensor_nodes_patch.json' `
  -OutputPath $scenarioPath `
  -Enable `
  -EnableOperatorObserver
$scenarioPath
```

The builder refuses to overwrite an existing scenario. That is intentional;
generate a new timestamp instead of deleting or editing an earlier receipt.
The builder requires nonempty, unique `NodeId` values. This checked-in patch
produces four enabled runtime sensor nodes and one enabled simulated quadcopter.
The quadcopter is a synthetic UAS ingress target with
`bHostileScenarioTruth=false`; it is not an assertion of a real attack. This
scenario is separate from the two-site dashboard study.

## Part 3: open the exact Unreal map

Launch the V5D map with the generated scenario override:

```powershell
& .\scripts\Start-IstanaSimulation.ps1 `
  -ProjectPath (Join-Path $projectRoot 'TRIAD.uproject') `
  -ScenarioConfigPath $scenarioPath `
  -MapPackage '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid' `
  -UnrealEngineRoot $engineRoot
```

Do not omit, shorten, or substitute the `-MapPackage` value. The rejected
`Istana_1km_Context_v2` prototype and other maps are not valid substitutes for
this showcase.

The launcher checks the map, enabled scenario, presence of at least one
`SensorNode`, exact 1,000 m circular study area, and any dedicated-RF map binding
before opening Unreal. The scenario builder, rather than this launcher, performs
the unique, nonempty `NodeId` check. When the editor has finished loading, press
the green **Play** button, then click once inside the game viewport so it owns
keyboard and mouse input. The launcher opens the editor; it does not start
Play-In-Editor for you.

Manual fallback: double-click `TRIAD.uproject` inside `$projectRoot`, open the
Content Browser, go to `Content/Maps`, and open
`Istana_PublicView_Explore_v5d_hybrid`. The scripted route is preferred because
manual opening does not supply the generated scenario override. Treat the
manual route as visual inspection only; return to the scripted route above for
the drone-and-detection showcase.

## Part 4: move around

On first open, wait until shader/asset loading settles, press **Play**, and click
the viewport. If the keys do nothing, click the viewport again. Use `Shift+F1`
to release the mouse back to the editor.

- `W` / `S`: forward / backward.
- `A` / `D`: left / right.
- `E` / `Q`: rise / descend.
- Mouse: look around.
- `Shift`: move at 4x speed.
- `Ctrl`: move at 0.25x speed for precise framing.
- `O`: hide or show the operator panel.
- `N`: select the next current contact.
- `Shift+F1`: release the mouse to the editor.
- `Esc`: stop Play-In-Editor.

The free-roam pawn is collision-swept, limited to 950 m horizontally and
1.5--300 m local altitude, and moves at 12 m/s before modifiers.

## Part 5: show the simulated drone detection

After Play starts, the scenario spawns one simulated quadcopter on a 1,800 m
east-to-west path. It travels at 12 m/s, reverses at each end, and is sampled
every 0.25 seconds.

There is no separate attack button in Unreal: pressing **Play** is the trigger
that starts this non-hostile synthetic ingress automatically. In the separate
offline dashboard, **Run simulated ingress** is its corresponding replay
trigger. Neither action represents or initiates a real attack.

1. Press `O` if the operator panel is hidden.
2. Wait for the target to enter a simulated sensor's range.
3. Point out the reporting node, range, and RF SNR and/or radar confidence in
   the panel.
4. Press `N` to cycle active contacts.

A visible drone, route line, arrow, or marker is not by itself a detection.
Only current positive RF or search-radar evidence creates an Unreal contact.
The values are synthetic model evidence, not measured confidence or a
field-calibrated probability. The simulation never generates an engagement
action.

If the panel stays empty, confirm that Play is running and inspect the scenario
and snapshot without editing them:

```powershell
$config = Get-Content -Raw -LiteralPath $scenarioPath | ConvertFrom-Json
[pscustomobject]@{
  Enabled = $config.bEnabled
  SpawnDemoTargets = $config.bSpawnDemoTargets
  EnabledDemoTargets = @($config.DemoTargets | Where-Object bEnabled).Count
  OperatorObserver = $config.OperatorObserver.bEnabled
  SensorNodes = @($config.SensorNodes).Count
}
$snapshotPath = Join-Path $projectRoot `
  'Saved\SingaporeSensorFusion\latest_rf_snapshot.json'
Get-Item -LiteralPath $snapshotPath |
  Select-Object FullName, LastWriteTime, Length
```

The expected configuration values are `True`, `True`, `1`, `True`, and `4`.
The snapshot must be from this Play session, not an older file. Check it twice:

```powershell
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
  ConfiguredNodes = $second.counts.configuredSensorNodes
  SpawnedNodes = $second.counts.spawnedSensorNodes
  NodeIdsMatch = @(Compare-Object $expectedNodeIds $actualNodeIds).Count -eq 0
  AllNodesEnabledAndSpawned = @(
    $second.sensorNodes | Where-Object {
      $_.enabled -ne $true -or $_.spawned -ne $true
    }
  ).Count -eq 0
  ScenarioTargets = $second.counts.scenarioTargets
  NonHostileDemoTruth = @($second.scenarioTargets | Where-Object hostileScenarioTruth).Count -eq 0
  CurrentPositiveEvidence = (
    $second.counts.retainedDetectedRFLinks -gt 0 -or
    $second.counts.retainedSearchRadarDetections -gt 0)
}
```

A healthy sample reports schema `triad.live_rf_snapshot.v3`, `Advanced=True`,
`SampleComplete=True`, `DetectionOnly=True`, `ActionsTaken=none`, four configured
and spawned nodes, `NodeIdsMatch=True`, `AllNodesEnabledAndSpawned=True`, one
scenario target, and `NonHostileDemoTruth=True`.
`CurrentPositiveEvidence` may be false while the target is out of range; it must
become true before you claim a detection. The operator panel must agree. A
visible drone mesh alone never passes the detection check.

## Optional: overlay fresh Unreal telemetry in the dashboard

This is a separate, loopback-only view. Start Unreal Play first. Install the
Python core once if its virtual environment does not already exist:

```powershell
if (-not (Test-Path '.\core\.venv\Scripts\python.exe' -PathType Leaf)) {
  py -3.11 -m venv .\core\.venv
}
& .\core\.venv\Scripts\python.exe -m pip install -e '.\core'
```

In terminal 1, validate and fuse current Unreal snapshots:

```powershell
Set-Location $repoRoot
$env:PYTHONPATH = (Resolve-Path '.\core\src').Path
$snapshotDirectory = Join-Path $projectRoot 'Saved\SingaporeSensorFusion'
& .\core\.venv\Scripts\python.exe -m singapore_sensor_fusion.layered_runtime `
  --input (Join-Path $snapshotDirectory 'latest_rf_snapshot.json') `
  --output (Join-Path $snapshotDirectory 'latest_layered_snapshot.json') `
  --interval 0.25
```

In terminal 2, expose the bounded snapshot through loopback GET only:

```powershell
Set-Location $repoRoot
$env:PYTHONPATH = (Resolve-Path '.\core\src').Path
& .\core\.venv\Scripts\python.exe -m singapore_sensor_fusion.c3_bridge `
  --saved-dir (Join-Path $projectRoot 'Saved\SingaporeSensorFusion') `
  --host 127.0.0.1 `
  --port 8765
```

In terminal 3, check health and open the live overlay:

```powershell
Set-Location $repoRoot
Invoke-RestMethod 'http://127.0.0.1:8765/health'
& .\scripts\Show-IstanaPlacementDashboard.ps1 `
  -PythonExecutable '.\core\.venv\Scripts\python.exe' `
  -ReplayPath '.\core\reports\istana_four_class_dashboard_replay.v2.json' `
  -LiveUrl 'http://127.0.0.1:8765/api/snapshot' `
  -LivePollSeconds 0.25 `
  -LiveTimeoutSeconds 0.75
```

Hollow blue squares remain the frozen two-site recommendation. Solid nodes,
tracks, and cue links come from fresh Unreal telemetry. The current four-node
Unreal scenario normally shows **REFERENCE MISMATCH** against the two-site
study; that is the correct result, not a fault. Stale or malformed telemetry
suppresses live tracks and cues.

## Storage and memory check

Disk capacity and Windows commit memory are different. This read-only check is
safe to run before opening Unreal:

```powershell
$os = Get-CimInstance Win32_OperatingSystem
$projectVolume = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($projectRoot))
[pscustomobject]@{
  ProjectVolumeFreeGiB = [math]::Round($projectVolume.AvailableFreeSpace / 1GB, 2)
  FreeCommitGiB = [math]::Round(([int64]$os.FreeVirtualMemory * 1KB) / 1GB, 2)
}
```

The dashboard itself is light, but Unreal, shader compilation, cooks, and
capture rollback copies can be large. `Saved\SingaporeSensorFusion` can grow to
tens of gigabytes. A guarded rollback is roughly 15 GiB on this workstation,
and accumulated capture retries can exceed 120 GiB. The current R30 capture
harness has no fixed process-RAM ceiling, reserves 2 GiB of emergency system
commit, and uses eight shader workers; these are system-memory settings, not a
GPU-VRAM setting. The completed R30 transaction and R31--R33 wrappers retain
their 10 GiB launch, 6 GiB continuous, and 12 GiB private-memory limits. None
of these values is a disk-space threshold.

Stop Unreal and Python normally before investigating resource pressure. Never
delete or edit `Saved\TRIAD\NativeTransactions`, `TRIAD_R30Evidence`, later
capture-evidence roots, rollback backups, recovery directories, or certification
receipts to make space. Do not clear `Binaries`, `Intermediate`, cooked output,
or derived-data caches while a wrapper is active; ask a maintainer to identify
an exact disposable cache or approved archive destination.

## Stop cleanly

- Press `Esc` to stop Play-In-Editor, then close Unreal normally.
- Press `Ctrl+C` in each Python telemetry terminal.
- Close the dashboard window normally.
- Generated scenario files under `$projectRoot\Saved\TRIAD\Configs` are
  immutable run inputs; leave them in place unless a maintainer has verified a
  specific file is disposable.

## If something fails

- Script blocked by PowerShell: run
  `Set-ExecutionPolicy -Scope Process Bypass` in that terminal only.
- No dashboard Python: pass `-PythonExecutable` pointing to Python 3.11+ with
  both `tkinter` and `numpy`.
- Unreal opens but nothing moves: wait for loading to finish and press **Play**.
- Provider context is absent: verify network/entitlement separately; do not
  claim that the local fallback proves Cesium provider readiness.
- Visual quality differs from the description above: first confirm that the
  exact map opened and that the current R30-19-plus-hotfix identities pass.
  Wait for shader compilation and Cesium refinement to settle. Final run 14 did
  not complete the required five-view Player0 set, and no accepted human review
  exists. Do not rerun it for this delivery or turn an operator impression into
  a photorealism or hyperrealism claim.
- R31, R32, or R33 appears absent: that is the current expected state. Repository
  source readiness does not prove native promotion; do not copy staged assets
  manually or skip the guarded order.
- Map or DLL identity differs from the table: stop making a current-state claim
  and validate the applicable hotfix transaction receipt.
- An older script tries to use `D:\triad\TRIAD`: do not use historical build,
  capture, repair, or native-transaction scripts as new-device launchers. For
  the portable showcase, use the three entry points in this guide and pass the
  explicit local paths shown above.

For deeper diagnosis, use
[Troubleshooting](ISTANA_OPERATOR_MANUAL.md#troubleshooting). Native promotion
is a maintainer workflow with stage-specific memory guards and mandatory human
capture; it is documented separately for future authorized work in
[Istana R30--R33 guarded native execution](ISTANA_R30_R33_NATIVE_EXECUTION_GUIDE.md).
