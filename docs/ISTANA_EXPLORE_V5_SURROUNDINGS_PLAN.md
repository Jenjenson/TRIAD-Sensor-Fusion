# Istana Explore V5 surroundings plan

Status: **V5A implementation authorised after the cold-valid V4 baseline**.

The frozen source map is
`/Game/Maps/Istana_PublicView_Explore_v4` (12,153,074 bytes, SHA-256
`D31EBF410A8C0E7201103BCCBDBC05F3BD1A2DF124FF090D080DCFAC27DEBA72`).
Its accepted eight-view run exposed an acceptance defect: every PNG was
786x642 despite a 1920x1080 launch request. V5 matched acceptance therefore
requires decoded PNG dimensions of exactly 1920x1080; a requested window size
or a broad minimum/maximum range is not evidence of capture resolution.

This is an additive, appearance-only follow-up. It must not overwrite V4,
invent missing site geometry, or promote the public-data approximation to a
survey/as-built claim.

## Exact delta allowlist

Only these target-local changes are admitted:

- replace only `ATRIADIstanaPublicViewSceneActor::TerrainComponent` /
  `TerrainVisualCollision`, material slot zero, with the V5-owned
  `MI_IPV5_Grass001_Lawn`; reuse the exact frozen V4 ambientCG `Grass001`
  base-colour, DirectX-normal, roughness and AO texture packages; use two
  non-harmonic continuous-metre UV0 scales, restrained macro breakup and
  distance-faded micro-normal response, with no WPO, PDO, displacement, RVT,
  topology, collision or navigation change;
- on only `V4AnimatedCloseTurfGeometryCardReplacement`, keep the exact V4
  mesh, material parent, 18,432 transforms, culling, WPO, shadow and collision
  state, but override its existing `BaseColorTexture`, `NormalTexture`,
  `RoughnessTexture` and `OpacityTexture` runtime MID parameters with the
  frozen ambientCG `Foliage008` atlas for which those cards were authored;
- replace the existing hardscape `M_IPV_Water` slot with a V5-owned procedural
  fountain-water material;
- replace the existing hardscape stone slot with the V5-owned
  `MI_IPV5_HardscapeStone`, whose exact direct parent is the already frozen and
  validated lawful `MI_IPV_Hero_Stone_V5`; keep `M_IPV_Planting` unchanged
  unless a separately licensed soil source is frozen by object path, bytes and
  SHA-256;
- replace only the already visible OSM context wall/roof slots with V5-owned
  instances whose exact direct parents are the already frozen and validated
  lawful `MI_IPV_Hero_Render_V5` and `MI_IPV_Hero_Slate_V5`. This deliberately
  admits their full generic PBR base/normal/ORM/detail response, plus only the
  recorded restrained V5 scalar overrides; it does not change context geometry;
- enable short world-space contact shadows only on an exact frozen
  actor/component-path roster; the V5 contract must record every enabled and
  disabled path plus its expected pre-state, including all trees, Heritage
  trees, shrubs, flowers, understorey, grass/turf, hardscape and V8 portico;
- use a V5-only pawn/game mode that freezes the full neutral camera, SSGI and
  SSR state while preserving the Explore `80` degree field of view.

The hidden V3 synthetic terrain, road and water components must stay hidden and
`NoCollision`. Terrain topology, portico geometry/materials, every transform,
instance count, collision response and navigation state are outside this
allowlist. The separately frozen CC0 Pachira source under
`unreal/SourceAssets/IstanaPublicViewExploreV5/Sources/Vegetation` is acquisition
evidence only and remains deferred to a distinct V5B placement/visual review.

## Material constraints

`M_IPV5_FountainWater` must be `Surface / DefaultLit / Translucent /
SurfacePerPixel`, with IOR `1.333`, opacity clamped to `0.35–0.65`, depth-fade
distance clamped to `25–150 cm`, and material-local SSR. It may use two
nonparallel normal fields whose UV speeds are each no greater than `0.03` UV/s
and differ by at least 25 percent. The frozen V5 contract must record every
scalar/vector default and range, graph digest and exact texture object
path/bytes/SHA-256. It must use no world-position offset, displacement,
emissive output or geometry animation. The existing basin-surface and jet
sections stay intact.

