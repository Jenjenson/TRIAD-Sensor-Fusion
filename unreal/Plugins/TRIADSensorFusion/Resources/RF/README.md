# TightV1 runtime RF resources

The three JSON files in this directory are byte-for-byte staged copies of the
canonical TightV1 sources under
`unreal/SourceAssets/IstanaPublicViewRF/TightV1`. They are NonUFS runtime
dependencies, not separately authored assets.

Refresh and verify them only with:

```powershell
python unreal/SourceAssets/IstanaPublicViewRF/TightV1/sync_runtime_resources.py
```

The indexed geometry loader reads each strict LF/UTF-8 geometry/catalog file
exactly once, checks the configured hashes against those buffers, parses the
same buffers transactionally, and verifies the geometry-to-catalog hash
binding. Scenario initialization also reads the staged scene contract once,
hashes that same text, and requires the configured hash and geometry-declared
contract hash to agree before the query can become ready. Telemetry retains all
three verified hashes.
The resources remain assumption-bound simulation inputs and are not survey or
field-validation evidence.

The staged geometry includes the same closed, main-hero-only modeled coverage
envelope as the canonical source. The indexed query rejects endpoints or a
finite segment outside that envelope, so an out-of-domain link cannot be
reported as a clear/direct path. The envelope is explicitly not the
one-kilometre AOI and does not model surroundings. Loading also recomputes the
exact minimum AABB over every canonical vertex and bounded offline-witness
endpoint; missing, malformed, duplicated, or coverage-expanding witness data
fails before the query becomes ready.

## OneKilometreV2

The three `OneKilometreV2` JSON files are byte-identical staged copies of the
frozen R24C source package. They are a separate opt-in contract and do not
replace or alter TightV1. Runtime startup binds their geometry, catalog, scene
contract, coverage identity, simulation perimeter, Cesium georeference, and
EPSG:3414 construction frame before enabling dedicated RF.

Immediately before each V2 query, both Unreal endpoints are transformed to
WGS84 LLH by the uniquely admitted Cesium georeference. Horizontal study-domain
membership uses the strict closed 1,000 m WGS84 Vincenty circle. Geometry
coordinates use the separately pinned Singapore Transverse Mercator mapping:
X is EPSG:3414 easting minus 29064.15860639389 m, Y is the negative northing
delta from 32157.571268641685 m, and Z is ellipsoid height minus 47 m. Both
endpoints must also pass the loader's closed AABB. Any transform or admission
failure yields no direct, transmission, or legacy fallback result.
