#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR29VegetationEditorLibrary.generated.h"

class ATRIADIstanaExploreV5DR29VegetationActor;

/** Asset-only/configuration boundary for the isolated R29 visual successor. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR29VegetationEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Idempotently import three exact modeled-blade carriers and derive four
     * grass materials from the admitted R28 profiles. No map is loaded/saved
     * and no source package is modified.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Editor")
    static bool BuildOrValidateR29VegetationAssets(FString& OutReport);

    /** Cold, read-only validation of the exact seven-package R29 asset root. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Editor")
    static bool ValidateR29VegetationAssets(FString& OutReport);

    /** Configure an already spawned R29 actor; does not spawn or save a map. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Editor")
    static bool ConfigureR29VegetationActor(
        ATRIADIstanaExploreV5DR29VegetationActor* Actor,
        FString& OutReport);

    /**
     * Guarded one-save successor transaction for the exact loaded clean V5D
     * hybrid map. The complete R28 world, exact map bytes, and an external
     * byte-identical backup are validated before the first actor mutation.
     * Assets and deterministic R28/R29 geography are validated before the R28
     * render owner is removed; any failure is owned by the external wrapper's
     * file journal and never silently leaves both render owners resident.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Editor")
    static bool ApplyR29VegetationSuccessorToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    /** Cold/read-only validation of the exact saved R29 successor roster. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Vegetation|Editor")
    static bool ValidateR29VegetationSuccessorMap(FString& OutReport);
};
