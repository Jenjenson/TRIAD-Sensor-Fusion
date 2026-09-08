#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.generated.h"

/** Isolated editor-only R29 facade asset and guarded no-save map boundary. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Idempotently create/save exactly eleven materials and one facade mesh. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade|Editor")
    static bool EnsureR29FacadeEnvironmentAssets(FString& OutMessage);

    /** Cold/read-only validation, including exact OBJ/MTL/manifest SHA-256. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade|Editor")
    static bool ValidateR29FacadeEnvironmentAssets(FString& OutReport);

    /**
     * Replace exactly one validated R28 environment actor with the R29 facade
     * successor in the exact loaded V5D hybrid map. The successor is prepared
     * fully hidden, the R28 actor is removed, and only then is R29 activated.
     * The unchanged R28 public realm and outer ground are retained. This
     * endpoint never saves the map; the external native transaction owns the
     * sole map commit and rollback. It accepts exactly one validated
     * vegetation owner: retained R28 landmark vegetation xor the R29
     * vegetation successor. It never mutates that owner.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade|Editor")
    static bool ApplyR29FacadeReplacementToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * Guarded one-save entry point for the external native transaction. It
     * requires the clean on-disk predecessor and a separate byte-identical
     * backup to match the caller-supplied receipt immediately before mutation
     * and again before the sole save, then cold-reloads and validates R29. The
     * caller must also pin whether the predecessor's sole vegetation owner is
     * R28 or R29; both are supported, but mismatch or coexistence is refused.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade|Editor")
    static bool CommitR29FacadeReplacementToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& ExpectedVegetationOwner,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport);

    /** Strict loaded-map validation of the mutually-exclusive successor. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R29 Facade|Editor")
    static bool ValidateR29FacadeReplacementInLoadedV5DHybridMap(
        FString& OutReport);
};
