#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary.generated.h"

/**
 * Editor-only, unnumbered, fail-closed receipt-inspection boundary for
 * OrdinaryDistanceGrassSurfaceCandidate. The only reflected endpoint is
 * read-only and cannot authorize execution. The dormant materializer is
 * private and additionally requires reviewed, compiled-in receipt hashes.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Reads the exact source/provenance closure and two caller-pinned receipts.
     * This is inspection only: success is never execution authority.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Ordinary-Distance Grass")
    static bool InspectGrassSurfaceReceipts(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);

private:
    /**
     * Dormant implementation only. It is absent from reflection and public
     * C++ API, and current source has no valid compiled trust pins. A future
     * reviewed source change must pin both receipts, recompile, and explicitly
     * expose/call it before any package write can become reachable.
     */
    static bool MaterializeTrustedGrassSurfaceAssetsInternal(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);
};
