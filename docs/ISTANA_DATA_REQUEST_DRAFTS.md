# Istana digital-twin data request drafts

These are drafts only. Sending them, accepting terms, ordering data, arranging
site access, or paying a supplier requires the project owner's approval. Do not
attach the repository, credentials, current Cesium cache, or restricted site
information.

Before sending, replace every bracketed field and choose exactly one reference
epoch. The current working choice is the publicly visible April 2024
pre-restoration exterior, specifically **21 April 2024**. This is a working,
unapproved target, not proof that every requested layer existed on that date.

## Send-package checklist

Before any request leaves the project, attach or record all of the following:

- the exact 1 km area-of-interest polygon, centre coordinate, CRS and SHA-256
  digest of the machine-readable boundary file;
- the working target epoch (`2024-04-21_PRE_RESTORATION_PUBLIC_EXTERIOR`) and
  the maximum acceptable age of each requested layer;
- required horizontal, vertical, photometric and botanical tolerances;
- the legal entity, named users, contractors and intended private/public
  audiences;
- every proposed source, derivative, build, cloud, backup and archive storage
  location;
- intended Unreal authoring, packaged-simulation, rendered-output and hosting
  uses, each stated separately;
- budget ceiling, procurement route, data owner, technical approver, licence
  reviewer and security reviewer.

A current survey cannot by itself establish the April 2024 condition. Require a
layer-by-layer acquisition-date register and an explicit 2024-to-current change
register wherever the supplier or owner can provide one.

Verified official request routes as of 26 August 2026:

- SLA/GeoWorks product and enquiry routes:
  <https://www.sla.gov.sg/geospatial/digitised-land-information/> and
  <https://geoworks.sla.gov.sg/contact-us/>;
- Istana owner contact: <https://www.istana.gov.sg/contact-info/>;
- NParks contact and authoritative taxonomy/tree references:
  <https://www.nparks.gov.sg/contact-us>,
  <https://www.nparks.gov.sg/treessg> and
  <https://www.nparks.gov.sg/florafaunaweb/species-search>;
- SVY21 and SGEOID09 reference definitions:
  <https://app.sla.gov.sg/sirent/About/PlaneCoordinateSystem> and
  <https://app.sla.gov.sg/sirent/About/SGEOID09>;
- Singapore Open Data Licence for explicitly covered data.gov.sg datasets:
  <https://data.gov.sg/open-data-licence>.

Google Maps, Google Earth, Street View, satellite tiles and Elevation API output
are orientation references only for this project. Standard Google terms do not
authorise tracing/baking roads or buildings, creating 3D models from imagery,
storing/redistributing tiles, or making terrain from Elevation API data. See
<https://cloud.google.com/maps-platform/terms>,
<https://about.google/brand-resource-center/products-and-services/geo-guidelines/>
and <https://developers.google.com/maps/faq>. OneMap is likewise a live/service
reference unless a specific dataset licence or written permission expressly
allows offline derivative use; do not bake its basemap tiles into Unreal.

## SLA / GeoWorks geospatial-data enquiry

**Subject:** Licensed 3D geospatial data enquiry — 1 km exterior digital-twin
study around the Istana Main Building

Hello,

I am enquiring on behalf of **[organisation]** about authoritative geospatial
data for a private **[research/simulation]** project. The area of interest is a
geodesic circle of 1,000 metres around the Istana Main Building in Singapore.
The project models exterior terrain, buildings, roads and vegetation only. It
does not require interiors, security systems, restricted access information or
concealed infrastructure.

Could you advise whether the following datasets, preferably acquired on or as
close as possible to **21 April 2024**, can be licensed for this area and
purpose? Please also identify newer or older coverage where it is the only
authoritative source.

- classified LiDAR point cloud, including ground and vegetation classes;
- digital terrain model and digital surface model;
- orthophotography with acquisition date and ground sample distance;
- 3D building geometry or building outline/height/LoD data;
- road, kerb and hardscape geometry;
- any available tree inventory or 3D vegetation data.

For each available product, please provide or confirm:

1. acquisition/reference date and update cycle;
2. horizontal CRS, vertical datum/geoid, axes and units;
3. horizontal/vertical accuracy and confidence level;
4. point density, raster resolution, classification or LoD;
5. native delivery format and approximate volume for the area;
6. price, minimum order and lead time;
7. rights to transform the raw data and create Unreal Engine derivatives;
8. rights to distribute packaged simulations and still/video renders;
9. rights to host the simulation or rendered output for **[private/public]**
   users;
