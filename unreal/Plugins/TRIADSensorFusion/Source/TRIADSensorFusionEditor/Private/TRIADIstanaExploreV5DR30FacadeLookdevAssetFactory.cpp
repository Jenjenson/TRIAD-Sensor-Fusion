#include "TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHIFeatureLevel.h"
#include "ShaderCompiler.h"
#include "Ssl.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
#include "UObject/UObjectGlobals.h"

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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsLookdevR30"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_R30_ContextFacadePBR_Master"));
const FString MasterObjectPath(
    MaterialRoot + TEXT("/") + MasterName + TEXT(".") + MasterName);
const FString PublicViewTextureRoot(
    TEXT("/Game/TRIAD/IstanaPublicView/Textures"));

const FName BaseColorTextureParameter(TEXT("BaseColorTexture"));
const FName NormalTextureParameter(TEXT("NormalTexture"));
const FName PackedOrmTextureParameter(TEXT("PackedORMTexture"));
const FName UvScaleParameter(TEXT("UvScale"));
const FName TextureInfluenceParameter(TEXT("TextureInfluence"));
const FName NormalStrengthParameter(TEXT("NormalStrength"));
const FName RoughnessParameter(TEXT("Roughness"));
const FName RoughnessTextureWeightParameter(TEXT("RoughnessTextureWeight"));
const FName MetallicParameter(TEXT("Metallic"));
const FName MetallicTextureWeightParameter(TEXT("MetallicTextureWeight"));
const FName SpecularParameter(TEXT("Specular"));
const FName AoTextureWeightParameter(TEXT("AoTextureWeight"));
const FName TintParameter(TEXT("Tint"));
const FName UseTextureSetParameter(TEXT("UseTextureSet"));
const FName ParameterGroup(TEXT("Istana Explore V5D R30 Context Facade"));

// Opaque glazing is intentional: it keeps deterministic Nanite ordering while
// introducing the view response and shallow depth cues that the former dark
// constant-tint branch lacked. UV0 is in metres before UvScale, so a 0.75
// period on the 4 m glass instances produces a restrained 3 m storey rhythm.
constexpr float OpaqueGlassParallaxDepth = 0.035f;
constexpr float OpaqueGlassInteriorBandPeriod = 0.75f;
constexpr float OpaqueGlassInteriorBrightnessVariation = 0.075f;
constexpr float OpaqueGlassFresnelExponent = 4.0f;
constexpr float OpaqueGlassFresnelBaseReflectance = 0.12f;
constexpr float OpaqueGlassReflectionStrength = 0.86f;
constexpr float OpaqueGlassRoughnessVariation = 0.018f;
const FLinearColor OpaqueGlassReflectionTint(0.20f, 0.34f, 0.48f, 1.0f);

enum class ETextureUsage : uint8
{
    BaseColor,
    Normal,
    PackedOrm
};

struct FMaterialSpec
{
    const TCHAR* R29SlotName;
    const TCHAR* Name;
    const TCHAR* TextureSet;
    bool bUseTextureSet;
    float MetresPerTile;
    FLinearColor Tint;
    float TextureInfluence;
    float NormalStrength;
    float Roughness;
    float RoughnessTextureWeight;
    float Metallic;
    float MetallicTextureWeight;
    float Specular;
    float AoTextureWeight;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("MI_IPV5D_R29_GlassCool"), TEXT("MI_IPV5D_R30_GlassCool"), nullptr, false, 4.0f, FLinearColor(0.032f, 0.078f, 0.115f), 0.0f, 0.0f, 0.09f, 0.0f, 0.0f, 0.0f, 0.50f, 0.0f},
    {TEXT("MI_IPV5D_R29_GlassWarm"), TEXT("MI_IPV5D_R30_GlassWarm"), nullptr, false, 4.0f, FLinearColor(0.105f, 0.075f, 0.045f), 0.0f, 0.0f, 0.12f, 0.0f, 0.0f, 0.0f, 0.50f, 0.0f},
    {TEXT("MI_IPV5D_R29_GlassNeutral"), TEXT("MI_IPV5D_R30_GlassNeutral"), nullptr, false, 4.0f, FLinearColor(0.060f, 0.085f, 0.098f), 0.0f, 0.0f, 0.105f, 0.0f, 0.0f, 0.0f, 0.50f, 0.0f},
    {TEXT("MI_IPV5D_R29_FrameLight"), TEXT("MI_IPV5D_R30_FrameLight"), TEXT("Shutter"), true, 0.8f, FLinearColor(0.82f, 0.80f, 0.72f), 0.55f, 0.42f, 0.48f, 0.42f, 0.18f, 0.0f, 0.42f, 0.38f},
    {TEXT("MI_IPV5D_R29_FrameDark"), TEXT("MI_IPV5D_R30_FrameDark"), TEXT("Shutter"), true, 0.8f, FLinearColor(0.16f, 0.18f, 0.19f), 0.34f, 0.38f, 0.36f, 0.34f, 0.58f, 0.0f, 0.48f, 0.30f},
    {TEXT("MI_IPV5D_R29_FrameBronze"), TEXT("MI_IPV5D_R30_FrameBronze"), TEXT("Shutter"), true, 0.8f, FLinearColor(0.38f, 0.25f, 0.14f), 0.28f, 0.30f, 0.34f, 0.30f, 0.66f, 0.0f, 0.50f, 0.28f},
    {TEXT("MI_IPV5D_R29_SillLight"), TEXT("MI_IPV5D_R30_SillLight"), TEXT("Stone"), true, 1.5f, FLinearColor(0.88f, 0.86f, 0.78f), 0.78f, 0.62f, 0.66f, 0.62f, 0.0f, 1.0f, 0.32f, 0.72f},
    {TEXT("MI_IPV5D_R29_SillDark"), TEXT("MI_IPV5D_R30_SillDark"), TEXT("Stone"), true, 1.5f, FLinearColor(0.36f, 0.38f, 0.37f), 0.82f, 0.70f, 0.72f, 0.66f, 0.0f, 1.0f, 0.28f, 0.78f},
    {TEXT("MI_IPV5D_R29_RoofTrim"), TEXT("MI_IPV5D_R30_RoofTrim"), TEXT("Slate"), true, 1.2f, FLinearColor(0.38f, 0.42f, 0.44f), 0.90f, 0.78f, 0.72f, 0.72f, 0.0f, 1.0f, 0.30f, 0.84f},
    {TEXT("MI_IPV5D_R29_Canopy"), TEXT("MI_IPV5D_R30_Canopy"), TEXT("DarkTimber"), true, 1.0f, FLinearColor(0.62f, 0.38f, 0.20f), 0.82f, 0.66f, 0.58f, 0.58f, 0.0f, 1.0f, 0.30f, 0.76f},
    {TEXT("MI_IPV5D_R29_BalconyRail"), TEXT("MI_IPV5D_R30_BalconyRail"), TEXT("Shutter"), true, 0.8f, FLinearColor(0.12f, 0.14f, 0.15f), 0.24f, 0.32f, 0.32f, 0.28f, 0.72f, 0.0f, 0.50f, 0.24f}};

static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 11);
constexpr int32 ExpectedAssetCount = UE_ARRAY_COUNT(MaterialSpecs) + 1;
constexpr int32 ExpectedMasterExpressionCount = 52;
constexpr int32 ExpectedTextureParameterCount = 3;
constexpr int32 ExpectedScalarParameterCount = 9;
constexpr int32 ExpectedVectorParameterCount = 1;

const TCHAR* const TextureSets[] = {
    TEXT("Shutter"), TEXT("DarkTimber"), TEXT("Stone"), TEXT("Slate")};

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

FString MaterialObjectPath(const FMaterialSpec& Spec)
{
    return ObjectPath(MaterialRoot, Spec.Name);
}

const TCHAR* TextureSuffix(ETextureUsage Usage)
{
    if (Usage == ETextureUsage::Normal)
    {
        return TEXT("Normal");
    }
    if (Usage == ETextureUsage::PackedOrm)
    {
        return TEXT("ORM");
    }
    return TEXT("BaseColor");
}

FString TextureObjectPath(const TCHAR* TextureSet, ETextureUsage Usage)
{
    const FString Name = FString::Printf(
        TEXT("T_IPV_%s_%s"), TextureSet, TextureSuffix(Usage));
    return ObjectPath(PublicViewTextureRoot, Name);
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& Path)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *Path);
    return IsValid(Object) && Object->GetPathName() == Path ? Object : nullptr;
}

template <typename ElementType>
bool SameSet(const TSet<ElementType>& A, const TSet<ElementType>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (const ElementType& Value : A)
    {
        if (!B.Contains(Value))
        {
            return false;
        }
    }
    return true;
}

