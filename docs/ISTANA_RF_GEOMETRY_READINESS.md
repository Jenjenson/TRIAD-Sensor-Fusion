# Istana RF geometry readiness

Status: **not RF-ready for deployment or survey truth**. This document covers
the April 2024 public-reference approximation, the current source contract,
and the bounded guarded UE 5.5 verification recorded through 2026-09-06
Singapore time. It does not claim access to surveyed building fabric, interior
construction, door state, glazing composition, reinforcement,
frequency-calibrated material
measurements, or a successful live deployment.

## Runtime modes represented in source

The backwards-compatible **default** remains legacy mode:
`bUseDedicatedRFPropagation=false` in the generic configuration.
`ATRIADSensorFusionScenarioManager::ComputeLineOfSight` performs one
`LineTraceSingleByChannel` on the configured generic collision channel
(Visibility by default). The legacy link budget applies free-space path loss,
weather loss, and either zero obstruction loss or one configured scalar
`NonLineOfSightAdditionalLossDb`.

An **explicit opt-in source implementation** is also present and wired into the
scenario manager. `IstanaHighFidelityRF.example.json` sets
`bUseDedicatedRFPropagation=true`, requires the dedicated load, asserts the
exact world-origin/identity frame, and pins the TightV1 geometry, material
catalog, independently loaded scene-contract hashes, modeled-coverage identity,
and semantic IDs. That path uses the CPU indexed-geometry query
and deterministic interaction model for direct or paired straight-transmission
candidates. It does not use the Visibility result as RF loss or a detection
gate. Resource/path/frame/hash/schema/coverage failures fail closed when the
dedicated mode is required.

Neither mode calculates diffraction, coherent multipath phase, polarization,
or vegetation volume scattering. The opt-in scenario path does not enumerate
reflection candidates. In legacy mode, `lineOfSight` remains a binary generic-
collision observation rather than an RF ray solution.

These behaviors are source facts. Guarded native evidence now establishes
compilation, linking, and bounded contract-test execution for the relevant UE
5.5 implementation; it does not establish deployment behavior, a cold packaged
run, or field truth.

## Guarded UE 5.5 evidence

The guarded 2026-09-03 transaction finished with source/native hash parity and
recorded a clean Build receipt at
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260903_032947_525_25528\receipt.json`
(SHA-256
`9877E68A1C5E4F23C9E33689D0A1F49383F6A436DF17ADB143E920F33E98649E`).
Its exact build products were:

- runtime DLL: 4,275,200 bytes, SHA-256
  `47C66266773E6208313FA1CB51C20D1E335C6A5A841C4E137B415C4879148614`;
- editor DLL: 7,135,232 bytes, SHA-256
  `0B7C337BF7949C650DD22B233AB5F4D4F925E2A3C91230429E66A757A31D6F77`.

The cold Migrate+Test transaction is recorded at
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260903_032533_044_24980\receipt.json`
(SHA-256
`36436D05A5902B14064C9F87FE336B4D6200251DDA8F5AB28BE851A849C86ADE`).
The R24 map remained byte-identical across that rerun at SHA-256
`B79252C4919D6985CD4824B296B92D78F2B2BACD594BC1816CA130BAA8D5B460`,
and all ten owned R24 MacDonald House assets remained unchanged. The recorded
native filters completed as follows:

- `TRIAD.SensorFusion.Istana.GeodesicCircle`: 1/1;
- `TRIAD.RF.IndexedGeometryQuery.Contract`: 1/1;
- `TRIAD.SensorFusion.RF.GeometryInteractionModel`: 1/1;
- `TRIAD.Istana.ExploreV5D.MacDonaldHouse`: 3/3;
- `TRIAD.Istana.ExploreV5D.MacDonaldHouse.EndToEndContract`: 1/1 during
  migration.

