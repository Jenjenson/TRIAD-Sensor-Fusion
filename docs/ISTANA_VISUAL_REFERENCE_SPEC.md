# Istana Main Building exterior visual-reference specification

> **Digital-twin status: rejected.** This document describes the public-reference
> `Istana_1km_Context_v2` prototype and its visual-review contract. The generated
> mesh, generated textures, approximate ground and streamed context are not
> survey-controlled and cannot be promoted to a one-to-one digital twin. New
> work must follow `ISTANA_DIGITAL_TWIN_ACQUISITION_SPEC.md`; the v2 map remains a
> non-authoritative fallback only.

## Scope and reference epoch

This is the visual ground-truth brief for the **Istana Main Building and its
immediate public-facing forecourt**. It is not a brief for Sri Temasek, The
Villa, the Lodge, or Istana Kampong Gelam. The intended appearance is the
publicly visible, pre-restoration exterior photographed at the April 2024 Open
House. Scaffolding, temporary event furniture, crowds, signs, and the present
restoration works are out of scope.

The result is a photo-matched simulation asset, not an as-built, survey, BIM,
security, or facilities-management model. Public sources do not provide a
blueprint or floor plan. NUS-ISS records that the Straits Times' 2019 model was
also built manually from two-dimensional photographs because the Istana could
not provide those drawings.

## Evidence hierarchy

Resolve conflicts in this order:

1. the official Istana and National Heritage Board descriptions;
2. the published *The Istana*, third edition, sample pages;
3. modern, file-level licensed photographs from several viewpoints;
4. the OpenStreetMap footprint for the broad horizontal envelope only;
5. visual inference, explicitly labelled and reviewed in a photo overlay.

Do not infer hidden structure, security details, restricted circulation, or
interiors from streamed imagery.

## Canonical dimensions and uncertainty

| Quantity | Working target | Authority and caveat |
| --- | ---: | --- |
| Main footprint envelope | about 123.8 m east-west by 116.4 m north-south | Derived from the 55-point outline of OpenStreetMap way 41895536 on 21 August 2026. ODbL, Bing-traced, mapping-grade, not survey-grade. |
| Approximate footprint centre | 1.30716 N, 103.84299 E | Mean of the same OSM outline vertices; use the project's canonical anchor for runtime placement. |
| Principal horizontal axis | about 2.2 degrees clockwise from east-west | PCA of the OSM outline, useful only as an initial registration. Calibrate the front elevation against imagery. |
| Central tower height | **28 m** | *The Istana*, third edition, p. 45 calls it the “28m-tall central tower” and says it is topped by a slate mansard roof. Treat 28 m as the building/tower target including the mansard and excluding the separate flag mast until a record drawing establishes otherwise. The publication does not define its measurement endpoints. |
| Raised undercroft | more than 1 m | The same publication, pp. 46-47, says dwarfed arches and piers elevate the building over a metre above ground. |
| Historic foundation | 15 ft / about 4.57 m | Published historical construction fact, not a reason to expose or simulate underground structure. |

The overall horizontal envelope is plausible at approximately 120 by 118 m.
A generated 43.2 m architectural height is not consistent with the published
28 m tower dimension or the modern front-elevation proportions. Do not scale
the flag mast into the architectural-height check.

The following vertical bands are reconstruction guides, not measured facts:

| Band | Approximate range above local finished grade |
| --- | ---: |
| Raised plinth / undercroft | 1.0-1.3 m |
| Ground-floor arcade and first entablature | 1.3-7.0 m |
| Upper wing colonnade and continuous cornice | 7.0-13.0 m |
| Wing balustrade / parapet top | 13.0-14.5 m |
| Recessed central third stage | about 14.5-23 m |
| Central slate mansard to architectural top | about 23-28 m |
| Flag cage and mast | separate, above the 28 m target |

Freeze exact storey heights only after a perspective-correct front-photo
overlay and an oblique-photo check agree.

