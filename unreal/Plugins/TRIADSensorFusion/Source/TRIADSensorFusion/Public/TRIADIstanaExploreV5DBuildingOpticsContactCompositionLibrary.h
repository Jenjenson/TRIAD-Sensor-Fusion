#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary.generated.h"

class UMaterialInterface;

/** Exact ordered appearance-only roster for the combined optical/contact look. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DBuildingOpticsContactCompositionAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    TArray<TObjectPtr<UMaterialInterface>> OrderedSlotMaterials;
};
/** Pointer-only result for a separately authorized future presentation choice. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DOptionalBuildingOpticsContactComposition
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    TArray<TObjectPtr<UMaterialInterface>> SelectedSlotMaterials;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    bool bCompositionCandidateSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    bool bOpticsFallbackSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    bool bR31FallbackSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    bool bExactOpticsAndR31FallbacksPreserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    bool bMeshTopologyUvSlotRosterFootprintMassingOrTransformModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    bool bGeographyTerrainCollisionNavigationLosRfSensorOrSimulationAuthority = false;
};

/**
 * Unnumbered pointer-only post-R33 boundary. It never binds a material to a
 * map, actor, mesh, component, or simulation system.
 */
UCLASS()
class TRIADSENSORFUSION_API UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static bool ValidateCandidateMaterialRoster(
        const FTRIADIstanaExploreV5DBuildingOpticsContactCompositionAssets& Assets,
        FString& OutError);
    static bool ValidateExactOpticsFallbackRoster(
        const TArray<TObjectPtr<UMaterialInterface>>& ExistingOpticsFallbacks,
        FString& OutError);
    static bool ValidateExactR31FallbackRoster(
        const TArray<TObjectPtr<UMaterialInterface>>& ExistingR31Fallbacks,
        FString& OutError);

    UFUNCTION(BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    static bool ResolveOptionalPresentationMaterials(
        const TArray<UMaterialInterface*>& ExistingOpticsFallbacks,
        const TArray<UMaterialInterface*>& ExistingR31Fallbacks,
        const FTRIADIstanaExploreV5DBuildingOpticsContactCompositionAssets& CandidateAssets,
        bool bExplicitlySelectCompositionCandidate,
        bool bPreferOpticsFallback,
        FTRIADIstanaExploreV5DOptionalBuildingOpticsContactComposition& OutSelection,
        FString& OutError);

    static int32 ExpectedMaterialSlotCount();
    static int32 ExpectedOutputAssetCount();
    static bool IsCandidateRuntimeSelectionCompiledAuthorized();
    static bool IsCandidateRuntimeActivationCompiledAuthorized();
    static FString OutputNamespace();
    static FString MasterMaterialObjectPath();
    static FString CandidateMaterialObjectPath(int32 SlotIndex);
    static FString ExistingOpticsFallbackObjectPath(int32 SlotIndex);
    static FString ExistingR31FallbackObjectPath(int32 SlotIndex);
    static FString SlotName(int32 SlotIndex);
};
