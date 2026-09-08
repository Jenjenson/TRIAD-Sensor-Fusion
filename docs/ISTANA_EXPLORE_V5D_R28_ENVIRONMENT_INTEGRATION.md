# Istana Explore V5D R28 environment integration

## Outcome and boundary

R28 is a focused fallback-presentation successor for the public streetscape
views. It adds:

- 1,923 sanitized public-road ribbon segments between 300 m and 1 km, with
  generic dry asphalt, low kerb, sidewalk, and verge geometry;
- 14,786 projected window panes plus physically offset frames, selected
  awnings, and 0.58 m parapet silhouettes across 128 deterministically selected
  building parts, including all 40 MacDonald/Temasek priority-sector parts; and
- a greener texture-free override on a render-only copy of the exact existing
  outer-ground annulus, lifted 0.5 cm to cover the khaki loading surface.

It does not replace the frozen current-surroundings shell, public-realm mesh,
outer-ground mesh, collision, navigation, sensor occlusion, RF geometry or RF
materials. Road/sidewalk widths are generic visual assumptions. Their vertical
coordinates now come from deterministic barycentric sampling of the exact
hash-pinned existing synthetic `SM_IstanaPublicView_Terrain.obj`, with the
  configured surface-relative lifts preserved. This is not DTM, survey, Cesium
  height, collision, sensor, RF, or route/access authority. Sample-only radial
rim clamping handles width vertices just beyond the inscribed 1 km terrain
polygon without changing output XY. Whole segments whose generated surfaces
would enter the protected 0–300 m disc are omitted. No feature name, source ID,
raw tag, restricted way, or operational/security detail is emitted.

The isolated R28 Unreal material graph is also texture-free but no longer
single-scale or constant-roughness. It combines smooth deterministic macro and
micro value-noise, per-material roughness breakup, and optional paving-joint
cues. Asphalt uses a fine aggregate scale; sidewalks and kerbs receive restrained
joint definition; verge and outer-ground greens receive separate microstructure.
The two glazing instances alone also apply a restrained, deterministic Fresnel
grazing-angle lift to the existing dark base colours; all eight non-glass
instances pin that strength to exact zero. This is a bounded view-dependent
visual cue in the same opaque master, not transparency, transmission,
refraction, screen-space reflection, or measured glazing. It remains a visual
assumption pending guarded native material compilation and fresh capture.
The graph has no texture inputs, normal output, displacement, world-position
offset, physical material, collision, navigation, sensor, or RF effect. The
connective-public-realm OBJ and preview MTL remain byte-identical; the expanded
architecture OBJ and manifest have new exact source pins.

The pass deliberately makes no global exposure, light, camera, or anti-aliasing
change. Those shared settings remain under the accepted R27 dynamic-range and
capture contracts; changing them in this focused transaction would make the
surroundings result harder to attribute and risk unrelated scenes.

## Repo checks before any native mutation

Run from the repository root with CPython 3.11.x. CPython 3.11.9 produced the
current authoritative bytes, but the contract intentionally admits all 3.11
patch releases. The generator fails before output mutation on another Python
implementation or major/minor version.

```powershell
python -B unreal\SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\build_r28_environmental_dressing.py --check
python -B -m unittest unreal.Plugins.TRIADSensorFusion.Tests.test_istana_explore_v5d_r28_environment_contract -v
```

Both commands must pass. The generated manifest admits 149,758 architecture
triangles, 449,274 source corners, 32,310 connective-public-realm triangles,
128 selected building parts (all 40 priority-sector parts), and 1,923 road
segments. The architecture stays below explicit hard ceilings of 128 parts,
15,000 windows, 150,000 triangles, and 450,000 source corners; its OBJ is
59,534,571 bytes rather than the rejected near-cap 80,591,984-byte prototype.
It also proves
64,620 resolved terrain samples, zero unresolved samples, 99.7585887% direct
coverage, 156 deterministic rim-clamped samples, a 3.035870501 m maximum sample
overrun, zero interpolation/repeated-XY seam disagreement, a 0.472485646 degree
maximum surface slope, zero computed lift/horizontal-alignment error, and a
300.865118675 m minimum generated radius. The former flat-road baseline had a
1.218944035 m P95 absolute terrain-height mismatch.

