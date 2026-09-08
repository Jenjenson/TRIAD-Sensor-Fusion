#include "TRIADIstanaExploreV5DMacDonaldHouseActor.h"

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
#include "TRIADIstanaExploreV5DMacDonaldHouseProvenance.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADIstanaExploreV5DMacDonaldHouse,
    Log,
    All);

namespace
{
const FName MacDonaldHouseActorTag(
    TEXT("TRIADIstanaExploreV5DMacDonaldHouseR24"));
const FName DedicatedRenderComponentTag(
    TEXT("TRIADV5DR24MacDonaldHouseDedicatedPersistentRenderOnly"));
const FString MacDonaldHouseMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/"
         "R24MacDonaldHouse/SM_IPV5D_R24_MacDonaldHouse_Render."
         "SM_IPV5D_R24_MacDonaldHouse_Render"));

struct FMaterialSpec
{
    const TCHAR* Slot;
    const TCHAR* Asset;
    uint32 Triangles;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("M_MD_BalconyConcrete"), TEXT("M_MD_BalconyConcrete_PBR_R24"), 336},
    {TEXT("M_MD_DarkMetal"), TEXT("M_MD_DarkMetal_PBR_R24"), 36},
    {TEXT("M_MD_DarkWindowGlass"), TEXT("M_MD_DarkWindowGlass_PBR_R24"), 804},
    {TEXT("M_MD_GreenGlazedRoofTile"), TEXT("M_MD_GreenGlazedRoofTile_PBR_R24"), 360},
    {TEXT("M_MD_HeritagePlaque"), TEXT("M_MD_HeritagePlaque_PBR_R24"), 108},
    {TEXT("M_MD_MarbleColumn"), TEXT("M_MD_MarbleColumn_PBR_R24"), 240},
    {TEXT("M_MD_RedSandFacedBrick"), TEXT("M_MD_RedSandFacedBrick_PBR_R24"), 240},
    {TEXT("M_MD_ShadowRecess"), TEXT("M_MD_ShadowRecess_PBR_R24"), 72},
    {TEXT("M_MD_WhitePaintedFrame"), TEXT("M_MD_WhitePaintedFrame_PBR_R24"), 4884},
};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 9);

FString MaterialObjectPath(const TCHAR* Asset)
{
    return FString::Printf(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/"
             "R24MacDonaldHouse/Materials/%s.%s"),
        Asset,
        Asset);
}

