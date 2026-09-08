# Istana Explore V5D R24 MacDonald House integration

R24 adds one dedicated MacDonald House public-exterior renderer to the native
UE 5.5 V5D map. It is a bounded visual improvement, not a new simulation truth
surface. The deterministic source package is the import input; the guarded
editor transaction has now imported it into cooked Unreal assets and integrated
the actor into the native map. The runtime actor loads only that cooked mesh.

## Placement receipt and limitations

The 2026-09-03 placement receipt binds the actor to volunteered OSM way
`46521250`, feature geometry SHA-256
`755536CDE8E666826FA6A325BCD707BDC7E0132C018BC87F0ABE8275F4557A39`,
and part geometry SHA-256
`B7204683A26E7DA56E1A97E516E36220C1764341CA485B904D736F509B481A34`.
The volunteered local footprint corners are:

| X metres | Y metres |
| ---: | ---: |
| 337.732772 | 878.532304 |
| 347.248015 | 849.815968 |
| 389.037878 | 863.482807 |
| 379.511510 | 892.199144 |

The actor transform is translation
`(36320.390052826, 87155.365772797, 0.0)` cm, rotation
`(pitch=0, yaw=-161.885822122792, roll=0)` degrees, and scale
`(1.4799104705961, 1.98130612769146, 1.3)`. This centres the scaled synthetic
building mass on the volunteered quadrilateral and aligns the modeled public
facade with its southern long edge. The 30 x 15.42 x 31 m synthetic mass is
non-uniformly fit toward a 43.96 x 30.25 m volunteered footprint and 40 m
volunteered top. It is explicitly not survey-grade, as-built, dimensionally
1:1, or proof of exact facade alignment.

The detailed mass over-envelopes the retained coarse shell by small disclosed
margins when that coarse fallback is visible. This reduces exact coplanarity
between the two local representations; it does not prove separation from live
provider geometry or upgrade either source into measured geometry.

## Provider composition

MacDonald House is outside the existing 64-point, 185 x 245 m authored-core
Cesium exclusion. R24 does not enlarge that clip and does not add another
provider exclusion polygon. The frozen context-policy source remains
byte-identical to its pre-R24 contract. Global Cesium load progress is not
landmark-specific proof that provider pixels cover MacDonald House, so it is
recorded as telemetry only and never gates this overlay. The dedicated actor
ticks after the policy through an explicit prerequisite when an exact binding
exists, observes readiness edges every render frame, and runs its complete
local mesh/material/provenance audit at a bounded 2 Hz or immediately on an
edge. The overlay remains visible through provider `false -> true -> false`;
the coarse surroundings may continue to hide/restore under their unchanged
fallback policy. A missing or invalid telemetry binding does not hide the
landmark. Only a failed local mesh/provenance/render contract hides its own
component fail-closed.

This preserves provider context, thresholds, clipping, the coarse-fallback
transaction, public realm, grass, and trees. It does not claim that live
provider overlap or z-fighting has been excluded. That composition question is
explicitly unresolved until landmark-specific provider readiness/exclusion
evidence exists; the overlay is not a provider-ready live successor. Telemetry
therefore includes the distinct R24 actor/visibility state,
`providerStateTelemetryOnly=true`,
`visibilityInvariantAcrossProviderTransitions=true`, OSM receipts, and
`providerReadyLiveSuccessor=false`.

## Materials, performance, and authority

The flat OBJ MTL is never imported as live material authority. The editor
factory creates nine deterministic, texture-free Unreal PBR materials: sand-
faced brick coursing/grain/mortar microvariation, painted frame variation,
opaque roughness-controlled recessed glazing, green glazed roof tiles,
procedural marble, concrete, dark metal, bronze plaque, and shadow recess.
They use world-position/normal procedural inputs with no texture acquisition,
displacement, world-position offset, or tessellation. Opaque glass is a bounded
Nanite-compatible visual choice; the modeled 0.21 m recess supplies physical
depth separation. These are uncalibrated visual priors, not sampled or measured
materials.

