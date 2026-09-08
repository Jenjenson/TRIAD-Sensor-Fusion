#pragma once

#include "Engine/AssetUserData.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.generated.h"

/**
 * Cooked, machine-readable admission contract for the exact 2026-08-31
 * public-OSM surroundings render derivative. The negative authority fields
 * are part of the contract: presence of this data never promotes the mesh to
 * survey, collision, navigation, sensor, propagation, or RF truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DCurrentSurroundingsProvenance
    : public UAssetUserData
{
    GENERATED_BODY()

public:
    UTRIADIstanaExploreV5DCurrentSurroundingsProvenance();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    FString SourceEpoch;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    FString OutputSetSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    FString RenderObjSha256;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    int32 SelectedFeatures = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    int32 PolygonParts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    int32 SourceVertices = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    int32 SourceVertexInstances = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    int32 Triangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    bool bPhysicalMetreUv0Validated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    bool bRenderOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    bool bCollisionNavigationSensorRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    bool bMeasuredSurveyAsBuiltHyperreal = false;

    /** Reset every serialized field to the one admitted cooked contract. */
    void SetCanonicalContract();

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana|V5D Current Surroundings Provenance")
    bool IsCanonicalContract() const;
};
