#include "TRIADIstanaExploreV5DPalmHeroSourceLibrary.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

namespace
{
constexpr int32 LodCount = 3;
constexpr int32 MaterialCount = 3;
constexpr int32 TextureCount = 4;
const int32 ExpectedTriangles[] = {104244, 37968, 9864};
const int32 ExpectedTrianglesByMaterial[LodCount][MaterialCount] = {
    {5984, 90832, 7428},
    {2400, 33308, 2260},
    {880, 8576, 408}};
const FVector3f ExpectedBoundsMinMeters[] = {
    FVector3f(-4.588270f, -5.099290f, 0.0f),
    FVector3f(-4.524105f, -4.885492f, 0.0f),
    FVector3f(-4.711650f, -4.809668f, 0.0f)};
const FVector3f ExpectedBoundsMaxMeters[] = {
    FVector3f(7.059138f, 6.303532f, 17.793116f),
    FVector3f(6.948630f, 6.108454f, 17.764737f),
    FVector3f(6.798521f, 6.397235f, 17.741958f)};
const TCHAR* const MaterialSlotNames[] = {
    TEXT("Bark"), TEXT("FrondLive"), TEXT("FrondDry")};
const TCHAR* const MaterialObjectPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Materials/M_IPV5D_PalmHero_Bark.M_IPV5D_PalmHero_Bark"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Materials/M_IPV5D_PalmHero_FrondLive.M_IPV5D_PalmHero_FrondLive"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Materials/M_IPV5D_PalmHero_FrondDry.M_IPV5D_PalmHero_FrondDry")};
const TCHAR* const TextureObjectPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Textures/T_IPV5D_PalmHero_Bark_BaseColor.T_IPV5D_PalmHero_Bark_BaseColor"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Textures/T_IPV5D_PalmHero_Bark_NormalDX.T_IPV5D_PalmHero_Bark_NormalDX"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Textures/T_IPV5D_PalmHero_Bark_Roughness.T_IPV5D_PalmHero_Bark_Roughness"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Textures/T_IPV5D_PalmHero_Bark_AmbientOcclusion.T_IPV5D_PalmHero_Bark_AmbientOcclusion")};
const TCHAR* const MeshPath =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Meshes/SM_IPV5D_PalmHero.SM_IPV5D_PalmHero");
const TCHAR* const ExistingPalmPath =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0.SM_IPV5D_Tree_Palm_NearLOD0");
static_assert(UE_ARRAY_COUNT(ExpectedTriangles) == LodCount);
static_assert(UE_ARRAY_COUNT(MaterialSlotNames) == MaterialCount);
static_assert(UE_ARRAY_COUNT(MaterialObjectPaths) == MaterialCount);
static_assert(UE_ARRAY_COUNT(TextureObjectPaths) == TextureCount);

bool NearCentimetres(const FVector3f& Actual, const FVector3f& Metres)
{
    return Actual.Equals(Metres * 100.0f, 0.05f);
}

