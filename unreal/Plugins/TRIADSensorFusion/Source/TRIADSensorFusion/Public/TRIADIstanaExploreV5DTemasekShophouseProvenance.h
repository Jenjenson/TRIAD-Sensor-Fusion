#pragma once

#include "Engine/AssetUserData.h"
#include "TRIADIstanaExploreV5DTemasekShophouseProvenance.generated.h"

class UStaticMesh;

/**
 * Cooked admission record for the R24B Temasek Shophouse render mesh.
 * Every positive visual field is paired with explicit negative authority:
 * this data never promotes the public-exterior interpretation to surveyed,
 * collision, navigation, sensor-occlusion, propagation, or RF-material truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DTemasekShophouseProvenance : public UAssetUserData
{
    GENERATED_BODY()

public:
    UTRIADIstanaExploreV5DTemasekShophouseProvenance();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString SourceOutputSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString RenderObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString CanonicalGeometrySha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString PlacementReceiptSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString OSMFeatureGeometrySha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString OSMPartGeometrySha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance|Foliage Handoff")
    FString FoliageLayoutSchema;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance|Foliage Handoff")
    FString FoliageRenderOwnerClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance|Foliage Handoff")
    FTransform FoliageSourcePlacementTransform;

    /** Exact R24 source-local metre anchors, in umbrella/dome/high-fork order. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance|Foliage Handoff")
    TArray<FVector> FoliageTreeAnchorsLocalMeters;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance|Foliage Handoff")
    int32 BakedFoliageRenderComponents = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance|Foliage Handoff")
    bool bFoliageRenderedByLandmarkVegetationActor = false;

    /** Canonical path below ProjectDir; deliberately independent of a checkout drive/root. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString SourceObjProjectRelativePath;

    /** Canonical byte-stream schema used for the cooked LOD0 payload digest. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString CookedRenderPayloadSchema;

    /**
     * SHA-256 recomputed from cooked LOD0 positions, indices, sections, and
     * material assignments. This is an accidental-drift guard, not a signed
     * supply-chain or survey-authority claim.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    FString CookedRenderPayloadSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    int32 Components = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    int32 SourceVertices = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    int32 Triangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    int32 MaterialSlots = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool bProceduralTextureFreePbrPresentation = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool bProceduralPbrMaterialsCalibrated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool bRenderOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool bCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool bSurveyAsBuiltOrOneToOne = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool bProviderReadyLiveSuccessor = false;

    void SetCanonicalContract(const FString& InCookedRenderPayloadSha256);

    /** Recompute the canonical cooked raster LOD0 render-payload digest. */
    static bool ComputeCookedRenderPayloadSha256(
        const UStaticMesh* Mesh,
        FString& OutSha256,
        FString& OutError);

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana|V5D Temasek Shophouse Provenance")
    bool IsCanonicalContract() const;
};
