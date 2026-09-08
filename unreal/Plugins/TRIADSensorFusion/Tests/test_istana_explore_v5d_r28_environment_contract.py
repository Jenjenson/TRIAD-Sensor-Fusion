from __future__ import annotations

import json
import importlib.util
import hashlib
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
SOURCE_ROOT = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R28EnvironmentalDressing"
)
GENERATED = SOURCE_ROOT / "Generated"
CONTRACT = SOURCE_ROOT / "r28_environmental_dressing.contract.json"
MANIFEST = GENERATED / "IstanaPublicViewV5DR28EnvironmentalDressing.manifest.json"
GENERATOR = SOURCE_ROOT / "build_r28_environmental_dressing.py"
ARCH_OBJ = GENERATED / "SM_IPV5D_R28_ContextArchitecturalDressing_Render.obj"
REALM_OBJ = GENERATED / "SM_IPV5D_R28_ConnectivePublicRealm_Render.obj"
TERRAIN_OBJ = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicView"
    / "Generated"
    / "SM_IstanaPublicView_Terrain.obj"
)
ACTOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DR28EnvironmentActor.h"
)
ACTOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DR28EnvironmentActor.cpp"
)
FACTORY_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h"
)
FACTORY_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DR28EnvironmentAssetFactory.cpp"
)
EDITOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp"
)


