# TRIAD Sensor Fusion

Simulation-only sensor fusion for the TRIAD Unreal/Cesium environment, packaged for collaborative development. This repository contains the complete Python fusion core, the Unreal Engine producer plugin and scenario configuration, and the adapter that publishes fused tracks into Globe-C2.

It contains no engagement, targeting, or weapon-control logic. Scores are simulation evidence indexes, not calibrated real-world probabilities.

## Repository layout

```text
core/                         Python sensing, association, fusion, audits, and tests
unreal/                       UE 5.5 TRIADSensorFusion plugin and active scenario config
integrations/globe-c2/        Layered-snapshot-to-C2 adapter and contract tests
docs/ARCHITECTURE.md          Data flow, fusion rules, provenance, and safety boundaries
docs/ISTANA_OPERATOR_MANUAL.md Short build, survey, recommend, and simulation workflow
```

Generated Unreal frames, telemetry, caches, build products, virtual environments, credentials, and model binaries are intentionally excluded.

## Install on another Windows device

A Git clone contains the code, configuration, documentation, deterministic
dashboard replay, and small test data. It does **not** by itself contain the
multi-gigabyte Unreal map/content or `unreal/SourceAssets`. Project-owned large
files are distributed as immutable GitHub Release bundles so a clone can be
reconstructed without relying on this workstation's drive layout.

Before starting, install:

- Git, Git LFS, PowerShell 7, GitHub CLI, and Python 3.11+ with Tcl/Tk;
- Unreal Engine 5.5 and Visual Studio 2022 with **Game development with C++**
  and a Windows SDK;
- a DirectX 12 / Shader Model 6-capable GPU and current graphics driver (the
  portable Unreal configuration enables DX12 SM6 for the V5D Nanite content);
- Cesium for Unreal compatible with UE 5.5 and an authorised per-device Cesium
  ion account/server configuration when streamed context is wanted;
- the authorised `AirSimTriadRuntime` plugin; and
- the separately licensed/Fab content mounted at
  `<PROJECT>/Content/Scene_RoadsideConstruction`, including
  `Urb_Str_Shrub_Common_Set_01` variants `A`, `D`, `F`, and `H` referenced by
  the R30 map and `Ground_Asphalt_Fresh_01` textures `D`, `N`, and `ORDp`
  referenced by bundled material `M_IPV5D_PublicRealm_AsphaltDry_R2`.

`AirSimTriadRuntime` and the `Scene_RoadsideConstruction` source content are
external prerequisites and cannot be redistributed from this repository.
`AirSimTriadRuntime` is required to compile and run the Unreal plugin; never
substitute the original `AirSim` plugin. The project can open without the
licensed roadside pack, but Unreal will report missing-asset warnings and use a
fallback appearance. Install those exact assets for visuals matching the
audited host.
`Content/CesiumSettings` is also deliberately omitted because its server asset
contains credential-like per-device state. Configure Cesium locally through its
supported editor panel; never commit or upload the generated server/token asset.
Allow roughly 15 GiB for the current release archives plus extracted TRIAD
payload alone; Unreal Engine, licensed dependencies, builds, and derived-data
caches require additional tens of gigabytes. No minimum GPU-VRAM target has
been formally qualified, so validate the intended device before a showcase.

From PowerShell 7, choose any empty local project directory:

```powershell
git lfs install
git clone https://github.com/Jenjenson/TRIAD-Sensor-Fusion.git
Set-Location .\TRIAD-Sensor-Fusion
git lfs pull
gh auth login

$repoRoot = (Get-Location).Path
$projectRoot = 'D:\TRIAD-Work\TRIAD' # change this to an empty local directory
$engineRoot = 'C:\Program Files\Epic Games\UE_5.5'

pwsh -File .\scripts\Install-GitHubFreeReleaseBundles.ps1 `
  -Tag latest `
  -UnrealProjectDestination $projectRoot
```

The installer downloads the latest TRIAD asset release, checks every archive's
recorded size and SHA-256, rejects unsafe paths, extracts the project-owned map
and content, installs the tracked credential-free portable `DefaultEngine.ini`,
and links the repository's source/plugin trees into the new Unreal project. The
bundle builder refuses to include the native host's `DefaultEngine.ini` or
`Content/CesiumSettings`. The installer refuses to merge into existing included
roots unless the operator explicitly requests `-AllowExisting`; a fresh
destination is recommended.
Confirm that the selected Git commit matches the release manifest before
building:

