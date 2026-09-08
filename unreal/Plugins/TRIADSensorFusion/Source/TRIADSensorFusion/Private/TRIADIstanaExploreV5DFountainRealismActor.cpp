#include "TRIADIstanaExploreV5DFountainRealismActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "TRIADIstanaExploreV5AppearanceActor.h"
#include "TRIADIstanaExploreV5BVisualActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADIstanaExploreV5DFountainRealism,
    Log,
    All);

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString SuccessorWaterMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials/M_IPV5D_FountainWater.M_IPV5D_FountainWater"));
const FString SuccessorSprayMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials/M_IPV5D_FountainSpray.M_IPV5D_FountainSpray"));
const FString EmbeddedWaterSuppressorMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials/M_IPV5D_FountainEmbeddedWaterSuppressor.M_IPV5D_FountainEmbeddedWaterSuppressor"));
const FString HardscapeRenderSuccessorMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5B/Props/Meshes/SM_IPV5B_Hardscape_NoLegacyBeds.SM_IPV5B_Hardscape_NoLegacyBeds"));
const FString FountainClaimLabel(
    TEXT("ISTANA_EXPLORE_V5D_APPEARANCE_ONLY_FOUNTAIN_LOOKDEV_NOT_SURVEY_NOT_AS_BUILT_NOT_HYDRAULIC_SIMULATION_NOT_OPERATING_STATE_NOT_COLLISION_NAVIGATION_SENSOR_OR_RF_TRUTH"));
const FName FountainActorTag(
    TEXT("TRIADIstanaExploreV5DFountainRealism"));
const FName FountainSourceReplacementTag(
    TEXT("TRIADIstanaExploreV5DFountainSuccessorOwnsRendering"));

constexpr int32 SourceOuterPlumeCount = 12;
constexpr int32 EdgeFoamCount = 3;
constexpr int32 PrimaryJetCount = 12;
constexpr int32 SecondarySprayCount = 24;
constexpr int32 ImpactRippleCount = 18;
constexpr int32 CentralSprayCount = 8;
constexpr int32 CentralRippleCount = 5;
constexpr int32 HardscapeWaterMaterialSlot = 1;
constexpr int32 EmbeddedHardscapeWaterSectionTriangles = 736;
constexpr int32 EmbeddedLegacyJetGroups = 12;
constexpr int32 EmbeddedLegacyJetTriangles = 480;
const FName HardscapeWaterMaterialSlotName(TEXT("M_IPV_Water"));
constexpr int32 StartCullDistanceCm = 0;
constexpr int32 EndCullDistanceCm = 90000;
constexpr int32 WpoDisableDistanceCm = 70000;
constexpr double TransformTolerance = 0.01;
constexpr double CentralRippleMinimumCenterRadiusCm = 210.0;
constexpr double CentralRippleMaximumCenterRadiusCm = 310.0;

int32 FindExactHardscapeWaterMaterialSlot(const UStaticMesh* HardscapeMesh)
{
    if (!HardscapeMesh ||
        HardscapeMesh->GetStaticMaterials().Num() != 3)
    {
        return INDEX_NONE;
    }

    const int32 WaterMaterialSlot =
        HardscapeMesh->GetMaterialIndex(HardscapeWaterMaterialSlotName);
    if (WaterMaterialSlot != HardscapeWaterMaterialSlot ||
        !HardscapeMesh->GetStaticMaterials().IsValidIndex(
            WaterMaterialSlot))
    {
        return INDEX_NONE;
    }

    const FStaticMaterial& WaterStaticMaterial =
        HardscapeMesh->GetStaticMaterials()[WaterMaterialSlot];
    bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
    bImportedSlotNameMatches =
        WaterStaticMaterial.ImportedMaterialSlotName ==
            HardscapeWaterMaterialSlotName;
#endif
    return WaterStaticMaterial.MaterialSlotName ==
                HardscapeWaterMaterialSlotName &&
            bImportedSlotNameMatches
        ? WaterMaterialSlot
        : INDEX_NONE;
}

bool IsExactTargetWorld(const UWorld* World)
{
    return World && World->GetOutermost() &&
        UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) ==
            TargetMapPackage;
}

bool IsTrustedUntitledBuilderWorld(
    const UWorld* World,
    bool bTrustedUntitledHybridBuilder)
{
    return World && World->GetOutermost() &&
        bTrustedUntitledHybridBuilder &&
        FPackageName::IsTempPackage(World->GetOutermost()->GetName());
}

uint32 Hash32(uint32 Value)
{
    Value ^= Value >> 16;
    Value *= 0x7FEB352Du;
    Value ^= Value >> 15;
    Value *= 0x846CA68Bu;
    Value ^= Value >> 16;
    return Value;
}

double HashUnit(uint32 Value)
{
    return static_cast<double>(Hash32(Value)) /
        static_cast<double>(MAX_uint32);
}

double HashRange(uint32 Value, double Minimum, double Maximum)
{
    return FMath::Lerp(Minimum, Maximum, HashUnit(Value));
}

bool IsFinitePositiveTransform(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    const FVector Location = Transform.GetLocation();
    return !Transform.ContainsNaN() &&
        FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) &&
        FMath::IsFinite(Location.Z) &&
        FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) &&
        FMath::IsFinite(Scale.Z) &&
        Scale.X > 0.0001 && Scale.Y > 0.0001 && Scale.Z > 0.0001;
}

bool TransformArraysEqual(
    const TArray<FTransform>& A,
    const TArray<FTransform>& B,
    double Tolerance = TransformTolerance)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (!A[Index].Equals(B[Index], Tolerance))
        {
            return false;
        }
    }
    return true;
}

bool ValidateTransformArray(
    const TArray<FTransform>& Transforms,
    int32 ExpectedCount,
    const TCHAR* Label,
    FString& OutError)
{
    if (Transforms.Num() != ExpectedCount)
    {
        OutError = FString::Printf(
            TEXT("V5D fountain %s count drifted: expected %d, found %d."),
            Label,
            ExpectedCount,
            Transforms.Num());
        return false;
    }
    for (int32 Index = 0; Index < Transforms.Num(); ++Index)
    {
        if (!IsFinitePositiveTransform(Transforms[Index]))
        {
            OutError = FString::Printf(
                TEXT("V5D fountain %s transform %d is not finite with positive scale."),
                Label,
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ReadExactInstanceTransforms(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExpectedCount,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    OutTransforms.Reset();
    if (!Component || Component->GetInstanceCount() != ExpectedCount)
    {
        OutError = FString::Printf(
            TEXT("V5D fountain component requires exactly %d serialized instances."),
            ExpectedCount);
        return false;
    }
    // HISM cluster trees are derived renderer caches and legitimately rebuild
    // after cold load. Construction already proved a synchronous tree build;
    // serialized CPU instance transforms are the cold/runtime layout truth.
    OutTransforms.Reserve(ExpectedCount);
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        FTransform Transform = FTransform::Identity;
        if (!Component->GetInstanceTransform(Index, Transform, true) ||
            !IsFinitePositiveTransform(Transform))
        {
            OutTransforms.Reset();
            OutError = FString::Printf(
                TEXT("V5D fountain could not read finite source transform %d/%d."),
                Index,
                ExpectedCount);
            return false;
        }
        OutTransforms.Add(Transform);
    }
    OutError.Reset();
    return true;
}

bool ReadExactSingleTransform(
    UHierarchicalInstancedStaticMeshComponent* Component,
    FTransform& OutTransform,
    FString& OutError)
{
    TArray<FTransform> Transforms;
    if (!ReadExactInstanceTransforms(Component, 1, Transforms, OutError))
    {
        OutTransform = FTransform::Identity;
        return false;
    }
    OutTransform = Transforms[0];
    return true;
}

void ConfigureRenderOnlyHism(
    UHierarchicalInstancedStaticMeshComponent* Component)
{
    check(Component);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false);
    Component->SetRenderInMainPass(true);
    Component->SetRenderInDepthPass(true);
    Component->SetCastShadow(false);
    Component->bCastContactShadow = false;
    Component->bEnableDensityScaling = false;
    Component->CurrentDensityScaling = 1.0f;
#if WITH_EDITOR
    Component->bCanEnableDensityScaling = false;
#endif
    Component->SetCullDistances(StartCullDistanceCm, EndCullDistanceCm);
    Component->SetWorldPositionOffsetDisableDistance(WpoDisableDistanceCm);
    Component->ForcedLodModel = 0;
    Component->bOverrideMinLOD = true;
    Component->MinLOD = 0;
}

void AssignMeshAndMaterial(
    UHierarchicalInstancedStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    UMaterialInterface* Material)
{
    check(Component);
    Component->SetStaticMesh(Mesh);
    Component->EmptyOverrideMaterials();
    Component->SetMaterial(0, Material);
}

bool ValidateRenderOnlyHism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const USceneComponent* ExpectedParent,
    UStaticMesh* ExpectedMesh,
    UMaterialInterface* ExpectedMaterial,
    const TArray<FTransform>& ExpectedTransforms,
    bool bExpectedVisible,
    const TCHAR* Label,
    FString& OutError)
{
    int32 StartCull = 0;
    int32 EndCull = 0;
    if (Component)
    {
        Component->GetCullDistances(StartCull, EndCull);
    }
    const bool bVisibilityMatches = Component &&
        (bExpectedVisible
            ? Component->IsVisible() && !Component->bHiddenInGame
            : !Component->IsVisible() && Component->bHiddenInGame);
    const bool bRenderPassesMatch = Component &&
        (bExpectedVisible
            ? Component->bRenderInMainPass && Component->bRenderInDepthPass
            : !Component->bRenderInMainPass &&
                !Component->bRenderInDepthPass);
    TArray<FTransform> ActualTransforms;
    if (!Component || Component->GetAttachParent() != ExpectedParent ||
        !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetCollisionResponseToChannels() !=
            FCollisionResponseContainer(ECR_Ignore) ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        !bVisibilityMatches || !bRenderPassesMatch ||
        Component->CastShadow || Component->bCastContactShadow ||
        Component->bEnableDensityScaling ||
        StartCull != StartCullDistanceCm || EndCull != EndCullDistanceCm ||
        Component->WorldPositionOffsetDisableDistance !=
            WpoDisableDistanceCm ||
        Component->ForcedLodModel != 0 || !Component->bOverrideMinLOD ||
        Component->MinLOD != 0 ||
        Component->GetStaticMesh() != ExpectedMesh ||
        Component->GetMaterial(0) != ExpectedMaterial ||
        !ReadExactInstanceTransforms(
            Component,
            ExpectedTransforms.Num(),
            ActualTransforms,
            OutError) ||
        !TransformArraysEqual(ActualTransforms, ExpectedTransforms))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("V5D fountain successor component '%s' drifted from its exact render-only asset/layout contract."),
                Label);
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool SourceComponentIsHiddenRenderOnly(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const AActor* ExpectedOwner)
{
    return Component && Component->GetOwner() == ExpectedOwner &&
        !Component->IsVisible() && Component->bHiddenInGame &&
        !Component->bRenderInMainPass &&
        !Component->bRenderInDepthPass &&
        Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore) &&
        !Component->GetGenerateOverlapEvents() &&
        !Component->CanEverAffectNavigation();
}

