#include "TRIADIstanaExploreV5DTemasekShophouseActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DTemasekShophouseProvenance.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADIstanaExploreV5DTemasekShophouse,
    Log,
    All);

namespace
{
const FName TemasekShophouseActorTag(
    TEXT("TRIADIstanaExploreV5DTemasekShophouseR24"));
const FName DedicatedRenderComponentTag(
    TEXT("TRIADV5DR24TemasekShophouseDedicatedPersistentRenderOnly"));
const FString TemasekShophouseMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/"
         "R24TemasekShophouse/SM_IPV5D_R24_TemasekShophouse_Render."
         "SM_IPV5D_R24_TemasekShophouse_Render"));

struct FMaterialSpec
{
    const TCHAR* Slot;
    const TCHAR* Asset;
    uint32 Triangles;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("M_TSH_Brass"), TEXT("M_TSH_Brass_PBR_R24"), 396},
    {TEXT("M_TSH_CharcoalTrim"), TEXT("M_TSH_CharcoalTrim_PBR_R24"), 3492},
    {TEXT("M_TSH_DarkWindowGlass"), TEXT("M_TSH_DarkWindowGlass_PBR_R24"), 612},
    {TEXT("M_TSH_PaverDark"), TEXT("M_TSH_PaverDark_PBR_R24"), 468},
    {TEXT("M_TSH_PaverLight"), TEXT("M_TSH_PaverLight_PBR_R24"), 480},
    {TEXT("M_TSH_PinkGreyMosaic"), TEXT("M_TSH_PinkGreyMosaic_PBR_R24"), 192},
    {TEXT("M_TSH_PrecastConcrete"), TEXT("M_TSH_PrecastConcrete_PBR_R24"), 900},
    {TEXT("M_TSH_Rainwater"), TEXT("M_TSH_Rainwater_PBR_R24"), 36},
    {TEXT("M_TSH_ReusedTimber"), TEXT("M_TSH_ReusedTimber_PBR_R24"), 192},
    {TEXT("M_TSH_ShadowRecess"), TEXT("M_TSH_ShadowRecess_PBR_R24"), 468},
    {TEXT("M_TSH_SoilMulch"), TEXT("M_TSH_SoilMulch_PBR_R24"), 72},
    {TEXT("M_TSH_SolarPanel"), TEXT("M_TSH_SolarPanel_PBR_R24"), 216},
    {TEXT("M_TSH_Terracotta"), TEXT("M_TSH_Terracotta_PBR_R24"), 168},
    {TEXT("M_TSH_Timber"), TEXT("M_TSH_Timber_PBR_R24"), 2796},
    {TEXT("M_TSH_WhiteShanghaiPlaster"), TEXT("M_TSH_WhiteShanghaiPlaster_PBR_R24"), 5272},
};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 15);

FString MaterialObjectPath(const TCHAR* Asset)
{
    return FString::Printf(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/"
             "R24TemasekShophouse/Materials/%s.%s"),
        Asset,
        Asset);
}

const UTRIADIstanaExploreV5DTemasekShophouseProvenance* FindProvenance(
    const UStaticMesh* Mesh,
    int32& OutCount)
{
    OutCount = 0;
    const UTRIADIstanaExploreV5DTemasekShophouseProvenance* Result = nullptr;
    const TArray<UAssetUserData*>* Data = Mesh
        ? Mesh->GetAssetUserDataArray()
        : nullptr;
    if (Data)
    {
        for (const UAssetUserData* Row : *Data)
        {
            if (Row && Row->IsA(
                    UTRIADIstanaExploreV5DTemasekShophouseProvenance::
                        StaticClass()))
            {
                ++OutCount;
                Result = Cast<
                    UTRIADIstanaExploreV5DTemasekShophouseProvenance>(Row);
            }
        }
    }
    return Result;
}

bool ValidateMesh(const UStaticMesh* Mesh, FString& OutError)
{
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    int32 ProvenanceCount = 0;
    const UTRIADIstanaExploreV5DTemasekShophouseProvenance* Provenance =
        FindProvenance(Mesh, ProvenanceCount);
    FString RecomputedPayloadSha256;
    FString PayloadDigestError;
    const bool bPayloadDigestValid = Provenance &&
        UTRIADIstanaExploreV5DTemasekShophouseProvenance::
            ComputeCookedRenderPayloadSha256(
                Mesh,
                RecomputedPayloadSha256,
                PayloadDigestError) &&
        RecomputedPayloadSha256 == Provenance->CookedRenderPayloadSha256;
#if WITH_EDITORONLY_DATA
    const bool bFullNaniteSettings = Mesh && Mesh->NaniteSettings.bEnabled &&
        FMath::IsNearlyEqual(Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) &&
        FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) &&
        Mesh->NaniteSettings.FallbackTarget ==
            ENaniteFallbackTarget::PercentTriangles &&
        FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles,
            1.0f) &&
        FMath::IsNearlyZero(Mesh->NaniteSettings.FallbackRelativeError);
