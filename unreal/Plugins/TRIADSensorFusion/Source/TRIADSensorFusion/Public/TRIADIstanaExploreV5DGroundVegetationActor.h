#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.generated.h"

class ATRIADIstanaExploreV4LandscapeActor;
class ATRIADIstanaExploreV5BVisualActor;
class ATRIADIstanaExploreV5DContextPolicyActor;
class ATRIADIstanaPublicViewSceneActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Artistic season/moisture look-development control. Values are deliberately
 * not a statement about the current date, irrigation, maintenance or weather.
 */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV5DSeasonProfile : uint8
{
    HumidWet,
    Transition,
    DryStressPreview
};

/** Exact visual assets consumed by the V5D ground/vegetation detail layer. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DGroundVegetationAssets
{
    GENERATED_BODY()

    /** Exact public-view terrain mesh, reused by one 2 mm render-only overlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Ground")
    TObjectPtr<UStaticMesh> GroundOverlayMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Ground")
    TObjectPtr<UMaterialInterface> GroundOverlayMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Grass")
    TObjectPtr<UStaticMesh> GrassMicroDetailMesh = nullptr;

    /** Exact order: manicured, humid, shade, dry edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Grass")
    TArray<TObjectPtr<UMaterialInterface>> GrassProfileMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Edge Grass")
    TObjectPtr<UStaticMesh> EdgeGrassMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Edge Grass")
    TObjectPtr<UMaterialInterface> EdgeGrassMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Soil")
    TObjectPtr<UStaticMesh> SoilPatchMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Soil")
    TObjectPtr<UMaterialInterface> SoilPatchMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UStaticMesh> ShrubMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UMaterialInterface> ShrubMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UStaticMesh> UnderstoreyMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UMaterialInterface> UnderstoreyMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UStaticMesh> FlowerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UMaterialInterface> FlowerMaterial = nullptr;
};

/** Pure world-space output shared by live population and native automation. */
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DGroundVegetationLayout
{
    TArray<FTransform> GrassManicured;
    TArray<FTransform> GrassHumid;
    TArray<FTransform> GrassShade;
    TArray<FTransform> GrassDryEdge;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> SupplementalSoil;
    TArray<FTransform> SupplementalShrubs;
    TArray<FTransform> SupplementalUnderstorey;
    TArray<FTransform> SupplementalFlowers;

    int32 GrassTotal() const
    {
        return GrassManicured.Num() + GrassHumid.Num() +
            GrassShade.Num() + GrassDryEdge.Num();
    }
};

