#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaEditorLibrary.generated.h"

/** Narrow, deterministic editor operations suitable for the local Remote Control API. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Verify that a Remote Control caller is connected to the intended host
     * project. ExpectedProjectPath may be the project directory or .uproject.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaRemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport);

    /**
     * Load /Game/SDTH as an untitled template, build the Istana AOI, and save
     * only /Game/Maps/Istana_1km. Refuses overwrite, PIE, or any dirty package.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool BuildIstanaStudyMap(FString& OutMessage);

    /** Validate the currently loaded /Game/Maps/Istana_1km without modifying it. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaStudyMap(FString& OutReport);

    /**
     * Create /Game/Maps/Istana_1km_Context_v2 from an untitled /Game/SDTH
     * template. Unlike the legacy builder, this preserves the template's
     * georeference so inherited sky/start actors remain spatially aligned,
     * persists the TRIAD-owned AirSim GameMode wrapper, and saves only the new
     * destination.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool BuildIstanaRuntimeMapV2(FString& OutMessage);

    /**
     * Repair only the GameMode Override of a loaded exact v2 destination.
     * Creates a non-overwriting backup and proves the result after disk reload.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool RepairIstanaRuntimeMapV2GameMode(
        FString& OutBackupFile,
        FString& OutMessage);

    /**
     * Migrate only the tagged runtime camera and imported-exterior heading of
     * a loaded exact v2 destination to the current ceremonial-facade contract.
     * Creates a non-overwriting backup and proves the result after disk reload.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool RepairIstanaRuntimeMapV2Camera(
        FString& OutBackupFile,
        FString& OutMessage);

    /**
     * Swap only the exact v2 map's exterior actor from the reviewed legacy
     * mesh to the separately imported refined asset. Creates a non-overwriting
     * backup and proves the result after a disk reload.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool RepairIstanaRuntimeMapV2ExteriorAsset(
        FString& OutBackupFile,
        FString& OutMessage);

    /** Validate the currently loaded v2 runtime map without modifying it. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaRuntimeMapV2(FString& OutReport);

    /**
     * Validate the v2 map and require every Cesium tileset to be >=99% loaded
     * with physics meshes before a context-completeness claim or capture.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaRuntimeMapV2Readiness(FString& OutReport);

    /**
     * Validate the editor map structurally, then inspect the rebased PIE world
     * using runtime-relative camera and Cesium checks only. No PlayWorld LLH or
     * geodesic-spline reconstruction is performed.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaPlayWorldReadiness(FString& OutReport);

    /**
     * Apply the normal PlayWorld readiness proof, then additionally require
     * the explicit non-production, no-lidar/no-RPC visual acceptance profile.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaVisualAcceptancePlayWorldReadiness(FString& OutReport);

    /**
     * Best-effort pause of AirSim simulation modes before explicit PIE stop.
     * Does not validate, edit, capture, save, or itself end the PlayWorld.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool QuiesceIstanaPlayWorldForStop(FString& OutMessage);

    /**
     * Drive Cesium editor streaming from the active Level Editor viewport.
     * This is transient: it never saves or edits map packages and does not
     * require the tilesets to be ready before it runs.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool PrepareIstanaRuntimeMapV2Streaming(FString& OutMessage);

    /**
     * After all Cesium tiles report loaded, sample collision on two exterior
     * rings outside the building footprint, reject outliers, and save a median
     * WGS84 ground-height calibration to the v2 destination only.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool CalibrateIstanaRuntimeMapV2Ground(FString& OutMessage);

    /**
     * Import the complete generated 4096px PBR texture set and create eight
     * exact-slot project-owned materials. This is all-or-nothing: a complete
     * valid set is an idempotent success, while any partial/invalid target set
     * is reported and never overwritten.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ImportIstanaPbrMaterials(FString& OutMessage);

    /** Read-only validation for the exact Istana texture/material asset set. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ValidateIstanaPbrMaterials(FString& OutReport);

    /**
     * Read Stage-0 metadata for one immutable release below
     * SourceAssets/IstanaDigitalTwin/Releases. This never hashes bytes, reruns
     * ColorQA, verifies signatures/geometry semantics, or creates assets; the
     * standalone validator still returns NON_IMPORTABLE.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Digital Twin")
    static bool ValidateIstanaDigitalTwinReleaseMetadata(
        const FString& ManifestPath,
        FString& OutReport);

    /**
     * Reserved fail-closed import hook. Until a reviewed format-specific
     * importer and validation-receipt contract ship, this always creates no
     * assets and returns false with an actionable explanation.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Digital Twin")
    static bool ImportIstanaDigitalTwinRelease(
        const FString& ManifestPath,
        FString& OutMessage);

    /**
     * Reserved fail-closed v3 map hook. It never opens, duplicates, saves, or
     * mutates SDTH, Istana_1km, or Istana_1km_Context_v2.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Digital Twin")
    static bool BuildIstanaDigitalTwinMapV3(
        const FString& ReleaseId,
        FString& OutMessage);

    /**
     * Retired legacy entry point. It always refuses to recreate or overwrite
     * SM_IstanaExterior and directs callers to the refined import function.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ImportIstanaExteriorLod0(FString& OutMessage);

    /**
     * Import the reviewed regenerated LOD0 into the new non-overwriting
     * SM_IstanaExterior_Refined asset. Requires the existing exact PBR set and
     * validates the reviewed manifest declaration, imported bounds/triangles,
     * simple collision, and slot names. The narrow PowerShell caller separately
     * proves the source OBJ byte SHA-256 before invoking this function.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool ImportIstanaExteriorRefinedLod0(FString& OutMessage);

    /**
     * Export deterministic geometry-authoritative placement rows under
     * Saved/TRIAD/PlacementSurveys. The argument must be one .json filename.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool GenerateIstanaPlacementSurvey(
        const FString& RequestJsonPath,
        const FString& OutputFileName,
        FString& OutMessage);

    /**
     * Schedule a deterministic FRONT, OBLIQUE, or SIDE screenshot of the
     * validated v2 imported-mesh exterior. OutputFileName must start with
     * v2_editor_.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool CaptureIstanaPreview(
        const FString& PresetName,
        const FString& OutputFileName,
        FString& OutMessage);

    /**
     * While v2 PIE is already running, capture the actual Player 0 game view
     * only after the runtime-origin-safe PlayWorld readiness gate passes.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana|Editor")
    static bool CaptureIstanaPlaySpawnPreview(
        const FString& OutputFileName,
        FString& OutMessage);
};