class IstanaExploreV5DR28EnvironmentContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (
            CONTRACT,
            MANIFEST,
            GENERATOR,
            ARCH_OBJ,
            REALM_OBJ,
            TERRAIN_OBJ,
            ACTOR_H,
            ACTOR_CPP,
            FACTORY_H,
            FACTORY_CPP,
            EDITOR_H,
            EDITOR_CPP,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R28 environment contract input: {path}")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor = ACTOR_CPP.read_text(encoding="utf-8")
        cls.factory_h = FACTORY_H.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")

    def test_generated_outputs_are_deterministic(self):
        result = subprocess.run(
            [sys.executable, "-B", str(GENERATOR), "--check"],
            cwd=REPO,
            text=True,
            capture_output=True,
            check=False,
            timeout=60,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("deterministic check: PASS", result.stdout)

    def test_python_runtime_guard_accepts_311_and_refuses_312_before_output(self):
        module_name = "triad_r28_python_runtime_contract_test_module"
        spec = importlib.util.spec_from_file_location(module_name, GENERATOR)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        try:
            spec.loader.exec_module(module)
            module.require_supported_python_runtime((3, 11, 0), "CPython")
            module.require_supported_python_runtime((3, 11, 99), "CPython")
            original_version_info = module.sys.version_info
            original_argv = module.sys.argv
            module.sys.version_info = (3, 12, 0, "final", 0)
            try:
                with tempfile.TemporaryDirectory(prefix="triad-r28-runtime-guard-") as root:
                    direct_output = Path(root) / "direct-build-must-not-be-created"
                    with self.assertRaisesRegex(
                        RuntimeError,
                        r"requires CPython 3\.11\.x.*detected CPython 3\.12\.0.*before output mutation",
                    ):
                        module.build(direct_output)
                    self.assertFalse(direct_output.exists())
                    cli_output = Path(root) / "cli-build-must-not-be-created"
                    module.sys.argv = [str(GENERATOR), "--output", str(cli_output)]
                    with self.assertRaisesRegex(
                        RuntimeError,
                        r"requires CPython 3\.11\.x.*detected CPython 3\.12\.0.*before output mutation",
                    ):
                        module.main()
                    self.assertFalse(cli_output.exists())
            finally:
                module.sys.version_info = original_version_info
                module.sys.argv = original_argv
        finally:
            sys.modules.pop(module_name, None)

    def test_focused_visual_census_and_source_pins_are_exact(self):
        architecture = self.manifest["architecturalDressing"]
        realm = self.manifest["connectivePublicRealm"]
        self.assertEqual(architecture["sourceFeatureCount"], 1305)
        self.assertEqual(architecture["sourcePartCount"], 1305)
        self.assertEqual(architecture["baselineSelectedPartCount"], 85)
        self.assertEqual(architecture["retainedBaselinePartCount"], 41)
        self.assertEqual(architecture["baselinePartNotSelectedCount"], 44)
        self.assertEqual(architecture["prioritySectorSelectedPartCount"], 40)
        self.assertEqual(architecture["prioritySectorCandidatePartCount"], 40)
        self.assertEqual(architecture["selectedPartCount"], 128)
        self.assertEqual(architecture["newlySelectedPartCount"], 87)
        self.assertEqual(architecture["backgroundSamplePercent"], 4)
        self.assertEqual(architecture["windowCount"], 14786)
        self.assertEqual(architecture["triangleCount"], 149758)
        self.assertEqual(architecture["sourceCornerCount"], 449274)
        self.assertEqual(architecture["frameToGlassPhysicalReliefMeters"], 0.105)
        self.assertEqual(architecture["parapetVisualHeightMeters"], 0.58)
        self.assertFalse(
            architecture["originalBuildingFootprintsHeightsAndRfShellModified"]
        )
        self.assertEqual(realm["sourceSegmentCount"], 1986)
        self.assertEqual(realm["acceptedSegmentCount"], 1923)
        self.assertEqual(realm["skippedInsideExistingPublicRealmCount"], 53)
        self.assertEqual(realm["skippedIntersectingExistingPublicRealmBoundaryCount"], 10)
        self.assertEqual(realm["triangleCount"], 32310)
        self.assertFalse(realm["existingZeroToThreeHundredMetrePublicRealmModified"])
        self.assertFalse(realm["verticalDatumIsFallbackPresentationAssumption"])
        self.assertTrue(realm["verticalAlignmentSampledFromExactPinnedExistingVisualTerrain"])
        self.assertFalse(realm["terrainSampleBoundaryClampChangesGeneratedXy"])
        self.assertEqual(
            self.contract["sourceBindings"], self.manifest["sourceBindings"]
        )

    def test_camera_radial_coverage_and_import_ceilings_are_exact(self):
        architecture = self.manifest["architecturalDressing"]
        ceilings = architecture["hardCeilings"]
        self.assertEqual(
            self.contract["architecturalDressing"]["hardCeilings"],
            {
                "maximumSelectedPartCount": 128,
                "maximumSourceCornerCount": 450000,
                "maximumTriangleCount": 150000,
                "maximumWindowCount": 15000,
            },
        )
        self.assertLessEqual(architecture["selectedPartCount"], 128)
        self.assertLessEqual(architecture["windowCount"], 15000)
        self.assertLessEqual(architecture["triangleCount"], 150000)
        self.assertLessEqual(architecture["sourceCornerCount"], 450000)
        self.assertEqual(ceilings["selectedPartHeadroom"], 0)
        self.assertEqual(ceilings["windowHeadroom"], 214)
        self.assertEqual(ceilings["triangleHeadroom"], 242)
        self.assertEqual(ceilings["sourceCornerHeadroom"], 726)

        evidence = architecture["evidenceCameraCoverageProxy"]
        self.assertEqual(evidence["eligibleUnionPartCount"], 516)
        self.assertEqual(evidence["baselineSelectedUnionPartCount"], 47)
        self.assertEqual(evidence["selectedUnionPartCount"], 86)
        self.assertEqual(
            set(evidence["cameras"]),
            {
                "R28_CLOSE_NORMAL_LAWN_TREE",
                "R28_ISTANA_WIDE",
                "R28_TEMASEK_MACDONALD_STREETSCAPE_CONTEXT",
                "R28_BROADER_SURROUNDINGS",
            },
        )
        central = evidence["cameras"]["R28_CLOSE_NORMAL_LAWN_TREE"]
        self.assertEqual(central["baselineSelectedPartCount"], 13)
        self.assertEqual(central["selectedPartCount"], 42)
        self.assertEqual(central["baselineProjectedFacadeProxyFraction"], 0.029220547)
        self.assertEqual(central["selectedProjectedFacadeProxyFraction"], 0.44379694)
        street = evidence["cameras"][
            "R28_TEMASEK_MACDONALD_STREETSCAPE_CONTEXT"
        ]
        self.assertEqual(street["baselineSelectedPartCount"], 34)
        self.assertEqual(street["selectedPartCount"], 44)
        self.assertEqual(street["baselineProjectedFacadeProxyFraction"], 0.877326528)
        self.assertEqual(street["selectedProjectedFacadeProxyFraction"], 0.923058336)

        priority = architecture["priorityStreetscapeCoverageProxy"]
        self.assertEqual(priority["eligibleUnionPartCount"], 547)
        self.assertEqual(priority["baselineSelectedUnionPartCount"], 60)
        self.assertEqual(priority["selectedUnionPartCount"], 69)
        self.assertEqual(
            set(priority["cameras"]),
            {
                "R24_MACDONALD_STREETSCAPE_CONTEXT",
                "R24_TEMASEK_STREETSCAPE_CONTEXT",
            },
        )
        macdonald = priority["cameras"]["R24_MACDONALD_STREETSCAPE_CONTEXT"]
        temasek = priority["cameras"]["R24_TEMASEK_STREETSCAPE_CONTEXT"]
        self.assertEqual(macdonald["selectedPartCount"], 65)
        self.assertEqual(macdonald["selectedProjectedFacadeProxyFraction"], 0.721584324)
        self.assertEqual(temasek["selectedPartCount"], 65)
        self.assertEqual(temasek["selectedProjectedFacadeProxyFraction"], 0.748637276)

        rings = architecture["omnidirectionalCoverage"]["radialBands"]
        self.assertEqual(len(rings), 3)
        self.assertEqual(
            [
                (
                    ring["baselineSelectedPartCount"],
                    ring["selectedPartCount"],
                    ring["baselineSectorsMeetingQuotaCount"],
                    ring["selectedSectorsMeetingQuotaCount"],
                    ring["nonemptySectorCount"],
                )
                for ring in rings
            ],
            [(5, 25, 0, 8, 8), (24, 39, 4, 8, 8), (56, 64, 5, 8, 8)],
        )
        self.assertTrue(
            all(
                sector["selectedPartCount"] >= sector["requiredSelectedPartCount"]
                for ring in rings
                for sector in ring["sectors"]
                if sector["eligiblePartCount"] > 0
            )
        )

    def test_exact_visual_terrain_pin_and_drape_audit_are_strong(self):
        terrain_binding = self.contract["sourceBindings"]["visualTerrainObj"]
        self.assertEqual(TERRAIN_OBJ.stat().st_size, 3909911)
        self.assertEqual(terrain_binding["bytes"], TERRAIN_OBJ.stat().st_size)
        self.assertEqual(
            hashlib.sha256(TERRAIN_OBJ.read_bytes()).hexdigest().upper(),
            "78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23",
        )
        self.assertEqual(
            terrain_binding["sha256"],
            "78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23",
        )
        realm = self.manifest["connectivePublicRealm"]
        self.assertEqual(realm["terrainSourceTriangleCount"], 10112)
        self.assertEqual(realm["terrainSourceCornerCount"], 30336)
        self.assertEqual(realm["terrainSourceOuterVertexCount"], 128)
        self.assertEqual(realm["terrainSampleRequestCount"], 64620)
        self.assertEqual(realm["resolvedTerrainSampleCount"], 64620)
        self.assertEqual(realm["unresolvedTerrainSampleCount"], 0)
        self.assertEqual(realm["boundaryClampedTerrainSampleCount"], 156)
        self.assertGreaterEqual(realm["directTerrainSampleFraction"], 0.995)
        self.assertLessEqual(realm["maximumBoundaryClampOverrunMeters"], 4.0)
        self.assertLessEqual(realm["maximumSharedEdgeHeightDisagreementMeters"], 1e-7)
        self.assertLessEqual(
            realm["maximumRepeatedXyTerrainHeightDisagreementMeters"], 1e-7
        )
        self.assertLessEqual(realm["maximumSurfaceSlopeDegrees"], 1.0)
        self.assertLessEqual(realm["maximumTerrainRelativeLiftErrorMeters"], 1e-9)
        self.assertEqual(realm["maximumHorizontalAlignmentErrorMeters"], 0.0)
        self.assertGreaterEqual(realm["minimumGeneratedVertexRadiusMeters"], 300.0)
        mismatch = realm["preDrapeFlatRoadHeightMismatchMeters"]
        self.assertEqual(mismatch["sampleCount"], 7692)
        self.assertGreaterEqual(mismatch["p95Absolute"], 1.0)
        self.assertGreater(mismatch["maximumAbsolute"], mismatch["p95Absolute"])

    def test_committed_public_realm_vertices_follow_pinned_terrain_and_lifts(self):
        module_name = "triad_r28_environmental_dressing_test_module"
        spec = importlib.util.spec_from_file_location(module_name, GENERATOR)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        try:
            spec.loader.exec_module(module)
            terrain = module.TerrainSurface.from_obj(
                TERRAIN_OBJ, self.contract["connectivePublicRealm"]
            )
            vertices = []
            vertex_materials = {}
            current_material = None
            for line in REALM_OBJ.read_text(encoding="utf-8").splitlines():
                if line.startswith("v "):
                    _, x, y, z = line.split()
                    vertices.append((float(x) / 100.0, -float(y) / 100.0, float(z) / 100.0))
                elif line.startswith("usemtl "):
                    current_material = line.removeprefix("usemtl ")
                elif line.startswith("f "):
                    self.assertIsNotNone(current_material)
                    for token in line.split()[1:]:
                        vertex_materials[int(token.split("/", 1)[0]) - 1] = current_material
            self.assertEqual(len(vertices), 96930)
            self.assertEqual(len(vertex_materials), len(vertices))
            expected_lifts = {
                "MI_IPV5D_R28_Asphalt": (0.06,),
                "MI_IPV5D_R28_Curb": (0.06, 0.18),
                "MI_IPV5D_R28_Sidewalk": (0.18,),
                "MI_IPV5D_R28_Verge": (0.10,),
            }
            maximum_error = 0.0
            minimum_radius = float("inf")
            minimum_z = float("inf")
            maximum_z = -float("inf")
            for index, (x, y, z) in enumerate(vertices):
                terrain_z, _ = terrain.sample(x, y)
                lift = z - terrain_z
                allowed = expected_lifts[vertex_materials[index]]
                maximum_error = max(maximum_error, min(abs(lift - item) for item in allowed))
                minimum_radius = min(minimum_radius, (x * x + y * y) ** 0.5)
                minimum_z = min(minimum_z, z)
                maximum_z = max(maximum_z, z)
            self.assertLessEqual(maximum_error, 2e-8)
            self.assertGreaterEqual(minimum_radius, 300.0)
            self.assertGreater(maximum_z - minimum_z, 2.5)
            self.assertEqual(terrain.unresolved_sample_count, 0)
        finally:
            sys.modules.pop(module_name, None)

    def test_negative_authority_and_provider_boundary_are_explicit(self):
        self.assertTrue(
            all(value is False for value in self.contract["negativeAuthority"].values())
        )
        boundary = self.contract["unrealIntegrationBoundary"]
        self.assertEqual(boundary["componentCount"], 3)
        self.assertTrue(boundary["providerReadyMirroredVisibilityRequired"])
        self.assertFalse(boundary["sceneCaptureVisibility"])
        for key in (
            "existingCesiumActorOrProviderPolicyMutation",
            "existingCurrentSurroundingsMeshOrRfShellMutation",
            "existingOuterGroundMeshMutation",
            "existingPublicRealmMeshMutation",
        ):
            self.assertFalse(boundary[key])

    def test_generated_meshes_have_only_sanitized_semantic_materials(self):
        architecture = ARCH_OBJ.read_text(encoding="utf-8")
        realm = REALM_OBJ.read_text(encoding="utf-8")
        self.assertEqual(architecture.count("\nf "), 149758)
        self.assertEqual(realm.count("\nf "), 32310)
        self.assertEqual(
            {
                line.removeprefix("usemtl ")
                for line in architecture.splitlines()
                if line.startswith("usemtl ")
            },
            {
                "MI_IPV5D_R28_GlassCool",
                "MI_IPV5D_R28_GlassWarm",
                "MI_IPV5D_R28_FrameLight",
                "MI_IPV5D_R28_FrameDark",
                "MI_IPV5D_R28_RoofTrim",
            },
        )
        self.assertEqual(
            {
                line.removeprefix("usemtl ")
                for line in realm.splitlines()
                if line.startswith("usemtl ")
            },
            {
                "MI_IPV5D_R28_Asphalt",
                "MI_IPV5D_R28_Curb",
                "MI_IPV5D_R28_Sidewalk",
                "MI_IPV5D_R28_Verge",
            },
        )
        combined = architecture + realm
        for forbidden in (
            "elementId",
            "partId",
            "sourceKey",
            "Newton Food Centre",
            "MacDonald House",
            "Temasek Shophouse",
            "highway=",
            "building=",
        ):
            self.assertNotIn(forbidden, combined)

    def test_runtime_actor_is_three_component_render_only_fallback(self):
        combined = self.actor_h + self.actor
        for required in (
            "ATRIADIstanaExploreV5DR28EnvironmentActor",
            "ConfigureR28Environment",
            "SetProviderReady",
            "ValidateR28Environment",
            "ConnectivePublicRealmRenderOnly",
            "ContextArchitecturalDressingRenderOnly",
            "OuterGroundColourReliefOverlayRenderOnly",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCanEverAffectNavigation(false)",
            "bHiddenInSceneCapture = true",
            "ComponentTags.Remove(HumanOnlyOverlayTag)",
            "Component->ComponentTags.Contains(HumanOnlyOverlayTag)",
            "const bool bVisible = !bProviderReady",
            "OuterGroundRenderOffsetCentimetres = 0.5f",
            "existingSimulationRfInputsModified=false",
        ):
            self.assertIn(required, combined)
        self.assertNotIn("ComponentTags.AddUnique(HumanOnlyOverlayTag)", self.actor)
        self.assertNotIn("SetCollisionEnabled(ECollisionEnabled::Query", self.actor)
        self.assertNotIn("CreateMeshSection", self.actor)
        self.assertNotIn("LineTrace", self.actor)

    def test_factory_isolated_assets_and_exact_topology_are_pinned(self):
        combined = self.factory_h + self.factory
        for required in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR28",
            "M_IPV5D_R28_Surface_Master",
            "CreateFreshAssets",
            "ValidateAssets",
            "LoadValidatedRuntimeContract",
            "Texture-free stable world-cell variation",
            "149758",
            "449274",
            "32310",
            "96930",
            "12484549",
            "59534571",
            "3909911",
            "78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23",
            "terrainDrapeSourcePinned=true",
            "terrainSamplesUnresolved=0",
            "architectureSourceCornerCeiling=450000",
            "priorityStreetscapeCameraCount=2",
            "omnidirectionalSectorsMeetingQuota=24",
            "bAutoGenerateCollision = false",
            "MarkAsNotHavingNavigationData",
            "NaniteSettings.bEnabled = true",
            "NaniteSettings.KeepPercentTriangles = 1.0f",
            "NaniteSettings.TrimRelativeError = 0.0f",
            "ENaniteFallbackTarget::PercentTriangles",
            "NaniteSettings.FallbackPercentTriangles = 1.0f",
            "NaniteSettings.FallbackRelativeError = 0.0f",
            "HasValidNaniteData()",
            "refuses an invalid or partially populated isolated asset root",
        ):
            self.assertIn(required, combined)
        self.assertEqual(self.factory.count("{TEXT(\"MI_IPV5D_R28_"), 10)
        self.assertNotIn("bImportMaterials = true", self.factory)
        self.assertNotIn("bImportTextures = true", self.factory)

    def test_material_graph_has_deterministic_multiscale_street_response(self):
        for required in (
            '#include "MaterialDomain.h"',
            'MicroStrengthParameter(TEXT("MicroStrength"))',
            'MicroScaleParameter(TEXT("MicroScaleCm"))',
            'JointStrengthParameter(TEXT("JointStrength"))',
            'JointSpacingParameter(TEXT("JointSpacingCm"))',
            'RoughnessVariationParameter(TEXT("RoughnessVariation"))',
            "const FString RoughnessCode(",
            "macroBlend=macroBlend*macroBlend*(3.0-2.0*macroBlend)",
            "microBlend=microBlend*microBlend*(3.0-2.0*microBlend)",
            "jointMask=1.0-smoothstep",
            "Data->Roughness.Connect(0, SurfaceRoughness)",
            "Data->ExpressionCollection.Expressions.Num() != 15",
            "Instance->ScalarParameterValues.Num() != 10",
            "Surface->Inputs.Num() != 10",
            "SurfaceRoughness->Inputs.Num() != 6",
            'TEXT("MI_IPV5D_R28_Asphalt"), FLinearColor(0.065f, 0.072f, 0.078f), 0.88f, 0.12f, 0.22f, 330.0f, 0.16f, 7.0f',
            'TEXT("MI_IPV5D_R28_Sidewalk"), FLinearColor(0.30f, 0.29f, 0.26f), 0.84f, 0.13f, 0.16f, 210.0f, 0.12f, 11.0f, 0.18f, 120.0f',
            "proceduralMicroSurface=true",
            "sidewalkJointCues=true",
            "roughnessVariation=true",
            "const auto* CustomExpression =",
            "const auto* ScalarParameter =",
            "const auto* FresnelExpression =",
        ):
            self.assertIn(required, self.factory)
        self.assertNotIn("const auto* Candidate =", self.factory)
        for forbidden in (
            "UMaterialExpressionTexture",
            "Data->Normal.Connect",
            "Data->WorldPositionOffset.Connect",
            "Data->PixelDepthOffset.Connect",
            "SetPhysMaterial",
        ):
            self.assertNotIn(forbidden, self.factory)

    def test_glazing_has_bounded_opaque_view_response_only_on_glass_instances(self):
        for required in (
            '#include "Materials/MaterialExpressionFresnel.h"',
            'GlazingGrazingStrengthParameter(TEXT("GlazingGrazingStrength"))',
            "float glazingCue=saturate(Fresnel*GlazingGrazingStrength)",
            "R28.BoundedOpaqueGlazingFresnel",
            "GlazingFresnel->Exponent = 5.0f",
            "GlazingFresnel->BaseReflectFraction = 0.04f",
            "GlazingFresnel->ExponentIn.Expression",
            "GlazingFresnel->BaseReflectFractionIn.Expression",
            "GlazingFresnel->Normal.Expression",
            "Surface->Inputs[8].Input.Expression != GlazingGrazingStrength",
            "Surface->Inputs[9].Input.Expression != GlazingFresnel",
            "GlazingGrazingStrengthParameter, Spec.GlazingGrazingStrength",
            "Spec.GlazingGrazingStrength > 0.35f",
            "!FMath::IsNearlyZero(Spec.GlazingGrazingStrength)",
            "viewDependentGlazingCue=true",
            "opaqueGlazing=true",
            "glazingTransparencyClaimed=false",
            "screenSpaceReflections=false",
            "Material->BlendMode = BLEND_Opaque",
            "Material->bScreenSpaceReflections = false",
        ):
            self.assertIn(required, self.factory)

        strengths = {}
        for line in self.factory.splitlines():
            if '{TEXT("MI_IPV5D_R28_' not in line:
                continue
            name_match = re.search(r'TEXT\("(MI_IPV5D_R28_[^"]+)"\)', line)
            strength_match = re.search(r", ([0-9.]+f)\},$", line.strip())
            self.assertIsNotNone(name_match, line)
            self.assertIsNotNone(strength_match, line)
            strengths[name_match.group(1)] = strength_match.group(1)
        self.assertEqual(
            strengths,
            {
                "MI_IPV5D_R28_GlassCool": "0.32f",
                "MI_IPV5D_R28_GlassWarm": "0.26f",
                "MI_IPV5D_R28_FrameLight": "0.0f",
                "MI_IPV5D_R28_FrameDark": "0.0f",
                "MI_IPV5D_R28_RoofTrim": "0.0f",
                "MI_IPV5D_R28_Asphalt": "0.0f",
                "MI_IPV5D_R28_Curb": "0.0f",
                "MI_IPV5D_R28_Sidewalk": "0.0f",
                "MI_IPV5D_R28_Verge": "0.0f",
                "MI_IPV5D_R28_OuterGround": "0.0f",
            },
        )
        for forbidden in (
            "BLEND_Translucent",
            "Data->Opacity.Connect",
            "Data->OpacityMask.Connect",
            "Data->Refraction.Connect",
        ):
            self.assertNotIn(forbidden, self.factory)

    def test_callable_editor_seam_owns_assets_but_not_map_save(self):
        combined = self.editor_h + self.editor
        for endpoint in (
            "EnsureR28EnvironmentAssets",
            "ValidateR28EnvironmentAssets",
            "ApplyR28EnvironmentToLoadedV5DHybridMap",
            "ValidateR28EnvironmentInLoadedV5DHybridMap",
        ):
            self.assertIn(endpoint, combined)
        apply_start = self.editor.index("ApplyR28EnvironmentToLoadedV5DHybridMap(")
        apply_end = self.editor.index(
            "ValidateR28EnvironmentInLoadedV5DHybridMap(", apply_start
        )
        apply_body = self.editor[apply_start:apply_end]
        self.assertIn("ClassCount != 0 || TagCount != 0", apply_body)
        self.assertIn("ConfigureR28Environment(Assets, false, Error)", apply_body)
        self.assertIn("mapSaved=false", apply_body)
        for forbidden in (
            "SaveMap",
            "SavePackage",
            "EditorLoadingAndSavingUtils",
            "FileHelpers",
        ):
            self.assertNotIn(forbidden, apply_body)


if __name__ == "__main__":
    unittest.main()
