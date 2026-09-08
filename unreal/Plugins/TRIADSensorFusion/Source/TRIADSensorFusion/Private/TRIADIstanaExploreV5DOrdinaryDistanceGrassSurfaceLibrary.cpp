#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.h"

#include "Engine/Texture2D.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr int32 TextureCount = 5;
constexpr int32 OutputAssetCount = 6;
constexpr double TileMetres = 1.4;

const TCHAR* const TexturePaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_BaseColor.T_IPV5D_Grass001_BaseColor"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_NormalDX.T_IPV5D_Grass001_NormalDX"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_Roughness.T_IPV5D_Grass001_Roughness"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_AmbientOcclusion.T_IPV5D_Grass001_AmbientOcclusion"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_Height.T_IPV5D_Grass001_Height")};
const TCHAR* const MaterialPath =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Materials/M_IPV5D_OrdinaryDistanceGrassSurface.M_IPV5D_OrdinaryDistanceGrassSurface");
const TCHAR* const ExistingLawnPath =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_LawnMacroVariation.M_IPV5D_LawnMacroVariation");

static_assert(UE_ARRAY_COUNT(TexturePaths) == TextureCount);

bool HasExactEditorOnlyTexturePolicy(const UTexture2D* Texture)
{
#if WITH_EDITORONLY_DATA
    return Texture &&
        Texture->MipGenSettings == TMGS_FromTextureGroup &&
        !Texture->bFlipGreenChannel;
#else
    // These source/import settings are stripped from non-editor targets.
    // Their admitted values remain enforced by the editor materializer before
    // save/cook; packaged validation must use only runtime-visible state.
    return Texture != nullptr;
#endif
}