FString SourceContractPath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
             "R30FacadeLookdev/r30_facade_lookdev.contract.json")));
}

FString BytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
}

bool ValidateSourceContract(FString& OutError)
{
    constexpr int64 ExpectedBytes = 8277;
    const FString ExpectedSha256(
        TEXT("BD11A517E8AC3F6EDBE0F6FE0912409D11180AD4FFAACD91E684391CC2B19F03"));
    const FString Filename = SourceContractPath();
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("R30 lookdev source-contract byte guard failed: expected=%lld actual=%d file='%s'."),
            ExpectedBytes, Bytes.Num(), *Filename);
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) == nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH) != ExpectedSha256)
    {
        OutError = TEXT("R30 lookdev source-contract SHA-256 guard failed for '") +
            Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R30 lookdev source admission requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

bool ValidateTexture(UTexture2D* Texture, ETextureUsage Usage, FString& OutError)
{
    if (!Texture ||
        !Texture->GetPathName().StartsWith(PublicViewTextureRoot + TEXT("/")) ||
        Texture->GetPathName().Contains(TEXT("/HeroMaterials")) ||
        Texture->Source.GetSizeX() != 2048 ||
        Texture->Source.GetSizeY() != 2048 ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        Texture->bFlipGreenChannel ||
        static_cast<bool>(Texture->SRGB) !=
            (Usage == ETextureUsage::BaseColor) ||
        Texture->CompressionSettings !=
            (Usage == ETextureUsage::Normal
                 ? TC_Normalmap
                 : Usage == ETextureUsage::PackedOrm ? TC_Masks : TC_Default) ||
        Texture->LODGroup !=
            (Usage == ETextureUsage::Normal
                 ? TEXTUREGROUP_WorldNormalMap
                 : TEXTUREGROUP_World))
    {
        OutError = TEXT("An R30 context texture lost its exact project-owned 2048px color-space/compression/mip/wrap contract.");
        return false;
    }
    return true;
}

bool LoadTextureSet(
    const TCHAR* TextureSet,
    TMap<FName, UTexture2D*>& OutTextures,
    FString& OutError)
{
    for (ETextureUsage Usage : {
             ETextureUsage::BaseColor,
             ETextureUsage::Normal,
             ETextureUsage::PackedOrm})
    {
        const FString Path = TextureObjectPath(TextureSet, Usage);
        UTexture2D* Texture = LoadExact<UTexture2D>(Path);
        if (!ValidateTexture(Texture, Usage, OutError))
        {
            OutError = TEXT("R30 refused context texture '") + Path +
                TEXT("': ") + OutError;
            return false;
        }
        OutTextures.Add(
            FName(*FString::Printf(
                TEXT("%s_%s"), TextureSet, TextureSuffix(Usage))),
            Texture);
    }
    return true;
}

UTexture2D* FindTexture(
    const TMap<FName, UTexture2D*>& Textures,
    const TCHAR* TextureSet,
    ETextureUsage Usage)
{
    return Textures.FindRef(FName(*FString::Printf(
        TEXT("%s_%s"), TextureSet, TextureSuffix(Usage))));
}

template <typename TExpression>
TExpression* AddExpression(
    UMaterial* Material,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    TExpression* Expression = Cast<TExpression>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material, TExpression::StaticClass(), X, Y));
    if (Expression)
    {
        Expression->Desc = Description;
    }
    return Expression;
}

UMaterialExpressionScalarParameter* AddScalar(
    UMaterial* Material,
    const TCHAR* Description,
    const FName& Name,
    float DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, Description, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = DefaultValue;
        Parameter->SliderMin = 0.0f;
        Parameter->SliderMax = Name == UvScaleParameter ? 2.0f : 1.0f;
    }
    return Parameter;
}

UMaterialExpressionTextureSampleParameter2D* AddTextureParameter(
    UMaterial* Material,
    const TCHAR* Description,
    const FName& Name,
    UTexture2D* Texture,
    EMaterialSamplerType SamplerType,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Parameter =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, Description, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->Group = ParameterGroup;
        Parameter->Texture = Texture;
        Parameter->SamplerType = SamplerType;
        Parameter->AutoSetSampleType();
    }
    return Parameter;
}

UMaterialExpressionStaticBoolParameter* AddStaticBoolParameter(
    UMaterial* Material,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    UMaterialExpressionStaticBoolParameter* Parameter =
        AddExpression<UMaterialExpressionStaticBoolParameter>(
            Material, Description, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = UseTextureSetParameter;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = true;
        Parameter->DynamicBranch = false;
    }
    return Parameter;
}

UMaterialExpressionStaticSwitch* AddStaticSwitch(
    UMaterial* Material,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    UMaterialExpressionStaticSwitch* Switch =
        AddExpression<UMaterialExpressionStaticSwitch>(
            Material, Description, X, Y);
    return Switch;
}

bool Connect(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    UMaterialExpression* To,
    const TCHAR* ToInput,
    const TCHAR* Label,
    FString& OutError)
{
    if (!From || !To ||
        !UMaterialEditingLibrary::ConnectMaterialExpressions(
            From, FString(FromOutput), To, FString(ToInput)))
    {
        OutError = TEXT("Could not connect R30 material graph edge '") +
            FString(Label) + TEXT("'.");
        return false;
    }
    return true;
}

bool ConnectProperty(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    EMaterialProperty Property,
    const TCHAR* Label,
    FString& OutError)
{
    if (!From || !UMaterialEditingLibrary::ConnectMaterialProperty(
                     From, FString(FromOutput), Property))
    {
        OutError = TEXT("Could not connect R30 material property '") +
            FString(Label) + TEXT("'.");
        return false;
    }
    return true;
}

