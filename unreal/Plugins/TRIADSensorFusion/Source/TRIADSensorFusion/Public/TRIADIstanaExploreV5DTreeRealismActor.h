#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.generated.h"

class ATRIADIstanaExploreV4LandscapeActor;
class ATRIADIstanaExploreV5BVisualActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMesh;

/** Exact visual-only assets used by the V5D tree realism pass. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeRealismAssets
{
    GENERATED_BODY()

    /** Exact order: umbrella, dome, high-fork, columnar, palm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Trees")
    TArray<TObjectPtr<UStaticMesh>> NearLodZeroTreeMeshes;

    /** Existing small irregular V5B patch mesh, paired with an isolated V5D edge-blended soil material. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Bases")
    TObjectPtr<UStaticMesh> TreeBaseTransitionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Tree Bases")
    TObjectPtr<UMaterialInterface> TreeBaseTransitionMaterial = nullptr;
};

/** Pure deterministic tree-base output shared by population and automation. */
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DTreeRealismLayout
{
    TArray<int32> SelectedSourceTreeIndices;
    TArray<FTransform> RootApronWorldTransforms;
    TArray<FTransform> LeafLitterWorldTransforms;
};

/**
 * V5D-only tree presentation layer.
 *
 * The 729 exact V4 transforms, census, blockers, collision, navigation and RF
 * authority remain untouched. In Play, this actor swaps only the ten visual
 * HISM mesh references to isolated V5D derivatives that retain the complete
 * source LOD chain for provenance validation. Runtime automatic LOD selection
 * exposes exact source LOD0 close to the camera and uses a 1.8 distance scale
 * to retain the admitted spreading/high-fork tropical crown cues and visible
 * branch breaks through the medium field without forcing all 729 trees to
 * LOD0. The form labels are visual proxies, never species identifications.
 * The derivative mesh slots bind thirteen isolated bounded response
 * materials. During Play, twenty-six response MIDs mirror the exact six wind
 * values from the snapshotted V4 main/Heritage MIDs after V4's tick; opacity,
 * normals and WPO remain unchanged.
 * The source bindings are snapshotted and restored at the world EndPlay
 * boundary. V5D also
 * suppresses, then restores, the inherited nominally 2.2--3.6 m V5B opaque
 * mulch aprons during Play.
 * Small irregular root and leaf-litter fragments are separately owned
 * render-only transitions.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DTreeRealismActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DTreeRealismActor();

    bool ConfigureTreeRealism(
        ATRIADIstanaExploreV4LandscapeActor* InV4Landscape,
        ATRIADIstanaExploreV5BVisualActor* InV5BVisual,
        const FTRIADIstanaExploreV5DTreeRealismAssets& InAssets,
        FString& OutError);

    bool ValidateTreeRealism(FString& OutReport) const;
    bool ApplyRuntimeTreeVisualOverride(FString& OutError);
    void RestoreRuntimeTreeVisualOverride();

    /** Does not call back into V4 validation; safe for V4's admitted override gate. */
    bool ValidateAppliedRuntimeOverrideForSourceActor(
        const ATRIADIstanaExploreV4LandscapeActor* Candidate,
        FString& OutReport) const;

    static bool ValidateActiveRuntimeOverrideForSourceActor(
        const ATRIADIstanaExploreV4LandscapeActor* Candidate,
        FString& OutReport);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DTreeRealismAssets& Assets,
        const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
        FString& OutError);

    static bool BuildDeterministicTreeBaseLayout(
        const TArray<FTransform>& SourceTreeWorldTransforms,
        const TArray<FTransform>& ExistingTreeBaseWorldTransforms,
        FTRIADIstanaExploreV5DTreeRealismLayout& OutLayout,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    static const FName& RuntimeOverrideTag();
    static int32 ExpectedRuntimeTreeMinimumLod();
    static int32 ExpectedRuntimeTreeForcedLodModel();
    static int32 ExpectedSourceTreeCount();
    static int32 ExpectedExistingTreeBaseCount();
    static int32 ExpectedSelectedNearTreeCount();
    static int32 ExpectedLeafLitterCount();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Bases")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RootApronInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Tree Bases")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LeafLitterInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ATRIADIstanaExploreV4LandscapeActor> V4LandscapeActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ATRIADIstanaExploreV5BVisualActor> V5BVisualActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Assets")
    FTRIADIstanaExploreV5DTreeRealismAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSourceTransformsOrCensusModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bBlockerCollisionNavigationOrRfAuthorityModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSourceAssetPackagesModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bBotanicalSurveyOrCurrentSeasonClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bContactShadowSuppressionIsVisualLookdevOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bRuntimeOverrideApplied = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ApplyRuntimeTreeVisualOverrideAtStartup();
    void HandleV4WindRuntimeReady();
    void HandleWorldBegunPlayStateChanged(bool bHasBegunPlay);
    void RemoveV4WindRuntimeReadyBinding();
    void RemoveWorldLifecycleBinding();
    bool CreateRuntimeTreeMaterialResponseInstances(FString& OutError);
    bool SyncRuntimeTreeWindParameters(
        bool bForceWrite,
        FString& OutError);
    bool ValidateRuntimeTreeMaterialResponse(FString& OutError) const;
    void ResetRuntimeTreeMaterialResponseState();

    bool ExtractSourceTreeTransforms(
        TArray<FTransform>& OutTrees,
        FString& OutError) const;
    bool ValidateOwnedTransitions(FString& OutError) const;
    bool PopulateOwnedTransitions(
        const FTRIADIstanaExploreV5DTreeRealismLayout& Layout,
        FString& OutError);
    bool ApplyRuntimeLegacyTreeBaseApronSuppression(FString& OutError);
    bool ValidateRuntimeLegacyTreeBaseApronSuppression(
        FString& OutError) const;
    void RestoreRuntimeLegacyTreeBaseApronSuppression();

    UPROPERTY()
    TArray<TObjectPtr<UStaticMesh>> SavedOriginalTreeMeshes;

    UPROPERTY()
    TArray<int32> SavedSourceComponentInstanceCounts;

    UPROPERTY()
    TArray<FTransform> SavedSourceTreeWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedExistingTreeBaseWorldTransforms;

    UPROPERTY()
    TArray<int32> SavedSelectedSourceTreeIndices;

    UPROPERTY()
    TArray<FTransform> SavedRootApronWorldTransforms;

    UPROPERTY()
    TArray<FTransform> SavedLeafLitterWorldTransforms;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMesh>> RuntimeOriginalComponentMeshes;

    UPROPERTY(Transient)
    TArray<int32> RuntimeOriginalMinLods;

    UPROPERTY(Transient)
    TArray<float> RuntimeOriginalLodDistanceScales;

    UPROPERTY(Transient)
    TArray<uint8> RuntimeOriginalCastContactShadow;

    UPROPERTY(Transient)
    TArray<int32> RuntimeMaterialOffsets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> RuntimeOriginalMaterials;

    /**
     * Isolated V5D response MIDs. Their parents are the exact managed V5D
     * slot materials; the original V4 wind MIDs remain snapshotted above and
     * authoritative for all six wind values.
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeTreeResponseMaterials;

    /** Five scalar values per response MID, in the fixed wind-parameter order. */
    TArray<float> RuntimeCachedWindScalarValues;

    /** One cached direction value per response MID. */
    TArray<FLinearColor> RuntimeCachedWindDirectionValues;

    UPROPERTY(Transient)
    bool bRuntimeLegacyTreeBaseApronSnapshotValid = false;

    UPROPERTY(Transient)
    bool bRuntimeLegacyTreeBaseApronWasVisible = true;

    UPROPERTY(Transient)
    bool bRuntimeLegacyTreeBaseApronWasHiddenInGame = false;

    FDelegateHandle V4WindRuntimeReadyHandle;
    FDelegateHandle WorldLifecycleHandle;

    UPROPERTY()
    bool bConfigured = false;
};