`MI_IPV5_HardscapeStone` must directly inherit only
`/Game/TRIAD/IstanaPublicView/HeroMaterialsV5/MI_IPV_Hero_Stone_V5`, and the V5
asset preflight must first pass the existing exact Hero V5 asset validator. The
contract must freeze the effective parent, inherited texture dependencies and
all local scalar/vector overrides. It must not synthesize new cracks, joints,
stains, moss, puddles or site-specific weathering.

Visible context walls/roofs may receive the exact frozen generic Hero Render and
Hero Slate PBR responses through `MI_IPV5_ContextRender` and
`MI_IPV5_ContextRoof`, with their complete inherited dependency rosters and
bounded local overrides validated before save and cold load. The exact
`M_IPV_ContextRender` and `M_IPV_ContextRoof` named-slot mappings remain fixed.
Do not fabricate windows, storeys, signs, roof equipment, facade grids or
building-specific ageing.

### Later V5C render-only massing successor

The later V5C official-preferred surroundings layer is outside the frozen V5A
material allowlist above. At the user's direction it may replace the two shared
V5 context parents with one isolated, texture-free procedural massing master,
while retaining the exact four provenance-separated V5C wall/roof slot names
and the 35,424-triangle topology. Each building polygon part may sample the
already frozen synthetic visual terrain once, preserve its selected height
above that sample, and extend the existing wall down by a one-metre hidden
anti-gap skirt. Wall UV0 may use local ring-distance/height metres. This
successor may add restrained colour variation, camera-distance muting and an
anti-aliased framed/inset **aperture-rhythm hint** with deterministic cell
variation and a plinth cue on wall slots only. Those are screen-readable tonal
breakup, not window geometry, openings, storey counts, facade records,
foundations, physical grade, or evidence about any specific building. Roof
slots must set aperture strength to exactly zero.

The V5C master remains opaque, one-sided and Default Lit, with no texture
dependencies, emissive output, opacity, world-position offset, displacement,
collision, navigation, sensor-occlusion or RF role. Its distance tint is a
bounded look-development approximation used because the public-view runtime
deliberately contains no exponential-height-fog actor; it is not measured
weather or atmospheric calibration. All public-source limitations still hold:
the sampled terrain Z is synthetic visual relief rather than DTM/survey grade,
heights remain synthetic or volunteered, roofs remain flat caps, and no real
foundation/facade/aperture/material authority is created. The V5C factory must
validate the master graph and exact child parameter rosters before save and
after cold load.

## Lighting and contact-shadow profile

The first test point is:

- world-space contact-shadow length: `50 cm`, bounded trial range `25–100 cm`;
- casting intensity `1`, non-casting intensity `0`;
- `r.ContactShadows=1`, with global override CVars disabled;
- contact shadows explicitly disabled on geometry grass and close-turf cards;
- before the sweep, freeze exact enabled/disabled actor/component paths and
  each path's mobility, CastShadow and bCastContactShadow pre-state; after the
  matched sweep, freeze the smallest passing `25–100 cm` value in the contract;
- existing sun direction, colour and `80,000 lux` intensity unchanged;
- manual exposure `1/125 s`, ISO 100, f/8, 6500 K, zero tint;
- SSGI and SSR explicitly pinned;
- no LUT, local exposure, bloom, vignette, depth of field, motion blur,
  chromatic aberration or film grain.

This is a neutral look-development state, not verified 21 April 2024 solar or
weather truth.

## Implementation sequence

1. Cold-build and validate V4; freeze its map SHA-256, 85-asset manifest,
   protected hashes, eight fixed captures and traversal performance. **Done.**
