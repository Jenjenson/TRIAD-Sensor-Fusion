#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary.generated.h"

/** Unnumbered fail-closed inspection boundary for the explicit composition. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Read-only inspection. Success never confers write or activation authority. */
    UFUNCTION(BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Building Optics Contact Composition")
    static bool InspectBuildingOpticsContactCompositionReceipts(
        const FString& CompositionCandidateRoot,
        const FString& OpticsCandidateRoot,
        const FString& FootContactCandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);

private:
    /**
     * Dormant, non-reflected, fresh-only materializer. It has no call site and
     * cannot pass until two distinct reviewed hashes replace invalid sentinels
     * in source and the module is recompiled.
     */
    static bool MaterializeTrustedBuildingOpticsContactCompositionAssetsInternal(
        const FString& CompositionCandidateRoot,
        const FString& OpticsCandidateRoot,
        const FString& FootContactCandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport);
};
