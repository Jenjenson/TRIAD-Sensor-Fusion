import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = ROOT / "distribution" / "github-free-release-bundles.json"
BUILDER_PATH = ROOT / "scripts" / "Build-GitHubFreeReleaseBundles.ps1"
PORTABLE_ENGINE_CONFIG = (
    ROOT
    / "distribution"
    / "portable-unreal-project"
    / "Config"
    / "DefaultEngine.ini"
)


class GitHubReleaseBundleSecurityTests(unittest.TestCase):
    def test_native_credential_bearing_paths_are_not_bundled(self) -> None:
        configuration = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        native = next(
            bundle
            for bundle in configuration["bundles"]
            if bundle["name"] == "unreal-project-owned"
        )

        self.assertNotIn("Content/CesiumSettings", native["includes"])
        self.assertIn("Config/DefaultEngine.ini", native["excludes"])

    def test_portable_engine_config_replaces_native_config_without_tokens(self) -> None:
        configuration = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        portable = next(
            bundle
            for bundle in configuration["bundles"]
            if bundle["name"] == "portable-default-engine-config"
        )
        text = PORTABLE_ENGINE_CONFIG.read_text(encoding="utf-8")

        self.assertEqual(portable["source"], "repository")
        self.assertEqual(portable["destination"], "unreal-project")
        self.assertEqual(portable["includes"], ["Config/DefaultEngine.ini"])
        self.assertNotIn("SecurityToken=", text)
        self.assertNotIn("AccessToken=", text)
        self.assertIn("D3D12TargetedShaderFormats=PCD3D_SM6", text)

    def test_builder_fails_closed_if_native_paths_are_reintroduced(self) -> None:
        script = BUILDER_PATH.read_text(encoding="utf-8")

        self.assertIn("'Config/DefaultEngine.ini'", script)
        self.assertIn("'Content/CesiumSettings/*'", script)
        self.assertIn("Credential-bearing native path", script)


if __name__ == "__main__":
    unittest.main()
