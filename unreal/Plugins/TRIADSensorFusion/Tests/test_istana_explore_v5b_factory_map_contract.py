from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
ASSET_FACTORY_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5BAssetFactory.cpp"
)
V5_MATERIAL_FACTORY_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5MaterialFactory.cpp"
)
EDITOR_LIBRARY_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5BEditorLibrary.cpp"
)
EDITOR_LIBRARY_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5BEditorLibrary.h"
)
RUNTIME_TEST_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "Tests"
    / "TRIADIstanaExploreV5BRuntimeTests.cpp"
)
VISUAL_ACTOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5BVisualActor.cpp"
)
EXACT_V5_GAME_MODE_PATH = (
    "/Script/TRIADSensorFusion.TRIADIstanaExploreV5GameMode"
)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated C++ function: {signature}")


class IstanaExploreV5BFactoryMapContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory = text(ASSET_FACTORY_CPP)
        cls.v5_material_factory = text(V5_MATERIAL_FACTORY_CPP)
        cls.editor = text(EDITOR_LIBRARY_CPP)
        cls.editor_header = text(EDITOR_LIBRARY_H)
        cls.runtime_test = text(RUNTIME_TEST_CPP)
        cls.visual_actor = text(VISUAL_ACTOR_CPP)

    def test_render_successor_hides_the_exact_legacy_ground_plant_roster(self) -> None:
        legacy = function_body(
            self.visual_actor, "TArray<UPrimitiveComponent*> LegacyGroundPlantRenderSources("
        )
        for contract in (
            "LegacyGroundPlantRenderSourceCount = 8",
            "InheritedRenderSourceCount =",
            "LegacyGroundPlantInstanceCount = 2096",
            "V2->ShrubInstancesA",
            "V2->ShrubInstancesB",
            "V2->HedgeInstances",
            "V2->GroundcoverInstances",
            "V3->Shrub02Instances",
            "V3->Fern02Instances",
            "V3->Moss01Instances",
            "V3->BermudaGrassInstances",
            "LayeredShrubsA",
            "LayeredShrubsB",
            "ClippedHedges",
            "FoundationGroundcover",
            "CC0Shrub02",
            "CC0Fern02",
            "CC0Moss01ShadedMicroAreas",
            "CC0BermudaGrassEdgePatches",
            "512, 320, 192, 768, 96, 160, 24, 24",
        ):
            self.assertIn(contract, self.visual_actor)
        self.assertIn("Hism->GetInstanceCount() != ExpectedCounts[Index]", legacy)

        successor = function_body(
            self.visual_actor, "bool SetInheritedRenderSuccessorState("
        )
        self.assertIn("Components.Num() != InheritedRenderSourceCount", successor)
        self.assertIn("Component->SetVisibility(!bSuccessorOwnsRendering, true)", successor)
        self.assertIn("Component->SetHiddenInGame(bSuccessorOwnsRendering)", successor)

    def test_paver_is_a_dedicated_cook_safe_material_at_the_frozen_path(self) -> None:
        self.assertIn(
            'const FString PaverMaterialName(TEXT("MI_IPV5B_PaverStone"));',
            self.factory,
        )
        self.assertNotIn("V5StoneParentPath", self.factory)
        self.assertNotIn("MaterialInstanceConstantFactoryNew", self.factory)
        self.assertNotIn(
            "LoadExact<UMaterialInstanceConstant>(PaverMaterialObjectPath)",
            self.factory,
        )
        create_paver = function_body(self.factory, "UMaterial* CreatePaverStoneMaterial(")
        for contract in (
            "CreateEmptyMaterial(",
            "AssetTools, PaverMaterialName, OutError",
            "Material->BlendMode = BLEND_Opaque;",
            "Material->TwoSided = false;",
            "Material->bTangentSpaceNormal = true;",
            "Material->bUsedWithInstancedStaticMeshes = true;",
            "Material->MaxWorldPositionOffsetDisplacement = 0.0f;",
        ):
            self.assertIn(contract, create_paver)

    def test_paver_graph_has_uv_and_per_instance_procedural_stone_channels(self) -> None:
        create_paver = function_body(self.factory, "UMaterial* CreatePaverStoneMaterial(")
        for contract in (
            "/Script/Engine.MaterialExpressionPerInstanceRandom",
            "PaverBaseDescription",
            "PaverRoughnessDescription",
            "PaverNormalDescription",
            'Custom->Inputs[0].InputName = TEXT("UV");',
            'Custom->Inputs[1].InputName = TEXT("Random01");',
            "Data->BaseColor.Connect(0, Base);",
            "Data->Roughness.Connect(0, Roughness);",
            "Data->Normal.Connect(0, Normal);",
            "Data->Specular.Connect(0, Specular);",
        ):
            self.assertIn(contract, create_paver)
        for shader_marker in (
            "PAVER_PROCEDURAL_STONE_COLOR_V1",
            "PAVER_PROCEDURAL_STONE_ROUGHNESS_V1",
            "PAVER_PROCEDURAL_STONE_NORMAL_V1",
            "paleFleck",
            "darkFleck",
        ):
            self.assertIn(shader_marker, self.factory)

    def test_paver_cold_validator_pins_class_usage_graph_and_no_displacement(self) -> None:
        validate = function_body(self.factory, "bool ValidateInternal(")
        for contract in (
            "UMaterial* Paver = LoadExact<UMaterial>(PaverMaterialObjectPath);",
            "Paver->GetClass() != UMaterial::StaticClass()",
            "!Paver->bUsedWithInstancedStaticMeshes",
            "PaverData->ExpressionCollection.Expressions.Num() != 6",
            "PaverUvNodes != 1",
            "PaverRandomNodes != 1",
            "PaverBaseNodes != 1",
            "PaverRoughnessNodes != 1",
            "PaverNormalNodes != 1",
            "PaverSpecularNodes != 1",
            "PaverData->WorldPositionOffset.Expression",
            "PaverData->Refraction.Expression",
            "cook-safe instanced procedural-stone material graph",
        ):
            self.assertIn(contract, validate)

    def test_asset_count_and_material_path_contract_include_tree_grounding(self) -> None:
        create_materials = function_body(self.factory, "bool CreateMaterials(")
        self.assertIn(
            "UMaterial* Paver = CreatePaverStoneMaterial(AssetTools, OutError);",
            create_materials,
        )
        self.assertIn("{PaverMaterialName, Paver}", create_materials)
        self.assertIn("return OutMaterials.Num() == 8;", create_materials)
        self.assertIn("assets=37", self.factory)
        self.assertIn("meshes=20", self.factory)
        self.assertIn("treeBaseMulchMoundTriangles=224", self.factory)
        self.assertIn("TreeBaseMulchObjBytes = 17630", self.factory)
        self.assertIn("TreeBaseMulchMtlBytes = 280", self.factory)
        self.assertIn("TreeBaseMulchManifestBytes = 1456", self.factory)
        self.assertIn(
            "93F07CB64B37AB46696ACDAC9433E16071BB231AE865EF0BB0E67684C49F0887",
            self.factory,
        )
        self.assertIn(
            "CC5403A5A9B6535403FE7F4DFD2E194E196CD3093F7D59AC084F59BE926D4277",
            self.factory,
        )
        self.assertIn(
            "DF1352077E0DA19F7619D8E0A4C43E2807952BB77EC79BC87091C48BBD87722C",
            self.factory,
        )
        self.assertIn("TreeBaseMulchMaterialLibrarySource()", self.factory)
        self.assertIn("TreeBaseMulchManifestSource()", self.factory)
        self.assertIn("ValidateTreeBaseMulchSourceHashes(OutError)", self.factory)
        for contract in (
            "ExpectedTreeMulchRadialFractions[]",
            "0.30f",
            "0.65f",
            "0.88f",
            "!FMath::IsNearlyZero(Uv.Y, 0.00025f)",
            "radial fraction/imported tree-mulch sentinel",
            "imported formal-bed sentinel",
        ):
            self.assertIn(contract, self.factory)
        self.assertIn("materials=8 textures=9", self.factory)
        create_fresh = function_body(
            self.factory,
            "bool CreateFreshExploreV5BAssets(",
        )
        self.assertIn("OutAssets.Num() != 37", create_fresh)
        self.assertNotIn("OutAssets.Num() != 36", create_fresh)
        self.assertIn("FreshAssets.Num() != 37", self.editor)

    def test_tree_grounding_is_bounded_render_only_and_read_only_to_v4(self) -> None:
        build = function_body(
            self.visual_actor,
            "bool ATRIADIstanaExploreV5BVisualActor::BuildTreeGrounding(",
        )
        select_maximin = function_body(
            self.visual_actor,
            "bool ATRIADIstanaExploreV5BVisualActor::SelectCanonicalMaximinCandidateIndices(",
        )
        extract = function_body(
            self.visual_actor,
            "bool ATRIADIstanaExploreV5BVisualActor::ExtractTreeSources(",
        )
        for contract in (
            "TreeGroundingAnchorCount = 64",
            "TreeGroundingShrubCount =",
            "TreeGroundingUnderstoreyCount =",
            "TreeGroundingMinimumAbsXCm = 5200.0",
            "TreeGroundingMaximumAbsXCm = 17000.0",
            "TreeGroundingMaximumYCm = 22000.0",
            "TreeBaseRadiusHeightRatio = 0.043",
            "TreeBaseRadiusMinCm = 110.0",
            "TreeBaseRadiusMaxCm = 180.0",
            "TreeGroundingMinimumMulchEdgeClearanceCm = 90.0",
            "TreeGroundingMinimumSpacingCm = 550.0",
            "TreeGroundingDuplicateRootToleranceCm = 75.0",
            "TreeBaseGrassSuppressionRadiusFraction = 0.84",
            "TreeBasePlantForcedLodModel = 1",
            "TreeBaseShrubRootInsetCm = 3.0",
            "TreeBaseUnderstoreyRootInsetCm = 4.5",
            "TreeBaseShrubMinimumRadialFraction = 0.42",
            "TreeBaseShrubMaximumRadialFraction = 0.56",
            "TreeBaseShrubMinimumUniformScale = 1.35",
            "TreeBaseShrubMaximumUniformScale = 1.85",
            "TreeBaseShrubHorizontalScaleMultiplier = 0.72",
            "TreeBaseUnderstoreyMinimumRadialFraction = 0.44",
            "TreeBaseUnderstoreyMaximumRadialFraction = 0.58",
            "TreeBaseUnderstoreyMinimumUniformScale = 1.15",
            "TreeBaseUnderstoreyMaximumUniformScale = 1.60",
            "TreeBaseUnderstoreyHorizontalScaleMultiplier = 0.60",
            "TreeBaseUnderstoreyTriadAngleStepRadians = 0.14",
            "TreeBaseMoundSurfaceZCm(RadialFraction)",
            "const auto ShrubClusterAngle = [IdentityHash]",
            "const int32 ClusterIndex = PlantIndex / 3",
            "const int32 MemberIndex = PlantIndex % 3",
            "TreeBaseMulchWorldTransforms",
            "TreeBaseShrubWorldTransforms",
            "TreeBaseUnderstoreyWorldTransforms",
            "AnalyticAccentTerrainHeightCm",
        ):
            self.assertIn(contract, self.visual_actor)
        self.assertNotIn("SetStaticMesh", extract)
        self.assertNotIn("SetCollision", extract)
        self.assertNotIn("SetVisibility", extract)
        self.assertNotIn("SetHiddenInGame", extract)
        self.assertNotIn("AddInstance", extract)
        self.assertIn("ValidateExploreV4Landscape", extract)
        self.assertIn("AccentTransform.SetTranslation(AccentLocation)", build)
        for token in (
            "ownedPrimitives=24",
            "treeBaseMulch=%d",
            "treeBaseShrubs=%d",
            "treeBaseUnderstorey=%d",
            "treeBaseMulchUv=radialSentinelV1",
            "treeBaseMulchFinish=darkFeatheredV12",
            "treeBaseRadiusCm=110..180",
            "treeBaseShrubScale=1.35..1.85x0.72XY",
            "treeBaseUnderstoreyScale=1.15..1.60x0.60XY",
            "treeBasePlantRadial=shrubs0.42..0.56/understorey0.44..0.58",
            "treeBaseRootInsetCm=shrubs3.0/understorey4.5",
            "treeBaseUnderstoreyLayout=twoTriads",
            "treeBasePlantSourceLod=1",
            "treeBaseSelection=deterministicMultiSeedMaximin",
            "treeBaseFlankEnvelopeCm=5200..17000/3500..22000",
            "treeBaseMinimumSpacingCm=550",
            "treeBaseMinimumMulchEdgeClearanceCm=90",
            "treeBaseGrassSuppressionRadiusFraction=0.84",
            "treeBaseGrassCarrierFootprintMarginCm=0",
            "treeBaseCentralLawnExcluded=true",
            "v4TreeRenderersUnchanged=true",
        ):
            self.assertIn(token, self.visual_actor)
        self.assertIn("SelectCanonicalMaximinCandidateIndices(", build)
        self.assertIn("TNumericLimits<double>::Max()", select_maximin)
        self.assertIn("NearestDistanceSquared.Init(", select_maximin)
        self.assertIn(
            "NearestDistanceSquared[CandidateIndex] = FMath::Min(",
            select_maximin,
        )
        self.assertNotIn(
            "for (const int32 ExistingIndex : TrialSelection)",
            select_maximin,
        )
        self.assertIn("BestMinimumDistanceSquared", build)
        self.assertIn("TreeGroundingMinimumSpacingCm", build)
        self.assertNotIn("admitted only", build)
        for contract in (
            "Tree-base plant center remains inside its host mulch silhouette",
            "Tree-base plant vertical scale remains in the realistic range",
            "Tree-base plant uses the pinned restrained horizontal spread",
            "Tree-base plant keeps an upright grounded rotation",
            "Tree-base shrub root is inset 3.0 cm into the mound profile",
            "Tree-base understorey root is inset 4.5 cm into the mound profile",
            "Tree-base understorey forms two coherent triads",
        ):
            self.assertIn(contract, self.runtime_test)
        for contract in (
            "TreeBaseShrubInstances->ForcedLodModel =",
            "TreeBaseUnderstoreyInstances->ForcedLodModel =",
            "source-LOD1-only presentation contract",
        ):
            self.assertIn(contract, self.visual_actor)

    def test_existing_v5b_map_has_an_explicit_unsaved_grounding_refresh(self) -> None:
        self.assertIn(
            "static bool RefreshIstanaExploreV5BGroundingUnsaved(FString& OutMessage);",
            self.editor_header,
        )
        refresh = function_body(
            self.editor,
            "bool UTRIADIstanaExploreV5BEditorLibrary::\n"
            "    RefreshIstanaExploreV5BGroundingUnsaved(",
        )
        for contract in (
            "ValidateExploreV5BAssets(",
            "exact 37-asset roster required",
            "World->GetOutermost()->GetName() != DestinationMapPackage",
            "FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>",
            "ATRIADIstanaExploreV5GameMode::StaticClass()->GetPathName() !=",
            "WorldSettings->DefaultGameMode !=",
            "RuntimePolicy->bRequireIstanaAirSimGameMode",
            "RuntimePolicy->bEnforceFixedPrimaryCamera",
            "RuntimePolicy->RequiredGameModeClassPath != V5BGameModeClassPath",
            "Visual->GetActorTransform().Equals(FTransform::Identity, 0.0)",
            "Visual->Tags.Contains(V5BActorTag)",
            "Scene->ValidatePublicViewScene(SceneReport, false)",
            "V4->ValidateExploreV4Landscape(V4Report)",
            "V5->ValidateExploreV5Appearance(V5Report, false)",
            "ValidateSceneV5BHeroBoundary(Scene, false, BoundaryError)",
            "CaptureTouchedPackageDirtyFlags(TouchedObjects)",
            "FScopedTransaction",
            "Object->Modify();",
            "Visual->ConfigureExploreV5BVisuals(Scene, V4, Roster, Error)",
            "Transaction.Cancel();",
            "RestoreTouchedPackageDirtyFlags(PackageDirtyFlagsBefore);",
            "World->MarkPackageDirty();",
            "REFRESHED_UNSAVED",
            "intentionally UNSAVED",
        ):
            self.assertIn(contract, refresh)
        configure_call = refresh.index(
            "Visual->ConfigureExploreV5BVisuals(Scene, V4, Roster, Error)"
        )
        success_tail = refresh[configure_call:]
        self.assertNotIn("ValidateV5BWorld(", success_tail)
        self.assertNotIn("ValidateExploreV5BVisuals(", success_tail)
        self.assertNotIn("FAILED_READBACK", success_tail)
        self.assertLess(
            refresh.index("CaptureTouchedPackageDirtyFlags(TouchedObjects)"),
            refresh.index("FScopedTransaction Transaction"),
        )
        self.assertLess(
            refresh.index("Transaction.Cancel();"),
            refresh.index(
                "RestoreTouchedPackageDirtyFlags(PackageDirtyFlagsBefore);"
            ),
        )
        self.assertNotIn("SaveMap", refresh)
        self.assertNotIn("SaveLoadedAssets", refresh)

    def test_v5b_configure_rollback_is_raw_version_aware_and_complete(self) -> None:
        configure = function_body(
            self.visual_actor,
            "bool ATRIADIstanaExploreV5BVisualActor::ConfigureExploreV5BVisuals(",
        )
        for contract in (
            "CaptureOwnedV5BVisualState(",
            "PreviousAssets",
            "PreviousLayout",
            "PreviousPreservedAccentSources",
            "CaptureSceneRenderState(",
            "CaptureSourceRenderState(",
            "PreviousSceneActor != InPublicViewSceneActor",
            "PreviousSourceActor != InExploreV4LandscapeActor",
            "RestoreOwnedV5BVisualState(",
            "PublicViewSceneActor = PreviousSceneActor;",
            "ExploreV4LandscapeActor = PreviousSourceActor;",
            "SavedAssetRoster = PreviousAssets;",
            "SaveLayout(PreviousLayout);",
            "PreservedAccentSourceWorldTransforms =",
            "bConfigured = bHadPreviousConfiguration;",
            "RestoreSceneRenderState(Snapshot)",
            "RestoreSourceRenderState(Snapshot)",
            "FailAfterMutation(ApplyError)",
            "FailAfterMutation(ValidationReport)",
        ):
            self.assertIn(contract, configure)
        self.assertNotIn(
            "ApplyLayout(PreviousAssets, PreviousLayout", configure
        )
        for contract in (
            "UStaticMesh* Mesh = nullptr;",
            "OverrideMaterials;",
            "LocalInstanceTransforms;",
            "NumCustomDataFloats",
            "PerInstanceCustomData",
            "bAutoRebuildTreeOnInstanceChanges",
            "BuildTreeIfOutdated(false, true)",
            "IsAsyncBuilding()",
            "IsTreeFullyBuilt()",
        ):
            self.assertIn(contract, self.visual_actor)
        for native_regression in (
            "OldSchemaFailureAtomicRollback",
            "Old saved 36-asset schema cannot re-enter current ApplyLayout",
            "Raw rollback restores despite old-schema ApplyLayout rejection",
            "PackageDirtyRollback",
            "Rollback restores an initially clean touched package",
            "Rollback restores an initially dirty touched package",
        ):
            self.assertIn(
                native_regression,
                self.visual_actor + self.editor,
            )

    def test_pachira_foliage_graph_pins_realistic_two_sided_leaf_policy(self) -> None:
        create_pachira = function_body(
            self.factory, "UMaterial* CreatePachiraMaterial("
        )
        for contract in (
            "/Script/Engine.MaterialExpressionPerInstanceRandom",
            "PachiraLeafColorDescription",
            "PachiraWindDescription",
            "UMaterialExpressionTwoSidedSign",
            "V5B_PACHIRA_TWO_SIDED_NORMAL",
            "PachiraLeavesSubsurfaceAttenuation",
            "Data->OpacityMask.Connect(1, Opacity);",
            "Data->SubsurfaceColor.Connect(0, SubsurfaceColor);",
            "Facing->B.Connect(0, TwoSidedSign);",
            "CorrectedNormal->B.Connect(0, Facing);",
            "Material->OpacityMaskClipValue = PachiraOpacityClipValue;",
            "Material->SetShadingModel(MSM_TwoSidedFoliage);",
            "ValidatePachiraMaterialGraph(Material, bLeaves, OutError)",
        ):
            self.assertIn(contract, create_pachira)
        for shader_contract in (
            "TRIAD_EXPLORE_V5B_PACHIRA_PER_INSTANCE_LEAF_COLOR_V1",
            "TRIAD_EXPLORE_V5B_INSTANCE_LOCAL_PACHIRA_WPO_V1",
            "return saturate(BaseColor * lerp(shadeTint, sunTint, seed) * tonal);",
            "float h = saturate(max(InstanceLocalPosition.z, 0.0)",
            "float bend = clamp(WindStrengthCm * ResponseScale",
        ):
            self.assertIn(shader_contract, self.factory)

        validate_pachira = function_body(
            self.factory,
            "bool ValidatePachiraMaterialGraph(\n"
            "    UMaterial* Material,\n"
            "    bool bLeaves,\n"
            "    FString& OutError)\n{",
        )
        for contract in (
            "const int32 ExpectedExpressionCount = bLeaves ? 23 : 15;",
            "TwoSidedSignNodes != (bLeaves ? 1 : 0)",
            "MultiplyNodes != (bLeaves ? 2 : 0)",
            "Wind->Inputs.Num() == 10",
            'TEXT("Random01")',
            "PachiraLeavesSubsurfaceAttenuation, 0.000001f",
            "InputMatches(Data->OpacityMask, Opacity, 1)",
            "InputMatches(Data->SubsurfaceColor, SubsurfaceColor, 0)",
            "Data->Metallic.Expression",
            "Data->AmbientOcclusion.Expression",
            "Data->PixelDepthOffset.Expression",
        ):
            self.assertIn(contract, validate_pachira)
        cold_validate = function_body(self.factory, "bool ValidateInternal(")
        self.assertIn(
            "ValidatePachiraMaterialGraph(Bark, false, OutReport)",
            cold_validate,
        )
        self.assertIn(
            "ValidatePachiraMaterialGraph(Leaves, true, OutReport)",
            cold_validate,
        )

    def test_pachira_leaf_alpha_coverage_matches_the_mask_clip(self) -> None:
        self.assertIn(
            "constexpr float PachiraOpacityClipValue = 0.34f;",
            self.factory,
        )
        import_textures = function_body(self.factory, "bool ImportTextures(")
        cold_validate = function_body(self.factory, "bool ValidateInternal(")
        for body in (import_textures, cold_validate):
            for contract in (
                "Spec.AssetName == PachiraLeavesOpacityTextureName",
                "bBermudaOpacity || bPachiraLeavesOpacity",
                "bPreserveMaskedAlphaCoverage",
            ):
                self.assertIn(contract, body)
        self.assertIn(
            "FVector4(PachiraOpacityClipValue, 0.0, 0.0, 0.0)",
            import_textures,
        )
        self.assertIn(
            "? FVector4(\n"
            "                          PachiraOpacityClipValue, 0.0, 0.0, 0.0)",
            cold_validate,
        )
        self.assertIn(
            "Texture->MipGenSettings = bPreserveMaskedAlphaCoverage",
            import_textures,
        )
        self.assertIn(
            "Texture->bDoScaleMipsForAlphaCoverage =\n"
            "            bPreserveMaskedAlphaCoverage;",
            import_textures,
        )
        self.assertIn(
            "const bool bBermudaBladeTexture = bBermudaOpacity ||",
            import_textures,
        )
        self.assertIn(
            "Texture->AddressX = bBermudaBladeTexture ? TA_Clamp : TA_Wrap;",
            import_textures,
        )

    def test_r14_grass_shadow_readability_and_formal_bed_finish_are_pinned(self) -> None:
        for contract in (
            "MODELED_BLADE_COLOR_V7_PHYSICAL_NORMAL_READABILITY",
            "lerp(0.96, 1.04",
            "float3 root = float3(0.070, 0.155, 0.048)",
            "float3 body = float3(0.082, 0.185, 0.058)",
            "float3 cutColor = float3(0.076, 0.174, 0.054)",
            "FLinearColor(0.058f, 0.145f, 0.038f)",
            "TEXT(\"Roughness\"), 0.80f",
            "TEXT(\"Specular\"), 0.30f",
            "INSTANCE_LOCAL_BERMUDA_WPO_V3_FADE_MATCHED",
            "float wpoFade = smoothstep(0.9090909, 1.0, saturate(InstanceFade))",
            "FORMAL_BED_APRON_AND_TREE_MULCH_IMPORTED_V_SENTINEL_COLOR_V12",
            "float treeMulchSentinel = 1.0 - step(0.5, ApronUV.y)",
            "float edgeNoise = (detail - 0.5) * 0.060",
            "float mulchMask = 1.0 - smoothstep(0.82 + edgeNoise, 0.995, apronFraction)",
            "float3 mulchDark = float3(0.040, 0.030, 0.021)",
            "float3 mulchWarm = float3(0.090, 0.060, 0.035)",
            "float3 mulchFleck = float3(0.135, 0.092, 0.050)",
            "float3 mulchEdgeDark = float3(0.045, 0.055, 0.022)",
            "float3 mulchEdgeWarm = float3(0.065, 0.078, 0.028)",
            "return lerp(formalBed, treeMulch, treeMulchSentinel)",
            "FORMAL_BED_MULCH_FINE_MICRO_NORMAL_V2",
        ):
            self.assertIn(contract, self.factory)
        create_accent = function_body(
            self.factory, "UMaterial* CreateAccentTurfMaterial("
        )
        for contract in (
            "V5B_BERMUDA_IMPORTED_CURVATURE_NORMAL_WS",
            "V5B_BERMUDA_TWO_SIDED_SIGN",
            "V5B_BERMUDA_FACING_CORRECTED_NORMAL",
            "V5B_BERMUDA_DISTANCE_MATCHED_NORMAL",
            "FacingCorrectedNormal->A.Connect(0, ImportedCurvatureNormal);",
            "FacingCorrectedNormal->B.Connect(0, TwoSidedSign);",
            "DistanceMatchedNormal->A.Connect(0, WorldUpNormal);",
            "DistanceMatchedNormal->B.Connect(0, FacingCorrectedNormal);",
            "DistanceMatchedNormal->Alpha.Connect(0, InstanceFade);",
            "InstanceFadeInput.InputName = TEXT(\"InstanceFade\")",
            "Data->Normal.Connect(0, DistanceMatchedNormal);",
        ):
            self.assertIn(contract, create_accent)
        self.assertNotIn("Data->Normal.Connect(0, WorldUpNormal);", create_accent)
        self.assertIn(
            "BuildSettings.bUseHighPrecisionTangentBasis =",
            self.factory,
        )
        validate = function_body(self.factory, "bool ValidateInternal(")
        for contract in (
            "AccentData->ExpressionCollection.Expressions.Num() != 23",
            "ExactWind->Inputs.Num() == 12",
            "for (int32 Index = 0; Index < 12; ++Index)",
            "!InputMatches(AccentData->Normal, DistanceMatchedNormal, 0)",
            ".BuildSettings.bUseHighPrecisionTangentBasis ||",
            "!VertexBuffer.GetUseHighPrecisionTangentBasis()",
            "ExpectedAccentVertices[] = {3910, 2040, 612}",
            "ExpectedAccentIndices[] = {7650, 2040, 612}",
            "ExpectedAccentSeeds[] = {680, 680, 204}",
            "ClippedSeedCount != 510",
            "JuvenileSeedCount != 170",
            "FIntVector(2, 0, 1)",
            "Lod.Sections[0].MaterialIndex != 0",
            "VertexBuffer.GetNumTexCoords() != 1",
            "VertexBuffer.VertexTangentX(VertexIndex)",
            "VertexBuffer.VertexTangentZ(VertexIndex)",
            "MinimumNormalZ[LodIndex]",
            "MaximumNormalZ[LodIndex]",
        ):
            self.assertIn(contract, validate)
        self.assertIn(
            "FormalBedUnderstoreyInfillCount = 1920",
            self.visual_actor,
        )
        create_soil = function_body(
            self.factory, "UMaterial* CreateFormalBedSoilMaterial("
        )
        self.assertIn("Data->Normal.Connect(0, Normal);", create_soil)
        self.assertIn(
            "FormalBedData->ExpressionCollection.Expressions.Num() != 6",
            validate,
        )
        self.assertIn(
            "FormalBedData->Normal.Expression != FormalBedNormal",
            validate,
        )
        self.assertIn("2880 deterministic layered infill plants", self.editor)
        self.assertIn("3968 formal-bed plants", self.editor)

    def test_tufted_turf_lods_and_scoped_upgrade_are_fail_closed(self) -> None:
        for contract in (
            "AccentTurfLod0Triangles = 2550",
            "AccentTurfLod1Triangles = 680",
            "AccentTurfLod2Triangles = 204",
            "AccentTurfLod0ObjBytes = 515342",
            "AccentTurfLod1ObjBytes = 238493",
            "AccentTurfLod2ObjBytes = 70572",
            "AccentTurfManifestBytes = 17943",
            "294032971A4F9049A474A0226248184106509001B07C5E7EE0073FEC2331E7B6",
            "2AEAE5F3358B8385D48CAB46FB8AED8AB69ACDB4A94475301AB1DAED996A8475",
            "8EEA89426433072677F99C2DB3EAF9F68D0DE9A2FFEB7F3B971FAED14FC07743",
            "E30A1FA46F559933F6AB209F7018EE590DFD0546C389294ABFC4784F69B10D80",
            "AccentTurfLod1ScreenSize = 0.10f",
            "AccentTurfLod2ScreenSize = 0.040f",
            "const float MinimumNormalZ[] = {0.03f, 0.06f, 0.06f}",
            "const float MaximumNormalZ[] = {0.93f, 0.83f, 0.83f}",
            "FMath::IsWithinInclusive(AccentMin.X, -78.05f, -77.72f)",
            "FMath::IsWithinInclusive(AccentMin.Y, -68.62f, -68.29f)",
            "FMath::IsWithinInclusive(AccentMax.X, 79.02f, 79.35f)",
            "FMath::IsWithinInclusive(AccentMax.Y, 68.22f, 68.55f)",
            "ValidateAccentTurfSourceHashes",
            "FbxMeshUtils::ImportStaticMeshLOD",
            "ImportedLodZero->GetPathName()",
            "Mesh->CreateMeshDescription(0, *ImportedDescription)",
            "Mesh->CommitMeshDescription(0)",
            "Mesh->bAutoComputeLODScreenSize = false",
            "SourceModel.BuildSettings.bGenerateLightmapUVs = false",
            "SourceModel.BuildSettings.bRecomputeNormals = false",
            "SourceModel.BuildSettings.bUseFullPrecisionUVs = true",
            "!VertexBuffer.GetUseFullPrecisionUVs()",
            "ConfigureAccentTurfMesh",
            "RebuildExistingAccentTurfAsset",
            "ValidateAccentTurfAssetForR11Upgrade",
            "IsExactUnpersistedAccentTurfLodImportScratch",
            "Material->ForceRecompileForRendering",
            "R10Vertices[] = {4250, 2550, 768}",
            "R10Triangles[] = {2550, 850, 256}",
            "bOutAlreadyR11 = true",
            "ActualPaths != ExactObjectPaths()",
            "Object->GetOutermost()->IsDirty()",
            "ValidateInternal(false, PreSaveValidation)",
        ):
            self.assertIn(contract, self.factory)
        for contract in (
            "UpgradeIstanaExploreV5BAccentTurfAsset",
            "V5B_ACCENT_TURF_R11_UPGRADE_PASS",
            "geometryRevision=R11",
            "uniformR10Input=true",
            "IDEMPOTENT_V5B_ACCENT_TURF_R11_ALREADY_VALID",
            "fineTurfAppearanceProxy=true",
            "botanicalSpeciesClaim=false",
            "SnapshotOtherV5BPackageHashes",
            "preservedPackageHashes=36",
            "BackUpAccentTurfPackage",
            "backup.receipt.txt",
            "PackageArtifactCandidates",
            "ArtifactCandidates.Num() != 6",
            "TRIAD_V5B_ACCENT_TURF_R11_BACKUP_V1",
            "TRIAD/Backups/V5B_AccentTurfR11",
            "artifactCandidates=6",
            'Base + TEXT(".m.ubulk")',
            'Base + TEXT(".upayload")',
            "state=ABSENT",
            "rollback=DELETE_IF_PRESENT",
            "SaveLoadedAsset(RebuiltAsset, false)",
            "UPackageTools::ReloadPackages",
            "RestoreAccentTurfPackage",
            "rollback pre-reload hash verification failed",
            "rollback pre-reload absence verification failed",
            "rollback post-reload artifact verification failed",
            "AUTOMATIC_ROLLBACK_OK",
            "PreservedAfterRollback",
            "RebuiltAsset != OriginalAsset",
            "Backup.OriginalHashes.Find(PackageFilename)",
            "FileMd5(PackageFilename) != OriginalAccentHash",
            "NewAccentHash == OriginalAccentHash",
        ):
            self.assertIn(contract, self.editor)
        self.assertIn(
            "UpgradeIstanaExploreV5BAccentTurfAsset",
            self.editor_header,
        )
        for contract in (
            "AccentFrozenTransformHeightDenominatorCm = 4.8",
            "AccentTurfCount = 18432",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCanEverAffectNavigation(false)",
        ):
            self.assertIn(contract, self.visual_actor)
        self.assertIn(
            "AccentFrozenMaterialWindHeightNormalizerCm = 4.8f",
            self.factory,
        )
        self.assertIn("R11's exact source maximum is 4.4 cm", self.factory)
        self.assertIn("AccentScale.Z * 4.8", self.runtime_test)
        self.assertIn("AccentScale.Z * 4.4", self.runtime_test)
        self.assertIn(
            "reports the R10 4.4 cm source-tip placement truthfully",
            self.runtime_test,
        )
        self.assertNotIn("AccentClusterSourceHeightCm", self.visual_actor)
        self.assertNotIn("botanicalSpeciesClaim=true", self.visual_actor)

    def test_lod_import_scratch_filter_is_exact_unpersisted_and_fail_closed(self) -> None:
        self.assertIn(
            'VegetationMeshPath + TEXT("/None")', self.factory
        )
        self.assertIn(
            'AccentTurfLodImportScratchPackageName + TEXT(".StaticMesh_0")',
            self.factory,
        )
        scratch = function_body(
            self.factory,
            "bool IsExactUnpersistedAccentTurfLodImportScratch(",
        )
        for contract in (
            "Row.GetObjectPathString() != AccentTurfLodImportScratchObjectPath",
            "Row.PackageName != FName(*AccentTurfLodImportScratchPackageName)",
            "Row.PackagePath != FName(*VegetationMeshPath)",
            'Row.AssetName != FName(TEXT("StaticMesh_0"))',
            "Row.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName()",
            "AccentTurfLodImportScratchArtifactCandidates()",
            "Artifacts.Num() != 6",
            "IFileManager::Get().FileExists(*Artifact)",
        ):
            self.assertIn(contract, scratch)
        for artifact in (
            'Base + TEXT(".uasset")',
            'Base + TEXT(".uexp")',
            'Base + TEXT(".ubulk")',
            'Base + TEXT(".uptnl")',
            'Base + TEXT(".m.ubulk")',
            'Base + TEXT(".upayload")',
        ):
            self.assertIn(artifact, self.factory)

        filtered = function_body(self.factory, "bool GatherRootAssets(")
        self.assertIn("GatherRootAssetsUnfiltered(OutAssets)", filtered)
        self.assertIn("OutAssets.RemoveAll", filtered)
        self.assertIn(
            "IsExactUnpersistedAccentTurfLodImportScratch(Row)", filtered
        )
        self.assertNotIn("ExactObjectPaths", filtered)

        rollback = function_body(self.factory, "~FScopedFreshRollback()")
        self.assertIn("GatherRootAssetsUnfiltered(Data)", rollback)
        validate = function_body(self.factory, "bool ValidateInternal(")
        self.assertIn("Data.Num() != 37", validate)
        self.assertIn("Actual.Num() != 37", validate)
        self.assertIn("!bRosterExact", validate)

    def test_r13_lawn_uses_a_hash_pinned_procedural_macro_field(self) -> None:
        for contract in (
            "TRIAD_IPV5_LAWN_MACRO_HEALTH_MOW_RESPONSE_V2_DEPERIODIZED",
            "GrassLawnTint(0.940f, 0.985f, 0.918f, 1.0f)",
            "GrassLawnTint.GetMax() > 1.0f",
            "Grass.ProceduralMacroField",
            "TextureSampleCount != 8",
            "RotatorCount != 1",
            "CustomCount != 2",
            "BuildExactNodeMap(EditorOnly, 53",
            "MacroTint->Alpha.Connect(0, MacroField)",
            "MacroField->Code != GrassMacroFieldCode",
            "MacroField->OutputType != CMOT_Float1",
            "MacroField->Inputs[0].InputName != TEXT(\"MacroUV\")",
        ):
            self.assertIn(contract, self.v5_material_factory)
        self.assertNotIn(
            "Grass001AOMacroBreakup", self.v5_material_factory
        )

    def test_broad_lawn_keeps_a_bounded_far_micro_normal_floor(self) -> None:
        for contract in (
            "constexpr float GrassFarMicroNormalStrength = 0.06f;",
            "BuildExactNodeMap(EditorOnly, 53",
            "ExactNode<UMaterialExpressionLinearInterpolate>(",
            'Nodes, TEXT("Grass.DistanceFadedMicroNormalAlpha")',
            "MicroNormalAlpha->ConstA = GrassFarMicroNormalStrength;",
            "MicroNormalAlpha->B.Connect(0, MicroNormalStrength);",
            "MicroNormalAlpha->Alpha.Connect(0, NearFade);",
            "MicroNormalAlpha->A.Expression",
            "MicroNormalAlpha->ConstA,",
            "GrassFarMicroNormalStrength,",
            "0.06 far micro-normal floor",
        ):
            self.assertIn(contract, self.v5_material_factory)
        self.assertNotIn(
            "MicroNormalAlpha->A.Connect(0, NearFade);",
            self.v5_material_factory,
        )

    def test_broad_lawn_base_owns_and_validates_its_nanite_usage_flag(self) -> None:
        creator = function_body(self.v5_material_factory, "UMaterial* CreateGrassBase(")
        validator = function_body(self.v5_material_factory, "bool ValidateGrassGraph(")
        water = function_body(self.v5_material_factory, "UMaterial* CreateWaterMaterial(")
        self.assertIn(
            "Material->bUsedWithNanite = true;\n"
            "    UMaterialEditingLibrary::RecompileMaterial(Material);",
            creator,
        )
        self.assertIn("!Material->GetUsageByFlag(MATUSAGE_Nanite)", validator)
        self.assertNotIn("bUsedWithNanite", water)
        self.assertEqual(
            1, self.v5_material_factory.count("bUsedWithNanite = true;")
        )
        self.assertEqual(
            1,
            self.v5_material_factory.count(
                "GetUsageByFlag(MATUSAGE_Nanite)"
            ),
        )

    def test_map_builder_sets_the_one_inherited_runtime_policy_exactly(self) -> None:
        self.assertIn(EXACT_V5_GAME_MODE_PATH, self.editor)
        build = function_body(
            self.editor,
            "bool UTRIADIstanaExploreV5BEditorLibrary::BuildIstanaExploreV5BMap(",
        )
        for contract in (
            "FindExactlyOne<ATRIADIstanaPublicViewRuntimePolicyActor>(",
            "RuntimePolicyCount != 1",
            "RuntimePolicy->Modify();",
            "RuntimePolicy->bEnforceFixedPrimaryCamera = false;",
            "if (RuntimePolicy->bEnforceFixedPrimaryCamera ||",
            "RuntimePolicy->bRequireIstanaAirSimGameMode = true;",
            "RuntimePolicy->RequiredGameModeClassPath = V5BGameModeClassPath;",
            "ValidateV5BWorld(ReloadedTarget, false, ColdReport)",
        ):
            self.assertIn(contract, build)

    def test_cold_and_pie_validation_pin_policy_census_path_and_settlement(self) -> None:
        validate_world = function_body(self.editor, "bool ValidateV5BWorld(")
        for contract in (
            "RuntimePolicyCount != 1",
            "RuntimePolicy->bEnforceFixedPrimaryCamera ||",
            "RuntimePolicy->RequiredGameModeClassPath != V5BGameModeClassPath",
            "!RuntimePolicy->bRuntimePolicySettledAtRuntime",
            "!RuntimePolicy->bGameModeOverrideVerifiedAtRuntime",
            "World->GetAuthGameMode()->GetClass() !=",
            "ATRIADIstanaExploreV5GameMode::StaticClass()",
        ):
            self.assertIn(contract, validate_world)
        get_play_state = function_body(self.editor, "bool GetValidatedPlayState(")
        self.assertIn("ValidateV5BWorld(OutWorld, true, WorldReport)", get_play_state)
        validate_pie = function_body(
            self.editor,
            "bool UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BPlayWorld(",
        )
        self.assertIn("GetValidatedPlayState(", validate_pie)
        self.assertIn("exact-one settled runtime-policy actor", validate_pie)

    def test_stop_quiesce_uses_only_exact_pie_identity(self) -> None:
        stop_identity = function_body(
            self.editor, "bool ValidateV5BPlayWorldIdentityForStop("
        )
        for contract in (
            "GEditor->PlayWorld",
            "World->WorldType != EWorldType::PIE",
            "UWorld::RemovePIEPrefix",
            "LogicalPackage != DestinationMapPackage",
            "SceneCount != 1",
            "V2Count != 1",
            "V3Count != 1",
            "V4Count != 1",
            "V5Count != 1",
            "V5BCount != 1",
            "RuntimePolicyCount != 1",
        ):
            self.assertIn(contract, stop_identity)
        for forbidden_gate in (
            "ValidateV5BWorld(",
            "bRuntimePolicySettledAtRuntime",
            "bGameModeOverrideVerifiedAtRuntime",
            "GetAuthGameMode",
            "GetViewTarget",
            "ValidateRuntimeFacadePresentation",
            "ValidateSceneV5BHeroBoundary",
        ):
            self.assertNotIn(forbidden_gate, stop_identity)

        quiesce = function_body(
            self.editor,
            "bool UTRIADIstanaExploreV5BEditorLibrary::QuiesceIstanaExploreV5BPlayWorldForStop(",
        )
        self.assertIn("ValidateV5BPlayWorldIdentityForStop(Error)", quiesce)
        self.assertNotIn("GetValidatedPlayState(", quiesce)

    def test_qa_teleport_freezes_tick_before_full_rotation_readback(self) -> None:
        teleport = function_body(
            self.editor,
            "bool UTRIADIstanaExploreV5BEditorLibrary::TeleportIstanaExploreV5BPlayPawnForQa(",
        )
        for contract in (
            "Pawn->SetActorTickEnabled(false);",
            "Pawn->IsActorTickEnabled() ||",
            "Pawn->SetActorLocationAndRotation(",
            "Pawn->GetActorRotation().Equals(Normalized, 0.1f)",
            "Teleported and QA-froze validated Explore V5B Player0 pawn",
        ):
            self.assertIn(contract, teleport)

    def test_native_runtime_contract_pins_class_path_and_runtime_derived_flags(self) -> None:
        for contract in (
            '#include "TRIADIstanaExploreV5GameMode.h"',
            '#include "TRIADIstanaPublicViewRuntimePolicyActor.h"',
            EXACT_V5_GAME_MODE_PATH,
            "bEnforceFixedPrimaryCamera",
            "bRuntimePolicySettledAtRuntime",
            "bGameModeOverrideVerifiedAtRuntime",
        ):
            self.assertIn(contract, self.runtime_test)

    def test_runtime_report_pins_the_current_r11_carrier_and_r14_exclusion(self) -> None:
        start = self.visual_actor.index(
            'TEXT("ISTANA_EXPLORE_V5B_VISUALS_VALID'
        )
        end = self.visual_actor.index("return true;", start)
        report = self.visual_actor[start:end]
        for marker in (
            "accentCarrierRevision=R11",
            "accentCarrierTuftCenters=272",
            "accentCarrierPairTufts=136",
            "accentCarrierTriadTufts=136",
            "accentCarrierBlades=680",
            "accentCarrierRoots=680",
            "accentCarrierClippedJuvenileBlades=510/170",
            "accentCarrierPostureMix=306/306/68",
            "accentCarrierCShapeCounterBend=612/68",
            "accentCarrierHeightMix=462/184/34",
            "accentCarrierTotalTriangleSurfaceAreaCm2=505.174458",
            "accentCarrierTotalTriangleSurfaceAreaBoundsCm2=500/535",
            "accentCarrierProjectedAreaRatioToR10Bounds=0.92/1.08",
            "accentCarrierProjectedAreaMaxRatiosAtDownwardElevation0/10/20/30=1.064873/1.070234/1.076868/1.072940",
            "accentCarrierLodTriangles=2550/680/204",
            "accentCarrierVertices=3910/2040/612",
            "treeBaseGrassSuppressionRadiusFraction=0.84",
            "treeBaseGrassCarrierFootprintMarginCm=0",
        ):
            self.assertIn(marker, report)
        for stale in (
            "accentCarrierTuftCenters=340",
            "accentCarrierBlades=850",
            "accentCarrierPostureMix=170/510/170",
            "accentCarrierCShapeCounterBend=722/128",
            "accentCarrierTotalTriangleSurfaceAreaRatioToR9=1.103173",
            "1.007447/1.019530/1.041748/1.077778",
            "accentCarrierLodTriangles=2550/850/256",
        ):
            self.assertNotIn(stale, report)
        self.assertIn(
            "source-height evidence: R11's exact 4.4 cm source tip",
            self.visual_actor,
        )


if __name__ == "__main__":
    unittest.main()