```powershell
$releaseTag = (gh release view --repo Jenjenson/TRIAD-Sensor-Fusion `
  --json tagName --jq '.tagName').Trim()
$releaseManifestPath = Join-Path $env:LOCALAPPDATA `
  "TRIAD\ReleaseBundles\$releaseTag\triad-release-manifest.json"
$releaseManifest = Get-Content -Raw -LiteralPath $releaseManifestPath |
  ConvertFrom-Json
$gitCommit = (& git rev-parse HEAD).Trim()
if ($gitCommit -cne $releaseManifest.sourceCommit) {
  throw "Source/assets mismatch: Git=$gitCommit release=$($releaseManifest.sourceCommit)"
}
```

Install the external plugins/content into that project, then build the two TRIAD
modules with the editor closed:

```powershell
$projectFile = Join-Path $projectRoot 'TRIAD.uproject'
$build = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
& $build UnrealEditor Win64 Development $projectFile `
  -DisablePlugin=AirSim `
  -Module=TRIADSensorFusion `
  -Module=TRIADSensorFusionEditor `
  -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed: $LASTEXITCODE" }

py -3.11 -m venv .\core\.venv
& .\core\.venv\Scripts\python.exe -m pip install -e '.\core[test]'
```

If no GitHub Release contains `triad-release-manifest.json`, the large-file
delivery has not been published yet and the Unreal showcase cannot be restored
from the repository. The Python core and offline dashboard still work from the
clone. See [GitHub Free collaboration](docs/GITHUB_FREE_COLLABORATION.md) for
manual/offline installation and maintainer release instructions, then follow
the [newcomer quickstart](docs/ISTANA_NEWCOMER_QUICKSTART.md) to operate the
result.

The portable operator entry points are
`scripts/New-IstanaScenarioConfig.ps1`,
`scripts/Start-IstanaSimulation.ps1` with explicit `-ProjectPath` and
`-UnrealEngineRoot`, and `scripts/Show-IstanaPlacementDashboard.ps1` with an
explicit `-PythonExecutable`. Many older build, capture, repair, and native
transaction scripts deliberately pin the audited `D:\triad\TRIAD` evidence
environment. They are retained for provenance/recovery and are not portable
new-device launchers.

## Istana digital-twin status

The legacy `Istana_1km_Context_v2` level and its generated Main Building mesh
are rejected prototypes. The runnable showcase uses `TRIAD.uproject` and the
exact level `/Game/Maps/Istana_PublicView_Explore_v5d_hybrid`. On the audited
workstation the project is at `D:\triad\TRIAD\TRIAD.uproject`; another device
must use its own release-bundle destination. Use the scripted launch in the
[newcomer quickstart](docs/ISTANA_NEWCOMER_QUICKSTART.md); manually opening the
level is suitable for visual inspection but does not load the generated sensor
scenario.

The installed scene is the guarded **R30-19 content commit** plus its committed
UE 5.5 cooked-runtime compatibility hotfix. The hotfix preserves the map and
all twelve R30 content packages while replacing only the runtime module and six
source files that implement UE 5.5 cooked-data compatibility handling. Its
authoritative receipt is
`D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DCookedRuntimeUE55V1\r30-cooked-runtime-ue55-20260908-01\commit.json`
(15,166 bytes, SHA-256
`94A94303468D6045137CC74241117CC8C8047C9D9388E5672955B138745453EE`).
The current installed identities are:

