#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV2LandscapeActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Additive Explore V2 landscape and material-wind controller.
 *
 * The deterministic composition uses public-reference landscape cues: a clear
 * formal axis and fountain, layered flanking beds, mixed tropical tree habits,
 * denser turf and an outer meadow/tree frame.  It is deliberately labelled as
 * a qualitative composition, not a botanical inventory, tree survey, hidden-
 * grounds record, or one-to-one reconstruction.  Existing maps and the V5 OSM
 * context are never modified by this actor.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV2LandscapeActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV2LandscapeActor();

    virtual void Tick(float DeltaSeconds) override;

    /** Assign existing mesh assets without creating landscape instances. */
    bool ConfigureExploreV2Assets(
        UStaticMesh* InUmbrellaBroadleaf,
        UStaticMesh* InColumnarBroadleaf,
        UStaticMesh* InDomeBroadleaf,
        UStaticMesh* InPalm,
        UStaticMesh* InShrubA,
        UStaticMesh* InShrubB,
        UStaticMesh* InHedge,
        UStaticMesh* InGroundcover,
        UStaticMesh* InNearTurf,
        UStaticMesh* InMeadowSedge,
        UStaticMesh* InTrunkCollisionProxy,
        FString& OutError);

    /**
     * Apply saved, wind-capable V2 base materials.  Each broadleaf HISM uses
     * exact trunk, branch and leaf slots; turf and meadow share one exact grass
     * slot. Runtime-only MIDs are created in BeginPlay so transient objects are
     * not serialized into the map.
     */
    bool ConfigureWindMaterials(
        UMaterialInterface* InTreeTrunkWindMaterial,
        int32 InTreeTrunkSlotIndex,
        UMaterialInterface* InTreeBranchWindMaterial,
        int32 InTreeBranchSlotIndex,
        UMaterialInterface* InTreeLeafWindMaterial,
        int32 InTreeLeafSlotIndex,
        UMaterialInterface* InGrassWindMaterial,
        int32 InGrassWindSlotIndex,
        FString& OutError);

    /**
     * Rebuild the exact deterministic V2 planting composition, preserving the
     * exact 560 inherited outer-tree world positions while replacing their old
     * proxy scale/form with bounded 18-37 m mixed-canopy transforms.
     */
    bool PopulateDeterministicLandscape(
        const TArray<FTransform>& OuterTreeTransforms,
        FString& OutError);

    /** Start a gust now; a negative peak uses GustPeakStrengthCm. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Wind")
    void TriggerWindGust(float PeakStrengthCm = -1.0f);

    /** True only in play after the exact eleven-MID wind roster is live/finite. */
    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V2|Wind")
    bool IsWindRuntimeActive() const;

    /** Persist the exact distant-building component inherited from V5/V1. */
    bool RecordPreservedDistantContext(
        const UStaticMeshComponent* Component,
        FString& OutError);

    /** Compare live/reloaded distant buildings against the persisted record. */
    bool ValidatePreservedDistantContext(
        const UStaticMeshComponent* Component,
        FString& OutError) const;

    /** Fail-closed readback for census, layout, collision, truth and wind. */
    bool ValidateExploreV2Landscape(FString& OutReport) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UmbrellaBroadleafInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ColumnarBroadleafInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DomeBroadleafInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PalmInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Shrubs")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstancesA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Shrubs")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstancesB;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Shrubs")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HedgeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Groundcover")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundcoverInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> NearTurfInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MeadowSedgeInstances;

    /** Hidden simple cylinders: Pawn-only blockers, never visibility/sensor truth. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Collision")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeTrunkPawnBlockers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Truth")
    FString ClaimLabel =
        TEXT("ISTANA_PUBLIC_REFERENCE_EXPLORE_V2_QUALITATIVE_COMPOSITION_NOT_BOTANICAL_INVENTORY_NOT_SURVEY_CONTROLLED_NOT_ONE_TO_ONE_1KM_REPLICA");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Truth")
    bool bExactSpeciesOrCultivarsClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Truth")
    bool bExactIndividualTreePlacementClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Truth")
    bool bOneToOneKilometerReplicaClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Truth")
    bool bVegetationUsedForSensorTruth = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Truth")
    bool bPublicGroundsImagesUsedAsCompositionReferenceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Layout")
    float ClearCeremonialAxisHalfWidthMeters = 17.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Layout")
    FVector2D FountainCenterMeters = FVector2D(0.0f, 95.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Layout")
    float FountainClearanceRadiusMeters = 18.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Layout")
    int32 DeterministicPlacementSeed = 0x2757A6A5;

    /** Calm motion remains present between the deterministic gusts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind", meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float BaseWindStrengthCm = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind", meta = (ClampMin = "1.0", ClampMax = "150.0"))
    float GustPeakStrengthCm = 42.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind", meta = (ClampMin = "0.05", ClampMax = "10.0"))
    float WindSpeed = 1.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    FVector2D PrevailingWindDirection = FVector2D(0.93f, 0.37f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    float GustIntervalMinimumSeconds = 7.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    float GustIntervalMaximumSeconds = 14.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    float GustDurationMinimumSeconds = 1.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    float GustDurationMaximumSeconds = 3.0f;

    /** Natural frequency of the underdamped recovery spring. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float RecoveryFrequencyHz = 0.65f;

    /** Less than one is intentionally underdamped, leaving a visible after-sway. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind", meta = (ClampMin = "0.05", ClampMax = "0.95"))
    float RecoveryDampingRatio = 0.32f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    TObjectPtr<UMaterialInterface> TreeTrunkWindMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    TObjectPtr<UMaterialInterface> TreeBranchWindMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    TObjectPtr<UMaterialInterface> TreeLeafWindMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    TObjectPtr<UMaterialInterface> GrassWindMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    int32 TreeTrunkWindSlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    int32 TreeBranchWindSlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    int32 TreeLeafWindSlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    int32 GrassWindSlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Wind")
    bool bWindMaterialsConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Preservation")
    FString PreservedDistantContextMeshPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Preservation")
    TArray<FString> PreservedDistantContextMaterialPaths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Preservation")
    FTransform PreservedDistantContextRelativeTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Preservation")
    FTransform PreservedDistantContextWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V2|Preservation")
    TArray<FTransform> PreservedOuterTreeWorldTransforms;

protected:
    virtual void BeginPlay() override;

private:
    static double TerrainHeightMeters(double X, double Y);
    static bool IsFiniteMesh(const UStaticMesh* Mesh);
    static void ConfigureVisualInstances(
        UHierarchicalInstancedStaticMeshComponent* Component,
        int32 StartCullDistance,
        int32 EndCullDistance,
        bool bCastShadow);
    void ClearAllInstances();
    bool ValidateVisualComponent(
        const UHierarchicalInstancedStaticMeshComponent* Component,
        int32 ExpectedInstances,
        const TCHAR* Label,
        FString& OutError) const;
    bool ValidateWindConfiguration(FString& OutError) const;
    bool CreateRuntimeWindMaterialInstances(FString& OutError);
    void ApplyWindParameters();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> WindMaterialInstances;

    FRandomStream GustRandom;
    FVector2D CurrentWindDirection = FVector2D(1.0f, 0.0f);
    FVector2D WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    FVector2D ActiveGustDirection = FVector2D(1.0f, 0.0f);
    float CurrentWindStrengthCm = 0.0f;
    float WindStrengthVelocityCmPerSecond = 0.0f;
    float TimeUntilNextGustSeconds = 7.0f;
    float ActiveGustElapsedSeconds = -1.0f;
    float ActiveGustDurationSeconds = 1.5f;
    float ActiveGustPeakStrengthCm = 0.0f;
};
