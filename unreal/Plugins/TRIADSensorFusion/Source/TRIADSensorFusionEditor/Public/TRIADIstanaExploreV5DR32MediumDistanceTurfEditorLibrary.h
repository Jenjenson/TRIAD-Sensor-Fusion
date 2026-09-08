#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.generated.h"

/** Editor-only, add-only R32 map-integration boundary. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Build or idempotently validate exactly four isolated R32 materials. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Medium Distance Turf|Editor")
    static bool EnsureR32MediumDistanceTurfMaterials(FString& OutMessage);

    /** Cold/read-only validation of the exact saved R32 material roster. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Medium Distance Turf|Editor")
    static bool ValidateR32MediumDistanceTurfMaterials(FString& OutReport);

    /**
     * Add one exact R32 render-only actor to the exact loaded R31 map.
     * This endpoint never saves. A later external wrapper must verify the
     * complete R30 commit/capture -> R31 commit/capture receipt chain before
     * invoking it.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Medium Distance Turf|Editor")
    static bool ApplyR32MediumDistanceTurfToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * Guarded one-save map endpoint. It requires a clean, hash-pinned R31
     * predecessor and a byte-identical external backup, then cold-reloads and
     * validates the exact R32 successor. External orchestration still owns
     * receipt-order admission and file-journal rollback.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Medium Distance Turf|Editor")
    static bool CommitR32MediumDistanceTurfToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    /** Cold/read-only validation of the exact loaded R32 successor roster. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Medium Distance Turf|Editor")
    static bool ValidateR32MediumDistanceTurfInLoadedV5DHybridMap(
        FString& OutReport);
};
