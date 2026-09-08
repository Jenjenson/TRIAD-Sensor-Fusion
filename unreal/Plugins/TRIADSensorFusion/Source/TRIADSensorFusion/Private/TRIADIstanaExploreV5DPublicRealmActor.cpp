#include "TRIADIstanaExploreV5DPublicRealmActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
const FString CoreMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/"
         "SM_IPV5D_PublicRealm_Core_Render."
         "SM_IPV5D_PublicRealm_Core_Render"));
const FString FallbackMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/"
         "SM_IPV5D_PublicRealm_Fallback_Render."
         "SM_IPV5D_PublicRealm_Fallback_Render"));
const FString LegacyRoadBaseMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone"));
const FString LegacyRoadGraphicMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadGraphic."
         "MI_IPV5C_OfficialPlanningRoadGraphic"));
const FString RoadAsphaltMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmVisualR2/"
         "Materials/M_IPV5D_PublicRealm_AsphaltDry_R2."
         "M_IPV5D_PublicRealm_AsphaltDry_R2"));
const FString RoadGraphicSuppressionMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmVisualR2/"
         "Materials/M_IPV5D_PublicRealm_RoadGraphicFullyClipped_R2."
         "M_IPV5D_PublicRealm_RoadGraphicFullyClipped_R2"));
const FString ConcreteMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/Materials/"
         "MI_IPV5D_PublicRealmConcrete."
         "MI_IPV5D_PublicRealmConcrete"));
const FString ProvenanceSchemaRevision(
    TEXT("TRIAD_IPV5D_PUBLIC_REALM_PROVENANCE_V1"));
const FString PublicRealmClaimLabel(
    TEXT("SOURCE_IDENTIFIED_RENDER_ONLY_VISUAL_APPROXIMATION_NOT_SURVEY_AS_BUILT_SENSOR_OR_RF_AUTHORITY"));
const FName PublicRealmActorTag(TEXT("TRIADIstanaExploreV5DPublicRealm"));
const FName CoreRenderOnlyTag(TEXT("TRIADV5DCorePublicRealmRenderOnly"));
const FName FallbackRenderOnlyTag(
    TEXT("TRIADV5DFallbackPublicRealmRenderOnly"));
const FName HumanOnlyOverlayTag(TEXT("TRIADHumanOnlyOverlay"));
constexpr int32 LegacyAppearanceRevision = 1;
constexpr int32 PbrAppearanceRevision = 2;

bool IsNonEmptyControlFree(const FString& Value)
{
    if (Value.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool IsCanonicalUppercaseSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        const bool bDecimal = Character >= TEXT('0') && Character <= TEXT('9');
        const bool bUpperHex = Character >= TEXT('A') && Character <= TEXT('F');
        if (!bDecimal && !bUpperHex)
        {
            return false;
        }
    }
    return true;
}

bool IsCanonicalIsoDate(const FString& Value)
{
    if (Value.Len() != 10 || Value[4] != TEXT('-') ||
        Value[7] != TEXT('-'))
    {
        return false;
    }
    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        if (Index != 4 && Index != 7 && !FChar::IsDigit(Value[Index]))
        {
            return false;
        }
    }
    return FDateTime::Validate(
        FCString::Atoi(*Value.Left(4)),
        FCString::Atoi(*Value.Mid(5, 2)),
        FCString::Atoi(*Value.Right(2)),
        0,
        0,
        0,
        0);
}

void ConfigureRenderOnlyComponent(
    UStaticMeshComponent* Component,
    const FName& RenderOnlyTag)
{
    if (!Component)
    {
        return;
    }
    Component->SetRelativeTransform(FTransform::Identity);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(false);
    Component->SetRenderInMainPass(true);
    Component->SetRenderCustomDepth(false);
    Component->bHiddenInSceneCapture = true;
    Component->EmptyOverrideMaterials();
    Component->SetOverlayMaterial(nullptr);
    Component->ComponentTags.AddUnique(RenderOnlyTag);
    Component->ComponentTags.Remove(HumanOnlyOverlayTag);
}