void SetSourceRendererReplacementState(
    const TArray<UHierarchicalInstancedStaticMeshComponent*>& Components,
    bool bSuccessorOwnsRendering)
{
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (Component)
        {
            Component->SetVisibility(!bSuccessorOwnsRendering, true);
            Component->SetHiddenInGame(bSuccessorOwnsRendering);
            Component->SetRenderInMainPass(!bSuccessorOwnsRendering);
            Component->SetRenderInDepthPass(!bSuccessorOwnsRendering);
        }
    }
}

void ClearSuccessorComponents(
    const TArray<UHierarchicalInstancedStaticMeshComponent*>& Components)
{
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (Component)
        {
            Component->ClearInstances();
            Component->SetStaticMesh(nullptr);
            Component->EmptyOverrideMaterials();
        }
    }
}
} // namespace

ATRIADIstanaExploreV5DFountainRealismActor::
    ATRIADIstanaExploreV5DFountainRealismActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    ClaimLabel = ExpectedClaimLabel();
    Tags.AddUnique(ExpectedActorTag());

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("V5DFountainRealismRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    const auto CreateHism = [this](const TCHAR* Name)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                Name);
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureRenderOnlyHism(Component);
        return Component;
    };
    WaterSurfaceComponent = CreateHism(TEXT("V5DFountainWaterSurface"));
    EdgeFoamComponent = CreateHism(TEXT("V5DFountainLayeredEdgeFoam"));
    PrimaryJetInstances = CreateHism(TEXT("V5DFountainPrimaryJets"));
    SecondarySprayInstances = CreateHism(
        TEXT("V5DFountainSecondarySprayFragments"));
    ImpactRippleInstances = CreateHism(
        TEXT("V5DFountainAsymmetricImpactRipples"));
    CentralPlumeComponent = CreateHism(
        TEXT("V5DFountainCentralPlume"));
    CentralSprayInstances = CreateHism(
        TEXT("V5DFountainCentralSprayFragments"));
    CentralRippleInstances = CreateHism(
        TEXT("V5DFountainCentralDisturbanceRipples"));
}

const FString&
ATRIADIstanaExploreV5DFountainRealismActor::ExpectedClaimLabel()
{
    return FountainClaimLabel;
}

const FString&
ATRIADIstanaExploreV5DFountainRealismActor::ExpectedTargetMapPackage()
{
    return TargetMapPackage;
}

const FString&
ATRIADIstanaExploreV5DFountainRealismActor::ExpectedWaterMaterialPath()
{
    return SuccessorWaterMaterialPath;
}

const FString&
ATRIADIstanaExploreV5DFountainRealismActor::ExpectedSprayMaterialPath()
{
    return SuccessorSprayMaterialPath;
}

const FString& ATRIADIstanaExploreV5DFountainRealismActor::
    ExpectedEmbeddedWaterSuppressorMaterialPath()
{
    return EmbeddedWaterSuppressorMaterialPath;
}

const FName& ATRIADIstanaExploreV5DFountainRealismActor::ExpectedActorTag()
{
    return FountainActorTag;
}

const FName&
ATRIADIstanaExploreV5DFountainRealismActor::SourceRendererReplacementTag()
{
    return FountainSourceReplacementTag;
}

