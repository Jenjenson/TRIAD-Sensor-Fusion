from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
import sys


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
TOOL_ROOT = PLUGIN / "Tools/IstanaExploreV5D"
TOOL = TOOL_ROOT / "build_local_fallback_suppression_v1.py"
CONTRACT = TOOL_ROOT / "local_fallback_suppression_v1.contract.json"
SOURCE = (
    Path("D:/triad/TRIAD/Saved/TRIAD/IstanaReferences/V5D/Current20260831")
    / "GeneratedCurrent"
)

SPEC = importlib.util.spec_from_file_location(
    "build_local_fallback_suppression_v1", TOOL
)
assert SPEC and SPEC.loader
build = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = build
SPEC.loader.exec_module(build)


def retained_obj_lines(payload: bytes) -> list[bytes]:
    targets = set(build.EXPECTED_GROUPS)
    retained: list[bytes] = []
    suppress = False
    for line in payload.splitlines(keepends=True):
        if line.startswith(b"g "):
            suppress = line[2:].decode("utf-8").strip() in targets
        if not suppress:
            retained.append(line)
    return retained


class IstanaExploreV5DLocalFallbackSuppressionV1Tests(unittest.TestCase):
    def test_contract_is_exact_render_only_negative_authority(self) -> None:
        contract = build.load_contract(CONTRACT)
        self.assertEqual(
            [row["sourceKey"] for row in contract["suppression"]["sourceKeys"]],
            ["OSM:way:46521250", "OSM:way:1551538490"],
        )
        self.assertEqual(
            [row["objGroup"] for row in contract["suppression"]["sourceKeys"]],
            ["OSM_way_46521250_P00", "OSM_way_1551538490_P00"],
        )
        self.assertEqual(
            sum(
                row["triangleRange"][1]
                for row in contract["suppression"]["sourceKeys"]
            ),
            52,
        )
        canonical = contract["suppression"]["expectedCanonical"]
        filtered = contract["suppression"]["expectedFiltered"]
        self.assertEqual(43544, canonical["triangles"])
        self.assertEqual(43492, filtered["triangles"])
        self.assertEqual(52, filtered["omittedTriangles"])
        self.assertEqual(
            "C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9",
            filtered["sha256"],
        )
        self.assertTrue(contract["authorityPolicy"]["renderOnly"])
        self.assertTrue(contract["authorityPolicy"]["localFallbackOnly"])
        for key in (
            "providerOverlapResolved",
            "collisionEnabled",
            "navigationAuthority",
            "sensorOcclusionAuthority",
            "rfGeometryAuthority",
            "rfMaterialAuthority",
            "surveyOrAsBuiltAuthority",
            "facadeOrApertureAuthority",
            "measuredHeightClaimed",
        ):
            self.assertFalse(contract["authorityPolicy"][key], key)
        self.assertEqual(7, len(contract["sourceInputs"]))
        self.assertTrue(
            all(row["mustRemainByteIdentical"] for row in contract["sourceInputs"])
        )

    def test_filter_is_only_an_exact_group_block_deletion(self) -> None:
        lines = [
            b"# fixture\n",
            b"mtllib fixture.mtl\n",
            b"v 0 0 0\n",
            b"v 1 0 0\n",
            b"v 0 1 0\n",
            b"vt 0 0\n",
            b"vt 1 0\n",
            b"vt 0 1\n",
            b"g RETAINED_A\n",
            b"usemtl MAT_GENERIC_BUILDING_HINT\n",
            b"f 1/1 2/2 3/3\n",
            b"g OSM_way_46521250_P00\n",
            b"usemtl MAT_GENERIC_BUILDING_HINT\n",
        ]
        lines.extend([b"f 1/1 2/2 3/3\n"] * 8)
        lines.append(b"usemtl MAT_ROOF_GENERIC_BUILDING_HINT\n")
        lines.extend([b"f 1/1 2/2 3/3\n"] * 2)
        lines.append(b"usemtl MAT_BOTTOM_HIDDEN\n")
        lines.extend([b"f 1/1 2/2 3/3\n"] * 2)
        lines.extend(
            [
                b"g RETAINED_B\n",
                b"usemtl MAT_COMMERCIAL_HINT\n",
                b"f 1/1 2/2 3/3\n",
                b"g OSM_way_1551538490_P00\n",
                b"usemtl MAT_COMMERCIAL_HINT\n",
            ]
        )
        lines.extend([b"f 1/1 2/2 3/3\n"] * 22)
        lines.append(b"usemtl MAT_ROOF_COMMERCIAL_HINT\n")
        lines.extend([b"f 1/1 2/2 3/3\n"] * 9)
        lines.append(b"usemtl MAT_BOTTOM_HIDDEN\n")
        lines.extend([b"f 1/1 2/2 3/3\n"] * 9)
        lines.extend(
            [
                b"g RETAINED_C\n",
                b"usemtl MAT_RESIDENTIAL_HINT\n",
                b"f 1/1 2/2 3/3\n",
            ]
        )
        source = b"".join(lines)
        filtered, blocks = build.filter_render_obj(source, build.EXPECTED_GROUPS)
        self.assertEqual(retained_obj_lines(source), filtered.splitlines(keepends=True))
        self.assertEqual(
            [
                ("OSM_way_46521250_P00", 12),
                ("OSM_way_1551538490_P00", 40),
            ],
            [(row["objGroup"], row["triangles"]) for row in blocks],
        )
        self.assertEqual(3, build.scan_obj(filtered)["triangles"])
        self.assertEqual(
            [b"v 0 0 0\n", b"v 1 0 0\n", b"v 0 1 0\n"],
            [line for line in filtered.splitlines(keepends=True) if line.startswith(b"v ")],
        )

    def test_filter_fails_closed_for_missing_or_expanded_scope(self) -> None:
        source = (
            b"g OSM_way_46521250_P00\n"
            b"usemtl MAT_GENERIC_BUILDING_HINT\n"
            b"f 1 2 3\n"
        )
        with self.assertRaises(ValueError):
            build.filter_render_obj(source, build.EXPECTED_GROUPS)
        with self.assertRaises(ValueError):
            build.filter_render_obj(
                source,
                (*build.EXPECTED_GROUPS, "OSM_way_Unexpected_P00"),
            )

    @unittest.skipUnless(
        (SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.obj").is_file(),
        "hash-pinned external 2026-08-31 derivative is not mounted",
    )
    def test_mounted_derivative_build_is_exact_deterministic_and_non_mutating(
        self,
    ) -> None:
        contract = build.load_contract(CONTRACT)
        before = build.validate_inputs(SOURCE, contract)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first_dir = root / "first"
            second_dir = root / "second"
            first = build.build(SOURCE, first_dir, CONTRACT)
            second = build.build(SOURCE, second_dir, CONTRACT)
            output = contract["output"]
            for key in ("filteredRenderObj", "renderMtl", "metadata", "manifest"):
                self.assertEqual(
                    (first_dir / output[key]).read_bytes(),
                    (second_dir / output[key]).read_bytes(),
                    key,
                )
            filtered = first_dir / output["filteredRenderObj"]
            payload = filtered.read_bytes()
            expected = contract["suppression"]["expectedFiltered"]
            self.assertEqual(expected["bytes"], len(payload))
            self.assertEqual(expected["sha256"], build.sha256_bytes(payload))
            stats = build.scan_obj(payload)
            self.assertEqual(43492, stats["triangles"])
            self.assertEqual(expected["materialTriangles"], stats["materialTriangles"])
            self.assertEqual(24522, stats["vertexLines"])
            self.assertEqual(130632, stats["textureCoordinateLines"])
            source_payload = (
                SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.obj"
            ).read_bytes()
            self.assertEqual(
                retained_obj_lines(source_payload),
                payload.splitlines(keepends=True),
            )
            self.assertEqual(
                (SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.mtl").read_bytes(),
                (first_dir / output["renderMtl"]).read_bytes(),
            )
            metadata = json.loads(
                (first_dir / output["metadata"]).read_text(encoding="utf-8")
            )
            self.assertEqual(build.METADATA_SCHEMA, metadata["schema"])
            self.assertEqual(
                list(build.EXPECTED_SOURCE_KEYS),
                [row["sourceKey"] for row in metadata["suppression"]["sourceKeys"]],
            )
            self.assertTrue(
                metadata["preservation"]["sourceInputsMatchedBeforeAndAfter"]
            )
            self.assertFalse(metadata["authorityPolicy"]["providerOverlapResolved"])
            self.assertEqual(first["outputSetSha256"], second["outputSetSha256"])
        after = build.validate_inputs(SOURCE, contract)
        self.assertEqual(before, after)


if __name__ == "__main__":
    unittest.main()
