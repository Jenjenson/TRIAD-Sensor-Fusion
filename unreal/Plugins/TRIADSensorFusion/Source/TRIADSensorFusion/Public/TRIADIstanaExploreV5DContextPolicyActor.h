#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.generated.h"

class ACesium3DTileset;
class ATRIADIstanaExploreV5DPublicRealmActor;
class ATRIADIstanaExploreV5DR28EnvironmentActor;
class ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor;
class ATRIADIstanaPublicViewSceneActor;
class UCesiumPolygonRasterOverlay;
class UStaticMesh;
class UStaticMeshComponent;

/** Runtime policy for dated local and streamed V5D visual context. */
UCLASS()
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DContextPolicyActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DContextPolicyActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool ValidateHybridContext(FString& OutReport) const;
    bool ValidateCurrentSurroundingsSuccessorForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const;
    bool ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const;
    bool ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const;
    bool ValidateCurrentSurroundingsBroadShellR31ForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const;
    static const FString& ExpectedCurrentSurroundingsMeshObjectPath();
    static const FString& ExpectedCurrentSurroundingsV2MeshObjectPath();
    static const FString& ExpectedOuterGroundLoadingFallbackMeshObjectPath();
    bool ConfigureCurrentSurroundingsPresentation(
        UStaticMesh* Mesh,
        ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError);
    bool ConfigureCurrentSurroundingsAndOuterGroundPresentation(
        UStaticMesh* CurrentSurroundingsMesh,
        UStaticMesh* OuterGroundLoadingFallbackMesh,
        ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError);
    /** Apply only the exact 17-entry R25 render-material override roster. */
    bool ApplyCurrentSurroundingsContextFacadeR25(FString& OutError);
    /** Replace exact R25 overrides in place with the exact 17-entry R31 roster. */
    bool ApplyCurrentSurroundingsBroadShellR31(FString& OutError);
    bool SuppressInheritedPlanningGroundPresentation(
        ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError);

    /**
     * Opt this policy into the exact two-tileset R33 roster. The legacy
     * one-tileset resolver remains unchanged until this explicit registration.
     */
    bool RegisterR33CesiumWorldTerrainController(
        ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Controller,
        ACesium3DTileset* GoogleTileset,
        ACesium3DTileset* CwtTileset,
        FString& OutError);
    void UnregisterR33CesiumWorldTerrainController(
        ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Controller);
    /** Apply only the local visual-fallback side of an R33 state transition. */
    bool ApplyR33CesiumPresentationMode(
        const ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
            Controller,
        bool bCwtPresented,
        bool bSafeLocal,
        FString& OutError);
    bool ValidateR33CesiumPresentationBinding(
        const ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
            Controller,
        FString& OutReport) const;

    /** Renderer-only policy. Source terrain collision remains authoritative. */
    bool ShouldHideSourceTerrainRendererForVisualContext() const;
    /** Whether the validated render-only R29 DEM should cover local terrain. */
    bool ShouldPresentR29CopernicusFallback() const;

    static int32 ExpectedProviderSiteClipSplinePoints();
    static FVector2D ExpectedProviderSiteClipCenterCentimeters();
    static FVector2D ExpectedProviderSiteClipSemiAxesCentimeters();
    static FVector ExpectedProviderSiteClipRippleAmplitudes();
    static FVector ExpectedProviderSiteClipRipplePhasesRadians();
    static double ExpectedProviderSiteClipSegmentParameterEpsilon();
    static FVector2D ExpectedProviderSiteClipPointCentimeters(int32 Index);
    static double EvaluateProviderSiteClipSignedInwardDistanceCentimeters(
        const FVector2D& WorldXYCentimeters);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    int64 ExpectedIonAssetId = 2275207;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    double RequiredMaximumScreenSpaceError = 1.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    float HideLocalBuildingFallbackAtLoadProgress = 98.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    float RestoreLocalBuildingFallbackBelowLoadProgress = 90.0f;
    /** Consecutive half-second readiness samples required before removing the local visual fallback. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    int32 RequiredProviderReadyConsecutiveSamples = 3;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCesiumLayerIsVisualOnly = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCesiumCollisionNavigationSensorOrRfAuthority = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCesiumStandardPersistentHttpRequestCacheAcknowledged = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bTriadReadSerializedOrLoggedProviderToken = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent = false;

    /** Dated volunteered LoD2 geometry; visual presentation only. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    UStaticMeshComponent* CurrentSurroundingsRenderOnlyComponent = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCurrentSurroundingsRenderOnly = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCurrentSurroundingsCollisionNavigationSensorOrRfAuthority = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCurrentSurroundingsMeasuredSurveyAsBuiltOrHyperreal = false;

    /** Synthetic edge continuation shown only while the provider is not ready. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    UStaticMeshComponent* OuterGroundLoadingFallbackRenderOnlyComponent = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bOuterGroundLoadingFallbackRenderOnly = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bOuterGroundLoadingFallbackSurveyAsBuilt = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bProviderContentClippedFromAuthoredCore = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    FVector2D ProviderSiteClipCenterMeters = FVector2D(0.0, 55.0);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    FVector2D ProviderSiteClipSemiAxesMeters = FVector2D(185.0, 245.0);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Visual Context")
    int32 ProviderSiteClipSplinePoints = 64;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bLocalBuildingFallbackCurrentlyHidden = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bAerialProviderHandoffRequested = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bAerialProviderHandoffCurrentlyActive = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bProviderSiteClipCurrentlyActive = true;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bAuthoredCoreVisualsCurrentlyVisible = true;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    int32 AerialProviderReadySamples = 0;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    int32 ProviderReadyConsecutiveSamples = 0;
    /**
     * Runtime-only current-view workload isolation. The saved map keeps its
     * reviewed fog-culling value; game worlds enable fog culling transiently
     * so fog-hidden tiles do not compete with the visible 1 px view.
     */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bTransientCurrentViewProviderWorkloadPolicyApplied = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime|R33")
    bool bR33DualCesiumContextConfigured = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime|R33")
    bool bR33CwtPresentationActive = false;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime|R33")
    bool bR33SafeLocalPresentationActive = false;

