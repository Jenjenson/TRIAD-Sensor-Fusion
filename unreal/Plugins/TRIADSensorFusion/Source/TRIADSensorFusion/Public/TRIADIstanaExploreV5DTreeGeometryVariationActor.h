#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DTreeGeometryVariationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

/** Broad appearance forms only; these values are not species identities. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV5DTreeGeometryForm : uint8
{
    Umbrella,
    Dome,
    HighForkRounded,
    Columnar,
    Palm,
    Count UMETA(Hidden)
};

/** Source-domain identity required by the 736-row selector manifest. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV5DTreeGeometrySourceDomain : uint8
{
    V4Main,
    V4Heritage,
    R29Landmark
};

/**
 * One ordered source anchor supplied by a future, separately authorized
 * transaction. The transform is copied; this pass never perturbs it.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeGeometrySourceAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    FString SourceInstanceKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    ETRIADIstanaExploreV5DTreeGeometrySourceDomain SourceDomain =
        ETRIADIstanaExploreV5DTreeGeometrySourceDomain::V4Main;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    int32 SourceIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    ETRIADIstanaExploreV5DTreeGeometryForm ResolvedForm =
        ETRIADIstanaExploreV5DTreeGeometryForm::Dome;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    FTransform SourceWorldTransform = FTransform::Identity;
};

/** Exact form-major A/B/C mesh roster. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeGeometryVariationAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    TArray<TObjectPtr<UStaticMesh>> VariantMeshes;
};

/** Immutable recipe mirrored from GeometryVariationCandidateV4. */
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeGeometryVariationRecipe
{
    ETRIADIstanaExploreV5DTreeGeometryForm Form =
        ETRIADIstanaExploreV5DTreeGeometryForm::Dome;
    TCHAR Selector = TEXT('A');
    const TCHAR* AssetName = nullptr;
    float RootHoldNormalizedHeight = 0.0f;
    float CrownStartNormalizedHeight = 0.0f;
    float CrownFullNormalizedHeight = 0.0f;
    FVector2f TrunkScaleXY = FVector2f(1.0f, 1.0f);
    FVector2f CrownScaleXY = FVector2f(1.0f, 1.0f);
    float HeightScale = 1.0f;
    FVector2f BendBySourceHeightXY = FVector2f(0.0f, 0.0f);
    float TwistDegrees = 0.0f;
    float RadialRippleFraction = 0.0f;
    int32 RadialLobes = 0;
    float RadialPhaseDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeGeometryVariationBucket
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    TArray<int32> SourceOrdinals;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    TArray<FTransform> WorldTransforms;
};

/** Pure deterministic result: fifteen buckets and an exact ordered copy. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeGeometryVariationLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    TArray<FTRIADIstanaExploreV5DTreeGeometryVariationBucket> VariantBuckets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    TArray<FTransform> OrderedSourceWorldTransforms;

    /** Parallel exact routing result, one form-major A/B/C recipe index per source ordinal. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    TArray<int32> RecipeIndexBySourceOrdinal;

    int32 TotalInstances() const;
};

/**
 * Unnumbered, post-R33 visual-only geometry-variation presentation scaffold.
 *
 * This actor cannot discover or reorder anchors. A future explicit transaction
 * must supply the exact manifest order, prove source-render suppression, and
 * activate the presentation. It owns no collision, navigation, LOS, RF,
 * sensor, terrain, geospatial, survey, botanical, or current-condition truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DTreeGeometryVariationActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DTreeGeometryVariationActor();

    bool ValidateGeometryVariation(FString& OutReport) const;

    static bool BuildDeterministicLayout(
        const TArray<FTRIADIstanaExploreV5DTreeGeometrySourceAnchor>& InOrderedAnchors,
        FTRIADIstanaExploreV5DTreeGeometryVariationLayout& OutLayout,
        FString& OutError);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DTreeGeometryVariationAssets& Assets,
        FString& OutError);

    static bool GetRecipe(
        int32 RecipeIndex,
        FTRIADIstanaExploreV5DTreeGeometryVariationRecipe& OutRecipe);
    static FString CandidatePackagePath(int32 RecipeIndex);
    static FString CandidateObjectPath(int32 RecipeIndex);
    static FString ExpectedSourceInstanceKey(int32 Ordinal);
    static bool IsResolvedFormAllowed(
        int32 Ordinal,
        ETRIADIstanaExploreV5DTreeGeometryForm Form);
    static TCHAR ExpectedSelector(int32 Ordinal);
    static int32 ExpectedSourceAnchorCount();
    static int32 ExpectedVariantMeshCount();
    static const FName& ExpectedActorTag();
    static bool RuntimeCompiledTrustAnchorsConfigured();
    /** Pure, non-reflected inspection helper used to prove saved-layout receipt equality. */
    static bool ExactSourceTransformBitsMatch(
        const FTransform& Actual,
        const FTransform& Expected);
    static bool NativeInstanceTransformMatches(
        const FTransform& Actual,
        const FTransform& Expected);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Components")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> VariantComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Assets")
    FTRIADIstanaExploreV5DTreeGeometryVariationAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Layout")
    FTRIADIstanaExploreV5DTreeGeometryVariationLayout SavedLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    bool bPresentationActivated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    bool bExactSourcePresentationSuppressionProven = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    FString AcceptedR33ReceiptSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    FString FutureTransactionReceiptSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    bool bNativeVisualAcceptanceProvided = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    bool bSourceTransformsOrGeographyModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Geometry|Truth")
    bool bCollisionNavigationLosRfSensorOrTerrainAuthority = false;

private:
    /**
     * Deliberately private and uncalled. A future reviewed source change must
     * pin both runtime trust anchors, recompile, and explicitly wire the
     * assets-only transaction before configuration or visibility is reachable.
     */
    bool ConfigureGeometryVariation(
        const FTRIADIstanaExploreV5DTreeGeometryVariationAssets& InAssets,
        const TArray<FTRIADIstanaExploreV5DTreeGeometrySourceAnchor>&
            InOrderedAnchors,
        FString& OutError);

    /** Does not suppress any source component; that proof belongs to the future transaction. */
    bool ActivatePresentationAfterExactSourceSuppression(
        const FString& InAcceptedR33ReceiptSha256,
        const FString& InFutureTransactionReceiptSha256,
        bool bInExactSourcePresentationSuppressionProven,
        FString& OutError);

    void ClearOwnedInstances();
    bool PopulateComponents(FString& OutError);
};
