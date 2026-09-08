#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.generated.h"

class ATRIADIstanaExploreV5DLandmarkVegetationActor;

/**
 * Isolated asset/configuration boundary for landmark vegetation. R27 and its
 * additive R28 successor may each create only four new grass-material
 * derivatives in their dedicated namespaces; neither endpoint edits a source
 * material, spawns an actor, or changes a map.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Idempotently create and save the exact four R27 landmark-only material
     * derivatives. A partial namespace or a non-R23B source graph fails closed.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool BuildOrValidateLandmarkGrassMaterialsR27(FString& OutReport);

    /** Cold/read-only graph validation for the four saved R27 derivatives. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool ValidateLandmarkGrassMaterialsR27(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool ValidateReusableLandmarkVegetationAssets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool ConfigureLandmarkVegetationActor(
        ATRIADIstanaExploreV5DLandmarkVegetationActor* Actor,
        FString& OutReport);

    /**
     * Idempotently create/save exactly four R28 materials from clean R27
     * sources, allowing only the stable-gate label and blade-colour custom
     * expression to differ from their complete canonical R23B graphs.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool BuildOrValidateLandmarkGrassMaterialsR28(
        FString& OutReport);

    /** Cold/read-only graph validation for the four saved R28 derivatives. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool ValidateLandmarkGrassMaterialsR28(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool ValidateReusableLandmarkVegetationAssetsR28(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Landmark Vegetation|Editor")
    static bool ConfigureLandmarkVegetationActorR28(
        ATRIADIstanaExploreV5DLandmarkVegetationActor* Actor,
        FString& OutReport);
};
