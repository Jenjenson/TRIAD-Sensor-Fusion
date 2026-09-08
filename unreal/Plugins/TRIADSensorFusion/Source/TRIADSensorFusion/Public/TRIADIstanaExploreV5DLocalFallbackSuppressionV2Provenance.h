#pragma once

#include "Engine/AssetUserData.h"
#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.generated.h"

/**
 * Cooked admission record for the additive V2 local-fallback render
 * derivative. V1 and the canonical 43,544-triangle surroundings contract
 * remain separate and unchanged.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance
    : public UAssetUserData
{
    GENERATED_BODY()

public:
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString ContractVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString SourceEpoch;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString CanonicalOutputSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString CanonicalRenderObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString FilteredRenderObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString SuppressionMetadataSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString SuppressionManifestSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    FString SuppressionOutputSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    TArray<FString> SuppressedSourceKeys;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 CanonicalTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 RenderTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 SuppressedTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 VisibleFeatures = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 VisiblePolygonParts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 SourceVertexLines = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 SourceTextureCoordinateLines = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 ImportedVertices = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    int32 ImportedVertexInstances = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bPhysicalMetreUv0Validated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bNaniteFullMesh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bRasterFallbackFullMesh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bCanonicalRenderAndRfInputsHashPinnedUnchanged = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bRenderOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bLocalFallbackOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bProviderOverlapResolved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bCollisionNavigationSensorRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool bMeasuredSurveyAsBuiltHyperreal = false;

    /** Reset every serialized field to the exact admitted V2 derivative. */
    void SetCanonicalContract();

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana|V5D Local Fallback Suppression V2")
    bool IsCanonicalContract() const;
};
