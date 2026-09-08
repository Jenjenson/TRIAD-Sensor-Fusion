# Istana visual-fidelity reference

Status: implementation reference, not a survey, botanical inventory, or placement authority<br>
Geography: Istana grounds and immediately visible central-Singapore context<br>
Evidence checked: 2026-09-07 (Asia/Singapore)

## Purpose

This reference turns the request for a more realistic Istana environment into
bounded visual targets. It may guide mesh morphology, material response, LOD,
lighting, and evidence-camera choices. It must not move an existing instance,
invent a species at an unverified coordinate, change terrain height truth, or
replace the project's approved geospatial and simulation-authority records.

## Evidence-backed landscape cues

- The Istana describes its grounds as a managed botanical landscape comprising
  the Fairway, lawn, Swan Pond, Gun Terrace, Japanese Garden, Inclusive Garden,
  and tree-lined paths. Its current official gallery shows expansive closely
  mown lawn, scattered large mature trees, dense perimeter vegetation, a pond
  with lily pads, and a formal fountain axis toward the Main Building.
  Source: [The Grounds — The Istana](https://www.istana.gov.sg/visit-and-explore/the-grounds/).
- NParks' Heritage Tree Register identifies a Rain Tree (*Samanea saman*) on
  the Istana grounds. NParks' Civic District tree-trail material also identifies
  Rain Trees at the Istana entrance. This supports a broad, high-spreading,
  umbrella-like mature-canopy target for at least the already verified Rain Tree
  anchors; it does not identify every broadleaf tree on the estate.
  Sources: [NParks Heritage Tree Register](https://heritagetrees.nparks.gov.sg/explore/heritage-tree-register/)
  and [Avenue of Heritage Trees](https://www.nparks.gov.sg/-/media/nparks-real-content/news/2016/avenue-of-heritage-trees/4-annex-bnparks-unveils-avenue-of-heritage-trees.pdf).
- The Istana states that its nursery cultivates flowering plants and ornamental
  palms, supporting restrained palm and ornamental-understorey diversity rather
  than a uniform broadleaf-only population.
  Source: [Meet our people — The Istana](https://www.istana.gov.sg/presidents-office/meet-our-people/).
- The official grounds guide identifies a Merkus pine in the Japanese Garden,
  while the Istana's garden announcement records a planted Sempilor
  (*Dacrydium elatum*), described as a soft-form tropical conifer. A future
  Japanese-Garden-specific mesh family therefore needs needle/conifer
  morphology; the five existing generic broad forms are insufficient for that
  feature. These sources do not authorize a guessed coordinate.
  Sources: [Istana grounds guide](https://www.istana.gov.sg/-/media/Files/IOH-e-brochure-10-Jan.ashx)
  and [Japanese Garden announcement](https://www.istana.gov.sg/newsroom/news-release-detail-page/).

## Visual implementation requirements

### Trees

- Preserve all admitted world translations and instance order.
- Make the dominant verified Rain Tree family read as a mature spreading crown:
  high lateral branching, irregular umbrella silhouette, layered foliage depth,
  visible trunk taper, and non-circular crown edges.
- Keep palm crowns, upright/columnar trees, dense dome trees, and high-fork
  rounded trees visually distinct. Do not distribute forms with a visible
  modulo pattern.
- Use deterministic per-instance yaw, bounded anisotropic scale, leaf hue,
  roughness, wind phase, branch-break, and age variation where the material and
  mesh provenance allow it.
- Add root flare, litter, and trunk-to-ground transitions to every hero/near
  tree that enters the evidence cameras; avoid large opaque mulch discs.
- Use automatic near/mid/far LOD selection with dithered or otherwise visually
  stable transitions. Never force all 729 trees to the same LOD.
- Add a conifer/needle family only after a verified Japanese Garden anchor is
  available. Until then, record it as missing rather than placing it by eye.

### Turf and planted ground

- Preserve the formal lawn/fairway extents and every admitted source
  translation.
- Break up carpet-like repetition with deterministic blue-noise/hash-based
  selection, yaw, scale, lean, height, color, and dry-tip variation.
- Maintain readable blades and micro-normal response at 12 m, 20 m, and 50 m;
  fade into a matching macro albedo/roughness field instead of abruptly
  disappearing.
- Keep formal lawns closely mown. Reserve taller, denser, or flowering planting
  for already verified beds and understorey regions.
- Player0 and RGB SceneCapture evidence must state whether the same turf layer
  is visible. A showcase cannot claim camera realism from a layer hidden from
  the simulated RGB camera.

### Buildings and surroundings

- Preserve every admitted OSM footprint, height, group identifier, and transform.
- Treat the local shell as mid/far context: vary facade family, window cadence,
  occupancy, glazing response, roof tone, and restrained weathering by a stable
  per-building hash, not array position or a short repeating cycle.
- The Istana facade needs separate readable wall, frame/reveal, roof, and glazing
  responses. Opaque-safe glazing should still show view-angle Fresnel response,
  subtle roughness/occupancy variation, and a shallow interior cue.
- Cesium Google Photorealistic 3D Tiles may remain the primary real-world
  surrounding-building presentation. The local shell must not be described as
  close-range photoreal architecture.

### Terrain and geospatial truth

- Keep the sole Cesium georeference and the approved horizontal placement.
- Cesium World Terrain asset 1 is geometry-only in R33; without an approved
  imagery raster overlay it is a diagnostic terrain reference, not a
  photoreal surface. Cesium's own Unreal documentation treats imagery as a
  separate raster-overlay component draped over a tileset and its dataset
  tutorial adds World Terrain together with an aerial-imagery layer.
  Sources: [Cesium Unreal FAQ](https://cesium.com/learn/unreal/unreal-faq/)
  and [Adding Datasets](https://cesium.com/learn/unreal/unreal-datasets/).
- Do not hide a vertical-datum mismatch with an arbitrary Z offset. Native
  acceptance must use datum-labelled checkpoints and inspect tree/building/ground
  contact for floating or burial.

## Required visual evidence before acceptance

1. Existing four fixed overview/hero views for every affected stage.
2. A 2 m and 8 m Istana facade/glazing view.
3. Turf grazing views at 12 m, 20 m, and 50 m.
4. A same-camera high-occupancy surrounding-building oblique pair with Nanite
   on and raster fallback forced. It must expose facade repetition and be
   rejected for render-path-only cadence, texture, normal, seam, scale,
   silhouette, or geometry changes.
5. Near, medium, and skyline tree views that expose crown diversity and LOD
   transitions.
6. A terrain-contact view at representative trees and buildings for each
   presented Cesium mode.

All evidence remains pending until rendered from the native map and explicitly
accepted by a person. Source contracts, offline previews, and static tests do
not constitute visual acceptance.
