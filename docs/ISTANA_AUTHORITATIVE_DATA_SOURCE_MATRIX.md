# Istana authoritative data source matrix

Research checked against official Singapore sources on 28 August 2026.

## Decision

No official public download currently provides a one-to-one exterior BIM,
survey mesh, terrestrial LiDAR scan, calibrated material set, or complete tree
inventory for the Istana and its 1 km surroundings.

## Implemented public-reference acquisition

On 28 August 2026 the repository pipeline downloaded and SHA-256 bound all
eleven default rows from the public dataset matrix below, then clipped each
source to the exact 1,000 m geodesic AOI. The raw external cache contains
`467,282,059` bytes; the WGS84 derivatives contain `13,368,634` bytes.

- acquisition manifest: `12,560` bytes, SHA-256
  `322FBBEF4C6786F5B52A7FA9EB41A7DD06D195BDDEDDF3D548C9102B3D8DAC7B`;
- AOI derivative manifest: `14,493` bytes, SHA-256
  `95D4DFA4DD62D552561DB2D8875EE879AF4129A6BE30905E4B1B7FCDE59004C7`;
- external root:
  `D:\triad\TRIAD\Saved\TRIAD\IstanaReferences\PublicContext_20260828`;
- source and derivative pipeline:
  `unreal/SourceAssets/IstanaPublicViewExploreV5B/Surroundings`.

The build records 9 Heritage Tree points, 45 Master Plan 2019 building
features, 46 amendment features, 471 conserved-building features, 84 road
features, 101 height-control polygons, 1,608 land-use polygons, and 2,673
cadastral parcels inside or intersecting the AOI. These are lawful planning,
control, and species-position references. They remain explicitly unsuitable as
measured building heights, facade/roof geometry, terrain, a complete tree
inventory, apertures, material composition, or RF truth.

The public downloads below can establish a lawful planning and control
baseline. They cannot prove facade dimensions, roof geometry, terrain height,
tree form, brick scale, paint colour, or current restoration state. A
survey-controlled digital twin requires, in this order:

1. written authorisation from the Office of the President/Istana;
2. owner/restoration records and NHB monument drawings;
3. licensed SLA National 3D Mapping data for the 1 km context;
4. a licensed NParks tree-registry extract;
5. an owner-authorised terrestrial survey and colour-managed material capture;
6. written rights for every intended source, derivative, collaborator, hosting
   environment, backup and release.

> **No unauthorised capture.** The Istana is a security-sensitive protected
> location. Do not fly a drone, perform terrestrial/mobile laser scanning,
> conduct photogrammetry, set up tripods or targets, sample materials, or
> photograph restricted areas without specific written owner and regulatory
> permission. Do not place raw or derived controlled data in GitHub, even a
> private repository, until the applicable licence and security approval
> explicitly allow that audience and storage location.

## Status legend

| Status | Meaning |
| --- | --- |
| **PUBLIC-DOWNLOAD** | The official file can be downloaded now under the Singapore Open Data Licence. |
| **PUBLIC-SERVICE** | An official API or tile service is available now, but it is not a downloadable 3D source package. |
| **LICENCE/QUOTE** | The authority confirms the product exists, but the current official page does not publish an open download, delivery format, price or redistribution grant. |
| **OWNER-AUTHORISED** | Access is controlled by the property owner/occupier or an owner-authorised consultant. |
| **ARCHIVE/PERMISSION** | A historical record exists; viewing, copying and reuse depend on the record-specific access and copyright conditions. |

## Immediately downloadable public data

All rows in this section are useful for control, discovery, coverage checks and
coarse context. None is a substitute for current surveyed 3D geometry.

### Exact download method

1. Open the linked data.gov.sg dataset page and use **Download GEOJSON**.
2. For an automated GeoJSON download, call:

       GET https://api-open.data.gov.sg/v1/public/api/datasets/{dataset_id}/poll-download

3. Read the returned data.url and download that object. For non-CSV files such
   as GeoJSON, the official guide says the initiate-download call may be
   skipped. Public testing does not require an API key; production use should
   use a key for higher limits.
