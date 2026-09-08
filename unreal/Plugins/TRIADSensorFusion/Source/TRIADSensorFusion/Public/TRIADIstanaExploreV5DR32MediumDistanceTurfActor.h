#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.generated.h"

class ATRIADIstanaExploreV5DGroundVegetationActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

/** Exact R29 meshes plus isolated R32 render-only material derivatives. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets
{
    GENERATED_BODY()

    /** Exact order: fine, broad, mixed modeled-blade carriers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R32 Turf")
    TArray<TObjectPtr<UStaticMesh>> GrassMeshVariants;

    /** Exact order: manicured, humid, shade, dry edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R32 Turf")
    TArray<TObjectPtr<UMaterialInterface>> GrassProfileMaterials;
};

/** One deterministic profile/silhouette HISM population. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR32MediumDistanceTurfBucket
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf")
    TArray<FTransform> WorldTransforms;
};

/** Four grass profiles x three modeled silhouettes, profile-major. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf")
    TArray<FTRIADIstanaExploreV5DR32MediumDistanceTurfBucket> GrassBuckets;

    int32 Total() const;
};

/**
 * Bounded medium-distance lawn-detail bridge.
 *
 * Exactly one quarter of each immutable R23 profile is selected by a
 * versioned, deterministic spatial hash and copied into twelve render-only
 * HISM buckets. World translations stay exact. A second independent hash
 * removes mesh-carrier repetition and adds bounded yaw, XY scale, and 5--9 cm
 * local tip-height variation. Isolated R32 material derivatives add a
 * continuous, screen-band-limited 11 m / 3.4 m organic colour-and-roughness
 * response over the 20--65 m readability band while preserving the exact
 * R29 opacity, WPO, normal, and 65--90 m visibility policies. The layer never
 * owns collision, navigation,
 * sensor, RF, geospatial, botanical, survey, or current-condition truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DR32MediumDistanceTurfActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR32MediumDistanceTurfActor();

    bool ConfigureR32MediumDistanceTurf(
        ATRIADIstanaExploreV5DGroundVegetationActor* InSourceGroundVegetation,
        const FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& InAssets,
        FString& OutError);

    bool ValidateR32MediumDistanceTurf(FString& OutReport) const;

    static bool BuildDeterministicLayout(
        const TArray<FTransform>& Manicured,
        const TArray<FTransform>& Humid,
        const TArray<FTransform>& Shade,
        const TArray<FTransform>& DryEdge,
        FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& OutLayout,
        FString& OutError);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& Assets,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    static const FName& ExpectedComponentTag();
    static int32 ExpectedSourceInstanceCount();
    static int32 ExpectedInstanceCount();
    static int32 ExpectedSelectionDenominator();
    static int32 ExpectedProfileCount();
    static int32 ExpectedMeshVariantCount();
    static int32 ExpectedBucketCount();
    static int32 ExpectedProfileQuota(int32 ProfileIndex);
    static int32 ExpectedCullStartDistanceCm();
    static int32 ExpectedCullEndDistanceCm();
    static int32 ExpectedWpoDisableDistanceCm();
    static double ExpectedMinimumTipHeightCm();
    static double ExpectedMaximumTipHeightCm();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Components")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Source")
    TObjectPtr<ATRIADIstanaExploreV5DGroundVegetationActor> SourceGroundVegetation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Assets")
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Layout")
    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout SavedLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    bool bSourceActorsOrAssetsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    bool bSensorRfOrGeospatialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Truth")
    bool bBotanicalSurveyAsBuiltOrCurrentConditionClaimed = false;

    /**
     * Actor-side acceptance claims are forbidden. A later native capture may
     * be accepted only by its external immutable receipt; this stays false.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R32 Turf|Delivery")
    bool bVisualCaptureAccepted = false;

private:
    bool PopulateSavedLayout(FString& OutError);
    void ClearOwnedInstances();
};
