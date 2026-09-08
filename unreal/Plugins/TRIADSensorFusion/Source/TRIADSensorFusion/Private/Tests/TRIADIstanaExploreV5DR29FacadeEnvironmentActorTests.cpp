#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR29FacadeNativeLayoutTest,
    "TRIAD.Istana.ExploreV5D.R29Facade.NativeComponentLayout",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR29FacadeNativeLayoutTest::RunTest(
    const FString& Parameters)
{
    const ATRIADIstanaExploreV5DR29FacadeEnvironmentActor* Actor =
        GetDefault<ATRIADIstanaExploreV5DR29FacadeEnvironmentActor>();
    TestNotNull(TEXT("R29 facade actor CDO exists"), Actor);
    if (!Actor)
    {
        return false;
    }
    TestEqual(
        TEXT("Exact owned renderer count"),
        Actor->ExpectedOwnedRendererCount(),
        3);
    TestNotNull(TEXT("Static scene root exists"), Actor->SceneRoot.Get());
    TestNotNull(
        TEXT("Retained R28 public realm renderer exists"),
        Actor->RetainedR28ConnectivePublicRealmRenderOnly.Get());
    TestNotNull(
        TEXT("R29 facade renderer exists"),
        Actor->R29ContextFacadeCoverageRenderOnly.Get());
    TestNotNull(
        TEXT("Retained R28 outer-ground renderer exists"),
        Actor->RetainedR28OuterGroundOverlayRenderOnly.Get());
    TestFalse(TEXT("Actor never ticks"), Actor->PrimaryActorTick.bCanEverTick);
    TestFalse(TEXT("Actor collision disabled"), Actor->GetActorEnableCollision());
    TestTrue(TEXT("Render-only truth default"), Actor->bRenderOnly);
    TestTrue(
        TEXT("R28 public realm retained truth default"),
        Actor->bR28PublicRealmRetainedUnmodified);
    TestFalse(
        TEXT("R28 architecture cannot be retained or co-rendered"),
        Actor->bR28ArchitectureRetainedOrCoRendered);
    TestFalse(
        TEXT("No simulation/sensor/RF authority"),
        Actor->bCollisionNavigationSimulationSensorOrRfAuthority);
    TestFalse(
        TEXT("No survey/as-built/current/material claim"),
        Actor->bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed);
    TestFalse(
        TEXT("No source inputs modified"),
        Actor->bExistingSimulationRfProviderOrGeospatialInputsModified);
    TestFalse(TEXT("No runtime geometry"), Actor->bRuntimeGeometryGenerated);
    TestFalse(TEXT("CDO is not configured"), Actor->bConfigured);
    TestFalse(TEXT("CDO replacement is not active"), Actor->bReplacementActivated);

    const UStaticMeshComponent* Components[] = {
        Actor->RetainedR28ConnectivePublicRealmRenderOnly,
        Actor->R29ContextFacadeCoverageRenderOnly,
        Actor->RetainedR28OuterGroundOverlayRenderOnly};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Components); ++Index)
    {
        const UStaticMeshComponent* Component = Components[Index];
        if (!Component)
        {
            continue;
        }
        TestEqual(
            *FString::Printf(TEXT("Component %d is attached to root"), Index),
            Component->GetAttachParent(),
            Actor->SceneRoot.Get());
        TestEqual(
            *FString::Printf(TEXT("Component %d has no collision"), Index),
            Component->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        TestFalse(
            *FString::Printf(TEXT("Component %d cannot navigate"), Index),
            Component->CanEverAffectNavigation());
        TestTrue(
            *FString::Printf(TEXT("Component %d hidden from scene capture"), Index),
            Component->bHiddenInSceneCapture);
        TestFalse(
            *FString::Printf(TEXT("Component %d starts hidden"), Index),
            Component->IsVisible());
        TestTrue(
            *FString::Printf(TEXT("Component %d starts hidden in game"), Index),
            Component->bHiddenInGame);
    }

    TestEqual(
        TEXT("Exact R29 material slot roster"),
        Actor->ExpectedR29MaterialSlotNames().Num(),
        11);
    TestTrue(
        TEXT("R29 facade object path is isolated"),
        Actor->ExpectedR29FacadeMeshObjectPath().Contains(
            TEXT("/SurroundingsRealismR29/")));
    TestNotEqual(
        TEXT("R29 facade does not alias R28 architecture"),
        Actor->ExpectedR29FacadeMeshObjectPath(),
        Actor->ExpectedR28ArchitectureMeshObjectPath());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