The one 7,080-triangle, nine-section mesh uses full Nanite data and a full-
triangle raster fallback. Its small raster LOD0 stays CPU-readable so runtime
and editor validation can recompute a provenance-stamped SHA-256 over position
bits, indices, section fields, slot names, and assigned material object paths.
This catches accidental count/bounds-preserving render-payload drift; it is not
a signature, hostile-tamper defence, material-calibration proof, or geometry
authority. The mesh owns no collision primitives, navigation data,
overlaps, sensor-occlusion authority, RF geometry, or RF material authority.
No interior, non-public face, security system, protected layout, or rooftop
equipment detail is asserted.

## Editor transaction

`ImportApplyAndValidateIstanaExploreV5DMacDonaldHouseR24` is the idempotent
command-line entry point. It:

1. hash-validates all eight source/placement inputs;
2. admits an exact empty registry-and-disk asset-root predecessor, creates
   exactly one mesh and nine material assets, saves, reloads, and cold-validates
   them as one active ownership transaction; every post-create failure deletes
   only that exact newly owned root diff, verifies the root empty, and reports
   retryability;
3. builds the V5D map if absent, otherwise loads the existing destination;
4. accepts only a clean, semantically exact no-landmark predecessor;
5. copies it without overwrite to
   `Saved/TRIAD/MapBackups/V5DMacDonaldHouseR24_20260903`, with the admitted
   predecessor SHA in the filename, then verifies byte/hash equality;
6. adds one exact actor, validates before save, saves only the destination map,
   reloads through the V5B source, and cold-validates the result;
7. restores the verified predecessor on a failed save or cold check by copying
   the preserved backup to a sibling temporary, verifying that temporary,
   move-replacing the destination, then verifying both final hash and preserved
   backup. It never directly overwrites the map from the backup.

The mesh import identity stored in provenance is the canonical path below
`ProjectDir` (`SourceAssets/.../SM_IPV5D_R24_MacDonaldHouse_Render.obj`), not an
absolute checkout or junction target, so moving the reviewed project does not
invalidate identity by itself.

The native automation filter is
`TRIAD.Istana.ExploreV5D.MacDonaldHouse`. It covers default/runtime truth,
cooked mesh/material provenance, and the idempotent end-to-end editor path.

The required cold non-interactive sequence has now completed under the guarded
UE 5.5 transaction. To reproduce it after a reviewed source/native sync and
build, run only
`TRIAD.Istana.ExploreV5D.MacDonaldHouse.EndToEndContract` in a fresh
rendering-capable `UnrealEditor-Cmd` process (do not use `-NullRHI` for this
first import/Nanite build). Wait for that process to exit. Then run the full
`TRIAD.Istana.ExploreV5D.MacDonaldHouse` filter from a second cold process;
the end-to-end case is then idempotent and the map bytes remain unchanged.
Never run both processes concurrently.

## Guarded UE 5.5 integration evidence

The 2026-09-03 guarded transaction verified exact source/native hash parity and
recorded a clean Build receipt at
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260903_032947_525_25528\receipt.json`
(SHA-256
`9877E68A1C5E4F23C9E33689D0A1F49383F6A436DF17ADB143E920F33E98649E`).
The exact runtime DLL is 4,275,200 bytes with SHA-256
`47C66266773E6208313FA1CB51C20D1E335C6A5A841C4E137B415C4879148614`;
the exact editor DLL is 7,135,232 bytes with SHA-256
`0B7C337BF7949C650DD22B233AB5F4D4F925E2A3C91230429E66A757A31D6F77`.

The cold idempotence rerun is recorded by the Migrate+Test receipt at
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260903_032533_044_24980\receipt.json`
(SHA-256
`36436D05A5902B14064C9F87FE336B4D6200251DDA8F5AB28BE851A849C86ADE`).
The rendering-capable migration filter
`TRIAD.Istana.ExploreV5D.MacDonaldHouse.EndToEndContract` completed 1/1,
and the subsequent cold aggregate
`TRIAD.Istana.ExploreV5D.MacDonaldHouse` completed 3/3. The integrated map was
unchanged across the rerun at SHA-256
`B79252C4919D6985CD4824B296B92D78F2B2BACD594BC1816CA130BAA8D5B460`,
and the exact one-mesh/nine-material asset inventory remained unchanged. The
same Test phase also completed the geodesic-circle, indexed-geometry-query, and
RF-interaction-model native filters at 1/1 each.

