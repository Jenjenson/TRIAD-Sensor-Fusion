from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
HEADER = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DContextPolicyActor.h"
)
SOURCE = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DContextPolicyActor.cpp"
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
    raise AssertionError(f"unterminated block after {marker!r}")


class IstanaExploreV5DR28ProviderCouplingContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_optional_actor_is_zero_or_one_valid_tagged_cached_instance(self) -> None:
        for fragment in (
            "class ATRIADIstanaExploreV5DR28EnvironmentActor;",
            "ResolveOptionalR28EnvironmentActor(",
            "ValidateOptionalR28EnvironmentCoherence(",
            "RestoreOptionalR28EnvironmentToFallback(",
            "TWeakObjectPtr<ATRIADIstanaExploreV5DR28EnvironmentActor>\n"
            "        CachedR28Environment;",
        ):
            self.assertIn(fragment, self.header)
        self.assertIn(
            '#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"', self.source
        )

        resolver = extract_braced_block(
            self.source,
            "ResolveOptionalR28EnvironmentActor(",
        )
        for fragment in (
            "FindExactlyOne<ATRIADIstanaExploreV5DR28EnvironmentActor>",
            "R28EnvironmentCount > 1",
            "R28EnvironmentCount == 0",
            "permits zero or one R28 render-only environment actor, never more",
            "ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag()",
            "OutEnvironment->ValidateR28Environment(R28Report)",
        ):
            self.assertIn(fragment, resolver)

        begin_play = extract_braced_block(
            self.source,
            "void ATRIADIstanaExploreV5DContextPolicyActor::BeginPlay(",
        )
        self.assertIn(
            "ResolveOptionalR28EnvironmentActor(R28Environment, Error)", begin_play
        )
        self.assertIn("CachedR28Environment = R28Environment;", begin_play)

    def test_provider_transition_is_three_party_atomic_and_failure_restoring(self) -> None:
        transition = extract_braced_block(
            self.source,
            "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
            "    SetProviderReadyPresentation(",
        )
        for fragment in (
            "const bool bPreviousProviderReady = PublicRealm->bProviderReady;",
            "const bool bPreviousLocalFallbackHidden =",
            "const bool bPreviousR28ProviderReady =",
            "PublicRealm->SetProviderReady(bProviderReady, PublicRealmError)",
            "R28Environment->SetProviderReady(\n            bProviderReady,",
            "SetLocalBuildingFallbackVisible(Scene, !bProviderReady);",
            "ValidateOptionalR28EnvironmentCoherence(",
            "bLocalBuildingFallbackCurrentlyHidden",
            "bPreviousR28ProviderReady",
            "bRollbackValidated",
            "every participant was rolled back",
        ):
            self.assertIn(fragment, transition)

        mutate_public = transition.index(
            "PublicRealm->SetProviderReady(bProviderReady, PublicRealmError)"
        )
        mutate_r28 = transition.index(
            "R28Environment->SetProviderReady(\n            bProviderReady,"
        )
        mutate_local = transition.index(
            "SetLocalBuildingFallbackVisible(Scene, !bProviderReady);"
        )
        validate_all = transition.index(
            "ValidateOptionalR28EnvironmentCoherence(", mutate_local
        )
        self.assertLess(mutate_public, mutate_r28)
        self.assertLess(mutate_r28, mutate_local)
        self.assertLess(mutate_local, validate_all)

        rollback_region = transition[validate_all:]
        for fragment in (
            "PublicRealm->SetProviderReady(\n            bPreviousProviderReady",
            "R28Environment->SetProviderReady(\n                bPreviousR28ProviderReady",
            "SetLocalBuildingFallbackVisible(\n            Scene,\n"
            "            !bPreviousLocalFallbackHidden)",
            "ValidateCurrentSurroundingsPresentation(",
            "PublicRealm->ValidatePublicRealm(",
            "ValidateOptionalR28EnvironmentCoherence(",
        ):
            self.assertIn(fragment, rollback_region)

    def test_runtime_validation_keeps_r28_render_only_and_non_authoritative(self) -> None:
        coherence = extract_braced_block(
            self.source,
            "ValidateOptionalR28EnvironmentCoherence(",
        )
        for fragment in (
            "ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_ABSENT optional=true count=0",
            "!Environment->bConfigured",
            "!Environment->bRenderOnly",
            "Environment->bCollisionNavigationSensorOrRfAuthority",
            "Environment->bMeasuredSurveyAsBuiltOrCurrentCompleteClaimed",
            "Environment->bExistingSimulationOrRfInputsModified",
            "Environment->bRuntimeGeometryGenerated",
            "Environment->bProviderReady != bExpectedProviderReady",
            "Environment->ValidateR28Environment(EnvironmentReport)",
            "ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_VALID",
        ):
            self.assertIn(fragment, coherence)

        validation = extract_braced_block(
            self.source,
            "bool ATRIADIstanaExploreV5DContextPolicyActor::ValidateHybridContext(",
        )
        for fragment in (
            "ResolveOptionalR28EnvironmentActor(R28Environment, Error)",
            "ValidateOptionalR28EnvironmentCoherence(",
            "r28EnvironmentOptional=true",
            "r28EnvironmentRenderOnlyWhenPresent=true",
            "r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false",
        ):
            self.assertIn(fragment, validation)

        transition = extract_braced_block(
            self.source,
            "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
            "    SetProviderReadyPresentation(",
        )
        for forbidden in (
            "SetActorEnableCollision",
            "SetCollision",
            "SetCanEverAffectNavigation",
            "CreateDefaultSubobject",
            "SetStaticMesh",
            "Sensor",
            "RfAuthority = true",
        ):
            self.assertNotIn(forbidden, transition)

    def test_ground_restore_and_end_play_force_r28_not_ready(self) -> None:
        restore = extract_braced_block(
            self.source,
            "RestoreOptionalR28EnvironmentToFallback(",
        )
        for fragment in (
            "TActorIterator<ATRIADIstanaExploreV5DR28EnvironmentActor>",
            "Environment->SetProviderReady(false, TransitionError)",
            "ValidateOptionalR28EnvironmentCoherence(",
            "Environments.Num() == 1",
            "Environments.Num() > 1",
            "failed closed; zero or one actor is permitted",
        ):
            self.assertIn(fragment, restore)

        ground = extract_braced_block(
            self.source,
            "RestoreGroundLevelPresentation(",
        )
        restore_r28 = ground.index("RestoreOptionalR28EnvironmentToFallback(")
        resolve_scene = ground.index("ResolveSceneAndTileset(")
        self.assertLess(restore_r28, resolve_scene)
        self.assertIn("!bR28EnvironmentRestored", ground)
        self.assertIn(
            "SetProviderReadyPresentation(\n            Scene,\n            false,", ground
        )

        end_play = extract_braced_block(
            self.source,
            "void ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(",
        )
        for fragment in (
            "R28Environment->SetProviderReady(false, R28EndPlayError)",
            "RestoreOptionalR28EnvironmentToFallback(R28EndPlayRestoreError)",
            "CachedR28Environment.Reset();",
        ):
            self.assertIn(fragment, end_play)


if __name__ == "__main__":
    unittest.main()
