from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DFountainRealismActor.h"
)
RUNTIME_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DFountainRealismActor.cpp"
)
V5B_RUNTIME_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5BVisualActor.cpp"
)
POLICY_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
HYBRID_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
HYBRID_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
MATERIAL_FACTORY_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DFountainMaterialFactory.h"
)
MATERIAL_FACTORY_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DFountainMaterialFactory.cpp"
)


class IstanaExploreV5DFountainRealismContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = RUNTIME_H.read_text(encoding="utf-8")
        cls.runtime_cpp = RUNTIME_CPP.read_text(encoding="utf-8")
        cls.v5b_runtime_cpp = V5B_RUNTIME_CPP.read_text(encoding="utf-8")
        cls.policy_cpp = POLICY_CPP.read_text(encoding="utf-8")
        cls.hybrid_cpp = HYBRID_CPP.read_text(encoding="utf-8")
        cls.hybrid_h = HYBRID_H.read_text(encoding="utf-8")
        cls.material_factory_h = MATERIAL_FACTORY_H.read_text(encoding="utf-8")
        cls.material_factory_cpp = MATERIAL_FACTORY_CPP.read_text(
            encoding="utf-8"
        )

    def test_successor_is_v5d_only_and_preserves_inherited_assets(self) -> None:
        for fragment in (
            '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid',
            'ExpectedFountainWaterMaterialPath()',
            'ExpectedSprayMaterialPath()',
            'ExpectedEmbeddedWaterSuppressorMaterialPath()',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials/M_IPV5D_FountainWater.M_IPV5D_FountainWater',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials/M_IPV5D_FountainSpray.M_IPV5D_FountainSpray',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials/M_IPV5D_FountainEmbeddedWaterSuppressor.M_IPV5D_FountainEmbeddedWaterSuppressor',
            'MaterialDomain != MD_Surface',
            'SuccessorWaterBase->GetBlendMode() != BLEND_Opaque',
            'SuccessorSprayBase->GetBlendMode() != BLEND_Additive',
            'SavedSourceWaterSurfaceMesh',
            'SavedSourceOuterPlumeMesh',
            'SavedSourceImpactRingMesh',
            'SavedSuccessorWaterMaterial',
            'SavedSuccessorSprayMaterial',
            'SavedEmbeddedWaterSuppressorMaterial',
            'SavedHardscapeOriginalOverrideMaterials',
            'ISTANA_EXPLORE_V5D_FOUNTAIN_VALID revision=R7',
            'sourceMeshMaterialAssetsUntouched=true',
            'sourceTransformsCensusUntouched=true',
            'hardscapeWaterComponentOverrideApplied=true',
            'embeddedHardscapeWaterSectionSuppressed=true',
            'embeddedHardscapeWaterSectionTriangles=%d',
            'embeddedLegacyJetGroups=%d',
            'embeddedLegacyJetTriangles=%d',
            'replacementWaterSurfaceVisibleInstances=1',
            'hardscapeStoneMetalPreserved=true',
            'remainingVisibleSuccessorMeshesPlanar=true',
            'waterMaterial=M_IPV5D_FountainWater',
            'waterBlend=Opaque',
            'waterRefraction=NotApplicableOpaque',
            'waterNormal=worldXYCalmWorldSpace',
            'sprayMaterial=M_IPV5D_FountainSpray',
            'sprayBlend=Additive',
            'sprayShading=Unlit',
            'sprayRefraction=Unplugged',
            'sprayTwoSided=false',
            'sprayOpacityFloor=0',
            'successorFoamRippleMaterial=M_IPV5D_FountainSpray',
            'embeddedWaterSuppressorMaterial=M_IPV5D_FountainEmbeddedWaterSuppressor',
            'embeddedWaterSuppressorBlend=Additive',
            'embeddedWaterSuppressorShading=Unlit',
            'embeddedWaterSuppressorOutput=Zero',
        ):
            self.assertIn(fragment, self.runtime_h + self.runtime_cpp)

        # The pass creates no material/mesh assets and does not import content.
        for forbidden in (
            'AssetTools',
            'CreateAsset(',
            'ImportAsset',
            'SaveLoadedAsset',
            'SetNanite',
        ):
            self.assertNotIn(forbidden, self.runtime_cpp)

    def test_dedicated_material_factory_is_exact_atomic_and_cold_validated(
        self,
    ) -> None:
        combined = self.material_factory_h + self.material_factory_cpp
        for fragment in (
            '/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials',
            'M_IPV5D_FountainWater',
            'M_IPV5D_FountainSpray',
            'M_IPV5D_FountainEmbeddedWaterSuppressor',
            'GetEmbeddedWaterSuppressorMaterialObjectPath',
            'EnsureFountainRealismMaterialAssets',
            'ValidateFountainRealismMaterialAssets',
            'UMaterialFactoryNew',
            'AssetTools.CreateAsset(',
            'RM_PixelNormalOffset',
            'WPT_ExcludeAllShaderOffsets',
            '3.40 + TimeSeconds * 0.055',
            '1.20 - TimeSeconds * 0.11',
            '0.0020 * cos(a)',
            '0.0010 * cos(b)',
            'WaterRoughness = 0.18f',
            'WaterSpecular = 0.50f',
            'SprayDepthFadeCm = 6.0f',
            'SprayMaxOpacity = 0.07f',
            'return 0.07 * strand * axial',
            'Material->BlendMode = BLEND_Opaque',
            'Material->BlendMode = BLEND_Additive',
            'Material->SetShadingModel(MSM_Unlit)',
            'Material->bTangentSpaceNormal = false',
            'Data->EmissiveColor.Connect(0, Tint)',
            'Material->bScreenSpaceReflections = false',
            'TRIAD_IPV5D_WORLD_XY_CALM_WORLDSPACE_DUAL_WAVE_NORMAL_R4',
            'TRIAD_IPV5D_SPRAY_UNLIT_SPARSE_BREAKUP_R4',
            'revision=R5',
            'waterBlend=Opaque',
            'waterRefraction=NotApplicableOpaque',
            'sprayBlend=Additive',
            'sprayShading=Unlit',
            'sprayRefraction=Unplugged',
            'sprayTwoSided=false',
            'suppressorBlend=Additive',
            'suppressorShading=Unlit',
            'suppressorExpressions=0',
            'suppressorOutputs=Zero',
            'Data->EmissiveColor.UseConstant = 1',
            'Data->Opacity.UseConstant = 1',
            'Data->Normal.Expression || Data->Refraction.Expression',
            'FAssetCompilingManager::Get().FinishAllCompilation()',
            'SaveLoadedAssets(ExactSaveTargets, false)',
            'UPackageTools::ReloadPackages(',
            'ValidateCompiledMaterial(Water, OutReport)',
            'ValidateCompiledMaterial(Spray, OutReport)',
            'ValidateCompiledMaterial(Suppressor, OutReport)',
            'Resource->IsDefaultMaterial()',
            'ShaderMap->IsValidForRendering()',
            'TRIAD_V5D_FOUNTAIN_MATERIAL_ABSENT_BACKUP_V2',
            'TRIAD_V5D_FOUNTAIN_MATERIAL_R4_TO_R5_BACKUP_V1',
            'AUTOMATIC_ROLLBACK_OK restoredToAbsent=true',
            'AUTOMATIC_ROLLBACK_OK newArtifactsAbsent=true '
            'existingPairBytesUntouched=true',
            'IDEMPOTENT_',
            'V5D_FOUNTAIN_MATERIALS_REFUSED_MIXED',
            'sourceV5V5BMaterialsUntouched=true',
        ):
            self.assertIn(fragment, combined)
        for extension in (
            '.uasset',
            '.uexp',
            '.ubulk',
            '.uptnl',
            '.m.ubulk',
            '.upayload',
        ):
            self.assertIn(f'TEXT("{extension}")', self.material_factory_cpp)
        for forbidden in (
            'DuplicateAsset(',
            'M_IPV5_FountainWater.M_IPV5_FountainWater',
            'M_IPV5B_FountainSpray.M_IPV5B_FountainSpray',
            'RM_IndexOfRefraction',
            'WaterOpacity',
            'WaterDepthFadeCm',
            'WaterPixelNormalOffset',
        ):
            self.assertNotIn(forbidden, self.material_factory_cpp)

        self.assertEqual(
            2,
            self.material_factory_cpp.count(
                'Material->RefractionMethod = RM_PixelNormalOffset;'
            ),
        )
        self.assertEqual(
            3,
            self.material_factory_cpp.count('Material->TwoSided = false;'),
        )

        self.assertEqual(
            2,
            self.runtime_cpp.count(
                'SuccessorWaterBase->bTangentSpaceNormal ||'
            ),
        )
        self.assertEqual(
            2,
            self.runtime_cpp.count(
                'SuccessorWaterBase->GetBlendMode() != BLEND_Opaque ||'
            ),
        )
        self.assertEqual(
            2,
            self.runtime_cpp.count('SuccessorSprayBase->TwoSided ||'),
        )
        self.assertEqual(
            2,
            self.runtime_cpp.count(
                'SuccessorSprayBase->GetBlendMode() != BLEND_Additive ||'
            ),
        )
        self.assertEqual(
            2,
            self.runtime_cpp.count(
                'SuccessorSprayBase->RefractionMethod != '
                'RM_PixelNormalOffset ||'
            ),
        )
        create_start = self.material_factory_cpp.index(
            'bool EnsureFountainRealismMaterialAssets('
        )
        create = self.material_factory_cpp[create_start:]
        self.assertLess(
            create.index('CreateAbsentStateReceipt('),
            create.index('CreateWaterMaterial('),
        )
        self.assertLess(
            create.index('SaveLoadedAssets(ExactSaveTargets, false)'),
            create.index('UPackageTools::ReloadPackages('),
        )
        reload_index = create.index('UPackageTools::ReloadPackages(')
        self.assertGreater(
            create.index('ValidateFountainRealismMaterialAssets(', reload_index),
            reload_index,
        )

    def test_deterministic_layout_breaks_cones_rings_and_empty_center(self) -> None:
        expected_counts = (
            'SourceOuterPlumeCount = 12',
            'EdgeFoamCount = 3',
            'PrimaryJetCount = 12',
            'SecondarySprayCount = 24',
            'ImpactRippleCount = 18',
            'CentralSprayCount = 8',
            'CentralRippleCount = 5',
        )
        for fragment in expected_counts:
            self.assertIn(fragment, self.runtime_cpp)
        for fragment in (
            'BuildDeterministicSuccessorLayout',
            'Hash32',
            'HashRange',
            'SecondarySprayWorldTransforms',
            'ImpactRippleWorldTransforms',
            'CentralSprayWorldTransforms',
            'CentralRippleWorldTransforms',
            'FMath::Abs(Scale.X - Scale.Y) < 0.04',
            'perfectConesAndIdenticalCircularRingsBroken=true',
            'primaryJetVisibleInstances=0',
            'centralPlumeVisibleInstances=0',
            'secondarySprayVisibleInstances=0',
            'centralSprayVisibleInstances=0',
            'allLegacyAndSuccessorPlumeCarrierSilhouettesSuppressed=true',
            'TRIAD.Istana.ExploreV5D.Fountain.DeterministicSuccessorLayout',
        ):
            self.assertIn(fragment, self.runtime_cpp)
        for nondeterministic in ('FMath::FRand', 'FMath::Rand', 'FRandomStream'):
            self.assertNotIn(nondeterministic, self.runtime_cpp)
        self.assertIn(
            'FMath::Abs(SecondaryX - SecondaryY) < 0.08', self.runtime_cpp
        )
        self.assertIn(
            'SecondaryY = SecondaryY >= SecondaryX',
            self.runtime_cpp,
        )
        self.assertIn('FMath::Min(0.64, SecondaryY + 0.11)', self.runtime_cpp)
        self.assertIn('RippleY = RippleY >= RippleX', self.runtime_cpp)
        self.assertIn('FMath::Min(1.02, RippleY + 0.11)', self.runtime_cpp)
        self.assertIn(
            'Every central ripple breaks perfect circular repetition',
            self.runtime_cpp,
        )
        self.assertIn(
            'visually perfect circular central ripple instead of bounded anisotropy',
            self.runtime_cpp,
        )
        for fragment in (
            'HashRange(Seed ^ 0x6D21F38Bu, 0.16, 0.26)',
            'HashRange(Seed ^ 0xB8A4D217u, 0.14, 0.24)',
            'HashRange(Seed ^ 0x2F9C561Du, 0.70, 0.92)',
            'HashRange(FragmentSeed ^ 0xBD2F1907u, 0.055, 0.11)',
            'FVector(0.34, 0.38, 1.55)',
            'HashRange(Seed ^ 0xB51E73C9u, 35.0, 125.0)',
            'HashRange(Seed ^ 0x58A6E2D3u, 0.07, 0.16)',
            '220.0 + 20.0 * static_cast<double>(Index)',
            'central ripple outside the visible inherited water annulus',
            'Every central ripple lands on the visible inherited water annulus',
        ):
            self.assertIn(fragment, self.runtime_cpp)
        central_assignment = self.runtime_cpp.split(
            'AssignMeshAndMaterial(\n        CentralPlumeComponent,', 1
        )[1].split(');', 1)[0]
        self.assertIn('SourceOuterMesh', central_assignment)
        self.assertNotIn('SourceCentralMesh', central_assignment)
        self.assertIn(
            'Primary jets use the physically narrower R2 scale envelope',
            self.runtime_cpp,
        )
        for fragment in (
            'bool bExpectedVisible',
            'const bool bVisibilityMatches = Component &&',
            'PrimaryJetInstances->SetVisibility(false, true)',
            'PrimaryJetInstances->SetHiddenInGame(true)',
            'PrimaryJetInstances->SetRenderInMainPass(false)',
            'PrimaryJetInstances->SetRenderInDepthPass(false)',
            'SecondarySprayInstances->SetVisibility(false, true)',
            'SecondarySprayInstances->SetHiddenInGame(true)',
            'SecondarySprayInstances->SetRenderInMainPass(false)',
            'SecondarySprayInstances->SetRenderInDepthPass(false)',
            'CentralPlumeComponent->SetVisibility(false, true)',
            'CentralPlumeComponent->SetHiddenInGame(true)',
            'CentralPlumeComponent->SetRenderInMainPass(false)',
            'CentralPlumeComponent->SetRenderInDepthPass(false)',
            'CentralSprayInstances->SetVisibility(false, true)',
            'CentralSprayInstances->SetHiddenInGame(true)',
            'CentralSprayInstances->SetRenderInMainPass(false)',
            'CentralSprayInstances->SetRenderInDepthPass(false)',
        ):
            self.assertIn(fragment, self.runtime_cpp)

    def test_successor_spray_material_owns_every_foam_and_ripple_renderer(
        self,
    ) -> None:
        configure_start = self.runtime_cpp.index('ConfigureFountainRealism(')
        configure_end = self.runtime_cpp.index(
            'ValidateFountainRealism(FString& OutReport)', configure_start
        )
        configure = self.runtime_cpp[configure_start:configure_end]
        assignment_start = configure.index(
            'AssignMeshAndMaterial(\n        WaterSurfaceComponent,'
        )
        assignment_end = configure.index(
            'WaterSurfaceComponent->AddInstance(', assignment_start
        )
        assignments = configure[assignment_start:assignment_end]
        self.assertEqual(7, assignments.count('InSuccessorSprayMaterial'))
        self.assertNotIn('SourceFoamMaterial', assignments)
        for component in (
            'EdgeFoamComponent',
            'ImpactRippleInstances',
            'CentralRippleInstances',
        ):
            block = assignments.split(
                f'AssignMeshAndMaterial(\n        {component},', 1
            )[1].split(');', 1)[0]
            self.assertIn('InSuccessorSprayMaterial', block)

        validation_start = self.runtime_cpp.index(
            'const TArray<FTransform> SurfaceTransforms = {'
        )
        validation_end = self.runtime_cpp.index(
            'FTRIADIstanaExploreV5DFountainSuccessorLayout RebuiltLayout;',
            validation_start,
        )
        validation = self.runtime_cpp[validation_start:validation_end]
        self.assertEqual(7, validation.count('SavedSuccessorSprayMaterial'))
        for component in (
            'EdgeFoamComponent',
            'ImpactRippleInstances',
            'CentralRippleInstances',
        ):
            block = validation.split(
                f'ValidateRenderOnlyHism(\n            {component},', 1
            )[1].split('Error)', 1)[0]
            self.assertIn('SavedSuccessorSprayMaterial', block)
            self.assertNotIn('SavedSourceEdgeFoamMaterial', block)

        read_start = self.runtime_cpp.index('bool ReadExactInstanceTransforms(')
        read_end = self.runtime_cpp.index(
            'bool ReadExactSingleTransform(', read_start
        )
        read_contract = self.runtime_cpp[read_start:read_end]
        self.assertIn('serialized CPU instance transforms', read_contract)
        self.assertNotIn('BuildTreeIfOutdated', read_contract)
        self.assertNotIn('IsTreeFullyBuilt', read_contract)
        self.assertNotIn('IsAsyncBuilding', read_contract)

    def test_source_replacement_and_embedded_section_are_failure_atomic(
        self,
    ) -> None:
        replacement_start = self.runtime_cpp.index(
            'void SetSourceRendererReplacementState('
        )
        replacement_end = self.runtime_cpp.index(
            'void ClearSuccessorComponents(', replacement_start
        )
        replacement = self.runtime_cpp[replacement_start:replacement_end]
        self.assertIn('SetVisibility(!bSuccessorOwnsRendering, true)', replacement)
        self.assertIn('SetHiddenInGame(bSuccessorOwnsRendering)', replacement)
        self.assertIn(
            'SetRenderInMainPass(!bSuccessorOwnsRendering)', replacement
        )
        self.assertIn(
            'SetRenderInDepthPass(!bSuccessorOwnsRendering)', replacement
        )
        for forbidden in (
            'SetStaticMesh',
            'SetMaterial',
            'ClearInstances',
            'UpdateInstanceTransform',
            'SetCollision',
            'SetCanEverAffectNavigation',
        ):
            self.assertNotIn(forbidden, replacement)

        configure_start = self.runtime_cpp.index('ConfigureFountainRealism(')
        configure_end = self.runtime_cpp.index(
            'ValidateFountainRealism(FString& OutReport)', configure_start
        )
        configure = self.runtime_cpp[configure_start:configure_end]
        apply_index = configure.index(
            'SetSourceRendererReplacementState(SourceComponents, true)'
        )
        suppress_index = configure.index(
            'HardscapeSuccessor->SetMaterial(', apply_index
        )
        validate_index = configure.index('ValidateFountainRealism(Report)')
        restore_index = configure.index(
            'SetSourceRendererReplacementState(SourceComponents, false)',
            validate_index,
        )
        self.assertLess(apply_index, validate_index)
        self.assertLess(suppress_index, validate_index)
        self.assertLess(validate_index, restore_index)
        self.assertIn(
            'HardscapeSuccessor->OverrideMaterials =\n'
            '            OriginalHardscapeOverrides',
            configure,
        )

        for fragment in (
            'RestoreInheritedFountainRenderingForFailure()',
            'V5D fountain successor drifted at runtime and restored',
            'SetActorHiddenInGame(true)',
            'virtual void EndPlay',
        ):
            self.assertIn(fragment, self.runtime_h + self.runtime_cpp)
        restore_start = self.runtime_cpp.index(
            'RestoreInheritedFountainRenderingForFailure()\n{'
        )
        restore_end = self.runtime_cpp.index(
            'void ATRIADIstanaExploreV5DFountainRealismActor::EndPlay',
            restore_start,
        )
        restore = self.runtime_cpp[restore_start:restore_end]
        for successor in (
            'WaterSurfaceComponent',
            'EdgeFoamComponent',
            'PrimaryJetInstances',
            'SecondarySprayInstances',
            'ImpactRippleInstances',
            'CentralPlumeComponent',
            'CentralSprayInstances',
            'CentralRippleInstances',
        ):
            self.assertIn(successor, restore)
        self.assertIn('Component->SetVisibility(false, true)', restore)
        self.assertIn('Component->SetHiddenInGame(true)', restore)
        self.assertIn('Component->SetRenderInMainPass(false)', restore)
        self.assertIn('Component->SetRenderInDepthPass(false)', restore)
        self.assertIn(
            'SavedHardscapeOriginalOverrideMaterials.Num() == 3', restore
        )
        self.assertIn(
            'OverrideMaterials =\n'
            '                    SavedHardscapeOriginalOverrideMaterials',
            restore,
        )

    def test_every_successor_primitive_is_render_only(self) -> None:
        configure_start = self.runtime_cpp.index('void ConfigureRenderOnlyHism(')
        configure_end = self.runtime_cpp.index(
            'void AssignMeshAndMaterial(', configure_start
        )
        configure = self.runtime_cpp[configure_start:configure_end]
        for fragment in (
            'ECollisionEnabled::NoCollision',
            'SetCollisionResponseToAllChannels(ECR_Ignore)',
            'SetGenerateOverlapEvents(false)',
            'SetCanEverAffectNavigation(false)',
            'SetRenderInMainPass(true)',
            'SetRenderInDepthPass(true)',
            'SetCastShadow(false)',
            'bCastContactShadow = false',
            'bEnableDensityScaling = false',
        ):
            self.assertIn(fragment, configure)

        for truth in (
            'bAppearanceOnly = true',
            'bSourceMeshMaterialAssetsModified = false',
            'bSourceTransformsOrCensusModified = false',
            'bEmbeddedHardscapeWaterSectionSuppressed = false',
            'bCollisionNavigationSensorOrRfAuthority = false',
            'bHydraulicSimulationOrOperatingStateClaimed = false',
            'bSurveyOrAsBuiltClaimed = false',
        ):
            self.assertIn(truth, self.runtime_h)

    def test_hybrid_builder_and_cold_runtime_validator_require_one_actor(self) -> None:
        for fragment in (
            '#include "TRIADIstanaExploreV5DFountainRealismActor.h"',
            '#include "TRIADIstanaExploreV5DFountainMaterialFactory.h"',
            'FountainRealismCount',
            'FountainRealismCount != 1',
            'FountainRealism->V5AppearanceActor != V5',
            'FountainRealism->V5BVisualActor != V5B',
            'FountainRealism->ValidateFountainRealism(',
            'fountainRealism={%s}',
            'ExistingFountainRealismCount != 0',
            'SpawnActor<ATRIADIstanaExploreV5DFountainRealismActor>',
            'ConfigureFountainRealism(',
            'EnsureFountainRealismMaterialAssets(',
            'FountainWaterMaterial',
            'FountainSprayMaterial',
            'EmbeddedWaterSuppressorMaterial',
            'FountainMaterials.Num() != 3',
            'GetEmbeddedWaterSuppressorMaterialObjectPath()',
            'EXPLORE_V5D_HYBRID_BUILD_FAILED_FOUNTAIN_MATERIALS',
            'EXPLORE_V5D_HYBRID_BUILD_FAILED_FOUNTAIN_REALISM',
        ):
            self.assertIn(fragment, self.hybrid_cpp)
        self.assertIn(
            'EnsureIstanaExploreV5DFountainRealismMaterialAssets',
            self.hybrid_h,
        )
        builder_start = self.hybrid_cpp.index(
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)'
        )
        builder = self.hybrid_cpp[builder_start:]
        self.assertLess(
            builder.index('EnsureFountainRealismMaterialAssets('),
            builder.index('SpawnActor<ATRIADIstanaExploreV5DFountainRealismActor>'),
        )
        self.assertLess(
            builder.index('EnsureFountainRealismMaterialAssets('),
            builder.index('UEditorLoadingAndSavingUtils::SaveMap('),
        )

    def test_v5b_admits_only_the_complete_exact_tagged_r7_water_override(
        self,
    ) -> None:
        for fragment in (
            '#include "TRIADIstanaExploreV5DFountainRealismActor.h"',
            'bV5DFountainHardscapeWaterPresentationTag',
            'ValidateActiveHardscapeWaterPresentationForSourceActor(',
            'bV5DFountainHardscapeWaterPresentationTag &&\n'
            '        !bV5DFountainHardscapeWaterPresentation',
            '(!bV5DFountainHardscapeWaterPresentation &&\n'
            '         !ValidateMeshAndMaterials(',
            'exact sole-owner/source/three-slot/simulation-isolation gate',
        ):
            self.assertIn(fragment, self.v5b_runtime_cpp)

        bridge_start = self.runtime_cpp.index(
            'ValidateActiveHardscapeWaterPresentationForSourceActor('
        )
        bridge_end = self.runtime_cpp.index(
            'ExpectedSourceOuterPlumeCount()', bridge_start
        )
        bridge = self.runtime_cpp[bridge_start:bridge_end]
        for fragment in (
            'TActorIterator<ATRIADIstanaExploreV5DFountainRealismActor>',
            'It->V5BVisualActor == Candidate',
            'MatchCount != 1',
            'Match->ValidateFountainRealism(FountainReport)',
            'completeR7FountainContract=true',
        ):
            self.assertIn(fragment, bridge)
        self.assertNotIn('ValidateExploreV5BVisuals', bridge)

        full_validator_start = self.runtime_cpp.index(
            'ValidateFountainRealism(FString& OutReport) const'
        )
        full_validator_end = self.runtime_cpp.index(
            'void ATRIADIstanaExploreV5DFountainRealismActor::BeginPlay()',
            full_validator_start,
        )
        full_validator = self.runtime_cpp[
            full_validator_start:full_validator_end
        ]
        self.assertNotIn('ValidateExploreV5BVisuals(', full_validator)
        self.assertNotIn(
            'ValidateActiveHardscapeWaterPresentationForSourceActor(',
            full_validator,
        )

    def test_hardscape_water_slot_identity_survives_cooking(self) -> None:
        helper_start = self.runtime_cpp.index(
            'int32 FindExactHardscapeWaterMaterialSlot('
        )
        helper_end = self.runtime_cpp.index(
            'bool IsExactTargetWorld(', helper_start
        )
        helper = self.runtime_cpp[helper_start:helper_end]
        for fragment in (
            'GetMaterialIndex(HardscapeWaterMaterialSlotName)',
            'WaterStaticMaterial.MaterialSlotName ==',
            '#if WITH_EDITORONLY_DATA',
            'WaterStaticMaterial.ImportedMaterialSlotName ==',
        ):
            self.assertIn(fragment, helper)
        self.assertNotIn(
            'GetMaterialIndexFromImportedMaterialSlotName(',
            self.runtime_cpp,
        )
        self.assertEqual(
            self.runtime_cpp.count(
                'FindExactHardscapeWaterMaterialSlot(HardscapeMesh)'
            ),
            2,
        )

    def test_aerial_handoff_treats_successor_as_authored_visual_owner(self) -> None:
        resolver_start = self.policy_cpp.index(
            'ResolveAuthoredCoreVisualActors('
        )
        resolver_end = self.policy_cpp.index(
            'RestoreGroundLevelPresentation(', resolver_start
        )
        resolver = self.policy_cpp[resolver_start:resolver_end]
        self.assertIn(
            'FindExactlyOne<ATRIADIstanaExploreV5DFountainRealismActor>',
            resolver,
        )
        self.assertIn('FountainCount != 1', resolver)
        self.assertIn(
            'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>',
            resolver,
        )
        self.assertIn('PublicRealmCount != 1', resolver)
        for owner in (
            'Scene,', 'V2,', 'V3,', 'V4,', 'V5B,', 'Fountain,',
            'Ground,', 'Tree,', 'PublicRealm}',
        ):
            self.assertIn(owner, resolver)


if __name__ == "__main__":
    unittest.main()
