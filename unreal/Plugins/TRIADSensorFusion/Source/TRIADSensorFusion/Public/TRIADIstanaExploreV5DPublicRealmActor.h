#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DPublicRealmActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Exact prebuilt assets consumed by the V5D public-realm presentation. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DPublicRealmAssets
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm")
    TObjectPtr<UStaticMesh> CoreMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm")
    TObjectPtr<UStaticMesh> FallbackMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm")
    TObjectPtr<UMaterialInterface> RoadBaseMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm")
    TObjectPtr<UMaterialInterface> RoadGraphicMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm")
    TObjectPtr<UMaterialInterface> ConcreteMaterial = nullptr;
};

/**
 * Caller-supplied, source-specific provenance for the two prebuilt meshes.
 * The schema and negative-authority fields are fixed; source identifiers,
 * epoch, and digests must be populated from the eventual asset transaction.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV5DPublicRealmProvenance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    FString SchemaRevision = TEXT("TRIAD_IPV5D_PUBLIC_REALM_PROVENANCE_V1");

    /** ISO calendar date, YYYY-MM-DD. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    FString SourceEpoch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    FString CoreSourceIdentifier;

    /** Uppercase hexadecimal SHA-256 of the admitted core source artifact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    FString CoreSourceSha256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    FString FallbackSourceIdentifier;

    /** Uppercase hexadecimal SHA-256 of the admitted fallback source artifact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    FString FallbackSourceSha256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    bool bRenderOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    bool bMeasuredSurveyOrAsBuiltClaimed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    bool bCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    bool bProviderContentBakedCachedTracedOrAnalysed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    bool bR15GroundMaterialPackagesUntouched = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V5D|Public Realm|Provenance")
    bool bExistingRfInputsUntouched = true;
};

/**
 * Two-layer, visual-only V5D public-realm presentation.
 *
 * Core is always rendered after configuration. Fallback is rendered only
 * while the external provider is not ready. The readiness setter changes no
 * geometry, material, transform, collision, navigation, scene-capture, sensor,
 * RF, R15 ground-material, or core-renderer state.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DPublicRealmActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DPublicRealmActor();

    bool ConfigurePublicRealm(
        const FTRIADIstanaExploreV5DPublicRealmAssets& InAssets,
        const FTRIADIstanaExploreV5DPublicRealmProvenance& InProvenance,
        FString& OutError);

    /**
     * Upgrade only the presentation-material references and component
     * overrides of the exact persisted appearance-revision-1 actor. Meshes,
     * topology, transforms, collision, navigation, provenance, visibility,
     * provider readiness, sensor/RF authority, and R15 packages are not
     * changed. Failure restores the admitted revision-1 state in memory.
     */
    bool UpgradeVisualMaterials(
        const FTRIADIstanaExploreV5DPublicRealmAssets& InAssets,
        FString& OutError);

    bool ValidatePublicRealm(FString& OutReport) const;

    /** Exact predecessor admission used only by the hash-gated R2 migration. */
    bool ValidateLegacyVisualMaterialContract(FString& OutReport) const;

    /** Changes only FallbackPublicRealmRenderOnly renderer visibility. */
    bool SetProviderReady(bool bInProviderReady, FString& OutError);

    static bool ValidateProvenanceContract(
        const FTRIADIstanaExploreV5DPublicRealmProvenance& Provenance,
        FString& OutError);

    static const FString& ExpectedCoreMeshObjectPath();
    static const FString& ExpectedFallbackMeshObjectPath();
    static const FString& ExpectedRoadBaseMaterialObjectPath();
    static const FString& ExpectedRoadGraphicMaterialObjectPath();
    static const FString& ExpectedConcreteMaterialObjectPath();
    static const FString& ExpectedProvenanceSchemaRevision();
    static const FString& ExpectedClaimLabel();
    static const FName& ExpectedActorTag();
    static int32 ExpectedAppearanceRevision();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<UStaticMeshComponent> CorePublicRealmRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<UStaticMeshComponent> FallbackPublicRealmRenderOnly;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Assets")
    FTRIADIstanaExploreV5DPublicRealmAssets SavedAssets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Provenance")
    FTRIADIstanaExploreV5DPublicRealmProvenance SavedProvenance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSensorOrRfMaterialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bMeasuredSurveyOrAsBuiltClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bRuntimeGeometryGenerated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bR15GroundMaterialPackagesModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bExistingRfInputsModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bConfigured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bProviderReady = false;

    /** Revision 1 is the immutable planning predecessor; 2 is corrected presentation. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    int32 AppearanceRevision = 1;
};
