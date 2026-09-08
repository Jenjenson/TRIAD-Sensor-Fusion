# TRIAD Singapore Sensor Fusion (UE 5.5 runtime + editor tooling)

This plugin provides a non-weaponized simulation and data-generation layer for a Cesium Singapore world. It creates visible, labeled geodetic sensor nodes, discovers drone-like actors, models the configured wideband RF channels (including 2.4/5.8 GHz), evaluates Unreal collision line-of-sight, simulates a geometry-only search-radar and radar-cued EO/thermal PTZ layer, and records bounded telemetry and camera frames.

## Activation

The `UTRIADSensorFusionWorldSubsystem` is instantiated only for `EWorldType::PIE` and `EWorldType::Game`. It starts a scenario manager only when this file exists and deserializes with `bEnabled: true`:

`<Project>/Config/SingaporeSensorFusion.json`

The active project configuration is `Config/SingaporeSensorFusion.json`; `Resources/SingaporeSensorFusion.example.json` is a smaller reference. Property names in the JSON intentionally match the reflected C++ property names. The map must already contain a `CesiumGeoreference`; the plugin logs an error and stops instead of creating or changing one.

Because this is a new C++ module, close the editor, build the editor target, and reopen the project before testing PIE. This project has an explicit enabled `TRIADSensorFusion` entry in `TRIAD.uproject`; the plugin descriptor declares its `CesiumForUnreal` and enabled `AirSimTriadRuntime` dependencies.

## Isolated Istana public-view tooling

`UTRIADIstanaPublicViewEditorLibrary` imports ten exact static meshes, 24 PBR
textures, and 22 project-owned materials only beneath
`/Game/TRIAD/IstanaPublicView`, then builds only
`/Game/Maps/Istana_PublicView_Exterior_v1`. The tenth mesh is the frozen
`SM_IstanaPublicView_OSMContextBuildings`: ODbL-derived, building-only,
mapping-grade massing from the 300 m hero exclusion to the one-kilometre edge.
Its source/contract/manifest hashes, identity BuildScale3D, UE5.5 legacy OBJ
coordinate precondition, imported normals, full-precision metre UV0, and Mikk
tangents are fail-closed validation gates. It binds only
`M_IPV_ContextRender` and `M_IPV_ContextRoof`.

The scene displays that OSM component with `NoCollision`, all channels ignored,
and overlaps disabled; it is never navigation, sensor, target, or occlusion
truth. The original 180-box synthetic context asset is still imported and
validated but its component is hidden and inactive to prevent duplicates.
Coordinate alignment is validated, while visual/photo acceptance remains false.
Required attribution is `© OpenStreetMap contributors` under ODbL 1.0. The
public-view workflow does not mutate SDTH, v1, v2, Istana, or DigitalTwin
content and retains the non-survey claim.

### Hero V3 guarded editor and PIE workflow

Hero V3 is an additive exterior-visual path from
`/Game/Maps/Istana_PublicView_Exterior_v2` to the non-overwriting
`/Game/Maps/Istana_PublicView_Exterior_v3`. With every editor closed, first run
`scripts/Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1 -ProjectPath <TRIAD-project>`;
the installer additively copies and hash-checks the dedicated V3 profile without
changing the established V1/V2 profile. Then run
`scripts/Start-IstanaPublicViewHeroV3Editor.ps1 -ProjectPath <TRIAD-project> -ValidateOnly`,
then rerun without `-ValidateOnly`. The launcher accepts only the project-owned
1022-byte `Config/IstanaPublicViewHeroV3VisualAcceptance.settings.json` profile
(ComputerVision, no lidar), exact loopback Remote Control on
`127.0.0.1:30010`, and the
`-TRIADIstanaVisualAcceptance` process flag. If V3 is absent it opens the exact
V2 map for import and migration; once V3 exists it opens and remotely validates
only V3. It hashes the project descriptor, settings, V2, and selected map before
and after readiness and never copies, edits, or overwrites them.

