#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DPublicRealmEditorLibrary.generated.h"

/**
 * Remote-Control-callable, editor-only admission boundary for the frozen V5D
 * public-realm render assets. These operations never edit an actor or map.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DPublicRealmEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Public Realm|Editor")
    static bool ImportIstanaExploreV5DPublicRealmAssets(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Public Realm|Editor")
    static bool ValidateIstanaExploreV5DPublicRealmAssets(
        FString& OutReport);
};