#else
    const bool bFullNaniteSettings = true;
#endif
    if (!Mesh || Mesh->GetPathName() != TemasekShophouseMeshObjectPath ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() != 15760 ||
        RenderData->LODResources[0].GetNumVertices() == 0 ||
        RenderData->LODResources[0].Sections.Num() != 15 ||
        Mesh->GetStaticMaterials().Num() != 15 || !Mesh->HasValidNaniteData() ||
        !bFullNaniteSettings || !Body ||
        Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision() ||
        !Mesh->bAllowCPUAccess ||
        ProvenanceCount != 1 || !Provenance ||
        Provenance->GetClass() !=
            UTRIADIstanaExploreV5DTemasekShophouseProvenance::StaticClass() ||
        Provenance->GetOuter() != Mesh || !Provenance->IsCanonicalContract() ||
        !bPayloadDigestValid)
    {
        OutError = TEXT("R24B Temasek Shophouse mesh lost its exact 15,760-triangle, fifteen-section, zero-baked-foliage, full-Nanite, zero-collision, no-navigation, CPU-readable cooked-provenance/render-payload contract. ") +
            PayloadDigestError;
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector Minimum = Bounds.Origin - Bounds.BoxExtent;
    const FVector Maximum = Bounds.Origin + Bounds.BoxExtent;
    if (!Minimum.Equals(FVector(-2225.0, -2305.0, 0.0), 0.25) ||
        !Maximum.Equals(FVector(2225.0, 1681.9333, 1463.0642), 0.25))
    {
        OutError = FString::Printf(
            TEXT("R24B Temasek Shophouse imported centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            Minimum.X, Minimum.Y, Minimum.Z,
            Maximum.X, Maximum.Y, Maximum.Z);
        return false;
    }

    TArray<bool> Seen;
    Seen.Init(false, UE_ARRAY_COUNT(MaterialSpecs));
    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    for (const FStaticMeshSection& Section : Lod.Sections)
    {
        if (!Seen.IsValidIndex(Section.MaterialIndex) ||
            Seen[Section.MaterialIndex] ||
            Section.NumTriangles != MaterialSpecs[Section.MaterialIndex].Triangles)
        {
            OutError = TEXT("R24B Temasek Shophouse semantic render-section census drifted.");
            return false;
        }
        Seen[Section.MaterialIndex] = true;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MaterialSpecs); ++Index)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Index];
        const UMaterial* Material = Cast<UMaterial>(Binding.MaterialInterface);
        bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            Binding.ImportedMaterialSlotName == FName(MaterialSpecs[Index].Slot);
#endif
        if (!Seen[Index] ||
            Binding.MaterialSlotName != FName(MaterialSpecs[Index].Slot) ||
            !bImportedSlotNameMatches ||
            !Material || Material->GetClass() != UMaterial::StaticClass() ||
            Material->GetPathName() != MaterialObjectPath(MaterialSpecs[Index].Asset) ||
            Material->MaterialDomain != MD_Surface ||
            Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
            !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
            Material->bUseMaterialAttributes ||
            Material->bEnableTessellation ||
            Material->bEnableDisplacementFade ||
            !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
            !Material->GetUsageByFlag(MATUSAGE_Nanite))
        {
            OutError = FString::Printf(
                TEXT("R24B Temasek Shophouse material binding or bounded procedural PBR policy drifted at slot %d ('%s')."),
                Index,
                MaterialSpecs[Index].Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}
} // namespace

const FName& ATRIADIstanaExploreV5DTemasekShophouseActor::ExpectedActorTag()
{
    return TemasekShophouseActorTag;
}

const FString& ATRIADIstanaExploreV5DTemasekShophouseActor::
    ExpectedMeshObjectPath()
{
    return TemasekShophouseMeshObjectPath;
}

FTransform ATRIADIstanaExploreV5DTemasekShophouseActor::
    ExpectedPlacementTransform()
{
    return FTransform(
        FRotator(0.0, -162.5152283523459, 0.0),
        FVector(40411.657951, 88424.311139, 0.0),
        FVector(1.053891, 1.221655, 1.0));
}

bool ATRIADIstanaExploreV5DTemasekShophouseActor::ValidateRuntimeMesh(
    UStaticMesh* Mesh,
    FString& OutError)
{
    return ValidateMesh(Mesh, OutError);
}

