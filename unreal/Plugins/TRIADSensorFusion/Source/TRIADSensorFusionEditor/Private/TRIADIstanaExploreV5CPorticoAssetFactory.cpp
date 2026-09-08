#include "TRIADIstanaExploreV5CPorticoAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Ssl.h"
#include "UObject/Package.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Portico"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MeshName(TEXT("SM_IPV5C_CentralPorticoFidelityOverlay"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);

constexpr int32 ExpectedAssetCount = 9;
constexpr int32 ExpectedTriangleCount = 13772;
constexpr int32 ExpectedMaterialCount = 8;
constexpr int64 ExpectedObjBytes = 2913389;
constexpr int64 ExpectedMtlBytes = 1432;
constexpr int64 ExpectedManifestBytes = 8252;
const FString ExpectedObjSha256(
    TEXT("883EAC65449A54D284D1E0E15B42F134F73C509A5ACE95BFD6DDD1DD326A3EC5"));
const FString ExpectedMtlSha256(
    TEXT("DC38D80587AEDFD31143A755388D4248A7C6E2029C4C9FD92794DDE06F2E339C"));
const FString ExpectedManifestSha256(
    TEXT("380A2DE1204029C26FA06A7BBFD99105AA9D7979625575B28FF3E45A4B0337A1"));
const FString ExpectedSemanticSha256(
    TEXT("84E99970FEAF2F9ACCE9D7BE07C320C4A7E8AAA342F5C30463F0ADE311958567"));

struct FMaterialSpec
{
    const TCHAR* Name;
    FLinearColor BaseColor;
    float RoughnessMean;
    float AlbedoBreakup;
    float RoughnessBreakup;
    float Metallic;
    float Specular;
    bool bGlazing;
    int32 Triangles;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("M_IPV5C_Portico_StoneWarm"), FLinearColor(0.4452f, 0.4564f, 0.4452f), 0.66f, 0.016f, 0.028f, 0.0f, 0.35f, false, 2616},
    {TEXT("M_IPV5C_Portico_TrimIvory"), FLinearColor(0.7157f, 0.7157f, 0.6724f), 0.54f, 0.010f, 0.020f, 0.0f, 0.35f, false, 5504},
    {TEXT("M_IPV5C_Portico_RenderWarm"), FLinearColor(0.5029f, 0.5029f, 0.4793f), 0.72f, 0.024f, 0.050f, 0.0f, 0.30f, false, 180},
    {TEXT("M_IPV5C_Portico_RecessWarmShadow"), FLinearColor(0.0160f, 0.0144f, 0.0116f), 0.84f, 0.004f, 0.012f, 0.0f, 0.15f, false, 240},
    {TEXT("M_IPV5C_Portico_Soffit"), FLinearColor(0.2831f, 0.2747f, 0.2462f), 0.72f, 0.012f, 0.020f, 0.0f, 0.25f, false, 1632},
    {TEXT("M_IPV5C_Portico_DoorTimber"), FLinearColor(0.0648f, 0.0319f, 0.0160f), 0.56f, 0.014f, 0.024f, 0.0f, 0.40f, false, 432},
    {TEXT("M_IPV5C_Portico_FanlightGlass"), FLinearColor(0.0296f, 0.0343f, 0.0369f), 0.14f, 0.003f, 0.018f, 0.0f, 0.55f, true, 288},
    {TEXT("M_IPV5C_Portico_LouvreWarmIvory"), FLinearColor(0.3916f, 0.3712f, 0.3278f), 0.58f, 0.008f, 0.018f, 0.0f, 0.28f, false, 2880}};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == ExpectedMaterialCount);

const FString SurfaceWorldPositionDescription(
    TEXT("V5C_PORTICO_WORLD_POSITION_NO_SHADER_OFFSETS"));
const FString SurfaceResponseDescription(
    TEXT("TRIAD_IPV5C_PORTICO_PROCEDURAL_NO_TEXTURE_PBR_RESPONSE_V1"));
const FString SurfaceBaseOutputDescription(
    TEXT("V5C_PORTICO_PROCEDURAL_BASE_COLOR_RGB"));
const FString SurfaceRoughnessOutputDescription(
    TEXT("V5C_PORTICO_PROCEDURAL_ROUGHNESS_A"));
const FString SurfaceMetallicDescription(
    TEXT("V5C_PORTICO_DIELECTRIC_METALLIC"));
