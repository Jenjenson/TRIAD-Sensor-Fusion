#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.generated.h"

/** Editor-only import and cold-validation boundary for the R24B landmark. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DTemasekShophouseEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Temasek Shophouse|Editor")
    static bool ImportIstanaExploreV5DTemasekShophouseR24Assets(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Temasek Shophouse|Editor")
    static bool ValidateIstanaExploreV5DTemasekShophouseR24Assets(
        FString& OutReport);

    /** Idempotent cold entry point for command-line import, map apply, validate. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Temasek Shophouse|Editor")
    static bool ImportApplyAndValidateIstanaExploreV5DTemasekShophouseR24(
        FString& OutReport);
};
