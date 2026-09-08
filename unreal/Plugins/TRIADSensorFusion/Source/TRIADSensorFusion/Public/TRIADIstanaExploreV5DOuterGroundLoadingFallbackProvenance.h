#pragma once

#include "Engine/AssetUserData.h"
#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.generated.h"

/**
 * Cooked, negative-authority admission receipt for the bounded V5D outer
 * ground provider-loading visual. This describes a synthetic render-only
 * annulus; it is never terrain, survey, collision, navigation, sensor or RF
 * geometry.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance
    : public UAssetUserData
{
    GENERATED_BODY()

public:
    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString ContractVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString LockedSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString ObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString MtlSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString AuditSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString ManifestSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString AcceptanceLockSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString ContractSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString GeneratorSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString MaterialSlot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FString ClaimStatus;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    int32 SourceCornerVertices = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    int32 SourceTextureCoordinates = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    int32 SourceNormals = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    int32 Triangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    int32 AngularSectors = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    int32 RadialBands = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    double InnerRadiusMetres = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    double OuterRadiusMetres = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FVector BoundsMinCentimetres = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    FVector BoundsMaxCentimetres = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bExactInheritedInnerSeam = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bPhysicalMetreUv0 = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bIdentityImportScale = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bOneLod = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bNaniteFullMesh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bRasterFallbackFullMesh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bRenderOnlyProviderLoadingVisual = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bCollisionAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bShadowAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bDistanceFieldAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bTerrainSurveyOrAsBuiltAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bSensorOcclusionOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool bLiveMapOrProviderPolicyIntegrationIncluded = false;

    void SetCanonicalContract();

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana|V5D Outer Ground Loading Fallback")
    bool IsCanonicalContract() const;
};