/**
 * Additive visual micro-detail for the V5D hybrid map.
 *
 * Source V5B turf transforms, mesh, census, materials and simulation state plus
 * every V4 vegetation component remain untouched. During Play only, V5D takes
 * an exact full snapshot of the inherited turf renderer and retains it as a
 * visible medium-range layer behind the denser V5D near grass. Only its
 * render cull and LOD-distance policy is extended (40--65 m, scale 1.10), then
 * the exact visibility, material array, cull/LOD state and ownership tag are
 * restored on every failure and EndPlay.
 * The source terrain mesh, material, transform, collision, navigation and RF
 * authority are also preserved.  Only its renderer visibility is switched at
 * runtime: it remains visible while the exact V5D local fallback is visible,
 * and is hidden only while that validated policy says the provider is ready
 * and the fallback is hidden.  Every owned primitive is render-only,
 * NoCollision and excluded from navigation. Placement/materials are visual
 * assumptions rather than survey, botanical, sensor or RF truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DGroundVegetationActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DGroundVegetationActor();

    bool ConfigureGroundVegetationRealism(
        ATRIADIstanaPublicViewSceneActor* InPublicViewScene,
        ATRIADIstanaExploreV4LandscapeActor* InV4Landscape,
        ATRIADIstanaExploreV5BVisualActor* InV5BVisual,
        const FTRIADIstanaExploreV5DGroundVegetationAssets& InAssets,
        ETRIADIstanaExploreV5DSeasonProfile InSeasonProfile,
        FString& OutError);

    bool ReapplyGroundVegetationRealism(FString& OutError);
    bool ValidateGroundVegetationRealism(FString& OutReport) const;

    /** Read-only exact component-to-deterministic-saved-layout instance check. */
    bool ValidateOwnedGrassInstanceTransforms(FString& OutReport) const;

    /**
     * Read-only R32 handoff for the four exact R23 grass-profile transform
     * rosters.  It never rebuilds, moves, hides, or edits the source layer.
     */
    bool GetMediumDistanceTurfSourceProfilesR32(
        TArray<FTransform>& OutManicured,
        TArray<FTransform>& OutHumid,
        TArray<FTransform>& OutShade,
        TArray<FTransform>& OutDryEdge,
        FString& OutReport) const;

    /** Current serialized grass-layout presentation revision. */
    static int32 ExpectedGrassPresentationRevision();

    /** Only predecessor admitted by the actor-side R20 migration. */
    static int32 ExpectedPredecessorGrassPresentationRevision();

    /** Only predecessor admitted by the actor-side R23 migration. */
    static int32 ExpectedR23PredecessorGrassPresentationRevision();

    /** Cold, read-only exact admission gate for the serialized R14 predecessor. */
    bool ValidateR14PredecessorForR20Migration(FString& OutReport) const;

    /**
     * Cold in-place deterministic R14 -> R20 rebuild. R20 is idempotent;
     * every rejected or failed predecessor is restored to its exact prior
     * actor/component state before false is returned.
     */
    bool RebuildGroundVegetationLayoutToR20(FString& OutError);

    /** Cold, read-only exact admission gate for the serialized R20 predecessor. */
    bool ValidateR20PredecessorForR23Migration(FString& OutReport) const;

    /**
     * Cold in-place deterministic R20 -> R23 rebuild. R23 is idempotent;
     * every rejected or failed predecessor is restored to its exact prior
     * actor/component state before false is returned.
     */
    bool RebuildGroundVegetationLayoutToR23(FString& OutError);

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DGroundVegetationAssets& Assets,
        FString& OutError);

    static bool BuildDeterministicLayout(
        const TArray<FTransform>& V5BGrassWorldTransforms,
        const TArray<FTransform>& V4EdgeGrassWorldTransforms,
        const TArray<FTransform>& TreeWorldTransforms,
        const TArray<FTransform>& ExistingV5BSoilWorldTransforms,
        ETRIADIstanaExploreV5DSeasonProfile SeasonProfile,
        FTRIADIstanaExploreV5DGroundVegetationLayout& OutLayout,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static int32 ExpectedSourceGrassCount();
    static int32 ExpectedSourceEdgeGrassCount();
    static int32 ExpectedSourceTreeCount();
    static int32 ExpectedExistingSoilCount();
    static int32 ExpectedGrassMicroDetailCount();
    static int32 ExpectedSupplementalSoilCount();
    static int32 ExpectedEdgeGrassCount(
        ETRIADIstanaExploreV5DSeasonProfile SeasonProfile);

    /** Exact compact irregular-ellipse mask shared with the provider clip. */
    static FVector2D ExpectedProviderSiteClipCenterMeters();
    static FVector2D ExpectedProviderSiteClipSemiAxesMeters();
    static int32 ExpectedProviderSiteClipSplinePoints();
    /** Full-opacity authored cover outside the active provider clip. */
    static double ExpectedGroundOverlayOpaqueCollarMeters();
    static double ExpectedGroundOverlayOutwardFeatherMeters();
    static double ExpectedGroundOverlayDitherCellMeters();
    static const FString& ExpectedEdgeGrassFadeMaterialPath();
    static bool ValidateNativeEdgeGrassFadeCookDependency(
        FString& OutReport);

    /** Exact V5D-runtime presentation tag; absent from every cold editor map. */
    static const FName& RuntimeSourceTurfPresentationTag();
    static int32 ExpectedRuntimeSourceTurfCullStartDistanceCm();
    static int32 ExpectedRuntimeSourceTurfCullEndDistanceCm();
    static float ExpectedRuntimeSourceTurfLodDistanceScale();

    /** Does not call back into V5B validation; safe for V5B's admitted gate. */
    bool ValidateAppliedRuntimeSourceTurfPresentationForSourceActor(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport) const;

    static bool ValidateActiveRuntimeSourceTurfPresentationForSourceActor(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport);

    /** Restores the exact cold array immediately before V5B mutates layout. */
    static bool SuspendActiveRuntimeSourceTurfPresentationForV5BReapply(
        ATRIADIstanaExploreV5BVisualActor* Candidate,
        ATRIADIstanaExploreV5DGroundVegetationActor*& OutSuspendedOwner,
        FString& OutReport);

    /** Freshly snapshots and reapplies only after cold V5B validation passes. */
    static bool ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply(
        ATRIADIstanaExploreV5DGroundVegetationActor* SuspendedOwner,
        ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport);

    /** Continuous pre-dither coverage used by native seam-contract tests. */
    static double EvaluateGroundOverlayCoreCoverage(
        const FVector2D& WorldXYCentimeters);

    /** Stable binary world-space mask; never depends on camera, time or TAA. */
    static bool EvaluateGroundOverlayStableDitherMask(
        const FVector2D& WorldXYCentimeters);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    /** Masked fine-turf proxy; the authoritative terrain component remains untouched. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground")
    TObjectPtr<UStaticMeshComponent> GroundMacroVariationOverlay;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassManicuredInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassHumidInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassShadeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassDryEdgeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Edge Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> EdgeGrassInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Soil")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SupplementalSoilInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SupplementalShrubInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SupplementalUnderstoreyInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Understorey")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SupplementalFlowerInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ATRIADIstanaPublicViewSceneActor> PublicViewSceneActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ATRIADIstanaExploreV4LandscapeActor> V4LandscapeActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ATRIADIstanaExploreV5BVisualActor> V5BVisualActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Assets")
    FTRIADIstanaExploreV5DGroundVegetationAssets SavedAssets;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Lookdev")
    ETRIADIstanaExploreV5DSeasonProfile SeasonProfile =
        ETRIADIstanaExploreV5DSeasonProfile::HumidWet;

    /**
     * Existing maps predate this property and therefore load as the exact R14
     * predecessor. Fresh configuration serializes R23; legacy upgrades remain
     * explicit, cold and staged R14 -> R20 -> R23.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Grass")
    int32 GrassPresentationRevision = 14;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSourceTransformsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSensorOrRfMaterialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSurveyOrAsBuiltClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bBotanicalInventoryClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCurrentSeasonOrWeatherClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCesiumContentBakedCachedTracedOrAnalysedByGroundPass = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bGoogleOrOneMapContentUsed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    FVector2D GroundOverlayCoreCenterMeters = FVector2D(0.0, 55.0);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    FVector2D GroundOverlayCoreSemiAxesMeters = FVector2D(185.0, 245.0);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    int32 GroundOverlayClipSplinePoints = 64;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    double GroundOverlayOpaqueCollarMeters = 50.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    double GroundOverlayOutwardFeatherMeters = 8.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    double GroundOverlayDitherCellMeters = 0.25;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ground Seam")
    bool bGroundOverlayUsesStableWorldSpaceDitheredCoreMask = true;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bSourceTerrainRendererHiddenForReadyProvider = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bSourceTerrainRendererRestoredForFallback = true;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bLastContextPolicyStateValid = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bRuntimeSourceV5BTurfPresentationApplied = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ApplyDeferredRuntimeGroundVegetationPresentation();
    bool ExtractSourceTransforms(
        TArray<FTransform>& OutGrass,
        TArray<FTransform>& OutEdgeGrass,
        TArray<FTransform>& OutTrees,
        TArray<FTransform>& OutExistingSoil,
        FString& OutError) const;
    bool ApplyLayout(
        const FTRIADIstanaExploreV5DGroundVegetationLayout& Layout,
        FString& OutError);
    static bool BuildDeterministicLayoutForPresentationRevision(
        const TArray<FTransform>& V5BGrassWorldTransforms,
        const TArray<FTransform>& V4EdgeGrassWorldTransforms,
        const TArray<FTransform>& TreeWorldTransforms,
        const TArray<FTransform>& ExistingV5BSoilWorldTransforms,
        ETRIADIstanaExploreV5DSeasonProfile SeasonProfile,
        int32 PresentationRevision,
        FTRIADIstanaExploreV5DGroundVegetationLayout& OutLayout,
        FString& OutError);
    void ConfigureGrassPresentationComponentsForRevision(
        int32 PresentationRevision);
    bool ResolveExactContextPolicy(
        ATRIADIstanaExploreV5DContextPolicyActor*& OutPolicy,
        FString& OutError) const;
    bool SynchronizeSourceTerrainRendererWithProviderPolicy(
        FString& OutError);
    bool RestoreSourceTerrainRendering(FString& OutError);
    static int32 FindLocalRuntimeSourceV5BTurfClaims(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        const ATRIADIstanaExploreV5DGroundVegetationActor* ExcludedOwner,
        ATRIADIstanaExploreV5DGroundVegetationActor*& OutSoleClaim);
    bool EnsureRuntimeSourceV5BTurfPresentation(FString& OutError);
    bool ApplyRuntimeSourceV5BTurfPresentation(FString& OutError);
    bool ValidateRuntimeSourceV5BTurfPresentation(FString& OutError) const;
    bool RestoreRuntimeSourceV5BTurfPresentation(FString& OutError);
    bool EnsureRuntimeEdgeGrassFadePresentation(FString& OutError);
    bool ApplyRuntimeEdgeGrassFadePresentation(FString& OutError);
    bool ValidateRuntimeEdgeGrassFadePresentation(FString& OutError) const;
    bool RestoreRuntimeEdgeGrassFadePresentation(FString& OutError);
    void ClearOwnedInstances();

    UPROPERTY()
    TArray<FTransform> SavedGrassManicured;

    UPROPERTY()
    TArray<FTransform> SavedGrassHumid;

    UPROPERTY()
    TArray<FTransform> SavedGrassShade;

    UPROPERTY()
    TArray<FTransform> SavedGrassDryEdge;

    UPROPERTY()
    TArray<FTransform> SavedEdgeGrass;

    UPROPERTY()
    TArray<FTransform> SavedSupplementalSoil;

    UPROPERTY()
    TArray<FTransform> SavedSupplementalShrubs;

    UPROPERTY()
    TArray<FTransform> SavedSupplementalUnderstorey;

    UPROPERTY()
    TArray<FTransform> SavedSupplementalFlowers;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> SavedSourceTerrainMaterial;

    UPROPERTY()
    FTransform SavedSourceTerrainWorldTransform = FTransform::Identity;

    UPROPERTY()
    FTransform SavedGroundOverlayWorldTransform = FTransform::Identity;

    UPROPERTY()
    bool bSavedSourceTerrainVisible = true;

    UPROPERTY()
    bool bSavedSourceTerrainHiddenInGame = false;

    UPROPERTY(Transient)
    TWeakObjectPtr<ATRIADIstanaExploreV5DContextPolicyActor>
        RuntimeContextPolicyActor;

    UPROPERTY(Transient)
    bool bRuntimePolicyFailureWasLogged = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>>
        RuntimeOriginalSourceV5BTurfOverrideMaterials;

    UPROPERTY(Transient)
    TWeakObjectPtr<ATRIADIstanaExploreV5BVisualActor>
        RuntimeOriginalSourceV5BTurfOwnerActor;

    UPROPERTY(Transient)
    TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>
        RuntimeOriginalSourceV5BTurfComponent;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface>
        RuntimeOriginalSourceV5BTurfResolvedMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> RuntimeOriginalSourceV5BTurfMesh;

    UPROPERTY(Transient)
    TArray<FTransform> RuntimeOriginalSourceV5BTurfWorldTransforms;

    UPROPERTY(Transient)
    FTransform RuntimeOriginalSourceV5BTurfComponentTransform =
        FTransform::Identity;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfInstanceCount = 0;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfCullStartDistanceCm = 0;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfCullEndDistanceCm = 0;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfWpoDisableDistanceCm = 0;

    UPROPERTY(Transient)
    uint8 RuntimeOriginalSourceV5BTurfMobility = 0;

    UPROPERTY(Transient)
    uint8 RuntimeOriginalSourceV5BTurfCollisionEnabled = 0;

    UPROPERTY(Transient)
    TArray<uint8> RuntimeOriginalSourceV5BTurfCollisionResponses;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfForcedLodModel = 0;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfOverrideMinLod = false;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfMinLod = 0;

    UPROPERTY(Transient)
    float RuntimeOriginalSourceV5BTurfLodDistanceScale = 1.0f;

    UPROPERTY(Transient)
    int32 RuntimeOriginalSourceV5BTurfNumCustomDataFloats = 0;

    UPROPERTY(Transient)
    TArray<float> RuntimeOriginalSourceV5BTurfCustomData;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfVisible = true;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfHiddenInGame = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfHadPresentationTag = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfCastShadow = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfCastContactShadow = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfAffectDistanceFieldLighting = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfGenerateOverlapEvents = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfCanEverAffectNavigation = false;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfEnableDensityScaling = false;

    UPROPERTY(Transient)
    float RuntimeOriginalSourceV5BTurfCurrentDensityScaling = 1.0f;

    UPROPERTY(Transient)
    bool bRuntimeOriginalSourceV5BTurfAutoRebuildTree = false;

    UPROPERTY(Transient)
    bool bRuntimeSourceV5BTurfSnapshotValid = false;

    /**
     * Native-CDO hard reference used by the cooker.  Frozen V5D maps retain
     * their historical V4 edge-material pointer; this reference makes the
     * V5D-owned fade derivative an explicit native-class runtime/cook
     * dependency. The R11 transaction never dirties or saves a map; the
     * existing serialized SavedAssets pointer therefore remains byte-exact.
     */
    UPROPERTY()
    TObjectPtr<UMaterialInterface> EdgeGrassFadeMaterialCookReference;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>>
        RuntimeOriginalEdgeGrassOverrideMaterials;

    UPROPERTY(Transient)
    TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>
        RuntimeOriginalEdgeGrassComponent;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> RuntimeOriginalEdgeGrassResolvedMaterial;

    UPROPERTY(Transient)
    bool bRuntimeEdgeGrassFadeSnapshotValid = false;

    UPROPERTY(Transient)
    bool bRuntimeEdgeGrassFadePresentationApplied = false;

    UPROPERTY()
    bool bConfigured = false;
};