The current two-landmark successor was then cold-verified at
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260904_162757_895_2424\receipt.json`
(106,599 bytes, SHA-256
`6D4C2F17396E81AA0688F173EF444FC3E7C3800B478711A8D1E8E54C4A6DFE61`).
The MacDonald and Temasek migration contracts each passed 1/1 while the hybrid
map stayed byte-identical at 34,991,378 bytes / SHA-256
`57CA4A4C2440454E1F903ECCA166A6BBB566A2CE0E535AB26FFA62818F1D17C2`.
A clean module-only build subsequently pinned the runtime DLL at 4,471,808
bytes / SHA-256
`DED213B2315107F77BAA89470D82CE837FED784141FDE6688E8F2F56D1CB2D58`
and editor DLL at 7,286,784 bytes / SHA-256
`EEA65BEC55C0E8D34AD24107857F89B3071A3589F77C1A1E8F40AE88D153EB16`.
That build receipt is
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260904_163218_385_17040\receipt.json`
(SHA-256
`AD6FF196A6033FFC5DDB1B78674F2ECE187ACA8446D5091AA3B674B57D468670`).
The post-build test receipt at
`D:\triad\TRIAD\Saved\TRIAD\Automation\R24NativeTransaction\20260904_164257_543_21120\receipt.json`
(SHA-256
`83C8A3D67FEF6C9949CC6789331BE85BA53E15726D4238D8A89BE77DB617F06D`)
again passed geodesic circle, indexed geometry query, interaction model,
MacDonald aggregate, and Temasek aggregate at 1/1, 1/1, 1/1, 3/3, and 3/3.

The later guarded OneKilometreV2 actual-file RF transaction is recorded at
`D:\triad\TRIAD\Saved\TRIAD\RFActualFileNativeTransactions\rf_actual_fix_20260905T184017Z\receipt.json`
(75,932 bytes, SHA-256
`1E7856C2435F6EEBBCB362B4B09C4DF19E8964544528F6204CC64A8A6690F0E2`).
It records `PASS`, one exact success and zero failures for
`TRIAD.RF.IndexedGeometryQuery.OneKilometreV2ActualFile`. The transaction's
final runtime DLL is 4,577,280 bytes with SHA-256
`C7A80FD955ED831A7148068444079E54D7D9847759D7DAAC24272022CC8A695A`.
For the hash-pinned 1 km geometry, deterministic replay completes clear after
29,904,936 containment triangle checks. The actual-file test therefore opts
into the finite 30,000,000-check cap while the ordinary loader default remains
20,000,000. Its completed 42,700-triangle load constructs exactly 16,383 BVH
nodes.

The transactions used exact file and asset allowlists. They preserved the
protected unrelated UE 5.4 editor/project content, left content outside the
admitted transaction unchanged, and performed no recursive deletion. These
receipts prove only the enumerated native build and automation contracts. They
do not provide a deployed/live dedicated-RF scenario receipt, a cold packaged
build receipt, observed field telemetry, or calibration against measurements.
The full OneKilometreV2 resource now has its own passing, hash-bound native
actual-file geometry-loader test in addition to static and synthetic checks.
That result validates bounded geometry ingestion and topology/overlap/BVH
construction; it does not validate material response against measurements,
field RF accuracy, or behavior in a live deployed scenario. The broader
indexed-query contract test continues to exercise the TightV1 resources.

## Geometry audit

The building authority reused by Explore V2, V3, and V4 is the V1
`SM_IstanaPublicView_Building_Collision`: seven individually closed,
outward-wound axis-aligned boxes and 84 triangles. Closed boxes are useful for
stable pawn and coarse occlusion behavior, but their union is not a
surface-faithful building envelope. Five hull pairs overlap with positive
volume, leaving internal faces and ambiguous entry/exit pairing for any future
thickness-based transmission calculation.

Two deterministic counterexamples prevent an RF-tight answer:

- V8 visually models three layered portal reveals centered at x = -6, 0, and
  +6 m. Every corresponding front-to-rear ray intersects the solid 29 m-wide,
  102 m-deep V1 central box in both directions. Whether the real portals are
  open, glazed, shuttered, or backed by doors remains owner/survey evidence,
  not something the visual opening can decide.
- The lower V1 proxy stops at z = 14.00 m and the tower proxy begins at
  z = 14.20 m. A bidirectional ray at z = 14.10 m passes without a hit even
  though the V5 opaque foundation plinth spans that height.

The V8 portico, V3 supplements, V4 vegetation, and mapping-grade OSM context
are deliberately `NoCollision`/non-authority. Tree pawn blockers respond only
to `Pawn`, so none of those additions improve RF geometry.

Machine-readable evidence and the exact acceptance sequence are in
`unreal/SourceAssets/IstanaPublicViewRF/istana_public_exterior_rf_readiness.v1.json`.
The predecessor collision checks bind the generator and frozen OBJ source, not
a cold-loaded Unreal map. There is no live-map receipt yet for the loaded
`BodySetup`, component transform, collision object type/responses, or the
world-space witness traces. In source, the building component enables
`QueryAndPhysics` but does not override `UStaticMeshComponent`'s
`BlockAllDynamic`/`WorldDynamic` default; a `WorldStatic` claim would therefore
require explicit configuration or proof of a serialized map override.

