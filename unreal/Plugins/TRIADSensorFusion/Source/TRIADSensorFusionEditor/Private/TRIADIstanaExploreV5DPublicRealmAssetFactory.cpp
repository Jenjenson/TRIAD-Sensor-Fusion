#include "TRIADIstanaExploreV5DPublicRealmAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/MaterialFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "RHIFeatureLevel.h"
#include "ShaderCompiler.h"
#include "Ssl.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5CGroundContextAssetFactory.h"
#include "TRIADIstanaExploreV5DPublicRealmActor.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <cfloat>

#if WITH_SSL
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include <openssl/sha.h>
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#endif

namespace
{
const FString AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString VisualMaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/"
         "PublicRealmVisualR2/Materials"));
const FString CoreMeshName(TEXT("SM_IPV5D_PublicRealm_Core_Render"));
const FString FallbackMeshName(
    TEXT("SM_IPV5D_PublicRealm_Fallback_Render"));
const FString ConcreteMaterialName(TEXT("MI_IPV5D_PublicRealmConcrete"));
const FString AsphaltMaterialName(
    TEXT("M_IPV5D_PublicRealm_AsphaltDry_R2"));
const FString RoadGraphicSuppressionMaterialName(
    TEXT("M_IPV5D_PublicRealm_RoadGraphicFullyClipped_R2"));

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const FString CoreMeshObjectPath(ObjectPath(AssetRoot, CoreMeshName));
const FString FallbackMeshObjectPath(
    ObjectPath(AssetRoot, FallbackMeshName));
const FString ConcreteMaterialObjectPath(
    ObjectPath(MaterialRoot, ConcreteMaterialName));
const FString AsphaltMaterialObjectPath(
    ObjectPath(VisualMaterialRoot, AsphaltMaterialName));
const FString RoadGraphicSuppressionMaterialObjectPath(
    ObjectPath(VisualMaterialRoot, RoadGraphicSuppressionMaterialName));
const FString LegacyRoadBaseMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone"));
const FString LegacyRoadGraphicMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadGraphic."
         "MI_IPV5C_OfficialPlanningRoadGraphic"));
const FString ParentMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/"
         "MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone"));

const FString AsphaltBaseColorTextureObjectPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/Surfaces/"
         "Ground_Asphalt_Fresh_01/T_Ground_Asphalt_Fresh_01_D."
         "T_Ground_Asphalt_Fresh_01_D"));
const FString AsphaltNormalTextureObjectPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/Surfaces/"
         "Ground_Asphalt_Fresh_01/T_Ground_Asphalt_Fresh_01_N."
         "T_Ground_Asphalt_Fresh_01_N"));
const FString AsphaltOrdpTextureObjectPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/Surfaces/"
         "Ground_Asphalt_Fresh_01/T_Ground_Asphalt_Fresh_01_ORDp."
         "T_Ground_Asphalt_Fresh_01_ORDp"));
constexpr int64 AsphaltBaseColorTexturePackageBytes = 23508700;
constexpr int64 AsphaltNormalTexturePackageBytes = 18074043;
constexpr int64 AsphaltOrdpTexturePackageBytes = 16481152;
const FString AsphaltBaseColorTexturePackageSha256(
    TEXT("8BB620174317FDF2AE97909CBF08E11E6C3AFCCEAE40B1C7D399A3D42CE05C1E"));
const FString AsphaltNormalTexturePackageSha256(
    TEXT("8E166FB343C8E1F80E41675FDEC80FE00F3506A6D5E68EBD46C80AE579826280"));
const FString AsphaltOrdpTexturePackageSha256(
    TEXT("E66E2C80B3D1D8CDB8516131B646A496CC9F857D49ACA98BED966115C6AC4B7D"));

constexpr int32 ExpectedAssetCount = 5;
constexpr int32 RoadBaseMaterialIndex = 0;
constexpr int32 RoadGraphicMaterialIndex = 1;
constexpr int32 ConcreteMaterialIndex = 2;
constexpr int32 ExpectedMaterialCount = 3;

const TCHAR* const MaterialSlotNames[] = {
    TEXT("MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("MI_IPV5C_OfficialPlanningRoadGraphic"),
    TEXT("MI_IPV5D_PublicRealmConcrete"),
};
static_assert(UE_ARRAY_COUNT(MaterialSlotNames) == ExpectedMaterialCount);

constexpr int32 ExpectedCoreVertexCount = 1332;
constexpr int32 ExpectedCoreVertexInstanceCount = 1332;
constexpr int32 ExpectedCoreTriangleCount = 444;
constexpr int32 ExpectedFallbackVertexCount = 3303;
constexpr int32 ExpectedFallbackVertexInstanceCount = 3303;
constexpr int32 ExpectedFallbackTriangleCount = 1101;
constexpr int32 ExpectedFallbackRoadBaseTriangles = 697;
constexpr int32 ExpectedFallbackRoadGraphicTriangles = 329;
constexpr int32 ExpectedFallbackSidewalkTriangles = 27;
constexpr int32 ExpectedFallbackKerbTopTriangles = 24;
constexpr int32 ExpectedFallbackKerbWallTriangles = 24;
constexpr int32 ExpectedFallbackConcreteTriangles =
    ExpectedFallbackSidewalkTriangles + ExpectedFallbackKerbTopTriangles +
    ExpectedFallbackKerbWallTriangles;
static_assert(ExpectedFallbackConcreteTriangles == 75);
static_assert(
    ExpectedFallbackRoadBaseTriangles +
        ExpectedFallbackRoadGraphicTriangles +
        ExpectedFallbackConcreteTriangles ==
    ExpectedFallbackTriangleCount);

constexpr int64 ExpectedCoreObjBytes = 168139;
constexpr int64 ExpectedFallbackObjBytes = 420002;
constexpr int64 ExpectedMtlBytes = 707;
constexpr int64 ExpectedFeaturesBytes = 19432;
constexpr int64 ExpectedManifestBytes = 21577;
constexpr int64 ExpectedAcceptanceLockBytes = 7607;
constexpr int64 ExpectedContractBytes = 17548;
constexpr int64 ExpectedGeneratorBytes = 68011;

const FString ExpectedCoreObjSha256(
    TEXT("6418A023D64FA0A0F6C4CA14C79195BF96C02818B49C2AC03E4C81A61ECE9438"));
const FString ExpectedFallbackObjSha256(
    TEXT("EFB1E7FE2371D5C522297698647DFC01C30240BE5A8A54945A7BCFDFA7C488F9"));
const FString ExpectedMtlSha256(
    TEXT("F03216B73AB47ACCBC7FC8BE2DADD1B6067AF2DA39F6A037CEDC107515FA18E8"));
const FString ExpectedFeaturesSha256(
    TEXT("DBBC471315517DBC0B6AB69C1F4169FCAA9CD475F708009580C1C25A4CF3155A"));
const FString ExpectedManifestSha256(
    TEXT("FD5448DEA86724EB1AF611394829B72C543ABAA8D1DF6A4BD275A509F5B70018"));
const FString ExpectedAcceptanceLockSha256(
    TEXT("3EF1EA38422505920F920E06B3447D99135A56BC82144CE64946BC025EDBD4FF"));
const FString ExpectedContractSha256(
    TEXT("C5B4BFFF1FD90D5A0E7F056CE510CFC76BF3E7914C008F4D0CBCA4852B853C28"));
const FString ExpectedGeneratorSha256(
    TEXT("E5DF1DADB4DE6DD96CBE3A74CCE815055373F16A18E99F9F1A0F77D27762B547"));
const FString ExpectedLockedSetSha256(
    TEXT("C99E09A59E6E7BB41127815C8D73A74937E231B2946B7EB53EE9F8DD122F0287"));
const FString ExpectedPrimaryOutputSetSha256(
    TEXT("C0AC6D6D2141E764160BB0BCFB9E879603A26CB2953FDB5587C178081F909EA4"));

const FString CoreSourceIdentifier(
    TEXT("SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
         "SM_IPV5D_PublicRealm_Core_Render.obj"));
const FString FallbackSourceIdentifier(
    TEXT("SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
         "SM_IPV5D_PublicRealm_Fallback_Render.obj"));

struct FImportedSectionSpec
{
    const TCHAR* Label;
    int32 MaterialIndex;
    int32 Triangles;
};

// UE's legacy OBJ path retains one section per material. The three source
// concrete groups remain distinguishable in the hash-pinned OBJ/manifest
// topology and intentionally aggregate into the one concrete render section.
const FImportedSectionSpec CoreSections[] = {
    {
        TEXT("V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION"),
        RoadBaseMaterialIndex,
        ExpectedCoreTriangleCount,
    },
};
// UE5.5's legacy OBJ importer emits render sections in reverse usemtl order
// for this multi-material mesh. Keep the persisted material slots canonical
// (road base, road graphic, concrete), but admit and then remap this exact
// deterministic imported section order.
const FImportedSectionSpec FallbackSections[] = {
    {
        TEXT("V5D_CONCRETE_SIDEWALK_AND_KERB_VISUAL_ASSUMPTIONS"),
        ConcreteMaterialIndex,
        ExpectedFallbackConcreteTriangles,
    },
    {
        TEXT("V5D_OFFICIAL_ROAD_GRAPHIC_VISUAL_REFERENCE"),
        RoadGraphicMaterialIndex,
        ExpectedFallbackRoadGraphicTriangles,
    },
    {
        TEXT("V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION"),
        RoadBaseMaterialIndex,
        ExpectedFallbackRoadBaseTriangles,
    },
};

struct FMeshSpec
{
    const FString* Name;
    const FString* ObjectPath;
    const TCHAR* SourceRelativePath;
    int64 SourceBytes;
    const FString* SourceSha256;
    int32 VertexCount;
    int32 VertexInstanceCount;
    int32 TriangleCount;
    int32 MaterialCount;
    const FImportedSectionSpec* Sections;
    int32 SectionCount;
    FVector BoundsMinCentimetres;
    FVector BoundsMaxCentimetres;
    FVector2f UvMinSourceMetres;
    FVector2f UvMaxSourceMetres;
};

const FMeshSpec CoreMeshSpec = {
    &CoreMeshName,
    &CoreMeshObjectPath,
    TEXT("IstanaPublicViewExploreV5D/PublicRealm/Generated/"
         "SM_IPV5D_PublicRealm_Core_Render.obj"),
    ExpectedCoreObjBytes,
    &ExpectedCoreObjSha256,
    ExpectedCoreVertexCount,
    ExpectedCoreVertexInstanceCount,
    ExpectedCoreTriangleCount,
    1,
    CoreSections,
    UE_ARRAY_COUNT(CoreSections),
    FVector(-18526.3744, -19779.2272, -72.7225801242491),
    FVector(2593.3948, 15560.9145, -17.0206276058938),
    FVector2f(-185.263744f, -197.792272f),
    FVector2f(25.933948f, 155.609145f),
};

const FMeshSpec FallbackMeshSpec = {
    &FallbackMeshName,
    &FallbackMeshObjectPath,
    TEXT("IstanaPublicViewExploreV5D/PublicRealm/Generated/"
         "SM_IPV5D_PublicRealm_Fallback_Render.obj"),
    ExpectedFallbackObjBytes,
    &ExpectedFallbackObjSha256,
    ExpectedFallbackVertexCount,
    ExpectedFallbackVertexInstanceCount,
    ExpectedFallbackTriangleCount,
    3,
    FallbackSections,
    UE_ARRAY_COUNT(FallbackSections),
    FVector(-30000.0, -29906.1733, -83.0364228435806),
    FVector(12398.4972, 25275.4547, 8.65989430239755),
    FVector2f(-300.0f, -299.061733f),
    FVector2f(276.92337811f, 252.754547f),
};

struct FSourceFileSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const FString* Sha256;
};

const FSourceFileSpec AdditionalSourceFiles[] = {
    {
        TEXT("IstanaPublicViewExploreV5D/PublicRealm/Generated/"
             "SM_IPV5D_PublicRealm_Render.mtl"),
        ExpectedMtlBytes,
        &ExpectedMtlSha256,
    },
    {
        TEXT("IstanaPublicViewExploreV5D/PublicRealm/Generated/"
             "IstanaPublicViewV5DPublicRealm.features.json"),
        ExpectedFeaturesBytes,
        &ExpectedFeaturesSha256,
    },
    {
        TEXT("IstanaPublicViewExploreV5D/PublicRealm/Generated/"
             "IstanaPublicViewV5DPublicRealm.manifest.json"),
        ExpectedManifestBytes,
        &ExpectedManifestSha256,
    },
    {
        TEXT("IstanaPublicViewExploreV5D/PublicRealm/Generated/"
             "IstanaPublicViewV5DPublicRealm.acceptance.lock.json"),
        ExpectedAcceptanceLockBytes,
        &ExpectedAcceptanceLockSha256,
    },
    {
        TEXT("IstanaPublicViewExploreV5D/PublicRealm/"
             "istana_public_view_v5d_public_realm.contract.json"),
        ExpectedContractBytes,
        &ExpectedContractSha256,
    },
    {
        TEXT("IstanaPublicViewExploreV5D/PublicRealm/"
             "build_public_realm_v5d.py"),
        ExpectedGeneratorBytes,
        &ExpectedGeneratorSha256,
    },
};

struct FScalarOverrideSpec
{
    const TCHAR* Name;
    float Value;
};

