from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
SOURCE = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Terrain"
    / "R33CesiumWorldTerrainReference"
)
CONTRACT = SOURCE / "r33_cesium_world_terrain_reference.contract.json"
README = SOURCE / "README.md"
ACTOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Public"
    / "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h"
)
ACTOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.cpp"
)
CONTEXT_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Public"
    / "TRIADIstanaExploreV5DContextPolicyActor.h"
)
CONTEXT_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
GROUND_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DGroundVegetationActor.cpp"
)
R29_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp"
)
EDITOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.cpp"
)

EXPECTED_STATES = (
    "GooglePrimary",
    "CwtWarming",
    "CwtPresented",
    "SafeLocal",
)
EXPECTED_ROLE_ASSETS = {
    "GooglePhotorealisticVisual": 2_275_207,
    "CesiumWorldTerrainVisualReference": 1,
}
EXPECTED_STATE_TUPLES = {
    "googlePrimary": (True, False, False, True),
    "cwtWarming": (True, False, False, False),
    "cwtPresented": (False, True, True, False),
    "safeLocal": (False, True, False, True),
}


def function_region(source: str, start_marker: str, next_markers: tuple[str, ...]) -> str:
    start = source.find(start_marker)
    if start < 0:
        raise AssertionError(f"missing function start marker: {start_marker}")
    ends = [
        source.find(marker, start + len(start_marker))
        for marker in next_markers
    ]
    ends = [end for end in ends if end >= 0]
    return source[start : min(ends) if ends else len(source)]


def has_non_null_shared_server_guard(
    source: str, google_expression: str, cwt_expression: str
) -> bool:
    compact = re.sub(r"\s+", "", source)
    google = f"!IsValid({google_expression}->GetCesiumIonServer())"
    cwt = f"!IsValid({cwt_expression}->GetCesiumIonServer())"
    identity = (
        f"{google_expression}->GetCesiumIonServer()!="
        f"{cwt_expression}->GetCesiumIonServer()"
    )
    reverse_identity = (
        f"{cwt_expression}->GetCesiumIonServer()!="
        f"{google_expression}->GetCesiumIonServer()"
    )
    return google in compact and cwt in compact and (
        identity in compact or reverse_identity in compact
    )


class R33CesiumWorldTerrainReferenceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (
            CONTRACT,
            README,
            ACTOR_H,
            ACTOR_CPP,
            CONTEXT_H,
            CONTEXT_CPP,
            GROUND_CPP,
            R29_CPP,
            EDITOR_CPP,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R33 contract input: {path}")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.readme = README.read_text(encoding="utf-8")
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor_cpp = ACTOR_CPP.read_text(encoding="utf-8")
        cls.context_h = CONTEXT_H.read_text(encoding="utf-8")
        cls.context_cpp = CONTEXT_CPP.read_text(encoding="utf-8")
        cls.ground_cpp = GROUND_CPP.read_text(encoding="utf-8")
        cls.r29_cpp = R29_CPP.read_text(encoding="utf-8")
        cls.editor_cpp = EDITOR_CPP.read_text(encoding="utf-8")

    def test_contract_is_source_only_and_cannot_claim_native_acceptance(self) -> None:
        contract = self.contract
        self.assertEqual(
            "triad.istana_explore_v5d.r33_cesium_world_terrain_reference.v1",
            contract["schema"],
        )
        self.assertEqual("R33", contract["revision"])
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
            contract["targetMap"],
        )
        self.assertEqual(
            "VISUAL_REFERENCE_ONLY_R33_CESIUM_WORLD_TERRAIN",
            contract["claim"],
        )

        state = contract["deliveryState"]
        self.assertTrue(state["sourceImplementationComplete"])
        self.assertTrue(state["captureRevalidationRequired"])
        for name in (
            "nativeProjectApplied",
            "targetMapMutated",
            "unrealEditorLaunchedForThisDelivery",
            "nativeCaptureProduced",
            "visualCaptureAccepted",
            "hiddenStreamingVerifiedInPie",
            "nativeExecutionEligibleNow",
        ):
            self.assertFalse(state[name], name)

        runtime = contract["runtimeActor"]
        self.assertTrue(runtime["sourceOnly"])
        self.assertTrue(runtime["configureEditorWorldOnly"])
        self.assertTrue(runtime["configureRejectsGameNonEditorAndBegunPlayWorlds"])
        self.assertTrue(runtime["runtimeRegistrationOwnedByBeginPlay"])
        self.assertTrue(runtime["sharedClipPreflightBeforeProviderMutation"])
        self.assertTrue(runtime["postMutationFailureRestoresInputTagsAndCwtTuple"])
        self.assertTrue(runtime["postMutationFailureRestoresGoogleFogCulling"])
        self.assertTrue(
            runtime[
                "postMutationFailureRestoresControllerBindingsCollisionTruthAndRuntimeState"
            ]
        )
        self.assertTrue(runtime["configureDoesNotOwnRuntimeGoogleFogPolicy"])
        self.assertTrue(runtime["partialRoleFailureHidesAndSuspendsSurvivingRole"])
        self.assertFalse(runtime["mapPersistenceWrites"])
        self.assertFalse(runtime["heightSamplingApisCalled"])

    def test_configuration_is_transactional_and_partial_loss_fails_closed(self) -> None:
        configure = function_region(
            self.actor_cpp,
            "ConfigureR33CesiumWorldTerrainReference(",
            ("ValidateR33CesiumWorldTerrainReference(",),
        )
        self.assertLess(
            configure.find("ValidateSharedProviderSiteClip(OutError)"),
            configure.find("InGoogleTileset->Tags.AddUnique("),
        )
        for token in (
            "const TArray<FName> GoogleTagsBefore",
            "const TArray<FName> CwtTagsBefore",
            "const auto RestoreInputState",
            "InGoogleTileset->Tags = GoogleTagsBefore",
            "InCwtTileset->Tags = CwtTagsBefore",
            "InCwtTileset->SetTilesetSource(CwtSourceBefore)",
            "InCwtTileset->SetIonAssetID(CwtIonAssetIdBefore)",
            "InCwtTileset->SetGeoreference(CwtGeoreferenceBefore)",
            "InCwtTileset->SetCesiumIonServer(CwtIonServerBefore)",
            "InCwtTileset->SetMaximumScreenSpaceError(CwtMaximumSseBefore)",
            "const bool bGoogleEnableFogCullingBefore",
            "InGoogleTileset->EnableFogCulling = bGoogleEnableFogCullingBefore",
            "const auto RestoreControllerState",
            "SetActorEnableCollision(bControllerCollisionBefore)",
            "bVisualReferenceOnly = bVisualReferenceOnlyBefore",
            "bCollisionNavigationLineOfSightSensorOrRfAuthority =",
            "PresentationState = PresentationStateBefore",
            "CwtRetryBackoffRemainingSecondsBefore",
            "CwtWarmingElapsedSecondsBefore",
            "RestoreInputState();",
            "RestoreControllerState();",
            "!IsValid(InGoogleTileset)",
            "!IsValid(InCwtTileset)",
            "InCwtTileset->Tags.Contains(GoogleRoleTag)",
            "World->IsGameWorld()",
            "World->WorldType != EWorldType::Editor",
            "HasActorBegunPlay()",
        ):
            self.assertIn(token, configure)
        self.assertGreaterEqual(configure.count("RestoreInputState();"), 3)
        self.assertGreaterEqual(configure.count("RestoreControllerState();"), 4)
        self.assertIn("ApplyR33SurvivingRolesSafeLocal(", self.actor_cpp)
        self.assertIn(
            "Configuration is transactional.",
            self.readme,
        )

    def test_contract_requires_exact_google_and_cwt_roles_only(self) -> None:
        roster = self.contract["tilesetRoster"]
        self.assertEqual(2, roster["exactTotalTilesets"])
        self.assertEqual(2, roster["exactRoleCount"])
        self.assertEqual(
            "TRIADIstanaExploreV5DR33CesiumContextMember",
            roster["commonMemberTag"],
        )
        self.assertEqual(2, roster["exactCommonMemberTagCount"])
        self.assertTrue(roster["bothRolesRequireCommonMemberTag"])
        self.assertFalse(roster["additionalTilesetsAllowed"])
        self.assertEqual([2], roster["forbiddenIonAssetIds"])
        self.assertFalse(roster["bingMapsAerialRoleAllowed"])
        self.assertTrue(roster["rolesMustBeDistinctActors"])
        self.assertTrue(roster["rolesMustShareControllerWorld"])
        self.assertTrue(roster["classificationByExactIonAssetIdAndTag"])
        self.assertTrue(roster["r33ConfigureMayAddOnlyCommonMemberTagToGoogle"])
        self.assertTrue(roster["duplicateOrUnknownRoleFailsClosed"])

        roles = roster["roles"]
        self.assertEqual(2, len(roles))
        self.assertEqual(EXPECTED_ROLE_ASSETS, {
            role["role"]: role["ionAssetId"] for role in roles
        })
        self.assertEqual(
            ["FromCesiumIon", "FromCesiumIon"],
            [role["tilesetSource"] for role in roles],
        )
        self.assertEqual([1.0, 8.0], [
            role["maximumScreenSpaceError"] for role in roles
        ])
        self.assertEqual(
            [
                "TRIADIstanaExploreV5DVisualTileset",
                "TRIADIstanaExploreV5DR33CesiumWorldTerrainTileset",
            ],
            [role["actorTag"] for role in roles],
        )
        self.assertTrue(all(role["visualOnly"] for role in roles))

    def test_contract_binds_both_tilesets_to_the_sole_georeference(self) -> None:
        georef = self.contract["sharedGeoreference"]
        self.assertEqual(1, georef["exactGeoreferenceActors"])
        self.assertTrue(georef["bothTilesetsBindSameSoleGeoreference"])
        self.assertTrue(georef["pointerIdentityRequiredAfterResolution"])
        self.assertTrue(georef["tilesetActorTransformsIdentityRequired"])
        self.assertTrue(georef["controllerActorTransformIdentityRequired"])
        self.assertAlmostEqual(103.84288055, georef["originLongitudeDegrees"])
        self.assertAlmostEqual(1.30709615, georef["originLatitudeDegrees"])
        self.assertEqual(47.0, georef["configuredOriginHeightMeters"])
        self.assertFalse(georef["originHeightDatumAssertedByR33"])
        self.assertFalse(georef["verticalDatumConversionImplemented"])
        self.assertTrue(georef["georeferenceIsAlignmentAnchorNotSurveyProof"])

        server = self.contract["sharedIonServer"]
        self.assertTrue(server["bothRolesUseSameIonServer"])
        self.assertTrue(server["pointerIdentityRequiredAfterConfiguration"])
        self.assertTrue(server["exactNonNullOpaqueServerRequired"])
        self.assertTrue(server["cwtCopiesOpaqueServerReferenceFromGoogle"])
        self.assertFalse(server["googleServerBindingChangedByR33"])
        self.assertFalse(server["serverObjectValidityProvesTokenPresence"])
        self.assertFalse(server["serverObjectValidityProvesAssetEntitlement"])
        self.assertFalse(server["ionTokenReadByTriad"])
        self.assertFalse(server["ionTokenSerializedOrLoggedByTriad"])
        self.assertFalse(server["ionTokenValueOrFingerprintRecordedByTriad"])
        self.assertTrue(
            server[
                "runtimeResolverConfigureValidateApplyAndReadinessBoundariesRequireNonNullServer"
            ]
        )
        self.assertTrue(
            server[
                "contextResolverRegisterApplyAndValidateBoundariesRequireNonNullServer"
            ]
        )
        self.assertTrue(
            server[
                "editorResolverPredecessorApplyAndSuccessorValidationRequireNonNullServer"
            ]
        )

    def test_every_r33_validation_layer_rejects_a_null_opaque_ion_server(self) -> None:
        for source in (self.actor_cpp, self.context_cpp, self.editor_cpp):
            self.assertIn('#include "CesiumIonServer.h"', source)
            self.assertNotIn("GetIonAccessToken(", source)
            self.assertNotIn("DefaultIonAccessToken", source)

        actor_boundaries = (
            (
                function_region(
                    self.actor_cpp,
                    "ResolveExactWorldRoster(",
                    ("ValidateSharedProviderSiteClip(",),
                ),
                "OutGoogleTileset",
                "OutCwtTileset",
            ),
            (
                function_region(
                    self.actor_cpp,
                    "ApplyState(",
                    ("EnterGooglePrimary(",),
                ),
                "GoogleTileset",
                "CwtTileset",
            ),
            (
                function_region(
                    self.actor_cpp,
                    "ConfigureR33CesiumWorldTerrainReference(",
                    ("ValidateR33CesiumWorldTerrainReference(",),
                ),
                "InGoogleTileset",
                "InCwtTileset",
            ),
            (
                function_region(
                    self.actor_cpp,
                    "ValidateR33CesiumWorldTerrainReference(",
                    ("RequestCwtPresentation(",),
                ),
                "GoogleTileset",
                "CwtTileset",
            ),
        )
        context_boundaries = (
            (
                function_region(
                    self.context_cpp,
                    "ResolveSceneAndR33Tilesets(",
                    ("ResolvePublicRealmActor(",),
                ),
                "OutGoogleTileset",
                "OutCwtTileset",
            ),
            (
                function_region(
                    self.context_cpp,
                    "RegisterR33CesiumWorldTerrainController(",
                    ("UnregisterR33CesiumWorldTerrainController(",),
                ),
                "GoogleTileset",
                "CwtTileset",
            ),
            (
                function_region(
                    self.context_cpp,
                    "ApplyR33CesiumPresentationMode(",
                    ("ValidateR33CesiumPresentationBinding(",),
                ),
                "GoogleTileset",
                "CwtTileset",
            ),
            (
                function_region(
                    self.context_cpp,
                    "ValidateR33CesiumPresentationBinding(",
                    ("ShouldHideSourceTerrainRendererForVisualContext(",),
                ),
                "GoogleTileset",
                "CwtTileset",
            ),
            (
                function_region(
                    self.context_cpp,
                    "ValidateHybridContext(",
                    ("void ATRIADIstanaExploreV5DContextPolicyActor::BeginPlay()",),
                ),
                "Tileset",
                "R33CwtTileset",
            ),
        )
        editor_boundaries = (
            (
                function_region(
                    self.editor_cpp,
                    "R33NativeResolveWorldRoster(",
                    ("R33NativeOverlayMatches(",),
                ),
                "OutRoster.Google",
                "OutRoster.Cwt",
            ),
            (
                function_region(
                    self.editor_cpp,
                    "R33NativeValidateSuccessorWorld(",
                    ("R33NativeUndoAndValidateR32(",),
                ),
                "OutRoster.Google",
                "OutRoster.Cwt",
            ),
        )
        for region, google, cwt in (
            *actor_boundaries,
            *context_boundaries,
            *editor_boundaries,
        ):
            with self.subTest(google=google, cwt=cwt):
                self.assertTrue(
                    has_non_null_shared_server_guard(region, google, cwt)
                )

        predecessor = function_region(
            self.editor_cpp,
            "R33NativeValidateR32Predecessor(",
            ("R33NativeValidateSuccessorWorld(",),
        )
        apply_endpoint = function_region(
            self.editor_cpp,
            "ApplyR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap(",
            ("ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap(",),
        )
        self.assertIn(
            "!IsValid(OutRoster.Google->GetCesiumIonServer())", predecessor
        )
        self.assertIn(
            "!IsValid(Predecessor.Google->GetCesiumIonServer())", apply_endpoint
        )

        runtime = self.contract["runtimeActor"]
        self.assertTrue(runtime["configurationRejectsNullOrInvalidOpaqueIonServer"])
        self.assertTrue(runtime["validationRejectsNullOrInvalidOpaqueIonServer"])
        self.assertTrue(runtime["presentationApplyRejectsNullOrInvalidOpaqueIonServer"])
        self.assertTrue(
            runtime["readinessRequestAndTickRejectNullOrInvalidOpaqueIonServer"]
        )

    def test_null_null_pointer_equality_mutation_cannot_satisfy_source_guard(self) -> None:
        resolver = function_region(
            self.actor_cpp,
            "ResolveExactWorldRoster(",
            ("ValidateSharedProviderSiteClip(",),
        )
        self.assertTrue(
            has_non_null_shared_server_guard(
                resolver, "OutGoogleTileset", "OutCwtTileset"
            )
        )
        compact = re.sub(r"\s+", "", resolver)
        equality_only_mutation = compact.replace(
            "!IsValid(OutGoogleTileset->GetCesiumIonServer())||", "", 1
        ).replace(
            "!IsValid(OutCwtTileset->GetCesiumIonServer())||", "", 1
        )
        self.assertIn(
            "OutGoogleTileset->GetCesiumIonServer()!=OutCwtTileset->GetCesiumIonServer()",
            equality_only_mutation,
        )
        self.assertFalse(
            has_non_null_shared_server_guard(
                equality_only_mutation, "OutGoogleTileset", "OutCwtTileset"
            )
        )
        null_google_valid = False
        null_cwt_valid = False
        null_pointers_compare_equal = True
        self.assertFalse(
            null_google_valid
            and null_cwt_valid
            and null_pointers_compare_equal
        )

    def test_cwt_visual_tuple_is_hidden_suspended_and_non_colliding(self) -> None:
        cwt = self.contract["worldTerrainVisualTuple"]
        self.assertEqual(1, cwt["ionAssetId"])
        self.assertEqual("FromCesiumIon", cwt["tilesetSource"])
        self.assertEqual(8.0, cwt["maximumScreenSpaceError"])
        self.assertIn("one eighth", cwt["maximumScreenSpaceErrorRationale"])
        self.assertEqual("No", cwt["applyDpiScaling"])
        for name in (
            "createPhysicsMeshes",
            "createNavCollision",
            "actorCollisionEnabled",
            "initialVisible",
        ):
            self.assertFalse(cwt[name], name)
        for name in (
            "showCreditsOnScreen",
            "initialHiddenInGame",
            "initialSuspendUpdate",
            "refreshAfterInitialConfiguration",
            "opaqueClippingMaterialRequiredNonNull",
            "translucentClippingMaterialRequiredNonNull",
            "opaqueClippingMaterialSharedWithGoogle",
            "translucentClippingMaterialSharedWithGoogle",
            "noHeightSampling",
            "noPersistedHeightSamples",
            "noDerivedTerrainBakeOrExport",
        ):
            self.assertTrue(cwt[name], name)
        self.assertEqual(0, cwt["imageryRasterOverlayCount"])
        self.assertFalse(cwt["imageryDraped"])
        self.assertFalse(cwt["photorealSurfaceClaimed"])
        self.assertEqual("GooglePrimary", cwt["initialState"])

        google = self.contract["googleVisualTuple"]
        self.assertEqual(2_275_207, google["ionAssetId"])
        self.assertEqual(1.0, google["maximumScreenSpaceError"])
        self.assertEqual("No", google["applyDpiScaling"])
        self.assertFalse(google["createPhysicsMeshes"])
        self.assertFalse(google["createNavCollision"])
        self.assertTrue(google["showCreditsOnScreen"])
        self.assertTrue(google["configurationOwnedByExistingContextPolicy"])
        self.assertFalse(google["r33MayChangeAssetOrQualityTuple"])

    def test_state_machine_truth_table_never_double_renders(self) -> None:
        machine = self.contract["presentationStateMachine"]
        self.assertEqual(
            "ETRIADIstanaExploreV5DR33TerrainPresentationState",
            machine["enum"],
        )
        self.assertEqual(list(EXPECTED_STATES), machine["states"])
        self.assertEqual("GooglePrimary", machine["initialState"])
        self.assertTrue(machine["neverDoubleVisible"])
        self.assertTrue(machine["hideCurrentProviderBeforeShowingNext"])
        self.assertTrue(machine["noOverlappingGroundSurfaces"])
        self.assertTrue(machine["failedValidationEntersSafeLocal"])
        self.assertTrue(machine["stateTransitionsPersistNoMapMutation"])

        for key, expected in EXPECTED_STATE_TUPLES.items():
            state = machine[key]
            actual = (
                state["googleVisible"],
                state["googleSuspendUpdate"],
                state["cwtVisible"],
                state["cwtSuspendUpdate"],
            )
            self.assertEqual(expected, actual, key)
            self.assertFalse(
                state["googleVisible"] and state["cwtVisible"],
                f"double-visible state: {key}",
            )
        self.assertTrue(machine["cwtWarming"]["hiddenStreamingIsUnverifiedUntilNativePie"])
        google = machine["googlePrimary"]
        self.assertIn("readiness hysteresis", google["terrainRendererPolicy"])
        self.assertIn("readiness gate", google["localSurroundingBuildingsPolicy"])
        warming = machine["cwtWarming"]
        self.assertIn("readiness hysteresis", warming["terrainRendererPolicy"])
        self.assertIn("readiness gate", warming["localSurroundingBuildingsPolicy"])
        presented = machine["cwtPresented"]
        self.assertIn(
            "hides both R29 DEM and original source renderer",
            presented["terrainRendererPolicy"],
        )
        self.assertIn(
            "local current-surroundings buildings",
            presented["localSurroundingBuildingsPolicy"],
        )
        self.assertFalse(presented["outerGroundLoadingFallbackVisible"])
        safe_local = machine["safeLocal"]["terrainRendererPolicy"]
        self.assertIn("shows valid R29 DEM", safe_local)
        self.assertIn("if R29 validation fails", safe_local)
        self.assertIn("shows original source renderer", safe_local)
        self.assertIn("source collision stays enabled", safe_local)
        self.assertTrue(machine["safeLocal"]["outerGroundLoadingFallbackVisible"])
        self.assertFalse(machine["terrainRendererPolicyOwnedByR33Actor"])
        self.assertTrue(
            machine["terrainRendererPolicyOwnedByContextPolicyConsumer"]
        )

        integration = self.contract["contextPolicyIntegration"]
        for name in (
            "explicitR33RegistrationGateRequired",
            "legacyOneTilesetResolverUnchangedWhenGateIsFalse",
            "r33ExactDualTilesetResolverUsedWhenGateIsTrue",
            "localSurroundingBuildingsDecoupledFromOuterGround",
            "cwtPresentedKeepsLocalSurroundingBuildingsVisible",
            "cwtPresentedKeepsPublicRealmAndR28EnvironmentVisible",
            "cwtPresentedHidesOuterGroundLoadingFallback",
            "safeLocalRestoresOuterGroundLoadingFallback",
            "googlePrimaryAndCwtWarmingRetainExistingReadinessHysteresis",
            "groundVegetationConsumesStateAwareSourceRendererPolicy",
            "r29ConsumesStateAwareFallbackPolicy",
            "streamedTilesetStateAndLocalFallbackStateValidatedTogether",
            "dualProviderQuitDrainUsesOneGlobalHttpFlush",
        ):
            self.assertTrue(integration[name], name)

    def test_readiness_is_hysteretic_but_not_accuracy_evidence(self) -> None:
        readiness = self.contract["readinessPolicy"]
        self.assertEqual(0.5, readiness["sampleIntervalSeconds"])
        self.assertEqual(98.0, readiness["presentAtOrAboveLoadProgressPercent"])
        self.assertEqual(3, readiness["presentRequiredConsecutiveSamples"])
        self.assertEqual(90.0, readiness["restoreBelowLoadProgressPercent"])
        self.assertEqual(2, readiness["restoreRequiredConsecutiveSamples"])
        self.assertTrue(readiness["hysteresisEnabled"])
        self.assertTrue(readiness["countersResetOnDirectionChange"])
        self.assertTrue(readiness["presentedViewRegressionReturnsToWarming"])
        self.assertFalse(readiness["presentedViewRegressionEntersSafeLocal"])
        self.assertFalse(readiness["presentedViewRegressionArmsRetryCooldown"])
        self.assertTrue(readiness["loadProgressMeansCurrentViewCompletionOnly"])
        self.assertTrue(readiness["entitlementOrLoadFailureEntersSafeLocal"])
        self.assertTrue(readiness["cesiumLoadFailureDelegateSubscribedDuringPlay"])
        self.assertTrue(readiness["loadFailureFilteredToExactSameWorldCwtRole"])
        self.assertTrue(readiness["loadFailureRequiresActiveCwtRequest"])
        self.assertTrue(readiness["firstMatchingFailureEdgeLatched"])
        self.assertFalse(readiness["duplicateFailureExtendsRetryCooldown"])
        self.assertFalse(readiness["providerFailureMessageLogged"])
        self.assertEqual(10.0, readiness["retryCooldownSeconds"])
        self.assertEqual(30.0, readiness["warmingTimeoutSeconds"])
        for name in (
            "safeLocalRetryRequiresCooldownExpiry",
            "freshExplicitRequestRefreshesCwtBeforeWarming",
            "retryRefreshesCwtOnceBeforeWarming",
            "silentWarmingTimeoutEntersSafeLocal",
            "allRequestedCwtFailClosedTransitionsArmCooldown",
            "repeatTrueRequestDuringCooldownAcceptedAndDeferred",
            "explicitFalseRequestClearsRetryLatch",
            "delayedFailureAfterRequestCancellationIgnored",
            "http401And403DisableAutomaticRetry",
            "entitlementRetryRequiresFreshOperatorRequest",
            "loadFailureDelegateRemovedBeforeEndPlayTeardown",
        ):
            self.assertTrue(readiness[name], name)
        for name in (
            "loadProgressProvesGeographicCoverage",
            "loadProgressProvesHeightAccuracy",
            "loadProgressProvesNoHoles",
        ):
            self.assertFalse(readiness[name], name)

    def test_source_terrain_keeps_all_simulation_authority(self) -> None:
        authority = self.contract["simulationAuthority"]
        for name in (
            "existingSourceTerrainCollisionRemainsEnabledInEveryState",
            "originalSourceTerrainRendererMayBeHiddenWhileCollisionRemainsEnabled",
            "r29DemIsLocalLoadingOrSafeLocalVisualFallbackOnly",
            "existingSourceTerrainIsSoleCollisionAuthority",
            "existingSourceTerrainIsSoleLineOfSightAuthority",
            "existingSourceTerrainIsSoleRfGeometryAuthority",
            "existingSourceTerrainIsSoleSensorOcclusionAuthority",
            "contextPolicyConsumerMayChangeR29AndSourceTerrainRendererVisibility",
            "stateMachineNeverChangesExistingSourceTerrainCollisionOrQuerySettings",
        ):
            self.assertTrue(authority[name], name)
        self.assertFalse(authority["r29DemHasCollisionLineOfSightRfOrSensorAuthority"])
        for name, value in authority.items():
            if name.startswith("worldTerrainTileset") or name.startswith(
                "googleTileset"
            ) or name in (
                "stateMachineMayMutateSourceTerrain",
                "stateMachineMayRebindSimulationQueries",
                "r33ActorMayMutateR29OrSourceTerrainRendererVisibility",
            ):
                self.assertFalse(value, name)

    def test_height_privacy_and_claim_boundaries_fail_closed(self) -> None:
        height = self.contract["heightAndAccuracyBoundary"]
        self.assertIn("ellipsoid", height["worldTerrainNativeHeightReference"])
        self.assertIn("not established", height["configuredLocalOriginHeightReference"])
        self.assertTrue(height["noHeightSamplesSerializedLoggedOrExported"])
        for name in (
            "ellipsoidAndOrthometricHeightsInterchangeable",
            "egm2008OrEpsg3855ConversionImplemented",
            "surveyControlPointsUsed",
            "checkpointResidualsMeasured",
            "terrainAccuracyValidatedAtIstana",
            "surveyGradeClaimed",
            "engineeringGradeClaimed",
            "asBuiltClaimed",
            "currentTerrainClaimed",
            "centimeterAccuracyClaimed",
        ):
            self.assertFalse(height[name], name)

        privacy = self.contract["privacyAndProviderBoundary"]
        self.assertFalse(privacy["triadReadsSerializesOrLogsIonToken"])
        self.assertFalse(privacy["triadRecordsIonTokenValueOrFingerprint"])
        self.assertFalse(privacy["nonNullIonServerProvesTokenOrEntitlement"])
        self.assertTrue(privacy["standardCesiumPersistentHttpCacheAcknowledged"])
        self.assertFalse(privacy["providerGeometryExportedBakedOrDerived"])
        self.assertTrue(privacy["providerTermsAndEntitlementMustBeVerifiedAtNativeGate"])
        self.assertTrue(privacy["creditsMustRemainVisible"])

        truth = self.contract["truthBoundary"]
        self.assertTrue(truth["appearanceOnly"])
        self.assertTrue(truth["visualReferenceOnly"])
        self.assertTrue(truth["visualAcceptanceOwnedByExternalReceiptOnly"])
        for name, value in truth.items():
            if name not in (
                "appearanceOnly",
                "visualReferenceOnly",
                "visualAcceptanceOwnedByExternalReceiptOnly",
            ):
                self.assertFalse(value, name)

    def test_native_order_is_strictly_r30_r31_r32_then_r33(self) -> None:
        order = self.contract["nativeOrdering"]
        promotion = order["promotionOrder"]
        offsets = [promotion.index(name) for name in ("R30", "R31", "R32", "R33")]
        self.assertEqual(sorted(offsets), offsets)
        for name in (
            "r30CommittedReceiptRequired",
            "r30AcceptedFivePosePlayer0CaptureRequired",
            "r30ExplicitHumanReviewReceiptRequired",
            "r31CommittedReceiptRequired",
            "r31AcceptedNativeCaptureRequired",
            "r31ExplicitHumanReviewReceiptRequired",
            "r32CommittedReceiptRequired",
            "r32AcceptedNativeCaptureRequired",
            "r32ExplicitHumanReviewReceiptRequired",
            "nativeTransactionImplemented",
        ):
            self.assertTrue(order[name], name)
        for name in (
            "r33MayBypassR30R31OrR32",
            "prerequisitesSatisfiedForThisDelivery",
            "nativeTransactionExecuted",
        ):
            self.assertFalse(order[name], name)

    def test_actor_public_api_enum_and_constants_match_contract(self) -> None:
        for token in (
            "enum class ETRIADIstanaExploreV5DR33TerrainPresentationState",
            *EXPECTED_STATES,
            "ConfigureR33CesiumWorldTerrainReference(",
            "ValidateR33CesiumWorldTerrainReference(",
            "RequestCwtPresentation(",
            "TickPresentationState(",
            "EnterGooglePrimary(",
            "EnterCwtWarming(",
            "EnterCwtPresented(",
            "EnterSafeLocal(",
        ):
            self.assertIn(token, self.actor_h)
        self.assertRegex(
            self.actor_h,
            r"class\s+TRIADSENSORFUSION_API\s+"
            r"ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor",
        )

        expected_cpp_tokens = (
            "constexpr int64 GooglePhotorealisticIonAssetId = 2275207;",
            "constexpr int64 CesiumWorldTerrainIonAssetId = 1;",
            "constexpr double GoogleMaximumScreenSpaceError = 1.0;",
            "constexpr double CesiumWorldTerrainMaximumScreenSpaceError = 8.0;",
            "constexpr float ReadinessSampleIntervalSeconds = 0.5f;",
            "constexpr float PresentLoadProgressPercent = 98.0f;",
            "constexpr float RestoreLoadProgressPercent = 90.0f;",
            "constexpr int32 PresentRequiredConsecutiveSamples = 3;",
            "constexpr int32 RestoreRequiredConsecutiveSamples = 2;",
            "VISUAL_REFERENCE_ONLY_R33_CESIUM_WORLD_TERRAIN",
        )
        for token in expected_cpp_tokens:
            self.assertIn(token, self.actor_cpp)

    def test_actor_handles_only_requested_cwt_load_failures_with_bounded_retry(self) -> None:
        for token in (
            '#include "Delegates/Delegate.h"',
            "struct FCesium3DTilesetLoadFailureDetails;",
            "FDelegateHandle CwtLoadFailureDelegateHandle;",
            "bCwtLoadFailureRetryLatched",
            "CwtRetryBackoffRemainingSeconds",
            "CwtWarmingElapsedSeconds",
            "HandleCwtTilesetLoadFailure(",
        ):
            self.assertIn(token, self.actor_h)
        for token in (
            '#include "Cesium3DTilesetLoadFailureDetails.h"',
            "constexpr float R33CwtLoadFailureRetryBackoffSeconds = 10.0f;",
            "constexpr float R33CwtWarmingTimeoutSeconds = 30.0f;",
            "OnCesium3DTilesetLoadFailure.AddUObject(",
            "OnCesium3DTilesetLoadFailure.Remove(CwtLoadFailureDelegateHandle)",
            "CwtLoadFailureDelegateHandle.Reset()",
        ):
            self.assertIn(token, self.actor_cpp)

        handler = function_region(
            self.actor_cpp,
            "void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::\n"
            "    HandleCwtTilesetLoadFailure(",
            ("bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::ApplyState(",),
        )
        for token in (
            "!bCwtPresentationRequested",
            "!Details.Tileset.IsValid()",
            "CwtTileset->GetWorld() != World",
            "Details.Tileset.Get() != CwtTileset.Get()",
            "bCwtLoadFailureRetryLatched",
            "ArmCwtRetryBackoff();",
            "EnterSafeLocal(SafeLocalError)",
            "Details.Type",
            "Details.HttpStatusCode",
            "Details.HttpStatusCode == 401",
            "Details.HttpStatusCode == 403",
            "bCwtPresentationRequested = false;",
        ):
            self.assertIn(token, handler)
        self.assertLess(
            handler.find("ArmCwtRetryBackoff();"),
            handler.find("EnterSafeLocal(SafeLocalError)"),
        )
        self.assertNotIn("Details.Message", self.actor_cpp)

        request = function_region(
            self.actor_cpp,
            "RequestCwtPresentation(bool bRequest, FString& OutError)",
            ("TickPresentationState(float DeltaSeconds, FString& OutError)",),
        )
        self.assertIn("if (bCwtLoadFailureRetryLatched)", request)
        self.assertIn("bCwtLoadFailureRetryLatched = false;", request)
        self.assertIn("CwtRetryBackoffRemainingSeconds = 0.0f;", request)
        self.assertIn("CwtTileset->RefreshTileset();", request)
        self.assertLess(
            request.find("if (bCwtLoadFailureRetryLatched)"),
            request.find("return EnterCwtWarming(OutError);"),
        )

        tick_state = function_region(
            self.actor_cpp,
            "TickPresentationState(float DeltaSeconds, FString& OutError)",
            ("void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::BeginPlay()",),
        )
        for token in (
            "CwtRetryBackoffRemainingSeconds - DeltaSeconds",
            "CwtRetryBackoffRemainingSeconds > 0.0f",
            "bCwtLoadFailureRetryLatched = false;",
            "CwtTileset->RefreshTileset();",
            "return EnterCwtWarming(OutError);",
            "CwtWarmingElapsedSeconds + DeltaSeconds",
            "CwtWarmingElapsedSeconds >= R33CwtWarmingTimeoutSeconds",
            "warming exceeded %.1f seconds without readiness",
        ):
            self.assertIn(token, tick_state)
        self.assertLess(
            tick_state.find("CwtTileset->RefreshTileset();"),
            tick_state.find("return EnterCwtWarming(OutError);"),
        )

        begin_play = function_region(
            self.actor_cpp,
            "void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::BeginPlay()",
            ("void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::Tick(",),
        )
        self.assertLess(
            begin_play.find("RegisterCwtLoadFailureHandler();"),
            begin_play.find("RegisterR33CesiumWorldTerrainController("),
        )
        end_play = function_region(
            self.actor_cpp,
            "void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::EndPlay(",
            (),
        )
        self.assertLess(
            end_play.find("UnregisterCwtLoadFailureHandler();"),
            end_play.find("ApplyR33SurvivingRolesSafeLocal("),
        )

    def test_actor_resolves_exact_roles_and_shared_same_world_georeference(self) -> None:
        for token in (
            '#include "Cesium3DTileset.h"',
            '#include "CesiumGeoreference.h"',
            "TActorIterator<ACesium3DTileset>",
            "TActorIterator<ACesiumGeoreference>",
            "GetTilesetSource() != ETilesetSource::FromCesiumIon",
            "GetIonAssetID()",
            "GooglePhotorealisticIonAssetId",
            "CesiumWorldTerrainIonAssetId",
            "TRIADIstanaExploreV5DVisualTileset",
            "TRIADIstanaExploreV5DR33CesiumWorldTerrainTileset",
            "TRIADIstanaExploreV5DR33CesiumContextMember",
            "GetGeoreference()",
            "GoogleTileset->GetWorld() != World",
            "CwtTileset->GetWorld() != World",
            "Georeference->GetWorld() != World",
            "TotalTilesetCount != 2",
            "ContextMemberCount != 2",
            "GeoreferenceCount != 1",
            "GoogleTileset == CwtTileset",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertRegex(
            self.actor_cpp,
            r"GoogleTileset->GetGeoreference\(\)\s*!=\s*"
            r"CwtTileset->GetGeoreference\(\)",
        )
        self.assertNotIn("SetIonAssetID(2)", self.actor_cpp)
        self.assertNotIn("BingMaps", self.actor_cpp)
        self.assertNotIn("Bing Aerial", self.actor_cpp)

    def test_actor_configures_cwt_visual_tuple_and_cold_hidden_default(self) -> None:
        for token in (
            "SetTilesetSource(ETilesetSource::FromCesiumIon)",
            "SetIonAssetID(CesiumWorldTerrainIonAssetId)",
            "SetGeoreference(",
            "InCwtTileset->SetCesiumIonServer(InGoogleTileset->GetCesiumIonServer())",
            "SetMaximumScreenSpaceError(CesiumWorldTerrainMaximumScreenSpaceError)",
            "ApplyDpiScaling = EApplyDpiScaling::No",
            "SetCreatePhysicsMeshes(false)",
            "SetCreateNavCollision(false)",
            "SetActorEnableCollision(false)",
            "ShowCreditsOnScreen = true",
            "RefreshTileset()",
            "EnterGooglePrimary(",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertGreaterEqual(self.actor_cpp.count("SuspendUpdate = true"), 1)
        self.assertGreaterEqual(
            self.actor_cpp.count("SetActorHiddenInGame(true)"), 1
        )

        configure = function_region(
            self.actor_cpp,
            "ConfigureR33CesiumWorldTerrainReference(",
            ("ValidateR33CesiumWorldTerrainReference(",),
        )
        self.assertIn("InGoogleTileset->GetWorld() != World", configure)
        self.assertIn("InCwtTileset->GetWorld() != World", configure)
        self.assertIn("InGeoreference->GetWorld() != World", configure)
        self.assertIn("InGoogleTileset == InCwtTileset", configure)
        self.assertIn("InGoogleTileset->Tags.AddUnique(", configure)
        self.assertIn("InGeoreference", configure)
        self.assertIn(
            "InCwtTileset->GetCesiumIonServer() != InGoogleTileset->GetCesiumIonServer()",
            configure,
        )
        self.assertIn(
            "CwtTileset->GetMaterial() != GoogleTileset->GetMaterial()",
            self.actor_cpp,
        )
        self.assertIn(
            "CwtTileset->GetTranslucentMaterial() !=",
            self.actor_cpp,
        )
        self.assertIn('#include "CesiumRasterOverlay.h"', self.actor_cpp)
        site_clip = function_region(
            self.actor_cpp,
            "ValidateSharedProviderSiteClip(FString& OutError) const",
            ("ResetReadinessCounters()",),
        )
        for token in (
            "TArray<UCesiumRasterOverlay*> GoogleRasterOverlays",
            "TArray<UCesiumRasterOverlay*> CwtRasterOverlays",
            "GoogleRasterOverlays.Num() != 1",
            "CwtRasterOverlays.Num() != 1",
            "GoogleRasterOverlays[0] != GoogleOverlays[0]",
            "CwtRasterOverlays[0] != CwtOverlays[0]",
        ):
            self.assertIn(token, site_clip)
        self.assertNotIn("InGoogleTileset->SetIonAssetID", configure)
        self.assertNotIn("InGoogleTileset->SetMaximumScreenSpaceError", configure)
        self.assertNotIn("InGoogleTileset->SetTilesetSource", configure)
        self.assertNotIn("SetVisibility(", configure)
        self.assertNotIn("SourceTerrain", self.actor_h)
        self.assertNotIn("UPrimitiveComponent", self.actor_h)
        self.assertNotIn("UStaticMeshComponent", self.actor_h)

    def test_actor_enforces_exclusive_states_and_hysteretic_readiness(self) -> None:
        for method in (
            "EnterGooglePrimary(",
            "EnterCwtWarming(",
            "EnterCwtPresented(",
            "EnterSafeLocal(",
        ):
            self.assertIn(method, self.actor_cpp)
        for token in (
            "ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary",
            "ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming",
            "ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented",
            "ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal",
            "CwtTileset->GetLoadProgress()",
            "PresentReadySamples",
            "RestoreFailureSamples",
            "PresentRequiredConsecutiveSamples",
            "RestoreRequiredConsecutiveSamples",
            "PresentLoadProgressPercent",
            "RestoreLoadProgressPercent",
            "ReadinessSampleIntervalSeconds",
            "ApplyState(",
            "bGoogleVisible && bCwtVisible",
            "doubleVisible=false",
            "sourceTerrainAuthorityUnchanged=true",
        ):
            self.assertIn(token, self.actor_cpp)

        exclusive = function_region(
            self.actor_cpp,
            "bool ApplyExclusivePresentationState(",
            (
                "bool OverlayMatches(",
            ),
        )
        # The helper must hide both streamed providers before selecting the one
        # admitted by the new state; this is the static hide-before-show gate.
        self.assertGreaterEqual(exclusive.count("SetActorHiddenInGame(true)"), 2)
        self.assertLess(
            exclusive.find("SetActorHiddenInGame(true)"),
            exclusive.find("SetActorHiddenInGame(false)"),
        )

        tick = function_region(
            self.actor_cpp,
            "TickPresentationState(float DeltaSeconds, FString& OutError)",
            ("void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::BeginPlay()",),
        )
        presented_branch = tick[tick.rindex("PresentReadySamples = 0;") :]
        self.assertIn("return EnterCwtWarming(OutError);", presented_branch)
        self.assertNotIn("ArmCwtRetryBackoff();", presented_branch)
        self.assertNotIn("EnterSafeLocal(", presented_branch)

    def test_context_policy_keeps_buildings_independent_from_r33_terrain(self) -> None:
        for token in (
            "RegisterR33CesiumWorldTerrainController(",
            "UnregisterR33CesiumWorldTerrainController(",
            "ApplyR33CesiumPresentationMode(",
            "ValidateR33CesiumPresentationBinding(",
            "ShouldHideSourceTerrainRendererForVisualContext() const",
            "ShouldPresentR29CopernicusFallback() const",
            "bR33DualCesiumContextConfigured",
            "bR33CwtPresentationActive",
            "bR33SafeLocalPresentationActive",
            "ResolveSceneAndR33Tilesets(",
            "SetR33LocalFallbackVisibility(",
        ):
            self.assertIn(token, self.context_h + self.context_cpp)
        for token in (
            "ContextPolicyR33CwtTilesetTag",
            "ContextPolicyR33CesiumContextMemberTag",
            "TotalTilesetCount != 2",
            "ContextMemberCount != 2",
            "GoogleRoleCount != 1",
            "CwtRoleCount != 1",
            "OutGoogleTileset->GetIonAssetID() != 2275207",
            "OutCwtTileset->GetIonAssetID() != 1",
            "bR33CwtPresentationActive",
            "bExpectedOuterVisible",
            "Controller->GetPresentationState()",
            "GoogleTileset->SuspendUpdate != bExpectedGoogleSuspended",
            "CwtTileset->SuspendUpdate != bExpectedCwtSuspended",
        ):
            self.assertIn(token, self.context_cpp)
        self.assertIn(
            "Policy->ShouldHideSourceTerrainRendererForVisualContext()",
            self.ground_cpp,
        )
        self.assertIn(
            "Policy->ShouldPresentR29CopernicusFallback()",
            self.r29_cpp,
        )
        self.assertIn(
            "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
            "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp",
            self.contract["nativeTransaction"]["exactPromotion"]["files"],
        )
        end_play = function_region(
            self.context_cpp,
            "void ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(",
            ("#if WITH_DEV_AUTOMATION_TESTS",),
        )
        self.assertIn("Cwt->SuspendUpdate = true;", end_play)
        self.assertIn("Cwt->RefreshTileset();", end_play)
        self.assertIn("Cwt->IsReadyForFinishDestroy();", end_play)
        self.assertEqual(
            1,
            end_play.count("FHttpModule::Get().GetHttpManager().Flush("),
        )

    def test_actor_has_no_height_sampling_persistence_or_simulation_authority(self) -> None:
        forbidden_tokens = (
            "SampleHeightMostDetailed",
            "SampleHeight(",
            "FFileHelper",
            "SaveStringToFile",
            "SaveArrayToFile",
            "Serialize(",
            "CreatePhysicsMeshes(true)",
            "CreateNavCollision(true)",
            "QueryAndPhysics",
            "SetCanEverAffectNavigation(true)",
        )
        for token in forbidden_tokens:
            self.assertNotIn(token, self.actor_cpp)
        for token in (
            "visualReferenceOnly=true",
            "heightSamplesPersisted=false",
            "surveyAccuracyClaimed=false",
            "verticalDatumResolved=false",
            "collisionAuthority=false",
            "navigationAuthority=false",
            "lineOfSightAuthority=false",
            "sensorAuthority=false",
            "rfGeometryAuthority=false",
            "visualAcceptance=false",
            "nativeMapApplicationClaimed=false",
        ):
            self.assertIn(token, self.actor_cpp)

    def test_readme_explains_visible_behavior_and_nonclaims(self) -> None:
        readme = " ".join(self.readme.split())
        for text in (
            "source-only, visual-reference integration",
            "exactly two role-tagged Cesium tilesets",
            "asset `2275207` and CWT ion asset `1`",
            "same sole Cesium georeference",
            "same server pointer",
            "never reads, serializes, or logs the token",
            "editor-world-only transaction",
            "global native tileset-load-failure delegate",
            "provider failure message is intentionally never logged",
            "deterministic 10-second cooldown",
            "bounded to 30 seconds",
            "HTTP 401 and 403",
            "CWT starts hidden, hidden-in-game, and update-suspended",
            "screen-space error `8.0`",
            "must never be visible together",
            "three consecutive samples at or above 98%",
            "Two consecutive samples below 90%",
            "return the controller to `CwtWarming`",
            "ordinary view-dependent churn does not enter `SafeLocal`",
            "current view only",
            "QueryAndPhysics collision remains enabled in every state",
            "sole collision, line-of-sight, RF-geometry, and sensor-occlusion authority",
            "current local surrounding-building shell",
            "synthetic outer-ground sheet is hidden",
            "R29 is a render-only loading/SafeLocal visual fallback",
            "ready Google view or a presented CWT view hides both local terrain renderers",
            "fail-closes to the original source renderer",
            "R33 actor never accepts or mutates R29 or the source terrain",
            "No state changes source-terrain collision or query settings",
            "does not sample, serialize, log, export, or bake CWT heights",
            "does not transfer Google's imagery",
            "show unimagery terrain geometry, not a photoreal surface",
            "claims no survey-grade",
            "repository source only",
            "R30",
            "R31",
            "R32",
            "only then may R33",
        ):
            self.assertIn(text, readme)


if __name__ == "__main__":
    unittest.main()
