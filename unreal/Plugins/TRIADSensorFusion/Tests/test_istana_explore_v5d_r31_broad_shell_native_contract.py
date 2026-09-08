from pathlib import Path
import json
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
FACTORY_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DR31BroadShellAssetFactory.cpp"
)
EDITOR_HEADER = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DR31BroadShellEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DR31BroadShellEditorLibrary.cpp"
)
R30_CAPTURE_SOURCE = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DR30Player0CaptureLibrary.cpp"
)
WRAPPER = REPO / "scripts/Invoke-IstanaExploreV5DBroadShellR31NativeTransactionV1.ps1"
CONTRACT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev/r31_broad_shell_lookdev.contract.json"
)


class IstanaExploreV5DR31BroadShellNativeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.factory = FACTORY_SOURCE.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor = EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.capture = R30_CAPTURE_SOURCE.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))

    def test_strict_r25_endpoint_is_retained_and_generic_dispatcher_is_bounded(self):
        for fragment in (
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene",
            "ValidateContextFacadeR25Overrides",
            "ValidateCurrentSurroundingsBroadShellR31ForInheritedScene",
            "ValidateBroadShellR31Overrides",
            "ValidateAdmittedCurrentShellMaterialState",
            "EAdmittedCurrentShellMaterialState::LegacyEmbedded",
            "EAdmittedCurrentShellMaterialState::ContextFacadeR25",
            "EAdmittedCurrentShellMaterialState::BroadShellR31",
            "bVersionedOverridesUseExactV2Mesh",
            "CurrentSurroundingsV2MeshObjectPath",
        ):
            self.assertIn(fragment, self.runtime_header + self.runtime_source)
        strict_start = self.runtime_source.index(
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene("
        )
        strict_end = self.runtime_source.index(
            "ValidateCurrentSurroundingsBroadShellR31ForInheritedScene(", strict_start
        )
        strict_body = self.runtime_source[strict_start:strict_end]
        self.assertIn("ValidateContextFacadeR25Overrides", strict_body)
        self.assertIn("CurrentSurroundingsV2MeshObjectPath", strict_body)
        self.assertNotIn("ValidateBroadShellR31Overrides", strict_body)

    def test_r30_capture_resolution_remains_r25_only_and_r31_free(self):
        start = self.capture.index("bool ResolveValidatedCaptureState(")
        end = self.capture.index("FString CaptureFilename(", start)
        body = self.capture[start:end]
        self.assertIn(
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene", body
        )
        self.assertNotIn("BroadShellR31", body)
        self.assertNotIn("ValidateAdmittedCurrentShellMaterialState", body)

    def test_exact_17_slot_r31_override_roster_is_in_place_only(self):
        expected_roles = [
            "R31FallbackRoofMaterialPath",
            "R31FallbackWallMaterialPath",
            "R31FallbackWallMaterialPath",
            "R31FallbackWallMaterialPath",
            "R31OfficialWallMaterialPath",
            "R31FallbackWallMaterialPath",
            "R31OfficialWallMaterialPath",
            "R31FallbackWallMaterialPath",
            "R31FallbackRoofMaterialPath",
            "R31FallbackRoofMaterialPath",
            "R31FallbackRoofMaterialPath",
            "R31OfficialRoofMaterialPath",
            "R31FallbackRoofMaterialPath",
            "R31OfficialRoofMaterialPath",
            "R31FallbackRoofMaterialPath",
            "R31FallbackRoofMaterialPath",
            "R31FallbackWallMaterialPath",
        ]
        match = re.search(
            r"BroadShellR31Overrides\[\]\s*=\s*\{(?P<body>.*?)\};",
            self.runtime_source,
            re.DOTALL,
        )
        self.assertIsNotNone(match)
        roles = re.findall(r"&([A-Za-z0-9_]+)\}", match.group("body"))
        self.assertEqual(roles, expected_roles)
        apply_start = self.runtime_source.index(
            "ApplyCurrentSurroundingsBroadShellR31(FString& OutError)"
        )
        apply_end = self.runtime_source.index(
            "SuppressInheritedPlanningGroundPresentation", apply_start
        )
        body = self.runtime_source[apply_start:apply_end]
        self.assertIn("CurrentSurroundingsV2MeshObjectPath", body)
        self.assertIn("ValidateContextFacadeR25Overrides", body)
        self.assertIn("ValidateBroadShellR31Overrides", body)
        self.assertLess(body.index("LoadObject<UMaterialInterface>"), body.index("Component->Modify()"))
        for forbidden in (
            "ConfigureCurrentSurroundingsAndOuterGroundPresentation",
            "SetStaticMesh(",
            "SetCollision",
            "SetCanEverAffectNavigation",
            "SpawnActor",
        ):
            self.assertNotIn(forbidden, body)

    def test_master_graph_is_exactly_validated_and_keeps_full_depth_readability(self):
        for fragment in (
            "ExpectedMasterExpressionCount = 42",
            "ExpectedScalarParameterCount = 22",
            "ExpectedVectorParameterCount = 3",
            "ExpectedTextureParameterCount = 3",
            "Expression->Desc = Description",
            "Surface->Inputs.Num() != 23",
            "ExpectedInputExpressions",
            "InputMatchesExactOutput",
            "InputIsExactlyDisconnected",
            "OutputsMatchExplicitR31Contract",
            'OutputMatches(5, TEXT("RGBA"), 1, 1, 1, 1, 1)',
            'OutputMatches(2, TEXT("Z"), 1, 0, 0, 1, 0)',
            "ExpectedNodeDescriptions",
            "ExpectedNodeClasses",
            "Actual->GetClass() != Pair.Value",
            "Expression->GetOuter() != Material",
            "Expression->Function || Expression->SubgraphExpression",
            "ExpressionCollection.EditorComments.IsEmpty()",
            "ExpressionCollection.ExpressionExecBegin",
            "ExpressionCollection.ExpressionExecEnd",
            "Material->bEnableExecWire",
            "const bool bQuerySucceeded = Registry.Get().GetAssetsByPath",
            "ValidatePinnedRepositoryFile",
            "SourcePins->Num() != 5",
            "TexturePins->Num() != 9",
            "Normalized.RightChop(7)",
            "ValidateSingleTextureSourceProvenance",
            "FMD5Hash::HashFile",
            "Texture->Source.GetIdString().IsEmpty()",
            "Texture->Source.GetNumLayers() != 1",
            "Texture->Source.GetNumMips() != 1",
            "Texture->Source.GetFormat() != TSF_BGRA8",
            "Texture->Filter != TF_Default",
            "Texture->VirtualTextureStreaming || Texture->NeverStream",
            "Texture->CompressionNoAlpha",
            "texturePayloadBinding=source-provenance-admitted-existing-payload",
            "sourceIdClaimedAsPayloadDigest=false",
            "embeddedPixelByteEqualityClaim=false",
            "SAMPLERTYPE_Color",
            "SAMPLERTYPE_Normal",
            "SAMPLERTYPE_Masks",
            "SamplerSource != SSM_FromTextureAsset",
            "MipValueMode != TMVM_None",
            "ConstCoordinate != 0",
            "ConstMipValue != INDEX_NONE",
            "CoordinateIndex != 0",
            "ExactUv->UnMirrorU || ExactUv->UnMirrorV",
            "TA_Wrap",
            'Materials/MaterialExpressionVertexTangentWS.h',
            "SeamSafeMetricUv->Inputs.Num() != 4",
            "float2 wallTangentRaw = VertexTangentWS.xy",
            "float tangentQuantizationLevels = 4096.0",
            "float tangentQuantizationPhase = 89.0 / 1048576.0",
            "float wallUMetres = signedWorldAxisMetres / max(quantizedDominantMagnitude, 0.0001)",
            "sourceProbeProvesWithinRenderPathContinuity",
            "sourceProbeProvesNaniteRasterAppearanceParity",
            "nativeNaniteRasterHighOccupancyPairRequired",
            "nativeReviewMustRejectNaniteRasterCadenceFamilyTextureOrNormalPop",
            "sourceWithinRenderPathContinuityProven=true",
            "naniteRasterAppearanceParityProven=false",
            "nativeNaniteRasterPairRequired=true",
            "float sourceAbsoluteZMetres = 1.0 - UV0.y",
            "float facadePlaneDistanceMetres =",
            "float facadeSignalA = 0.5 + 0.5 * sin",
            "float4 familyWeights =",
            "familyWeights /= max",
            "float familyBayScale =",
            "float familyStoreyScale =",
            "float familyPhaseU =",
            "float familyPhaseV =",
            "float3 familyPalette =",
            "float familySpandrelStrength =",
            "float familyPierStrength =",
            "float familySlabStrength =",
            "float twoStoreyWave = 0.5+0.5*cos",
            "float architecturalRange = wallMask * fade * microReadability",
            "float visibleSpandrelRhythm = architecturalRange",
            "float visiblePierRhythm = architecturalRange",
            "float visibleSlabRhythm = architecturalRange",
            "smoothstep(0.035-aa.x,0.105+aa.x,facadeEdgeU)",
            "smoothstep(0.018-aa.y,0.072+aa.y,facadeEdgeV)",
            "lerp(float3(1.0,1.0,1.0),familyPalette,wallMask)",
            "float runoff = wallMask",
            "float roofMottle = (1.0-wallMask)",
            "float mullion =",
            "float transom =",
            "float revealMask =",
            "float occupancyThreshold =",
            "float occupied =",
            "float cellOccupancy =",
            "float cellTone =",
            "float blindMask =",
            "float2 interiorShift = 0.115",
            "float2 interiorPhase =",
            "float glassFresnel =",
            "float atmosphere = saturate",
            "InputMatchesExactOutput(EditorOnly->BaseColor, Surface, 0)",
            "EditorOnly->WorldPositionOffset.Expression",
            "EditorOnly->Displacement.Expression",
            "EditorOnly->BaseColor.UseConstant",
            "EditorOnly->MaterialAttributes.PropertyConnectedMask",
            "ExactBaseBlend->ConstAlpha",
            "ExactNormalBlend->ConstAlpha",
            "ExactAoBlend->ConstAlpha",
            "UE_ARRAY_COUNT(EditorOnly->CustomizedUVs) != 8",
            "StaticParameters.StaticSwitchParameters.IsEmpty()",
            "EMaterialParameterAssociation::GlobalParameter",
            "Value.ParameterInfo.Index != INDEX_NONE",
            "ShaderMap->IsCompilationFinalized()",
            "ShaderMap->CompiledSuccessfully()",
            "exactOsmGroupIdVisibleToPixelShader",
            "wallOnlyFamilyColour=true",
            "architecturalSpandrelPierTwoStoreySlabEdge=true",
            "antialiasedArchitecturalEdges=true",
            "hardFloorParitySelector=false",
            "exactPerBuildingStyleClaimed=false",
            "balconyGeometryClaimed=false",
            "midFarOnly=true",
        ):
            self.assertIn(fragment, self.factory)
        for forbidden in (
            "Expression->Description = Description",
            "float2 buildingCell =",
            "float verticalDirt = wallMask",
            "float plinthMask = wallMask",
            "facadePlaneDistanceKey = round",
            "step(0.25, facade",
            "step(0.50, facade",
            "step(0.75, facade",
            "floor(q.y)",
        ):
            self.assertNotIn(forbidden, self.factory)

    def test_native_contract_parser_binds_every_instance_spec_field(self):
        field_to_member = {
            "tint": "Tint",
            "apertureTint": "ApertureTint",
            "atmosphereTint": "AtmosphereTint",
            "metresPerTile": "MetresPerTile",
            "textureInfluence": "TextureInfluence",
            "normalStrength": "NormalStrength",
            "surfaceRoughness": "SurfaceRoughness",
            "roughnessTextureWeight": "RoughnessTextureWeight",
            "metallic": "Metallic",
            "specular": "Specular",
            "aoTextureWeight": "AoTextureWeight",
            "variationCellMeters": "VariationCellMeters",
            "weatheringStrength": "WeatheringStrength",
            "wallVerticalWeatherMask": "WallVerticalWeatherMask",
            "bayMeters": "BayMeters",
            "storeyMeters": "StoreyMeters",
            "apertureWidthFraction": "ApertureWidthFraction",
            "apertureHeightFraction": "ApertureHeightFraction",
            "apertureSillFraction": "ApertureSillFraction",
            "apertureHintStrength": "ApertureHintStrength",
            "apertureFadeStartCm": "ApertureFadeStartCm",
            "apertureFadeEndCm": "ApertureFadeEndCm",
            "atmosphereStartCm": "AtmosphereStartCm",
            "atmosphereEndCm": "AtmosphereEndCm",
            "atmosphereStrength": "AtmosphereStrength",
        }
        for field, member in field_to_member.items():
            self.assertRegex(
                self.factory,
                rf'TEXT\("{field}"\)[\s\S]{{0,80}}Spec\.{member}',
                field,
            )

    def test_world_validation_proves_exact_owners_v2_uniqueness_and_provider_state(self):
        for fragment in (
            "ExpectedCurrentSurroundingsV2MeshObjectPath()",
            "ExactV2ComponentCount != 1",
            "R28FacadeClassCount != 0",
            "R28FacadeTagCount != 0",
            "R29FacadeClassCount != 0",
            "R29FacadeTagCount != 0",
            "VegetationClassCount != 1",
            "VegetationTagCount != 1",
            "LandmarkVegetationClassCount != 0",
            "LandmarkVegetationTagCount != 0",
            "TerrainClassCount != 1",
            "TerrainTagCount != 1",
            "TreeClassCount != 1",
            "TreeTagCount != 1",
            "OutRoster.R30->bProviderReady !=",
            "bLocalBuildingFallbackCurrentlyHidden",
            "ValidateR29Vegetation",
            "ValidateCopernicusTerrainFallback",
            "ValidateTreeRealism",
        ):
            self.assertIn(fragment, self.editor)
        self.assertNotIn(
            'const FString CurrentSurroundingsV2MeshObjectPath(', self.editor
        )

    def test_editor_transaction_is_outstanding_before_modify_and_fully_revalidated(self):
        for endpoint in (
            "EnsureR31BroadShellAssets",
            "ValidateR31BroadShellAssets",
            "ApplyR31BroadShellToLoadedV5DHybridMap",
            "CommitR31BroadShellToLoadedV5DHybridMap",
            "ValidateR31BroadShellInLoadedV5DHybridMap",
        ):
            self.assertIn(endpoint, self.editor_header)
            self.assertIn(endpoint, self.editor)
        apply_start = self.editor.index(
            "ApplyR31BroadShellToLoadedV5DHybridMap(FString& OutMessage)"
        )
        apply_end = self.editor.index(
            "ValidateR31BroadShellInLoadedV5DHybridMap", apply_start
        )
        body = self.editor[apply_start:apply_end]
        self.assertLess(body.index("Transaction->IsOutstanding()"), body.index("Predecessor.Policy->Modify()"))
        self.assertIn("UndoR31AndValidateR30", body)
        self.assertNotIn("Transaction.Cancel", body)
        self.assertNotIn("Cancel()", body)

    def test_wrapper_is_frozen_to_r31_closure_r30_receipts_and_memory_gates(self):
        for fragment in (
            "$sourcePins.Count -ne 6",
            "function Get-PinRelativePath",
            "RepositoryRelativePath = 'SourceAssets\\IstanaPublicViewExploreV5D\\Surroundings\\R31BroadShellLookdev\\NativeSourceClosure\\TRIADIstanaExploreV5DContextPolicyActor.h'",
            "NativeRelativePath = 'Plugins\\TRIADSensorFusion\\Source\\TRIADSensorFusion\\Public\\TRIADIstanaExploreV5DContextPolicyActor.h'",
            "RepositoryRelativePath = 'SourceAssets\\IstanaPublicViewExploreV5D\\Surroundings\\R31BroadShellLookdev\\NativeSourceClosure\\TRIADIstanaExploreV5DContextPolicyActor.cpp'",
            "NativeRelativePath = 'Plugins\\TRIADSensorFusion\\Source\\TRIADSensorFusion\\Private\\TRIADIstanaExploreV5DContextPolicyActor.cpp'",
            "R31 pin lacks both $explicitName and same-path RelativePath semantics.",
            "R33 source or declaration contaminated the immutable R31 context-policy closure",
            "R33SourceOrDeclarationAllowed = $false",
            "$sourceAssetPins.Count -ne 1",
            "$contractReferencedPins.Count -ne 15",
            "$promotedReferencedPins.Count -ne 4",
            "$retainedReferencedPins.Count -ne 11",
            "$r31ContentRelativePaths.Count -ne 5",
            "R30ContentPackageCount' 'R30 transaction receipt') -ne 12",
            "R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31",
            "R31DependencyAllowed' 'R30 capture receipt') -ne $false",
            "MechanicalCaptureValidationPassed' 'R30 capture receipt') -ne $true",
            "ExplicitHumanReviewAcceptance' 'R30 capture receipt') -ne $true",
            "ConfirmedFiveImagesReviewed' 'R30 capture receipt') -ne $true",
            "HumanVisualReviewAttested' 'R30 capture receipt') -ne $true",
            "AutomaticVisualAcceptanceAllowed' 'R30 capture receipt') -ne $false",
            "VisualReviewRequired' 'R30 capture receipt') -ne $false",
            "VisualReviewAccepted' 'R30 capture receipt') -ne $true",
            "R31AdmissionAuthorized' 'R30 capture receipt') -ne $true",
            "Test-ReceiptStateMatches $commitMap $ExpectedMapBytes",
            "Assert-RequiredImmutablePrestate",
            "R30FacadeLookdev).Count -ne 12",
            "Required retained immutable root is missing or empty",
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "$minimumSystemFreeVirtualBytes = 6442450944L",
            "$privateMemoryCeilingBytes = 12884901888L",
            "'-MaxParallelActions=1'",
            "'-NoUBA', '-NoUBALocal'",
            "Remove-IsolatedR31Content",
            "Restore-FileJournal $mapJournal",
            "Restore-TreeJournal $buildJournal",
            "Assert-NativePreState $sourcePins",
            "Assert-Pins $sourcePins $repositoryUnrealRoot",
            "Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter",
            "Assert-NativePreState $sourceAssetPins",
            "Assert-NativePreState $contractReferencedPins",
            "Assert-Pins $contractReferencedPins $nativeProjectRoot -NativeAfter",
            "Restore-FileJournal $contractReferencedJournal",
            "Restore-DirectoryPresenceJournal $contractReferencedDirectoryJournal",
            "ContractReferencedFileCount = $contractReferencedPins.Count",
            "ContractReferencedFiles = @(",
            "$Handle.Kill($true)",
            "CreationDate -cne",
        ):
            self.assertIn(fragment, self.wrapper)
        self.assertNotIn("_PENDING", self.wrapper)
        self.assertNotIn("Bytes = 0L; Sha256 = 'R31_", self.wrapper)

    def test_wrapper_closes_all_15_contract_referenced_files(self):
        rows = [self.contract["sourceMesh"]["sourceObj"]]
        rows.extend(self.contract["sourcePins"])
        rows.extend(self.contract["texturePins"])
        self.assertEqual(len(rows), 15)
        for row in rows:
            relative = row["file"].removeprefix("unreal/").replace("/", "\\")
            self.assertIn(f"RelativePath = '{relative}'", self.wrapper)
            self.assertIn(f"Bytes = {row['bytes']}L", self.wrapper)
            self.assertIn(row["sha256"], self.wrapper)
        self.assertEqual(self.wrapper.count("NativeBeforePresent = $false; Promote = $true"), 4)
        self.assertEqual(self.wrapper.count("; Promote = $false"), 11)


if __name__ == "__main__":
    unittest.main()