The V3-only file requires both `RpcEnabled=false` (the upstream documented
contract) and `EnableRpc=false` (the deployed AirSim 1.8.1 parser) exactly once,
plus `LocalHostIp=127.0.0.1`; this dual-key compatibility is deliberate. The
launcher, PIE wrapper, and C++ readiness gate prove the exact file, exact
command-line selection, and the live ComputerVision SimMode. AirSim exposes its
loaded `enable_rpc` state only through a protected/header-static interface whose
identity is unsafe to infer across plugin DLLs, so this integration does not
claim an independent in-process readback of that private state. It instead
fails closed on the frozen dual-key file and command line; Remote Control itself
is separately restricted to `127.0.0.1:30010`.

Use `scripts/Set-IstanaPublicViewHeroV3PlayInEditor.ps1 -ProjectPath <TRIAD-project>`
for PIE and the same command with `-Stop` for guarded teardown. Both start and
stop require exactly one editor whose actual command line names the canonical
settings file and visual-acceptance flag; C++ PlayWorld readiness repeats the
exact hash/profile proof. Arbitrary or lidar-enabled settings are rejected,
because AirSim pause is not a join for an already-running lidar worker.

## Istana 1 km editor tooling

The editor-only `UTRIADIstanaEditorLibrary` exposes narrow operations that can be called locally (including through Unreal Remote Control) without enabling arbitrary Python:

