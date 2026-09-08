# Istana public-view photoreal reconstruction

## Conditional release claim

This track targets a photoreal **public exterior** of the Istana Main Building
and a fully three-dimensional one-kilometre visual context. It may be described
only as:

> Public-reference visual reconstruction, April 2024 exterior, selected-view
> validated; not survey controlled.

The current structural preview has not passed the acceptance gates below and
must not yet use the quoted release description. It is not an as-built model,
a digital twin, or evidence of exact hidden geometry. A render may pass a blind
photographic comparison at a declared
camera without making the unrestricted scene one-to-one.

The implementation is additive. It must not overwrite `SDTH`, either existing
Istana map, the current refined source asset, or the locked digital-twin ingest
path. Its destination is `/Game/Maps/Istana_PublicView_Exterior_v1`, with assets
under `/Game/TRIAD/IstanaPublicView`.

## Safe scope

The scene includes only public-facing exterior form and landscaping. It must
not model or infer interiors, concealed systems, security equipment, guard
positions, access-control details, restricted circulation, service routes, or
operational procedures. Doors and windows may use shallow, non-traversable
depth proxies so that a public exterior view does not look flat.

The April 2024 pre-restoration appearance is the reference epoch. Mixing
photographs from other years is allowed only for architectural interpretation;
time-dependent colour, planting, furniture, maintenance state, and adjacent
development must be resolved in favour of the declared epoch or marked
unresolved.

## Evidence that may be used

Every input receives a file-level provenance record before it influences
pixels or geometry.

Allowed inputs are:

- original project-authored geometry, textures, shaders, and procedural data;
- file-level CC0, CC BY, or CC BY-SA exterior photographs, subject to their
  attribution and share-alike obligations;
- OpenStreetMap mapping data with ODbL attribution, for approximate footprints
  and public road/path context;
- Singapore open datasets explicitly distributed under the Singapore Open Data
  Licence, within the scope of that licence;
- commercially licensed generic terrain, vegetation, or material assets whose
  licence permits the intended private Unreal simulation; and
- Google Photorealistic 3D Tiles used live through the supported Cesium
  integration, with required attribution, beyond the locally authored patch.

Official-site imagery and other reference-only photographs may guide a human
review, but their pixels must not be copied, projected, trained on, or bundled.
Google/Cesium tiles must not be extracted into a new mesh or texture set.

CC BY-SA-derived assets must be isolated and accompanied by the exact author,
file URL, licence URL, source hash, transformation description, and applicable
share-alike notice. The repository has no general licence, so a contributor
must not assume that adding a source image is automatically permitted.

High-resolution generic PBR and vegetation replacements may come from a CC0
library such as Poly Haven after a file/version review. The initial candidates
are `white_plaster_02`, `roof_slates_03`, `tree_small_02`, `island_tree_01`,
`pachira_aquatica_01`, and `fern_02`. They are generic visual assets, not
evidence of the Istana's exact material, species, placement, or condition.
Asset licensing and any API/download-service terms must be recorded separately;
the source generator does not download them automatically.

## Reconstruction architecture

The map is split into independently reviewable visual and collision layers:

| Layer | Purpose |
| --- | --- |
| Hero building visual | Segmented Nanite-ready exterior with real facade depth and close-range architectural detail |
| Hero building collision | Simple, stable exterior collision independent of visual triangles |
| Estate terrain and hardscape | Non-flat ground, lawn, fountain, stairs, paths, curbs, planting beds, and surface transitions |
| Estate vegetation | Full-volume trunks, branches, crowns, hedges, and shrubs around the public approach |
| Outer 1 km structures | ODbL-derived, mapping-grade three-dimensional building massing from 300 m to 1 km; building-only, with no roads/routes and no collision or sensor truth |
| Outer 1 km vegetation | Lower-detail but still volumetric vegetation; no full-tree billboard LODs |
| Beyond-1 km Cesium | Live provider context only, visually clipped out of the authored inner patch |
| Calibrated lighting | One deterministic sun/sky/exposure/white-balance state for QA |
| QA | Cameras, landmark overlays, masks, and reports excluded from normal simulation |

The initial deterministic source package uses these explicit assets:

- `SM_IstanaPublicView_Building_Hero` and the separate
  `SM_IstanaPublicView_Building_Collision`;
- `SM_IstanaPublicView_Terrain` and `SM_IstanaPublicView_Hardscape`;
- the retained but hidden/inactive procedural fallback
  `SM_IstanaPublicView_ContextBuildings`;
- the visible `SM_IstanaPublicView_OSMContextBuildings`, derived from the exact
  reviewed building-only OpenStreetMap snapshot and attributed to
  `© OpenStreetMap contributors` under ODbL 1.0; and
