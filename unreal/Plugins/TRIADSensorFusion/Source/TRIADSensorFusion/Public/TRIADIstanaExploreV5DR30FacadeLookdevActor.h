#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class ATRIADIstanaExploreV5DContextPolicyActor;

/** Exact presentation assets admitted by the R30 context-facade lookdev successor. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR30FacadeLookdevAssets
{
    GENERATED_BODY()

    /** Exact unchanged R28 connective-public-realm mesh retained by R29. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    TObjectPtr<UStaticMesh> RetainedR28ConnectivePublicRealmMesh = nullptr;

    /** Exact unchanged, hash-bound R29 context-facade mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    TObjectPtr<UStaticMesh> RetainedR29ContextFacadeCoverageMesh = nullptr;

    /** Exact unchanged outer-ground annulus retained by R29. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    TObjectPtr<UStaticMesh> RetainedOuterGroundMesh = nullptr;

    /** Exact unchanged R28 outer-ground material retained by R29. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    TObjectPtr<UMaterialInterface> RetainedR28OuterGroundMaterial = nullptr;

    /** Exact eleven slot-ordered R30 context-safe PBR overrides. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    TArray<TObjectPtr<UMaterialInterface>> ContextFacadeMaterialOverrides;
};

/**
 * Render-only R30 material successor for the R29 facade environment actor.
 *
 * R30 retains all predecessor meshes and transforms byte-for-byte and changes
 * only the eleven context-facade material bindings. The successor is prepared
 * hidden, the sole R29 owner is removed, and only then can R30 activate. Every
 * renderer remains static, no-collision, non-navigable, excluded from scene
 * captures, and carries no simulation, sensor, RF, geospatial, survey,
 * as-built, current-complete, or physical-material authority.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DR30FacadeLookdevActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR30FacadeLookdevActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool ConfigurePreparedR30FacadeLookdevHandoff(
        const FTRIADIstanaExploreV5DR30FacadeLookdevAssets& InAssets,
        bool bInPredecessorProviderReady,
        FString& OutError);

    bool ActivateAfterR29FacadeRemoval(FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    bool SetProviderReady(bool bInProviderReady, FString& OutError);

    bool ValidatePreparedR30FacadeLookdevHandoff(FString& OutReport) const;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev")
    bool ValidateR30FacadeLookdev(FString& OutReport) const;

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DR30FacadeLookdevAssets& Assets,
        FString& OutError);

    static const FString& ExpectedR29FacadeMeshObjectPath();
    static const FString& ExpectedR28PublicRealmMeshObjectPath();
    static const FString& ExpectedOuterGroundMeshObjectPath();
    static const FString& ExpectedR28OuterGroundMaterialObjectPath();
    static const TArray<FName>& ExpectedR29MaterialSlotNames();
    static const TArray<FString>& ExpectedR30MaterialObjectPaths();
    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    static int32 ExpectedOwnedRendererCount();
    static int32 ExpectedFacadeMaterialOverrideCount();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Components")
    TObjectPtr<UStaticMeshComponent> RetainedR28ConnectivePublicRealmRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Components")
    TObjectPtr<UStaticMeshComponent> R30ContextFacadeLookdevRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Components")
    TObjectPtr<UStaticMeshComponent> RetainedR28OuterGroundOverlayRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Assets")
    FTRIADIstanaExploreV5DR30FacadeLookdevAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bPredecessorMeshesAndTransformsRetainedUnmodified = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bR28OrR29ArchitectureRetainedOrCoRendered = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bCollisionNavigationSimulationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bExistingSimulationRfProviderCesiumOrGeospatialInputsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bRuntimeGeometryGenerated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Truth")
    bool bProviderReadinessObservedWithoutPolicyMutation = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Runtime")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Runtime")
    bool bReplacementActivated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Runtime")
    bool bProviderReady = false;

private:
    bool SynchronizeOwnedVisibilityWithContextPolicy(FString& OutError);

    TWeakObjectPtr<ATRIADIstanaExploreV5DContextPolicyActor>
        RuntimeContextPolicyActor;

    bool bLoggedRuntimeProviderMirrorFailure = false;
};