10. rights to store raw data, derivatives and backups at the locations in the
    attached data-handling plan, and to share them with **[number]** named
    collaborators/contractors;
11. attribution, confidentiality, retention, deletion and licence-expiry terms;
12. whether any part of the requested area is unavailable or subject to
    additional approval.

The working release epoch is **21 April 2024, pre-restoration**, subject to owner
approval. Please include each source acquisition date and flag every material
epoch mismatch. If a different product, historical refresh or custom extraction
would be more appropriate, please recommend it.

Thank you,

**[name]**<br>
**[role / organisation]**<br>
**[business email and telephone]**

## President's Office / Istana owner-authorisation enquiry

**Subject:** Request to discuss authorised exterior digital-twin source data and
survey access

Hello,

I am writing on behalf of **[organisation]** regarding a private
**[research/simulation]** project that requires a high-fidelity exterior digital
twin of the Istana Main Building and its public-facing grounds. We will not
represent an approximate model as an as-built record and will not collect,
model, request or distribute interiors, security infrastructure, restricted
routes, surveillance equipment, access-control details or concealed systems.

We would like to ask whether the appropriate office is willing to discuss one
or both of the following:

- access to approved exterior as-built/BIM/CAD or previously commissioned
  terrestrial-scan data, with written terms for private simulation use; or
- tightly controlled access for an appointed qualified survey/reality-capture
  supplier to capture only an approved exterior scope using terrestrial
  instruments and calibrated photography.

We understand that visitor access and ordinary visitor photography do not
authorise a survey. We will not use a UAV and will comply with all site,
security, privacy, heritage and operational requirements. A proposed method,
equipment list, personnel list, capture positions, data handling plan and
redaction workflow would be submitted for approval before any work.

Could you identify the correct facilities, heritage, security and intellectual-
property contacts and advise what application, procurement, confidentiality or
review process is required? We also request guidance on:

1. the permitted reference epoch, especially while restoration is ongoing;
2. whether exterior record geometry or existing survey data can be supplied;
3. permitted capture zones, dates, instruments and photography;
4. review/redaction requirements before data leave the secure workflow;
5. rights to create and share a private Unreal Engine derivative with **[number]**
   named collaborators;
6. restrictions on the Presidential crest, standard or other protected marks;
7. required retention, storage, access-control and deletion measures;
8. the appropriate restoration consultant or record-data custodian who can
   identify changes between 21 April 2024 and the present;
9. the nominated security/licensing reviewer and whether the owner can sponsor
   necessary SLA, NParks, NHB or BCA enquiries;
10. whether a redacted, exterior-only 2024-to-current change register can be
    supplied without revealing restricted operations or systems.

No site visit or data collection will occur without written approval.

Thank you,

**[name]**<br>
**[role / organisation]**<br>
**[business email and telephone]**

## NParks historical tree-data enquiry

**Subject:** Historical exterior tree-inventory enquiry — 1 km Istana study area

Hello,

I am enquiring on behalf of **[organisation]** about authoritative, non-sensitive
tree data for a private exterior **[research/simulation]** project. The attached
area is a 1 km geodesic circle around the Istana Main Building. The working
reference date is **21 April 2024**.

Could you advise whether NParks can supply or license a historical Tree Registry,
RTMS or equivalent extract for that area and reference date? We seek only
exterior botanical/geometry attributes that are approved for release, such as:

- stable record identifier and observation date;
- taxon and common name with identification confidence;
- SVY21 position and positional accuracy;
- height, girth/diameter, crown dimensions and condition where recorded;
- planting/removal/status dates or a redacted change history;
- Heritage Tree or conservation status already approved for publication.

Please state dataset lineage, completeness, update cadence, coordinate reference
system, accuracy, price/lead time, attribution, permitted contractor access and
rights to create and distribute an Unreal Engine derivative. We will not infer
unreleased trees from public records or request restricted site/security data.
Owner sponsorship will be supplied if required.

Thank you,

**[name]**<br>
**[role / organisation]**<br>
**[business email and telephone]**

## Species-authenticated vegetation and turf asset RFQ

