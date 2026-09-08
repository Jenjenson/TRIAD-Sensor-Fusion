# Istana Explore V5D provider-quality preset

The V5D visual-context tileset uses a maximum screen-space error of 1.0 pixel,
an exact 2 GiB `MaximumCachedBytes` setting, and 12 simultaneous tile loads.
The cache value is a target for evicting non-required tiles, not a process-memory
ceiling: tiles required to render the current view may keep memory above it. The former
2.0-pixel threshold could leave more visible geometric error in distant public
buildings and streetscape. A 1.0-pixel target is a bounded quality step: in a
worst-case planar view it can request roughly four times as many visible tiles,
which is why the cache and provider-safe load budget are pinned rather than left
open-ended. The 12-load setting matches the capture process's per-server HTTP
connection cap and limits burst pressure; it is not a claim that the provider
retries HTTP 429 responses.

Streaming continuity is now pinned as well. `ForbidHoles=true` keeps a loaded
ancestor visible until requested child tiles are ready, instead of showing an
empty patch. The normal loading-descendant limit of 20 is pinned so detail keeps
arriving in smaller tiers instead of one larger pop. DPI scaling is explicitly
disabled for this tileset, making the 1-pixel target independent of host display
scaling. Ancestor preloading stays enabled, while sibling preloading is disabled
to avoid speculative off-frustum requests. The saved map keeps fog culling
disabled and keeps the exact 8-pixel culled screen-space-error setting. In a
game world, the context-policy actor now validates that complete saved tuple
and then enables fog culling transiently, without `Modify`, a map save, or a
tileset refresh. Visible, non-fog-culled content still converges against the
exact 1-pixel screen-space-error target.
The local building fallback is removed only after
three consecutive 0.5-second policy samples at or above 98 percent load
progress, while a drop below 90 percent restores it immediately. This reduces
one-sample readiness flicker after camera movement without changing the exact
geospatial anchor or authored-core clipping footprint.

Dithered Cesium LOD transitions remain disabled because enabling them also
disables frustum and fog culling, changing the bounded loading behavior at the
1-pixel target. Provider-supplied unlit material intent is preserved and smooth
normals are not synthesized, avoiding an unsupported relighting or geometry
quality claim.

This preset does not improve the source provider's capture quality, remove
photogrammetry artefacts, or establish survey, as-built, material, collision,
navigation, sensor, or RF authority. It preserves the exact authored-core clip,
provider-readiness hysteresis and dwell, on-screen credits, visual-only collision policy,
the exact ancestor/sibling preload policy, and non-export boundary. The local dated LoD2 surroundings remain a
fallback only. In the verified native visual-quality successor, that local fallback is
the deterministic 43,492-triangle suppression-V1 derivative: only the two
coarse landmark shells identified by `OSM:way:46521250` and
`OSM:way:1551538490` are removed, leaving their dedicated detailed overlays as
the sole local renderers. A 1,000–1,250 m synthetic annulus is coupled to the
same readiness state solely to cover the inherited terrain rim while streamed
content is unavailable. Neither derivative has collision, navigation, sensor,
RF, survey, as-built, or terrain authority.

## Runtime geospatial anchor and original-layout isolation

The streamed visual context and the authored Istana layout share one explicit
Cesium cartographic origin: longitude `103.84288055`, latitude `1.30709615`,
and `47.0 m` WGS84 ellipsoid height at Cesium scale `100.0`. The horizontal
coordinate is the repository's approximate Main Building centroid. The height
is the documented SRTM30-derived fallback, not surveyed local grade; pinning it
prevents silent misalignment but does not make it more accurate than its source.