ATRIADIstanaExploreV5DTemasekShophouseActor::
    ATRIADIstanaExploreV5DTemasekShophouseActor()
{
    // Global provider readiness is sampled for telemetry only. This cheap tick
    // is ordered after the policy actor when an exact binding is available;
    // the provider state never gates the dedicated landmark renderer.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;
    PrimaryActorTick.TickInterval = 0.0f;
    SetActorEnableCollision(false);
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Root->SetMobility(EComponentMobility::Static);
    DedicatedRenderOnlyComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("DedicatedTemasekShophouseRenderOnly"));
    DedicatedRenderOnlyComponent->SetupAttachment(Root);
    DedicatedRenderOnlyComponent->SetMobility(EComponentMobility::Static);
    DedicatedRenderOnlyComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    DedicatedRenderOnlyComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    DedicatedRenderOnlyComponent->SetGenerateOverlapEvents(false);
    DedicatedRenderOnlyComponent->SetCanEverAffectNavigation(false);
    DedicatedRenderOnlyComponent->SetCastShadow(true);
    DedicatedRenderOnlyComponent->SetRenderInMainPass(true);
    DedicatedRenderOnlyComponent->SetRenderInDepthPass(true);
    DedicatedRenderOnlyComponent->EmptyOverrideMaterials();
    DedicatedRenderOnlyComponent->SetOverlayMaterial(nullptr);
    DedicatedRenderOnlyComponent->ComponentTags.AddUnique(
        DedicatedRenderComponentTag);
    Tags.AddUnique(TemasekShophouseActorTag);
}

