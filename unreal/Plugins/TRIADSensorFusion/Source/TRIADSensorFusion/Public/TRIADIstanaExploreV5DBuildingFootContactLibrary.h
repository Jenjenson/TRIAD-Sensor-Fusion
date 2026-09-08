#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DBuildingFootContactLibrary.generated.h"

class UMaterialInterface;

/**
 * Exact, ordered, appearance-only material roster for the seventeen retained
 * R31 broad-shell slots. No mesh or component is owned by this structure.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DBuildingFootContactAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    TArray<TObjectPtr<UMaterialInterface>> OrderedSlotMaterials;
};
/** Read-only result for a separately authorized future presentation choice. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DOptionalBuildingFootContact
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    TArray<TObjectPtr<UMaterialInterface>> SelectedSlotMaterials;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    bool bCandidateSelected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    bool bExactR31FallbacksPreserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    bool bMeshTopologyUvSlotRosterOrTransformModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    bool bGeographyTerrainCollisionNavigationLosRfSensorOrSimulationAuthority = false;
};
/**
 * Unnumbered runtime selection boundary for a future post-R33 transaction.
 * It validates and returns material pointers only. It never binds them to a
 * map, actor, mesh, component, or simulation authority.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DBuildingFootContactLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static bool ValidateCandidateMaterialRoster(
        const FTRIADIstanaExploreV5DBuildingFootContactAssets& Assets,
        FString& OutError);

    static bool ValidateExactR31FallbackRoster(
        const TArray<TObjectPtr<UMaterialInterface>>& ExistingR31Fallbacks,
        FString& OutError);

    UFUNCTION(BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Building Foot Contact")
    static bool ResolveOptionalPresentationMaterials(
        const TArray<UMaterialInterface*>& ExistingR31Fallbacks,
        const FTRIADIstanaExploreV5DBuildingFootContactAssets& CandidateAssets,
        bool bExplicitlySelectCandidate,
        FTRIADIstanaExploreV5DOptionalBuildingFootContact& OutSelection,
        FString& OutError);

    static int32 ExpectedMaterialSlotCount();
    static int32 ExpectedOutputAssetCount();
    static bool IsCandidateRuntimeSelectionCompiledAuthorized();
    static FString MasterMaterialObjectPath();
    static FString CandidateMaterialObjectPath(int32 SlotIndex);
    static FString ExistingR31FallbackObjectPath(int32 SlotIndex);
    static FString SlotName(int32 SlotIndex);
};
