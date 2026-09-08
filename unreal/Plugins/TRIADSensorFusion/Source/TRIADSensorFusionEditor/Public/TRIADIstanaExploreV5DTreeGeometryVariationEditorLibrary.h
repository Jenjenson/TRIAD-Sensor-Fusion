#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary.generated.h"

/**
 * Editor-only receipt inspection boundary for the existing source candidate
 * pack. It is intentionally unconnected to every numbered wrapper and
 * performs no map integration.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Read-only inspection. Caller-provided paths and hashes never confer
     * execution authority, and no package, source, map, or native state is
     * changed.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Tree Geometry")
    static bool InspectTreeGeometryVariationReceipts(
        const FString& CandidateContractPath,
        const FString& SelectorManifestPath,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);

private:
    /**
     * Dormant implementation only. It is deliberately absent from reflection
     * and the public C++ API, and current source has no valid compiled trust
     * pins. A future reviewed source change must pin both receipts, recompile,
     * and explicitly expose or call this implementation before any package
     * write can become reachable. It otherwise preserves the existing exact
     * fifteen-mesh materialization logic.
     */
    static bool MaterializeTrustedPostR33CandidateMeshesInternal(
        const FString& CandidateContractPath,
        const FString& SelectorManifestPath,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);
};
