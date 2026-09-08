#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Exact prebuilt presentation assets admitted by the isolated R28 seam. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DR28EnvironmentAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R28")
    TObjectPtr<UStaticMesh> ConnectivePublicRealmMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R28")
    TObjectPtr<UStaticMesh> ContextArchitecturalDressingMesh = nullptr;

    /** Existing immutable outer-ground annulus; R28 supplies only an override. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R28")
    TObjectPtr<UStaticMesh> OuterGroundMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|R28")
    TObjectPtr<UMaterialInterface> OuterGroundMaterial = nullptr;
};

/**
 * Additive, human-view-only surroundings realism for the V5D fallback.
 *
 * The actor owns no simulation geometry.  Its three prebuilt renderers have
 * no collision or navigation, are excluded from scene captures, and mirror
 * provider-ready visibility as one atomic presentation group.  It never
 * mutates the frozen surroundings/RF shell, the existing public-realm mesh,
 * or the existing outer-ground asset.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DR28EnvironmentActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DR28EnvironmentActor();

    bool ConfigureR28Environment(
        const FTRIADIstanaExploreV5DR28EnvironmentAssets& InAssets,
        bool bInitialProviderReady,
        FString& OutError);

    /** Changes only the three owned renderer visibility flags. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R28")
    bool SetProviderReady(bool bInProviderReady, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R28")
    bool ValidateR28Environment(FString& OutReport) const;

    static const FString& ExpectedConnectivePublicRealmMeshObjectPath();
    static const FString& ExpectedContextArchitecturalDressingMeshObjectPath();
    static const FString& ExpectedOuterGroundMeshObjectPath();
    static const FString& ExpectedOuterGroundMaterialObjectPath();
    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Components")
    TObjectPtr<UStaticMeshComponent> ConnectivePublicRealmRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Components")
    TObjectPtr<UStaticMeshComponent> ContextArchitecturalDressingRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Components")
    TObjectPtr<UStaticMeshComponent> OuterGroundColourReliefOverlayRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Assets")
    FTRIADIstanaExploreV5DR28EnvironmentAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Truth")
    bool bRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Truth")
    bool bCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Truth")
    bool bMeasuredSurveyAsBuiltOrCurrentCompleteClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Truth")
    bool bExistingSimulationOrRfInputsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Truth")
    bool bRuntimeGeometryGenerated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Runtime")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|R28|Runtime")
    bool bProviderReady = false;
};