- the volumetric `SM_IstanaPublicView_Tree_Rain`,
  `SM_IstanaPublicView_Tree_Palm`, and
  `SM_IstanaPublicView_Tree_Framing` archetypes.

Their placement contract is `IstanaPublicView.instances.json`. The map builder
must reject a missing or invalid role rather than create a placeholder success.
Generated objects are imported beneath the corresponding `Building`, `Ground`,
`Context`, or `Vegetation` folder of `/Game/TRIAD/IstanaPublicView`.

The OSM mesh is coordinate-contract aligned to the hero frame but remains
mapping-grade and is not visually, photographically, cadastral, or survey
accepted. The visible OSM component has `NoCollision`, ignores all trace
channels, and generates no overlaps. It must never be treated as navigation,
occlusion, target, or sensor truth. The synthetic 180-box fallback remains
imported and hash-validated but is hidden and inactive to prevent duplicates.

The locally authored terrain and context cover the entire one-kilometre circle.
They may be approximate when no authoritative data exists, but they must remain
three-dimensional. An aerial photograph, flat collision disk, image plane,
camera-facing tree, or crossed-plane tree crown cannot be the accepted visual
surface inside the circle.

## Building workflow

1. Select at least six lawfully reusable modelling photographs covering the
   centred front, both front obliques, arcade, tower, and forecourt. Reserve at
   least three independent photographs as holdouts.
2. Solve a camera for each reference using vanishing lines and at least 24
   stable, named exterior landmarks. Store the crop, sensor/focal assumptions,
   transform, and residuals.
3. Register the broad horizontal envelope to the mapping reference and the
   published approximately 28 m tower target. These are constraints, not an
   assertion of survey accuracy.
4. Rebuild the exterior as a segmented hero asset. Arcades, reveals, columns,
   capitals, louvres, balusters, dentils, pediments, stairs, fountain, roof
   edges, glazing offsets, doors, soffit beams, and pendants must create real
   parallax at their accepted review distance.
5. Use dedicated collision proxies. Do not use provider photogrammetry or the
   visual Nanite triangles as authoritative simulation collision.
6. Validate both modelling and held-out cameras. A front-only texture
   projection or a mesh that fails the oblique holdouts is rejected.

Sparse unrelated internet photographs are not automatically suitable for
photogrammetry. They commonly lack consistent exposure, scale, lens metadata,
and the greater-than-60-percent neighbouring overlap expected by conventional
alignment. A failed alignment must not be patched with invented geometry and
reported as a scan.

## Terrain and vegetation workflow

- Generate a non-flat terrain base from an allowed elevation source, then
  manually refine the public forecourt to match reusable references.
- Model roads, paths, curbs, stair profiles, fountain and building-grade seams
  as geometry rather than markings painted on an aerial image.
- Use deterministic HISM/PCG placement with full-volume tree archetypes.
- Hero trees require trunks, major branches, a walk-around crown volume, leaf
  transmission, and non-zero collision where simulation requires it.
- The final LOD inside one kilometre must remain volumetric. Thin leaf geometry
  is permitted; a whole tree may not collapse to a camera-facing card.
- Tree species, exact trunk coordinates, crown dimensions, and terrain relief
  remain approximate unless a reusable source establishes them.

## Rendering profiles

The simulation and cinematic profiles share the same world, camera transforms,
geometry, materials, and provenance. Only quality settings may differ.

The simulation profile targets 1920x1080 and a stable 30 fps on the current
TITAN V using software Lumen, Virtual Shadow Maps, TSR, explicit collision,
fixed exposure, bounded or frozen wind, and deterministic weather. The
cinematic profile uses Movie Render Queue and accumulated samples for offline
comparison; it must not hide geometric problems with depth of field, motion
blur, bloom, vignette, local exposure, or fog.

Every QA camera records focal length, sensor/crop assumptions, transform, focus
distance, fixed EV100, white balance, display transform, and reference image
hash. Editor and PIE must use the same colour transform.

## Neural-rendering boundary

A NeRF or 3D Gaussian splat may be evaluated only as a non-colliding cinematic
comparison layer when its source photographs are licensed and sufficiently
dense. It is not accepted as TRIAD terrain, vegetation, building collision,
line-of-sight geometry, or a relightable PBR model. Sparse public photos cannot
constrain unseen sides, and a radiance field cannot turn that missing evidence
into exact structure.

## Delivery sequence

1. Build and validate the additive source assets without opening Unreal.
2. Import into the new namespace without overwriting an existing asset.
3. Build only the new map and prove the existing map hashes are unchanged.
4. Validate structural and rights contracts before Play-In-Editor.
5. Capture the modelling and held-out cameras with the locked lighting state.
6. Run the quantitative and blind-review gates in
   `ISTANA_PUBLIC_VIEW_ACCEPTANCE.md`.
7. Publish only the scoped public-reference claim. Any failed gate remains
   visible in the release report.