UMaterial* CreateMaster(
    IAssetTools& AssetTools,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    UTexture2D* BaseTexture = FindTexture(
        Textures, TEXT("Shutter"), ETextureUsage::BaseColor);
    UTexture2D* NormalTexture = FindTexture(
        Textures, TEXT("Shutter"), ETextureUsage::Normal);
    UTexture2D* OrmTexture = FindTexture(
        Textures, TEXT("Shutter"), ETextureUsage::PackedOrm);
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MasterName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR30FacadeLookdev"))))
        : nullptr;
    if (!Material || !BaseTexture || !NormalTexture || !OrmTexture)
    {
        OutError = TEXT("Could not allocate the isolated R30 material master or its safe default textures.");
        return nullptr;
    }

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithNanite = true;

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("R30.UV0.PhysicalMetres"), -1500, 0);
    UMaterialExpressionScalarParameter* UvScale = AddScalar(
        Material, TEXT("R30.UvScale"), UvScaleParameter, 0.25f, -1500, 140);
    UMaterialExpressionMultiply* ScaledUv =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.ScalePhysicalMetreUV"), -1250, 20);
    UMaterialExpressionTextureSampleParameter2D* Base = AddTextureParameter(
        Material, TEXT("R30.BaseColorTexture"), BaseColorTextureParameter,
        BaseTexture, SAMPLERTYPE_Color, -1000, -360);
    UMaterialExpressionTextureSampleParameter2D* Normal = AddTextureParameter(
        Material, TEXT("R30.NormalTexture"), NormalTextureParameter,
        NormalTexture, SAMPLERTYPE_Normal, -1000, 0);
    UMaterialExpressionTextureSampleParameter2D* Orm = AddTextureParameter(
        Material, TEXT("R30.PackedORMTexture"), PackedOrmTextureParameter,
        OrmTexture, SAMPLERTYPE_Masks, -1000, 360);
    UMaterialExpressionVectorParameter* Tint =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material, TEXT("R30.Tint"), -720, -520);
    UMaterialExpressionMultiply* TexturedTint =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.TexturedTint"), -480, -340);
    UMaterialExpressionScalarParameter* TextureInfluence = AddScalar(
        Material, TEXT("R30.TextureInfluence"), TextureInfluenceParameter,
        1.0f, -480, -520);
    UMaterialExpressionLinearInterpolate* BaseBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R30.BlendTintAndTexture"), -180, -340);
    UMaterialExpressionConstant3Vector* FlatNormal =
        AddExpression<UMaterialExpressionConstant3Vector>(
            Material, TEXT("R30.FlatNormal"), -720, -60);
    UMaterialExpressionScalarParameter* NormalStrength = AddScalar(
        Material, TEXT("R30.NormalStrength"), NormalStrengthParameter,
        1.0f, -480, 80);
    UMaterialExpressionLinearInterpolate* NormalBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R30.BlendFlatAndTextureNormal"), -180, 0);
    UMaterialExpressionScalarParameter* Roughness = AddScalar(
        Material, TEXT("R30.Roughness"), RoughnessParameter,
        0.55f, -720, 220);
    UMaterialExpressionScalarParameter* RoughnessTextureWeight = AddScalar(
        Material, TEXT("R30.RoughnessTextureWeight"),
        RoughnessTextureWeightParameter, 1.0f, -480, 260);
    UMaterialExpressionLinearInterpolate* RoughnessBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R30.BlendRoughness"), -180, 220);
    UMaterialExpressionScalarParameter* Metallic = AddScalar(
        Material, TEXT("R30.Metallic"), MetallicParameter,
        0.0f, -720, 420);
    UMaterialExpressionScalarParameter* MetallicTextureWeight = AddScalar(
        Material, TEXT("R30.MetallicTextureWeight"),
        MetallicTextureWeightParameter, 1.0f, -480, 460);
    UMaterialExpressionLinearInterpolate* MetallicBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R30.BlendMetallic"), -180, 420);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material, TEXT("R30.Specular"), SpecularParameter,
        0.35f, -180, 580);
    UMaterialExpressionConstant* AoOne =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R30.AmbientOcclusionOne"), -720, 660);
    UMaterialExpressionScalarParameter* AoTextureWeight = AddScalar(
        Material, TEXT("R30.AoTextureWeight"), AoTextureWeightParameter,
        1.0f, -480, 700);
    UMaterialExpressionLinearInterpolate* AoBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R30.BlendAmbientOcclusion"), -180, 680);
    UMaterialExpressionStaticBoolParameter* UseTextureSet =
        AddStaticBoolParameter(
            Material, TEXT("R30.UseTextureSet"), -180, 860);
    UMaterialExpressionStaticSwitch* BaseSwitch = AddStaticSwitch(
        Material, TEXT("R30.UseTextureSet.BaseColor"), 80, -340);
    UMaterialExpressionStaticSwitch* NormalSwitch = AddStaticSwitch(
        Material, TEXT("R30.UseTextureSet.Normal"), 80, 0);
    UMaterialExpressionStaticSwitch* RoughnessSwitch = AddStaticSwitch(
        Material, TEXT("R30.UseTextureSet.Roughness"), 80, 220);
    UMaterialExpressionStaticSwitch* MetallicSwitch = AddStaticSwitch(
        Material, TEXT("R30.UseTextureSet.Metallic"), 80, 420);
    UMaterialExpressionStaticSwitch* AoSwitch = AddStaticSwitch(
        Material, TEXT("R30.UseTextureSet.AmbientOcclusion"), 80, 680);

    UMaterialExpressionCameraVectorWS* GlassCameraVector =
        AddExpression<UMaterialExpressionCameraVectorWS>(
            Material, TEXT("R30.Glass.CameraVectorWS"), -1500, 1040);
    UMaterialExpressionTransform* GlassCameraToTangent =
        AddExpression<UMaterialExpressionTransform>(
            Material, TEXT("R30.Glass.CameraWorldToTangent"), -1260, 1040);
    UMaterialExpressionComponentMask* GlassCameraTangentXY =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("R30.Glass.CameraTangentXY"), -1020, 1040);
    UMaterialExpressionConstant* GlassParallaxDepth =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R30.Glass.ParallaxDepth"), -1020, 1180);
    UMaterialExpressionMultiply* GlassParallaxOffset =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.Glass.ParallaxOffset"), -780, 1040);
    UMaterialExpressionAdd* GlassParallaxUv =
        AddExpression<UMaterialExpressionAdd>(
            Material, TEXT("R30.Glass.ParallaxUV"), -540, 1040);
    UMaterialExpressionComponentMask* GlassParallaxV =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("R30.Glass.ParallaxV"), -300, 1040);
    UMaterialExpressionSine* GlassInteriorBands =
        AddExpression<UMaterialExpressionSine>(
            Material, TEXT("R30.Glass.InteriorStoreyBands"), -60, 1040);
    UMaterialExpressionConstant* GlassInteriorVariation =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R30.Glass.InteriorBrightnessVariation"), -60, 1180);
    UMaterialExpressionMultiply* GlassInteriorBandAmount =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.Glass.InteriorBandAmount"), 180, 1040);
    UMaterialExpressionConstant* GlassInteriorOne =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R30.Glass.InteriorBrightnessOne"), 180, 1180);
    UMaterialExpressionAdd* GlassInteriorBrightness =
        AddExpression<UMaterialExpressionAdd>(
            Material, TEXT("R30.Glass.InteriorBrightness"), 420, 1040);
    UMaterialExpressionMultiply* GlassInteriorTint =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.Glass.InteriorTint"), 660, 940);
    UMaterialExpressionFresnel* GlassFresnel =
        AddExpression<UMaterialExpressionFresnel>(
            Material, TEXT("R30.Glass.ViewAngleFresnel"), 180, 1320);
    UMaterialExpressionConstant* GlassReflectionStrength =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R30.Glass.ReflectionStrength"), 420, 1420);
    UMaterialExpressionMultiply* GlassFresnelWeight =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.Glass.FresnelReflectionWeight"), 660, 1320);
    UMaterialExpressionSaturate* GlassFresnelSaturate =
        AddExpression<UMaterialExpressionSaturate>(
            Material, TEXT("R30.Glass.ClampReflectionWeight"), 900, 1320);
    UMaterialExpressionConstant3Vector* GlassReflectionTint =
        AddExpression<UMaterialExpressionConstant3Vector>(
            Material, TEXT("R30.Glass.SkyReflectionTint"), 900, 1080);
    UMaterialExpressionLinearInterpolate* GlassBase =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R30.Glass.BlendInteriorAndReflection"), 1140, 1040);
    UMaterialExpressionConstant* GlassRoughnessVariation =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R30.Glass.RoughnessVariation"), 420, 1580);
    UMaterialExpressionMultiply* GlassRoughnessBand =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R30.Glass.RoughnessBand"), 660, 1540);
    UMaterialExpressionAdd* GlassRoughness =
        AddExpression<UMaterialExpressionAdd>(
            Material, TEXT("R30.Glass.RoughnessWithOccupancy"), 900, 1540);
    UMaterialExpressionSaturate* GlassRoughnessSaturate =
        AddExpression<UMaterialExpressionSaturate>(
            Material, TEXT("R30.Glass.ClampRoughness"), 1140, 1540);

    if (!Uv0 || !UvScale || !ScaledUv || !Base || !Normal || !Orm ||
        !Tint || !TexturedTint || !TextureInfluence || !BaseBlend ||
        !FlatNormal || !NormalStrength || !NormalBlend || !Roughness ||
        !RoughnessTextureWeight || !RoughnessBlend || !Metallic ||
        !MetallicTextureWeight || !MetallicBlend || !Specular || !AoOne ||
        !AoTextureWeight || !AoBlend || !UseTextureSet || !BaseSwitch || !NormalSwitch ||
        !RoughnessSwitch || !MetallicSwitch || !AoSwitch || !GlassCameraVector ||
        !GlassCameraToTangent || !GlassCameraTangentXY || !GlassParallaxDepth ||
        !GlassParallaxOffset || !GlassParallaxUv || !GlassParallaxV ||
        !GlassInteriorBands || !GlassInteriorVariation || !GlassInteriorBandAmount ||
        !GlassInteriorOne || !GlassInteriorBrightness || !GlassInteriorTint ||
        !GlassFresnel || !GlassReflectionStrength || !GlassFresnelWeight ||
        !GlassFresnelSaturate || !GlassReflectionTint || !GlassBase ||
        !GlassRoughnessVariation || !GlassRoughnessBand || !GlassRoughness ||
        !GlassRoughnessSaturate)
    {
        OutError = TEXT("Could not allocate the exact 52-node R30 material graph.");
        return nullptr;
    }

    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    Tint->ParameterName = TintParameter;
    Tint->Group = ParameterGroup;
    Tint->DefaultValue = FLinearColor::White;
    FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
    AoOne->R = 1.0f;
    GlassCameraToTangent->TransformSourceType = TRANSFORMSOURCE_World;
    GlassCameraToTangent->TransformType = TRANSFORM_Tangent;
    GlassCameraTangentXY->R = true;
    GlassCameraTangentXY->G = true;
    GlassCameraTangentXY->B = false;
    GlassCameraTangentXY->A = false;
    GlassParallaxDepth->R = OpaqueGlassParallaxDepth;
    GlassParallaxV->R = false;
    GlassParallaxV->G = true;
    GlassParallaxV->B = false;
    GlassParallaxV->A = false;
    GlassInteriorBands->Period = OpaqueGlassInteriorBandPeriod;
    GlassInteriorVariation->R = OpaqueGlassInteriorBrightnessVariation;
    GlassInteriorOne->R = 1.0f;
    GlassFresnel->Exponent = OpaqueGlassFresnelExponent;
    GlassFresnel->BaseReflectFraction = OpaqueGlassFresnelBaseReflectance;
    GlassReflectionStrength->R = OpaqueGlassReflectionStrength;
    GlassReflectionTint->Constant = OpaqueGlassReflectionTint;
    GlassRoughnessVariation->R = OpaqueGlassRoughnessVariation;

    if (!Connect(Uv0, TEXT(""), ScaledUv, TEXT("A"), TEXT("UV0 to metric scale"), OutError) ||
        !Connect(UvScale, TEXT(""), ScaledUv, TEXT("B"), TEXT("reciprocal metres-per-tile to metric scale"), OutError) ||
        !Connect(ScaledUv, TEXT(""), Base, TEXT("UVs"), TEXT("metric UV to base"), OutError) ||
        !Connect(ScaledUv, TEXT(""), Normal, TEXT("UVs"), TEXT("metric UV to normal"), OutError) ||
        !Connect(ScaledUv, TEXT(""), Orm, TEXT("UVs"), TEXT("metric UV to ORM"), OutError) ||
        !Connect(Base, TEXT("RGB"), TexturedTint, TEXT("A"), TEXT("base texture to tint"), OutError) ||
        !Connect(Tint, TEXT(""), TexturedTint, TEXT("B"), TEXT("tint to textured base"), OutError) ||
        !Connect(Tint, TEXT(""), BaseBlend, TEXT("A"), TEXT("plain tint base"), OutError) ||
        !Connect(TexturedTint, TEXT(""), BaseBlend, TEXT("B"), TEXT("textured tint base"), OutError) ||
        !Connect(TextureInfluence, TEXT(""), BaseBlend, TEXT("Alpha"), TEXT("texture influence"), OutError) ||
        !Connect(FlatNormal, TEXT(""), NormalBlend, TEXT("A"), TEXT("flat normal"), OutError) ||
        !Connect(Normal, TEXT("RGB"), NormalBlend, TEXT("B"), TEXT("texture normal"), OutError) ||
        !Connect(NormalStrength, TEXT(""), NormalBlend, TEXT("Alpha"), TEXT("normal strength"), OutError) ||
        !Connect(Roughness, TEXT(""), RoughnessBlend, TEXT("A"), TEXT("constant roughness"), OutError) ||
        !Connect(Orm, TEXT("G"), RoughnessBlend, TEXT("B"), TEXT("ORM roughness"), OutError) ||
        !Connect(RoughnessTextureWeight, TEXT(""), RoughnessBlend, TEXT("Alpha"), TEXT("roughness texture weight"), OutError) ||
        !Connect(Metallic, TEXT(""), MetallicBlend, TEXT("A"), TEXT("constant metallic"), OutError) ||
        !Connect(Orm, TEXT("B"), MetallicBlend, TEXT("B"), TEXT("ORM metallic"), OutError) ||
        !Connect(MetallicTextureWeight, TEXT(""), MetallicBlend, TEXT("Alpha"), TEXT("metallic texture weight"), OutError) ||
        !Connect(AoOne, TEXT(""), AoBlend, TEXT("A"), TEXT("neutral AO"), OutError) ||
        !Connect(Orm, TEXT("R"), AoBlend, TEXT("B"), TEXT("ORM ambient occlusion"), OutError) ||
        !Connect(AoTextureWeight, TEXT(""), AoBlend, TEXT("Alpha"), TEXT("AO texture weight"), OutError) ||
        !Connect(UseTextureSet, TEXT(""), BaseSwitch, TEXT("Value"), TEXT("texture-set base switch control"), OutError) ||
        !Connect(UseTextureSet, TEXT(""), NormalSwitch, TEXT("Value"), TEXT("texture-set normal switch control"), OutError) ||
        !Connect(UseTextureSet, TEXT(""), RoughnessSwitch, TEXT("Value"), TEXT("texture-set roughness switch control"), OutError) ||
        !Connect(UseTextureSet, TEXT(""), MetallicSwitch, TEXT("Value"), TEXT("texture-set metallic switch control"), OutError) ||
        !Connect(UseTextureSet, TEXT(""), AoSwitch, TEXT("Value"), TEXT("texture-set AO switch control"), OutError) ||
        !Connect(BaseBlend, TEXT(""), BaseSwitch, TEXT("True"), TEXT("textured base switch branch"), OutError) ||
        !Connect(NormalBlend, TEXT(""), NormalSwitch, TEXT("True"), TEXT("textured normal switch branch"), OutError) ||
        !Connect(FlatNormal, TEXT(""), NormalSwitch, TEXT("False"), TEXT("procedural normal switch branch"), OutError) ||
        !Connect(RoughnessBlend, TEXT(""), RoughnessSwitch, TEXT("True"), TEXT("textured roughness switch branch"), OutError) ||
        !Connect(MetallicBlend, TEXT(""), MetallicSwitch, TEXT("True"), TEXT("textured metallic switch branch"), OutError) ||
        !Connect(Metallic, TEXT(""), MetallicSwitch, TEXT("False"), TEXT("procedural metallic switch branch"), OutError) ||
        !Connect(AoBlend, TEXT(""), AoSwitch, TEXT("True"), TEXT("textured AO switch branch"), OutError) ||
        !Connect(AoOne, TEXT(""), AoSwitch, TEXT("False"), TEXT("procedural AO switch branch"), OutError) ||
        !Connect(GlassCameraVector, TEXT(""), GlassCameraToTangent, TEXT(""), TEXT("glass camera vector to tangent transform"), OutError) ||
        !Connect(GlassCameraToTangent, TEXT(""), GlassCameraTangentXY, TEXT(""), TEXT("glass tangent camera XY"), OutError) ||
        !Connect(GlassCameraTangentXY, TEXT(""), GlassParallaxOffset, TEXT("A"), TEXT("glass tangent camera to parallax"), OutError) ||
        !Connect(GlassParallaxDepth, TEXT(""), GlassParallaxOffset, TEXT("B"), TEXT("glass shallow parallax depth"), OutError) ||
        !Connect(ScaledUv, TEXT(""), GlassParallaxUv, TEXT("A"), TEXT("metric glass UV to parallax"), OutError) ||
        !Connect(GlassParallaxOffset, TEXT(""), GlassParallaxUv, TEXT("B"), TEXT("glass camera offset to parallax UV"), OutError) ||
        !Connect(GlassParallaxUv, TEXT(""), GlassParallaxV, TEXT(""), TEXT("glass parallax V extraction"), OutError) ||
        !Connect(GlassParallaxV, TEXT(""), GlassInteriorBands, TEXT(""), TEXT("glass parallax V to storey bands"), OutError) ||
        !Connect(GlassInteriorBands, TEXT(""), GlassInteriorBandAmount, TEXT("A"), TEXT("glass storey bands to brightness"), OutError) ||
        !Connect(GlassInteriorVariation, TEXT(""), GlassInteriorBandAmount, TEXT("B"), TEXT("restrained glass interior variation"), OutError) ||
        !Connect(GlassInteriorOne, TEXT(""), GlassInteriorBrightness, TEXT("A"), TEXT("neutral glass interior brightness"), OutError) ||
        !Connect(GlassInteriorBandAmount, TEXT(""), GlassInteriorBrightness, TEXT("B"), TEXT("glass occupancy band brightness"), OutError) ||
        !Connect(Tint, TEXT(""), GlassInteriorTint, TEXT("A"), TEXT("glass family tint to interior"), OutError) ||
        !Connect(GlassInteriorBrightness, TEXT(""), GlassInteriorTint, TEXT("B"), TEXT("glass interior brightness modulation"), OutError) ||
        !Connect(GlassFresnel, TEXT(""), GlassFresnelWeight, TEXT("A"), TEXT("glass view-angle Fresnel"), OutError) ||
        !Connect(GlassReflectionStrength, TEXT(""), GlassFresnelWeight, TEXT("B"), TEXT("restrained glass reflection strength"), OutError) ||
        !Connect(GlassFresnelWeight, TEXT(""), GlassFresnelSaturate, TEXT(""), TEXT("clamp glass reflection weight"), OutError) ||
        !Connect(GlassInteriorTint, TEXT(""), GlassBase, TEXT("A"), TEXT("glass interior base"), OutError) ||
        !Connect(GlassReflectionTint, TEXT(""), GlassBase, TEXT("B"), TEXT("glass sky reflection tint"), OutError) ||
        !Connect(GlassFresnelSaturate, TEXT(""), GlassBase, TEXT("Alpha"), TEXT("glass Fresnel reflection blend"), OutError) ||
        !Connect(GlassInteriorBands, TEXT(""), GlassRoughnessBand, TEXT("A"), TEXT("glass occupancy bands to roughness"), OutError) ||
        !Connect(GlassRoughnessVariation, TEXT(""), GlassRoughnessBand, TEXT("B"), TEXT("restrained glass roughness variation"), OutError) ||
        !Connect(Roughness, TEXT(""), GlassRoughness, TEXT("A"), TEXT("glass base roughness"), OutError) ||
        !Connect(GlassRoughnessBand, TEXT(""), GlassRoughness, TEXT("B"), TEXT("glass occupancy roughness cue"), OutError) ||
        !Connect(GlassRoughness, TEXT(""), GlassRoughnessSaturate, TEXT(""), TEXT("clamp glass roughness"), OutError) ||
        !Connect(GlassBase, TEXT(""), BaseSwitch, TEXT("False"), TEXT("opaque glass base switch branch"), OutError) ||
        !Connect(GlassRoughnessSaturate, TEXT(""), RoughnessSwitch, TEXT("False"), TEXT("opaque glass roughness switch branch"), OutError) ||
        !ConnectProperty(BaseSwitch, TEXT(""), MP_BaseColor, TEXT("Base Color"), OutError) ||
        !ConnectProperty(NormalSwitch, TEXT(""), MP_Normal, TEXT("Normal"), OutError) ||
        !ConnectProperty(RoughnessSwitch, TEXT(""), MP_Roughness, TEXT("Roughness"), OutError) ||
        !ConnectProperty(MetallicSwitch, TEXT(""), MP_Metallic, TEXT("Metallic"), OutError) ||
        !ConnectProperty(Specular, TEXT(""), MP_Specular, TEXT("Specular"), OutError) ||
        !ConnectProperty(AoSwitch, TEXT(""), MP_AmbientOcclusion, TEXT("Ambient Occlusion"), OutError))
    {
        return nullptr;
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return Material;
}

