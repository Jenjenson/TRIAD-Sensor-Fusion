#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR31BroadShellEditorLibrary.generated.h"

/** Editor-only R31 material-package and one-save map transaction boundary. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR31BroadShellEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Broad Shell|Editor")
    static bool EnsureR31BroadShellAssets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Broad Shell|Editor")
    static bool ValidateR31BroadShellAssets(FString& OutReport);

    /** In-place material override only; never saves or resets provider state. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Broad Shell|Editor")
    static bool ApplyR31BroadShellToLoadedV5DHybridMap(FString& OutMessage);

    /** One-save commit guarded by an exact clean map preimage and external backup. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Broad Shell|Editor")
    static bool CommitR31BroadShellToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Broad Shell|Editor")
    static bool ValidateR31BroadShellInLoadedV5DHybridMap(FString& OutReport);
};
