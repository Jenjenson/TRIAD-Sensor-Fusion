#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreLandscapeActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Additive, visual-only landscape for the Istana Explore map.
 *
 * The composition follows lawful public-reference cues (open formal lawn,
 * layered flanking beds and mature broadleaf framing), but is explicitly not
 * an individual-tree inventory, species assertion, survey, or April-2024
 * planting record.  V1-V5 maps and the distant OSM context remain untouched.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreLandscapeActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreLandscapeActor();

    /** Assign imported/reused assets without creating any instances. */
    bool ConfigureExploreAssets(
        UStaticMesh* InBroadleafTree,
        UStaticMesh* InShrubA,
        UStaticMesh* InShrubB,
        UStaticMesh* InGroundcover,
        UStaticMesh* InGrassClump,
        UStaticMesh* InTrunkCollisionProxy,
        FString& OutError);

    /** Rebuild the deterministic Explore V1 planting composition. */
    bool PopulateDeterministicLandscape(FString& OutError);

    /** Persist the exact distant-building component inherited from V5. */
    bool RecordPreservedDistantContext(
        const UStaticMeshComponent* Component,
        FString& OutError);

    /** Compare live/reloaded distant buildings against that persisted record. */
    bool ValidatePreservedDistantContext(
        const UStaticMeshComponent* Component,
        FString& OutError) const;

    /** Fail-closed map/editor readback for placement and collision policy. */
    bool ValidateExploreLandscape(FString& OutReport) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BroadleafTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Shrubs")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstancesA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Shrubs")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstancesB;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Shrubs")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HedgeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Groundcover")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundcoverInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> NearGrassInstances;

    /** Hidden simple cylinders: Pawn-only blockers, never sensor/visibility truth. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Collision")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeTrunkPawnBlockers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Truth")
    FString ClaimLabel =
        TEXT("ISTANA_LIKE_PUBLIC_REFERENCE_LANDSCAPE_APPROXIMATION_NOT_BOTANICAL_INVENTORY_NOT_SURVEY_CONTROLLED");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Truth")
    bool bExactSpeciesOrCultivarsClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Truth")
    bool bExactIndividualTreePlacementClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Truth")
    bool bVegetationUsedForSensorTruth = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Layout")
    float ClearCeremonialAxisHalfWidthMeters = 16.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Layout")
    int32 DeterministicPlacementSeed = 0x1757A6A5;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Preservation")
    FString PreservedDistantContextMeshPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Preservation")
    TArray<FString> PreservedDistantContextMaterialPaths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Preservation")
    FTransform PreservedDistantContextRelativeTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore|Preservation")
    FTransform PreservedDistantContextWorldTransform = FTransform::Identity;

private:
    static double TerrainHeightMeters(double X, double Y);
    static bool IsFiniteMesh(const UStaticMesh* Mesh);
    static void ConfigureVisualInstances(
        UHierarchicalInstancedStaticMeshComponent* Component,
        int32 StartCullDistance,
        int32 EndCullDistance,
        bool bCastShadow);
    void ClearAllInstances();
    bool ValidateVisualComponent(
        const UHierarchicalInstancedStaticMeshComponent* Component,
        int32 ExpectedInstances,
        const TCHAR* Label,
        FString& OutError) const;
};
