#pragma once

#include "Engine/AssetUserData.h"
#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.generated.h"

/**
 * Cooked admission record for the additive V5D local-fallback render
 * derivative. The original 43,544-triangle surroundings asset and its
 * provenance remain a separate, unchanged contract.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance
    : public UAssetUserData
{
    GENERATED_BODY()

public:
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString ContractVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString SourceEpoch;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString CanonicalOutputSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString CanonicalRenderObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString FilteredRenderObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString SuppressionMetadataSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString SuppressionManifestSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    FString SuppressionOutputSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    TArray<FString> SuppressedSourceKeys;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 CanonicalTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 RenderTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 SuppressedTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 VisibleFeatures = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 VisiblePolygonParts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 SourceVertexLines = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 SourceTextureCoordinateLines = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 ImportedVertices = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    int32 ImportedVertexInstances = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bPhysicalMetreUv0Validated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bNaniteFullMesh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bRasterFallbackFullMesh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bCanonicalRenderAndRfInputsHashPinnedUnchanged = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bRenderOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bLocalFallbackOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bProviderOverlapResolved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bCollisionNavigationSensorRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool bMeasuredSurveyAsBuiltHyperreal = false;

    /** Reset every serialized field to the exact admitted V1 derivative. */
    void SetCanonicalContract();

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana|V5D Local Fallback Suppression V1")
    bool IsCanonicalContract() const;
};
