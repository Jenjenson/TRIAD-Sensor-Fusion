# Istana Explore V5D R24 Temasek Shophouse integration

This integration adds one persistent, dedicated high-detail
Temasek Shophouse public-exterior renderer to the V5D hybrid map. It imports
the validated `R24TemasekShophouse` OBJ as one 25,600-triangle, 19-section
static mesh and replaces the flat MTL appearance with 19 deterministic,
texture-free Unreal PBR material graphs. The material graphs provide bounded
microvariation for plaster, charcoal trim, glass, timber, brass, mosaic,
precast concrete, terracotta, solar panels, reclaimed pavers, foliage, bark,
soil, rainwater, flowers, and deep recesses. They are visual priors, not
calibrated material measurements or RF material classifications.

## Placement receipt and limits

The actor transform is pinned to volunteered OSM way `1551538490`, feature
geometry SHA-256
`A273E09B21C71AA7436C5B156E643AF4176F3B90A215DD0FEA56928FA5BDDEF7`
and part SHA-256
`EBEF89D86FF0BA7FD6DBF188ED2389CC392E2E859ACEEADF6F0900E6E880E928`.
The applied transform is translation
`(40411.657951, 88424.311139, 0)` cm, yaw `-162.5152283523459` degrees,
and non-uniform plan scale `(1.053891, 1.221655, 1)`. This is an oriented
visual fit to a volunteered irregular footprint. It is not survey-grade,
as-built, one-to-one, or exact facade/landscape alignment. The volunteered
40 m height is deliberately ignored; the synthetic three-storey 13.2 m
building proportion is retained.

The modeled scope is the publicly visible exterior and a bounded synthetic
frontage. Interiors, security systems or layouts, occupant/event
configuration, protected artwork, exact species and planting schedules, and
adjoining-building reconstructions are excluded.

## Composition policy

The overlay remains visible across provider unavailable/ready/unavailable
transitions. Global provider readiness is telemetry only; it is not
landmark-specific coverage proof. No Cesium provider exclusion is added and
no provider-ready live-successor claim is made.

The existing coarse surroundings shell is retained. Its volunteered 40 m
Temasek geometry conflicts with this 13.2 m interpretation, so coarse-shell
overlap is explicitly unresolved. Provider overlap is also explicitly
unresolved. This integration does not silently suppress either context and
must not be presented as a final composition solution until a separately
validated, landmark-specific exclusion or shell-suppression policy exists.

## Authority and performance boundary

The mesh is render-only: actor/component collision is disabled, navigation is
disabled, and the asset has no collision primitives or nav data. It is never
sensor-occlusion, propagation, RF-geometry, RF-material, survey, as-built,
interior, or security authority. One full raster LOD is retained as the
Nanite fallback, CPU access is enabled only so the cooked payload can be
recomputed, and Nanite is enabled without geometric trimming.

Source admission checks exact project-relative paths, byte counts, and
SHA-256 receipts. Provenance also recomputes a cooked LOD0 digest over
positions, indices, sections, and material assignments. This catches
accidental asset drift; it is not a signature or supply-chain attestation.

## Transaction and tests

`ImportIstanaExploreV5DTemasekShophouseR24Assets` admits only an exactly empty
20-package predecessor, creates one mesh plus 19 materials, saves, reloads,
and cold-validates before commit. Failure rolls back only the exact owned
package roster and verifies registry and disk emptiness.

`ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap` requires the clean
exactly-one-MacDonald/no-Temasek V5D predecessor, makes a verified non-overwriting backup
under `Saved/TRIAD/MapBackups/V5DTemasekShophouseR24_20260903`, spawns exactly
one actor, saves, reloads, and cold-validates. Restore uses a verified sibling
temporary followed by replacement and preserves the backup; no OS-atomic
replacement claim is made.

The shared validator uses explicit `Forbidden`, `Optional`, and `Required`
landmark-presence policies. The clean predecessor forbids both landmarks, the
MacDonald successor and Temasek predecessor require MacDonald while forbidding
Temasek, and the final map/PIE gate requires both. The fresh hybrid builder
preflights and creates both overlays; its unchanged `Hybrid.BuildAndValidate`
gate therefore validates the same final state. MacDonald EndToEnd reruns admit
an already exact final two-landmark map without mutating it.

Focused native filters are:

- `TRIAD.Istana.ExploreV5D.TemasekShophouse.RuntimeContract`
- `TRIAD.Istana.ExploreV5D.TemasekShophouse.AssetContract`
- `TRIAD.Istana.ExploreV5D.TemasekShophouse.EndToEndContract`

The intended cold entry point is
`ImportApplyAndValidateIstanaExploreV5DTemasekShophouseR24`. Native sync,
build, map mutation, and visual capture remain separate guarded operations.

