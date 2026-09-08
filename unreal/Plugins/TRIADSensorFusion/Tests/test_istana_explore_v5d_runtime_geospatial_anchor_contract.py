from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
EDITOR_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
PROVIDER_DOC = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Docs/"
    "IstanaExploreV5DProviderQuality.md"
)


def extract_braced_block(source: str, marker: str) -> str:
    marker_index = source.index(marker)
    open_brace = source.index("{", marker_index)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace + 1 : index]
    raise AssertionError(f"Unterminated C++ block after {marker!r}")


class IstanaExploreV5DRuntimeGeospatialAnchorContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_cpp = RUNTIME_CPP.read_text(encoding="utf-8")
        cls.editor_cpp = EDITOR_CPP.read_text(encoding="utf-8")
        cls.provider_doc = PROVIDER_DOC.read_text(encoding="utf-8")

    def test_runtime_tuple_matches_the_authoritative_cold_map_tuple(self) -> None:
        for fragment in (
            '#include "CesiumGeoreference.h"',
            'HybridGeoreferenceTag(TEXT("TRIADIstanaExploreV5DGeoreference"))',
            'CesiumDefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"))',
            "RequiredIstanaLongitudeDegrees = 103.84288055",
            "RequiredIstanaLatitudeDegrees = 1.30709615",
            "RequiredIstanaFallbackEllipsoidHeightMeters = 47.0",
            "RequiredCesiumScale = 100.0",
        ):
            self.assertIn(fragment, self.runtime_cpp)
        for fragment in (
            "IstanaLongitudeDegrees = 103.84288055",
            "IstanaLatitudeDegrees = 1.30709615",
            "IstanaFallbackEllipsoidHeightMeters = 47.0",
            "Georeference->SetScale(100.0)",
        ):
            self.assertIn(fragment, self.editor_cpp)

    def test_value_gate_rejects_every_cartographic_degree_of_freedom(self) -> None:
        gate = extract_braced_block(
            self.runtime_cpp,
            "bool HasExpectedIstanaGeoreferenceValues(",
        )
        for fragment in (
            "OriginPlacement == EOriginPlacement::CartographicOrigin",
            "OriginLongitudeLatitudeHeight.X",
            "RequiredIstanaLongitudeDegrees",
            "OriginLongitudeLatitudeHeight.Y",
            "RequiredIstanaLatitudeDegrees",
            "OriginLongitudeLatitudeHeight.Z",
            "RequiredIstanaFallbackEllipsoidHeightMeters",
            "RequiredCesiumScale",
        ):
            self.assertIn(fragment, gate)

    def test_world_gate_requires_unique_explicit_shared_anchor(self) -> None:
        gate = extract_braced_block(
            self.runtime_cpp,
            "bool ValidateExactIstanaGeospatialAnchor(",
        )
        for fragment in (
            "TActorIterator<ACesiumGeoreference>",
            "GeoreferenceCount != 1",
            "TaggedSiteClipCount != 1",
            "Georeference->Tags.Contains(HybridGeoreferenceTag)",
            "Georeference->Tags.Contains(CesiumDefaultGeoreferenceTag)",
            "HasExpectedIstanaGeoreferenceValues(",
            "Tileset->GetGeoreference().Get() != Georeference",
            "SiteClip->GlobeAnchor->GetGeoreference().Get() != Georeference",
            "SiteClip->GlobeAnchor->GetResolvedGeoreference() != Georeference",
            "Georeference->GetActorTransform().Equals(",
            "Tileset->GetActorTransform().Equals(FTransform::Identity, 0.001)",
            "SiteClip->GetActorTransform().Equals(FTransform::Identity, 0.001)",
        ):
            self.assertIn(fragment, gate)
        for mutation in (
            "SetOriginPlacement",
            "SetOriginLongitudeLatitudeHeight",
            "SetScale",
            "SetGeoreference",
            "SetActorTransform",
            "Modify(",
            "RefreshTileset",
            "SpawnActor",
            "Destroy(",
        ):
            self.assertNotIn(mutation, gate)

    def test_geospatial_preflight_precedes_transient_fog_mutation(self) -> None:
        apply = extract_braced_block(
            self.runtime_cpp,
            "ApplyTransientCurrentViewProviderWorkloadPolicy(",
        )
        geospatial_preflight = apply.index(
            "ValidateExactIstanaGeospatialAnchor("
        )
        clip_preflight = apply.index("ValidateProviderSiteClip(")
        mutation = apply.index("Tileset->EnableFogCulling = true;")
        self.assertLess(geospatial_preflight, clip_preflight)
        self.assertLess(clip_preflight, mutation)
        self.assertIn("before mutation", apply)
        self.assertIn("geospatialAnchorValidated=true", apply)
        self.assertIn("geospatialActorTransformsIdentity=true", apply)
        self.assertIn("uniqueCesiumTilesetRoster=true", apply)

    def test_runtime_resolution_rejects_every_extra_cesium_tileset(self) -> None:
        roster_gate = extract_braced_block(
            self.runtime_cpp,
            "bool HasExpectedVisualTilesetRoster(",
        )
        self.assertIn("TotalTilesetCount == 1", roster_gate)
        self.assertIn("TaggedVisualTilesetCount == 1", roster_gate)

        resolution = extract_braced_block(
            self.runtime_cpp,
            "ATRIADIstanaExploreV5DContextPolicyActor::ResolveSceneAndTileset(",
        )
        tileset_loop = extract_braced_block(
            resolution,
            "for (TActorIterator<ACesium3DTileset>",
        )
        self.assertIn("++TotalTilesetCount;", tileset_loop)
        self.assertIn("It->Tags.Contains(HybridVisualTilesetTag)", tileset_loop)
        self.assertIn("++TaggedVisualTilesetCount;", tileset_loop)
        self.assertLess(
            tileset_loop.index("++TotalTilesetCount;"),
            tileset_loop.index("It->Tags.Contains(HybridVisualTilesetTag)"),
        )
        self.assertIn("HasExpectedVisualTilesetRoster(", resolution)
        self.assertIn("totalTilesets=%d taggedVisualTilesets=%d", resolution)
        for mutation in ("Destroy(", "SetActorHiddenInGame", "SetVisibility"):
            self.assertNotIn(mutation, resolution)

    def test_hybrid_validation_fails_closed_on_anchor_drift(self) -> None:
        validation = extract_braced_block(
            self.runtime_cpp,
            "ATRIADIstanaExploreV5DContextPolicyActor::ValidateHybridContext(",
        )
        self.assertIn(
            "ValidateExactIstanaGeospatialAnchor(GetWorld(), Tileset, Error)",
            validation,
        )
        self.assertIn("ISTANA_EXPLORE_V5D_HYBRID_INVALID", validation)
        self.assertIn(
            "cesiumGeoreference=(%.8f,%.8f,%.3f) "
            "geospatialAnchorValidated=true",
            validation,
        )
        self.assertIn("geospatialActorTransformsIdentity=true", validation)
        self.assertIn("uniqueCesiumTilesetRoster=true", validation)
        for authority in (
            "bCesiumLayerIsVisualOnly",
            "bCesiumCollisionNavigationSensorOrRfAuthority",
            "bProviderContentClippedFromAuthoredCore",
            "cesiumCollisionNavigationSensorRfAuthority=false",
        ):
            self.assertIn(authority, validation)

    def test_native_boundary_test_covers_exact_and_drifted_values(self) -> None:
        for fragment in (
            "TRIAD.Istana.ExploreV5D.Hybrid.GeospatialAnchorValues",
            "Exact Istana cartographic-origin values are accepted",
            "A non-cartographic origin placement is rejected",
            "A longitude drift outside the exact tolerance is rejected",
            "A latitude drift outside the exact tolerance is rejected",
            "An ellipsoid-height drift is rejected",
            "A Cesium scale drift is rejected",
        ):
            self.assertIn(fragment, self.runtime_cpp)

    def test_native_boundary_test_covers_unique_visual_tileset_roster(self) -> None:
        for fragment in (
            "TRIAD.Istana.ExploreV5D.Hybrid.UniqueVisualTilesetRoster",
            "Exactly one Cesium tileset carrying the visual-provider tag is accepted",
            "An extra untagged Cesium tileset is rejected",
            "A sole untagged Cesium tileset is rejected",
            "Multiple tagged Cesium tilesets are rejected",
            "A world without a Cesium tileset is rejected",
            "HasExpectedVisualTilesetRoster(1, 1)",
            "HasExpectedVisualTilesetRoster(2, 1)",
            "HasExpectedVisualTilesetRoster(1, 0)",
            "HasExpectedVisualTilesetRoster(2, 2)",
            "HasExpectedVisualTilesetRoster(0, 0)",
        ):
            self.assertIn(fragment, self.runtime_cpp)

    def test_docs_state_both_alignment_and_accuracy_boundaries(self) -> None:
        normalized = " ".join(self.provider_doc.split())
        for fragment in (
            "## Runtime geospatial anchor and original-layout isolation",
            "longitude `103.84288055`, latitude `1.30709615`",
            "`47.0 m` WGS84 ellipsoid height",
            "documented SRTM30-derived fallback, not surveyed local grade",
            "exactly one Cesium georeference",
            "exactly one Cesium tileset in the entire runtime world",
            "The Google visual tileset must bind directly to that actor",
            "georeference and tileset actors must retain identity transforms",
            "This check runs before the transient fog-culling write",
            "does not hide, move, or destroy an unknown extra tileset",
            "does not move the authored model",
            "does not move the authored model, rewrite the saved map",
            "collision, navigation, terrain, sensor-occlusion, or RF authority",
            "an explicit ellipsoid/geoid conversion",
        ):
            self.assertIn(" ".join(fragment.split()), normalized)


if __name__ == "__main__":
    unittest.main()
