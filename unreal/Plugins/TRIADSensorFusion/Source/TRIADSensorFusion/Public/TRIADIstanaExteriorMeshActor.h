#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExteriorMeshActor.generated.h"

class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Globe-anchored owner for the project-imported Istana public exterior mesh.
 *
 * There is intentionally no procedural geometry fallback in this actor. A map
 * that requires the fidelity asset must fail validation when it is unavailable.
 */
UCLASS(Blueprintable)
class TRIADSENSORFUSION_API ATRIADIstanaExteriorMeshActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExteriorMeshActor();

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana")
    void ConfigureGeodeticPlacement(
        double InLongitudeDegrees,
        double InLatitudeDegrees,
        double InHeightMeters,
        double InFootprintYawDegrees,
        ACesiumGeoreference* InGeoreference);

    /** Assign an already validated project-owned mesh. Null is rejected. */
    bool SetExteriorMesh(UStaticMesh* InExteriorMesh, FString& OutError);

    /** Load the required soft asset. Returns false without substituting geometry. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana")
    bool LoadRequiredExteriorMesh(FString& OutError);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana")
    TObjectPtr<UStaticMeshComponent> ExteriorMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Asset")
    TSoftObjectPtr<UStaticMesh> RequiredExteriorMeshAsset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Asset")
    bool bRequiredExteriorMeshLoaded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double LongitudeDegrees = 103.84288055;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double LatitudeDegrees = 1.30709615;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double HeightMeters = 47.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Geodetic")
    double FootprintYawDegrees = 182.3;

    /**
     * A sub-percent XY expansion avoids coplanar z-fighting while this authored
     * primary is rendered. Only an explicit streamed-primary opt-in may yield
     * its pixels; this component always remains deterministic collision/offline
     * geometry.
     * It never clips, deletes, or alters surrounding Cesium tile/context.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Replacement", meta = (ClampMin = "1.0", ClampMax = "1.01"))
    double StreamedReplacementHorizontalScale = 1.004;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Replacement")
    FString StreamedReplacementStrategy = TEXT("Refined authored public-exterior renderer and collision primary; optional runtime opt-in may yield only its pixels to exact-ready streamed photogrammetry; no Cesium footprint clip or surrounding-context deletion.");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Replacement")
    FString ImportedFrontOrientationEvidence = TEXT("OBJ handedness maps source ceremonial +Y to Unreal local -Y; ESU yaw 182.3 degrees points local -Y broadly geodetic south toward the tagged facade camera.");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Provenance")
    FString FidelityStatement = TEXT("PROJECT-OWNED REFINED PUBLIC-EXTERIOR VISUAL RECONSTRUCTION; mapping/reference-image grade, not a scan, survey, BIM model, or as-built record.");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Provenance")
    FString Exclusions = TEXT("No interiors, security routes, concealed systems, or restricted operational details.");
};
