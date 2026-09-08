#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5AppearanceActor.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5Pawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5RuntimeAppearanceContractTest,
    "TRIAD.Istana.ExploreV5.RuntimeAppearanceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5RuntimeAppearanceContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const ATRIADIstanaExploreV5AppearanceActor* AppearanceDefaults =
        GetDefault<ATRIADIstanaExploreV5AppearanceActor>();
    TestNotNull(TEXT("V5 appearance class default object"), AppearanceDefaults);
    if (AppearanceDefaults)
    {
        TestEqual(
            TEXT("Exact appearance-only claim label"),
            AppearanceDefaults->ClaimLabel,
            ATRIADIstanaExploreV5AppearanceActor::ExpectedClaimLabel());
        TestTrue(TEXT("V5 actor is appearance-only"), AppearanceDefaults->bAppearanceOnly);
        TestFalse(TEXT("No geometry/transform authority"), AppearanceDefaults->bGeometryOrTransformAuthority);
        TestFalse(TEXT("No collision/navigation authority"), AppearanceDefaults->bCollisionOrNavigationAuthority);
        TestFalse(TEXT("No survey/as-built claim"), AppearanceDefaults->bSurveyOrAsBuiltClaimed);
        TestFalse(TEXT("No botanical-inventory claim"), AppearanceDefaults->bBotanicalInventoryClaimed);
        TestFalse(TEXT("No sensor/RF material authority"), AppearanceDefaults->bSensorOrRfMaterialAuthority);
        TestFalse(TEXT("No Google/OneMap content"), AppearanceDefaults->bGoogleOrOneMapContentUsed);
        TestEqual(
            TEXT("Appearance actor owns only its non-primitive root"),
            AppearanceDefaults->GetComponents().Num(),
            1);
        TestEqual(
            TEXT("Exact appearance root remains authoritative root"),
            AppearanceDefaults->GetRootComponent(),
            AppearanceDefaults->SceneRoot.Get());
        TestTrue(
            TEXT("Appearance root remains static"),
            AppearanceDefaults->SceneRoot &&
                AppearanceDefaults->SceneRoot->Mobility ==
                    EComponentMobility::Static);
        TestFalse(
            TEXT("Sun pose/colour/intensity mutation is forbidden"),
            AppearanceDefaults->bSunPoseColorOrIntensityModified);
        TestFalse(
            TEXT("Unconfigured CDO has no inherited sun baseline"),
            AppearanceDefaults->bInheritedSunPoseColorIntensityRecorded);
        TestNull(
            TEXT("Unconfigured CDO has no directional sun"),
            AppearanceDefaults->DirectionalSunActor.Get());
        TestEqual(
            TEXT("Exact inherited directional-sun tag"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedDirectionalSunTag(),
            FName(TEXT("TRIADIstanaPublicViewLighting_v1")));
        TestEqual(
            TEXT("World-space contact-shadow length is 50 cm"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedContactShadowLengthCentimeters(),
            50.0f,
            0.0001f);
        TestTrue(
            TEXT("Contact-shadow length uses world-space centimetres"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedContactShadowLengthInWorldSpace());
        TestEqual(
            TEXT("Contact-shadow casting intensity is one"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedContactShadowCastingIntensity(),
            1.0f,
            0.0001f);
        TestEqual(
            TEXT("Contact-shadow non-casting intensity is zero"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedContactShadowNonCastingIntensity(),
            0.0f,
            0.0001f);
        TestEqual(
            TEXT("Exact enabled contact-shadow census"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedContactShadowEnabledComponentCount(),
            17);
        TestEqual(
            TEXT("Exact disabled contact-shadow census"),
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedContactShadowDisabledComponentCount(),
            11);

        const TArray<FName> ContactShadowComponentNames = {
            FName(TEXT("BuildingHeroVisual")),
            FName(TEXT("PublicForecourtHardscape")),
            FName(TEXT("V4UmbrellaTreeReclassification")),
            FName(TEXT("V4DomeTreeReclassification")),
            FName(TEXT("V4HighForkRoundedTreeReclassification")),
            FName(TEXT("V4ColumnarNarrowTreeReclassification")),
            FName(TEXT("V4PalmTreeReclassification")),
            FName(TEXT("V4HeritageUmbrellaSilhouetteProxies")),
            FName(TEXT("V4HeritageDomeSilhouetteProxies")),
            FName(TEXT("V4HeritageHighForkSilhouetteProxies")),
            FName(TEXT("V4HeritageColumnarSilhouetteProxies")),
            FName(TEXT("V4HeritagePalmSilhouetteProxies")),
            FName(TEXT("V4LayeredShrubs")),
            FName(TEXT("V4FloweringAccents")),
            FName(TEXT("V4TropicalUnderstorey")),
            FName(TEXT("V8CentralPorticoRenderOnlySuccessor")),
            FName(TEXT("V5CCentralPorticoRenderOnlySuccessor")),
            FName(TEXT("BuildingCollision")),
            FName(TEXT("TerrainVisualCollision")),
            FName(TEXT("SyntheticTerrainBoundarySkirt")),
            FName(TEXT("AnonymousContextBuildings")),
            FName(TEXT("ODbLMappingGradeContextBuildings")),
            FName(TEXT("VolumetricRainTrees")),
            FName(TEXT("VolumetricPalmTrees")),
            FName(TEXT("VolumetricFramingTrees")),
            FName(TEXT("V4HeritageAnchorPawnOnlyBlockers")),
            FName(TEXT("V4TallerEdgeGeometryGrass")),
            FName(TEXT("V4AnimatedCloseTurfGeometryCardReplacement"))};
        TArray<uint8> ContactShadowMobilities;
        ContactShadowMobilities.Init(
            static_cast<uint8>(EComponentMobility::Static),
            ContactShadowComponentNames.Num());
        TArray<bool> ContactShadowOrdinaryCastFlags;
        ContactShadowOrdinaryCastFlags.Init(
            true,
            ContactShadowComponentNames.Num());
        ContactShadowOrdinaryCastFlags[26] = false;
        ContactShadowOrdinaryCastFlags[27] = false;
        TArray<bool> InheritedContactShadowFlags;
        InheritedContactShadowFlags.Init(
            true,
            ContactShadowComponentNames.Num());

        FString ContactShadowContractError;
        TestTrue(
            TEXT("All 28 exact inherited contact-shadow rows validate"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateContactShadowRosterStateForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    InheritedContactShadowFlags,
                    false,
                    ContactShadowContractError));

        TArray<uint8> MobilityDrift = ContactShadowMobilities;
        MobilityDrift[0] =
            static_cast<uint8>(EComponentMobility::Movable);
        TestFalse(
            TEXT("Inherited mobility drift is rejected"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateContactShadowRosterStateForAutomation(
                    ContactShadowComponentNames,
                    MobilityDrift,
                    ContactShadowOrdinaryCastFlags,
                    InheritedContactShadowFlags,
                    false,
                    ContactShadowContractError));

        TArray<bool> OrdinaryCastDrift =
            ContactShadowOrdinaryCastFlags;
        OrdinaryCastDrift[24] = false;
        TestFalse(
            TEXT("Inherited CastShadow drift is rejected"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateContactShadowRosterStateForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    OrdinaryCastDrift,
                    InheritedContactShadowFlags,
                    false,
                    ContactShadowContractError));

        TArray<bool> PriorContactShadowDrift =
            InheritedContactShadowFlags;
        PriorContactShadowDrift[27] = false;
        const TArray<bool> PriorContactShadowDriftBeforeApply =
            PriorContactShadowDrift;
        TestFalse(
            TEXT("Inherited bCastContactShadow drift is rejected before mutation"),
            ATRIADIstanaExploreV5AppearanceActor::
                ApplyContactShadowRosterStateForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    PriorContactShadowDrift,
                    ContactShadowContractError));
        TestTrue(
            TEXT("Rejected inherited drift leaves every contact flag untouched"),
            PriorContactShadowDrift ==
                PriorContactShadowDriftBeforeApply);

        TArray<bool> AppliedContactShadowFlags =
            InheritedContactShadowFlags;
        TestTrue(
            TEXT("Exact inherited roster applies transactionally"),
            ATRIADIstanaExploreV5AppearanceActor::
                ApplyContactShadowRosterStateForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    AppliedContactShadowFlags,
                    ContactShadowContractError));
        for (int32 Index = 0;
             Index < AppliedContactShadowFlags.Num();
             ++Index)
        {
            TestEqual(
                *FString::Printf(
                    TEXT("Applied contact-shadow row %d"),
                    Index),
                AppliedContactShadowFlags[Index],
                Index < 17);
        }
        TestTrue(
            TEXT("All 28 exact applied contact-shadow rows read back"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateContactShadowRosterStateForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    AppliedContactShadowFlags,
                    true,
                    ContactShadowContractError));

        TArray<bool> V5DTreeRuntimeContactShadowFlags =
            AppliedContactShadowFlags;
        for (int32 Index = 2; Index < 12; ++Index)
        {
            V5DTreeRuntimeContactShadowFlags[Index] = false;
        }
        TestTrue(
            TEXT("Exact V5D runtime state suppresses only ten V4 tree rows"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateV5DTreeRuntimeContactShadowRosterForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    V5DTreeRuntimeContactShadowFlags,
                    ContactShadowContractError));
        int32 V5DTreeRuntimeEnabledContactShadowCount = 0;
        for (const bool bCastContactShadow :
             V5DTreeRuntimeContactShadowFlags)
        {
            V5DTreeRuntimeEnabledContactShadowCount +=
                bCastContactShadow ? 1 : 0;
        }
        TestEqual(
            TEXT("Exact V5D runtime effective enabled contact-shadow census"),
            V5DTreeRuntimeEnabledContactShadowCount,
            7);
        TestEqual(
            TEXT("Exact V5D runtime effective disabled contact-shadow census"),
            V5DTreeRuntimeContactShadowFlags.Num() -
                V5DTreeRuntimeEnabledContactShadowCount,
            21);

        TArray<bool> V5DTreeRuntimeDrift =
            V5DTreeRuntimeContactShadowFlags;
        V5DTreeRuntimeDrift[2] = true;
        TestFalse(
            TEXT("V5D runtime tree contact-shadow drift is rejected"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateV5DTreeRuntimeContactShadowRosterForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    V5DTreeRuntimeDrift,
                    ContactShadowContractError));

        V5DTreeRuntimeDrift = V5DTreeRuntimeContactShadowFlags;
        V5DTreeRuntimeDrift[12] = false;
        TestFalse(
            TEXT("V5D cannot suppress a non-tree V5 contact-shadow row"),
            ATRIADIstanaExploreV5AppearanceActor::
                ValidateV5DTreeRuntimeContactShadowRosterForAutomation(
                    ContactShadowComponentNames,
                    ContactShadowMobilities,
                    ContactShadowOrdinaryCastFlags,
                    V5DTreeRuntimeDrift,
                    ContactShadowContractError));
        TestEqual(
            TEXT("Exact V5 lawn material path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedLawnMaterialPath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_Grass001_Lawn.MI_IPV5_Grass001_Lawn")));
        TestEqual(
            TEXT("Exact V5 water material path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedFountainWaterMaterialPath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/M_IPV5_FountainWater.M_IPV5_FountainWater")));
        TestEqual(
            TEXT("Exact V5 stone material path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedHardscapeStoneMaterialPath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone")));
        TestEqual(
            TEXT("Exact V5 context-render material path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedContextRenderMaterialPath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_ContextRender.MI_IPV5_ContextRender")));
        TestEqual(
            TEXT("Exact V5 context-roof material path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedContextRoofMaterialPath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_ContextRoof.MI_IPV5_ContextRoof")));
        TestEqual(
            TEXT("Exact V4 close-turf base path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfBaseMaterialPath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_CloseTurf_Wind.M_IPV4_CloseTurf_Wind")));
        TestEqual(
            TEXT("Exact close-turf base-colour texture path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfBaseColorTexturePath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_Color.Foliage008_Color")));
        TestEqual(
            TEXT("Exact close-turf normal texture path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfNormalTexturePath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_NormalGL.Foliage008_NormalGL")));
        TestEqual(
            TEXT("Exact close-turf roughness texture path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfRoughnessTexturePath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_Roughness.Foliage008_Roughness")));
        TestEqual(
            TEXT("Exact close-turf opacity texture path"),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfOpacityTexturePath(),
            FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_Opacity.Foliage008_Opacity")));

        IConsoleManager& Console = IConsoleManager::Get();
        const IConsoleVariable* ContactShadows =
            Console.FindConsoleVariable(TEXT("r.ContactShadows"));
        const IConsoleVariable* OverrideLength =
            Console.FindConsoleVariable(
                TEXT("r.ContactShadows.OverrideLength"));
        const IConsoleVariable* OverrideCasting =
            Console.FindConsoleVariable(
                TEXT("r.ContactShadows.OverrideShadowCastingIntensity"));
        const IConsoleVariable* OverrideNonCasting =
            Console.FindConsoleVariable(
                TEXT("r.ContactShadows.OverrideNonShadowCastingIntensity"));
        const IConsoleVariable* IntensityMode =
            Console.FindConsoleVariable(
                TEXT("r.ContactShadows.Intensity.Mode"));
        TestNotNull(TEXT("r.ContactShadows exists"), ContactShadows);
        TestNotNull(TEXT("Contact-shadow length override exists"), OverrideLength);
        TestNotNull(TEXT("Casting-intensity override exists"), OverrideCasting);
        TestNotNull(
            TEXT("Non-casting-intensity override exists"),
            OverrideNonCasting);
        TestNotNull(TEXT("Contact-shadow intensity mode exists"), IntensityMode);
        if (ContactShadows && OverrideLength && OverrideCasting &&
            OverrideNonCasting && IntensityMode)
        {
            TestEqual(
                TEXT("Global contact shadows remain enabled"),
                ContactShadows->GetInt(),
                1);
            TestTrue(
                TEXT("Global contact-shadow length override is disabled"),
                OverrideLength->GetFloat() < 0.0f);
            TestTrue(
                TEXT("Global casting-intensity override is disabled"),
                OverrideCasting->GetFloat() < 0.0f);
            TestTrue(
                TEXT("Global non-casting-intensity override is disabled"),
                OverrideNonCasting->GetFloat() < 0.0f);
            TestEqual(
                TEXT("Primitive contact-shadow roster mode is selected"),
                IntensityMode->GetInt(),
                0);
        }

        FString UnconfiguredReport;
        TestFalse(
            TEXT("Unconfigured appearance CDO fails closed"),
            AppearanceDefaults->ValidateExploreV5Appearance(
                UnconfiguredReport));
        TestTrue(
            TEXT("Unconfigured failure is explicit"),
            UnconfiguredReport.StartsWith(
                TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID:")));
    }

    const ATRIADIstanaExploreV5Pawn* PawnDefaults =
        GetDefault<ATRIADIstanaExploreV5Pawn>();
    TestNotNull(TEXT("V5 pawn class default object"), PawnDefaults);
    if (PawnDefaults)
    {
        TestTrue(
            TEXT("V5 pawn retains exact base neutral camera"),
            PawnDefaults->HasExpectedExploreCameraProfile());
        TestTrue(
            TEXT("V5 pawn pins exact SSGI/SSR profile"),
            PawnDefaults->HasExpectedExploreV5CameraProfile());
        const UCameraComponent* Camera =
            PawnDefaults->GetExploreCameraComponent();
        TestNotNull(TEXT("V5 Explore camera"), Camera);
        if (Camera)
        {
            const FPostProcessSettings& Settings =
                Camera->PostProcessSettings;
            TestTrue(
                TEXT("SSGI method override"),
                Settings.bOverride_DynamicGlobalIlluminationMethod);
            TestEqual(
                TEXT("Screen Space GI selected"),
                Settings.DynamicGlobalIlluminationMethod.GetValue(),
                EDynamicGlobalIlluminationMethod::ScreenSpace);
            TestTrue(
                TEXT("SSR method override"),
                Settings.bOverride_ReflectionMethod);
            TestEqual(
                TEXT("Screen Space Reflections selected"),
                Settings.ReflectionMethod.GetValue(),
                EReflectionMethod::ScreenSpace);
            TestEqual(
                TEXT("SSR intensity is exact"),
                Settings.ScreenSpaceReflectionIntensity,
                ATRIADIstanaExploreV5Pawn::
                    ExpectedScreenSpaceReflectionIntensity(),
                0.0001f);
            TestEqual(
                TEXT("SSR quality is exact"),
                Settings.ScreenSpaceReflectionQuality,
                ATRIADIstanaExploreV5Pawn::
                    ExpectedScreenSpaceReflectionQuality(),
                0.0001f);
            TestEqual(
                TEXT("SSR max roughness is exact"),
                Settings.ScreenSpaceReflectionMaxRoughness,
                ATRIADIstanaExploreV5Pawn::
                    ExpectedScreenSpaceReflectionMaxRoughness(),
                0.0001f);
            TestTrue(TEXT("Bloom remains disabled"), FMath::IsNearlyZero(Settings.BloomIntensity));
            TestTrue(TEXT("Vignette remains disabled"), FMath::IsNearlyZero(Settings.VignetteIntensity));
            TestTrue(TEXT("Motion blur remains disabled"), FMath::IsNearlyZero(Settings.MotionBlurAmount));
            TestTrue(TEXT("Chromatic aberration remains disabled"), FMath::IsNearlyZero(Settings.SceneFringeIntensity));
            TestTrue(TEXT("Depth of field remains disabled"), FMath::IsNearlyZero(Settings.DepthOfFieldScale));
            TestTrue(TEXT("Film grain remains disabled"), FMath::IsNearlyZero(Settings.FilmGrainIntensity));
            TestNull(TEXT("No colour-grading LUT"), Settings.ColorGradingLUT.Get());
        }
    }

    const ATRIADIstanaExploreV5GameMode* GameModeDefaults =
        GetDefault<ATRIADIstanaExploreV5GameMode>();
    TestNotNull(TEXT("V5 game mode class default object"), GameModeDefaults);
    if (GameModeDefaults)
    {
        TestEqual(
            TEXT("V5 game mode selects V5 pawn"),
            GameModeDefaults->DefaultPawnClass.Get(),
            ATRIADIstanaExploreV5Pawn::StaticClass());
        TestNull(TEXT("V5 Explore HUD remains null"), GameModeDefaults->HUDClass.Get());
        TestEqual(
            TEXT("Tagged Explore start selection remains exact"),
            ATRIADIstanaExploreV5GameMode::ExpectedExplorePlayerStartTag(),
            FName(TEXT("TRIADIstanaExplorePlayerStartV1")));

        int32 MatchCount = INDEX_NONE;
        TArray<TArray<FName>> CandidateTags;
        TestEqual(
            TEXT("No exact tagged starts fails closed"),
            ATRIADIstanaExploreV5GameMode::
                FindUniqueExplorePlayerStartIndex(
                    CandidateTags, MatchCount),
            INDEX_NONE);
        TestEqual(TEXT("No-start exact match count"), MatchCount, 0);

        CandidateTags.Add(TArray<FName>{FName(TEXT("UnrelatedStart"))});
        CandidateTags.Add(TArray<FName>{
            FName(TEXT("AuxiliaryTag")),
            ATRIADIstanaExploreV5GameMode::
                ExpectedExplorePlayerStartTag()});
        TestEqual(
            TEXT("Exactly one exact tagged start succeeds"),
            ATRIADIstanaExploreV5GameMode::
                FindUniqueExplorePlayerStartIndex(
                    CandidateTags, MatchCount),
            1);
        TestEqual(TEXT("Unique-start exact match count"), MatchCount, 1);

        CandidateTags.Add(TArray<FName>{
            ATRIADIstanaExploreV5GameMode::
                ExpectedExplorePlayerStartTag()});
        TestEqual(
            TEXT("More than one exact tagged start fails closed"),
            ATRIADIstanaExploreV5GameMode::
                FindUniqueExplorePlayerStartIndex(
                    CandidateTags, MatchCount),
            INDEX_NONE);
        TestEqual(TEXT("Duplicate-start exact match count"), MatchCount, 2);

        CandidateTags.Reset();
        CandidateTags.Add(TArray<FName>{
            FName(TEXT("TRIADIstanaExplorePlayerStartV1_NearMiss"))});
        TestEqual(
            TEXT("Near-miss start tag fails closed"),
            ATRIADIstanaExploreV5GameMode::
                FindUniqueExplorePlayerStartIndex(
                    CandidateTags, MatchCount),
            INDEX_NONE);
        TestEqual(TEXT("Near-miss exact match count"), MatchCount, 0);
    }

    return true;
}

#endif
