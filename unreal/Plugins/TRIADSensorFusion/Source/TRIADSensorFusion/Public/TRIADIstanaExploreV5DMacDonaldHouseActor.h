#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DMacDonaldHouseActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class ATRIADIstanaExploreV5DContextPolicyActor;

/**
 * Dedicated R24A MacDonald House public-exterior renderer.
 *
 * The actor is a persistent, dedicated visual layer. Global Cesium readiness
 * is observed for telemetry only because it is not landmark-specific proof;
 * provider transitions never hide this overlay. No provider exclusion or
 * provider-ready live-successor claim is implied, and no collision,
 * navigation, sensor, or RF authority is created.
 */
UCLASS()
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DMacDonaldHouseActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DMacDonaldHouseActor();

    static const FName& ExpectedActorTag();
    static const FString& ExpectedMeshObjectPath();
    static FTransform ExpectedPlacementTransform();
    static bool ValidateRuntimeMesh(UStaticMesh* Mesh, FString& OutError);

    bool ConfigureMacDonaldHouse(
        UStaticMesh* Mesh,
        bool bInProviderReady,
        FString& OutReport);

    /** Record global provider state without changing overlay visibility. */
    bool RecordProviderReadyState(bool bInProviderReady, FString& OutError);

    bool ValidateMacDonaldHouse(FString& OutReport) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House")
    UStaticMeshComponent* DedicatedRenderOnlyComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Composition")
    bool bProviderReady = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Composition")
    bool bDedicatedOverlayVisible = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Composition")
    bool bInsideExistingAuthoredCoreProviderClip = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Composition")
    bool bDedicatedProviderExclusionPolygonPresent = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Composition")
    bool bProviderReadyLiveSuccessorClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Truth")
    bool bRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Truth")
    bool bCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Truth")
    bool bSurveyAsBuiltOneToOneOrCalibratedMaterialClaimed = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Runtime")
    bool bRuntimeProviderTelemetryBindingValid = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|MacDonald House|Runtime")
    bool bRuntimeAssetContractFailClosed = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool SynchronizeWithContextPolicy(FString& OutReport);

    TWeakObjectPtr<ATRIADIstanaExploreV5DContextPolicyActor>
        RuntimeContextPolicyActor;
    bool bRuntimePolicyFailureWasLogged = false;
    double NextRuntimeContractAuditTimeSeconds = 0.0;
};