const FString SurfaceSpecularDescription(
    TEXT("V5C_PORTICO_BOUNDED_SPECULAR"));
const FString SurfaceGlassFresnelDescription(
    TEXT("V5C_PORTICO_OPAQUE_GLASS_FRESNEL"));

FString MakeSurfaceResponseCode(const FMaterialSpec& Spec)
{
    FString Code = FString::Printf(
        TEXT("float3 p = WorldPosition * 0.01;\n")
        TEXT("float macro = 0.5 + 0.25 * sin(dot(p, float3(0.73, 1.07, 0.61)) + 0.7) + 0.15 * sin(dot(p, float3(-1.31, 0.83, 1.19)) + 2.1);\n")
        TEXT("float detail = 0.5 + 0.25 * sin(dot(p, float3(12.7, -9.7, 11.3)) + 1.3) + 0.15 * sin(dot(p, float3(-19.1, 17.3, 13.9)) + 0.4);\n")
        TEXT("float n = (0.72 * macro + 0.28 * detail - 0.5) * 2.0;\n")
        TEXT("float3 baseColor = saturate(float3(%.6f, %.6f, %.6f) * (1.0 + n * %.6f));\n")
        TEXT("float roughness = saturate(%.6f + n * %.6f);\n"),
        Spec.BaseColor.R,
        Spec.BaseColor.G,
        Spec.BaseColor.B,
        Spec.AlbedoBreakup,
        Spec.RoughnessMean,
        Spec.RoughnessBreakup);
    if (Spec.bGlazing)
    {
        Code +=
            TEXT("float edge = saturate((Fresnel - 0.04) / 0.96);\n")
            TEXT("baseColor = lerp(baseColor, saturate(baseColor * 2.2), edge * 0.45);\n")
            TEXT("roughness = max(0.10, roughness - edge * 0.04);\n");
    }
    Code += TEXT("return float4(baseColor, roughness);");
    return Code;
}

// UE5.5's legacy FBX/OBJ path applies the FBX-node material roster when
// bReorderMaterialToFbxOrder is enabled. Pin that observed import permutation,
// then normalize the persisted mesh back to the manifest's canonical order.
constexpr int32 ExpectedImportedToCanonical[] = {5, 6, 1, 0, 7, 3, 2, 4};
static_assert(
    UE_ARRAY_COUNT(ExpectedImportedToCanonical) == ExpectedMaterialCount);

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const TArray<FString>& OrderedMaterialPaths()
{
    static TArray<FString> Paths;
    if (Paths.IsEmpty())
    {
        Paths.Reserve(ExpectedMaterialCount);
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Paths.Add(ObjectPath(MaterialRoot, Spec.Name));
        }
    }
    return Paths;
}

FString ProjectSourcePath(const TCHAR* Relative)
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceAssets"), Relative));
}

FString ObjSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Portico/Generated/SM_IPV5C_CentralPorticoFidelityOverlay.obj"));
}

FString MtlSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Portico/Generated/SM_IPV5C_CentralPorticoFidelityOverlay.mtl"));
}