Issue this RFQ only after an authorised arboricultural inventory identifies the
taxa and landscape condition to reproduce. Generic visual resemblance is not a
species claim.

Please quote licensed, production-ready tropical vegetation assets for the
approved taxon list and target viewing distances. For each tree/shrub/groundcover
taxon, require:

- botanical identification provenance and confidence;
- multiple age, height, crown/form, health and seasonal variants;
- full 3D hero geometry plus documented LoD/Nanite strategy, wind rigging and
  physically plausible leaf transmission/two-sided shading;
- calibrated PBR textures, real-world dimensions and pivot conventions;
- Unreal Editor and packaged-build rights for the named users, contractors,
  platforms, hosting model and rendered outputs;
- source/derivative storage, backup, modification, attribution and licence-term
  rights stated separately.

For turf, quote a site-calibrated material/geometry set only after the owner or
landscape consultant confirms species/cultivar, mowing height, blade density,
thatch/soil visibility, irrigation/wetness state, colour target and maintenance
condition. Require measured macro-variation, edge/bed transitions, close-range
blade geometry and performance budgets at the approved camera heights.

### Candidate commercial shortlist — approval required

The following products are leads for a controlled evaluation, not approved
sources and not evidence that a taxon occurs at the Istana. Do not purchase,
download, accept a licence, or import one until the legal entity, licence tier,
named users and botanical inventory have been approved.

1. **Low-cost look-development trial:** Maxtree's *Plant Models Samples Vol 1*
   currently lists Unreal/FBX delivery, *Albizia saman* (Rain tree),
   *Cynodon dactylon* (Bermuda grass), *Alpinia zerumbet*, *Aglaonema* and
   other plants. It is useful for close-range silhouette, turf-density and
   memory-budget tests, but its high-poly variants can reach millions of
   polygons and the vendor states that FBX materials need manual setup. The
   vendor also says its Unreal models are not intended for games, so this is an
   evaluation-only candidate for the interactive simulation until a packaged
   performance and licence review passes. Review the attached licence before
   purchase:
   <https://maxtree.org/products/plant-model-samples-vol-1/>.
2. **Candidate named hero tree:** the Fab listing *Nanite Plant Tree —
   Tamarindus indica* advertises nine Unreal variations. Treat the name as a
   vendor claim until checked against the authorised inventory and require a
   technical trial for crown form, wind, translucency and memory:
   <https://www.fab.com/listings/db486629-3868-4b80-8437-0f0227b276f1>.
3. **Generic background diversity only:** Fab's *Realistic Tropical Trees 1*
   lists 33 meshes with three LoDs, wind and colour variation. It may be useful
   outside hero range, but it carries no site-specific species authority:
   <https://www.fab.com/listings/776257f3-b9e2-4b28-bb8f-e4e4fe82b157>.
4. **High-cost/high-memory option:** Fab's photogrammetry-based *Rain Forest
   Pack* advertises more than 120 assets, 4K textures, Nanite, wind and wetness,
   but its own showcase used a high-spec PC with 64 GB RAM. It is a visual
   reference or separately profiled background candidate, not a safe default
   for the current memory-constrained build:
   <https://www.fab.com/listings/03b628b4-955a-447e-b25f-daffaf94327d>.