The legacy 85-part policy remains the frozen before comparator, not a retention
requirement: 41 legacy parts remain and 87 newly selected parts provide the
bounded successor set. Across the 300–500 m, 500–750 m, and 750–995 m rings,
selected counts rise from 5/24/56 to 25/39/64, while sectors meeting the
declared quota rise from 0/4/5 to 8/8/8. The four enforced R28 view envelopes
are audited separately from the two exact R24 context cameras named below.
The deterministic camera-facing-facade/distance-squared proxy rises from
0.029220547 to 0.443796940 for the co-located central R28 camera family and
from 0.877326528 to 0.923058336 for the R28 landmark-street view. It rises from
0.656445570 to 0.721584324 for the exact MacDonald R24 context and from
0.700601818 to 0.748637276 for the exact Temasek R24 context. This proxy is a
selection/audit measure, not raster visibility, occlusion, or proof-capture
acceptance; fresh native captures remain required.

## Callable seams

Runtime header and actor:

```text
TRIADIstanaExploreV5DR28EnvironmentActor.h
ATRIADIstanaExploreV5DR28EnvironmentActor
```

The runtime API is:

```cpp
bool ConfigureR28Environment(
    const FTRIADIstanaExploreV5DR28EnvironmentAssets& InAssets,
    bool bInitialProviderReady,
    FString& OutError);
bool SetProviderReady(bool bInProviderReady, FString& OutError);
bool ValidateR28Environment(FString& OutReport) const;
```

`providerReady=false` means all three fallback renderers are visible;
`providerReady=true` means all three are hidden. A failed readiness transition
restores the previous state. Components have `bHiddenInSceneCapture=true` but
do **not** carry `TRIADHumanOnlyOverlay`: EO/event scene-capture feeds exclude
them, while Player0/manual proof screenshots retain them.

Editor CDO:

```text
/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR28EnvironmentEditorLibrary
```

Callable functions:

```text
EnsureR28EnvironmentAssets
ValidateR28EnvironmentAssets
ApplyR28EnvironmentToLoadedV5DHybridMap
ValidateR28EnvironmentInLoadedV5DHybridMap
```

`ApplyR28EnvironmentToLoadedV5DHybridMap` accepts only the exact loaded package
`/Game/Maps/Istana_PublicView_Explore_v5d_hybrid`, requires zero predecessor
R28 class/tag actors, spawns one identity actor initially provider-not-ready,
and does not save the map. Its success marker is
`ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_APPLY_PASS`. The caller owns the combined
transaction's one map save.

## Guarded native transaction

This is a specification for the native wrapper; it is not authorization for an
unguarded copy or editor launch.

1. Require an explicit execute switch and caller-supplied exact predecessor
   pins. On 2026-09-06 at 06:55 SGT the observed R27 map was 36,335,930 bytes,
   SHA-256
   `9C9660B02F3B9FBF8C679182E8EB39B8FECD0A618AECA6ED546AAD39E0C895E5`.
   Treat this only as the observed handoff, not a timeless pin: refuse if the
   caller's reviewed value differs from the current file.
2. Verify the CAPSTONE process (PID 7272 in the current session) remains alive
   and outside the transaction. Never stop, suspend, inject into, or reuse it.
   Refuse if any Unreal Editor/UBT process would make the build non-exclusive.
3. Create one new transaction journal. Before changing anything, copy the exact
   map package, both plugin DLL/PDB/module rosters, every source file to be
   synchronized, relevant Intermediate object/dependency artifacts, and an
   explicit initially-absent receipt for every R28 asset package. Hash every
   journal entry.
4. Record pre-transaction hashes for the frozen/current-surroundings packages,
   LocalFallbackSuppressionV2 mesh, R25 facade materials, existing public-realm
   packages, existing outer-ground mesh/material packages, V3/V4/V5B source
   assets, the exact existing hash-pinned visual-terrain OBJ used by the R28
   drape, and all RF geometry/material inputs. These are immutable guards, not
   backup candidates for an intended mutation.
5. Synchronize only the reviewed R28 files plus the separately reviewed hybrid
   and provider-coupling sources. Copy the generated R28 source directory to
   the native project's identical `SourceAssets/.../R28EnvironmentalDressing`
   path. Verify, but do not replace or mutate, the separately pinned existing
   `SourceAssets/IstanaPublicView/Generated/SM_IstanaPublicView_Terrain.obj`.
   Do not broad-copy the repository or any unrelated dirty source.