FString ManifestSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Portico/Generated/IstanaPublicViewV5CPorticoFidelityOverlay.manifest.json"));
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
            TEXT("V5C source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            ExpectedBytes,
            Bytes.Num());
        return false;
    }
    FString ActualSha256;
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
        nullptr)
    {
        OutError = TEXT("V5C source SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5C source SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5C source SHA-256 guard failed for '%s': expected=%s actual=%s."),
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
    return ValidateSourceHash(
               ObjSourcePath(), ExpectedObjBytes, ExpectedObjSha256, OutError) &&
        ValidateSourceHash(
               MtlSourcePath(), ExpectedMtlBytes, ExpectedMtlSha256, OutError) &&
        ValidateSourceHash(
               ManifestSourcePath(), ExpectedManifestBytes,
               ExpectedManifestSha256, OutError);
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets, FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("V5C Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExpectedObjectPaths()
{
    TArray<FString> Paths = OrderedMaterialPaths();
    Paths.Add(MeshObjectPath);
    Paths.Sort();
    return Paths;
}

bool ValidateExactRootRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherRootAssets(Assets, OutError))
    {
        return false;
    }
    const TArray<FString> Expected = ExpectedObjectPaths();
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("V5C portico root must contain exactly nine assets; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        for (const FString& Path : Expected)
        {
            UObject* Object = LoadObject<UObject>(nullptr, *Path);
            if (!Object || !FPackageName::DoesPackageExist(
                    Object->GetOutermost()->GetName()))
            {
                OutError = TEXT("A V5C portico asset is not persisted: ") + Path;
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
    FScopedFreshRollback(TArray<UObject*>& InAssets, FString& InError)
        : Assets(InAssets), Error(InError)
    {
    }

    ~FScopedFreshRollback()
    {
        if (bCommitted)
        {
            return;
        }
        TArray<FAssetData> Data;
        FString Ignored;
        GatherRootAssets(Data, Ignored);
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
            ObjectTools::DeleteObjectsUnchecked(Disposable) != Disposable.Num())
        {
            Error += TEXT(" V5C_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
    }

    void Commit() { bCommitted = true; }

private:
    TArray<UObject*>& Assets;
    FString& Error;
    bool bCommitted = false;
};

template <typename T>
T* AddExpression(
    UMaterial* Material,
    const FString& Description,
    int32 X,
    int32 Y)
{
    UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    T* Expression = Data
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        Data->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterial* CreateMaterial(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              Spec.Name,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5CPorticoAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Material || !Data)
    {
        OutError = FString::Printf(
            TEXT("Could not create exact V5C material '%s'."), Spec.Name);
        return nullptr;
    }

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;

    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, SurfaceWorldPositionDescription, -760, -160);
    UMaterialExpressionCustom* SurfaceResponse =
        AddExpression<UMaterialExpressionCustom>(
            Material, SurfaceResponseDescription, -520, -160);
    UMaterialExpressionComponentMask* BaseOutput =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, SurfaceBaseOutputDescription, -240, -180);
    UMaterialExpressionComponentMask* RoughnessOutput =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, SurfaceRoughnessOutputDescription, -240, -40);
    UMaterialExpressionConstant* Metallic =
        AddExpression<UMaterialExpressionConstant>(
            Material, SurfaceMetallicDescription, -240, 100);
    UMaterialExpressionConstant* Specular =
        AddExpression<UMaterialExpressionConstant>(
            Material, SurfaceSpecularDescription, -240, 220);
    UMaterialExpressionFresnel* GlassFresnel = Spec.bGlazing
        ? AddExpression<UMaterialExpressionFresnel>(
              Material, SurfaceGlassFresnelDescription, -760, 40)
        : nullptr;
    if (!WorldPosition || !SurfaceResponse || !BaseOutput ||
        !RoughnessOutput || !Metallic || !Specular ||
        (Spec.bGlazing && !GlassFresnel))
    {
        OutError = FString::Printf(
            TEXT("Could not author exact V5C material graph '%s'."), Spec.Name);
        return nullptr;
    }
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    SurfaceResponse->Description = SurfaceResponseDescription;
    SurfaceResponse->Code = MakeSurfaceResponseCode(Spec);
    SurfaceResponse->OutputType = CMOT_Float4;
    SurfaceResponse->Inputs.SetNum(Spec.bGlazing ? 2 : 1);
    SurfaceResponse->Inputs[0].InputName = TEXT("WorldPosition");
    SurfaceResponse->Inputs[0].Input.Connect(0, WorldPosition);
    if (GlassFresnel)
    {
        GlassFresnel->Exponent = 5.0f;
        GlassFresnel->BaseReflectFraction = 0.04f;
        SurfaceResponse->Inputs[1].InputName = TEXT("Fresnel");
        SurfaceResponse->Inputs[1].Input.Connect(0, GlassFresnel);
    }
    BaseOutput->R = true;
    BaseOutput->G = true;
    BaseOutput->B = true;
    BaseOutput->A = false;
    BaseOutput->Input.Connect(0, SurfaceResponse);
    RoughnessOutput->R = false;
    RoughnessOutput->G = false;
    RoughnessOutput->B = false;
    RoughnessOutput->A = true;
    RoughnessOutput->Input.Connect(0, SurfaceResponse);
    Metallic->R = Spec.Metallic;
    Specular->R = Spec.Specular;
    Data->BaseColor.Connect(0, BaseOutput);
    Data->Roughness.Connect(0, RoughnessOutput);
    Data->Metallic.Connect(0, Metallic);
    Data->Specular.Connect(0, Specular);
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

bool ValidateMaterial(
    UMaterial* Material,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    const UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    const UMaterialExpressionCustom* SurfaceResponse = nullptr;
    const UMaterialExpressionComponentMask* BaseOutput = nullptr;
    const UMaterialExpressionComponentMask* RoughnessOutput = nullptr;
    const UMaterialExpressionConstant* Metallic = nullptr;
    const UMaterialExpressionConstant* Specular = nullptr;
    const UMaterialExpressionFresnel* GlassFresnel = nullptr;
    int32 WorldPositionCount = 0;
    int32 CustomCount = 0;
    int32 ComponentMaskCount = 0;
    int32 ConstantCount = 0;
    int32 FresnelCount = 0;
    if (Data)
    {
        for (const UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (const auto* WorldPositionCandidate =
                    Cast<UMaterialExpressionWorldPosition>(Expression))
            {
                ++WorldPositionCount;
                WorldPosition = WorldPositionCandidate;
            }
            else if (const auto* CustomCandidate =
                         Cast<UMaterialExpressionCustom>(Expression))
            {
                ++CustomCount;
                SurfaceResponse = CustomCandidate;
            }
            else if (const auto* MaskCandidate =
                         Cast<UMaterialExpressionComponentMask>(Expression))
            {
                ++ComponentMaskCount;
                if (MaskCandidate->R && MaskCandidate->G && MaskCandidate->B &&
                    !MaskCandidate->A)
                {
                    BaseOutput = MaskCandidate;
                }
                if (!MaskCandidate->R && !MaskCandidate->G && !MaskCandidate->B &&
                    MaskCandidate->A)
                {
                    RoughnessOutput = MaskCandidate;
                }
            }
            else if (const auto* ConstantCandidate =
                         Cast<UMaterialExpressionConstant>(Expression))
            {
                ++ConstantCount;
                if (ConstantCandidate->Desc == SurfaceMetallicDescription)
                {
                    Metallic = ConstantCandidate;
                }
                if (ConstantCandidate->Desc == SurfaceSpecularDescription)
                {
                    Specular = ConstantCandidate;
                }
            }
            else if (const auto* FresnelCandidate =
                         Cast<UMaterialExpressionFresnel>(Expression))
            {
                ++FresnelCount;
                GlassFresnel = FresnelCandidate;
            }
        }
    }
    const auto InputIs = [](
        const FExpressionInput& Input,
        const UMaterialExpression* Expected)
    {
        return Input.Expression == Expected && Input.OutputIndex == 0;
    };
    const int32 ExpectedExpressionCount = Spec.bGlazing ? 7 : 6;
    const int32 ExpectedFresnelCount = Spec.bGlazing ? 1 : 0;
    const bool bExactCustomInputs = SurfaceResponse &&
        SurfaceResponse->Inputs.Num() == (Spec.bGlazing ? 2 : 1) &&
        SurfaceResponse->Inputs[0].InputName == FName(TEXT("WorldPosition")) &&
        InputIs(SurfaceResponse->Inputs[0].Input, WorldPosition) &&
        (!Spec.bGlazing ||
         (SurfaceResponse->Inputs[1].InputName == FName(TEXT("Fresnel")) &&
          InputIs(SurfaceResponse->Inputs[1].Input, GlassFresnel)));
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != ObjectPath(MaterialRoot, Spec.Name) ||
        !Data ||
        Data->ExpressionCollection.Expressions.Num() != ExpectedExpressionCount ||
        WorldPositionCount != 1 || CustomCount != 1 ||
        ComponentMaskCount != 2 || ConstantCount != 2 ||
        FresnelCount != ExpectedFresnelCount || !WorldPosition ||
        !SurfaceResponse || !BaseOutput || !RoughnessOutput ||
        !Metallic || !Specular || !bExactCustomInputs ||
        WorldPosition->Desc != SurfaceWorldPositionDescription ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        SurfaceResponse->Desc != SurfaceResponseDescription ||
        SurfaceResponse->Description != SurfaceResponseDescription ||
        SurfaceResponse->OutputType != CMOT_Float4 ||
        !SurfaceResponse->AdditionalOutputs.IsEmpty() ||
        !SurfaceResponse->AdditionalDefines.IsEmpty() ||
        !SurfaceResponse->IncludeFilePaths.IsEmpty() ||
        SurfaceResponse->Code != MakeSurfaceResponseCode(Spec) ||
        BaseOutput->Desc != SurfaceBaseOutputDescription ||
        RoughnessOutput->Desc != SurfaceRoughnessOutputDescription ||
        !InputIs(BaseOutput->Input, SurfaceResponse) ||
        !InputIs(RoughnessOutput->Input, SurfaceResponse) ||
        Metallic->Desc != SurfaceMetallicDescription ||
        Specular->Desc != SurfaceSpecularDescription ||
        !FMath::IsNearlyEqual(Metallic->R, Spec.Metallic, 0.000001f) ||
        !FMath::IsNearlyEqual(Specular->R, Spec.Specular, 0.000001f) ||
        (Spec.bGlazing &&
         (!GlassFresnel ||
          GlassFresnel->Desc != SurfaceGlassFresnelDescription ||
          GlassFresnel->ExponentIn.Expression ||
          GlassFresnel->BaseReflectFractionIn.Expression ||
          GlassFresnel->Normal.Expression ||
          !FMath::IsNearlyEqual(GlassFresnel->Exponent, 5.0f, 0.000001f) ||
          !FMath::IsNearlyEqual(
              GlassFresnel->BaseReflectFraction, 0.04f, 0.000001f))) ||
        Data->BaseColor.Expression != BaseOutput ||
        Data->BaseColor.OutputIndex != 0 ||
        Data->Roughness.Expression != RoughnessOutput ||
        Data->Roughness.OutputIndex != 0 ||
        Data->Metallic.Expression != Metallic ||
        Data->Metallic.OutputIndex != 0 ||
        Data->Specular.Expression != Specular ||
        Data->Specular.OutputIndex != 0 ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !FMath::IsNearlyZero(
            Material->MaxWorldPositionOffsetDisplacement, 0.000001f) ||
        Data->Normal.Expression || Data->EmissiveColor.Expression ||
        Data->AmbientOcclusion.Expression || Data->SubsurfaceColor.Expression ||
        Data->Refraction.Expression ||
        Data->Opacity.Expression || Data->OpacityMask.Expression ||
        Data->WorldPositionOffset.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5C bounded procedural no-texture PBR material drifted: %s."),
            Spec.Name);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledMaterial(
    UMaterial* Material,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(ERHIFeatureLevel::SM5)
        : nullptr;
    if (Resource)
    {
        Resource->FinishCompilation();
    }
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Material || !Resource ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        !Resource->IsCompilationFinished() ||
        !Resource->GetGameThreadShaderMap() ||
        !Resource->IsGameThreadShaderMapComplete() || Errors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("V5C portico material '%s' has no complete compiled SM5 shader map: [%s]."),
            Spec.Name,
            *FString::Join(Errors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

UAssetImportTask* MakeImportTask()
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
    Options->StaticMeshImportData->ImportUniformScale = 1.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = true;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = false;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = ObjSourcePath();
    Task->DestinationPath = AssetRoot;
    Task->DestinationName = MeshName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool ValidateImportTask(const UAssetImportTask* Task, FString& OutError)
{
    const UFbxImportUI* Options =
        Task ? Cast<UFbxImportUI>(Task->Options) : nullptr;
    const UFbxStaticMeshImportData* Data =
        Options ? Options->StaticMeshImportData : nullptr;
    if (!Task || !Options || !Data ||
        !FPaths::IsSamePath(Task->Filename, ObjSourcePath()) ||
        Task->DestinationPath != AssetRoot ||
        Task->DestinationName != MeshName || Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType || !Options->bImportMesh ||
        Options->bImportMaterials || Options->bImportTextures ||
        Data->bConvertScene || Data->bConvertSceneUnit ||
        Data->bForceFrontXAxis ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 1.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        !Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ImportNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The exact V5C identity OBJ import policy changed.");
        return false;
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

void MakeRenderOnly(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    Mesh->Modify();
    Mesh->NaniteSettings.bEnabled = false;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
}

bool NormalizeAndBindMaterials(
    UStaticMesh* Mesh,
    const TArray<UMaterial*>& Materials,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Materials.Num() != ExpectedMaterialCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("V5C material normalization requires the exact eight-slot/eight-section imported mesh.");
        return false;
    }

    TMap<FName, int32> CanonicalOrder;
    for (int32 CanonicalIndex = 0;
         CanonicalIndex < ExpectedMaterialCount;
         ++CanonicalIndex)
    {
        const FName Name(MaterialSpecs[CanonicalIndex].Name);
        if (!Materials[CanonicalIndex] || CanonicalOrder.Contains(Name))
        {
            OutError = TEXT("V5C canonical material roster is incomplete or ambiguous.");
            return false;
        }
        CanonicalOrder.Add(Name, CanonicalIndex);
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
    TArray<FString> ImportedMaterialDigest;
    ImportedMaterialDigest.Reserve(ImportedMaterials.Num());
    for (const FStaticMaterial& Imported : ImportedMaterials)
    {
        ImportedMaterialDigest.Add(Imported.MaterialSlotName.ToString());
    }
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(ExpectedMaterialCount);
    TArray<int32> ImportedToCanonical;
    ImportedToCanonical.Init(INDEX_NONE, ExpectedMaterialCount);
    TArray<bool> SeenCanonical;
    SeenCanonical.Init(false, ExpectedMaterialCount);
    for (int32 ImportedIndex = 0;
         ImportedIndex < ExpectedMaterialCount;
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const int32* CanonicalIndex =
            CanonicalOrder.Find(Imported.MaterialSlotName);
        if (!CanonicalIndex || Imported.MaterialSlotName.IsNone() ||
            Imported.MaterialSlotName != Imported.ImportedMaterialSlotName ||
            SeenCanonical[*CanonicalIndex] ||
            *CanonicalIndex != ExpectedImportedToCanonical[ImportedIndex])
        {
            OutError = FString::Printf(
                TEXT("V5C OBJ material slots are not an exact unique canonical permutation at imported index %d: slot='%s' imported='%s' roster=[%s]."),
                ImportedIndex,
                *Imported.MaterialSlotName.ToString(),
                *Imported.ImportedMaterialSlotName.ToString(),
                *FString::Join(ImportedMaterialDigest, TEXT(",")));
            return false;
        }
        SeenCanonical[*CanonicalIndex] = true;
        ImportedToCanonical[ImportedIndex] = *CanonicalIndex;
        OrderedMaterials[*CanonicalIndex] = Imported;
        OrderedMaterials[*CanonicalIndex].MaterialSlotName =
            FName(MaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].ImportedMaterialSlotName =
            FName(MaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].MaterialInterface =
            Materials[*CanonicalIndex];
    }
    for (bool bSeen : SeenCanonical)
    {
        if (!bSeen)
        {
            OutError = TEXT("V5C OBJ omitted one or more canonical material slots.");
            return false;
        }
    }

    const FStaticMeshLODResources& Lod =
        RenderData->LODResources[0];
    for (int32 SectionIndex = 0;
         SectionIndex < Lod.Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection = Lod.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToCanonical.IsValidIndex(ImportedIndex) ||
            ImportedToCanonical[ImportedIndex] == INDEX_NONE ||
            Section.MaterialIndex != ImportedIndex ||
            Original.MaterialIndex != ImportedIndex)
        {
            OutError = FString::Printf(
                TEXT("V5C section %d references an invalid imported material index."),
                SectionIndex);
            return false;
        }
        const int32 CanonicalIndex =
            ImportedToCanonical[ImportedIndex];
        if (RenderSection.NumTriangles !=
            MaterialSpecs[CanonicalIndex].Triangles)
        {
            OutError = FString::Printf(
                TEXT("V5C section %d semantic triangle count changed: material='%s' expected=%d actual=%u."),
                SectionIndex,
                MaterialSpecs[CanonicalIndex].Name,
                MaterialSpecs[CanonicalIndex].Triangles,
                RenderSection.NumTriangles);
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

bool ValidateMesh(
    UStaticMesh* Mesh,
    bool bRequireMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData =
        Mesh ? Mesh->GetAssetImportData() : nullptr;
    const TArray<FString> Sources =
        ImportData ? ImportData->ExtractFilenames() : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = ObjSourcePath();
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Mesh->GetPathName() != MeshObjectPath ||
        Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        Mesh->GetNumSourceModels() != 1 ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D != FVector::OneVector ||
        TriangleCount(Mesh) != ExpectedTriangleCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount ||
        Mesh->NaniteSettings.bEnabled)
    {
        OutError = TEXT("V5C mesh lost exact source, scale, 13,772-triangle, one-LOD, eight-slot or non-Nanite state.");
        return false;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("V5C mesh must have no simple or complex-as-simple collision.");
        return false;
    }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(FVector(-1350.0, 1250.0, 64.0), 0.5) ||
        !BoundsMax.Equals(FVector(1350.0, 2592.0, 2118.0), 0.5))
    {
        OutError = FString::Printf(
            TEXT("V5C imported centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X, BoundsMin.Y, BoundsMin.Z,
            BoundsMax.X, BoundsMax.Y, BoundsMax.Z);
        return false;
    }

    TSet<int32> SeenSections;
    TArray<FString> SectionDigest;
    bool bSectionCensusExact = true;
    const auto& Sections =
        RenderData->LODResources[0].Sections;
    SectionDigest.Reserve(Sections.Num());
    for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
    {
        const FStaticMeshSection& Section = Sections[SectionIndex];
        SectionDigest.Add(FString::Printf(
            TEXT("%d:%d:%u"),
            SectionIndex,
            Section.MaterialIndex,
            Section.NumTriangles));
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedMaterialCount ||
            SeenSections.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                MaterialSpecs[Section.MaterialIndex].Triangles)
        {
            bSectionCensusExact = false;
        }
        if (Section.MaterialIndex >= 0 &&
            Section.MaterialIndex < ExpectedMaterialCount)
        {
            SeenSections.Add(Section.MaterialIndex);
        }
    }
    if (!bSectionCensusExact || SeenSections.Num() != ExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5C material-section triangle census drifted: section:material:triangles=[%s]."),
            *FString::Join(SectionDigest, TEXT(",")));
        return false;
    }
    for (int32 Slot = 0; Slot < ExpectedMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial = Mesh->GetStaticMaterials()[Slot];
        if (StaticMaterial.MaterialSlotName != FName(MaterialSpecs[Slot].Name) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            (bRequireMaterials &&
             (!Mesh->GetMaterial(Slot) ||
              Mesh->GetMaterial(Slot)->GetPathName() !=
                  OrderedMaterialPaths()[Slot])))
        {
            OutError = FString::Printf(
                TEXT("V5C imported material slot/order/binding drifted at %d."),
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateAllSourceHashes(OutReport) ||
        !ValidateExactRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(MaterialRoot, Spec.Name));
        if (!ValidateMaterial(Material, Spec, OutReport) ||
            !ValidateCompiledMaterial(Material, Spec, OutReport))
        {
            return false;
        }
    }
    if (!ValidateMesh(
            LoadExact<UStaticMesh>(MeshObjectPath), true, OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5C_PORTICO_ASSETS_VALID assets=9 meshTriangles=13772 materialSlots=8 objBytes=%lld objSha256=%s mtlBytes=%lld mtlSha256=%s manifestBytes=%lld manifestSha256=%s semanticSha256=%s materials=proceduralNoTexturePBR opaqueGlass=true compiledSM5=true noTextureClaims=true renderOnly=true."),
        ExpectedObjBytes,
        *ExpectedObjSha256,
        ExpectedMtlBytes,
        *ExpectedMtlSha256,
        ExpectedManifestBytes,
        *ExpectedManifestSha256,
        *ExpectedSemanticSha256);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5CPorticoAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetPorticoMeshObjectPath() { return MeshObjectPath; }
const TArray<FString>& GetOrderedMaterialObjectPaths()
{
    return OrderedMaterialPaths();
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    if (!ValidateAllSourceHashes(OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty())
    {
        OutError = TEXT("CreateFreshAssets requires an empty exact V5C portico asset root.");
        return false;
    }

    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    TArray<UMaterial*> Materials;
    Materials.Reserve(ExpectedMaterialCount);
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Material = CreateMaterial(AssetTools, Spec, OutError);
        if (!Material)
        {
            return false;
        }
        Materials.Add(Material);
        OutAssets.Add(Material);
    }

    UAssetImportTask* Task = MakeImportTask();
    if (!Task || !ValidateImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate the exact V5C OBJ import task.");
        }
        return false;
    }
    AssetTools.ImportAssetTasks({Task});
    UStaticMesh* Mesh = nullptr;
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == MeshObjectPath)
        {
            Mesh = Cast<UStaticMesh>(Object);
        }
    }
    if (!Mesh)
    {
        Mesh = LoadExact<UStaticMesh>(MeshObjectPath);
    }
    MakeRenderOnly(Mesh);
    if (!NormalizeAndBindMaterials(Mesh, Materials, OutError) ||
        !ValidateMesh(Mesh, true, OutError))
    {
        return false;
    }
    OutAssets.Add(Mesh);
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Validation;
    if (OutAssets.Num() != ExpectedAssetCount ||
        !ValidateInternal(false, Validation))
    {
        OutError = TEXT("Fresh V5C portico validation failed before save: ") +
            Validation;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5CPorticoAssetFactory
