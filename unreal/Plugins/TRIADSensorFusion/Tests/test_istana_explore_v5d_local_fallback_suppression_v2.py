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
TOOL = TOOL_ROOT / "build_local_fallback_suppression_v2.py"
CONTRACT = TOOL_ROOT / "local_fallback_suppression_v2.contract.json"
GENERATED = (
    REPO / "unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV2"
)
SOURCE = (
    Path("D:/triad/TRIAD/Saved/TRIAD/IstanaReferences/V5D/Current20260831")
    / "GeneratedCurrent"
)
RUNTIME = PLUGIN / "Source/TRIADSensorFusion"
PROVENANCE_H = (
    RUNTIME
    / "Public/TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h"
)
PROVENANCE_CPP = (
    RUNTIME
    / "Private/TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.cpp"
)
CPP_TEST = (
    RUNTIME
    / "Private/Tests/TRIADIstanaExploreV5DLocalFallbackSuppressionV2ProvenanceTests.cpp"
)

SPEC = importlib.util.spec_from_file_location(
    "build_local_fallback_suppression_v2", TOOL
)
assert SPEC and SPEC.loader
build = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = build
SPEC.loader.exec_module(build)


V1_PINS = {
    TOOL_ROOT / "local_fallback_suppression_v1.contract.json":
        "68D68B4D906076A49AA070C7341D38245100A7577C4AF7C66C2D62C9A12D0BB5",
    TOOL_ROOT / "build_local_fallback_suppression_v1.py":
        "D8B35571AD49E07FD226D8D65EAD6D7F809E3C347D19D779E5ABBCFA7B6E1B80",
    REPO / "unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1.obj":
        "C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9",
    REPO / "unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/IstanaPublicViewV5DLocalFallbackSuppression.v1.metadata.json":
        "154BE543F3E7398900F39658FDE41604ABE4642A11EFE097DF1C9C3F8BF1CC61",
    REPO / "unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/IstanaPublicViewV5DLocalFallbackSuppression.v1.manifest.json":
        "D8627FECAC184B9B658E8C36048544A026811D56E130336441792FA997A9B88D",
}


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


