#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR30FacadeLookdevNativeLayoutTest,
    "TRIAD.Istana.ExploreV5D.R30FacadeLookdev.NativeComponentLayout",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR30FacadeLookdevNativeLayoutTest::RunTest(
    const FString& Parameters)
{
    const ATRIADIstanaExploreV5DR30FacadeLookdevActor* Actor =
        GetDefault<ATRIADIstanaExploreV5DR30FacadeLookdevActor>();
    TestNotNull(TEXT("R30 facade-lookdev CDO exists"), Actor);
    if (!Actor)
    {
        return false;
    }
    TestEqual(TEXT("Owned renderer count"), Actor->ExpectedOwnedRendererCount(), 3);
    TestEqual(
        TEXT("Facade override count"),
        Actor->ExpectedFacadeMaterialOverrideCount(),
        11);
    TestNotNull(TEXT("Scene root exists"), Actor->SceneRoot.Get());
    TestNotNull(
        TEXT("Retained R28 public-realm renderer exists"),
        Actor->RetainedR28ConnectivePublicRealmRenderOnly.Get());
    TestNotNull(
        TEXT("R30 facade-lookdev renderer exists"),
        Actor->R30ContextFacadeLookdevRenderOnly.Get());
    TestNotNull(
        TEXT("Retained R28 outer-ground renderer exists"),
        Actor->RetainedR28OuterGroundOverlayRenderOnly.Get());
    TestTrue(TEXT("Provider mirror tick exists"), Actor->PrimaryActorTick.bCanEverTick);
    TestTrue(
        TEXT("Provider mirror starts enabled"),
        Actor->PrimaryActorTick.bStartWithTickEnabled);
    TestTrue(
        TEXT("Provider mirror is exactly 2 Hz"),
        FMath::IsNearlyEqual(Actor->PrimaryActorTick.TickInterval, 0.5f));
    TestFalse(TEXT("Actor collision disabled"), Actor->GetActorEnableCollision());
    TestTrue(TEXT("Render-only truth"), Actor->bRenderOnly);
    TestTrue(
        TEXT("Meshes and transforms retained"),
        Actor->bPredecessorMeshesAndTransformsRetainedUnmodified);
    TestFalse(
        TEXT("R28/R29 environment owner cannot co-render"),
        Actor->bR28OrR29ArchitectureRetainedOrCoRendered);
    TestFalse(
        TEXT("No collision/navigation/simulation/sensor/RF authority"),
        Actor->bCollisionNavigationSimulationSensorOrRfAuthority);
    TestFalse(
        TEXT("No survey/as-built/current/material claim"),
        Actor->bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed);
    TestFalse(
        TEXT("No provider/Cesium/geospatial input mutation"),
        Actor->bExistingSimulationRfProviderCesiumOrGeospatialInputsModified);
    TestFalse(TEXT("No runtime geometry"), Actor->bRuntimeGeometryGenerated);
    TestTrue(
        TEXT("Provider readiness is observed only"),
        Actor->bProviderReadinessObservedWithoutPolicyMutation);
    TestFalse(TEXT("CDO unconfigured"), Actor->bConfigured);
    TestFalse(TEXT("CDO inactive"), Actor->bReplacementActivated);

    const UStaticMeshComponent* Components[] = {
        Actor->RetainedR28ConnectivePublicRealmRenderOnly,
        Actor->R30ContextFacadeLookdevRenderOnly,
        Actor->RetainedR28OuterGroundOverlayRenderOnly};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Components); ++Index)
    {
        const UStaticMeshComponent* Component = Components[Index];
        if (!Component)
        {
            continue;
        }
        TestEqual(
            *FString::Printf(TEXT("Component %d parent"), Index),
            Component->GetAttachParent(), Actor->SceneRoot.Get());
        TestEqual(
            *FString::Printf(TEXT("Component %d no collision"), Index),
            Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestFalse(
            *FString::Printf(TEXT("Component %d cannot navigate"), Index),
            Component->CanEverAffectNavigation());
        TestTrue(
            *FString::Printf(TEXT("Component %d excluded from captures"), Index),
            Component->bHiddenInSceneCapture);
        TestFalse(
            *FString::Printf(TEXT("Component %d starts hidden"), Index),
            Component->IsVisible());
        TestTrue(
            *FString::Printf(TEXT("Component %d starts hidden in game"), Index),
            Component->bHiddenInGame);
    }

    TestEqual(
        TEXT("R29 semantic slot roster"),
        Actor->ExpectedR29MaterialSlotNames().Num(), 11);
    TestEqual(
        TEXT("R30 material path roster"),
        Actor->ExpectedR30MaterialObjectPaths().Num(), 11);
    TestTrue(
        TEXT("R29 mesh stays in its immutable root"),
        Actor->ExpectedR29FacadeMeshObjectPath().Contains(
            TEXT("/SurroundingsRealismR29/")));
    for (const FString& Path : Actor->ExpectedR30MaterialObjectPaths())
    {
        TestTrue(
            TEXT("Override stays in isolated R30 root"),
            Path.Contains(TEXT("/SurroundingsLookdevR30/")));
        TestFalse(
            TEXT("Context override never depends on HeroMaterials"),
            Path.Contains(TEXT("HeroMaterials")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