## Reviewed Unreal asset contract

The accepted building renderer is the project-authored refined asset
`/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined`. It is a new asset; the
legacy `/Game/TRIAD/Istana/Meshes/SM_IstanaExterior` asset is never
overwritten. The reviewed LOD0 source manifest declares `175,974` source
vertices, `58,658` triangles, and bounds of
`12500 x 11769.4444 x 3685 cm` including the mast. The live import wrapper
verifies the actual OBJ bytes against SHA-256
`4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91`
before invoking Unreal.

The refined mesh uses the eight existing, validated project PBR materials:
`M_Istana_Plaster`, `M_Istana_PlasterTrim`, `M_Istana_Slate`,
`M_Istana_Shutter`, `M_Istana_Glass`, `M_Istana_Stone`,
`M_Istana_Metal`, and `M_Istana_Door`. The refined importer does not create or
mutate those materials. Direct OBJ import creates auto-generated simple
collision. The separate eight-hull collision OBJ is not imported by this path,
so acceptance must not describe the refined asset as having eight UCX hulls.

## Form and facade inventory

### Plan and massing

- Preserve the documented symmetrical, cross-shaped main plan.
- Model the long two-storey wings and the central three-storey tower as
  distinct masses. The central tower and entrance portico project beyond the
  wing plane.
- Preserve the later rear/east annex in the footprint. A two-storey annex was
  added in 1914 and a third storey in 1936; it must not be mirrored into a
  fictitious perfectly symmetric rear.
- The main approach elevation is strongly symmetrical: a projected central
  portico is flanked by two end porticoes capped by triangular pediments.

### Ground level

- The dominant motif is a **continuous arcade of round arches** on stout Doric
  piers/pilasters. It is not a row of generic freestanding cylinders.
- The central projected portico reads as three tall entrance arches in the
  frontal photographs. It has layered archivolts, springing/capital mouldings,
  deep reveals, and real shaded veranda depth.
- Dwarfed arches and piers below the principal floor visually raise the
  building like a tropical house on stilts.
- Doors are panelled and recessed. Arched fanlights, mullions, and glass sit
  behind the arcade rather than on the facade plane.

### Upper wings

- Use the correct Palladian hierarchy: Doric elements and arches below, Ionic
  columns/pilasters on the second tier, and Corinthian elements on the highest
  central tier.
- Tall, close-spaced **horizontal louvres** fill most upper openings. In modern
  neutral-daylight photographs they are light warm grey, taupe, or ivory, not
  deep green-black.
- Geometry, not a flat normal map, must carry the louvre blade thickness,
  spacing, recess, side frames, and shadow gap.
- The central projected block has four principal upper louvred bays in the
  clearest front view. Each pedimented end portico has three principal upper
  louvred bays. Intermediate wing bays repeat at a close, regular rhythm; their
  final count must be frozen by the calibrated elevation rather than guessed.
- Include narrow balustrades beneath the upper openings, the continuous roof
  balustrade, corner/newel piers, and the repeated small balusters.

### Central tower and roof

- The highest mass is rectangular and set back in tiers. It is not an
  octagonal lantern or a dome.
- The central tower alone is crowned by a dark natural-slate mansard roof. The
  modern front view shows a single centred, round-headed dormer on the visible
  roof face; oblique views reveal side dormers.
- The wings read as flat/parapeted from the public approaches. Do not add large
  visible long-wing or cross-wing mansards without photographic evidence.
- The front porch roof was converted into a **flat terrace** in 1940. A
  pitched/frustum portico roof is incorrect for the 2024 reference epoch.
- Include the simple metal flag cage/railing and a slender flag mast as
  separate objects. They must not inflate the building height or collision.

### Classical detail

- Cornices are deep and multiply stepped, with dense dentil courses. One box
  strip is not sufficient at hero distance.
- Model capitals by order: restrained Doric below, voluted Ionic above, and
  leafier Corinthian detail on the central upper stage.
