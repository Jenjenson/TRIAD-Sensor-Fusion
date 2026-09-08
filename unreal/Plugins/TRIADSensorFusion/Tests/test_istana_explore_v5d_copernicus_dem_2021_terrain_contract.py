from __future__ import annotations

import csv
import hashlib
import importlib.util
import json
import math
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO = Path(__file__).resolve().parents[4]
PACKAGE = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Terrain"
    / "CopernicusDEM2021"
)
SOURCES = PACKAGE / "Sources"
GENERATED = PACKAGE / "Generated"
CONTRACT_PATH = PACKAGE / "copernicus_dem_2021.contract.json"
GENERATOR = PACKAGE / "prepare_copernicus_dem_2021.py"
MANIFEST_PATH = GENERATED / "IstanaCopernicusDEM2021Terrain.manifest.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def read_tiff_tags(path: Path) -> dict[int, tuple[int | float, ...]]:
    type_sizes = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 11: 4, 12: 8}
    with path.open("rb") as handle:
        byte_order = handle.read(2)
        if byte_order == b"II":
            endian = "<"
        elif byte_order == b"MM":
            endian = ">"
        else:
            raise AssertionError("invalid TIFF byte order")
        magic, first_ifd = struct.unpack(endian + "HI", handle.read(6))
        if magic != 42:
            raise AssertionError("not a classic TIFF")
        handle.seek(first_ifd)
        entry_count = struct.unpack(endian + "H", handle.read(2))[0]
        tags: dict[int, tuple[int | float, ...]] = {}
        for _ in range(entry_count):
            entry = handle.read(12)
            tag, value_type, count = struct.unpack(endian + "HHI", entry[:8])
            if value_type not in type_sizes:
                continue
            byte_count = type_sizes[value_type] * count
            if byte_count <= 4:
                payload = entry[8 : 8 + byte_count]
            else:
                offset = struct.unpack(endian + "I", entry[8:12])[0]
                position = handle.tell()
                handle.seek(offset)
                payload = handle.read(byte_count)
                handle.seek(position)
            formats = {1: "B", 3: "H", 4: "I", 11: "f", 12: "d"}
            if value_type not in formats:
                continue
            tags[tag] = struct.unpack(endian + formats[value_type] * count, payload)
    return tags


def png_header(path: Path) -> tuple[int, int, int, int]:
    with path.open("rb") as handle:
        if handle.read(8) != b"\x89PNG\r\n\x1a\n":
            raise AssertionError("not a PNG")
        length = struct.unpack(">I", handle.read(4))[0]
        if handle.read(4) != b"IHDR" or length != 13:
            raise AssertionError("PNG does not begin with a canonical IHDR")
        width, height, bit_depth, colour_type = struct.unpack(">IIBB", handle.read(10))
    return width, height, bit_depth, colour_type