| Native file | Bytes | SHA-256 |
| --- | ---: | --- |
| `Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap` | `37,468,415` | `126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusion.dll` | `5,076,992` | `7BB3D5225D832900A6C675C4026BD2EF75588DB3B5FDFEE6A4EE2AB608BE00BE` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusionEditor.dll` | `8,448,000` | `3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553` |

The original R30-19 content receipt remains immutable at
`D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json`
(SHA-256
`F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F`).
Its older runtime-DLL pin is the hotfix preimage, so use the hotfix receipt and
table above when checking the currently installed files.

What is visible now: the main grounds combine authored terrain and lawn with a
dense procedural turf layer; tropical vegetation uses the preserved 729 tree
positions, five high-resolution tree forms, automatic LOD, PBR/wind response,
and seven landmark-tree anchors. Surrounding buildings are present. R29/R30
add aperture, frame, sill, roofline, and texture-distance cues, while the broad
base shells remain simplified, dated LoD2 context. MacDonald House and the
Temasek Shophouse are more articulated study proxies, not measured as-built
models. Google Photorealistic 3D Tiles is configured as the preferred Cesium
visual context, with a local 2 km Copernicus-derived mesh and local context as
the visual fallback when streaming is unavailable.

Those are implementation facts, not a claim of survey accuracy or completed
human visual review. R31, R32, and R33 are not native. The scene therefore must
not be described as an exact digital twin, survey-grade terrain, provider-ready,
photoreal, or hyperreal.

### Final validation status

Run `r30-capture-20260908-14` is the final packaged validation for this
delivery. Its full UE 5.5 build and cook succeeded; the cook completed all
`1,151/1,151` packages with zero shaders left. The packaged Game initially
reported
`ISTANA_EXPLORE_V5D_FOUNTAIN_VALID`,
`ISTANA_EXPLORE_V5D_TREE_REALISM_VALID`, and
`ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID`, and logged one 75 m Player0 PNG.
It then detected runtime tree-override ownership drift, changed to
`FOUNTAIN_INVALID` plus ground/hybrid fail-closed state. The exact owned Game
was stopped before the five-view set completed, and the wrapper restored the
project without rollback errors. It published no `pending-visual-review.json`,
and no human visual acceptance exists. Build/cook success and the single logged
frame are therefore not a packaged-runtime or visual pass.

The immutable evidence is the
`D:\triad\TRIAD_R30Evidence\r30-capture-20260908-14\rollback.json` rollback
(600 bytes, SHA-256
`1B042C466B2B54E976634A9CD4DC37D3ADC5B2801BE4CDB91F2E27A7B49D18D5`),
the
`D:\triad\TRIAD_R30Evidence\r30-capture-20260908-14\logs\cook.runtime.log`
cook log (601,132 bytes, SHA-256
`35AD2D0EC3DB456BDC794C473C3C71A366B9BB6599AD36EC7C63F6922936BA63`),
and the
`D:\triad\TRIAD_R30Evidence\r30-capture-20260908-14\logs\game.runtime.log`
Game log (368,236 bytes, SHA-256
`A1A152902E21A1EBE9FBD77759841FD90E454BFA811A77C211DB8D019475497A`).
Per the final delivery decision, do not repair or rerun this capture. Use the
best installed R30-19-plus-hotfix editor scene and the simulation/dashboard
workflow below.

A defensible one-to-one replacement remains governed by
[the digital-twin acquisition specification](docs/ISTANA_DIGITAL_TWIN_ACQUISITION_SPEC.md).
It requires authorised BIM/terrestrial capture, licensed terrain/context data,
surveyed 3D vegetation, calibrated material photography and held-out numerical
QA. The release validator fails closed rather than substituting a flat plane,
billboard, generated facade or low-detail provider mesh. The v2 map remains
untouched for regression only. The current public/licensed/owner-controlled
source options and exact access paths are in
[the authoritative data matrix](docs/ISTANA_AUTHORITATIVE_DATA_SOURCE_MATRIX.md),
and the reasons the current scene fails are recorded in
[the fidelity audit](docs/ISTANA_CURRENT_FIDELITY_AUDIT.md).

## Istana placement recommendation

The repository now includes a simulation-only recommendation workflow for a
separate 1 km Istana study area. Unreal owns terrain/building collision and LOS;
Python owns deterministic constraint evaluation and site/orientation selection.
The workflow produces a new review-only `SensorNodes` patch and never edits the
live Singapore scenario automatically.

This is a **synthetic, stage-gated demonstration**, not an approved production
placement. The Stage-0 example still has no eligible approved mounts, 29
unverified evidence artifacts, 10 blocked readiness gates, and five pending
approvals. Its four sensor models are uncalibrated fixtures. The versioned demo
does exercise an 80-row precompute, deterministic greedy/local search, bounded
robust branch-and-bound, and equal-budget derivative-free/evolutionary pose
comparison, but it records `exactSimulatorReplay=false` and
`rfGeometryQueried=false`. The older
`core/reports/istana_1km_recommendation.*` 95.2% result is a legacy regression
fixture and must not be used to select physical sites. The two-site placement
replay and four-node Unreal scenario are separate demonstrations; their live
overlay is expected to report `REFERENCE MISMATCH` until an approved Stage-0
study binds one layout to the exact simulator.

New operators should start with the
[Istana newcomer quickstart](docs/ISTANA_NEWCOMER_QUICKSTART.md), then use
[the full Istana operator manual](docs/ISTANA_OPERATOR_MANUAL.md) for evidence
details and troubleshooting. The model/data limitations and source ledger are in
[Istana data provenance](docs/ISTANA_DATA_PROVENANCE.md), and measurable
acceptance criteria are in
[the Istana acceptance checklist](docs/ISTANA_ACCEPTANCE_CHECKLIST.md).

For the shortest showcase, use two PowerShell windows from the repository root:

Set the guide's `$repoRoot`, `$projectRoot`, and `$engineRoot` variables to the
local installation. Keep the exact V5D Unreal package and checked-in artifact
names.

- **Terminal A:** validate and open the separate deterministic placement replay
  with `scripts/Show-IstanaPlacementDashboard.ps1`.
- **Terminal B:** create a new enabled scenario, then launch the exact map with
  `scripts/Start-IstanaSimulation.ps1` and map argument
  `/Game/Maps/Istana_PublicView_Explore_v5d_hybrid`. Never omit or substitute
  that map argument; after the editor loads, press **Play** and click the
  viewport before using the movement keys.

The placement replay is a local Tk desktop window, not a web application, so
there is no browser URL for the default dashboard. The optional live overlay
reads the loopback-only API at `http://127.0.0.1:8765/api/snapshot`; the
quickstart includes its separate server and health-check commands.

