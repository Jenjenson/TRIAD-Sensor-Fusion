import hashlib
import json
import os
import re
import unittest
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
SURROUNDINGS_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings"
)
CANDIDATE_ROOT = SURROUNDINGS_ROOT / "BuildingOpticsContactCompositionCandidate"
INTEGRATION_ROOT = SURROUNDINGS_ROOT / "BuildingOpticsContactCompositionIntegration"
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "building_optics_contact_composition_post_r33.source_contract.v1.json"
)
CANDIDATE_CONTRACT_PATH = (
    CANDIDATE_ROOT
    / "building_optics_contact_composition_candidate.contract.v1.json"
)
API_AUDIT_PATH = (
    INTEGRATION_ROOT
    / "building_optics_contact_composition_ue55_api_audit.source.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibraryTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary.cpp"
)


def io_path(path: Path) -> Path:
    """Use Win32's extended path prefix so junction-backed long paths stay testable."""
    if os.name == "nt" and not str(path).startswith("\\\\?\\"):
        return Path("\\\\?\\" + str(path.absolute()))
    return path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with io_path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_strict_json(path: Path):
    return json.loads(
        io_path(path).read_text(encoding="utf-8"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")
        ),
    )


def cpp_function_body(source: str, signature: str) -> str:
    """Return one C++ function body, failing closed on missing/unbalanced text."""
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"unbalanced C++ function: {signature}")


HISTORICAL_WRAPPER_RECEIPT_PINS = {
    "scripts/Invoke-IstanaExploreV5DContextFacadeR30NativeTransactionV1.ps1": (
        92139,
        "2445F9BBD96E39AC8C92C8AA3C279D9CDB88226EFABFC9AAF2F2351A767EC148",
    ),
    "scripts/Capture-IstanaExploreV5DR30Player0Evidence.ps1": (
        152520,
        "1C2427A5A15463D250A19E17466831B1862B13D40F864B706D60E74E689E5524",
    ),
}


