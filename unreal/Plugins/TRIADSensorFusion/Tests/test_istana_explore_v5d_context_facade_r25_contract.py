from hashlib import sha256
from pathlib import Path
import re
import unittest


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
RUNTIME_HEADER = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h"
)
RUNTIME_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
FACTORY_HEADER = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h"
)
FACTORY_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DContextFacadeR25AssetFactory.cpp"
)
ASSET_EDITOR_HEADER = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h"
)
ASSET_EDITOR_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp"
)
HYBRID_HEADER = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
HYBRID_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
V5C_FACTORY = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CSurroundingsAssetFactory.cpp"
)
V2_FACTORY = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.cpp"
)


class IstanaExploreV5DContextFacadeR25ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.factory_header = FACTORY_HEADER.read_text(encoding="utf-8")
        cls.factory_source = FACTORY_SOURCE.read_text(encoding="utf-8")
        cls.asset_editor_header = ASSET_EDITOR_HEADER.read_text(encoding="utf-8")
        cls.asset_editor_source = ASSET_EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.hybrid_header = HYBRID_HEADER.read_text(encoding="utf-8")
        cls.hybrid_source = HYBRID_SOURCE.read_text(encoding="utf-8")

    def test_additive_factory_owns_exact_five_asset_namespace(self):
        for fragment in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25",
            "M_IPV5D_ContextFacadeR25_Master",
            "MI_IPV5D_ContextFacadeR25_OfficialWall",
            "MI_IPV5D_ContextFacadeR25_OfficialRoof",
            "MI_IPV5D_ContextFacadeR25_FallbackWall",
            "MI_IPV5D_ContextFacadeR25_FallbackRoof",
            "constexpr int32 ExpectedAssetCount = 5",
            "CreateFreshAssets",
            "ValidateAssets",
            "requires an empty exact R25 context-facade asset root",
            "root must contain exactly five assets",
        ):
            self.assertIn(fragment, self.factory_source + self.factory_header)
        self.assertNotIn("UStaticMesh", self.factory_source)
        self.assertNotIn("Fbx", self.factory_source)
        self.assertNotIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/",
            self.factory_source,
        )

    def test_surface_response_adds_distance_readable_depth_cues_without_geometry(self):
        for fragment in (
            "microReadability = 1.0 - smoothstep(0.35, 0.90",
            "float mullion =",
            "float transom =",
            "float dividerMask =",
            "float revealMask =",
            "float3 revealTint =",
            "plinthMask",
            "facadeSurface * 0.48",
            "55000.0f, 100000.0f",
            "55000.0f, 95000.0f",
            "0.78f",
            "0.74f",
            "materialOnlyNoGeometryChange=true",
        ):
            self.assertIn(fragment, self.factory_source)
        self.assertGreaterEqual(
            self.factory_source.count(
                "0.0f,\n        55000.0f"
            ),
            2,
        )
        for forbidden in (
            "EditorOnly->Normal.Connect",
            "EditorOnly->WorldPositionOffset.Connect",
            "EditorOnly->Displacement.Connect",
            "BLEND_Translucent",
            "BLEND_Masked",
        ):
            self.assertNotIn(forbidden, self.factory_source)
        for required in (
            "Material->BlendMode = BLEND_Opaque",
            "Material->SetShadingModel(MSM_DefaultLit)",
            "Material->TwoSided = false",
            "Material->bEnableTessellation = false",
            "Material->bEnableDisplacementFade = false",
            "Material->MaxWorldPositionOffsetDisplacement = 0.0f",
            "Material->bUsedWithNanite = true",
        ):
            self.assertIn(required, self.factory_source)

    def test_compiled_gate_uses_the_active_render_feature_level(self):
        self.assertIn('#include "RHIFeatureLevel.h"', self.factory_source)
        self.assertIn('#include "ShaderCompiler.h"', self.factory_source)
        start = self.factory_source.index(
            "bool ValidateCompiledMaterial("
        )
        end = self.factory_source.index(
            "bool ValidateInternal(", start
        )
        gate = self.factory_source[start:end]
        self.assertIn(
            "const ERHIFeatureLevel::Type FeatureLevel = "
            "GMaxRHIFeatureLevel;",
            gate,
        )
        self.assertEqual(
            gate.count("GetMaterialResource(FeatureLevel)"), 1
        )
        self.assertNotIn("ERHIFeatureLevel::SM5", gate)
        self.assertIn("Material->EnsureIsComplete()", gate)
        self.assertIn("IsGameThreadShaderMapComplete()", gate)
        self.assertIn("GetGameThreadShaderMap()", gate)
        self.assertIn("ShaderMap->IsValidForRendering()", gate)
        self.assertIn("Resource->IsDefaultMaterial()", gate)
        self.assertIn("Resource->GetCompileErrors()", gate)

    def test_runtime_override_roster_is_exact_and_lexical(self):
        start = self.runtime_source.index(
            "const FContextFacadeR25OverrideSpec ContextFacadeR25Overrides[]"
        )
        end = self.runtime_source.index(
            "static_assert(UE_ARRAY_COUNT(ContextFacadeR25Overrides) == 17)",
            start,
        )
        roster = self.runtime_source[start:end]
        rows = re.findall(
            r'\{TEXT\("(MAT_[A-Z_]+)"\), &(R25[A-Za-z]+MaterialPath)\}',
            roster,
        )
        self.assertEqual(
            rows,
            [
                ("MAT_BOTTOM_HIDDEN", "R25FallbackRoofMaterialPath"),
                ("MAT_COMMERCIAL_HINT", "R25FallbackWallMaterialPath"),
                ("MAT_GENERIC_BUILDING_HINT", "R25FallbackWallMaterialPath"),
                ("MAT_HEALTHCARE_HINT", "R25FallbackWallMaterialPath"),
                ("MAT_HOTEL_HINT", "R25OfficialWallMaterialPath"),
                ("MAT_INDUSTRIAL_HINT", "R25FallbackWallMaterialPath"),
                ("MAT_RELIGIOUS_HINT", "R25OfficialWallMaterialPath"),
                ("MAT_RESIDENTIAL_HINT", "R25FallbackWallMaterialPath"),
                ("MAT_ROOF_COMMERCIAL_HINT", "R25FallbackRoofMaterialPath"),
                ("MAT_ROOF_GENERIC_BUILDING_HINT", "R25FallbackRoofMaterialPath"),
                ("MAT_ROOF_HEALTHCARE_HINT", "R25FallbackRoofMaterialPath"),
                ("MAT_ROOF_HOTEL_HINT", "R25OfficialRoofMaterialPath"),
                ("MAT_ROOF_INDUSTRIAL_HINT", "R25FallbackRoofMaterialPath"),
                ("MAT_ROOF_RELIGIOUS_HINT", "R25OfficialRoofMaterialPath"),
                ("MAT_ROOF_RESIDENTIAL_HINT", "R25FallbackRoofMaterialPath"),
                ("MAT_ROOF_TRANSPORT_HINT", "R25FallbackRoofMaterialPath"),
                ("MAT_TRANSPORT_HINT", "R25FallbackWallMaterialPath"),
            ],
        )
        self.assertEqual([name for name, _ in rows], sorted(name for name, _ in rows))
        self.assertEqual(
            [role for _, role in rows].count("R25OfficialWallMaterialPath"), 2
        )
        self.assertEqual(
            [role for _, role in rows].count("R25OfficialRoofMaterialPath"), 2
        )
        self.assertEqual(
            [role for _, role in rows].count("R25FallbackWallMaterialPath"), 6
        )
        self.assertEqual(
            [role for _, role in rows].count("R25FallbackRoofMaterialPath"), 7
        )

    def test_runtime_keeps_strict_r25_and_dispatches_only_admitted_shell_states(self):
        for fragment in (
            "ApplyCurrentSurroundingsContextFacadeR25",
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene",
            "ValidateContextFacadeR25Overrides",
            "GetNumOverrideMaterials() == 0",
            "ValidateAdmittedCurrentShellMaterialState",
            "EAdmittedCurrentShellMaterialState::LegacyEmbedded",
            "EAdmittedCurrentShellMaterialState::ContextFacadeR25",
            "EAdmittedCurrentShellMaterialState::BroadShellR31",
            "bVersionedOverridesUseExactV2Mesh",
            "Component->SetMaterial(Index, ExactMaterials[Index])",
            "materialOverrides=17",
            "meshGeometryUnchanged=true",
            "collisionNavigationSensorRfAuthority=false",
            "sharedV5CAssetsMutated=false",
        ):
            self.assertIn(fragment, self.runtime_header + self.runtime_source)
        apply_start = self.runtime_source.index(
            "ApplyCurrentSurroundingsContextFacadeR25(FString& OutError)"
        )
        apply_end = self.runtime_source.index(
            "SuppressInheritedPlanningGroundPresentation(", apply_start
        )
        apply_body = self.runtime_source[apply_start:apply_end]
        self.assertIn("CurrentSurroundingsV2MeshObjectPath", apply_body)
        self.assertNotIn("SetStaticMesh(", apply_body)
        self.assertNotIn("SetCollision", apply_body)
        self.assertNotIn("SetCanEverAffectNavigation", apply_body)

    def test_editor_asset_boundary_saves_only_five_additive_assets(self):
        for fragment in (
            "ImportIstanaExploreV5DContextFacadeR25Assets",
            "ValidateIstanaExploreV5DContextFacadeR25Assets",
            "FScopedContextFacadeR25SavedImportRollback",
            "FreshAssets.Num() != 5",
            "ReloadPackages.Num() != 5",
            "SaveLoadedAssets(FreshAssets, false)",
            "The existing V2 mesh and shared V5C assets were not changed",
        ):
            self.assertIn(
                fragment, self.asset_editor_header + self.asset_editor_source
            )

    def test_r25_remote_entrypoints_pin_reflected_display_metadata(self):
        for function_name, display_name in (
            (
                "ImportIstanaExploreV5DContextFacadeR25Assets",
                "Import Istana Explore V5D Context Facade R25 Assets",
            ),
            (
                "ValidateIstanaExploreV5DContextFacadeR25Assets",
                "Validate Istana Explore V5D Context Facade R25 Assets",
            ),
        ):
            declaration = re.search(
                rf"UFUNCTION\((?P<metadata>.*?)\)\s*static bool {function_name}\(",
                self.asset_editor_header,
                re.DOTALL,
            )
            self.assertIsNotNone(declaration)
            metadata = declaration.group("metadata")
            self.assertIn("BlueprintCallable", metadata)
            self.assertIn(f'DisplayName = "{display_name}"', metadata)

    def test_hybrid_migration_is_hash_gated_one_save_and_map_only(self):
        for fragment in (
            "ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap",
            "ValidateIstanaExploreV5DContextFacadeR25SuccessorMap",
            "constexpr int64 PreContextFacadeR25Bytes = 34992354",
            "4A5F5514C7C3B508567465BA1F3B2FE8F31F4DAAAC5E317C8C57F1C30B50FD08",
            "CreateVerifiedPreContextFacadeR25MapBackup",
            "RestoreVerifiedPreContextFacadeR25MapViaSiblingTemp",
            "TRIAD_ContextFacadeR25_Restore_",
            "ApplyCurrentSurroundingsContextFacadeR25",
            "IDEMPOTENT_EXPLORE_V5D_CONTEXT_FACADE_R25_ALREADY_VALID",
            "soleMapDelta=CurrentSurroundingsRenderOnly.OverrideMaterials",
            "V2MeshUnchanged=true",
            "landmarksUnchanged=true",
            "providerSettingsUnchanged=true",
            "sharedV5CAssetsMutated=false",
        ):
            self.assertIn(fragment, self.hybrid_header + self.hybrid_source)
        start = self.hybrid_source.index(
            "ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap("
        )
        end = self.hybrid_source.index(
            "ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap(",
            start,
        )
        migration = self.hybrid_source[start:end]
        self.assertEqual(migration.count("SaveMap("), 1)
        self.assertNotIn("SetStaticMesh(", migration)
        for forbidden in (
            "SetMaximumScreenSpaceError",
            "SetCreatePhysicsMeshes",
            "SetCreateNavCollision",
            "MaximumSimultaneousTileLoads =",
            "PreloadSiblings =",
        ):
            self.assertNotIn(forbidden, migration)

    def test_shared_v5c_and_v2_asset_factories_are_byte_unchanged(self):
        self.assertEqual(
            sha256(V5C_FACTORY.read_bytes()).hexdigest().upper(),
            "C604FCBA33CCA96FC58EEEABD9F21298E315875834CABEADF69F9218F7211A4C",
        )
        self.assertEqual(
            sha256(V2_FACTORY.read_bytes()).hexdigest().upper(),
            "4F9ED43F7F85CE662A9285BCBC17F962D6BBAB8C91B84F8D39FCC685721F56BD",
        )


if __name__ == "__main__":
    unittest.main()