bool ValidateExactMeshSourceBindings(
    const UStaticMesh* Mesh,
    const FString& ExpectedMeshPath,
    const TArray<const FString*>& ExpectedMaterialPaths,
    const TCHAR* Label,
    FString& OutError)
{
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Mesh || Mesh->GetPathName() != ExpectedMeshPath ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialPaths.Num() ||
        (Body && (Body->AggGeom.GetElementCount() != 0 ||
                  Body->CollisionTraceFlag == CTF_UseComplexAsSimple)))
    {
        OutError = FString::Printf(
            TEXT("V5D %s public-realm asset lost its exact object path, material, or no-collision mesh contract."),
            Label);
        return false;
    }
    for (int32 Slot = 0; Slot < ExpectedMaterialPaths.Num(); ++Slot)
    {
        const UMaterialInterface* BoundMaterial =
            Mesh->GetStaticMaterials()[Slot].MaterialInterface;
        const FString* ExpectedPath = ExpectedMaterialPaths[Slot];
        if (!ExpectedPath || !BoundMaterial ||
            BoundMaterial->GetPathName() != *ExpectedPath)
        {
            OutError = FString::Printf(
                TEXT("V5D %s public-realm mesh source slot %d lost its exact immutable planning/concrete binding."),
                Label,
                Slot);
            return false;
        }
    }
    return true;
}

bool ValidateRenderOnlyComponent(
    const UStaticMeshComponent* Component,
    const USceneComponent* ExpectedParent,
    const UStaticMesh* ExpectedMesh,
    const FName& ExpectedTag,
    const bool bExpectedVisible,
    const TArray<const UMaterialInterface*>& ExpectedOverrides,
    FString& OutError)
{
    if (!Component || Component->GetAttachParent() != ExpectedParent ||
        Component->GetStaticMesh() != ExpectedMesh ||
        !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        !Component->GetComponentTransform().Equals(FTransform::Identity, 0.001) ||
        Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetCollisionResponseToChannels() !=
            FCollisionResponseContainer(ECR_Ignore) ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        !Component->CastShadow || Component->bCastContactShadow ||
        Component->bAffectDistanceFieldLighting ||
        !Component->bRenderInMainPass || Component->bRenderCustomDepth ||
        !Component->bHiddenInSceneCapture ||
        Component->GetNumOverrideMaterials() != ExpectedOverrides.Num() ||
        Component->GetOverlayMaterial() != nullptr ||
        !Component->ComponentTags.Contains(ExpectedTag) ||
        Component->ComponentTags.Contains(HumanOnlyOverlayTag) ||
        Component->IsVisible() != bExpectedVisible ||
        Component->bHiddenInGame == bExpectedVisible)
    {
        OutError = TEXT("A V5D public-realm component lost its exact identity/static render-only, no-collision, no-navigation, scene-capture-excluded renderer contract.");
        return false;
    }
    for (int32 Slot = 0; Slot < ExpectedOverrides.Num(); ++Slot)
    {
        if (!ExpectedOverrides[Slot] ||
            Component->GetMaterial(Slot) != ExpectedOverrides[Slot])
        {
            OutError = TEXT("A V5D public-realm component lost its exact ordered presentation-material override.");
            return false;
        }
    }
    return true;
}

bool ValidatePresentationAssets(
    const FTRIADIstanaExploreV5DPublicRealmAssets& Assets,
    FString& OutError)
{
    const TArray<const FString*> CoreSourceMaterials = {
        &LegacyRoadBaseMaterialObjectPath};
    const TArray<const FString*> FallbackSourceMaterials = {
        &LegacyRoadBaseMaterialObjectPath,
        &LegacyRoadGraphicMaterialObjectPath,
        &ConcreteMaterialObjectPath};
    if (!Assets.RoadBaseMaterial ||
        Assets.RoadBaseMaterial->GetPathName() !=
            RoadAsphaltMaterialObjectPath ||
        !Assets.RoadGraphicMaterial ||
        Assets.RoadGraphicMaterial->GetPathName() !=
            RoadGraphicSuppressionMaterialObjectPath ||
        !Assets.ConcreteMaterial ||
        Assets.ConcreteMaterial->GetPathName() != ConcreteMaterialObjectPath ||
        !ValidateExactMeshSourceBindings(
            Assets.CoreMesh,
            CoreMeshObjectPath,
            CoreSourceMaterials,
            TEXT("core"),
            OutError) ||
        !ValidateExactMeshSourceBindings(
            Assets.FallbackMesh,
            FallbackMeshObjectPath,
            FallbackSourceMaterials,
            TEXT("fallback"),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D public realm requires exact dry-asphalt road presentation and a distinct fully clipped planning-graphic presentation while retaining immutable source bindings.");
        }
        return false;
    }
    return true;
}
} // namespace

