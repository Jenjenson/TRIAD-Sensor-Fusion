# Istana GeospatialAuthorityV1 semantic admission

## Purpose and status

`GeospatialAuthorityV1` closes the format-semantics gap after strict Stage-0
release integrity. It decodes the actual terrain and model bytes instead of
accepting a TIFF or GLB header as evidence. A passing receipt has status
`GEOSPATIAL_AUTHORITY_V1_ADMITTED_NON_IMPORTABLE`.

That status is intentionally narrow. It does not prove that a supplier is
authentic, that measurements are truthful, or that every digital-twin package
has been validated. It does not validate LAS/LAZ, E57, IFC, imagery, camera
calibration, or vegetation-inventory semantics, and it does not unlock Unreal
import or confer collision, sensor, or RF authority.

## Inputs and fail-closed checks

The gate accepts a strict
`Releases/<releaseId>/manifest.json` and a reviewed policy using
`triad.istana_geospatial_authority.v1`. The policy schema is at
`unreal/SourceAssets/IstanaDigitalTwin/Schemas/triad.istana_geospatial_authority.v1.schema.json`.
The validator first reruns the existing strict Stage-0 validator, then requires
the policy to bind the exact release ID, inventory digest, acceptance-payload
digest, asset SHA-256 values, and byte lengths.

GeoTIFF checks include:

- projected SVY21 / EPSG:3414 coordinates matching the release CRS, whose
  Stage-0 axis order is exactly `easting,northing`, with
  `ProjLinearUnitsGeoKey` fixed to EPSG:9001 metres; the GeoKey directory must
  use the exact reviewed 1.0 header and contain exactly the four reviewed
  inline SHORT keys (`GTModelType`, `GTRasterType`, `ProjectedCSType`, and
  `ProjLinearUnits`); projected-parameter/unit-size overrides and other
  unreviewed keys fail closed;
- terrain-asset metadata fixed to `SOURCE_CRS` metres, with the reviewed
  decoded-bounds policy contained by the Stage-0 asset envelope;
- a nonsingular, axis-aligned ModelTransformation, or ModelPixelScale plus one
  tiepoint; rotated/sheared grids fail closed until a polygonal footprint and
  seam profile is reviewed;
- explicit PixelIsArea/PixelIsPoint and GDAL nodata metadata, with nodata
  canonicalized to the decoded integer/IEEE sample domain before exact
  comparison, so a distinct representable elevation is never erased by a
  relative tolerance;
- no embedded vertical CRS, datum, or unit GeoKeys: those keys fail closed
  until a separate reviewed height-reference contract exists, rather than
  overriding or contradicting the Stage-0 metre declaration;
- a single numeric elevation band in the reviewed classic-TIFF NONE/DEFLATE
  profile, with exactly one complete strip or tile storage family, bounded
  file/compressed-block and padded-tile decode sizes, a cumulative
  metadata-read ceiling, and no materialization of irrelevant/private tag
  payloads;
- decoded finite elevation values and independently expected full-raster 3D
  decoded bounds, plus AOI-scoped minimum/maximum/relief, bounded adjacent-cell
  jumps across every cell with positive AOI intersection, area-weighted
  per-file nodata, and
  exact zero-gap AOI coverage from a sweep-line union of the clipped extents of
  every decoded valid raster cell (not point sampling, so sub-cell strip gaps
  and nodata cells are detected);
- explicitly declared multi-file seams, swept over the exact joint along-seam
  cell-boundary partition so each valid count is a distinct cell-pair support
  segment and offset-grid jumps contribute to maximum/P95 delta limits.

GLB checks include:

- exact glTF 2.0 length/chunks and one embedded buffer, with no external buffer
  or image URI (including data URIs), unsupported `asset.minVersion` (it must
  be absent or exactly `2.0`),
  used/required extension declaration, nested extension payload, or sparse
  accessor bypass;
