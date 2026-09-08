#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.h"
#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Editor-only, unnumbered, fail-closed boundary. Its sole reflected endpoint
 * reads receipts only. The combined asset materialization and exact six-source
 * presentation swap is private, uncalled, and compiled false.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Tropical Umbrella Hero")
    static bool InspectTropicalUmbrellaHeroReceipts(
        const FString& CandidateRoot,
        const FString& AcceptedCurrentReceiptPath,
        const FString& ExpectedAcceptedCurrentReceiptSha256,
        const FString& FutureAuthorizationReceiptPath,
        const FString& ExpectedFutureAuthorizationReceiptSha256,
        FString& OutReport);

private:
    /**
     * Deliberately unreachable. A future reviewed edit must independently
     * enable materialization, pin two distinct compiled receipt hashes, and
     * recompile. It also requires the exact target persistent editor world and
     * childless named owner-property components. Any failure rolls both fresh
     * registry/live/package/physical namespaces back and restores only those
     * components, with visibility propagation disabled.
     */
    static bool MaterializeAndStageTrustedTropicalUmbrellaHeroInternal(
        const FString& CandidateRoot,
        const FString& AcceptedCurrentReceiptPath,
        const FString& ExpectedAcceptedCurrentReceiptSha256,
        const FString& FutureAuthorizationReceiptPath,
        const FString& ExpectedFutureAuthorizationReceiptSha256,
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor* CandidateActor,
        UHierarchicalInstancedStaticMeshComponent*
            ExistingV4HeritageUmbrellaComponent,
        UHierarchicalInstancedStaticMeshComponent*
            ExistingR29LandmarkUmbrellaComponent,
        const TArray<
            FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
            ExactOrderedNativeAnchors,
        FString& OutReport);
};
