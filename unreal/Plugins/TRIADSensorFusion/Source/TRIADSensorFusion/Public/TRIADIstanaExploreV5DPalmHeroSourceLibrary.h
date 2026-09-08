#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DPalmHeroSourceLibrary.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

/** Exact isolated native asset route prepared from PalmHeroCandidate. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DPalmHeroAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    TObjectPtr<UStaticMesh> PalmMesh;

    /** Bark, FrondLive, FrondDry in immutable first-use order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    TArray<TObjectPtr<UMaterialInterface>> Materials;

    /** Diffuse, NormalDX, Roughness, AmbientOcclusion in provenance order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    TArray<TObjectPtr<UTexture2D>> BarkTextures;
};

/**
 * Read-only source decision for the unnumbered geometry-variation path.
 * The admitted TreeRealism palm remains mandatory as the fallback; selecting
 * this option changes no source actor, transform, map, or simulation policy.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DOptionalPalmSource
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    TObjectPtr<UStaticMesh> SelectedSourceMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    bool bPalmHeroSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    bool bExistingPalmFallbackPreserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    bool bMapOrSourceTransformModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    bool bCollisionNavigationLosRfSensorOrTerrainAuthority = false;
};

/**
 * Unnumbered, appearance-only PalmHero asset and source-selection boundary.
 * It performs no spawning, replacement, source suppression, or map mutation.
 */
UCLASS()
class TRIADSENSORFUSION_API UTRIADIstanaExploreV5DPalmHeroSourceLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static bool ValidateAssetRoster(
        const FTRIADIstanaExploreV5DPalmHeroAssets& Assets,
        FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Palm Hero")
    static bool ResolveOptionalGeometryVariationPalmSource(
        UStaticMesh* ExistingTreeRealismPalmSource,
        const FTRIADIstanaExploreV5DPalmHeroAssets& PalmHeroAssets,
        bool bExplicitlySelectPalmHero,
        FTRIADIstanaExploreV5DOptionalPalmSource& OutSelection,
        FString& OutError);

    static FString MeshObjectPath();
    static FString MaterialObjectPath(int32 SlotIndex);
    static FString TextureObjectPath(int32 TextureIndex);
    static FString ExistingPalmFallbackObjectPath();
    static int32 ExpectedLodCount();
    static int32 ExpectedMaterialCount();
    static int32 ExpectedTextureCount();
};
