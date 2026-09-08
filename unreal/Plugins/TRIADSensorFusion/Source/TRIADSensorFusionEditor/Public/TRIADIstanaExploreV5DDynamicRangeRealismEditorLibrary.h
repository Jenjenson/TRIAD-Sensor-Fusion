#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.generated.h"

class UWorld;

/** Editor-only application and validation for the isolated V5D lookdev pass. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Add exactly one appearance-only dynamic-range actor to the loaded exact
     * V5D hybrid map. The caller owns the save boundary.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ApplyDynamicRangeRealismPassToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * C++-only atomic hybrid-builder hook. The trust flag admits only an
     * untitled temp package before the hybrid builder's first destination
     * save. It does not weaken the Blueprint exact-map gate.
     */
    static bool ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(
        UWorld* InWorld,
        bool bTrustedUntitledHybridBuilder,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ValidateDynamicRangeRealismPassInLoadedV5DHybridMap(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ValidateDynamicRangeRealismPassInPlayWorld(
        FString& OutReport);
};
