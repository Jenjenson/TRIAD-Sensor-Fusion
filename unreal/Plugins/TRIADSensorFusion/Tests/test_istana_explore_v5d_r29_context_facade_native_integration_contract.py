from __future__ import annotations

import hashlib
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
PLUGIN = UNREAL / "Plugins" / "TRIADSensorFusion"
RUNTIME_PUBLIC = PLUGIN / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = PLUGIN / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = PLUGIN / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = PLUGIN / "Source" / "TRIADSensorFusionEditor" / "Private"

ACTOR_H = RUNTIME_PUBLIC / "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
ACTOR_CPP = RUNTIME_PRIVATE / "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.cpp"
NATIVE_TEST = (
    RUNTIME_PRIVATE
    / "Tests"
    / "TRIADIstanaExploreV5DR29FacadeEnvironmentActorTests.cpp"
)
FACTORY_H = (
    EDITOR_PRIVATE / "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.h"
)
FACTORY_CPP = (
    EDITOR_PRIVATE / "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.cpp"
)
EDITOR_H = (
    EDITOR_PUBLIC / "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.h"
)
EDITOR_CPP = (
    EDITOR_PRIVATE / "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.cpp"
)

SOURCE_ROOT = (
    UNREAL
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R29ContextFacadeCoverage"
    / "Generated"
)
SOURCE_RECEIPTS = {
    "SM_IPV5D_R29_ContextFacadeCoverage_Render.obj": (
        102_051_194,
        "8CE9659A991FC18369EC980FBCA8F9EFAF00B81784D9B2F423BEC935D13181C7",
    ),
    "IstanaPublicViewV5DR29ContextFacadeCoverage.mtl": (
        1_580,
        "4989AFC4478B48C55E038F1E85233C8FA1405360EA19C7E623FDB98FD3B59ADD",
    ),
    "IstanaPublicViewV5DR29ContextFacadeCoverage.manifest.json": (
        11_557,
        "DE2543394A10901FFA6025E325E8E92449CF866F4FC9934B99D9C9FCAEF0ECA2",
    ),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


class R29ContextFacadeNativeIntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        paths = (
            ACTOR_H,
            ACTOR_CPP,
            NATIVE_TEST,
            FACTORY_H,
            FACTORY_CPP,
            EDITOR_H,
            EDITOR_CPP,
        )
        for path in paths:
            if not path.is_file():
                raise AssertionError(f"missing isolated R29 facade native source: {path}")
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor = ACTOR_CPP.read_text(encoding="utf-8")
        cls.native_test = NATIVE_TEST.read_text(encoding="utf-8")
        cls.factory_h = FACTORY_H.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")

    def test_runtime_actor_is_an_isolated_three_renderer_successor(self) -> None:
        self.assertEqual(
            3, self.actor_h.count("TObjectPtr<UStaticMeshComponent>")
        )
        self.assertEqual(
            3,
            self.actor.count("CreateDefaultSubobject<UStaticMeshComponent>"),
        )
        for token in (
            "RetainedR28ConnectivePublicRealmRenderOnly",
            "R29ContextFacadeCoverageRenderOnly",
            "RetainedR28OuterGroundOverlayRenderOnly",
            "ExpectedOwnedRendererCount()",
            "return 3;",
            "/SurroundingsRealismR29",
            "SM_IPV5D_R29_ContextFacadeCoverage_Render",
            "SM_IPV5D_R28_ConnectivePublicRealm_Render",
            "SM_IPV5D_OuterGroundLoadingFallback_Render",
            "MI_IPV5D_R28_OuterGround",
            "bR28PublicRealmRetainedUnmodified = true",
            "bR28ArchitectureRetainedOrCoRendered = false",
        ):
            self.assertIn(token, self.actor_h + self.actor)
        self.assertNotIn(
            "TObjectPtr<UStaticMeshComponent> R28ContextArchitectural",
            self.actor_h,
        )

    def test_two_phase_handoff_never_co_renders_r28_architecture(self) -> None:
        prepare = between(
            self.actor,
            "ConfigurePreparedR29FacadeHandoff(",
            "ActivateAfterR28EnvironmentRemoval(",
        )
        activate = between(
            self.actor,
            "ActivateAfterR28EnvironmentRemoval(",
            "SetProviderReady(",
        )
        for token in (
            "R28ClassCount != 1",
            "R28TagCount != 1",
            "R29ClassCount != 1",
            "R29TagCount != 1",
            "SetOwnedPresentationVisible(this, false)",
            "ValidatePreparedR29FacadeHandoff",
        ):
            self.assertIn(token, prepare)
        for token in (
            "R28ClassCount != 0",
            "R28TagCount != 0",
            "R29ClassCount != 1",
            "R29TagCount != 1",
            "ActiveR28ArchitectureRenderers != 0",
            "SetOwnedPresentationVisible(this, !bProviderReady)",
            "ValidateR29FacadeEnvironment",
        ):
            self.assertIn(token, activate)
        apply = between(
            self.editor,
            "ApplyR29FacadeReplacementToLoadedV5DHybridMap(",
            "CommitR29FacadeReplacementToLoadedV5DHybridMap(",
        )
        prepare_position = apply.index("ConfigurePreparedR29FacadeHandoff")
        remove_position = apply.index("World->DestroyActor(Roster.R28)")
        activate_position = apply.index("ActivateAfterR28EnvironmentRemoval")
        self.assertLess(prepare_position, remove_position)
        self.assertLess(remove_position, activate_position)
        self.assertNotIn("SaveMap(", apply)

    def test_every_renderer_is_presentation_only(self) -> None:
        for token in (
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCollisionResponseToAllChannels(ECR_Ignore)",
            "SetGenerateOverlapEvents(false)",
            "SetCanEverAffectNavigation(false)",
            "SetAffectDistanceFieldLighting(false)",
            "bHiddenInSceneCapture = true",
            "SetRenderCustomDepth(false)",
            "bCollisionNavigationSimulationSensorOrRfAuthority = false",
            "bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed = false",
            "bExistingSimulationRfProviderOrGeospatialInputsModified = false",
            "bRuntimeGeometryGenerated = false",
            "PrimaryActorTick.bCanEverTick = false",
        ):
            self.assertIn(token, self.actor_h + self.actor)
        for forbidden in (
            "SetCollisionEnabled(ECollisionEnabled::QueryOnly)",
            "SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics)",
            "SetSimulatePhysics(true)",
            "bCanEverTick = true",
        ):
            self.assertNotIn(forbidden, self.actor_h + self.actor)

    def test_factory_hash_pins_exact_current_r29_sources(self) -> None:
        for name, (expected_bytes, expected_sha) in SOURCE_RECEIPTS.items():
            path = SOURCE_ROOT / name
            self.assertTrue(path.is_file(), path)
            self.assertEqual(expected_bytes, path.stat().st_size, name)
            self.assertEqual(expected_sha, sha256(path), name)
            self.assertIn(str(expected_bytes), self.factory)
            self.assertIn(expected_sha, self.factory)
        for token in (
            "FacadeTriangleCount = 256850",
            "FacadeSourceCornerCount = 770550",
            "FacadeMaterialTriangleCounts[]",
            "ExpectedFacadeTriangleCountForMaterial",
            "13686, // GlassCool",
            "168};  // BalconyRail",
            "Sections.Num() ==",
            "Section.MaterialIndex != SectionIndex",
            "Section.NumTriangles !=",
            "Section.FirstIndex !=",
            "OriginalSection.MaterialIndex != SectionIndex",
            "renderSections=11",
            "exactPerMaterialTriangleCensus=true",
            "exactSectionMaterialMapping=true",
            "bottomFrameRails=20011",
            "everyGroupedApertureFourSided=true",
            "MaterialSpecs) == 11",
            "ExpectedAssetCount = UE_ARRAY_COUNT(MaterialSpecs) + 1",
            "OutAssets.Num() != 12",
            "bImportMaterials = false",
            "bImportTextures = false",
            "bAutoGenerateCollision = false",
            "Section.bEnableCollision = false",
            "Mesh->MarkAsNotHavingNavigationData()",
            "Mesh->NaniteSettings.bEnabled = true",
            "KeepPercentTriangles = 1.0f",
            "TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets",
        ):
            self.assertIn(token, self.factory)
        self.assertNotIn(
            "FacadeMaterialTriangleCounts[SectionIndex]", self.factory
        )
        self.assertNotIn("SaveMap(", self.factory)

    def test_material_roster_has_visible_semantic_variation_without_textures(self) -> None:
        expected_materials = (
            "GlassCool",
            "GlassWarm",
            "GlassNeutral",
            "FrameLight",
            "FrameDark",
            "FrameBronze",
            "SillLight",
            "SillDark",
            "RoofTrim",
            "Canopy",
            "BalconyRail",
        )
        for suffix in expected_materials:
            self.assertIn(f"MI_IPV5D_R29_{suffix}", self.factory)
        for token in (
            "TintParameter",
            "RoughnessParameter",
            "SpecularParameter",
            "MacroStrengthParameter",
            "MicroStrengthParameter",
            "RoughnessVariationParameter",
            "GlazingGrazingStrengthParameter",
            "TextureParameterValues.IsEmpty()",
            "textureInputsAdded=0",
        ):
            self.assertIn(token, self.factory)

    def test_editor_accepts_exactly_one_r28_or_r29_vegetation_owner(self) -> None:
        for token in (
            '#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"',
            '#if __has_include("TRIADIstanaExploreV5DR29VegetationActor.h")',
            '#include "TRIADIstanaExploreV5DR29VegetationActor.h"',
            "TRIAD_HAS_R29_VEGETATION_ACTOR",
            "/Script/TRIADSensorFusion.TRIADIstanaExploreV5DR29VegetationActor",
            "TRIAD_IstanaExploreV5D_R29Vegetation_RenderOnly",
            "run the guarded R29 vegetation transaction first",
            "ValidateComposableVegetationOwner",
            "R28VegetationClassCount == 1",
            "R28VegetationTagCount == 1",
            "R29VegetationClassCount == 0",
            "R29VegetationTagCount == 0",
            "R29VegetationClassCount == 1",
            "R29VegetationTagCount == 1",
            "R28VegetationClassCount == 0",
            "R28VegetationTagCount == 0",
            "ValidateLandmarkVegetationR28",
            "ValidateR29Vegetation",
            "Composable vegetation owner requires exact R28 xor R29",
            "vegetationOwnerMode=EXACT_R28_XOR_R29",
            "vegetationActorsOrAssetsModified=false",
        ):
            self.assertIn(token, self.editor)
        vegetation_validation = between(
            self.editor,
            "bool ValidateComposableVegetationOwner(",
            "bool HasExactFreshSaveRoster(",
        )
        self.assertNotIn("DestroyActor", vegetation_validation)
        self.assertNotIn("ConfigureR29Vegetation", vegetation_validation)
        self.assertNotIn("SetProviderReady", vegetation_validation)

    def test_editor_library_owns_exact_endpoints_and_one_map_save(self) -> None:
        endpoints = (
            "EnsureR29FacadeEnvironmentAssets",
            "ValidateR29FacadeEnvironmentAssets",
            "ApplyR29FacadeReplacementToLoadedV5DHybridMap",
            "CommitR29FacadeReplacementToLoadedV5DHybridMap",
            "ValidateR29FacadeReplacementInLoadedV5DHybridMap",
        )
        for endpoint in endpoints:
            self.assertIn(endpoint, self.editor_h)
            self.assertIn(endpoint, self.editor)
        commit_start = self.editor.index(
            "CommitR29FacadeReplacementToLoadedV5DHybridMap("
        )
        final_validator_start = self.editor.rindex(
            "bool UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::\n"
            "    ValidateR29FacadeReplacementInLoadedV5DHybridMap("
        )
        commit = self.editor[commit_start:final_validator_start]
        self.assertEqual(1, commit.count("UEditorLoadingAndSavingUtils::SaveMap("))
        for token in (
            "ExpectedPredecessorBytes",
            "ExpectedPredecessorSha256",
            "ExpectedVegetationOwner",
            "NormalizedExpectedVegetationOwner",
            "VerifiedExternalBackupFilename",
            "V5DContextFacadeR29V1",
            "PreSaveMapSha256",
            "PreSaveBackupSha256",
            "COMMIT_REFUSED_PREDECESSOR_WORLD",
            "PredecessorRoster.R28ClassCount != 1",
            "PredecessorRoster.R29ClassCount != 0",
            "PredecessorVegetationOwner != NormalizedExpectedVegetationOwner",
            "ValidateComposableVegetationOwner",
            "UEditorLoadingAndSavingUtils::NewBlankMap(false)",
            "UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)",
            "bSuccessorChanged",
            "bBackupPreserved",
            "oneSave=true",
            "coldReload=true",
        ):
            self.assertIn(token, commit)

    def test_final_map_contract_is_mutually_exclusive_and_retains_public_realm(self) -> None:
        validation = self.editor[
            self.editor.index(
                "ValidateR29FacadeReplacementInLoadedV5DHybridMap("
            ) :
        ]
        for token in (
            "Roster.R28ClassCount != 0",
            "Roster.R28TagCount != 0",
            "Roster.R29ClassCount != 1",
            "Roster.R29TagCount != 1",
            "ValidateR29FacadeEnvironment",
            "r28ArchitectureComponentsOwned=0",
            "r28ArchitectureActiveRenderers=0",
            "retainedR28PublicRealmComponentsOwned=1",
            "retainedR28OuterGroundComponentsOwned=1",
            "mutuallyExclusiveArchitecture=true",
        ):
            self.assertIn(token, validation)

    def test_native_automation_covers_component_layout_and_negative_authority(self) -> None:
        for token in (
            "TRIAD.Istana.ExploreV5D.R29Facade.NativeComponentLayout",
            "Exact owned renderer count",
            "Retained R28 public realm renderer exists",
            "R29 facade renderer exists",
            "Retained R28 outer-ground renderer exists",
            "R28 architecture cannot be retained or co-rendered",
            "No simulation/sensor/RF authority",
            "Component %d has no collision",
            "Component %d cannot navigate",
            "Component %d hidden from scene capture",
            "Exact R29 material slot roster",
        ):
            self.assertIn(token, self.native_test)


if __name__ == "__main__":
    unittest.main()
