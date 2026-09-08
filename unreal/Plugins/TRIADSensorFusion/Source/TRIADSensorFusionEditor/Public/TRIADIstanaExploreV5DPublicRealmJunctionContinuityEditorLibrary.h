#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary.generated.h"

/**
 * Editor-only, unnumbered, fail-closed receipt-inspection boundary for the
 * completed PublicRealm/JunctionContinuityCandidate source pack. The only
 * reflected endpoint is read-only and cannot authorize execution.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Reads the hash-pinned candidate closure and two caller-pinned receipts.
     * Success is inspection only and never execution authority.
     */
    UFUNCTION(BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Public Realm Junction Continuity")
    static bool InspectPublicRealmJunctionContinuityReceipts(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);

private:
    /**
     * Dormant implementation only. It is non-reflected, private, has no call
     * site, and cannot pass its two deliberately invalid compiled trust
     * anchors. A reviewed source change and recompile are required before any
     * isolated package write can become reachable.
     */
    static bool MaterializeTrustedPublicRealmJunctionContinuityAssetsInternal(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);
};
