import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = ROOT.parent
TREE_ROOT = ROOT / "SourceAssets" / "IstanaPublicViewExploreV5D" / "TreeRealism"
AMENDMENT_PATH = (
    TREE_ROOT
    / "istana_public_view_v5d_tree_realism.material_response_amendment.v3.json"
)
RENDERER = TREE_ROOT / "render_tree_material_response_audit.py"
AUDIT_ROOT = TREE_ROOT / "MaterialResponseAudit"
RUNTIME_H = (
    ROOT
    / "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DTreeRealismActor.h"
)
RUNTIME_CPP = (
    ROOT
    / "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DTreeRealismActor.cpp"
)
EDITOR_CPP = (
    ROOT
    / "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp"
)
OUTPUT_FILES = {
    "README.md",
    "tree_material_response_audit.json",
    "tree_material_response_contact_sheet.png",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


class ExploreV5DTreeMaterialResponseContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.amendment = json.loads(AMENDMENT_PATH.read_text(encoding="utf-8"))
        cls.report = json.loads(
            (AUDIT_ROOT / "tree_material_response_audit.json").read_text(
                encoding="utf-8"
            )
        )
        cls.runtime_h = RUNTIME_H.read_text(encoding="utf-8")
        cls.runtime_cpp = RUNTIME_CPP.read_text(encoding="utf-8")
        cls.editor_cpp = EDITOR_CPP.read_text(encoding="utf-8")

    def test_amendment_is_existing_stage_and_preserves_exact_census(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism."
            "material_response_amendment.v3",
            self.amendment["schema"],
        )
        stage = self.amendment["stagePolicy"]
        self.assertTrue(stage["existingStageOnly"])
        self.assertFalse(stage["newStageCreated"])
        self.assertFalse(stage["r34Created"])
        self.assertFalse(stage["r32OwnedFilesModified"])
        census = self.amendment["visualTreeCensus"]
        self.assertEqual(729, census["v5dMainAndHeritage"])
        self.assertEqual(7, census["r29LandmarkAdditionsInheritingManagedMeshMaterials"])
        self.assertEqual(736, census["combinedVisualInstances"])
        self.assertEqual(10, census["v5dSourceHismComponents"])
        self.assertEqual(5, census["managedMeshDerivatives"])
        self.assertFalse(census["meshTransformsOrInstanceOrderChanged"])

    def test_exact_thirteen_material_form_slot_roster(self):
        materials = self.amendment["materials"]
        self.assertEqual(13, len(materials))
        self.assertEqual([0, 3, 6, 9, 12, 13], self.amendment["materialResponse"]["formSlotOffsets"])
        self.assertEqual(
            [
                ("umbrella", 0, "bark"),
                ("umbrella", 1, "canopy"),
                ("umbrella", 2, "bark"),
                ("dome", 0, "bark"),
                ("dome", 1, "canopy"),
                ("dome", 2, "bark"),
                ("highFork", 0, "bark"),
                ("highFork", 1, "canopy"),
                ("highFork", 2, "bark"),
                ("columnar", 0, "bark"),
                ("columnar", 1, "bark"),
                ("columnar", 2, "canopy"),
                ("palm", 0, "palmComposite"),
            ],
            [(row["form"], row["slot"], row["role"]) for row in materials],
        )
        response_paths = [row["responseMaterial"] for row in materials]
        source_paths = [row["sourceMaterial"] for row in materials]
        self.assertEqual(13, len(set(response_paths)))
        self.assertEqual(13, len(set(source_paths)))
        self.assertTrue(
            all(
                path.startswith(
                    "/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/"
                )
                for path in response_paths
            )
        )
        self.assertTrue(
            all(
                path.startswith(
                    "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/"
                )
                for path in source_paths
            )
        )

    def test_every_actual_texture_is_exactly_receipt_pinned(self):
        pins = self.amendment["sourceTexturePins"]
        referenced = {
            row[key]
            for row in self.amendment["materials"]
            for key in ("baseColorTexture", "roughnessTexture", "opacityTexture")
            if row[key]
        }
        self.assertEqual(referenced, set(pins))
        self.assertEqual(29, len(pins))
        for relative, receipt in pins.items():
            path = REPO_ROOT / relative
            self.assertTrue(path.is_file(), relative)
            self.assertEqual(receipt["bytes"], path.stat().st_size, relative)
            self.assertEqual(receipt["sha256"], sha256(path), relative)

    def test_opacity_wpo_simulation_and_truth_boundaries_are_fail_closed(self):
        opacity = self.amendment["opacityAndSilhouette"]
        self.assertEqual(0.333, opacity["opacityMaskClipValue"])
        for key, value in opacity.items():
            if key != "opacityMaskClipValue":
                self.assertFalse(value, key)
        wind = self.amendment["windPreservation"]
        self.assertEqual(
            "2532EE61555FF7D4C2B691859E6F1DE2FFE48E6AA176468D959DDE5D95A38117",
            wind["customExpressionCodeSha256"],
        )
        self.assertEqual(633, wind["customExpressionCodeUtf8Bytes"])
        self.assertEqual(
            [
                "TRIAD_WindStrengthCm",
                "TRIAD_WindSpeed",
                "TRIAD_WindHeightCm",
                "TRIAD_WindResponseScale",
                "TRIAD_MaxWpoCm",
            ],
            wind["scalarParameters"],
        )
        self.assertEqual(["TRIAD_WindDirection"], wind["vectorParameters"])
        self.assertEqual(26, wind["runtimeResponseMids"])
        preservation = self.amendment["preservation"]
        self.assertTrue(all(value is False for value in preservation.values()))
        truth = dict(self.amendment["truthBoundary"])
        self.assertTrue(truth.pop("appearanceOnly"))
        self.assertTrue(all(value is False for value in truth.values()))

    def test_editor_factory_duplicates_only_isolated_materials_and_wraps_bounded_outputs(self):
        for token in (
            "TreeMaterialCount = 13",
            "TreeMaterialOffsets[] = {0, 3, 6, 9, 12, 13}",
            "EnsureTreeResponseMaterials",
            "StaticDuplicateObject(",
            "RF_Public | RF_Standalone | RF_Transactional",
            "TRIAD_V5D_TREE_RESPONSE_PER_INSTANCE_RANDOM",
            "TRIAD_TreeResponseTintLow",
            "TRIAD_TreeResponseTintHigh",
            "TRIAD_TreeResponseLumaLow",
            "TRIAD_TreeResponseLumaHigh",
            "TRIAD_TreeResponseDesaturationFraction",
            "TRIAD_TreeResponseRoughnessMin",
            "TRIAD_TreeResponseRoughnessMax",
            "TRIAD_TreeResponseBaseColorMax",
            "TRIAD_TreeResponseSubsurfaceTint",
            "EditorOnly->OpacityMask.Expression != OriginalOpacity",
            "EditorOnly->WorldPositionOffset.Expression != OriginalWpo",
            "OriginalSubsurfaceOutput",
            "SubsurfaceResponse->A,",
            "SourceEditorOnly->SubsurfaceColor",
            "Material->OpacityMaskClipValue, 0.333f",
            "ResponseSlots[Slot].MaterialInterface = ResponseMaterial",
        ):
            self.assertIn(token, self.editor_cpp)
        ensure_start = self.editor_cpp.index("bool EnsureTreeResponseMaterials(")
        ensure_end = self.editor_cpp.index("bool LoadAndValidateMeshes(", ensure_start)
        ensure = self.editor_cpp[ensure_start:ensure_end]
        self.assertNotIn("Sources[Index]->Modify", ensure)
        self.assertNotIn("Sources[Index]->MarkPackageDirty", ensure)
        self.assertNotIn("Sources[Index]->PostEditChange", ensure)

    def test_runtime_uses_cached_six_parameter_propagation_and_exact_restore(self):
        for token in (
            "RuntimeTreeResponseMaterialCount = TreeMaterialCount * 2",
            "PrimaryActorTick.bCanEverTick = true",
            "PrimaryActorTick.bStartWithTickEnabled = false",
            "AddTickPrerequisiteActor(V4LandscapeActor)",
            "SyncRuntimeTreeWindParameters(false, Error)",
            "RuntimeCachedWindScalarValues[CacheIndex]",
            "RuntimeCachedWindDirectionValues[MaterialIndex]",
            "FMath::IsNearlyEqual(",
            "Response->SetScalarParameterValue(",
            "Response->SetVectorParameterValue(",
            "RuntimeTreeResponseMaterials[MaterialStart + Slot]",
            "RuntimeOriginalMaterials[Start + Slot]",
            "ResetRuntimeTreeMaterialResponseState()",
            "RemoveTickPrerequisiteActor(V4LandscapeActor)",
        ):
            self.assertIn(token, self.runtime_cpp)
        for parameter in self.amendment["windPreservation"]["scalarParameters"]:
            self.assertIn(f'TEXT("{parameter}")', self.runtime_cpp)
        self.assertIn('TEXT("TRIAD_WindDirection")', self.runtime_cpp)
        self.assertIn("virtual void Tick(float DeltaSeconds) override", self.runtime_h)

    def test_committed_offline_audit_has_exact_roster_receipts_and_false_claims(self):
        self.assertEqual(OUTPUT_FILES, {path.name for path in AUDIT_ROOT.iterdir() if path.is_file()})
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism."
            "material_response_offline_audit.v1",
            self.report["schema"],
        )
        self.assertEqual("PASS", self.report["validationStatus"])
        self.assertEqual(13, self.report["materialRosterCount"])
        self.assertEqual([128, 32, 8], self.report["mipProxySizesPixels"])
        self.assertEqual(0.333, self.report["opacityMaskClipValue"])
        self.assertTrue(all(self.report["validationChecks"].values()))
        self.assertTrue(all(value is False for value in self.report["claimBoundary"].values()))
        preview = AUDIT_ROOT / self.report["previewReceipt"]["file"]
        self.assertEqual(self.report["previewReceipt"]["bytes"], preview.stat().st_size)
        self.assertEqual(self.report["previewReceipt"]["sha256"], sha256(preview))
        self.assertEqual(812, self.report["previewReceipt"]["widthPixels"])
        self.assertEqual(2182, self.report["previewReceipt"]["heightPixels"])

    def test_check_is_read_only_and_deterministic_rerender_matches(self):
        before = {
            path.name: (sha256(path), path.stat().st_mtime_ns)
            for path in AUDIT_ROOT.iterdir()
            if path.is_file()
        }
        checked = subprocess.run(
            [sys.executable, "-B", str(RENDERER), "--check"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=45,
            check=False,
        )
        self.assertEqual(0, checked.returncode, checked.stdout + checked.stderr)
        after = {
            path.name: (sha256(path), path.stat().st_mtime_ns)
            for path in AUDIT_ROOT.iterdir()
            if path.is_file()
        }
        self.assertEqual(before, after)

        try:
            import PIL  # noqa: F401
            import numpy  # noqa: F401
        except ImportError:
            self.skipTest("deterministic rerender requires Pillow and NumPy")
        with tempfile.TemporaryDirectory() as temp:
            rerendered = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(RENDERER),
                    "--render",
                    "--output-dir",
                    temp,
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=60,
                check=False,
            )
            self.assertEqual(
                0, rerendered.returncode, rerendered.stdout + rerendered.stderr
            )
            generated = json.loads(
                (Path(temp) / "tree_material_response_audit.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(
                self.report["previewReceipt"]["sha256"],
                generated["previewReceipt"]["sha256"],
            )
            self.assertEqual(
                self.report["previewReceipt"]["decodedRgbaSha256"],
                generated["previewReceipt"]["decodedRgbaSha256"],
            )


if __name__ == "__main__":
    unittest.main()
