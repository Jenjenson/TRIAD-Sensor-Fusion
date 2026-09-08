# Istana current-fidelity audit and replacement architecture

> **Historical audit snapshot — superseded for current native identity.** This
> document preserves the dated V2/R29 observations, hashes, performance notes,
> and design findings as historical evidence. The current native level is now
> the committed R30-19 result, but no R30 Player0 capture has been
> human-accepted and R31--R33 remain non-native. Use the
> [operator manual](ISTANA_OPERATOR_MANUAL.md#v5d-current-r30-native-scene-status)
> for current pins/status. References below to a “current” V2 or R29 state must
> be read strictly at their recorded audit date and must not be used as current
> publication evidence.

## Decision

At the 22 August 2026 baseline, the then-current
`/Game/Maps/Istana_1km_Context_v2` scene was **not a one-to-one
digital twin** and must not be accepted or described as one. It is a useful
georeferenced prototype: a procedural public-reference building is placed over
Google Photorealistic 3D Tiles, with a hidden flat collision fallback. It does
not contain the measured geometry, site-specific three-dimensional vegetation,
survey terrain, or calibrated material response needed for an exact-real-life
claim.

This is not primarily a tuning problem. A few settings can make the existing
frame sharper and better lit, but no renderer setting can recover tree trunks,
branches, terrain relief, facade ornament, or reflectance data that are absent
from the sources. The credible remedy is a new, versioned World Partition map
built from authorised survey/BIM and licensed national 3D data. The v2 map
should remain intact as a comparison and rollback baseline.

Audit date: 22 August 2026, Asia/Singapore. The live map, assets, source
pipeline, engine source, renderer console variables, and accepted Play capture
were inspected read-only. No secret-bearing Cesium server asset, editor user
settings, endpoint URL, access token, request header, or project engine-config
file was read or copied.

## Audited evidence

### Accepted frame

The audited Play capture is:

`D:/triad/TRIAD/Saved/TRIAD/IstanaPreviews/V2/v2_play_visual_acceptance_authored_refined_final_20260821.png`

SHA-256:
`658A15A805DEFA796C56241863ACF073755D1F603543F3825C5A790001B29E27`

The frame visibly shows the failure modes described by the user:

- the Istana is pale, low-contrast, and lacks hero-scale relief and material
  response;
- foreground lawn and tree crowns read as aerial imagery melted into a coarse
  photogrammetry surface;
- surrounding trees are not discrete Unreal vegetation with trunks, branches,
  crowns, wind, or site-specific collision;
- the authored building does not integrate with a locally reconstructed
  forecourt, terrain, fountain, curbs, paths, or planting beds;
- captured background lighting is baked into the streamed texture while the
  authored building is separately lit, so the two sources cannot form a
  physically coherent photograph.

### Live level inventory

The v2 package is 328,954 bytes and contains one `Cesium3DTileset`. Searches of
the package and the live project show **none** of the following in the Istana
level:

- `Landscape` or Landscape Streaming Proxy;
- `InstancedFoliageActor` or any placed Foliage Type;
- a PCG actor or graph-generated site layer;
- Runtime Virtual Texture or Virtual Heightfield Mesh;
- HLOD context;
- World Partition metadata.

The only project vegetation relevant to this audit is a generic Megascans
common-shrub set under the unrelated roadside-construction content, plus the
Starter Content bush. There is no site-specific mature-tree library in the
Istana content and no evidence that the shrub set is instantiated in v2.

Therefore, the visible lawn, canopy, and distant context are supplied by the
single Cesium source, not by locally authored three-dimensional site assets.

### Cesium source and settings

Safe read-only calls to the live `Cesium3DTileset_0` returned:

| Property | Live value | Consequence |
| --- | ---: | --- |
| Tileset source | Cesium ion | Network-streamed provider data |
| ion asset ID | `2275207` | Google Photorealistic 3D Tiles |
| Maximum screen-space error | `16` | Moderate rather than hero-quality current-view LOD |
| Maximum cached bytes | `268435456` (256 MiB) | Small cache for a dense 1 km city view |
| Maximum simultaneous tile loads | `20` | Default-scale loading throughput |
| Preload ancestors / siblings | `true` / `true` | Sensible transition support |
| Generate smooth normals | `false` | No generated lit-surface normals |
| Ignore `KHR_materials_unlit` | `false` | Provider's unlit/baked appearance is honoured |
| Custom opaque/translucent material | none | No project-authored lighting integration |
| LOD transitions | `false` | Tile replacement can remain visible as popping |
| Occlusion culling | `false` | No occlusion saving in the current map state |
| Create physics meshes | `true` | Streamed photogrammetry also drives collision |
| Persistent frustum culling | `false` | Full-AOI physics policy increases load |
| Persistent fog culling | `false` | Context is not discarded because of fog |
| Culled SSE | `32` | Coarse off-view retention policy |

The visual-acceptance runtime transiently turns frustum culling on, but does not
change the source data. Asset `2275207` is Google Photorealistic 3D Tiles; a
Cesium-maintained issue identifies the same asset ID and source. Google
photogrammetry is a textured 3D capture, not literally a flat bitmap, but in
this location its ground and vegetation have insufficient geometric separation
and read as two-dimensional or melted. The unlit extension preserves baked
capture colour and lighting, so Lumen, a directional light, or a shadow setting
cannot make individual branches or correct the baked illumination.

Lowering maximum SSE to `2` or `4`, raising the cache to 1-2 GiB, and increasing
load concurrency can display the highest provider LOD sooner where it exists.
It cannot exceed Google's capture resolution or turn a fused canopy surface
into discrete trees. Enabling generated normals and ignoring the unlit extension
may produce more Unreal-like shading, but the normals are inferred from the
coarse mesh and baked shadows remain in the albedo. That is a diagnostic option,
not one-to-one source recovery.

The outside-only 1 km polygon is semantically correct: `InvertSelection=true`
and `ExcludeSelectedTiles=true` preserve the circle interior. It is not the
cause of the 2D look. Cesium also documents that polygon material clipping does
not reliably remove underlying physics. A local survey patch therefore needs a
deliberate collision-layer replacement, not only a visual mask.

### Ground and collision

`ATRIADIstanaStudyAreaActor` creates an Engine cylinder scaled to the 1,000 m
radius, 0.5 m thick, with its top at the study anchor. It is hidden and does not
cause the visible aerial-photo look, but it is enabled for query and physics.
Its default height is a single 47 m WGS84 ellipsoid value derived from a 30 m
SRTM cell. Optional calibration only translates the same flat disk to a median
Cesium height; it does not create terrain relief.

This fallback is deterministic and useful for a failed-stream baseline, but it
is unsuitable for one-to-one vehicle, sensor, drainage, curb, stair, or
line-of-sight simulation. It must not remain authoritative once survey terrain
is installed.

### Authored Istana geometry

The refined mesh source manifest declares:

- mapping/reference-image grade, explicitly not survey/BIM;
- approximately 124.0 by 118.2 m plan envelope from an OSM outline;
- 58,658 LOD0 triangles and 175,974 unshared OBJ vertices;
- procedural construction dominated by boxes, cylinders, frustums, arches,
  and repeated inferred bay dimensions;
- no interiors or concealed detail.

The live static mesh reports one LOD and eight sections. Its `NaniteSettings`
readback has `bEnabled=false`; the importer explicitly sets
`bBuildNanite=false`. The current direct OBJ route auto-generates simple
collision. It has not installed the authored LOD1/LOD2 files or the separate
eight-hull UCX package.

Nanite could render a denser replacement efficiently, but enabling Nanite on
this asset would only preserve its existing 58,658 triangles. It would not add
missing capitals, relief, balusters, roof assemblies, joinery, drainage,
surface deformation, or measured asymmetry.

### Materials and colour

The eight project materials are technically valid Default Lit materials with
4,096 px base-colour, DirectX normal, and packed ORM inputs. The plaster's mean
sRGB value is approximately `(218.69, 210.88, 198.66)`, so the file is warm
off-white rather than pure white. The loss of colour separation in the accepted
frame is a lighting, exposure, distance, and source-authority problem as well as
a palette problem.

More importantly, all five base material sources were generated by OpenAI image
generation prompts. They are original and license-audited, but they are not
cross-polarised scans, spectrophotometer measurements, or calibrated Istana
photography. Normal and material-property maps are procedurally derived rather
than physically measured. Exact facade colour, gloss, mineral grain, slate,
glass, timber, and wet response cannot be claimed from these maps.

The Play camera applies exposure bias `-0.25`, local bilateral exposure,
highlight/shadow contrast changes, detail strength `1.15`, and global contrast
`1.08`. Auto exposure remains globally enabled. This makes the acceptance image
view-dependent and unsuitable for a repeatable colour comparison. Exact colour
requires fixed exposure, fixed white balance, known illuminance/sun/sky, and a
calibrated colour pipeline.

### Renderer state

Read-only console-variable queries in the connected UE 5.5 editor returned:

| Setting | Value | Interpretation |
| --- | ---: | --- |
| `r.DynamicGlobalIlluminationMethod` | `0` | No dynamic GI |
| `r.ReflectionMethod` | `2` | Screen-space reflections, not Lumen |
| `r.GenerateMeshDistanceFields` | `0` | No software-Lumen distance-field basis |
| `r.RayTracing` | `0` | Hardware ray tracing disabled |
| `r.Shadow.Virtual.Enable` | `0` | Virtual Shadow Maps disabled |
| `r.VirtualTextures` | `0` | Virtual Texturing disabled |
| `r.Nanite.ProjectEnabled` | `1` | Project support exists, but the Istana mesh is not Nanite |
| `r.Streaming.PoolSize` | `1000` | 1,000 MB texture-streaming pool |
| Scalability groups | `3` | High, not Cinematic |
| `r.DefaultFeature.AutoExposure` | `1` | Automatic exposure active |

The project therefore lacks the dynamic indirect light, high-resolution virtual
shadows, large texture residency, and virtualised hero geometry expected of a
modern photoreal exterior. Those features will improve a good dataset, but they
cannot compensate for missing survey/source information.

### Audit workstation

The current workstation is an AMD Ryzen 7 5700X3D, NVIDIA TITAN V with 12,288
MiB, and 31.9 GiB system RAM. The TITAN V has no dedicated RT cores. A sensible
baseline is software Lumen and 1080p/30 fps, not hardware-ray-traced 4K/60 fps.
A full 1 km no-billboard, scan-density world may require a stronger deployment
GPU or a reduced simultaneous streaming radius even after optimisation.

## Rejected sources for a one-to-one claim

The following may remain useful for registration, outer background, or design
reference, but are rejected as final authority:

1. **Google Photorealistic 3D Tiles / ion asset 2275207** — retain beyond the
   authored area or for comparison only. Capture epoch, geometric accuracy,
   mesh topology, vegetation separation, baked illumination, and provider LOD
   are not under project control.
2. **The 47 m SRTM30 cylinder** — retain only as an explicit fail-safe. A 30 m
   sample and a flat plane cannot represent local grade.
3. **OSM footprints, OneMap display layers, and URA planning-height controls** —
   useful for rough alignment and semantics, not facade or as-built geometry.
4. **The procedural 58,658-triangle OBJ** — useful as a placeholder and
   silhouette study, not a measured heritage asset.
5. **AI-generated tileable PBR maps** — useful as temporary neutral materials,
   not exact colour or reflectance evidence.
6. **Internet photographs without calibrated cameras, overlap, scale control,
   and explicit reuse rights** — useful as visual references, not a survey or
   a defensible photogrammetry dataset.
7. **Generic Starter Content, common shrubs, full-tree billboards, and distant
   impostor cards inside the 1 km AOI** — not site-specific volumetric
   vegetation. Alpha-masked individual leaves may be used only when the whole
   plant retains a genuinely three-dimensional trunk/branch/crown silhouette.
8. **Unacquired marketplace Istana models** — reject until procurement proves
   licence, provenance, scale, topology, texture pedigree, and scan/BIM
   accuracy. A store listing is not verification.
9. **Provider photogrammetry as sensor collision in the hero patch** — fused
   tree/ground/building surfaces produce false line-of-sight and contact
   semantics.
10. **The SPH/NUS public virtual-tour model as an assumed exact source** — the
    published production account says it was manually reconstructed from 2D
    photographs because plans were unavailable. It may be licensable reference
    work, but is not automatically as-built.

## Required acquisition gate

“One-to-one” must be tied to a dated capture and measurable tolerances. No work
should claim exact replication until the following inputs are lawfully acquired
and added to the asset ledger.

### Permission and scope

- Written permission from the Istana/asset owner for exterior surveying,
  capture, storage, processing, and simulation use.
- Required Singapore Police Force and CAAS permissions for any unmanned-aircraft
  operation or aerial photography. CAAS identifies protected areas as no-fly
  zones without the required permit and specifically requires an SPF permit
  for UA operation within protected areas. The current AIP identifies the
  Istana restricted area. No capture should be attempted first and regularised
  later.
- A public-exterior-only scope that excludes interiors, concealed systems,
  security arrangements, restricted circulation, and operational details.
- A reference epoch, weather state, and renovation state. Trees, paint,
  temporary works, and adjacent buildings change over time, so an undated
  “exact forever” target is undefined.

### Preferred existing records

Request these before commissioning new capture:

- authorised architectural BIM/as-built model: Revit/Datasmith, IFC4, or
  equivalent native CAD plus material schedules;
- record survey/control and a coordinate transformation to SVY21 and the
  relevant Singapore vertical datum;
- authorised restoration elevations, sections, ornament details, and finish
  samples;
- SLA National 3D Mapping / SG Digital Twin terrain, building, infrastructure,
  and vegetation data for the outer 1 km, with the exact licence, accuracy,
  epoch, and redistribution conditions recorded.

SLA states that its National 3D Mapping programme captures the island with
laser scanning and photogrammetry, and that high-resolution models include
buildings, vegetation, infrastructure, and terrain. Availability to this
project and coverage of the protected estate must be confirmed by SLA and the
asset owner; public display in OneMap 3D is not an automatic download or reuse
licence.

### Commissioned survey when records are insufficient

- Establish a controlled network with a licensed surveyor using total station
  and GNSS/SiReNT control.
- Capture the public exterior and forecourt with terrestrial laser scanning
  and calibrated terrestrial photogrammetry. Add authorised aerial capture
  only after permits exist.
- Collect E57 and/or LAS/LAZ point clouds, calibrated RAW photographs, camera
  intrinsics/extrinsics, control/check points, uncertainty reports, and the
  capture log.
- Capture facade materials with cross-polarised photography, a ColorChecker or
  spectral reference, grey card, and measured illuminance/white balance.
  Record spectrophotometer values where colour tolerance matters.
- Survey terrain breaklines, walls, curbs, drains, steps, road crowns, paths,
  fountain edges, and all vertical features that a heightmap alone cannot
  represent.
- Record each material tree's trunk position, species, diameter, height, crown
  spread, and health/epoch. Scan trunks and major branches where authorised;
  build foliage from species-specific volumetric assets because moving leaves
  are poor photogrammetry subjects.
- For every other property within 1 km, use licensed SLA 3D data where its
  accuracy meets the agreed tier. Surveying every facade and private parcel to
  hero accuracy is a separate multi-owner programme, not a software task.

RealityScan supports combining photographs and LiDAR/laser scans and exporting
textured OBJ, PLY, GLB, USD, FBX, LAS, and other formats. Preserve E57/LAS/LAZ
masters and control reports even when a render mesh is exported.

## Replacement Unreal 5.5 architecture

### New level and data layers

Create a new non-overwriting map, for example
`/Game/Maps/Istana_1km_DigitalTwin_v1`, with World Partition. Do not in-place
upgrade v2. Use separate Data Layers so visual fidelity, physics, source epoch,
and fallback state remain explicit:

- `DL_HeroSurvey_Visual`
- `DL_HeroSurvey_Collision`
- `DL_SiteTerrainAndHardscape`
- `DL_SiteVegetation`
- `DL_Outer1km_Visual`
- `DL_Outer1km_Collision`
- `DL_Cesium_BeyondSurvey`
- `DL_CalibratedLighting`
- `DL_DebugAndProvenance`

Use approximately 128 m World Partition cells for the 1 km-radius AOI's 2 km
bounding square, with a 300-400 m visual prefetch radius and HLOD for distant authored
structures. Final cell and HLOD sizes must be profiled rather than accepted by
convention.

### Coordinate authority

Maintain survey masters in their declared Singapore CRS and vertical datum;
keep the transformation report with the data. Convert to geodetic/ECEF through
one audited path and place layers with `CesiumGeoreference` / globe anchors.
Do not manually nudge independent datasets until screenshots happen to align.
Use independent check points to catch axis, geoid/ellipsoid, and unit errors.

### Hero building and forecourt

Preferred paths are:

- BIM/CAD through Datasmith where source semantics and hierarchy matter;
- survey photogrammetry/LiDAR through RealityScan, then cleaned and segmented
  to FBX, glTF/GLB, USD, or Datasmith;
- Nanite-enabled static meshes for the measured visual master;
- separate authored collision proxies rather than complex collision on the
  entire scan.

Keep structural masses, openings, columns, capitals, balusters, louvres,
cornices, roof edges, stairs, fountain, retaining walls, curbs, and drainage as
actual geometry to the agreed distance threshold. Use 4K virtual-texture tiles
or 4K UDIM-equivalent regions for normal surfaces and 8K only where a measured
texel-density test proves it is useful. De-light captured colour before making
base colour, and derive roughness/normal/displacement from measured or
cross-polarised sources rather than a generative prompt.

### Terrain, roads, and hardscape

For the 2 km diameter context base, import a licensed DTM/DSM through a
geospatial preprocessing step into a tiled 16-bit Landscape heightmap. A
4,033-by-4,033 Landscape over roughly 2,048 m gives about 0.51 m base spacing;
it is an outer-context carrier, not the hero forecourt.

Overlay the inner estate with decimated Nanite survey meshes or Landscape edit
layers at materially finer spacing. Represent curbs, steps, drains, walls,
road crowns, fountain, and undercuts as meshes/splines; a heightfield cannot
model vertical or overhanging surfaces. Blend the surveyed patch to the outer
DTM with a measured seam, not an arbitrary grass slab.

### Vegetation

Use site-survey points and attributes to drive deterministic PCG or HISM
placement. Build a Singapore-appropriate species library with multiple age and
form variants, volumetric trunks/branches/crowns, physically plausible leaf
transmission, and bounded wind. No camera-facing full-tree billboard or
cross-plane impostor may appear anywhere inside the 1 km acceptance AOI.

Use the most detailed measured trees around the ceremonial axis and facade.
Outer trees may use lower-poly three-dimensional branch/crown variants and
three-dimensional HLOD clusters. Treat trunks and major limbs as collision;
use explicit simplified canopy volumes only if the sensor model requires
canopy occlusion. Do not make every leaf a physics body.

### Outer 1 km structures

Use the licensed SLA/national 3D dataset at its documented LOD and epoch. Tile
large models as glTF/3D Tiles or World Partition static-mesh cells. Preserve
semantic IDs and per-building source accuracy. Buildings that affect the
simulation but fail the outer-tier tolerance need targeted owner-authorised
survey, not a guessed procedural extrusion.

### Cesium integration

Keep Google Photorealistic 3D Tiles only outside the authoritative local
survey/national-data footprint or as a toggleable comparison layer. Clip the
provider visuals out of the replacement patch so old aerial imagery and the
old Istana do not remain under the new model.

Do not use one tileset for both highest-quality camera visuals and always-
resident full-AOI physics. Split the concerns:

- **visual layer:** frustum/occlusion driven, SSE `1-2` at acceptance, larger
  cache, no physics;
- **collision layer:** simplified, watertight, deterministic geometry with
  full-AOI residency and no expensive visual materials;
- **hero survey:** local Nanite visual meshes plus audited local collision.

Cesium polygon clipping is a material mask and does not by itself remove all
hidden collision. Disable provider collision in the hero patch by source
separation or an explicitly tested component/tile collision policy before
accepting sensor traces.

### Renderer baseline

For the current TITAN V, prefer UE 5.5 software Lumen and avoid assuming useful
hardware-ray-tracing performance:

- enable Lumen GI and Lumen reflections;
- enable mesh distance fields;
- enable Virtual Shadow Maps;
- enable Nanite on measured opaque hard-surface meshes after AirSim/query
  validation;
- enable virtual textures/streaming virtual textures for the survey material
  set;
- raise the texture pool from 1,000 MB only after measuring total GPU memory;
- use TSR at High/Cinematic and profile 1080p before raising output resolution;
- use calibrated physical sun/sky or a measured HDRI, not a visually convenient
  mix of baked Google lighting and a different Unreal sky.

For colour sign-off, use a dedicated calibration profile with fixed manual
exposure, fixed white balance/tint, no local exposure, no creative contrast,
no bloom/vignette/fringe/motion blur, and a documented ACES/OCIO display path.
Match source imagery captured at the same epoch, solar conditions, camera
response, and exposure. Do not grade until a white facade looks pleasing and
then call the result physically exact.

### Collision and simulation

Replace the flat cylinder with surveyed terrain collision after the new layer
passes load/readiness tests. Keep the old fallback available only behind an
explicit degraded-mode flag and make degraded mode fail digital-twin
acceptance.

Generate collision as a separate product:

- centimetre/decimetre-tolerance hardscape and building proxies appropriate to
  the sensor question;
- simple trunk/major-branch and optional canopy volumes for vegetation;
- simplified outer-building and terrain collision in always-resident cells;
- regression traces against independent survey checkpoints and known open
  apertures.

Visual triangles, Nanite fallback triangles, and collision triangles must be
reported separately. Never infer collision fidelity from a photoreal frame.

## Performance budget for this workstation

Initial acceptance target: 1,920 by 1,080, stable 30 fps in the representative
AirSim camera path, with no full-tree billboards inside 1 km.

| Resource | Initial budget |
| --- | ---: |
| GPU frame | <= 30 ms; reserve the rest of 33.3 ms for variance |
| Game/render thread | <= 22 ms each in the acceptance path |
| Peak GPU memory | <= 10 GiB of 12 GiB |
| Peak process RAM | <= 24 GiB of 31.9 GiB |
| Survey texture/VT residency | 3-4 GiB maximum working set |
| Nanite/static geometry residency | approximately 2-2.5 GiB initial cap |
| Shadows, render targets, engine/AirSim headroom | at least 3 GiB |
| Texture-streaming pool starting test | 3,000-4,000 MB, then profile |
| World Partition authored cell | approximately 128 m starting point |
| Visual prefetch radius | approximately 300-400 m starting point |

A measured hero building may reasonably contain 10-40 million source triangles
before Nanite clustering, rather than 58,658, provided the scan is cleaned and
segmented. Triangle count is not an acceptance metric by itself. Preserve only
geometry that changes silhouette, parallax, shading, or sensor interaction.
Use instancing for repeated vegetation and architectural elements. Profile
overdraw and masked foliage materials: a low triangle count with severe leaf
overdraw can be slower than a denser Nanite mesh.

If the no-billboard 1 km requirement, 60 fps, 4K output, and maximum survey LOD
must all be simultaneous, the current GPU budget is unlikely to be credible.
Change hardware or concurrency expectations openly; do not hide the compromise
with an unlabelled impostor.

## Acceptance contract for a defensible digital twin

Freeze these values with the surveyor and owner before acquisition. Suggested
starting gates are:

1. **Source and licence:** every tile, mesh, texture, point cloud, and photo has
   source, epoch, CRS, vertical datum, accuracy, licence, attribution, and
   redistribution status.
2. **Control:** survey control RMS <= 10 mm for the hero site, with independent
   check points excluded from reconstruction.
3. **Hero geometry:** exterior surface point-to-mesh 95th percentile <= 20 mm;
   principal opening/edge/check dimensions within the agreed 10-20 mm tier.
4. **Hero terrain/hardscape:** 95th percentile <= 30 mm against independent
   checks; all steps, curbs, drains, walls, and fountain edges represented.
5. **Outer terrain/buildings:** declare the national dataset's actual accuracy;
   use <= 100 mm terrain and <= 250 mm building-envelope targets only where
   the source licence/specification supports them.
6. **Colour:** measured hero materials achieve CIEDE2000 `Delta E00 <= 3` under
   the declared neutral-light calibration setup, with no clipped facade
   highlights. This does not mean every weather/time render has the same pixel
   colour.
7. **Vegetation:** surveyed trunk positions within 100 mm; species, height,
   diameter, and crown spread recorded; no full-tree billboard/impostor inside
   1 km; walk-around silhouettes remain volumetric.
8. **Held-out photo match:** perspective-correct overlays from cameras not used
   to build the model pass facade, roofline, terrain, vegetation, and context
   review. Save intrinsics, transform, time, lighting, and render settings.
9. **Collision:** independent trace/checkpoint suite passes without the SRTM
   disk or hidden Google hero collision. Visual and collision readiness are
   separate gates.
10. **Streaming:** the representative route has no missing cells, unacceptable
    mip/LOD drops, or provider geometry under the local patch.
11. **Performance:** 30-minute representative run meets the frame, memory, and
    stability budgets; report average and 1% low, not a single screenshot.
12. **Claims:** the release states the capture epoch and numerical tolerances.
    “One-to-one” is rejected if the agreed survey accuracy or source coverage
    is not available.

## Phased execution order

1. Obtain written owner/security/aviation permission and freeze the accuracy,
   scope, reference epoch, and permitted deliverables.
2. Request BIM/as-built and SLA National 3D Mapping data; perform a licence and
   accuracy gap analysis.
3. Commission only the missing terrestrial/aerial survey and calibrated
   material capture.
4. Process LiDAR plus imagery in RealityScan or an equivalent metrology-aware
   pipeline; preserve masters and uncertainty reports.
5. Build the new World Partition map and coordinate-control harness.
6. Install surveyed terrain/hardscape, hero building, then volumetric
   vegetation; validate each layer against independent checks.
7. Install the licensed outer 1 km model and separate visual/collision layers.
8. Clip or remove Google data only where authoritative replacements exist;
   retain it as beyond-survey background if licensing permits.
9. Enable and profile Nanite, software Lumen, VSM, VT, World Partition, and HLOD.
10. Calibrate lighting/exposure/colour, execute held-out visual comparisons,
    collision tests, AirSim tests, and the sustained performance run.
11. Release only after source, accuracy, legal, security, visual, collision,
    streaming, and performance gates all pass.

## Authoritative technical references

- Singapore CAAS, no-fly zones and UA areas:
  <https://www.caas.gov.sg/unmanned-aircraft/no-fly-zones-and-ua-flying-areas/>
- Singapore CAAS, UA requirements and protected-area permit rule:
  <https://www.caas.gov.sg/unmanned-aircraft/requirements-for-flying-ua/>
- Singapore AIP, restricted/prohibited areas:
  <https://aim-sg.caas.gov.sg/aim-content/uploads/aip/22-JAN-2026/AIP-2/2026-01-22-000000/html/eAIP/SG-ENR-5.1-en-GB.html>
- SLA National 3D Mapping / SG Digital Twin description:
  <https://geospatial.sla.gov.sg/media-hub/speeches/international-seminar-on-unggim-effective-land-administration-welcome-address/>
- SLA National 3D Mapping sustainability use cases:
  <https://geospatial.sla.gov.sg/geospatial-for-good/sustainability-use-cases/>
- SLA 3D topographic mapping specification:
  <https://www.sla.gov.sg/qql/slot/u143/Newsroom/Circulars/2013/csdsgsd/Standard_and_Specifications_for_3D_Topographic_Mapping_in_Singapore_Version1_Nov_2013.pdf>
- Cesium for Unreal clipping and its physics limitation:
  <https://cesium.com/learn/unreal/unreal-clipping/>
- Cesium for Unreal tileset/raster FAQ:
  <https://cesium.com/learn/unreal/unreal-faq/>
- Cesium-maintained report identifying asset 2275207 as Google
  Photorealistic 3D Tiles:
  <https://github.com/CesiumGS/cesium-unity/issues/551>
- Unreal Landscape heightmap import formats:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine>
- Unreal Datasmith supported BIM/CAD formats:
  <https://dev.epicgames.com/documentation/unreal-engine/datasmith-supported-software-and-file-types>
- Unreal PCG framework:
  <https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-framework-in-unreal-engine>
- Unreal foliage instancing model:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/foliage-mode-in-unreal-engine>
- RealityScan photo plus LiDAR workflow:
  <https://rshelp.capturingreality.com/en-US/tutorials/laserandimages.htm>
- RealityScan textured model/point-cloud export formats:
  <https://rshelp.capturingreality.com/en-US/tools/export.htm>

The architectural reference and existing model limitations remain documented in
`docs/ISTANA_VISUAL_REFERENCE_SPEC.md` and
`docs/ISTANA_DATA_PROVENANCE.md`. This audit supersedes any visual-acceptance
wording that could be read as certification of real-life fidelity.

## V5D inner-estate completeness delta — 30 August 2026

`unreal/SourceAssets/IstanaPublicViewExploreV5D/InnerEstateCompleteness` now
freezes and machine-validates 23 public-reference items selected for Stage-0 gap
tracking against repository implementation evidence. This is deliberately not
an exhaustive official inventory or a completion certificate. All 23 selected
rows remain blocking. The V5/V5B Main Building is an April 2024 pre-restoration public-
reference visual while the current official visitor notice says the building is
closed for restoration until further notice. Only the immediate plaza, formal
lawn/fountain, and hero have deterministic local render approximations; they are
not current survey/as-built evidence. Three other named buildings and Swan Pond
exist only as non-live public-context derivatives, while the remaining gates,
Lodge blocks, gardens, grounds landmarks, roads, paths, and curb/edge system are
unmodelled, partial, or provider-incidental at most.

The gate therefore rejects every current-complete or hyperreal-complete claim.
Exact status profiles and evidence lineages prevent provider tiles or source
derivatives from satisfying authored, deterministic, current, collision, sensor,
or RF requirements. Exact official-source metadata and normalized facts are
bound through a local hashed extracted-facts receipt. Official named features,
mixed reference systems, and derived completeness systems are classified
separately; no coordinates or non-public operational detail are added.

## V5D suppression-V2 and R25 context-façade update — 6 September 2026

This update concerns the package
`/Game/Maps/Istana_PublicView_Explore_v5d_hybrid`, stored as
`D:\triad\TRIAD\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap`.
It does not retroactively change the v2 inventory or the replacement-
architecture decision above. The suppression-V2 migration remains the exact
geometry predecessor. The later guarded R25 transaction completed successfully
and now provides one combined receipt for the current map, both plugin DLLs,
the five new façade materials, cold validation, the single map save, and the
byte-identical idempotent second apply:

```text
D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR25V1\r25_forceuht_native_20260905T2053Z\commit.json
```

| R25 committed artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap` | `34,993,427` | `38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusion.dll` | `4,585,984` | `31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusionEditor.dll` | `7,671,296` | `471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34` |

The receipt records `Status: PASS`, five additive assets, 17 exact material
overrides, unchanged V2 mesh geometry, unchanged shared V5C assets, one map
save, successful cold successor validation, and no byte change on the second
apply. The active render-only surroundings still contain exactly 1,386 context
features, 1,388 polygon parts, and 43,448 triangles. R25 maps its 17 existing
material slots as two official-wall, two official-roof, six fallback-wall, and
seven fallback-roof overrides. Its five new assets are one texture-free master
material plus the four corresponding material instances under
`Content/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25/Materials`.

R25 improves the readable surface treatment of the wider surrounding-building
massing without altering a vertex. The material response adds bounded façade
colour variation, clearer wall/roof separation, a repeated bay-and-storey
rhythm, simulated inset glazing, mullion/transom lines, darker plinth contact,
roughness/specular separation, and distance/atmosphere fading. These cues should
make façades and windows legible from ordinary streetscape distances instead of
reading as uniformly grey blocks. They are shader cues on the existing LoD2
surfaces—not real window openings, façade depth, interiors, scanned texture, or
new building geometry. The dedicated MacDonald House and Temasek Shophouse R24
proxies, landmark geometry, tree/grass layers, provider policy, collision, and
sensor/RF authority were not changed by R25.

The earlier OneKilometreV2 actual-file RF receipt remains at
`D:\triad\TRIAD\Saved\TRIAD\RFActualFileNativeTransactions\rf_actual_fix_20260905T184017Z\receipt.json`.
It records a passing bounded load of 42,700 triangles into 16,383 BVH nodes and
29,904,936 containment checks. Its former runtime-DLL pin has been superseded
by the R25 build, so it must not be presented as a test of the current R25 DLL.
R25 explicitly preserves the RF inputs and adds no RF, sensor-occlusion,
collision, navigation, or terrain authority.

The map continues to serialize 12 maximum simultaneous Cesium tile loads,
disables sibling preloading, and retains ancestor preloading. These settings
reduce speculative and burst request pressure; they do not improve provider
source resolution and do not prove readiness. In the strict `ProviderReady`
vegetation attempt `throttle12_ready_20260905T1618Z`, observed load progress
peaked at approximately 64 percent before repeated HTTP `429` responses
prevented the global ready gate. The cleanly stopped run produced no accepted
`ProviderReady` proof. A later attempt, `ready_retry_20260905T1643Z_a1`,
received no HTTP `429` responses but ended at 47.254 percent after briefly
reaching about 60.6 percent; it also published no PNG or proof manifest.
Cesium `ProviderReady` therefore remains unproven.

The exact-R27 bounded first-pose memory/queue diagnostic also remains non-proof,
but it now supplies authoritative startup-headroom evidence. Receipt
`D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_cesium_first_pose_memory_queue_diagnostic_v1_r27_cesium_diag_20260906T0730SGT.json`
(SHA-256
`B91A4F9F1EAFFEB783F09A03BE974B9696DC377A5E533E8FD39032ECDFDD9940`)
records `ABORTED_BY_MEMORY_GUARD_NON_PROOF`. Its independent watchdog contained
the owned UE5.5 helper when available system commit crossed the 6 GiB floor;
the trigger sample was 6,435,196,928 free bytes at 1,920,385,024 helper-private
bytes, with a 6,254,166,016-byte minimum. The abort preceded map validation,
PIE, first-pose acknowledgement, provider resume, and selection telemetry.
Consequently it does not measure provider quality or the queued first-pose
profile. It does establish that the tested machine state, with the protected
UE5.4 CAPSTONE session open, lacked safe startup commit headroom. Exact map and
DLL pins and the protected session were unchanged, and cleanup left no UE5.5
helper or Remote Control listener. A retry requires materially more system
commit headroom; weakening the guard would not be valid evidence.
The post-receipt wrapper now also requires 10 GiB free virtual/commit before it
will launch the helper, while retaining the continuous 6 GiB watchdog floor.
That strengthened current wrapper is not the byte-identical script pinned by
the historical abort receipt.

The existing native local-fallback tree and grass presentation remains useful:
layered canopies, readable trunks, and grounded shadows are visible. The current
R29 lawn, however, is still smooth at ordinary camera distance; it is not
defensible to say that its fine texture is already readable without moving close.
Close grass remains flat and repetitive, not a measured blade-level surface.
MacDonald House and the Temasek Shophouse
remain recognisable procedural study proxies with simplified materials and
sparse street dressing. The R25 context façades are likewise procedural and
distance-readable, not survey, as-built, field-calibrated, or hyperreal. The
transaction receipt alone is not screenshot acceptance; pair it with a passing
exact-pin visual-evidence manifest when judging or presenting the rendered
result. Surrounding
buildings are present, but the defensible claim remains diagnostic visual
context—not a hyperreal digital twin or provider-ready completeness.

## V5D R29 native state and R30--R33 source queue — 7 September 2026

The current native map is later than the R25 snapshot described above, but it
still precedes R30. Its exact state is:

| Current native artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap` | `37,465,844` | `A05C95CEF30DFF1675B049970D993F1DBA75CB88BE38BFC73E7C8077BC5C1F92` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusion.dll` | `4,981,248` | `30EF244443737D3BA909F96C2B5846951621378D32F81AAB13CF765635D42276` |
| `Plugins/TRIADSensorFusion/Binaries/Win64/UnrealEditor-TRIADSensorFusionEditor.dll` | `8,252,928` | `E0CC5AD603DCB3E243EAFEC09D4E66666CD6FE00922E4D57FB6DA87B7A8E61DE` |

No later source-prepared revision may be described as visible in this native
map yet. The ordered queue is deliberately strict:

1. Commit and capture R30. The first transaction now promotes the exact
   TreeRealism material-response v3 runtime/editor source closure before the
   facade stages, admits exactly thirteen new response-material packages and
   five rebound managed tree meshes, and cold-validates twenty-six runtime
   response MIDs. Its capture wrapper requires five mechanically valid images,
   explicit human tree review, and hash-pinned acceptance before it can satisfy
   the strict successor chain; no R30 capture has run yet.
2. Commit R31, capture its five baseline Player0 views plus the unchanged-pose
   `surroundings_oblique_macdonald` raster-fallback comparison, and stop at
   `PENDING_VISUAL_REVIEW` with exactly six PNGs.
3. After a human reviews all five baseline images and explicitly accepts the
   same-pose Nanite/raster pair, re-decode and re-hash all six PNGs and publish
   the R31 accepted receipt.
4. Commit R32 and capture its ten Player0 views, including 12 m, 50 m, 65 m,
   90 m, and 95 m turf probes, using the same two-phase review boundary.
5. Only after accepted R32 evidence may R33 add the visual-only Cesium World
   Terrain reference. Capture R33 as the same four fixed Player0 poses in both
   `GooglePrimary` and naturally reached `CwtPresented` states, then stop at
   `PENDING_VISUAL_REVIEW`.
6. A human must review and explicitly accept all eight hash-bound R33 PNGs.
   Acceptance restores and preserves `GooglePrimary`, excludes `SafeLocal`
   diagnostics, and does not authorize an R34 successor.

R31 source-prepares the retained 43,448-triangle surroundings shell with a
seventeen-slot façade treatment so windows, floor rhythm, roof/wall separation,
and restrained Singapore-context/HDB-inspired spandrel, pier and two-storey
slab-edge shading remain readable at medium distance. Those are render-only
depth cues—not balcony geometry, exact per-building classification, or
current-site façade truth—and R31 still deliberately omits fabricated local
plinth/contact dirt. R32 source-prepares one render-only owner with twelve HISM
buckets and 4,608 deterministic modeled turf clusters. Fixed source-audit probes
show why geometry alone cannot solve the ordinary-distance lawn: modeled tips
span approximately 9--16 pixels at 12 m with a 0.46-pixel median blade width,
6--10 pixels at 20 m with a 0.23-pixel median width, 2--4 pixels at 50 m with a
0.09-pixel median width, and 2--3 pixels at 65 m with a 0.074-pixel median
width. The latter widths are sub-pixel and remain a native visual-acceptance
risk. The revised source therefore adds four isolated 32-expression turf
material derivatives with continuous rotated 11.0 m and 3.4 m value-noise
response. Material response ramps over 14--20 m, is fully active through
20--65 m, and fades over 65--76 m; HISM geometry still fades over 65--90 m and
wind WPO remains disabled from 60 m. This changes neither the exact transforms
nor geometry census and adds no collision, navigation, RF, sensor, terrain, or
site-truth authority. R33 remains visual terrain context only: Google
Photorealistic 3D Tiles asset `2275207` and Cesium World Terrain asset `1` share
the existing georeference, while the original terrain remains the only
simulation/collision authority.

The V5D tree-realism source pack now also includes an isolated material-response
amendment for the 729 main/heritage trees. It creates thirteen V5D-owned material
derivatives and twenty-six runtime response MIDs from the exact pinned V4 source
texture/material roster. Bounded per-instance canopy and bark variation,
canopy-only subsurface tint, and roughness clamps reduce the pale, repeated look
without changing the five admitted mesh derivatives, transforms, material-slot
order, LOD topology, opacity graph, `0.333` mask clip, or the six-parameter wind
WPO path. The accompanying contact sheet uses the actual pinned texture inputs,
but it is a CPU source/MIP diagnostic rather than an Unreal beauty render. It
does not establish species, botanical health, season, survey, or native visual
acceptance, and it adds no collision, navigation, RF, sensor, or terrain
authority.

That source pack is no longer orphaned from the native queue. R30 now pins the
four exact TreeRealism runtime/editor sources and their native R29 preimages,
the v3 contract, and four audit artifacts. Its rollback journal restores the
complete prior TreeRealism namespace and source preimages on failure. On
success, R30 records exact before/after content receipts; each R31--R33
transaction and capture replays the same nested identity and refuses drift in
the thirteen response materials, five rebound meshes, twenty-six runtime MIDs,
or accepted human-review chain. This is still source/static proof only until
the guarded native R30 transaction and five-image review are completed.

A separate, explicitly unadmitted
`TreeRealism/GeometryVariationCandidateV4` pack now addresses the remaining
five-mesh repetition without changing that queue. It decodes the exact five
pinned source vertex arrays, defines three deterministic nonlinear geometry
recipes for each broad form (fifteen candidates), and binds a 736-row selector
covering all 729 existing general/Heritage trees plus the seven R29 landmark
additions. Its fixed-view source audit holds the sampled root plane exactly and
keeps material-slot, UV, polygon-group, LOD-count, geography, collision,
navigation, line-of-sight, RF, sensor, and terrain authority unchanged.

An unnumbered post-R33 source scaffold now mirrors those fifteen recipes and the
exact selector into an initially hidden, render-only fifteen-HISM owner. Its
only reflected editor endpoint is read-only receipt inspection; caller-supplied
paths and hashes explicitly cannot authorize execution. The exact assets-only
materializer is private and non-reflected. It retains the source-defined logic
to clone and warp every explicit LOD while preserving topology, UVs, vertex
colours, polygon groups and material-slot bindings, then recompute normals and
tangents, but two deliberately invalid compiled trust anchors make every write
attempt fail before source-mesh loading or namespace mutation. A future reviewed
source change must pin the exact human-accepted R33 and narrow successor
authorization receipts, recompile, and explicitly expose or call the private
implementation. The runtime owner's configuration and activation methods are
likewise private, non-reflected and uncalled, with a separate pair of deliberately
invalid compiled receipt anchors; arbitrary caller hashes and a suppression
Boolean cannot authorize presentation. Exact source-layout comparison uses
scalar bit patterns (including quaternion sign and signed zero), while native
HISM matrix readback alone uses bounded `0.02 cm` translation and `1e-5`
rotation/scale tolerances. The installed-header audit remains static and
source-only. The integrated candidate, TreeRealism, and material-response suites pass `46/46`
under the bundled Python runtime. This is still not a native result: UE5.5
compilation, trust-anchor activation, source-LOD MeshDescription availability,
creation/cold reload of the fifteen packages, the 488 V4-main form resolutions,
exact native transform hashes, atomic source suppression/rollback, PIE, capture,
performance and human acceptance remain outstanding.

**Source-only tropical umbrella hero candidate and dormant integration.** The
[candidate record](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroCandidate/README.md)
and [post-R33 integration record](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/README.md)
are repository source only, non-live and not visible in the current R29 map.
Only six source HISM instances whose selector rows are already native-classified
`sourceForm=umbrella` are eligible: four from
`V4HeritageUmbrellaSilhouetteProxies` and two from
`V5DLandmarkTreesUmbrella`. Every one of the 720 unresolved V4-main raster rows
is excluded. Their exact ordered routing is `C/B/B/B/A/B`, or A1/B4/C1 by
distribution. The remaining classified non-umbrella rows are not selected.

The private source path requires the exact target world, owner, childless
source components and 4+2 component counts. It reads the six ordered native
world transforms, requires the caller copies to match bit-for-bit and requires
the canonical before/after transform SHA-256 to remain identical. Before
suppression it snapshots both counts, all six transforms, visibility and
hidden-in-game state. Suppression changes only those two presentation fields
with propagation disabled. Every later failure clears the candidate, restores
both source components, revalidates their counts and transforms, and attempts
physical cleanup of both staging and output namespaces that were proven empty
at entry. Post-import OBJ/MTL bytes are also rehashed. This is an exact
fail-closed source implementation, not evidence that native snapshot, swap or
rollback has executed.

Editor materialize/swap, runtime selection and runtime activation remain three
independent compile-time `false` gates with distinct invalid current/future
trust-anchor pairs. Only the read-only receipt inspector is reflected; the
private materializer has no caller, and no package write, activation, map save
or numbered successor is authorized. The sealed
[integration contract](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/tropical_umbrella_hero_post_r33_integration.source_contract.v1.json)
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

The exact native order is unchanged: R30 commit plus human capture, R31 commit
plus human capture, R32 commit plus human capture, then R33's explicit
eight-image human acceptance. Only after that chain may a separately reviewed,
recompiled and explicitly authorized successor be considered. Outstanding
proof includes UE5.5 compile/link, import/save/cold reload, injected rollback
after each suppression branch, native LOD transitions and culling, wind/alpha,
shadows/subsurface, equal-condition frame-time and memory, fixed-view comparison
and explicit human visual acceptance. The broad public reference supports an
umbrella-canopy design cue only, not a Rain Tree identity, current inventory,
survey or placement claim. This scaffold has no geography, collision,
navigation, line-of-sight, RF, sensor or terrain authority.

The old admitted palm remains only 3,208 triangles and is not sufficient for a
close-range hero tree. A separate unadmitted `TreeRealism/PalmHeroCandidate`
source pack now provides deterministic 104,244-, 37,968- and 9,864-triangle
LODs, a curved annulated/root-flared trunk, an asymmetric crown, closed-volume
rachises and leaflets, stable `Bark`/`FrondLive`/`FrondDry` slots, exact `Z=0`
root contact and four byte-pinned 2K CC0 Poly Haven bark maps. Its checker,
two-clean-build reproducibility proof and `8/8` tests pass. A separate
source-only PalmHero integration scaffold now hash-pins every candidate input,
checks the three exact LODs and material routes, provides an optional runtime
source with the admitted palm as mandatory fallback, and preserves the exact
eight-package import/materialization behavior. Its sole reflected Blueprint
method, `InspectPalmHeroReceipts`, is inspection-only: it can verify the exact
candidate closure plus caller-pinned, human-accepted R33 and narrow future
authorization receipts, but cannot authorize execution or write packages. The
materializer is private and non-reflected, and its two compiled SHA-256 trust
anchors are deliberately unset non-hash sentinels. Caller-provided paths and
hashes therefore cannot self-authorize, and no native package write is reachable
in this source state; activation requires a separately reviewed source change
pinning two distinct trusted receipt hashes, a recompile, and an explicit call
or exposure of the private method. Future authorization also rejects extensions
outside its exact field roster. If activated later, partial or unexpected
recursively inventoried output namespaces still fail closed, rollback remains
limited to namespaces proven empty at entry, and every output remains bound to
exact candidate, source, accepted-R33 and future-authorization metadata.
Existing textures additionally require one resolved import source with matching
MD5, exact retained-JPEG source payload bytes and SHA-256, pinned source
topology/format, and the intended unflipped DirectX normal setting. The scaffold
also models the installed UE5.5 legacy OBJ importer's unconditional `(X,Y,Z)` to
`(X,-Y,Z)` conversion and validates separately pinned UE-space bounds. The
focused PalmHero suite passes `16/16`; the combined Palm-candidate, PalmHero and
tree-geometry regression passes `48/48` under the bundled runtime. It has
still not been compiled, imported, saved, cold-reloaded, shaded, performance
tested, or accepted as an Istana species/current-condition match. Native OBJ
semantics, leaf transmission, subsurface, wind, LOD transitions and rollback at
each save boundary therefore remain explicit future proof obligations. Both
tree candidates may be considered only after R33 human acceptance through a
separate successor transaction.

**Source-only outer-context vegetation candidate and dormant integration.** The
separate, unnumbered `Vegetation/OuterContextVegetationCandidate` pack reads
only the frozen 25 August 2026 OpenStreetMap snapshot and emits exactly 145
sanitized, unjittered local-XY placements under the existing pinned hero-local
axis convention in the strict `300 < r <= 1000 m` ring: 32 of 33
`natural=tree` points plus 113 of 115 complete-bin samples from 22
`natural=tree_row` ways after exactly two `< 4 m` point-priority removals. All
145 Z values remain null. Raw names, descriptions, species and access/security
tags are not retained; source identifiers remain for traceability, and the pack
carries the required OpenStreetMap attribution and ODbL notice.

The new unnumbered `Vegetation/OuterContextVegetationIntegration` scaffold
mirrors all 145 keys, XY values, yaws and scales exactly as fixed-point integers,
routes them into 12 render-only HISM buckets, and accepts only an exact ordered
key-to-finite-Z roster from separately proven terrain-contact evidence. Its
source values remain zero-tolerance; only native HISM matrix readback receives
separate bounded tolerances of 0.02 cm translation and `1e-5` rotation/scale.
It admits only the four exact TreeRealism fallback forms or the 12 exact
GeometryVariationCandidateV4 assets with the expected four LODs, slots and
response materials. Configuration and presentation methods are private and
uncalled; four compiled trust anchors remain deliberately invalid,
`candidateDistributionGatesSatisfied=false`, and no caller value can activate
it. It is hidden in `GooglePrimary` and `CwtWarming` and is merely future-eligible
in `CwtPresented` and `SafeLocal` after a reviewed source change.

The candidate checker and two-clean-build byte-reproducibility proof pass
`9/9`; the integration source contract passes `12/12`, for `21/21` focused
checks. Both directories are excluded from the GitHub-free source-assets bundle
while ODbL share-alike, machine-readable-access and owned runtime-visible
attribution gates remain unsatisfied. The XY and oblique PNGs remain labelled
source-distribution audits. Because repository `SourceAssets` is an existing
D:-backed junction, the storage boundary is no live native `Content`/`Binaries`,
map or DLL mutation, not a literal no-D-write claim. This is not a native result,
does not authorize a numbered stage, and proves neither terrain contact nor
inventory completeness, currency, species identity, survey accuracy,
performance, visual acceptance, collision, navigation, line-of-sight, RF or
sensor authority.

The source-only
`Vegetation/OrdinaryDistanceGrassSurfaceCandidate` now supplies a restrained
distance-surface option for the smooth-lawn gap. It reuses the byte-pinned CC0
ambientCG `Grass001` 2K maps at the provider-published 1.4 m scale, blends two
rotated phases over 7 m, and adds bounded 3.5/8.4/18.2 m meso/macro variation.
Its corrected audit measures only the intended +/-5 m world-distance bands:
contrast/detail-delta are `0.080690/0.024243` at 20 m,
`0.081796/0.025028` at 35 m, and `0.082635/0.025432` at 50 m. All exceed the
unchanged `0.08` and `0.0025` source gates; its checker, two-clean-build proof,
and `10/10` tests pass, and an independent ray/mask rebuild reproduces the
metrics exactly. This proves deterministic source look-development only, not
native material response, hardscape exclusion, temporal stability, performance,
or human-visible Unreal acceptance.

An unnumbered post-R33 source scaffold now defines a dormant assets-only route
for that grass candidate: five 2K retained-JPEG textures and one standalone
material below a single isolated namespace. The material preserves the
provider's 1.4 m scale, dual rotated phases, 7 m blend, 3.5/8.4/18.2 m
variation and 20/35/50 m normal-response intent; its phase normal is
inverse-rotated through the exact UV Jacobian before blending. Only Base Color,
Roughness, Normal and Ambient Occlusion may be connected. Height is retained
as a zero-contribution input and cannot reach WPO, displacement, pixel depth,
collision, physical material, Nanite override or any simulation authority.
Runtime selection requires the exact existing lawn material as its fallback
and performs no map or component binding. The only reflected editor endpoint
is read-only receipt inspection: caller-supplied paths and hashes cannot
authorize execution. The write-capable materializer is private and
non-reflected, requires two distinct compiled SHA-256 trust anchors before any
source load or namespace write, and both anchors are deliberately invalid
sentinels. Its future-authorization document also requires the exact contracted
field roster with no extensions. A separately reviewed source change must pin
the exact human-accepted R33 receipt and distinct narrow authorization,
recompile, and explicitly expose or call the private implementation before the
six-asset write path can become reachable. Namespace and fresh rollback checks
remain fail-closed. Its source contract suite passes `16/16`; combined with the
candidate suite this is `26/26`, and the additional related R32 turf suites
bring the bounded grass regression run to `60/60`. UE5.5 compile/link, actual
import/save/cold reload, injected rollback, map exclusion/crossfade, native
capture, performance and human acceptance have not run.

A higher-impact source-only successor,
`Vegetation/GradedTurfPresentationIntegrationV2`, now preserves the actual
accepted R23B lawn-overlay boundary instead of substituting the standalone
opaque candidate material. Its private materializer first clones the exact
37,844-byte `M_IPV5D_LawnMacroVariation` package, keeps `BLEND_Masked`, the
`0.5` clip and the complete 64-point estate-boundary/core-feather opacity-mask
subgraph, and grafts the existing 25-node Grass001 dual-phase response onto
only Base Color, Roughness, Normal and Ambient Occlusion. Specular and
OpacityMask remain inherited; World Position Offset, displacement and Pixel
Depth Offset remain disconnected. R29 retains exclusive ownership of all
6,144 placements and R32 of all 4,608 placements, while the surface-response
intent continues through the 65--90 m blade fade to the 95 m proof probe. The
runtime boundary snapshots and exactly restores the complete
`V5DGroundMacroVariationOverlay` material state, but selection and activation
are independently compiled `false`. The one-material materializer is private,
non-reflected and uncalled behind distinct invalid accepted-R33 and future-
authorization anchors. The upstream OrdinaryDistance Grass001 material also
has no accepted native package yet, so a separate byte/SHA-256 dependency pin
is deliberately invalid. The private path must admit a clean, persisted,
reviewed package and independently validate its complete static 25-node code,
parameters and topology before any graph copy; a live 25-node roster cannot
self-authorize. Final validation compares full output-index/mask selectors for
the inherited OpacityMask and Specular roots, exhaustively permits only those
two plus the four grafted roots, and rejects expressions outside those six
closures. The focused source contract passes `14/14`; no package
has been materialized, saved, bound to the map, compiled into a native result,
captured or visually accepted. The 0--95 m result is therefore a source design
and proof obligation, not a claim about the live R29 view.

The separate, unnumbered
`Surroundings/BuildingSurfaceOpticsCandidate` now layers a bounded presentation
response over the exact R31 broad-shell inputs without emitting or changing any
geometry. It preserves the 43,448-triangle mesh, all 17 material slots, all
1,388 retained groups and the identity transform. Its deterministic comparison
adds glazing Fresnel/clear-coat response, aluminium-like frame separation,
continuous wall breakup and restrained roof colour/normal/roughness variation.
The pane signal is keyed to `floor(q_u,q_v)` from the same archetype-weighted
R31 aperture coordinates, so it is constant within each existing pane rather
than crossing the façade grid: 126 qualified sampled cells have a maximum
within-cell range of `0.0`, while all 16 bounded adjacent-cell probes differ.
The source comparison records an 18.745% local-neighbour-gradient increase, a
15.505% shell-luminance-standard-deviation increase, 29.819890% of shell pixels
changing by more than 4/255, and wall/glass roughness separation of `0.565626`.
Its focused `12/12` tests, read-only check and formula self-test pass. These are
CPU look-development measurements only. Native reflections, temporal response,
matched-camera readability, performance and human acceptance remain unproved;
the candidate does not establish exact façade/material truth for any real
surrounding building.

An unnumbered post-R33 source scaffold now defines the corresponding isolated
Unreal material route without exposing a write-capable public endpoint. It
preserves the exact ordered R31 fallback roster, clones the R31 master graph
and seventeen slot-specific material instances, and connects the candidate's
Base Color, tangent Normal, Roughness, Metallic, Ambient Occlusion, Clear Coat
and Clear Coat Roughness outputs under the compatible Clear Coat shading model.
The only reflected editor endpoint is receipt inspection. Materialization is
private and non-reflected, and two deliberately unset compiled trust anchors
make every write attempt fail before namespace mutation until a separately
reviewed source change pins both receipts and recompiles the plugin. Recursive
namespace validation and fresh-only rollback are source-defined; no map or
component binding is provided. The focused candidate and integration suites
pass `27/27`, and an independent installed-header/semantic audit found no
remaining source defect. UBT/UHT and shader compilation, trusted-anchor
activation, save/cold reload, native component application, matched captures,
Nanite/raster parity, performance, authority regressions and human acceptance
remain unproved native work.

The separate unnumbered
`Surroundings/BuildingFootContactCandidate` addresses only the visibly floating
building-foot subset without modifying the frozen shell. Its deterministic
audit proves that 1,370 of 1,388 retained groups put 23,746 opaque wall
triangles exactly on the existing source Z=0 plane; the eighteen elevated
groups and their 614 wall triangles start at or above 3.2 m and receive zero
response. The restrained cue is fully active only through 0.10 m and zero at
1.25 m, excludes glazing, and caps its changes at 8% base-colour darkening,
+0.05 roughness and 7% AO darkening. Candidate tests pass `9/9`.

Its matching dormant post-R33 scaffold emits no native asset and exposes no
write-capable public endpoint. It derives one isolated Default Lit master and
seventeen exact R31-fallback clones, preserves all 42 R31 expressions and the
existing Base Color, Roughness, Normal, Metallic and Specular edges, and
reroutes only one added Ambient Occlusion output. The private, non-reflected
materializer has no production call site; two invalid compiled trust anchors,
a compiled-off runtime selector and fresh-only namespace admission keep it
unreachable. Integration tests pass `14/14`, for `23/23` with the candidate;
the adjacent optics/R31 regression run passes `49/49`, and independent static
review found no remaining concrete source defect. This material roster is an
alternative to, not a cumulative overlay on, `BuildingSurfaceOptics`; a future
accepted transaction must deliberately compose the two effects or select an
accepted predecessor. Native compile/shaders, save/cold reload, visible-foot
source-plane alignment, map application, Nanite/raster parity, performance,
authority regressions and human acceptance remain unproved. The public-realm
junction and outer 950--1,000 m rim therefore remained the two open seam
subsets at that point.

That earlier either/or limitation is now resolved in source by the separate
`Surroundings/BuildingOpticsContactCompositionIntegration` scaffold. Its exact
order is the R31 broad-shell presentation, then every BuildingSurfaceOptics
output, then the bounded BuildingFootContact response. Contact can modify only
the already-computed Base Color, Roughness and Ambient Occlusion; tangent
Normal, Metallic, Clear Coat and Clear Coat Roughness pass through unchanged.
It preserves the R31 43,448 triangles, seventeen ordered slots, 1,388 retained
groups and identity transform, and would create only one isolated Clear Coat
master plus seventeen direct instances. The exact optics roster is the
preferred fallback and the exact R31 roster the ultimate fallback. Candidate
selection and activation are independently compiled `false`; the materializer
is private, non-reflected and has no call site behind two distinct invalid
trust anchors.

Final source validation closes the exact normalized 42-node R31 graph: every
unchanged predecessor node class, payload and input edge, every root selector,
the Specular route and the original `R31.SurfaceRoughnessA -> MP_Roughness`
route must match. All 42 nodes must be root-reachable, with no orphan or custom-
output substitution admitted. The first 23 custom Surface inputs remain exact;
added inputs 23--25 require exact names, source expressions, selected output
indices, independent `FExpressionInput::InputName` values and component masks.
All seven Surface outputs require their exact ordered names, zero masks and
`bShowOutputNameOnPin=true`. The eleven presentation override families are
three-way exact across candidate, optics and R31. Editor validation denies
direct physical material, the separate `PhysMaterialMask`, and physical-
material maps across that chain; runtime validation also denies direct
`PhysMaterialMask` and effective `GetPhysicalMaterialMask()` on the candidate
and preferred-optics Clear Coat rosters.

The independently rerun focused source contract passes `17/17`; the combined
result is `68/68`, including `51/51` adjacent optics/contact/R31 checks. The
final sealed integration contract is exactly 12,632 bytes with SHA-256
`021C445DC42BD8461FE2741792C5E013D3B55F86DBA6FFC050C17760AEA09E6A`.
This is still dormant source evidence only: no Unreal launch, UBT/UHT compile
or link, native automation run, shader compile, materialize/save/cold reload,
map or component binding, source-plane alignment, capture, performance proof,
Nanite/raster parity or human visual acceptance has occurred. No asset or
visible building change is live, and the synthetic source plane remains
explicitly not measured local grade.

The new source-only
`PublicRealm/JunctionContinuityCandidate` now narrows the first subset without
claiming a geometry repair. Its audit reconstructs the exact 1,141 road-base
triangles as valid zero-gap/zero-overlap touching Core/Fallback surfaces and
proves all 3,423 road-corner UV0 records equal logical hero-local XY metres.
Only the sixty cross-Core/Fallback duplicate/tolerance-equivalent corner
clusters are admitted: 355 indexed normal rows receive a deterministic
source-triangle-area-weighted mean, with maximum adjustment
`0.044152281` degrees under the fixed `0.05`-degree cap. Same-role clusters are
excluded and both input OBJs remain byte-identical. Builder `--check` and
`10/10` tests pass, including two byte-identical clean builds, exact pins,
recomputation and authority boundaries. The separate dormant
`PublicRealm/JunctionContinuityIntegration` scaffold now hash-seals the exact
candidate closure and prepares two isolated sibling-namespace render meshes.
Its private, non-reflected materializer has no call site, its accepted-R33 and
future-authorization anchors are distinct invalid sentinels, runtime selection
and activation are independently compiled off, and the exact existing
Core/Fallback pair remains mandatory. Recursive fresh-namespace validation and
rollback cover partial or unexpected output. The integration suite passes
`14/14`, for `24/24` candidate-plus-integration checks. No Unreal asset or map
binding was produced, so UE 5.5 compile/link, materialize/save/cold reload,
exact native normal-only readback, junction captures, Nanite/raster parity,
performance/authority regressions and human acceptance remain open.

The older PublicRealm provider-clip lineage is now closed without repinning
the intentionally evolved live actor. The exact 78,979-byte historical
`TRIADIstanaExploreV5DContextPolicyActor.cpp` at SHA-256
`9AFE7C7BD858D6C533AF36A005AFC820AA7037D73850B8DA8BACBF9AAC8208BC`
remains the immutable provider-clip replay input. In addition,
`PublicRealm/NativeSourceClosure/VQSP20260905` now preserves the ten exact
promotion inputs that evolved after the 5 September visual-quality successor
manifest. Its authoritative completed 52-file snapshot has manifest SHA-256
`9DF232B3724A43C621DFAD03E611D6DC5B5C779135577CC1DDA375B293004072`;
the 31,206-byte completion receipt at SHA-256
`4268ED027D07B31CAC896AA8F4268038B227FE8E3AEC9268A7119A2621572AB4`
records `State=COMPLETE`, and an independent rehash matched all 52 files. The
closed ten-row helper maps short physical closure names back to the original
canonical composite-snapshot paths without overwriting current repository or
native source. The historical promotion-input and successor-promotion suites
pass `21/21`. This restores reproducible historical replay isolation only; it
is neither a rollback nor a promotion or alteration of any current native
asset.

The new source-only `Terrain/OuterContextRimSeamCandidate` is deliberately
`INELIGIBLE_FAIL_CLOSED`. It preserves exact R29 response through 950 m,
proposes only Base Color/Roughness blending across 950--1,000 m, returns exact
endpoint inputs explicitly, and passes the R29 opacity mask unchanged. It adds
no mesh, height, normal, tangent, UV, WPO/PDO/displacement, transform or
authority. A receipt and recorded-patch chronology proved that the earlier
OuterGround failure was a current direct-source closure drift caused by
intentional capture-harness improvements, not a terrain, mesh, material or
native-asset change. The current MacDonald harness is therefore pinned at
68,376 bytes / SHA-256
`2044B6C16610BEE849E4365F9929E0A9BCABE47DA998123D84F8A95AE9E2FFB1`,
and the current Temasek harness at 92,280 bytes / SHA-256
`8B95101C1E5B7B027057E9777652B44BC6D8943AF4C27A6A206B9CFB32BB1`.
Only the current direct-source contract/manifest/lock cascade was rebuilt;
the older native-factory and cooked-provenance closures remain explicitly
historical and were not repinned. Both OuterGround suites pass `14/14`, and the
rim builder/check/self-test plus focused suite pass `10/10`. Activation remains
denied because native evidence has not classified the seam as material-only,
the topmost visible outer renderer is unresolved between the policy-owned
material and the separate R28 +0.5 cm overlay, and the historical cooked
identity still requires a separate admission. No integration scaffold exists;
the recipe cannot hide missing provider data, a transition failure or an
unresolved vertical datum. The broader successor-promotion suite remains
`12/13` on an unrelated historical `ContextPolicyActor.h` size pin and was not
blindly repinned.

`docs/ISTANA_PUBLIC_VEGETATION_REFERENCE.md` now separates public visual
evidence from implementation claims. It uses official Istana, NParks,
Roots/NHB and Meteorological Service Singapore sources to justify broad targets
such as umbrella, dense-dome, high-fork, columnar and palm silhouettes, while
explicitly declining to infer private coordinates, a complete current
inventory, sensor consequences, or species identity for any simulated anchor.
Its source/claim-boundary contract passes `4/4` tests.

The complete guarded R30--R33 transaction/capture source chain is now
repository-validated, including R33's exact eight-image, two-phase Player0
review gate. Fourteen TreeRealism plus R30--R33 source/native/capture suites pass
`194/194`; the three deterministic R30--R32 offline audit suites pass `27/27`,
for `221/221` across the current visual queue. All eight repository-only static
self-checks independently pass without native access or an Unreal launch. The
staged actor closure is now compile-correct at both ends: the R29 wrapper maps
an explicit historical 19,064-byte
`TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp` closure with
SHA-256
`F57C4C51F666290EF3C74CB03E4A536C476250CCC3AC3BD008D92456C824CA36`
to the canonical native destination, while R33 promotes the current
19,215-byte successor with SHA-256
`1F598910BC4D763BBBA04FD55DBE07D0F59B80078BB973924CDFBB40745E1DF7`
as its ninth C++ input. This prevents R33-only context-policy symbols from
leaking into an R29 replay while preserving the intentional dual-terrain
presentation rule at R33. R33 now has nine C++ inputs, five replacements and
four additions. The focused R29/R33/capture, terrain, outer-vegetation, grass
and building-optics repair regression passes `163/163`; the R29, R33 and R33
capture static self-checks also pass. No R30, R31, R32, or R33 native
transaction has committed for
this queue. One R30 attempt (`r30-lookdev-20260906a`) entered the guarded build
and then rolled back cleanly when its continuous watchdog measured only
`4,349,239,296` free commit bytes; its `rollback.json` records no rollback
errors, and the exact R29 native map and DLL pins remain unchanged.

An earlier 8 September native-gate audit sampled only `5.555`, `5.541` and
`5.579 GiB` free commit. That audit's final sample was `4,746,776,576` bytes
below the fixed 10 GiB prelaunch gate and `451,809,280` bytes below the fixed
6 GiB continuous floor, despite `10.782 GiB` free physical memory. Commit headroom,
not disk capacity or physical memory, is the blocker. The only relevant Unreal
process is the protected UE 5.4 Capstone editor, PID `22908`: exact UE_5.4
`UnrealEditor` opening
`C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject`, created
`2026-09-07T18:04:24.2152750Z`, with `8.629494 GiB` private memory at audit.
There were zero UE 5.5 commandlet, UBT, UHT, ShaderCompileWorker or TRIAD
mutator processes, and port `30010` had no listener. The exact disposable-helper
audit offered only about `191.45 MiB` optimistic recovery, which cannot reach
either gate. No native attempt was made during this source-only work. Do not
weaken a guard or stop Capstone implicitly; R30 must wait until the Capstone
operator closes that session when safe or the machine gains materially more
commit capacity.

### Final R33 Cesium readiness result — 8 September 2026

The authoritative final result is recorded in the
[Istana R33 Cesium readiness audit — 2026-09-08](ISTANA_R33_CESIUM_READINESS_AUDIT_2026-09-08.md).
R33 is repository source-ready only. Its runtime resolver/configure/apply/
validate/readiness paths, context resolver/register/apply/validate paths,
editor resolver/predecessor/apply/successor-validation paths and Player0
capture-state resolver now all require valid, non-null opaque
`UCesiumIonServer` objects before shared pointer identity is accepted. Focused
negative tests prove that an equality-only null/null mutation fails. The final
focused R33 source/native/capture run passes `49/49`; the adjacent
R29/R32/context/R33 regression passes `93/93`; both of the two repository-only
R33 static self-checks (transaction and capture) pass.

Installed Cesium for Unreal `2.18.0` (`Version 78`, UE `5.5.0`) supplies the
declared R33 API surface, including load-failure, tileset source/asset/server,
georeference, load-progress and height-sampling symbols. This establishes API
availability only. It does not prove that the promoted TRIAD closure compiles,
that either provider asset streams, or that any provider/runtime acceptance
gate succeeds.

The final native re-read remains exact R29: map `37,465,844` bytes / SHA-256
`A05C95CEF30DFF1675B049970D993F1DBA75CB88BE38BFC73E7C8077BC5C1F92`,
runtime DLL `4,981,248` bytes /
`30EF244443737D3BA909F96C2B5846951621378D32F81AAB13CF765635D42276`,
and editor DLL `8,252,928` bytes /
`E0CC5AD603DCB3E243EAFEC09D4E66666CD6FE00922E4D57FB6DA87B7A8E61DE`.
At `2026-09-07T22:34:48Z`, only `4,953,522,176` free commit bytes
(`4.613 GiB`) remained, below the fixed 10 GiB launch and 6 GiB continuous
gates, with the protected UE 5.4 Capstone editor, PID `22908`, still active.
R30 has zero commits and one rollback; the R30 evidence root and every
R31/R32/R33 transaction/evidence root are absent. R33 therefore cannot bypass
R30 commit plus human capture, R31 commit plus human capture, and R32 commit
plus human capture.

No provider token value or fingerprint was inspected or recorded. A non-null
shared server identity is not evidence of a token, entitlement to Google asset
`2275207` or CWT asset `1`, provider terms, provider readiness or live
streaming. No CWT height was sampled or persisted; no vertical-datum
conversion, independent checkpoint residual, present-day terrain-accuracy
result or simulation-terrain authority exists. R33 remains non-live and
visual-reference-only.

Terrain-accuracy readiness has also advanced without overstating the R33 visual
reference. The immutable public SLA Vertical Control shortlist contains three
nearby index candidates, all quarantined because their GeoJSON Z=`0` is not a
height. The v1 hash-bound, source-only admission contract remains intentionally
incapable of admitting sampling. Its separate v2 successor can verify bounded
external provider-terms, datum-operation, reference-source, R33 transaction,
and accepted visual-review artifacts under a caller-supplied evidence root; it
rejects unsafe paths, symlink/reparse escapes, unstable opened-file identity,
hash drift, stale acceptance, non-committed receipts, and broken R33
cross-bindings. A separate v3 semantic-evidence gate now prevents arbitrary
hash-pinned opaque bytes from being treated as sufficient evidence. It requires
reviewed JSON attestations for provider rights, datum operations and formulas
with test vectors, exact point values and surface/measurement semantics, source
locators and hashes, and independent reviewers; it also recomputes the admitted
datum formula and binds the complete v2 contract. Its focused tests pass
`16/16`, bringing v1/v2/acquisition/v3 TerrainAccuracy coverage to `74/74`.
Both checked-in successors remain denied (`PRE_SAMPLING_ADMISSION_DENIED` for
v2 and a structurally valid denied v3 template). No CWT output was sampled or
persisted and no terrain authority changed.

Current visual evidence therefore remains split:

- the ground-level R27 image shows strong mature-canopy massing and grounded
  shadows, but grass is still smooth at ordinary viewing distance;
- the current R23 provider-ready skyline frames show dense real-world
  surrounding buildings at middle and far distance, with no visually exposed
  connected black/grid band. The primary remaining mismatch is instead the
  authored estate layer: pale, sparse, repeated tree crowns, a smooth lawn, and
  saturated/blotchy aerial ground texture. The three current frame receipts are
  `E55D22D57EE4128465CAAAD05E71A608B0551B303C1052CD88BE2FE9A19E4C0E`,
  `86EF78465CE96EAA92203FFACC1F39D7FC40969A25825697BDC199F4ECA60611`,
  and `CBBBD3063F7B67532DBFDA7AF518D2C856CEDD8424A25780CB188ED5A3D46015`;
- the R31 CPU lookdev shows the intended façade readability improvement, but it
  is explicitly an offline source approximation, not an Unreal/native capture.

The current result is suitable for reviewing composition and geographical
context. It is not yet suitable for a hyperreal, current-complete, surveyed, or
performance-accepted claim.