- `BuildIstanaStudyMap(OutMessage)` refuses PIE, dirty packages, and an existing destination. It opens `/Game/SDTH` as an untitled template, recenters the duplicate on `103.84288055 E, 1.30709615 N, 47 m` WGS84 ellipsoid height, clips every Cesium tileset outside an exact 64-point/1000 m geodesic circle, adds collision-survey settings and deterministic exterior/AOI actors, validates, and saves only `/Game/Maps/Istana_1km`.
- `ValidateIstanaStudyMap(OutReport)` is read-only and validates the currently loaded destination map, anchors, 64 geodesic vertices, tileset clipping/collision settings, and source-map cleanliness.
- `ImportIstanaPbrMaterials(OutMessage)` imports the manifest-declared 4096 px PNG set from `<Project>/SourceAssets/Istana/Generated/Textures` into `/Game/TRIAD/Istana/Textures` and creates eight exact-slot project-owned `UMaterial` assets under `/Game/TRIAD/Istana/Materials`. Opaque materials bind sRGB Base Color, linear DirectX Normal, and linear packed ORM (`R=AO`, `G=roughness`, `B=metallic`); slate additionally uses its linear height map through a conservative `BumpOffset`, while glazing is a two-sided lit translucent material with fixed opacity/roughness/IOR. It is all-or-nothing: a complete validated 22-texture/8-material set is an idempotent success, and any partial or invalid target set is refused without overwrite. `ValidateIstanaPbrMaterials(OutReport)` is the read-only companion.
- `ImportIstanaExteriorLod0(OutMessage)` is retired and fails closed; it cannot overwrite the legacy `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior` asset. `ImportIstanaExteriorRefinedLod0(OutMessage)` is the only refined LOD0 importer. It imports `<Project>/SourceAssets/Istana/Generated/SM_IstanaExterior_LOD0.obj` at scale `1.0` into the new, non-overwriting asset `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined`. It requires the complete pre-existing set of eight validated project-owned PBR materials and neither creates nor mutates that set. Before Remote Control is called, `scripts/Import-IstanaExteriorRefinedLod0.ps1` hashes the actual OBJ bytes and requires SHA-256 `4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91`. The importer also validates the reviewed manifest, all eight exact material slots, `175,974` source vertices, `58,658` triangles, and `12500 x 11769.4444 x 3685 cm` bounds (including the mast), then assigns the existing materials and saves only the new mesh asset. Direct OBJ import generates simple collision; it does not import the separately packaged eight-hull collision OBJ and therefore makes no eight-UCX-hull claim.
- `BuildIstanaRuntimeMapV2(OutMessage)` is the corrected PIE/runtime builder. It refuses overwrite and creates only `/Game/Maps/Istana_1km_Context_v2` from an untitled `/Game/SDTH` template. It requires `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined` and returns an actionable error when it is missing or invalid; it never silently substitutes the legacy asset or procedural HISM actor. It deliberately preserves the template georeference so inherited sky/start actors remain aligned, keeps the exact inverted outside-only 1 km Cesium clip, disables frustum/fog culling so complete in-circle context can stream, selects `ATRIADIstanaAirSimGameMode` (a behavior-identical TRIAD-owned subclass of the native AirSim GameMode), and adds globe-anchored PlayerStart/Player-0 camera actors plus a map-local clear-weather/zero-load recovery policy. The imported OBJ's source ceremonial `+Y` maps to Unreal local `-Y`, so the exterior uses an independently validated `182.3`-degree ESU heading (the mapping-grade footprint long-axis plus the required 180-degree handedness correction). The policy zeroes inherited primary/secondary exponential-fog density and opacity and disables volumetric fog at runtime without deleting the authored sky or cloud actors. The GameMode Override is applied as a notified editor property edit with explicit package dirtiness, and the builder reopens the saved destination from disk before reporting success.
- `RepairIstanaRuntimeMapV2GameMode(OutBackupFile, OutMessage)` is the narrow recovery path for a previously saved v2 map whose only invalid contract is the missing/incorrect GameMode Override. With PIE stopped and all packages clean, it requires that exact v2 map to be loaded, verifies every other v2 contract, creates a non-overwriting pre-repair `.umap` backup below `Saved/TRIAD/IstanaMapBackups`, edits/saves only `/Game/Maps/Istana_1km_Context_v2`, reopens it from disk, and runs full validation. It neither deletes/rebuilds the destination nor loads, edits, or saves SDTH/v1. The companion script performs read-only hashes of SDTH, v1, the original v2 map, and the backup.
- `RepairIstanaRuntimeMapV2Camera(OutBackupFile, OutMessage)` is the equivalently guarded migration for a previously saved v2 map that still has the old approach camera or reversed imported facade. It first runs every current structural check except the camera pose/FOV and imported-front heading, then independently requires exactly one persistent tagged camera and one imported exterior whose actors, globe anchors, and relevant components all serialize in the destination `.umap`. Using the runtime policy's persisted calibrated-or-fallback ground height, it applies the 210 m south / 24 m AGL / 13 m aim / 52-degree FOV contract and the imported exterior's `182.3`-degree ceremonial-front ESU heading, creates a non-overwriting backup, saves only the exact v2 map, reopens it, and requires full validation. `scripts/Repair-IstanaRuntimeMapV2Camera.ps1` verifies the connected project and hashes SDTH, v1, v2, and the returned backup; it never deletes or overwrites a backup.
- `RepairIstanaRuntimeMapV2ExteriorAsset(OutBackupFile, OutMessage)` is the dedicated exact-v2 migration from the legacy mesh to `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined`. With PIE stopped, clean packages, and the exact v2 map loaded, it validates the map except for the deferred refined-mesh path, requires map-local actor/component/anchor state, validates the separately saved refined asset, and snapshots the hard/soft mesh reference, `182.3`-degree placement, transform, visibility, and full collision configuration. It creates a non-overwriting backup before modifying the map, changes only the exterior mesh reference, proves placement/visibility/collision are preserved, saves only `/Game/Maps/Istana_1km_Context_v2`, reloads it from disk, and runs full refined validation. `scripts/Repair-IstanaRuntimeMapV2ExteriorAsset.ps1` hashes SDTH, v1, v2, the legacy mesh, the refined mesh, and the returned backup. It never edits SDTH/v1 or either mesh asset and is an idempotent success when v2 already uses the refined mesh.
- `ValidateIstanaRuntimeMapV2(OutReport)` performs read-only structural validation, including required mesh bounds/material/collision provenance, absence of the procedural fallback, preserved georeference metadata, the exact TRIAD-owned AirSim GameMode wrapper and its runtime-policy contract, start/camera LLH and aim, visual-weather suppression, bounded Cesium refresh, and correct interior-preserving polygon overlay. It never equates a zero-progress tileset with context readiness.
- `ValidateIstanaRuntimeMapV2Readiness(OutReport)` additionally fails closed unless every Cesium tileset reports at least 99% load with physics meshes. Use this readiness result—not structural validation alone—before context-completeness claims, calibration, screenshots, or PIE acceptance.
- `ValidateIstanaPlayWorldReadiness(OutReport)` first validates the editor map separately, then inspects the actual rebased PIE world without reusing editor-only LLH/geodesic-spline checks. It requires the exact instantiated TRIAD AirSim wrapper and lineage, current read-back AirSim weather suppression, exact zero-density/zero-opacity/non-volumetric readback for every inherited exponential-height-fog component (with the component count reported), completion of the bounded post-AirSim Player-0 camera enforcement, the tagged camera as the live view target, a finite camera-manager POV matching its south/front 210 m horizontal, 24 m height, 13 m aim-height and 52-degree-FOV facade view, the exact transient unconstrained-aspect/conservative exposure-detail profile, and every PIE Cesium tileset at least 99% loaded with physics. Its default state is `AUTHORED_REFINED_PRIMARY`: the refined renderer remains visible and collision-enabled while Cesium supplies surrounding context. The reversible streamed substitution remains available only when `bPreferStreamedIstanaVisualWhenReady` is explicitly enabled; that property defaults to `false`.
- `ValidateIstanaVisualAcceptancePlayWorldReadiness(OutReport)` adds a separate, non-production proof: the process must have the explicit visual-acceptance flag and exact canonical `-settings=<Project>/Config/IstanaVisualAcceptance.settings.json` argument; that bounded file must carry the exact profile marker, ComputerVision/`EnableRpc=false`, one auto-created ComputerVision vehicle, an empty `Sensors` object, and no default/lidar declaration. The PlayWorld must independently contain exactly one begun exported `ASimModeComputerVision` and no other AirSim SimMode, avoiding the header-static `AirSimSettings::singleton()` cross-DLL identity trap. The duplicated PlayWorld must also have applied transient view-driven Cesium frustum culling, every current-view tileset must report finite exact 100% with physics, the exact refined authored exterior renderer must remain visible and collision-enabled, the hard and soft mesh references must both resolve to `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined`, streamed-primary opt-in must be off, and only `TRIADHumanOnlyOverlay` components on the study-area actor may be hidden. Cesium is accepted as exact-100% surrounding context only: the provider's Istana pixels are not building authority because that building has been observed flattened or absent. Fog/overlay suppression, the 1 km AOI/clip, and collision state remain enforced. This does not claim that all offscreen sensor-physics geometry is resident and never claims a provider-rendered Istana building.
- `QuiesceIstanaPlayWorldForStop(OutMessage)` is a non-destructive explicit-stop preflight. It pauses each active AirSim simulation mode through its public API but neither validates nor ends PIE. Because AirSim exposes no join barrier for an already-running lidar scan, the PowerShell stop caller now fails closed and leaves PIE alive if quiescence cannot be verified; only after success does it wait a bounded worker-drain interval and request `EditorRequestEndPlay`.
- `PrepareIstanaRuntimeMapV2Streaming(OutMessage)` is a transient zero-progress recovery helper for the loaded, structurally valid v2 map. With PIE stopped and a focused Level Editor viewport, it switches that viewport to real-time perspective, adopts the tagged runtime-camera transform/FOV, draws a frame, refreshes each still-zero tileset exactly once during that call, and draws again after a refresh. It does not require readiness, capture an image, edit/save a package, or claim that context has loaded.
- `CalibrateIstanaRuntimeMapV2Ground(OutMessage)` is an optional guarded second pass on the loaded, clean v2 destination. It requires every Cesium tileset to report at least 99% load with physics meshes, then makes 16 collision traces at 95 m and 125 m radii—outside the approximately 124 x 118 m footprint and never at the roof/centre. It rejects non-Cesium hits and vertical outliers, requires at least six samples in a four-metre cluster, takes the median WGS84 ellipsoid height, and moves the refined exterior foundation, PlayerStart, and Player-0 camera consistently before saving only the v2 destination. The PowerShell wrapper polls that readiness gate for a bounded 120 seconds by default. Failure retains the explicit `47.0 m` public SRTM30 height fallback; neither result is survey-grade.
- `GenerateIstanaPlacementSurvey(RequestJsonPath, OutputFileName, OutMessage)` reads only an allowlisted `triad.placement_request.v1` JSON below project `Config`, `Saved/TRIAD/PlacementRequests`, or this plugin's `Resources`. It refuses overwrite and writes collision/complex-LOS evidence below `Saved/TRIAD/PlacementSurveys` using `triad.placement_survey.v1`.
- `CaptureIstanaPreview(PresetName, OutputFileName, OutMessage)` accepts `FRONT`, `OBLIQUE`, or `SIDE`, requires the ready v2 imported-mesh map, requires a `v2_editor_*.png` filename, refuses overwrite, and writes the active Level Editor viewport below `Saved/TRIAD/IstanaPreviews/V2`.
- `CaptureIstanaPlaySpawnPreview(OutputFileName, OutMessage)` runs only after `ValidateIstanaPlayWorldReadiness` passes, requires a `v2_play_*.png` filename, and captures the actual Player 0 game viewport to the same v2-only folder. It does not synthesize a camera pose or label a legacy/procedural capture as v2.