bool ValidateCompiledMaterial(UMaterialInterface* Material, FString& OutError)
{
    const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
    if (FeatureLevel != ERHIFeatureLevel::SM6)
    {
        OutError = FString::Printf(
            TEXT("R30 lookdev validation requires the transaction's active SM6 feature level; actual=%d."),
            static_cast<int32>(FeatureLevel));
        return false;
    }
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(FeatureLevel)
        : nullptr;
    if (Resource && !Resource->IsGameThreadShaderMapComplete())
    {
        Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
        Resource->FinishCompilation();
    }
    FMaterialShaderMap* ShaderMap = Resource
        ? Resource->GetGameThreadShaderMap()
        : nullptr;
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Material || !Resource || !ShaderMap ||
        !Resource->IsCompilationFinished() ||
        !Resource->IsGameThreadShaderMapComplete() ||
        !ShaderMap->IsValidForRendering() || Resource->IsDefaultMaterial() ||
        Errors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("R30 lookdev material '%s' lacks a valid compiled D3D12 SM6 transaction resource: [%s]."),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            *FString::Join(Errors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMaster(
    UMaterial* Material,
    bool bRequireSaved,
    FString& OutError)
{
    if (Material)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || Material->GetPathName() != MasterObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->bUsedWithNanite ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        Material->GetExpressions().Num() != ExpectedMasterExpressionCount ||
        !EditorOnly || !EditorOnly->BaseColor.Expression ||
        !EditorOnly->Normal.Expression || !EditorOnly->Roughness.Expression ||
        !EditorOnly->Metallic.Expression || !EditorOnly->Specular.Expression ||
        !EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->Displacement.Expression ||
        (bRequireSaved &&
         (!Material->GetOutermost() || Material->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              FPackageName::ObjectPathToPackageName(MasterObjectPath)))))
    {
        OutError = TEXT("R30 context-facade master lost its exact opaque, DefaultLit, Nanite, no-displacement graph contract.");
        return false;
    }

    TSet<FName> TextureParameters;
    TSet<FName> ScalarParameters;
    TSet<FName> VectorParameters;
    TMap<FString, const UMaterialExpression*> ExpressionsByDescription;
    TMap<FString, const UMaterialExpressionStaticSwitch*> SwitchesByDescription;
    const UMaterialExpressionStaticBoolParameter* StaticBoolParameter = nullptr;
    TArray<const UMaterialExpressionStaticSwitch*> StaticSwitches;
    int32 StaticBoolParameterCount = 0;
    int32 StaticSwitchNodeCount = 0;
    for (const UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (!Expression || Expression->Desc.IsEmpty() ||
            ExpressionsByDescription.Contains(Expression->Desc))
        {
            OutError = TEXT("R30 master contains a null, unnamed, or duplicate-described graph node.");
            return false;
        }
        ExpressionsByDescription.Add(Expression->Desc, Expression);
        if (Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
        {
            OutError = TEXT("R30 master contains a forbidden duplicate-prone StaticSwitchParameter node.");
            return false;
        }
        if (const UMaterialExpressionStaticBoolParameter* StaticBool =
                Cast<UMaterialExpressionStaticBoolParameter>(Expression))
        {
            if (StaticBool->ParameterName != UseTextureSetParameter ||
                !StaticBool->DefaultValue || StaticBool->DynamicBranch)
            {
                OutError = TEXT("R30 master static texture-set parameter drifted.");
                return false;
            }
            StaticBoolParameter = StaticBool;
            ++StaticBoolParameterCount;
        }
        else if (const UMaterialExpressionStaticSwitch* StaticSwitch =
                     Cast<UMaterialExpressionStaticSwitch>(Expression))
        {
            StaticSwitches.Add(StaticSwitch);
            SwitchesByDescription.Add(
                StaticSwitch->Desc, StaticSwitch);
            ++StaticSwitchNodeCount;
        }
        else if (const UMaterialExpressionTextureSampleParameter2D* Texture =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            if (!Texture->Texture ||
                !Texture->Texture->GetPathName().StartsWith(
                    PublicViewTextureRoot + TEXT("/")) ||
                Texture->Texture->GetPathName().Contains(TEXT("HeroMaterials")) ||
                TextureParameters.Contains(Texture->ParameterName))
            {
                OutError = TEXT("R30 master has a duplicate, missing, or non-context texture dependency.");
                return false;
            }
            TextureParameters.Add(Texture->ParameterName);
        }
        else if (const UMaterialExpressionScalarParameter* Scalar =
                     Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            if (ScalarParameters.Contains(Scalar->ParameterName))
            {
                OutError = TEXT("R30 master has a duplicate scalar parameter.");
                return false;
            }
            ScalarParameters.Add(Scalar->ParameterName);
        }
        else if (const UMaterialExpressionVectorParameter* Vector =
                     Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            if (VectorParameters.Contains(Vector->ParameterName))
            {
                OutError = TEXT("R30 master has a duplicate vector parameter.");
                return false;
            }
            VectorParameters.Add(Vector->ParameterName);
        }
    }
    const TSet<FName> ExpectedTextures = {
        BaseColorTextureParameter,
        NormalTextureParameter,
        PackedOrmTextureParameter};
    const TSet<FName> ExpectedScalars = {
        UvScaleParameter,
        TextureInfluenceParameter,
        NormalStrengthParameter,
        RoughnessParameter,
        RoughnessTextureWeightParameter,
        MetallicParameter,
        MetallicTextureWeightParameter,
        SpecularParameter,
        AoTextureWeightParameter};
    const TSet<FName> ExpectedVectors = {TintParameter};
    bool bSwitchControlsValid = StaticBoolParameter != nullptr;
    for (const UMaterialExpressionStaticSwitch* StaticSwitch : StaticSwitches)
    {
        bSwitchControlsValid = bSwitchControlsValid && StaticSwitch &&
            StaticSwitch->Value.Expression == StaticBoolParameter &&
            StaticSwitch->A.Expression && StaticSwitch->B.Expression;
    }
    const auto HasExactSwitch = [&ExpressionsByDescription,
                                 &SwitchesByDescription,
                                 StaticBoolParameter](
                                    const TCHAR* SwitchDescription,
                                    const TCHAR* TrueDescription,
                                    const TCHAR* FalseDescription,
                                    const UMaterialExpression* OutputExpression)
    {
        const UMaterialExpressionStaticSwitch* const* Switch =
            SwitchesByDescription.Find(SwitchDescription);
        const UMaterialExpression* const* TrueExpression =
            ExpressionsByDescription.Find(TrueDescription);
        const UMaterialExpression* const* FalseExpression =
            ExpressionsByDescription.Find(FalseDescription);
        return Switch && *Switch && TrueExpression && FalseExpression &&
            (*Switch)->Value.Expression == StaticBoolParameter &&
            (*Switch)->A.Expression == *TrueExpression &&
            (*Switch)->B.Expression == *FalseExpression &&
            OutputExpression == *Switch;
    };
    const bool bExactStaticBranchTopology =
        HasExactSwitch(
            TEXT("R30.UseTextureSet.BaseColor"),
            TEXT("R30.BlendTintAndTexture"),
            TEXT("R30.Glass.BlendInteriorAndReflection"),
            EditorOnly->BaseColor.Expression) &&
        HasExactSwitch(
            TEXT("R30.UseTextureSet.Normal"),
            TEXT("R30.BlendFlatAndTextureNormal"), TEXT("R30.FlatNormal"),
            EditorOnly->Normal.Expression) &&
        HasExactSwitch(
            TEXT("R30.UseTextureSet.Roughness"),
            TEXT("R30.BlendRoughness"), TEXT("R30.Glass.ClampRoughness"),
            EditorOnly->Roughness.Expression) &&
        HasExactSwitch(
            TEXT("R30.UseTextureSet.Metallic"),
            TEXT("R30.BlendMetallic"), TEXT("R30.Metallic"),
            EditorOnly->Metallic.Expression) &&
        HasExactSwitch(
            TEXT("R30.UseTextureSet.AmbientOcclusion"),
            TEXT("R30.BlendAmbientOcclusion"),
            TEXT("R30.AmbientOcclusionOne"),
            EditorOnly->AmbientOcclusion.Expression);

    const auto FindExpression = [&ExpressionsByDescription](
                                    const TCHAR* Description)
        -> const UMaterialExpression*
    {
        const UMaterialExpression* const* Found =
            ExpressionsByDescription.Find(Description);
        return Found ? *Found : nullptr;
    };
    const UMaterialExpressionCameraVectorWS* GlassCameraVector =
        Cast<const UMaterialExpressionCameraVectorWS>(
            FindExpression(TEXT("R30.Glass.CameraVectorWS")));
    const UMaterialExpressionTransform* GlassCameraToTangent =
        Cast<const UMaterialExpressionTransform>(
            FindExpression(TEXT("R30.Glass.CameraWorldToTangent")));
    const UMaterialExpressionComponentMask* GlassCameraTangentXY =
        Cast<const UMaterialExpressionComponentMask>(
            FindExpression(TEXT("R30.Glass.CameraTangentXY")));
    const UMaterialExpressionConstant* GlassParallaxDepth =
        Cast<const UMaterialExpressionConstant>(
            FindExpression(TEXT("R30.Glass.ParallaxDepth")));
    const UMaterialExpressionMultiply* GlassParallaxOffset =
        Cast<const UMaterialExpressionMultiply>(
            FindExpression(TEXT("R30.Glass.ParallaxOffset")));
    const UMaterialExpressionAdd* GlassParallaxUv =
        Cast<const UMaterialExpressionAdd>(
            FindExpression(TEXT("R30.Glass.ParallaxUV")));
    const UMaterialExpressionComponentMask* GlassParallaxV =
        Cast<const UMaterialExpressionComponentMask>(
            FindExpression(TEXT("R30.Glass.ParallaxV")));
    const UMaterialExpressionSine* GlassInteriorBands =
        Cast<const UMaterialExpressionSine>(
            FindExpression(TEXT("R30.Glass.InteriorStoreyBands")));
    const UMaterialExpressionConstant* GlassInteriorVariation =
        Cast<const UMaterialExpressionConstant>(
            FindExpression(TEXT("R30.Glass.InteriorBrightnessVariation")));
    const UMaterialExpressionMultiply* GlassInteriorBandAmount =
        Cast<const UMaterialExpressionMultiply>(
            FindExpression(TEXT("R30.Glass.InteriorBandAmount")));
    const UMaterialExpressionConstant* GlassInteriorOne =
        Cast<const UMaterialExpressionConstant>(
            FindExpression(TEXT("R30.Glass.InteriorBrightnessOne")));
    const UMaterialExpressionAdd* GlassInteriorBrightness =
        Cast<const UMaterialExpressionAdd>(
            FindExpression(TEXT("R30.Glass.InteriorBrightness")));
    const UMaterialExpressionMultiply* GlassInteriorTint =
        Cast<const UMaterialExpressionMultiply>(
            FindExpression(TEXT("R30.Glass.InteriorTint")));
    const UMaterialExpressionFresnel* GlassFresnel =
        Cast<const UMaterialExpressionFresnel>(
            FindExpression(TEXT("R30.Glass.ViewAngleFresnel")));
    const UMaterialExpressionConstant* GlassReflectionStrength =
        Cast<const UMaterialExpressionConstant>(
            FindExpression(TEXT("R30.Glass.ReflectionStrength")));
    const UMaterialExpressionMultiply* GlassFresnelWeight =
        Cast<const UMaterialExpressionMultiply>(
            FindExpression(TEXT("R30.Glass.FresnelReflectionWeight")));
    const UMaterialExpressionSaturate* GlassFresnelSaturate =
        Cast<const UMaterialExpressionSaturate>(
            FindExpression(TEXT("R30.Glass.ClampReflectionWeight")));
    const UMaterialExpressionConstant3Vector* GlassReflectionTint =
        Cast<const UMaterialExpressionConstant3Vector>(
            FindExpression(TEXT("R30.Glass.SkyReflectionTint")));
    const UMaterialExpressionLinearInterpolate* GlassBase =
        Cast<const UMaterialExpressionLinearInterpolate>(
            FindExpression(TEXT("R30.Glass.BlendInteriorAndReflection")));
    const UMaterialExpressionConstant* GlassRoughnessVariation =
        Cast<const UMaterialExpressionConstant>(
            FindExpression(TEXT("R30.Glass.RoughnessVariation")));
    const UMaterialExpressionMultiply* GlassRoughnessBand =
        Cast<const UMaterialExpressionMultiply>(
            FindExpression(TEXT("R30.Glass.RoughnessBand")));
    const UMaterialExpressionAdd* GlassRoughness =
        Cast<const UMaterialExpressionAdd>(
            FindExpression(TEXT("R30.Glass.RoughnessWithOccupancy")));
    const UMaterialExpressionSaturate* GlassRoughnessSaturate =
        Cast<const UMaterialExpressionSaturate>(
            FindExpression(TEXT("R30.Glass.ClampRoughness")));
    const UMaterialExpression* ScaledUvExpression =
        FindExpression(TEXT("R30.ScalePhysicalMetreUV"));
    const UMaterialExpression* TintExpression =
        FindExpression(TEXT("R30.Tint"));
    const UMaterialExpression* RoughnessExpression =
        FindExpression(TEXT("R30.Roughness"));
    const bool bExactOpaqueGlassTopology =
        GlassCameraVector && GlassCameraToTangent && GlassCameraTangentXY &&
        GlassParallaxDepth && GlassParallaxOffset && GlassParallaxUv &&
        GlassParallaxV && GlassInteriorBands && GlassInteriorVariation &&
        GlassInteriorBandAmount && GlassInteriorOne &&
        GlassInteriorBrightness && GlassInteriorTint && GlassFresnel &&
        GlassReflectionStrength && GlassFresnelWeight &&
        GlassFresnelSaturate && GlassReflectionTint && GlassBase &&
        GlassRoughnessVariation && GlassRoughnessBand && GlassRoughness &&
        GlassRoughnessSaturate && ScaledUvExpression && TintExpression &&
        RoughnessExpression &&
        GlassCameraToTangent->Input.Expression == GlassCameraVector &&
        GlassCameraToTangent->TransformSourceType == TRANSFORMSOURCE_World &&
        GlassCameraToTangent->TransformType == TRANSFORM_Tangent &&
        GlassCameraTangentXY->Input.Expression == GlassCameraToTangent &&
        GlassCameraTangentXY->R && GlassCameraTangentXY->G &&
        !GlassCameraTangentXY->B && !GlassCameraTangentXY->A &&
        FMath::IsNearlyEqual(
            GlassParallaxDepth->R, OpaqueGlassParallaxDepth, 0.000001f) &&
        GlassParallaxOffset->A.Expression == GlassCameraTangentXY &&
        GlassParallaxOffset->B.Expression == GlassParallaxDepth &&
        GlassParallaxUv->A.Expression == ScaledUvExpression &&
        GlassParallaxUv->B.Expression == GlassParallaxOffset &&
        GlassParallaxV->Input.Expression == GlassParallaxUv &&
        !GlassParallaxV->R && GlassParallaxV->G &&
        !GlassParallaxV->B && !GlassParallaxV->A &&
        GlassInteriorBands->Input.Expression == GlassParallaxV &&
        FMath::IsNearlyEqual(
            GlassInteriorBands->Period,
            OpaqueGlassInteriorBandPeriod,
            0.000001f) &&
        FMath::IsNearlyEqual(
            GlassInteriorVariation->R,
            OpaqueGlassInteriorBrightnessVariation,
            0.000001f) &&
        GlassInteriorBandAmount->A.Expression == GlassInteriorBands &&
        GlassInteriorBandAmount->B.Expression == GlassInteriorVariation &&
        FMath::IsNearlyEqual(GlassInteriorOne->R, 1.0f, 0.000001f) &&
        GlassInteriorBrightness->A.Expression == GlassInteriorOne &&
        GlassInteriorBrightness->B.Expression == GlassInteriorBandAmount &&
        GlassInteriorTint->A.Expression == TintExpression &&
        GlassInteriorTint->B.Expression == GlassInteriorBrightness &&
        !GlassFresnel->ExponentIn.Expression &&
        !GlassFresnel->BaseReflectFractionIn.Expression &&
        !GlassFresnel->Normal.Expression &&
        FMath::IsNearlyEqual(
            GlassFresnel->Exponent,
            OpaqueGlassFresnelExponent,
            0.000001f) &&
        FMath::IsNearlyEqual(
            GlassFresnel->BaseReflectFraction,
            OpaqueGlassFresnelBaseReflectance,
            0.000001f) &&
        FMath::IsNearlyEqual(
            GlassReflectionStrength->R,
            OpaqueGlassReflectionStrength,
            0.000001f) &&
        GlassFresnelWeight->A.Expression == GlassFresnel &&
        GlassFresnelWeight->B.Expression == GlassReflectionStrength &&
        GlassFresnelSaturate->Input.Expression == GlassFresnelWeight &&
        GlassReflectionTint->Constant.Equals(
            OpaqueGlassReflectionTint, 0.000001f) &&
        GlassBase->A.Expression == GlassInteriorTint &&
        GlassBase->B.Expression == GlassReflectionTint &&
        GlassBase->Alpha.Expression == GlassFresnelSaturate &&
        FMath::IsNearlyEqual(
            GlassRoughnessVariation->R,
            OpaqueGlassRoughnessVariation,
            0.000001f) &&
        GlassRoughnessBand->A.Expression == GlassInteriorBands &&
        GlassRoughnessBand->B.Expression == GlassRoughnessVariation &&
        GlassRoughness->A.Expression == RoughnessExpression &&
        GlassRoughness->B.Expression == GlassRoughnessBand &&
        GlassRoughnessSaturate->Input.Expression == GlassRoughness;
    if (TextureParameters.Num() != ExpectedTextureParameterCount ||
        ScalarParameters.Num() != ExpectedScalarParameterCount ||
        VectorParameters.Num() != ExpectedVectorParameterCount ||
        StaticBoolParameterCount != 1 ||
        StaticSwitchNodeCount != 5 ||
        !bSwitchControlsValid ||
        !bExactStaticBranchTopology ||
        !bExactOpaqueGlassTopology ||
        !SameSet(TextureParameters, ExpectedTextures) ||
        !SameSet(ScalarParameters, ExpectedScalars) ||
        !SameSet(VectorParameters, ExpectedVectors))
    {
        OutError = TEXT("R30 master drifted from its exact texture roster, static branches, or opaque Fresnel glass topology.");
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

void SetScalar(
    UMaterialInstanceConstant* Instance,
    const FName& Name,
    float Value)
{
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(Name), Value);
}

UMaterialInstanceConstant* CreateInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    UMaterialInstanceConstantFactoryNew* Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (Factory)
    {
        Factory->InitialParent = Parent;
    }
    UMaterialInstanceConstant* Instance = Factory
        ? Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
              Spec.Name,
              MaterialRoot,
              UMaterialInstanceConstant::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR30FacadeLookdev"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create R30 facade-lookdev material '%s'."),
            Spec.Name);
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(TintParameter), Spec.Tint);
    SetScalar(Instance, UvScaleParameter, 1.0f / Spec.MetresPerTile);
    SetScalar(Instance, TextureInfluenceParameter, Spec.TextureInfluence);
    SetScalar(Instance, NormalStrengthParameter, Spec.NormalStrength);
    SetScalar(Instance, RoughnessParameter, Spec.Roughness);
    SetScalar(
        Instance, RoughnessTextureWeightParameter,
        Spec.RoughnessTextureWeight);
    SetScalar(Instance, MetallicParameter, Spec.Metallic);
    SetScalar(
        Instance, MetallicTextureWeightParameter,
        Spec.MetallicTextureWeight);
    SetScalar(Instance, SpecularParameter, Spec.Specular);
    SetScalar(Instance, AoTextureWeightParameter, Spec.AoTextureWeight);
    Instance->SetStaticSwitchParameterValueEditorOnly(
        FMaterialParameterInfo(UseTextureSetParameter), Spec.bUseTextureSet);
    if (Spec.bUseTextureSet)
    {
        Instance->SetTextureParameterValueEditorOnly(
            FMaterialParameterInfo(BaseColorTextureParameter),
            FindTexture(Textures, Spec.TextureSet, ETextureUsage::BaseColor));
        Instance->SetTextureParameterValueEditorOnly(
            FMaterialParameterInfo(NormalTextureParameter),
            FindTexture(Textures, Spec.TextureSet, ETextureUsage::Normal));
        Instance->SetTextureParameterValueEditorOnly(
            FMaterialParameterInfo(PackedOrmTextureParameter),
            FindTexture(Textures, Spec.TextureSet, ETextureUsage::PackedOrm));
    }
    Instance->UpdateStaticPermutation();
    Instance->PostEditChange();
    Instance->EnsureIsComplete();
    Instance->MarkPackageDirty();
    return Instance;
}

