import ast
import pathlib
import unittest


REPO = pathlib.Path(__file__).resolve().parents[4]
REPAIR_SCRIPT = REPO / "scripts" / "Repair-AirSimTriadRuntimeOpticalFlowUE55.py"


class AirSimOpticalFlowUE55RepairContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = REPAIR_SCRIPT.read_text(encoding="utf-8")
        cls.tree = ast.parse(cls.source)

    def test_scope_is_exactly_the_two_legacy_optical_flow_materials(self):
        self.assertIn(
            '"/AirSimTriadRuntime/HUDAssets/OpticalFlowMaterial"', self.source
        )
        self.assertIn(
            '"/AirSimTriadRuntime/HUDAssets/OpticalFlowRGBMaterial"', self.source
        )
        self.assertIn("exactAssets={len(statuses)}", self.source)

    def test_ue55_motion_vector_replacement_is_exact_and_fail_closed(self):
        self.assertIn(
            'LEGACY_LINE = "float4 previousPos = mul(worldPos, View.PrevViewProj);"',
            self.source,
        )
        self.assertIn(
            'UE55_LINE = "float4 previousPos = mul(H, ResolvedView.ClipToPrevClip);"',
            self.source,
        )
        for marker in (
            "code_before.replace(LEGACY_LINE, UE55_LINE)",
            "if len(motion_nodes) != 1:",
            "legacy_count == 1 and ue55_count == 0",
            "legacy_count == 0 and ue55_count == 1",
            "post-repair code validation failed",
        ):
            self.assertIn(marker, self.source)

    def test_ue55_object_enumeration_save_recompile_and_receipt_markers_exist(self):
        for marker in (
            "unreal.ObjectIterator(unreal.MaterialExpressionCustom)",
            "outer = obj.get_outer()",
            "unreal.MaterialEditingLibrary.recompile_material(material)",
            "unreal.EditorAssetLibrary.save_loaded_asset(",
            "TRIAD_AIRSIM_OPTICAL_FLOW_UE55_MATERIAL_VALID",
            "TRIAD_AIRSIM_OPTICAL_FLOW_UE55_REPAIR_PASS",
            "ALREADY_REPAIRED",
        ):
            self.assertIn(marker, self.source)


if __name__ == "__main__":
    unittest.main()
