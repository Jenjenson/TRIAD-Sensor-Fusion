#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h"
#include "TRIADIstanaExploreV5DOuterContextVegetationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

/** Preferred A/B/C meshes plus mandatory admitted broad-form fallback meshes. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOuterContextVegetationAssets
{
    GENERATED_BODY()

    /** Optional preferred roster, form-major A/B/C. It is either empty or exactly twelve. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<TObjectPtr<UStaticMesh>> VariantMeshes;

    /** Required order: umbrella, dome, highForkRounded, columnar. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<TObjectPtr<UStaticMesh>> FallbackMeshes;
};

/**
 * One externally established visual terrain contact. The candidate deliberately
 * contains no Z value, so the runtime scaffold accepts no implicit/default Z.
 */
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOuterContextTerrainContact
{
    FString PlacementKey;
    TOptional<double> ZMeters;
};

/**
 * Evidence supplied by a future reviewed assets-only integration transaction.
 * Caller values are necessary but never sufficient: compiled trust anchors are
 * intentionally unset in this source-only delivery.
 */
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOuterContextVegetationAdmission
{
    FString CandidateContractSha256;
    FString PlacementDatasetSha256;
    FString AcceptedR33ReceiptSha256;
    FString FutureAssetsOnlyAuthorizationReceiptSha256;
    FString TerrainContactReceiptSha256;
    FString TerrainContactPayloadSha256;
    FString PublicDistributionReceiptSha256;
    FString RuntimeVisibleAttributionText;
    FString RuntimeAttributionSurfaceId;
    bool bAcceptedR33HumanVisualReviewProven = false;
    bool bFutureAssetsOnlyIntegrationExplicitlyAuthorized = false;
    bool bTerrainContactZExternallyProven = false;
    bool bDerivativeDatabaseOdblShareAlikeSatisfied = false;
    bool bDerivativeDatabaseMachineReadableAccessSatisfied = false;
    bool bRuntimeVisibleOpenStreetMapAttributionSatisfied = false;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOuterContextVegetationBucket
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<int32> PlacementOrdinals;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<FTransform> LocalTransforms;
};

/** Deterministic exact-XY/yaw/scale layout plus externally proven Z. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOuterContextVegetationLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<FTRIADIstanaExploreV5DOuterContextVegetationBucket> VariantBuckets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<FString> OrderedPlacementKeys;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<FTransform> OrderedLocalTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<int32> VariantIndexByPlacementOrdinal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<int64> XEastMicrometers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<int64> YSouthMicrometers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<int32> YawMillidegrees;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<int32> UniformScaleMillionths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    TArray<double> TerrainContactZMeters;

    int32 TotalInstances() const;
};

/** Immutable row mirrored from the sanitized 145-placement candidate. */
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement
{
    const TCHAR* PlacementKey = nullptr;
    int32 VariantIndex = INDEX_NONE;
    int64 XEastMicrometers = 0;
    int64 YSouthMicrometers = 0;
    int32 YawMillidegrees = 0;
    int32 UniformScaleMillionths = 1000000;
};

/**
 * Unnumbered post-R33 source-only presentation scaffold for the exact 145-row
 * outer-context vegetation candidate.
 *
 * It owns render-only HISM components and never samples terrain, binds a map,
 * changes geography, or provides collision, navigation, LOS, RF, sensor,
 * terrain, geospatial, survey, botanical, or current-inventory authority.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DOuterContextVegetationActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DOuterContextVegetationActor();

    bool ValidateOuterContextVegetation(FString& OutReport) const;

    static bool BuildDeterministicLayout(
        const TArray<FTRIADIstanaExploreV5DOuterContextTerrainContact>&
            InOrderedTerrainContacts,
        FTRIADIstanaExploreV5DOuterContextVegetationLayout& OutLayout,
        FString& OutError);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DOuterContextVegetationAssets& InAssets,
        FString& OutError);

    static bool GetExpectedPlacement(
        int32 Ordinal,
        FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement&
            OutPlacement);
    static int32 ExpectedPlacementCount();
    static int32 ExpectedVariantMeshCount();
    static const FString& ExpectedAttributionText();
    static const FString& ExpectedCandidateContractSha256();
    static const FString& ExpectedPlacementDatasetSha256();
    static const FName& ExpectedActorTag();
    static bool CanRenderInPresentationState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState State);
    static bool CompiledTrustAnchorsConfigured();
    static bool CandidateDistributionGatesDeclaredSatisfied();
    static bool NativeInstanceTransformMatches(
        const FTransform& Actual,
        const FTransform& Expected);

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    FString GetRuntimeVisibleAttributionText() const
    {
        return RuntimeVisibleAttributionText;
    }

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V5D|Outer Vegetation")
    bool IsRuntimeVisibleAttributionSatisfied() const
    {
        return bRuntimeVisibleAttributionSatisfied;
    }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Components")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>
        VariantComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Assets")
    FTRIADIstanaExploreV5DOuterContextVegetationAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Layout")
    FTRIADIstanaExploreV5DOuterContextVegetationLayout SavedLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Runtime")
    ETRIADIstanaExploreV5DR33TerrainPresentationState PresentationState =
        ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Attribution")
    FString RuntimeVisibleAttributionText;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Attribution")
    FString RuntimeAttributionSurfaceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bAdmissionSatisfied = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bTerrainContactZExternallyProven = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bRuntimeVisibleAttributionSatisfied = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bPublicDistributionGatesSatisfied = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bSourceXYKeysYawOrScaleModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bCollisionNavigationLosRfSensorTerrainOrGeospatialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bMapBindingImplemented = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Outer Vegetation|Truth")
    bool bNativeVisualAcceptanceProvided = false;

private:
    /**
     * Deliberately private and uncalled. A future reviewed source change must
     * pin trust anchors, implement the owned attribution presenter, and
     * explicitly wire the assets-only transaction before this can mutate state.
     */
    bool ConfigureOuterContextVegetation(
        const FTRIADIstanaExploreV5DOuterContextVegetationAssets& InAssets,
        const TArray<FTRIADIstanaExploreV5DOuterContextTerrainContact>&
            InOrderedTerrainContacts,
        const FTRIADIstanaExploreV5DOuterContextVegetationAdmission&
            InAdmission,
        FString& OutError);
    bool ApplyPresentationState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState InState,
        FString& OutError);
    bool ValidateAdmission(
        const FTRIADIstanaExploreV5DOuterContextVegetationAdmission&
            InAdmission,
        FString& OutError) const;
    bool PopulateComponents(FString& OutError);
    void ClearOwnedInstances();
    void ApplyFailClosedVisibility();

    FTRIADIstanaExploreV5DOuterContextVegetationAdmission SavedAdmission;
};
