#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.generated.h"

/** Editor-only import and cold-validation boundary for the R24A landmark. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DMacDonaldHouseEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|MacDonald House|Editor")
    static bool ImportIstanaExploreV5DMacDonaldHouseR24Assets(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|MacDonald House|Editor")
    static bool ValidateIstanaExploreV5DMacDonaldHouseR24Assets(
        FString& OutReport);

    /** Idempotent cold entry point for command-line import, map apply, validate. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|MacDonald House|Editor")
    static bool ImportApplyAndValidateIstanaExploreV5DMacDonaldHouseR24(
        FString& OutReport);
};
