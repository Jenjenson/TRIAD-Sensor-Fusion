#pragma once

#include "GameFramework/Actor.h"
#include "TRIADIstanaBuildingActor.generated.h"

class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Deterministic public-exterior-only visual/physics approximation of the
 * Istana Main Building. Geometry is mapping/reference-image grade rather than
 * survey/BIM grade; it contains no interior or security-sensitive detail.
 */
UCLASS(BlueprintType, Blueprintable)
class TRIADSENSORFUSION_API ATRIADIstanaBuildingActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaBuildingActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana")
    void ConfigureGeodeticPlacement(
        double InLongitudeDegrees,
        double InLatitudeDegrees,
        double InHeightMeters,
        double InFootprintYawDegrees,
        ACesiumGeoreference* InGeoreference);

    /** Rebuild deterministic instances after an editor property change. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "TRIAD|Istana")
    void RebuildExterior();

    /**
     * Optional guarded downward trace after Cesium collision has loaded.
     * The actor is ignored by the trace; no movement occurs when no surface is hit.
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "TRIAD|Istana")
    bool TrySnapFoundationToSurface(double TraceHalfHeightMeters = 250.0);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double LongitudeDegrees = 103.84288055;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double LatitudeDegrees = 1.30709615;

    /** Approximate SRTM-30 fallback, WGS84 ellipsoid metres; not survey-grade. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double HeightMeters = 47.0;

    /** Mapping-grade PCA evidence: long axis about 2.3 degrees toward local south. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double FootprintYawDegrees = 2.3;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Provenance")
    FString FidelityStatement = TEXT("PUBLIC EXTERIOR ONLY - deterministic visual/physics approximation; not survey, BIM, or operational security data.");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Provenance")
    FString FootprintEvidence = TEXT("OSM way 41895536 mapping-grade envelope approximately 123.8 m east-west by 117.2 m north-south.");

private:
    void AddBox(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector& CenterMeters,
        const FVector& SizeMeters,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void AddCylinder(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector& CenterMeters,
        double RadiusMeters,
        double HeightMeters,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void AddSphere(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector& CenterMeters,
        double RadiusMeters);
    void ApplyMaterials();

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Masonry;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MasonryAccent;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Columns;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Roofs;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DarkDetails;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Glass;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Metal;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Stone;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Landscape;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Water;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Domes;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Foliage;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Istana|Geometry")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RoofCones;
};