class BuildingOpticsContactCompositionPostR33SourceContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls):
        cls.contract = load_strict_json(CONTRACT_PATH)
        cls.candidate = load_strict_json(CANDIDATE_CONTRACT_PATH)
        cls.api_audit = load_strict_json(API_AUDIT_PATH)
        cls.runtime_header = io_path(RUNTIME_HEADER).read_text(encoding="utf-8")
        cls.runtime_source = io_path(RUNTIME_SOURCE).read_text(encoding="utf-8")
        cls.runtime_test = io_path(RUNTIME_TEST).read_text(encoding="utf-8")
        cls.editor_header = io_path(EDITOR_HEADER).read_text(encoding="utf-8")
        cls.editor_source = io_path(EDITOR_SOURCE).read_text(encoding="utf-8")

    def test_source_only_candidate_explicitly_composes_in_order(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d."
            "building_optics_contact_composition_candidate.v1",
            self.candidate["schema"],
        )
        self.assertEqual(
            "POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE",
            self.candidate["status"],
        )
        self.assertTrue(self.candidate["sourceOnly"])
        self.assertEqual(
            [
                "EXACT_R31_BROAD_SHELL_PRESENTATION",
                "BUILDING_SURFACE_OPTICS",
                "BOUNDED_BUILDING_FOOT_CONTACT",
            ],
            self.candidate["compositionOrder"],
        )
        self.assertTrue(
            all(value is False for value in self.candidate["nativeClaims"].values())
        )
        self.assertTrue(
            all(
                value is False
                for value in self.candidate["authorityBoundary"].values()
            )
        )

    def test_candidate_reuses_every_hash_pinned_source_without_modifying_it(self):
        for row in self.candidate["sourcePins"].values():
            path = REPO_ROOT / row["file"]
            self.assertTrue(io_path(path).is_file(), path)
            self.assertEqual(row["bytes"], io_path(path).stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
        for row in self.contract["sourcePins"].values():
            path = REPO_ROOT / row["file"]
            self.assertTrue(io_path(path).is_file(), path)
            self.assertEqual(row["bytes"], io_path(path).stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
            self.assertIn(row["sha256"], self.editor_source)

    def test_contact_changes_only_three_post_optics_channels(self):
        channels = self.candidate["exactOpticsChannelPreservation"]
        self.assertEqual(
            [
                "CandidateBaseColor",
                "CandidateRoughness",
                "CandidateAmbientOcclusion",
            ],
            channels["contactAdjustedChannels"],
        )
        protected = [
            "CandidateTangentNormal",
            "CandidateMetallic",
            "CandidateClearCoat",
            "CandidateClearCoatRoughness",
        ]
        self.assertEqual(protected, channels["bitExactPassThroughChannels"])
        self.assertTrue(channels["allExistingOpticsPresentationChannelsRemainConnected"])
        self.assertTrue(channels["clearCoatShadingModelPreserved"])

        source = self.editor_source
        optics = source.index(
            "BuildingOpticsContactComposition: exact optics splice begins."
        )
        contact = source.index(
            "BuildingOpticsContactComposition: bounded foot-contact splice begins "
            "after every optics output."
        )
        final_return = source.index(
            "return float4(CandidateBaseColor, CandidateRoughness);",
            contact,
        )
        self.assertLess(optics, contact)
        self.assertLess(contact, final_return)
        contact_tail = source[contact:final_return]
        for token in (
            "float3 ExistingBaseColor = CandidateBaseColor;",
            "float ExistingRoughness = CandidateRoughness;",
            "float ExistingAmbientOcclusion = CandidateAmbientOcclusion;",
            "CandidateBaseColor = ContactBaseColor;",
            "CandidateRoughness = ContactRoughness;",
            "CandidateAmbientOcclusion = ContactAmbientOcclusion;",
        ):
            self.assertIn(token, contact_tail)
        for channel in protected:
            self.assertNotIn(channel + " =", contact_tail)

    def test_exact_r31_graph_inheritance_is_mutation_closed(self):
        boundary = self.contract["implementation"][
            "compositionValidationBoundary"
        ]
        self.assertEqual(
            boundary,
            self.api_audit["compositionValidationBoundary"],
        )
        self.assertTrue(all(boundary.values()))
        graph = cpp_function_body(
            self.editor_source,
            "bool ValidateExactR31GraphInheritance(",
        )
        validator = cpp_function_body(
            self.editor_source,
            "bool ValidateCandidateMaster(",
        )
        self.assertIn("ValidateExactR31GraphInheritance(", validator)
        for token in (
            "BuildExpressionMap(",
            "CandidateMaterial, true, CandidateNodes",
            "ExactR31Master, false, ExactNodes",
            "CandidateNodes.Num() != ExpectedMasterExpressionCount",
            "ExactNodes.Num() != ExpectedMasterExpressionCount",
            "CandidateNodes.FindRef(ExactR31SurfaceDesc) != CandidateSurface",
            "ExactNodes.FindRef(ExactR31SurfaceDesc) != ExactR31Surface",
            "PersistentExpressionPayloadMatchesExcept(",
            "Candidate->GetClass() != Exact->GetClass()",
            "ExpressionOutputsMatch(Candidate, Exact)",
            "ExpressionInputEdgeMatches(",
            "Index < ExpectedR31SurfaceInputCount",
            "ValidateExactUnchangedR31RootSelectors(",
            "CandidateReachable.Num() != CandidateNodes.Num()",
            "ExactReachable.Num() != ExactNodes.Num()",
            "CandidateReachable.Contains(Pair.Value)",
            "ExactReachable.Contains(ExactNodes.FindRef(Pair.Key))",
            "orphan expression outside the exact root-reachable node set",
        ):
            self.assertIn(token, graph)

        allowed_match = re.search(
            r"SurfaceAllowedPayloadDifferences\s*=\s*\{(.*?)\};",
            graph,
            re.DOTALL,
        )
        self.assertIsNotNone(allowed_match)
        allowed = set(
            re.findall(r'FName\(TEXT\("([^"]+)"\)\)', allowed_match.group(1))
        )
        self.assertEqual(
            {
                "Desc",
                "Description",
                "Code",
                "OutputType",
                "Inputs",
                "AdditionalOutputs",
                "Outputs",
                "bShowOutputNameOnPin",
            },
            allowed,
        )
        self.assertNotIn("Specular", allowed)

        root = cpp_function_body(
            self.editor_source,
            "bool ValidateExactUnchangedR31RootSelectors(",
        )
        for changed_root in (
            "BaseColor",
            "Normal",
            "Metallic",
            "AmbientOcclusion",
            "ClearCoat",
            "ClearCoatRoughness",
        ):
            self.assertRegex(
                root,
                rf"RootValueInputStateMatchesExceptEdge\(\s*"
                rf"Candidate->{changed_root},\s*Exact->{changed_root}\)",
            )
        for unchanged_root in (
            "Specular",
            "Roughness",
            "Anisotropy",
            "Tangent",
            "EmissiveColor",
            "Opacity",
            "OpacityMask",
            "WorldPositionOffset",
            "Displacement",
            "SubsurfaceColor",
            "Refraction",
            "PixelDepthOffset",
            "ShadingModelFromMaterialExpression",
            "SurfaceThickness",
            "FrontMaterial",
        ):
            self.assertRegex(
                root,
                rf"RootValueInputMatches\(\s*Candidate->{unchanged_root},\s*"
                rf"Exact->{unchanged_root},\s*CandidateNodes,\s*ExactNodes\)",
            )
        self.assertIn("MaterialAttributesInputMatches(", root)
        self.assertIn("Candidate->CustomizedUVs[Index]", root)
        self.assertIn("MP_Specular edge", root)

        exact_root_edges = {
            "BaseColor": 0,
            "Normal": 1,
            "Metallic": 3,
            "AmbientOcclusion": 4,
            "ClearCoat": 5,
            "ClearCoatRoughness": 6,
        }
        for field, output_index in exact_root_edges.items():
            self.assertIn(
                f"!InputMatches(EditorOnly->{field}, Surface, {output_index})",
                validator,
            )
        self.assertIn(
            "!InputMatches(EditorOnly->Roughness, RoughnessOutput, 0)",
            validator,
        )

        apply_graph = cpp_function_body(
            self.editor_source,
            "bool ApplyCandidateGraphToFreshClone(",
        )
        for token in (
            "TEXT(\"R31.SurfaceRoughnessA\")",
            "!InputMatches(EditorOnly->Roughness, RoughnessOutput, 0)",
            "Surface->OutputType = CMOT_Float4;",
        ):
            self.assertIn(token, apply_graph)
        compose_code = cpp_function_body(
            self.editor_source,
            "bool ComposeCandidateSurfaceCode(",
        )
        self.assertIn(
            "return float4(CandidateBaseColor, CandidateRoughness);",
            compose_code,
        )
        self.assertNotIn("MP_Roughness", apply_graph)
        self.assertNotIn(
            "Surface->OutputType = CMOT_Float3;",
            self.editor_source,
        )

    def test_added_surface_selectors_and_outputs_are_mutation_closed(self):
        boundary = self.api_audit["compositionValidationBoundary"]
        self.assertTrue(boundary["exactComposedExtraInputNamesAndSelectorsCompared"])
        self.assertTrue(boundary["exactSevenSurfaceOutputNamesAndMasksCompared"])
        invariant = self.candidate["customSurfaceInvariant"]
        self.assertEqual(23, invariant["exactR31InheritedInputCount"])
        self.assertEqual(26, invariant["composedInputCount"])
        self.assertEqual(
            [
                "R31TangentNormalInput",
                "R31MetallicInput",
                "R31AmbientOcclusionInput",
            ],
            invariant["extraInputNames"],
        )
        self.assertEqual("None", invariant["extraInputExpressionInputName"])
        self.assertTrue(invariant["extraInputMasksMatchSelectedSourceOutputs"])
        self.assertTrue(invariant["allOutputMasksZero"])
        self.assertTrue(invariant["showOutputNameOnPin"])
        output_names_match = re.search(
            r"CandidateSurfaceOutputNames\[\]\s*=\s*\{(.*?)\};",
            self.editor_source,
            re.DOTALL,
        )
        self.assertIsNotNone(output_names_match)
        self.assertEqual(
            (
                "return",
                "CandidateTangentNormal",
                "CandidateRoughness",
                "CandidateMetallic",
                "CandidateAmbientOcclusion",
                "CandidateClearCoat",
                "CandidateClearCoatRoughness",
            ),
            tuple(
                re.findall(
                    r'TEXT\("([^"]+)"\)', output_names_match.group(1)
                )
            ),
        )
        self.assertEqual(
            list(
                re.findall(
                    r'TEXT\("([^"]+)"\)', output_names_match.group(1)
                )
            ),
            invariant["outputNames"],
        )

        selector = cpp_function_body(
            self.editor_source,
            "bool InputExactlySelectsExpressionOutput(",
        )
        for token in (
            "Input.Expression != ExpectedExpression",
            "Input.OutputIndex != ExpectedOutputIndex",
            "Input.InputName != ExpectedInputName",
            "Input.Mask == Output.Mask",
            "Input.MaskR == Output.MaskR",
            "Input.MaskG == Output.MaskG",
            "Input.MaskB == Output.MaskB",
            "Input.MaskA == Output.MaskA",
        ):
            self.assertIn(token, selector)

        outputs = cpp_function_body(
            self.editor_source,
            "bool HasExactCandidateSurfaceOutputs(",
        )
        for token in (
            "Surface->bShowOutputNameOnPin",
            "Surface->Outputs.Num() != ExpectedCandidateSurfaceOutputCount",
            "Output.OutputName != CandidateSurfaceOutputNames[Index]",
            "Output.Mask != 0",
            "Output.MaskR != 0",
            "Output.MaskG != 0",
            "Output.MaskB != 0",
            "Output.MaskA != 0",
        ):
            self.assertIn(token, outputs)

        validator = cpp_function_body(
            self.editor_source,
            "bool ValidateCandidateMaster(",
        )
        graph = cpp_function_body(
            self.editor_source,
            "bool ValidateExactR31GraphInheritance(",
        )
        self.assertIn("!HasExactCandidateSurfaceOutputs(Surface)", validator)
        self.assertIn(
            "!HasExactCandidateSurfaceOutputs(CandidateSurface)", graph
        )
        exact_extras = (
            (23, "R31TangentNormalInput", "Normal"),
            (24, "R31MetallicInput", "Metallic"),
            (25, "R31AmbientOcclusionInput", "AmbientOcclusion"),
        )
        for index, input_name, expression in exact_extras:
            self.assertIn(
                f'Surface->Inputs[{index}].InputName != TEXT("{input_name}")',
                validator,
            )
            self.assertRegex(
                validator,
                rf"!InputExactlySelectsExpressionOutput\(\s*"
                rf"Surface->Inputs\[{index}\]\.Input,\s*{expression},\s*"
                rf"0,\s*NAME_None\)",
            )

        apply_graph = cpp_function_body(
            self.editor_source,
            "bool ApplyCandidateGraphToFreshClone(",
        )
        self.assertIn("Input.Input.InputName = NAME_None;", apply_graph)
        self.assertIn(
            "Surface->Outputs.Reset(ExpectedCandidateSurfaceOutputCount)",
            apply_graph,
        )
        admission = cpp_function_body(
            self.editor_source,
            "bool ValidateCompositionCandidateContract(",
        )
        for token in (
            'TEXT("customSurfaceInvariant")',
            'TEXT("exactR31InheritedInputCount")',
            'TEXT("composedInputCount")',
            'TEXT("extraInputNames")',
            'TEXT("extraInputExpressionInputName")',
            'TEXT("extraInputMasksMatchSelectedSourceOutputs")',
            'TEXT("outputNames")',
            'TEXT("allOutputMasksZero")',
            'TEXT("showOutputNameOnPin")',
        ):
            self.assertIn(token, admission)

    def test_all_instance_override_families_are_three_way_exact(self):
        helper = cpp_function_body(
            self.editor_source,
            "bool HasExactInstanceOverrideState(",
        )
        override_families = (
            "ScalarParameterValues",
            "VectorParameterValues",
            "DoubleVectorParameterValues",
            "TextureParameterValues",
            "TextureCollectionParameterValues",
            "RuntimeVirtualTextureParameterValues",
            "SparseVolumeTextureParameterValues",
            "FontParameterValues",
            "UserSceneTextureOverrides",
            "GetStaticParameters()",
            "BasePropertyOverrides",
        )
        self.assertEqual(11, len(override_families))
        self.assertEqual(
            11,
            self.api_audit["verifiedSemantics"][
                "materialInstancePresentationOverrideFamilyCount"
            ],
        )
        boundary = self.api_audit["compositionValidationBoundary"]
        self.assertTrue(boundary["allElevenPresentationOverrideFamiliesCompared"])
        self.assertNotIn("allOverrideFamiliesCompared", boundary)
        for family in override_families:
            self.assertIn(f"Left->{family}", helper)
            self.assertIn(f"Right->{family}", helper)

        candidate = cpp_function_body(
            self.editor_source,
            "bool ValidateCandidateInstance(",
        )
        self.assertRegex(
            candidate,
            r"!HasExactInstanceOverrideState\(Instance, ExactOpticsFallback\)",
        )
        self.assertRegex(
            candidate,
            r"!HasExactInstanceOverrideState\(\s*"
            r"ExactOpticsFallback, ExactR31Fallback\)",
        )

        fallback = cpp_function_body(
            self.editor_source,
            "bool ValidateExactOpticsFallbackAssets(",
        )
        for token in (
            "LoadExact<UMaterialInstanceConstant>(R31FallbackPaths[Index])",
            "!HasExactInstanceOverrideState(Material, ExactR31Fallback)",
            "!SameReferencedTextures(Material, ExactR31Fallback)",
            "Material->Parent->GetPathName() != OpticsMasterObjectPath",
            "ExactR31Fallback->Parent->GetPathName()",
        ):
            self.assertIn(token, fallback)

    def test_physical_material_mask_authority_is_explicitly_denied(self):
        self.assertTrue(
            self.api_audit["compositionValidationBoundary"][
                "physicalMaterialAndMaskAuthorityDeniedAcrossCandidateAndFallbacks"
            ]
        )
        authority = self.candidate["authorityBoundary"]
        self.assertFalse(authority["physicalMaterial"])
        self.assertFalse(authority["physicalMaterialMask"])
        self.assertTrue(
            self.api_audit["verifiedSemantics"][
                "materialInstanceConstantPhysicalMaterialMaskIsSeparateOverride"
            ]
        )

        helper = cpp_function_body(
            self.editor_source,
            "bool HasNoInstancePhysicalAuthority(",
        )
        for token in (
            "Instance->PhysMaterial",
            "Instance->PhysMaterialMask",
            "Instance->PhysicalMaterialMap",
        ):
            self.assertIn(token, helper)

        fallback = cpp_function_body(
            self.editor_source,
            "bool ValidateExactOpticsFallbackAssets(",
        )
        self.assertIn("!HasNoInstancePhysicalAuthority(Material)", fallback)
        self.assertIn(
            "!HasNoInstancePhysicalAuthority(ExactR31Fallback)", fallback
        )
        candidate = cpp_function_body(
            self.editor_source,
            "bool ValidateCandidateInstance(",
        )
        self.assertIn("!HasNoInstancePhysicalAuthority(Instance)", candidate)
        admission = cpp_function_body(
            self.editor_source,
            "bool ValidateCompositionCandidateContract(",
        )
        self.assertIn('TEXT("physicalMaterialMask")', admission)

        runtime_boundary = self.api_audit["runtimeTrustBoundary"]
        self.assertTrue(
            runtime_boundary["physicalMaterialMaskDeniedDirectAndEffective"]
        )
        self.assertTrue(
            self.api_audit["verifiedSemantics"][
                "materialInstanceConstantPhysicalMaterialMaskGetterReturnsDirectOverride"
            ]
        )
        self.assertIn(
            "The development automation physical-mask probe was not compiled or executed.",
            self.api_audit["limitations"],
        )
        direct_runtime = cpp_function_body(
            self.runtime_source,
            "bool HasNoDirectPhysicalAuthority(",
        )
        for token in (
            "Cast<UMaterialInstanceConstant>(Material)",
            "ConstantInstance->PhysMaterialMask",
            "Instance->PhysMaterial",
            "Instance->PhysicalMaterialMap",
        ):
            self.assertIn(token, direct_runtime)
        effective_runtime = cpp_function_body(
            self.runtime_source,
            "bool HasNoPhysicalAuthority(",
        )
        for token in (
            "HasNoDirectPhysicalAuthority(Material)",
            "!Material->GetPhysicalMaterial()",
            "!Material->GetPhysicalMaterialMask()",
        ):
            self.assertIn(token, effective_runtime)
        runtime_roster = cpp_function_body(
            self.runtime_source,
            "bool ValidateExactClearCoatRoster(",
        )
        self.assertIn("!HasNoPhysicalAuthority(Instance)", runtime_roster)

        for token in (
            '#include "PhysicalMaterials/PhysicalMaterialMask.h"',
            "NewObject<UMaterialInstanceConstant>(",
            "NewObject<UPhysicalMaterialMask>(",
            "PhysicalMaskProbe->PhysMaterialMask = AdversarialMask;",
            "PhysicalMaskProbe->GetPhysicalMaterialMask() ==",
            "TRIADBuildingOpticsContactCompositionTestHasNoDirectPhysicalAuthority(",
            "TRIADBuildingOpticsContactCompositionTestHasNoPhysicalAuthority(",
            'TEXT("Direct runtime physical-mask mutation is denied")',
            'TEXT("Effective runtime physical-mask mutation is denied")',
        ):
            self.assertIn(token, self.runtime_test)

    def test_foot_response_is_exactly_bounded_and_source_plane_only(self):
        response = self.candidate["boundedFootContactResponse"]
        self.assertEqual("AFTER_OPTICS_OUTPUTS", response["applicationOrder"])
        self.assertEqual(0.1, response["fullWeightThroughSourceZMetres"])
        self.assertEqual(1.25, response["zeroWeightAtAndAboveSourceZMetres"])
        self.assertEqual(0.08, response["maximumBaseColorDarkening"])
        self.assertEqual(0.05, response["maximumRoughnessDelta"])
        self.assertEqual(0.07, response["maximumAmbientOcclusionDarkening"])
        self.assertTrue(response["existingGlassMaskExcludesGlazing"])
        self.assertTrue(response["wallVerticalWeatherMaskRequired"])
        self.assertFalse(response["syntheticSourcePlaneIsMeasuredGrade"])
        for token in (
            "void TRIADBuildingSurfaceOpticsCandidate(",
            "void TRIADBuildingFootContactResponse(",
            "const float SourcePlaneEnvelope = 1.0 - smoothstep(",
            "TEXT(\"0.10,\")",
            "TEXT(\"1.25,\")",
            "ContactBaseColor = ExistingBaseColor * lerp(1.0, 0.92",
            "ContactRoughness = saturate(ExistingRoughness + 0.05",
            "ExistingAmbientOcclusion * (1.0 - 0.07",
            "float SourceAbsoluteZMetres = sourceAbsoluteZMetres;",
            "float2 AbsoluteWorldPositionMetresXY = WorldPositionCm.xy * 0.01;",
        ):
            self.assertIn(token, self.editor_source)

    def test_geometry_fallback_and_authority_boundaries_are_exact(self):
        geometry = self.contract["geometryInvariant"]
        self.assertEqual(43448, geometry["sourceTriangles"])
        self.assertEqual(17, geometry["materialSlots"])
        self.assertEqual(1388, geometry["retainedGroups"])
        self.assertEqual("IDENTITY", geometry["componentTransform"])
        self.assertTrue(
            all(
                value is False
                for key, value in geometry.items()
                if key.endswith("Mutation") or key.endswith("Modified")
            )
        )
        fallback = self.contract["fallbackBoundary"]
        self.assertTrue(fallback["exactOpticsRosterMandatory"])
        self.assertTrue(fallback["exactR31RosterMandatory"])
        self.assertTrue(fallback["opticsPreferredFallback"])
        self.assertTrue(fallback["r31UltimateFallback"])
        self.assertTrue(fallback["pointerSelectionOnly"])
        self.assertFalse(fallback["mapOrComponentBinding"])
        self.assertTrue(
            all(value is False for value in self.contract["preservationBoundary"].values())
        )

    def test_runtime_is_pointer_only_and_candidate_gates_are_independently_off(self):
        source = self.runtime_header + self.runtime_source + self.runtime_test
        for token in (
            "bRuntimeCandidateSelectionCompiledAuthorized = false",
            "bRuntimeCandidateActivationCompiledAuthorized = false",
            "ValidateExactOpticsFallbackRoster",
            "ValidateExactR31FallbackRoster",
            "ResolveOptionalPresentationMaterials",
            "The exact seventeen-entry BuildingSurfaceOptics roster is a mandatory preferred fallback",
            "The exact seventeen-entry R31 roster is a mandatory ultimate fallback",
        ):
            self.assertIn(token, source)
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/"
            "BuildingOpticsContactComposition/Materials/MI_IPV5D_BOC_",
            source,
        )
        self.assertNotIn("SetMaterial(", source)
        self.assertNotIn("SetStaticMesh(", source)
        runtime = self.contract["implementation"]["runtimeTrustBoundary"]
        self.assertTrue(runtime["pointerSelectionOnly"])
        self.assertTrue(runtime["exactOpticsFallbackRosterRequired"])
        self.assertTrue(runtime["exactR31FallbackRosterRequired"])
        self.assertFalse(runtime["candidateSelectionCompiledAuthorized"])
        self.assertFalse(runtime["candidateActivationCompiledAuthorized"])

    def test_public_editor_is_inspection_only_and_private_materializer_has_no_call_site(self):
        public_text, private_text = self.editor_header.split("private:", 1)
        symbol = (
            "MaterializeTrustedBuildingOpticsContactCompositionAssetsInternal"
        )
        self.assertIn("InspectBuildingOpticsContactCompositionReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(symbol, private_text)
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        source_files = list((PLUGIN_ROOT / "Source").rglob("*.h")) + list(
            (PLUGIN_ROOT / "Source").rglob("*.cpp")
        )
        hits = {
            path.relative_to(REPO_ROOT).as_posix(): path.read_text(
                encoding="utf-8"
            ).count(symbol)
            for path in source_files
            if symbol in path.read_text(encoding="utf-8")
        }
        self.assertEqual(
            {
                EDITOR_HEADER.relative_to(REPO_ROOT).as_posix(): 1,
                EDITOR_SOURCE.relative_to(REPO_ROOT).as_posix(): 1,
            },
            hits,
        )
        inspection = self.editor_source.index(
            "InspectBuildingOpticsContactCompositionReceipts("
        )
        materializer = self.editor_source.index(symbol + "(")
        self.assertIn(
            "ExpectedFutureTransactionAuthorizationSha256,\n            false,",
            self.editor_source[inspection:materializer],
        )
        self.assertIn(
            "ExpectedFutureTransactionAuthorizationSha256,\n            true,",
            self.editor_source[materializer:],
        )
        for token in (
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "!IsSha256(TrustedAcceptedR33ReceiptSha256)",
            "!IsSha256(TrustedFutureAuthorizationSha256)",
            "bRequireCompiledTrustAnchors",
            "callerSuppliedHashesNeverAuthorizeExecution=true",
            "privateMaterializerInvoked=false",
        ):
            self.assertIn(token, self.editor_source)

    def test_execution_authorization_is_distinct_exact_and_narrow(self):
        source = self.editor_source
        for token in (
            "ValidateAcceptedR33Receipt",
            "ValidateFutureAuthorization",
            "triad.istana_explore_v5d.r33_player0_capture.v1",
            "MATERIALIZE_BUILDING_OPTICS_CONTACT_COMPOSITION_ASSETS_ONLY",
            "CompositionContractSha256",
            "OpticsCandidateContractSha256",
            "OpticsMaterialIntentSha256",
            "FootContactCandidateContractSha256",
            "FootContactMaterialIntentSha256",
            "ExactOpticsAndR31FallbacksPreserved",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "FPaths::IsSamePath(FullR33, FullAuthorization)",
        ):
            self.assertIn(token, source)
        trust = self.contract["implementation"]["executionTrustBoundary"]
        accepted = trust["acceptedR33CompiledTrustAnchor"]
        future = trust["futureAuthorizationCompiledTrustAnchor"]
        self.assertNotEqual(accepted, future)
        self.assertIsNone(re.fullmatch(r"[0-9A-F]{64}", accepted))
        self.assertIsNone(re.fullmatch(r"[0-9A-F]{64}", future))
        self.assertFalse(trust["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(trust["callerSuppliedHashesAloneSufficient"])
        self.assertFalse(trust["nativeWriteReachable"])

    def test_namespace_and_rollback_are_recursive_fresh_only_and_fail_closed(self):
        for token in (
            "CollectNamespaceState",
            "GetAssetsByPath",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "TryConvertLongPackageNameToFilename",
            "TryConvertFilenameToLongPackageName",
            "FindFilesRecursive",
            "PhysicalFiles",
            "PhysicalDirectories",
            "uncontracted physical file",
            "uncontracted physical directory",
            "DeleteLoadedAssets",
            "DeleteDirectory",
            "DirectoryExists(*After.PhysicalRoot)",
            "ROLLBACK_REFUSED",
            "ROLLBACK_FAILED",
            "ROLLBACK_COMPLETE",
            "EXISTING_NAMESPACE_REUSE_DENIED",
            "PARTIAL_NAMESPACE_DENIED",
        ):
            self.assertIn(token, self.editor_source)
        self.assertGreaterEqual(self.editor_source.count("FindFilesRecursive"), 2)
        self.assertTrue(all(self.contract["namespaceAndRollback"].values()))

    def test_no_map_component_actor_or_numbered_stage_mutation_surface(self):
        watched = (
            self.runtime_header,
            self.runtime_source,
            self.runtime_test,
            self.editor_header,
            self.editor_source,
            io_path(API_AUDIT_PATH).read_text(encoding="utf-8"),
        )
        forbidden_calls = (
            "SpawnActor",
            "LoadMap(",
            "OpenLevel(",
            "GetEditorWorld(",
            "SetActorLocation",
            "SetActorRotation",
            "SetActorTransform",
            "SetWorldTransform",
            "SetRelativeTransform",
            "SetStaticMesh(",
            "SetMaterial(",
            "SetCollisionEnabled",
            "SetCanEverAffectNavigation",
            "AddInstance(",
            "RemoveInstance(",
            "DestroyActor",
        )
        forbidden_stage = "R" + str(34)
        for text in watched:
            self.assertNotIn(forbidden_stage, text)
            for call in forbidden_calls:
                self.assertNotIn(call, text, call)
        for row in self.contract["preservedExternalPins"]["numberedWrappers"]:
            path = REPO_ROOT / row["file"]
            self.assertTrue(io_path(path).is_file(), path)
            historical = HISTORICAL_WRAPPER_RECEIPT_PINS.get(row["file"])
            if historical is not None:
                self.assertEqual(historical, (row["bytes"], row["sha256"]), path)
                continue
            self.assertEqual(row["bytes"], io_path(path).stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_api_audit_is_static_and_exact_when_ue55_is_present(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.api_audit["status"],
        )
        self.assertFalse(self.api_audit["launchOrBuildPerformed"])
        self.assertTrue(
            all(value is False for value in self.api_audit["nativeClaims"].values())
        )
        self.assertEqual(
            self.contract["implementation"]["runtimeTrustBoundary"],
            self.api_audit["runtimeTrustBoundary"],
        )
        engine_source = Path("C:/Program Files/Epic Games/UE_5.5/Engine/Source")
        if not engine_source.is_dir():
            self.skipTest("Pinned UE 5.5 install is not present on this host")
        for row in self.api_audit["headersAndSources"]:
            path = engine_source / row["pathBelowEngineSource"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_artifact_roster_and_remaining_native_proof_are_exact(self):
        expected = {
            path.relative_to(REPO_ROOT).as_posix()
            for path in (
                CANDIDATE_CONTRACT_PATH,
                RUNTIME_HEADER,
                RUNTIME_SOURCE,
                RUNTIME_TEST,
                EDITOR_HEADER,
                EDITOR_SOURCE,
                API_AUDIT_PATH,
                README_PATH,
                Path(__file__).absolute(),
            )
        }
        rows = self.contract["implementation"]["artifacts"]
        self.assertEqual(expected, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertEqual(row["bytes"], io_path(path).stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
        required = {
            "UE55_UBT_UHT_COMPILE_AND_LINK",
            "REVIEWED_COMPILED_TRUST_ANCHOR_ACTIVATION",
            "MATERIAL_SHADER_COMPILE_WITH_EXACT_OPTICS_THEN_CONTACT_ORDER",
            "MATERIALIZE_SAVE_UNLOAD_COLD_RELOAD_EXACT_18_ASSETS",
            "EXACT_R31_43448_TRIANGLES_17_SLOTS_1388_GROUPS_IDENTITY_TRANSFORM_REVALIDATION",
            "EXACT_OPTICS_SEVEN_CHANNEL_AND_CONTACT_THREE_CHANNEL_NATIVE_GRAPH_PROOF",
            "VISIBLE_FOOT_SOURCE_PLANE_ALIGNMENT_REVIEW",
            "OPTIONAL_APPLICATION_INSIDE_SEPARATELY_GUARDED_MAP_TRANSACTION",
            "NANITE_AND_RASTER_MATCHED_CAMERA_PARITY",
            "MATCHED_NEAR_MID_AND_950_TO_1000_METRE_CAPTURES",
            "TEMPORAL_STABILITY_UNDER_CAMERA_MOTION",
            "RUNTIME_CANDIDATE_AND_OPTICS_PHYSICAL_MATERIAL_MASK_MUTATION_REJECTION",
            "COLLISION_NAVIGATION_LOS_RF_SENSOR_GEOGRAPHY_TERRAIN_REGRESSION",
            "TARGET_HARDWARE_MEMORY_FRAME_TIME_AND_DRAW_CALL_ACCEPTANCE",
            "EXPLICIT_HUMAN_VISUAL_ACCEPTANCE",
        }
        self.assertEqual(required, set(self.contract["remainingNativeProof"]))

    def test_readme_is_honest_and_cpp_scaffolding_is_balanced(self):
        readme = io_path(README_PATH).read_text(encoding="utf-8")
        for token in (
            "dormant, fail-closed post-R33 source scaffold",
            "all BuildingSurfaceOptics outputs",
            "Base Color, Roughness and Ambient Occlusion",
            "Tangent Normal, Metallic, Clear Coat and Clear Coat Roughness pass through",
            "exact BuildingSurfaceOptics roster remains the preferred fallback",
            "exact R31 roster remains the ultimate fallback",
            "three added Surface inputs",
            "exactly seven ordered names",
            "eleven presentation override families",
            "UMaterialInstanceConstant::PhysMaterialMask",
            "effective runtime checks to reject it",
            "private, non-reflected materializer has no call site",
            "Existing or partial output is denied",
            "does not bind a map, actor, mesh or component",
            "Passing source tests do not establish",
        ):
            self.assertIn(token, readme)
        for path, source in (
            (RUNTIME_SOURCE, self.runtime_source),
            (RUNTIME_TEST, self.runtime_test),
            (EDITOR_SOURCE, self.editor_source),
        ):
            self.assertEqual(source.count("{"), source.count("}"), path)
            self.assertEqual(source.count("#if"), source.count("#endif"), path)
            self.assertNotRegex(source, r"\bTODO\b|\bFIXME\b")


if __name__ == "__main__":
    unittest.main(verbosity=2)