`Resources/IstanaSensorPlacementRequest.example.json` is sanitized and matches the optimizer contract. Its candidate survey includes a 950 m ring so fixed inward-facing RGB cameras can evaluate the 900 m target ring without an artificial perimeter blind annulus. The custom building is explicitly a public-exterior-only visual/physics approximation. Its mapping-grade OSM envelope, reference-image architectural details, and flat 47 m collision fallback derived from public 30 m SRTM are not survey/BIM data. Survey output reports `geometryAuthorityScope: LOADED_SIMULATION_COLLISION_ONLY` and `surveyGrade: false`. No interiors, security routes, credentials, or Cesium tokens are included.

The v2 runtime policy waits ten seconds and calls Cesium `RefreshTileset()` once only when a tileset is still at exactly zero load progress. This recovers a stale provider session without persisting or logging endpoint URLs, query strings, access tokens, or request headers. A provider/network failure can still leave context unavailable; structural validation reports zero progress as a note, while the separate readiness validator fails closed. Disabling frustum culling improves off-camera context/collision completeness inside the clipped kilometre but increases tile load and memory pressure.

The refined project mesh is the primary authored Istana renderer and collision authority. It remains visible at finite exact-100% Cesium readiness because the streamed provider building has been observed flattened or absent; Cesium supplies terrain and surrounding structures, not the accepted building. `bPreferStreamedIstanaVisualWhenReady` defaults to `false`. Explicitly opting in retains the old reversible PlayWorld-only behavior: after every Cesium tileset reports finite exact 100% with physics, only the authored mesh component's pixels may be hidden while its collision stays enabled, and rendering is restored if readiness becomes non-finite, loses physics, or falls below the bounded 99% hysteresis threshold. That opt-in state cannot pass visual acceptance. The acceptance run also applies a runtime-only unconstrained-aspect camera profile (exposure bias `-0.25`, bilateral local exposure, conservative contrast/detail/sharpen, and no bloom/vignette/motion blur/fringe) and hides only the study actor's tagged human boundary overlays. None of these transient operations edits a package, changes the exact 1 km clip, or deletes streamed context.