bool ValidateInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    bool bRequireSaved,
    FString& OutError)
{
    if (!Instance || Instance->GetPathName() != MaterialObjectPath(Spec) ||
        Instance->Parent != Parent ||
        Instance->ScalarParameterValues.Num() != ExpectedScalarParameterCount ||
        Instance->VectorParameterValues.Num() != ExpectedVectorParameterCount ||
        Instance->TextureParameterValues.Num() !=
            (Spec.bUseTextureSet ? ExpectedTextureParameterCount : 0) ||
        !Instance->RuntimeVirtualTextureParameterValues.IsEmpty() ||
        !Instance->SparseVolumeTextureParameterValues.IsEmpty() ||
        Instance->GetPathName().Contains(TEXT("HeroMaterials")) ||
        (bRequireSaved &&
         (!Instance->GetOutermost() || Instance->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              FPackageName::ObjectPathToPackageName(
                  MaterialObjectPath(Spec))))))
    {
        OutError = FString::Printf(
            TEXT("R30 material '%s' lost its exact parent/parameter/save contract."),
            Spec.Name);
        return false;
    }

    const FStaticParameterSet StaticParameters = Instance->GetStaticParameters();
    if (StaticParameters.StaticSwitchParameters.Num() != 1 ||
        StaticParameters.StaticSwitchParameters[0].ParameterInfo.Name !=
            UseTextureSetParameter ||
        !StaticParameters.StaticSwitchParameters[0].IsOverride() ||
        StaticParameters.StaticSwitchParameters[0].Value != Spec.bUseTextureSet)
    {
        OutError = FString::Printf(
            TEXT("R30 material '%s' lost its exact UseTextureSet permutation."),
            Spec.Name);
        return false;
    }

    const TMap<FName, float> ExpectedScalars = {
        {UvScaleParameter, 1.0f / Spec.MetresPerTile},
        {TextureInfluenceParameter, Spec.TextureInfluence},
        {NormalStrengthParameter, Spec.NormalStrength},
        {RoughnessParameter, Spec.Roughness},
        {RoughnessTextureWeightParameter, Spec.RoughnessTextureWeight},
        {MetallicParameter, Spec.Metallic},
        {MetallicTextureWeightParameter, Spec.MetallicTextureWeight},
        {SpecularParameter, Spec.Specular},
        {AoTextureWeightParameter, Spec.AoTextureWeight}};
    TSet<FName> SeenScalars;
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        const float* Expected = ExpectedScalars.Find(Value.ParameterInfo.Name);
        if (!Expected || SeenScalars.Contains(Value.ParameterInfo.Name) ||
            !FMath::IsNearlyEqual(Value.ParameterValue, *Expected, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("R30 material '%s' has a missing, duplicate, or drifted scalar override."),
                Spec.Name);
            return false;
        }
        SeenScalars.Add(Value.ParameterInfo.Name);
    }
    if (SeenScalars.Num() != ExpectedScalars.Num() ||
        Instance->VectorParameterValues[0].ParameterInfo.Name != TintParameter ||
        !Instance->VectorParameterValues[0].ParameterValue.Equals(
            Spec.Tint, 0.000001f))
    {
        OutError = FString::Printf(
            TEXT("R30 material '%s' has a drifted scalar/vector value roster."),
            Spec.Name);
        return false;
    }

    TMap<FName, FString> ExpectedTexturePaths;
    if (Spec.bUseTextureSet)
    {
        ExpectedTexturePaths = {
            {BaseColorTextureParameter,
             TextureObjectPath(Spec.TextureSet, ETextureUsage::BaseColor)},
            {NormalTextureParameter,
             TextureObjectPath(Spec.TextureSet, ETextureUsage::Normal)},
            {PackedOrmTextureParameter,
             TextureObjectPath(Spec.TextureSet, ETextureUsage::PackedOrm)}};
    }
    TSet<FName> SeenTextures;
    for (const FTextureParameterValue& Value : Instance->TextureParameterValues)
    {
        const FString* Expected = ExpectedTexturePaths.Find(
            Value.ParameterInfo.Name);
        if (!Expected || !Value.ParameterValue ||
            Value.ParameterValue->GetPathName() != *Expected ||
            Value.ParameterValue->GetPathName().Contains(TEXT("HeroMaterials")) ||
            SeenTextures.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("R30 material '%s' has a missing, duplicate, or forbidden texture override."),
                Spec.Name);
            return false;
        }
        SeenTextures.Add(Value.ParameterInfo.Name);
    }
    if (SeenTextures.Num() != ExpectedTexturePaths.Num())
    {
        OutError = FString::Printf(
            TEXT("R30 material '%s' does not bind exactly three admitted textures."),
            Spec.Name);
        return false;
    }
    return ValidateCompiledMaterial(Instance, OutError);
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    return true;
}