Cold editor validation already required that tuple. Runtime validation now also
fails closed unless the world contains exactly one Cesium georeference carrying
both the V5D and `DEFAULT_GEOREFERENCE` tags, using `CartographicOrigin` with
the exact values above. There must also be exactly one Cesium tileset in the
entire runtime world, and it must carry the V5D visual-provider tag; a second
untagged terrain or building tileset is rejected rather than ignored. The
Google visual tileset must bind directly to that actor. Because Cesium composes
the [georeference actor transform](https://cesium.com/learn/cesium-unreal/ref-doc/classACesiumGeoreference.html)
and the [tileset actor transform](https://cesium.com/learn/cesium-unreal/ref-doc/classACesium3DTileset.html)
into global-to-Unreal placement, the georeference and tileset actors must retain
identity transforms. The authored-core clipping polygon's globe anchor must
bind to and resolve the same georeference actor while the polygon also retains
its identity local transform. This check runs before the transient fog-culling
write and on every hybrid-world validation, so an extra tileset, duplicate or
drifted origin, or transformed provider cannot be mistaken for correctly
aligned real-world context merely because asset `2275207` and its quality tuple
still match.

The check is read-only: it does not move the authored model, rewrite the saved
map, or select a different provider. It does not hide, move, or destroy an
unknown extra tileset. A rejected roster therefore keeps the owned local
presentation in its fail-closed fallback state while requiring the unexpected
actor to be corrected in the authored map. It does not confer collision,
navigation, terrain,
sensor-occlusion, or RF authority on streamed content. Replacing the
approximate height or centroid with survey-grade values would require
authoritative source data, an explicit ellipsoid/geoid conversion, an additive
map migration, and new alignment evidence rather than a runtime correction.

Provider-ready presentation remains one validated transaction for the current
surroundings mesh, its outer-ground loading annulus, public realm, and optional
R28 render-only environment; zero or one R28 actor is permitted, never more.
The source terrain renderer follows the same accepted policy state on its own
dependent tick and restores visible on missing, invalid, or not-yet-started
policy state while preserving its `QueryAndPhysics` collision. These visibility
changes do not alter the original authored transforms or any collision,
navigation, sensor, RF, survey, as-built, or terrain-authority boundary.

Static source-contract tests verify the exact quality tuple and truth boundary.
The one-shot provider-throttle successor was admitted only from the exact
34,992,305-byte SHA-256-pinned final V5D map. It created a verified,
non-overwriting predecessor backup, serializes only
`MaximumSimultaneousTileLoads` 64 -> 12 and `PreloadSiblings` true -> false,
then unloaded and cold-validated the saved map. Any post-mutation failure restores
and cold-validates the exact predecessor. Re-running is accepted only when the
current map satisfies the complete successor contract and the exact predecessor
backup still exists.

Visual acceptance still requires matched-camera evidence in both provider-
loading and stable provider-ready states. Guarded native migration, cold
validation, and idempotence completed on 5 September 2026. That migration's
34,992,354-byte successor had SHA-256
`859734CB9EFCB429AE7D863E677B7B370CC897EB9C100AACA7AAD6F227805815`;
the historical migration evidence is under
`D:/triad/TRIAD/Saved/TRIAD/MigrationEvidence/V5D_ProviderThrottle_20260905T161206075Z`.
It is superseded by the R25 native map at 34,993,427 bytes with SHA-256
`38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9`.
Provider-ready visual acceptance has not completed. One strict attempt was
rate-limited after reaching approximately 64 percent. A later bounded attempt
received no HTTP 429 responses, peaked at approximately 60.6 percent, and ended
non-ready at 47.254 percent after the full timeout; it correctly produced no
PNG or proof manifest. These are provider-loading diagnostics, not accepted
provider-ready evidence.

## Current-view provider workload isolation

The two historical provider-ready failures have different proven causes. The
first was blocked by repeated HTTP `429` responses after reaching roughly 64
percent. The later run received no `429` response, but its non-monotonic global
load progress peaked near 60.6 percent and ended at 47.254 percent. It did not
record tile-selection telemetry, so the repository must not claim that one
setting was the sole cause of that second result.

There was nevertheless a concrete competing workload in the reviewed tuple:
`EnableFogCulling=false`, `EnforceCulledScreenSpaceError=true`, and
`CulledScreenSpaceError=8.0`. Cesium's
[tileset reference](https://cesium.com/learn/cesium-unreal/ref-doc/classACesium3DTileset.html)
states that when frustum or fog culling is disabled, would-be-culled tiles
continue to be processed and, with enforcement enabled, refine toward the
culled SSE. At a fixed ground-level Istana view, that can spend the bounded
12-load queue and cache budget on distant fog-hidden context while the visible
view is still converging.

`ATRIADIstanaExploreV5DContextPolicyActor` now isolates the game-world workload
without changing the map package. `BeginPlay` first requires the exact saved
asset ID, 1-pixel visible SSE, 2 GiB cache setting, 12-load cap, hole
prohibition, preload tuple, fog-disabled/culled-8 tuple, and collision/nav
negative-authority settings. Only then does it set the runtime copy's
`EnableFogCulling=true`. With both frustum and fog culling enabled, culled
content no longer competes for refinement while the visible, non-fog-culled
view retains the 1-pixel target. The change is a direct transient property
write. Cesium's current
[tileset implementation](https://github.com/CesiumGS/cesium-unreal/blob/main/Source/CesiumRuntime/Private/Cesium3DTileset.cpp)
copies `EnableFogCulling` into the native tileset options during each tick,
before view selection, so this does not require tileset recreation. It does not
call `Modify`, `RefreshTileset`, any provider-source setter, or any map/package
save. `EndPlay` restores the exact saved `false` readback. Any missing snapshot,
unexpected saved tuple, failed runtime readback, or later state drift fails
validation and leaves the local render-only fallback visible.

This does not lower the 98-percent, three-consecutive-sample readiness gate,
does not change the 90-percent restore hysteresis, and does not establish a
provider-ready result. A guarded live run with exact pins and selection
telemetry is still required to measure whether the isolated workload reaches
readiness on this machine. Cesium content remains visual-only with zero
collision, navigation, terrain, sensor-occlusion, or RF authority.

## First-pose memory and queue isolation

An initial guarded first-pose attempt on 6 September 2026 exposed a startup
safety gap before useful measurement began: the owned UE5.5 process reached
about 7.53 GiB private memory while system free virtual/commit reserve fell to
about 2.25 GiB. The operator interrupted that exact helper; post-interrupt
checks found zero UE5.5 helpers, zero listeners on port 30010, about 7.99 GiB
free virtual/commit, and the protected UE5.4 CAPSTONE process unchanged. This
is historical failure evidence, not a completed diagnostic or provider-ready
result.

The subsequent exact-R27 bounded diagnostic is recorded by
`D:/triad/TRIAD/Saved/TRIAD/IstanaPreviews/ExploreV5D/explore_v5d_cesium_first_pose_memory_queue_diagnostic_v1_r27_cesium_diag_20260906T0730SGT.json`
with SHA-256
`B91A4F9F1EAFFEB783F09A03BE974B9696DC377A5E533E8FD39032ECDFDD9940`.
Its outcome is `ABORTED_BY_MEMORY_GUARD_NON_PROOF`. The independent watchdog
contained the exact owned helper at a trigger sample of 1,920,385,024 private
bytes and 6,435,196,928 free virtual/commit bytes, just below the fixed 6 GiB
floor; the minimum observed reserve was 6,254,166,016 bytes. The abort occurred
before map validation, PIE, first-pose acknowledgement, provider resume, or
selection telemetry. It therefore proves neither provider quality nor the
effect of the queued first-pose profile. The exact R27 map and DLL pins and the
protected UE5.4 CAPSTONE identity remained unchanged, and containment left no
UE5.5 helper or Remote Control listener.

`Measure-IstanaExploreV5DCesiumFirstPoseMemoryQueueDiagnosticV1.ps1` now has
three repo-side controls for the next bounded run:

- a fixed 10 GiB free-virtual/commit admission requirement immediately before
  helper launch. This preserves the continuous 6 GiB floor while allowing for
  the approximately 1.95 GiB startup burst observed in the R27 abort plus a
  safety margin;

- an independent CLR-thread watchdog samples every 100 ms from immediately
  after `Start-Process` through teardown. It retains the existing 12 GiB
  process-private ceiling and 6 GiB system free-virtual/commit floor. A breach
  is surfaced to the main workflow immediately for exact graceful quit; if it
  remains continuous for two seconds, containment is permitted only after
  rechecking the exact PID, creation time and UE5.5 executable path. A memory
  abort may publish only an explicit non-proof safety receipt after process,
  RC, file-pin, cache-path and CAPSTONE postconditions pass.
- the sole `EditorWorldSuspendedPoseBeforeResumeV1` scheduling profile writes
  only transient `SuspendUpdate`. It pauses the editor-world tileset at the
  first owned RC boundary, verifies PIE inherited that state, installs the
  exact 75 m pose, then resumes only the PIE tileset. This avoids two selectors
  and an uncontrolled initial PIE pose competing for the same provider queue.

`Capture-IstanaExploreV5DVegetationProviderEvidence.ps1` now applies the same
fixed 10 GiB admission requirement immediately before any helper launch. Its
static report explicitly records that continuous-watchdog integration is still
outstanding. Until the watchdog is safely factored into a shared helper, the
leaf fails closed before native-path reads or process launch when
`ProviderEvidenceMode=ProviderReady`. The bounded retry orchestrator is R27-
aware—it forwards the exact R27 commit receipt and expects the seven-pose R27
set—but cannot be used for live proof while that leaf guard remains closed.

These controls do not change or save the 1.0-pixel maximum SSE, 2 GiB Cesium cache
target, 12 simultaneous loads, preload policy, clipping, georeference, provider
readiness thresholds, or proof requirements. The expected pressure reduction
from first-pose queue isolation is not yet a measured result; it must be
established by a completed non-proof diagnostic receipt before continuous
watchdog logic is shared into the strict leaf and ProviderReady capture is
retried.

## Post-transaction R28 Player0 evidence seam

`Capture-IstanaExploreV5DVegetationProviderEvidence.ps1` also has an explicit
`-RequireR28VisualSuccessor` mode. It is mutually exclusive with the R27 mode
and accepts only an exact committed receipt at
`Saved/TRIAD/NativeTransactions/V5DVisualRealismR28V1/<run>/commit.json`.
The receipt must use schema
`triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1`, retain
the exact fourteen-stage build/apply/cold-validation/idempotence/postvalidation
order, identify exactly 17 new assets, bind the exact R27 predecessor, and bind
the caller-supplied R28 successor map plus both DLLs. Pending pre-capture state
must remain `VisualCaptureAccepted=false` and
`CaptureRevalidationRequired=true`; collision, navigation, sensor, RF and
terrain authority must remain false.

Before PIE, this mode calls only the versioned
`ValidateIstanaExploreV5DR28VisualSuccessorMap` endpoint. In PIE it uses only
the versioned R28 play-world, state, exact-pose and diagnostic-capture
endpoints. Each readback must contain the combined R28 world marker, exact
one-actor counts, provider negative-authority markers, and the R28 Player0
presentation marker. The fallback-visible capture path also proves that all
three render-only R28 environment components are visible to Player0 while
remaining hidden from sensor `SceneCapture` feeds.

The R28 mode deliberately requires `ProviderEvidenceMode=ProviderFallback`.
Its bounded four-image roster is classified as: a normal close lawn/tree view,
an Istana wide view, a target-locked MacDonald/Temasek streetscape-context
view, and broader surroundings. The final manifest can set
`VisualCaptureAccepted=true` only after all four exact filenames exist, are
non-empty and distinct, and all pre-/post-capture R28 state and world reports
pass. That flag accepts the complete validated raster set; provider readiness
is recorded separately and is never implied, while photorealism or
hyperrealism still requires manual review at native 2560x1440 resolution.

No R28 Player0 evidence run has been executed by this repo-only change. The
same fixed 10 GiB pre-launch admission gate remains in force, and strict
provider-ready live capture remains fail-closed until continuous-watchdog
integration is shared safely into the leaf.