The guarded operation used exact file and asset allowlists, preserved the
protected unrelated UE 5.4 editor/project content, left content outside the
admitted transaction unchanged, and performed no recursive deletion. This is
native build, import, map-integration, and cold editor automation evidence. It
is not a deployed/live scenario or cold packaged-build receipt, and it does not
upgrade collision, navigation, sensing, RF, dimensions, placement, or materials
to survey truth. Landmark-specific raster visual-QA evidence is recorded below;
live-provider composition remains unresolved.

### MacDonald House raster visual-QA evidence

The strict provider-ready capture attempt `r24_macdonald_final_a` failed closed
after its fixed timeout. Its final exact-pose state reported
`cesiumLoadProgress=36.896`, `providerReadyForProof=false`,
`localFallbackHidden=false`, and zero of three captures. It exited cleanly and
produced no PNG or evidence manifest. This failed attempt is not proof and was
not converted into a partial-success receipt.

The follow-up telemetry-only run `r24_macdonald_final_b` completed all three
exact target-locked poses and exited cleanly. Its 127,519-byte manifest is
`D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_r24_macdonald_evidence_r24_macdonald_final_b.json`
with SHA-256
`49E0398664C5ADF43176F5BDB8A51F29B5E739DFD7F911FB7D8CEE07911A0723`.
The manifest classifies the result as
`RASTER_VISUAL_QA_EVIDENCE_NOT_SURVEY_NOT_PROVIDER_GEOMETRY_PROOF`, records
`ProviderEvidenceMode=TelemetryOnly`, three captures, editor exit code 0, no
forced containment, and no remaining non-protected editor or Remote Control
listener.

The exact diagnostic PNG receipts under
`D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D` are:

- `explore_v5d_diagnostic_r24_macdonald_facade_close_r24_macdonald_final_b.png`
  (5,998,728 bytes; SHA-256
  `4B1E2D43D1D702ED78A86E143E53CF48A818EFA5D203A33D800CC7E52372E62A`);
- `explore_v5d_diagnostic_r24_macdonald_front_corner_oblique_r24_macdonald_final_b.png`
  (5,066,390 bytes; SHA-256
  `0E3220D6F69AB468F0ADA71F6CC9BA07D6C0E83B2BB7701B291544689D97D520`);
- `explore_v5d_diagnostic_r24_macdonald_streetscape_context_r24_macdonald_final_b.png`
  (3,601,851 bytes; SHA-256
  `F729C53D944933406A704AF3A99F8B26F114B8B7F3D27143819BA705D2DB85B7`).

All six telemetry samples at every captured pose retained
`providerReadyForProof=false` and `localFallbackHidden=false`. Visual review
confirms that the MacDonald House mesh, brick treatment, windows, balconies,
plaque, and entrance render at the intended location, while the oblique and
context views are dominated by coarse surrounding blocks and flat local
ground. These images therefore validate landmark raster integration and camera
composition only; they do not establish hyperreal surroundings, provider pixel
coverage, provider geometry, or absence of coarse-shell overlap.

The underlying Remote Control object is
`/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary`
and its single entry point is
`ImportApplyAndValidateIstanaExploreV5DMacDonaldHouseR24`. It internally runs
asset import/validation, existing-map load or absent-map build, the guarded
map migration, and final asset/map validation. The individual functions remain
available for diagnosis, but are not a substitute for the one-command cold
transaction.