The quickstart provides the complete copy/paste commands, first-open controls,
snapshot pass criteria, and the optional live-overlay terminals.

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
$repoRoot = (Get-Location).Path # run this from the repository root
$coreRoot = Join-Path $repoRoot 'core'
$python = Join-Path $coreRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
  py -3.11 -m venv (Join-Path $coreRoot '.venv')
}
Push-Location $coreRoot
try {
  & $python -m pip install -e '.[test]'
  & $python -m unittest discover -s tests -p 'test_*.py' -v
}
finally {
  Pop-Location
}
```

Optional visual tooling:

```powershell
Push-Location $coreRoot
try {
  & $python -m pip install -e '.[test,event,rgbd]'
}
finally {
  Pop-Location
}
```

## Connect it to TRIAD Unreal

The Unreal plugin requires:

- Unreal Engine 5.5;
- Cesium for Unreal;
- the existing sibling `AirSimTriadRuntime` plugin;
- a map containing an existing `CesiumGeoreference` actor.

For a new or separate host project, copy `unreal/Plugins/TRIADSensorFusion` into
that host's `Plugins` directory and
`unreal/Config/SingaporeSensorFusion.json` into its `Config` directory. Enable
`TRIADSensorFusion`, `CesiumForUnreal`, and `AirSimTriadRuntime`, then build the
editor target before opening Play in Editor. **Do not perform that copy/build
step on the audited `D:\triad\TRIAD` installation before following the Istana
quickstart:** changing its map, sources, or DLLs invalidates the current
R30-19-plus-hotfix identities above.

Use the Cesium panel's supported account/server workflow for any authorised ion
access. Never paste, print, commit, screenshot, or copy a provider token into a
scenario, shell command, log, issue, or documentation. Verify the existing
Google tileset by its non-secret asset ID (`2275207`) and a locally configured,
non-null server reference. A release-bundle install intentionally has no
`Content/CesiumSettings` credential asset until the operator creates it locally.
Visible streamed context and the absence of authorization errors are runtime
observations, not formal provider-readiness or terrain-accuracy proof.

Set the host project once per shell:

```powershell
$projectRoot = 'D:\TRIAD-Work\TRIAD' # use your release-bundle destination
$env:TRIAD_UNREAL_PROJECT = $projectRoot
```

With Unreal running in Play mode, start the layered runtime:

```powershell
Push-Location $coreRoot
try {
  & $python -m singapore_sensor_fusion.layered_runtime --interval 0.25
}
finally {
  Pop-Location
}
```

You can also pass `--input` and `--output` explicitly. `TRIAD_SCENARIO_CONFIG`, `TRIAD_FUSION_OUTPUT_DIR`, and `TRIAD_MODEL_ROOT` override the portable configuration, audit-output, and model defaults.

## Publish tracks to Globe-C2

The included local Globe-C2-compatible service has a separately pinned runtime
dependency file. Install it into the active Python environment if you use that
service:

```powershell
Set-Location $repoRoot
& $python -m pip install -r `
  '.\integrations\globe-c2\service\requirements.txt'
Push-Location '.\integrations\globe-c2'
try {
  & $python -m uvicorn service.app:app --host 127.0.0.1 --port 8100
}
finally {
  Pop-Location
}
```

