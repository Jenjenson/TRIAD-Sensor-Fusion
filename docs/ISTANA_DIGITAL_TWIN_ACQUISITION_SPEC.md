# Istana exterior digital-twin acquisition specification

## Status and non-negotiable boundary

The existing public-reference Main Building mesh and streamed Google/Cesium
context are **rejected as digital-twin source data**. They may remain available
as labelled development fallbacks, but they must never satisfy a digital-twin
release gate. A flat terrain disk, orthophoto draped over a plane, billboard or
camera-facing tree, procedural facade, inferred roof, generated PBR texture, or
provider mesh with visibly flattened geometry is not acceptable final evidence.

"One-to-one" means a dated, survey-controlled exterior digital twin with stated
and tested tolerances. It cannot mean a mathematically exact, timeless copy of a
changing physical site. Every release therefore identifies its reference epoch,
survey datum, source revision, tolerances, permissions, and known occlusions.

The working epoch remains the publicly visible **April 2024 pre-restoration
exterior** until the project owner approves either an as-restored/current epoch
and obtains the corresponding records or a new authorised survey. Do not mix
features from different epochs into one unlabeled release.

Scope is the exterior Main Building, public-facing grounds, and visible 1 km
context required by the simulation. Restricted interiors, security systems,
surveillance equipment, protected routes, access controls, and concealed
infrastructure are excluded. Presidential insignia and other protected marks
require separate written permission before reproduction.

## Required authority packages

No release can pass without all packages below. Each package must have written
redistribution/use terms appropriate to the intended private team repository
and simulation deployment.

| Package | Preferred authority | Minimum deliverables | Final use |
| --- | --- | --- | --- |
| Main Building record geometry | President's Office/facilities owner or owner-authorised surveyor | As-built BIM/IFC or registered terrestrial-laser-scan point cloud; surveyed control network; roof and facade coverage; dimensional report | Authoritative building shell, architectural detail, registration and error checks |
| Immediate grounds | Owner-authorised terrestrial/mobile scan and survey | Classified point cloud, breaklines, kerbs, stairs, walls, fountains, paved areas, finished-ground DTM, survey control | True grade and hardscape; no flat ground plane |
| 1 km terrain and surface | SLA licensed 3D mapping | DTM, DSM, orthophoto and metadata at the best available currency/resolution; SVY21 control information | Terrain, roofs/canopy cross-check and colour reference |
| 1 km buildings/roads | SLA licensed data or a specifically licensed 3D city product | Building geometry/outlines with height/LoD metadata; road and hardscape geometry; revision date | Surrounding structures and collision |
| Vegetation | Licensed tree inventory plus LiDAR/mobile scan | Per-tree position, species where available, height, trunk diameter, crown width/height; classified canopy/trunk points | Real 3D trunks and crowns; no billboards |
| Photometric material capture | Owner-authorised calibrated photography | RAW photographs, colour chart and grey card frames, lens profiles, exposure/lighting log, scale targets, cross-polarised surface sets where permitted | Calibrated albedo/roughness/normal/displacement and visual validation |
| Independent QA evidence | Surveyor and visual-QA team | Held-out control points, held-out photographs, cloud-to-mesh report, photo-overlay report | Evidence that the model meets the declared tolerances |

Singapore Land Authority describes itself as the trusted centre for
authoritative government geospatial data and lists licensed building, road,
cadastral and address products. SLA also documents 3D products including LiDAR
scans, orthophotos, DSM and DTM. Availability, area, currency, precision,
delivery format, price and redistribution rights must be confirmed in writing
for this project; a public web description is not itself a data licence.

OpenStreetMap, URA indicative footprints, OneMap display/API data, 30 m SRTM,
and streamed Google/Cesium content may be used only for discovery, coarse
cross-checking or a visibly labelled preview. They cannot fill a missing
authority package or pass a survey-grade release gate.

## Acquisition request

The request to each data owner/vendor must state:

1. project organisation and named responsible person;
2. private simulation/research purpose and whether external collaborators will
   receive source or derivative files;
3. exact 1,000 m geodesic area of interest, centred at the approved Istana Main
   Building control point, with the bounding box supplied only as an envelope;
4. requested reference epoch and maximum acceptable age of each layer;
5. coordinate reference system, vertical datum, geoid model and units;
6. horizontal/vertical accuracy, confidence level and survey method;
7. native formats, classifications, density/resolution and metadata;
8. rights to store in a private GitHub repository, create derivatives, render,
   simulate, share with named contractors, and retain backups;
9. attribution, confidentiality, export, deletion and licence-expiry terms;
10. explicit confirmation that the delivered package omits restricted/security
    information not required by this exterior simulation.