## Executed native state

The guarded full native transaction completed successfully on 2026-09-03.
Its receipt is
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260903_043946_604_20372\receipt.json`
(100,244 bytes, SHA-256
`449364AB4B904BA6C2F7C6A1BA5D351091B11977F5CEE462BD6A1E535BF995FD`).
That transaction synchronized the reviewed 56-file allowlist, built the two
TRIAD modules, applied MacDonald House and then Temasek Shophouse, and passed
the five allowlisted native test filters. The final hybrid map is 34,991,378
bytes with SHA-256
`57CA4A4C2440454E1F903ECCA166A6BBB566A2CE0E535AB26FFA62818F1D17C2`.

A cold migration-and-test rerun completed on 2026-09-05. Its receipt is
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260904_162757_895_2424\receipt.json`
(106,599 bytes, SHA-256
`6D4C2F17396E81AA0688F173EF444FC3E7C3800B478711A8D1E8E54C4A6DFE61`).
The map was byte-identical before and after. MacDonald and Temasek
EndToEndContract each completed exactly 1/1 test; geodesic circle, indexed RF
geometry query, RF interaction model, MacDonald aggregate, and Temasek
aggregate completed 1/1, 1/1, 1/1, 3/3, and 3/3 respectively.

The current clean module build receipt is
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260904_163218_385_17040\receipt.json`
(2,321 bytes, SHA-256
`AD6FF196A6033FFC5DDB1B78674F2ECE187ACA8446D5091AA3B674B57D468670`).
It pins the runtime DLL at 4,471,808 bytes / SHA-256
`DED213B2315107F77BAA89470D82CE837FED784141FDE6688E8F2F56D1CB2D58`
and the editor DLL at 7,286,784 bytes / SHA-256
`EEA65BEC55C0E8D34AD24107857F89B3071A3589F77C1A1E8F40AE88D153EB16`.
The same five test filters passed again against those exact binaries; the
post-build receipt is
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260904_164257_543_21120\receipt.json`
(54,226 bytes, SHA-256
`83C8A3D67FEF6C9949CC6789331BE85BA53E15726D4238D8A89BE77DB617F06D`).
All runs finished with source/native hashes equal, no recursive delete, and
the unrelated UE5.4 Capstone editor absent before, during, and after.

## Visual QA and realism cutoff

The final telemetry-only capture manifest is
`D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D\explore_v5d_r24_temasek_shophouse_evidence_r24_temasek_final_20260905_0032.json`
(140,324 bytes, SHA-256
`79C9E68BA6AA8CCD6E833620A078E156282562A211030665893EA7E67CB7A432`).
It pins the map and current DLLs above, validates the exact map/asset/PIE
contracts, holds each of three exact camera poses for 12 seconds, captures at
2560 x 1440, completes PIE teardown, exits the owned editor without forced
containment, and leaves zero editors and zero Remote Control listeners.

The three raster QA frames are:

- `explore_v5d_diagnostic_r24_temasek_shophouse_facade_close_r24_temasek_final_20260905_0032.png`, 4,982,485 bytes, SHA-256 `D7EF89AC33A10C3C31E2B6FAC9E4D1E3DBC8AEEA901990554F650B59AC139E8B`;
- `explore_v5d_diagnostic_r24_temasek_shophouse_front_corner_oblique_r24_temasek_final_20260905_0032.png`, 4,860,759 bytes, SHA-256 `BB47DCFD8B35681F99B71E0BBD7A877BC04B7517BABFE3A56080E82B0637B904`;
- `explore_v5d_diagnostic_r24_temasek_shophouse_streetscape_context_r24_temasek_final_20260905_0032.png`, 3,555,778 bytes, SHA-256 `6FBFBE67498131B8C6F7C72D5F50C601EDAD8B0F9960BD88AA75E385F1E3616B`.

Visual inspection accepts the result only as **somewhat realistic local-scene
QA**. The close view has readable facade rhythm, windows, timber/charcoal
separation, verandah depth, planting, shadows, and distinct material response.
The oblique and context views also expose the remaining limits: flat turf at
these ranges, stylized low-detail trees, coarse block-mass surrounding
buildings, unresolved shell/provider overlap, hard ground edges and black void
pixels. Provider progress remained 0%, so the frames are explicitly classified
`RASTER_VISUAL_QA_EVIDENCE_NOT_SURVEY_NOT_PROVIDER_GEOMETRY_PROOF`. They are
not hyperrealistic, not a present-day as-built reconstruction, and not evidence
that Cesium coverage was loaded. Provider imagery/geometry was not exported,
traced, analysed, derived, or baked.
