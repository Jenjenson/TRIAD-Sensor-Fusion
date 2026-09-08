#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.generated.h"

class ACesium3DTileset;
class ACesiumGeoreference;
class ATRIADIstanaExploreV5DContextPolicyActor;
class USceneComponent;
struct FCesium3DTilesetLoadFailureDetails;

/** Mutually exclusive streamed-terrain presentation modes admitted by R33. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV5DR33TerrainPresentationState : uint8
{
    GooglePrimary,
    CwtWarming,
    CwtPresented,
    SafeLocal
};

/**
 * Source-only R33 controller for an optional Cesium World Terrain visual
 * reference.  It never samples terrain heights and never owns collision,
 * navigation, line-of-sight, sensor, RF, or geospatial truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool ConfigureR33CesiumWorldTerrainReference(
        ACesium3DTileset* InGoogleTileset,
        ACesium3DTileset* InCwtTileset,
        ACesiumGeoreference* InGeoreference,
        ATRIADIstanaExploreV5DContextPolicyActor* InContextPolicy,
        FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Terrain")
    bool ValidateR33CesiumWorldTerrainReference(FString& OutReport) const;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Terrain")
    bool RequestCwtPresentation(bool bRequest, FString& OutError);

    /** One deterministic readiness sample; Tick calls this at the fixed cadence. */
    bool TickPresentationState(float DeltaSeconds, FString& OutError);

    /** Public for the owning context policy's fail-closed lifecycle only. */
    bool EnterGooglePrimary(FString& OutError);
    bool EnterCwtWarming(FString& OutError);
    bool EnterCwtPresented(FString& OutError);
    bool EnterSafeLocal(FString& OutError);

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V5D|R33 Terrain")
    ETRIADIstanaExploreV5DR33TerrainPresentationState
        GetPresentationState() const
    {
        return PresentationState;
    }

    static const FName& ExpectedControllerTag();
    static const FName& ExpectedGoogleRoleTag();
    static const FName& ExpectedCwtRoleTag();
    static const FName& ExpectedContextMemberTag();
    static int64 ExpectedGoogleIonAssetId();
    static int64 ExpectedCwtIonAssetId();
    static float ExpectedReadinessSampleIntervalSeconds();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Bindings")
    TObjectPtr<ACesium3DTileset> GoogleTileset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Bindings")
    TObjectPtr<ACesium3DTileset> CwtTileset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Bindings")
    TObjectPtr<ACesiumGeoreference> SharedGeoreference;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Bindings")
    TObjectPtr<ATRIADIstanaExploreV5DContextPolicyActor> ContextPolicy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    ETRIADIstanaExploreV5DR33TerrainPresentationState PresentationState =
        ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    int32 PresentReadySamples = 0;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    int32 RestoreFailureSamples = 0;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    bool bCwtPresentationRequested = false;

    /** A CWT load failure keeps SafeLocal active until this bounded retry delay expires. */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    bool bCwtLoadFailureRetryLatched = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    float CwtRetryBackoffRemainingSeconds = 0.0f;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Runtime")
    float CwtWarmingElapsedSeconds = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    bool bVisualReferenceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    bool bHeightSamplesPersisted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    bool bSurveyAccuracyClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    bool bVerticalDatumResolved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Truth")
    bool bCollisionNavigationLineOfSightSensorOrRfAuthority = false;

    /** Acceptance belongs only to a later immutable native capture receipt. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R33 Terrain|Delivery")
    bool bVisualAcceptance = false;

private:
    bool ResolveExactWorldRoster(
        ACesium3DTileset*& OutGoogleTileset,
        ACesium3DTileset*& OutCwtTileset,
        ACesiumGeoreference*& OutGeoreference,
        FString& OutError) const;
    bool ValidateSharedProviderSiteClip(FString& OutError) const;
    bool ApplyState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState NewState,
        FString& OutError);
    void ResetReadinessCounters();
    void ArmCwtRetryBackoff();
    void RegisterCwtLoadFailureHandler();
    void UnregisterCwtLoadFailureHandler();
    void HandleCwtTilesetLoadFailure(
        const FCesium3DTilesetLoadFailureDetails& Details);

    float ReadinessAccumulatorSeconds = 0.0f;
    bool bLoggedRuntimeFailure = false;
    FDelegateHandle CwtLoadFailureDelegateHandle;
};
