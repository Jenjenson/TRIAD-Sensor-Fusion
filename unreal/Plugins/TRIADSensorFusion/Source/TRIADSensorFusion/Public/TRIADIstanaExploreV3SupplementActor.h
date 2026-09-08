#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV3SupplementActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Additive, source-bound Explore V3 visual supplement.
 *
 * The frozen Explore V2 actor remains intact.  V3 adds a deliberately bounded
 * roster of CC0 mid-storey/understorey/turf instances, an identity render-only
 * portico detail layer and, when a complete generated manifest is supplied,
 * visual-only low-frequency terrain/road/water context.  None of these visual
 * components supplies visibility, LiDAR, radar, RF, navigation, or collision
 * truth.  Optional trunk blockers are separate hidden Pawn-only proxies.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV3SupplementActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV3SupplementActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLoad() override;

    /** Assign the complete required V3 mesh roster without adding instances. */
    bool ConfigureRequiredAssets(
        UStaticMesh* InIslandTree01,
        UStaticMesh* InShrub02,
        UStaticMesh* InFern02,
        UStaticMesh* InMoss01,
        UStaticMesh* InBermudaGrass,
        UStaticMesh* InAmbientCgNearTurfCard,
        UStaticMesh* InBoundedV2Broadleaf,
        UMaterialInterface* InV2TreeTrunkWindMaterial,
        UMaterialInterface* InV2TreeBranchWindMaterial,
        UMaterialInterface* InV3DarkColumnarLeafWindMaterial,
        UStaticMesh* InPawnBlockerCylinder,
        UStaticMesh* InPorticoV7V5Live,
        FString& OutError);

    /**
     * Configure all four geospatial meshes or none.  Partial generated context
     * is refused.  The manifest SHA is persisted for reload validation.
     */
    bool ConfigureOptionalVisualContext(
        UStaticMesh* InLowFrequencyTerrain,
        UStaticMesh* InOsmPublicRoads,
        UStaticMesh* InUraIndicativeRoads,
        UStaticMesh* InOsmWater,
        const FString& InGeneratedManifestSha256,
        FString& OutError);

    /** Build the exact bounded additive instance census at deterministic sites. */
    bool PopulateDeterministicSupplement(
        const TArray<FTransform>& InheritedV2ColumnarWorldTransforms,
        FString& OutError);

    /** Persist the exact inherited OSM/HDB component state from Explore V2. */
    bool RecordPreservedDistantContext(
        const UStaticMeshComponent* Component,
        FString& OutError);

    /** Compare the live/reloaded OSM/HDB component with the persisted record. */
    bool ValidatePreservedDistantContext(
        const UStaticMeshComponent* Component,
        FString& OutError) const;

    /** Start one deterministic underdamped gust; negative uses the default. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Wind")
    void TriggerWindGust(float PeakStrengthCm = -1.0f);

    /** True only after every required HISM material has a live MID in Play. */
    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V3|Wind")
    bool IsWindRuntimeActive() const;

    /** Stable readback used by editor QA wrappers; never relies on PIE paths. */
    FString BuildWindRuntimeStateReport() const;

    /** Fail-closed source, placement, collision, LOD, portico and truth readback. */
    bool ValidateExploreV3Supplement(FString& OutReport) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> IslandTree01Instances;

    /** Exact 232-transform replacement for the hidden target-map V2 columnar HISM. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DarkColumnarBroadleafInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Shrub02Instances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Fern02Instances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Moss01Instances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BermudaGrassInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> AmbientCgNearTurfInstances;

    /** Hidden, simple and Pawn-only; never a visibility or sensor proxy. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Collision")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SupplementalTreePawnBlockers;

    /** V5-native V7 detail at exact actor/component identity; render-only. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Portico")
    TObjectPtr<UStaticMeshComponent> PorticoV7RenderOnlyComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Context")
    TObjectPtr<UStaticMeshComponent> LowFrequencyTerrainComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Context")
    TObjectPtr<UStaticMeshComponent> OsmPublicRoadsComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Context")
    TObjectPtr<UStaticMeshComponent> UraIndicativeRoadsComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Context")
    TObjectPtr<UStaticMeshComponent> OsmWaterComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    FString ClaimLabel =
        TEXT("ISTANA_EXPLORE_V3_LAWFUL_PUBLIC_SOURCE_VISUAL_APPROXIMATION_NOT_ONE_TO_ONE_NOT_SURVEY_NOT_SENSOR_TRUTH");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bOneToOneOneKilometerClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bSurveyAccurateTerrainClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bExactBotanicalInventoryClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bSensorTruthAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bGoogleOrOneMapPixelsUsed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bPorticoCollisionAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Truth")
    bool bV2ColumnarVisualsReplacedWithExactTransforms = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Sources")
    FString FrozenLawfulSourceManifestSha256 =
        TEXT("CA26023F45BAA1344ED0DC031766355D734EC9B89C8B25F089ADFA18FEA12290");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Sources")
    FString FrozenPorticoObjSha256 =
        TEXT("164624CABCF0B093A7D53F87F0A3ACA5B009A001ABCA9CBD3950ECFC6336FFFD");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Sources")
    FString GeneratedContextManifestSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Context")
    bool bCompleteGeneratedContextConfigured = false;

    /** False until required credits are exposed in a distribution surface. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Sources")
    bool bPublicDistributionReady = false;

    /** Exact provider credits retained even while generated context is hidden. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Sources")
    TArray<FString> RequiredDistributionAttribution = {
        TEXT("© OpenStreetMap contributors; https://www.openstreetmap.org/copyright"),
        TEXT("Contains information from Heritage Trees and Master Plan 2019 Road Graphic accessed on 25 August 2026 from data.gov.sg under the Singapore Open Data Licence 1.0."),
        TEXT("Terrain derivative produced using Copernicus WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved."),
        TEXT("The organisations in charge of the Copernicus programme by law or by delegation do not incur any liability for any use of the Copernicus WorldDEM-30.")};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Layout")
    int32 DeterministicPlacementSeed = 0x3757A6A5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Wind")
    float BaseWindStrengthCm = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Wind")
    float GustPeakStrengthCm = 46.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Wind")
    float WindSpeed = 1.42f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Wind")
    FVector2D PrevailingWindDirection = FVector2D(0.93f, 0.37f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Wind")
    float RecoveryFrequencyHz = 0.62f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Wind")
    float RecoveryDampingRatio = 0.30f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Preservation")
    FString PreservedDistantContextMeshPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Preservation")
    TArray<FString> PreservedDistantContextMaterialPaths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Preservation")
    FTransform PreservedDistantContextRelativeTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Preservation")
    FTransform PreservedDistantContextWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V3|Preservation")
    TArray<FTransform> PreservedV2ColumnarWorldTransforms;

protected:
    virtual void BeginPlay() override;

private:
    static double TerrainHeightMeters(double X, double Y);
    static bool IsFiniteMesh(const UStaticMesh* Mesh);
    static void ConfigureVisualHism(
        UHierarchicalInstancedStaticMeshComponent* Component,
        int32 StartCullDistance,
        int32 EndCullDistance,
        int32 WpoDisableDistance,
        bool bCastShadow);
    static void ConfigureVisualStaticMesh(UStaticMeshComponent* Component);
    void ClearAllInstances();
    bool RefreshSavedAssetRuntimeBindings(FString& OutError);
    bool CreateRuntimeWindMaterialInstances(FString& OutError);
    void ApplyWindParameters();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> WindMaterialInstances;

    UPROPERTY()
    int32 ExpectedRuntimeWindMidCount = 0;

    UPROPERTY()
    int32 ColumnarTrunkMaterialSlot = INDEX_NONE;

    UPROPERTY()
    int32 ColumnarBranchMaterialSlot = INDEX_NONE;

    UPROPERTY()
    int32 ColumnarLeafMaterialSlot = INDEX_NONE;
    FRandomStream GustRandom;
    FVector2D CurrentWindDirection = FVector2D(1.0f, 0.0f);
    FVector2D WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    FVector2D ActiveGustDirection = FVector2D(1.0f, 0.0f);
    float CurrentWindStrengthCm = 0.0f;
    float WindStrengthVelocityCmPerSecond = 0.0f;
    float TimeUntilNextGustSeconds = 8.0f;
    float ActiveGustElapsedSeconds = -1.0f;
    float ActiveGustDurationSeconds = 1.8f;
    float ActiveGustPeakStrengthCm = 0.0f;
};