After compiling the plugin into the host project and reopening one editor session, first create and validate the eight PBR materials with `scripts/Import-IstanaPbrMaterials.ps1` and its separate `-ValidateOnly` call if that complete set does not already exist. For an already-saved v2 map, the exact live migration order is: run `scripts/Import-IstanaExteriorRefinedLod0.ps1`, then `scripts/Repair-IstanaRuntimeMapV2ExteriorAsset.ps1`, then run structural validation and PIE with `scripts/Build-IstanaRuntimeMapV2.ps1 -ValidateOnly -StructuralOnly` and `scripts/Set-IstanaPlayInEditor.ps1`. Do not call the retired legacy exterior importer, do not overwrite either mesh asset, and do not enter PIE between refined import and exact-v2 repair. A fresh v2 build instead consumes the already imported refined asset directly and needs no exterior migration. The PIE caller uses structural editor-map validation as its start gate and polls runtime-origin-safe PlayWorld readiness for 600 seconds by default (configurable to 1800). If context is still incomplete, it deliberately leaves the same PIE session running so Cesium can continue streaming; rerunning the command polls that existing session instead of creating a second one. It never lowers the 99%/physics proof. Explicit `-Stop` requires only project identity/current PIE state, requests AirSim quiescence, and fails closed without calling EndPlay if that proof fails. Other narrowly scoped v2 repairs remain available only when their corresponding validator proves that exact stale contract; they do not replace the exterior migration. Every caller first verifies the connected editor project. Import and repair wrappers hash their protected inputs, the exterior repair creates a non-overwriting backup before changing v2, and SDTH/v1/legacy/refined assets remain byte-identical. Broader failures require a new destination name and rebuild, never a destructive in-place repair.

