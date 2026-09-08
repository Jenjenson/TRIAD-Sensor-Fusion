# Istana study-area data and model provenance

> **Prototype provenance only.** The sources below explain the existing v2
> approximation; they do not establish survey-grade geometry or colour. This
> prototype was rejected for digital-twin release. See
> `ISTANA_DIGITAL_TWIN_ACQUISITION_SPEC.md` for the replacement authority and
> validation requirements.

## Scope and reference epoch

The study area is a geodesic circle with a radius of exactly 1,000 metres,
centred on the approximate centroid of the Istana Main Building at WGS84
`1.30709615 N, 103.84288055 E`. The bounding box sometimes shown by tools is
only an envelope; it is not the simulation boundary.

The custom Main Building exterior targets the publicly visible, pre-restoration
appearance photographed in April 2024. It intentionally omits interiors,
restricted circulation, security systems, and operational details. The model is
a visual and collision approximation for simulation, not a survey, record
drawing, cadastral product, or as-built engineering model.

The deterministic fallback ellipsoid height is 47 metres from a 30 metre SRTM
cell. The map includes an invisible flat collision disk at that height so a
survey fails predictably rather than depending on whether streamed terrain has
materialised at every sample. This disk is suitable only for a controlled
simulation baseline; it does not reproduce local grade and is not survey
terrain. Loaded Cesium collision may add context, but it does not turn the
fallback or the procedural building into an as-built model.

## Architectural evidence

The public exterior model follows these documented characteristics:

- cross-shaped neo-Palladian plan adapted to a tropical/Malay house layout;
- open verandahs and repeated open/arched facade bays;
- louvred windows and panelled doors;
- a three-storey central tower with Corinthian elements;
- two-storey wings, with Ionic elements over Doric elements;
- mansard roof, dormers, broad central stair, formal lawn and fountain.

Primary reference pages:

- [Istana — Buildings](https://www.istana.gov.sg/visit-and-explore/buildings/)
- [Istana — The grounds](https://www.istana.gov.sg/visit-and-explore/the-grounds/)
- [Istana visitor brochure](https://www.istana.gov.sg/-/media/Files/IOH-e-brochure-10-Jan.ashx)
- [NHB Roots architectural record](https://www.roots.gov.sg/Collection-Landing/listing/1122592)
- [URA conservation record](https://www.ura.gov.sg/conservation/find-a-building/conservation-portal/024-istnsrtm/)

Official-site imagery is reference-only unless separate written permission is
obtained. Do not package official pixels as textures.

Reusable visual references must be checked file by file before redistribution:

- [Wikimedia Commons: Istana, Singapore](https://commons.wikimedia.org/wiki/Category:Istana_Singapore)
- [The Istana, April 2024 — CC BY-SA 4.0](https://commons.wikimedia.org/wiki/File:The_Istana,_April_2024.jpg)
- [The Istana Main Building, April 2024 — CC BY-SA 4.0](https://commons.wikimedia.org/wiki/File:The_Istana_Main_Building,_April_2024.jpg)
- [Historic Government House view — CC0](https://commons.wikimedia.org/wiki/File:Gezicht_op_Government_House_in_Singapore,_RP-F-AA3188-BO.jpg)

The shipped procedural model does not embed those images or create derivative
textures from them.

## Surrounding context

The current map reuses the host project's streamed Cesium context for low-detail
surroundings and clips tiles outside the 1 km polygon. Cesium access credentials
are user/environment supplied and are never copied into this repository.

Future static context should use vector data fetched from Overpass rather than
scraping standard OpenStreetMap tiles. OpenStreetMap data is ODbL and requires
attribution and, where applicable, derived-database compliance:

- [OpenStreetMap copyright and licence](https://www.openstreetmap.org/copyright)
- [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/)

Useful cross-checks are the indicative
[URA Master Plan 2019 building layer](https://data.gov.sg/datasets/d_e8e3249d4433845bdd8034ae44329d9e/view)
under the Singapore Open Data Licence and OneMap. Neither should be represented
as survey-grade geometry. Precise terrain, building envelopes, mount points, or
property constraints require licensed SLA data, record drawings, or a site
survey.

## Physics and recommendation limits

Unreal collision and line traces are authoritative only for the loaded
simulation scene's occlusion tests. Survey output therefore declares
`geometryAuthorityScope: LOADED_SIMULATION_COLLISION_ONLY` and
`surveyGrade: false`. A placement result is still a simulation recommendation
and requires human review for structural suitability, access, ownership,
electrical/network provision, privacy, safety, and regulatory approval.

Coverage values are deterministic model indexes, not calibrated probabilities
of detection. Recommended sites and orientations must be replayed in Unreal and
re-surveyed after any geometry, weather, sensor model, or terrain change.

## Asset ledger requirements

Any future imported asset must record:

1. source URL and author;
2. access date and reference epoch;
3. exact licence and version;
4. required attribution;
5. whether it is reference-only or redistributed;
6. modifications and derivative-licence obligations;
7. coordinate reference system and stated accuracy, for geospatial data.

Keep share-alike texture derivatives segregated from proprietary asset packs.
Never commit Cesium tokens, editor user settings, generated telemetry, frame
captures, caches, or Unreal build products.
