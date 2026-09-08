#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary.generated.h"

class UStaticMesh;

/** Isolated render-only Core/Fallback candidates; no component is owned here. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DPublicRealmJunctionContinuityAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    TObjectPtr<UStaticMesh> CoreCandidateMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    TObjectPtr<UStaticMesh> FallbackCandidateMesh = nullptr;
};
/** Pointer-only result for a separately authorized future presentation choice. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOptionalPublicRealmJunctionContinuity
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    TObjectPtr<UStaticMesh> SelectedCoreMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    TObjectPtr<UStaticMesh> SelectedFallbackMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    bool bCandidateSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    bool bExactCoreAndFallbackPreserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    bool bPositionUvTopologyGroupSlotTransformOrIdentityModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    bool bTerrainCollisionNavigationLosRfSensorOrSimulationAuthority = false;
};

/**
 * Unnumbered pointer-only selection boundary for a future post-R33
 * transaction. It never binds a mesh to a map, actor, component, terrain,
 * collision, navigation, LOS, RF, sensor, or simulation system.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static bool ValidateCandidateMeshRoster(
        const FTRIADIstanaExploreV5DPublicRealmJunctionContinuityAssets& Assets,
        FString& OutError);

    static bool ValidateExactFallbackMeshRoster(
        UStaticMesh* ExistingCoreMesh,
        UStaticMesh* ExistingFallbackMesh,
        FString& OutError);

    UFUNCTION(BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    static bool ResolveOptionalRenderMeshes(
        UStaticMesh* ExistingCoreMesh,
        UStaticMesh* ExistingFallbackMesh,
        const FTRIADIstanaExploreV5DPublicRealmJunctionContinuityAssets& CandidateAssets,
        bool bExplicitlySelectCandidate,
        FTRIADIstanaExploreV5DOptionalPublicRealmJunctionContinuity& OutSelection,
        FString& OutError);

    static int32 ExpectedCandidateAssetCount();
    static int32 ExpectedNormalOverrideCount();
    static bool IsCandidateRuntimeSelectionCompiledAuthorized();
    static bool IsCandidateRuntimeActivationCompiledAuthorized();
    static FString ExistingCoreMeshObjectPath();
    static FString ExistingFallbackMeshObjectPath();
    static FString CandidateCoreMeshObjectPath();
    static FString CandidateFallbackMeshObjectPath();
    static FString OutputNamespace();
};
