#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Exact presentation assets admitted by the R29 facade successor. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets
{
    GENERATED_BODY()

    /** Exact unchanged R28 connective-public-realm mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Facade")
    TObjectPtr<UStaticMesh> RetainedR28ConnectivePublicRealmMesh = nullptr;

    /** Hash-bound R29 context facade coverage mesh; replaces R28 architecture. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Facade")
    TObjectPtr<UStaticMesh> ContextFacadeCoverageMesh = nullptr;

    /** Exact unchanged outer-ground annulus used by R28. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Facade")
    TObjectPtr<UStaticMesh> RetainedOuterGroundMesh = nullptr;

    /** Exact unchanged R28 outer-ground material. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R29 Facade")
    TObjectPtr<UMaterialInterface> RetainedR28OuterGroundMaterial = nullptr;
};

/**
 * Render-only R29 successor for the R28 environment actor.
 *
 * It retains the exact R28 connective public realm and outer-ground overlay,
 * replaces only the R28 context architecture with the hash-bound R29 facade
 * mesh, and deliberately has no R28 architecture component. Configuration is
 * a hidden handoff state. Activation is refused until the world contains zero
 * R28 environment owners, preventing the old and new architecture from ever
 * co-rendering. Every renderer is NoCollision, non-navigable, excluded from
 * scene captures, and carries no simulation, sensor, RF, survey, as-built, or
 * current-completeness authority.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DR29FacadeEnvironmentActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR29FacadeEnvironmentActor();

    /** Prepare an all-hidden successor beside exactly one validated R28 owner. */
    bool ConfigurePreparedR29FacadeHandoff(
        const FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets& InAssets,
        bool bInPredecessorProviderReady,
        FString& OutError);

    /** Activate only after the exact R28 actor/tag roster reaches zero. */
    bool ActivateAfterR28EnvironmentRemoval(FString& OutError);

    /** Changes only the three owned renderer visibility flags. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade")
    bool SetProviderReady(bool bInProviderReady, FString& OutError);

    /** Validate the hidden, pre-destruction handoff state. */
    bool ValidatePreparedR29FacadeHandoff(FString& OutReport) const;

    /** Validate the final mutually-exclusive successor state. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade")
    bool ValidateR29FacadeEnvironment(FString& OutReport) const;

    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets& Assets,
        FString& OutError);

    static const FString& ExpectedR29FacadeMeshObjectPath();
    static const FString& ExpectedR28ArchitectureMeshObjectPath();
    static const FString& ExpectedR28PublicRealmMeshObjectPath();
    static const FString& ExpectedOuterGroundMeshObjectPath();
    static const FString& ExpectedR28OuterGroundMaterialObjectPath();
    static const TArray<FName>& ExpectedR29MaterialSlotNames();
    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    static int32 ExpectedOwnedRendererCount();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Components")
    TObjectPtr<UStaticMeshComponent> RetainedR28ConnectivePublicRealmRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Components")
    TObjectPtr<UStaticMeshComponent> R29ContextFacadeCoverageRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Components")
    TObjectPtr<UStaticMeshComponent> RetainedR28OuterGroundOverlayRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Assets")
    FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bR28PublicRealmRetainedUnmodified = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bR28ArchitectureRetainedOrCoRendered = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bCollisionNavigationSimulationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bExistingSimulationRfProviderOrGeospatialInputsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Truth")
    bool bRuntimeGeometryGenerated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Runtime")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Runtime")
    bool bReplacementActivated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R29 Facade|Runtime")
    bool bProviderReady = false;
};