bool ATRIADIstanaExploreV5DFountainRealismActor::
    ValidateActiveHardscapeWaterPresentationForSourceActor(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport)
{
    if (!Candidate || !Candidate->GetWorld() ||
        !Candidate->Tags.Contains(SourceRendererReplacementTag()))
    {
        OutReport = TEXT("V5D active fountain hardscape-water presentation source, world, or exact tag is absent.");
        return false;
    }

    const ATRIADIstanaExploreV5DFountainRealismActor* Match = nullptr;
    int32 MatchCount = 0;
    for (TActorIterator<ATRIADIstanaExploreV5DFountainRealismActor> It(
             Candidate->GetWorld());
         It;
         ++It)
    {
        if (IsValid(*It) && It->V5BVisualActor == Candidate)
        {
            ++MatchCount;
            Match = *It;
        }
    }
    if (MatchCount != 1 || !Match)
    {
        OutReport = FString::Printf(
            TEXT("V5D expected exactly one active fountain owner for the tagged V5B hardscape-water presentation; found %d."),
            MatchCount);
        return false;
    }

    FString FountainReport;
    if (!Match->ValidateFountainRealism(FountainReport))
    {
        OutReport = TEXT("V5D tagged fountain hardscape-water owner failed its complete source/material/visibility/simulation-isolation gate: ") +
            FountainReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_HARDSCAPE_WATER_PRESENTATION_VALID exactSoleOwner=true exactSourceActor=true exactMeshAndThreeSlotRoster=true stoneMetalPreserved=true waterSlotSuppressorExact=true completeR7FountainContract=true");
    return true;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::
    ExpectedSourceOuterPlumeCount()
{
    return SourceOuterPlumeCount;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::ExpectedEdgeFoamCount()
{
    return EdgeFoamCount;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::ExpectedPrimaryJetCount()
{
    return PrimaryJetCount;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::
    ExpectedSecondarySprayCount()
{
    return SecondarySprayCount;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::
    ExpectedImpactRippleCount()
{
    return ImpactRippleCount;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::
    ExpectedCentralSprayCount()
{
    return CentralSprayCount;
}

int32 ATRIADIstanaExploreV5DFountainRealismActor::
    ExpectedCentralRippleCount()
{
    return CentralRippleCount;
}

bool ATRIADIstanaExploreV5DFountainRealismActor::
    BuildDeterministicSuccessorLayout(
        const FTransform& SourceWaterSurfaceWorldTransform,
        const FTransform& SourceEdgeFoamWorldTransform,
        const TArray<FTransform>& SourceOuterPlumeWorldTransforms,
        const TArray<FTransform>& SourceImpactRingWorldTransforms,
        const FTransform& SourceCentralPlumeWorldTransform,
        FTRIADIstanaExploreV5DFountainSuccessorLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DFountainSuccessorLayout();
    if (!IsFinitePositiveTransform(SourceWaterSurfaceWorldTransform) ||
        !IsFinitePositiveTransform(SourceEdgeFoamWorldTransform) ||
        !IsFinitePositiveTransform(SourceCentralPlumeWorldTransform) ||
        !ValidateTransformArray(
            SourceOuterPlumeWorldTransforms,
            SourceOuterPlumeCount,
            TEXT("source outer-plume"),
            OutError) ||
        !ValidateTransformArray(
            SourceImpactRingWorldTransforms,
            SourceOuterPlumeCount,
            TEXT("source impact-ring"),
            OutError))
    {
        return false;
    }

    const FVector Center = SourceWaterSurfaceWorldTransform.GetLocation();
    if (!FVector2D(
            SourceEdgeFoamWorldTransform.GetLocation().X - Center.X,
            SourceEdgeFoamWorldTransform.GetLocation().Y - Center.Y)
            .IsNearlyZero(1.0) ||
        FVector::Dist2D(
            SourceCentralPlumeWorldTransform.GetLocation(),
            Center) > 1.0)
    {
        OutError = TEXT("V5D fountain source surface, edge foam, and central plume no longer share the inherited basin center.");
        return false;
    }

    OutLayout.WaterSurfaceWorldTransform =
        SourceWaterSurfaceWorldTransform;
    OutLayout.EdgeFoamWorldTransforms.Reserve(EdgeFoamCount);
    OutLayout.EdgeFoamWorldTransforms.Add(SourceEdgeFoamWorldTransform);
    for (int32 Layer = 1; Layer < EdgeFoamCount; ++Layer)
    {
        const double Sign = Layer == 1 ? 1.0 : -1.0;
        FVector Location = SourceEdgeFoamWorldTransform.GetLocation();
        Location.X += Sign * 0.7;
        Location.Y -= Sign * 0.5;
        Location.Z += static_cast<double>(Layer) * 0.28;
        OutLayout.EdgeFoamWorldTransforms.Add(FTransform(
            FRotator(0.0, Layer == 1 ? 23.0 : 101.0, 0.0),
            Location,
            FVector(
                Layer == 1 ? 1.006 : 0.995,
                Layer == 1 ? 0.994 : 1.007,
                Layer == 1 ? 0.76 : 0.58)));
    }

    OutLayout.PrimaryJetWorldTransforms.Reserve(PrimaryJetCount);
    OutLayout.SecondarySprayWorldTransforms.Reserve(
        SecondarySprayCount);
    OutLayout.ImpactRippleWorldTransforms.Reserve(ImpactRippleCount);
    for (int32 Index = 0; Index < SourceOuterPlumeCount; ++Index)
    {
        const FTransform& SourceJet =
            SourceOuterPlumeWorldTransforms[Index];
        const FTransform& SourceRipple =
            SourceImpactRingWorldTransforms[Index];
        const FVector SourceLocation = SourceJet.GetLocation();
        FVector Radial(
            SourceLocation.X - Center.X,
            SourceLocation.Y - Center.Y,
            0.0);
        Radial.Normalize();
        if (Radial.IsNearlyZero())
        {
            OutLayout = FTRIADIstanaExploreV5DFountainSuccessorLayout();
            OutError = FString::Printf(
                TEXT("V5D fountain source jet %d collapsed onto the basin center."),
                Index);
            return false;
        }
        const FVector Tangent(-Radial.Y, Radial.X, 0.0);
        const uint32 Seed =
            (static_cast<uint32>(Index) + 1u) * 0x9E3779B9u;

        FVector PrimaryLocation = SourceLocation;
        PrimaryLocation += Radial *
            HashRange(Seed ^ 0x43A7B19Du, -13.0, 11.0);
        PrimaryLocation += Tangent *
            HashRange(Seed ^ 0xA71D04C3u, -18.0, 18.0);
        PrimaryLocation.Z +=
            HashRange(Seed ^ 0xD38E27F1u, -4.0, 7.0);
        const FRotator SourceRotation = SourceJet.Rotator();
        const FRotator PrimaryRotation(
            SourceRotation.Pitch + 8.0 +
                HashRange(Seed ^ 0x852BCA6Fu, -4.0, 5.0),
            SourceRotation.Yaw +
                HashRange(Seed ^ 0x1C739A45u, -4.5, 4.5),
            SourceRotation.Roll +
                HashRange(Seed ^ 0xF46D28B7u, -5.0, 5.0));
        const FVector PrimaryScale(
            HashRange(Seed ^ 0x6D21F38Bu, 0.16, 0.26),
            HashRange(Seed ^ 0xB8A4D217u, 0.14, 0.24),
            HashRange(Seed ^ 0x2F9C561Du, 0.70, 0.92));
        const FTransform PrimaryTransform(
            PrimaryRotation,
            PrimaryLocation,
            PrimaryScale);
        OutLayout.PrimaryJetWorldTransforms.Add(PrimaryTransform);

        for (int32 Fragment = 0; Fragment < 2; ++Fragment)
        {
            const uint32 FragmentSeed = Seed ^
                (Fragment == 0 ? 0xC1473A95u : 0x57D6E20Bu);
            FVector FragmentLocation = PrimaryLocation;
            FragmentLocation += Radial *
                HashRange(FragmentSeed ^ 0x8CB2157Fu, -22.0, 27.0);
            FragmentLocation += Tangent *
                HashRange(FragmentSeed ^ 0x3E94A6C1u, -34.0, 34.0);
            FragmentLocation.Z += Fragment == 0
                ? HashRange(FragmentSeed ^ 0xD0F74B23u, 38.0, 82.0)
                : HashRange(FragmentSeed ^ 0xA5C91D6Fu, 92.0, 158.0);
            OutLayout.SecondarySprayWorldTransforms.Add(FTransform(
                FRotator(
                    PrimaryRotation.Pitch + HashRange(
                        FragmentSeed ^ 0x76A4E1D9u, -45.0, 45.0),
                    PrimaryRotation.Yaw + HashRange(
                        FragmentSeed ^ 0x2097F38Du, -24.0, 24.0),
                    PrimaryRotation.Roll + HashRange(
                        FragmentSeed ^ 0xE61B4A73u, -45.0, 45.0)),
                FragmentLocation,
                FVector(
                    HashRange(FragmentSeed ^ 0x91C5D42Fu, 0.16, 0.28),
                    HashRange(FragmentSeed ^ 0x4A7385E1u, 0.14, 0.26),
                    HashRange(FragmentSeed ^ 0xBD2F1907u, 0.055, 0.11))));
        }

        FVector RippleLocation = SourceRipple.GetLocation();
        RippleLocation += Radial *
            HashRange(Seed ^ 0x9A63D1F5u, -13.0, 18.0);
        RippleLocation += Tangent *
            HashRange(Seed ^ 0x4E2B87C9u, -19.0, 19.0);
        RippleLocation.Z +=
            HashRange(Seed ^ 0xF1936ABDu, -0.8, 1.5);
        double RippleX = HashRange(Seed ^ 0x28B4C76Fu, 0.72, 1.11);
        double RippleY = HashRange(Seed ^ 0xD61E395Bu, 0.50, 0.91);
        if (FMath::Abs(RippleX - RippleY) < 0.08)
        {
            RippleY = RippleY >= RippleX
                ? FMath::Min(1.02, RippleY + 0.11)
                : FMath::Max(0.42, RippleY - 0.11);
        }
        OutLayout.ImpactRippleWorldTransforms.Add(FTransform(
            FRotator(
                HashRange(Seed ^ 0x51D8A2E7u, -1.2, 1.2),
                HashRange(Seed ^ 0xB3479C15u, 0.0, 360.0),
                HashRange(Seed ^ 0x6C25F8D3u, -1.2, 1.2)),
            RippleLocation,
            FVector(RippleX, RippleY, 1.0)));

        if ((Index & 1) == 0)
        {
            FVector SecondaryRippleLocation = RippleLocation;
            SecondaryRippleLocation += Radial *
                HashRange(Seed ^ 0x7E14B3C9u, -24.0, 31.0);
            SecondaryRippleLocation += Tangent *
                HashRange(Seed ^ 0xC3915A27u, -27.0, 27.0);
            SecondaryRippleLocation.Z +=
                HashRange(Seed ^ 0x14D8E65Bu, 0.4, 1.8);
            const double SecondaryX =
                HashRange(Seed ^ 0xA27C49F1u, 0.39, 0.66);
            double SecondaryY =
                HashRange(Seed ^ 0x5B83D61Fu, 0.27, 0.53);
            if (FMath::Abs(SecondaryX - SecondaryY) < 0.08)
            {
                SecondaryY = SecondaryY >= SecondaryX
                    ? FMath::Min(0.64, SecondaryY + 0.11)
                    : FMath::Max(0.20, SecondaryY - 0.11);
            }
            OutLayout.ImpactRippleWorldTransforms.Add(FTransform(
                FRotator(
                    0.0,
                    HashRange(Seed ^ 0xE4A17B35u, 0.0, 360.0),
                    0.0),
                SecondaryRippleLocation,
                FVector(SecondaryX, SecondaryY, 0.82)));
        }
    }

    FVector CentralLocation = SourceCentralPlumeWorldTransform.GetLocation();
    CentralLocation.X += 3.0;
    CentralLocation.Y -= 2.0;
    OutLayout.CentralPlumeWorldTransform = FTransform(
        FRotator(1.7, 11.0, -1.2),
        CentralLocation,
        FVector(0.34, 0.38, 1.55));

    OutLayout.CentralSprayWorldTransforms.Reserve(CentralSprayCount);
    for (int32 Index = 0; Index < CentralSprayCount; ++Index)
    {
        const uint32 Seed =
            (static_cast<uint32>(Index) + 31u) * 0x85EBCA6Bu;
        const double Angle =
            (2.0 * PI * static_cast<double>(Index) / CentralSprayCount) +
            HashRange(Seed ^ 0x34A7D19Fu, -0.18, 0.18);
        const double Radius = HashRange(Seed ^ 0xB51E73C9u, 35.0, 125.0);
        const FVector Location(
            Center.X + Radius * FMath::Cos(Angle),
            Center.Y + Radius * FMath::Sin(Angle),
            CentralLocation.Z +
                HashRange(Seed ^ 0x6D2C98A5u, 45.0, 260.0));
        OutLayout.CentralSprayWorldTransforms.Add(FTransform(
            FRotator(
                HashRange(Seed ^ 0x97A4C21Du, -50.0, 50.0),
                FMath::RadiansToDegrees(Angle) +
                    HashRange(Seed ^ 0x42D8F76Bu, -30.0, 30.0),
                HashRange(Seed ^ 0xE15B39C7u, -50.0, 50.0)),
            Location,
            FVector(
                HashRange(Seed ^ 0x2C91A7E5u, 0.20, 0.34),
                HashRange(Seed ^ 0xF73D4B19u, 0.18, 0.31),
                HashRange(Seed ^ 0x58A6E2D3u, 0.07, 0.16))));
    }

    OutLayout.CentralRippleWorldTransforms.Reserve(CentralRippleCount);
    const double RippleZ =
        SourceImpactRingWorldTransforms[0].GetLocation().Z;
    for (int32 Index = 0; Index < CentralRippleCount; ++Index)
    {
        const uint32 Seed =
            (static_cast<uint32>(Index) + 71u) * 0xC2B2AE35u;
        const double XScale = 0.39 + 0.16 * static_cast<double>(Index) +
            HashRange(Seed ^ 0x4B17D39Fu, -0.025, 0.025);
        const double YScale = 0.31 + 0.14 * static_cast<double>(Index) +
            HashRange(Seed ^ 0xA9C65E21u, -0.025, 0.025);
        const double RippleAngleDegrees =
            72.0 * static_cast<double>(Index) +
            HashRange(Seed ^ 0xD37A85C9u, -6.0, 6.0);
        const double RippleAngleRadians =
            FMath::DegreesToRadians(RippleAngleDegrees);
        const double RippleRadius =
            220.0 + 20.0 * static_cast<double>(Index) +
            HashRange(Seed ^ 0x61E4B2F7u, -6.0, 6.0);
        OutLayout.CentralRippleWorldTransforms.Add(FTransform(
            FRotator(
                0.0,
                RippleAngleDegrees +
                    HashRange(Seed ^ 0x9C27D451u, -12.0, 12.0),
                0.0),
            FVector(
                Center.X + RippleRadius * FMath::Cos(RippleAngleRadians),
                Center.Y + RippleRadius * FMath::Sin(RippleAngleRadians),
                RippleZ + 0.35 * static_cast<double>(Index)),
            FVector(XScale, YScale, 0.78 + 0.035 * Index)));
    }

    if (!IsFinitePositiveTransform(OutLayout.WaterSurfaceWorldTransform) ||
        !IsFinitePositiveTransform(OutLayout.CentralPlumeWorldTransform) ||
        !ValidateTransformArray(
            OutLayout.EdgeFoamWorldTransforms,
            EdgeFoamCount,
            TEXT("edge-foam successor"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.PrimaryJetWorldTransforms,
            PrimaryJetCount,
            TEXT("primary-jet successor"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.SecondarySprayWorldTransforms,
            SecondarySprayCount,
            TEXT("secondary-spray successor"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.ImpactRippleWorldTransforms,
            ImpactRippleCount,
            TEXT("impact-ripple successor"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.CentralSprayWorldTransforms,
            CentralSprayCount,
            TEXT("central-spray successor"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.CentralRippleWorldTransforms,
            CentralRippleCount,
            TEXT("central-ripple successor"),
            OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DFountainSuccessorLayout();
        return false;
    }
    for (const FTransform& Ripple : OutLayout.ImpactRippleWorldTransforms)
    {
        const FVector Scale = Ripple.GetScale3D();
        if (FMath::Abs(Scale.X - Scale.Y) < 0.04)
        {
            OutLayout = FTRIADIstanaExploreV5DFountainSuccessorLayout();
            OutError = TEXT("V5D fountain generated a visually perfect circular impact ripple instead of bounded anisotropy.");
            return false;
        }
    }
    for (const FTransform& Ripple : OutLayout.CentralRippleWorldTransforms)
    {
        const FVector Scale = Ripple.GetScale3D();
        const FVector Offset = Ripple.GetLocation() - Center;
        const double CenterRadiusCm = FVector2D(Offset.X, Offset.Y).Size();
        if (FMath::Abs(Scale.X - Scale.Y) < 0.04)
        {
            OutLayout = FTRIADIstanaExploreV5DFountainSuccessorLayout();
            OutError = TEXT("V5D fountain generated a visually perfect circular central ripple instead of bounded anisotropy.");
            return false;
        }
        if (CenterRadiusCm < CentralRippleMinimumCenterRadiusCm ||
            CenterRadiusCm > CentralRippleMaximumCenterRadiusCm)
        {
            OutLayout = FTRIADIstanaExploreV5DFountainSuccessorLayout();
            OutError = TEXT("V5D fountain generated a central ripple outside the visible inherited water annulus.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DFountainRealismActor::ConfigureFountainRealism(
    ATRIADIstanaExploreV5AppearanceActor* InV5AppearanceActor,
    ATRIADIstanaExploreV5BVisualActor* InV5BVisualActor,
    UMaterialInterface* InSuccessorWaterMaterial,
    UMaterialInterface* InSuccessorSprayMaterial,
    UMaterialInterface* InEmbeddedWaterSuppressorMaterial,
    bool bTrustedUntitledHybridBuilder,
    FString& OutError)
{
    UWorld* World = GetWorld();
    if ((!IsExactTargetWorld(World) &&
         !IsTrustedUntitledBuilderWorld(
             World,
             bTrustedUntitledHybridBuilder)) ||
        !InV5AppearanceActor || !InV5BVisualActor ||
        !InSuccessorWaterMaterial || !InSuccessorSprayMaterial ||
        !InEmbeddedWaterSuppressorMaterial ||
        InV5AppearanceActor->GetWorld() != World ||
        InV5BVisualActor->GetWorld() != World ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutError = TEXT("V5D fountain realism admits only the exact V5D map or trusted untitled hybrid-builder world with exact identity V5/V5B sources.");
        return false;
    }
    if (bConfigured || V5AppearanceActor || V5BVisualActor)
    {
        FString ExistingReport;
        if (ValidateFountainRealism(ExistingReport))
        {
            OutError.Reset();
            return true;
        }
        OutError = TEXT("V5D fountain realism refuses to overwrite an already-configured invalid actor: ") +
            ExistingReport;
        return false;
    }

    FString V5Report;
    if (!InV5AppearanceActor->ValidateExploreV5Appearance(V5Report, false) ||
        !InV5AppearanceActor->FountainWaterMaterial ||
        InV5AppearanceActor->FountainWaterMaterial->GetPathName() !=
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedFountainWaterMaterialPath())
    {
        OutError = TEXT("V5D fountain realism requires the exact inherited validated V5 source state. ") +
            V5Report;
        return false;
    }
    UMaterial* InheritedWaterBase =
        InV5AppearanceActor->FountainWaterMaterial->GetMaterial();
    UMaterial* SuccessorWaterBase = InSuccessorWaterMaterial->GetMaterial();
    UMaterial* SuccessorSprayBase = InSuccessorSprayMaterial->GetMaterial();
    UMaterial* EmbeddedWaterSuppressorBase =
        InEmbeddedWaterSuppressorMaterial->GetMaterial();
    if (!InheritedWaterBase ||
        InheritedWaterBase->MaterialDomain != MD_Surface ||
        InheritedWaterBase->GetBlendMode() != BLEND_Translucent ||
        InSuccessorWaterMaterial->GetPathName() !=
            ExpectedWaterMaterialPath() ||
        InSuccessorSprayMaterial->GetPathName() !=
            ExpectedSprayMaterialPath() ||
        InEmbeddedWaterSuppressorMaterial->GetPathName() !=
            ExpectedEmbeddedWaterSuppressorMaterialPath() ||
        SuccessorWaterBase != InSuccessorWaterMaterial ||
        SuccessorSprayBase != InSuccessorSprayMaterial ||
        EmbeddedWaterSuppressorBase !=
            InEmbeddedWaterSuppressorMaterial ||
        SuccessorWaterBase->MaterialDomain != MD_Surface ||
        SuccessorWaterBase->GetBlendMode() != BLEND_Opaque ||
        SuccessorWaterBase->TwoSided ||
        SuccessorWaterBase->bTangentSpaceNormal ||
        SuccessorWaterBase->bScreenSpaceReflections ||
        SuccessorSprayBase->MaterialDomain != MD_Surface ||
        SuccessorSprayBase->GetBlendMode() != BLEND_Additive ||
        SuccessorSprayBase->TwoSided ||
        SuccessorSprayBase->bTangentSpaceNormal ||
        SuccessorSprayBase->TranslucencyLightingMode !=
            TLM_VolumetricNonDirectional ||
        SuccessorSprayBase->RefractionMethod != RM_PixelNormalOffset ||
        SuccessorSprayBase->bScreenSpaceReflections ||
        EmbeddedWaterSuppressorBase->MaterialDomain != MD_Surface ||
        EmbeddedWaterSuppressorBase->GetBlendMode() != BLEND_Additive ||
        EmbeddedWaterSuppressorBase->TwoSided ||
        EmbeddedWaterSuppressorBase->bTangentSpaceNormal ||
        EmbeddedWaterSuppressorBase->bScreenSpaceReflections ||
        !EmbeddedWaterSuppressorBase->GetShadingModels()
             .HasOnlyShadingModel(MSM_Unlit))
    {
        OutError = TEXT("V5D fountain successor requires the exact V5D-owned opaque calm water, sparse spray, and zero-output embedded-water suppressor material trio while preserving inherited material assets.");
        return false;
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*> SourceComponents = {
        InV5BVisualActor->FountainSurfaceComponent,
        InV5BVisualActor->FountainEdgeFoamComponent,
        InV5BVisualActor->OuterPlumeInstances,
        InV5BVisualActor->ImpactRingInstances,
        InV5BVisualActor->CentralPlumeComponent};
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         SourceComponents)
    {
        if (!Component || Component->GetOwner() != InV5BVisualActor ||
            !Component->IsVisible() || Component->bHiddenInGame ||
            !Component->bRenderInMainPass ||
            !Component->bRenderInDepthPass ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->GetCollisionResponseToChannels() !=
                FCollisionResponseContainer(ECR_Ignore) ||
            Component->GetGenerateOverlapEvents() ||
            Component->CanEverAffectNavigation())
        {
            OutError = TEXT("V5D fountain source renderers must begin as the exact visible V5B render-only/no-collision roster.");
            return false;
        }
    }
    if (InV5BVisualActor->Tags.Contains(SourceRendererReplacementTag()))
    {
        OutError = TEXT("V5D fountain source is already claimed by another successor owner.");
        return false;
    }

    UStaticMeshComponent* HardscapeSuccessor =
        InV5BVisualActor->HardscapeRenderSuccessorComponent;
    UStaticMesh* HardscapeMesh = HardscapeSuccessor
        ? HardscapeSuccessor->GetStaticMesh()
        : nullptr;
    const TArray<TObjectPtr<UMaterialInterface>>& HardscapeSourceMaterials =
        InV5BVisualActor->SavedAssetRoster.
            HardscapeRenderSuccessorMaterials;
    const int32 WaterMaterialSlot =
        FindExactHardscapeWaterMaterialSlot(HardscapeMesh);
    if (!HardscapeSuccessor || HardscapeSuccessor->GetOwner() !=
            InV5BVisualActor ||
        !HardscapeMesh || HardscapeMesh->GetPathName() !=
            HardscapeRenderSuccessorMeshPath ||
        WaterMaterialSlot != HardscapeWaterMaterialSlot ||
        HardscapeMesh->GetStaticMaterials().Num() != 3 ||
        HardscapeSourceMaterials.Num() != 3 ||
        HardscapeSuccessor->OverrideMaterials.Num() != 3 ||
        !HardscapeSuccessor->IsVisible() ||
        HardscapeSuccessor->bHiddenInGame ||
        !HardscapeSuccessor->bRenderInMainPass ||
        !HardscapeSuccessor->bRenderInDepthPass ||
        HardscapeSuccessor->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        HardscapeSuccessor->GetGenerateOverlapEvents() ||
        HardscapeSuccessor->CanEverAffectNavigation())
    {
        OutError = TEXT("V5D fountain successor requires the exact visible three-slot V5B hardscape render successor before suppressing its embedded M_IPV_Water section.");
        return false;
    }
    for (int32 MaterialIndex = 0; MaterialIndex < 3; ++MaterialIndex)
    {
        if (!HardscapeSourceMaterials[MaterialIndex] ||
            HardscapeSuccessor->GetMaterial(MaterialIndex) !=
                HardscapeSourceMaterials[MaterialIndex].Get())
        {
            OutError = FString::Printf(
                TEXT("V5D fountain hardscape source material slot %d drifted before the exact water-section override."),
                MaterialIndex);
            return false;
        }
    }
    const TArray<TObjectPtr<UMaterialInterface>>
        OriginalHardscapeOverrides = HardscapeSuccessor->OverrideMaterials;

    FTransform SourceSurface = FTransform::Identity;
    FTransform SourceFoam = FTransform::Identity;
    FTransform SourceCentral = FTransform::Identity;
    TArray<FTransform> SourceOuter;
    TArray<FTransform> SourceImpact;
    if (!ReadExactSingleTransform(
            InV5BVisualActor->FountainSurfaceComponent,
            SourceSurface,
            OutError) ||
        !ReadExactSingleTransform(
            InV5BVisualActor->FountainEdgeFoamComponent,
            SourceFoam,
            OutError) ||
        !ReadExactInstanceTransforms(
            InV5BVisualActor->OuterPlumeInstances,
            SourceOuterPlumeCount,
            SourceOuter,
            OutError) ||
        !ReadExactInstanceTransforms(
            InV5BVisualActor->ImpactRingInstances,
            SourceOuterPlumeCount,
            SourceImpact,
            OutError) ||
        !ReadExactSingleTransform(
            InV5BVisualActor->CentralPlumeComponent,
            SourceCentral,
            OutError))
    {
        return false;
    }

    FTRIADIstanaExploreV5DFountainSuccessorLayout Layout;
    if (!BuildDeterministicSuccessorLayout(
            SourceSurface,
            SourceFoam,
            SourceOuter,
            SourceImpact,
            SourceCentral,
            Layout,
            OutError))
    {
        return false;
    }

    UStaticMesh* SourceSurfaceMesh =
        InV5BVisualActor->FountainSurfaceComponent->GetStaticMesh();
    UStaticMesh* SourceFoamMesh =
        InV5BVisualActor->FountainEdgeFoamComponent->GetStaticMesh();
    UStaticMesh* SourceOuterMesh =
        InV5BVisualActor->OuterPlumeInstances->GetStaticMesh();
    UStaticMesh* SourceImpactMesh =
        InV5BVisualActor->ImpactRingInstances->GetStaticMesh();
    UStaticMesh* SourceCentralMesh =
        InV5BVisualActor->CentralPlumeComponent->GetStaticMesh();
    UMaterialInterface* SourceSurfaceMaterial =
        InV5BVisualActor->FountainSurfaceComponent->GetMaterial(0);
    UMaterialInterface* SourceFoamMaterial =
        InV5BVisualActor->FountainEdgeFoamComponent->GetMaterial(0);
    UMaterialInterface* SourceOuterMaterial =
        InV5BVisualActor->OuterPlumeInstances->GetMaterial(0);
    if (!SourceSurfaceMesh || !SourceFoamMesh || !SourceOuterMesh ||
        !SourceImpactMesh || !SourceCentralMesh || !SourceSurfaceMaterial ||
        !SourceFoamMaterial || !SourceOuterMaterial ||
        InV5BVisualActor->ImpactRingInstances->GetMaterial(0) !=
            SourceFoamMaterial ||
        InV5BVisualActor->CentralPlumeComponent->GetMaterial(0) !=
            SourceOuterMaterial)
    {
        OutError = TEXT("V5D fountain inherited mesh/material roster is incomplete or no longer shares the exact foam/spray bindings.");
        return false;
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*>
        SuccessorComponents = {
            WaterSurfaceComponent,
            EdgeFoamComponent,
            PrimaryJetInstances,
            SecondarySprayInstances,
            ImpactRippleInstances,
            CentralPlumeComponent,
            CentralSprayInstances,
            CentralRippleInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         SuccessorComponents)
    {
        Component->bAutoRebuildTreeOnInstanceChanges = false;
        Component->ClearInstances();
    }
    AssignMeshAndMaterial(
        WaterSurfaceComponent,
        SourceSurfaceMesh,
        InSuccessorWaterMaterial);
    AssignMeshAndMaterial(
        EdgeFoamComponent,
        SourceFoamMesh,
        InSuccessorSprayMaterial);
    AssignMeshAndMaterial(
        PrimaryJetInstances,
        SourceOuterMesh,
        InSuccessorSprayMaterial);
    AssignMeshAndMaterial(
        SecondarySprayInstances,
        SourceOuterMesh,
        InSuccessorSprayMaterial);
    AssignMeshAndMaterial(
        ImpactRippleInstances,
        SourceImpactMesh,
        InSuccessorSprayMaterial);
    AssignMeshAndMaterial(
        CentralPlumeComponent,
        SourceOuterMesh,
        InSuccessorSprayMaterial);
    AssignMeshAndMaterial(
        CentralSprayInstances,
        SourceOuterMesh,
        InSuccessorSprayMaterial);
    AssignMeshAndMaterial(
        CentralRippleInstances,
        SourceImpactMesh,
        InSuccessorSprayMaterial);

    WaterSurfaceComponent->AddInstance(
        Layout.WaterSurfaceWorldTransform,
        true);
    EdgeFoamComponent->AddInstances(
        Layout.EdgeFoamWorldTransforms, false, true, false);
    PrimaryJetInstances->AddInstances(
        Layout.PrimaryJetWorldTransforms, false, true, false);
    SecondarySprayInstances->AddInstances(
        Layout.SecondarySprayWorldTransforms, false, true, false);
    ImpactRippleInstances->AddInstances(
        Layout.ImpactRippleWorldTransforms, false, true, false);
    CentralPlumeComponent->AddInstance(
        Layout.CentralPlumeWorldTransform,
        true);
    CentralSprayInstances->AddInstances(
        Layout.CentralSprayWorldTransforms, false, true, false);
    CentralRippleInstances->AddInstances(
        Layout.CentralRippleWorldTransforms, false, true, false);

    bool bTreesBuiltSynchronously = true;
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         SuccessorComponents)
    {
        bTreesBuiltSynchronously =
            Component->BuildTreeIfOutdated(false, true) &&
            !Component->IsAsyncBuilding() && Component->IsTreeFullyBuilt() &&
            bTreesBuiltSynchronously;
        Component->bAutoRebuildTreeOnInstanceChanges = true;
    }
    if (!bTreesBuiltSynchronously)
    {
        ClearSuccessorComponents(SuccessorComponents);
        OutError = TEXT("A V5D fountain successor HISM failed its forced synchronous tree build.");
        return false;
    }

    // The inherited plume carrier is a long tapered shell. R4's sparse
    // additive shader improved it, but a continuous carrier still reads as a
    // rigid glass rod from the public-view camera. Keep its exact serialized
    // mesh/material/transform census for deterministic validation while only
    // the short spray fragments, foam, and ripples remain visible.
    PrimaryJetInstances->SetVisibility(false, true);
    PrimaryJetInstances->SetHiddenInGame(true);
    PrimaryJetInstances->SetRenderInMainPass(false);
    PrimaryJetInstances->SetRenderInDepthPass(false);
    SecondarySprayInstances->SetVisibility(false, true);
    SecondarySprayInstances->SetHiddenInGame(true);
    SecondarySprayInstances->SetRenderInMainPass(false);
    SecondarySprayInstances->SetRenderInDepthPass(false);
    CentralPlumeComponent->SetVisibility(false, true);
    CentralPlumeComponent->SetHiddenInGame(true);
    CentralPlumeComponent->SetRenderInMainPass(false);
    CentralPlumeComponent->SetRenderInDepthPass(false);
    CentralSprayInstances->SetVisibility(false, true);
    CentralSprayInstances->SetHiddenInGame(true);
    CentralSprayInstances->SetRenderInMainPass(false);
    CentralSprayInstances->SetRenderInDepthPass(false);

    V5AppearanceActor = InV5AppearanceActor;
    V5BVisualActor = InV5BVisualActor;
    SavedSourceWaterSurfaceMesh = SourceSurfaceMesh;
    SavedSourceEdgeFoamMesh = SourceFoamMesh;
    SavedSourceOuterPlumeMesh = SourceOuterMesh;
    SavedSourceImpactRingMesh = SourceImpactMesh;
    SavedSourceCentralPlumeMesh = SourceCentralMesh;
    SavedSourceWaterSurfaceMaterial = SourceSurfaceMaterial;
    SavedSourceEdgeFoamMaterial = SourceFoamMaterial;
    SavedSourceSprayMaterial = SourceOuterMaterial;
    SavedSuccessorWaterMaterial = InSuccessorWaterMaterial;
    SavedSuccessorSprayMaterial = InSuccessorSprayMaterial;
    SavedEmbeddedWaterSuppressorMaterial =
        InEmbeddedWaterSuppressorMaterial;
    SavedHardscapeOriginalOverrideMaterials =
        OriginalHardscapeOverrides;
    SavedHardscapeRenderSuccessorMesh = HardscapeMesh;
    SavedHardscapeWaterMaterialSlot = WaterMaterialSlot;
    SavedSourceWaterSurfaceWorldTransform = SourceSurface;
    SavedSourceEdgeFoamWorldTransform = SourceFoam;
    SavedSourceOuterPlumeWorldTransforms = SourceOuter;
    SavedSourceImpactRingWorldTransforms = SourceImpact;
    SavedSourceCentralPlumeWorldTransform = SourceCentral;
    SavedSuccessorLayout = Layout;
    bEmbeddedHardscapeWaterSectionSuppressed = true;
    bConfigured = true;
    bConfiguredInTrustedUntitledHybridBuilder =
        bTrustedUntitledHybridBuilder;

    SetSourceRendererReplacementState(SourceComponents, true);
    HardscapeSuccessor->SetMaterial(
        WaterMaterialSlot,
        InEmbeddedWaterSuppressorMaterial);
    HardscapeSuccessor->MarkRenderStateDirty();
    InV5BVisualActor->Tags.AddUnique(SourceRendererReplacementTag());
    FString Report;
    if (!ValidateFountainRealism(Report))
    {
        SetSourceRendererReplacementState(SourceComponents, false);
        HardscapeSuccessor->OverrideMaterials =
            OriginalHardscapeOverrides;
        HardscapeSuccessor->MarkRenderStateDirty();
        InV5BVisualActor->Tags.Remove(SourceRendererReplacementTag());
        ClearSuccessorComponents(SuccessorComponents);
        V5AppearanceActor = nullptr;
        V5BVisualActor = nullptr;
        SavedSourceWaterSurfaceMesh = nullptr;
        SavedSourceEdgeFoamMesh = nullptr;
        SavedSourceOuterPlumeMesh = nullptr;
        SavedSourceImpactRingMesh = nullptr;
        SavedSourceCentralPlumeMesh = nullptr;
        SavedSourceWaterSurfaceMaterial = nullptr;
        SavedSourceEdgeFoamMaterial = nullptr;
        SavedSourceSprayMaterial = nullptr;
        SavedSuccessorWaterMaterial = nullptr;
        SavedSuccessorSprayMaterial = nullptr;
        SavedEmbeddedWaterSuppressorMaterial = nullptr;
        SavedHardscapeOriginalOverrideMaterials.Reset();
        SavedHardscapeRenderSuccessorMesh = nullptr;
        SavedHardscapeWaterMaterialSlot = INDEX_NONE;
        SavedSourceOuterPlumeWorldTransforms.Reset();
        SavedSourceImpactRingWorldTransforms.Reset();
        SavedSuccessorLayout =
            FTRIADIstanaExploreV5DFountainSuccessorLayout();
        bEmbeddedHardscapeWaterSectionSuppressed = false;
        bConfigured = false;
        bConfiguredInTrustedUntitledHybridBuilder = false;
        OutError = TEXT("V5D fountain successor failed atomic post-apply validation and restored every source renderer: ") +
            Report;
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DFountainRealismActor::
    ValidateFountainRealism(FString& OutReport) const
{
    UWorld* World = GetWorld();
    const bool bAdmittedWorld = IsExactTargetWorld(World) ||
        IsTrustedUntitledBuilderWorld(
            World,
            bConfiguredInTrustedUntitledHybridBuilder);
    if (!bConfigured || !bAdmittedWorld ||
        !V5AppearanceActor || !V5BVisualActor ||
        V5AppearanceActor->GetWorld() != World ||
        V5BVisualActor->GetWorld() != World ||
        !Tags.Contains(ExpectedActorTag()) ||
        !V5BVisualActor->Tags.Contains(SourceRendererReplacementTag()) ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SceneRoot || GetRootComponent() != SceneRoot ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !ClaimLabel.Equals(ExpectedClaimLabel(), ESearchCase::CaseSensitive) ||
        !bAppearanceOnly || !bExactV5DMapOnly ||
        !bInheritedFountainRenderersReplaced ||
        bSourceMeshMaterialAssetsModified ||
        bSourceTransformsOrCensusModified ||
        !bEmbeddedHardscapeWaterSectionSuppressed ||
        bCollisionNavigationSensorOrRfAuthority ||
        bHydraulicSimulationOrOperatingStateClaimed ||
        bSurveyOrAsBuiltClaimed ||
        (World->IsGameWorld() &&
            (!HasActorBegunPlay() || !IsActorTickEnabled())))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: owner, source bindings, identity map gate, or truth tuple changed.");
        return false;
    }

    FString V5Report;
    UMaterial* InheritedWaterBase = V5AppearanceActor->FountainWaterMaterial
        ? V5AppearanceActor->FountainWaterMaterial->GetMaterial()
        : nullptr;
    UMaterial* SuccessorWaterBase = SavedSuccessorWaterMaterial
        ? SavedSuccessorWaterMaterial->GetMaterial()
        : nullptr;
    UMaterial* SuccessorSprayBase = SavedSuccessorSprayMaterial
        ? SavedSuccessorSprayMaterial->GetMaterial()
        : nullptr;
    UMaterial* EmbeddedWaterSuppressorBase =
        SavedEmbeddedWaterSuppressorMaterial
        ? SavedEmbeddedWaterSuppressorMaterial->GetMaterial()
        : nullptr;
    if (!V5AppearanceActor->ValidateExploreV5Appearance(
            V5Report,
            false) ||
        !V5AppearanceActor->FountainWaterMaterial ||
        V5AppearanceActor->FountainWaterMaterial->GetPathName() !=
            ATRIADIstanaExploreV5AppearanceActor::
                ExpectedFountainWaterMaterialPath() ||
        !InheritedWaterBase ||
        InheritedWaterBase->MaterialDomain != MD_Surface ||
        InheritedWaterBase->GetBlendMode() != BLEND_Translucent ||
        !SavedSuccessorWaterMaterial ||
        SavedSuccessorWaterMaterial->GetPathName() !=
            ExpectedWaterMaterialPath() ||
        !SavedSuccessorSprayMaterial ||
        SavedSuccessorSprayMaterial->GetPathName() !=
            ExpectedSprayMaterialPath() ||
        !SavedEmbeddedWaterSuppressorMaterial ||
        SavedEmbeddedWaterSuppressorMaterial->GetPathName() !=
            ExpectedEmbeddedWaterSuppressorMaterialPath() ||
        SuccessorWaterBase != SavedSuccessorWaterMaterial ||
        SuccessorSprayBase != SavedSuccessorSprayMaterial ||
        EmbeddedWaterSuppressorBase !=
            SavedEmbeddedWaterSuppressorMaterial ||
        SuccessorWaterBase->MaterialDomain != MD_Surface ||
        SuccessorWaterBase->GetBlendMode() != BLEND_Opaque ||
        SuccessorWaterBase->TwoSided ||
        SuccessorWaterBase->bTangentSpaceNormal ||
        !SuccessorWaterBase->bUsedWithInstancedStaticMeshes ||
        SuccessorWaterBase->bScreenSpaceReflections ||
        SuccessorSprayBase->MaterialDomain != MD_Surface ||
        SuccessorSprayBase->GetBlendMode() != BLEND_Additive ||
        SuccessorSprayBase->TwoSided ||
        SuccessorSprayBase->bTangentSpaceNormal ||
        !SuccessorSprayBase->bUsedWithInstancedStaticMeshes ||
        SuccessorSprayBase->TranslucencyLightingMode !=
            TLM_VolumetricNonDirectional ||
        SuccessorSprayBase->RefractionMethod != RM_PixelNormalOffset ||
        SuccessorSprayBase->bScreenSpaceReflections ||
        EmbeddedWaterSuppressorBase->MaterialDomain != MD_Surface ||
        EmbeddedWaterSuppressorBase->GetBlendMode() != BLEND_Additive ||
        EmbeddedWaterSuppressorBase->TwoSided ||
        EmbeddedWaterSuppressorBase->bTangentSpaceNormal ||
        EmbeddedWaterSuppressorBase->bScreenSpaceReflections ||
        !EmbeddedWaterSuppressorBase->GetShadingModels()
             .HasOnlyShadingModel(MSM_Unlit))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: inherited V5 source material or exact V5D-owned water/spray/suppressor trio drifted. ") +
            V5Report;
        return false;
    }

    UStaticMeshComponent* HardscapeSuccessor =
        V5BVisualActor->HardscapeRenderSuccessorComponent;
    UStaticMesh* HardscapeMesh = HardscapeSuccessor
        ? HardscapeSuccessor->GetStaticMesh()
        : nullptr;
    const TArray<TObjectPtr<UMaterialInterface>>& HardscapeSourceMaterials =
        V5BVisualActor->SavedAssetRoster.HardscapeRenderSuccessorMaterials;
    const int32 CurrentWaterMaterialSlot =
        FindExactHardscapeWaterMaterialSlot(HardscapeMesh);
    if (!HardscapeSuccessor ||
        HardscapeSuccessor->GetOwner() != V5BVisualActor ||
        !HardscapeMesh || HardscapeMesh !=
            SavedHardscapeRenderSuccessorMesh ||
        HardscapeMesh->GetPathName() != HardscapeRenderSuccessorMeshPath ||
        HardscapeMesh->GetStaticMaterials().Num() != 3 ||
        CurrentWaterMaterialSlot != HardscapeWaterMaterialSlot ||
        SavedHardscapeWaterMaterialSlot != HardscapeWaterMaterialSlot ||
        HardscapeSourceMaterials.Num() != 3 ||
        SavedHardscapeOriginalOverrideMaterials.Num() != 3 ||
        HardscapeSuccessor->OverrideMaterials.Num() != 3 ||
        !HardscapeSuccessor->IsVisible() ||
        HardscapeSuccessor->bHiddenInGame ||
        !HardscapeSuccessor->bRenderInMainPass ||
        !HardscapeSuccessor->bRenderInDepthPass ||
        HardscapeSuccessor->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        HardscapeSuccessor->GetGenerateOverlapEvents() ||
        HardscapeSuccessor->CanEverAffectNavigation())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: the exact visible V5B hardscape successor or its M_IPV_Water slot contract drifted.");
        return false;
    }
    for (int32 MaterialIndex = 0; MaterialIndex < 3; ++MaterialIndex)
    {
        if (!HardscapeSourceMaterials[MaterialIndex] ||
            SavedHardscapeOriginalOverrideMaterials[MaterialIndex] !=
                HardscapeSourceMaterials[MaterialIndex])
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: saved hardscape source override %d no longer matches the exact V5B asset roster."),
                MaterialIndex);
            return false;
        }
        UMaterialInterface* ExpectedCurrentMaterial =
            MaterialIndex == HardscapeWaterMaterialSlot
            ? SavedEmbeddedWaterSuppressorMaterial.Get()
            : SavedHardscapeOriginalOverrideMaterials[MaterialIndex].Get();
        if (HardscapeSuccessor->GetMaterial(MaterialIndex) !=
            ExpectedCurrentMaterial)
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: hardscape material slot %d escaped its exact stone/suppressed-water/metal presentation."),
                MaterialIndex);
            return false;
        }
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*> SourceComponents = {
        V5BVisualActor->FountainSurfaceComponent,
        V5BVisualActor->FountainEdgeFoamComponent,
        V5BVisualActor->OuterPlumeInstances,
        V5BVisualActor->ImpactRingInstances,
        V5BVisualActor->CentralPlumeComponent};
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         SourceComponents)
    {
        if (!SourceComponentIsHiddenRenderOnly(Component, V5BVisualActor))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: one inherited V5B fountain renderer escaped the exact render-only successor replacement state.");
            return false;
        }
    }

    TArray<FTransform> SourceSurface;
    TArray<FTransform> SourceFoam;
    TArray<FTransform> SourceOuter;
    TArray<FTransform> SourceImpact;
    TArray<FTransform> SourceCentral;
    FString Error;
    if (V5BVisualActor->FountainSurfaceComponent->GetStaticMesh() !=
            SavedSourceWaterSurfaceMesh ||
        V5BVisualActor->FountainEdgeFoamComponent->GetStaticMesh() !=
            SavedSourceEdgeFoamMesh ||
        V5BVisualActor->OuterPlumeInstances->GetStaticMesh() !=
            SavedSourceOuterPlumeMesh ||
        V5BVisualActor->ImpactRingInstances->GetStaticMesh() !=
            SavedSourceImpactRingMesh ||
        V5BVisualActor->CentralPlumeComponent->GetStaticMesh() !=
            SavedSourceCentralPlumeMesh ||
        V5BVisualActor->FountainSurfaceComponent->GetMaterial(0) !=
            SavedSourceWaterSurfaceMaterial ||
        V5BVisualActor->FountainEdgeFoamComponent->GetMaterial(0) !=
            SavedSourceEdgeFoamMaterial ||
        V5BVisualActor->OuterPlumeInstances->GetMaterial(0) !=
            SavedSourceSprayMaterial ||
        V5BVisualActor->ImpactRingInstances->GetMaterial(0) !=
            SavedSourceEdgeFoamMaterial ||
        V5BVisualActor->CentralPlumeComponent->GetMaterial(0) !=
            SavedSourceSprayMaterial ||
        !ReadExactInstanceTransforms(
            V5BVisualActor->FountainSurfaceComponent,
            1,
            SourceSurface,
            Error) ||
        !ReadExactInstanceTransforms(
            V5BVisualActor->FountainEdgeFoamComponent,
            1,
            SourceFoam,
            Error) ||
        !ReadExactInstanceTransforms(
            V5BVisualActor->OuterPlumeInstances,
            SourceOuterPlumeCount,
            SourceOuter,
            Error) ||
        !ReadExactInstanceTransforms(
            V5BVisualActor->ImpactRingInstances,
            SourceOuterPlumeCount,
            SourceImpact,
            Error) ||
        !ReadExactInstanceTransforms(
            V5BVisualActor->CentralPlumeComponent,
            1,
            SourceCentral,
            Error) ||
        !SourceSurface[0].Equals(
            SavedSourceWaterSurfaceWorldTransform,
            TransformTolerance) ||
        !SourceFoam[0].Equals(
            SavedSourceEdgeFoamWorldTransform,
            TransformTolerance) ||
        !TransformArraysEqual(
            SourceOuter,
            SavedSourceOuterPlumeWorldTransforms) ||
        !TransformArraysEqual(
            SourceImpact,
            SavedSourceImpactRingWorldTransforms) ||
        !SourceCentral[0].Equals(
            SavedSourceCentralPlumeWorldTransform,
            TransformTolerance))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: inherited V5B meshes, materials, transforms, or census changed. ") +
            Error;
        return false;
    }

    const TArray<FTransform> SurfaceTransforms = {
        SavedSuccessorLayout.WaterSurfaceWorldTransform};
    const TArray<FTransform> CentralPlumeTransforms = {
        SavedSuccessorLayout.CentralPlumeWorldTransform};
    if (!ValidateRenderOnlyHism(
            WaterSurfaceComponent,
            SceneRoot,
            SavedSourceWaterSurfaceMesh,
            SavedSuccessorWaterMaterial,
            SurfaceTransforms,
            true,
            TEXT("water surface"),
            Error) ||
        !ValidateRenderOnlyHism(
            EdgeFoamComponent,
            SceneRoot,
            SavedSourceEdgeFoamMesh,
            SavedSuccessorSprayMaterial,
            SavedSuccessorLayout.EdgeFoamWorldTransforms,
            true,
            TEXT("layered edge foam"),
            Error) ||
        !ValidateRenderOnlyHism(
            PrimaryJetInstances,
            SceneRoot,
            SavedSourceOuterPlumeMesh,
            SavedSuccessorSprayMaterial,
            SavedSuccessorLayout.PrimaryJetWorldTransforms,
            false,
            TEXT("primary jets"),
            Error) ||
        !ValidateRenderOnlyHism(
            SecondarySprayInstances,
            SceneRoot,
            SavedSourceOuterPlumeMesh,
            SavedSuccessorSprayMaterial,
            SavedSuccessorLayout.SecondarySprayWorldTransforms,
            false,
            TEXT("secondary spray fragments"),
            Error) ||
        !ValidateRenderOnlyHism(
            ImpactRippleInstances,
            SceneRoot,
            SavedSourceImpactRingMesh,
            SavedSuccessorSprayMaterial,
            SavedSuccessorLayout.ImpactRippleWorldTransforms,
            true,
            TEXT("asymmetric impact ripples"),
            Error) ||
        !ValidateRenderOnlyHism(
            CentralPlumeComponent,
            SceneRoot,
            SavedSourceOuterPlumeMesh,
            SavedSuccessorSprayMaterial,
            CentralPlumeTransforms,
            false,
            TEXT("central plume"),
            Error) ||
        !ValidateRenderOnlyHism(
            CentralSprayInstances,
            SceneRoot,
            SavedSourceOuterPlumeMesh,
            SavedSuccessorSprayMaterial,
            SavedSuccessorLayout.CentralSprayWorldTransforms,
            false,
            TEXT("central spray fragments"),
            Error) ||
        !ValidateRenderOnlyHism(
            CentralRippleInstances,
            SceneRoot,
            SavedSourceImpactRingMesh,
            SavedSuccessorSprayMaterial,
            SavedSuccessorLayout.CentralRippleWorldTransforms,
            true,
            TEXT("central disturbance ripples"),
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: ") + Error;
        return false;
    }

    FTRIADIstanaExploreV5DFountainSuccessorLayout RebuiltLayout;
    if (!BuildDeterministicSuccessorLayout(
            SavedSourceWaterSurfaceWorldTransform,
            SavedSourceEdgeFoamWorldTransform,
            SavedSourceOuterPlumeWorldTransforms,
            SavedSourceImpactRingWorldTransforms,
            SavedSourceCentralPlumeWorldTransform,
            RebuiltLayout,
            Error) ||
        !RebuiltLayout.WaterSurfaceWorldTransform.Equals(
            SavedSuccessorLayout.WaterSurfaceWorldTransform,
            TransformTolerance) ||
        !RebuiltLayout.CentralPlumeWorldTransform.Equals(
            SavedSuccessorLayout.CentralPlumeWorldTransform,
            TransformTolerance) ||
        !TransformArraysEqual(
            RebuiltLayout.EdgeFoamWorldTransforms,
            SavedSuccessorLayout.EdgeFoamWorldTransforms) ||
        !TransformArraysEqual(
            RebuiltLayout.PrimaryJetWorldTransforms,
            SavedSuccessorLayout.PrimaryJetWorldTransforms) ||
        !TransformArraysEqual(
            RebuiltLayout.SecondarySprayWorldTransforms,
            SavedSuccessorLayout.SecondarySprayWorldTransforms) ||
        !TransformArraysEqual(
            RebuiltLayout.ImpactRippleWorldTransforms,
            SavedSuccessorLayout.ImpactRippleWorldTransforms) ||
        !TransformArraysEqual(
            RebuiltLayout.CentralSprayWorldTransforms,
            SavedSuccessorLayout.CentralSprayWorldTransforms) ||
        !TransformArraysEqual(
            RebuiltLayout.CentralRippleWorldTransforms,
            SavedSuccessorLayout.CentralRippleWorldTransforms))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID: saved successor layout no longer matches the pure deterministic builder. ") +
            Error;
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_FOUNTAIN_VALID revision=R7 inheritedV5BRenderersReplaced=5 sourceMeshMaterialAssetsUntouched=true sourceTransformsCensusUntouched=true hardscapeWaterComponentOverrideApplied=true embeddedHardscapeWaterSectionSuppressed=true embeddedHardscapeWaterSectionTriangles=%d embeddedLegacyJetGroups=%d embeddedLegacyJetTriangles=%d replacementWaterSurfaceVisibleInstances=1 hardscapeStoneMetalPreserved=true remainingVisibleSuccessorMeshesPlanar=true waterMaterial=M_IPV5D_FountainWater waterBlend=Opaque waterRefraction=NotApplicableOpaque waterNormal=worldXYCalmWorldSpace waterRoughness=0.18 waterSpecular=0.50 waterSsr=false sprayMaterial=M_IPV5D_FountainSpray sprayBlend=Additive sprayShading=Unlit sprayRefraction=Unplugged sprayTwoSided=false sprayMaxOpacity=0.07 sprayOpacityFloor=0 successorFoamRippleMaterial=M_IPV5D_FountainSpray embeddedWaterSuppressorMaterial=M_IPV5D_FountainEmbeddedWaterSuppressor embeddedWaterSuppressorBlend=Additive embeddedWaterSuppressorShading=Unlit embeddedWaterSuppressorOutput=Zero layeredEdgeFoam=%d primaryJetCarrierTransforms=%d primaryJetVisibleInstances=0 secondarySprayCarrierTransforms=%d secondarySprayVisibleInstances=0 asymmetricImpactRipples=%d centralPlumeCarrierTransforms=1 centralPlumeVisibleInstances=0 centralSprayCarrierTransforms=%d centralSprayVisibleInstances=0 centralDisturbanceRipples=%d allLegacyAndSuccessorPlumeCarrierSilhouettesSuppressed=true deterministic=true perfectConesAndIdenticalCircularRingsBroken=true runtimeFailClosedMonitor=%s renderOnly=true collision=false navigation=false sensorRfAuthority=false hydraulicSimulation=false operatingStateClaim=false survey=false asBuilt=false"),
        EmbeddedHardscapeWaterSectionTriangles,
        EmbeddedLegacyJetGroups,
        EmbeddedLegacyJetTriangles,
        EdgeFoamCount,
        PrimaryJetCount,
        SecondarySprayCount,
        ImpactRippleCount,
        CentralSprayCount,
        CentralRippleCount,
        World->IsGameWorld() ? TEXT("active") : TEXT("notRequired"));
    return true;
}

void ATRIADIstanaExploreV5DFountainRealismActor::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickInterval(1.0f);
    SetActorTickEnabled(true);
    FString Report;
    if (ValidateFountainRealism(Report))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DFountainRealism,
            Display,
            TEXT("%s"),
            *Report);
    }
    else
    {
        RestoreInheritedFountainRenderingForFailure();
        SetActorTickEnabled(false);
        UE_LOG(
            LogTRIADIstanaExploreV5DFountainRealism,
            Error,
            TEXT("%s"),
            *Report);
    }
}

void ATRIADIstanaExploreV5DFountainRealismActor::Tick(
    float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FString Report;
    if (!ValidateFountainRealism(Report))
    {
        RestoreInheritedFountainRenderingForFailure();
        SetActorTickEnabled(false);
        UE_LOG(
            LogTRIADIstanaExploreV5DFountainRealism,
            Error,
            TEXT("V5D fountain successor drifted at runtime and restored the inherited V5B renderers: %s"),
            *Report);
    }
}

void ATRIADIstanaExploreV5DFountainRealismActor::
    RestoreInheritedFountainRenderingForFailure()
{
    if (IsValid(V5BVisualActor))
    {
        const TArray<UHierarchicalInstancedStaticMeshComponent*>
            SourceComponents = {
                V5BVisualActor->FountainSurfaceComponent,
                V5BVisualActor->FountainEdgeFoamComponent,
                V5BVisualActor->OuterPlumeInstances,
                V5BVisualActor->ImpactRingInstances,
                V5BVisualActor->CentralPlumeComponent};
        SetSourceRendererReplacementState(SourceComponents, false);
        if (IsValid(V5BVisualActor->HardscapeRenderSuccessorComponent) &&
            SavedHardscapeOriginalOverrideMaterials.Num() == 3)
        {
            V5BVisualActor->HardscapeRenderSuccessorComponent->
                OverrideMaterials =
                    SavedHardscapeOriginalOverrideMaterials;
            V5BVisualActor->HardscapeRenderSuccessorComponent->
                MarkRenderStateDirty();
        }
        V5BVisualActor->Tags.Remove(SourceRendererReplacementTag());
    }
    const TArray<UHierarchicalInstancedStaticMeshComponent*>
        SuccessorComponents = {
            WaterSurfaceComponent,
            EdgeFoamComponent,
            PrimaryJetInstances,
            SecondarySprayInstances,
            ImpactRippleInstances,
            CentralPlumeComponent,
            CentralSprayInstances,
            CentralRippleInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         SuccessorComponents)
    {
        if (IsValid(Component))
        {
            Component->SetVisibility(false, true);
            Component->SetHiddenInGame(true);
            Component->SetRenderInMainPass(false);
            Component->SetRenderInDepthPass(false);
        }
    }
    bEmbeddedHardscapeWaterSectionSuppressed = false;
    SetActorHiddenInGame(true);
}

void ATRIADIstanaExploreV5DFountainRealismActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    RestoreInheritedFountainRenderingForFailure();
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DFountainDeterminismTest,
    "TRIAD.Istana.ExploreV5D.Fountain.DeterministicSuccessorLayout",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DFountainDeterminismTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const FVector Center(0.0, 9500.0, 117.0);
    const FTransform Surface(
        FRotator::ZeroRotator,
        Center,
        FVector::OneVector);
    const FTransform Foam(
        FRotator::ZeroRotator,
        FVector(Center.X, Center.Y, Center.Z + 0.4),
        FVector::OneVector);
    const FTransform Central(
        FRotator::ZeroRotator,
        FVector(Center.X, Center.Y, 120.0),
        FVector::OneVector);
    TArray<FTransform> Outer;
    TArray<FTransform> Impact;
    for (int32 Index = 0; Index < SourceOuterPlumeCount; ++Index)
    {
        const double AngleDegrees = 30.0 * Index;
        const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
        const FVector Location(
            Center.X + 720.0 * FMath::Cos(AngleRadians),
            Center.Y + 720.0 * FMath::Sin(AngleRadians),
            122.0);
        Outer.Add(FTransform(
            FRotator(0.0, AngleDegrees, 0.0),
            Location,
            FVector::OneVector));
        Impact.Add(FTransform(
            FRotator(0.0, AngleDegrees, 0.0),
            Location,
            FVector::OneVector));
    }

    FTRIADIstanaExploreV5DFountainSuccessorLayout A;
    FTRIADIstanaExploreV5DFountainSuccessorLayout B;
    FString Error;
    TestTrue(
        TEXT("First pure fountain layout builds"),
        ATRIADIstanaExploreV5DFountainRealismActor::
            BuildDeterministicSuccessorLayout(
                Surface, Foam, Outer, Impact, Central, A, Error));
    TestTrue(
        TEXT("Second pure fountain layout builds"),
        ATRIADIstanaExploreV5DFountainRealismActor::
            BuildDeterministicSuccessorLayout(
                Surface, Foam, Outer, Impact, Central, B, Error));
    TestEqual(TEXT("Primary jet census"), A.PrimaryJetWorldTransforms.Num(), 12);
    TestEqual(
        TEXT("Secondary spray census"),
        A.SecondarySprayWorldTransforms.Num(),
        24);
    TestEqual(
        TEXT("Asymmetric impact-ripple census"),
        A.ImpactRippleWorldTransforms.Num(),
        18);
    TestEqual(
        TEXT("Central spray census"),
        A.CentralSprayWorldTransforms.Num(),
        8);
    TestEqual(
        TEXT("Central ripple census"),
        A.CentralRippleWorldTransforms.Num(),
        5);
    TestTrue(
        TEXT("Primary transforms are deterministic"),
        TransformArraysEqual(
            A.PrimaryJetWorldTransforms,
            B.PrimaryJetWorldTransforms));
    bool bPrimaryJetScalesUseR2Envelope = true;
    for (const FTransform& Transform : A.PrimaryJetWorldTransforms)
    {
        const FVector Scale = Transform.GetScale3D();
        bPrimaryJetScalesUseR2Envelope =
            bPrimaryJetScalesUseR2Envelope &&
            Scale.X >= 0.16 && Scale.X <= 0.26 &&
            Scale.Y >= 0.14 && Scale.Y <= 0.24 &&
            Scale.Z >= 0.70 && Scale.Z <= 0.92;
    }
    TestTrue(
        TEXT("Primary jets use the physically narrower R2 scale envelope"),
        bPrimaryJetScalesUseR2Envelope);
    TestTrue(
        TEXT("Central jet uses the narrower outer-plume R2 carrier scale"),
        A.CentralPlumeWorldTransform.GetScale3D().Equals(
            FVector(0.34, 0.38, 1.55),
            TransformTolerance));
    TestTrue(
        TEXT("Impact transforms are deterministic"),
        TransformArraysEqual(
            A.ImpactRippleWorldTransforms,
            B.ImpactRippleWorldTransforms));
    bool bAllImpactRipplesAnisotropic = true;
    for (const FTransform& Transform : A.ImpactRippleWorldTransforms)
    {
        const FVector Scale = Transform.GetScale3D();
        bAllImpactRipplesAnisotropic =
            bAllImpactRipplesAnisotropic &&
            FMath::Abs(Scale.X - Scale.Y) >= 0.04;
    }
    TestTrue(
        TEXT("Every impact ripple breaks perfect circular repetition"),
        bAllImpactRipplesAnisotropic);
    bool bAllCentralRipplesAnisotropic = true;
    bool bAllCentralRipplesOnVisibleWater = true;
    for (const FTransform& Transform : A.CentralRippleWorldTransforms)
    {
        const FVector Scale = Transform.GetScale3D();
        const FVector Offset = Transform.GetLocation() - Center;
        const double CenterRadiusCm = FVector2D(Offset.X, Offset.Y).Size();
        bAllCentralRipplesAnisotropic =
            bAllCentralRipplesAnisotropic &&
            FMath::Abs(Scale.X - Scale.Y) >= 0.04;
        bAllCentralRipplesOnVisibleWater =
            bAllCentralRipplesOnVisibleWater &&
            CenterRadiusCm >= CentralRippleMinimumCenterRadiusCm &&
            CenterRadiusCm <= CentralRippleMaximumCenterRadiusCm;
    }
    TestTrue(
        TEXT("Every central ripple breaks perfect circular repetition"),
        bAllCentralRipplesAnisotropic);
    TestTrue(
        TEXT("Every central ripple lands on the visible inherited water annulus"),
        bAllCentralRipplesOnVisibleWater);

    TArray<FTransform> PerturbedOuter = Outer;
    PerturbedOuter[0].AddToTranslation(FVector(7.0, 0.0, 0.0));
    FTRIADIstanaExploreV5DFountainSuccessorLayout Perturbed;
    TestTrue(
        TEXT("Perturbed source still builds"),
        ATRIADIstanaExploreV5DFountainRealismActor::
            BuildDeterministicSuccessorLayout(
                Surface,
                Foam,
                PerturbedOuter,
                Impact,
                Central,
                Perturbed,
                Error));
    TestFalse(
        TEXT("Successor remains source-transform dependent"),
        TransformArraysEqual(
            A.PrimaryJetWorldTransforms,
            Perturbed.PrimaryJetWorldTransforms));
    return true;
}
#endif
