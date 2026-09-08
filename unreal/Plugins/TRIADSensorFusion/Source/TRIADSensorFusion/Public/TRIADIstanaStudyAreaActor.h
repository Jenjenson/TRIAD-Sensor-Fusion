#pragma once

#include "GameFramework/Actor.h"
#include "TRIADIstanaStudyAreaActor.generated.h"

class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Human-visible, non-colliding 1 km simulation boundary and AOI metadata. */
UCLASS(BlueprintType, Blueprintable)
class TRIADSENSORFUSION_API ATRIADIstanaStudyAreaActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaStudyAreaActor();
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana")
    void ConfigureStudyArea(
        double InCenterLongitudeDegrees,
        double InCenterLatitudeDegrees,
        double InCenterHeightMeters,
        double InRadiusMeters,
        int32 InBoundarySegments,
        ACesiumGeoreference* InGeoreference);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "TRIAD|Istana")
    void RebuildBoundary();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|AOI")
    double CenterLongitudeDegrees = 103.84288055;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|AOI")
    double CenterLatitudeDegrees = 1.30709615;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|AOI")
    double CenterHeightMeters = 47.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|AOI", meta = (ClampMin = "1.0"))
    double RadiusMeters = 1000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|AOI", meta = (ClampMin = "16", ClampMax = "256"))
    int32 BoundarySegments = 64;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Provenance")
    FString StudyAreaStatement = TEXT("Exact WGS84 geodesic 1000 m simulation AOI; not a legal, security, or property boundary.");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Provenance")
    FString TerrainStatement = TEXT("A flat 47 m collision fallback derives from public 30 m SRTM and is not survey-grade; loaded Cesium collision may add surrounding structure detail.");

    /**
     * Invisible, colliding simulation surface covering the exact AOI. This
     * keeps placement traces deterministic when streamed Cesium terrain has
     * not materialized, while remaining explicitly non-survey-grade.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Terrain")
    TObjectPtr<UStaticMeshComponent> ApproximateTerrainFallback;

private:
    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Boundary")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BoundarySegmentsMesh;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Boundary")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BoundaryPosts;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Boundary")
    TObjectPtr<UTextRenderComponent> BoundaryLabel;
};