2. Add a frozen V5 contract containing this allowlist and the V4 evidence.
3. Duplicate V4 non-destructively to V5 and prove exact pre-mutation
   inheritance before applying any V5 delta.
4. Add the V5-only pawn/game mode and complete render-state validator. After
   compiling shared plugin code, cold-load V4 and rerun its runtime validation;
   an unchanged map hash alone does not prove unchanged base-class behaviour.
5. Trial contact shadows only on V5 and accept/reject from matched captures.
6. Add lawn and close-turf response only on V5, then repeat the exact decoded
   1920x1080 matched visual/performance acceptance.
7. Add fountain water only on V5 and repeat matched visual/performance
   acceptance, including bounded motion: over a fixed 10-second capture the
   water crop must change, the static surround must remain within `2/255` p99,
   and no water pixel may move outside the unchanged mesh silhouette.
8. Add stone only if close views still read flat. Keep planting unchanged until
   an exact frozen soil source is approved.
9. Add visible context-material response last.
10. Save and cold-reload V5 after each accepted phase, prove V4 remains
   cold-valid and byte-identical, then run PIE movement, gust/material
   animation, the fixed capture suite and a release soak.

## Acceptance gates

Structural:

- no mesh, section, triangle, transform, instance, collision, visibility or
  navigation delta outside the allowlist;
- named-slot/path validation rather than unverified numeric-slot assumptions;
- unchanged V4 map, protected assets and inherited source hashes;
- all V5 materials compile for SM5 with exact dependency/parameter validation;
- all hidden geospatial components remain hidden and `NoCollision`;
- exactly one tagged directional sun, skylight and sky atmosphere; zero fog and
  zero unapproved light actors; exact sun/skylight intensity, colour, rotation,
  mobility and realtime-skylight state;
- exact editable light/camera digests plus runtime readback of SSGI, SSR,
  `r.ContactShadows` and every global contact-shadow override CVar;
- every admitted component snapshot includes mobility, ordinary/contact shadow,
  custom-depth/stencil, editor/game/scene-capture visibility, active/autoactivate
  state and, for HISM/foliage, LOD, cull and WPO-disable distances.

Visual:

- eight fixed views whose decoded PNG dimensions equal exactly 1920x1080, plus
  fountain, stone, tree-base, turf and context crops after shader/texture
  warmup; launcher/window resolution alone does not satisfy this gate;
- no clipped all-channel-white pixels; frame mean luminance within 5 percent
  of matched V4 unless a separately approved exposure change explains it;
- unchanged-region RGB p99 error no greater than `2/255` for material-only A/B;
- no opaque water slab, refraction holes, clipped sparkles, silhouette change or
  screen-edge SSR streaks;
- no floating contacts, grass-card shadow combing, repeated hardscape tile,
  z-fighting, shimmer or fabricated context detail.

The inherited `portico_front` pose remains a matched regression view but is not
the sole portico-quality diagnostic: it sits under retained closed cross-shell
geometry and its V4 frame is approximately 37 percent near-black. Add a
non-acceptance close view from outside every visible render AABB (nominally
Y=65-70 m, Z=5-6 m, aimed at the entrance) plus left/right obliques.

Performance on the same route and machine:

- average at least 30 fps; 1 percent low at least 24 fps;
- GPU p95 no more than 30 ms; game/render-thread p95 no more than 22 ms;
- GPU memory no more than 10 GiB; process RAM no more than 24 GiB;
- no more than 10 percent GPU-frame-time regression from V4.

## Authoritative-data boundary

Do not implement bare-earth grade, roads, paths, kerbs, drainage, exact
fountain geometry, surrounding building heights/roofs/facades, 2024 planting,
historical sunlight/weather or RF material/thickness authority from the current
sources. Those require the owner-approved 2024 orthophoto, bare-earth DTM,
classified LiDAR, surveyed breaklines/control, calibrated imagery and the
separate RF survey described in `ISTANA_DATA_REQUEST_DRAFTS.md`.