bool HasEditorNaniteEnabled(const UStaticMesh* Mesh)
{
#if WITH_EDITORONLY_DATA
    return Mesh && Mesh->IsNaniteEnabled();
#else
    // The source-materialization endpoint independently validates this before
    // saving. UStaticMesh::IsNaniteEnabled is editor-only in UE 5.5.
    return false;
#endif
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

bool ValidateMesh(
    const UStaticMesh* Mesh,
    const TArray<TObjectPtr<UMaterialInterface>>& Materials,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Mesh || Mesh->GetPathName() != MeshPath || !RenderData ||
        RenderData->LODResources.Num() != LodCount ||
        Mesh->GetNumLODs() != LodCount || HasEditorNaniteEnabled(Mesh) ||
        Mesh->GetStaticMaterials().Num() != MaterialCount ||
        Materials.Num() != MaterialCount || Materials.Contains(nullptr) ||
        (Body && (Body->AggGeom.GetElementCount() != 0 ||
                  Body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)))
    {
        OutError = TEXT("PalmHero mesh lost its exact render-only three-LOD/three-slot policy.");
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Slot];
        if (Binding.MaterialSlotName != FName(MaterialSlotNames[Slot]) ||
            Binding.ImportedMaterialSlotName !=
                FName(MaterialSlotNames[Slot]) ||
            Mesh->GetMaterial(Slot) != Materials[Slot] ||
            Materials[Slot]->GetPathName() != MaterialObjectPaths[Slot])
        {
            OutError = TEXT("PalmHero Bark/FrondLive/FrondDry material route drifted.");
            return false;
        }
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        const FStaticMeshLODResources& Resources =
            RenderData->LODResources[Lod];
        const FBox3f Bounds = PositionBounds(Resources);
        if (Resources.GetNumTriangles() != ExpectedTriangles[Lod] ||
            Resources.Sections.Num() != MaterialCount ||
            !Bounds.IsValid ||
            !NearCentimetres(Bounds.Min, ExpectedBoundsMinMeters[Lod]) ||
            !NearCentimetres(Bounds.Max, ExpectedBoundsMaxMeters[Lod]))
        {
            OutError = FString::Printf(
                TEXT("PalmHero LOD%d triangle, section, or source bounds contract drifted."),
                Lod);
            return false;
        }
        int32 TrianglesByMaterial[MaterialCount] = {0, 0, 0};
        for (const FStaticMeshSection& Section : Resources.Sections)
        {
            if (Section.MaterialIndex < 0 ||
                Section.MaterialIndex >= MaterialCount)
            {
                OutError = TEXT("PalmHero LOD section references an unknown material slot.");
                return false;
            }
            TrianglesByMaterial[Section.MaterialIndex] +=
                Section.NumTriangles;
        }
        for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
        {
            if (TrianglesByMaterial[Slot] !=
                ExpectedTrianglesByMaterial[Lod][Slot])
            {
                OutError = FString::Printf(
                    TEXT("PalmHero LOD%d material triangle route drifted at slot %d."),
                    Lod,
                    Slot);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DPalmHeroAssets& Assets,
    FString& OutError)
{
    if (Assets.BarkTextures.Num() != TextureCount ||
        Assets.BarkTextures.Contains(nullptr))
    {
        OutError = TEXT("PalmHero requires all four provenance-pinned bark textures.");
        return false;
    }
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        const UTexture2D* Texture = Assets.BarkTextures[Index];
        const bool bExpectedSrgb = Index == 0;
        const TextureCompressionSettings ExpectedCompression = Index == 0
            ? TC_Default
            : (Index == 1 ? TC_Normalmap : TC_Masks);
        if (!Texture || Texture->GetPathName() != TextureObjectPaths[Index] ||
            Texture->GetSizeX() != 2048 || Texture->GetSizeY() != 4096 ||
            Texture->SRGB != bExpectedSrgb ||
            Texture->CompressionSettings != ExpectedCompression ||
            Texture->VirtualTextureStreaming || Texture->NeverStream)
        {
            OutError = FString::Printf(
                TEXT("PalmHero bark texture %d lost its exact path, dimensions, or color/compression route."),
                Index);
            return false;
        }
    }
    return ValidateMesh(Assets.PalmMesh, Assets.Materials, OutError);
}

bool UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
    ResolveOptionalGeometryVariationPalmSource(
        UStaticMesh* ExistingTreeRealismPalmSource,
        const FTRIADIstanaExploreV5DPalmHeroAssets& PalmHeroAssets,
        bool bExplicitlySelectPalmHero,
        FTRIADIstanaExploreV5DOptionalPalmSource& OutSelection,
        FString& OutError)
{
    OutSelection = FTRIADIstanaExploreV5DOptionalPalmSource{};
    if (!ExistingTreeRealismPalmSource ||
        ExistingTreeRealismPalmSource->GetPathName() != ExistingPalmPath)
    {
        OutError = TEXT("The exact existing TreeRealism palm fallback is mandatory.");
        return false;
    }
    if (bExplicitlySelectPalmHero &&
        !ValidateAssetRoster(PalmHeroAssets, OutError))
    {
        return false;
    }
    OutSelection.SelectedSourceMesh = bExplicitlySelectPalmHero
        ? PalmHeroAssets.PalmMesh.Get()
        : ExistingTreeRealismPalmSource;
    OutSelection.bPalmHeroSelected = bExplicitlySelectPalmHero;
    OutSelection.bExistingPalmFallbackPreserved = true;
    OutSelection.bMapOrSourceTransformModified = false;
    OutSelection.bCollisionNavigationLosRfSensorOrTerrainAuthority = false;
    OutError.Reset();
    return true;
}

FString UTRIADIstanaExploreV5DPalmHeroSourceLibrary::MeshObjectPath()
{
    return MeshPath;
}

FString UTRIADIstanaExploreV5DPalmHeroSourceLibrary::MaterialObjectPath(
    int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < MaterialCount
        ? MaterialObjectPaths[SlotIndex]
        : FString();
}

FString UTRIADIstanaExploreV5DPalmHeroSourceLibrary::TextureObjectPath(
    int32 TextureIndex)
{
    return TextureIndex >= 0 && TextureIndex < TextureCount
        ? TextureObjectPaths[TextureIndex]
        : FString();
}

FString UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
    ExistingPalmFallbackObjectPath()
{
    return ExistingPalmPath;
}

int32 UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ExpectedLodCount()
{
    return LodCount;
}

int32 UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ExpectedMaterialCount()
{
    return MaterialCount;
}

int32 UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ExpectedTextureCount()
{
    return TextureCount;
}