ATRIADIstanaExploreV5DPublicRealmActor::
    ATRIADIstanaExploreV5DPublicRealmActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetActorEnableCollision(false);
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("V5DPublicRealmRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    SceneRoot->SetRelativeTransform(FTransform::Identity);

    CorePublicRealmRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("CorePublicRealmRenderOnly"));
    FallbackPublicRealmRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("FallbackPublicRealmRenderOnly"));
    CorePublicRealmRenderOnly->SetupAttachment(SceneRoot);
    FallbackPublicRealmRenderOnly->SetupAttachment(SceneRoot);
    ConfigureRenderOnlyComponent(CorePublicRealmRenderOnly, CoreRenderOnlyTag);
    ConfigureRenderOnlyComponent(
        FallbackPublicRealmRenderOnly,
        FallbackRenderOnlyTag);

    // Unconfigured defaults are inert. ConfigurePublicRealm atomically makes
    // core and not-ready fallback presentation visible after exact validation.
    CorePublicRealmRenderOnly->SetVisibility(false, true);
    CorePublicRealmRenderOnly->SetHiddenInGame(true, true);
    FallbackPublicRealmRenderOnly->SetVisibility(false, true);
    FallbackPublicRealmRenderOnly->SetHiddenInGame(true, true);
    Tags.AddUnique(ExpectedActorTag());
    Tags.Remove(HumanOnlyOverlayTag);
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::
    ExpectedCoreMeshObjectPath()
{
    return CoreMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::
    ExpectedFallbackMeshObjectPath()
{
    return FallbackMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::
    ExpectedRoadBaseMaterialObjectPath()
{
    return RoadAsphaltMaterialObjectPath;
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::
    ExpectedRoadGraphicMaterialObjectPath()
{
    return RoadGraphicSuppressionMaterialObjectPath;
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::
    ExpectedConcreteMaterialObjectPath()
{
    return ConcreteMaterialObjectPath;
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::
    ExpectedProvenanceSchemaRevision()
{
    return ProvenanceSchemaRevision;
}

const FString& ATRIADIstanaExploreV5DPublicRealmActor::ExpectedClaimLabel()
{
    return PublicRealmClaimLabel;
}

const FName& ATRIADIstanaExploreV5DPublicRealmActor::ExpectedActorTag()
{
    return PublicRealmActorTag;
}

int32 ATRIADIstanaExploreV5DPublicRealmActor::ExpectedAppearanceRevision()
{
    return PbrAppearanceRevision;
}

bool ATRIADIstanaExploreV5DPublicRealmActor::ValidateProvenanceContract(
    const FTRIADIstanaExploreV5DPublicRealmProvenance& Provenance,
    FString& OutError)
{
    OutError.Reset();
    if (Provenance.SchemaRevision != ExpectedProvenanceSchemaRevision() ||
        !IsCanonicalIsoDate(Provenance.SourceEpoch) ||
        !IsNonEmptyControlFree(Provenance.CoreSourceIdentifier) ||
        !IsCanonicalUppercaseSha256(Provenance.CoreSourceSha256) ||
        !IsNonEmptyControlFree(Provenance.FallbackSourceIdentifier) ||
        !IsCanonicalUppercaseSha256(Provenance.FallbackSourceSha256) ||
        !Provenance.bRenderOnly ||
        Provenance.bMeasuredSurveyOrAsBuiltClaimed ||
        Provenance.bCollisionNavigationSensorOrRfAuthority ||
        Provenance.bProviderContentBakedCachedTracedOrAnalysed ||
        !Provenance.bR15GroundMaterialPackagesUntouched ||
        !Provenance.bExistingRfInputsUntouched)
    {
        OutError = TEXT("V5D public-realm provenance requires the exact V1 schema, ISO source epoch, nonempty source identifiers, uppercase SHA-256 digests, render-only status, no survey/as-built/sensor/RF/provider-derived authority, and explicit R15/RF preservation.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DPublicRealmActor::ConfigurePublicRealm(
    const FTRIADIstanaExploreV5DPublicRealmAssets& InAssets,
    const FTRIADIstanaExploreV5DPublicRealmProvenance& InProvenance,
    FString& OutError)
{
    OutError.Reset();
    if (bConfigured || !GetWorld() ||
        !CorePublicRealmRenderOnly || !FallbackPublicRealmRenderOnly ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidatePresentationAssets(InAssets, OutError) ||
        !ValidateProvenanceContract(InProvenance, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D public realm requires a fresh identity actor in a world plus the exact prebuilt assets, materials, and provenance contract.");
        }
        return false;
    }

    SavedAssets = InAssets;
    SavedProvenance = InProvenance;
    SetActorEnableCollision(false);
    Tags.Remove(HumanOnlyOverlayTag);
    CorePublicRealmRenderOnly->SetStaticMesh(SavedAssets.CoreMesh);
    FallbackPublicRealmRenderOnly->SetStaticMesh(SavedAssets.FallbackMesh);
    ConfigureRenderOnlyComponent(CorePublicRealmRenderOnly, CoreRenderOnlyTag);
    ConfigureRenderOnlyComponent(
        FallbackPublicRealmRenderOnly,
        FallbackRenderOnlyTag);
    CorePublicRealmRenderOnly->SetMaterial(0, SavedAssets.RoadBaseMaterial);
    FallbackPublicRealmRenderOnly->SetMaterial(
        0,
        SavedAssets.RoadBaseMaterial);
    // The source RoadGraphic is an official planning display stroke, not
    // observed lane/crossing paint. Its distinct constant-zero masked
    // override fully clips those triangles without changing frozen topology.
    FallbackPublicRealmRenderOnly->SetMaterial(
        1,
        SavedAssets.RoadGraphicMaterial);
    CorePublicRealmRenderOnly->SetVisibility(true, true);
    CorePublicRealmRenderOnly->SetHiddenInGame(false, true);
    FallbackPublicRealmRenderOnly->SetVisibility(true, true);
    FallbackPublicRealmRenderOnly->SetHiddenInGame(false, true);
    bProviderReady = false;
    AppearanceRevision = PbrAppearanceRevision;
    bConfigured = true;

    FString Report;
    if (!ValidatePublicRealm(Report))
    {
        CorePublicRealmRenderOnly->EmptyOverrideMaterials();
        FallbackPublicRealmRenderOnly->EmptyOverrideMaterials();
        CorePublicRealmRenderOnly->SetStaticMesh(nullptr);
        FallbackPublicRealmRenderOnly->SetStaticMesh(nullptr);
        CorePublicRealmRenderOnly->SetVisibility(false, true);
        CorePublicRealmRenderOnly->SetHiddenInGame(true, true);
        FallbackPublicRealmRenderOnly->SetVisibility(false, true);
        FallbackPublicRealmRenderOnly->SetHiddenInGame(true, true);
        SavedAssets = FTRIADIstanaExploreV5DPublicRealmAssets();
        SavedProvenance = FTRIADIstanaExploreV5DPublicRealmProvenance();
        bProviderReady = false;
        AppearanceRevision = LegacyAppearanceRevision;
        bConfigured = false;
        OutError = TEXT("V5D public-realm post-configuration validation failed atomically: ") +
            Report;
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DPublicRealmActor::UpgradeVisualMaterials(
    const FTRIADIstanaExploreV5DPublicRealmAssets& InAssets,
    FString& OutError)
{
    OutError.Reset();
    FString LegacyReport;
    if (!ValidateLegacyVisualMaterialContract(LegacyReport) ||
        !ValidatePresentationAssets(InAssets, OutError) ||
        InAssets.CoreMesh != SavedAssets.CoreMesh ||
        InAssets.FallbackMesh != SavedAssets.FallbackMesh ||
        InAssets.ConcreteMaterial != SavedAssets.ConcreteMaterial)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D public-realm visual upgrade requires the exact revision-1 actor and changes only its road presentation materials. ") +
                LegacyReport;
        }
        return false;
    }

    const FTRIADIstanaExploreV5DPublicRealmAssets PreviousAssets =
        SavedAssets;
    Modify();
    CorePublicRealmRenderOnly->Modify();
    FallbackPublicRealmRenderOnly->Modify();
    SavedAssets = InAssets;
    CorePublicRealmRenderOnly->SetMaterial(0, SavedAssets.RoadBaseMaterial);
    FallbackPublicRealmRenderOnly->SetMaterial(
        0,
        SavedAssets.RoadBaseMaterial);
    FallbackPublicRealmRenderOnly->SetMaterial(
        1,
        SavedAssets.RoadGraphicMaterial);
    AppearanceRevision = PbrAppearanceRevision;

    FString CorrectedReport;
    if (!ValidatePublicRealm(CorrectedReport))
    {
        CorePublicRealmRenderOnly->EmptyOverrideMaterials();
        FallbackPublicRealmRenderOnly->EmptyOverrideMaterials();
        SavedAssets = PreviousAssets;
        AppearanceRevision = LegacyAppearanceRevision;
        FString RestoredReport;
        const bool bRestored =
            ValidateLegacyVisualMaterialContract(RestoredReport);
        OutError = FString::Printf(
            TEXT("V5D public-realm visual-material upgrade failed and restored revision 1=%s: %s restored={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            *CorrectedReport,
            *RestoredReport);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DPublicRealmActor::SetProviderReady(
    bool bInProviderReady,
    FString& OutError)
{
    OutError.Reset();
    FString BeforeReport;
    if (!bConfigured || !ValidatePublicRealm(BeforeReport))
    {
        OutError = TEXT("V5D public-realm provider readiness requires an already valid configured actor: ") +
            BeforeReport;
        return false;
    }

    const bool bCoreVisibleBefore = CorePublicRealmRenderOnly->IsVisible();
    const bool bCoreHiddenInGameBefore =
        CorePublicRealmRenderOnly->bHiddenInGame;
    const bool bFallbackVisibleBefore =
        FallbackPublicRealmRenderOnly->IsVisible();
    const bool bFallbackHiddenInGameBefore =
        FallbackPublicRealmRenderOnly->bHiddenInGame;
    const bool bProviderReadyBefore = bProviderReady;

    // This is intentionally the only runtime renderer mutation owned here.
    FallbackPublicRealmRenderOnly->SetVisibility(!bInProviderReady, true);
    FallbackPublicRealmRenderOnly->SetHiddenInGame(bInProviderReady, true);
    bProviderReady = bInProviderReady;

    FString AfterReport;
    if (CorePublicRealmRenderOnly->IsVisible() != bCoreVisibleBefore ||
        CorePublicRealmRenderOnly->bHiddenInGame != bCoreHiddenInGameBefore ||
        !ValidatePublicRealm(AfterReport))
    {
        FallbackPublicRealmRenderOnly->SetVisibility(
            bFallbackVisibleBefore,
            true);
        FallbackPublicRealmRenderOnly->SetHiddenInGame(
            bFallbackHiddenInGameBefore,
            true);
        bProviderReady = bProviderReadyBefore;
        OutError = TEXT("V5D public-realm fallback-only provider transition failed and was restored: ") +
            AfterReport;
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DPublicRealmActor::ValidatePublicRealm(
    FString& OutReport) const
{
    FString Error;
    const TArray<const UMaterialInterface*> CoreOverrides = {
        SavedAssets.RoadBaseMaterial};
    const TArray<const UMaterialInterface*> FallbackOverrides = {
        SavedAssets.RoadBaseMaterial,
        SavedAssets.RoadGraphicMaterial};
    if (!bConfigured || !GetWorld() ||
        !Tags.Contains(ExpectedActorTag()) ||
        Tags.Contains(HumanOnlyOverlayTag) ||
        ClaimLabel != ExpectedClaimLabel() ||
        !bRenderOnly || bCollisionOrNavigationAuthority ||
        bSensorOrRfMaterialAuthority || bMeasuredSurveyOrAsBuiltClaimed ||
        bRuntimeGeometryGenerated || bR15GroundMaterialPackagesModified ||
        bExistingRfInputsModified ||
        AppearanceRevision != PbrAppearanceRevision ||
        PrimaryActorTick.bCanEverTick ||
        GetActorEnableCollision() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SceneRoot || SceneRoot != RootComponent ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !SceneRoot->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidatePresentationAssets(SavedAssets, Error) ||
        !ValidateProvenanceContract(SavedProvenance, Error) ||
        !ValidateRenderOnlyComponent(
            CorePublicRealmRenderOnly,
            SceneRoot,
            SavedAssets.CoreMesh,
            CoreRenderOnlyTag,
            true,
            CoreOverrides,
            Error) ||
        !ValidateRenderOnlyComponent(
            FallbackPublicRealmRenderOnly,
            SceneRoot,
            SavedAssets.FallbackMesh,
            FallbackRenderOnlyTag,
            !bProviderReady,
            FallbackOverrides,
            Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D public realm lost its exact asset, identity, renderer, provenance, R15-preservation, or no-authority contract.")
            : Error;
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_PUBLIC_REALM_VALID appearanceRevision=%d pbrAsphaltOverride=true fullyClippedPlanningGraphic=true laneOrCrossingPaintAuthored=false core=visible fallback=%s providerReady=%s identity=true static=true runtimeGeometry=false renderOnly=true sceneCaptureSensor=false collision=false navigation=false sensorRfAuthority=false r15GroundMaterialsModified=false existingRfInputsModified=false provenanceSchema=%s sourceEpoch=%s."),
        AppearanceRevision,
        bProviderReady ? TEXT("hidden") : TEXT("visible"),
        bProviderReady ? TEXT("true") : TEXT("false"),
        *SavedProvenance.SchemaRevision,
        *SavedProvenance.SourceEpoch);
    return true;
}

bool ATRIADIstanaExploreV5DPublicRealmActor::
    ValidateLegacyVisualMaterialContract(FString& OutReport) const
{
    FString Error;
    const TArray<const FString*> CoreSourceMaterials = {
        &LegacyRoadBaseMaterialObjectPath};
    const TArray<const FString*> FallbackSourceMaterials = {
        &LegacyRoadBaseMaterialObjectPath,
        &LegacyRoadGraphicMaterialObjectPath,
        &ConcreteMaterialObjectPath};
    const TArray<const UMaterialInterface*> NoOverrides;
    if (!bConfigured || !GetWorld() ||
        !Tags.Contains(ExpectedActorTag()) ||
        Tags.Contains(HumanOnlyOverlayTag) ||
        ClaimLabel != ExpectedClaimLabel() ||
        !bRenderOnly || bCollisionOrNavigationAuthority ||
        bSensorOrRfMaterialAuthority || bMeasuredSurveyOrAsBuiltClaimed ||
        bRuntimeGeometryGenerated || bR15GroundMaterialPackagesModified ||
        bExistingRfInputsModified || PrimaryActorTick.bCanEverTick ||
        GetActorEnableCollision() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SceneRoot || SceneRoot != RootComponent ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !SceneRoot->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        AppearanceRevision != LegacyAppearanceRevision ||
        !SavedAssets.CoreMesh ||
        SavedAssets.CoreMesh->GetPathName() != CoreMeshObjectPath ||
        !SavedAssets.FallbackMesh ||
        SavedAssets.FallbackMesh->GetPathName() != FallbackMeshObjectPath ||
        !SavedAssets.RoadBaseMaterial ||
        SavedAssets.RoadBaseMaterial->GetPathName() !=
            LegacyRoadBaseMaterialObjectPath ||
        !SavedAssets.RoadGraphicMaterial ||
        SavedAssets.RoadGraphicMaterial->GetPathName() !=
            LegacyRoadGraphicMaterialObjectPath ||
        !SavedAssets.ConcreteMaterial ||
        SavedAssets.ConcreteMaterial->GetPathName() !=
            ConcreteMaterialObjectPath ||
        !ValidateExactMeshSourceBindings(
            SavedAssets.CoreMesh,
            CoreMeshObjectPath,
            CoreSourceMaterials,
            TEXT("legacy core"),
            Error) ||
        !ValidateExactMeshSourceBindings(
            SavedAssets.FallbackMesh,
            FallbackMeshObjectPath,
            FallbackSourceMaterials,
            TEXT("legacy fallback"),
            Error) ||
        !ValidateProvenanceContract(SavedProvenance, Error) ||
        !ValidateRenderOnlyComponent(
            CorePublicRealmRenderOnly,
            SceneRoot,
            SavedAssets.CoreMesh,
            CoreRenderOnlyTag,
            true,
            NoOverrides,
            Error) ||
        !ValidateRenderOnlyComponent(
            FallbackPublicRealmRenderOnly,
            SceneRoot,
            SavedAssets.FallbackMesh,
            FallbackRenderOnlyTag,
            !bProviderReady,
            NoOverrides,
            Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D public realm is not the exact configured appearance-revision-1 predecessor.")
            : Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_PUBLIC_REALM_LEGACY_VISUAL_VALID appearanceRevision=1 overrides=0 exactPlanningBindings=true.");
    return true;
}