For the final fog/context screenshot only, run `scripts/Install-IstanaDevelopmentAssets.ps1` while every editor is closed; it copies the single repository-owned `unreal/Config/IstanaVisualAcceptance.settings.json` file into the host project's `Config` directory, treats an identical file as idempotent, refuses to overwrite a different file, and rechecks the SDTH hash. Then run `scripts/Start-IstanaVisualAcceptanceEditor.ps1 -ValidateOnly`, followed by the same script without that switch. The launcher validates the exact project/map/profile, refuses a second editor for the project, and passes both `-settings=<project-owned-file>` and `-TRIADIstanaVisualAcceptance`; it never reads, edits, or copies the user's global AirSim settings. In the launched editor, use `scripts/Set-IstanaPlayInEditor.ps1 -VisualAcceptanceOnly`, which requires three consecutive exact-100% current-view readiness polls, then `scripts/Capture-IstanaPlaySpawnPreview.ps1 -VisualAcceptanceOnly -OutputFileName v2_play_visual_acceptance_<name>.png`. This profile is ComputerVision, no lidar, no RPC, and is not a production sensor test.

The correction is intentionally a new builder rather than an in-place repair. The first map recentered the Cesium georeference but did not globe-anchor/reproject inherited PlayerStart, sky, or weather actors; PIE therefore used their old local transforms while the Istana actor moved to the new cartographic origin. PIE also ran with `GameModeBase`, and the sensor-fusion subsystem applied attachment-based AirSim visual weather only at runtime, explaining why the editor camera looked normal while the player view did not. The missing surrounding structures were independently observed with Cesium load progress at zero and repeated provider HTTP 400 responses. The inverted overlay contract itself retains polygon interiors; v2 keeps that correct clip and adds bounded session refresh rather than removing the 1 km limit.

UE 5.5 itself uses `Modify()` plus direct `DefaultGameMode` assignment, so absence of property-change notification alone did not explain the failure. The original builder also validated the same in-memory `AWorldSettings` after `SaveMap`, which could not prove serialized state. After disk-round-trip validation was added, both a fresh build and an exact-map repair still reopened with the direct `/Script/AirSimTriadRuntime.AirSimGameMode` reference stripped to `None`. The map now serializes `/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode` instead. That wrapper introduces no behavior overrides and inherits the native AirSim GameMode unchanged; it is a persistence boundary in the same module as other TRIAD classes already stored by the map. Builder and repair validate both its AirSim lineage and exact class path after reopening the saved destination. Run these operations with exactly one editor session; a failed reload is reported and never treated as success.

The production full-1 km Google photogrammetry selection is intentionally expensive: physics meshes are enabled, frustum/fog culling are disabled, and off-camera tiles retain a bounded culled screen-space error. `GetLoadProgress()` at 44.5% after two minutes is traversal/refinement work progress, not 44.5% geographic AOI coverage; it may be non-monotonic as requested tiles change. Keep one production PIE session alive and extend/repeat its readiness poll. The explicitly flagged visual-acceptance profile changes only the duplicated PIE tilesets to frustum-driven refinement, retains fog culling disabled and physics enabled, refreshes once, and demands stable exact-100% current-view readiness. The saved v2 map and its complete-AOI production policy remain unchanged.