const UTRIADIstanaExploreV5DMacDonaldHouseProvenance* FindProvenance(
    const UStaticMesh* Mesh,
    int32& OutCount)
{
    OutCount = 0;
    const UTRIADIstanaExploreV5DMacDonaldHouseProvenance* Result = nullptr;
    const TArray<UAssetUserData*>* Data = Mesh
        ? Mesh->GetAssetUserDataArray()
        : nullptr;
    if (Data)
    {
        for (const UAssetUserData* Row : *Data)
        {
            if (Row && Row->IsA(
                    UTRIADIstanaExploreV5DMacDonaldHouseProvenance::
                        StaticClass()))
            {
                ++OutCount;
                Result = Cast<
                    UTRIADIstanaExploreV5DMacDonaldHouseProvenance>(Row);
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
    const UTRIADIstanaExploreV5DMacDonaldHouseProvenance* Provenance =
        FindProvenance(Mesh, ProvenanceCount);
    FString RecomputedPayloadSha256;
    FString PayloadDigestError;
    const bool bPayloadDigestValid = Provenance &&
        UTRIADIstanaExploreV5DMacDonaldHouseProvenance::
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
    if (!Mesh || Mesh->GetPathName() != MacDonaldHouseMeshObjectPath ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() != 7080 ||
        RenderData->LODResources[0].GetNumVertices() == 0 ||
        RenderData->LODResources[0].Sections.Num() != 9 ||
        Mesh->GetStaticMaterials().Num() != 9 || !Mesh->HasValidNaniteData() ||
        !bFullNaniteSettings || !Body ||
        Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision() ||
        !Mesh->bAllowCPUAccess ||
        ProvenanceCount != 1 || !Provenance ||
        Provenance->GetClass() !=
            UTRIADIstanaExploreV5DMacDonaldHouseProvenance::StaticClass() ||
        Provenance->GetOuter() != Mesh || !Provenance->IsCanonicalContract() ||
        !bPayloadDigestValid)
    {
        OutError = TEXT("R24A MacDonald House mesh lost its exact 7,080-triangle, nine-section, full-Nanite, zero-collision, no-navigation, CPU-readable cooked-provenance/render-payload contract. ") +
            PayloadDigestError;
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector Minimum = Bounds.Origin - Bounds.BoxExtent;
    const FVector Maximum = Bounds.Origin + Bounds.BoxExtent;
    if (!Minimum.Equals(FVector(-1516.0, -960.0, 0.0), 0.25) ||
        !Maximum.Equals(FVector(1516.0, 808.0, 3142.0), 0.25))
    {
        OutError = FString::Printf(
            TEXT("R24A MacDonald House imported centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
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
            OutError = TEXT("R24A MacDonald House semantic render-section census drifted.");
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
                TEXT("R24A MacDonald House material binding or bounded procedural PBR policy drifted at slot %d ('%s')."),
                Index,
                MaterialSpecs[Index].Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}
} // namespace

const FName& ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedActorTag()
{
    return MacDonaldHouseActorTag;
}

const FString& ATRIADIstanaExploreV5DMacDonaldHouseActor::
    ExpectedMeshObjectPath()
{
    return MacDonaldHouseMeshObjectPath;
}

FTransform ATRIADIstanaExploreV5DMacDonaldHouseActor::
    ExpectedPlacementTransform()
{
    return FTransform(
        FRotator(0.0, -161.885822122792, 0.0),
        FVector(36320.390052826, 87155.365772797, 0.0),
        FVector(1.4799104705961, 1.98130612769146, 1.3));
}

bool ATRIADIstanaExploreV5DMacDonaldHouseActor::ValidateRuntimeMesh(
    UStaticMesh* Mesh,
    FString& OutError)
{
    return ValidateMesh(Mesh, OutError);
}

ATRIADIstanaExploreV5DMacDonaldHouseActor::
    ATRIADIstanaExploreV5DMacDonaldHouseActor()
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
            TEXT("DedicatedMacDonaldHouseRenderOnly"));
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
    Tags.AddUnique(MacDonaldHouseActorTag);
}

bool ATRIADIstanaExploreV5DMacDonaldHouseActor::
    ConfigureMacDonaldHouse(
        UStaticMesh* Mesh,
        bool bInProviderReady,
        FString& OutReport)
{
    FString MeshError;
    if (!DedicatedRenderOnlyComponent || !ValidateMesh(Mesh, MeshError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_CONFIGURE_REFUSED: ") +
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
        !ValidateMacDonaldHouse(OutReport))
    {
        return false;
    }
    return true;
}

bool ATRIADIstanaExploreV5DMacDonaldHouseActor::
    RecordProviderReadyState(bool bInProviderReady, FString& OutError)
{
    if (!DedicatedRenderOnlyComponent)
    {
        OutError = TEXT("R24A MacDonald House provider telemetry has no render component.");
        return false;
    }
    DedicatedRenderOnlyComponent->SetVisibility(true, true);
    DedicatedRenderOnlyComponent->SetHiddenInGame(false, true);
    bProviderReady = bInProviderReady;
    bDedicatedOverlayVisible = true;
    if (!DedicatedRenderOnlyComponent->IsVisible() ||
        DedicatedRenderOnlyComponent->bHiddenInGame)
    {
        OutError = TEXT("R24A MacDonald House provider telemetry changed persistent overlay visibility.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DMacDonaldHouseActor::
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
        for (TActorIterator<ATRIADIstanaExploreV5DMacDonaldHouseActor> It(World);
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
            TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_ASSET_FAIL_CLOSED telemetry={") +
            TelemetryError + TEXT("}");
        return false;
    }
    if (bFullAuditDue)
    {
        NextRuntimeContractAuditTimeSeconds = NowSeconds + 0.5;
    }

    FString ExistingReport;
    if (bFullAuditDue && !ValidateMacDonaldHouse(ExistingReport))
    {
        DedicatedRenderOnlyComponent->SetVisibility(false, true);
        DedicatedRenderOnlyComponent->SetHiddenInGame(true, true);
        bDedicatedOverlayVisible = false;
        bRuntimeProviderTelemetryBindingValid = false;
        bRuntimeAssetContractFailClosed = true;
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_ASSET_FAIL_CLOSED policyCount=%d landmarkCount=%d providerReadyObserved=%s landmark={%s}"),
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
            TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_PERSISTENT_OVERLAY_VALID providerTelemetryBinding=false providerReadyObserved=%s policyCount=%d landmarkCount=%d overlayVisible=true visibilityInvariantAcrossProviderTransitions=true policy={%s}"),
            bReady ? TEXT("true") : TEXT("false"),
            PolicyCount,
            LandmarkCount,
            *PolicyReport);
        return true;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_PERSISTENT_OVERLAY_VALID providerTelemetryBinding=true providerReadyObserved=%s overlayVisible=true visibilityInvariantAcrossProviderTransitions=true orderedAfterContextPolicy=true fullContractAuditHz=2"),
        bReady ? TEXT("true") : TEXT("false"));
    return true;
}

void ATRIADIstanaExploreV5DMacDonaldHouseActor::BeginPlay()
{
    Super::BeginPlay();
    FString Report;
    if (!SynchronizeWithContextPolicy(Report))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DMacDonaldHouse,
            Warning,
            TEXT("%s"),
            *Report);
        bRuntimePolicyFailureWasLogged = true;
    }
}

void ATRIADIstanaExploreV5DMacDonaldHouseActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FString Report;
    if (!SynchronizeWithContextPolicy(Report))
    {
        if (!bRuntimePolicyFailureWasLogged)
        {
            UE_LOG(
                LogTRIADIstanaExploreV5DMacDonaldHouse,
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
            LogTRIADIstanaExploreV5DMacDonaldHouse,
            Display,
            TEXT("%s"),
            *Report);
        bRuntimePolicyFailureWasLogged = false;
    }
}

void ATRIADIstanaExploreV5DMacDonaldHouseActor::EndPlay(
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

bool ATRIADIstanaExploreV5DMacDonaldHouseActor::
    ValidateMacDonaldHouse(FString& OutReport) const
{
    FString MeshError;
#if WITH_EDITOR
    const bool bHiddenInEditor = IsHiddenEd();
#else
    const bool bHiddenInEditor = false;
#endif
    if (!Tags.Contains(MacDonaldHouseActorTag) ||
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
        bProviderReadyLiveSuccessorClaimed || !bRenderOnly ||
        bCollisionNavigationSensorOrRfAuthority ||
        bSurveyAsBuiltOneToOneOrCalibratedMaterialClaimed)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_INVALID: ") +
            MeshError +
            TEXT(" Exact volunteered-OSM fit, persistent dedicated renderer, cooked payload, or negative-authority contract changed.");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R24_MACDONALD_VALID sourceFeature=OSM:way:46521250 featureGeometrySha256=755536CDE8E666826FA6A325BCD707BDC7E0132C018BC87F0ABE8275F4557A39 partGeometrySha256=B7204683A26E7DA56E1A97E516E36220C1764341CA485B904D736F509B481A34 placementReceiptSha256=E5B40D02A3D712303A2D99734A42E9ADD68F4E528DED31DD763F37034961ED81 transformCm=(36320.390,87155.366,0.000) yawDeg=-161.885822 scale=(1.479910,1.981306,1.300000) nonUniformVolunteeredOsmFit=true oneToOne=false meshTriangles=7080 materialSlots=9 cookedRenderPayloadDigestVerified=true proceduralTextureFreePbr=true materialsCalibrated=false dedicatedOverlayVisible=%s coarseLocalShellRetained=true providerReadyObserved=%s providerStateTelemetryOnly=true visibilityInvariantAcrossProviderTransitions=true insideExistingProviderClip=false dedicatedProviderExclusion=false providerReadyLiveSuccessor=false renderOnly=true collisionNavigationSensorRfAuthority=false surveyAsBuilt=false runtimeProviderTelemetryBindingValid=%s runtimeAssetContractFailClosed=%s."),
        bDedicatedOverlayVisible ? TEXT("true") : TEXT("false"),
        bProviderReady ? TEXT("true") : TEXT("false"),
        bRuntimeProviderTelemetryBindingValid ? TEXT("true") : TEXT("false"),
        bRuntimeAssetContractFailClosed ? TEXT("true") : TEXT("false"));
    return true;
}
