#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

namespace
{
constexpr int32 SourceAnchorCount = 6;
constexpr int32 VariantCount = 3;
constexpr int32 LodCount = 3;
constexpr int32 MaterialCount = 3;
constexpr int32 TreeCullStartDistanceCm = 18000;
constexpr int32 TreeCullEndDistanceCm = 80000;
constexpr float TreeLodDistanceScale = 1.0f;

// Independent compile gates. Both remain false even if somebody supplies
// plausible receipt strings; a reviewed source edit and recompile is required.
constexpr bool bRuntimeSelectionCompiled = false;
constexpr bool bRuntimeActivationCompiled = false;
static_assert(!bRuntimeSelectionCompiled);
static_assert(!bRuntimeActivationCompiled);

const FString TrustedRuntimeAcceptedCurrentReceiptSha256(
    TEXT("UNSET_RUNTIME_CURRENT_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedRuntimeFutureAuthorizationSha256(
    TEXT("UNSET_RUNTIME_FUTURE_TRUST_ANCHOR_REQUIRES_SEPARATE_REVIEWED_SOURCE_CHANGE"));
const FName ActorTag(
    TEXT("TRIAD_IstanaExploreV5D_TropicalUmbrellaHero_PostR33_Dormant"));

const TCHAR* const SourceKeys[SourceAnchorCount] = {
    TEXT("v4.heritage.HT2003-108"),
    TEXT("v4.heritage.HT2008-169"),
    TEXT("v4.heritage.HT2018-298"),
    TEXT("v4.heritage.HT2019-306"),
    TEXT("r29.landmark.00"),
    TEXT("r29.landmark.05")};
const int32 SourceIndices[SourceAnchorCount] = {2, 6, 7, 8, 0, 5};
const ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain
    SourceDomains[SourceAnchorCount] = {
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::V4Heritage,
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::V4Heritage,
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::V4Heritage,
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::V4Heritage,
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::R29Landmark,
        ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::R29Landmark};
const TCHAR VariantSelectors[SourceAnchorCount] = {
    TEXT('C'), TEXT('B'), TEXT('B'), TEXT('B'), TEXT('A'), TEXT('B')};
static_assert(UE_ARRAY_COUNT(SourceKeys) == SourceAnchorCount);
static_assert(UE_ARRAY_COUNT(SourceIndices) == SourceAnchorCount);
static_assert(UE_ARRAY_COUNT(SourceDomains) == SourceAnchorCount);
static_assert(UE_ARRAY_COUNT(VariantSelectors) == SourceAnchorCount);

const TCHAR* const CandidateMeshObjectPaths[VariantCount] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/Meshes/SM_IPV5D_TropicalUmbrellaHero_A.SM_IPV5D_TropicalUmbrellaHero_A"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/Meshes/SM_IPV5D_TropicalUmbrellaHero_B.SM_IPV5D_TropicalUmbrellaHero_B"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/Meshes/SM_IPV5D_TropicalUmbrellaHero_C.SM_IPV5D_TropicalUmbrellaHero_C")};
const TCHAR* const CandidateMaterialObjectPaths[MaterialCount] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/Materials/M_IPV5D_TropicalUmbrella_Bark.M_IPV5D_TropicalUmbrella_Bark"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/Materials/M_IPV5D_TropicalUmbrella_LeafLive.M_IPV5D_TropicalUmbrella_LeafLive"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/Materials/M_IPV5D_TropicalUmbrella_LeafDry.M_IPV5D_TropicalUmbrella_LeafDry")};
const TCHAR* const CandidateSlotNames[MaterialCount] = {
    TEXT("Bark"), TEXT("LeafLive"), TEXT("LeafDry")};

const TCHAR* const ExistingFallbackMeshPath =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0");
const TCHAR* const ExistingFallbackMaterialObjectPaths[MaterialCount] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Trunk_Response.M_IPV5D_Tree_Umbrella_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Leaves_Response.M_IPV5D_Tree_Umbrella_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Branches_Response.M_IPV5D_Tree_Umbrella_Branches_Response")};
const TCHAR* const ExistingFallbackSlotNames[MaterialCount] = {
    TEXT("island_tree_02"),
    TEXT("island_tree_02_leaves"),
    TEXT("island_tree_02_branches")};

const int32 ExpectedTriangles[VariantCount][LodCount] = {
    {54828, 21352, 4776},
    {54828, 21352, 4776},
    {54828, 21352, 4776}};
const int32 ExpectedTrianglesByMaterial[VariantCount][LodCount][MaterialCount] = {
    {{18540, 35544, 744}, {7912, 13104, 336}, {2088, 2648, 40}},
    {{18540, 35584, 704}, {7912, 13184, 256}, {2088, 2640, 48}},
    {{18540, 35392, 896}, {7912, 13144, 296}, {2088, 2632, 56}}};

// Legacy UE OBJ import converts source metres (X,Y,Z) to centimetres
// (X,-Y,Z). These bounds are pins, not placement/geospatial authority.
const FVector3f ExpectedBoundsMinCm[VariantCount][LodCount] = {
    {FVector3f(-1153.7861f, -1028.6197f, 0.0f), FVector3f(-1173.6045f, -994.5727f, 0.0f), FVector3f(-979.6528f, -959.4625f, 0.0f)},
    {FVector3f(-1172.4090f, -1280.5380f, 0.0f), FVector3f(-1116.5429f, -1227.4144f, 0.0f), FVector3f(-1117.9435f, -1140.6434f, 0.0f)},
    {FVector3f(-1294.6117f, -1028.4023f, 0.0f), FVector3f(-1310.5309f, -1084.1193f, 0.0f), FVector3f(-1223.2171f, -991.3024f, 0.0f)}};
const FVector3f ExpectedBoundsMaxCm[VariantCount][LodCount] = {
    {FVector3f(1360.6162f, 1024.7927f, 1208.6435f), FVector3f(1347.8084f, 987.0468f, 1225.2170f), FVector3f(1357.0960f, 826.2744f, 1184.0472f)},
    {FVector3f(1074.8558f, 1200.1115f, 1374.9297f), FVector3f(1038.6270f, 1159.2829f, 1336.1616f), FVector3f(817.4451f, 926.1799f, 1283.4635f)},
    {FVector3f(1260.0605f, 999.4995f, 1139.1916f), FVector3f(1170.2789f, 1060.1997f, 1148.6171f), FVector3f(1145.5205f, 772.1522f, 1090.9971f)}};

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

bool IsFinitePositiveTransform(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return !Transform.ContainsNaN() && Transform.GetRotation().IsNormalized() &&
        Scale.X > UE_SMALL_NUMBER && Scale.Y > UE_SMALL_NUMBER &&
        Scale.Z > UE_SMALL_NUMBER;
}

FBox3f PositionBounds(const FStaticMeshLODResources& Resources)
{
    FBox3f Bounds(ForceInit);
    const FPositionVertexBuffer& Positions =
        Resources.VertexBuffers.PositionVertexBuffer;
    for (uint32 Index = 0; Index < Positions.GetNumVertices(); ++Index)
    {
        Bounds += Positions.VertexPosition(Index);
    }
    return Bounds;
}

bool ValidateCandidateMesh(
    const UStaticMesh* Mesh,
    int32 Variant,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Mesh || Variant < 0 || Variant >= VariantCount ||
        Mesh->GetPathName() != CandidateMeshObjectPaths[Variant] ||
        Mesh->GetNumLODs() != LodCount || !RenderData ||
        RenderData->LODResources.Num() != LodCount ||
        Mesh->GetStaticMaterials().Num() != MaterialCount ||
        (Body && (Body->AggGeom.GetElementCount() != 0 ||
                  Body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)))
    {
        OutError = TEXT("Tropical umbrella candidate mesh lost exact path, three-LOD/slot, or render-only collision policy.");
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Slot];
        const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
        if (Binding.MaterialSlotName != FName(CandidateSlotNames[Slot]) ||
            Binding.ImportedMaterialSlotName !=
                FName(CandidateSlotNames[Slot]) ||
            !Material ||
            Material->GetPathName() != CandidateMaterialObjectPaths[Slot])
        {
            OutError = TEXT("Tropical umbrella Bark/LeafLive/LeafDry material route drifted.");
            return false;
        }
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        const FStaticMeshLODResources& Resources = RenderData->LODResources[Lod];
        const FBox3f Bounds = PositionBounds(Resources);
        int32 TrianglesByMaterial[MaterialCount] = {0, 0, 0};
        for (const FStaticMeshSection& Section : Resources.Sections)
        {
            if (Section.MaterialIndex < 0 ||
                Section.MaterialIndex >= MaterialCount)
            {
                OutError = TEXT("Tropical umbrella LOD section references an unknown material slot.");
                return false;
            }
            TrianglesByMaterial[Section.MaterialIndex] +=
                Section.NumTriangles;
        }
        if (Resources.GetNumTriangles() != ExpectedTriangles[Variant][Lod] ||
            Resources.Sections.Num() != MaterialCount || !Bounds.IsValid ||
            !Bounds.Min.Equals(ExpectedBoundsMinCm[Variant][Lod], 0.05f) ||
            !Bounds.Max.Equals(ExpectedBoundsMaxCm[Variant][Lod], 0.05f))
        {
            OutError = TEXT("Tropical umbrella LOD triangles, sections, root plane, or converted bounds drifted.");
            return false;
        }
        for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
        {
            if (TrianglesByMaterial[Slot] !=
                ExpectedTrianglesByMaterial[Variant][Lod][Slot])
            {
                OutError = TEXT("Tropical umbrella LOD material triangle routing drifted.");
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateFallback(
    const FTRIADIstanaExploreV5DTropicalUmbrellaAssets& Assets,
    FString& OutError)
{
    const UStaticMesh* Mesh = Assets.ExistingTreeRealismUmbrellaFallback;
    if (!Mesh || Mesh->GetPathName() != ExistingFallbackMeshPath ||
        Assets.ExistingFallbackMaterials.Num() != MaterialCount ||
        Assets.ExistingFallbackMaterials.Contains(nullptr) ||
        Mesh->GetStaticMaterials().Num() != MaterialCount)
    {
        OutError = TEXT("Exact TreeRealism umbrella mesh/material fallback is mandatory.");
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Slot];
        const UMaterialInterface* Material =
            Assets.ExistingFallbackMaterials[Slot];
        if (Binding.MaterialSlotName != FName(ExistingFallbackSlotNames[Slot]) ||
            Binding.ImportedMaterialSlotName !=
                FName(ExistingFallbackSlotNames[Slot]) ||
            !Material ||
            Mesh->GetMaterial(Slot) != Material ||
            Material->GetPathName() !=
                ExistingFallbackMaterialObjectPaths[Slot])
        {
            OutError = TEXT("Exact TreeRealism umbrella fallback material semantics drifted.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void ConfigureDormantRenderOnly(
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
    Component->SetLODDistanceScale(TreeLodDistanceScale);
    Component->ComponentTags.AddUnique(ActorTag);
    Component->SetVisibility(false, true);
    Component->SetHiddenInGame(true, true);
}

bool HasDormantRenderOnlyPolicy(
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
        !Component->CanEverAffectNavigation() &&
        CullStart == TreeCullStartDistanceCm &&
        CullEnd == TreeCullEndDistanceCm &&
        Component->GetLODDistanceScale() == TreeLodDistanceScale &&
        Component->ComponentTags.Contains(ActorTag);
}
} // namespace

int32 FTRIADIstanaExploreV5DTropicalUmbrellaLayout::TotalInstances() const
{
    int32 Total = 0;
    for (const FTRIADIstanaExploreV5DTropicalUmbrellaBucket& Bucket :
         VariantBuckets)
    {
        Total += Bucket.WorldTransforms.Num();
    }
    return Total;
}

ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);
    SetActorEnableCollision(false);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    CandidateComponents.Reserve(VariantCount);
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                FName(*FString::Printf(
                    TEXT("TropicalUmbrellaVariant_%c"),
                    TEXT('A') + Variant)));
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureDormantRenderOnly(Component);
        CandidateComponents.Add(Component);
    }
}

FString ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExpectedSourceInstanceKey(int32 IntegrationOrdinal)
{
    return IntegrationOrdinal >= 0 && IntegrationOrdinal < SourceAnchorCount
        ? SourceKeys[IntegrationOrdinal]
        : FString();
}

int32 ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExpectedSourceIndex(int32 IntegrationOrdinal)
{
    return IntegrationOrdinal >= 0 && IntegrationOrdinal < SourceAnchorCount
        ? SourceIndices[IntegrationOrdinal]
        : INDEX_NONE;
}

ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain
ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::ExpectedSourceDomain(
    int32 IntegrationOrdinal)
{
    return IntegrationOrdinal >= 0 && IntegrationOrdinal < SourceAnchorCount
        ? SourceDomains[IntegrationOrdinal]
        : ETRIADIstanaExploreV5DTropicalUmbrellaSourceDomain::V4Heritage;
}

TCHAR ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExpectedVariantSelector(int32 IntegrationOrdinal)
{
    return IntegrationOrdinal >= 0 && IntegrationOrdinal < SourceAnchorCount
        ? VariantSelectors[IntegrationOrdinal]
        : TEXT('\0');
}

FString ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    CandidateMeshObjectPath(int32 VariantIndex)
{
    return VariantIndex >= 0 && VariantIndex < VariantCount
        ? CandidateMeshObjectPaths[VariantIndex]
        : FString();
}

FString ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    CandidateMaterialObjectPath(int32 MaterialIndex)
{
    return MaterialIndex >= 0 && MaterialIndex < MaterialCount
        ? CandidateMaterialObjectPaths[MaterialIndex]
        : FString();
}

FString ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExistingFallbackMeshObjectPath()
{
    return ExistingFallbackMeshPath;
}

FString ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExistingFallbackMaterialObjectPath(int32 MaterialIndex)
{
    return MaterialIndex >= 0 && MaterialIndex < MaterialCount
        ? ExistingFallbackMaterialObjectPaths[MaterialIndex]
        : FString();
}

int32 ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExpectedSourceAnchorCount()
{
    return SourceAnchorCount;
}

int32 ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::ExpectedVariantCount()
{
    return VariantCount;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    RuntimeSelectionCompiled()
{
    return bRuntimeSelectionCompiled;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    RuntimeActivationCompiled()
{
    return bRuntimeActivationCompiled;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    RuntimeTrustAnchorsConfigured()
{
    return IsSha256(TrustedRuntimeAcceptedCurrentReceiptSha256) &&
        IsSha256(TrustedRuntimeFutureAuthorizationSha256) &&
        !TrustedRuntimeAcceptedCurrentReceiptSha256.Equals(
            TrustedRuntimeFutureAuthorizationSha256,
            ESearchCase::IgnoreCase);
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ExactSourceTransformBitsMatch(
        const FTransform& Actual,
        const FTransform& Expected)
{
    return ExactTransformValue(Actual, Expected);
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    BuildDeterministicLayout(
        const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
            InOrderedNativeAnchors,
        FTRIADIstanaExploreV5DTropicalUmbrellaLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DTropicalUmbrellaLayout{};
    if (InOrderedNativeAnchors.Num() != SourceAnchorCount)
    {
        OutError = TEXT("Tropical umbrella selector requires exactly six existing native-classified source anchors.");
        return false;
    }
    OutLayout.VariantBuckets.SetNum(VariantCount);
    OutLayout.OrderedSourceInstanceKeys.Reserve(SourceAnchorCount);
    OutLayout.OrderedSourceWorldTransforms.Reserve(SourceAnchorCount);
    OutLayout.VariantIndexBySourceOrdinal.Reserve(SourceAnchorCount);
    for (int32 Ordinal = 0; Ordinal < SourceAnchorCount; ++Ordinal)
    {
        const FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor& Anchor =
            InOrderedNativeAnchors[Ordinal];
        if (Anchor.SourceInstanceKey != SourceKeys[Ordinal] ||
            Anchor.SourceDomain != SourceDomains[Ordinal] ||
            Anchor.SourceIndex != SourceIndices[Ordinal] ||
            !IsFinitePositiveTransform(Anchor.SourceWorldTransform))
        {
            OutLayout = FTRIADIstanaExploreV5DTropicalUmbrellaLayout{};
            OutError = FString::Printf(
                TEXT("Tropical umbrella source row %d changed exact identity, native umbrella classification, or transform validity."),
                Ordinal);
            return false;
        }
        const int32 VariantIndex =
            VariantSelectors[Ordinal] - TEXT('A');
        if (VariantIndex < 0 || VariantIndex >= VariantCount)
        {
            OutLayout = FTRIADIstanaExploreV5DTropicalUmbrellaLayout{};
            OutError = TEXT("Tropical umbrella selector contains an invalid variant.");
            return false;
        }
        // Intentional FTransform value copy. No decomposition, coordinate
        // conversion, reconstruction, jitter, scale change, or new placement.
        const FTransform ExactCopy = Anchor.SourceWorldTransform;
        OutLayout.OrderedSourceInstanceKeys.Add(Anchor.SourceInstanceKey);
        OutLayout.OrderedSourceWorldTransforms.Add(ExactCopy);
        OutLayout.VariantIndexBySourceOrdinal.Add(VariantIndex);
        OutLayout.VariantBuckets[VariantIndex].SourceInstanceKeys.Add(
            Anchor.SourceInstanceKey);
        OutLayout.VariantBuckets[VariantIndex].WorldTransforms.Add(ExactCopy);
        if (!ExactTransformValue(ExactCopy, Anchor.SourceWorldTransform))
        {
            OutLayout = FTRIADIstanaExploreV5DTropicalUmbrellaLayout{};
            OutError = TEXT("Tropical umbrella source transform copy was not bit-exact.");
            return false;
        }
    }
    if (OutLayout.TotalInstances() != SourceAnchorCount ||
        OutLayout.VariantBuckets[0].WorldTransforms.Num() != 1 ||
        OutLayout.VariantBuckets[1].WorldTransforms.Num() != 4 ||
        OutLayout.VariantBuckets[2].WorldTransforms.Num() != 1)
    {
        OutLayout = FTRIADIstanaExploreV5DTropicalUmbrellaLayout{};
        OutError = TEXT("Tropical umbrella selector distribution must remain A=1, B=4, C=1.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DTropicalUmbrellaAssets& Assets,
    FString& OutError)
{
    if (Assets.CandidateVariantMeshes.Num() != VariantCount ||
        Assets.CandidateVariantMeshes.Contains(nullptr) ||
        !ValidateFallback(Assets, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Tropical umbrella assets require variants A/B/C and the exact TreeRealism fallback.");
        }
        return false;
    }
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        if (!ValidateCandidateMesh(
                Assets.CandidateVariantMeshes[Variant],
                Variant,
                OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ClearAndHideOwnedPresentation()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         CandidateComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
            Component->SetStaticMesh(nullptr);
            Component->SetVisibility(false, true);
            Component->SetHiddenInGame(true, true);
        }
    }
}

void ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    RollbackPresentationToMandatoryFallbackInternal()
{
    ClearAndHideOwnedPresentation();
    SavedAssets = FTRIADIstanaExploreV5DTropicalUmbrellaAssets{};
    SavedLayout = FTRIADIstanaExploreV5DTropicalUmbrellaLayout{};
    bCandidateSelectionConfigured = false;
    bPresentationActivated = false;
    bExactSourcePresentationSuppressionProven = false;
    bExistingTreeRealismMeshAndMaterialsFallbackPreserved = true;
    bSourceKeysTransformsOrCensusModified = false;
    bGeospatialCollisionNavigationLosRfSensorOrTerrainAuthority = false;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ConfigureCandidateSelectionInternal(
        const FTRIADIstanaExploreV5DTropicalUmbrellaAssets& InAssets,
        const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
            InOrderedNativeAnchors,
        const FString& InAcceptedCurrentReceiptSha256,
        const FString& InFutureAuthorizationReceiptSha256,
        FString& OutError)
{
    if (!bRuntimeSelectionCompiled || !RuntimeTrustAnchorsConfigured() ||
        !InAcceptedCurrentReceiptSha256.Equals(
            TrustedRuntimeAcceptedCurrentReceiptSha256,
            ESearchCase::IgnoreCase) ||
        !InFutureAuthorizationReceiptSha256.Equals(
            TrustedRuntimeFutureAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Tropical umbrella runtime selection is independently compiled false and its distinct trust anchors are unset.");
        return false;
    }
    FTRIADIstanaExploreV5DTropicalUmbrellaLayout NewLayout;
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0) ||
        !ValidateAssetRoster(InAssets, OutError) ||
        !BuildDeterministicLayout(
            InOrderedNativeAnchors,
            NewLayout,
            OutError))
    {
        return false;
    }
    ClearAndHideOwnedPresentation();
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(NewLayout);
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CandidateComponents[Variant];
        if (!Component || !HasDormantRenderOnlyPolicy(Component))
        {
            ClearAndHideOwnedPresentation();
            OutError = TEXT("Tropical umbrella component lost its render-only policy.");
            return false;
        }
        Component->SetStaticMesh(InAssets.CandidateVariantMeshes[Variant]);
        Component->AddInstances(
            SavedLayout.VariantBuckets[Variant].WorldTransforms,
            false,
            true,
            false);
        Component->BuildTreeIfOutdated(false, true);
    }
    bCandidateSelectionConfigured = true;
    bPresentationActivated = false;
    bExactSourcePresentationSuppressionProven = false;
    bExistingTreeRealismMeshAndMaterialsFallbackPreserved = true;
    bSourceKeysTransformsOrCensusModified = false;
    bGeospatialCollisionNavigationLosRfSensorOrTerrainAuthority = false;
    Tags.AddUnique(ActorTag);
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ActivateAfterAtomicExactSourceSuppressionInternal(
        const FString& InAcceptedCurrentReceiptSha256,
        const FString& InFutureAuthorizationReceiptSha256,
        bool bInExactSixSourceVisualsSuppressed,
        FString& OutError)
{
    if (!bRuntimeActivationCompiled || !bRuntimeSelectionCompiled ||
        !RuntimeTrustAnchorsConfigured() || !bCandidateSelectionConfigured ||
        bPresentationActivated || !bInExactSixSourceVisualsSuppressed ||
        !InAcceptedCurrentReceiptSha256.Equals(
            TrustedRuntimeAcceptedCurrentReceiptSha256,
            ESearchCase::IgnoreCase) ||
        !InFutureAuthorizationReceiptSha256.Equals(
            TrustedRuntimeFutureAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Tropical umbrella activation is independently compiled false and requires future atomic suppression proof for exactly six source visuals.");
        return false;
    }
    bExactSourcePresentationSuppressionProven = true;
    bPresentationActivated = true;
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         CandidateComponents)
    {
        if (Component)
        {
            Component->SetHiddenInGame(false, true);
            Component->SetVisibility(true, true);
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
    ValidateDormantScaffold(FString& OutReport) const
{
    if (RuntimeSelectionCompiled() || RuntimeActivationCompiled() ||
        RuntimeTrustAnchorsConfigured() || bCandidateSelectionConfigured ||
        bPresentationActivated || bExactSourcePresentationSuppressionProven ||
        !bExistingTreeRealismMeshAndMaterialsFallbackPreserved ||
        bSourceKeysTransformsOrCensusModified ||
        bGeospatialCollisionNavigationLosRfSensorOrTerrainAuthority ||
        GetActorEnableCollision() || CandidateComponents.Num() != VariantCount)
    {
        OutReport = TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_DORMANT_SCAFFOLD_INVALID_TRUTH");
        return false;
    }
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         CandidateComponents)
    {
        if (!HasDormantRenderOnlyPolicy(Component) ||
            Component->GetStaticMesh() || Component->GetInstanceCount() != 0 ||
            Component->IsVisible() || !Component->bHiddenInGame)
        {
            OutReport = TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_DORMANT_COMPONENT_NOT_EMPTY_HIDDEN_RENDER_ONLY");
            return false;
        }
    }
    OutReport = TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_DORMANT_SCAFFOLD_VALID sourceIdentities=6 selector=A1_B4_C1 transformsCaptured=false newPlacements=0 deletedPlacements=0 selectionCompiled=false activationCompiled=false candidateVisible=false fallbackPreserved=true mapModified=false geospatialAuthority=false collision=false navigation=false losAuthority=false rfAuthority=false sensorAuthority=false terrainAuthority=false nativeCompileImportRuntimeCapturePerformanceOrHumanAcceptanceClaimed=false");
    return true;
}