Host-source review found that AirSim lidar performs Unreal line traces and `AActor::GetComponents` inside `ParallelFor` while holding raw actor pointers; PIE can destroy those actors before the AirSim simulation-mode actor receives `EndPlay`. `pause(true)` only sets a scheduler flag and is not a join for an in-flight scan. The fork's camera director also omits `Super::EndPlay`, explaining the separate ensure. TRIAD therefore avoids automatic timeout stop, fails closed when scripted quiescence is unverified, and supplies the no-lidar visual profile for this acceptance run. Manual toolbar Stop remains unsafe with arbitrary enabled-lidar production settings until AirSim stops/joins sensor work in `OnWorldBeginTearDown` and moves Unreal object access to a safe thread; no AirSim source is modified here.

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

Generic scenarios remain on the backwards-compatible FSPL plus binary Visibility-NLOS scalar because `bUseDedicatedRFPropagation` defaults to false. The additive `Resources/IstanaHighFidelityRF.example.json` enables the TightV1 CPU query, requires a successful transactional load, verifies exact geometry/catalog/scene-contract SHA-256 values and semantic IDs, and binds the only supported world-origin/identity RF frame and declared coverage identity to the exact Istana V5D map package. The loader reads each strict UTF-8 geometry/catalog resource once, hashes and parses those same buffers, retains the consumed hashes for telemetry, and independently derives the exact coverage AABB from canonical vertices plus bounded witness endpoints. Scenario initialization independently reads the staged scene contract once and requires its hash to match both configuration and the geometry binding. Its runtime resources are byte-for-byte synchronized from `SourceAssets/IstanaPublicViewRF/TightV1` by `sync_runtime_resources.py` and staged NonUFS. A required path/hash/schema/frame/coverage load failure destroys the scenario manager. A per-link ambiguous query or opaque non-transmissive straight path fails that RF link closed; it never falls back to Visibility LOS.

Dedicated records add `propagationMode`, readiness/degraded state, geometry, scene-contract, and material-catalog IDs/hashes/schema metadata, `pathId`, `pathKind`, calibration state/provenance, and `triad.rf_interaction_trace.v3` rows. Each interaction keeps the first physical entry in its singular compatibility fields and carries an authoritative ordered `contributors` list for every solid in a merged same-material span, including entry/exit surfaces and points plus source/uncertainty provenance. It also carries material and selected-profile IDs, profile frequency/incidence ranges, explicit selection semantics, all available scalar coefficients, geometry-derived thickness/traversal/incidence, and applied loss; CSV carries the same structured array as escaped compact JSON. Only direct and straight-transmission candidates are admitted. Reflection enumeration, diffraction, coherent phase, and polarization are not implemented. `lineOfSight` remains an optical/diagnostic field and cannot control dedicated RF loss or detection. All dedicated outputs remain `SIMULATION_READY_ASSUMPTION_BOUND`; they explicitly report `readyForSurveyTruth: false` and `fieldValidated: false`.

For a live local dashboard, every sample also atomically replaces the bounded current-state document:

`<Project>/Saved/SingaporeSensorFusion/latest_rf_snapshot.json`

This snapshot is independent of the capped session JSONL/CSV files. It contains all configured sensor-node definitions and current status, verified/unverified visual-weather state, the strongest currently detected RF links (limited by `MaxLiveSnapshotDetectedLinks`, default 512), and target summaries. Links are ordered by descending SNR and then nearest slant range. Schema `triad.live_rf_snapshot.v3` retains every v2 field and adds optional bounded `searchRadarDetections` and `ptzConfirmations` arrays. Every `sensorNodes` item also advertises the stable SEARCH_RADAR/EO_PTZ/THERMAL_PTZ configured range and runtime status. `frameRelativePath`/`metadataRelativePath` are accepted only below `RadarPtzFrames/eo/` or `RadarPtzFrames/thermal/`, contain no parent traversal, and are relative to `Saved/SingaporeSensorFusion`. Existing `preliminaryCueEvidenceNodeIds`, `rfMultinodePreliminaryCueRuleSatisfied`, and compatibility `alertEvidence*` fields retain their v2 meanings. The document labels `hostileScenarioTruth` as authored synthetic truth that never gates cues, and declares every sensor model uncalibrated and detection-only.

