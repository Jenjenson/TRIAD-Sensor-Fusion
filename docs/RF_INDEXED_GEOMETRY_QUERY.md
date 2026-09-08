# RF indexed-geometry query (P1)

`FTRIADRFIndexedGeometryQuery` is the deterministic CPU consumer for canonical,
assumption-bound indexed geometry and material catalogs. TightV1 remains its
hash-pinned production baseline. It is additive: it
does not replace render, Pawn/navigation collision, the legacy Visibility
trace, or the repository-wide readiness decision.

## Bound inputs

- Geometry:
  `unreal/SourceAssets/IstanaPublicViewRF/TightV1/Generated/IstanaPublicViewRFTightV1.geometry.json`
- Material catalog:
  `unreal/SourceAssets/IstanaPublicViewRF/TightV1/istana_rf_materials_tight_v1.catalog.json`

Both files must be staged together as exact NUL-free LF/UTF-8 without a BOM.
For packaged builds they need an explicit NonUFS staging rule (or an external
configuration-managed absolute path); files below `SourceAssets` are not
automatically packaged by Unreal.

The integrated plugin keeps byte-identical runtime copies under
`Plugins/TRIADSensorFusion/Resources/RF`, and its runtime module declares both
as `RuntimeDependencies`. The high-fidelity example selects them with
`Plugin/Resources/RF/...`; the scenario resolver intentionally rejects raw
absolute paths and parent traversal. A deployment must also set the exact
world-package binding, set `bDedicatedRFFrameIsWorldOriginIdentity=true`, and
pin the geometry hash, catalog hash, geometry-query ID, and catalog ID. See
`Resources/IstanaHighFidelityRF.example.json` for the complete opt-in block.

The production overload reads each strict canonical geometry/catalog file once,
hashes those buffers, requires their configured SHA-256 values, and parses the
same buffers transactionally. The consumed hashes are retained in
`GetMetadata()` and emitted to telemetry. The loader also checks the exact
catalog SHA-256 recorded in the geometry file. Scenario initialization
independently reads and hashes the staged scene contract, then requires that
hash to match both the scenario configuration and the geometry-declared
`contractSha256` before dedicated RF can become ready.

## Lifecycle and call sequence

1. Construct one `FTRIADRFIndexedGeometryQuery` owned by the scenario runtime.
2. Resolve the two configured paths and call the expected-hash
   `LoadFromJsonFiles` overload once during initialization. Do not pre-hash and
   reopen the resources in the caller.
3. Require both a successful return and `IsReady()`. Log the immutable values
   from `GetMetadata()` with the run receipt.
   `TryGetMaterialMetadata`, `TryGetSolidMetadata`, and
   `TryGetSurfaceMetadata` expose stable provenance, boundary, uncertainty,
   aperture, and finite-primitive bindings without reparsing either source
   document.
4. For each link, call `BuildPathCandidates(tx, rx, frequency, limits, ...)`.
   The query first requires both endpoints and the complete finite segment to
   remain inside the closed, hash-bound modeled-coverage envelope.
5. A `true` return with no candidates is a valid opaque straight-path block.
   Do not pass an empty set to the interaction model.
6. A `false` return means malformed data, an unsupported/ambiguous geometry
   contact, or an out-of-envelope query. Mark that RF result unavailable; do
   not silently reinterpret it through generic Visibility collision.
7. Evaluate the returned Direct or Transmitted candidate with
   `FTRIADDeterministicRFInteractionModel`. Preserve the geometry-query,
   surface, solid, material, profile/provenance, and full catalog-hash
   identifiers in run telemetry. The catalog has stable profile IDs but does
   not define independent per-profile hashes.

Each `triad.rf_interaction_trace.v3` telemetry row preserves the physical
first-entry owner in its singular compatibility fields and carries an ordered
`contributors` array as the authoritative provenance for every solid merged
into a continuous same-material span. Each contributor retains its solid,
entry/exit surfaces, source and uncertainty classes, and entry/exit points.
The row also preserves material, selected profile,
calibration state/provenance, selected frequency and incidence intervals, the
explicit no-interpolation selection rule, all four available scalar
coefficients, geometry-derived traversal/thickness/incidence, and the applied
interaction loss. JSONL stores the structured rows directly; CSV stores the
same array as an escaped compact JSON field. This makes the applied prior
auditable but does not calibrate it or grant RF/survey authority.

Loading mutates the query and is intentionally transactional/fail-closed.
Do not reload it concurrently with read-only segment or path queries.

## Coordinate contract

The canonical input is right-handed Z-up metres with ceremonial approach in
`+Y`. The loader applies the contract once:

```text
Unreal centimetres = (100*x, -100*y, 100*z)
triangle indices   = (i0, i2, i1)
```

