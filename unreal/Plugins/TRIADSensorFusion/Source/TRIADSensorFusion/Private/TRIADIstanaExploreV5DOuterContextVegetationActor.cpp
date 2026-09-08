#include "TRIADIstanaExploreV5DOuterContextVegetationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV5DTreeGeometryVariationActor.h"

namespace
{
constexpr int32 FormCount = 4;
constexpr int32 VariantsPerForm = 3;
constexpr int32 VariantMeshCount = FormCount * VariantsPerForm;
constexpr int32 PlacementCount = 145;
constexpr int32 OuterTreeCullStartDistanceCm = 0;
constexpr int32 OuterTreeCullEndDistanceCm = 140000;
constexpr float OuterTreeLodDistanceScale = 2.20f;
constexpr double InnerRadiusMicrometers = 300000000.0;
constexpr double OuterRadiusMicrometers = 1000000000.0;
constexpr double MinimumAcceptedZMeters = -1000.0;
constexpr double MaximumAcceptedZMeters = 1000.0;
// HISM instance data is matrix-backed. Preserve exact fixed-point source values
// in SavedLayout, but allow only the sub-millimetre/native numeric drift caused
// by the AddInstances/GetInstanceTransform round trip.
constexpr double NativeInstanceTranslationToleranceCm = 0.02;
constexpr double NativeInstanceRotationTolerance = 0.00001;
constexpr double NativeInstanceScaleTolerance = 0.00001;

const FString CandidateContractSha256(
    TEXT("FEEAD01475A9A2F6105F9BEC616BA71053FCFCFFA1DEB146112589E0BC162560"));
const FString PlacementDatasetSha256(
    TEXT("60BC748B0536BFF1E7C1ADF2AFB86307A96D96E3D779D827393A8275C3717984"));
const FString AttributionText(
    TEXT("\u00a9 OpenStreetMap contributors; https://www.openstreetmap.org/copyright"));
const FName ActorTag(
    TEXT("TRIAD_IstanaExploreV5D_OuterContextVegetation_PostR33_RenderOnly"));

// Deliberately invalid sentinels. A future reviewed source change must replace
// every value with a distinct immutable receipt SHA before configuration can
// become reachable. Caller-supplied values never populate these anchors.
const FString TrustedAcceptedR33ReceiptSha256(
    TEXT("UNSET_ACCEPTED_R33_RECEIPT_SHA256"));
const FString TrustedFutureAssetsOnlyAuthorizationReceiptSha256(
    TEXT("UNSET_FUTURE_ASSETS_ONLY_AUTHORIZATION_RECEIPT_SHA256"));
const FString TrustedTerrainContactReceiptSha256(
    TEXT("UNSET_TERRAIN_CONTACT_RECEIPT_SHA256"));
const FString TrustedPublicDistributionReceiptSha256(
    TEXT("UNSET_PUBLIC_DISTRIBUTION_RECEIPT_SHA256"));

// The current candidate explicitly records all three distribution gates as
// unsatisfied. This remains false until a reviewed source change can point to
// the owned runtime attribution presenter and redistribution evidence.
constexpr bool bCompiledCandidateDistributionGatesSatisfied = false;

const TCHAR* const FallbackMeshPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.SM_IPV5D_Tree_ColumnarNarrow_NearLOD0")};

const int32 MaterialOffsets[] = {0, 3, 6, 9, 12};
const TCHAR* const ExpectedSlotNames[] = {
    TEXT("island_tree_02"),
    TEXT("island_tree_02_leaves"),
    TEXT("island_tree_02_branches"),
    TEXT("tree_small_02_branches"),
    TEXT("tree_small_02_leaves"),
    TEXT("tree_small_02_trunk"),
    TEXT("island_tree_01"),
    TEXT("island_tree_01_leaves"),
    TEXT("island_tree_01_branches"),
    TEXT("jacaranda_tree_branches"),
    TEXT("jacaranda_tree_trunk"),
    TEXT("jacaranda_tree_leaves")};
const TCHAR* const ExpectedResponseMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Trunk_Response.M_IPV5D_Tree_Umbrella_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Leaves_Response.M_IPV5D_Tree_Umbrella_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Branches_Response.M_IPV5D_Tree_Umbrella_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Dome_Branches_Response.M_IPV5D_Tree_Dome_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Dome_Leaves_Response.M_IPV5D_Tree_Dome_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Dome_Trunk_Response.M_IPV5D_Tree_Dome_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_HighFork_Trunk_Response.M_IPV5D_Tree_HighFork_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_HighFork_Leaves_Response.M_IPV5D_Tree_HighFork_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_HighFork_Branches_Response.M_IPV5D_Tree_HighFork_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Branches_Response.M_IPV5D_Tree_Columnar_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Trunk_Response.M_IPV5D_Tree_Columnar_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Leaves_Response.M_IPV5D_Tree_Columnar_Leaves_Response")};

#define TRIAD_OUTER_CONTEXT_VEGETATION_PLACEMENT(KeyLiteral, Variant, XMicrometers, YMicrometers, YawMillidegrees, ScaleMillionths) \
    {TEXT(KeyLiteral), Variant, XMicrometers, YMicrometers, YawMillidegrees, ScaleMillionths},
const FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement
    ExpectedPlacements[] = {
#include "TRIADIstanaExploreV5DOuterContextVegetationPlacements.inl"
};
#undef TRIAD_OUTER_CONTEXT_VEGETATION_PLACEMENT

static_assert(UE_ARRAY_COUNT(FallbackMeshPaths) == FormCount);
static_assert(UE_ARRAY_COUNT(MaterialOffsets) == FormCount + 1);
static_assert(UE_ARRAY_COUNT(ExpectedSlotNames) == 12);
static_assert(UE_ARRAY_COUNT(ExpectedResponseMaterialPaths) == 12);
static_assert(UE_ARRAY_COUNT(ExpectedPlacements) == PlacementCount);

bool IsSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

bool AreDistinctTrustAnchors()
{
    const TArray<FString> Values = {
        TrustedAcceptedR33ReceiptSha256,
        TrustedFutureAssetsOnlyAuthorizationReceiptSha256,
        TrustedTerrainContactReceiptSha256,
        TrustedPublicDistributionReceiptSha256};
    TSet<FString> Unique;
    for (const FString& Value : Values)
    {
        const FString Upper = Value.ToUpper();
        if (!IsSha256(Value) || Unique.Contains(Upper))
        {
            return false;
        }
        Unique.Add(Upper);
    }
    return Unique.Num() == Values.Num();
}

bool IsFiniteTerrainContactZ(double ZMeters)
{
    return FMath::IsFinite(ZMeters) && ZMeters >= MinimumAcceptedZMeters &&
        ZMeters <= MaximumAcceptedZMeters;
}

bool IsExpectedRadius(const
    FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement& Placement)
{
    const long double X = static_cast<long double>(Placement.XEastMicrometers);
    const long double Y = static_cast<long double>(Placement.YSouthMicrometers);
    const long double RadiusSquared = X * X + Y * Y;
    const long double InnerSquared =
        static_cast<long double>(InnerRadiusMicrometers) * InnerRadiusMicrometers;
    const long double OuterSquared =
        static_cast<long double>(OuterRadiusMicrometers) * OuterRadiusMicrometers;
    return RadiusSquared > InnerSquared && RadiusSquared <= OuterSquared;
}

FTransform MakeExpectedTransform(
    const FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement&
        Placement,
    double ZMeters)
{
    const FVector Translation(
        static_cast<double>(Placement.XEastMicrometers) / 10000.0,
        static_cast<double>(Placement.YSouthMicrometers) / 10000.0,
        ZMeters * 100.0);
    const double YawDegrees =
        static_cast<double>(Placement.YawMillidegrees) / 1000.0;
    const double UniformScale =
        static_cast<double>(Placement.UniformScaleMillionths) / 1000000.0;
    return FTransform(
        FRotator(0.0, YawDegrees, 0.0).Quaternion(),
        Translation,
        FVector(UniformScale));
}

bool ExactTransformValue(const FTransform& A, const FTransform& B)
{
    return A.GetTranslation().Equals(B.GetTranslation(), 0.0) &&
        A.GetRotation().Equals(B.GetRotation(), 0.0) &&
        A.GetScale3D().Equals(B.GetScale3D(), 0.0);
}

bool NativeInstanceTransformValue(const FTransform& A, const FTransform& B)
{
    return A.GetTranslation().Equals(
               B.GetTranslation(),
               NativeInstanceTranslationToleranceCm) &&
        A.GetRotation().Equals(
            B.GetRotation(),
            NativeInstanceRotationTolerance) &&
        A.GetScale3D().Equals(B.GetScale3D(), NativeInstanceScaleTolerance);
}

void ConfigureRenderOnly(
    UHierarchicalInstancedStaticMeshComponent* Component)
{
    if (!Component)
    {
        return;
    }
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(true);
    Component->SetCullDistances(
        OuterTreeCullStartDistanceCm,
        OuterTreeCullEndDistanceCm);
    Component->SetForcedLodModel(0);
    Component->SetLODDistanceScale(OuterTreeLodDistanceScale);
    Component->ComponentTags.AddUnique(ActorTag);
    Component->SetVisibility(false, true);
    Component->SetHiddenInGame(true, true);
}

bool HasRenderOnlyPolicy(
    const UHierarchicalInstancedStaticMeshComponent* Component)
{
    int32 CullStart = 0;
    int32 CullEnd = 0;
    if (Component)
    {
        Component->GetCullDistances(CullStart, CullEnd);
    }
    return Component &&
        Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore) &&
        !Component->GetGenerateOverlapEvents() &&
        !Component->CanEverAffectNavigation() && Component->CastShadow &&
        !Component->bCastContactShadow &&
        Component->GetLODDistanceScale() == OuterTreeLodDistanceScale &&
        CullStart == OuterTreeCullStartDistanceCm &&
        CullEnd == OuterTreeCullEndDistanceCm &&
        Component->ComponentTags.Contains(ActorTag);
}