- Pediments require layered raking cornices and tympanum depth. The state crest
  is a separately reviewed decal/mesh; do not bake it into a generic tileable
  material.
- Add parapet caps, pilaster bases, plinth courses, quoins where evidenced,
  drip edges, arch keystones/mouldings, and realistic reveal depths.
- Deep verandahs have dark exposed timber beams against white soffits. Bell-
  shaped white opaline pendants hang at regular intervals; the 1996-98 work
  alternated them with three-bladed period ceiling fans.
- Current verandahs include glass walls introduced in the 1996-98 overhaul.
  Keep them subtle and behind the historic arcade.

## Surface and PBR specification

The Main Building may have historic brick masonry in its construction, but the
public exterior is painted/rendered white. **Do not put an exposed brick pattern
or brick-scale normal map on the facade.** Realism comes from render grain,
paint response, precise mouldings, deep recesses, and controlled weathering.

| Surface | Base-colour character | Geometry / normal | Roughness and response |
| --- | --- | --- | --- |
| Main render | Warm off-white mineral render; avoid pure RGB white | Fine sub-millimetre grain, slight hand-applied waviness, low-frequency rain wash | Dry 0.65-0.85; broad soft highlight; minimal edge chipping |
| Classical trim | Slightly cleaner/lighter off-white than field render | Profiles and dentils in geometry; only micrograin in normal | 0.50-0.72 |
| Louvres | Light warm grey/taupe/ivory painted finish | Every hero louvre blade in geometry; restrained paint microtexture | 0.45-0.65; subtle colour variation, no baked shadow |
| Central roof | Cool charcoal to brown-charcoal natural slate, never red terracotta | Slate courses and edge thickness in geometry/displacement at near LOD | 0.65-0.85 dry; slightly darker and smoother when wet |
| Veranda/step stone | White to cool pale grey granite with restrained crystalline fleck | Rounded stair noses and slab joints in geometry | 0.35-0.58 dry; 0.15-0.32 wet |
| Glass walls/windows | Near-neutral, lightly aged glass; never opaque black | Real panes, mullions, and a small physical offset | IOR about 1.5; roughness 0.02-0.10; reflection controlled by exposure |
| Dark soffit beams | Near-black/dark brown painted timber | Beam depth and spacing in geometry | 0.42-0.62 |
| Pendants | Milky white opaline globe/bell shade with dark cap and chain | Dedicated shade mesh | Subsurface/transmission impression without blown emissive white |
| Doors | Photo-matched panelled finish; the public opening is mostly shadowed | Panels, rails, stiles, fanlight and handles in geometry | 0.35-0.60 depending on painted versus sealed timber element |

Use unique masks for vertical rain streaks, sheltered grime, and ground splash;
do not bake those directions into repeating source textures. Keep weathering
subtle: the reference facade is very well maintained.

## Immediate forecourt and landscaping

- Preserve the broad formal axis from the circular fountain through the lower
  lawn and landscape stair to the three-arch entrance.
- The main landscape stair is much broader and longer than a small porch
  stair. Modern photographs show numerous shallow, warm stone/tile risers,
  white side walls, and dark handrails.
- Recreate the low circular fountain basin with a polished dark inner curb, a
  broad pale tan/pink paver ring, and several concentric fountain jets. A single
  generic jet is not a match.
- Use clipped, high-density tropical lawn, symmetric flowering shrubs and rose
  masses, low formal hedges, and mature trees that frame rather than hide the
  facade. Keep vegetation species/placement approximate unless separately
  sourced.
- Include the Ceremonial Plaza's four flagpoles and restrained chain/bollard
  barriers only where visible in the chosen epoch.
- Do not replace the surrounding context with an isolated rectangular grass
  slab. The building and forecourt must meet the streamed/local grade and blend
  into the retained one-kilometre context.

## Fidelity and acceptance gates

