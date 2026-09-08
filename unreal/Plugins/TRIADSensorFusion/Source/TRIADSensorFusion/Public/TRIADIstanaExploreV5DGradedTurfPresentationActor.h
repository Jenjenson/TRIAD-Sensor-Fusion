#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DGradedTurfPresentationActor.generated.h"

class ATRIADIstanaExploreV5DGroundVegetationActor;
class ATRIADIstanaExploreV5DR29VegetationActor;
class ATRIADIstanaExploreV5DR32MediumDistanceTurfActor;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Dormant, appearance-only post-R33 presentation boundary.
 *
 * This actor owns no lawn geometry.  Its only prospective mutation is a
 * transaction-scoped slot-0 material override on the exact existing
 * V5DGroundMacroVariationOverlay component.  Candidate selection and
 * activation are independently compiled false, the private mutation methods
 * have no production call site, and no instance is present in a numbered map.
 */
UCLASS(NotBlueprintable)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DGradedTurfPresentationActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DGradedTurfPresentationActor();

    /** Read-only admission proof.  This never snapshots or mutates a component. */
    static bool ValidateDormantPresentationInputs(
        const ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation,
        const ATRIADIstanaExploreV5DR29VegetationActor* R29Vegetation,
        const ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* R32Turf,
        UMaterialInterface* ExactAcceptedLawnFallback,
        UMaterialInterface* GradedTurfCandidate,
        FString& OutReport);

    static bool IsRuntimeCandidateSelectionCompiledAuthorized();
    static bool IsRuntimeCandidateActivationCompiledAuthorized();
    static int32 ExpectedR29OwnedPlacementCount();
    static int32 ExpectedR32OwnedPlacementCount();
    static int32 ExpectedPresentationStartDistanceCm();
    static int32 ExpectedPresentationProofEndDistanceCm();
    static int32 ExpectedBladeFadeStartDistanceCm();
    static int32 ExpectedBladeFadeEndDistanceCm();
    static int32 ExpectedSurfaceResponseFadeStartDistanceCm();
    static int32 ExpectedSurfaceResponseFadeEndDistanceCm();
    static FString ExactOverlayComponentName();
    static FString ExactOverlayMeshObjectPath();
    static FString ExactAcceptedLawnMaterialObjectPath();
    static FString CandidateMaterialObjectPath();
    static FString CandidateOutputNamespace();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Graded Turf V2")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Graded Turf V2|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Graded Turf V2|Truth")
    bool bMapGeographyTerrainOrPlacementModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Graded Turf V2|Truth")
    bool bCollisionNavigationLosRfSensorOrSimulationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Graded Turf V2|Delivery")
    bool bNativeMaterializedOrActivated = false;

private:
    struct FGroundOverlaySnapshot
    {
        TWeakObjectPtr<ATRIADIstanaExploreV5DGroundVegetationActor> Owner;
        TWeakObjectPtr<UStaticMeshComponent> Component;
        TWeakObjectPtr<USceneComponent> AttachParent;
        TWeakObjectPtr<UStaticMesh> StaticMesh;
        TWeakObjectPtr<UMaterialInterface> ResolvedSlotZeroMaterial;
        TArray<TWeakObjectPtr<UMaterialInterface>> OverrideMaterials;
        TArray<FName> ComponentTags;
        FName AttachSocketName = NAME_None;
        FTransform RelativeTransform = FTransform::Identity;
        FTransform WorldTransform = FTransform::Identity;
        EComponentMobility::Type Mobility = EComponentMobility::Movable;
        ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
        bool bCanEverAffectNavigation = false;
        bool bVisible = false;
        bool bHiddenInGame = false;
        bool bActive = false;
        bool bCastShadow = false;
        bool bCastContactShadow = false;
        bool bAffectDistanceFieldLighting = false;
        bool bRenderInMainPass = false;
        bool bRenderCustomDepth = false;
        bool bValid = false;
    };

    /** Exact pre-apply component/slot snapshot; private and non-reflected. */
    bool SnapshotGroundMacroVariationOverlayInternal(
        ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation,
        FString& OutError);

    /**
     * Dormant one-slot apply.  Both independent compile-time gates reject
     * before SetMaterial can execute in current source.
     */
    bool ApplyGradedTurfMaterialInternal(
        UMaterialInterface* GradedTurfCandidate,
        FString& OutError);

    /** Restores the exact full override array and proves every saved field. */
    bool RestoreGroundMacroVariationOverlayInternal(FString& OutError);

    bool ValidateSnapshotBoundaryInternal(
        bool bExpectCandidateInSlotZero,
        UMaterialInterface* GradedTurfCandidate,
        FString& OutError) const;

    FGroundOverlaySnapshot GroundOverlaySnapshot;
};
