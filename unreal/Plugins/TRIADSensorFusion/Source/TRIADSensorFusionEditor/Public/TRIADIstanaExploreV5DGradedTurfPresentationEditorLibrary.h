#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary.generated.h"

/**
 * Editor-only receipt inspection for the dormant post-R33 graded-turf V2
 * scaffold.  The sole reflected endpoint is read-only.  Materialization is
 * private, non-reflected, has no production call site, and is unreachable
 * until two distinct compiled trust anchors are reviewed and recompiled.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Graded Turf V2")
    static bool InspectGradedTurfPresentationReceipts(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);

private:
    /**
     * Dormant one-material implementation.  It clones the exact accepted
     * masked overlay into a fresh namespace, retains the entire OpacityMask
     * dependency closure byte-for-byte in graph terms, and replaces only the
     * BaseColor/Roughness/Normal/AO response with the exact existing
     * Grass001 dual-phase dependency graph.
     */
    static bool MaterializeTrustedGradedTurfPresentationInternal(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);
};
