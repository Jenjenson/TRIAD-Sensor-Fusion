#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DFountainRealismActor.generated.h"

class ATRIADIstanaExploreV5AppearanceActor;
class ATRIADIstanaExploreV5BVisualActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Serializable deterministic layout for the V5D fountain successor.
 *
 * The successor retains the inherited basin datum and source meshes, but
 * breaks the perfect-cone/perfect-ring read with bounded asymmetric spray
 * fragments, non-circular impact ripples, and a layered central disturbance.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DFountainSuccessorLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform WaterSurfaceWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FTransform> EdgeFoamWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FTransform> PrimaryJetWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FTransform> SecondarySprayWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FTransform> ImpactRippleWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform CentralPlumeWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FTransform> CentralSprayWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FTransform> CentralRippleWorldTransforms;
};

/**
 * V5D-only render successor for the inherited formal fountain.
 *
 * It reuses the exact V5B meshes, replaces the five copied V5B fountain
 * renderer components, and suppresses the legacy fountain-water section that
 * is embedded in the V5B hardscape render successor. Only that component's
 * exact M_IPV_Water override changes; its stone/metal slots and every source
 * mesh/material asset, instance transform, collision, navigation, sensor, RF,
 * and hydraulic state remain untouched. Geometry remains an appearance proxy
 * rather than a surveyed fountain or simulated operating state.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DFountainRealismActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DFountainRealismActor();

    /** Exact-map/trusted-builder configuration used by the hybrid builder. */
    bool ConfigureFountainRealism(
        ATRIADIstanaExploreV5AppearanceActor* InV5AppearanceActor,
        ATRIADIstanaExploreV5BVisualActor* InV5BVisualActor,
        UMaterialInterface* InSuccessorWaterMaterial,
        UMaterialInterface* InSuccessorSprayMaterial,
        UMaterialInterface* InEmbeddedWaterSuppressorMaterial,
        bool bTrustedUntitledHybridBuilder,
        FString& OutError);

    /** Fail-closed saved/runtime readback of geometry, assets, and truth. */
    bool ValidateFountainRealism(FString& OutReport) const;

    /** Pure deterministic builder shared by configuration and automation. */
    static bool BuildDeterministicSuccessorLayout(
        const FTransform& SourceWaterSurfaceWorldTransform,
        const FTransform& SourceEdgeFoamWorldTransform,
        const TArray<FTransform>& SourceOuterPlumeWorldTransforms,
        const TArray<FTransform>& SourceImpactRingWorldTransforms,
        const FTransform& SourceCentralPlumeWorldTransform,
        FTRIADIstanaExploreV5DFountainSuccessorLayout& OutLayout,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static const FString& ExpectedTargetMapPackage();
    static const FString& ExpectedWaterMaterialPath();
    static const FString& ExpectedSprayMaterialPath();
    static const FString& ExpectedEmbeddedWaterSuppressorMaterialPath();
    static const FName& ExpectedActorTag();
    static const FName& SourceRendererReplacementTag();

    /** Does not call back into V5B validation; safe for V5B's admitted gate. */
    static bool ValidateActiveHardscapeWaterPresentationForSourceActor(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport);

    static int32 ExpectedSourceOuterPlumeCount();
    static int32 ExpectedEdgeFoamCount();
    static int32 ExpectedPrimaryJetCount();
    static int32 ExpectedSecondarySprayCount();
    static int32 ExpectedImpactRippleCount();
    static int32 ExpectedCentralSprayCount();
    static int32 ExpectedCentralRippleCount();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WaterSurfaceComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> EdgeFoamComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PrimaryJetInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SecondarySprayInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ImpactRippleInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CentralPlumeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CentralSprayInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CentralRippleInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Sources")
    TObjectPtr<ATRIADIstanaExploreV5AppearanceActor> V5AppearanceActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Sources")
    TObjectPtr<ATRIADIstanaExploreV5BVisualActor> V5BVisualActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UStaticMesh> SavedSourceWaterSurfaceMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UStaticMesh> SavedSourceEdgeFoamMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UStaticMesh> SavedSourceOuterPlumeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UStaticMesh> SavedSourceImpactRingMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UStaticMesh> SavedSourceCentralPlumeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UMaterialInterface> SavedSourceWaterSurfaceMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UMaterialInterface> SavedSourceEdgeFoamMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UMaterialInterface> SavedSourceSprayMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UMaterialInterface> SavedSuccessorWaterMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UMaterialInterface> SavedSuccessorSprayMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UMaterialInterface> SavedEmbeddedWaterSuppressorMaterial;

    /** Exact pre-R7 V5B hardscape override array, restored on failure/EndPlay. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TArray<TObjectPtr<UMaterialInterface>> SavedHardscapeOriginalOverrideMaterials;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    TObjectPtr<UStaticMesh> SavedHardscapeRenderSuccessorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Assets")
    int32 SavedHardscapeWaterMaterialSlot = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Layout")
    FTransform SavedSourceWaterSurfaceWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Layout")
    FTransform SavedSourceEdgeFoamWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Layout")
    TArray<FTransform> SavedSourceOuterPlumeWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Layout")
    TArray<FTransform> SavedSourceImpactRingWorldTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Layout")
    FTransform SavedSourceCentralPlumeWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Layout")
    FTRIADIstanaExploreV5DFountainSuccessorLayout SavedSuccessorLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bExactV5DMapOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bInheritedFountainRenderersReplaced = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bSourceMeshMaterialAssetsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bSourceTransformsOrCensusModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bEmbeddedHardscapeWaterSectionSuppressed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bHydraulicSimulationOrOperatingStateClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Fountain|Truth")
    bool bSurveyOrAsBuiltClaimed = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void RestoreInheritedFountainRenderingForFailure();

    UPROPERTY()
    bool bConfigured = false;

    UPROPERTY()
    bool bConfiguredInTrustedUntitledHybridBuilder = false;
};
