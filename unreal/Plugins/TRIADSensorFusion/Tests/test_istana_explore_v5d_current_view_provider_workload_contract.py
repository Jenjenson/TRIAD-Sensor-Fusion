from __future__ import annotations

import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_H = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h"
)
RUNTIME_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
PROVIDER_DOC = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Docs/"
    "IstanaExploreV5DProviderQuality.md"
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


class IstanaExploreV5DCurrentViewProviderWorkloadContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = RUNTIME_H.read_text(encoding="utf-8")
        cls.runtime_cpp = RUNTIME_CPP.read_text(encoding="utf-8")
        cls.provider_doc = PROVIDER_DOC.read_text(encoding="utf-8")

    def test_state_is_explicit_and_runtime_only(self) -> None:
        for fragment in (
            "bool bTransientCurrentViewProviderWorkloadPolicyApplied = false;",
            "bool bProviderFogCullingSnapshotValid = false;",
            "bool bProviderFogCullingBeforeTransientPolicy = false;",
            "ApplyTransientCurrentViewProviderWorkloadPolicy(",
            "RestoreTransientCurrentViewProviderWorkloadPolicy(",
        ):
            self.assertIn(fragment, self.runtime_h)
        self.assertGreaterEqual(self.runtime_h.count("UPROPERTY(Transient"), 6)

    def test_apply_requires_the_exact_saved_quality_and_authority_tuple(self) -> None:
        apply = extract_braced_block(
            self.runtime_cpp,
            "ApplyTransientCurrentViewProviderWorkloadPolicy(",
        )
        for fragment in (
            "World->IsGameWorld()",
            "HideLocalBuildingFallbackAtLoadProgress",
            "RestoreLocalBuildingFallbackBelowLoadProgress",
            "RequiredProviderReadyConsecutiveSamples",
            "bCesiumLayerIsVisualOnly",
            "bCesiumCollisionNavigationSensorOrRfAuthority",
            "bCesiumStandardPersistentHttpRequestCacheAcknowledged",
            "bTriadReadSerializedOrLoggedProviderToken",
            "bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent",
            "bProviderContentClippedFromAuthoredCore",
            "Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon",
            "Tileset->GetIonAssetID() != ExpectedIonAssetId",
            "Tileset->GetMaximumScreenSpaceError()",
            "RequiredMaximumScreenSpaceError",
            "Tileset->GetCreatePhysicsMeshes()",
            "Tileset->GetCreateNavCollision()",
            "Tileset->MaximumCachedBytes != RequiredProviderCacheBytes",
            "RequiredProviderSimultaneousLoads",
            "Tileset->ForbidHoles",
            "RequiredProviderLoadingDescendantLimit",
            "Tileset->ShowCreditsOnScreen",
            "Tileset->PreloadAncestors",
            "Tileset->PreloadSiblings",
            "Tileset->EnableFrustumCulling",
            "Tileset->EnableFogCulling",
            "Tileset->EnforceCulledScreenSpaceError",
            "RequiredProviderCulledScreenSpaceError",
            "Tileset->GetUseLodTransitions()",
            "Tileset->GetIgnoreKhrMaterialsUnlit()",
            "Tileset->GetGenerateSmoothNormals()",
            "ValidateProviderSiteClip(World, Tileset, SiteClipError)",
        ):
            self.assertIn(fragment, apply)

    def test_apply_mutates_only_runtime_fog_culling_without_refresh(self) -> None:
        apply = extract_braced_block(
            self.runtime_cpp,
            "ApplyTransientCurrentViewProviderWorkloadPolicy(",
        )
        snapshot = apply.index(
            "bProviderFogCullingBeforeTransientPolicy =\n"
            "        Tileset->EnableFogCulling;"
        )
        clip_preflight = apply.index(
            "ValidateProviderSiteClip(World, Tileset, SiteClipError)"
        )
        mutation = apply.index("Tileset->EnableFogCulling = true;", snapshot)
        readback = apply.index(
            "HasExpectedCurrentViewProviderWorkloadState(", mutation
        )
        self.assertLess(clip_preflight, snapshot)
        self.assertLess(snapshot, mutation)
        self.assertLess(mutation, readback)
        self.assertEqual(apply.count("Tileset->EnableFogCulling = true;"), 1)
        self.assertEqual(
            re.findall(r"Tileset->([A-Za-z][A-Za-z0-9_]*)\s*=", apply),
            ["EnableFogCulling", "EnableFogCulling"],
        )
        for forbidden in (
            "Modify(",
            "RefreshTileset(",
            "MarkPackageDirty",
            "SaveMap(",
            "SavePackage(",
            "SetIonAssetID(",
            "SetIonAccessToken(",
            "GetIonAccessToken(",
            "SetTilesetSource(",
            "SetUrl(",
            "SetRequestHeaders(",
            "SetGeoreference(",
            "SetMaximumScreenSpaceError(",
            "SetCreatePhysicsMeshes(",
            "SetCreateNavCollision(",
            "MaximumCachedBytes =",
            "MaximumSimultaneousTileLoads =",
            "LoadingDescendantLimit =",
            "PreloadAncestors =",
            "PreloadSiblings =",
            "ForbidHoles =",
            "EnforceCulledScreenSpaceError =",
            "CulledScreenSpaceError =",
            "SuspendUpdate =",
            "Overlay->Activate(",
            "Overlay->Deactivate(",
        ):
            self.assertNotIn(forbidden, apply)

    def test_begin_play_and_validation_require_the_runtime_readback(self) -> None:
        begin_play = extract_braced_block(
            self.runtime_cpp,
            "ATRIADIstanaExploreV5DContextPolicyActor::BeginPlay()",
        )
        cache = begin_play.index("CachedTileset = Tileset;")
        apply = begin_play.index(
            "ApplyTransientCurrentViewProviderWorkloadPolicy(", cache
        )
        fallback = begin_play.index("RestoreGroundLevelPresentation(Error);")
        self.assertLess(cache, apply)
        self.assertLess(apply, fallback)

        validate = extract_braced_block(
            self.runtime_cpp,
            "ValidateHybridContext(\n    FString& OutReport) const",
        )
        for fragment in (
            "const bool bIsGameWorld = GetWorld()->IsGameWorld();",
            "const bool bExpectedRuntimeFogCulling = bIsGameWorld;",
            "HasExpectedCurrentViewProviderWorkloadState(",
            "Tileset->EnableFogCulling != bExpectedRuntimeFogCulling",
            "!bHasExpectedCurrentViewProviderWorkloadState",
            "savedFogCulling=false",
            "currentFogCulling=%s",
            "transientCurrentViewProviderWorkloadPolicy=%s",
        ):
            self.assertIn(fragment, validate)

    def test_end_play_restores_the_exact_saved_value_before_quit_drain(self) -> None:
        restore = extract_braced_block(
            self.runtime_cpp,
            "RestoreTransientCurrentViewProviderWorkloadPolicy(FString& OutError)",
        )
        self.assertIn(
            "Tileset->EnableFogCulling =\n"
            "        bProviderFogCullingBeforeTransientPolicy;",
            restore,
        )
        self.assertIn("savedFogCulling=false", restore)
        self.assertIn("runtimeMutationPersisted=false", restore)
        for forbidden in (
            "Modify(",
            "RefreshTileset(",
            "MarkPackageDirty",
            "SaveMap(",
            "SavePackage(",
        ):
            self.assertNotIn(forbidden, restore)

        end_play = extract_braced_block(
            self.runtime_cpp,
            "ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(",
        )
        restore_index = end_play.index(
            "RestoreTransientCurrentViewProviderWorkloadPolicy("
        )
        quit_drain = end_play.index(
            "if (EndPlayReason == EEndPlayReason::Quit)"
        )
        self.assertLess(restore_index, quit_drain)

    def test_readiness_and_negative_authority_gates_are_not_weakened(self) -> None:
        tick = extract_braced_block(
            self.runtime_cpp,
            "ATRIADIstanaExploreV5DContextPolicyActor::Tick(float DeltaSeconds)",
        )
        for fragment in (
            "float HideLocalBuildingFallbackAtLoadProgress = 98.0f;",
            "float RestoreLocalBuildingFallbackBelowLoadProgress = 90.0f;",
            "int32 RequiredProviderReadyConsecutiveSamples = 3;",
            "bool bCesiumLayerIsVisualOnly = true;",
            "bool bCesiumCollisionNavigationSensorOrRfAuthority = false;",
        ):
            self.assertIn(fragment, self.runtime_h)
        for fragment in (
            "LoadProgress >= HideLocalBuildingFallbackAtLoadProgress",
            "NextProviderReadyConsecutiveSamples(",
            "RequiredProviderReadyConsecutiveSamples);",
            "ShouldRestoreLocalBuildingFallback(",
        ):
            self.assertIn(fragment, tick)
        apply = extract_braced_block(
            self.runtime_cpp,
            "ApplyTransientCurrentViewProviderWorkloadPolicy(",
        )
        for forbidden in (
            "bCesiumLayerIsVisualOnly =",
            "bCesiumCollisionNavigationSensorOrRfAuthority =",
            "bProviderContentClippedFromAuthoredCore =",
            "HideLocalBuildingFallbackAtLoadProgress =",
            "RestoreLocalBuildingFallbackBelowLoadProgress =",
            "RequiredProviderReadyConsecutiveSamples =",
        ):
            self.assertNotIn(forbidden, apply)

    def test_native_state_matrix_and_audit_are_present(self) -> None:
        for fragment in (
            "TRIAD.Istana.ExploreV5D.Hybrid.CurrentViewProviderWorkloadState",
            "A cold editor world retains the exact saved fog-culling state",
            "A game world requires the exact transient current-view state",
            "A game world rejects an unapplied workload policy",
            "A game world rejects a noncanonical saved fog-culling value",
            "A cold editor world rejects leaked runtime state",
            "No transient current-view workload state is serialized",
        ):
            self.assertIn(fragment, self.runtime_cpp)
        normalized_doc = " ".join(self.provider_doc.split())
        for fragment in (
            "## Current-view provider workload isolation",
            "repeated HTTP `429` responses",
            "peaked near 60.6 percent and ended at 47.254 percent",
            "one setting was the sole cause of that second result",
            "`EnableFogCulling=false`",
            "`EnforceCulledScreenSpaceError=true`",
            "`CulledScreenSpaceError=8.0`",
            "cesium-unreal/blob/main/Source/CesiumRuntime/Private/Cesium3DTileset.cpp",
            "copies `EnableFogCulling` into the native tileset options during each tick",
            "does not call `Modify`, `RefreshTileset`",
            "does not lower the 98-percent, three-consecutive-sample readiness gate",
            "zero collision, navigation, terrain, sensor-occlusion, or RF authority",
        ):
            self.assertIn(" ".join(fragment.split()), normalized_doc)


if __name__ == "__main__":
    unittest.main()