1. **Silhouette:** a level front camera must match the 2012 and April 2024
   photographs for wing parapets, end pediments, stepped central tower, slate
   mansard, dormer, and flag mast. No dome/cupola may appear.
2. **Scale:** OSM-derived horizontal envelope remains within 2 percent after
   registration; architectural tower target is 28 m excluding the flag mast.
3. **Facade rhythm:** central and end-portico bay counts match the photographs;
   repeated wing louvres and ground arches remain aligned vertically.
4. **Hero detail:** at a 10-25 m camera distance, louvre blades, arch mouldings,
   capitals, dentils, balusters, glass offsets, floor joints, pendants, and dark
   soffit beams are real geometry or Nanite-quality displacement.
5. **Materials:** neutral overcast and tropical noon comparisons show warm
   white render, pale louvres, and charcoal slate without clipping the white
   facade. Exposed brick, maroon tile, and green-black louvres fail.
6. **Forecourt:** front fountain, lawns, broad stair, and planting masses align
   with the centred reference shot and sit on the real scene grade.
7. **Runtime:** Play-In-Editor opens at the intended approach camera with the
   building, terrain and surrounding structures visible; fog cannot obscure
   the acceptance view. The deterministic south/front acceptance camera is
   210 m horizontally from the exterior origin, 24 m above the calibrated
   ground, aimed 13 m above ground, with a 52-degree horizontal FOV. The
   imported OBJ's source ceremonial `+Y` becomes Unreal local `-Y`; the
   validated `182.3`-degree ESU heading must point that local front broadly
   geodetic south rather than relying on a screenshot label.
8. **Authored refined primary:** visual acceptance requires the exact refined
   mesh above to remain visible and collision-enabled with both its hard and
   soft references intact. Every current-view Cesium tileset must still report
   finite exact 100% progress with physics, but Cesium is surrounding context
   only. The provider-rendered Istana has been observed flattened or absent and
   is not accepted or claimed as the building. Fog suppression remains active;
   visual-acceptance mode may hide only study-area components tagged
   `TRIADHumanOnlyOverlay`; the 1 km clip, AOI, and collision remain unchanged.
9. **Streamed opt-in boundary:** `bPreferStreamedIstanaVisualWhenReady`
   defaults to `false`. Its reversible hide/restore behavior remains available
   only as an explicit opt-in, and any run using it cannot pass visual
   acceptance. Loss of finite progress, exact readiness, or physics restores
   authored rendering; authored collision is never disabled.

Recommended review captures are a centred fountain/front view, a right-front
oblique at veranda height, a low left/right arcade detail, a central-tower
telephoto, and a Play-In-Editor spawn capture. Save the source photo, camera
transform, FOV, weather/time, and overlay for every signed-off capture.

## Source and licence ledger

Accessed 21 August 2026 unless noted otherwise.

