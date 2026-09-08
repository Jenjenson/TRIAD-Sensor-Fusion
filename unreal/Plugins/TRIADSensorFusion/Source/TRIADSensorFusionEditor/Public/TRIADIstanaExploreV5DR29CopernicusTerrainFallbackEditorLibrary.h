#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.generated.h"

class ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor;

/** Guarded asset/configuration/map boundary for the R29 DEM visual fallback. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Editor")
    static bool BuildOrValidateCopernicusTerrainFallbackAssets(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Editor")
    static bool ValidateCopernicusTerrainFallbackAssets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Editor")
    static bool ConfigureCopernicusTerrainFallbackActor(
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Actor,
        FString& OutReport);

    /**
     * Add one fallback actor to the exact clean map after validating a
     * byte-identical external backup.  The two isolated assets are validated
     * before the first map mutation; one map save is permitted.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Editor")
    static bool ApplyCopernicusTerrainFallbackToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    /** Cold/read-only exact-roster validation of the saved successor map. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Terrain|Editor")
    static bool ValidateCopernicusTerrainFallbackSuccessorMap(
        FString& OutReport);
};