bool ATRIADIstanaExploreV5DTemasekShophouseActor::
    ConfigureTemasekShophouse(
        UStaticMesh* Mesh,
        bool bInProviderReady,
        FString& OutReport)
{
    FString MeshError;
    if (!DedicatedRenderOnlyComponent || !ValidateMesh(Mesh, MeshError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_CONFIGURE_REFUSED: ") +
            MeshError;
        return false;
    }
    SetActorTransform(
        ExpectedPlacementTransform(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    DedicatedRenderOnlyComponent->SetRelativeTransform(FTransform::Identity);
    DedicatedRenderOnlyComponent->SetStaticMesh(Mesh);
    DedicatedRenderOnlyComponent->EmptyOverrideMaterials();
    DedicatedRenderOnlyComponent->SetOverlayMaterial(nullptr);
    DedicatedRenderOnlyComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    DedicatedRenderOnlyComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    DedicatedRenderOnlyComponent->SetGenerateOverlapEvents(false);
    DedicatedRenderOnlyComponent->SetCanEverAffectNavigation(false);
    if (!RecordProviderReadyState(bInProviderReady, OutReport) ||
        !ValidateTemasekShophouse(OutReport))
    {
        return false;
    }
    return true;
}

bool ATRIADIstanaExploreV5DTemasekShophouseActor::
    RecordProviderReadyState(bool bInProviderReady, FString& OutError)
{
    if (!DedicatedRenderOnlyComponent)
    {
        OutError = TEXT("R24B Temasek Shophouse provider telemetry has no render component.");
        return false;
    }
    DedicatedRenderOnlyComponent->SetVisibility(true, true);
    DedicatedRenderOnlyComponent->SetHiddenInGame(false, true);
    bProviderReady = bInProviderReady;
    bDedicatedOverlayVisible = true;
    if (!DedicatedRenderOnlyComponent->IsVisible() ||
        DedicatedRenderOnlyComponent->bHiddenInGame)
    {
        OutError = TEXT("R24B Temasek Shophouse provider telemetry changed persistent overlay visibility.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTemasekShophouseActor::
    SynchronizeWithContextPolicy(FString& OutReport)
{
    UWorld* World = GetWorld();
    int32 PolicyCount = 0;
    int32 LandmarkCount = 0;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    if (World)
    {
        for (TActorIterator<ATRIADIstanaExploreV5DContextPolicyActor> It(World);
             It;
             ++It)
        {
            if (IsValid(*It))
            {
                ++PolicyCount;
                Policy = *It;
            }
        }
        for (TActorIterator<ATRIADIstanaExploreV5DTemasekShophouseActor> It(World);
             It;
             ++It)
        {
            if (IsValid(*It))
            {
                ++LandmarkCount;
            }
        }
    }

    if (PolicyCount == 1 && Policy &&
        RuntimeContextPolicyActor.Get() != Policy)
    {
        if (ATRIADIstanaExploreV5DContextPolicyActor* Previous =
                RuntimeContextPolicyActor.Get())
        {
            RemoveTickPrerequisiteActor(Previous);
        }
        RuntimeContextPolicyActor = Policy;
        AddTickPrerequisiteActor(Policy);
    }
    else if (PolicyCount != 1 && RuntimeContextPolicyActor.IsValid())
    {
        RemoveTickPrerequisiteActor(RuntimeContextPolicyActor.Get());
        RuntimeContextPolicyActor.Reset();
    }

    const bool bBasicBindingValid = World && PolicyCount == 1 &&
        LandmarkCount == 1 && Policy && Policy->HasActorBegunPlay();
    const bool bReady = bBasicBindingValid &&
        Policy->bLocalBuildingFallbackCurrentlyHidden;
    const double NowSeconds = World ? World->GetTimeSeconds() : 0.0;
    const bool bReadinessChanged = bProviderReady != bReady;
    const bool bFullAuditDue = bRuntimeAssetContractFailClosed ||
        bReadinessChanged ||
        NowSeconds >= NextRuntimeContractAuditTimeSeconds;
    FString TelemetryError;
    if (!RecordProviderReadyState(bReady, TelemetryError))
    {
        bRuntimeProviderTelemetryBindingValid = false;
        bRuntimeAssetContractFailClosed = true;
        OutReport =
            TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSET_FAIL_CLOSED telemetry={") +
            TelemetryError + TEXT("}");
        return false;
    }
    if (bFullAuditDue)
    {
        NextRuntimeContractAuditTimeSeconds = NowSeconds + 0.5;
    }

    FString ExistingReport;
    if (bFullAuditDue && !ValidateTemasekShophouse(ExistingReport))
    {
        DedicatedRenderOnlyComponent->SetVisibility(false, true);
        DedicatedRenderOnlyComponent->SetHiddenInGame(true, true);
        bDedicatedOverlayVisible = false;
        bRuntimeProviderTelemetryBindingValid = false;
        bRuntimeAssetContractFailClosed = true;
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSET_FAIL_CLOSED policyCount=%d landmarkCount=%d providerReadyObserved=%s landmark={%s}"),
            PolicyCount,
            LandmarkCount,
            bReady ? TEXT("true") : TEXT("false"),
            *ExistingReport);
        return false;
    }

    FString PolicyReport;
    const bool bPolicyContractValid = bBasicBindingValid &&
        (!bFullAuditDue || Policy->ValidateHybridContext(PolicyReport));
    bRuntimeProviderTelemetryBindingValid = bPolicyContractValid;
    bRuntimeAssetContractFailClosed = false;
    if (!bPolicyContractValid)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_PERSISTENT_OVERLAY_VALID providerTelemetryBinding=false providerReadyObserved=%s policyCount=%d landmarkCount=%d overlayVisible=true visibilityInvariantAcrossProviderTransitions=true policy={%s}"),
            bReady ? TEXT("true") : TEXT("false"),
            PolicyCount,
            LandmarkCount,
            *PolicyReport);
        return true;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_PERSISTENT_OVERLAY_VALID providerTelemetryBinding=true providerReadyObserved=%s overlayVisible=true visibilityInvariantAcrossProviderTransitions=true orderedAfterContextPolicy=true fullContractAuditHz=2"),
        bReady ? TEXT("true") : TEXT("false"));
    return true;
}

void ATRIADIstanaExploreV5DTemasekShophouseActor::BeginPlay()
{
    Super::BeginPlay();
    FString Report;
    if (!SynchronizeWithContextPolicy(Report))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DTemasekShophouse,
            Warning,
            TEXT("%s"),
            *Report);
        bRuntimePolicyFailureWasLogged = true;
    }
}

void ATRIADIstanaExploreV5DTemasekShophouseActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FString Report;
    if (!SynchronizeWithContextPolicy(Report))
    {
        if (!bRuntimePolicyFailureWasLogged)
        {
            UE_LOG(
                LogTRIADIstanaExploreV5DTemasekShophouse,
                Warning,
                TEXT("%s"),
                *Report);
            bRuntimePolicyFailureWasLogged = true;
        }
        return;
    }
    if (bRuntimePolicyFailureWasLogged)
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DTemasekShophouse,
            Display,
            TEXT("%s"),
            *Report);
        bRuntimePolicyFailureWasLogged = false;
    }
}

void ATRIADIstanaExploreV5DTemasekShophouseActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (ATRIADIstanaExploreV5DContextPolicyActor* Policy =
            RuntimeContextPolicyActor.Get())
    {
        RemoveTickPrerequisiteActor(Policy);
    }
    RuntimeContextPolicyActor.Reset();
    NextRuntimeContractAuditTimeSeconds = 0.0;
    bRuntimeProviderTelemetryBindingValid = false;
    Super::EndPlay(EndPlayReason);
}