| Source | What it establishes | Reuse status |
| --- | --- | --- |
| [Official Istana: Buildings](https://www.istana.gov.sg/visit-and-explore/buildings/) | Neo-Palladian/Malay adaptation; verandahs, louvres, panelled doors; central slate mansard; Doric/Ionic/Corinthian hierarchy; end pediments | Authoritative **reference only**. Do not copy official image pixels without separate permission. |
| [NHB Roots: The Istana and Sri Temasek](https://www.roots.gov.sg/places/places-landing/Places/national-monuments/the-istana-and-sri-temasek) | Cross-cultural architecture, symmetrical projected portico and twin pedimented porticoes, Java marble | Authoritative **reference only** unless a specific media item states otherwise. |
| [*The Istana*, third edition sample](https://www.marshallcavendish.com/docs/default-source/default-document-library/sample-page/9789814868501.pdf), pp. 45-47 | 28 m central tower, slate mansard, raised piers, classical hierarchy, 1940 flat porch terrace, 1996-98 granite/glass/lights/fans | Copyrighted publication, **reference only**; do not redistribute pages or imagery. |
| [NLB BiblioAsia: The Istana Turns 150](https://biblioasia.nlb.gov.sg/all-sections/vol-15-issue-4-jan-mar-2020-istana-turns-150/) | Restoration history, mechanically activated louvres, restored timber beams, formal lawn and 18 mature framing trees | Article and credited images are **reference only** unless each item has separate permission. |
| [The Istana, April 2024](https://commons.wikimedia.org/wiki/File:The_Istana,_April_2024.jpg), GoAheadFan95 | High-resolution current front elevation and material colours | CC BY-SA 4.0. Redistributable only with creator credit, link, change notice, and share-alike compliance. No copy is committed here. |
| [The Istana Main Building, April 2024](https://commons.wikimedia.org/wiki/File:The_Istana_Main_Building,_April_2024.jpg), GoAheadFan95 | Right-front oblique, arcade depth, louvres, cornices, roofline | CC BY-SA 4.0, same obligations. No copy is committed here. |
| [Istana (Singapore).jpg](https://commons.wikimedia.org/wiki/File:Istana_(Singapore).jpg), Elisa.rolle | Centred fountain/front view and landscaping proportions | CC BY-SA 4.0, same obligations. No copy is committed here. |
| [Istana 32, Singapore, Jan 06](https://commons.wikimedia.org/wiki/File:Istana_32,_Singapore,_Jan_06.JPG), [Istana 37](https://commons.wikimedia.org/wiki/File:Istana_37,_Singapore,_Jan_06.JPG), and [Istana 38](https://commons.wikimedia.org/wiki/File:Istana_38,_Singapore,_Jan_06.JPG), Sengkang | Oblique arcade, stair, dark soffit beams, pendants and close veranda detail | Commons pages say “copyrighted free use”, but this is not a standard explicit licence. Treat as **reference only** unless project legal accepts the file-page grant. |
| [Rijksmuseum historic Government House view](https://commons.wikimedia.org/wiki/File:Gezicht_op_Government_House_in_Singapore,_RP-F-AA3188-BO.jpg) | Historic massing cross-check, not the modern material epoch | CC0 1.0 / public domain dedication; redistributable. |
| [OpenStreetMap way 41895536](https://www.openstreetmap.org/way/41895536) and [OSM licence](https://www.openstreetmap.org/copyright) | Approximate cross-plan outline and registration | ODbL 1.0. Attribute “© OpenStreetMap contributors” and identify the ODbL when data/derived database is publicly used. Not survey-grade. |
| [NUS-ISS: Inside the Istana virtual-tour production](https://www.iss.nus.edu.sg/community/newsroom/news-detail/2020/11/24/designing-the-award-winning-inside-the-istana-virtual-tour-a-gdipsa-intern-experience) | Confirms SPH built a detailed model by hand from photos because no plan was supplied | **Reference only**. The SPH model has no verified reusable/download licence. Contact the rights holder before any use. |
| [Commercial 1xmerch listing](https://cults3d.com/en/3d-model/architecture/the-istana-singapore-3d-model-presidential-palace-heritage-building-singap) and [TurboSquid listing](https://www.turbosquid.com/3d-models/3d-the-istana-singapore-3d-model-presidential-palace-heritage-building-singapore-architecture-lan-2491935) | A purchasable mesh exists | **Not acquired.** Listings disagree on licence/quality metadata; one marks editorial use, and the asset lacks a verified PBR/photo-match pedigree. Do not download, commit, or redistribute without procurement and licence review. |
| [NHB Heritage on Sketchfab](https://sketchfab.com/nhbheritage) | Search for official open 3D assets | No Istana Main Building asset was identified. “Istana Kampong Gelam” is a different monument. Do not substitute it. |

If a licensed photograph is later committed as a reference plate, add its exact
filename, source revision, creator, licence, modifications, and attribution to
the repository's asset ledger. A model built from measurements and observation
is project-authored; a texture projected or cloned from a CC BY-SA photograph
is a derivative and inherits the relevant obligations.
