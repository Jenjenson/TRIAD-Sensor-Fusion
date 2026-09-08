#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.generated.h"

class ATRIADIstanaExploreV5DContextPolicyActor;
class ATRIADIstanaExploreV5DGroundVegetationActor;
class ATRIADIstanaPublicViewSceneActor;
class USceneComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Render-only Copernicus GLO-30 fallback for the V5D terrain presentation.
 *
 * Cesium World Terrain remains preferred.  While the provider is unavailable,
 * this actor presents the admitted relative-height DSM only outside the exact
 * authored-core mask.  The existing ground-macro overlay retains the occupied
 * core and the original terrain component retains all collision.  The DEM can
 * never provide collision, navigation, sensor line-of-sight, RF, geospatial,
 * survey, bare-earth, or absolute-height authority.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool ConfigureCopernicusTerrainFallback(
        UStaticMesh* InMesh,
        UMaterialInterface* InComplementaryCoreMaskMaterial,
        FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Terrain")
    bool ValidateCopernicusTerrainFallback(FString& OutReport) const;

    static const FName& ExpectedActorTag();
    static const FString& ExpectedClaimLabel();
    static const FString& ExpectedMeshObjectPath();
    static const FString& ExpectedMaterialObjectPath();
    static double ExpectedImportUniformScale();
    static FVector ExpectedBoundsMinimumCentimeters();
    static FVector ExpectedBoundsMaximumCentimeters();
    /** CPU oracle for the exact existing core-overlay coverage. */
    static double EvaluateAuthoredCoreCoverage(
        const FVector2D& WorldXYCentimeters);
    /** Exact binary complement used by the DEM opacity mask. */
    static bool EvaluateFallbackStableDitherMask(
        const FVector2D& WorldXYCentimeters);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Components")
    TObjectPtr<UStaticMeshComponent> TerrainFallbackVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    double ImportUniformScale = 100.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    FVector ImportedBoundsMinimumCentimeters =
        FVector(-100000.0, -100000.0, -3030.3865);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    FVector ImportedBoundsMaximumCentimeters =
        FVector(100000.0, 100000.0, 119.3868);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    int32 ProviderSiteClipSplinePoints = 64;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    double AuthoredCoreOpaqueCollarMeters = 50.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    double AuthoredCoreOutwardFeatherMeters = 8.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Contract")
    double StableDitherCellMeters = 0.25;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Sources")
    FString FrozenObjSha256 =
        TEXT("6057AE3C23287E9AFFED72B1C0842177A980BFE7AAB4089BF51E6FD7B2A621DF");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Sources")
    FString FrozenMtlSha256 =
        TEXT("AA98AABD87F409748B24AD344EFC43F68C3A0840CD670EC503A67597ACBEB720");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Sources")
    FString FrozenManifestSha256 =
        TEXT("06FD4D5A2F3988DAC311601F8255255C632A4FE60FC7D923F1F2C62CF08AFC5D");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Sources")
    FString FrozenSourceContractSha256 =
        TEXT("ED96EEE1B070E09F33F469747DDC27F482018959CF763A7D2B20755D6794A415");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bCesiumWorldTerrainPreferred = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bRelativeDsmVisualFallbackOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bAuthoredCoreProtectedByExactComplementaryMask = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bCollisionNavigationSensorRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bAbsoluteHeightGeospatialOrSurveyAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Truth")
    bool bBareEarthDtmClaimed = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Runtime")
    bool bFallbackCurrentlyVisible = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Runtime")
    bool bSourceTerrainRendererHiddenByThisActor = false;

private:
    bool ResolveRuntimeDependencies(
        ATRIADIstanaPublicViewSceneActor*& OutScene,
        ATRIADIstanaExploreV5DContextPolicyActor*& OutPolicy,
        ATRIADIstanaExploreV5DGroundVegetationActor*& OutGround,
        FString& OutError);
    bool SynchronizeWithProvider(FString& OutError);
    bool SetFallbackVisible(bool bVisible, FString& OutError);
    void FailClosedToSourceTerrain();

    TWeakObjectPtr<ATRIADIstanaPublicViewSceneActor> RuntimeScene;
    TWeakObjectPtr<ATRIADIstanaExploreV5DContextPolicyActor> RuntimePolicy;
    TWeakObjectPtr<ATRIADIstanaExploreV5DGroundVegetationActor> RuntimeGround;
    bool bLoggedRuntimeFailure = false;
};