def load_generator():
    spec = importlib.util.spec_from_file_location("triad_copernicus_dem_2021", GENERATOR)
    if spec is None or spec.loader is None:
        raise AssertionError("cannot load Copernicus terrain generator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class IstanaExploreV5DCopernicusDem2021TerrainContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (PACKAGE / "README.md", PACKAGE / "NOTICE.md", CONTRACT_PATH, GENERATOR, MANIFEST_PATH):
            if not path.is_file():
                raise AssertionError(f"missing Copernicus terrain package input: {path}")
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    def test_all_nine_public_source_files_are_exactly_receipted(self) -> None:
        expected = {
            "dem": (31871871, "FE2CF9DBC8A06AC328FB6D3D11A3764ADAC203E91ADFAE6CB066E07390480417"),
            "heightErrorMask": (31149202, "D90353AE17E0804DE2DFE42AF365EB23278B3BB515BE67CCD70DC53AF2A48B62"),
            "editMask": (451628, "4C0310D9144CDAC65DDF5AEA0D4C6F6DEB3AB1E2BF324BE9EFABB9FAE7229B2A"),
            "fillMask": (409281, "BD8D8C0E65CB7DB66F94D7701FA72F7A9DBFF01244035B9AA10AD65BF18F6C7D"),
            "waterBodyMask": (200245, "50CEC0967E8D2D5DDA682A6A62D761F8B4E2921357EA9CBEDD14F05D0A183CA8"),
            "productMetadata": (44718, "9F810156C34A06DABD5C88F0B9B37CDED449F9AAA21A6D7F1A0308E0CA0EA467"),
            "accuracyLayer": (671, "E9E06A64B4BA4B171B515E7E1351E66440E1D651EDEEA91A246A250BA6B2DDD3"),
            "sourceDataLayer": (48360, "2FDE354A5006FA7487A11544C5914EEA43B13710D81363CD8A29132AB5372D10"),
            "licence": (117522, "32049914C37F14E7D53B48D13D74A49E77C030236E2ACC7A0DF426F9344FEBA2"),
        }
        self.assertEqual(self.contract["schema"], "triad.copernicus_dem_2021.contract.v1")
        self.assertEqual({entry["role"] for entry in self.contract["sources"]}, set(expected))
        for entry in self.contract["sources"]:
            size, digest = expected[entry["role"]]
            path = SOURCES / entry["file"]
            self.assertEqual(entry["bytes"], size)
            self.assertEqual(entry["sha256"], digest)
            self.assertEqual(path.stat().st_size, size)
            self.assertEqual(sha256(path), digest)
            self.assertTrue(entry["sourceUrl"].startswith("https://copernicus-dem-30m.s3.eu-central-1.amazonaws.com/"))

    def test_geotiff_grid_is_wgs84_pixel_is_point_float32(self) -> None:
        raster_names = [
            "Copernicus_DSM_COG_10_N01_00_E103_00_DEM.tif",
            "Copernicus_DSM_COG_10_N01_00_E103_00_HEM.tif",
            "Copernicus_DSM_COG_10_N01_00_E103_00_EDM.tif",
            "Copernicus_DSM_COG_10_N01_00_E103_00_FLM.tif",
            "Copernicus_DSM_COG_10_N01_00_E103_00_WBM.tif",
        ]
        grids = [read_tiff_tags(SOURCES / name) for name in raster_names]
        for tags in grids:
            self.assertEqual(tags[256], (3600,))
            self.assertEqual(tags[257], (3600,))
            self.assertEqual(tags[33550], (1.0 / 3600.0, 1.0 / 3600.0, 0.0))
            self.assertEqual(tags[33922], (0.0, 0.0, 0.0, 103.0, 2.0, 0.0))
            geokeys = tags[34735]
            self.assertIn(1025, geokeys)
            self.assertIn(4326, geokeys)
        dem = grids[0]
        self.assertEqual(dem[258], (32,))
        self.assertEqual(dem[339], (3,))

    def test_product_metadata_and_licence_bind_vertical_datum_and_accuracy(self) -> None:
        metadata = (SOURCES / "Copernicus_DSM_10_N01_00_E103_00.xml").read_text(encoding="utf-8")
        accuracy = (SOURCES / "Copernicus_DSM_10_N01_00_E103_00_ACM.kml").read_text(encoding="utf-8")
        notice = (PACKAGE / "NOTICE.md").read_text(encoding="utf-8")
        self.assertIn('horizontal: "WGS84-G1150"', metadata)
        self.assertIn('vertical: "WGS 84 Geoid EGM08"', metadata)
        self.assertIn('codeSpace="EPSG::3855"', metadata)
        self.assertIn('codeListValue="4326"', metadata)
        self.assertIn("<value>2.701</value>", accuracy)
        self.assertIn("<value>1.918</value>", accuracy)
        self.assertIn("Copernicus WorldDEM-30", notice)
        self.assertIn("do not incur any liability", notice)

    def test_aoi_derivative_retains_real_relief_but_fails_closed_as_authority(self) -> None:
        manifest = self.manifest
        self.assertEqual(manifest["schema"], "triad.copernicus_dem_2021.aoi.v1")
        self.assertEqual(manifest["aoi"]["radiusMeters"], 1000.0)
        self.assertEqual(manifest["aoi"]["sourceMarginMeters"], 225.0)
        self.assertEqual(
            manifest["aoi"]["sourceSampleCount"],
            manifest["aoi"]["sourceSampleRows"]
            * manifest["aoi"]["sourceSampleColumns"],
        )
        self.assertGreater(manifest["aoi"]["sourceSampleRows"], 71)
        self.assertGreater(manifest["aoi"]["sourceSampleColumns"], 70)
        self.assertEqual(manifest["sourceDataset"]["verticalCrs"], "EGM2008 height / EPSG:3855")
        self.assertAlmostEqual(manifest["sourceDataset"]["accuracyLayerLe90Metres"], 2.701)
        self.assertAlmostEqual(manifest["heightSemantics"]["centreBilinearDsmHeightEgm2008Meters"], 49.04186671265586)
        self.assertAlmostEqual(manifest["heightSemantics"]["visualProxyCentreReferenceHeightEgm2008Meters"], 32.84910015044898)
        self.assertGreater(manifest["visualGroundProxy"]["reliefSpanInsideCircleMeters"], 30.0)
        self.assertLess(manifest["visualGroundProxy"]["reliefSpanInsideCircleMeters"], 33.0)
        self.assertFalse(manifest["qualityAdmission"]["heightErrorMaskAoiComplete"])
        self.assertTrue(
            manifest["visualGroundProxy"][
                "sourceMarginProtectsOutputFromEdgePadding"
            ]
        )
        self.assertGreaterEqual(
            manifest["visualGroundProxy"]["configuredSourceMarginMeters"],
            manifest["visualGroundProxy"]["minimumRequiredSourceMarginMeters"],
        )
        self.assertEqual(
            manifest["visualGroundProxy"]["filterSupportRadiusSourceSamples"], 6
        )
        self.assertEqual(
            manifest["visualGroundProxy"]["bilinearSafetySourceSamples"], 1
        )
        self.assertLess(
            manifest["sourceAoiMetrics"]["heightErrorSigmaMeters"][
                "validSampleFraction"
            ],
            1.0,
        )
        self.assertFalse(manifest["qualityAdmission"]["authoritativeTerrainAdmitted"])
        self.assertTrue(manifest["qualityAdmission"]["publicVisualFallbackAdmitted"])
        self.assertEqual(manifest["preferredRuntimeSource"], "CESIUM_WORLD_TERRAIN_WHEN_PROVIDER_READY")
        self.assertIn("NOT_DTM", manifest["claimBoundary"])
        self.assertTrue(all(value is False for value in manifest["authorityFlags"].values()))

    def test_derived_receipts_geometry_and_images_are_consistent(self) -> None:
        receipts = {
            entry["file"]: entry for entry in self.manifest["derivedOutputReceipts"]
        }
        self.assertEqual(len(receipts), 5)
        for name, receipt in receipts.items():
            path = GENERATED / name
            self.assertEqual(path.stat().st_size, receipt["bytes"])
            self.assertEqual(sha256(path), receipt["sha256"])

        obj = GENERATED / "SM_IPV5D_CopernicusDEM2021_TerrainProxy_Visual.obj"
        counts = {"v": 0, "vt": 0, "vn": 0, "f": 0}
        horizontal_bounds = {
            "x": [math.inf, -math.inf],
            "y": [math.inf, -math.inf],
        }
        with obj.open("r", encoding="utf-8") as handle:
            for line in handle:
                prefix = line.split(" ", 1)[0]
                if prefix in counts:
                    counts[prefix] += 1
                if prefix == "v":
                    _, x_text, y_text, _ = line.split()
                    x_value = float(x_text)
                    y_value = float(y_text)
                    horizontal_bounds["x"][0] = min(
                        horizontal_bounds["x"][0], x_value
                    )
                    horizontal_bounds["x"][1] = max(
                        horizontal_bounds["x"][1], x_value
                    )
                    horizontal_bounds["y"][0] = min(
                        horizontal_bounds["y"][0], y_value
                    )
                    horizontal_bounds["y"][1] = max(
                        horizontal_bounds["y"][1], y_value
                    )
        self.assertEqual(counts, {"v": 16641, "vt": 16641, "vn": 16641, "f": 32768})
        import_contract = self.manifest["objUnrealImport"]
        self.assertEqual(import_contract["sourceCoordinateUnit"], "metres")
        self.assertEqual(import_contract["requiredUniformScaleToCentimetres"], 100.0)
        self.assertEqual(
            import_contract["expectedHorizontalBoundsCentimetres"],
            [-100000.0, 100000.0],
        )
        self.assertEqual(horizontal_bounds, {"x": [-1000.0, 1000.0], "y": [-1000.0, 1000.0]})
        for axis in ("x", "y"):
            converted = [
                value * import_contract["requiredUniformScaleToCentimetres"]
                for value in horizontal_bounds[axis]
            ]
            self.assertEqual(converted, [-100000.0, 100000.0])
            self.assertEqual(
                import_contract["convertedHorizontalBoundsCentimetres"][axis],
                converted,
            )
        self.assertFalse(import_contract["nativeUnrealImportAppliedAndVerified"])
        obj_header = "\n".join(obj.read_text(encoding="utf-8").splitlines()[:6])
        self.assertIn("OBJ vertex coordinates are metres", obj_header)
        self.assertIn("apply exactly x100 uniformly to centimetres", obj_header)
        self.assertEqual(
            png_header(GENERATED / "T_IPV5D_CopernicusDEM2021_RelativeHeight_R16.png"),
            (257, 257, 16, 0),
        )
        self.assertEqual(
            png_header(GENERATED / "r29_copernicus_dem_2021_aoi_diagnostic.png"),
            (1520, 860, 8, 2),
        )

    def test_sample_table_covers_the_contract_crop_with_finite_values(self) -> None:
        path = GENERATED / "Istana_CopernicusDEM2021_AOI_SourceSamples.csv"
        with path.open("r", encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle))
        expected_count = self.manifest["aoi"]["sourceSampleCount"]
        self.assertEqual(len(rows), expected_count)
        self.assertEqual(
            len({(row["row"], row["column"]) for row in rows}), expected_count
        )
        for row in (rows[0], rows[len(rows) // 2], rows[-1]):
            for key in (
                "longitude_wgs84_degrees",
                "latitude_wgs84_degrees",
                "x_east_metres",
                "y_south_metres",
                "dsm_egm2008_metres",
                "relative_dsm_metres",
                "height_error_sigma_metres",
            ):
                self.assertTrue(math.isfinite(float(row[key])))
        self.assertLess(min(float(row["x_east_metres"]) for row in rows), -1000.0)
        self.assertGreater(max(float(row["x_east_metres"]) for row in rows), 1000.0)
        self.assertLess(min(float(row["y_south_metres"]) for row in rows), -1000.0)
        self.assertGreater(max(float(row["y_south_metres"]) for row in rows), 1000.0)

    def test_insufficient_filter_halo_fails_closed(self) -> None:
        module = load_generator()
        tampered = json.loads(json.dumps(self.contract))
        tampered["aoi"]["sourceMarginMeters"] = 90.0
        with mock.patch.object(
            module,
            "load_dependencies",
            side_effect=AssertionError("optional dependencies must not load"),
        ) as dependency_loader:
            with self.assertRaisesRegex(module.ContractError, "margin is too small"):
                module.analyse_sources(tampered, {})
        dependency_loader.assert_not_called()

        tampered["sourceDirectory"] = str(SOURCES)
        with tempfile.TemporaryDirectory() as temporary:
            tampered_path = Path(temporary) / "insufficient-halo.contract.json"
            tampered_path.write_text(json.dumps(tampered), encoding="utf-8")
            checked = subprocess.run(
                [
                    sys.executable,
                    str(GENERATOR),
                    "--check",
                    "--contract",
                    str(tampered_path),
                ],
                cwd=REPO,
                text=True,
                capture_output=True,
                check=False,
            )
        self.assertEqual(checked.returncode, 1, checked.stdout + checked.stderr)
        self.assertIn("margin is too small", checked.stderr)
        self.assertNotIn("NumPy and Pillow are required", checked.stderr)

    def test_obj_unreal_import_scale_and_bounds_fail_closed(self) -> None:
        module = load_generator()
        axes = [-1000.0, 0.0, 1000.0]
        accepted = module.validate_obj_unreal_import_contract(
            self.contract, axes, axes
        )
        self.assertEqual(
            accepted["sourceSideContractStatus"],
            "PASS_EXACT_X100_TO_PLUS_MINUS_100000_CM",
        )

        wrong_scale = json.loads(json.dumps(self.contract))
        wrong_scale["derived"]["objUnrealImport"][
            "requiredUniformScaleToCentimetres"
        ] = 1.0
        with self.assertRaisesRegex(module.ContractError, "exactly x100"):
            module.validate_obj_unreal_import_contract(wrong_scale, axes, axes)

        with self.assertRaisesRegex(module.ContractError, "expected exact"):
            module.validate_obj_unreal_import_contract(
                self.contract, [-999.0, 999.0], axes
            )

    def test_halo_and_obj_import_boundaries_are_documented_in_both_readmes(self) -> None:
        for path in (PACKAGE / "README.md", GENERATED / "README.md"):
            text = path.read_text(encoding="utf-8")
            self.assertIn("225", text)
            self.assertIn("216.39855", text)
            self.assertIn("metres", text)
            self.assertIn("x100", text)
            self.assertIn("-100000 cm to +100000 cm", text)
            self.assertIn("does not prove", text)

    def test_225m_halo_matches_a_larger_crop_over_the_complete_output(self) -> None:
        if importlib.util.find_spec("numpy") is None or importlib.util.find_spec("PIL") is None:
            self.skipTest("NumPy/Pillow runtime is not active")
        np = importlib.import_module("numpy")
        module = load_generator()
        _, source_paths = module.validate_source_receipts(self.contract, CONTRACT_PATH)
        admitted = module.analyse_sources(self.contract, source_paths)
        larger_contract = json.loads(json.dumps(self.contract))
        larger_contract["aoi"]["sourceMarginMeters"] = 500.0
        larger = module.analyse_sources(larger_contract, source_paths)
        proxy_delta = np.abs(admitted["proxyGrid"] - larger["proxyGrid"])
        raw_delta = np.abs(admitted["rawGrid"] - larger["rawGrid"])
        self.assertLessEqual(float(proxy_delta.max()), 1.0e-12)
        self.assertLessEqual(float(raw_delta.max()), 1.0e-12)

    def test_wrong_source_hash_is_rejected_before_processing(self) -> None:
        module = load_generator()
        tampered = json.loads(json.dumps(self.contract))
        tampered["sourceDirectory"] = str(SOURCES)
        tampered["sources"][0]["sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "tampered.json"
            path.write_text(json.dumps(tampered), encoding="utf-8")
            loaded = module.load_contract(path)
            with self.assertRaisesRegex(module.ContractError, "receipt mismatch"):
                module.validate_source_receipts(loaded, path)

    def test_bundled_dependency_runtime_can_check_and_reproduce_outputs(self) -> None:
        if importlib.util.find_spec("numpy") is None or importlib.util.find_spec("PIL") is None:
            self.skipTest("NumPy/Pillow runtime is not active")
        checked = subprocess.run(
            [sys.executable, str(GENERATOR), "--check"],
            cwd=REPO,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
        self.assertIn("PASS_PUBLIC_DSM_SOURCE_AND_DERIVATIVE_CONSISTENCY_ONLY", checked.stdout)
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "Generated"
            generated = subprocess.run(
                [sys.executable, str(GENERATOR), "--generate", "--output-dir", str(output)],
                cwd=REPO,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(generated.returncode, 0, generated.stdout + generated.stderr)
            self.assertEqual(
                {path.name for path in output.iterdir()},
                {path.name for path in GENERATED.iterdir()},
            )
            for canonical in GENERATED.iterdir():
                self.assertEqual(sha256(output / canonical.name), sha256(canonical))


if __name__ == "__main__":
    unittest.main()