## Dedicated interaction seam and current wiring

`TRIADRFInteractionModel.h` introduces two independent interfaces:

- `ITRIADRFGeometryQuery` produces bounded deterministic direct, straight
  transmission, and single-specular-reflection candidates from a dedicated RF
  geometry layer. Every segment requires a fail-closed clear-leg witness.
- `ITRIADRFInteractionModel` evaluates frequency- and incidence-bounded
  empirical surface coefficients. Transmission entry/exit points are bound to
  the one straight segment; a reflection point and unit normal are bound to the
  sole interior vertex and checked against the specular reflection law.

Open apertures are paths with no collision interaction. Transmission requires
a paired entry/exit chord. One-bounce reflection has an explicit bounce vertex,
surface point, normal, and surface identity. Candidate count, vertices,
interactions, power count, frequency, and path length have configured limits
plus compiled safety ceilings. In V2 those hard maxima are 4,096 candidates,
3 vertices, 32 interactions per path, 4,096 received powers, 100 GHz, and
5,000 m. Candidate-owned path, geometry-query, witness, surface, material, and
optional calibration-provenance identifiers are preflighted at 256 characters
or fewer, together with every candidate array count, before any of them are
copied into output telemetry. The minimum distance/frequency envelope prevents
use where the scalar Friis expression would report negative path loss, but no
antenna-aperture data is present, so actual far-field applicability remains
explicitly unvalidated. Incoherent power summation is named as such; coherent
phase synthesis is outside this version.

Material calibration is tri-state: `NotApplicable` for a direct path,
`Uncalibrated`, or `Calibrated` with a provenance identifier. That state only
describes the scalar coefficient input. It does not validate geometry or the
propagation model. The V2 evaluator has no immutable external acceptance
context, so `bExternalAcceptanceContextBound` and `bReadyForSurveyTruth` are
hard false for every result; `bFriisFarFieldApplicabilityValidated` is also
false. This applies even to a mathematically valid path using calibrated
coefficients. A geometry-query clear witness is traceability input, not survey
authority.

The interface, TightV1 CPU indexed-geometry query, transactional resource
loader, scenario-manager selection, telemetry, and native automation tests are
present in source. The source-wired mode consumes the separate staged TightV1
JSON/catalog resources rather than translating the legacy seven-box Visibility
collision. The generic configuration still selects the legacy mode by default;
the dedicated implementation requires explicit opt-in.

Native compilation, linking, and the enumerated native contract tests are now
proven for the exact recorded UE 5.5 build and cold editor transactions. A
cold-loaded deployed or packaged dedicated-RF scenario, live resource/telemetry
binding, and field execution remain unproven. TightV1 remains a
public-reference, hero-only, uncalibrated assumption model. Its successful load
may establish `SIMULATION_READY_ASSUMPTION_BOUND`; it cannot establish field
validation, deployment readiness, or survey truth.

## Smallest truthful implementation path

1. Obtain owner-approved survey/control for the exterior envelope and the
   state/material/thickness of doors, glazing, louvres, and true openings.
2. Author a separate always-resident RF mesh, independent of render triangles
   and pawn collision, with stable surface IDs and a dedicated trace channel.
3. Fail the build on non-manifold solids, inconsistent winding,
   self-intersections, unclassified overlaps, zero thickness, missing material
   IDs, or unpaired entry/exit hits.
4. Run a deterministic bidirectional ray grid through every known solid and
   aperture plus independent survey checkpoints. Report RF, pawn-collision,
   Nanite, and render triangle counts separately.
5. Cold-load the exact target map and record the map/asset hashes, loaded body
   setup, component collision policy and world transform, and world-space
   bidirectional trace results in a real automation receipt.
6. Bind measured or approved frequency-bounded material profiles. Exercise the
   existing opt-in geometry/interaction wiring end to end in the exact packaged
   target, then retain a cold-loaded hash-bound status and telemetry receipt.
7. Introduce an immutable external acceptance context bound to the accepted
   geometry/profile hashes, epoch, frequency/incidence scope, phenomena, and
   measurement receipt. Only that external layer may expose readiness.
8. Validate direct, transmission, and single-reflection paths against field or
   controlled measurements for each claimed frequency band and epoch.

Only after those gates pass can “RF-tight” be answered yes—and then only for
the explicitly validated exterior, apertures, material states, bands, and
epoch. Diffraction and more complete multipath require a later, separately
validated model.
