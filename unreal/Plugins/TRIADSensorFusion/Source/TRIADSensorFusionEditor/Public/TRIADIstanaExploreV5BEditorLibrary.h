#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5BEditorLibrary.generated.h"

/**
 * Editor integration and exact QA surface for the additive Explore V5B visual
 * successor. V5B owns a new asset namespace, one render-only visual actor, one
 * trimmed building-render mesh, and a new map. Inherited collision, navigation,
 * physical hardscape, V8 overlay, sensor geometry and RF authority remain V5.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV5BEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool ImportIstanaExploreV5BAssets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool ValidateIstanaExploreV5BAssets(FString& OutReport);

    /**
     * Atomically upgrade only the exact clean R10 accent-turf carrier to R11,
     * or validate an idempotent R11 carrier. The other 36 V5B package hashes
     * and all object paths must remain stable.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool UpgradeIstanaExploreV5BAccentTurfAsset(FString& OutMessage);

    /** Refresh the unique loaded V5B visual actor without saving its map. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool RefreshIstanaExploreV5BGroundingUnsaved(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool BuildIstanaExploreV5BMap(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool ValidateIstanaExploreV5BMap(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool ValidateIstanaExploreV5BPlayWorld(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool CaptureIstanaExploreV5BPlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool TeleportIstanaExploreV5BPlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool TriggerIstanaExploreV5BPlayWindGust(
        float PeakStrengthCm,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool GetIstanaExploreV5BPlayStateReport(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5B|Editor")
    static bool QuiesceIstanaExploreV5BPlayWorldForStop(FString& OutMessage);
};
