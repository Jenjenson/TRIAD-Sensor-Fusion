#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreEditorLibrary.generated.h"

/** Additive editor integration for the non-survey Istana Explore V1 map. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore|Editor")
    static bool ValidateIstanaExploreRemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore|Editor")
    static bool ImportIstanaExploreV1Assets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore|Editor")
    static bool BuildIstanaExploreV1Map(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore|Editor")
    static bool ValidateIstanaExploreV1Map(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore|Editor")
    static bool ValidateIstanaExploreV1PlayWorld(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore|Editor")
    static bool QuiesceIstanaExploreV1PlayWorldForStop(FString& OutMessage);
};