4. Preserve the unmodified source file, source metadata, access date, hash and
   licence notice. Clip a derivative to the exact 1,000 m geodesic study area;
   do not replace the circular scope with its bounding square.

Official instructions: [data.gov.sg download-dataset API guide](https://guide.data.gov.sg/developer-guide/dataset-apis/download-dataset).

### Public dataset matrix

| Authority and official dataset | Dataset ID | Published format/size | Appropriate use | Critical limit |
| --- | --- | --- | --- | --- |
| URA [Amendment to Master Plan 2019 Building layer](https://data.gov.sg/datasets/d_ee3bd86e1334fbc58b520cd987090e54/view) | **d_ee3bd86e1334fbc58b520cd987090e54** | GeoJSON, about 50.2 MB; data from Sep 2025 | Most recent public URA amendment footprint cross-check found in this review | Indicative; the page says it does not reflect all buildings islandwide; no heights or facade/roof detail |
| URA [Master Plan 2019 Building layer](https://data.gov.sg/datasets/d_e8e3249d4433845bdd8034ae44329d9e/view) | **d_e8e3249d4433845bdd8034ae44329d9e** | GeoJSON, about 50.2 MB; source epoch Dec 2019 | Approved-plan footprint baseline | Old and indicative; no actual height, roof, facade or current-restoration state |
| URA [Master Plan 2025 SDCP Conserved Building layer](https://data.gov.sg/datasets/d_a328030a7344573a34fd92808cb5713c/view) | **d_a328030a7344573a34fd92808cb5713c** | GeoJSON, about 5.7 MB; data from Dec 2025 | Monument/conserved-building footprint cross-check | Conserved-building control layer only, not an as-built survey |
| URA [Master Plan 2025 Land Use layer](https://data.gov.sg/datasets/d_a8c3546b26712e35021f3a681d0353ae/view) | **d_a8c3546b26712e35021f3a681d0353ae** | GeoJSON, about 180.8 MB | Zoning and land-use context | Planning designation, not surface geometry |
| URA [Master Plan 2025 Building Height Control](https://data.gov.sg/datasets/d_0720d955bce304046d173d9e2a309652/view) | **d_0720d955bce304046d173d9e2a309652** | GeoJSON, about 4.2 MB | Height-control policy check | Control envelope, not measured building height |
| URA [Master Plan 2019 Road Graphic](https://data.gov.sg/datasets/d_95a29fbb10cf94a3c263d33861d7b6c6/view) | **d_95a29fbb10cf94a3c263d33861d7b6c6** | GeoJSON, about 5.6 MB | Indicative kerb/divider and road-layout cross-check | Planning graphic, not a surveyed road surface |
| SLA [Cadastral Land Parcel](https://data.gov.sg/datasets/d_e7395d743076a2bcc487b0d12b9bf33b/view) | **d_e7395d743076a2bcc487b0d12b9bf33b** | GeoJSON, about 138 MB | Parcel/boundary control and AOI QA | Does not locate current facades, trees or hardscape; do not treat a visual export as a site survey |
| SLA [National Map Line](https://data.gov.sg/datasets/d_10480c0b59e65663dfae1028ff4aa8bb/view) | **d_10480c0b59e65663dfae1028ff4aa8bb** | GeoJSON, about 604.6 MB | Major roads, boundaries and available contour-line context | Contour lines are not a DTM or a dense terrain surface |
| SLA [National Map Polygon](https://data.gov.sg/datasets/d_29f066d67df3eae91df8a42f443863c8/view) | **d_29f066d67df3eae91df8a42f443863c8** | GeoJSON, about 7.7 MB | Coastal, recreation and hydrographic context | 2D thematic polygons only |
| LTA [Lane Marking](https://data.gov.sg/datasets/d_fa71cc0c433275f4d2b0133358cc4fbf/view) | **d_fa71cc0c433275f4d2b0133358cc4fbf** | GeoJSON, about 276.3 MB | Public-road marking/layout cross-check | No road elevation, pavement material or guarantee of complete private-ground coverage |
| NParks [Heritage Trees](https://data.gov.sg/datasets/d_644ff187b6d14d6316f47284a4a6c81f/view) | **d_644ff187b6d14d6316f47284a4a6c81f** | GeoJSON, about 146.5 KB | Named heritage-tree position, species and description where present | A very small recognised subset, not the Tree Registry or a 1 km inventory |
| NParks [Tree Conservation Area](https://data.gov.sg/datasets/d_52b9eabe398353bd6acd9aee15b13f72/view) | **d_52b9eabe398353bd6acd9aee15b13f72** | GeoJSON | Regulatory-area context | Polygon only; no individual tree locations or dimensions |
| NParks [Parks and Nature Reserves](https://data.gov.sg/datasets/d_77d7ec97be83d44f61b85454f844382f/view) | **d_77d7ec97be83d44f61b85454f844382f** | GeoJSON, about 2.8 MB | Managed-green-area context | Indicative areas, not vegetation geometry |

Published sizes and coverage dates can change. Re-read the official metadata
and record the exact revision at acquisition time.

### Rights and cost for data.gov.sg files

The [Singapore Open Data Licence version 1.0](https://data.gov.sg/open-data-licence)
grants a worldwide, perpetual, royalty-free, non-exclusive right to access,
download, copy, distribute, transmit, modify and adapt covered datasets for
commercial or non-commercial use. The linked dataset pages describe the files
as free downloads.

Every product using the data must conspicuously acknowledge the dataset name,
access date, source and current licence URL. The licence does **not** grant
rights over personal data, third-party rights, patents, trademarks or design
rights; it does not permit implied government endorsement; and it limits
downstream sublicensing. Keep the attribution and rights record with every
derived layer.

## Public services that are not downloadable 3D assets

| Service | Available now | Formats | Rights and practical limit |
| --- | --- | --- | --- |
| [OneMap APIs](https://www.onemap.gov.sg/apidocs/) | Search, routing and thematic/map queries | REST/JSON, as documented per endpoint | API datasets are governed by the Singapore Open Data Licence plus [OneMap API Terms](https://www.onemap.gov.sg/legal/apitermsofservice.html). Endpoint-specific limits still apply. No official API documented here supplies an exportable one-to-one Istana BIM or city mesh. |
| [OneMap basemap services](https://www.onemap.gov.sg/docs/maps/index.html) | Authoritative national 2D basemap | XYZ, HD XYZ, TileJSON, Mapbox Style, WMS and WMTS; EPSG:3857 | The OneMap logo and attribution are mandatory. Use as a map/reference layer, not as a facade or terrain texture source. |
| OneMap/OneMap3D website | Interactive viewing | Web display | The [site Terms of Use](https://www.onemap.gov.sg/legal/termsofuse.html) restrict archiving, redistribution, modification and incorporation of site material outside the granted terms. Do not scrape, screen-capture or reverse-engineer OneMap3D into project assets. |
| [TreesSG](https://www.nparks.gov.sg/treessg) | Public interactive tree lookup | Web display | Useful for manual confirmation, but no official bulk download or redistribution licence for the full registry was found. Do not scrape it. Request an extract from NParks instead. |

## Licensed, owner-controlled and permission-only matrix

| Source/status | Fidelity-critical material | Exact access path | Published formats/cost | Rights and security position |
| --- | --- | --- | --- | --- |
| Office of the President/Istana — **OWNER-AUTHORISED** | Current restoration federated BIM/as-builts; owner survey/laser scan; exterior elevations, sections and details; landscape survey; material, paint and finish schedules; approved photography/capture access | Submit a project request to **istana_feedback@istana.gov.sg**, telephone **8720 6021**, Office of the President, Orchard Road, Singapore 238823. Official details: [Istana contact](https://www.istana.gov.sg/contact-info/) | No public model format, fee or release process is published | Written owner permission must identify permitted capture methods, areas, people, storage, derivatives, collaborators and publication. Request a security-reviewed/redacted package. |
| NHB Preservation of Sites and Monuments — **OWNER-AUTHORISED** | Monument-specific Preservation Guidelines. Volume 2 contains measured drawings and annotated details required to be retained | The official [monument-owner guidance](https://www.nhb.gov.sg/what-we-do/our-work/preserve-our-stories-treasures-and-places/national-monuments-and-marked-historic-sites/preservation-of-sites-and-monuments/upkeep/general-information-for-monument-owners) says the owner/occupier can request the guidelines. With owner sponsorship, write to **NHB_NationalMonuments@nhb.gov.sg**. The [URA Istana conservation record](https://www.ura.gov.sg/conservation/find-a-building/conservation-portal/024-istnsrtm/) gives the same PSM contact. | No public download, delivery format or price is published | These are controlled monument records, not open data. Obtain written permission for use, digitisation, derived geometry, private-team sharing and retention. |
| SLA National 3D Mapping Programme — **LICENCE/QUOTE** | LiDAR, orthophoto, DSM, DTM, and high-resolution 3D representations of buildings, vegetation and infrastructure | Send the 1 km AOI polygon, project purpose, organisation, named users and full rights request to **geoworks@sla.gov.sg**, **+65 6323 9829**, GeoWorks, 55 Newton Road, Revenue House Level 14. Official product evidence: [SLA LiDAR/orthophoto/DSM/DTM release](https://www.sla.gov.sg/news/press-release/deepening-regional-skills-development-in-land-administration-and-management--and-support-solutions-to-tackle-singapore-s-urban-heat-effect/) and [National 3D Mapping use cases](https://geospatial.sla.gov.sg/geospatial-for-good/sustainability-use-cases/). | The current official pages do not publish delivery formats or prices; obtain a written quote and product specification | Government has described private-sector 3D Singapore Sandbox access as [restricted](https://www.mlaw.gov.sg/news/parliamentary-speeches/response-speech-by-senior-minister-of-state-for-law-edwin-tong-at-the-committee-of-supply-debate-2020-%28ministry%20of%20law%29/). Sandbox/view access is not an export or redistribution licence. Request explicit raw-data, derivative, render, simulation, hosting, contractor, backup, retention and deletion rights. Expect review/redaction around the Istana. |
| SLA Digitised Land Information — **LICENCE/QUOTE** | Licensed Building Outline, Road Network, Digitised Cadastral, Address Point and Street Directory products | Apply for a business licence through [SLA Digitised Land Information](https://www.sla.gov.sg/geospatial/digitised-land-information/) or the GeoWorks contact above; attach the 1 km AOI, purpose, user count and distribution model | Official formats: **DGN, DXF, DMP or SHP**. No current public price is shown | Contract controls source and derivative distribution. Ask whether private GitHub, Unreal packaging, collaborator access and generated meshes are permitted. |
| NParks Tree Registry/RTMS — **LICENCE/REQUEST** | Per-tree geospatial location and available physical parameters; request species, height, girth/trunk diameter, crown dimensions/geometry, survey epoch and accuracy | Submit a scoped data request through [NParks Contact Us](https://www.nparks.gov.sg/contact-us), with the 1 km AOI, owner authorisation and intended audience. NParks confirms that its Remote Tree Management System extracts geospatial locations and physical parameters from LiDAR and updates its Tree Registry in the [NParks Sustainability Report 2024/2025](https://www.nparks.gov.sg/docs/default-source/resources/2025/nparks-2025_sustainability-report.pdf). | No public bulk format, price or licence was found | Ask for a security-reviewed external-use export and explicit derivative/redistribution rights. Operational condition/risk attributes may be withheld. The public Heritage Trees file is not an adequate substitute. |
| BCA approved building records — **OWNER-AUTHORISED** | Approved architectural and structural drawings held by BCA; not MEP records and not necessarily current as-built BIM | The [BCA Legal Search](https://www.bca.gov.sg/legalsearch/LegalSearchapp.aspx) permits only the current owner, prospective purchaser or MCST chair to apply, or to authorise a consultant. Supply the property address, documentary proof and owner authorisation. BCA says a search outcome is generally emailed in five working days, after which selected records are copied in person. | Search **S$22/address**; copying **S$28/set**; printing **S$4/A2 sheet**. The portal describes printed A2 extracts, not BIM/IFC delivery. | Owner eligibility/authorisation is mandatory. Records are supplied as-is without an accuracy warranty. Obtain separate permission before digitising or redistributing them. |
| National Archives of Singapore — **ARCHIVE/PERMISSION** | Historical plans and photographs for chronology and missing historical details | Search [Archives Online](https://www.nas.gov.sg/archivesonline/), use **Request to View** or **Request a Copy**, and state the declared use. The exact [MAIN GATE TO ISTANA](https://www.nas.gov.sg/archivesonline/maps_building_plans/record-details/43d0bff3-2dcd-11ea-a1ea-001a4a5ba61b) record is accession **PWD021_268_268**, dated 1971, with 3–5 working-day request processing. The [Archives Reading Room](https://corporate.nas.gov.sg/archives-reading-room/about-archives-reading-room/) asks for requests at least five days before visiting. | Item-specific. The gate record does not publish a reproduction price on its record page. | The gate record says viewing, use and reproduction require written permission from the copyright owner/source. Archival material is historical reference, not evidence of the current/restored state. |
| Owner-authorised survey and material capture — **OWNER-AUTHORISED/PROCUREMENT** | Survey-control network, terrestrial/mobile LiDAR, close-range photogrammetry where authorised, calibrated exterior photography, reflectance/colour measurements and held-out QA | Procure only after the owner defines allowed areas/methods and the authorities clear the work. Require registration to Singapore survey control and an independent accuracy report. | Commercial quote. Request open/native deliverables where permitted: E57 and LAS/LAZ point clouds; IFC plus native BIM; GeoTIFF DTM/DSM/orthophoto; DXF/DGN breaklines; OBJ/FBX/glTF or 3D Tiles meshes; RAW photographs; 16-bit TIFF/EXR texture sets; CSV spectral/colour readings; calibration and processing reports. These are requested formats, not formats promised by an authority. | Contract must assign or license raw capture and derivatives for the exact team, storage and release. It must prohibit capture and disclosure of restricted interiors, security systems, surveillance equipment, protected routes, access controls and concealed infrastructure. |

## Exact requests to make

### 1. Owner/restoration package

Ask the Office of the President to nominate the data owner and restoration
consultant, then request:

- the approved reference epoch: pre-restoration, current works, or
  as-restored;
- current restoration BIM and as-built/record survey, including the exterior
  shell, roof, verandahs, facade details and visible hardscape;
- the current control network and survey report in SVY21 and Singapore Height
  Datum;
- landscape/topographic survey and visible tree schedule;
- paint codes, masonry/stone/timber/metal specifications, finish samples and
  approved colour/material schedules;
- a written site-access and capture method statement;
- a security-reviewed list of features that must be excluded or generalised;
- written rights for a private team repository, named external collaborators,
  derivative meshes/textures, Unreal builds, rendered output, backups,
  retention and eventual deletion.

The [Istana site](https://www.istana.gov.sg/) states that the Main Building is
currently closed for restoration until further notice. The restoration project
team is therefore the most plausible holder of current survey/BIM/material
records; old public references should not be mixed into an unlabeled current
model.

### 2. SLA 1 km licensed extract

Ask GeoWorks for a quotation and feasibility decision for:

- classified LiDAR point cloud;
- orthophoto;
- DTM and DSM;
- textured or attributed 3D building, vegetation and infrastructure products;
- the highest available acquisition epoch covering the complete study area;
- point density, ground sample distance, LoD, classification, occlusions,
  horizontal/vertical accuracy and confidence;
- horizontal CRS, vertical datum/geoid and transformation metadata;
- native/delivery formats and tile scheme;
- export, derivative, render, simulation, hosting, collaborator, backup,
  retention, deletion and attribution terms;
- explicit confirmation of any Istana redaction or area that cannot be
  supplied.

SLA says its National 3D Mapping Programme has maintained authoritative 3D maps
since 2012 and conducted a third nationwide aerial-mapping refresh in 2024:
[Geo Connect Asia 2024 opening address](https://geospatial.sla.gov.sg/opening-address-at-geo-connect-asia-2024/).
The authority does not claim on these public pages that the regional product
contains facade-level or centimetre-level building detail, so it must not
replace the owner-authorised close survey.

### 3. NParks 1 km tree extract

Ask NParks for every releasable managed-tree record intersecting the AOI,
including:

- stable tree identifier;
- easting, northing and vertical reference if available;
- species/taxon;
- height, trunk/girth measure, crown diameter or crown geometry;
- measurement method and survey date;
- uncertainty/accuracy and source lineage;
- conditions for deriving 3D vegetation and sharing it with named
  collaborators.

Do not request or retain operational risk, inspection or security details that
are unnecessary for visual simulation.

### 4. Survey/material-capture procurement

The owner-approved method statement should require:

- survey control tied to SVY21 / Singapore TM and Singapore Height Datum;
- independent held-out checkpoints and a signed residual report;
- full visible roof, facade, hardscape and terrain coverage without scanning
  restricted areas;
- calibrated RAW photography with grey card and colour target, documented
  lens/exposure/lighting, and cross-polarised capture only where allowed;
- non-contact material measurement unless the owner separately approves
  sampling;
- capture and processing logs, immutable source hashes and a rights manifest;
- a pre-agreed redaction review before any project upload or release.

## Photography, imagery and material rights

- The [Istana Open House FAQ](https://www.istana.gov.sg/visit-and-explore/istana-open-house/faq/)
  says ordinary photography/video at that public event is generally allowed
  except in restricted areas and subject to onsite instructions. It also lists
  tripods as prohibited and says the Main Building is closed for extensive
  restoration. This is not permission for commercial photogrammetry, laser
  scanning, target placement, material sampling or redistribution.
- The [Istana Terms of Use](https://www.istana.gov.sg/terms-of-use/) prohibit
  reproducing, modifying or redistributing website images without prior
  written permission. Official web JPEGs must not be turned into textures.
- NHB's [Roots Terms of Use](https://www.roots.gov.sg/terms-of-use) require
  prior written permission to reproduce or adapt content. A request must state
  the exact content, nature and timeframe of use and user identity; the listed
  address is **NHB_Feedback@nhb.gov.sg**.
- NAS records remain subject to each record's access/copyright condition.
  Receiving a reference copy does not establish a redistributable texture
  licence.

Exact real-life colour cannot be established from browser screenshots or
uncalibrated public photographs. Use owner material schedules plus an
authorised colour-managed survey, and record the reference epoch, illuminant,
instrument/profile and uncertainty.

## Security and aviation gate

The [Singapore Police Force guidance for Protected Areas and Protected
Places](https://www.police.gov.sg/Knowledge-Hub/Infrastructure-Protection/Protected-Areas-and-Protected-Places)
states that photography/videography of a PA/PP is not allowed without the
owner's permission and directs photographers to approach the owner. That gate
does not itself create permission for this project; do not assume that placing
a camera outside or above the premises removes the need for an owner and
regulatory decision.

CAAS has specifically identified the Istana as a security-sensitive Protected
Area in its [official unmanned-aircraft release](https://www.caas.gov.sg/resources/media-and-publication/newsroom/caas-facilitates-permit-applications-for-unmanned-aircraft-operations/).
The current legal/operational boundary must be rechecked in OneMap and with the
authorities before every proposed flight. Current [CAAS requirements](https://www.caas.gov.sg/unmanned-aircraft/requirements-for-flying-ua/)
also require an SPF permit for flight in a protected area and prohibit UA
activity in prohibited areas.

For a business/non-recreational drone survey, the current [CAAS operator and
activity permit page](https://www.caas.gov.sg/unmanned-aircraft/operator-and-activity-permits/)
publishes the following general charges and process:

| Item | Published charge |
| --- | ---: |
| Operator Permit, first UA type | S$700, plus S$200 CFMS subscription before grant |
| Additional different UA type | S$500 |
| Class 1 Activity Permit, ordinary activity | S$120 |
| Repeat/variation of the same ordinary activity | S$45 |
| CFMS UA tracker | S$265, subject to prevailing GST |

CAAS lists average processing times of ten working days for an Operator Permit,
five for an Activity Permit and seven for a tracker, subject to completeness
and complexity. Application is through eSOMS using Singpass or Corppass:
prepare the required supporting documents, obtain the Operator Permit before
the Class 1 Activity Permit, purchase/attach the tracker where required and use
FlyItSafe. These steps and fees do **not** imply approval to operate at the
Istana. Written owner and SPF authority remain necessary, and other current
restrictions may make aerial capture unavailable.

Ground-based LiDAR and photogrammetry avoid aviation rules but do not avoid
owner permission, protected-place photography restrictions, site-access rules
or security review.

## Survey reference and published positioning cost

Require all authoritative capture in the local survey frame:

- horizontal: SVY21 / Singapore Transverse Mercator;
- vertical: Singapore Height Datum, with the exact geoid/conversion method
  stated;
- Unreal: a documented local frame derived from approved survey control, never
  a hand-tuned visual offset.

Official references:

- [SLA Survey Reference System](https://www.sla.gov.sg/regulatory/property-boundaries/survey-reference-system/)
- [SiReNT SVY21 plane coordinate system](https://app.sla.gov.sg/sirent/About/PlaneCoordinateSystem)
- [SLA Singapore Geoid Model/SHD](https://app.sla.gov.sg/sirent/About/SGEOID09)

The [SiReNT services portal](https://app.sla.gov.sg/sirent/Page/Services)
currently displays these GST-inclusive values:

| Service | Published portal value |
| --- | ---: |
| One-time administration | S$32.10 |
| Post Processing On-Demand | S$10.70/account/month plus S$0.32/minute |
| RTK or DGNSS, accounts 1–9 | S$107/account/month |
| RTK or DGNSS, accounts 10–50 | S$64.20/account/month |
| RTK or DGNSS, accounts 51+ | S$32.10/account/month |
| Post Processing Archive | S$53.50/month |

The portal says subscription starts with Singpass/Corppass and archive GNSS
data are supplied as RINEX. Its service table is marked last updated 8 June
2020, so confirm the amounts and terms before ordering. SiReNT supplies survey
reference/positioning; it does not grant access to the Istana.

## Acquisition order and release rule

1. Freeze the target epoch, exterior-only scope, required tolerances and exact
   data audience.
2. Obtain an owner sponsorship/permission letter before asking agencies for
   controlled Istana material.
3. Request the current restoration BIM/as-built package and NHB Volume 2
   measured drawings.
4. Request and quote the SLA National 3D Mapping and licensed 2D products.
5. Request the NParks Tree Registry/RTMS extract.
6. Use NAS material only after record-specific permission, and only as dated
   historical evidence.
7. Procure the terrestrial survey and calibrated material capture only after
   the method statement passes owner/security review.
8. Maintain a per-file rights matrix covering source storage, private
   repository, named collaborators, derivative creation, rendering, simulation
   packaging, backups, publication, expiry and deletion.
9. Keep restricted raw sources outside Git unless the written licence expressly
   authorises GitHub and all repository members. Commit only approved
   derivatives or retrieval manifests.
10. Validate the final model against independent survey checkpoints and
    calibrated held-out photographs. Do not describe the model as one-to-one
    unless the dated tolerance report and all authority packages pass.

Until the owner, SLA, NParks and authorised survey packages are obtained, the
correct project status is:

**BLOCKED ON AUTHORITATIVE FIDELITY DATA — public 2D context available.**
