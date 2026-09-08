#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

/** The two deterministic R25 landmark planting patches. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV5DLandmarkVegetationSite : uint8
{
    MacDonaldHouse,
    TemasekShophouse
};

/**
 * Exact visual assets consumed by the landmark vegetation layer. The grass
 * carrier and every non-grass asset are reused without mutation. The four
 * grass materials are isolated R27 or R28 derivatives. R27 changes only the
 * landmark-specific 65--90 m stable-visibility envelope; R28 additionally
 * applies the visual-evidence-driven healthy-tropical blade-colour correction
 * while preserving the carrier, graph topology and truth boundary.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DLandmarkVegetationAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TObjectPtr<UStaticMesh> GrassCarrierMesh = nullptr;

    /** Exact order: manicured, humid, shade, dry edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TArray<TObjectPtr<UMaterialInterface>> GrassProfileMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TObjectPtr<UStaticMesh> UmbrellaTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TObjectPtr<UStaticMesh> DomeTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TObjectPtr<UStaticMesh> HighForkTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UStaticMesh> ShrubMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UMaterialInterface> ShrubMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UStaticMesh> UnderstoreyMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UMaterialInterface> UnderstoreyMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UStaticMesh> FlowerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UMaterialInterface> FlowerMaterial = nullptr;
};

/** Serialized world-space transforms for one landmark planting patch. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TArray<FTransform> GrassManicured;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TArray<FTransform> GrassHumid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TArray<FTransform> GrassShade;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TArray<FTransform> GrassDryEdge;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TArray<FTransform> TreesUmbrella;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TArray<FTransform> TreesDome;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TArray<FTransform> TreesHighFork;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TArray<FTransform> Shrubs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TArray<FTransform> Understorey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TArray<FTransform> Flowers;

    int32 GrassTotal() const
    {
        return GrassManicured.Num() + GrassHumid.Num() +
            GrassShade.Num() + GrassDryEdge.Num();
    }
};

/** Combined deterministic, serialized world-space landmark layout. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DLandmarkVegetationLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation")
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout MacDonaldHouse;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation")
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout TemasekShophouse;
};

/**
 * Additive landmark vegetation presentation used by the R26 map successor.
 *
 * Every owned primitive is render-only, has no collision or navigation
 * influence, and carries no sensor/RF authority. Placement is deterministic
 * world-space look-development around the exact landmark actor anchors. The
 * source V4/V5B/V5D packages are referenced but never modified. Locations and
 * planting forms are assumption-bound, not survey, botanical or seasonal fact.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DLandmarkVegetationActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DLandmarkVegetationActor();

    bool ConfigureLandmarkVegetation(
        const FTRIADIstanaExploreV5DLandmarkVegetationAssets& InAssets,
        FString& OutError);

    bool ValidateLandmarkVegetation(FString& OutReport) const;

    /** Configure the additive R28 density/colour/mature-canopy successor. */
    bool ConfigureLandmarkVegetationR28(
        const FTRIADIstanaExploreV5DLandmarkVegetationAssets& InAssets,
        FString& OutError);

    /** Validate the actor specifically as the R28 visual successor. */
    bool ValidateLandmarkVegetationR28(FString& OutReport) const;

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DLandmarkVegetationAssets& Assets,
        FString& OutError);

    static bool BuildDeterministicLayout(
        FTRIADIstanaExploreV5DLandmarkVegetationLayout& OutLayout,
        FString& OutError);

    static bool ValidateLayout(
        const FTRIADIstanaExploreV5DLandmarkVegetationLayout& Layout,
        FString& OutError);

    static bool ValidateAssetRosterR28(
        const FTRIADIstanaExploreV5DLandmarkVegetationAssets& Assets,
        FString& OutError);

    static bool BuildDeterministicLayoutR28(
        FTRIADIstanaExploreV5DLandmarkVegetationLayout& OutLayout,
        FString& OutError);

    static bool ValidateLayoutR28(
        const FTRIADIstanaExploreV5DLandmarkVegetationLayout& Layout,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    /** Exact R24 source-to-runtime foliage handoff, shared with provenance. */
    static const FString& ExpectedTemasekFoliageLayoutSchema();
    static const FString& ExpectedTemasekFoliageRenderOwnerClass();
    static const TArray<FVector>& ExpectedTemasekTreeAnchorsLocalMeters();
    static FTransform ExpectedTemasekSourcePlacementTransform();
    static FVector ExpectedSiteAnchorCentimeters(
        ETRIADIstanaExploreV5DLandmarkVegetationSite Site);
    static double ExpectedSiteYawDegrees(
        ETRIADIstanaExploreV5DLandmarkVegetationSite Site);
    static int32 ExpectedGrassCount(
        ETRIADIstanaExploreV5DLandmarkVegetationSite Site);
    static int32 MaximumGrassInstancesPerSite();
    static int32 ExpectedGrassCullStartDistanceCm();
    static int32 ExpectedGrassCullEndDistanceCm();
    static int32 ExpectedGrassWpoDisableDistanceCm();
    static float ExpectedGrassLodDistanceScale();
    static double ExpectedGrassCoverageScaleMinimum();
    static double ExpectedGrassCoverageScaleMaximum();
    static double ExpectedGrassHeightScaleMinimum();
    static double ExpectedGrassHeightScaleMaximum();
    static double ExpectedEvidenceViewDistanceCm();
    static double ExpectedMaterialVisibilityAtEvidenceView();
    static double ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView();

    static const FString& ExpectedR28ClaimLabel();
    static int32 ExpectedR28GrassCount(
        ETRIADIstanaExploreV5DLandmarkVegetationSite Site);
    static int32 MaximumR28GrassInstancesPerSite();
    static double ExpectedR28HardscapeClearanceCm();
    static double ExpectedR28NominalCarrierCoverageMinimum();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassManicuredInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassHumidInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassShadeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassDryEdgeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UmbrellaTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DomeTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HighForkTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UnderstoreyInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Planting")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FlowerInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Assets")
    FTRIADIstanaExploreV5DLandmarkVegetationAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Layout")
    FTRIADIstanaExploreV5DLandmarkVegetationLayout SavedLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Truth")
    bool bSourceAssetPackagesModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Truth")
    bool bSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Truth")
    bool bBotanicalSurveyOrCurrentSeasonClaimed = false;

private:
    bool PopulateSavedLayout(FString& OutError);
    void ClearOwnedInstances();
};