5. **Missing taxon/custom route:** no reputable ready-made *Horsfieldia irya*
   model has been identified. Verify the taxon with the NParks reference
   (<https://www.nparks.gov.sg/florafaunaweb/flora/2/9/2964>), then commission
   a model from authorised multi-view/tree-measurement reference. Maxtree offers
   a custom-service route, but the scope, exclusivity, delivery time, source
   rights and species evidence must be quoted in writing:
   <https://maxtree.org/custom-service/>.

For Fab candidates, the Standard License summary permits private/commercial
project use, modification, incorporated project distribution and sharing with
project collaborators, but prohibits standalone redistribution; Reference-Only
does not provide the source asset. The full EULA, the selected listing licence
and the buyer's account/entity remain controlling:
<https://www.fab.com/eula?lang=en>. Maxtree licensing varies by user/team/studio
tier and is non-transferable; obtain written confirmation for repository,
contractor, packaged-build, render and backup use before checkout:
<https://maxtree.org/terms-and-conditions/>.

Use a two-stage approval: first buy at most one low-cost evaluation product and
benchmark it in an isolated Unreal project; only then approve production
licences for assets that pass botanical, visual, LoD, shader, wind, collision,
memory and packaged-build tests. Never rename a generic model to a real taxon.

Before checkout or download, the buyer must complete this acquisition record:

- calculate the purchasing entity's gross commercial revenue for the preceding
  12 months and select the Fab Personal tier only at or below USD 100,000, or
  Professional above USD 100,000; see
  <https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab>;
- confirm that the listing supplies the Standard/source asset needed for Unreal
  authoring, not a Reference-Only entitlement;
- record the legal buyer, account, permitted users, exact listing/version,
  engine compatibility, licence/tier, price/tax, order ID and approval ID;
- before accepting terms, save a PDF or immutable snapshot of the listing,
  selected licence/EULA and vendor terms for the procurement record;
- for Maxtree, verify the exact format and software version before downloading:
  its current terms limit a link to five downloads, make backups the buyer's
  responsibility, generally remove cancellation rights after download and do
  not refund incompatibility;
- immediately after authorised download, preserve the untouched master in the
  approved restricted storage, calculate a SHA-256 manifest, archive the
  receipt/licence snapshot, and give working copies only to permitted users.

## Survey / reality-capture request for quotation

**Subject:** RFQ — survey-controlled exterior digital twin, Istana study area

This RFQ must be issued only after the site owner confirms the permitted scope.

Please quote a controlled exterior capture and production workflow for:

- the approved Istana Main Building exterior and immediate public-facing
  hardscape/grounds at centimetre-level accuracy;
- survey control tied to SVY21 / Singapore TM (EPSG:3414) and the owner-approved
  vertical datum;
- registered terrestrial LiDAR and calibrated terrestrial photogrammetry with
  authorised facade/roof coverage, plus an occlusion and unresolved-surfaces
  report rather than an unsupported completeness claim;
- classified ground, hardscape, building and vegetation point clouds;
- record mesh/BIM plus Unreal-optimised Nanite/LoD derivatives;
- cross-/parallel-polarised calibrated RAW material photography with
  grey/spectral references, colour charts, lens profiles, illuminance, scale
  records and capture logs;
- de-lit, scale-correct albedo, roughness, normal and displacement deliverables,
  with declared colour/reflectance tolerances and validation evidence;
- per-tree stem position, height and crown dimensions where in scope;
- independent held-out control/check points and cloud-to-mesh residual report;
- complete metadata, processing lineage, native sources and SHA-256 file
  manifest.

The survey will document the capture date, not automatically reconstruct the
April 2024 condition. Quote the current capture and any defensible historical
reconciliation/change-register work as separate deliverables.

If radio-frequency simulation is approved, quote a separate engineering model:
watertight solids where intended, true exterior openings, measured assembly
layer stacks and thicknesses, surface roughness, and per-surface material
identifiers. For the specified **[frequency bands]**, include measured or
traceable frequency-dependent complex permittivity/conductivity or loss tangent,
measurement provenance and uncertainty, plus incidence-angle and polarisation
validation. Exclude concealed infrastructure and all security systems. A visual
render mesh is not acceptable as RF authority; if those electromagnetic inputs
cannot be supplied, reflection and transmission must remain explicitly
unvalidated.

The proposal must state achievable horizontal/vertical accuracy, confidence,
coverage/occlusion handling, equipment, qualifications, processing software,
formats, schedule, cost, security controls, insurance, IP ownership, permitted
derivative/contractor use and deletion/retention obligations. Do not propose UAV
capture unless the owner independently authorises it in writing.

## Decision record before procurement

Record these answers next to the approved request:

- organisation/legal entity:
- named owner and technical approver:
- owner authorisation ID/date:
- licence reviewer and security reviewer:
- exact AOI file/hash and CRS:
- external/private-repository collaborator count:
- working reference epoch and approval status:
- maximum layer age and per-layer acquisition dates:
- geometric, photometric and botanical tolerances:
- exterior-only scope confirmed:
- intended simulation and publication use:
- source-data redistribution required:
- derivative redistribution required:
- approved source/derivative/build/backup/compute locations:
- budget ceiling and procurement route:
- target delivery date:
- approved contacts:
- confidentiality classification:
- owner/security review required before release:
- RF scope, target bands and authorised deliverables (or explicitly out of scope):
