from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
ACTOR_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentActor.cpp"
)
ACTOR_HEADER = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DR28EnvironmentActor.h"
)


def extract_braced_block(source: str, marker: str) -> str:
    marker_index = source.index(marker)
    open_brace = source.index("{", marker_index)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace + 1 : index]
    raise AssertionError(f"Unterminated C++ block after {marker!r}")


class IstanaExploreV5DR28ActorCompileContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.actor_cpp = ACTOR_CPP.read_text(encoding="utf-8")
        cls.actor_header = ACTOR_HEADER.read_text(encoding="utf-8")

    def test_static_claim_accessor_returns_namespace_constant(self) -> None:
        self.assertIn(
            "const FString ExpectedEnvironmentClaimLabel(",
            self.actor_cpp,
        )
        accessor = extract_braced_block(
            self.actor_cpp,
            "ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedClaimLabel()",
        )
        self.assertIn("return ExpectedEnvironmentClaimLabel;", accessor)
        self.assertNotIn("return ClaimLabel;", accessor)

    def test_instance_claim_property_keeps_the_expected_truth_contract(self) -> None:
        self.assertIn(
            "static const FString& ExpectedClaimLabel();",
            self.actor_header,
        )
        self.assertIn("FString ClaimLabel;", self.actor_header)
        self.assertIn("ClaimLabel = ExpectedClaimLabel();", self.actor_cpp)
        self.assertIn(
            "ClaimLabel != ExpectedClaimLabel() || !bRenderOnly",
            self.actor_cpp,
        )
        self.assertIn(
            "PUBLIC_SOURCE_ALIGNED_RENDER_ONLY_VISUAL_DRESSING_NOT_SURVEY_"
            "AS_BUILT_CURRENT_COMPLETE_SENSOR_OR_RF_AUTHORITY",
            self.actor_cpp,
        )


if __name__ == "__main__":
    unittest.main()
