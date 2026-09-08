from pathlib import Path
import re
import unittest


REPO = Path(__file__).resolve().parents[4]
HEADER = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h"
SOURCE = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp"


class IstanaExploreV5DContextPolicySuccessorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_runtime_admits_only_the_suppression_successor(self):
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV1/",
            self.source,
        )
        self.assertIn("GetNumTriangles() != 43492", self.source)
        self.assertIn(
            "UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance",
            self.source,
        )
        self.assertIn("LegacyProvenanceCount != 0", self.source)
        self.assertNotIn("GetNumTriangles() != 43544", self.source)
        self.assertNotIn(
            'TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/")',
            self.source,
        )

    def test_successor_section_census_is_exact(self):
        expected = {
            "MAT_BOTTOM_HIDDEN": 9446,
            "MAT_COMMERCIAL_HINT": 2166,
            "MAT_GENERIC_BUILDING_HINT": 14696,
            "MAT_HEALTHCARE_HINT": 278,
            "MAT_HOTEL_HINT": 414,
            "MAT_INDUSTRIAL_HINT": 8,
            "MAT_RELIGIOUS_HINT": 150,
            "MAT_RESIDENTIAL_HINT": 6340,
            "MAT_ROOF_COMMERCIAL_HINT": 1029,
            "MAT_ROOF_GENERIC_BUILDING_HINT": 5452,
            "MAT_ROOF_HEALTHCARE_HINT": 131,
            "MAT_ROOF_HOTEL_HINT": 169,
            "MAT_ROOF_INDUSTRIAL_HINT": 2,
            "MAT_ROOF_RELIGIOUS_HINT": 53,
            "MAT_ROOF_RESIDENTIAL_HINT": 2694,
            "MAT_ROOF_TRANSPORT_HINT": 132,
            "MAT_TRANSPORT_HINT": 332,
        }
        v1_roster = self.source[
            self.source.index(
                "const FCurrentSurroundingsMaterialSpec CurrentSurroundingsMaterials[]"
            ) : self.source.index(
                "const FCurrentSurroundingsMaterialSpec CurrentSurroundingsV2Materials[]"
            )
        ]
        rows = dict(
            (name, int(count))
            for name, count in re.findall(
                r'\{TEXT\("(MAT_[A-Z_]+)"\),\s*(\d+),', v1_roster
            )
        )
        self.assertEqual(rows, expected)
        self.assertEqual(sum(rows.values()), 43492)

    def test_outer_ground_mesh_contract_is_pinned(self):
        for fragment in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/",
            "SM_IPV5D_OuterGroundLoadingFallback_Render",
            "M_IPV5D_OuterGroundLoadingFallback",
            "GetNumTriangles() != 1280",
            "OuterGroundLoadingFallbackSourceCornerCount = 3840",
            "OuterGroundLoadingFallbackRenderVertexCount = 768",
            "GetNumVertices() !=\n            OuterGroundLoadingFallbackRenderVertexCount",
            "outerGroundLoadingFallbackSourceCorners=3840",
            "outerGroundLoadingFallbackRenderVertices=768",
            "FVector(-125000.0, -125000.0, -51.51655292)",
            "FVector(125000.0, 125000.0, 201.68388265)",
            "UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance",
            "Body->CollisionTraceFlag != CTF_UseSimpleAsComplex",
            "Mesh->bHasNavigationData || Mesh->GetNavCollision()",
            "bHasExactEditorSourceBuildSettings",
            "Mesh->GetLightMapCoordinateIndex() != 0",
            "Mesh->bGenerateMeshDistanceField",
            "Section.bEnableCollision || Section.bCastShadow",
            "Section.bAffectDistanceFieldLighting",
        ):
            self.assertIn(fragment, self.source)
        self.assertNotIn("GetNumVertices() != 3840", self.source)

    def test_native_component_is_render_only_and_fail_closed_visible(self):
        for fragment in (
            "OuterGroundLoadingFallbackRenderOnlyComponent =",
            'CreateDefaultSubobject<UStaticMeshComponent>(\n            TEXT("OuterGroundLoadingFallbackRenderOnly"))',
            "SetCollisionEnabled(\n        ECollisionEnabled::NoCollision)",
            "SetCollisionResponseToAllChannels(ECR_Ignore)",
            "SetGenerateOverlapEvents(false)",
            "SetCanEverAffectNavigation(false)",
            "SetCastShadow(false)",
            "bAffectDistanceFieldLighting = false",
            "bAffectDynamicIndirectLighting = false",
            "SetVisibility(true, true)",
            "SetHiddenInGame(false, true)",
        ):
            self.assertIn(fragment, self.source)
        self.assertIn(
            "bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority = false",
            self.header,
        )
        self.assertIn("bOuterGroundLoadingFallbackSurveyAsBuilt = false", self.header)

    def test_combined_configuration_preflights_and_reasserts_current_component_policy(self):
        configure = self.source[
            self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    ConfigureCurrentSurroundingsAndOuterGroundPresentation("
            ) : self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    SuppressInheritedPlanningGroundPresentation("
            )
        ]
        preflight = configure.index(
            "V5D fallback configuration preflight rejected"
        )
        mutation = configure.index(
            "CurrentSurroundingsRenderOnlyComponent->SetStaticMesh("
        )
        self.assertLess(preflight, mutation)
        for fragment in (
            "!GetWorld() || Scene->GetWorld() != GetWorld()",
            "!Scene->ContextBuildingsComponent",
            "!Scene->OSMContextBuildingsComponent",
            "!Scene->V5CSurroundingsRenderOnlyComponent",
            "!Scene->V5CGroundContextRenderOnlyComponent",
            "CurrentSurroundingsRenderOnlyComponent->SetCollisionResponseToAllChannels(\n        ECR_Ignore)",
            "CurrentSurroundingsRenderOnlyComponent->SetCastShadow(true)",
        ):
            self.assertIn(fragment, configure)
        presentation = self.source[
            self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    ValidateCurrentSurroundingsPresentation("
            ) : self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    ValidateCurrentSurroundingsSuccessorForInheritedScene("
            )
        ]
        self.assertEqual(
            presentation.count(
                "GetCollisionResponseToChannels() !=\n"
                "            FCollisionResponseContainer(ECR_Ignore)"
            ),
            2,
        )

    def test_builder_has_one_exact_two_mesh_configuration_boundary(self):
        signature = "ConfigureCurrentSurroundingsAndOuterGroundPresentation("
        self.assertIn(signature, self.header)
        self.assertIn(signature, self.source)
        self.assertIn("UStaticMesh* CurrentSurroundingsMesh", self.header)
        self.assertIn("UStaticMesh* OuterGroundLoadingFallbackMesh", self.header)
        self.assertIn(
            "ExpectedOuterGroundLoadingFallbackMeshObjectPath()", self.header
        )

    def test_outer_visibility_tracks_the_existing_provider_fallback_state(self):
        setter = self.source[
            self.source.index(
                "void ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    SetLocalBuildingFallbackVisible("
            ) :
            self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    SetProviderReadyPresentation("
            )
        ]
        self.assertIn(
            "OuterGroundLoadingFallbackRenderOnlyComponent", setter
        )
        self.assertIn("bVisible && bUseOuterGroundLoadingFallback", setter)
        presentation = self.source[
            self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    ValidateCurrentSurroundingsPresentation("
            ) :
            self.source.index(
                "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
                "    ValidateCurrentSurroundingsSuccessorForInheritedScene("
            )
        ]
        self.assertIn(
            "const bool bExpectedVisible = !bLocalBuildingFallbackCurrentlyHidden",
            presentation,
        )
        self.assertGreaterEqual(
            presentation.count("IsVisible() != bExpectedVisible"), 2
        )
        self.assertIn("RestoreGroundLevelPresentation(Error);", self.source)
        self.assertIn("SetLocalBuildingFallbackVisible(CachedScene.Get(), true);", self.source)

    def test_native_automation_covers_paths_provenance_and_default_component(self):
        for fragment in (
            "FTRIADIstanaExploreV5DSuccessorFallbackContractTest",
            "Suppression successor exact section total",
            "Suppression successor provenance is canonical",
            "Outer-ground provenance is canonical",
            "Native outer-ground default subobject exists",
            "Outer ground starts fail-closed visible",
        ):
            self.assertIn(fragment, self.source)


if __name__ == "__main__":
    unittest.main()