class IstanaExploreV5DLocalFallbackSuppressionV2Tests(unittest.TestCase):
    def test_contract_is_exact_additive_negative_authority(self) -> None:
        contract = build.load_contract(CONTRACT)
        self.assertEqual(
            [row["sourceKey"] for row in contract["suppression"]["sourceKeys"]],
            [
                "OSM:way:46521250",
                "OSM:way:1551538490",
                "OSM:way:429681826",
            ],
        )
        self.assertEqual(
            [row["objGroup"] for row in contract["suppression"]["sourceKeys"]],
            [
                "OSM_way_46521250_P00",
                "OSM_way_1551538490_P00",
                "OSM_way_429681826_P00",
            ],
        )
        filtered = contract["suppression"]["expectedFiltered"]
        self.assertEqual(43448, filtered["triangles"])
        self.assertEqual(96, filtered["omittedTriangles"])
        self.assertEqual(1386, filtered["visibleFeatures"])
        self.assertEqual(1388, filtered["visiblePolygonParts"])
        self.assertEqual(6354063, filtered["bytes"])
        self.assertEqual(
            "99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110",
            filtered["sha256"],
        )
        self.assertEqual("LocalFallbackSuppressionV2", contract["output"]["directory"])
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
        self.assertEqual(
            {
                "role": "immutableV1ByteBlockOmissionImplementation",
                "file": "build_local_fallback_suppression_v1.py",
                "bytes": 17594,
                "sha256": "D8B35571AD49E07FD226D8D65EAD6D7F809E3C347D19D779E5ABBCFA7B6E1B80",
            },
            contract["implementationBase"],
        )

    def test_v1_files_remain_byte_identical(self) -> None:
        for path, digest in V1_PINS.items():
            self.assertTrue(path.is_file(), path)
            self.assertEqual(digest, build.sha256_bytes(path.read_bytes()), path)

    def test_filter_is_only_the_three_exact_group_block_deletions(self) -> None:
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
        ]
        group_specs = (
            (
                "OSM_way_46521250_P00",
                (("MAT_GENERIC_BUILDING_HINT", 8),
                 ("MAT_ROOF_GENERIC_BUILDING_HINT", 2),
                 ("MAT_BOTTOM_HIDDEN", 2)),
            ),
            (
                "OSM_way_1551538490_P00",
                (("MAT_COMMERCIAL_HINT", 22),
                 ("MAT_ROOF_COMMERCIAL_HINT", 9),
                 ("MAT_BOTTOM_HIDDEN", 9)),
            ),
            (
                "OSM_way_429681826_P00",
                (("MAT_RESIDENTIAL_HINT", 24),
                 ("MAT_ROOF_RESIDENTIAL_HINT", 10),
                 ("MAT_BOTTOM_HIDDEN", 10)),
            ),
        )
        for group, materials in group_specs:
            lines.append(f"g {group}\n".encode("utf-8"))
            for material, count in materials:
                lines.append(f"usemtl {material}\n".encode("utf-8"))
                lines.extend([b"f 1/1 2/2 3/3\n"] * count)
            lines.extend(
                [
                    f"g RETAINED_{group}\n".encode("utf-8"),
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
                ("OSM_way_429681826_P00", 44),
            ],
            [(row["objGroup"], row["triangles"]) for row in blocks],
        )
        self.assertEqual(4, build.scan_obj(filtered)["triangles"])

    def test_filter_fails_closed_for_v1_missing_or_expanded_scope(self) -> None:
        source = b"g OSM_way_46521250_P00\nusemtl MAT_GENERIC_BUILDING_HINT\nf 1 2 3\n"
        with self.assertRaises(ValueError):
            build.filter_render_obj(source, build.EXPECTED_GROUPS)
        with self.assertRaises(ValueError):
            build.filter_render_obj(source, build.EXPECTED_GROUPS[:2])
        with self.assertRaises(ValueError):
            build.filter_render_obj(
                source, (*build.EXPECTED_GROUPS, "OSM_way_Unexpected_P00")
            )

    def test_checked_in_outputs_are_hash_pinned_and_self_consistent(self) -> None:
        manifest_path = (
            GENERATED / "IstanaPublicViewV5DLocalFallbackSuppression.v2.manifest.json"
        )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(build.MANIFEST_SCHEMA, manifest["schema"])
        self.assertEqual(
            "FBE8F7D0C8BB935A3DFC2AE2953B9480902B4CCBFBE7765170FC09AF49081120",
            manifest["outputSetSha256"],
        )
        self.assertEqual(build.file_record(CONTRACT), manifest["contract"])
        self.assertEqual(build.file_record(TOOL), manifest["tool"])
        for record in manifest["outputs"]:
            path = GENERATED / record["file"]
            self.assertEqual(build.file_record(path), record)
        obj = GENERATED / "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.obj"
        stats = build.scan_obj(obj.read_bytes())
        self.assertEqual(43448, stats["triangles"])
        self.assertEqual(24522, stats["vertexLines"])
        self.assertEqual(130632, stats["textureCoordinateLines"])
        metadata = json.loads(
            (GENERATED / "IstanaPublicViewV5DLocalFallbackSuppression.v2.metadata.json")
            .read_text(encoding="utf-8")
        )
        self.assertEqual(build.METADATA_SCHEMA, metadata["schema"])
        self.assertEqual(
            [12, 40, 44],
            [row["triangles"] for row in metadata["suppression"]["omittedObjBlocks"]],
        )
        self.assertTrue(
            metadata["preservation"]["sourceInputsMatchedBeforeAndAfter"]
        )
        self.assertFalse(metadata["authorityPolicy"]["providerOverlapResolved"])

    def test_cooked_provenance_is_independent_versioned_and_fail_closed(self) -> None:
        header = PROVENANCE_H.read_text(encoding="utf-8")
        implementation = PROVENANCE_CPP.read_text(encoding="utf-8")
        cpp_test = CPP_TEST.read_text(encoding="utf-8")
        combined = header + implementation
        for marker in (
            "UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance",
            'ContractVersion = TEXT("local_fallback_suppression_v2")',
            'TEXT("OSM:way:46521250")',
            'TEXT("OSM:way:1551538490")',
            'TEXT("OSM:way:429681826")',
            "RenderTriangles = 43448",
            "SuppressedTriangles = 96",
            "VisibleFeatures = 1386",
            "VisiblePolygonParts = 1388",
            "ImportedVertices = 24468",
            "ImportedVertexInstances = 130344",
            "99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110",
            "31A32BCB8DAED756E0B8D90D0EE795A43B389BFB3148322A0FAC761A9BD73477",
            "7D455FE8C1E057F2380BE4941026F511AE5D5A1817795F238496FCEAD77EDE04",
            "FBE8F7D0C8BB935A3DFC2AE2953B9480902B4CCBFBE7765170FC09AF49081120",
            "bCanonicalRenderAndRfInputsHashPinnedUnchanged = true",
            "bProviderOverlapResolved = false",
            "IsCanonicalContract",
        ):
            self.assertIn(marker, combined)
        self.assertNotIn("WITH_EDITORONLY_DATA", combined)
        for marker in (
            "Reduced suppression scope invalidates provenance",
            "Expanded suppression scope invalidates provenance",
            "Claiming provider resolution invalidates provenance",
            "V1 triangle census invalidates V2 provenance",
            "Canonical reset is deterministic",
        ):
            self.assertIn(marker, cpp_test)

    @unittest.skipUnless(
        (SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.obj").is_file(),
        "hash-pinned external 2026-08-31 derivative is not mounted",
    )
    def test_mounted_build_is_deterministic_exact_and_non_mutating(self) -> None:
        contract = build.load_contract(CONTRACT)
        before = build.validate_inputs(SOURCE, contract)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = build.build(SOURCE, root / "first", CONTRACT)
            second = build.build(SOURCE, root / "second", CONTRACT)
            output = contract["output"]
            for key in ("filteredRenderObj", "renderMtl", "metadata", "manifest"):
                self.assertEqual(
                    (root / "first" / output[key]).read_bytes(),
                    (root / "second" / output[key]).read_bytes(),
                    key,
                )
            self.assertEqual(first["outputSetSha256"], second["outputSetSha256"])
            payload = (root / "first" / output["filteredRenderObj"]).read_bytes()
            source_payload = (
                SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.obj"
            ).read_bytes()
            self.assertEqual(retained_obj_lines(source_payload), payload.splitlines(keepends=True))
            self.assertEqual(43448, build.scan_obj(payload)["triangles"])
        self.assertEqual(before, build.validate_inputs(SOURCE, contract))


if __name__ == "__main__":
    unittest.main()
