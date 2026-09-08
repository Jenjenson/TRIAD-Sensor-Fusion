#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5DMacDonaldHouseActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DMacDonaldHouseRuntimeContractTest,
    "TRIAD.Istana.ExploreV5D.MacDonaldHouse.RuntimeContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DMacDonaldHouseRuntimeContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const ATRIADIstanaExploreV5DMacDonaldHouseActor* Defaults =
        GetDefault<ATRIADIstanaExploreV5DMacDonaldHouseActor>();
    TestNotNull(TEXT("R24A MacDonald House class default object"), Defaults);
    if (!Defaults)
    {
        return false;
    }

    TestTrue(TEXT("Dedicated landmark observes provider telemetry at runtime"),
        Defaults->PrimaryActorTick.bCanEverTick);
    TestTrue(TEXT("Provider-telemetry observer starts enabled"),
        Defaults->PrimaryActorTick.bStartWithTickEnabled);
    TestFalse(TEXT("Render-only provider observer does not tick dedicated servers"),
        Defaults->PrimaryActorTick.bAllowTickOnDedicatedServer);
    TestEqual(TEXT("Provider observer samples each render frame"),
        Defaults->PrimaryActorTick.TickInterval, 0.0f);
    TestFalse(TEXT("Dedicated landmark actor collision is disabled"),
        Defaults->GetActorEnableCollision());
    TestTrue(TEXT("Dedicated landmark is render-only"), Defaults->bRenderOnly);
    TestFalse(TEXT("Dedicated landmark has no collision/navigation/sensor/RF authority"),
        Defaults->bCollisionNavigationSensorOrRfAuthority);
    TestFalse(TEXT("Dedicated landmark makes no survey/as-built/1:1/material-calibration claim"),
        Defaults->bSurveyAsBuiltOneToOneOrCalibratedMaterialClaimed);
    TestFalse(TEXT("Landmark is outside the existing authored-core clip"),
        Defaults->bInsideExistingAuthoredCoreProviderClip);
    TestFalse(TEXT("Landmark adds no dedicated provider exclusion"),
        Defaults->bDedicatedProviderExclusionPolygonPresent);
    TestFalse(TEXT("Landmark makes no provider-ready successor claim"),
        Defaults->bProviderReadyLiveSuccessorClaimed);
    TestFalse(TEXT("Provider defaults unavailable"), Defaults->bProviderReady);
    TestTrue(TEXT("Persistent dedicated overlay defaults visible"),
        Defaults->bDedicatedOverlayVisible);
    TestFalse(TEXT("Runtime provider telemetry binding begins unresolved"),
        Defaults->bRuntimeProviderTelemetryBindingValid);
    TestFalse(TEXT("Runtime asset fail-closed state begins clear"),
        Defaults->bRuntimeAssetContractFailClosed);
    TestTrue(TEXT("Stable actor tag is present"),
        Defaults->ActorHasTag(
            ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedActorTag()));
    TestEqual(
        TEXT("Cooked mesh object path is exact"),
        ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedMeshObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/R24MacDonaldHouse/SM_IPV5D_R24_MacDonaldHouse_Render.SM_IPV5D_R24_MacDonaldHouse_Render")));

    const FTransform Placement =
        ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedPlacementTransform();
    TestTrue(TEXT("OSM-bound centimetre translation is exact"),
        Placement.GetTranslation().Equals(
            FVector(36320.390052826, 87155.365772797, 0.0),
            0.001));
    TestTrue(TEXT("OSM-bound disclosed non-uniform scale is exact"),
        Placement.GetScale3D().Equals(
            FVector(1.4799104705961, 1.98130612769146, 1.3),
            0.000001));
    TestTrue(TEXT("Facade-fit yaw is exact"),
        Placement.Rotator().Equals(
            FRotator(0.0, -161.885822122792, 0.0),
            0.0001));

    const UStaticMeshComponent* Component =
        Defaults->DedicatedRenderOnlyComponent;
    TestNotNull(TEXT("Dedicated renderer exists"), Component);
    if (Component)
    {
        TestEqual(TEXT("Dedicated renderer is attached to the root"),
            Component->GetAttachParent(), Defaults->GetRootComponent());
        TestEqual(TEXT("Dedicated renderer is static"),
            Component->Mobility, EComponentMobility::Static);
        TestNull(TEXT("Class default does not runtime-load SourceAssets"),
            Component->GetStaticMesh());
        TestEqual(TEXT("Dedicated renderer is NoCollision"),
            Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestTrue(TEXT("Dedicated renderer ignores every channel"),
            Component->GetCollisionResponseToChannels() ==
                FCollisionResponseContainer(ECR_Ignore));
        TestFalse(TEXT("Dedicated renderer generates no overlaps"),
            Component->GetGenerateOverlapEvents());
        TestFalse(TEXT("Dedicated renderer affects no navigation"),
            Component->CanEverAffectNavigation());
        TestTrue(TEXT("Dedicated renderer casts presentation shadows"),
            Component->CastShadow);
        TestTrue(TEXT("Dedicated renderer participates in the main pass"),
            Component->bRenderInMainPass);
        TestTrue(TEXT("Dedicated renderer participates in the depth pass"),
            Component->bRenderInDepthPass);
        TestFalse(TEXT("Dedicated renderer writes no custom depth"),
            Component->bRenderCustomDepth);
        TestEqual(TEXT("Dedicated renderer has no material overrides"),
            Component->GetNumOverrideMaterials(), 0);
        TestNull(TEXT("Dedicated renderer has no overlay material"),
            Component->GetOverlayMaterial());
        TestTrue(TEXT("Persistent renderer defaults visible"),
            Component->IsVisible());
        TestFalse(TEXT("Persistent renderer defaults not hidden in game"),
            Component->bHiddenInGame);
        TestFalse(TEXT("Dedicated renderer is not a debug-only overlay"),
            Component->ComponentHasTag(TEXT("TRIADHumanOnlyOverlay")));
    }

    ATRIADIstanaExploreV5DMacDonaldHouseActor* TransientActor =
        NewObject<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);
    TestNotNull(TEXT("Transient provider-transition test actor"), TransientActor);
    if (TransientActor && TransientActor->DedicatedRenderOnlyComponent)
    {
        FString TelemetryError;
        TestTrue(TEXT("Provider false telemetry records"),
            TransientActor->RecordProviderReadyState(false, TelemetryError));
        TestTrue(TEXT("Overlay visible at provider false"),
            TransientActor->DedicatedRenderOnlyComponent->IsVisible() &&
            !TransientActor->DedicatedRenderOnlyComponent->bHiddenInGame &&
            TransientActor->bDedicatedOverlayVisible);
        TestTrue(TEXT("Provider true telemetry records"),
            TransientActor->RecordProviderReadyState(true, TelemetryError));
        TestTrue(TEXT("Overlay remains visible at provider true"),
            TransientActor->DedicatedRenderOnlyComponent->IsVisible() &&
            !TransientActor->DedicatedRenderOnlyComponent->bHiddenInGame &&
            TransientActor->bDedicatedOverlayVisible);
        TestTrue(TEXT("Provider false restore telemetry records"),
            TransientActor->RecordProviderReadyState(false, TelemetryError));
        TestTrue(TEXT("Overlay remains visible after false-true-false"),
            TransientActor->DedicatedRenderOnlyComponent->IsVisible() &&
            !TransientActor->DedicatedRenderOnlyComponent->bHiddenInGame &&
            TransientActor->bDedicatedOverlayVisible);
        TestFalse(TEXT("Final observed provider state is false"),
            TransientActor->bProviderReady);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DMacDonaldHouseAssetContractTest,
    "TRIAD.Istana.ExploreV5D.MacDonaldHouse.AssetContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DMacDonaldHouseAssetContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const FString& Path =
        ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedMeshObjectPath();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
    TestNotNull(TEXT("Exact cooked MacDonald House mesh loads"), Mesh);
    if (!Mesh)
    {
        return false;
    }
    TestEqual(TEXT("Cooked mesh path remains exact"), Mesh->GetPathName(), Path);
    FString ContractError;
    const bool bValid =
        ATRIADIstanaExploreV5DMacDonaldHouseActor::ValidateRuntimeMesh(
            Mesh,
            ContractError);
    if (!bValid)
    {
        AddError(ContractError);
    }
    return bValid;
}

#endif