This preserves outward normals in Unreal's frame. TightV1 requires an identity
component transform and forbids negative component scale. World transmitter
and receiver positions may be passed directly only when the RF origin is the
same inherited public-view world origin. No unverified translation, rotation,
scale, or georeference correction should be inferred at runtime.

## Fail-closed geometry contract

Loading rejects resource-bound violations, hash mismatch, schema/status drift,
unknown or duplicate IDs, missing explicit V2 runtime coefficients, invalid
profile ranges, non-finite values, out-of-range indices, range overlap/gaps,
unused vertices within a solid's stable range,
surface/material mismatch, degenerate faces, disconnected solids, open or
inconsistently wound manifolds, non-positive volume, declared-bound mismatch,
indexed-polyhedron triangle self-intersection, positive-volume overlap or
containment between finite solids, missing or malformed bounded witnesses, and
any coverage AABB that is not the exact minimum over canonical vertices plus
every offline-witness endpoint. Zero-volume boundary contact remains valid.
The generic validation has explicit load-time ceilings for self-intersection,
cross-solid triangle, and winding-number containment checks. Candidate counts
are charged before full three-axis triangle-AABB rejection, and signed volume
uses a solid-local origin with compensated finite accumulation.

Solids may use TightV1's `AXIS_ALIGNED_BOX` primitive, whose declared primitive
bounds must still match the indexed and solid bounds exactly. Its indexed mesh
must additionally prove exact equivalence: eight unique min/max corners, all
vertices used, twelve triangles, and two outward-wound complete-area triangles
on each of the six box planes. A generic mesh cannot gain box semantics by
merely changing its primitive label. Solids may instead use
`INDEXED_CLOSED_POLYHEDRON`, whose indexed boundary is the finite primitive.
The AABB/AABB overlap fast path and TightV1 behavior remain unchanged.

The deterministic BVH returns increasing finite-segment crossings. Outward
winding classifies entry and exit. Coplanar/tangent contact, segment endpoints
on a boundary, unpaired triangle-edge contact, sharp-edge/corner ambiguity,
non-alternating crossings, or endpoints inside solids all fail closed.

TightV1's declared coverage is the closed AABB from
`[-57.5, -56.0, 0.0]` to `[57.5, 23.3, 36.0]` logical metres, derived from
the canonical hero and offline witness endpoints. Runtime loading independently
recomputes and exactly compares that derivation. `GetMetadata()` and
`IsFiniteSegmentWithinModeledCoverage()` expose that contract. A segment with
either endpoint outside is rejected before BVH traversal and cannot produce a
Direct candidate. The TightV1 box is a main-hero-only assumption boundary, not
the one-kilometre AOI, surrounding blocker coverage, survey truth, or field
validation. The loader no longer hardcodes that identity: it retains any
bounded, non-empty coverage scope and its one-kilometre, surroundings, survey,
and field flags as metadata. Scenario configuration must pin and authorize
those exact claims. The closed-AABB query policy, exact derived bounds,
canonical-geometry and witness inclusion, and outside-domain rejection remain
loader-enforced. `coversCanonicalGeometry` is the preferred field; schema-v1
`coversCanonicalHeroGeometry` remains an accepted legacy alias.
If both fields are present, their boolean values must agree.

## Current boundary

- Candidate enumeration is direct plus straight paired transmission only.
- A catalog material with `allowsTransmission=false` yields a successful query
  with no straight-path candidate.
- Single-reflection enumeration, diffraction, diffuse scattering, coherent
  phase, polarization, foliage/weather, interiors, surrounding buildings, and
  antenna near-field applicability remain outside this P1 implementation.
- A clear-segment witness is closed-world evidence for the exact loaded RF
  mesh inside its declared envelope. It is not proof that the
  environment is complete, that surroundings are modeled, or survey accepted.
- The loader and empirical material model do not set survey readiness or field
  validation true.

## Verification

`TRIAD.RF.IndexedGeometryQuery.Contract` covers portable SHA-256, transactional
load failure, manifold rejection, deterministic BVH hits, bidirectional
entry/exit classification, direct/transmission candidate construction,
opaque blocking, ambiguous profile boundaries, and the interaction-model seam.
It also covers same-buffer hash retention, expanded/missing/malformed coverage
witness rejection, scenario-pinned generalized coverage metadata, all six
closed envelope faces, and epsilon-outside failure. Generic coverage includes
a sloped tetrahedron, triangle self-intersection, partial overlap, containment,
and a valid zero-volume shared-face contact. Review regressions cover relabeled
and malformed boxes, unused vertices, far-origin volume stability, conflicting
coverage aliases, bounded contributor validation, and two adjacent
same-material contributors retaining distinct provenance while one paired
boundary prior keeps total material loss at 25 dB.
When launched with `-TRIADRFTightGeometry=...` and
`-TRIADRFTightCatalog=...`, it also loads the exact TightV1 artifact and checks
the open west portal and closed centre-door witnesses.
