# Istana post-R33 visual-fidelity plan

Status: planning evidence only. Do not activate, materialize, bind or execute a
successor native transaction until the already committed R30 transaction has an
explicitly human-accepted capture and the subsequent R31 transaction/capture,
R32 transaction/capture and R33 transaction/capture chain has passed explicit
human review.

The current native map is the committed R30-19 result. R31--R33 are
source-prepared, not visible native results. The exact current native pins are:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap` | 37,468,415 | `126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7` |
| `UnrealEditor-TRIADSensorFusion.dll` | 5,076,992 | `100B061CC5677508A67D644687923CA58F68F072305B6FF62B6BCD363D472028` |
| `UnrealEditor-TRIADSensorFusionEditor.dll` | 8,448,000 | `3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553` |

## Final R33 readiness boundary — 8 September 2026

The final source/readiness result at that audit date is preserved in the
[historical Istana R33 Cesium readiness audit — 2026-09-08](ISTANA_R33_CESIUM_READINESS_AUDIT_2026-09-08.md);
the current native checkpoint is the R30-19 status recorded above.
R33's runtime, context-policy, editor-transaction and Player0 capture-state
boundaries now require valid, non-null opaque `UCesiumIonServer` objects before
testing the Google/CWT pointer identity. Equality of two null pointers cannot
satisfy the contract. Focused R33 source/native/capture verification passes
`49/49`, the adjacent R29/R32/context/R33 regression passes `93/93`, and both
of the two repository-only R33 static self-checks (transaction and capture)
pass.

The installed Cesium for Unreal descriptor is version `2.18.0` (`Version 78`,
UE `5.5.0`), and its installed headers expose the R33 APIs used by the source,
including the tileset load-failure delegate, georeference/source/asset/server
getters and setters, load progress and height-sampling declaration. This closes
only the API-availability assumption. It is not a TRIAD compile, stream,
entitlement, provider-readiness or visual-acceptance result.

The native map and DLLs remain the exact R30-19 pins in the table above, bound
by the 65,411-byte committed receipt with SHA-256
`F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F`.
No R30 capture has been accepted. The latest attempt, run09, proved eight shader
workers and the exact Columnar-tree DDC hit, then failed closed on an
`AirSimTriadRuntime` optical-flow material compile error and rolled back with no
rollback errors. Its 658-byte receipt has SHA-256
`5827225197FE3BC4F57B28D36947F6C3642713410BD768B674C2F5D122144B87`.
The R30 capture harness has no fixed process-RAM ceiling and keeps a 2 GiB
emergency system-commit reserve; this is not GPU VRAM. R31--R33 stages retain
their 10/6/12-GiB limits. The mandatory remaining order is human-accepted R30
capture, R31 commit plus human capture, R32 commit plus human capture, and only
then R33. No post-R33 work is executable before that chain succeeds.

The server object is deliberately opaque: TRIAD neither reads nor records a
token value or fingerprint. A valid shared server object proves neither token
presence nor token validity nor entitlement to Google asset `2275207` or CWT
asset `1`, provider terms, live streaming or provider readiness. No height was
sampled or persisted, no vertical-datum conversion or independent checkpoint
validation exists, and CWT remains an optional visual reference rather than
authoritative terrain, collision, navigation, line-of-sight, sensor or RF
geometry. R33 is not live.

## Blocking R33 acceptance

R33 must first produce the same four fixed Player0 views in both `GooglePrimary` and `CwtPresented`: `075m`, `020m`, `095m`, and `surroundings_oblique_macdonald`. All eight captures must be raw 2560x1440 SDR images with fixed camera, exposure, weather, and renderer state.

Reject R33 if any image contains a black/missing terrain tile, an exposed clipping grid, visible Google/CWT double rendering, a floating/buried local contact caused by the presentation switch, or an unresolved connected crack/z-fight wider than 2 pixels for more than 50 pixels at 1440p. `CwtPresented` must be reached through the natural warming/readiness state machine, and `GooglePrimary` must be restored before exit.

SafeLocal is deliberately not part of this visual acceptance. A direct SafeLocal call or injected network failure would not prove genuine failover. The receipt must continue to state that failover/recovery was not accepted.

## Appearance-only successor order

Every stage preserves map geography and the existing collision, navigation, line-of-sight, sensor, and RF authority. Each stage receives its own native transaction, matched capture, and explicit human acceptance before the next stage begins.

1. **Freeze the matched baseline.** Pin camera transforms/FOV, resolution, exposure, white balance, sun/weather, scalability, provider state, map/DLL/source hashes, and authority-regression outputs.
2. **Tree appearance.** Preserve all 729 general-tree transforms and seven landmark anchors. Increase silhouette, age, and form variation at those existing anchors; improve leaf transmission, two-sided shading, bark roughness, root contact, controlled wind, and non-popping LOD transitions. Do not claim species identity without a licensed inventory.
3. **Turf appearance.** Preserve the 6,144 R29 placements, 4,608 R32 placements, and 65-90 m envelope. Add a graded near/medium/distance presentation and controlled crossfade; remove straw/bald bands, carrier repetition, moire, colour/roughness discontinuity, and the 90-95 m cutoff.
4. **Surrounding-building appearance.** Preserve the R29 shell/facade geometry, footprints, heights, transforms, and 43,448-triangle/17-slot broad shell. Improve semantic material families, deterministic macro variation, glazing/frame separation, restrained recess/parallax cues, roof differentiation, and weathering. Do not invent buildings or alter massing.
5. **Local seam/contact polish.** Fix visible public-realm junctions, building-foot contacts, the 300 m boundary, and 950-1000 m rim using render-only blending or existing visual geometry. Do not hide an unknown vertical datum by moving terrain.

The source-only, unadmitted
`TreeRealism/GeometryVariationCandidateV4` pack is the prepared input for step
2. It contains fifteen deterministic geometry recipes (three for each of the
five existing broad forms), an exact 736-row selector, and a fixed-view
source-vertex audit. An unnumbered source scaffold now provides the corresponding
fifteen-HISM runtime owner and dormant assets-only MeshDescription materializer.
Only read-only caller-pinned receipt inspection is reflected; it never confers
execution authority. The materializer is private and non-reflected, and its two
compiled trust anchors are deliberately invalid. The runtime owner's
configuration and activation methods are also private, non-reflected and have
no call site; their separate compiled receipt anchors are deliberately invalid,
so arbitrary caller hashes plus a suppression Boolean cannot authorize visibility.
A future reviewed source change must pin the exact accepted-R33 and separate
narrow successor receipts, recompile, and explicitly expose or call the private
implementations before any package write or presentation is reachable. Exact
source-layout comparison remains bit-for-bit, while native HISM matrix readback
alone uses bounded `0.02 cm` translation and `1e-5` rotation/scale tolerances.
The source suite passes `11/11`, including the self-authentication, runtime
authority-boundary, native-readback and installed UE5.5 header audits. It must
not be treated as an Unreal result: no candidate mesh package has been materialized,
488 V4-main form resolutions and exact before/after transform receipts remain
outstanding, and trust activation, UE5.5 compile/link, cold reload, map/PIE
integration, capture and performance proof have not run.

The separate, unadmitted, source-only
[Tropical umbrella hero candidate](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroCandidate/README.md)
and [dormant post-R33 integration scaffold](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/README.md)
narrow step 2 to six source HISM instances already native-classified as
`umbrella`: four in `V4HeritageUmbrellaSilhouetteProxies` and two in
`V5DLandmarkTreesUmbrella`. All 720 unresolved V4-main raster rows remain
excluded. The deterministic ordered route is `C/B/B/B/A/B`, giving the exact
distribution A1/B4/C1. This classification does not make the candidate native
or accepted; it remains source-only, non-native and non-live.

The source implementation reads the six ordered native world transforms,
requires caller copies to match bit-for-bit, and requires the canonical
before/after transform SHA-256 to remain identical. It snapshots the exact
4+2 counts, all transforms, visibility and hidden-in-game state; suppresses
only the two source components' presentation with propagation disabled; and,
on every later failure, clears the candidate, restores both source states,
revalidates counts and transforms, and attempts physical cleanup of both
namespaces proven empty at entry. These are fail-closed source semantics, not
native rollback proof. The editor materialize/swap, runtime selection and
runtime activation gates are three independent compile-time `false` gates with
unset current/future trust anchors. The sole reflected endpoint is read-only
inspection; no package write, activation, map save or numbered successor is
reachable.

The sealed [integration contract](../unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/tropical_umbrella_hero_post_r33_integration.source_contract.v1.json)
is 21,792 bytes with SHA-256
`C3CAD6385FD35AB648A2A7064B8BD14A9430227728282F367F942D9B1FCF9714`;
all 27 pins are exact. Focused verification passes `29/29`; the independent
adjacent run reports 67 passed and two expected native-only skips. Candidate
check and self-test both pass with byte-identical clean-build output. The prior
sole stale OuterContextVegetation R33 receipt chain is repaired through an exact
transitive reseal. The combined R33/outer/rim/promotion focused-plus-adjacent
suite is green at `83/83`. The outer candidate unittest passes `9/9`;
`--check` is `PASS`; and `--self-test` is `PASS`, with all five outputs
byte-identical.

Nothing here is visible in the current R30-19 level. Human-accepted R30 capture,
R31 commit plus human capture, R32 commit plus human capture and R33's
eight-image human acceptance must succeed in order before a separately
reviewed, recompiled explicit successor may be considered. UE5.5 compile/link,
import/save/cold reload, injected rollback after both suppression branches,
native LOD/culling, wind/alpha, shadow/subsurface, equal-condition frame-time
and memory, fixed-view comparison and explicit human visual acceptance remain
outstanding. The candidate has no geography or survey authority and no
collision, navigation, line-of-sight, RF, sensor or terrain authority.

The isolated `TreeRealism/PalmHeroCandidate` pack is the prepared higher-density
palm source option: 104,244 / 37,968 / 9,864 triangles across LOD0/1/2, stable
`Bark`/`FrondLive`/`FrondDry` slots, exact root contact, and byte-pinned 2K CC0
Poly Haven bark textures. A separate, unexecuted source scaffold now provides
the hash-pinned import/material route, exact three-LOD and slot validation, an
optional runtime source with the admitted palm as mandatory fallback, and an
exact eight-package materialization implementation. The only reflected
Blueprint endpoint, `InspectPalmHeroReceipts`, is inspection-only and cannot
write or turn caller-pinned receipts into execution authority. The eight-package
materializer is private and non-reflected; before any write it requires two
distinct compiled SHA-256 trust anchors, both deliberately left as invalid unset
sentinels. A future separately reviewed source edit must pin the exact
human-accepted R33 and narrow authorization receipt hashes, recompile, and
explicitly call or expose that private method. Future authorization is also
limited to its exact field roster. The dormant implementation retains the
partial/unexpected recursive-namespace denial, empty-entry rollback boundary,
per-output candidate/source/R33/authorization metadata, existing texture import
source, retained-JPEG payload, format and DirectX normal checks, and the installed
UE5.5 legacy OBJ importer's unconditional Y-axis handedness conversion with
separate pinned UE-space bounds. The focused source contract passes `16/16`, and
the combined Palm-candidate/PalmHero/tree-geometry regression passes `48/48`.
This is still not a
botanical or current-site match, and it may not silently replace the existing
palm contract.
The future transaction must still prove UE5.5 compile/link, native OBJ import
semantics, save/cold reload, injected-failure rollback, source resolution, leaf
transmission, subsurface, wind, LOD aliasing, human visual acceptance and
performance. Admission of either tree candidate can occur only in a separate
successor transaction after R33 acceptance, and neither authorizes a new
numbered stage here.

**Outer-context vegetation adjunct (source only; no new stage).** The
unnumbered `Vegetation/OuterContextVegetationCandidate` is a prepared,
render-only context input derived from the pinned 25 August 2026 OpenStreetMap
snapshot under ODbL with required OpenStreetMap attribution. Its exact strict
`300 < r <= 1000 m` census is 145 sanitized, unjittered local-XY placements
under the existing pinned hero-local axis convention: 32 of 33 `natural=tree`
points plus 113 of 115 complete-bin samples from 22 `natural=tree_row` ways
after exactly two `< 4 m` point-priority removals. Its checker, two
byte-identical clean builds, and `9/9` focused tests pass.

The corresponding unnumbered dormant integration scaffold now mirrors those
145 keys, XY values, yaws and scales as exact fixed-point data and routes them
into 12 render-only HISM buckets. It never samples terrain or invents Z: a future
transaction must supply the exact ordered key-to-finite-Z roster with separate
terrain-contact evidence. It validates the exact four TreeRealism fallback
forms or 12 GeometryVariationCandidateV4 meshes, four LODs, material slots and
response materials. Exact source-layout validation remains zero-tolerance;
native HISM readback alone uses bounded 0.02 cm translation and `1e-5`
rotation/scale tolerances. Its private configuration and presentation methods
have no call site, four compiled trust anchors remain invalid, and its
distribution gate is compiled false. The source integration suite passes
`12/12`, giving `21/21` focused candidate-plus-integration checks.

The explicit `publicDistributionReady=false` state may change only after future
derivative-database ODbL/share-alike, machine-readable-access and owned
runtime-visible-attribution gates are satisfied; both candidate and integration
directories are excluded from the GitHub-free source-assets bundle meanwhile.
Any future isolated owner must remain hidden in `GooglePrimary` and
`CwtWarming` and may be visible only in `CwtPresented` and `SafeLocal`; it is
independent of the building surface-optics candidate. Admission remains gated
on the hash-pinned, human-accepted R33 receipt and separate successor-transaction
authorization. Repository `SourceAssets` is already a D:-backed junction, so
the storage boundary is no live native `Content`/`Binaries`, map or DLL mutation,
not a literal no-D-write claim. All candidate Z values remain null, and this is
not a native result or proof of current/complete tree coverage, terrain contact,
species or survey truth, performance, human visual acceptance, collision,
navigation, line-of-sight, RF or sensor authority.

The source-only
`Vegetation/OrdinaryDistanceGrassSurfaceCandidate` is a prepared input for step
3. It reuses the pinned CC0 ambientCG `Grass001` 2K maps at their published
1.4 m tile scale, blends two rotated phases across 7 m, and layers restrained
3.5/8.4/18.2 m variation. Its corrected +/-5 m world-distance-band audit
records contrast/detail-delta of `0.080690/0.024243`,
`0.081796/0.025028`, and `0.082635/0.025432` at 20/35/50 m respectively,
passing the unchanged `0.08` and `0.0025` source thresholds. This is a
deterministic CPU lookdev result only; a successor must still prove native
material wiring, matched-camera readability, exclusion masks, crossfade/moire,
temporal stability, performance and human acceptance.

An unnumbered source scaffold now defines the corresponding dormant assets-only
materialization and optional runtime-selection boundaries. Its exact design is
five 2K retained-JPEG textures and one standalone 25-expression material under
an isolated namespace, with the current lawn material preserved as mandatory
fallback and no map/component edit. The graph permits only Base Color,
Roughness, Normal and Ambient Occlusion; height is retained but has no geometry
route, and physical-material/Nanite authority is denied. The rotated phase's
tangent normal is mapped back through the inverse UV rotation before blending.
Registry, live-package and physical filesystem inventories must match the exact
six-asset roster, and a failed fresh run must prove the output namespace
physically absent. Only read-only receipt inspection is reflected or public;
caller-supplied paths and hashes cannot authorize execution. The private,
non-reflected materializer checks two distinct compiled SHA-256 trust anchors
before any source load or write, and both are deliberately unset. Future
authorization also requires its exact contracted field roster without
extensions. A reviewed source change must pin the human-accepted R33 receipt
and distinct narrow authorization, recompile, and explicitly expose or call the
private implementation before materialization becomes reachable. The `16/16`
source-integration tests pass; the candidate, integration and related R32 turf
regression run is `60/60`. UE5.5 compile/link, import/save/cold reload,
injected rollback,
native map binding, exclusion/crossfade, capture, performance and human review
remain mandatory future evidence.

`Vegetation/GradedTurfPresentationIntegrationV2` is the prepared higher-impact
route for the live lawn overlay. Instead of assigning the standalone opaque
candidate, it clones the exact accepted R23B `M_IPV5D_LawnMacroVariation`
material and preserves `BLEND_Masked`, the `0.5` clip and the complete 64-point
estate-boundary/core-feather OpacityMask graph. It grafts the existing
Grass001 dual-phase treatment onto Base Color, Roughness, Normal and Ambient
Occlusion only; Specular and OpacityMask remain exact inherited responses, and
WPO, displacement and PDO remain disconnected. R29 continues to own exactly
6,144 placements and R32 exactly 4,608, while the masked surface is designed to
carry presentation continuously through their 65--90 m blade fade to the 95 m
probe. Runtime selection and activation are separate compiled `false` gates;
the exact `V5DGroundMacroVariationOverlay` snapshot/apply/restore path is
private and uncalled. The one-material editor materializer is likewise private,
non-reflected and unreachable behind distinct invalid accepted-R33 and future-
authorization anchors. Because the upstream OrdinaryDistance material is also
unmaterialized, a third byte/SHA-256 dependency pin remains deliberately unset;
a reviewed source change must bind its accepted cold-reloaded package. Before
copying, V2 then requires that clean persisted package and independently checks
all 25 node payloads and topology. Final validation compares exact selector
bits for inherited OpacityMask/Specular and permits no property or expression
outside the six intended roots. Its focused source contract passes `14/14`. Nothing has
been materialized, saved, map-bound, compiled as native work, captured or
accepted, so the 0--95 m presentation remains future native proof rather than
current visibility.

The unnumbered `Surroundings/BuildingSurfaceOpticsCandidate` is the prepared
appearance-only input for step 4. It preserves the exact R31 43,448-triangle,
17-slot, 1,388-group shell and identity transform while adding bounded
glass/frame separation and multiscale wall/roof response. Pane variation uses
the exact R31 aperture-cell identity rather than an independent nominal grid;
the deterministic audit reports zero variation within each of 126 qualified
cells and differing signals for all 16 bounded adjacent-cell probes. Its
`12/12` source tests and two deterministic self-checks pass. It remains an
offline CPU presentation candidate: it does not establish actual-site material
truth and may not alter footprints, massing, transforms, collision, navigation,
line-of-sight, sensor, RF, terrain or geospatial authority. A future application
requires the hash-pinned human-accepted R33 receipt, separate explicit
successor-transaction authorization, UE5.5 material construction, matched
native captures, performance proof and human acceptance.

The matching unnumbered post-R33 integration scaffold is also source-complete.
It defines an isolated clone of the exact R31 master plus seventeen ordered
slot instances and the seven intended presentation-channel connections, while
retaining R31 as the mandatory fallback and exposing no map/component binding.
Caller-supplied hashes cannot authorize execution: the only reflected endpoint
is read-only inspection, while the private materializer requires two compiled
trust anchors that are deliberately unset. Its focused candidate/integration
run passes `27/27`, with an independent UE5.5 installed-header and semantic
audit finding no remaining source defect. This does not prove native compile,
shader output, save/cold reload, component application, captures, performance,
Nanite/raster parity, authority regressions or visual acceptance; those remain
future transaction evidence after R33 acceptance and a reviewed trust-anchor
source change.

The separate unnumbered
`Surroundings/BuildingFootContactCandidate` now prepares the building-foot
subset of step 5 without changing the R31 shell. Its exact source audit proves
that 1,370 of 1,388 retained groups place 23,746 opaque wall triangles on the
existing source plane; the eighteen elevated groups and their 614 wall
triangles begin at or above 3.2 m and receive zero response. The bounded cue is
fully active only through source Z 0.10 m, reaches zero at 1.25 m, excludes
glazing, and is capped at 8% base-colour darkening, +0.05 roughness and 7% AO
darkening. Candidate tests pass `9/9`. Its matching dormant integration keeps
the R31 Default Lit master, all 42 existing expressions and the existing Base
Color, Roughness, Normal, Metallic and Specular edges; it adds one AO
input/output and emits one isolated master plus seventeen exact R31-fallback
clones. The only reflected editor endpoint is read-only inspection, both
compiled trust anchors are deliberately invalid, the private materializer has
no production call site, runtime candidate selection is compiled off, and a
pre-existing output namespace is denied. Integration tests pass `14/14`, for
`23/23` across candidate and scaffold; the adjacent optics/R31 regression run
passes `49/49`. This roster is an alternative to, not a cumulative layer over,
`BuildingSurfaceOptics`; a future accepted look must deliberately compose the
two effects or choose an accepted predecessor. Native UBT/UHT/link and shader
compile, save/cold reload, source-plane visual alignment, guarded binding,
Nanite/raster parity, performance, authority regression and human acceptance
remain required.

The source-only
`Surroundings/BuildingOpticsContactCompositionIntegration` now makes that
cumulative choice explicit. Its exact order is R31, then the complete seven-
channel BuildingSurfaceOptics response, then bounded BuildingFootContact.
Contact changes only the post-optics Base Color, Roughness and Ambient
Occlusion; tangent Normal, Metallic, Clear Coat and Clear Coat Roughness pass
through unchanged. The synthetic-plane response stays full only through
0.10 m, reaches zero at 1.25 m, excludes glazing and remains capped at 8% Base
Color darkening, +0.05 Roughness and 7% AO darkening. It preserves the R31
43,448 triangles, seventeen ordered slots, 1,388 groups and identity transform,
and proposes one isolated Clear Coat master plus seventeen direct instances.
The optics roster is the exact preferred fallback and R31 the exact ultimate
fallback. Selection and activation are independently compiled `false`; only
inspection is public, while the private non-reflected materializer has no call
site and two invalid trust anchors.

The final source closure compares the exact normalized 42-node R31 graph,
including every unchanged node class, payload and input edge, every root
selector, the Specular route and the original
`R31.SurfaceRoughnessA -> MP_Roughness` route. It requires all 42 nodes to be
root-reachable and rejects orphans and custom-output substitutions. The first
23 custom Surface inputs remain exact; new inputs 23--25 validate exact names,
source expressions, output indices, independent
`FExpressionInput::InputName` values and component masks. All seven Surface
outputs validate exact ordered names, zero masks and
`bShowOutputNameOnPin=true`. All eleven presentation override families are
three-way exact across candidate, optics and R31. Editor validation denies
direct physical material, `PhysMaterialMask` and physical-material maps across
that chain, while runtime validation denies both direct `PhysMaterialMask` and
effective `GetPhysicalMaterialMask()` for candidate and preferred-optics Clear
Coat rosters.

The independently rerun focused source contract passes `17/17`; the combined
result is `68/68`, including `51/51` adjacent optics/contact/R31 checks. Its
final contract is exactly 12,632 bytes with SHA-256
`021C445DC42BD8461FE2741792C5E013D3B55F86DBA6FFC050C17760AEA09E6A`.
No asset is materialized or live. Unreal launch, UBT/UHT compile/link, native
automation execution, material shader compile, save/cold reload, guarded map
or component binding, source-plane alignment, capture, performance proof,
Nanite/raster parity and human visual acceptance all remain outstanding.

The unnumbered
`PublicRealm/JunctionContinuityCandidate` now prepares the public-road junction
subset of step 5 without inventing a patch. Its audit reconstructs 1,141 exact
road-base triangles as valid touching Core/Fallback surfaces with zero gap or
overlap, and all 3,423 road-corner UV0 records equal logical hero-local XY
metres. It therefore admits only an indexed, source-triangle-area-weighted
normal override at the sixty duplicate/tolerance-equivalent corner clusters
shared across Core and Fallback: 355 rows, with a maximum per-row adjustment of
`0.044152281` degrees below the fixed `0.05`-degree cap. Same-role clusters are
excluded, both source OBJs remain byte-identical, and geometry, UVs, topology,
slots, transforms, terrain, geography and every simulation authority remain
unchanged. The deterministic check and `10/10` tests pass. A separate dormant
`PublicRealm/JunctionContinuityIntegration` scaffold now prepares exactly two
isolated sibling-namespace mesh copies and applies only the listed 174 Core and
181 Fallback normal rows. It hash-seals its nine-file implementation closure,
keeps the existing Core/Fallback pair mandatory, compiles runtime selection and
activation off independently, exposes inspection only, and leaves its private
materializer unreachable behind two distinct invalid trust anchors. Its
recursive fresh-namespace and rollback checks reject partial or unexpected
output. The integration suite passes `14/14`, for `24/24` focused checks.
UE5.5 compile/link, reviewed anchor activation, materialize/save/cold reload,
exact native normal-only readback, sharp-bend/four-way captures, Nanite/raster
parity, performance/authority regressions and human acceptance remain future
proof.

The legacy PublicRealm provider-clip evidence retains its immutable 78,979-byte
historical ContextPolicy actor at SHA-256
`9AFE7C7BD858D6C533AF36A005AFC820AA7037D73850B8DA8BACBF9AAC8208BC`.
The separate `PublicRealm/NativeSourceClosure/VQSP20260905` promotion closure
now also preserves the ten evolved inputs needed to reproduce the immutable
5 September successor manifest. Its completed 52-file source snapshot has
manifest SHA-256
`9DF232B3724A43C621DFAD03E611D6DC5B5C779135577CC1DDA375B293004072`;
the 31,206-byte completion receipt at SHA-256
`4268ED027D07B31CAC896AA8F4268038B227FE8E3AEC9268A7119A2621572AB4`
records `State=COMPLETE`, and all 52 entries independently rehash to the
manifest. The closed ten-row snapshot helper reconstructs canonical historical
inputs without overwriting current repository or native files; the two focused
promotion suites pass `21/21`. This is replay isolation, not rollback,
promotion authority or a post-R33 asset.

The unnumbered `Terrain/OuterContextRimSeamCandidate` prepares only a denied,
fail-closed diagnostic recipe for the 950--1,000 m subset of step 5. It keeps
the exact R29 response through 950 m, proposes Base Color/Roughness blending
only, uses direct endpoint returns, and passes the R29 opacity mask unchanged.
It adds no geometry, height, normal, UV, displacement, transform or authority.
The stale direct-source closure has been repaired from native-receipt and
recorded-patch chronology: current MacDonald and Temasek capture harnesses are
now pinned while the older native-factory and cooked-provenance closures remain
explicitly historical. OuterGround passes `14/14`; build/check/self-test and
the rim's `10/10` focused tests also pass. Activation nevertheless remains
false because native evidence has not classified the seam as material-only,
the actual topmost outer renderer is unresolved between the policy material
and the separate R28 +0.5 cm overlay, and the historical cooked identity still
requires separate admission. No integration scaffold exists, and the recipe
must not be used to hide missing Cesium data, a provider transition or an
unknown vertical datum.

The official-source reference in `docs/ISTANA_PUBLIC_VEGETATION_REFERENCE.md`
may guide the silhouette/form review in step 2. It supports only broad visual
targets such as umbrella, dense-dome, high-fork, columnar and palm forms. It
does not authorize invented private coordinates, a complete current inventory,
species assignment to individual simulated anchors, or any change to collision,
sensor, RF, terrain or geospatial authority.

## Common proof boundary

- Record distinct file and decoded-pixel hashes for every PNG.
- Revalidate map, DLL, source-tree, material, and mesh pins immediately before and after capture.
- Prove exact census/transform equality for 729 general trees, seven landmark anchors, 6,144 R29 grass placements, 4,608 R32 turf placements, 1,174 facade-covered parts, and the 43,448-triangle/17-slot surroundings shell.
- Prove all new presentation primitives are non-colliding, non-navigable, and have no line-of-sight, RF, sensor, or geospatial authority.
- Re-run the exact collision/LOS/RF/sensor query fixtures and require unchanged hashes.
- On target hardware, require p95 frame time and draw calls no more than 10% above the accepted R33 baseline, owned-process private memory below 12 GiB, and no free-virtual guard breach.
- Never infer visual acceptance, performance acceptance, botanical truth, survey accuracy, or current-site completeness from a source contract or mechanically decoded PNG.

## Tree acceptance

Capture 8-15 m trunk/root views, 35-55 m normal approach views, 75-100 m crown silhouettes, and one canopy oblique, plus a slow dolly through every LOD transition.

Reject:

- any foreground tree that visibly floats or sinks;
- adjacent foreground duplicates with the same silhouette, scale, and orientation;
- a crown-mask discontinuity above 3% through an LOD change;
- any connected disappearing branch region larger than 5 pixels at 1440p;
- clipped-black crown masses that erase branch/crown depth.

For the palm option, also require matched front, oblique, crown-plan and root
contact views at LOD0/1/2. Reject fused leaflet slabs, hard radial symmetry,
single-plane fronds, visible root hovering, or an obvious silhouette jump at an
LOD transition.

All five broad proxy morphologies must remain distinguishable. This is a visual diversity check, not botanical identification.

## Turf acceptance

Capture matched 2 m, 8 m, 20 m, and 50 m views plus grazing probes at 65 m, 75 m, 90 m, and 95 m.

Reject:

- blade/tuft definition that is not visible at 100% image scale through the 20-50 m range;
- a continuous bald band wider than 0.5 m in the accepted lawn region;
- adjacent radial regions with median luminance discontinuity above 5/255 or edge-density change above 20%;
- a hard cutoff at 90-95 m;
- a periodic carrier-grid spectral peak greater than three times neighbouring frequency bins;
- grass over any hardscape exclusion region.

## Building acceptance

Capture the MacDonald oblique, 75 m, 20 m, 8 m, and a full 1 km oblique.

Require 1,174/1,305 facade coverage, 17/17 broad-shell material slots, all 11 R30 facade overrides, and zero fallback-grey, magenta, or missing materials. Building footprint/silhouette masks and transforms must remain pixel-identical to the accepted pre-pass baseline. At 20-75 m, glazing, frames, walls, and roofs must remain separately readable; repeated painted-dot grids and opaque-black windows are rejection conditions.

## Seam acceptance

Capture a low oblique at the 300 m boundary, a grazing view at the 950-1000 m annulus, a sharp road bend, a four-way junction, and three representative building-foot contacts.

Reject any connected crack, overlap, or z-fight wider than 2 pixels and longer than 50 pixels at 1440p, or a floating-road/building-foot gap larger than 2 pixels. An apparent CWT/local vertical mismatch remains a failed or explicitly labelled comparison limitation; it is never authorization to alter geography or simulation terrain authority.
