#include "TRIADIstanaExploreV5DTreeGeometryVariationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV5DTreeGeometryVariationSelectors.inl"

namespace
{
constexpr int32 FormCount = 5;
constexpr int32 VariantsPerForm = 3;
constexpr int32 VariantMeshCount = FormCount * VariantsPerForm;
constexpr int32 V4MainCount = 720;
constexpr int32 V4HeritageCount = 9;
constexpr int32 R29LandmarkCount = 7;
constexpr int32 SourceAnchorCount =
    V4MainCount + V4HeritageCount + R29LandmarkCount;
constexpr int32 TreeCullStartDistanceCm = 18000;
constexpr int32 TreeCullEndDistanceCm = 80000;
constexpr float TreeLodDistanceScale = 1.80f;
// HISM instance data is matrix-backed. Source transforms remain bit-exact in
// SavedLayout; only native AddInstances/GetInstanceTransform readback receives
// these narrow numeric tolerances.
constexpr double NativeInstanceTranslationToleranceCm = 0.02;
constexpr double NativeInstanceRotationTolerance = 0.00001;
constexpr double NativeInstanceScaleTolerance = 0.00001;

const FString CandidateMeshNamespace(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/GeometryVariationCandidateV4/Meshes"));
const FName ActorTag(
    TEXT("TRIAD_IstanaExploreV5D_TreeGeometryVariation_PostR33_RenderOnly"));
const FString TrustedRuntimeAcceptedR33ReceiptSha256(
    TEXT("UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedRuntimeFutureAuthorizationSha256(
    TEXT("UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));

const TCHAR SelectorText[] =
    TRIAD_TREE_GEOMETRY_VARIATION_SELECTOR_TEXT;
static_assert(UE_ARRAY_COUNT(SelectorText) - 1 == SourceAnchorCount);

#define TRIAD_TREE_GEOMETRY_VARIATION_RECIPE(FormToken, SelectorLiteral, AssetNameLiteral, RootHold, CrownStart, CrownFull, TrunkX, TrunkY, CrownX, CrownY, Height, BendX, BendY, Twist, Ripple, Lobes, Phase) \
    {ETRIADIstanaExploreV5DTreeGeometryForm::FormToken, TEXT(SelectorLiteral)[0], TEXT(AssetNameLiteral), RootHold, CrownStart, CrownFull, FVector2f(TrunkX, TrunkY), FVector2f(CrownX, CrownY), Height, FVector2f(BendX, BendY), Twist, Ripple, Lobes, Phase},
const FTRIADIstanaExploreV5DTreeGeometryVariationRecipe Recipes[] = {
#include "TRIADIstanaExploreV5DTreeGeometryVariationRecipes.inl"
};
#undef TRIAD_TREE_GEOMETRY_VARIATION_RECIPE
static_assert(UE_ARRAY_COUNT(Recipes) == VariantMeshCount);

const int32 MaterialOffsets[] = {0, 3, 6, 9, 12, 13};
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
    TEXT("jacaranda_tree_leaves"),
    TEXT("Atlas")};
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Leaves_Response.M_IPV5D_Tree_Columnar_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Palm_Composite_Response.M_IPV5D_Tree_Palm_Composite_Response")};
const int32 ExpectedLodCounts[] = {4, 4, 4, 4, 3};
static_assert(UE_ARRAY_COUNT(MaterialOffsets) == FormCount + 1);
static_assert(UE_ARRAY_COUNT(ExpectedSlotNames) == 13);
static_assert(UE_ARRAY_COUNT(ExpectedResponseMaterialPaths) == 13);
static_assert(UE_ARRAY_COUNT(ExpectedLodCounts) == FormCount);

const TCHAR* const HeritageIds[] = {
    TEXT("HT2018-295"), TEXT("HT2020-313"), TEXT("HT2003-108"),
    TEXT("HT2003-87"), TEXT("HT2018-292"), TEXT("HT2021-319"),
    TEXT("HT2008-169"), TEXT("HT2018-298"), TEXT("HT2019-306")};
const ETRIADIstanaExploreV5DTreeGeometryForm HeritageForms[] = {
    ETRIADIstanaExploreV5DTreeGeometryForm::Dome,
    ETRIADIstanaExploreV5DTreeGeometryForm::Dome,
    ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella,
    ETRIADIstanaExploreV5DTreeGeometryForm::Dome,
    ETRIADIstanaExploreV5DTreeGeometryForm::HighForkRounded,
    ETRIADIstanaExploreV5DTreeGeometryForm::Dome,
    ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella,
    ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella,
    ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella};
const ETRIADIstanaExploreV5DTreeGeometryForm R29Forms[] = {
    ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella,
    ETRIADIstanaExploreV5DTreeGeometryForm::Dome,
    ETRIADIstanaExploreV5DTreeGeometryForm::HighForkRounded,
    ETRIADIstanaExploreV5DTreeGeometryForm::Columnar,
    ETRIADIstanaExploreV5DTreeGeometryForm::Palm,
    ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella,
    ETRIADIstanaExploreV5DTreeGeometryForm::Dome};
static_assert(UE_ARRAY_COUNT(HeritageIds) == V4HeritageCount);
static_assert(UE_ARRAY_COUNT(HeritageForms) == V4HeritageCount);
static_assert(UE_ARRAY_COUNT(R29Forms) == R29LandmarkCount);

bool IsFinitePositiveTransform(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return !Transform.ContainsNaN() && Transform.GetRotation().IsNormalized() &&
        Scale.X > UE_SMALL_NUMBER && Scale.Y > UE_SMALL_NUMBER &&
        Scale.Z > UE_SMALL_NUMBER;
}

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
        TreeCullStartDistanceCm,
        TreeCullEndDistanceCm);
    Component->SetForcedLodModel(0);
    Component->SetLODDistanceScale(TreeLodDistanceScale);
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
        Component->GetLODDistanceScale() == TreeLodDistanceScale &&
        CullStart == TreeCullStartDistanceCm &&
        CullEnd == TreeCullEndDistanceCm &&
        Component->ComponentTags.Contains(ActorTag);
}

template <typename T>
bool ExactScalarBits(const T& A, const T& B)
{
    return FMemory::Memcmp(&A, &B, sizeof(T)) == 0;
}

bool ExactVectorBits(const FVector& A, const FVector& B)
{
    return ExactScalarBits(A.X, B.X) && ExactScalarBits(A.Y, B.Y) &&
        ExactScalarBits(A.Z, B.Z);
}

bool ExactQuatBits(const FQuat& A, const FQuat& B)
{
    return ExactScalarBits(A.X, B.X) && ExactScalarBits(A.Y, B.Y) &&
        ExactScalarBits(A.Z, B.Z) && ExactScalarBits(A.W, B.W);
}

bool ExactTransformValue(const FTransform& A, const FTransform& B)
{
    return ExactVectorBits(A.GetTranslation(), B.GetTranslation()) &&
        ExactQuatBits(A.GetRotation(), B.GetRotation()) &&
        ExactVectorBits(A.GetScale3D(), B.GetScale3D());
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

bool ValidateLayoutInternal(
    const FTRIADIstanaExploreV5DTreeGeometryVariationLayout& Layout,
    FString& OutError)
{
    if (Layout.VariantBuckets.Num() != VariantMeshCount ||
        Layout.OrderedSourceWorldTransforms.Num() != SourceAnchorCount ||
        Layout.RecipeIndexBySourceOrdinal.Num() != SourceAnchorCount ||
        Layout.TotalInstances() != SourceAnchorCount)
    {
        OutError = TEXT("Tree geometry variation lost its exact 15-bucket/736-anchor census.");
        return false;
    }
    TBitArray<> Seen(false, SourceAnchorCount);
    for (int32 BucketIndex = 0; BucketIndex < VariantMeshCount; ++BucketIndex)
    {
        const FTRIADIstanaExploreV5DTreeGeometryVariationBucket& Bucket =
            Layout.VariantBuckets[BucketIndex];
        if (Bucket.SourceOrdinals.Num() != Bucket.WorldTransforms.Num())
        {
            OutError = TEXT("Tree geometry variation bucket keys and transforms diverged.");
            return false;
        }
        for (int32 Index = 0; Index < Bucket.SourceOrdinals.Num(); ++Index)
        {
            const int32 Ordinal = Bucket.SourceOrdinals[Index];
            if (!Seen.IsValidIndex(Ordinal) || Seen[Ordinal] ||
                Layout.RecipeIndexBySourceOrdinal[Ordinal] != BucketIndex ||
                !ExactTransformValue(
                    Bucket.WorldTransforms[Index],
                    Layout.OrderedSourceWorldTransforms[Ordinal]))
            {
                OutError = TEXT("Tree geometry variation routing duplicated, omitted, reordered, or changed a source transform.");
                return false;
            }
            Seen[Ordinal] = true;
        }
    }
    if (Seen.Contains(false))
    {
        OutError = TEXT("Tree geometry variation routing omitted a source ordinal.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ComponentMatches(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const UStaticMesh* Mesh,
    const TArray<FTransform>& Expected,
    bool bExpectedVisible)
{
    if (!Component || Component->GetStaticMesh() != Mesh ||
        Component->GetInstanceCount() != Expected.Num() ||
        Component->IsVisible() != bExpectedVisible ||
        Component->bHiddenInGame == bExpectedVisible)
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        FTransform Actual;
        if (!Component->GetInstanceTransform(Index, Actual, true) ||
            !NativeInstanceTransformValue(Actual, Expected[Index]))
        {
            return false;
        }
    }
    return true;
}
} // namespace

int32 FTRIADIstanaExploreV5DTreeGeometryVariationLayout::TotalInstances() const
{
    int32 Total = 0;
    for (const FTRIADIstanaExploreV5DTreeGeometryVariationBucket& Bucket :
         VariantBuckets)
    {
        Total += Bucket.WorldTransforms.Num();
    }
    return Total;
}

ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ATRIADIstanaExploreV5DTreeGeometryVariationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);
    SetActorEnableCollision(false);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    VariantComponents.Reserve(VariantMeshCount);
    for (int32 Index = 0; Index < VariantMeshCount; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                FName(*FString::Printf(TEXT("TreeGeometryVariant_%02d"), Index)));
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureRenderOnly(Component);
        VariantComponents.Add(Component);
    }
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::GetRecipe(
    int32 RecipeIndex,
    FTRIADIstanaExploreV5DTreeGeometryVariationRecipe& OutRecipe)
{
    if (RecipeIndex < 0 || RecipeIndex >= VariantMeshCount)
    {
        OutRecipe = FTRIADIstanaExploreV5DTreeGeometryVariationRecipe{};
        return false;
    }
    OutRecipe = Recipes[RecipeIndex];
    return true;
}

FString ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    CandidatePackagePath(int32 RecipeIndex)
{
    return RecipeIndex >= 0 && RecipeIndex < VariantMeshCount
        ? CandidateMeshNamespace / Recipes[RecipeIndex].AssetName
        : FString();
}

FString ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    CandidateObjectPath(int32 RecipeIndex)
{
    const FString PackagePath = CandidatePackagePath(RecipeIndex);
    return PackagePath.IsEmpty()
        ? FString()
        : PackagePath + TEXT(".") + Recipes[RecipeIndex].AssetName;
}

FString ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ExpectedSourceInstanceKey(int32 Ordinal)
{
    if (Ordinal >= 0 && Ordinal < V4MainCount)
    {
        return FString::Printf(TEXT("v4.main.%03d"), Ordinal);
    }
    if (Ordinal >= V4MainCount &&
        Ordinal < V4MainCount + V4HeritageCount)
    {
        return FString::Printf(
            TEXT("v4.heritage.%s"),
            HeritageIds[Ordinal - V4MainCount]);
    }
    if (Ordinal >= V4MainCount + V4HeritageCount &&
        Ordinal < SourceAnchorCount)
    {
        return FString::Printf(
            TEXT("r29.landmark.%02d"),
            Ordinal - V4MainCount - V4HeritageCount);
    }
    return FString();
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::IsResolvedFormAllowed(
    int32 Ordinal,
    ETRIADIstanaExploreV5DTreeGeometryForm Form)
{
    if (Ordinal >= 0 && Ordinal < V4MainCount)
    {
        if (Ordinal >= 272 && Ordinal < 504)
        {
            return Form == ETRIADIstanaExploreV5DTreeGeometryForm::Columnar;
        }
        return Form == ETRIADIstanaExploreV5DTreeGeometryForm::Umbrella ||
            Form == ETRIADIstanaExploreV5DTreeGeometryForm::Dome ||
            Form ==
                ETRIADIstanaExploreV5DTreeGeometryForm::HighForkRounded;
    }
    if (Ordinal >= V4MainCount &&
        Ordinal < V4MainCount + V4HeritageCount)
    {
        return Form == HeritageForms[Ordinal - V4MainCount];
    }
    if (Ordinal >= V4MainCount + V4HeritageCount &&
        Ordinal < SourceAnchorCount)
    {
        return Form == R29Forms[
            Ordinal - V4MainCount - V4HeritageCount];
    }
    return false;
}

TCHAR ATRIADIstanaExploreV5DTreeGeometryVariationActor::ExpectedSelector(
    int32 Ordinal)
{
    return Ordinal >= 0 && Ordinal < SourceAnchorCount
        ? SelectorText[Ordinal]
        : TEXT('\0');
}

int32 ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ExpectedSourceAnchorCount()
{
    return SourceAnchorCount;
}

int32 ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ExpectedVariantMeshCount()
{
    return VariantMeshCount;
}

const FName& ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ExpectedActorTag()
{
    return ActorTag;
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    RuntimeCompiledTrustAnchorsConfigured()
{
    return IsSha256(TrustedRuntimeAcceptedR33ReceiptSha256) &&
        IsSha256(TrustedRuntimeFutureAuthorizationSha256) &&
        !TrustedRuntimeAcceptedR33ReceiptSha256.Equals(
            TrustedRuntimeFutureAuthorizationSha256,
            ESearchCase::IgnoreCase);
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ExactSourceTransformBitsMatch(
        const FTransform& Actual,
        const FTransform& Expected)
{
    return ExactTransformValue(Actual, Expected);
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    NativeInstanceTransformMatches(
        const FTransform& Actual,
        const FTransform& Expected)
{
    return NativeInstanceTransformValue(Actual, Expected);
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DTreeGeometryVariationAssets& Assets,
    FString& OutError)
{
    if (Assets.VariantMeshes.Num() != VariantMeshCount ||
        Assets.VariantMeshes.Contains(nullptr))
    {
        OutError = TEXT("Tree geometry variation requires all fifteen candidate meshes.");
        return false;
    }
    for (int32 RecipeIndex = 0; RecipeIndex < VariantMeshCount; ++RecipeIndex)
    {
        const UStaticMesh* Mesh = Assets.VariantMeshes[RecipeIndex];
        const int32 FormIndex = static_cast<int32>(Recipes[RecipeIndex].Form);
        const int32 SlotStart = MaterialOffsets[FormIndex];
        const int32 SlotEnd = MaterialOffsets[FormIndex + 1];
        if (!Mesh || Mesh->GetPathName() != CandidateObjectPath(RecipeIndex) ||
            Mesh->GetNumLODs() != ExpectedLodCounts[FormIndex] ||
            Mesh->GetStaticMaterials().Num() != SlotEnd - SlotStart)
        {
            OutError = FString::Printf(
                TEXT("Tree geometry candidate %d changed path, LOD count, or slot count."),
                RecipeIndex);
            return false;
        }
        for (int32 Slot = 0; Slot < SlotEnd - SlotStart; ++Slot)
        {
            const FStaticMaterial& StaticMaterial =
                Mesh->GetStaticMaterials()[Slot];
            const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
            const int32 Flat = SlotStart + Slot;
            if (StaticMaterial.MaterialSlotName !=
                    FName(ExpectedSlotNames[Flat]) ||
                StaticMaterial.ImportedMaterialSlotName !=
                    FName(ExpectedSlotNames[Flat]) ||
                !Material || Material->GetPathName() !=
                    ExpectedResponseMaterialPaths[Flat])
            {
                OutError = FString::Printf(
                    TEXT("Tree geometry candidate %d changed exact material slot %d semantics."),
                    RecipeIndex,
                    Slot);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    BuildDeterministicLayout(
        const TArray<FTRIADIstanaExploreV5DTreeGeometrySourceAnchor>&
            InOrderedAnchors,
        FTRIADIstanaExploreV5DTreeGeometryVariationLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DTreeGeometryVariationLayout{};
    if (InOrderedAnchors.Num() != SourceAnchorCount)
    {
        OutError = TEXT("Tree geometry selector requires exactly 736 ordered source anchors.");
        return false;
    }
    OutLayout.VariantBuckets.SetNum(VariantMeshCount);
    OutLayout.OrderedSourceWorldTransforms.Reserve(SourceAnchorCount);
    OutLayout.RecipeIndexBySourceOrdinal.Reserve(SourceAnchorCount);
    for (int32 Ordinal = 0; Ordinal < SourceAnchorCount; ++Ordinal)
    {
        const FTRIADIstanaExploreV5DTreeGeometrySourceAnchor& Anchor =
            InOrderedAnchors[Ordinal];
        const ETRIADIstanaExploreV5DTreeGeometrySourceDomain ExpectedDomain =
            Ordinal < V4MainCount
                ? ETRIADIstanaExploreV5DTreeGeometrySourceDomain::V4Main
                : (Ordinal < V4MainCount + V4HeritageCount
                       ? ETRIADIstanaExploreV5DTreeGeometrySourceDomain::
                             V4Heritage
                       : ETRIADIstanaExploreV5DTreeGeometrySourceDomain::
                             R29Landmark);
        const int32 ExpectedIndex = Ordinal < V4MainCount
            ? Ordinal
            : (Ordinal < V4MainCount + V4HeritageCount
                   ? Ordinal - V4MainCount
                   : Ordinal - V4MainCount - V4HeritageCount);
        if (Anchor.SourceInstanceKey != ExpectedSourceInstanceKey(Ordinal) ||
            Anchor.SourceDomain != ExpectedDomain ||
            Anchor.SourceIndex != ExpectedIndex ||
            !IsResolvedFormAllowed(Ordinal, Anchor.ResolvedForm) ||
            !IsFinitePositiveTransform(Anchor.SourceWorldTransform))
        {
            OutLayout = FTRIADIstanaExploreV5DTreeGeometryVariationLayout{};
            OutError = FString::Printf(
                TEXT("Tree geometry source row %d changed manifest identity, form authority, or transform validity."),
                Ordinal);
            return false;
        }
        const TCHAR Selector = ExpectedSelector(Ordinal);
        const int32 VariantIndex = Selector - TEXT('A');
        const int32 FormIndex = static_cast<int32>(Anchor.ResolvedForm);
        const int32 RecipeIndex = FormIndex * VariantsPerForm + VariantIndex;
        if (VariantIndex < 0 || VariantIndex >= VariantsPerForm ||
            RecipeIndex < 0 || RecipeIndex >= VariantMeshCount)
        {
            OutLayout = FTRIADIstanaExploreV5DTreeGeometryVariationLayout{};
            OutError = TEXT("Tree geometry selector table contains an invalid variant.");
            return false;
        }

        // Assignment is intentional: no decomposition, reconstruction,
        // jitter, scale adjustment, or coordinate conversion is performed.
        const FTransform ExactCopy = Anchor.SourceWorldTransform;
        OutLayout.OrderedSourceWorldTransforms.Add(ExactCopy);
        OutLayout.RecipeIndexBySourceOrdinal.Add(RecipeIndex);
        OutLayout.VariantBuckets[RecipeIndex].SourceOrdinals.Add(Ordinal);
        OutLayout.VariantBuckets[RecipeIndex].WorldTransforms.Add(ExactCopy);
        if (!ExactTransformValue(ExactCopy, Anchor.SourceWorldTransform))
        {
            OutLayout = FTRIADIstanaExploreV5DTreeGeometryVariationLayout{};
            OutError = TEXT("Tree geometry transform copy was not component-exact.");
            return false;
        }
    }
    if (!ValidateLayoutInternal(OutLayout, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DTreeGeometryVariationLayout{};
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DTreeGeometryVariationActor::ClearOwnedInstances()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         VariantComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
            Component->SetVisibility(false, true);
            Component->SetHiddenInGame(true, true);
        }
    }
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::PopulateComponents(
    FString& OutError)
{
    if (VariantComponents.Num() != VariantMeshCount)
    {
        OutError = TEXT("Tree geometry actor lost its exact fifteen-component roster.");
        return false;
    }
    for (int32 Index = 0; Index < VariantMeshCount; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            VariantComponents[Index];
        if (!Component || !HasRenderOnlyPolicy(Component))
        {
            OutError = TEXT("Tree geometry actor component policy changed.");
            return false;
        }
        Component->ClearInstances();
        Component->SetStaticMesh(SavedAssets.VariantMeshes[Index]);
        Component->EmptyOverrideMaterials();
        const TArray<FTransform>& Transforms =
            SavedLayout.VariantBuckets[Index].WorldTransforms;
        if (!Transforms.IsEmpty())
        {
            Component->AddInstances(Transforms, false, true, false);
            Component->BuildTreeIfOutdated(false, true);
        }
        if (!ComponentMatches(
                Component,
                SavedAssets.VariantMeshes[Index],
                Transforms,
                false) ||
            (!Transforms.IsEmpty() && !Component->IsTreeFullyBuilt()))
        {
            OutError = FString::Printf(
                TEXT("Tree geometry HISM %d failed exact population validation."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ConfigureGeometryVariation(
        const FTRIADIstanaExploreV5DTreeGeometryVariationAssets& InAssets,
        const TArray<FTRIADIstanaExploreV5DTreeGeometrySourceAnchor>&
            InOrderedAnchors,
        FString& OutError)
{
    if (!RuntimeCompiledTrustAnchorsConfigured())
    {
        OutError = TEXT("Tree geometry runtime configuration is deliberately unavailable until both compiled trust anchors are pinned by a reviewed source change.");
        return false;
    }
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0))
    {
        OutError = TEXT("Tree geometry actor must remain at identity for world-space transform copies.");
        return false;
    }
    FTRIADIstanaExploreV5DTreeGeometryVariationLayout NewLayout;
    if (!ValidateAssetRoster(InAssets, OutError) ||
        !BuildDeterministicLayout(InOrderedAnchors, NewLayout, OutError))
    {
        return false;
    }
    ClearOwnedInstances();
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(NewLayout);
    bConfigured = true;
    bPresentationActivated = false;
    bExactSourcePresentationSuppressionProven = false;
    AcceptedR33ReceiptSha256.Reset();
    FutureTransactionReceiptSha256.Reset();
    bNativeVisualAcceptanceProvided = false;
    bSourceTransformsOrGeographyModified = false;
    bCollisionNavigationLosRfSensorOrTerrainAuthority = false;
    Tags.AddUnique(ExpectedActorTag());
    SetActorEnableCollision(false);
    if (!PopulateComponents(OutError))
    {
        ClearOwnedInstances();
        bConfigured = false;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ActivatePresentationAfterExactSourceSuppression(
        const FString& InAcceptedR33ReceiptSha256,
        const FString& InFutureTransactionReceiptSha256,
        bool bInExactSourcePresentationSuppressionProven,
        FString& OutError)
{
    if (!RuntimeCompiledTrustAnchorsConfigured() || !bConfigured ||
        bPresentationActivated ||
        !IsSha256(InAcceptedR33ReceiptSha256) ||
        !IsSha256(InFutureTransactionReceiptSha256) ||
        !InAcceptedR33ReceiptSha256.Equals(
            TrustedRuntimeAcceptedR33ReceiptSha256,
            ESearchCase::IgnoreCase) ||
        !InFutureTransactionReceiptSha256.Equals(
            TrustedRuntimeFutureAuthorizationSha256,
            ESearchCase::IgnoreCase) ||
        !bInExactSourcePresentationSuppressionProven)
    {
        OutError = TEXT("Tree geometry activation requires a configured cold actor, hash-pinned accepted R33 and future transaction receipts, and explicit exact source-presentation suppression proof.");
        return false;
    }
    bExactSourcePresentationSuppressionProven = true;
    AcceptedR33ReceiptSha256 =
        TrustedRuntimeAcceptedR33ReceiptSha256.ToUpper();
    FutureTransactionReceiptSha256 =
        TrustedRuntimeFutureAuthorizationSha256.ToUpper();
    bPresentationActivated = true;
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         VariantComponents)
    {
        if (Component)
        {
            Component->SetHiddenInGame(false, true);
            Component->SetVisibility(true, true);
        }
    }
    FString Report;
    if (!ValidateGeometryVariation(Report))
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
        bPresentationActivated = false;
        bExactSourcePresentationSuppressionProven = false;
        AcceptedR33ReceiptSha256.Reset();
        FutureTransactionReceiptSha256.Reset();
        OutError = TEXT("Tree geometry activation failed closed: ") + Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeGeometryVariationActor::
    ValidateGeometryVariation(FString& OutReport) const
{
    FString Error;
    if (!bConfigured || !RuntimeCompiledTrustAnchorsConfigured() ||
        !ValidateAssetRoster(SavedAssets, Error) ||
        !ValidateLayoutInternal(SavedLayout, Error) ||
        VariantComponents.Num() != VariantMeshCount ||
        !GetActorTransform().Equals(FTransform::Identity, 0.0) ||
        GetActorEnableCollision() || !Tags.Contains(ExpectedActorTag()) ||
        bNativeVisualAcceptanceProvided ||
        bSourceTransformsOrGeographyModified ||
        bCollisionNavigationLosRfSensorOrTerrainAuthority ||
        bPresentationActivated !=
            bExactSourcePresentationSuppressionProven ||
        (bPresentationActivated != IsSha256(
             AcceptedR33ReceiptSha256)) ||
        (bPresentationActivated != IsSha256(
             FutureTransactionReceiptSha256)) ||
        (bPresentationActivated &&
         (!AcceptedR33ReceiptSha256.Equals(
              TrustedRuntimeAcceptedR33ReceiptSha256,
              ESearchCase::IgnoreCase) ||
          !FutureTransactionReceiptSha256.Equals(
              TrustedRuntimeFutureAuthorizationSha256,
              ESearchCase::IgnoreCase))))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_INVALID_TRUTH_OR_LAYOUT: ") +
            Error;
        return false;
    }
    for (int32 Index = 0; Index < VariantMeshCount; ++Index)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            VariantComponents[Index];
        if (!HasRenderOnlyPolicy(Component) ||
            !ComponentMatches(
                Component,
                SavedAssets.VariantMeshes[Index],
                SavedLayout.VariantBuckets[Index].WorldTransforms,
                bPresentationActivated))
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_TREE_GEOMETRY_VARIATION_INVALID_COMPONENT_%d"),
                Index);
            return false;
        }
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_TREE_GEOMETRY_VARIATION_VALID sourceOnlyScaffold=true postR33=true numberedSuccessorAssigned=false variants=15 sourceAnchors=736 transformsCopiedWithoutMutation=true sourceGeographyModified=false collision=false navigation=false losAuthority=false rfAuthority=false sensorAuthority=false terrainAuthority=false nativeMeshGenerationProven=false nativeRuntimeIntegrationProven=false nativeVisualAcceptanceProvided=false presentationActivated=%s"),
        bPresentationActivated ? TEXT("true") : TEXT("false"));
    return true;
}