### Inbound approach telemetry

`SimulationPerimeter` remains an axis-aligned WGS84 `Rectangle` when `Shape` is omitted, preserving existing configurations. The active bounds are longitude 103.6200--104.0200 and latitude 1.2200--1.4700, which contain all eight configured nodes. A scenario may instead set `Shape: "Circle"` with `CenterLongitudeDegrees`, `CenterLatitudeDegrees`, and `RadiusMeters`; circle distance uses a WGS84 Vincenty geodesic. Both shapes are deliberately simple scenario references, **not** Singapore national, territorial, FIR, legal, or restricted-airspace boundaries. Boundaries are inclusive and signed distance retains the same positive-outside/negative-inside convention.

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

### Istana V5D R22 hyperreal managed turf

The current V5D target is the atomic R22 six-material grass system. Four fine
turf profiles and the edge cards use deterministic 12--18 m spatial visibility
with zero default wind; the Grass004 lawn overlay uses calibrated warm colour,
distance mip bias, high roughness, restrained specular, and a flat far normal.
Connected runtime revision markers prevent a partial or scalar-only state from
being reported as R22. The transaction preserves the exact R20 map, soil,
source materials, textures, transforms, collision, navigation, and sensor/RF
authority, and cold-validates the complete seven-material roster.

### Istana V5D R19 distance-filtered managed turf (historical)

The historical R19 target is exact R19 grass plus the R18 lawn overlay. The
four-package R19 editor transaction changes only existing grass colour,
roughness, specular, normal-alpha, and subsurface response nodes. It retains the
29-expression, zero-texture topology and exact WPO/opacity path while adding
distance filtering over 8--20 m, bounded dry-blade and root/thatch breakup, a
warm `(1.34, 1.18, 1.03)` diffuse gain, `0.88` final chroma, `0.70--0.90`
roughness, `0.16--0.23` specular, and `0.50/0.03` near/far normal response.
Exactly the four grass packages are saved and reloaded. Artifact hashes guard
the R18 overlay, soil, R11 edge fade, target map, V3/V4/V5B material sources,
and four Grass004 textures. Mesh, LOD, transform, census, collision, navigation,
sensor/RF authority, lighting, and emissive state remain outside the material
mutation path and are validated as unchanged state. The R18 overlay retains the R16 provider-
seam topology exactly: 50 m fully opaque authored collar plus 8 m deterministic
outward dither. R15/R16/R17/R18 migration endpoints remain historical evidence.

The engine-independent source-contract suite can run while Unreal Editor remains open:

`python -m unittest discover -s Plugins/TRIADSensorFusion/Tests -p "test_*.py" -v`

It guards the generic any-emitter node threshold, pseudonymized truth-free operator cue payload (including omission of raw corridor identity labels), the `rf_multinode_preliminary_cue` type, storage-cap/display separation, the additive v3 live-snapshot contract, RF independence of search radar, per-node 5 km-or-greater radar configuration, radar-cue slew/settle behavior, fail-closed PTZ occlusion, allowlisted atomic latest-frame paths, synthetic-thermal labeling, debug-aid exclusion, projection-not-model provenance, and preservation of physical drone mesh scale.

The coordinated Unreal automation test is:

`TRIAD.SensorFusion.LongRange.SearchRadar.CloseThrough500Meters`

It evaluates exact 25 m (close), 100 m, 250 m, and 500 m inputs with measurement noise disabled, requires a radar detection at every range, requires measured range to equal the input, and checks that a 0.5 m target at 4-degree/960-pixel EO settings retains at least two horizontal pixels through 500 m. This is deterministic simulation validation, not a field-performance claim. A normal Unreal Editor target build and automation run are still required to validate C++ integration after source changes.