For an external Globe-C2 instance, start it on loopback only, then run:

```powershell
Set-Location $repoRoot
$layeredSnapshot = Join-Path $projectRoot `
  'Saved\SingaporeSensorFusion\latest_layered_snapshot.json'
& $python '.\integrations\globe-c2\bridge\triad_bridge.py' `
  --snapshot $layeredSnapshot `
  --c2 http://127.0.0.1:3000 `
  --api-profile globe
```

The bridge rejects stale or unsafe snapshots and removes stale C2 cues. The current Globe-C2 mutation endpoints have no authentication, so do not expose them beyond `127.0.0.1`.

## Verification

Run the portable clean-checkout suites from the repository root. The explicit
Unreal module list intentionally excludes historical native-evidence and
release-asset checks whose inputs are not present in a Git clone; it is kept in
sync with `.github/workflows/ci.yml`.

```powershell
Set-Location $repoRoot
& $python -m pip install -e '.\core[test]'
& $python -m unittest discover -s '.\core\tests' -p 'test_*.py' -v
Push-Location '.\unreal\Plugins\TRIADSensorFusion\Tests'
try {
  & $python -m unittest -v `
    test_airsim_optical_flow_ue55_repair_contract `
    test_istana_explore_free_roam_contract `
    test_istana_explore_v5d_context_policy_successor_contract `
    test_istana_explore_v5d_current_view_provider_workload_contract `
    test_istana_explore_v5d_fountain_realism_contract `
    test_istana_explore_v5d_hybrid_contract `
    test_istana_explore_v5d_runtime_geospatial_anchor_contract `
    test_operator_dashboard_launcher_contract `
    test_rf_one_kilometre_actual_file_native_transaction_contract `
    test_rf_one_kilometre_v2_runtime_contract `
    test_rl_source_closure_contract `
    test_start_istana_simulation_launcher_contract
}
finally {
  Pop-Location
}
& $python -m pip install -r `
  '.\integrations\globe-c2\service\requirements.txt'
Push-Location '.\integrations\globe-c2'
try {
  & $python -m unittest bridge.test_triad_bridge -v
  & $python -m unittest service.test_fusion_api -v
}
finally {
  Pop-Location
}
```

The C++ search-radar automation test additionally runs inside a built Unreal editor target. See [unreal/README.md](unreal/README.md).

## Important exclusions

- `AirSimTriadRuntime` is an external prerequisite and is not vendored here.
- The licensed `Content/Scene_RoadsideConstruction` host pack is not vendored;
  the R30 scene expects the four shrub meshes and three asphalt textures listed
  in the install section for identical visuals.
- The OAK YOLO checkpoint is not committed. Its manifest and trusted SHA-256 are included under `core/models`; redistribution requires a license decision.
- `Saved/SingaporeSensorFusion` is runtime data and can grow to tens of gigabytes.
- Guarded capture evidence is intentionally immutable and can also be very
  large. A rollback journal is roughly 15 GiB on this workstation, and
  accumulated R30 retries can exceed 120 GiB. Those are capacity observations,
  not cleanup authority or fixed future requirements.
- Free disk space and Windows free commit memory are different. The current R30
  capture harness has no fixed process-RAM ceiling, reserves 2 GiB of emergency
  system commit, and uses eight shader workers. This is CPU/system memory, not
  GPU VRAM. The completed R30 transaction and the R31--R33 transaction/capture
  wrappers retain their 10 GiB launch, 6 GiB continuous, and 12 GiB
  private-memory limits. Do not delete transaction, capture, rollback,
  recovery, or certification evidence to make room; stop Unreal/Python cleanly
  and ask a maintainer to identify disposable caches or an approved archive
  target.
- No repository-wide license has been selected yet. See [LICENSE-NOTICE.md](LICENSE-NOTICE.md).

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) before changing fusion decisions, provenance rules, or C2 mapping behavior.
