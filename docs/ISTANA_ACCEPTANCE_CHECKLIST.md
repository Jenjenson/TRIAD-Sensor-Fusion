# Istana map and placement-system acceptance checklist

> **Prototype-only checklist.** Passing this checklist validates the existing
> simulation workflow, not a one-to-one or survey-grade digital twin. The visual
> and source baseline was rejected. A digital-twin release must instead pass
> `ISTANA_DIGITAL_TWIN_ACQUISITION_SPEC.md` and its signed release manifest.

This checklist separates software correctness from visual plausibility and
real-world calibration. Passing it does not certify a physical deployment.

## Verified local run (20 August 2026)

- Original `/Game/SDTH`: SHA-256
  `8FAA1E3AD9DCC74AF8FC0054BE7631D1F957577A750B75F17B34A0822B0EB50B`,
  118,319 bytes, timestamp unchanged after the build.
- Separate `/Game/Maps/Istana_1km`: created and validated; the 64-point clip
  reported `0.0000 m` maximum computed geodesic-radius error.
- Final survey: 53/53 candidate surfaces, 165/165 target surfaces, and 69,960
  modality/scenario rows resolved against loaded simulation collision.
- Final recommendation: `FEASIBLE`, four sites, 95.2380952% worst-scenario
  compliant coverage, 100% critical-sample coverage, and four declared failure
  domains. Clear, Monsoon, Haze, and RF-silent cases all passed the same 95%
  policy.
- PIE replay: `triad.live_rf_snapshot.v3` contained the four selected node IDs
  and the exact non-legal `wgs84_geodesic_circle` perimeter.
- Geometry scope: `LOADED_SIMULATION_COLLISION_ONLY`; `surveyGrade=false`.

## Map isolation and preservation

- `/Game/SDTH` has the same SHA-256 and timestamp as before the build.
- `/Game/Maps/Istana_1km` exists as a separate saved level.
- The level contains exactly one active Cesium georeference centred at
  longitude `103.84288055`, latitude `1.30709615`.
- The study polygon has 64 linear spline points whose geodesic radius is
  `1000 m` within numerical tolerance.
- The Cesium polygon overlay has `InvertSelection=true` and
  `ExcludeSelectedTiles=true`, so tiles wholly outside the circle are culled.
- The scenario perimeter uses the same circular centre/radius and does not
  silently substitute the bounding box.
- The map, builder, logs, and repository contain no embedded access token.

## Main Building exterior

- The building actor's hard and soft references both resolve to the new,
  non-overwriting asset
  `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined`; the legacy mesh is not
  modified or accepted as the final renderer.
- The reviewed LOD0 evidence records `175,974` source vertices, `58,658`
  triangles, and `12500 x 11769.4444 x 3685 cm` bounds including the mast. The
  import wrapper verifies the actual OBJ bytes against SHA-256
  `4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91`.
- All eight existing project PBR materials are assigned and validated. The
  direct OBJ path proves auto-generated simple collision only; it does not
  claim that the separate eight-hull collision OBJ was imported as UCX.
- The overall massing is approximately 124 m east-west by 117 m north-south
  and retains a cross-shaped plan.
- The south/front elevation reads as a long symmetrical white facade rather
  than one solid box.
- Verandah and colonnade bays are physically open; line traces can pass through
  documented open gaps and are blocked by walls, floors, columns, stairs and
  roofs.
- Two-storey wings and a taller three-storey central tower are distinguishable.
- Repeated louvred/shuttered openings, panelled doors, classical column tiers,
  balustrades, mansard roofs and dormers are present.
- Broad front steps, paved forecourt, circular fountain, lawn and planted edges
  provide readable scale and sensor-occlusion context.
- Collision is enabled on structural pieces; decorative pieces that should not
  affect sensing are explicitly non-colliding.
- Front, oblique and side screenshots have been compared with at least three
  independent public exterior references.
- Visual acceptance reports `AUTHORED_REFINED_PRIMARY`: the refined renderer
  remains visible and collision-enabled while every current-view Cesium
  tileset supplies finite exact-100% surrounding context with physics. The
  provider building is not accepted because it has been observed flattened or
  absent; fog and human-only overlay suppression remain enforced.
- `bPreferStreamedIstanaVisualWhenReady` defaults to `false`. Its reversible
  streamed hide/restore mode is explicit opt-in only and cannot satisfy visual
  acceptance.
- For an existing v2 map, the live order is refined import, exact-v2 exterior
  repair, full validation, then PIE:
  `scripts/Import-IstanaExteriorRefinedLod0.ps1` →
  `scripts/Repair-IstanaRuntimeMapV2ExteriorAsset.ps1` →
  `scripts/Build-IstanaRuntimeMapV2.ps1 -ValidateOnly -StructuralOnly` →
  `scripts/Set-IstanaPlayInEditor.ps1`. The repair preserves the `182.3`-degree
  heading and collision settings, creates a non-overwriting v2 backup, and
  neither edits SDTH/v1 nor overwrites either mesh asset.

## Recommendation pipeline

- The request validates against `triad.placement_request.v1`.
- Unreal exports `triad.placement_survey.v1` from the current map's collision
  and line traces; no Python-only geometry is presented as authoritative LOS.
- Candidate sites are inside the 1 km AOI, mount-feasible, and grouped into
  independent failure domains.
- Range, horizontal FOV, vertical field of regard, orientation, LOS and declared
  weather/scenario assumptions are explicit in coverage rows.
- Visual coverage requires LOS and a compatible active-radar cue where the
  fusion policy requires it.
- Co-family visual modalities do not count as independent corroboration.
- The solver is deterministic for identical request/survey digests and either
  returns a feasible recommendation or explicit infeasibility reasons.
- The solver writes a new review-required `SensorNodes` patch and refuses to
  overwrite the live scenario config.
- Selected sites are replayed in Unreal and the exported survey is regenerated
  before the result is accepted. The current run completed the first replay;
  re-survey again after any geometry or sensor-model change.

## Claims boundary

- Scores are labelled simulation coverage/evidence indexes, not probabilities.
- SRTM/OSM/Cesium/public-photo reconstruction is labelled approximate.
- Structural suitability, access, power/network, property rights, privacy,
  safety, licensing, and regulatory approval remain human decisions.
- Any field-performance claim requires calibrated sensor models and measured or
  survey-grade geometry that are outside this repository.
