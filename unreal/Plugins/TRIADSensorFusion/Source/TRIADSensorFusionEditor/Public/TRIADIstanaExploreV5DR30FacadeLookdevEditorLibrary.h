#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.generated.h"

/** Isolated editor-only R30 lookdev asset and guarded no-save map boundary. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Idempotently create/save one opaque master plus exactly eleven MICs. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Editor")
    static bool EnsureR30FacadeLookdevAssets(FString& OutMessage);

    /** Cold/read-only validation of the isolated assets and source contract. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Editor")
    static bool ValidateR30FacadeLookdevAssets(FString& OutReport);

    /**
     * Replace exactly one validated R29 facade owner with a fully prepared
     * R30 material-only successor in the exact loaded V5D hybrid map. This
     * endpoint never saves; the external native transaction owns the single
     * commit and byte rollback. Existing context-policy/current-surroundings,
     * vegetation, terrain, Cesium, sensor, RF, collision, and navigation state
     * is validated but never mutated.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Editor")
    static bool ApplyR30FacadeLookdevReplacementToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * Guarded one-save entry point. The clean predecessor and external backup
     * must match the supplied bytes/SHA before mutation and again before the
     * sole save. The endpoint then cold-reloads and revalidates R30 plus the
     * admitted context-policy shell.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Editor")
    static bool CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& ExpectedVegetationOwner,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    /** Strict loaded-map validation of the mutually-exclusive successor. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Facade Lookdev|Editor")
    static bool ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap(
        FString& OutReport);
};