const FScalarOverrideSpec ConcreteScalarOverrides[] = {
    {TEXT("TileMeters"), 1.50f},
    {TEXT("DetailTileMeters"), 0.18f},
    {TEXT("MacroTileMeters"), 24.0f},
    {TEXT("NormalStrength"), 0.18f},
    {TEXT("DetailNormalStrength"), 0.16f},
    {TEXT("RoughnessBias"), 0.20f},
    {TEXT("MacroAlbedoStrength"), 0.04f},
    {TEXT("MacroRoughnessStrength"), 0.04f},
    {TEXT("BumpOffsetStrength"), 0.0f},
    {TEXT("ExposedMetalMaskStrength"), 0.0f},
};
const FLinearColor ConcreteLookdevTint(0.48f, 0.49f, 0.47f, 1.0f);

constexpr float AsphaltPrimaryTileMeters = 2.0f;
constexpr float AsphaltSecondaryTileMeters = 3.37f;
constexpr float AsphaltMacroTileMeters = 37.0f;
constexpr float AsphaltSecondaryRotationRadians =
    FMath::DegreesToRadians(37.0f);
constexpr float AsphaltSecondaryOffsetU = 13.17f;
constexpr float AsphaltSecondaryOffsetV = -7.43f;
constexpr float AsphaltSecondaryBlend = 0.34f;
constexpr float AsphaltNormalStrength = 0.42f;
constexpr float AsphaltRoughnessBias = 0.04f;
constexpr float AsphaltSpecular = 0.25f;
const FLinearColor AsphaltAlbedoTint(0.82f, 0.85f, 0.88f, 1.0f);
const FLinearColor AsphaltMacroTintLow(0.94f, 0.95f, 0.96f, 1.0f);
const FLinearColor AsphaltMacroTintHigh(1.03f, 1.02f, 1.00f, 1.0f);
const FString AsphaltRotatedNormalCode(
    TEXT("return normalize(float3(\n")
    TEXT("    0.79863551 * RotatedNormal.x + 0.60181502 * RotatedNormal.y,\n")
    TEXT("   -0.60181502 * RotatedNormal.x + 0.79863551 * RotatedNormal.y,\n")
    TEXT("    RotatedNormal.z));"));
const FString AsphaltMacroFieldCode(
    TEXT("float2 p = MacroUV;\n")
    TEXT("float2 i = floor(p);\n")
    TEXT("float2 f = frac(p);\n")
    TEXT("f = f * f * (3.0 - 2.0 * f);\n")
    TEXT("float4 h = frac(sin(float4(\n")
    TEXT("    dot(i, float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(1.0, 0.0), float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(0.0, 1.0), float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(1.0, 1.0), float2(127.1, 311.7)))) * 43758.5453);\n")
    TEXT("float broad = lerp(lerp(h.x, h.y, f.x), lerp(h.z, h.w, f.x), f.y);\n")
    TEXT("float2 q = p * 2.173 + float2(11.17, -7.43);\n")
    TEXT("float2 j = floor(q);\n")
    TEXT("float2 g = frac(q);\n")
    TEXT("g = g * g * (3.0 - 2.0 * g);\n")
    TEXT("float4 k = frac(sin(float4(\n")
    TEXT("    dot(j, float2(269.5, 183.3)),\n")
    TEXT("    dot(j + float2(1.0, 0.0), float2(269.5, 183.3)),\n")
    TEXT("    dot(j + float2(0.0, 1.0), float2(269.5, 183.3)),\n")
    TEXT("    dot(j + float2(1.0, 1.0), float2(269.5, 183.3)))) * 43758.5453);\n")
    TEXT("float detail = lerp(lerp(k.x, k.y, g.x), lerp(k.z, k.w, g.x), g.y);\n")
    TEXT("return saturate(0.50 + 0.62 * (broad - 0.50) + 0.28 * (detail - 0.50));"));

FString ProjectSourcePath(const TCHAR* RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("SourceAssets"), RelativePath));
}

FString MeshSourcePath(const FMeshSpec& Spec)
{
    return ProjectSourcePath(Spec.SourceRelativePath);
}

