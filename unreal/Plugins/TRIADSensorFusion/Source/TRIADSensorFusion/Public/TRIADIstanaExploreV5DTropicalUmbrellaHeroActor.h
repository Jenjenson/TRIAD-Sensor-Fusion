#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary;

UENUM(BlueprintType)
enum class ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain : uint8
{
    V4Heritage,
    R29Landmark
};

/** One of the six already classified umbrella-form source identities. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    FString SourceInstanceKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain SourceDomain =
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::V4Heritage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    int32 SourceIndex = INDEX_NONE;

    /** Native world transform read from the existing source instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    FTransform SourceWorldTransform = FTransform::Identity;
};

/** Exact isolated candidate plus the mandatory existing TreeRealism fallback. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTropicalUmbrellaAssets
{
    GENERATED_BODY()

    /** Exact order: candidate variants A, B, C; each must contain LOD0/1/2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<TObjectPtr<UStaticMesh>> CandidateVariantMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TObjectPtr<UStaticMesh> ExistingTreeRealismUmbrellaFallback = nullptr;

    /** Exact existing fallback order: trunk, leaves, branches response. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<TObjectPtr<UMaterialInterface>> ExistingFallbackMaterials;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTropicalUmbrellaBucket
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<FString> SourceInstanceKeys;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<FTransform> WorldTransforms;
};

/** Deterministic C/B/B/B/A/B routing with a bit-exact ordered source copy. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTropicalUmbrellaLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<FTRIADIstanaExploreV5DTropicalUmbrellaBucket> VariantBuckets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<FString> OrderedSourceInstanceKeys;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<FTransform> OrderedSourceWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella")
    TArray<int32> VariantIndexBySourceOrdinal;

    int32 TotalInstances() const;
};

/**
 * Unnumbered post-R33 appearance-only scaffold. It never discovers, spawns,
 * moves, or deletes a source tree. Current source independently compiles both
 * candidate selection and presentation activation false.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor();

    static bool BuildDeterministicLayout(
        const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
            InOrderedNativeAnchors,
        FTRIADIstanaExploreV5DTropicalUmbrellaLayout& OutLayout,
        FString& OutError);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DTropicalUmbrellaAssets& Assets,
        FString& OutError);

    static FString ExpectedSourceInstanceKey(int32 IntegrationOrdinal);
    static int32 ExpectedSourceIndex(int32 IntegrationOrdinal);
    static ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain
        ExpectedSourceDomain(int32 IntegrationOrdinal);
    static TCHAR ExpectedVariantSelector(int32 IntegrationOrdinal);
    static FString CandidateMeshObjectPath(int32 VariantIndex);
    static FString CandidateMaterialObjectPath(int32 MaterialIndex);
    static FString ExistingFallbackMeshObjectPath();
    static FString ExistingFallbackMaterialObjectPath(int32 MaterialIndex);
    static int32 ExpectedSourceAnchorCount();
    static int32 ExpectedVariantCount();
    static bool RuntimeSelectionCompiled();
    static bool RuntimeActivationCompiled();
    static bool RuntimeTrustAnchorsConfigured();
    static bool ExactSourceTransformBitsMatch(
        const FTransform& Actual,
        const FTransform& Expected);

    bool ValidateDormantScaffold(FString& OutReport) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Components")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> CandidateComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Truth")
    bool bCandidateSelectionConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Truth")
    bool bPresentationActivated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Truth")
    bool bExactSourcePresentationSuppressionProven = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Truth")
    bool bExistingTreeRealismMeshAndMaterialsFallbackPreserved = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Truth")
    bool bSourceKeysTransformsOrCensusModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella|Truth")
    bool bGeospatialCollisionNavigationLosRfSensorOrTerrainAuthority = false;

private:
    friend class UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary;

    /**
     * No public/reflected caller. A reviewed edit must independently flip the
     * selection compile gate, pin both runtime trust anchors, and recompile.
     */
    bool ConfigureCandidateSelectionInternal(
        const FTRIADIstanaExploreV5DTropicalUmbrellaAssets& InAssets,
        const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
            InOrderedNativeAnchors,
        const FString& InAcceptedCurrentReceiptSha256,
        const FString& InFutureAuthorizationReceiptSha256,
        FString& OutError);

    /**
     * Independently compiled false and absent from reflection/call sites. A
     * future transaction must atomically suppress exactly the same six source
     * visuals before this can expose any candidate component.
     */
    bool ActivateAfterAtomicExactSourceSuppressionInternal(
        const FString& InAcceptedCurrentReceiptSha256,
        const FString& InFutureAuthorizationReceiptSha256,
        bool bInExactSixSourceVisualsSuppressed,
        FString& OutError);

    void ClearAndHideOwnedPresentation();
    void RollbackPresentationToMandatoryFallbackInternal();

    UPROPERTY()
    FTRIADIstanaExploreV5DTropicalUmbrellaAssets SavedAssets;

    UPROPERTY()
    FTRIADIstanaExploreV5DTropicalUmbrellaLayout SavedLayout;
};