bool ValidateMeshMaterials(UStaticMesh* Mesh, int32 FormIndex)
{
    if (!Mesh || FormIndex < 0 || FormIndex >= FormCount ||
        Mesh->GetNumLODs() != 4)
    {
        return false;
    }
    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    const int32 Begin = MaterialOffsets[FormIndex];
    const int32 End = MaterialOffsets[FormIndex + 1];
    if (Materials.Num() != End - Begin)
    {
        return false;
    }
    for (int32 LocalIndex = 0; LocalIndex < Materials.Num(); ++LocalIndex)
    {
        const int32 FlatIndex = Begin + LocalIndex;
        const FStaticMaterial& Material = Materials[LocalIndex];
        if (Material.MaterialSlotName != FName(ExpectedSlotNames[FlatIndex]) ||
            Material.ImportedMaterialSlotName !=
                FName(ExpectedSlotNames[FlatIndex]) ||
            !Material.MaterialInterface ||
            Material.MaterialInterface->GetPathName() !=
                ExpectedResponseMaterialPaths[FlatIndex])
        {
            return false;
        }
    }
    return true;
}

bool ValidateLayoutInternal(
    const FTRIADIstanaExploreV5DOuterContextVegetationLayout& Layout,
    FString& OutError)
{
    if (Layout.VariantBuckets.Num() != VariantMeshCount ||
        Layout.OrderedPlacementKeys.Num() != PlacementCount ||
        Layout.OrderedLocalTransforms.Num() != PlacementCount ||
        Layout.VariantIndexByPlacementOrdinal.Num() != PlacementCount ||
        Layout.XEastMicrometers.Num() != PlacementCount ||
        Layout.YSouthMicrometers.Num() != PlacementCount ||
        Layout.YawMillidegrees.Num() != PlacementCount ||
        Layout.UniformScaleMillionths.Num() != PlacementCount ||
        Layout.TerrainContactZMeters.Num() != PlacementCount ||
        Layout.TotalInstances() != PlacementCount)
    {
        OutError = TEXT("Outer vegetation layout census changed.");
        return false;
    }

    TArray<int32> BucketCursor;
    BucketCursor.Init(0, VariantMeshCount);
    for (int32 Ordinal = 0; Ordinal < PlacementCount; ++Ordinal)
    {
        const auto& Expected = ExpectedPlacements[Ordinal];
        const double ZMeters = Layout.TerrainContactZMeters[Ordinal];
        if (!Expected.PlacementKey || !IsExpectedRadius(Expected) ||
            Expected.VariantIndex < 0 ||
            Expected.VariantIndex >= VariantMeshCount ||
            Expected.YawMillidegrees < 0 ||
            Expected.YawMillidegrees >= 360000 ||
            Expected.UniformScaleMillionths <= 0 ||
            !IsFiniteTerrainContactZ(ZMeters) ||
            Layout.OrderedPlacementKeys[Ordinal] != Expected.PlacementKey ||
            Layout.VariantIndexByPlacementOrdinal[Ordinal] !=
                Expected.VariantIndex ||
            Layout.XEastMicrometers[Ordinal] != Expected.XEastMicrometers ||
            Layout.YSouthMicrometers[Ordinal] != Expected.YSouthMicrometers ||
            Layout.YawMillidegrees[Ordinal] != Expected.YawMillidegrees ||
            Layout.UniformScaleMillionths[Ordinal] !=
                Expected.UniformScaleMillionths ||
            !ExactTransformValue(
                Layout.OrderedLocalTransforms[Ordinal],
                MakeExpectedTransform(Expected, ZMeters)))
        {
            OutError = FString::Printf(
                TEXT("Outer vegetation layout row %d drifted."),
                Ordinal);
            return false;
        }

        const auto& Bucket = Layout.VariantBuckets[Expected.VariantIndex];
        const int32 BucketIndex = BucketCursor[Expected.VariantIndex]++;
        if (!Bucket.PlacementOrdinals.IsValidIndex(BucketIndex) ||
            !Bucket.LocalTransforms.IsValidIndex(BucketIndex) ||
            Bucket.PlacementOrdinals[BucketIndex] != Ordinal ||
            !ExactTransformValue(
                Bucket.LocalTransforms[BucketIndex],
                Layout.OrderedLocalTransforms[Ordinal]))
        {
            OutError = FString::Printf(
                TEXT("Outer vegetation bucket routing row %d drifted."),
                Ordinal);
            return false;
        }
    }

    for (int32 VariantIndex = 0; VariantIndex < VariantMeshCount;
         ++VariantIndex)
    {
        const auto& Bucket = Layout.VariantBuckets[VariantIndex];
        if (Bucket.PlacementOrdinals.Num() !=
                Bucket.LocalTransforms.Num() ||
            BucketCursor[VariantIndex] != Bucket.PlacementOrdinals.Num())
        {
            OutError = TEXT("Outer vegetation bucket census drifted.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ComponentMatches(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    UStaticMesh* ExpectedMesh,
    const TArray<FTransform>& ExpectedTransforms,
    bool bExpectedVisible)
{
    if (!HasRenderOnlyPolicy(Component) ||
        Component->GetStaticMesh() != ExpectedMesh ||
        Component->GetInstanceCount() != ExpectedTransforms.Num() ||
        Component->IsVisible() != bExpectedVisible ||
        Component->bHiddenInGame == bExpectedVisible)
    {
        return false;
    }
    for (int32 Index = 0; Index < ExpectedTransforms.Num(); ++Index)
    {
        FTransform Actual;
        if (!Component->GetInstanceTransform(Index, Actual, false) ||
            !NativeInstanceTransformValue(Actual, ExpectedTransforms[Index]))
        {
            return false;
        }
    }
    return true;
}
} // namespace

int32 FTRIADIstanaExploreV5DOuterContextVegetationLayout::TotalInstances()
    const
{
    int32 Total = 0;
    for (const auto& Bucket : VariantBuckets)
    {
        Total += Bucket.LocalTransforms.Num();
    }
    return Total;
}

ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ATRIADIstanaExploreV5DOuterContextVegetationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    VariantComponents.Reserve(VariantMeshCount);
    for (int32 Index = 0; Index < VariantMeshCount; ++Index)
    {
        const FName ComponentName(
            *FString::Printf(TEXT("OuterContextVariant_%02d"), Index));
        auto* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                ComponentName);
        Component->SetupAttachment(SceneRoot);
        ConfigureRenderOnly(Component);
        VariantComponents.Add(Component);
    }
    Tags.AddUnique(ExpectedActorTag());
    SetActorEnableCollision(false);
}