Do not commission drone capture at the Istana without written site and aviation
authority. The Istana's visitor rules restrict access and list UAVs as
prohibited; ordinary visitor photography is not permission to perform a survey
or commercial texture capture.

## Coordinate and provenance contract

Every source file is immutable after receipt and must be recorded in a signed
release manifest with:

- stable dataset and file identifiers;
- byte length and SHA-256 digest;
- supplier, acquisition method, acquisition time and delivery time;
- exact licence/contract identifier and permitted audience;
- reference epoch and temporal-validity notes;
- horizontal CRS EPSG/WKT, vertical datum/geoid, axes, units and transforms;
- stated horizontal/vertical accuracy and confidence;
- sensor, calibration and processing software/version where applicable;
- classification, point density, ground sample distance or LoD;
- derived-file lineage, processing commands and responsible reviewer;
- restricted/redacted status and approved storage locations.

The working geospatial frame is SVY21 / Singapore TM (EPSG:3414) with an
explicit approved vertical datum. WGS84 longitude/latitude is only an exchange
coordinate. Unreal receives a documented local East-South-Up frame derived from
survey control; no hand-entered visual offset may become the authoritative
transform.

Raw licensed/survey files must not be committed automatically. Store them only
where their licence permits, and commit manifests or retrieval instructions
when redistribution is forbidden. Secrets, access tokens, signed URLs and
personal editor settings never enter a manifest or Git history.

## Geometry production contract

### Main Building and hardscape

- Register all scans/BIM to the approved control network before retopology.
- Preserve the record geometry separately from Unreal-optimised derivatives.
- Use watertight, correctly scaled geometry for visible surfaces; retain facade
  reveals, louvres, balustrades, arches, cornices, steps and roof drainage at
  their measured dimensions.
- Use Nanite or reviewed LoDs without changing the surveyed silhouette. Produce
  collision from the real envelope and hardscape, not an oversized slab.
- Fill an occlusion only when supported by another authorised source; otherwise
  mark it unresolved rather than inventing it.

### Terrain and ground

- Build terrain from classified ground points and breaklines in the approved
  vertical datum. Preserve retaining walls, kerbs, stairs, drains and abrupt
  grade changes as geometry.
- Orthophotography may texture the terrain but must never substitute for its
  elevation mesh.
- Blend scan zones and regional DTM with a documented seam method and quantify
  seam residuals. No hidden flat collision disk may pass acceptance.

### Buildings and background

- All visible structures within the 1 km release area require volumetric 3D
  geometry at the licensed source's declared LoD. A facade card, panorama,
  skybox representation of nearby buildings, or flattened provider tile fails.
- Provider-streamed content can remain as an optional outside-AOI backdrop only
  if it is visibly and physically separated from the accepted 1 km dataset.

### Vegetation

- Every tree visible within the accepted area is represented by 3D trunk,
  branches/crown volume and collision appropriate to its range and simulation
  role. Camera-facing cards, crossed planes and 2D impostors are forbidden at
  every acceptance camera and sensor view.
- Place and scale each tree from its inventory/point-cloud record. Use a
  species-appropriate scanned model or procedural 3D reconstruction only when
  the source record identifies the specimen and the generated crown is checked
  against measured height/width.
- Near-field hero vegetation requires real leaf/branch geometry. Far-field
  geometry may use clustered 3D LoDs, but never a flat billboard.

### Materials and colour

- Derive base colour from calibrated RAW imagery, not screenshots, web JPEGs,
  generated textures or baked lighting. Remove illumination from albedo.
- Record chart profile, white balance, exposure, lens and lighting conditions.
- Use measured or vendor-documented physical scale for texel density and
  displacement. Preserve a neutral reference material and lighting scene.
- Validate colour in a colour-managed offline render and Unreal reference
  scene. Do not correct a source mismatch by arbitrary global exposure or tint.

## Quantitative acceptance gates

The supplier may propose tighter tolerances. The following are provisional
maximums and must be replaced by the signed survey specification before final
release:

| Check | Provisional gate |
| --- | ---: |
| Building control/check-point horizontal and vertical RMSE | <= 0.03 m |
| Building cloud-to-mesh 95th percentile on visible surveyed surfaces | <= 0.05 m |
| Immediate grounds/hardscape checkpoint RMSE | <= 0.05 m |
| Regional terrain vertical RMSE inside 1 km | <= 0.15 m, or the licensed source's tighter stated accuracy |
| Surrounding building envelope error | <= 0.20 m, unless the licensed LoD contract states otherwise |
| Surveyed tree stem position | <= 0.20 m |
| Surveyed tree height/crown-diameter error | <= 5% or 0.50 m, whichever is greater |
| Neutral-light material colour against calibrated target | median CIEDE2000 <= 3; 95th percentile <= 6 |
| Held-out photo silhouette/reprojection error | <= 2 pixels at the registered review resolution |
| Data coverage | 100% of required tiles/files present; no unresolved holes in acceptance views |

