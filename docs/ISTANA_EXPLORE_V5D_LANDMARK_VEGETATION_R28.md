# Istana Explore V5D Landmark Vegetation R28

R28 is an additive, appearance-only successor to R27. It addresses two
specific failures in the R27 acceptance evidence: sparse tan/straw turf with a
wide artificial bald band, and the undersized Phase-2 Temasek tree silhouette.
It does not claim botanical species, season, surveyed planting, collision,
navigation, sensor, RF, terrain, building, or geospatial authority.

## Repository-side result

- Each landmark lawn has 3,072 deterministic carrier instances, bounded by a
  4,096-instance per-site ceiling. The exact R11 Bermuda carrier and accepted
  R27 65--90 m visibility envelope remain unchanged.
- The exclusion remains the exact MacDonald 30 x 16 m or Temasek 40 x 25 m
  building footprint, plus 35 cm of render clearance. R27's extra three-metre
  visual margin is removed; grass is still forbidden beneath building
  geometry.
- The deterministic profile split is 70% manicured, 20% humid, 9% shade, and
  1% dry edge. Minimum-scale nominal carrier coverage is fail-closed at 0.75.
- Four new materials live only under
  `/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/`.
  They are exact R27/R23B derivatives with only the visibility provenance
  label and blade-colour custom expression allowed to differ. Dry/thatch
  influence is reduced and the final response is biased toward living green.
- All three Temasek Phase-2 anchor translations are preserved. Their visible
  roster is umbrella/dome/umbrella at anisotropic mature scales, so the former
  high-fork sapling is absent and the crowns do not collapse into spheres.
  MacDonald tree placement stays at the accepted R27 state.

## Native promotion transaction (required before claiming completion)

Do not copy or execute these changes ad hoc. Create
`scripts/Invoke-IstanaExploreV5DLandmarkVegetationR28NativeTransactionV1.ps1`
from the R27 wrapper and retain its fail-closed containment, reparse-point,
process-isolation, rollback, cold-validation, and receipt machinery.

The R28 wrapper must perform this exact sequence:

1. Run read-only by default; require an explicit `-Execute`, a bounded run
   token, and caller-supplied byte/SHA-256 pins for the committed R27 map,
   runtime DLL, and editor DLL. Require the four exact R27 material packages
   to exist and the four R28 target material packages to be absent.
2. Hash-pin and journal these six repository/native source pairs before any
   write:
   - `TRIADIstanaExploreV5DLandmarkVegetationActor.h`
   - `TRIADIstanaExploreV5DLandmarkVegetationActor.cpp`
   - `TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h`
   - `TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp`
   - `TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h`
   - `TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp`
3. Pin as immutable the R27 hybrid source pair, the complete Phase-2 compiled
   source closure, the R27 material quartet, all 16 Phase-2 Temasek packages,
   the committed map, and existing immutable R25/V2 content. Journal the map,
   six source destinations, four absent R28 targets, bounded TRIAD plugin
   `Binaries`/`Intermediate` files, and known project-global UBT sidecars.
4. Copy only the six sealed source files. Invalidate only their exact direct
   compile objects plus the corresponding UHT products and bounded build
   sidecars; do not recursively delete a build root. Build both TRIAD modules
   for Development Editor and prove the expected R28 markers exist in both
   DLLs while all immutable pins and protected UE 5.4 sessions remain stable.
5. Launch the dedicated UE 5.5 project, call
   `BuildOrValidateLandmarkGrassMaterialsR28`, and require exactly four new,
   clean, compiled packages. Cold-call
   `ValidateLandmarkGrassMaterialsR28` and
   `ValidateReusableLandmarkVegetationAssetsR28`.
6. A separate, deliberately small hybrid-library change is required before
   native execution: add an R28 apply endpoint which admits only the exact R27
   map and verified external backup, finds exactly one existing landmark actor,
   calls `ConfigureLandmarkVegetationActorR28`, saves the map exactly once,
   unloads/reloads, and validates the complete successor. Do not reuse the R27
   endpoint and do not spawn a second actor.
7. Cold-validate the successor map, then repeat the R28 apply call and prove it
   is byte-stable/idempotent. Recheck every immutable pin. On any failure,
   restore the exact map, source, build-surface, and absent/present asset states
   from the journal before emitting a failure receipt.
8. Capture at least the existing 12 m ground-grazing view and 19 m Phase-2
   Temasek view, plus one oblique context view. Acceptance requires continuous
   green turf without the central bald band, no straw-dominant foreground,
   three mature non-spherical tropical crowns at the preserved anchors, no
   clipping into either landmark, and no regression of buildings, provider
   context, camera controls, or sensor/RF behavior.

Until steps 1--8 succeed, reports must keep
`visualCaptureAccepted=false` and `captureRevalidationRequired=true`.