bool ATRIADIstanaExploreV5DTemasekShophouseActor::
    ValidateTemasekShophouse(FString& OutReport) const
{
    FString MeshError;
#if WITH_EDITOR
    const bool bHiddenInEditor = IsHiddenEd();
#else
    const bool bHiddenInEditor = false;
#endif
    if (!Tags.Contains(TemasekShophouseActorTag) ||
        !DedicatedRenderOnlyComponent ||
        !ValidateMesh(DedicatedRenderOnlyComponent->GetStaticMesh(), MeshError) ||
        !GetActorTransform().Equals(ExpectedPlacementTransform(), 0.01) ||
        IsHidden() || bHiddenInEditor ||
        !RootComponent || RootComponent->Mobility != EComponentMobility::Static ||
        DedicatedRenderOnlyComponent->GetAttachParent() != RootComponent ||
        !DedicatedRenderOnlyComponent->GetRelativeTransform().Equals(
            FTransform::Identity,
            0.001) ||
        DedicatedRenderOnlyComponent->Mobility != EComponentMobility::Static ||
        DedicatedRenderOnlyComponent->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        DedicatedRenderOnlyComponent->GetGenerateOverlapEvents() ||
        DedicatedRenderOnlyComponent->CanEverAffectNavigation() ||
        !DedicatedRenderOnlyComponent->CastShadow ||
        !DedicatedRenderOnlyComponent->bRenderInMainPass ||
        !DedicatedRenderOnlyComponent->bRenderInDepthPass ||
        DedicatedRenderOnlyComponent->GetNumOverrideMaterials() != 0 ||
        DedicatedRenderOnlyComponent->GetOverlayMaterial() != nullptr ||
        !DedicatedRenderOnlyComponent->ComponentTags.Contains(
            DedicatedRenderComponentTag) ||
        DedicatedRenderOnlyComponent->ComponentTags.Contains(
            TEXT("TRIADHumanOnlyOverlay")) ||
        !DedicatedRenderOnlyComponent->IsVisible() ||
        DedicatedRenderOnlyComponent->bHiddenInGame ||
        !bDedicatedOverlayVisible ||
        bInsideExistingAuthoredCoreProviderClip ||
        bDedicatedProviderExclusionPolygonPresent ||
        bProviderReadyLiveSuccessorClaimed || bCoarseShellOverlapResolved ||
        bProviderOverlapResolved || !bRenderOnly ||
        bCollisionNavigationSensorOrRfAuthority ||
        bSurveyAsBuiltOneToOneOrCalibratedMaterialClaimed)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_INVALID: ") +
            MeshError +
            TEXT(" Exact volunteered-OSM fit, persistent dedicated renderer, cooked payload, or negative-authority contract changed.");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_VALID sourceFeature=OSM:way:1551538490 featureGeometrySha256=A273E09B21C71AA7436C5B156E643AF4176F3B90A215DD0FEA56928FA5BDDEF7 partGeometrySha256=EBEF89D86FF0BA7FD6DBF188ED2389CC392E2E859ACEEADF6F0900E6E880E928 placementReceiptSha256=6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5 transformCm=(40411.658,88424.311,0.000) yawDeg=-162.515228 scale=(1.053891,1.221655,1.000000) nonUniformVolunteeredOsmFit=true ignoredVolunteeredHeight40m=true syntheticThreeStoreyHeight13_2m=true oneToOne=false meshTriangles=15760 materialSlots=15 cookedRenderPayloadDigestVerified=true proceduralTextureFreePbr=true materialsCalibrated=false foliageLayoutSchema=triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1 foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor bakedFoliageRenderComponents=0 foliageTreeAnchors=3 dedicatedOverlayVisible=%s coarseLocalShellRetained=true coarseShellOverlapUnresolved=true providerOverlapUnresolved=true providerReadyObserved=%s providerStateTelemetryOnly=true visibilityInvariantAcrossProviderTransitions=true dedicatedProviderExclusion=false providerReadyLiveSuccessor=false renderOnly=true collisionNavigationSensorRfAuthority=false surveyAsBuilt=false runtimeProviderTelemetryBindingValid=%s runtimeAssetContractFailClosed=%s."),
        bDedicatedOverlayVisible ? TEXT("true") : TEXT("false"),
        bProviderReady ? TEXT("true") : TEXT("false"),
        bRuntimeProviderTelemetryBindingValid ? TEXT("true") : TEXT("false"),
        bRuntimeAssetContractFailClosed ? TEXT("true") : TEXT("false"));
    return true;
}
