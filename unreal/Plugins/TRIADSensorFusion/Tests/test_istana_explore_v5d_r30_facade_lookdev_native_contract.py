from __future__ import annotations

import hashlib
import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
PLUGIN = UNREAL / "Plugins" / "TRIADSensorFusion"
RUNTIME_PUBLIC = PLUGIN / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = PLUGIN / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = PLUGIN / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = PLUGIN / "Source" / "TRIADSensorFusionEditor" / "Private"

ACTOR_H = RUNTIME_PUBLIC / "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
ACTOR_CPP = RUNTIME_PRIVATE / "TRIADIstanaExploreV5DR30FacadeLookdevActor.cpp"
NATIVE_TEST = (
    RUNTIME_PRIVATE
    / "Tests"
    / "TRIADIstanaExploreV5DR30FacadeLookdevActorTests.cpp"
)
FACTORY_H = EDITOR_PRIVATE / "TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h"
FACTORY_CPP = EDITOR_PRIVATE / "TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.cpp"
EDITOR_H = EDITOR_PUBLIC / "TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.h"
EDITOR_CPP = EDITOR_PRIVATE / "TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.cpp"
TREE_ACTOR_H = RUNTIME_PUBLIC / "TRIADIstanaExploreV5DTreeRealismActor.h"
TREE_ACTOR_CPP = RUNTIME_PRIVATE / "TRIADIstanaExploreV5DTreeRealismActor.cpp"
TREE_EDITOR_H = EDITOR_PUBLIC / "TRIADIstanaExploreV5DTreeRealismEditorLibrary.h"
TREE_EDITOR_CPP = EDITOR_PRIVATE / "TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp"
WRAPPER = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DContextFacadeR30NativeTransactionV1.ps1"
)
CONTRACT = (
    UNREAL
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R30FacadeLookdev"
    / "r30_facade_lookdev.contract.json"
)
TREE_ROOT = (
    UNREAL
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "TreeRealism"
)
TREE_SOURCE_CLOSURE = (
    CONTRACT.parent / "r30_tree_material_response_v3.source_closure.json"
)
TREE_V3_CONTRACT = (
    TREE_ROOT / "istana_public_view_v5d_tree_realism.material_response_amendment.v3.json"
)
TREE_AUDIT_RENDERER = TREE_ROOT / "render_tree_material_response_audit.py"
TREE_AUDIT_README = TREE_ROOT / "MaterialResponseAudit" / "README.md"
TREE_AUDIT_JSON = (
    TREE_ROOT / "MaterialResponseAudit" / "tree_material_response_audit.json"
)
TREE_AUDIT_PNG = (
    TREE_ROOT / "MaterialResponseAudit" / "tree_material_response_contact_sheet.png"
)

EXPECTED_SOURCE_PINS = (
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR30FacadeLookdevActor.h",
        ACTOR_H,
    ),
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR30FacadeLookdevActor.cpp",
        ACTOR_CPP,
    ),
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR30FacadeLookdevActorTests.cpp",
        NATIVE_TEST,
    ),
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h",
        FACTORY_H,
    ),
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.cpp",
        FACTORY_CPP,
    ),
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.h",
        EDITOR_H,
    ),
    (
        r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.cpp",
        EDITOR_CPP,
    ),
)

EXPECTED_SOURCE_ASSET_PINS = (
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_facade_lookdev.contract.json",
        CONTRACT,
    ),
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_tree_material_response_v3.source_closure.json",
        TREE_SOURCE_CLOSURE,
    ),
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\istana_public_view_v5d_tree_realism.material_response_amendment.v3.json",
        TREE_V3_CONTRACT,
    ),
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\render_tree_material_response_audit.py",
        TREE_AUDIT_RENDERER,
    ),
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\README.md",
        TREE_AUDIT_README,
    ),
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\tree_material_response_audit.json",
        TREE_AUDIT_JSON,
    ),
    (
        r"SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\tree_material_response_contact_sheet.png",
        TREE_AUDIT_PNG,
    ),
)

EXPECTED_TREE_PROMOTION_PINS = (
    (
        "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
        "TRIADIstanaExploreV5DTreeRealismActor.h",
        TREE_ACTOR_H,
    ),
    (
        "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
        "TRIADIstanaExploreV5DTreeRealismActor.cpp",
        TREE_ACTOR_CPP,
    ),
    (
        "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
        "TRIADIstanaExploreV5DTreeRealismEditorLibrary.h",
        TREE_EDITOR_H,
    ),
    (
        "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
        "TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp",
        TREE_EDITOR_CPP,
    ),
)

EXPECTED_MATERIAL_SPECS = (
    ("MI_IPV5D_R29_GlassCool", "MI_IPV5D_R30_GlassCool", None, False, 4.0),
    ("MI_IPV5D_R29_GlassWarm", "MI_IPV5D_R30_GlassWarm", None, False, 4.0),
    ("MI_IPV5D_R29_GlassNeutral", "MI_IPV5D_R30_GlassNeutral", None, False, 4.0),
    ("MI_IPV5D_R29_FrameLight", "MI_IPV5D_R30_FrameLight", "Shutter", True, 0.8),
    ("MI_IPV5D_R29_FrameDark", "MI_IPV5D_R30_FrameDark", "Shutter", True, 0.8),
    ("MI_IPV5D_R29_FrameBronze", "MI_IPV5D_R30_FrameBronze", "Shutter", True, 0.8),
    ("MI_IPV5D_R29_SillLight", "MI_IPV5D_R30_SillLight", "Stone", True, 1.5),
    ("MI_IPV5D_R29_SillDark", "MI_IPV5D_R30_SillDark", "Stone", True, 1.5),
    ("MI_IPV5D_R29_RoofTrim", "MI_IPV5D_R30_RoofTrim", "Slate", True, 1.2),
    ("MI_IPV5D_R29_Canopy", "MI_IPV5D_R30_Canopy", "DarkTimber", True, 1.0),
    ("MI_IPV5D_R29_BalconyRail", "MI_IPV5D_R30_BalconyRail", "Shutter", True, 0.8),
)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