bool ValidateTextures(
    const TArray<TObjectPtr<UTexture2D>>& Textures,
    FString& OutError)
{
    if (Textures.Num() != TextureCount || Textures.Contains(nullptr))
    {
        OutError = TEXT("Ordinary-distance grass requires exactly five source textures.");
        return false;
    }
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        const UTexture2D* Texture = Textures[Index];
        const bool bExpectedSrgb = Index == 0;
        const TextureCompressionSettings ExpectedCompression = Index == 0
            ? TC_Default
            : (Index == 1 ? TC_Normalmap : TC_Masks);
        if (!Texture || Texture->GetPathName() != TexturePaths[Index] ||
            Texture->GetSizeX() != 2048 || Texture->GetSizeY() != 2048 ||
            Texture->SRGB != bExpectedSrgb ||
            Texture->CompressionSettings != ExpectedCompression ||
            Texture->LODGroup != TEXTUREGROUP_World ||
            !HasExactEditorOnlyTexturePolicy(Texture) ||
            Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
            Texture->Filter != TF_Default || Texture->VirtualTextureStreaming ||
            Texture->NeverStream)
        {
            OutError = FString::Printf(
                TEXT("Ordinary-distance grass texture %d lost its exact path, 2K dimensions, DirectX/color/compression, wrap, mip, or streaming policy."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool HasExactPresentationPropertyConnections(const UMaterial* Material)
{
    if (!Material || !Material->HasBaseColorConnected() ||
        !Material->HasRoughnessConnected() ||
        !Material->HasNormalConnected() ||
        !Material->HasAmbientOcclusionConnected())
    {
        return false;
    }
    const EMaterialProperty DeniedProperties[] = {
        MP_EmissiveColor,
        MP_Opacity,
        MP_OpacityMask,
        MP_Metallic,
        MP_Specular,
        MP_Anisotropy,
        MP_Tangent,
        MP_WorldPositionOffset,
        MP_SubsurfaceColor,
        MP_CustomData0,
        MP_CustomData1,
        MP_Refraction,
        MP_CustomizedUVs0,
        MP_CustomizedUVs1,
        MP_CustomizedUVs2,
        MP_CustomizedUVs3,
        MP_CustomizedUVs4,
        MP_CustomizedUVs5,
        MP_CustomizedUVs6,
        MP_CustomizedUVs7,
        MP_PixelDepthOffset,
        MP_ShadingModel,
        MP_FrontMaterial,
        MP_SurfaceThickness,
        MP_Displacement,
        MP_MaterialAttributes};
    for (const EMaterialProperty Property : DeniedProperties)
    {
        if (Material->IsPropertyConnected(Property))
        {
            return false;
        }
    }
    return true;
}

bool HasNoPhysicalOrNaniteAuthority(const UMaterial* Material)
{
    if (!Material || Material->PhysMaterial || Material->PhysMaterialMask ||
        !Material->RenderTracePhysicalMaterialOutputs.IsEmpty() ||
        Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial())
    {
        return false;
    }
    for (const TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Material->PhysicalMaterialMap)
    {
        if (PhysicalMaterial)
        {
            return false;
        }
    }
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    ValidateAssetRoster(
        const FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets&
            Assets,
        FString& OutError)
{
    UMaterial* Material = Cast<UMaterial>(Assets.SurfaceMaterial);
    if (!ValidateTextures(Assets.SourceTextures, OutError))
    {
        return false;
    }
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != MaterialPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->bScreenSpaceReflections ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        Material->MaxWorldPositionOffsetDisplacement != 0.0f ||
        !HasExactPresentationPropertyConnections(Material) ||
        !HasNoPhysicalOrNaniteAuthority(Material))
    {
        OutError = TEXT("Ordinary-distance grass material lost its exact opaque no-displacement appearance-only policy.");
        return false;
    }

    TSet<const UObject*> ExpectedTextures;
    for (UTexture2D* Texture : Assets.SourceTextures)
    {
        ExpectedTextures.Add(Texture);
    }
    TSet<const UObject*> ReferencedTextures;
    for (const TObjectPtr<UObject>& Object : Material->GetReferencedTextures())
    {
        if (Object)
        {
            ReferencedTextures.Add(Object.Get());
        }
    }
    if (ExpectedTextures.Num() != TextureCount ||
        ReferencedTextures.Num() != TextureCount ||
        ExpectedTextures.Difference(ReferencedTextures).Num() != 0 ||
        ReferencedTextures.Difference(ExpectedTextures).Num() != 0)
    {
        OutError = TEXT("Ordinary-distance grass material must reference exactly the five admitted source textures, including retained height.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    ResolveOptionalPresentationMaterial(
        UMaterialInterface* ExistingLawnSurfaceFallback,
        const FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets&
            CandidateAssets,
        bool bExplicitlySelectCandidate,
        FTRIADIstanaExploreV5DOptionalGrassSurfaceMaterial& OutSelection,
        FString& OutError)
{
    OutSelection = FTRIADIstanaExploreV5DOptionalGrassSurfaceMaterial{};
    if (!ExistingLawnSurfaceFallback ||
        ExistingLawnSurfaceFallback->GetPathName() != ExistingLawnPath)
    {
        OutError = TEXT("The exact admitted lawn-surface fallback is mandatory.");
        return false;
    }
    if (bExplicitlySelectCandidate &&
        !ValidateAssetRoster(CandidateAssets, OutError))
    {
        return false;
    }

    OutSelection.SelectedMaterial = bExplicitlySelectCandidate
        ? CandidateAssets.SurfaceMaterial.Get()
        : ExistingLawnSurfaceFallback;
    OutSelection.bCandidateSelected = bExplicitlySelectCandidate;
    OutSelection.bExistingLawnFallbackPreserved = true;
    OutSelection.bMapGeographyTerrainOrSourceTransformModified = false;
    OutSelection.bCollisionNavigationLosRfSensorOrSimulationAuthority = false;
    OutError.Reset();
    return true;
}

FString UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    MaterialObjectPath()
{
    return MaterialPath;
}

FString UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    TextureObjectPath(int32 TextureIndex)
{
    return TextureIndex >= 0 && TextureIndex < TextureCount
        ? TexturePaths[TextureIndex]
        : FString();
}

FString UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    ExistingLawnFallbackObjectPath()
{
    return ExistingLawnPath;
}

int32 UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    ExpectedTextureCount()
{
    return TextureCount;
}

int32 UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    ExpectedOutputAssetCount()
{
    return OutputAssetCount;
}

double UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
    ProviderTileMetres()
{
    return TileMetres;
}
