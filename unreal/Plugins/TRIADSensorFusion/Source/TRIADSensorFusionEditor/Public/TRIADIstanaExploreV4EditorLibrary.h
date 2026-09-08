#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV4EditorLibrary.generated.h"

/**
 * Fail-closed, additive editor integration for Explore V4.
 *
 * The library duplicates the validated Explore V3 map into a new V4 map.  It
 * never repairs or overwrites a pre-existing partial V4 output, and takes a
 * streaming size/SHA-256 snapshot (including absent sidecars) of every
 * preceding Istana public-view map/content package before import or build.
 * V4 vegetation and the narrow V8 portico are presentation-only; the inherited
 * Explore V2 terrain/collision, HDB/OSM context and 720 Pawn blockers remain
 * authoritative and byte/state exact.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV4EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool PrewarmIstanaExploreV4ProtectedReferencesForImport(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool ImportIstanaExploreV4Assets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool BuildIstanaExploreV4Map(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool ValidateIstanaExploreV4Assets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool ValidateIstanaExploreV4Map(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool ValidateIstanaExploreV4PlayWorld(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool CaptureIstanaExploreV4PlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool TriggerIstanaExploreV4PlayWindGust(
        float PeakStrengthCm,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool MoveIstanaExploreV4PlayPawnForQa(
        FVector DeltaCentimeters,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool TeleportIstanaExploreV4PlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool GetIstanaExploreV4PlayStateReport(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Editor")
    static bool QuiesceIstanaExploreV4PlayWorldForStop(FString& OutMessage);
};