private:
    bool ResolveSceneAndTileset(
        ATRIADIstanaPublicViewSceneActor*& OutScene,
        ACesium3DTileset*& OutTileset,
        FString& OutError) const;
    bool ResolveSceneAndR33Tilesets(
        ATRIADIstanaPublicViewSceneActor*& OutScene,
        ACesium3DTileset*& OutGoogleTileset,
        ACesium3DTileset*& OutCwtTileset,
        FString& OutError) const;
    bool ResolveProviderSiteClipOverlay(
        ACesium3DTileset* Tileset,
        UCesiumPolygonRasterOverlay*& OutOverlay,
        FString& OutError) const;
    bool ResolvePublicRealmActor(
        ATRIADIstanaExploreV5DPublicRealmActor*& OutPublicRealm,
        FString& OutError) const;
    bool ResolveOptionalR28EnvironmentActor(
        ATRIADIstanaExploreV5DR28EnvironmentActor*& OutEnvironment,
        FString& OutError) const;
    bool ValidateOptionalR28EnvironmentCoherence(
        const ATRIADIstanaExploreV5DR28EnvironmentActor* Environment,
        bool bExpectedProviderReady,
        FString& OutReport) const;
    bool RestoreOptionalR28EnvironmentToFallback(FString& OutError);
    bool ResolveAuthoredCoreVisualActors(
        TArray<AActor*>& OutActors,
        FString& OutError) const;
    bool RestoreGroundLevelPresentation(FString& OutError);
    bool ApplyTransientCurrentViewProviderWorkloadPolicy(
        ACesium3DTileset* Tileset,
        FString& OutError);
    bool RestoreTransientCurrentViewProviderWorkloadPolicy(
        FString& OutError);
    void SetLocalBuildingFallbackVisible(
        ATRIADIstanaPublicViewSceneActor* Scene,
        bool bVisible);
    void SetR33LocalFallbackVisibility(
        ATRIADIstanaPublicViewSceneActor* Scene,
        bool bBuildingsVisible,
        bool bOuterGroundVisible);
    bool SetProviderReadyPresentation(
        ATRIADIstanaPublicViewSceneActor* Scene,
        bool bProviderReady,
        FString& OutError);
    bool ValidateCurrentSurroundingsPresentation(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError) const;

    TWeakObjectPtr<ATRIADIstanaPublicViewSceneActor> CachedScene;
    TWeakObjectPtr<ATRIADIstanaExploreV5DPublicRealmActor> CachedPublicRealm;
    TWeakObjectPtr<ATRIADIstanaExploreV5DR28EnvironmentActor>
        CachedR28Environment;
    TWeakObjectPtr<ACesium3DTileset> CachedTileset;
    TWeakObjectPtr<ACesium3DTileset> CachedR33CwtTileset;
    TWeakObjectPtr<ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor>
        CachedR33Controller;
    TWeakObjectPtr<UCesiumPolygonRasterOverlay> CachedSiteClipOverlay;

    UPROPERTY(Transient)
    bool bProviderFogCullingSnapshotValid = false;
    UPROPERTY(Transient)
    bool bProviderFogCullingBeforeTransientPolicy = false;
};
