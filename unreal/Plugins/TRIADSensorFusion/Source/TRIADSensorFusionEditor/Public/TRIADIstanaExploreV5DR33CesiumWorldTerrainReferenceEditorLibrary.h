#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.generated.h"

/** Editor-only, add-only R33 Cesium World Terrain map integration. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Add the exact visual-only CWT tileset and R33 controller to the loaded
     * R32 predecessor. This endpoint never saves. Receipt-chain admission and
     * filesystem rollback remain the responsibility of the external wrapper.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 CWT|Editor")
    static bool ApplyR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * Guarded one-save endpoint for the exact clean R32 predecessor. The
     * caller must provide a byte-identical external map backup below the
     * bounded R33 transaction root.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 CWT|Editor")
    static bool CommitR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    /** Cold/read-only validation of the exact loaded R33 successor roster. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 CWT|Editor")
    static bool ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap(
        FString& OutReport);
};