bool LoadAllTextures(
    TMap<FName, UTexture2D*>& OutTextures,
    FString& OutError)
{
    OutTextures.Reset();
    for (const TCHAR* TextureSet : TextureSets)
    {
        if (!LoadTextureSet(TextureSet, OutTextures, OutError))
        {
            return false;
        }
    }
    return OutTextures.Num() == UE_ARRAY_COUNT(TextureSets) * 3;
}

bool ValidateInternal(FString& OutReport)
{
    if (!ValidateSourceContract(OutReport))
    {
        return false;
    }
    FString R29Report;
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::ValidateAssets(
            R29Report))
    {
        OutReport = TEXT("R30 refused invalid immutable R29 facade assets: ") +
            R29Report;
        return false;
    }
    TMap<FName, UTexture2D*> Textures;
    if (!LoadAllTextures(Textures, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<FAssetData> RootAssets;
    GatherRootAssets(RootAssets);
    if (RootAssets.Num() != ExpectedAssetCount)
    {
        OutReport = FString::Printf(
            TEXT("R30 isolated asset-root roster changed: expected=%d actual=%d."),
            ExpectedAssetCount, RootAssets.Num());
        return false;
    }
    UMaterial* Master = LoadExact<UMaterial>(MasterObjectPath);
    if (!ValidateMaster(Master, true, OutReport))
    {
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        if (!ValidateInstance(
                LoadExact<UMaterialInstanceConstant>(MaterialObjectPath(Spec)),
                Spec, Master, true, OutReport))
        {
            return false;
        }
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSETS_VALID exactAssets=12 masters=1 materialInstances=11 facadeMaterialOverrides=11 textureBackedOverrides=8 proceduralOpaqueGlassOverrides=3 materialTextureParameterBindings=24 uniqueImmutableTextureDependencies=12 retainedR29FacadeMesh=true retainedR29Triangles=256850 retainedR29Sections=11 textureBackedTriangles=216828 textureBackedTrianglePercent=84.4 metricUv0=true baseColorTextures=true tangentNormals=true packedOrm=true staticUseTextureSet=true namedStaticBoolParameters=1 plainStaticSwitches=5 duplicateProneStaticSwitchParameterNodes=0 exactStaticBranchTopology=true opaqueGlassFresnel=true opaqueGlassTangentParallax=true opaqueGlassInteriorBandPeriodMetres=3 opaqueGlassRoughnessVariation=0.018 opaqueGlassTranslucency=false exactOpaqueGlassTopology=true activeFeatureLevel=SM6 compiledActiveFeatureLevelValidated=true allowedTextureRoot=/Game/TRIAD/IstanaPublicView/Textures heroMaterialDependencies=0 sourceContractBytes=8277 sourceContractSha256=BD11A517E8AC3F6EDBE0F6FE0912409D11180AD4FFAACD91E684391CC2B19F03 meshPackagesMutated=false topologyModified=false transformsModified=false geographyModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false cesiumModified=false providerPolicyModified=false vegetationModified=false terrainModified=false surveyClaim=false asBuiltClaim=false currentCompleteClaim=false physicalMaterialClaim=false visualCaptureAccepted=false captureRevalidationRequired=true.");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory
{
const FString& GetAssetRootPath()
{
    return AssetRoot;
}

const FString& GetMasterMaterialObjectPath()
{
    return MasterObjectPath;
}

const TArray<FString>& GetExpectedAssetObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result = {MasterObjectPath};
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Result.Add(MaterialObjectPath(Spec));
        }
        Result.Sort();
        return Result;
    }();
    return Paths;
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    FString ExistingReport;
    if (ValidateInternal(ExistingReport))
    {
        for (const FString& Path : GetExpectedAssetObjectPaths())
        {
            OutAssets.Add(LoadObject<UObject>(nullptr, *Path));
        }
        OutError = ExistingReport;
        return true;
    }
    if (!ValidateSourceContract(OutError))
    {
        return false;
    }
    FString R29Report;
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::ValidateAssets(
            R29Report))
    {
        OutError = TEXT("R30 creation refused invalid immutable R29 assets: ") +
            R29Report;
        return false;
    }
    TArray<FAssetData> Existing;
    GatherRootAssets(Existing);
    if (!Existing.IsEmpty())
    {
        OutError = TEXT("R30 creation refuses an invalid or partially populated isolated asset root.");
        return false;
    }
    TMap<FName, UTexture2D*> Textures;
    if (!LoadAllTextures(Textures, OutError))
    {
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Master = CreateMaster(AssetTools, Textures, OutError);
    if (!Master)
    {
        return false;
    }
    OutAssets.Add(Master);
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance = CreateInstance(
            AssetTools, Spec, Master, Textures, OutError);
        if (!Instance)
        {
            return false;
        }
        OutAssets.Add(Instance);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (OutAssets.Num() != ExpectedAssetCount ||
        !ValidateMaster(Master, false, OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MaterialSpecs); ++Index)
    {
        if (!ValidateInstance(
                Cast<UMaterialInstanceConstant>(OutAssets[Index + 1]),
                MaterialSpecs[Index], Master, false, OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(OutReport);
}

bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR30FacadeLookdevAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DR30FacadeLookdevAssets{};
    if (!ValidateInternal(OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets R29Assets;
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::
            LoadValidatedRuntimeContract(R29Assets, OutError))
    {
        return false;
    }
    OutAssets.RetainedR28ConnectivePublicRealmMesh =
        R29Assets.RetainedR28ConnectivePublicRealmMesh;
    OutAssets.RetainedR29ContextFacadeCoverageMesh =
        R29Assets.ContextFacadeCoverageMesh;
    OutAssets.RetainedOuterGroundMesh = R29Assets.RetainedOuterGroundMesh;
    OutAssets.RetainedR28OuterGroundMaterial =
        R29Assets.RetainedR28OuterGroundMaterial;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        OutAssets.ContextFacadeMaterialOverrides.Add(
            LoadExact<UMaterialInstanceConstant>(MaterialObjectPath(Spec)));
    }
    if (!ATRIADIstanaExploreV5DR30FacadeLookdevActor::ValidateAssetRoster(
            OutAssets, OutError))
    {
        OutAssets = FTRIADIstanaExploreV5DR30FacadeLookdevAssets{};
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory
