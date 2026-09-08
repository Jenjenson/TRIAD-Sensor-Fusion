#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5BVisualActor.generated.h"

class ATRIADIstanaExploreV4LandscapeActor;
class ATRIADIstanaPublicViewSceneActor;
class UHierarchicalInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Complete asset handoff for the additive Explore V5B render layer.
 *
 * Every pointer is required. The hardscape row is a render-only successor of
 * the exact collision/RF-authoritative source; its three materials retain the
 * effective Stone/Water/Metal order. The four Pachira rows are morphology
 * variants, not a claim about the Istana's species or planting inventory.
 * Repeated material pointers are allowed; mesh pointers remain exact and
 * distinct so an accidentally combined import cannot silently pass preflight.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5BAssetRoster
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Grass")
    TObjectPtr<UStaticMesh> AccentTurfMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Grass")
    TObjectPtr<UMaterialInterface> AccentTurfMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Hardscape")
    TObjectPtr<UStaticMesh> HardscapeRenderSuccessorMesh = nullptr;

    /** Exact order: M_IPV_Stone, M_IPV_Water, M_IPV_Metal. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Hardscape")
    TArray<TObjectPtr<UMaterialInterface>> HardscapeRenderSuccessorMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UStaticMesh> FormalBedVeneerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UMaterialInterface> FormalBedVeneerMaterial = nullptr;

    /** Borrowed exact full V4 roster; V5B owns the render-only successor copies. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UStaticMesh> FormalBedShrubMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UMaterialInterface> FormalBedShrubMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UStaticMesh> FormalBedFlowerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UMaterialInterface> FormalBedFlowerMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UStaticMesh> FormalBedUnderstoreyMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UMaterialInterface> FormalBedUnderstoreyMaterial = nullptr;

    /** Small irregular render-only patch reused beneath selected V4 tree roots. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Tree Grounding")
    TObjectPtr<UStaticMesh> TreeBaseMulchMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UStaticMesh> FountainSurfaceMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UMaterialInterface> FountainSurfaceMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UStaticMesh> FountainEdgeFoamMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UMaterialInterface> FountainEdgeFoamMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UStaticMesh> OuterPlumeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UMaterialInterface> OuterPlumeMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UStaticMesh> ImpactRingMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UMaterialInterface> ImpactRingMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UStaticMesh> CentralPlumeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UMaterialInterface> CentralPlumeMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Pavers")
    TObjectPtr<UStaticMesh> InnerPaverWedgeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Pavers")
    TObjectPtr<UStaticMesh> OuterPaverWedgeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Pavers")
    TObjectPtr<UMaterialInterface> PaverMaterial = nullptr;

    /** Exact order: bark a, bark b, bark c, bark d. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TArray<TObjectPtr<UStaticMesh>> PachiraBarkMeshes;

    /** Exact order: leaves a, leaves b, leaves c, leaves d. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TArray<TObjectPtr<UStaticMesh>> PachiraLeavesMeshes;

    /** Exact order matches PachiraBarkMeshes; entries may share one material. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TArray<TObjectPtr<UMaterialInterface>> PachiraBarkMaterials;

    /** Exact order matches PachiraLeavesMeshes; entries may share one material. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TArray<TObjectPtr<UMaterialInterface>> PachiraLeavesMaterials;
};

/** Read-only description of one exact V4 tree instance used by the pure builder. */
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5BTreeSource
{
    FTransform WorldTransform = FTransform::Identity;
    double SourceMeshHeightCm = 0.0;
    int32 SourceComponentIndex = INDEX_NONE;
    int32 SourceInstanceIndex = INDEX_NONE;
};

/**
 * Pure, inspectable world-space result shared by runtime population and tests.
 * The raw four-element array is deliberately not a reflected nested container.
 */
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5BDeterministicLayout
{
    TArray<int32> AccentTurfSourceIndices;
    TArray<FTransform> AccentTurfWorldTransforms;
    FTransform FormalBedVeneerWorldTransform = FTransform::Identity;
    TArray<FTransform> FormalBedShrubCorrectionWorldTransforms;
    TArray<FTransform> FormalBedFlowerCorrectionWorldTransforms;
    TArray<FTransform> FormalBedUnderstoreyCorrectionWorldTransforms;
    TArray<int32> TreeBaseSourceComponentIndices;
    TArray<int32> TreeBaseSourceInstanceIndices;
    TArray<FTransform> TreeBaseMulchWorldTransforms;
    TArray<FTransform> TreeBaseShrubWorldTransforms;
    TArray<FTransform> TreeBaseUnderstoreyWorldTransforms;
    FTransform FountainSurfaceWorldTransform = FTransform::Identity;
    FTransform FountainEdgeFoamWorldTransform = FTransform::Identity;
    TArray<FTransform> OuterPlumeWorldTransforms;
    TArray<FTransform> ImpactRingWorldTransforms;
    FTransform CentralPlumeWorldTransform = FTransform::Identity;
    TArray<FTransform> InnerPaverWorldTransforms;
    TArray<FTransform> OuterPaverWorldTransforms;
    TArray<FTransform> PachiraWorldTransformsByVariant[4];
};

/**
 * Additive V5B visual-detail actor for the public-data Istana approximation.
 *
 * It owns a render-only hardscape successor, dense grass, raised soil mounds,
 * fountain envelopes, paving accents, and synthetic tropical morphology
 * proxies. It never replaces or contributes collision, navigation, sensor
 * geometry, RF material authority, survey/as-built geometry, or a botanical
 * inventory.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5BVisualActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5BVisualActor();

    /**
     * Validate every input, snapshot the exact public hardscape plus 18,432
     * V4 lawn transforms and full planting rosters, then atomically replace
     * only render presentation while preserving source collision authority.
     */
    bool ConfigureExploreV5BVisuals(
        ATRIADIstanaPublicViewSceneActor* InPublicViewSceneActor,
        ATRIADIstanaExploreV4LandscapeActor* InExploreV4LandscapeActor,
        const FTRIADIstanaExploreV5BAssetRoster& InAssets,
        FString& OutError);

    /** Restore the persisted assets and exact saved deterministic transforms. */
    bool ReapplyExploreV5BVisuals(FString& OutError);

    /** Fail-closed saved/runtime readback of meshes, materials, transforms and policy. */
    bool ValidateExploreV5BVisuals(FString& OutReport) const;

    /** Runtime-only facade MIDs and isolated fill-light readback. */
    bool ValidateRuntimeFacadePresentation(FString& OutReport) const;

    /** Pure preflight used before any component or persistent-state mutation. */
    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5BAssetRoster& Assets,
        FString& OutError);

    /** Pure deterministic transform builder shared by Configure and automation. */
    static bool BuildDeterministicLayout(
        const TArray<FTransform>& CloseTurfWorldTransforms,
        FTRIADIstanaExploreV5BDeterministicLayout& OutLayout,
        FString& OutError);

    /** Move only each exact V4 formal prefix; preserve its broader flank suffix. */
    static bool BuildFormalBedCorrections(
        const TArray<FTransform>& ShrubSourceWorldTransforms,
        const TArray<FTransform>& FlowerSourceWorldTransforms,
        const TArray<FTransform>& UnderstoreySourceWorldTransforms,
        FTRIADIstanaExploreV5BDeterministicLayout& InOutLayout,
        FString& OutError);

    /** Add a bounded read-only grounding layer around an exact V4 tree census. */
    static bool BuildTreeGrounding(
        const TArray<FTRIADIstanaExploreV5BTreeSource>& TreeSources,
        FTRIADIstanaExploreV5BDeterministicLayout& InOutLayout,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static int32 ExpectedCloseTurfSourceCount();
    static int32 ExpectedAccentTurfCount();
    static int32 ExpectedFormalBedShrubCorrectionCount();
    static int32 ExpectedFormalBedFlowerCorrectionCount();
    static int32 ExpectedFormalBedUnderstoreyCorrectionCount();
    static int32 ExpectedTreeBaseMulchCount();
    static int32 ExpectedTreeBaseShrubCount();
    static int32 ExpectedTreeBaseUnderstoreyCount();
    static int32 ExpectedInnerPaverCount();
    static int32 ExpectedOuterPaverCount();
    static int32 ExpectedOuterPlumeCount();
    static int32 ExpectedImpactRingCount();
    static int32 ExpectedLogicalPachiraCount();
    static int32 ExpectedPachiraInstancesPerVariant();
    static int32 AccentSourceIndexForOrdinal(int32 AccentOrdinal);
    static double ExpectedFountainSurfaceZCm();
    static double ExpectedOuterPlumeOriginZCm();
    static double ExpectedCentralPlumeOriginZCm();
    static double ExpectedPaverZCm();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> AccentTurfInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Hardscape")
    TObjectPtr<UStaticMeshComponent> HardscapeRenderSuccessorComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Facade")
    TObjectPtr<UDirectionalLightComponent> FacadeFillLightComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FormalBedVeneerComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FormalBedShrubCorrections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FormalBedFlowerCorrections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Formal Beds")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FormalBedUnderstoreyCorrections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tree Grounding")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeBaseMulchInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tree Grounding")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeBaseShrubInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tree Grounding")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeBaseUnderstoreyInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FountainSurfaceComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FountainEdgeFoamComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> OuterPlumeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ImpactRingInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Fountain")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CentralPlumeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Pavers")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> InnerPaverInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Pavers")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> OuterPaverInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraBarkAInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraBarkBInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraBarkCInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraBarkDInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraLeavesAInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraLeavesBInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraLeavesCInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Tropical Morphology Proxy")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PachiraLeavesDInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Targets")
    TObjectPtr<ATRIADIstanaPublicViewSceneActor> PublicViewSceneActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Targets")
    TObjectPtr<ATRIADIstanaExploreV4LandscapeActor> ExploreV4LandscapeActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Assets")
    FTRIADIstanaExploreV5BAssetRoster SavedAssetRoster;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bRenderOnlyGeometry = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bSensorOrRfMaterialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bSurveyOrAsBuiltClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bBotanicalInventoryClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bPachiraUsedOnlyAsSyntheticMorphologyProxy = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bSyntheticBedLocationsClaimedObserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bFountainHydraulicSimulationClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5B|Truth")
    bool bGoogleOrOneMapContentUsed = false;

private:
    friend class FTRIADIstanaExploreV5BOldSchemaRollbackTest;
    friend class FTRIADIstanaExploreV5BMaximinRegressionTest;

    /**
     * Select the strongest canonical farthest-point traversal without
     * recomputing distances to the complete selected prefix on every step.
     */
    static bool SelectCanonicalMaximinCandidateIndices(
        const TArray<FVector2D>& CanonicalCandidateRoots,
        int32 SelectionCount,
        TArray<int32>& OutBestSelection,
        double& OutBestMinimumDistanceSquared);

    virtual void BeginPlay() override;
    bool ApplyRuntimeFacadePresentation(FString& OutError);
    bool ValidateOwnedComponentTopology(FString& OutError) const;
    bool ExtractCloseTurfWorldTransforms(
        const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
        TArray<FTransform>& OutTransforms,
        FString& OutError) const;
    bool ExtractFormalBedSourceWorldTransforms(
        const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
        TArray<FTransform>& OutShrubs,
        TArray<FTransform>& OutFlowers,
        TArray<FTransform>& OutUnderstorey,
        FString& OutError) const;
    bool ExtractTreeSources(
        const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
        TArray<FTRIADIstanaExploreV5BTreeSource>& OutSources,
        FString& OutError) const;
    bool ValidateHardscapeRenderSuccessorSource(
        const ATRIADIstanaPublicViewSceneActor* SourceActor,
        const FTRIADIstanaExploreV5BAssetRoster& Assets,
        FString& OutError) const;
    bool ApplyLayout(
        const FTRIADIstanaExploreV5BAssetRoster& Assets,
        const FTRIADIstanaExploreV5BDeterministicLayout& Layout,
        FString& OutError);
    bool ValidateAppliedLayout(
        const FTRIADIstanaExploreV5BAssetRoster& Assets,
        const FTRIADIstanaExploreV5BDeterministicLayout& Layout,
        FString& OutError) const;
    void SaveLayout(const FTRIADIstanaExploreV5BDeterministicLayout& Layout);
    FTRIADIstanaExploreV5BDeterministicLayout LoadSavedLayout() const;
    void ClearOwnedVisuals();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeFacadeMaterialInstances;

    UPROPERTY(Transient)
    bool bRuntimeFacadePresentationApplied = false;

    UPROPERTY()
    TArray<int32> PreservedAccentSourceIndices;

    UPROPERTY()
    TArray<FTransform> PreservedAccentSourceWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedAccentTurfWorldTransforms;

    UPROPERTY()
    FTransform SavedFormalBedVeneerWorldTransform = FTransform::Identity;

    UPROPERTY()
    TArray<FTransform> SavedFormalBedShrubCorrectionWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedFormalBedFlowerCorrectionWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedFormalBedUnderstoreyCorrectionWorldTransforms;

    UPROPERTY()
    TArray<int32> SavedTreeBaseSourceComponentIndices;

    UPROPERTY()
    TArray<int32> SavedTreeBaseSourceInstanceIndices;

    UPROPERTY()
    TArray<FTransform> SavedTreeBaseMulchWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedTreeBaseShrubWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedTreeBaseUnderstoreyWorldTransforms;

    UPROPERTY()
    FTransform SavedFountainSurfaceWorldTransform = FTransform::Identity;

    UPROPERTY()
    FTransform SavedFountainEdgeFoamWorldTransform = FTransform::Identity;

    UPROPERTY()
    TArray<FTransform> SavedOuterPlumeWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedImpactRingWorldTransforms;

    UPROPERTY()
    FTransform SavedCentralPlumeWorldTransform = FTransform::Identity;

    UPROPERTY()
    TArray<FTransform> SavedInnerPaverWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedOuterPaverWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedPachiraVariantAWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedPachiraVariantBWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedPachiraVariantCWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedPachiraVariantDWorldTransforms;

    UPROPERTY()
    bool bConfigured = false;
};
