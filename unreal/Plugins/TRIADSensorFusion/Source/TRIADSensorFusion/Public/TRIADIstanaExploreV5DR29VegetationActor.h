#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

/** Exact source-side assets admitted by the isolated R29 visual successor. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR29VegetationAssets
{
    GENERATED_BODY()

    /** Exact order: fine, broad, mixed modeled-blade carriers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Grass")
    TArray<TObjectPtr<UStaticMesh>> GrassMeshVariants;

    /** Exact order: manicured, humid, shade, dry edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Grass")
    TArray<TObjectPtr<UMaterialInterface>> GrassProfileMaterials;

    /**
     * Existing admitted high-resolution tree derivatives. Exact order:
     * spreading, dense dome, high fork, columnar, palm. These are morphology
     * cues for visual variety, never species identifications.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Trees")
    TArray<TObjectPtr<UStaticMesh>> TropicalTreeMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Planting")
    TObjectPtr<UStaticMesh> ShrubMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Planting")
    TObjectPtr<UMaterialInterface> ShrubMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Planting")
    TObjectPtr<UStaticMesh> UnderstoreyMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Planting")
    TObjectPtr<UMaterialInterface> UnderstoreyMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Planting")
    TObjectPtr<UStaticMesh> FlowerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Planting")
    TObjectPtr<UMaterialInterface> FlowerMaterial = nullptr;
};

/** One deterministic HISM population bucket. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR29VegetationBucket
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation")
    TArray<FTransform> WorldTransforms;
};

/** Serialized world-space result independent of component population. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR29VegetationLayout
{
    GENERATED_BODY()

    /** Four grass profiles x three carrier silhouettes, profile-major. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation")
    TArray<FTRIADIstanaExploreV5DR29VegetationBucket> GrassBuckets;

    /** Five admitted tropical morphology buckets. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation")
    TArray<FTRIADIstanaExploreV5DR29VegetationBucket> TreeBuckets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation")
    TArray<FTransform> Shrubs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation")
    TArray<FTransform> Understorey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation")
    TArray<FTransform> Flowers;

    int32 GrassTotal() const;
    int32 TreeTotal() const;
};

/**
 * R29 render-only successor for the isolated R28 landmark vegetation actor.
 *
 * It preserves the exact R28 world positions and census, then deterministically
 * distributes grass over three modeled-blade silhouettes and the seven trees
 * over five already admitted high-resolution tropical morphology proxies.
 * Every primitive is NoCollision, excluded from navigation, and carries no
 * sensor, RF, geospatial, botanical, survey, or current-season authority.
 * R28 and R29 are mutually exclusive render owners; this actor never hides or
 * mutates an R28 actor and therefore cannot silently double the presentation.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DR29VegetationActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR29VegetationActor();

    bool ConfigureR29Vegetation(
        const FTRIADIstanaExploreV5DR29VegetationAssets& InAssets,
        FString& OutError);

    bool ValidateR29Vegetation(FString& OutReport) const;

    static bool BuildDeterministicLayout(
        FTRIADIstanaExploreV5DR29VegetationLayout& OutLayout,
        FString& OutError);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DR29VegetationAssets& Assets,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    static int32 ExpectedGrassVariantCount();
    static int32 ExpectedGrassProfileCount();
    static int32 ExpectedGrassBucketCount();
    static int32 ExpectedGrassInstanceCount();
    static int32 ExpectedTreeMorphologyCount();
    static int32 ExpectedTreeInstanceCount();
    static int32 ExpectedNormalResponseEndDistanceCm();
    static int32 ExpectedRoughnessResponseEndDistanceCm();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Components")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Components")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> TreeComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UnderstoreyInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FlowerInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Assets")
    FTRIADIstanaExploreV5DR29VegetationAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Layout")
    FTRIADIstanaExploreV5DR29VegetationLayout SavedLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Truth")
    bool bSourceActorsOrAssetsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Truth")
    bool bSensorRfOrGeospatialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Truth")
    bool bBotanicalSurveyAsBuiltOrCurrentSeasonClaimed = false;

private:
    bool PopulateSavedLayout(FString& OutError);
    void ClearOwnedInstances();
};
