# Istana Cesium World Terrain height-readiness decision

Status: **fail-closed comparison admission prepared; comparison remains denied
until R33 native visual acceptance and independently verified licensed evidence;
not accuracy- or survey-ready**.

Current native checkpoint: R30-19 is committed under receipt SHA-256
`F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F`.
No R30 Player0 capture has been accepted; the latest run09 attempt failed
closed and rolled back cleanly, and R31--R33 remain non-native. Nothing in that
native-status change supplies a datum transform, licensed checkpoints, or CWT
accuracy evidence.

This decision preserves the requested geospatial layout without inventing a vertical alignment. R33 may present Cesium World Terrain (CWT) as a visual reference, but no current repository artifact proves that its ellipsoid heights, the local Unreal origin, the Copernicus DSM, and any Singapore height datum are interchangeable.

## Current evidence

- R33 pins one Cesium georeference at longitude `103.84288055`, latitude `1.30709615`, and origin height `47.0`, but does not assert a datum for that origin height.
- CWT is Cesium ion asset `1`. In Cesium for Unreal 2.18.0, [`ACesium3DTileset::SampleHeightMostDetailed`](https://cesium.com/learn/cesium-unreal/ref-doc/classACesium3DTileset.html) returns metres above the configured ellipsoid, usually WGS84, not mean sea level; the input height is ignored unless sampling fails.
- [Cesium's CWT specification](https://cesium.com/platform/cesium-ion/content/cesium-world-terrain/) describes a fused, visualization-optimized quantized mesh. Singapore has no published local resolution or accuracy guarantee; the applicable approximately -60 to 60 degree latitude band is listed only as approximately 30-90 m resolution.
- The local Copernicus GLO-30 source is a 30 m digital surface model in EGM2008 height (`EPSG:3855`). It includes buildings and vegetation and is not a bare-earth DTM.
- The Copernicus AOI is finite and hash-pinned, but its height-error mask covers only 59.8576% of retained source samples. No independent checkpoint coordinates are bundled.
- The centre bilinear Copernicus DSM value is `49.04186671265586 m` EGM2008. The separate `47 m` origin is documented as an SRTM-derived fallback, not a surveyed control point.
- No approved EGM2008 geoid grid, SGEOID09 operation, datum-labelled held-out checkpoints, LAS/LAZ survey, or signed residual report is present.
- A 7 September 2026 in-memory retrieval of the public SLA Vertical Control
  GeoJSON is now receipt-bound by byte length `168,293` and SHA-256
  `77449DA1774A943DCAF132E11C2BD6A7F4531D9E391DADFE8B6334EBC6A4D391`.
  Of its 314 index features, exactly three fall within the fixed 1 km R33
  centre: VCP `80157` at 418.7 m, `80185` at 687.7 m, and `80187` at 805.7 m.
  The derived register in
  `unreal/SourceAssets/IstanaDigitalTwin/TerrainAccuracy/` keeps all three
  quarantined: public-right-of-way suitability is unverified, no reduced level
  has been acquired, and each source Z=`0` is explicitly forbidden as a height.
  The downloaded GeoJSON and no CWT output were persisted.
- The source-only
  `istana_cwt_height_comparison.admission.v1.json` contract now binds that
  immutable register and structurally separates VCP top-of-mark anchors,
  historical ground points, Copernicus DSM comparisons, and current licensed
  bare-earth evidence. Its validator rejects the three index IDs and source
  dataset as height evidence, unknown/EGM96 datums, mixed evidence classes,
  unfrozen candidate/held-out partitions, and undeclared class-specific
  thresholds. V1 deliberately always reports
  `EXTERNAL_EVIDENCE_FILES_UNVERIFIED`, so its strict mode cannot authorize CWT
  sampling from declarations or plausible-looking hashes alone.
- The separate
  `istana_cwt_height_comparison.admission.v2.json` template and
  `validate_height_comparison_admission_v2.py` now provide the bounded
  successor gate. V2 requires a caller-supplied non-reparse evidence root and
  byte-verifies explicit relative-path/SHA-256 bindings for the R33 native
  transaction, accepted visual review, provider terms, applicable datum
  operations and every reference-point source. It strictly parses the two R33
  schemas, replays committed/human-accepted no-authority truth, and cross-binds
  the native receipt hash plus successor map and DLL states. Its admission
  expires within 24 hours and rejects stale R33 acceptance or operator intent.
  The checked-in template remains `PRE_SAMPLING_ADMISSION_DENIED`; no real
  external evidence bundle is checked in. The current TerrainAccuracy suite
  passes `58/58`; no CWT value or native Unreal asset was accessed.

Therefore neither the Copernicus DSM nor the `47 m` origin may be used to claim CWT accuracy.

## Official-source findings

### Singapore vertical datum and conversion

- The [Land Surveyors Board directives](https://lsb.mlaw.gov.sg/files/LSB_Directives_ver1.pdf) require Singapore Height Datum (SHD) for vertical height measurements, say SHD measurements are to be derived from SLA Vertical Control Points, and require SGeoid09 for GNSS-derived vertical measurements subject to the needed accuracy. The [current SLA survey-reference page](https://www.sla.gov.sg/regulatory/property-boundaries/survey-reference-system/) says the national vertical network has about 400 Precise Levelling Benchmarks at roughly 1 km intervals along major roads.
- The [SLA SGEOID09 page and calculator](https://app.sla.gov.sg/sirent/About/SGEOID09) provide the Singapore-specific relationship between WGS84 ellipsoidal height and reduced level. For an authorized, pinned SGEOID09 operation, use `H_SHD = h_WGS84 - N_SGEOID09`; in the other direction use `h_WGS84 = H_SHD + N_SGEOID09`.
- The [published BASE7 definition](https://app.sla.gov.sg/sirent/About/PlaneCoordinateSystem) supplies a non-local conversion regression test: WGS84 ellipsoidal height `26.824 m`, reduced level `17.113 m`, and geoidal undulation `9.711 m`, so `26.824 - 9.711 = 17.113`. This verifies sign and units only; it is not an Istana-area terrain checkpoint.
- SGEOID09 does not by itself establish precise or survey-grade height. SLA says the service is valid only for mainland Singapore, is not a replacement for PLBM information, is not intended for cadastral or hydrographic surveys, and must not be used to derive precise heights without established survey verification. The page's terms reserve SLA's data and intellectual-property rights and restrict commercial use, reproduction, publication, and creation of derived products or services unless expressly authorized. A production, batch, persisted, redistributed, or embedded conversion therefore needs an applicable licence or prior written SLA permission; the public interactive calculator is not evidence of those wider rights.
- Historic material must be checked for the obsolete false `+100 m` PWD datum. The [LSB SHD implementation notice](https://lsb.mlaw.gov.sg/notices-and-announcements/notices/lsb-notice--shd-in-survey-plans---implementation-advice/) says new work should use `SHD = 0.000 m` and specifically warns that contract material may still use the `+100 m` false datum. Never silently remove 100 m: quarantine the record until its datum is evidenced.

### Datum anchors are not ground-surface checkpoints

- The open [SLA Vertical Control dataset](https://data.gov.sg/datasets/d_c1f2b991f3932fb69b48edcceef54d13/view) can lawfully be used under the [Singapore Open Data Licence](https://www.sla.gov.sg/singapore-open-data-licence/) to locate candidate public-road VCP identifiers. Its download-discovery endpoint is `GET https://api-open.data.gov.sg/v1/public/api/datasets/d_c1f2b991f3932fb69b48edcceef54d13/poll-download`.
- That GeoJSON is an index, not an elevation source. Its published attributes contain identifiers/status metadata but no reduced level, and preview point coordinates contain a third ordinate of `0`. **The GeoJSON Z value of `0` is not a terrain height, benchmark reduced level, or valid checkpoint.** Any pipeline that interprets it as elevation must fail closed.
- [Chief Surveyor Circular 2/2015](https://lsb.mlaw.gov.sg/files/CS-CIRCULAR-NO-2-2015.pdf) says the reduced level in the VCP product purchased from INLIS is the mark's vertical distance from SHD. The [official SLA VCP datasheet example](https://www.sla.gov.sg/qql/slot/u143/Newsroom/Circulars/2010/CS%20CIRCULAR%20NO.%201_2010/Circular_2_2010.pdf) makes the measurement point explicit: the reduced level is measured to the top of the bolt. A VCP datasheet can therefore anchor the datum remotely, but it is not a ground-surface checkpoint unless a separately evidenced ground-to-mark tie exists.
- [INLIS](https://app.sla.gov.sg/inlis/) is SLA's public search/purchase route for land-survey and BCA borehole information. Current access, product, fee, reuse, and redistribution conditions must be captured from the transaction and licence; portal availability alone does not confer reuse rights.

### Remote ground-surface evidence and authoritative terrain

- BCA's [2025 site-investigation guideline](https://www1.bca.gov.sg/docs/default-source/docs-corp-news-and-publications/circulars/circular-on-guidelines-on-requirements-for-site-investigation-reports.pdf) identifies existing borehole data as publicly accessible through INLIS. The official [AGS(SG) data dictionary](https://www1.bca.gov.sg/docs/default-source/docs-corp-regulatory/building-control/electronic-transfer-si-data.pdf) defines `HOLE_NATE` and `HOLE_NATN` as SVY21 coordinates and `HOLE_GL` as the ground level in metres relative to SLA's Precise Levelling Datum. A suitable public-road record is a possible independent **historical ground-surface checkpoint**, not a control mark.
- A borehole record must retain its investigation date, source report, responsible/certifying party, stated datum, method, uncertainty if supplied, and evidence that the surface has not since been regraded. Reject unknown datum, suspected PWD `+100 m`, fabricated/example coordinates, missing `HOLE_GL`, stale or visibly regraded sites, protected-property-only records, and points whose surface class does not match the terrain being tested. The older `PLD` label must not be silently asserted to be current SHD without documented lineage.
- SLA states that it maintains authoritative terrain representations and that its third national aerial mapping exercise, begun in 2024 and completed in September 2025, covered 98% of Singapore; see the [2026 National 3D Mapping Programme account](https://www.sla.gov.sg/articles/-the-lines-that-define-singapore/). SLA also identifies LiDAR scans, orthophotos, DSM and DTM as 3D data products supplied for an [SLA-SMU research collaboration](https://www.sla.gov.sg/news/press-release/deepening-regional-skills-development-in-land-administration-and-management--and-support-solutions-to-tackle-singapore-s-urban-heat-effect/).
- No documented public self-service DTM, DSM, national LiDAR download, or terrain-height API was found in the current official [SLA licensed-data catalogue](https://www.sla.gov.sg/geospatial/digitised-land-information/), [OneMap API catalogue](https://www.onemap.gov.sg/apidocs/), or data.gov.sg SLA catalogue. OneMap's documented coordinate conversion is horizontal and cannot perform SHD conversion. The lawful route for current authoritative terrain is a scoped licence/research request through [GeoWorks/SLA](https://geospatial.sla.gov.sg/contact-us/) for a releasable public-surroundings cutout, with protected-property exclusions accepted.
- Such a request must state DTM versus DSM, acquisition epoch, horizontal and vertical CRS, geoid operation, ground classification, grid/point spacing, accuracy and control metadata, void policy, permitted storage, use in an offline Unreal simulation, derived-result rights, attribution, retention, and redistribution. Receipt of data without those fields would not close the readiness gap.

### Cesium and cross-product limits

- CWT is the dataset under evaluation and cannot validate itself. Its public source list includes SRTM, so an SRTM-derived `47 m` fallback is also not an independent CWT checkpoint. CWT remains visual context even after a successful datum conversion.
- Under the current [Cesium ion Terms of Service](https://cesium.com/legal/terms-of-service/), an active account/plan and attribution are required, and offline copying, storage, or redistribution of Cesium Data Output is generally prohibited except for permitted clips and limited caching. Treat sampled numeric CWT heights conservatively as Cesium Data Output unless Cesium confirms otherwise in writing. Do not persist a sampled point array merely because the runtime API returned it; record only outputs expressly allowed by the applicable plan/licence.
- The [Copernicus DEM GLO-30 specification](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM) and official [Sentinel Hub DEM API documentation](https://documentation.dataspace.copernicus.eu/APIs/SentinelHub/Data/DEM.html) permit an EGM2008 DSM to be normalized to WGS84 ellipsoidal height; the API's `egm=true` option returns ellipsoidal WGS84 height. The public [NGA EGM2008 resources](https://earth-info.nga.mil/GandG/wgs84/gravitymod/egm2008/index.html) are another authoritative datum-operation source. This creates a same-datum inter-product diagnostic, not an SHD operation, bare-earth checkpoint, or survey truth.

The evidence classes must remain separate:

| Evidence | Permitted role | It does not establish |
|---|---|---|
| INLIS VCP/PLBM reduced level | Authoritative SHD datum anchor at the monument measurement point | Adjacent ground-surface height or CWT accuracy |
| INLIS BCA `HOLE_GL` with acceptable provenance | Independent, dated historical ground-surface checkpoint | Current surface after regrading, a survey certificate, or national terrain authority |
| Copernicus GLO-30 normalized from EGM2008 | Independent-product DSM comparison in a common ellipsoidal datum | Bare-earth or SHD accuracy |
| CWT sampled height | Candidate visualization-terrain value under Cesium terms | Independent validation or a provider-version-pinned survey surface |
| Licensed SLA DTM/LiDAR with epoch, datum, accuracy and control metadata | Candidate present-day authoritative surface source | Accuracy of this implementation until held-out validation passes |

## R33/R34-independent acquisition and checkpoint sequence

The following evidence acquisition may begin before R33 visual acceptance and does not depend on or authorize R34. It must not mutate the Unreal map, native assets, the R33 controller, collision/navigation, sensor/RF authority, or any source contract.

1. Open an immutable acquisition register before searching. Record request purpose, source URL, access time, named licence/terms version, intended storage and use, requestor, approver, and a rule excluding unpublished or protected-compound internal data. Limit searches to officially offered public records on surrounding public rights-of-way or other expressly releasable areas.
2. Query the public SLA Vertical Control GeoJSON only to shortlist spatially distributed public-road VCP IDs. Record the dataset ID, retrieval metadata and Open Data Licence attribution. Assert that no coordinate with Z=`0` can populate `heightMeters`.
3. Search or purchase the shortlisted VCP products through INLIS. Preserve the product receipt and governing terms. Extract mark ID, SVY21/WGS84 horizontal location, SHD reduced level, monument/measurement-point description, observation/control date and stated uncertainty. Class every accepted VCP as `datum_anchor_top_of_mark`, never `ground_surface`.
4. Search INLIS for BCA boreholes on adjacent public roads or other non-sensitive public locations. Preserve the complete source report and transaction rights. Extract `HOLE_NATE`, `HOLE_NATN`, `HOLE_GL`, date, datum text, surface description, responsible party and uncertainty. Inspect public evidence for regrading. Class an accepted point as `historical_ground_surface`; quarantine it if PLD-to-SHD lineage, `+100 m` status, epoch, location, or surface stability is unresolved.
5. Resolve conversion rights before implementation. Pin an approved SGEOID09 operation for SHD-to-WGS84 ellipsoid conversion and a separately approved EGM2008 operation for Copernicus-to-WGS84 conversion. Run the BASE7 identity as a sign/unit test. EGM96 must not substitute for EGM2008, and a global EGM model must not substitute for SGEOID09 when converting an SHD anchor.
6. Submit a scoped GeoWorks/SLA request for a licensed public-surroundings DTM or classified LiDAR cutout. Ask for the smallest releasable area and accept security redactions. Do not infer download or reuse rights from OneMap3D visualization or scrape an undocumented endpoint.
7. Freeze candidate and held-out partitions before any CWT samples or residuals are viewed. Keep any points used to select offsets, fit a bias, tune thresholds, or reject an area out of the held-out set. Pin every point's authority, licence, epoch, uncertainty, measurement point and surface class.
8. Predeclare separate pass criteria for: datum-operation correctness; VCP datum-anchor residuals; historical ground-surface residuals; Copernicus DSM cross-product residuals; and, if licensed, current SLA DTM/LiDAR held-out residuals. Never pool unlike surfaces or promote one evidence class using another class's threshold.
9. Produce an acquisition-readiness receipt without sampling CWT. It may state only which legal/technical inputs are admitted, quarantined, missing, or denied. R33 visual acceptance and R34 admission remain unchanged.
10. Only after R33 native visual acceptance and provider persistence permission may the gated comparison implementation below sample CWT. A VCP-only result can close a datum-anchor check but not ground-surface accuracy. Historical boreholes can add dated spot checks but not a present-day claim. Without licensed current surface data and independent held-out validation, the final decision must remain `NOT_AUTHORITATIVE_PRESENT_DAY_TERRAIN_ACCURACY`.

## Allowed next implementation and current admission gate

The acquisition sequence above may proceed immediately because it neither
samples CWT nor admits an R34 successor. A first source-only admission layer is
now implemented with the claim:

`DATUM_NORMALIZED_INTERPRODUCT_COMPARISON_ONLY_NOT_SURVEY_OR_ACCURACY_VALIDATION`

It remains permanently denied in schema v1 because v1 cannot prove external
evidence-root containment, file existence and hashes, or accepted R33 receipt
truth. Schema v2 now implements those local-evidence checks without weakening
v1. Its checked-in state is also denied. The R30-19 native transaction exists,
but no R30 Player0 capture has been accepted, R31--R33 remain non-native, and
the licensed external evidence does not yet exist.
Only a v2 strict-mode success against a caller-supplied evidence root may admit
the eventual comparison implementation, which must then:

1. Resolve exactly one valid R33 controller, its CWT tileset, the shared georeference, CWT ion asset `1`, and the expected WGS84 ellipsoid radii.
2. Run only from one explicit, gated operator request. `SampleHeightMostDetailed` can load a tileset even when it is hidden or suspended, so construction, `BeginPlay`, and `Tick` must never initiate sampling.
3. Preflight approved provider terms, entitlement, attribution, persistence, offline-storage, derived-result and redistribution rights; permit only one in-flight immutable request; and keep the callback owner safely alive or weak-bound.
4. Reject unknown datum, missing transform/grid, non-finite or out-of-AOI coordinates, duplicate point IDs, changed inputs, result-count/order or longitude/latitude mismatch, any failed sample, and warnings when producing acceptance evidence.
5. Convert only with pinned, licensed operations: use `h_WGS84 = H_EGM2008 + N_EGM2008` for Copernicus and `h_WGS84 = H_SHD + N_SGEOID09` for admitted SHD anchors/checkpoints. EGM96 must not substitute for EGM2008, and EGM2008 must not be presented as an SHD operation.
6. Emit an immutable receipt that pins the point file, each geoid model/operation and its rights, conversion code, R33 contract and native receipt, map and DLL hashes, full plugin source tree, Cesium version, CWT asset/server identity without tokens, timestamp, individual residuals by evidence class, and aggregate bias/MAE/RMSE/median/NMAD. Do not persist provider output that the approved terms do not permit.
7. Preserve the existing collision, navigation, line-of-sight, sensor, and RF terrain authority and make no map or native-asset mutation.

Each reference point must include:

`id`, `longitudeDegrees`, `latitudeDegrees`, `heightMeters`, `horizontalCrs`, `verticalCrsOrDatum`, `epoch`, `sourceIdAndHash`, `uncertaintyMeters`, `surfaceClass`, `independentHeldOut`, `authority`, and `licence`.

## Prohibited claims

Until independent control and a valid vertical operation are admitted, do not claim:

- survey-grade terrain;
- verified absolute elevation accuracy;
- Singapore Height Datum alignment;
- that the SLA Vertical Control GeoJSON Z=`0` is a height;
- that an INLIS VCP top-of-mark reduced level is surrounding ground level;
- present-day terrain accuracy from an undated, unstable, regraded, or merely historical borehole ground level;
- that PLD-labelled or PWD `+100 m` material is SHD without documented resolution;
- bare-earth accuracy from the Copernicus DSM;
- that SLA's national mapping programme makes CWT or the current local terrain authoritative without licensed source data and held-out validation;
- collision, navigation, line-of-sight, sensor, or RF authority from CWT;
- reproducibility of provider heights beyond the recorded CWT asset/server identity, because the provider dataset epoch/version is not pinned.

## Evidence required for authoritative promotion

Promotion requires owner- or surveyor-approved, datum-labelled, spatially distributed held-out bare-earth checkpoints with stated uncertainty and suitable epoch; an approved EGM2008 operation where Copernicus is used; an authorized SGEOID09/SHD operation where national control is used; predeclared evidence-class-specific acceptance thresholds; provider persistence permission; and a signed residual report. A VCP datum anchor without a ground-to-mark tie cannot satisfy the bare-earth checkpoint requirement. A historical borehole alone cannot satisfy the present-day requirement. Until licensed current surface evidence and those controls exist, no authoritative present-day terrain accuracy claim is possible; the honest deliverable is a visual terrain reference plus a clearly labelled inter-product and datum-anchor diagnostic.
