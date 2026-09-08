#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.generated.h"

class UMaterialInterface;
class UTexture2D;

/**
 * Exact isolated asset roster materialized from the source-only
 * OrdinaryDistanceGrassSurfaceCandidate.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets
{
    GENERATED_BODY()

    /** BaseColor, NormalDX, Roughness, AmbientOcclusion, Height. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    TArray<TObjectPtr<UTexture2D>> SourceTextures;

    /** Standalone appearance-only surface; it is not map-bound here. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    TObjectPtr<UMaterialInterface> SurfaceMaterial;
};

/** Read-only result for a later, separately authorized presentation choice. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOptionalGrassSurfaceMaterial
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    TObjectPtr<UMaterialInterface> SelectedMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    bool bCandidateSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    bool bExistingLawnFallbackPreserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    bool bMapGeographyTerrainOrSourceTransformModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    bool bCollisionNavigationLosRfSensorOrSimulationAuthority = false;
};

/**
 * Unnumbered, appearance-only runtime boundary. It validates an exact asset
 * roster and resolves a material pointer only. It never changes a component,
 * actor, transform, map, terrain, collision, navigation, line-of-sight, RF,
 * sensor, or simulation state.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets&
            Assets,
        FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    static bool ResolveOptionalPresentationMaterial(
        UMaterialInterface* ExistingLawnSurfaceFallback,
        const FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets&
            CandidateAssets,
        bool bExplicitlySelectCandidate,
        FTRIADIstanaExploreV5DOptionalGrassSurfaceMaterial& OutSelection,
        FString& OutError);

    static FString MaterialObjectPath();
    static FString TextureObjectPath(int32 TextureIndex);
    static FString ExistingLawnFallbackObjectPath();
    static int32 ExpectedTextureCount();
    static int32 ExpectedOutputAssetCount();
    static double ProviderTileMetres();
};