6. Move only exact stale object/dependency artifacts for the changed source
   files into the journal, after resolving and verifying each absolute path is
   inside the native TRIAD project. Build UnrealEditor Development Win64 with
   hot reload disabled. Require fresh reflection for
   `UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary` and
   `ATRIADIstanaExploreV5DR28EnvironmentActor` and fresh runtime/editor DLL
   pins before launching any commandlet/editor phase.
7. Open the exact V5D hybrid map. Call `EnsureR28EnvironmentAssets`, then
   `ValidateR28EnvironmentAssets`. The isolated namespace must contain exactly
   13 assets: two meshes, one master material, and ten material instances. A
   partially populated namespace is a hard failure.
8. In the same loaded-world mutation, call
   `ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap(int64
   ExpectedPredecessorBytes, const FString& ExpectedPredecessorSha256, const
   FString& VerifiedExternalBackupFilename, FString& OutMessage)`. This
   reviewed combined endpoint reconfigures landmark vegetation, calls
   `ApplyR28EnvironmentToLoadedV5DHybridMap`, validates both changes, and owns
   the one map save. Require the
   `EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS` marker (or the exact clean
   idempotent marker), one R28 actor, and its
   `ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_VALID` report.
9. Unload through the established blank/predecessor world, cold-reload the V5D
   hybrid map, and require `ValidateR28EnvironmentInLoadedV5DHybridMap`,
   `ValidateIstanaExploreV5DR28VisualSuccessorMap` with marker
   `ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID`, R27 vegetation
   validation, Temasek Phase-2
   validation, R25 facade validation, public-realm validation, and full hybrid
   validation. Require provider-policy resolution of exactly one R28 actor.
10. Exercise provider readiness in both directions without saving: false must
    show all three R28 components; true must hide all three; returning false
    must restore them. Re-run the R28 and hybrid validators after each edge.
11. Re-hash every immutable guard from step 4. Any drift is a failure even if
    the map or screenshots look correct. Seal successor map/DLL/assets/source
    pins and the complete command/log inventory in `commit.json` only after all
    checks pass.
12. On any post-backup failure, close the owned Unreal process, restore exact
    journaled files and absent package states, remove only transaction-created
    R28 packages, cold-validate the predecessor map and immutable guards, and
    emit a rollback receipt. Never use a broad recursive delete or reset.

## Offline source audit (not proof capture)

The current repo-side audit lives at
`unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R28EnvironmentalDressing/OfflineVisualAudit/README.md`.
It binds the current architecture OBJ, connective-public-realm OBJ, MTL, and
manifest hashes; derives the 128-part, 14,786-window, 149,758-triangle census;
and receipts four CPU-rendered source views. Verify it without rendering or
mutation with:

```powershell
python -B unreal\SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\render_r28_offline_visual_audit.py --check
```

The canonical JSON deliberately records
`actualContextReferenceCoverage=0/4`, `comparisonPerformed=false`, and
`BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA`. None of its plan, height, or oblique
diagnostics is a Player0 frame, a sensor capture, a comparison to the actual
site, or evidence of live Unreal integration. It therefore cannot satisfy any
item below.

## Proof capture

Do not reuse an earlier R25/R27 image as R28 evidence. After the cold-valid
transaction, capture fresh full-resolution Player0/game-viewport frames with
the established deterministic dynamic-range settings:

1. the exact Temasek Phase-2 streetscape/context camera—the priority acceptance
   view for readable facade depth, dark glazing/frame variation, sidewalk,
   kerb, verge, asphalt, and removal of the khaki foreground;
2. the exact MacDonald streetscape/context camera, checking continuity through
   the middle distance rather than only the landmark foreground;
3. one low oblique 300 m seam view, proving the existing inner public realm and
   R28 continuation meet without a visible beige gap;
4. one low oblique 950–1,000 m annulus view, proving the greener outer-ground
   overlay covers the loading colour without Z-fighting; and
5. one matching scene-capture/sensor diagnostic proving the R28 actor remains
   absent there while the authoritative sensor/RF scene is unchanged.

Acceptance is qualitative as well as contractual: facades must read as planes,
frames, and roof edges rather than repeated painted dots; roads must connect
visually into sidewalks/verges; the foreground must not remain a flat khaki
field; there must be no material compile fallback, moiré, severe Z-fighting,
floating streets, duplicate landmark dressing, black tiles, or provider seam.
Keep both raw PNGs and a receipt containing file bytes, SHA-256, camera label,
map/DLL pins, provider state, and validation reports.