All coordinate values, residuals and image metrics must be finite. Pass/fail is
computed from held-out evidence that was not used to fit the model. A review
screenshot alone cannot prove geometry, scale, colour or completeness.

## Unreal release gates

A digital-twin map is a new versioned destination and never overwrites
`/Game/Maps/Istana_1km_Context_v2` or the existing authored meshes. The release
validator must fail closed unless:

- the signed manifest and every allowed local file match their hashes;
- the approved epoch, CRS and vertical datum are consistent across layers;
- terrain, hardscape, buildings and vegetation are project-owned 3D assets;
- the rejected procedural/reference Istana mesh is absent from the release;
- no billboard/impostor/flat-ground component is present in the 1 km release;
- material instances reference only approved calibrated source sets;
- the level opens and PIE runs without fog hiding acceptance evidence;
- collision, navigation and sensor line traces use the accepted geometry;
- survey, cloud-to-mesh, colour and held-out-photo reports all pass;
- the reviewer signs the exact Unreal asset-registry and map-package hashes.

No source may be silently substituted when data are missing. The validator must
name the missing authority package and stop.

The implemented intake now separates strict Stage-0 integrity from the
downstream [GeospatialAuthorityV1 semantic gate](ISTANA_GEOSPATIAL_AUTHORITY_V1.md).
That gate decodes and hash-binds the reviewed GeoTIFF/GLB subset, including
terrain coverage and seams plus model accessors, bounds, stable IDs and
topology. Its pass status remains `NON_IMPORTABLE`; the level/import locks stay
closed until every other format, trust root, Unreal package, runtime geometry,
and held-out acceptance requirement above has reviewed evidence.

## Execution order

1. Freeze the reference epoch and intended distribution audience.
2. Obtain written owner/site permission and a data-use decision for protected
   marks; appoint a registered/qualified survey and reality-capture supplier.
3. License SLA/context datasets and obtain the building/grounds authority
   packages.
4. Receive immutable sources, run malware/file checks, hash them and approve
   the manifest without exposing tokens or restricted metadata.
5. Register sources to survey control and produce independent residual reports.
6. Build terrain/hardscape, contextual structures, vegetation, the Main
   Building and calibrated materials as separate versioned layers.
7. Import into a new Unreal digital-twin map; run automated data, geometry,
   material, collision and runtime gates.
8. Validate against held-out survey points and photographs, then obtain owner,
   surveyor, licence and technical sign-off.
9. Publish only the derivative/source subset allowed by every licence and keep
   the release manifest with the build.

Until steps 1-4 are complete, implementation may build and test the ingest
pipeline but must report the digital twin as **BLOCKED ON AUTHORITATIVE DATA**,
not as complete or visually accepted.

The current official-source/access review is in
[the authoritative data source matrix](ISTANA_AUTHORITATIVE_DATA_SOURCE_MATRIX.md).
Project-owner-approved enquiry and RFQ text can be prepared from
[the data request drafts](ISTANA_DATA_REQUEST_DRAFTS.md).
The rejected scene's exact technical failure modes are recorded in
[the current-fidelity audit](ISTANA_CURRENT_FIDELITY_AUDIT.md), and the fixed
CIEDE2000 calculation/evidence contract is under
`unreal/SourceAssets/IstanaDigitalTwin/ColorQA/`.

## Official acquisition references

- [SLA Licensed Data](https://geoworks.sla.gov.sg/sla-products/licensed-data/)
- [SLA: LiDAR, orthophoto, DSM and DTM used for digital-twin work](https://www.sla.gov.sg/news/press-release/deepening-regional-skills-development-in-land-administration-and-management--and-support-solutions-to-tackle-singapore-s-urban-heat-effect/)
- [Singapore Open Data Licence](https://www.sla.gov.sg/singapore-open-data-licence/)
- [OneMap API documentation](https://www.onemap.gov.sg/apidocs/)
- [SLA contact](https://www.sla.gov.sg/contact-us/)
- [Istana visitor and access FAQ](https://www.istana.gov.sg/visit-and-explore/istana-open-house/faq/)
- [Istana buildings reference](https://www.istana.gov.sg/visit-and-explore/buildings/)