- the JSON-chunk length is checked against the fixed 16 MiB ceiling before its
  payload is read or allocated. Header, JSON, BIN and padding are consumed from
  the file stream instead of materializing and then slicing the whole GLB; only
  the admitted, unpadded BIN payload is retained for accessor decoding;
- decoded finite `POSITION` and index accessors, matching declared min/max,
  valid attribute counts and triangle indices, and no degenerate triangles;
- default-scene traversal with affine node transforms; cycles, repeated node
  references, multiple parents, and any node not reachable from the active
  default scene are rejected. Stable IDs must be unique and non-placeholder,
  and source-CRS bounds are recomputed. Every explicit raw node matrix must
  have the exact mathematical bottom row
  `[0,0,0,1]` and a nonsingular 3x3 linear part before any vertex is
  transformed; testing only the delivered sample vertices is not sufficient;
- a static-geometry-only profile: morph targets/weights, skins and animations
  fail closed because they can change rendered geometry beyond decoded base
  `POSITION` values;
- role-specific triangle floors, approved extents and bound residuals;
- edge incidence/non-manifold checks and, for building, collision, and context
  roles, a closed two-manifold plus positive decoded signed volume;
- per-accessor and cumulative decoded-element/component, transformed-vertex,
  triangle-topology, mesh-instance and primitive-instance work ceilings.

Unsupported or ambiguous encodings fail closed. They require a reviewed parser
revision; they are never silently skipped.

The GLB gate never dereferences a URI. BIN-backed image/material metadata may
remain in the container, but this geometry gate does not decode it or grant it
material/colour authority; the calibrated-reference rules below remain
separate and mandatory.

V1 also refuses before pixel allocation or AOI-quality scans when one file
exceeds its remaining cumulative decoded-cell budget or its intersecting AOI
cells exceed the exact-union budget. The exact sweep separately caps
raster-plan-by-Y-band work, preventing a many-offset-tile release from turning
the geometric proof into unbounded quadratic work.

## Run

Keep both policy and output receipt inside the controlled release directory:

```powershell
scripts/Validate-IstanaGeospatialAuthorityV1.ps1 `
  -ManifestPath unreal/SourceAssets/IstanaDigitalTwin/Releases/<releaseId>/manifest.json `
  -PolicyPath unreal/SourceAssets/IstanaDigitalTwin/Releases/<releaseId>/geospatial-authority-v1.json `
  -ReceiptPath unreal/SourceAssets/IstanaDigitalTwin/Releases/<releaseId>/Receipts/geospatial-authority-v1.receipt.json `
  -PythonExecutable python
```

Receipts are canonical JSON and are never overwritten. Each receipt binds the
manifest bytes, policy bytes and canonical policy digest, both validator
scripts and the policy schema, Stage-0 inventory and acceptance digests, every
inspected asset hash, decoded terrain/model results, and a canonical
`receiptPayloadSha256`. The validator rehashes inputs and its toolchain after
inspection to detect in-run mutation.

## Policy review rules

Thresholds are evidence, not tuning knobs. A survey/model-validation reviewer
must set relief, nodata, seam, bounds, extent, triangle, stable-ID, topology,
and volume requirements from the acquisition specification and delivered
product metadata before seeing a pass/fail result. The AOI must exactly match
the Stage-0 context AOI. Both legacy uncovered-fraction/contiguous-sample policy
declarations remain hard-coded to zero, while admission is decided by the exact
valid-cell union's zero-gap boolean and zero uncovered area.

Do not create a policy for a development fallback, generated public-reference
mesh, flat plane, placeholder box, OSM-only reconstruction, SRTM fallback, or
streamed-provider cache. Passing file syntax cannot create source authority;
strict provenance, rights, security review, independent QA, and the remaining
format-specific gates are still mandatory.

The synthetic positive and negative fixtures live only in
`unreal/SourceAssets/IstanaDigitalTwin/tests/test_validate_geospatial_authority.py`.
They exercise parsing and rejection behavior and explicitly are not data.