class R30FacadeLookdevNativeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (
            ACTOR_H,
            ACTOR_CPP,
            NATIVE_TEST,
            FACTORY_H,
            FACTORY_CPP,
            EDITOR_H,
            EDITOR_CPP,
            TREE_ACTOR_H,
            TREE_ACTOR_CPP,
            TREE_EDITOR_H,
            TREE_EDITOR_CPP,
            WRAPPER,
            CONTRACT,
            TREE_SOURCE_CLOSURE,
            TREE_V3_CONTRACT,
            TREE_AUDIT_RENDERER,
            TREE_AUDIT_README,
            TREE_AUDIT_JSON,
            TREE_AUDIT_PNG,
        ):
            if not path.is_file():
                raise AssertionError(f"missing isolated R30 native source: {path}")
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor = ACTOR_CPP.read_text(encoding="utf-8")
        cls.native_test = NATIVE_TEST.read_text(encoding="utf-8")
        cls.factory_h = FACTORY_H.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_runtime_successor_retains_exactly_three_predecessor_renderers(self) -> None:
        self.assertEqual(3, self.actor_h.count("TObjectPtr<UStaticMeshComponent>"))
        self.assertEqual(3, self.actor.count("CreateDefaultSubobject<UStaticMeshComponent>"))
        for token in (
            "RetainedR28ConnectivePublicRealmMesh",
            "RetainedR29ContextFacadeCoverageMesh",
            "RetainedOuterGroundMesh",
            "RetainedR28OuterGroundMaterial",
            "ContextFacadeMaterialOverrides",
            "RetainedR28ConnectivePublicRealmRenderOnly",
            "R30ContextFacadeLookdevRenderOnly",
            "RetainedR28OuterGroundOverlayRenderOnly",
            "ExpectedOwnedRendererCount()",
            "return 3;",
            "ExpectedFacadeMaterialOverrideCount()",
            "UE_ARRAY_COUNT(R30MaterialNames)",
        ):
            self.assertIn(token, self.actor_h + self.actor)
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsLookdevR30",
            self.actor,
        )

    def test_runtime_binds_all_overrides_by_r29_slot_name_not_numeric_position(self) -> None:
        prepare = between(
            self.actor,
            "ConfigurePreparedR30FacadeLookdevHandoff(",
            "ActivateAfterR29FacadeRemoval(",
        )
        validation = between(
            self.actor,
            "bool ValidateFacadeRenderer(",
            "bool ResolveOwnerRosters(",
        )
        for section in (prepare, validation):
            normalized = compact(section)
            self.assertIn(
                "GetMaterialIndex(FName(R29MaterialNames[SpecIndex]))",
                normalized,
            )
        self.assertIn(
            "SetMaterial(SlotIndex,InAssets.ContextFacadeMaterialOverrides[SpecIndex])",
            compact(prepare),
        )
        self.assertNotIn("SetMaterial(SpecIndex", compact(prepare))
        self.assertEqual(11, len(re.findall(r'TEXT\("MI_IPV5D_R29_[A-Za-z]+"\)', between(
            self.actor, "const TCHAR* const R29MaterialNames[]", "const TCHAR* const R30MaterialNames[]"
        ))))
        self.assertEqual(11, len(re.findall(r'TEXT\("MI_IPV5D_R30_[A-Za-z]+"\)', between(
            self.actor, "const TCHAR* const R30MaterialNames[]", "const TCHAR* const R28PublicRealmMaterialNames[]"
        ))))

    def test_runtime_preserves_identity_topology_geography_and_negative_authority(self) -> None:
        for token in (
            "SetActorEnableCollision(false)",
            "SetRelativeTransform(FTransform::Identity)",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCollisionResponseToAllChannels(ECR_Ignore)",
            "SetGenerateOverlapEvents(false)",
            "SetCanEverAffectNavigation(false)",
            "SetAffectDistanceFieldLighting(false)",
            "SetRenderCustomDepth(false)",
            "bHiddenInSceneCapture = true",
            "bPredecessorMeshesAndTransformsRetainedUnmodified = true",
            "bR28OrR29ArchitectureRetainedOrCoRendered = false",
            "bCollisionNavigationSimulationSensorOrRfAuthority = false",
            "bExistingSimulationRfProviderCesiumOrGeospatialInputsModified = false",
            "bRuntimeGeometryGenerated = false",
            "collision=false navigation=false simulationAuthority=false",
            "sensorAuthority=false rfAuthority=false cesiumModified=false",
            "geospatialInputsModified=false providerInputsModified=false",
        ):
            self.assertIn(token, self.actor_h + self.actor)
        for forbidden in (
            "SetCollisionEnabled(ECollisionEnabled::QueryOnly)",
            "SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics)",
            "SetSimulatePhysics(true)",
            "SetActorTransform(",
            "SetWorldTransform(",
            "SetMeshDescription(",
            "BuildFromMeshDescriptions(",
            "NaniteSettings.bEnabled",
        ):
            self.assertNotIn(forbidden, self.actor_h + self.actor)

    def test_runtime_observes_context_policy_at_two_hz_without_mutating_it(self) -> None:
        for token in (
            "virtual void BeginPlay() override",
            "virtual void Tick(float DeltaSeconds) override",
            "virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override",
            "SynchronizeOwnedVisibilityWithContextPolicy",
            "TWeakObjectPtr<ATRIADIstanaExploreV5DContextPolicyActor>",
            "bProviderReadinessObservedWithoutPolicyMutation = true",
            "constexpr float ProviderMirrorTickSeconds = 0.5f",
            "PrimaryActorTick.bCanEverTick = true",
            "PrimaryActorTick.bStartWithTickEnabled = true",
            "PrimaryActorTick.TickInterval = ProviderMirrorTickSeconds",
            "providerReadinessObservedOnly=true providerMirrorHz=2",
        ):
            self.assertIn(token, self.actor_h + self.actor)

        begin_play = between(
            self.actor,
            "void ATRIADIstanaExploreV5DR30FacadeLookdevActor::BeginPlay()",
            "void ATRIADIstanaExploreV5DR30FacadeLookdevActor::Tick(",
        )
        tick = between(
            self.actor,
            "void ATRIADIstanaExploreV5DR30FacadeLookdevActor::Tick(",
            "void ATRIADIstanaExploreV5DR30FacadeLookdevActor::EndPlay(",
        )
        end_play = between(
            self.actor,
            "void ATRIADIstanaExploreV5DR30FacadeLookdevActor::EndPlay(",
            "SynchronizeOwnedVisibilityWithContextPolicy(FString& OutError)",
        )
        for section in (begin_play, tick):
            self.assertIn("SynchronizeOwnedVisibilityWithContextPolicy(Error)", section)
        self.assertIn("RemoveTickPrerequisiteActor(Policy)", end_play)
        self.assertIn("RuntimeContextPolicyActor.Reset()", end_play)

        synchronization = between(
            self.actor,
            "SynchronizeOwnedVisibilityWithContextPolicy(FString& OutError)",
            "bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::ValidateAssetRoster(",
        )
        synchronization_compact = compact(synchronization)
        for token in (
            "ATRIADIstanaExploreV5DContextPolicyActor::StaticClass()",
            "PolicyCount != 1",
            "RuntimeContextPolicyActor = Policy",
            "AddTickPrerequisiteActor(Policy)",
            "Policy->bLocalBuildingFallbackCurrentlyHidden",
            "bProviderReady != bExpectedProviderReady",
            "SetProviderReady(bExpectedProviderReady, OutError)",
        ):
            self.assertIn(token, synchronization)
        for forbidden in (
            "Policy->Modify(",
            "Policy->SetProviderReady(",
            "Policy->bLocalBuildingFallbackCurrentlyHidden =",
            "Policy->bProviderReady =",
        ):
            self.assertNotIn(forbidden, synchronization)
        self.assertIn(
            "if(bProviderReady!=bExpectedProviderReady&&"
            "!SetProviderReady(bExpectedProviderReady,OutError))",
            synchronization_compact,
        )

        invalid_policy = between(
            synchronization,
            "if (PolicyCount != 1 || !Policy || !bConfigured ||",
            "if (RuntimeContextPolicyActor.Get() != Policy)",
        )
        for token in (
            "PolicyCount != 1",
            "RuntimeContextPolicyActor.Get()",
            "RemoveTickPrerequisiteActor(Previous)",
            "RuntimeContextPolicyActor.Reset()",
            "bConfigured &&",
            "bReplacementActivated &&",
            "SetProviderReady(false, LocalFallbackReport)",
            "ownedLocalFallbackRestored=%s",
        ):
            self.assertIn(token, invalid_policy)
        self.assertLess(
            invalid_policy.index("RemoveTickPrerequisiteActor(Previous)"),
            invalid_policy.index("RuntimeContextPolicyActor.Reset()"),
        )
        self.assertLess(
            invalid_policy.index("RuntimeContextPolicyActor.Reset()"),
            invalid_policy.index("SetProviderReady(false, LocalFallbackReport)"),
        )
        for forbidden in (
            "Policy->Modify(",
            "Policy->SetProviderReady(",
            "Policy->bLocalBuildingFallbackCurrentlyHidden =",
            "Policy->bProviderReady =",
        ):
            self.assertNotIn(forbidden, invalid_policy)

        provider_transition = between(
            self.actor,
            "SetProviderReady(\n    bool bInProviderReady,",
            "ValidatePreparedR30FacadeLookdevHandoff(FString& OutReport)",
        )
        for token in (
            "const bool bBefore = bProviderReady",
            "bProviderReady = bInProviderReady",
            "SetOwnedPresentationVisible(this, !bProviderReady)",
            "bProviderReady = bBefore",
            "provider visibility transition failed and was restored",
        ):
            self.assertIn(token, provider_transition)
        for token in (
            "Provider mirror tick exists",
            "Provider mirror starts enabled",
            "Provider mirror is exactly 2 Hz",
            "FMath::IsNearlyEqual(Actor->PrimaryActorTick.TickInterval, 0.5f)",
            "Provider readiness is observed only",
        ):
            self.assertIn(token, self.native_test)

    def test_two_phase_handoff_is_exactly_r29_to_r30_and_never_co_renders(self) -> None:
        prepare = between(
            self.actor,
            "ConfigurePreparedR30FacadeLookdevHandoff(",
            "ActivateAfterR29FacadeRemoval(",
        )
        activate = between(
            self.actor,
            "ActivateAfterR29FacadeRemoval(",
            "SetProviderReady(",
        )
        for token in (
            "R28ClassCount != 0",
            "R28TagCount != 0",
            "R29ClassCount != 1",
            "R29TagCount != 1",
            "R30ClassCount != 1",
            "R30TagCount != 1",
            "ValidateR29FacadeEnvironment",
            "SetOwnedPresentationVisible(this, false)",
        ):
            self.assertIn(token, prepare)
        for token in (
            "R28ClassCount != 0",
            "R28TagCount != 0",
            "R29ClassCount != 0",
            "R29TagCount != 0",
            "R30ClassCount != 1",
            "R30TagCount != 1",
            "ActiveR29FacadeRenderers != 0",
        ):
            self.assertIn(token, activate)

    def test_factory_material_specs_exactly_match_the_eleven_slot_policy(self) -> None:
        block = between(
            self.factory,
            "const FMaterialSpec MaterialSpecs[] = {",
            "static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 11);",
        )
        pattern = re.compile(
            r'\{TEXT\("(?P<slot>[^"]+)"\),\s*TEXT\("(?P<asset>[^"]+)"\),\s*'
            r'(?P<texture>nullptr|TEXT\("[^"]+"\)),\s*'
            r'(?P<switch>true|false),\s*(?P<metres>[0-9.]+)f,'
        )
        actual = []
        for match in pattern.finditer(block):
            texture_token = match.group("texture")
            texture_set = None if texture_token == "nullptr" else texture_token[6:-2]
            actual.append(
                (
                    match.group("slot"),
                    match.group("asset"),
                    texture_set,
                    match.group("switch") == "true",
                    float(match.group("metres")),
                )
            )
        self.assertEqual(EXPECTED_MATERIAL_SPECS, tuple(actual))

    def test_factory_implements_a_real_static_texture_switch_for_all_pbr_channels(self) -> None:
        for token in (
            '#include "Materials/MaterialExpressionStaticBoolParameter.h"',
            '#include "Materials/MaterialExpressionStaticSwitch.h"',
            "const FName UseTextureSetParameter(TEXT(\"UseTextureSet\"))",
            "UMaterialExpressionStaticBoolParameter* AddStaticBoolParameter(",
            "AddExpression<UMaterialExpressionStaticBoolParameter>",
            "Parameter->ParameterName = UseTextureSetParameter",
            "Parameter->DefaultValue = true",
            "Parameter->DynamicBranch = false",
            "UMaterialExpressionStaticSwitch* AddStaticSwitch(",
            "AddExpression<UMaterialExpressionStaticSwitch>",
            "UMaterialExpressionStaticBoolParameter* UseTextureSet =",
            "R30.UseTextureSet.BaseColor",
            "R30.UseTextureSet.Normal",
            "R30.UseTextureSet.Roughness",
            "R30.UseTextureSet.Metallic",
            "R30.UseTextureSet.AmbientOcclusion",
            "StaticBoolParameterCount != 1",
            "StaticSwitchNodeCount != 5",
            "Cast<UMaterialExpressionStaticSwitchParameter>(Expression)",
            "forbidden duplicate-prone StaticSwitchParameter node",
            "SetStaticSwitchParameterValueEditorOnly(",
            "FMaterialParameterInfo(UseTextureSetParameter), Spec.bUseTextureSet",
            "if (Spec.bUseTextureSet)",
            "(Spec.bUseTextureSet ? ExpectedTextureParameterCount : 0)",
            "StaticParameters.StaticSwitchParameters.Num() != 1",
            "StaticParameters.StaticSwitchParameters[0].Value != Spec.bUseTextureSet",
            "namedStaticBoolParameters=1 plainStaticSwitches=5",
            "duplicateProneStaticSwitchParameterNodes=0",
        ):
            self.assertIn(token, self.factory)
        self.assertNotIn(
            "AddExpression<UMaterialExpressionStaticSwitchParameter>",
            self.factory,
        )
        connections = compact(self.factory)
        for textured, procedural in (
            ("BaseBlend", "GlassBase"),
            ("NormalBlend", "FlatNormal"),
            ("RoughnessBlend", "GlassRoughnessSaturate"),
            ("MetallicBlend", "Metallic"),
            ("AoBlend", "AoOne"),
        ):
            switch = {
                "BaseBlend": "BaseSwitch",
                "NormalBlend": "NormalSwitch",
                "RoughnessBlend": "RoughnessSwitch",
                "MetallicBlend": "MetallicSwitch",
                "AoBlend": "AoSwitch",
            }[textured]
            self.assertIn(f'Connect({textured},TEXT(""),{switch},TEXT("True")', connections)
            self.assertIn(f'Connect({procedural},TEXT(""),{switch},TEXT("False")', connections)
            self.assertIn(
                f'Connect(UseTextureSet,TEXT(""),{switch},TEXT("Value")',
                connections,
            )

    def test_factory_builds_and_validates_exact_opaque_fresnel_glass_graph(self) -> None:
        for token in (
            '#include "Materials/MaterialExpressionCameraVectorWS.h"',
            '#include "Materials/MaterialExpressionTransform.h"',
            '#include "Materials/MaterialExpressionFresnel.h"',
            '#include "Materials/MaterialExpressionSine.h"',
            '#include "Materials/MaterialExpressionSaturate.h"',
            "constexpr float OpaqueGlassParallaxDepth = 0.035f",
            "constexpr float OpaqueGlassInteriorBandPeriod = 0.75f",
            "constexpr float OpaqueGlassInteriorBrightnessVariation = 0.075f",
            "constexpr float OpaqueGlassFresnelExponent = 4.0f",
            "constexpr float OpaqueGlassFresnelBaseReflectance = 0.12f",
            "constexpr float OpaqueGlassReflectionStrength = 0.86f",
            "constexpr float OpaqueGlassRoughnessVariation = 0.018f",
            "OpaqueGlassReflectionTint(0.20f, 0.34f, 0.48f, 1.0f)",
            "R30.Glass.CameraVectorWS",
            "R30.Glass.CameraWorldToTangent",
            "R30.Glass.ParallaxUV",
            "R30.Glass.InteriorStoreyBands",
            "R30.Glass.ViewAngleFresnel",
            "R30.Glass.BlendInteriorAndReflection",
            "R30.Glass.RoughnessWithOccupancy",
            "R30.Glass.ClampRoughness",
            "TransformSourceType = TRANSFORMSOURCE_World",
            "TransformType = TRANSFORM_Tangent",
            "GlassFresnel->Exponent = OpaqueGlassFresnelExponent",
            "GlassFresnel->BaseReflectFraction = OpaqueGlassFresnelBaseReflectance",
            "bExactOpaqueGlassTopology",
            "!bExactOpaqueGlassTopology",
            "opaqueGlassFresnel=true",
            "opaqueGlassTangentParallax=true",
            "opaqueGlassInteriorBandPeriodMetres=3",
            "opaqueGlassRoughnessVariation=0.018",
            "opaqueGlassTranslucency=false",
            "exactOpaqueGlassTopology=true",
        ):
            self.assertIn(token, self.factory)

        compact_factory = compact(self.factory)
        for mask_assignment in (
            "GlassCameraTangentXY->R=true",
            "GlassCameraTangentXY->G=true",
            "GlassCameraTangentXY->B=false",
            "GlassCameraTangentXY->A=false",
            "GlassParallaxV->R=false",
            "GlassParallaxV->G=true",
            "GlassParallaxV->B=false",
            "GlassParallaxV->A=false",
        ):
            self.assertIn(mask_assignment, compact_factory)
        for edge in (
            'Connect(GlassCameraVector,TEXT(""),GlassCameraToTangent,TEXT("")',
            'Connect(GlassCameraToTangent,TEXT(""),GlassCameraTangentXY,TEXT("")',
            'Connect(ScaledUv,TEXT(""),GlassParallaxUv,TEXT("A")',
            'Connect(GlassParallaxOffset,TEXT(""),GlassParallaxUv,TEXT("B")',
            'Connect(GlassParallaxV,TEXT(""),GlassInteriorBands,TEXT("")',
            'Connect(GlassInteriorTint,TEXT(""),GlassBase,TEXT("A")',
            'Connect(GlassReflectionTint,TEXT(""),GlassBase,TEXT("B")',
            'Connect(GlassFresnelSaturate,TEXT(""),GlassBase,TEXT("Alpha")',
            'Connect(GlassRoughness,TEXT(""),GlassRoughnessSaturate,TEXT("")',
            'Connect(GlassBase,TEXT(""),BaseSwitch,TEXT("False")',
            'Connect(GlassRoughnessSaturate,TEXT(""),RoughnessSwitch,TEXT("False")',
        ):
            self.assertIn(edge, compact_factory)
        self.assertIsNone(
            re.search(r'Connect\([^\r\n]*TEXT\("Input"\)', self.factory)
        )
        for forbidden in (
            "BLEND_Translucent",
            "MP_Opacity",
            "MP_OpacityMask",
            "MP_Refraction",
            "MP_PixelDepthOffset",
        ):
            self.assertNotIn(forbidden, self.factory)

    def test_factory_is_one_master_plus_eleven_instances_with_exact_texture_census(self) -> None:
        for token in (
            "static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 11)",
            "ExpectedAssetCount = UE_ARRAY_COUNT(MaterialSpecs) + 1",
            "ExpectedMasterExpressionCount = 52",
            "ExpectedTextureParameterCount = 3",
            "ExpectedScalarParameterCount = 9",
            "ExpectedVectorParameterCount = 1",
            'TEXT("Shutter"), TEXT("DarkTimber"), TEXT("Stone"), TEXT("Slate")',
            "OutTextures.Num() == UE_ARRAY_COUNT(TextureSets) * 3",
            "RootAssets.Num() != ExpectedAssetCount",
            "OutAssets.Num() != ExpectedAssetCount",
            "exactAssets=12 masters=1 materialInstances=11 facadeMaterialOverrides=11",
            "textureBackedOverrides=8 proceduralOpaqueGlassOverrides=3",
            "materialTextureParameterBindings=24 uniqueImmutableTextureDependencies=12",
            "retainedR29Triangles=256850 retainedR29Sections=11",
            "textureBackedTriangles=216828 textureBackedTrianglePercent=84.4",
        ):
            self.assertIn(token, self.factory)

    def test_factory_admits_only_context_textures_and_no_hero_material_dependency(self) -> None:
        for token in (
            'TEXT("/Game/TRIAD/IstanaPublicView/Textures")',
            "StartsWith(PublicViewTextureRoot + TEXT(\"/\"))",
            "Contains(TEXT(\"/HeroMaterials\"))",
            "Texture->Source.GetSizeX() != 2048",
            "Texture->Source.GetSizeY() != 2048",
            "Texture->AddressX != TA_Wrap",
            "Texture->AddressY != TA_Wrap",
            "TC_Normalmap",
            "TC_Masks",
            "TEXTUREGROUP_WorldNormalMap",
            "TEXTUREGROUP_World",
            "heroMaterialDependencies=0",
        ):
            self.assertIn(token, self.factory)
        self.assertNotIn("/Game/TRIAD/IstanaPublicView/HeroMaterialsV", self.factory)

    def test_factory_hash_guards_the_current_source_contract(self) -> None:
        payload = CONTRACT.read_bytes()
        digest = hashlib.sha256(payload).hexdigest().upper()
        guard = between(
            self.factory,
            "bool ValidateSourceContract(FString& OutError)",
            "bool ValidateTexture(",
        )
        self.assertIn(f"ExpectedBytes = {len(payload)}", guard)
        self.assertIn(digest, guard)
        for token in (
            "FFileHelper::LoadFileToArray",
            "Bytes.Num() != ExpectedBytes",
            "SHA256(",
            "BytesToHex(Digest, SHA256_DIGEST_LENGTH) != ExpectedSha256",
            "WITH_SSL SHA-256 support",
            "sourceContractBytes=",
            "sourceContractSha256=",
        ):
            self.assertIn(token, self.factory)

    def test_factory_requires_opaque_default_lit_nanite_sm6_without_displacement(self) -> None:
        for token in (
            "Material->MaterialDomain = MD_Surface",
            "Material->BlendMode = BLEND_Opaque",
            "Material->SetShadingModel(MSM_DefaultLit)",
            "Material->bUsedWithNanite = true",
            "Material->bTangentSpaceNormal = true",
            "EditorOnly->WorldPositionOffset.Expression",
            "EditorOnly->Displacement.Expression",
            "FeatureLevel != ERHIFeatureLevel::SM6",
            "GetMaterialResource(FeatureLevel)",
            "GetGameThreadShaderMap()",
            "compiledActiveFeatureLevelValidated=true",
        ):
            self.assertIn(token, self.factory)

    def test_editor_library_owns_all_five_required_endpoints(self) -> None:
        for endpoint in (
            "EnsureR30FacadeLookdevAssets",
            "ValidateR30FacadeLookdevAssets",
            "ApplyR30FacadeLookdevReplacementToLoadedV5DHybridMap",
            "CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap",
            "ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap",
        ):
            self.assertIn(endpoint, self.editor_h)
            self.assertIn(endpoint, self.editor)

    def test_editor_preserves_and_validates_context_shell_terrain_and_trees(self) -> None:
        shell = between(
            self.editor,
            "bool ValidateContextPolicyShell(",
            "bool UndoR30HandoffAndValidateR29(",
        )
        for token in (
            "ATRIADIstanaExploreV5DContextPolicyActor::StaticClass()",
            "ATRIADIstanaPublicViewSceneActor::StaticClass()",
            "ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor",
            "ATRIADIstanaExploreV5DTreeRealismActor",
            "PolicyCount != 1",
            "SceneCount != 1",
            "TerrainClassCount != 1",
            "TerrainTagCount != 1",
            "TreeClassCount != 1",
            "TreeTagCount != 1",
            "ValidateHybridContext",
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene",
            "ValidateCopernicusTerrainFallback",
            "ValidateTreeRealism",
            "r29TerrainActors=1 r29TerrainTags=1",
            "terrain64EdgeMaskValidated=true",
            "treeRealismActors=1 treeRealismTags=1",
            "treeSourceCensus=729 treeDerivativeMeshes=5",
            "terrainModified=false vegetationModified=false",
        ):
            self.assertIn(token, shell)
        for forbidden in (
            "DestroyActor(",
            "->Modify(",
            "SetActorTransform(",
            "SetStaticMesh(",
            "SetProviderReady(",
        ):
            self.assertNotIn(forbidden, shell)

    def test_apply_post_destroy_failures_close_and_undo_the_scoped_transaction(self) -> None:
        rollback = between(
            self.editor,
            "bool UndoR30HandoffAndValidateR29(",
            "} // namespace",
        )
        rollback_compact = compact(rollback)
        for token in (
            "Transaction->IsOutstanding()",
            "Transaction.Reset();",
            "GEditor->UndoTransaction(false)",
            "Restored.R28ClassCount == 0",
            "Restored.R28TagCount == 0",
            "Restored.R29ClassCount == 1",
            "Restored.R29TagCount == 1",
            "Restored.R29",
            "Restored.R30ClassCount == 0",
            "Restored.R30TagCount == 0",
            "!Restored.R30",
            "Restored.R29->ValidateR29FacadeEnvironment(R29Report)",
            "ValidateContextPolicyShell(",
            "ValidateComposableVegetationOwner(",
            'VegetationOwner == TEXT("R29")',
            "R30_IN_MEMORY_ROLLBACK_VALID",
            "undoTransaction=true r28Owners=0 r29Owners=1 r30Owners=0",
            "contextPolicyShellRestored=true",
            "vegetationOwner=R29 vegetationValidated=true",
        ):
            self.assertIn(token, rollback)
        self.assertLess(
            rollback.index("Transaction.Reset();"),
            rollback.index("GEditor->UndoTransaction(false)"),
        )
        self.assertIn(
            "ValidateContextPolicyShell(World,bExpectedProviderReady,ShellReport)",
            rollback_compact,
        )
        self.assertNotIn("Transaction->Cancel", rollback)

        vegetation = between(
            self.editor,
            "bool ValidateComposableVegetationOwner(",
            "bool HasExactFreshSaveRoster(",
        )
        exact_r29 = between(
            vegetation,
            "const bool bExactR29 =",
            "if (bExactR28)",
        )
        for token in (
            "Roster.R29VegetationClassCount == 1",
            "Roster.R29VegetationTagCount == 1",
            "Roster.R29Vegetation",
            "Roster.R28VegetationClassCount == 0",
            "Roster.R28VegetationTagCount == 0",
            "!Roster.R28Vegetation",
        ):
            self.assertIn(token, exact_r29)
        r29_validation = between(
            vegetation,
            "if (bExactR29)",
            'OutOwnerLabel = TEXT("INVALID")',
        )
        for token in (
            'OutOwnerLabel = TEXT("R29")',
            "Cast<ATRIADIstanaExploreV5DR29VegetationActor>",
            "TypedR29Vegetation->ValidateR29Vegetation(OutReport)",
        ):
            self.assertIn(token, r29_validation)

        apply = between(
            self.editor,
            "ApplyR30FacadeLookdevReplacementToLoadedV5DHybridMap(",
            "CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap(",
        )
        self.assertLess(
            apply.index("World->DestroyActor(Roster.R29)"),
            apply.index("Successor->ActivateAfterR29FacadeRemoval(Error)"),
        )
        post_destroy = between(
            apply,
            "if (!Successor->ActivateAfterR29FacadeRemoval(Error))",
            "Transaction.Reset();",
        )
        self.assertEqual(2, post_destroy.count("UndoR30HandoffAndValidateR29("))
        self.assertIn("APPLY_FAILED_ACTIVATE", post_destroy)
        self.assertIn("APPLY_FAILED_FINAL_VALIDATION", post_destroy)
        self.assertNotIn("Transaction->Cancel", post_destroy)

    def test_commit_requires_exact_r29_predecessor_and_r29_vegetation(self) -> None:
        commit_start = self.editor.index(
            "CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap("
        )
        final_validation_start = self.editor.rindex(
            "ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap("
        )
        commit = self.editor[commit_start:final_validation_start]
        for token in (
            "PredecessorRoster.R28ClassCount != 0",
            "PredecessorRoster.R28TagCount != 0",
            "PredecessorRoster.R28",
            "PredecessorRoster.R29ClassCount != 1",
            "PredecessorRoster.R29TagCount != 1",
            "!PredecessorRoster.R29",
            "PredecessorRoster.R30ClassCount != 0",
            "PredecessorRoster.R30TagCount != 0",
            "PredecessorRoster.R30",
            "PredecessorRoster.R29->ValidateR29FacadeEnvironment(",
            "ValidateContextPolicyShell(",
            "COMMIT_REFUSED_PREDECESSOR_WORLD",
        ):
            self.assertIn(token, commit)
        self.assertIn(
            'NormalizedExpectedVegetationOwner != TEXT("R29")', commit
        )
        self.assertNotIn(
            'NormalizedExpectedVegetationOwner != TEXT("R28")', commit
        )
        self.assertNotIn(
            'NormalizedExpectedVegetationOwner != TEXT("R30")', commit
        )

        vegetation = between(
            self.editor,
            "bool ValidateComposableVegetationOwner(",
            "bool HasExactFreshSaveRoster(",
        )
        for token in (
            "Roster.R28VegetationClassCount == 1",
            "Roster.R28VegetationTagCount == 1",
            "Roster.R29VegetationClassCount == 0",
            "Roster.R29VegetationTagCount == 0",
            "Roster.R29VegetationClassCount == 1",
            "Roster.R29VegetationTagCount == 1",
            "Roster.R28VegetationClassCount == 0",
            "Roster.R28VegetationTagCount == 0",
            'OutOwnerLabel = TEXT("R28")',
            'OutOwnerLabel = TEXT("R29")',
            "ValidateLandmarkVegetationR28",
            "ValidateR29Vegetation",
            "exact R28 xor R29",
        ):
            self.assertIn(token, vegetation)
        self.assertNotIn("R30Vegetation", vegetation)

    def test_wrapper_requires_exact_r29_predecessor_and_retains_airsim(self) -> None:
        wrapper = self.wrapper
        self.assertEqual(
            "Invoke-IstanaExploreV5DContextFacadeR30NativeTransactionV1.ps1",
            WRAPPER.name,
        )
        for endpoint in (
            "EnsureR30FacadeLookdevAssets",
            "ValidateR30FacadeLookdevAssets",
            "CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap",
            "ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap",
        ):
            self.assertIn(endpoint, wrapper)
        for token in (
            "[switch] $RequireR29FacadePredecessor",
            "[Alias('RequireR29Predecessor')]",
            "Live R30 facade lookdev execution requires -RequireR29FacadePredecessor",
            "Content\\TRIAD\\IstanaPublicViewExploreV5D\\SurroundingsRealismR29",
            "[ValidateSet('R29')]",
            "[string] $ExpectedVegetationOwner = 'R29'",
            "R29FacadeEnvironmentAssetsModified = $false",
            "R29FacadeMeshRetained = $true",
            "R29EnvironmentActiveRenderers = 0",
            "R29EnvironmentConcurrentRenderingAllowed = $false",
            "-d3d12",
            "-sm6",
        ):
            self.assertIn(token, wrapper)
        for forbidden in (
            "RequireR28FacadePredecessor",
            "-DisablePlugin=AirSim",
            "-DisablePlugins=AirSim",
            "-NoAirSim",
            "AirSimEnabled = $false",
            "AirSimRetained = $false",
        ):
            self.assertNotIn(forbidden, wrapper)

    def test_wrapper_source_pins_exactly_match_all_seven_current_sources(self) -> None:
        source_pin_block = between(
            self.wrapper,
            "$sourcePins = @(",
            "$sourceAssetPins = @(",
        )
        pin_pattern = re.compile(
            r"RelativePath\s*=\s*'(?P<path>[^']+)'\s*"
            r"Bytes\s*=\s*(?P<bytes>\d+)L\s*"
            r"Sha256\s*=\s*'(?P<sha256>[A-F0-9]{64})'\s*"
            r"NativeBeforePresent\s*=\s*\$false"
        )
        parsed_pins = tuple(
            (
                match.group("path"),
                int(match.group("bytes")),
                match.group("sha256"),
            )
            for match in pin_pattern.finditer(source_pin_block)
        )
        self.assertEqual(7, len(parsed_pins))
        self.assertEqual(
            tuple(relative_path for relative_path, _ in EXPECTED_SOURCE_PINS),
            tuple(relative_path for relative_path, _, _ in parsed_pins),
        )
        self.assertEqual(7, len({relative_path for relative_path, _, _ in parsed_pins}))

        for (expected_relative_path, source_path), (
            pinned_relative_path,
            pinned_bytes,
            pinned_sha256,
        ) in zip(EXPECTED_SOURCE_PINS, parsed_pins, strict=True):
            payload = source_path.read_bytes()
            self.assertEqual(expected_relative_path, pinned_relative_path)
            self.assertEqual(
                len(payload),
                pinned_bytes,
                f"stale byte pin for {expected_relative_path}",
            )
            self.assertEqual(
                hashlib.sha256(payload).hexdigest().upper(),
                pinned_sha256,
                f"stale SHA-256 pin for {expected_relative_path}",
            )

    def test_wrapper_promotes_exact_tree_material_response_v3_source_closure(self) -> None:
        replacements = (
            (
                r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTreeRealismActor.h",
                TREE_ACTOR_H,
                8739,
                "0C3E68F1ACFDAD3D298B98F9D583EAF72C70D0AFFF5BF7F15200E7F3CBC8DA3E",
            ),
            (
                r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTreeRealismActor.cpp",
                TREE_ACTOR_CPP,
                79575,
                "BC1B9A4D808681161245EB28981FDBA68C18AA6574D21556A08F196C5910C3A0",
            ),
            (
                r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTreeRealismEditorLibrary.h",
                TREE_EDITOR_H,
                1232,
                "CC4325D272A5871EABA0A2AD9028AF5CB039DF9160B8F01FC3363BA5D5EC2BAD",
            ),
            (
                r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp",
                TREE_EDITOR_CPP,
                25311,
                "A4C9A01D44293D67CD09C2D71D284E6E7BAED2EE4F6035E0B2048AD341ADA445",
            ),
        )
        source_pin_block = between(
            self.wrapper,
            "$sourcePins = @(",
            "$sourceAssetPins = @(",
        )
        for relative, path, before_bytes, before_sha in replacements:
            payload = path.read_bytes()
            current_sha = hashlib.sha256(payload).hexdigest().upper()
            pattern = re.compile(
                rf"RelativePath\s*=\s*'{re.escape(relative)}'\s*"
                rf"Bytes\s*=\s*{len(payload)}L\s*"
                rf"Sha256\s*=\s*'{current_sha}'\s*"
                rf"NativeBeforePresent\s*=\s*\$true\s*"
                rf"NativeBeforeBytes\s*=\s*{before_bytes}L\s*"
                rf"NativeBeforeSha256\s*=\s*'{before_sha}'"
            )
            self.assertRegex(source_pin_block, pattern)

        for marker in (
            "$treeResponseMaterialRelativePaths.Count -ne 13",
            "$treeDerivativeMeshRelativePaths.Count -ne 5",
            "EnsureTreeCanopyRealismMeshAssets",
            "EnsureTreeCanopyRealismMeshAsset",
            "TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID",
            "NativeSourcePins = @($sourcePins[7..10])",
            "SourceClosurePins = @($sourceAssetPins[1..6])",
            "ResponseMaterialPackageCount = 13",
            "ReboundManagedMeshPackageCount = 5",
            "RuntimeResponseMidCount = 26",
            "Restore-TreeJournal $treeRealismJournal",
        ):
            self.assertIn(marker, self.wrapper)

        labels_match = re.search(
            r"\$treeFormLabels\s*=\s*@\((?P<labels>[^)]*)\)",
            self.wrapper,
        )
        self.assertIsNotNone(labels_match)
        labels = tuple(re.findall(r"'([^']+)'", labels_match.group("labels")))
        self.assertEqual(
            (
                (0, "umbrella"),
                (1, "dome"),
                (2, "high_fork"),
                (3, "columnar"),
                (4, "palm"),
            ),
            tuple(enumerate(labels)),
        )

        execution = between(
            self.wrapper,
            "$stageResults.Add((Invoke-ColdStage `\n"
            "        -Stage '01_ensure_tree_material_response_v3_assets'",
            "$successQuiescence = Wait-NativeMutationQuiescence",
        )
        loop_header = (
            "for ($treeFormIndex = 0; "
            "$treeFormIndex -lt $treeFormLabels.Count; "
            "$treeFormIndex++) {"
        )
        self.assertEqual(2, execution.count(loop_header))
        self.assertEqual(
            2,
            execution.count(
                "-FunctionName 'EnsureTreeCanopyRealismMeshAsset'"
            ),
        )
        self.assertNotIn(
            "-FunctionName 'EnsureTreeCanopyRealismMeshAssets'",
            execution,
        )
        self.assertEqual(
            2,
            execution.count(
                "-Parameters @{ FormIndex = [int] $treeFormIndex }"
            ),
        )
        self.assertEqual(
            2,
            execution.count(
                "-ExpectedPrefix 'V5D_TREE_REALISM_MESH_ASSET_VALID'"
            ),
        )
        for marker in (
            '01b_ensure_tree_mesh_response_v3_$($treeFormLabels[$treeFormIndex])',
            '06b_cold_validate_tree_mesh_response_v3_$($treeFormLabels[$treeFormIndex])',
        ):
            self.assertEqual(1, execution.count(marker))
        self.assertEqual(10, len(labels) * execution.count(loop_header))

    def test_wrapper_pins_complete_tree_v3_contract_and_audit_evidence(self) -> None:
        source_asset_pin_block = between(
            self.wrapper,
            "$sourceAssetPins = @(",
            "$r30ContentRelativePaths = @(",
        )
        pin_pattern = re.compile(
            r"RelativePath\s*=\s*'(?P<path>[^']+)'\s*"
            r"Bytes\s*=\s*(?P<bytes>\d+)L\s*"
            r"Sha256\s*=\s*'(?P<sha256>[A-F0-9]{64})'\s*"
            r"NativeBeforePresent\s*=\s*\$false"
        )
        parsed_pins = tuple(
            (
                match.group("path"),
                int(match.group("bytes")),
                match.group("sha256"),
            )
            for match in pin_pattern.finditer(source_asset_pin_block)
        )
        self.assertEqual(7, len(parsed_pins))
        self.assertEqual(
            tuple(relative_path for relative_path, _ in EXPECTED_SOURCE_ASSET_PINS),
            tuple(relative_path for relative_path, _, _ in parsed_pins),
        )
        for (expected_relative_path, source_path), (
            pinned_relative_path,
            pinned_bytes,
            pinned_sha256,
        ) in zip(EXPECTED_SOURCE_ASSET_PINS, parsed_pins, strict=True):
            payload = source_path.read_bytes()
            self.assertEqual(expected_relative_path, pinned_relative_path)
            self.assertEqual(len(payload), pinned_bytes)
            self.assertEqual(
                hashlib.sha256(payload).hexdigest().upper(),
                pinned_sha256,
            )

        closure = json.loads(TREE_SOURCE_CLOSURE.read_text(encoding="utf-8"))
        self.assertEqual(
            "R30_COMMIT_AND_CAPTURE_THEN_R31_COMMIT_AND_CAPTURE_THEN_"
            "R32_COMMIT_AND_CAPTURE_THEN_R33_COMMIT_AND_CAPTURE",
            closure["nativeOrder"],
        )
        self.assertFalse(closure["newStageCreated"])
        self.assertFalse(closure["r34Created"])
        self.assertEqual(4, len(closure["nativeSourcePromotion"]))
        self.assertEqual(5, len(closure["sourceEvidence"]))
        self.assertEqual(13, closure["runtimeResponse"]["isolatedResponseMaterials"])
        self.assertEqual(5, closure["runtimeResponse"]["managedMeshDerivatives"])
        self.assertEqual(26, closure["runtimeResponse"]["runtimeResponseMids"])
        self.assertTrue(closure["captureGate"]["humanTreeReviewRequired"])
        self.assertFalse(closure["captureGate"]["automaticVisualAcceptanceAllowed"])
        self.assertTrue(closure["truthBoundary"]["appearanceOnly"])

        promoted = closure["nativeSourcePromotion"]
        for item, (relative_path, source_path) in zip(
            promoted, EXPECTED_TREE_PROMOTION_PINS, strict=True
        ):
            payload = source_path.read_bytes()
            self.assertEqual(relative_path, item["relativePath"])
            self.assertEqual(len(payload), item["bytes"])
            self.assertEqual(
                hashlib.sha256(payload).hexdigest().upper(),
                item["sha256"],
            )

        for item, (_, source_path) in zip(
            closure["sourceEvidence"],
            EXPECTED_SOURCE_ASSET_PINS[2:],
            strict=True,
        ):
            payload = source_path.read_bytes()
            self.assertEqual(
                source_path.relative_to(UNREAL).as_posix(),
                item["relativePath"],
            )
            self.assertEqual(len(payload), item["bytes"])
            self.assertEqual(
                hashlib.sha256(payload).hexdigest().upper(),
                item["sha256"],
            )

    def test_wrapper_has_fixed_launch_and_continuous_owned_process_memory_guards(self) -> None:
        wrapper = self.wrapper
        for token in (
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "$privateMemoryCeilingBytes = 12884901888L",
            "$minimumSystemFreeVirtualBytes = 6442450944L",
            "$memoryWatchdogPollMilliseconds = 500",
            "$memoryWatchdogPersistentBreachMilliseconds = 2000",
            "Assert-LaunchAdmission",
            "FreeVirtualMemory",
            "fixed 10 GiB FreeVirtualMemory gate",
            "ContinuousMemoryWatchdog",
            "Invoke-GuardedOwnedBuild",
            "Process.GetProcessById(this.processId)",
            "private static bool IsConfirmedExited(Process candidate)",
            "if (IsConfirmedExited(candidate))",
            "$expectedExitIdentityLeaseMilliseconds = 35000",
            "public void BeginExpectedExit(int identityLeaseMilliseconds)",
            "this.IsExpectedExitIdentityLeaseActive()",
            "allowExpectedExitIdentityLease",
            "out bool executablePathVerified",
            "this.exactIdentityVerifiedAtAlert =",
            "executablePathVerified;",
            "candidate.StartTime.ToUniversalTime().Ticks",
            "String.Equals(actualPath, this.executablePath",
            "process.PrivateMemorySize64",
            "memory.AvailablePageFile",
            "privateBytes >= this.privateCeilingBytes",
            "availableCommitBytes < this.minimumAvailableCommitBytes",
            '"MEMORY_GUARD_PRIVATE_BYTES"',
            '"MEMORY_GUARD_SYSTEM_FREE_VIRTUAL"',
            "this.TryGetExactProcess(",
            "this.exactIdentityVerifiedAtContainment =",
            "Assert-ContinuousMemoryWatchdogHealthy",
            "MEMORY_WATCHDOG_NO_SAMPLES",
            "MEMORY_WATCHDOG_STALE_SAMPLE",
            "[string] $ExpectedExecutablePath",
            "[string] $OwnedProcessLabel",
            "Stop-ContinuousMemoryWatchdog",
        ):
            self.assertIn(token, wrapper)
        cold_stage = between(
            wrapper,
            "function Invoke-ColdStage",
            "function Assert-StaticContract",
        )
        self.assertLess(
            cold_stage.index("Start-Process"),
            cold_stage.index("Start-ContinuousMemoryWatchdog"),
        )
        self.assertLess(
            cold_stage.index("Start-ContinuousMemoryWatchdog"),
            cold_stage.index("Assert-ContinuousMemoryWatchdogHealthy"),
        )
        self.assertIn("-ExpectedExecutablePath $editor", cold_stage)
        self.assertIn('-OwnedProcessLabel "UE5.5 helper $Stage"', cold_stage)
        self.assertIn("-WindowStyle Hidden", cold_stage)
        self.assertIsNone(re.search(r"(?mi)^\s*Stop-Process\b", wrapper))

        stop_helper = between(
            wrapper,
            "function Stop-OwnedHelper",
            "function Initialize-ContinuousMemoryWatchdogType",
        )
        self.assertLess(
            stop_helper.index("Get-OwnedHelperIdentity $Handle $Log"),
            stop_helper.index("$script:memoryWatchdog.BeginExpectedExit("),
        )

        watchdog_csharp = between(
            wrapper,
            "$source = @'",
            "'@\n    Add-Type",
        )
        exact_process_lookup = between(
            watchdog_csharp,
            "private bool TryGetExactProcess(",
            "private void SetMonitorError",
        )
        self.assertLess(
            exact_process_lookup.index(
                "if (actualTicks != this.creationUtcTicks)"
            ),
            exact_process_lookup.index("ProcessModule mainModule"),
        )
        self.assertIn(
            "if (allowExpectedExitIdentityLease &&",
            exact_process_lookup,
        )
        final_snapshot = between(
            wrapper,
            "function Assert-ContinuousMemoryWatchdogSnapshotComplete",
            "function Stop-ContinuousMemoryWatchdog",
        )
        self.assertIn("-not $Snapshot.Stopped", final_snapshot)
        self.assertIn("MEMORY_WATCHDOG_INCOMPLETE", final_snapshot)
        containment = between(
            watchdog_csharp,
            "private void ContainExactProcess()",
            "private void Run()",
        )
        for token in (
            "this.TryGetExactProcess(",
            "this.exactIdentityVerifiedAtContainment =",
            "executablePathVerified;",
            "process.Kill(true);",
            "this.forceKillUsed = true;",
        ):
            self.assertIn(token, containment)
        self.assertEqual(1, containment.count("process.Kill(true);"))
        self.assertNotIn("process.Kill();", containment)
        self.assertLess(
            containment.index("this.exactIdentityVerifiedAtContainment ="),
            containment.index("process.Kill(true);"),
        )

    def test_wrapper_uses_serial_non_uba_build_executor_without_weakening_guards(self) -> None:
        build_arguments = between(
            self.wrapper,
            "$buildArguments = @(",
            "$buildGuard = Invoke-GuardedOwnedBuild",
        )
        for token in (
            "'-MaxParallelActions=1'",
            "'-NoUBA'",
            "'-NoUBALocal'",
        ):
            self.assertIn(token, build_arguments)
        self.assertLess(
            build_arguments.index("'-MaxParallelActions=1'"),
            build_arguments.index("'-NoUBA'"),
        )
        self.assertLess(
            build_arguments.index("'-NoUBA'"),
            build_arguments.index("'-NoUBALocal'"),
        )
        for token in (
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "$privateMemoryCeilingBytes = 12884901888L",
            "$minimumSystemFreeVirtualBytes = 6442450944L",
        ):
            self.assertIn(token, self.wrapper)

    def test_wrapper_rejects_all_native_mutators_before_source_promotion(self) -> None:
        mutator_scan = between(
            self.wrapper,
            "function Get-NativeMutatorProcesses {",
            "function Register-OwnedNativeProcessHandle {",
        )
        for token in (
            "Get-CimInstance Win32_Process -ErrorAction Stop",
            "$isEngineEditor",
            "$engineRoot + '\\'",
            "$_.Name -like 'UnrealEditor*'",
            "$isExactProjectCommand",
            "$nativeProjectFile",
            "$nativeProjectRoot",
            "$isBuildTool",
            "'dotnet.exe'",
            "'UnrealBuildTool.exe'",
            "'UnrealHeaderTool.exe'",
            "'MSBuild.exe'",
            "'cl.exe'",
            "'link.exe'",
            "'rc.exe'",
            "'ShaderCompileWorker.exe'",
            "$isExactProjectUbt",
            "'UnrealBuildTool'",
            "$isEngineEditor -or $isExactProjectUbt -or",
            "($isExactProjectCommand -and $isBuildTool)",
        ):
            self.assertIn(token, mutator_scan)

        idle_gate = between(
            self.wrapper,
            "function Assert-NativeIdle {",
            "function Wait-NativeMutationQuiescence {",
        )
        for token in (
            "$busy = @(Get-NativeMutatorProcesses)",
            "if ($busy.Count -ne 0)",
            "UE5.5 editor/helper or exact-project build mutator is already present",
        ):
            self.assertIn(token, idle_gate)

        prewrite = between(
            self.wrapper,
            "$script:protectedBefore = @(Get-ProtectedProcesses)",
            "$buildArguments = @(",
        )
        source_promotion = "foreach ($pin in @($sourcePins) + @($sourceAssetPins))"
        self.assertIn("Assert-NativeIdle 'prewrite boundary'", prewrite)
        self.assertIn(source_promotion, prewrite)
        self.assertLess(
            prewrite.index("Assert-NativeIdle 'prewrite boundary'"),
            prewrite.index(source_promotion),
        )
        self.assertLess(
            prewrite.index("Assert-NativeIdle 'prewrite boundary'"),
            prewrite.index("Copy-Item -LiteralPath $source -Destination $destination"),
        )

    def test_wrapper_registers_every_launch_and_quiesces_success_and_rollback(self) -> None:
        build = between(
            self.wrapper,
            "function Invoke-GuardedOwnedBuild {",
            "function Invoke-ColdStage {",
        )
        editor = between(
            self.wrapper,
            "function Invoke-ColdStage {",
            "function Assert-StaticContract {",
        )
        launched_handles = re.findall(
            r"(?m)^\s*\$handle = Start-Process\b", self.wrapper
        )
        registered_handles = re.findall(
            r"(?m)^\s*Register-OwnedNativeProcessHandle\s*`", self.wrapper
        )
        self.assertEqual(2, len(launched_handles))
        self.assertEqual(2, len(registered_handles))
        for section, executable, label in (
            (build, "$dotnet", "'UnrealBuildTool dotnet host'"),
            (editor, "$editor", '"UE5.5 helper $Stage"'),
        ):
            self.assertEqual(1, len(re.findall(
                r"(?m)^\s*\$handle = Start-Process\b", section
            )))
            self.assertEqual(1, len(re.findall(
                r"(?m)^\s*Register-OwnedNativeProcessHandle\s*`", section
            )))
            self.assertIn("-Handle $handle", section)
            self.assertIn(f"-ExpectedExecutablePath {executable}", section)
            self.assertIn(f"-Label {label}", section)
            self.assertLess(
                section.index("$handle = Start-Process"),
                section.index("Register-OwnedNativeProcessHandle"),
            )
            self.assertLess(
                section.index("Register-OwnedNativeProcessHandle"),
                section.index("Start-ContinuousMemoryWatchdog"),
            )

        quiescence = between(
            self.wrapper,
            "function Wait-NativeMutationQuiescence {",
            "function Assert-LaunchAdmission {",
        )
        for token in (
            "$busy = @(Get-NativeMutatorProcesses)",
            "$activeOwned = @(Get-ActiveOwnedNativeProcessHandles)",
            "if ($busy.Count -eq 0 -and $activeOwned.Count -eq 0)",
            "Status = 'QUIESCENT'",
            "refused filesystem mutation because owned build/editor process trees are not quiescent",
        ):
            self.assertIn(token, quiescence)

        success = between(
            self.wrapper,
            "$stageResults.Add((Invoke-ColdStage '05_cold_validate_r30_facade_lookdev_replacement'",
            "catch {\n    $failure = $_.Exception",
        )
        for token in (
            "$successQuiescence = Wait-NativeMutationQuiescence",
            "before R30 success receipts and commit receipt",
            "NativeMutationQuiescence = $successQuiescence",
        ):
            self.assertIn(token, success)
        self.assertLess(
            success.index("$successQuiescence = Wait-NativeMutationQuiescence"),
            success.index("Assert-TreeReceipt $r29ContentRoot"),
        )
        self.assertLess(
            success.index("$successQuiescence = Wait-NativeMutationQuiescence"),
            success.index("$commit = [pscustomobject] [ordered]"),
        )

        rollback_start = self.wrapper.rindex(
            "finally {\n    if (-not $committed -and $transactionStarted)"
        )
        rollback = self.wrapper[rollback_start:]
        for token in (
            "$rollbackMayMutateFilesystem = $false",
            "Wait-NativeMutationQuiescence",
            "before R30 filesystem rollback restoration",
            "$rollbackMayMutateFilesystem = $true",
            "if ($rollbackMayMutateFilesystem)",
            "Filesystem rollback was intentionally not attempted",
            "exact journals were retained for guarded recovery",
        ):
            self.assertIn(token, rollback)
        wait_index = rollback.index("Wait-NativeMutationQuiescence")
        for mutation in (
            "Restore-FileJournal $mapJournal",
            "Remove-IsolatedR30Content",
            "Restore-TreeJournal $buildJournal",
            "Restore-TreeJournal $treeRealismJournal",
            "Restore-FileJournal $sourceAssetJournal",
            "Restore-FileJournal $sourceJournal",
        ):
            self.assertIn(mutation, rollback)
            self.assertLess(wait_index, rollback.index(mutation))
        guarded_restoration = between(
            rollback,
            "if ($rollbackMayMutateFilesystem) {",
            "else {",
        )
        for mutation in (
            "Restore-FileJournal $mapJournal",
            "Remove-IsolatedR30Content",
            "Restore-TreeJournal $buildJournal",
            "Restore-TreeJournal $treeRealismJournal",
            "Restore-FileJournal $sourceAssetJournal",
            "Restore-FileJournal $sourceJournal",
        ):
            self.assertIn(mutation, guarded_restoration)

    def test_final_and_cold_validator_require_exact_r29_vegetation(self) -> None:
        validation_start = self.editor.rindex(
            "ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap("
        )
        validation = self.editor[validation_start:]
        validation_compact = compact(validation)
        for token in (
            "ValidateComposableVegetationOwner(",
            'VegetationOwner != TEXT("R29")',
            "loaded-map vegetation owner is not exact R29",
            "vegetationOwnerMode=EXACT_R29",
        ):
            self.assertIn(token, validation)
        self.assertIn(
            'if(!ValidateComposableVegetationOwner(Roster,VegetationOwner,'
            'VegetationReport)||VegetationOwner!=TEXT("R29"))',
            validation_compact,
        )
        self.assertNotIn('VegetationOwner != TEXT("R28")', validation)
        self.assertNotIn('vegetationOwnerMode=EXACT_R28', validation)

        cold_stage = between(
            self.wrapper,
            "$stageResults.Add((Invoke-ColdStage '05_cold_validate_r30_facade_lookdev_replacement'",
            "Assert-TreeReceipt $r29ContentRoot",
        )
        self.assertIn(
            "'ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap'",
            cold_stage,
        )
        self.assertIn(
            "'ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_REPLACEMENT_MAP_VALID'",
            cold_stage,
        )
        self.assertIn("vegetationOwnerMode=EXACT_R29", self.wrapper)

    def test_wrapper_records_and_revalidates_all_immutable_context_receipts(self) -> None:
        wrapper = self.wrapper
        for token in (
            "function Get-TreeReceipt",
            "function Assert-TreeReceipt",
            "Bytes = [int64] $_.Length",
            "Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256",
            "R29FacadeEnvironment = @(Get-TreeReceipt $r29ContentRoot)",
            "R28Environment = @(Get-TreeReceipt $r28ContentRoot)",
            "VegetationR29 = @(Get-TreeReceipt $vegetationR29ContentRoot)",
            "LandmarkVegetationR28 = @(Get-TreeReceipt $landmarkVegetationR28ContentRoot)",
            "TreeRealism = @(Get-TreeReceipt $treeRealismContentRoot)",
            "TerrainR29 = @(Get-TreeReceipt $terrainR29ContentRoot)",
            "ContextFacadeR25 = @(Get-TreeReceipt $contextFacadeR25ContentRoot)",
            "ContextTextures = @(Get-TreeReceipt $contextTextureRoot)",
            "HeroMaterialsV2 = @(Get-TreeReceipt $heroV2ContentRoot)",
            "HeroMaterialsV3 = @(Get-TreeReceipt $heroV3ContentRoot)",
            "HeroMaterialsV4 = @(Get-TreeReceipt $heroV4ContentRoot)",
            "HeroMaterialsV5 = @(Get-TreeReceipt $heroV5ContentRoot)",
            "Assert-TreeReceipt $r29ContentRoot $immutableBefore.R29FacadeEnvironment",
            "Assert-TreeRealismV3ContentDelta",
            "-Before @($immutableBefore.TreeRealism)",
            "-After $treeRealismAfter",
            "TreeMaterialResponseV3 = $treeMaterialResponseV3Receipt",
            "TreeResponseMaterialPackageCount = 13",
            "TreeRuntimeResponseMidCount = 26",
            "Assert-TreeReceipt $terrainR29ContentRoot $immutableBefore.TerrainR29",
            "Assert-TreeReceipt $contextFacadeR25ContentRoot $immutableBefore.ContextFacadeR25",
            "TreeRealismValidated = $true",
            "TerrainR29Validated = $true",
            "ContextPolicyShellValidatedBeforeAndAfter = $true",
        ):
            self.assertIn(token, wrapper)


if __name__ == "__main__":
    unittest.main()