template <typename T>
T* LoadExact(const FString& Path)
{
    T* Object = LoadObject<T>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

bool ValidateSourceHash(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            ExpectedBytes,
            Bytes.Num());
        return false;
    }

    FString ActualSha256;
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr)
    {
        OutError = TEXT("V5D public-realm SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5D public-realm SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm SHA-256 guard failed for '%s': expected=%s actual=%s."),
            *Filename,
            *ExpectedSha256,
            *ActualSha256);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAllSourceHashes(FString& OutError)
{
    if (!ValidateSourceHash(
            MeshSourcePath(CoreMeshSpec),
            CoreMeshSpec.SourceBytes,
            *CoreMeshSpec.SourceSha256,
            OutError) ||
        !ValidateSourceHash(
            MeshSourcePath(FallbackMeshSpec),
            FallbackMeshSpec.SourceBytes,
            *FallbackMeshSpec.SourceSha256,
            OutError))
    {
        return false;
    }
    for (const FSourceFileSpec& Spec : AdditionalSourceFiles)
    {
        if (!ValidateSourceHash(
                ProjectSourcePath(Spec.RelativePath),
                Spec.Bytes,
                *Spec.Sha256,
                OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

const TArray<FString>& OrderedSemanticMaterialPaths()
{
    static const TArray<FString> Paths = {
        LegacyRoadBaseMaterialObjectPath,
        LegacyRoadGraphicMaterialObjectPath,
        ConcreteMaterialObjectPath,
    };
    return Paths;
}

TArray<FString> ExpectedObjectPaths()
{
    TArray<FString> Paths = {
        CoreMeshObjectPath,
        FallbackMeshObjectPath,
        ConcreteMaterialObjectPath,
    };
    Paths.Sort();
    return Paths;
}

TArray<FString> ExpectedVisualMaterialObjectPaths()
{
    TArray<FString> Paths = {
        AsphaltMaterialObjectPath,
        RoadGraphicSuppressionMaterialObjectPath,
    };
    Paths.Sort();
    return Paths;
}

bool GatherAssetsAtRoot(
    const FString& Root,
    TArray<FAssetData>& OutAssets,
    FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*Root), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("V5D public-realm Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets, FString& OutError)
{
    return GatherAssetsAtRoot(AssetRoot, OutAssets, OutError);
}

bool GatherVisualMaterialAssets(
    TArray<FAssetData>& OutAssets,
    FString& OutError)
{
    return GatherAssetsAtRoot(VisualMaterialRoot, OutAssets, OutError);
}

bool ValidateExactRootRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherRootAssets(Assets, OutError))
    {
        return false;
    }
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    if (Actual != ExpectedObjectPaths())
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm root must contain exactly two meshes and one concrete MIC; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        for (const FString& Path : ExpectedObjectPaths())
        {
            UObject* Object = LoadExact<UObject>(Path);
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (!Object || !Package ||
                !FPackageName::DoesPackageExist(Package->GetName()) ||
                Package->IsDirty())
            {
                OutError = TEXT("An exact V5D public-realm asset is not persisted and clean: ") +
                    Path;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateExactVisualMaterialRoster(
    bool bRequireSaved,
    FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherVisualMaterialAssets(Assets, OutError))
    {
        return false;
    }
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    const TArray<FString> Expected = ExpectedVisualMaterialObjectPaths();
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm Visual R2 root must contain exactly one owned asphalt and one owned fully clipped RoadGraphic material; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        for (const FString& Path : Expected)
        {
            UMaterial* Material = LoadExact<UMaterial>(Path);
            UPackage* Package = Material ? Material->GetOutermost() : nullptr;
            if (!Material || !Package ||
                !FPackageName::DoesPackageExist(Package->GetName()) ||
                Package->IsDirty())
            {
                OutError = TEXT("An exact V5D public-realm Visual R2 material is not persisted and clean: ") +
                    Path;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

class FScopedFreshRollback final
{
public:
    FScopedFreshRollback(
        TArray<UObject*>& InAssets,
        FString& InError,
        bool bInVisualUpgradeOnly)
        : Assets(InAssets),
          Error(InError),
          bVisualUpgradeOnly(bInVisualUpgradeOnly)
    {
    }

    ~FScopedFreshRollback()
    {
        if (bCommitted)
        {
            return;
        }
        TArray<FAssetData> Data;
        TArray<FAssetData> VisualData;
        FString Ignored;
        if (!bVisualUpgradeOnly)
        {
            GatherRootAssets(Data, Ignored);
        }
        GatherVisualMaterialAssets(VisualData, Ignored);
        Data.Append(VisualData);
        TArray<UObject*> Disposable;
        for (const FAssetData& Row : Data)
        {
            UObject* Object = Row.GetAsset();
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (Object && Package &&
                Package->HasAnyPackageFlags(PKG_NewlyCreated) &&
                !FPackageName::DoesPackageExist(Package->GetName()))
            {
                Disposable.AddUnique(Object);
            }
        }
        if (!Disposable.IsEmpty() &&
            ObjectTools::DeleteObjectsUnchecked(Disposable) !=
                Disposable.Num())
        {
            Error += TEXT(" V5D_PUBLIC_REALM_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
    }

    void Commit() { bCommitted = true; }

private:
    TArray<UObject*>& Assets;
    FString& Error;
    bool bVisualUpgradeOnly = false;
    bool bCommitted = false;
};

FString EnabledBasePropertyOverrideNames(
    const FMaterialInstanceBasePropertyOverrides& Overrides)
{
    TArray<FString> Names;
    if (Overrides.bOverride_OpacityMaskClipValue)
        Names.Add(TEXT("OpacityMaskClipValue"));
    if (Overrides.bOverride_BlendMode)
        Names.Add(TEXT("BlendMode"));
    if (Overrides.bOverride_ShadingModel)
        Names.Add(TEXT("ShadingModel"));
    if (Overrides.bOverride_DitheredLODTransition)
        Names.Add(TEXT("DitheredLODTransition"));
    if (Overrides.bOverride_CastDynamicShadowAsMasked)
        Names.Add(TEXT("CastDynamicShadowAsMasked"));
    if (Overrides.bOverride_TwoSided)
        Names.Add(TEXT("TwoSided"));
    if (Overrides.bOverride_bIsThinSurface)
        Names.Add(TEXT("ThinSurface"));
    if (Overrides.bOverride_OutputTranslucentVelocity)
        Names.Add(TEXT("OutputTranslucentVelocity"));
    if (Overrides.bOverride_bHasPixelAnimation)
        Names.Add(TEXT("HasPixelAnimation"));
    if (Overrides.bOverride_bEnableTessellation)
        Names.Add(TEXT("EnableTessellation"));
    if (Overrides.bOverride_DisplacementScaling)
        Names.Add(TEXT("DisplacementScaling"));
    if (Overrides.bOverride_bEnableDisplacementFade)
        Names.Add(TEXT("EnableDisplacementFade"));
    if (Overrides.bOverride_DisplacementFadeRange)
        Names.Add(TEXT("DisplacementFadeRange"));
    if (Overrides.bOverride_MaxWorldPositionOffsetDisplacement)
        Names.Add(TEXT("MaximumWorldPositionOffsetDisplacement"));
    Names.Sort();
    return FString::Join(Names, TEXT(","));
}

bool ValidateParentMaterial(
    UMaterialInstanceConstant* Parent,
    FString& OutError)
{
    UMaterial* Base = Parent ? Parent->GetMaterial() : nullptr;
    UMaterialEditorOnlyData* EditorOnly =
        Base ? Base->GetEditorOnlyData() : nullptr;
    UPackage* Package = Parent ? Parent->GetOutermost() : nullptr;
    if (!Parent || Parent->GetPathName() != ParentMaterialObjectPath ||
        !Package || !FPackageName::DoesPackageExist(Package->GetName()) ||
        Package->IsDirty() || !Base || !EditorOnly ||
        Base->MaterialDomain != MD_Surface ||
        Base->BlendMode != BLEND_Opaque || Base->TwoSided ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->Displacement.Expression ||
        !FMath::IsNearlyZero(Base->MaxWorldPositionOffsetDisplacement))
    {
        OutError = TEXT("The exact persisted opaque, one-sided, displacement-free MI_IPV5_HardscapeStone parent is absent or changed.");
        return false;
    }
    for (const FScalarOverrideSpec& Spec : ConcreteScalarOverrides)
    {
        float Value = 0.0f;
        if (!Parent->GetScalarParameterValue(
                FHashedMaterialParameterInfo(FName(Spec.Name)),
                Value,
                true))
        {
            OutError = TEXT("MI_IPV5_HardscapeStone lacks required scalar parameter '") +
                FString(Spec.Name) + TEXT("'.");
            return false;
        }
    }
    FLinearColor Tint;
    if (!Parent->GetVectorParameterValue(
            FHashedMaterialParameterInfo(FName(TEXT("LookdevTint"))),
            Tint,
            true))
    {
        OutError = TEXT("MI_IPV5_HardscapeStone lacks required vector parameter 'LookdevTint'.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateLegacyMaterialDependencies(
    TArray<UMaterialInstanceConstant*>& OutRoadMaterials,
    UMaterialInstanceConstant*& OutParent,
    FString& OutError)
{
    OutRoadMaterials.Reset();
    OutParent = nullptr;
    const TArray<FString> ExpectedV5CPaths = {
        LegacyRoadBaseMaterialObjectPath,
        LegacyRoadGraphicMaterialObjectPath,
    };
    if (TRIADIstanaExploreV5CGroundContextAssetFactory::
            GetOrderedMaterialObjectPaths() != ExpectedV5CPaths ||
        ATRIADIstanaExploreV5DPublicRealmActor::
                ExpectedRoadBaseMaterialObjectPath() !=
            AsphaltMaterialObjectPath ||
        ATRIADIstanaExploreV5DPublicRealmActor::
                ExpectedRoadGraphicMaterialObjectPath() !=
            RoadGraphicSuppressionMaterialObjectPath ||
        ATRIADIstanaExploreV5DPublicRealmActor::
                ExpectedConcreteMaterialObjectPath() !=
            ConcreteMaterialObjectPath ||
        ATRIADIstanaExploreV5DPublicRealmActor::
                ExpectedCoreMeshObjectPath() !=
            CoreMeshObjectPath ||
        ATRIADIstanaExploreV5DPublicRealmActor::
                ExpectedFallbackMeshObjectPath() !=
            FallbackMeshObjectPath)
    {
        OutError = TEXT("V5C immutable mesh-binding order or V5D R2 runtime presentation paths changed.");
        return false;
    }
    for (const FString& Path : ExpectedV5CPaths)
    {
        UMaterialInstanceConstant* Material =
            LoadExact<UMaterialInstanceConstant>(Path);
        UPackage* Package = Material ? Material->GetOutermost() : nullptr;
        if (!Material ||
            Material->GetClass() !=
                UMaterialInstanceConstant::StaticClass() ||
            !Package ||
            !FPackageName::DoesPackageExist(Package->GetName()) ||
            Package->IsDirty())
        {
            OutError = TEXT("A required exact, persisted, clean V5C material dependency is invalid: ") +
                Path;
            OutRoadMaterials.Reset();
            return false;
        }
        OutRoadMaterials.Add(Material);
    }
    OutParent = LoadExact<UMaterialInstanceConstant>(ParentMaterialObjectPath);
    if (OutRoadMaterials.Num() != 2 ||
        OutRoadMaterials.Contains(nullptr) ||
        !ValidateParentMaterial(OutParent, OutError))
    {
        OutRoadMaterials.Reset();
        OutParent = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

struct FAsphaltTextureSet
{
    UTexture2D* BaseColor = nullptr;
    UTexture2D* Normal = nullptr;
    UTexture2D* Ordp = nullptr;
};

EMaterialSamplerType ExactSamplerType(const UTexture2D* Texture)
{
    if (!Texture)
    {
        return SAMPLERTYPE_Color;
    }
    if (Texture->VirtualTextureStreaming)
    {
        if (Texture->CompressionSettings == TC_Normalmap)
        {
            return SAMPLERTYPE_VirtualNormal;
        }
        if (Texture->CompressionSettings == TC_Masks)
        {
            return SAMPLERTYPE_VirtualMasks;
        }
        return Texture->SRGB
            ? SAMPLERTYPE_VirtualColor
            : SAMPLERTYPE_VirtualLinearColor;
    }
    if (Texture->CompressionSettings == TC_Normalmap)
    {
        return SAMPLERTYPE_Normal;
    }
    if (Texture->CompressionSettings == TC_Masks)
    {
        return SAMPLERTYPE_Masks;
    }
    return Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
}

bool ValidateAsphaltTextureDependencies(
    FAsphaltTextureSet& OutTextures,
    FString& OutError)
{
    OutTextures = FAsphaltTextureSet();
    OutTextures.BaseColor =
        LoadExact<UTexture2D>(AsphaltBaseColorTextureObjectPath);
    OutTextures.Normal =
        LoadExact<UTexture2D>(AsphaltNormalTextureObjectPath);
    OutTextures.Ordp = LoadExact<UTexture2D>(AsphaltOrdpTextureObjectPath);
    const UTexture2D* Textures[] = {
        OutTextures.BaseColor,
        OutTextures.Normal,
        OutTextures.Ordp};
    const FString* Paths[] = {
        &AsphaltBaseColorTextureObjectPath,
        &AsphaltNormalTextureObjectPath,
        &AsphaltOrdpTextureObjectPath};
    const int64 PackageBytes[] = {
        AsphaltBaseColorTexturePackageBytes,
        AsphaltNormalTexturePackageBytes,
        AsphaltOrdpTexturePackageBytes};
    const FString* PackageSha256[] = {
        &AsphaltBaseColorTexturePackageSha256,
        &AsphaltNormalTexturePackageSha256,
        &AsphaltOrdpTexturePackageSha256};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Textures); ++Index)
    {
        const UTexture2D* Texture = Textures[Index];
        const UPackage* Package = Texture ? Texture->GetOutermost() : nullptr;
        const FString PackageFilename =
            FPackageName::LongPackageNameToFilename(
                FPackageName::ObjectPathToPackageName(*Paths[Index]),
                FPackageName::GetAssetPackageExtension());
        if (!ValidateSourceHash(
                PackageFilename,
                PackageBytes[Index],
                *PackageSha256[Index],
                OutError) ||
            !Texture || Texture->GetPathName() != *Paths[Index] ||
            !Package || !FPackageName::DoesPackageExist(Package->GetName()) ||
            Package->IsDirty() || Texture->GetSizeX() != Texture->GetSizeY() ||
            Texture->GetSizeX() < 2048 ||
            Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap)
        {
            OutError = TEXT("An exact hash-pinned, persisted, square, wrap-addressed asphalt PBR texture dependency is absent, dirty, or invalid: ") +
                *Paths[Index] + TEXT(". ") + OutError;
            OutTextures = FAsphaltTextureSet();
            return false;
        }
    }
    if (!OutTextures.BaseColor->SRGB ||
        OutTextures.BaseColor->CompressionSettings != TC_Default ||
        OutTextures.Normal->SRGB ||
        OutTextures.Normal->CompressionSettings != TC_Normalmap ||
        OutTextures.Ordp->SRGB ||
        OutTextures.Ordp->CompressionSettings != TC_Masks)
    {
        OutError = TEXT("The asphalt D/N/ORDp color-space or compression contract changed.");
        OutTextures = FAsphaltTextureSet();
        return false;
    }
    OutError.Reset();
    return true;
}

template <typename T>
T* AddAsphaltExpression(
    UMaterial* Material,
    const TCHAR* NodeId,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    T* Expression = EditorOnly
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = NodeId;
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpressionScalarParameter* AddAsphaltScalar(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    float DefaultValue,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddAsphaltExpression<UMaterialExpressionScalarParameter>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = TEXT("Istana Public Realm Visual R2");
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* AddAsphaltVector(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    const FLinearColor& DefaultValue,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionVectorParameter* Parameter =
        AddAsphaltExpression<UMaterialExpressionVectorParameter>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = TEXT("Istana Public Realm Visual R2");
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionTextureSampleParameter2D* AddAsphaltTexture(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    UTexture2D* Texture,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionTextureSampleParameter2D* Parameter =
        AddAsphaltExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = TEXT("Istana Public Realm Visual R2");
        Parameter->Texture = Texture;
        Parameter->SamplerType = ExactSamplerType(Texture);
        Parameter->SamplerSource = SSM_FromTextureAsset;
        Parameter->MipValueMode = TMVM_None;
        Parameter->ConstMipValue = 0;
        Parameter->AutomaticViewMipBias = true;
    }
    return Parameter;
}

UMaterialExpressionCustom* AddAsphaltCustom(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    const TCHAR* InputName,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionCustom* Custom =
        AddAsphaltExpression<UMaterialExpressionCustom>(
            Material, NodeId, EditorX, EditorY);
    if (Custom)
    {
        Custom->Description = Description;
        Custom->Code = Code;
        Custom->OutputType = OutputType;
        Custom->Inputs.Reset(1);
        FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputName;
    }
    return Custom;
}

UMaterial* CreateAsphaltMaterial(
    IAssetTools& AssetTools,
    const FAsphaltTextureSet& Textures,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              AsphaltMaterialName,
              VisualMaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DPublicRealmVisualR2"))))
        : nullptr;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data ||
        !Data->ExpressionCollection.Expressions.IsEmpty() ||
        !Textures.BaseColor || !Textures.Normal || !Textures.Ordp)
    {
        OutError = TEXT("Could not create the fresh V5D public-realm R2 asphalt material graph.");
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddAsphaltExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("Asphalt.UV0_Metres"), -1900, 0);
    UMaterialExpressionScalarParameter* PrimaryMeters = AddAsphaltScalar(
        Material, TEXT("Asphalt.PrimaryTileMeters"),
        TEXT("PrimaryTileMeters"), AsphaltPrimaryTileMeters, -1900, 120);
    UMaterialExpressionDivide* PrimaryUv =
        AddAsphaltExpression<UMaterialExpressionDivide>(
            Material, TEXT("Asphalt.PrimaryUV"), -1680, 0);
    UMaterialExpressionScalarParameter* SecondaryMeters = AddAsphaltScalar(
        Material, TEXT("Asphalt.SecondaryTileMeters"),
        TEXT("SecondaryTileMeters"), AsphaltSecondaryTileMeters, -1900, 260);
    UMaterialExpressionDivide* SecondaryUnrotated =
        AddAsphaltExpression<UMaterialExpressionDivide>(
            Material, TEXT("Asphalt.SecondaryUnrotatedUV"), -1680, 230);
    UMaterialExpressionConstant* Rotation =
        AddAsphaltExpression<UMaterialExpressionConstant>(
            Material, TEXT("Asphalt.StaticRotation37Degrees"), -1680, 360);
    UMaterialExpressionRotator* RotatedSecondary =
        AddAsphaltExpression<UMaterialExpressionRotator>(
            Material, TEXT("Asphalt.RotateSecondaryUV"), -1450, 230);
    UMaterialExpressionConstant2Vector* SecondaryOffset =
        AddAsphaltExpression<UMaterialExpressionConstant2Vector>(
            Material, TEXT("Asphalt.SecondaryStaticOffset"), -1450, 370);
    UMaterialExpressionAdd* SecondaryUv =
        AddAsphaltExpression<UMaterialExpressionAdd>(
            Material, TEXT("Asphalt.SecondaryUV"), -1220, 230);
    UMaterialExpressionScalarParameter* MacroMeters = AddAsphaltScalar(
        Material, TEXT("Asphalt.MacroTileMeters"),
        TEXT("MacroTileMeters"), AsphaltMacroTileMeters, -1900, 500);
    UMaterialExpressionDivide* MacroUv =
        AddAsphaltExpression<UMaterialExpressionDivide>(
            Material, TEXT("Asphalt.MacroUV"), -1680, 500);

    UMaterialExpressionTextureSampleParameter2D* BasePrimary =
        AddAsphaltTexture(
            Material, TEXT("Asphalt.BaseColorPrimary"),
            TEXT("AsphaltBaseColorPrimary"), Textures.BaseColor, -980, -520);
    UMaterialExpressionTextureSampleParameter2D* BaseSecondary =
        AddAsphaltTexture(
            Material, TEXT("Asphalt.BaseColorSecondary"),
            TEXT("AsphaltBaseColorSecondary"), Textures.BaseColor, -980, -380);
    UMaterialExpressionTextureSampleParameter2D* NormalPrimary =
        AddAsphaltTexture(
            Material, TEXT("Asphalt.NormalPrimary"),
            TEXT("AsphaltNormalPrimary"), Textures.Normal, -980, -180);
    UMaterialExpressionTextureSampleParameter2D* NormalSecondary =
        AddAsphaltTexture(
            Material, TEXT("Asphalt.NormalSecondary"),
            TEXT("AsphaltNormalSecondary"), Textures.Normal, -980, -40);
    UMaterialExpressionTextureSampleParameter2D* OrdpPrimary =
        AddAsphaltTexture(
            Material, TEXT("Asphalt.ORDpPrimary"),
            TEXT("AsphaltORDpPrimary"), Textures.Ordp, -980, 180);
    UMaterialExpressionTextureSampleParameter2D* OrdpSecondary =
        AddAsphaltTexture(
            Material, TEXT("Asphalt.ORDpSecondary"),
            TEXT("AsphaltORDpSecondary"), Textures.Ordp, -980, 330);
    UMaterialExpressionScalarParameter* SecondaryBlend = AddAsphaltScalar(
        Material, TEXT("Asphalt.SecondaryBlend"),
        TEXT("SecondaryBlend"), AsphaltSecondaryBlend, -700, 780);

    UMaterialExpressionLinearInterpolate* BaseBlend =
        AddAsphaltExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Asphalt.BaseColorAntiTileBlend"), -700, -450);
    UMaterialExpressionVectorParameter* AlbedoTint = AddAsphaltVector(
        Material, TEXT("Asphalt.AlbedoTint"),
        TEXT("AlbedoTint"), AsphaltAlbedoTint, -470, -580);
    UMaterialExpressionMultiply* TintedBase =
        AddAsphaltExpression<UMaterialExpressionMultiply>(
            Material, TEXT("Asphalt.NeutralAlbedoTint"), -250, -450);
    UMaterialExpressionCustom* MacroField = AddAsphaltCustom(
        Material, TEXT("Asphalt.ProceduralMacroField"),
        TEXT("TRIAD_IPV5D_ASPHALT_MACRO_VALUE_NOISE_V1"),
        AsphaltMacroFieldCode, CMOT_Float1, TEXT("MacroUV"), -980, 610);
    UMaterialExpressionVectorParameter* MacroTintLow = AddAsphaltVector(
        Material, TEXT("Asphalt.MacroTintLow"),
        TEXT("MacroTintLow"), AsphaltMacroTintLow, -250, -650);
    UMaterialExpressionVectorParameter* MacroTintHigh = AddAsphaltVector(
        Material, TEXT("Asphalt.MacroTintHigh"),
        TEXT("MacroTintHigh"), AsphaltMacroTintHigh, -250, -760);
    UMaterialExpressionLinearInterpolate* MacroTint =
        AddAsphaltExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Asphalt.LowAmplitudeMacroTint"), -20, -560);
    UMaterialExpressionMultiply* FinalBase =
        AddAsphaltExpression<UMaterialExpressionMultiply>(
            Material, TEXT("Asphalt.FinalBaseColor"), 210, -450);

    UMaterialExpressionLinearInterpolate* RoughnessBlend =
        AddAsphaltExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Asphalt.RoughnessAntiTileBlend"), -700, 210);
    UMaterialExpressionScalarParameter* RoughnessBias = AddAsphaltScalar(
        Material, TEXT("Asphalt.RoughnessBias"),
        TEXT("RoughnessBias"), AsphaltRoughnessBias, -470, 200);
    UMaterialExpressionAdd* RoughnessAdd =
        AddAsphaltExpression<UMaterialExpressionAdd>(
            Material, TEXT("Asphalt.DryRoughnessBias"), -250, 210);
    UMaterialExpressionSaturate* FinalRoughness =
        AddAsphaltExpression<UMaterialExpressionSaturate>(
            Material, TEXT("Asphalt.FinalRoughness"), -20, 210);
    UMaterialExpressionLinearInterpolate* AoBlend =
        AddAsphaltExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Asphalt.AOAntiTileBlend"), -700, 480);

    UMaterialExpressionCustom* RotatedNormal = AddAsphaltCustom(
        Material, TEXT("Asphalt.ReorientRotatedNormal37Degrees"),
        TEXT("TRIAD_IPV5D_ASPHALT_REORIENT_ROTATED_NORMAL_37_DEGREES_V1"),
        AsphaltRotatedNormalCode, CMOT_Float3, TEXT("RotatedNormal"),
        -700, -30);
    UMaterialExpressionLinearInterpolate* NormalBlend =
        AddAsphaltExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Asphalt.NormalAntiTileBlend"), -470, -30);
    UMaterialExpressionConstant3Vector* FlatNormal =
        AddAsphaltExpression<UMaterialExpressionConstant3Vector>(
            Material, TEXT("Asphalt.FlatTangentNormal"), -250, -100);
    UMaterialExpressionScalarParameter* NormalStrength = AddAsphaltScalar(
        Material, TEXT("Asphalt.NormalStrength"),
        TEXT("NormalStrength"), AsphaltNormalStrength, -250, 30);
    UMaterialExpressionLinearInterpolate* StrengthenedNormal =
        AddAsphaltExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Asphalt.ControlledNormalStrength"), -20, -30);
    UMaterialExpressionNormalize* FinalNormal =
        AddAsphaltExpression<UMaterialExpressionNormalize>(
            Material, TEXT("Asphalt.FinalNormalizedNormal"), 210, -30);
    UMaterialExpressionConstant* Specular =
        AddAsphaltExpression<UMaterialExpressionConstant>(
            Material, TEXT("Asphalt.DielectricSpecular"), -20, 360);
    UMaterialExpressionConstant* Metallic =
        AddAsphaltExpression<UMaterialExpressionConstant>(
            Material, TEXT("Asphalt.NonMetal"), -20, 440);

    if (!Uv0 || !PrimaryMeters || !PrimaryUv || !SecondaryMeters ||
        !SecondaryUnrotated || !Rotation || !RotatedSecondary ||
        !SecondaryOffset || !SecondaryUv || !MacroMeters || !MacroUv ||
        !BasePrimary || !BaseSecondary || !NormalPrimary ||
        !NormalSecondary || !OrdpPrimary || !OrdpSecondary ||
        !SecondaryBlend || !BaseBlend || !AlbedoTint || !TintedBase ||
        !MacroField || !MacroTintLow || !MacroTintHigh || !MacroTint ||
        !FinalBase || !RoughnessBlend || !RoughnessBias || !RoughnessAdd ||
        !FinalRoughness || !AoBlend || !RotatedNormal || !NormalBlend ||
        !FlatNormal || !NormalStrength || !StrengthenedNormal ||
        !FinalNormal || !Specular || !Metallic)
    {
        OutError = TEXT("Could not allocate the exact 39-node V5D asphalt anti-tiling graph.");
        return nullptr;
    }

    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    PrimaryUv->A.Connect(0, Uv0);
    PrimaryUv->B.Connect(0, PrimaryMeters);
    SecondaryUnrotated->A.Connect(0, Uv0);
    SecondaryUnrotated->B.Connect(0, SecondaryMeters);
    Rotation->R = AsphaltSecondaryRotationRadians;
    RotatedSecondary->Coordinate.Connect(0, SecondaryUnrotated);
    RotatedSecondary->Time.Connect(0, Rotation);
    RotatedSecondary->CenterX = 0.0f;
    RotatedSecondary->CenterY = 0.0f;
    RotatedSecondary->Speed = 1.0f;
    SecondaryOffset->R = AsphaltSecondaryOffsetU;
    SecondaryOffset->G = AsphaltSecondaryOffsetV;
    SecondaryUv->A.Connect(0, RotatedSecondary);
    SecondaryUv->B.Connect(0, SecondaryOffset);
    MacroUv->A.Connect(0, Uv0);
    MacroUv->B.Connect(0, MacroMeters);

    BasePrimary->Coordinates.Connect(0, PrimaryUv);
    BaseSecondary->Coordinates.Connect(0, SecondaryUv);
    NormalPrimary->Coordinates.Connect(0, PrimaryUv);
    NormalSecondary->Coordinates.Connect(0, SecondaryUv);
    OrdpPrimary->Coordinates.Connect(0, PrimaryUv);
    OrdpSecondary->Coordinates.Connect(0, SecondaryUv);
    BaseBlend->A.Connect(0, BasePrimary);
    BaseBlend->B.Connect(0, BaseSecondary);
    BaseBlend->Alpha.Connect(0, SecondaryBlend);
    TintedBase->A.Connect(0, BaseBlend);
    TintedBase->B.Connect(0, AlbedoTint);
    MacroField->Inputs[0].Input.Connect(0, MacroUv);
    MacroTint->A.Connect(0, MacroTintLow);
    MacroTint->B.Connect(0, MacroTintHigh);
    MacroTint->Alpha.Connect(0, MacroField);
    FinalBase->A.Connect(0, TintedBase);
    FinalBase->B.Connect(0, MacroTint);

    RoughnessBlend->A.Connect(2, OrdpPrimary);
    RoughnessBlend->B.Connect(2, OrdpSecondary);
    RoughnessBlend->Alpha.Connect(0, SecondaryBlend);
    RoughnessAdd->A.Connect(0, RoughnessBlend);
    RoughnessAdd->B.Connect(0, RoughnessBias);
    FinalRoughness->Input.Connect(0, RoughnessAdd);
    AoBlend->A.Connect(1, OrdpPrimary);
    AoBlend->B.Connect(1, OrdpSecondary);
    AoBlend->Alpha.Connect(0, SecondaryBlend);

    RotatedNormal->Inputs[0].Input.Connect(0, NormalSecondary);
    NormalBlend->A.Connect(0, NormalPrimary);
    NormalBlend->B.Connect(0, RotatedNormal);
    NormalBlend->Alpha.Connect(0, SecondaryBlend);
    FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
    StrengthenedNormal->A.Connect(0, FlatNormal);
    StrengthenedNormal->B.Connect(0, NormalBlend);
    StrengthenedNormal->Alpha.Connect(0, NormalStrength);
    FinalNormal->VectorInput.Connect(0, StrengthenedNormal);
    Specular->R = AsphaltSpecular;
    Metallic->R = 0.0f;

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Data->BaseColor.Connect(0, FinalBase);
    Data->Normal.Connect(0, FinalNormal);
    Data->Roughness.Connect(0, FinalRoughness);
    Data->Specular.Connect(0, Specular);
    Data->Metallic.Connect(0, Metallic);
    Data->AmbientOcclusion.Connect(0, AoBlend);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutError.Reset();
    return Material;
}

UMaterial* CreateRoadGraphicSuppressionMaterial(
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              RoadGraphicSuppressionMaterialName,
              VisualMaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DPublicRealmVisualR2"))))
        : nullptr;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data ||
        !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh V5D fully clipped RoadGraphic material graph.");
        return nullptr;
    }

    UMaterialExpressionConstant* FullyClipped =
        AddAsphaltExpression<UMaterialExpressionConstant>(
            Material,
            TEXT("RoadGraphic.FullyClippedOpacityMask"),
            -300,
            0);
    if (!FullyClipped)
    {
        OutError = TEXT("Could not allocate the exact one-node fully clipped RoadGraphic graph.");
        return nullptr;
    }
    FullyClipped->R = 0.0f;

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Masked;
    Material->OpacityMaskClipValue = 0.5f;
    Material->SetShadingModel(MSM_Unlit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = false;
    Material->bUseMaterialAttributes = false;
    Material->bEnableExecWire = false;
    Material->bScreenSpaceReflections = false;
    Material->bCastRayTracedShadows = false;
    Material->DitheredLODTransition = false;
    Material->bCastDynamicShadowAsMasked = false;
    Material->bIsThinSurface = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = false;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Data->OpacityMask.Connect(0, FullyClipped);
    // The mesh is Nanite-enabled; this usage compiles the same fully clipped
    // graph for Nanite without introducing a separate override material.
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutError.Reset();
    return Material;
}

bool InputIs(
    const FExpressionInput& Input,
    const UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    if (!Expression || !Expression->Outputs.IsValidIndex(OutputIndex))
    {
        return false;
    }
    const FExpressionOutput& Output = Expression->Outputs[OutputIndex];
    return Input.Expression == Expression && Input.OutputIndex == OutputIndex &&
        Input.Mask == Output.Mask && Input.MaskR == Output.MaskR &&
        Input.MaskG == Output.MaskG && Input.MaskB == Output.MaskB &&
        Input.MaskA == Output.MaskA;
}

bool ValidateRoadGraphicSuppressionMaterial(
    UMaterial* Material,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(FeatureLevel)
        : nullptr;
    if (Resource && !Resource->IsGameThreadShaderMapComplete())
    {
        Resource->SubmitCompileJobs_GameThread(
            EShaderCompileJobPriority::High);
        Resource->FinishCompilation();
    }
    FMaterialShaderMap* ShaderMap = Resource
        ? Resource->GetGameThreadShaderMap()
        : nullptr;
    const bool bCompileStateAccepted = ShaderMap &&
        (ShaderMap->CompiledSuccessfully() ||
         (!IsMaterialMapDDCEnabled() && IsShaderJobCacheDDCEnabled()));
    const TArray<FString> CompileErrors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpression* OnlyExpression =
        Data && Data->ExpressionCollection.Expressions.Num() == 1
        ? Data->ExpressionCollection.Expressions[0]
        : nullptr;
    const UMaterialExpressionConstant* FullyClipped =
        Cast<UMaterialExpressionConstant>(OnlyExpression);
    bool bCustomizedUvConnected = false;
    if (Data)
    {
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bCustomizedUvConnected |= CustomizedUv.Expression != nullptr;
        }
    }

    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() !=
            RoadGraphicSuppressionMaterialObjectPath ||
        !Data || Data->ExpressionCollection.Expressions.Num() != 1 ||
        !FullyClipped ||
        FullyClipped->GetClass() != UMaterialExpressionConstant::StaticClass() ||
        FullyClipped->Desc != TEXT("RoadGraphic.FullyClippedOpacityMask") ||
        FullyClipped->MaterialExpressionEditorX != -300 ||
        FullyClipped->MaterialExpressionEditorY != 0 ||
        !FMath::IsNearlyZero(FullyClipped->R) || !Resource || !ShaderMap ||
        !Resource->IsCompilationFinished() ||
        !Resource->IsGameThreadShaderMapComplete() ||
        !bCompileStateAccepted || !ShaderMap->IsValidForRendering() ||
        Resource->IsDefaultMaterial() || !CompileErrors.IsEmpty() ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked ||
        !FMath::IsNearlyEqual(Material->OpacityMaskClipValue, 0.5f) ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit) ||
        Material->TwoSided || Material->bTangentSpaceNormal ||
        Material->bUseMaterialAttributes || Material->bEnableExecWire ||
        Material->bScreenSpaceReflections ||
        Material->bCastRayTracedShadows ||
        Material->DitheredLODTransition ||
        Material->bCastDynamicShadowAsMasked || Material->bIsThinSurface ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !InputIs(Data->OpacityMask, FullyClipped) ||
        Data->BaseColor.Expression || Data->Metallic.Expression ||
        Data->Specular.Expression || Data->Roughness.Expression ||
        Data->Anisotropy.Expression || Data->Normal.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->WorldPositionOffset.Expression ||
        Data->Displacement.Expression || Data->SubsurfaceColor.Expression ||
        Data->ClearCoat.Expression || Data->ClearCoatRoughness.Expression ||
        Data->AmbientOcclusion.Expression || Data->Refraction.Expression ||
        bCustomizedUvConnected || Data->MaterialAttributes.Expression ||
        Data->PixelDepthOffset.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression || Data->FrontMaterial.Expression)
    {
        OutError = FString::Printf(
            TEXT("The V5D RoadGraphic suppressor lost its exact compiled one-node constant-zero masked/unlit, fully clipped, no-texture/parameter/custom/attribute/displacement/Nanite-override contract. nodes=%d compileErrors=[%s]."),
            Data ? Data->ExpressionCollection.Expressions.Num() : INDEX_NONE,
            *FString::Join(CompileErrors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

template <typename T>
const T* FindAsphaltNode(
    const TMap<FString, const UMaterialExpression*>& Nodes,
    const TCHAR* NodeId)
{
    const UMaterialExpression* const* Found = Nodes.Find(NodeId);
    return Found ? Cast<T>(*Found) : nullptr;
}

bool ValidateAsphaltMaterial(
    UMaterial* Material,
    const FAsphaltTextureSet& Textures,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(FeatureLevel)
        : nullptr;
    if (Resource && !Resource->IsGameThreadShaderMapComplete())
    {
        Resource->SubmitCompileJobs_GameThread(
            EShaderCompileJobPriority::High);
        Resource->FinishCompilation();
    }
    FMaterialShaderMap* ShaderMap = Resource
        ? Resource->GetGameThreadShaderMap()
        : nullptr;
    const bool bCompileStateAccepted = ShaderMap &&
        (ShaderMap->CompiledSuccessfully() ||
         (!IsMaterialMapDDCEnabled() && IsShaderJobCacheDDCEnabled()));
    const TArray<FString> CompileErrors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    TMap<FString, const UMaterialExpression*> Nodes;
    if (Data)
    {
        for (const UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (!Expression || Expression->Desc.IsEmpty() ||
                Nodes.Contains(Expression->Desc))
            {
                OutError = TEXT("The V5D asphalt graph has a null, unnamed, or duplicate node.");
                return false;
            }
            Nodes.Add(Expression->Desc, Expression);
        }
    }
    const auto* Uv0 = FindAsphaltNode<UMaterialExpressionTextureCoordinate>(
        Nodes, TEXT("Asphalt.UV0_Metres"));
    const auto* PrimaryMeters =
        FindAsphaltNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Asphalt.PrimaryTileMeters"));
    const auto* SecondaryMeters =
        FindAsphaltNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Asphalt.SecondaryTileMeters"));
    const auto* MacroMeters =
        FindAsphaltNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Asphalt.MacroTileMeters"));
    const auto* PrimaryUv = FindAsphaltNode<UMaterialExpressionDivide>(
        Nodes, TEXT("Asphalt.PrimaryUV"));
    const auto* SecondaryUnrotated =
        FindAsphaltNode<UMaterialExpressionDivide>(
            Nodes, TEXT("Asphalt.SecondaryUnrotatedUV"));
    const auto* Rotation = FindAsphaltNode<UMaterialExpressionConstant>(
        Nodes, TEXT("Asphalt.StaticRotation37Degrees"));
    const auto* RotatedSecondary = FindAsphaltNode<UMaterialExpressionRotator>(
        Nodes, TEXT("Asphalt.RotateSecondaryUV"));
    const auto* SecondaryOffset =
        FindAsphaltNode<UMaterialExpressionConstant2Vector>(
            Nodes, TEXT("Asphalt.SecondaryStaticOffset"));
    const auto* SecondaryUv = FindAsphaltNode<UMaterialExpressionAdd>(
        Nodes, TEXT("Asphalt.SecondaryUV"));
    const auto* MacroUv = FindAsphaltNode<UMaterialExpressionDivide>(
        Nodes, TEXT("Asphalt.MacroUV"));
    const auto* BasePrimary =
        FindAsphaltNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Asphalt.BaseColorPrimary"));
    const auto* BaseSecondary =
        FindAsphaltNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Asphalt.BaseColorSecondary"));
    const auto* NormalPrimary =
        FindAsphaltNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Asphalt.NormalPrimary"));
    const auto* NormalSecondary =
        FindAsphaltNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Asphalt.NormalSecondary"));
    const auto* OrdpPrimary =
        FindAsphaltNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Asphalt.ORDpPrimary"));
    const auto* OrdpSecondary =
        FindAsphaltNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Asphalt.ORDpSecondary"));
    const auto* SecondaryBlend =
        FindAsphaltNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Asphalt.SecondaryBlend"));
    const auto* BaseBlend =
        FindAsphaltNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Asphalt.BaseColorAntiTileBlend"));
    const auto* AlbedoTint =
        FindAsphaltNode<UMaterialExpressionVectorParameter>(
            Nodes, TEXT("Asphalt.AlbedoTint"));
    const auto* TintedBase = FindAsphaltNode<UMaterialExpressionMultiply>(
        Nodes, TEXT("Asphalt.NeutralAlbedoTint"));
    const auto* MacroField = FindAsphaltNode<UMaterialExpressionCustom>(
        Nodes, TEXT("Asphalt.ProceduralMacroField"));
    const auto* MacroTintLow =
        FindAsphaltNode<UMaterialExpressionVectorParameter>(
            Nodes, TEXT("Asphalt.MacroTintLow"));
    const auto* MacroTintHigh =
        FindAsphaltNode<UMaterialExpressionVectorParameter>(
            Nodes, TEXT("Asphalt.MacroTintHigh"));
    const auto* MacroTint =
        FindAsphaltNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Asphalt.LowAmplitudeMacroTint"));
    const auto* RotatedNormal = FindAsphaltNode<UMaterialExpressionCustom>(
        Nodes, TEXT("Asphalt.ReorientRotatedNormal37Degrees"));
    const auto* FinalBase = FindAsphaltNode<UMaterialExpressionMultiply>(
        Nodes, TEXT("Asphalt.FinalBaseColor"));
    const auto* RoughnessBlend =
        FindAsphaltNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Asphalt.RoughnessAntiTileBlend"));
    const auto* RoughnessBias =
        FindAsphaltNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Asphalt.RoughnessBias"));
    const auto* RoughnessAdd = FindAsphaltNode<UMaterialExpressionAdd>(
        Nodes, TEXT("Asphalt.DryRoughnessBias"));
    const auto* FinalRoughness =
        FindAsphaltNode<UMaterialExpressionSaturate>(
            Nodes, TEXT("Asphalt.FinalRoughness"));
    const auto* AoBlend =
        FindAsphaltNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Asphalt.AOAntiTileBlend"));
    const auto* NormalBlend =
        FindAsphaltNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Asphalt.NormalAntiTileBlend"));
    const auto* FlatNormal =
        FindAsphaltNode<UMaterialExpressionConstant3Vector>(
            Nodes, TEXT("Asphalt.FlatTangentNormal"));
    const auto* NormalStrength =
        FindAsphaltNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Asphalt.NormalStrength"));
    const auto* StrengthenedNormal =
        FindAsphaltNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Asphalt.ControlledNormalStrength"));
    const auto* FinalNormal = FindAsphaltNode<UMaterialExpressionNormalize>(
        Nodes, TEXT("Asphalt.FinalNormalizedNormal"));
    const auto* Specular = FindAsphaltNode<UMaterialExpressionConstant>(
        Nodes, TEXT("Asphalt.DielectricSpecular"));
    const auto* Metallic = FindAsphaltNode<UMaterialExpressionConstant>(
        Nodes, TEXT("Asphalt.NonMetal"));

    const auto ExactScalar = [](
        const UMaterialExpressionScalarParameter* Parameter,
        const TCHAR* Name,
        float Expected) -> bool
    {
        return Parameter && Parameter->ParameterName == Name &&
            Parameter->Group == TEXT("Istana Public Realm Visual R2") &&
            Parameter->SortPriority == 32 &&
            !Parameter->bUseCustomPrimitiveData &&
            Parameter->PrimitiveDataIndex == 0 &&
            FMath::IsNearlyEqual(Parameter->DefaultValue, Expected, 0.000001f);
    };
    const auto ExactVector = [](
        const UMaterialExpressionVectorParameter* Parameter,
        const TCHAR* Name,
        const FLinearColor& Expected) -> bool
    {
        return Parameter && Parameter->ParameterName == Name &&
            Parameter->Group == TEXT("Istana Public Realm Visual R2") &&
            Parameter->SortPriority == 32 &&
            !Parameter->bUseCustomPrimitiveData &&
            Parameter->PrimitiveDataIndex == 0 &&
            Parameter->DefaultValue.Equals(Expected, 0.000001f);
    };
    const auto ExactTexture = [](
        const UMaterialExpressionTextureSampleParameter2D* Sample,
        const TCHAR* Name,
        const UTexture2D* Texture) -> bool
    {
        return Sample && Sample->ParameterName == Name &&
            Sample->Group == TEXT("Istana Public Realm Visual R2") &&
            Sample->SortPriority == 32 &&
            Sample->Texture == Texture &&
            Sample->SamplerType == ExactSamplerType(Texture) &&
            Sample->SamplerSource == SSM_FromTextureAsset &&
            Sample->MipValueMode == TMVM_None &&
            Sample->ConstMipValue == 0 &&
            Sample->AutomaticViewMipBias;
    };
    const auto ExactCustom = [](
        const UMaterialExpressionCustom* Custom,
        const TCHAR* Description,
        const FString& Code,
        ECustomMaterialOutputType OutputType,
        const TCHAR* InputName,
        const UMaterialExpression* InputExpression) -> bool
    {
        return Custom && Custom->Description == Description &&
            Custom->Code == Code && Custom->OutputType == OutputType &&
            Custom->Inputs.Num() == 1 &&
            Custom->Inputs[0].InputName == InputName &&
            InputIs(Custom->Inputs[0].Input, InputExpression) &&
            Custom->AdditionalOutputs.IsEmpty() &&
            Custom->AdditionalDefines.IsEmpty() &&
            Custom->IncludeFilePaths.IsEmpty();
    };
    bool bCustomizedUvConnected = false;
    if (Data)
    {
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bCustomizedUvConnected |= CustomizedUv.Expression != nullptr;
        }
    }
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != AsphaltMaterialObjectPath || !Data ||
        Nodes.Num() != 39 ||
        !Resource || !ShaderMap || !Resource->IsCompilationFinished() ||
        !Resource->IsGameThreadShaderMapComplete() ||
        !bCompileStateAccepted || !ShaderMap->IsValidForRendering() ||
        Resource->IsDefaultMaterial() || !CompileErrors.IsEmpty() ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() || Material->bEnableTessellation ||
        Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
        Data->WorldPositionOffset.Expression || Data->Displacement.Expression ||
        Data->PixelDepthOffset.Expression || Data->MaterialAttributes.Expression ||
        Data->FrontMaterial.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->OpacityMask.Expression ||
        Data->Refraction.Expression || Data->SubsurfaceColor.Expression ||
        Data->ClearCoat.Expression || Data->ClearCoatRoughness.Expression ||
        Data->Anisotropy.Expression || Data->Tangent.Expression ||
        bCustomizedUvConnected ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression ||
        !Uv0 || Uv0->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(Uv0->UTiling, 1.0f) ||
        !FMath::IsNearlyEqual(Uv0->VTiling, 1.0f) ||
        Uv0->UnMirrorU || Uv0->UnMirrorV ||
        !ExactScalar(PrimaryMeters, TEXT("PrimaryTileMeters"),
            AsphaltPrimaryTileMeters) ||
        !ExactScalar(SecondaryMeters, TEXT("SecondaryTileMeters"),
            AsphaltSecondaryTileMeters) ||
        !ExactScalar(MacroMeters, TEXT("MacroTileMeters"),
            AsphaltMacroTileMeters) ||
        !ExactScalar(SecondaryBlend, TEXT("SecondaryBlend"),
            AsphaltSecondaryBlend) ||
        !ExactScalar(RoughnessBias, TEXT("RoughnessBias"),
            AsphaltRoughnessBias) ||
        !ExactScalar(NormalStrength, TEXT("NormalStrength"),
            AsphaltNormalStrength) ||
        !ExactVector(AlbedoTint, TEXT("AlbedoTint"), AsphaltAlbedoTint) ||
        !ExactVector(MacroTintLow, TEXT("MacroTintLow"),
            AsphaltMacroTintLow) ||
        !ExactVector(MacroTintHigh, TEXT("MacroTintHigh"),
            AsphaltMacroTintHigh) ||
        !PrimaryUv || !InputIs(PrimaryUv->A, Uv0) ||
        !InputIs(PrimaryUv->B, PrimaryMeters) ||
        !SecondaryUnrotated || !InputIs(SecondaryUnrotated->A, Uv0) ||
        !InputIs(SecondaryUnrotated->B, SecondaryMeters) ||
        !Rotation || !FMath::IsNearlyEqual(
            Rotation->R, AsphaltSecondaryRotationRadians, 0.000001f) ||
        !RotatedSecondary ||
        !InputIs(RotatedSecondary->Coordinate, SecondaryUnrotated) ||
        !InputIs(RotatedSecondary->Time, Rotation) ||
        !FMath::IsNearlyZero(RotatedSecondary->CenterX) ||
        !FMath::IsNearlyZero(RotatedSecondary->CenterY) ||
        !FMath::IsNearlyEqual(RotatedSecondary->Speed, 1.0f) ||
        RotatedSecondary->ConstCoordinate != 0 ||
        !SecondaryOffset || !FMath::IsNearlyEqual(
            SecondaryOffset->R, AsphaltSecondaryOffsetU) ||
        !FMath::IsNearlyEqual(
            SecondaryOffset->G, AsphaltSecondaryOffsetV) ||
        !SecondaryUv || !InputIs(SecondaryUv->A, RotatedSecondary) ||
        !InputIs(SecondaryUv->B, SecondaryOffset) ||
        !MacroUv || !InputIs(MacroUv->A, Uv0) ||
        !InputIs(MacroUv->B, MacroMeters) ||
        !ExactTexture(BasePrimary, TEXT("AsphaltBaseColorPrimary"),
            Textures.BaseColor) ||
        !ExactTexture(BaseSecondary, TEXT("AsphaltBaseColorSecondary"),
            Textures.BaseColor) ||
        !ExactTexture(NormalPrimary, TEXT("AsphaltNormalPrimary"),
            Textures.Normal) ||
        !ExactTexture(NormalSecondary, TEXT("AsphaltNormalSecondary"),
            Textures.Normal) ||
        !ExactTexture(OrdpPrimary, TEXT("AsphaltORDpPrimary"),
            Textures.Ordp) ||
        !ExactTexture(OrdpSecondary, TEXT("AsphaltORDpSecondary"),
            Textures.Ordp) ||
        !InputIs(BasePrimary->Coordinates, PrimaryUv) ||
        !InputIs(BaseSecondary->Coordinates, SecondaryUv) ||
        !InputIs(NormalPrimary->Coordinates, PrimaryUv) ||
        !InputIs(NormalSecondary->Coordinates, SecondaryUv) ||
        !InputIs(OrdpPrimary->Coordinates, PrimaryUv) ||
        !InputIs(OrdpSecondary->Coordinates, SecondaryUv) ||
        !BaseBlend || !InputIs(BaseBlend->A, BasePrimary) ||
        !InputIs(BaseBlend->B, BaseSecondary) ||
        !InputIs(BaseBlend->Alpha, SecondaryBlend) ||
        !TintedBase || !InputIs(TintedBase->A, BaseBlend) ||
        !InputIs(TintedBase->B, AlbedoTint) ||
        !ExactCustom(
            MacroField,
            TEXT("TRIAD_IPV5D_ASPHALT_MACRO_VALUE_NOISE_V1"),
            AsphaltMacroFieldCode,
            CMOT_Float1,
            TEXT("MacroUV"),
            MacroUv) ||
        !MacroTint || !InputIs(MacroTint->A, MacroTintLow) ||
        !InputIs(MacroTint->B, MacroTintHigh) ||
        !InputIs(MacroTint->Alpha, MacroField) ||
        !FinalBase || !InputIs(FinalBase->A, TintedBase) ||
        !InputIs(FinalBase->B, MacroTint) ||
        !RoughnessBlend ||
        !InputIs(RoughnessBlend->A, OrdpPrimary, 2) ||
        !InputIs(RoughnessBlend->B, OrdpSecondary, 2) ||
        !InputIs(RoughnessBlend->Alpha, SecondaryBlend) ||
        !RoughnessAdd || !InputIs(RoughnessAdd->A, RoughnessBlend) ||
        !InputIs(RoughnessAdd->B, RoughnessBias) ||
        !FinalRoughness ||
        !InputIs(FinalRoughness->Input, RoughnessAdd) ||
        !AoBlend || !InputIs(AoBlend->A, OrdpPrimary, 1) ||
        !InputIs(AoBlend->B, OrdpSecondary, 1) ||
        !InputIs(AoBlend->Alpha, SecondaryBlend) ||
        !ExactCustom(
            RotatedNormal,
            TEXT("TRIAD_IPV5D_ASPHALT_REORIENT_ROTATED_NORMAL_37_DEGREES_V1"),
            AsphaltRotatedNormalCode,
            CMOT_Float3,
            TEXT("RotatedNormal"),
            NormalSecondary) ||
        !NormalBlend || !InputIs(NormalBlend->A, NormalPrimary) ||
        !InputIs(NormalBlend->B, RotatedNormal) ||
        !InputIs(NormalBlend->Alpha, SecondaryBlend) ||
        !FlatNormal ||
        !FlatNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f, 1.0f), 0.000001f) ||
        !StrengthenedNormal ||
        !InputIs(StrengthenedNormal->A, FlatNormal) ||
        !InputIs(StrengthenedNormal->B, NormalBlend) ||
        !InputIs(StrengthenedNormal->Alpha, NormalStrength) ||
        !FinalNormal ||
        !InputIs(FinalNormal->VectorInput, StrengthenedNormal) ||
        !Specular || !FMath::IsNearlyEqual(
            Specular->R, AsphaltSpecular, 0.000001f) ||
        !Metallic || !FMath::IsNearlyZero(Metallic->R) ||
        !InputIs(Data->BaseColor, FinalBase) ||
        !InputIs(Data->Roughness, FinalRoughness) ||
        !InputIs(Data->AmbientOcclusion, AoBlend) ||
        !InputIs(Data->Normal, FinalNormal) ||
        !InputIs(Data->Specular, Specular) ||
        !InputIs(Data->Metallic, Metallic))
    {
        OutError = FString::Printf(
            TEXT("The V5D public-realm asphalt lost its exact dual-scale metric anti-tiling, macro-value-noise, PBR texture, dry roughness, controlled-normal, Nanite, no-displacement, or active-feature-level compiled contract. nodes=%d compileErrors=[%s]."),
            Nodes.Num(),
            *FString::Join(CompileErrors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterialInstanceConstant* CreateConcreteMaterial(
    IAssetTools& AssetTools,
    UMaterialInstanceConstant* Parent,
    FString& OutError)
{
    if (!ValidateParentMaterial(Parent, OutError))
    {
        return nullptr;
    }
    UMaterialInstanceConstantFactoryNew* Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (Factory)
    {
        Factory->InitialParent = Parent;
    }
    UMaterialInstanceConstant* Instance = Factory
        ? Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
              ConcreteMaterialName,
              MaterialRoot,
              UMaterialInstanceConstant::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DPublicRealmAssets"))))
        : nullptr;
    if (!Instance)
    {
        OutError = TEXT("Could not create the exact V5D public-realm concrete visual MIC.");
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Instance->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    for (const FScalarOverrideSpec& Spec : ConcreteScalarOverrides)
    {
        Instance->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(FName(Spec.Name)), Spec.Value);
    }
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(TEXT("LookdevTint")),
        ConcreteLookdevTint);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    OutError.Reset();
    return Instance;
}

bool SameNames(const TSet<FName>& Expected, const TSet<FName>& Actual)
{
    if (Expected.Num() != Actual.Num())
    {
        return false;
    }
    for (const FName& Name : Expected)
    {
        if (!Actual.Contains(Name))
        {
            return false;
        }
    }
    return true;
}

bool ValidateConcreteMaterial(
    UMaterialInstanceConstant* Instance,
    FString& OutError)
{
    const FStaticParameterSet StaticParameters = Instance
        ? Instance->GetStaticParameters()
        : FStaticParameterSet();
    const FString BaseOverrides = Instance
        ? EnabledBasePropertyOverrideNames(Instance->BasePropertyOverrides)
        : TEXT("instance-null");
    if (!Instance ||
        Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
        Instance->GetPathName() != ConcreteMaterialObjectPath ||
        !Instance->Parent ||
        Instance->Parent->GetPathName() != ParentMaterialObjectPath ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Instance->GetNaniteOverride() ||
        !Instance->TextureParameterValues.IsEmpty() ||
        !Instance->DoubleVectorParameterValues.IsEmpty() ||
        !Instance->TextureCollectionParameterValues.IsEmpty() ||
        !Instance->RuntimeVirtualTextureParameterValues.IsEmpty() ||
        !Instance->SparseVolumeTextureParameterValues.IsEmpty() ||
        !Instance->FontParameterValues.IsEmpty() ||
        !Instance->UserSceneTextureOverrides.IsEmpty() ||
        !StaticParameters.StaticSwitchParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.StaticComponentMaskParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.TerrainLayerWeightParameters.IsEmpty() ||
        StaticParameters.bHasMaterialLayers || !BaseOverrides.IsEmpty())
    {
        OutError = TEXT("The V5D public-realm concrete visual MIC lost its exact class, path, parent, parameter-only, no-static-override contract.");
        return false;
    }

    TSet<FName> ExpectedScalarNames;
    TSet<FName> ActualScalarNames;
    for (const FScalarOverrideSpec& Spec : ConcreteScalarOverrides)
    {
        ExpectedScalarNames.Add(FName(Spec.Name));
    }
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualScalarNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("The V5D concrete MIC has an invalid or duplicate scalar override.");
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    const TSet<FName> ExpectedVectorNames = {
        FName(TEXT("LookdevTint"))};
    TSet<FName> ActualVectorNames;
    for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("The V5D concrete MIC has an invalid or duplicate vector override.");
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameNames(ExpectedScalarNames, ActualScalarNames) ||
        !SameNames(ExpectedVectorNames, ActualVectorNames))
    {
        OutError = TEXT("The V5D concrete MIC override roster changed.");
        return false;
    }
    for (const FScalarOverrideSpec& Spec : ConcreteScalarOverrides)
    {
        float Actual = 0.0f;
        if (!Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(FName(Spec.Name)),
                Actual,
                true) ||
            !FMath::IsNearlyEqual(Actual, Spec.Value, 0.000001f))
        {
            OutError = TEXT("The V5D concrete MIC scalar changed: ") +
                FString(Spec.Name);
            return false;
        }
    }
    FLinearColor ActualTint;
    if (!Instance->GetVectorParameterValue(
            FHashedMaterialParameterInfo(FName(TEXT("LookdevTint"))),
            ActualTint,
            true) ||
        !ActualTint.Equals(ConcreteLookdevTint, 0.000001f))
    {
        OutError = TEXT("The V5D concrete MIC LookdevTint changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

UAssetImportTask* MakeImportTask(const FMeshSpec& Spec)
{
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    if (!Options || !Factory || !Task || !Options->StaticMeshImportData)
    {
        return nullptr;
    }
    Options->bImportAsSkeletal = false;
    Options->MeshTypeToImport = FBXIT_StaticMesh;
    Options->bAutomatedImportShouldDetectType = false;
    Options->bImportMesh = true;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->StaticMeshImportData->bConvertScene = false;
    Options->StaticMeshImportData->bConvertSceneUnit = false;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    // The OBJ encodes centimetres and is already preconditioned for the
    // legacy importer's Y reflection. No second scale or mirror is allowed.
    Options->StaticMeshImportData->ImportUniformScale = 1.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
    Options->StaticMeshImportData->bRemoveDegenerates = false;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = MeshSourcePath(Spec);
    Task->DestinationPath = AssetRoot;
    Task->DestinationName = *Spec.Name;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool ValidateImportTask(
    const UAssetImportTask* Task,
    const FMeshSpec& Spec,
    FString& OutError)
{
    const UFbxImportUI* Options =
        Task ? Cast<UFbxImportUI>(Task->Options) : nullptr;
    const UFbxStaticMeshImportData* Data =
        Options ? Options->StaticMeshImportData : nullptr;
    if (!Task || !Options || !Data ||
        !FPaths::IsSamePath(Task->Filename, MeshSourcePath(Spec)) ||
        Task->DestinationPath != AssetRoot ||
        Task->DestinationName != *Spec.Name || Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType || !Options->bImportMesh ||
        Options->bImportMaterials || Options->bImportTextures ||
        Data->bConvertScene || Data->bConvertSceneUnit ||
        Data->bForceFrontXAxis ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 1.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ImportNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        !Data->bBuildNanite || Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The exact V5D public-realm centimetre/legacy-Y/explicit-normal OBJ import policy changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

void MakeRenderOnly(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    Mesh->Modify();
    if (Mesh->GetNumSourceModels() == 1)
    {
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
        SourceModel.BuildSettings.BuildScale3D = FVector::OneVector;
        SourceModel.BuildSettings.bGenerateLightmapUVs = false;
        SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
        SourceModel.BuildSettings.bRecomputeNormals = false;
        SourceModel.BuildSettings.bRecomputeTangents = true;
        SourceModel.BuildSettings.bUseMikkTSpace = true;
        SourceModel.BuildSettings.bRemoveDegenerates = false;
    }
    Mesh->SetLightMapCoordinateIndex(0);
    Mesh->NaniteSettings.bEnabled = true;
    Mesh->NaniteSettings.KeepPercentTriangles = 1.0f;
    Mesh->NaniteSettings.TrimRelativeError = 0.0f;
    Mesh->NaniteSettings.FallbackTarget =
        ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        // An empty simple aggregate plus SimpleAsComplex also prevents the
        // source triangle mesh becoming query collision if a future component
        // accidentally enables collision.
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
}

bool NormalizeAndBindMaterials(
    UStaticMesh* Mesh,
    const FMeshSpec& Spec,
    const TArray<UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Materials.Num() != ExpectedMaterialCount ||
        Materials.Contains(nullptr) ||
        Mesh->GetStaticMaterials().Num() != Spec.MaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != Spec.SectionCount)
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm '%s' normalization requires the exact %d-slot/%d-section imported mesh."),
            **Spec.Name,
            Spec.MaterialCount,
            Spec.SectionCount);
        return false;
    }

    TMap<FName, int32> CanonicalOrder;
    for (int32 Index = 0; Index < Spec.MaterialCount; ++Index)
    {
        CanonicalOrder.Add(FName(MaterialSlotNames[Index]), Index);
    }
    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(Spec.MaterialCount);
    TArray<int32> ImportedToCanonical;
    ImportedToCanonical.Init(INDEX_NONE, Spec.MaterialCount);
    TArray<bool> SeenCanonical;
    SeenCanonical.Init(false, Spec.MaterialCount);
    for (int32 ImportedIndex = 0;
         ImportedIndex < ImportedMaterials.Num();
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const int32* CanonicalIndex =
            CanonicalOrder.Find(Imported.MaterialSlotName);
        if (!CanonicalIndex || Imported.MaterialSlotName.IsNone() ||
            Imported.MaterialSlotName != Imported.ImportedMaterialSlotName ||
            SeenCanonical[*CanonicalIndex])
        {
            OutError = FString::Printf(
                TEXT("V5D public-realm '%s' OBJ slots are not an exact unique canonical permutation at imported index %d: slot='%s' imported='%s'."),
                **Spec.Name,
                ImportedIndex,
                *Imported.MaterialSlotName.ToString(),
                *Imported.ImportedMaterialSlotName.ToString());
            return false;
        }
        SeenCanonical[*CanonicalIndex] = true;
        ImportedToCanonical[ImportedIndex] = *CanonicalIndex;
        OrderedMaterials[*CanonicalIndex] = Imported;
        OrderedMaterials[*CanonicalIndex].MaterialSlotName =
            FName(MaterialSlotNames[*CanonicalIndex]);
        OrderedMaterials[*CanonicalIndex].ImportedMaterialSlotName =
            FName(MaterialSlotNames[*CanonicalIndex]);
        OrderedMaterials[*CanonicalIndex].MaterialInterface =
            Materials[*CanonicalIndex];
    }

    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    TArray<FString> Digest;
    for (int32 SectionIndex = 0;
         SectionIndex < Spec.SectionCount;
         ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection =
            Lod.Sections[SectionIndex];
        const FImportedSectionSpec& Expected = Spec.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        const int32 CanonicalIndex =
            ImportedToCanonical.IsValidIndex(ImportedIndex)
            ? ImportedToCanonical[ImportedIndex]
            : INDEX_NONE;
        Digest.Add(FString::Printf(
            TEXT("%d:%d:%d:%u"),
            SectionIndex,
            ImportedIndex,
            CanonicalIndex,
            RenderSection.NumTriangles));
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (CanonicalIndex != Expected.MaterialIndex ||
            RenderSection.NumTriangles != Expected.Triangles ||
            Section.MaterialIndex != ImportedIndex ||
            Original.MaterialIndex != ImportedIndex)
        {
            OutError = FString::Printf(
                TEXT("V5D public-realm '%s' semantic section '%s' changed: [%s]."),
                **Spec.Name,
                Expected.Label,
                *FString::Join(Digest, TEXT(",")));
            return false;
        }
        Section.MaterialIndex = CanonicalIndex;
        Original.MaterialIndex = CanonicalIndex;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }

    Mesh->Modify();
    Mesh->SetStaticMaterials(OrderedMaterials);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

bool ValidateUv0(
    const FStaticMeshLODResources& Lod,
    const FMeshSpec& Spec,
    FString& OutError)
{
    const FStaticMeshVertexBuffer& Buffer =
        Lod.VertexBuffers.StaticMeshVertexBuffer;
    if (Buffer.GetNumVertices() == 0 || Buffer.GetNumTexCoords() != 1)
    {
        OutError = TEXT("A V5D public-realm mesh lost its exact one-channel imported UV0 structure.");
        return false;
    }
    FVector2f Minimum(FLT_MAX, FLT_MAX);
    FVector2f Maximum(-FLT_MAX, -FLT_MAX);
    for (uint32 VertexIndex = 0;
         VertexIndex < Buffer.GetNumVertices();
         ++VertexIndex)
    {
        const FVector2f Uv = Buffer.GetVertexUV(VertexIndex, 0);
        if (!FMath::IsFinite(Uv.X) || !FMath::IsFinite(Uv.Y))
        {
            OutError = TEXT("A V5D public-realm UV0 contains a non-finite value.");
            return false;
        }
        Minimum.X = FMath::Min(Minimum.X, Uv.X);
        Minimum.Y = FMath::Min(Minimum.Y, Uv.Y);
        Maximum.X = FMath::Max(Maximum.X, Uv.X);
        Maximum.Y = FMath::Max(Maximum.Y, Uv.Y);
    }
    if (!Minimum.Equals(Spec.UvMinSourceMetres, 0.002f) ||
        !Maximum.Equals(Spec.UvMaxSourceMetres, 0.002f))
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm '%s' source-metre UV0 bounds drifted: min=(%.6f,%.6f) max=(%.6f,%.6f)."),
            **Spec.Name,
            Minimum.X,
            Minimum.Y,
            Maximum.X,
            Maximum.Y);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshDescriptionCensus(
    const FMeshDescription& Description,
    const FMeshSpec& Spec,
    FString& OutError)
{
    if (Description.Vertices().Num() != Spec.VertexCount ||
        Description.VertexInstances().Num() != Spec.VertexInstanceCount ||
        Description.Triangles().Num() != Spec.TriangleCount ||
        Description.Polygons().Num() != Spec.TriangleCount ||
        Description.PolygonGroups().Num() != Spec.MaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm '%s' mesh-description census changed: vertices=%d vertexInstances=%d triangles=%d polygons=%d polygonGroups=%d."),
            **Spec.Name,
            Description.Vertices().Num(),
            Description.VertexInstances().Num(),
            Description.Triangles().Num(),
            Description.Polygons().Num(),
            Description.PolygonGroups().Num());
        return false;
    }

    const FStaticMeshConstAttributes Attributes(Description);
    const TPolygonGroupAttributesConstRef<FName> PolygonGroupSlots =
        Attributes.GetPolygonGroupMaterialSlotNames();
    TMap<FName, int32> ExpectedMaterialTriangles;
    TMap<FName, int32> ActualMaterialTriangles;
    for (int32 SectionIndex = 0;
         SectionIndex < Spec.SectionCount;
         ++SectionIndex)
    {
        const FImportedSectionSpec& Section = Spec.Sections[SectionIndex];
        ExpectedMaterialTriangles.FindOrAdd(
            FName(MaterialSlotNames[Section.MaterialIndex])) +=
            Section.Triangles;
    }
    for (const FTriangleID TriangleId :
         Description.Triangles().GetElementIDs())
    {
        const FPolygonGroupID GroupId =
            Description.GetTrianglePolygonGroup(TriangleId);
        const FName SlotName = PolygonGroupSlots[GroupId];
        ActualMaterialTriangles.FindOrAdd(SlotName) += 1;
    }
    if (ActualMaterialTriangles.Num() != ExpectedMaterialTriangles.Num())
    {
        OutError = TEXT("A V5D public-realm mesh-description material topology changed.");
        return false;
    }
    for (const TPair<FName, int32>& Expected : ExpectedMaterialTriangles)
    {
        const int32* Actual = ActualMaterialTriangles.Find(Expected.Key);
        if (!Actual || *Actual != Expected.Value)
        {
            OutError = FString::Printf(
                TEXT("V5D public-realm '%s' mesh-description slot '%s' triangle census changed: expected=%d actual=%d."),
                **Spec.Name,
                *Expected.Key.ToString(),
                Expected.Value,
                Actual ? *Actual : INDEX_NONE);
            return false;
        }
    }

    const TVertexInstanceAttributesConstRef<FVector3f> Normals =
        Attributes.GetVertexInstanceNormals();
    for (const FVertexInstanceID VertexInstanceId :
         Description.VertexInstances().GetElementIDs())
    {
        const FVector3f Normal = Normals[VertexInstanceId];
        if (Normal.ContainsNaN() ||
            !FMath::IsNearlyEqual(Normal.SizeSquared(), 1.0f, 0.002f))
        {
            OutError = TEXT("A V5D public-realm imported explicit normal is non-finite or non-unit.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

int32 TriangleCount(const UStaticMesh* Mesh)
{
    const FStaticMeshRenderData* Data = Mesh ? Mesh->GetRenderData() : nullptr;
    return Data && Data->LODResources.Num() == 1
        ? Data->LODResources[0].GetNumTriangles()
        : INDEX_NONE;
}

bool ValidatePersistedImportSettings(
    const UFbxStaticMeshImportData* Data,
    FString& OutError)
{
    if (!Data || Data->bConvertScene || Data->bConvertSceneUnit ||
        Data->bForceFrontXAxis ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 1.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ImportNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        !Data->bBuildNanite || Data->bRemoveDegenerates)
    {
        OutError = TEXT("A V5D public-realm mesh lost its persisted identity-scale, legacy-Y-preconditioned, explicit-normal import settings.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMesh(
    UStaticMesh* Mesh,
    const FMeshSpec& Spec,
    const TArray<UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData =
        Mesh ? Mesh->GetAssetImportData() : nullptr;
    const UFbxStaticMeshImportData* StaticImportData =
        Cast<UFbxStaticMeshImportData>(ImportData);
    const TArray<FString> Sources =
        ImportData ? ImportData->ExtractFilenames() : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = MeshSourcePath(Spec);
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != *Spec.ObjectPath || Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        Mesh->GetNumSourceModels() != 1 || !Description ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        Mesh->GetSourceModel(0).BuildSettings.bRecomputeNormals ||
        !Mesh->GetSourceModel(0).BuildSettings.bRecomputeTangents ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseMikkTSpace ||
        Mesh->GetSourceModel(0).BuildSettings.bRemoveDegenerates ||
        Mesh->GetLightMapCoordinateIndex() != 0 ||
        TriangleCount(Mesh) != Spec.TriangleCount ||
        Mesh->GetStaticMaterials().Num() != Spec.MaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != Spec.SectionCount ||
        !Mesh->NaniteSettings.bEnabled ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError))
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm '%s' lost its exact source, identity build scale, explicit-normal/Mikk-tangent settings, one-LOD topology, ordered slots, or full-fidelity Nanite/fallback state."),
            **Spec.Name);
        return false;
    }
    if (!ValidatePersistedImportSettings(StaticImportData, OutError))
    {
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (!Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm '%s' must have an empty default body setup, no collision geometry, and no navigation data."),
            **Spec.Name);
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(Spec.BoundsMinCentimetres, 0.25) ||
        !BoundsMax.Equals(Spec.BoundsMaxCentimetres, 0.25))
    {
        OutError = FString::Printf(
            TEXT("V5D public-realm '%s' imported centimetre/Y-reflected bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            **Spec.Name,
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }
    if (!ValidateMeshDescriptionCensus(*Description, Spec, OutError) ||
        !ValidateUv0(RenderData->LODResources[0], Spec, OutError))
    {
        return false;
    }

    TArray<FString> SectionDigest;
    for (int32 SectionIndex = 0;
         SectionIndex < Spec.SectionCount;
         ++SectionIndex)
    {
        const FStaticMeshSection& Section =
            RenderData->LODResources[0].Sections[SectionIndex];
        const FImportedSectionSpec& Expected = Spec.Sections[SectionIndex];
        SectionDigest.Add(FString::Printf(
            TEXT("%d:%d:%u"),
            SectionIndex,
            Section.MaterialIndex,
            Section.NumTriangles));
        if (Section.MaterialIndex != Expected.MaterialIndex ||
            Section.NumTriangles != Expected.Triangles ||
            Mesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Expected.MaterialIndex ||
            Mesh->GetOriginalSectionInfoMap()
                    .Get(0, SectionIndex)
                    .MaterialIndex != Expected.MaterialIndex)
        {
            OutError = FString::Printf(
                TEXT("V5D public-realm '%s' section:material:triangle contract changed at '%s': [%s]."),
                **Spec.Name,
                Expected.Label,
                *FString::Join(SectionDigest, TEXT(",")));
            return false;
        }
    }
    for (int32 Slot = 0; Slot < Spec.MaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[Slot];
        if (!Materials.IsValidIndex(Slot) || !Materials[Slot] ||
            StaticMaterial.MaterialSlotName !=
                FName(MaterialSlotNames[Slot]) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(MaterialSlotNames[Slot]) ||
            StaticMaterial.MaterialInterface != Materials[Slot] ||
            StaticMaterial.MaterialInterface->GetPathName() !=
                OrderedSemanticMaterialPaths()[Slot])
        {
            OutError = FString::Printf(
                TEXT("V5D public-realm '%s' ordered semantic material binding drifted at slot %d."),
                **Spec.Name,
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateLegacyInternal(bool bRequireSaved, FString& OutReport)
{
    TArray<UMaterialInstanceConstant*> RoadMaterials;
    UMaterialInstanceConstant* Parent = nullptr;
    if (!ValidateAllSourceHashes(OutReport) ||
        !ValidateLegacyMaterialDependencies(RoadMaterials, Parent, OutReport) ||
        !ValidateExactRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UMaterialInstanceConstant* Concrete =
        LoadExact<UMaterialInstanceConstant>(ConcreteMaterialObjectPath);
    if (!ValidateConcreteMaterial(Concrete, OutReport))
    {
        return false;
    }
    TArray<UMaterialInstanceConstant*> Materials = RoadMaterials;
    Materials.Add(Concrete);
    if (!ValidateMesh(
            LoadExact<UStaticMesh>(CoreMeshObjectPath),
            CoreMeshSpec,
            Materials,
            OutReport) ||
        !ValidateMesh(
            LoadExact<UStaticMesh>(FallbackMeshObjectPath),
            FallbackMeshSpec,
            Materials,
            OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_PUBLIC_REALM_LEGACY_ASSETS_VALID assets=3 meshes=2 concreteMics=1 sourceEpoch=2026-08-25 coreTriangles=%d fallbackTriangles=%d totalTriangles=%d sourceCorners=%d coreSlots=[MI_IPV5C_OfficialPlanningRoadZone] fallbackSlots=[MI_IPV5C_OfficialPlanningRoadZone,MI_IPV5C_OfficialPlanningRoadGraphic,MI_IPV5D_PublicRealmConcrete] fallbackSectionTriangles=[%d,%d,%d] sourceConcreteGroups=[sidewalk:%d,kerbTop:%d,kerbWall:%d] importUniformScale=1.0 encodedObjUnits=centimetres legacyObjYPreconditioned=true buildScaleIdentity=true explicitNormals=true mikkTangents=true sourceUv0Only=true fullPrecisionUv=true coreObjSha256=%s fallbackObjSha256=%s manifestSha256=%s acceptanceLockSha256=%s contractSha256=%s generatorSha256=%s lockedSetSha256=%s primaryOutputSetSha256=%s immutableV5CSourceBindings=true concreteParent=MI_IPV5_HardscapeStone physicalMaterialClaimed=false collision=false navigation=false renderOnly=true notSurveyAsBuiltCurrentCompleteSensorOrRfAuthority=true."),
        ExpectedCoreTriangleCount,
        ExpectedFallbackTriangleCount,
        ExpectedCoreTriangleCount + ExpectedFallbackTriangleCount,
        ExpectedCoreVertexCount + ExpectedFallbackVertexCount,
        ExpectedFallbackRoadBaseTriangles,
        ExpectedFallbackRoadGraphicTriangles,
        ExpectedFallbackConcreteTriangles,
        ExpectedFallbackSidewalkTriangles,
        ExpectedFallbackKerbTopTriangles,
        ExpectedFallbackKerbWallTriangles,
        *ExpectedCoreObjSha256,
        *ExpectedFallbackObjSha256,
        *ExpectedManifestSha256,
        *ExpectedAcceptanceLockSha256,
        *ExpectedContractSha256,
        *ExpectedGeneratorSha256,
        *ExpectedLockedSetSha256,
        *ExpectedPrimaryOutputSetSha256);
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    FString LegacyReport;
    FAsphaltTextureSet Textures;
    if (!ValidateLegacyInternal(bRequireSaved, LegacyReport) ||
        !ValidateAsphaltTextureDependencies(Textures, OutReport) ||
        !ValidateExactVisualMaterialRoster(bRequireSaved, OutReport) ||
        !ValidateAsphaltMaterial(
            LoadExact<UMaterial>(AsphaltMaterialObjectPath),
            Textures,
            OutReport) ||
        !ValidateRoadGraphicSuppressionMaterial(
            LoadExact<UMaterial>(RoadGraphicSuppressionMaterialObjectPath),
            OutReport))
    {
        if (OutReport.IsEmpty())
        {
            OutReport = LegacyReport;
        }
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_PUBLIC_REALM_ASSETS_VALID assets=5 legacyMeshes=2 concreteMics=1 isolatedVisualMaterials=2 pbrAsphaltMaterials=1 fullyClippedRoadGraphicMaterials=1 metricUvAntiTiling=true primaryTileMeters=2.0 secondaryTileMeters=3.37 secondaryRotationDegrees=37 macroTileMeters=37.0 pbrTextures=D,N,ORDp texturePackagesSha256Pinned=true ordpChannels=ao:R,roughness:G displacementConsumed=false normalStrength=0.42 roughnessBias=0.04 specular=0.25 nanite=true wpo=false displacement=false pdo=false immutableMeshPackages=true immutableV5CSourceBindings=true componentOverridePresentation=true fullyClippedPlanningGraphic=true laneOrCrossingPaintAuthored=false collision=false navigation=false renderOnly=true sensorRfAuthority=false. ") +
        LegacyReport;
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DPublicRealmAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }

const FString& GetCoreMeshObjectPath() { return CoreMeshObjectPath; }

const FString& GetFallbackMeshObjectPath()
{
    return FallbackMeshObjectPath;
}

const FString& GetConcreteMaterialObjectPath()
{
    return ConcreteMaterialObjectPath;
}

const FString& GetAsphaltMaterialObjectPath()
{
    return AsphaltMaterialObjectPath;
}

const FString& GetRoadGraphicSuppressionMaterialObjectPath()
{
    return RoadGraphicSuppressionMaterialObjectPath;
}

const TArray<FString>& GetOrderedSemanticMaterialObjectPaths()
{
    return OrderedSemanticMaterialPaths();
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    if (!ValidateAllSourceHashes(OutError))
    {
        return false;
    }
    TArray<UMaterialInstanceConstant*> RoadMaterials;
    UMaterialInstanceConstant* Parent = nullptr;
    FAsphaltTextureSet AsphaltTextures;
    if (!ValidateLegacyMaterialDependencies(
            RoadMaterials,
            Parent,
            OutError) ||
        !ValidateAsphaltTextureDependencies(AsphaltTextures, OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    TArray<FAssetData> ExistingVisual;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty() ||
        !GatherVisualMaterialAssets(ExistingVisual, OutError) ||
        !ExistingVisual.IsEmpty())
    {
        OutError = TEXT("CreateFreshAssets requires empty exact V5D public-realm legacy and Visual R2 roots.");
        return false;
    }

    FScopedFreshRollback Rollback(OutAssets, OutError, false);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterialInstanceConstant* Concrete =
        CreateConcreteMaterial(AssetTools, Parent, OutError);
    if (!Concrete)
    {
        return false;
    }
    OutAssets.Add(Concrete);
    UMaterial* Asphalt =
        CreateAsphaltMaterial(AssetTools, AsphaltTextures, OutError);
    if (!Asphalt || !ValidateAsphaltMaterial(
            Asphalt,
            AsphaltTextures,
            OutError))
    {
        return false;
    }
    OutAssets.Add(Asphalt);
    UMaterial* RoadGraphicSuppression =
        CreateRoadGraphicSuppressionMaterial(AssetTools, OutError);
    if (!RoadGraphicSuppression ||
        !ValidateRoadGraphicSuppressionMaterial(
            RoadGraphicSuppression,
            OutError))
    {
        return false;
    }
    OutAssets.Add(RoadGraphicSuppression);
    TArray<UMaterialInstanceConstant*> Materials = RoadMaterials;
    Materials.Add(Concrete);

    UAssetImportTask* CoreTask = MakeImportTask(CoreMeshSpec);
    UAssetImportTask* FallbackTask = MakeImportTask(FallbackMeshSpec);
    if (!CoreTask || !FallbackTask ||
        !ValidateImportTask(CoreTask, CoreMeshSpec, OutError) ||
        !ValidateImportTask(FallbackTask, FallbackMeshSpec, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate both exact V5D public-realm OBJ import tasks.");
        }
        return false;
    }
    AssetTools.ImportAssetTasks({CoreTask, FallbackTask});

    const auto ResolveImportedMesh = [](
        UAssetImportTask* Task,
        const FString& ExpectedPath) -> UStaticMesh*
    {
        for (UObject* Object : Task->GetObjects())
        {
            if (Object && Object->GetPathName() == ExpectedPath)
            {
                return Cast<UStaticMesh>(Object);
            }
        }
        return LoadExact<UStaticMesh>(ExpectedPath);
    };
    UStaticMesh* Core = ResolveImportedMesh(CoreTask, CoreMeshObjectPath);
    UStaticMesh* Fallback = ResolveImportedMesh(
        FallbackTask, FallbackMeshObjectPath);
    MakeRenderOnly(Core);
    MakeRenderOnly(Fallback);
    if (!NormalizeAndBindMaterials(
            Core, CoreMeshSpec, Materials, OutError) ||
        !NormalizeAndBindMaterials(
            Fallback, FallbackMeshSpec, Materials, OutError) ||
        !ValidateMesh(Core, CoreMeshSpec, Materials, OutError) ||
        !ValidateMesh(Fallback, FallbackMeshSpec, Materials, OutError))
    {
        return false;
    }
    OutAssets.Add(Core);
    OutAssets.Add(Fallback);

    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Validation;
    if (OutAssets.Num() != ExpectedAssetCount ||
        OutAssets.Contains(nullptr) ||
        !ValidateInternal(false, Validation))
    {
        OutError = TEXT("Fresh V5D public-realm validation failed before save: ") +
            Validation;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool CreateFreshVisualMaterialUpgrade(
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    OutAssets.Reset();
    FString LegacyReport;
    FAsphaltTextureSet Textures;
    TArray<FAssetData> ExistingVisual;
    if (!ValidateLegacyInternal(true, LegacyReport) ||
        !ValidateAsphaltTextureDependencies(Textures, OutError) ||
        !GatherVisualMaterialAssets(ExistingVisual, OutError) ||
        !ExistingVisual.IsEmpty())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Visual R2 upgrade requires the exact clean persisted three-asset predecessor and an empty isolated material root. ") +
                LegacyReport;
        }
        return false;
    }

    FScopedFreshRollback Rollback(OutAssets, OutError, true);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Asphalt = CreateAsphaltMaterial(
        AssetTools,
        Textures,
        OutError);
    if (!Asphalt || !ValidateAsphaltMaterial(Asphalt, Textures, OutError))
    {
        return false;
    }
    OutAssets.Add(Asphalt);
    UMaterial* RoadGraphicSuppression =
        CreateRoadGraphicSuppressionMaterial(AssetTools, OutError);
    if (!RoadGraphicSuppression ||
        !ValidateRoadGraphicSuppressionMaterial(
            RoadGraphicSuppression,
            OutError))
    {
        return false;
    }
    OutAssets.Add(RoadGraphicSuppression);
    FString Validation;
    if (!ValidateInternal(false, Validation))
    {
        OutError = TEXT("Fresh V5D public-realm Visual R2 validation failed before save: ") +
            Validation;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateLegacyAssets(FString& OutReport)
{
    return ValidateLegacyInternal(true, OutReport);
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}

bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DPublicRealmAssets& OutAssets,
    FTRIADIstanaExploreV5DPublicRealmProvenance& OutProvenance,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DPublicRealmAssets();
    OutProvenance = FTRIADIstanaExploreV5DPublicRealmProvenance();
    FString Report;
    if (!ValidateAssets(Report))
    {
        OutError = TEXT("V5D public-realm runtime contract refused unvalidated assets: ") +
            Report;
        return false;
    }

    OutAssets.CoreMesh = LoadExact<UStaticMesh>(CoreMeshObjectPath);
    OutAssets.FallbackMesh = LoadExact<UStaticMesh>(FallbackMeshObjectPath);
    OutAssets.RoadBaseMaterial =
        LoadExact<UMaterialInterface>(AsphaltMaterialObjectPath);
    OutAssets.RoadGraphicMaterial =
        LoadExact<UMaterialInterface>(
            RoadGraphicSuppressionMaterialObjectPath);
    OutAssets.ConcreteMaterial =
        LoadExact<UMaterialInterface>(ConcreteMaterialObjectPath);

    OutProvenance.SchemaRevision =
        ATRIADIstanaExploreV5DPublicRealmActor::
            ExpectedProvenanceSchemaRevision();
    OutProvenance.SourceEpoch = TEXT("2026-08-25");
    OutProvenance.CoreSourceIdentifier = CoreSourceIdentifier;
    OutProvenance.CoreSourceSha256 = ExpectedCoreObjSha256;
    OutProvenance.FallbackSourceIdentifier = FallbackSourceIdentifier;
    OutProvenance.FallbackSourceSha256 = ExpectedFallbackObjSha256;
    OutProvenance.bRenderOnly = true;
    OutProvenance.bMeasuredSurveyOrAsBuiltClaimed = false;
    OutProvenance.bCollisionNavigationSensorOrRfAuthority = false;
    OutProvenance.bProviderContentBakedCachedTracedOrAnalysed = false;
    OutProvenance.bR15GroundMaterialPackagesUntouched = true;
    OutProvenance.bExistingRfInputsUntouched = true;

    if (!OutAssets.CoreMesh || !OutAssets.FallbackMesh ||
        !OutAssets.RoadBaseMaterial || !OutAssets.RoadGraphicMaterial ||
        !OutAssets.ConcreteMaterial ||
        !ATRIADIstanaExploreV5DPublicRealmActor::
            ValidateProvenanceContract(OutProvenance, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D public-realm runtime contract could not load its exact admitted assets.");
        }
        OutAssets = FTRIADIstanaExploreV5DPublicRealmAssets();
        OutProvenance = FTRIADIstanaExploreV5DPublicRealmProvenance();
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DPublicRealmAssetFactory
