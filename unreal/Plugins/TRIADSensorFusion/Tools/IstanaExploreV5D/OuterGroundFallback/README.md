# V5D outer-ground loading fallback

This package fixes a narrow presentation defect in the frozen R24 landmark
captures: the inherited synthetic ground ends at exactly 1,000 m, while four
of the six MacDonald House and Temasek Shophouse QA cameras cross that rim.
The resulting black foreground is therefore consistent with a missing local
render surface, not evidence that Cesium terrain itself failed.

The generated mesh is an isolated 1,000–1,250 m annulus. It copies all 128
points of the inherited 1,000 m seam exactly, samples only the final synthetic
25 m ring slope, and damps that slope to zero at the outer boundary. Five
50 m radial bands produce 1,280 render triangles. The 1,250 m edge covers the
capture harnesses' complete 1,200 m camera envelope with a 50 m guard.

This is deliberately a visual loading fallback, not terrain. It encodes no
buildings, roads, water, vegetation, markings, land cover, or geographic
surface classification. It has no collision, overlap, navigation, sensor
occlusion, RF, route, access, operational, security, DEM/DTM, survey, as-built,
or physical-material authority.

## Build and verify

Run the builder, its deterministic byte check, and the package tests:

    python unreal\Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\OuterGroundFallback\build_outer_ground_loading_fallback_v1.py
    python unreal\Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\OuterGroundFallback\build_outer_ground_loading_fallback_v1.py --check
    python -m unittest unreal.Plugins.TRIADSensorFusion.Tests.test_istana_explore_v5d_outer_ground_loading_fallback_contract -v

The builder hash-pins the source terrain, its negative-authority contract, the
shared UE 5.5 OBJ writer, both R24 placement receipts, and both QA harnesses.
The check rebuilds all outputs in a temporary directory and requires exact
byte equality with the committed package.

## Unreal integration status and boundary

The workspace integration is implemented and contract-tested. The isolated
editor factory imports the generated OBJ and material into
`/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback`; the V5D
context policy owns a native render-only component whose visibility follows
the existing provider-readiness hysteresis; and the guarded hybrid-map
migration assigns this mesh together with the locally suppressed surroundings
successor. The current R30-19 native map inherits that hash-pinned integration.
Its committed presence does not establish provider readiness or visual quality:
the R30 Player0 capture has no accepted image set, and the latest run09 attempt
failed closed before review and rolled back cleanly.

The integrated boundary requires:

1. import the OBJ with identity build scale, explicit normals, UV0, no
   collision, no navigation, no shadows, and no distance-field contribution;
2. keep it visible fail-closed before provider policy begins and whenever the
   existing local provider fallback is visible;
3. hide it only after the existing policy separately proves provider readiness
   at 98%, and restore it below 90%;
4. restore safe visibility on failure and EndPlay; and
5. obtain cold map validation plus both provider-loading and provider-ready
   visual evidence before claiming the native map is improved.

Global Cesium load progress is not landmark-specific readiness proof. This
source package does not change that limitation.