int32 ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ExpectedPlacementCount()
{
    return PlacementCount;
}

int32 ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ExpectedVariantMeshCount()
{
    return VariantMeshCount;
}

const FString& ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ExpectedAttributionText()
{
    return AttributionText;
}

const FString& ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ExpectedCandidateContractSha256()
{
    return CandidateContractSha256;
}

const FString& ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ExpectedPlacementDatasetSha256()
{
    return PlacementDatasetSha256;
}

const FName& ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ExpectedActorTag()
{
    return ActorTag;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    CanRenderInPresentationState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState State)
{
    return State ==
            ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented ||
        State ==
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    CompiledTrustAnchorsConfigured()
{
    return AreDistinctTrustAnchors();
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    CandidateDistributionGatesDeclaredSatisfied()
{
    return bCompiledCandidateDistributionGatesSatisfied;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    NativeInstanceTransformMatches(
        const FTransform& Actual,
        const FTransform& Expected)
{
    return NativeInstanceTransformValue(Actual, Expected);
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    GetExpectedPlacement(
        int32 Ordinal,
        FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement&
            OutPlacement)
{
    if (Ordinal < 0 || Ordinal >= PlacementCount)
    {
        return false;
    }
    OutPlacement = ExpectedPlacements[Ordinal];
    return true;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    BuildDeterministicLayout(
        const TArray<FTRIADIstanaExploreV5DOuterContextTerrainContact>&
            InOrderedTerrainContacts,
        FTRIADIstanaExploreV5DOuterContextVegetationLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DOuterContextVegetationLayout();
    if (InOrderedTerrainContacts.Num() != PlacementCount)
    {
        OutError = FString::Printf(
            TEXT("Outer vegetation requires exactly %d ordered terrain contacts."),
            PlacementCount);
        return false;
    }

    OutLayout.VariantBuckets.SetNum(VariantMeshCount);
    OutLayout.OrderedPlacementKeys.Reserve(PlacementCount);
    OutLayout.OrderedLocalTransforms.Reserve(PlacementCount);
    OutLayout.VariantIndexByPlacementOrdinal.Reserve(PlacementCount);
    OutLayout.XEastMicrometers.Reserve(PlacementCount);
    OutLayout.YSouthMicrometers.Reserve(PlacementCount);
    OutLayout.YawMillidegrees.Reserve(PlacementCount);
    OutLayout.UniformScaleMillionths.Reserve(PlacementCount);
    OutLayout.TerrainContactZMeters.Reserve(PlacementCount);

    for (int32 Ordinal = 0; Ordinal < PlacementCount; ++Ordinal)
    {
        const auto& Expected = ExpectedPlacements[Ordinal];
        const auto& Contact = InOrderedTerrainContacts[Ordinal];
        if (!Expected.PlacementKey ||
            Contact.PlacementKey != Expected.PlacementKey ||
            !Contact.ZMeters.IsSet() ||
            !IsFiniteTerrainContactZ(Contact.ZMeters.GetValue()) ||
            !IsExpectedRadius(Expected) || Expected.VariantIndex < 0 ||
            Expected.VariantIndex >= VariantMeshCount)
        {
            OutError = FString::Printf(
                TEXT("Outer vegetation terrain-contact row %d is missing, reordered, non-finite, out of bounds, or geographically invalid."),
                Ordinal);
            OutLayout =
                FTRIADIstanaExploreV5DOuterContextVegetationLayout();
            return false;
        }
        const double ZMeters = Contact.ZMeters.GetValue();
        const FTransform Transform = MakeExpectedTransform(Expected, ZMeters);
        if (Transform.ContainsNaN() ||
            !Transform.GetRotation().IsNormalized())
        {
            OutError = FString::Printf(
                TEXT("Outer vegetation transform row %d is invalid."),
                Ordinal);
            OutLayout =
                FTRIADIstanaExploreV5DOuterContextVegetationLayout();
            return false;
        }

        auto& Bucket = OutLayout.VariantBuckets[Expected.VariantIndex];
        Bucket.PlacementOrdinals.Add(Ordinal);
        Bucket.LocalTransforms.Add(Transform);
        OutLayout.OrderedPlacementKeys.Add(Expected.PlacementKey);
        OutLayout.OrderedLocalTransforms.Add(Transform);
        OutLayout.VariantIndexByPlacementOrdinal.Add(Expected.VariantIndex);
        OutLayout.XEastMicrometers.Add(Expected.XEastMicrometers);
        OutLayout.YSouthMicrometers.Add(Expected.YSouthMicrometers);
        OutLayout.YawMillidegrees.Add(Expected.YawMillidegrees);
        OutLayout.UniformScaleMillionths.Add(
            Expected.UniformScaleMillionths);
        OutLayout.TerrainContactZMeters.Add(ZMeters);
    }

    if (!ValidateLayoutInternal(OutLayout, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DOuterContextVegetationLayout();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DOuterContextVegetationAssets& InAssets,
    FString& OutError)
{
    if (InAssets.FallbackMeshes.Num() != FormCount ||
        InAssets.FallbackMeshes.Contains(nullptr) ||
        (InAssets.VariantMeshes.Num() != 0 &&
         InAssets.VariantMeshes.Num() != VariantMeshCount) ||
        InAssets.VariantMeshes.Contains(nullptr))
    {
        OutError = TEXT("Outer vegetation asset roster must contain four mandatory fallback meshes and either zero or twelve preferred variants.");
        return false;
    }
    TSet<const UStaticMesh*> UniqueFallbacks;
    for (int32 FormIndex = 0; FormIndex < FormCount; ++FormIndex)
    {
        UStaticMesh* Mesh = InAssets.FallbackMeshes[FormIndex];
        if (UniqueFallbacks.Contains(Mesh) ||
            Mesh->GetPathName() != FallbackMeshPaths[FormIndex] ||
            !ValidateMeshMaterials(Mesh, FormIndex))
        {
            OutError = FString::Printf(
                TEXT("Outer vegetation fallback mesh %d is not the exact admitted broad-form asset."),
                FormIndex);
            return false;
        }
        UniqueFallbacks.Add(Mesh);
    }
    if (InAssets.VariantMeshes.Num() == VariantMeshCount)
    {
        TSet<const UStaticMesh*> UniqueVariants;
        for (int32 VariantIndex = 0; VariantIndex < VariantMeshCount;
             ++VariantIndex)
        {
            UStaticMesh* Mesh = InAssets.VariantMeshes[VariantIndex];
            const int32 FormIndex = VariantIndex / VariantsPerForm;
            if (UniqueVariants.Contains(Mesh) ||
                Mesh->GetPathName() !=
                    ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                        CandidateObjectPath(VariantIndex) ||
                !ValidateMeshMaterials(Mesh, FormIndex))
            {
                OutError = FString::Printf(
                    TEXT("Outer vegetation preferred mesh %d is not the exact admitted variant asset."),
                    VariantIndex);
                return false;
            }
            UniqueVariants.Add(Mesh);
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::ValidateAdmission(
    const FTRIADIstanaExploreV5DOuterContextVegetationAdmission& InAdmission,
    FString& OutError) const
{
    if (!CompiledTrustAnchorsConfigured() ||
        !CandidateDistributionGatesDeclaredSatisfied())
    {
        OutError = TEXT("Outer vegetation admission is deliberately unavailable: compiled receipt anchors and distribution gates remain unset.");
        return false;
    }
    if (InAdmission.CandidateContractSha256 != CandidateContractSha256 ||
        InAdmission.PlacementDatasetSha256 != PlacementDatasetSha256 ||
        InAdmission.AcceptedR33ReceiptSha256 !=
            TrustedAcceptedR33ReceiptSha256 ||
        InAdmission.FutureAssetsOnlyAuthorizationReceiptSha256 !=
            TrustedFutureAssetsOnlyAuthorizationReceiptSha256 ||
        InAdmission.TerrainContactReceiptSha256 !=
            TrustedTerrainContactReceiptSha256 ||
        InAdmission.PublicDistributionReceiptSha256 !=
            TrustedPublicDistributionReceiptSha256 ||
        !IsSha256(InAdmission.TerrainContactPayloadSha256) ||
        !InAdmission.bAcceptedR33HumanVisualReviewProven ||
        !InAdmission.bFutureAssetsOnlyIntegrationExplicitlyAuthorized ||
        !InAdmission.bTerrainContactZExternallyProven ||
        !InAdmission.bDerivativeDatabaseOdblShareAlikeSatisfied ||
        !InAdmission.bDerivativeDatabaseMachineReadableAccessSatisfied ||
        !InAdmission.bRuntimeVisibleOpenStreetMapAttributionSatisfied ||
        InAdmission.RuntimeVisibleAttributionText != AttributionText ||
        InAdmission.RuntimeAttributionSurfaceId.IsEmpty())
    {
        OutError = TEXT("Outer vegetation admission evidence is incomplete or differs from the compiled trust closure.");
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ApplyFailClosedVisibility()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         VariantComponents)
    {
        if (Component)
        {
            Component->SetVisibility(false, true);
            Component->SetHiddenInGame(true, true);
        }
    }
}

void ATRIADIstanaExploreV5DOuterContextVegetationActor::ClearOwnedInstances()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         VariantComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
            Component->SetStaticMesh(nullptr);
            Component->EmptyOverrideMaterials();
        }
    }
    ApplyFailClosedVisibility();
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::PopulateComponents(
    FString& OutError)
{
    if (VariantComponents.Num() != VariantMeshCount ||
        !ValidateLayoutInternal(SavedLayout, OutError))
    {
        return false;
    }
    const bool bUseVariants =
        SavedAssets.VariantMeshes.Num() == VariantMeshCount;
    for (int32 VariantIndex = 0; VariantIndex < VariantMeshCount;
         ++VariantIndex)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            VariantComponents[VariantIndex];
        UStaticMesh* Mesh = bUseVariants
            ? SavedAssets.VariantMeshes[VariantIndex]
            : SavedAssets.FallbackMeshes[VariantIndex / VariantsPerForm];
        const auto& Transforms =
            SavedLayout.VariantBuckets[VariantIndex].LocalTransforms;
        if (!Component || !Mesh)
        {
            OutError = TEXT("Outer vegetation component or mesh roster is incomplete.");
            return false;
        }
        Component->ClearInstances();
        Component->SetStaticMesh(Mesh);
        Component->EmptyOverrideMaterials();
        if (!Transforms.IsEmpty())
        {
            Component->AddInstances(Transforms, false, false, false);
            Component->BuildTreeIfOutdated(false, true);
        }
        Component->SetVisibility(false, true);
        Component->SetHiddenInGame(true, true);
        if (!ComponentMatches(Component, Mesh, Transforms, false) ||
            (!Transforms.IsEmpty() && !Component->IsTreeFullyBuilt()))
        {
            OutError = FString::Printf(
                TEXT("Outer vegetation HISM bucket %d failed exact population validation."),
                VariantIndex);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ConfigureOuterContextVegetation(
        const FTRIADIstanaExploreV5DOuterContextVegetationAssets& InAssets,
        const TArray<FTRIADIstanaExploreV5DOuterContextTerrainContact>&
            InOrderedTerrainContacts,
        const FTRIADIstanaExploreV5DOuterContextVegetationAdmission&
            InAdmission,
        FString& OutError)
{
    ApplyFailClosedVisibility();
    if (bConfigured ||
        !GetActorTransform().Equals(FTransform::Identity, 0.0) ||
        !ValidateAdmission(InAdmission, OutError) ||
        !ValidateAssetRoster(InAssets, OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DOuterContextVegetationLayout NewLayout;
    if (!BuildDeterministicLayout(
            InOrderedTerrainContacts,
            NewLayout,
            OutError))
    {
        return false;
    }

    ClearOwnedInstances();
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(NewLayout);
    SavedAdmission = InAdmission;
    RuntimeVisibleAttributionText = InAdmission.RuntimeVisibleAttributionText;
    RuntimeAttributionSurfaceId = InAdmission.RuntimeAttributionSurfaceId;
    bAdmissionSatisfied = true;
    bTerrainContactZExternallyProven = true;
    bRuntimeVisibleAttributionSatisfied = true;
    bPublicDistributionGatesSatisfied = true;
    bConfigured = true;
    PresentationState =
        ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary;
    SetActorEnableCollision(false);
    Tags.AddUnique(ExpectedActorTag());
    if (!PopulateComponents(OutError))
    {
        ClearOwnedInstances();
        SavedAssets =
            FTRIADIstanaExploreV5DOuterContextVegetationAssets();
        SavedLayout =
            FTRIADIstanaExploreV5DOuterContextVegetationLayout();
        SavedAdmission =
            FTRIADIstanaExploreV5DOuterContextVegetationAdmission();
        RuntimeVisibleAttributionText.Reset();
        RuntimeAttributionSurfaceId.Reset();
        bConfigured = false;
        bAdmissionSatisfied = false;
        bTerrainContactZExternallyProven = false;
        bRuntimeVisibleAttributionSatisfied = false;
        bPublicDistributionGatesSatisfied = false;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ApplyPresentationState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState InState,
        FString& OutError)
{
    ApplyFailClosedVisibility();
    if (!bConfigured || !bAdmissionSatisfied ||
        !bTerrainContactZExternallyProven ||
        !bRuntimeVisibleAttributionSatisfied ||
        !bPublicDistributionGatesSatisfied ||
        !CompiledTrustAnchorsConfigured() ||
        !CandidateDistributionGatesDeclaredSatisfied() ||
        !ValidateAdmission(SavedAdmission, OutError))
    {
        OutError = TEXT("Outer vegetation presentation remains fail-closed until every compiled admission and owned attribution gate is satisfied.");
        return false;
    }

    PresentationState = InState;
    const bool bVisible = CanRenderInPresentationState(InState);
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         VariantComponents)
    {
        if (Component)
        {
            Component->SetHiddenInGame(!bVisible, true);
            Component->SetVisibility(bVisible, true);
        }
    }
    FString Report;
    if (!ValidateOuterContextVegetation(Report))
    {
        ApplyFailClosedVisibility();
        PresentationState =
            ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary;
        OutError = TEXT("Outer vegetation presentation validation failed closed: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DOuterContextVegetationActor::
    ValidateOuterContextVegetation(FString& OutReport) const
{
    if (VariantComponents.Num() != VariantMeshCount ||
        !GetActorTransform().Equals(FTransform::Identity, 0.0) ||
        GetActorEnableCollision() || !Tags.Contains(ExpectedActorTag()) ||
        bSourceXYKeysYawOrScaleModified ||
        bCollisionNavigationLosRfSensorTerrainOrGeospatialAuthority ||
        bMapBindingImplemented || bNativeVisualAcceptanceProvided)
    {
        OutReport = TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_INVALID_BASE_TRUTH");
        return false;
    }

    if (!bConfigured)
    {
        if (bAdmissionSatisfied || bTerrainContactZExternallyProven ||
            bRuntimeVisibleAttributionSatisfied ||
            bPublicDistributionGatesSatisfied ||
            !RuntimeVisibleAttributionText.IsEmpty() ||
            !RuntimeAttributionSurfaceId.IsEmpty() ||
            !SavedLayout.OrderedPlacementKeys.IsEmpty() ||
            CompiledTrustAnchorsConfigured() ||
            CandidateDistributionGatesDeclaredSatisfied())
        {
            OutReport = TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_INVALID_DORMANT_TRUTH");
            return false;
        }
        for (const UHierarchicalInstancedStaticMeshComponent* Component :
             VariantComponents)
        {
            if (!HasRenderOnlyPolicy(Component) || Component->IsVisible() ||
                !Component->bHiddenInGame ||
                Component->GetInstanceCount() != 0)
            {
                OutReport = TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_INVALID_DORMANT_COMPONENT");
                return false;
            }
        }
        OutReport = TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_VALID sourceOnlyScaffold=true configured=false placementRows=145 variantBuckets=12 candidateDistributionReady=false compiledTrustAnchorsConfigured=false presentationVisible=false googlePrimaryVisible=false cwtWarmingVisible=false cwtPresentedEligibleOnlyAfterFutureAdmission=true safeLocalEligibleOnlyAfterFutureAdmission=true terrainSampled=false terrainContactZInvented=false mapBinding=false collision=false navigation=false losAuthority=false rfAuthority=false sensorAuthority=false terrainAuthority=false geospatialAuthority=false nativeVisualAcceptance=false");
        return true;
    }

    FString Error;
    if (!bAdmissionSatisfied || !bTerrainContactZExternallyProven ||
        !bRuntimeVisibleAttributionSatisfied ||
        !bPublicDistributionGatesSatisfied ||
        !CompiledTrustAnchorsConfigured() ||
        !CandidateDistributionGatesDeclaredSatisfied() ||
        RuntimeVisibleAttributionText != AttributionText ||
        RuntimeAttributionSurfaceId.IsEmpty() ||
        !ValidateLayoutInternal(SavedLayout, Error) ||
        !ValidateAssetRoster(SavedAssets, Error))
    {
        OutReport = TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_INVALID_ADMISSION_OR_LAYOUT: ") +
            Error;
        return false;
    }
    const bool bExpectedVisible = CanRenderInPresentationState(PresentationState);
    const bool bUseVariants =
        SavedAssets.VariantMeshes.Num() == VariantMeshCount;
    for (int32 VariantIndex = 0; VariantIndex < VariantMeshCount;
         ++VariantIndex)
    {
        UStaticMesh* ExpectedMesh = bUseVariants
            ? SavedAssets.VariantMeshes[VariantIndex]
            : SavedAssets.FallbackMeshes[VariantIndex / VariantsPerForm];
        if (!ComponentMatches(
                VariantComponents[VariantIndex],
                ExpectedMesh,
                SavedLayout.VariantBuckets[VariantIndex].LocalTransforms,
                bExpectedVisible))
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_INVALID_COMPONENT_%d"),
                VariantIndex);
            return false;
        }
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_OUTER_CONTEXT_VEGETATION_VALID sourceOnlyScaffold=true configured=true placementRows=145 variantBuckets=12 presentationState=%d presentationVisible=%s exactXYKeysYawScale=true externallyProvenZ=true runtimeAttributionProven=true publicDistributionGatesSatisfied=true mapBinding=false collision=false navigation=false losAuthority=false rfAuthority=false sensorAuthority=false terrainAuthority=false geospatialAuthority=false nativeVisualAcceptance=false"),
        static_cast<int32>(PresentationState),
        bExpectedVisible ? TEXT("true") : TEXT("false"));
    return true;
}
