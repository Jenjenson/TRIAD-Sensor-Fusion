#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.h"

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
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "Ssl.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MeshName(TEXT("SM_IPV5D_OuterGroundLoadingFallback_Render"));
const FString MaterialName(TEXT("M_IPV5D_OuterGroundLoadingFallback"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);
const FString MaterialObjectPath(
    MaterialRoot + TEXT("/") + MaterialName + TEXT(".") + MaterialName);

constexpr int32 ExpectedAssetCount = 2;
constexpr int32 ExpectedSourceCornerCount = 3840;
constexpr int32 ExpectedRenderVertexCount = 768;
constexpr int32 ExpectedTriangleCount = 1280;
constexpr int32 ExpectedMaterialCount = 1;
constexpr int32 ExpectedSectionCount = 1;
const FVector ExpectedBoundsMin(-125000.0, -125000.0, -51.51655292);
const FVector ExpectedBoundsMax(125000.0, 125000.0, 201.68388265);
const FVector2f ExpectedUvMin(-1250.0f, -1250.0f);
const FVector2f ExpectedUvMax(1250.0f, 1250.0f);

const FString MacroNodeId(TEXT("OuterGround.MacroNeutralResponse"));
const FString WorldPositionNodeId(TEXT("OuterGround.AbsoluteWorldPosition"));
const FString BaseColorNodeId(TEXT("OuterGround.BaseColorRGB"));
const FString RoughnessNodeId(TEXT("OuterGround.RoughnessA"));
const FString MetallicNodeId(TEXT("OuterGround.NonMetal"));
const FString SpecularNodeId(TEXT("OuterGround.DielectricSpecular"));
const FString MacroCode(
    TEXT("float2 p = WorldPosition.xy / 3700.0;\n")
    TEXT("float2 i = floor(p);\n")
    TEXT("float2 f = frac(p);\n")
    TEXT("f = f * f * (3.0 - 2.0 * f);\n")
    TEXT("float h00 = frac(sin(dot(i, float2(12.9898, 78.233))) * 43758.5453);\n")
    TEXT("float h10 = frac(sin(dot(i + float2(1,0), float2(12.9898, 78.233))) * 43758.5453);\n")
    TEXT("float h01 = frac(sin(dot(i + float2(0,1), float2(12.9898, 78.233))) * 43758.5453);\n")
    TEXT("float h11 = frac(sin(dot(i + float2(1,1), float2(12.9898, 78.233))) * 43758.5453);\n")
    TEXT("float n = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);\n")
    TEXT("float variation = (n - 0.5) * 0.08;\n")
    TEXT("float3 base = float3(0.19, 0.23, 0.13) * (1.0 + variation);\n")
    TEXT("float roughness = saturate(0.91 + (n - 0.5) * 0.04);\n")
    TEXT("return float4(base, roughness);"));

struct FSourceSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceSpec SourceSpecs[] = {
    {
        TEXT("Generated/SM_IPV5D_OuterGroundLoadingFallback_Render.obj"),
        492909,
        TEXT("13761E4CB255EB94C392F540436DA163AC3A3ECAB877C1D6F8460E5962A84A8C"),
    },
    {
        TEXT("Generated/SM_IPV5D_OuterGroundLoadingFallback_Render.mtl"),
        487,
        TEXT("DCFA8A56AF148E1D4E4FA4DC2E772D7A3E5767B0F91E07F25B3F87C2586A1A6A"),
    },
    {
        TEXT("Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.audit.json"),
        5211,
        TEXT("93F414E63E9221A5160B9533155E6535B72BE8B9ADDDC994D74FE50372E5B997"),
    },
    {
        TEXT("Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json"),
        11710,
        TEXT("9A9A12D127ED34D227F909F06DB55804B741F9ABBB22814BEF82DDC2301FB959"),
    },
    {
        TEXT("Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json"),
        3478,
        TEXT("736785C3AEB8D7D2E2DDC4943314F450DD2DD548C6010E9FBC51D0CEF3C1EBDE"),
    },
    {
        TEXT("outer_ground_loading_fallback_v1.contract.json"),
        9600,
        TEXT("8815790E9BC0D7E4149DF67759F73A9F98738EACED490984B0F5519B883DDE83"),
    },
    {
        TEXT("build_outer_ground_loading_fallback_v1.py"),
        32494,
        TEXT("B582AA05D74DFB3B8F0857E8D8B6A520C3DC9F09A2537A632AA674729B891BE6"),
    },
};
static_assert(UE_ARRAY_COUNT(SourceSpecs) == 7);

FString ToolRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
             "OuterGroundFallback")));
}

FString SourcePath(const TCHAR* RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        ToolRoot(), RelativePath));
}

FString ObjSourcePath()
{
    return SourcePath(SourceSpecs[0].RelativePath);
}

template <typename T>
T* LoadExact(const FString& ObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

bool ValidateSourceHash(const FSourceSpec& Spec, FString& OutError)
{
    const FString Filename = SourcePath(Spec.RelativePath);
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != Spec.Bytes)
    {
        OutError = FString::Printf(
            TEXT("Outer-ground source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            Spec.Bytes,
            Bytes.Num());
        return false;
    }

    FString Actual;
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
        nullptr)
    {
        OutError = TEXT("Outer-ground SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    Actual = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("Outer-ground source admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (Actual != Spec.Sha256)
    {
        OutError = FString::Printf(
            TEXT("Outer-ground source hash guard failed for '%s': expected=%s actual=%s."),
            *Filename,
            Spec.Sha256,
            *Actual);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAllSourceHashes(FString& OutError)
{
    for (const FSourceSpec& Spec : SourceSpecs)
    {
        if (!ValidateSourceHash(Spec, OutError))
        {
            return false;
        }
    }
    return true;
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets, FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    Registry.Get().SearchAllAssets(true);
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("Outer-ground Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateRootRoster(bool bRequireSaved, FString& OutError)
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
    TArray<FString> Expected = {MaterialObjectPath, MeshObjectPath};
    Expected.Sort();
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("Outer-ground root must contain exactly its one material and one mesh; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        const UObject* Objects[] = {
            LoadExact<UMaterial>(MaterialObjectPath),
            LoadExact<UStaticMesh>(MeshObjectPath),
        };
        for (const UObject* Object : Objects)
        {
            const UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (!Package ||
                !FPackageName::DoesPackageExist(Package->GetName()) ||
                Package->IsDirty())
            {
                OutError = TEXT("Both outer-ground assets must be persisted and clean.");
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
    explicit FScopedFreshRollback(FString& InError)
        : Error(InError)
    {
    }

    ~FScopedFreshRollback()
    {
        if (bCommitted)
        {
            return;
        }

        // The root was proven recursively empty before this guard was
        // constructed. Re-discover the whole namespace instead of trusting
        // returned pointers: CreateAsset can register an object before a
        // later graph-authoring failure, and an importer can register
        // unexpected objects that never appear in the expected-result slot.
        TArray<FAssetData> Registered;
        FString DiscoveryError;
        if (!GatherRootAssets(Registered, DiscoveryError))
        {
            Error += TEXT(" OUTER_GROUND_LOADING_FALLBACK_ROLLBACK_DISCOVERY_FAILED: ") +
                DiscoveryError;
            return;
        }
        TArray<UObject*> Disposable;
        for (const FAssetData& Asset : Registered)
        {
            UObject* Object = Asset.GetAsset();
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
            Error += TEXT(" OUTER_GROUND_LOADING_FALLBACK_ROLLBACK_INCOMPLETE");
        }

        TArray<FAssetData> Remaining;
        FString VerificationError;
        if (!GatherRootAssets(Remaining, VerificationError) ||
            !Remaining.IsEmpty())
        {
            Error += TEXT(" OUTER_GROUND_LOADING_FALLBACK_ROLLBACK_ROOT_NOT_EMPTY");
            if (!VerificationError.IsEmpty())
            {
                Error += TEXT(": ") + VerificationError;
            }
        }
    }

    void Commit() { bCommitted = true; }

private:
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

UMaterial* CreateMaterial(IAssetTools& AssetTools, FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MaterialName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DOuterGroundLoadingFallbackAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Material || !Data || !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh V5D outer-ground material.");
        return nullptr;
    }

    auto* WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(
        Material, WorldPositionNodeId, -760, -100);
    auto* Macro = AddExpression<UMaterialExpressionCustom>(
        Material, MacroNodeId, -520, -100);
    auto* BaseColor = AddExpression<UMaterialExpressionComponentMask>(
        Material, BaseColorNodeId, -260, -180);
    auto* Roughness = AddExpression<UMaterialExpressionComponentMask>(
        Material, RoughnessNodeId, -260, -20);
    auto* Metallic = AddExpression<UMaterialExpressionConstant>(
        Material, MetallicNodeId, -260, 120);
    auto* Specular = AddExpression<UMaterialExpressionConstant>(
        Material, SpecularNodeId, -260, 230);
    if (!WorldPosition || !Macro || !BaseColor || !Roughness ||
        !Metallic || !Specular)
    {
        OutError = TEXT("Could not author the exact six-node outer-ground material graph.");
        return nullptr;
    }

    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    Macro->Description = MacroNodeId;
    Macro->Code = MacroCode;
    Macro->OutputType = CMOT_Float4;
    Macro->Inputs.SetNum(1);
    Macro->Inputs[0].InputName = TEXT("WorldPosition");
    Macro->Inputs[0].Input.Connect(0, WorldPosition);
    BaseColor->R = true;
    BaseColor->G = true;
    BaseColor->B = true;
    BaseColor->A = false;
    BaseColor->Input.Connect(0, Macro);
    Roughness->R = false;
    Roughness->G = false;
    Roughness->B = false;
    Roughness->A = true;
    Roughness->Input.Connect(0, Macro);
    Metallic->R = 0.0f;
    Specular->R = 0.18f;

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bCastRayTracedShadows = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->PhysMaterial = nullptr;
    Material->PhysMaterialMask = nullptr;
    for (TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Material->PhysicalMaterialMap)
    {
        PhysicalMaterial = nullptr;
    }
    Material->RenderTracePhysicalMaterialOutputs.Reset();
    Material->NaniteOverrideMaterial.bEnableOverride = false;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Data->BaseColor.Connect(0, BaseColor);
    Data->Roughness.Connect(0, Roughness);
    Data->Metallic.Connect(0, Metallic);
    Data->Specular.Connect(0, Specular);
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
    const UMaterialExpression* Expected)
{
    return Expected && Input.Expression == Expected &&
        Input.OutputIndex == 0;
}

bool HasExplicitPhysicalMaterialBinding(const UMaterial* Material)
{
    if (!Material || Material->PhysMaterial || Material->PhysMaterialMask ||
        !Material->RenderTracePhysicalMaterialOutputs.IsEmpty())
    {
        return true;
    }
    for (const TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Material->PhysicalMaterialMap)
    {
        if (PhysicalMaterial)
        {
            return true;
        }
    }
    return false;
}

bool ValidateMaterial(UMaterial* Material, FString& OutError)
{
    const UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    const UMaterialExpressionCustom* Macro = nullptr;
    const UMaterialExpressionComponentMask* BaseColor = nullptr;
    const UMaterialExpressionComponentMask* Roughness = nullptr;
    const UMaterialExpressionConstant* Metallic = nullptr;
    const UMaterialExpressionConstant* Specular = nullptr;
    int32 WorldPositionCount = 0;
    int32 CustomCount = 0;
    int32 MaskCount = 0;
    int32 ConstantCount = 0;
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
                Macro = CustomCandidate;
            }
            else if (const auto* MaskCandidate =
                         Cast<UMaterialExpressionComponentMask>(Expression))
            {
                ++MaskCount;
                if (MaskCandidate->Desc == BaseColorNodeId)
                {
                    BaseColor = MaskCandidate;
                }
                if (MaskCandidate->Desc == RoughnessNodeId)
                {
                    Roughness = MaskCandidate;
                }
            }
            else if (const auto* ConstantCandidate =
                         Cast<UMaterialExpressionConstant>(Expression))
            {
                ++ConstantCount;
                if (ConstantCandidate->Desc == MetallicNodeId)
                {
                    Metallic = ConstantCandidate;
                }
                if (ConstantCandidate->Desc == SpecularNodeId)
                {
                    Specular = ConstantCandidate;
                }
            }
        }
    }
    const bool bMacroInputExact = Macro && Macro->Inputs.Num() == 1 &&
        Macro->Inputs[0].InputName == FName(TEXT("WorldPosition")) &&
        InputIs(Macro->Inputs[0].Input, WorldPosition);
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != MaterialObjectPath || !Data ||
        Data->ExpressionCollection.Expressions.Num() != 6 ||
        WorldPositionCount != 1 || CustomCount != 1 || MaskCount != 2 ||
        ConstantCount != 2 || !WorldPosition || !Macro || !BaseColor ||
        !Roughness || !Metallic || !Specular || !bMacroInputExact ||
        WorldPosition->Desc != WorldPositionNodeId ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        Macro->Desc != MacroNodeId || Macro->Description != MacroNodeId ||
        Macro->Code != MacroCode || Macro->OutputType != CMOT_Float4 ||
        BaseColor->Desc != BaseColorNodeId || !BaseColor->R ||
        !BaseColor->G || !BaseColor->B || BaseColor->A ||
        !InputIs(BaseColor->Input, Macro) ||
        Roughness->Desc != RoughnessNodeId || Roughness->R || Roughness->G ||
        Roughness->B || !Roughness->A ||
        !InputIs(Roughness->Input, Macro) ||
        Metallic->Desc != MetallicNodeId || !FMath::IsNearlyZero(Metallic->R) ||
        Specular->Desc != SpecularNodeId ||
        !FMath::IsNearlyEqual(Specular->R, 0.18f, 0.000001f) ||
        !InputIs(Data->BaseColor, BaseColor) ||
        !InputIs(Data->Roughness, Roughness) ||
        !InputIs(Data->Metallic, Metallic) ||
        !InputIs(Data->Specular, Specular) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections || Material->bCastRayTracedShadows ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        HasExplicitPhysicalMaterialBinding(Material) ||
        Data->Normal.Expression || Data->EmissiveColor.Expression ||
        Data->AmbientOcclusion.Expression || Data->Opacity.Expression ||
        Data->OpacityMask.Expression || Data->WorldPositionOffset.Expression ||
        Data->Displacement.Expression || Data->PixelDepthOffset.Expression ||
        Data->MaterialAttributes.Expression || Data->FrontMaterial.Expression)
    {
        OutError = TEXT("The V5D outer-ground material lost its exact six-node opaque, world-space macro-varied, no-texture/no-physical-material/no-WPO/PDO/displacement contract.");
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
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
    Options->StaticMeshImportData->bRemoveDegenerates = false;
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

bool ValidateImportSettings(
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
        OutError = TEXT("Outer-ground persisted import policy must remain identity-scale, centimetre/legacy-Y preconditioned, explicit-normal, no-LOD/no-collision/no-lightmap-generation and Nanite-enabled.");
        return false;
    }
    OutError.Reset();
    return true;
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
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync ||
        !ValidateImportSettings(Data, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact outer-ground OBJ import task changed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool NormalizeMesh(UStaticMesh* Mesh, UMaterial* Material, FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || !Material || Mesh->GetNumSourceModels() != 1 ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedSectionCount ||
        !Description ||
        Description->Vertices().Num() != ExpectedSourceCornerCount ||
        Description->VertexInstances().Num() != ExpectedSourceCornerCount ||
        Description->Triangles().Num() != ExpectedTriangleCount ||
        Description->Polygons().Num() != ExpectedTriangleCount ||
        Description->PolygonGroups().Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("Outer-ground normalization requires the exact one-LOD/one-slot/one-section source topology before full-fidelity raster fallback is rebuilt.");
        return false;
    }

    FStaticMaterial Imported = Mesh->GetStaticMaterials()[0];
    if (Imported.MaterialSlotName != FName(*MaterialName) ||
        Imported.ImportedMaterialSlotName != FName(*MaterialName))
    {
        OutError = TEXT("Outer-ground OBJ material slot is not exact.");
        return false;
    }
    Imported.MaterialInterface = Material;
    Mesh->Modify();
    const TArray<FStaticMaterial> ExactMaterials = {Imported};
    Mesh->SetStaticMaterials(ExactMaterials);
    FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
    SourceModel.BuildSettings.BuildScale3D = FVector::OneVector;
    SourceModel.BuildSettings.bGenerateLightmapUVs = false;
    SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
    SourceModel.BuildSettings.bRecomputeNormals = false;
    SourceModel.BuildSettings.bRecomputeTangents = true;
    SourceModel.BuildSettings.bUseMikkTSpace = true;
    SourceModel.BuildSettings.bRemoveDegenerates = false;
    Mesh->SetLightMapCoordinateIndex(0);
    Mesh->bGenerateMeshDistanceField = false;
    Mesh->NaniteSettings.bEnabled = true;
    Mesh->NaniteSettings.KeepPercentTriangles = 1.0f;
    Mesh->NaniteSettings.TrimRelativeError = 0.0f;
    Mesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, 0);
    FMeshSectionInfo Original = Mesh->GetOriginalSectionInfoMap().Get(0, 0);
    Section.MaterialIndex = 0;
    Section.bEnableCollision = false;
    Section.bCastShadow = false;
    Section.bAffectDistanceFieldLighting = false;
    Original.MaterialIndex = 0;
    Original.bEnableCollision = false;
    Original.bCastShadow = false;
    Original.bAffectDistanceFieldLighting = false;
    Mesh->GetSectionInfoMap().Set(0, 0, Section);
    Mesh->GetOriginalSectionInfoMap().Set(0, 0, Original);
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

int32 CountProvenance(const UStaticMesh* Mesh)
{
    const TArray<UAssetUserData*>* Data =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    int32 Count = 0;
    if (Data)
    {
        for (const UAssetUserData* Datum : *Data)
        {
            Count += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
                    StaticClass());
        }
    }
    return Count;
}

bool ValidateProvenance(const UStaticMesh* Mesh, FString& OutError)
{
    const TArray<UAssetUserData*>* Data =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    const UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance*
        Provenance = nullptr;
    if (Data)
    {
        for (const UAssetUserData* Datum : *Data)
        {
            if (Datum && Datum->IsA(
                    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
                        StaticClass()))
            {
                Provenance = Cast<
                    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance>(
                        Datum);
            }
        }
    }
    if (!Mesh || !Data || Data->Num() != 1 || CountProvenance(Mesh) != 1 ||
        !Provenance || Provenance->GetOuter() != Mesh ||
        Provenance->HasAnyFlags(RF_Transient) ||
        !Provenance->IsCanonicalContract())
    {
        OutError = TEXT("Outer-ground mesh must own exactly one canonical cooked negative-authority provenance object.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool StampProvenance(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh)
    {
        OutError = TEXT("Cannot stamp outer-ground provenance on a null mesh.");
        return false;
    }
    Mesh->Modify();
    while (CountProvenance(Mesh) > 0)
    {
        Mesh->RemoveUserDataOfClass(
            UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
                StaticClass());
    }
    auto* Provenance = NewObject<
        UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance>(
            Mesh, NAME_None, RF_Transactional);
    if (!Provenance)
    {
        OutError = TEXT("Could not allocate outer-ground cooked provenance.");
        return false;
    }
    Provenance->SetCanonicalContract();
    Mesh->AddAssetUserData(Provenance);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    return ValidateProvenance(Mesh, OutError);
}

bool ValidateUv0(
    const FStaticMeshLODResources& Lod,
    FString& OutError)
{
    const FStaticMeshVertexBuffer& Buffer =
        Lod.VertexBuffers.StaticMeshVertexBuffer;
    if (Buffer.GetNumVertices() != ExpectedRenderVertexCount ||
        Buffer.GetNumTexCoords() != 1)
    {
        OutError = FString::Printf(
            TEXT("Outer-ground render LOD welded topology drifted: vertices=%u expected=%d uvChannels=%u expectedUvChannels=1."),
            Buffer.GetNumVertices(),
            ExpectedRenderVertexCount,
            Buffer.GetNumTexCoords());
        return false;
    }
    FVector2f Minimum(FLT_MAX, FLT_MAX);
    FVector2f Maximum(-FLT_MAX, -FLT_MAX);
    for (uint32 Index = 0; Index < Buffer.GetNumVertices(); ++Index)
    {
        const FVector2f Uv = Buffer.GetVertexUV(Index, 0);
        if (!FMath::IsFinite(Uv.X) || !FMath::IsFinite(Uv.Y))
        {
            OutError = TEXT("Outer-ground UV0 contains a non-finite value.");
            return false;
        }
        Minimum.X = FMath::Min(Minimum.X, Uv.X);
        Minimum.Y = FMath::Min(Minimum.Y, Uv.Y);
        Maximum.X = FMath::Max(Maximum.X, Uv.X);
        Maximum.Y = FMath::Max(Maximum.Y, Uv.Y);
    }
    if (!Minimum.Equals(ExpectedUvMin, 0.002f) ||
        !Maximum.Equals(ExpectedUvMax, 0.002f))
    {
        OutError = FString::Printf(
            TEXT("Outer-ground physical-metre UV0 bounds drifted: min=(%.6f,%.6f) max=(%.6f,%.6f)."),
            Minimum.X, Minimum.Y, Maximum.X, Maximum.Y);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateDescription(
    const FMeshDescription& Description,
    FString& OutError)
{
    if (Description.Vertices().Num() != ExpectedSourceCornerCount ||
        Description.VertexInstances().Num() != ExpectedSourceCornerCount ||
        Description.Triangles().Num() != ExpectedTriangleCount ||
        Description.Polygons().Num() != ExpectedTriangleCount ||
        Description.PolygonGroups().Num() != ExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("Outer-ground mesh-description topology drifted: vertices=%d vertexInstances=%d triangles=%d polygons=%d polygonGroups=%d."),
            Description.Vertices().Num(),
            Description.VertexInstances().Num(),
            Description.Triangles().Num(),
            Description.Polygons().Num(),
            Description.PolygonGroups().Num());
        return false;
    }
    const FStaticMeshConstAttributes Attributes(Description);
    const auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    FName OnlySlot = NAME_None;
    for (const FPolygonGroupID Group :
         Description.PolygonGroups().GetElementIDs())
    {
        OnlySlot = Slots[Group];
    }
    if (OnlySlot != FName(*MaterialName))
    {
        OutError = TEXT("Outer-ground mesh-description material group drifted.");
        return false;
    }
    const auto Normals = Attributes.GetVertexInstanceNormals();
    for (const FVertexInstanceID Id :
         Description.VertexInstances().GetElementIDs())
    {
        const FVector3f Normal = Normals[Id];
        if (Normal.ContainsNaN() ||
            !FMath::IsNearlyEqual(Normal.SizeSquared(), 1.0f, 0.002f))
        {
            OutError = TEXT("An outer-ground explicit normal is non-finite or non-unit.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMesh(UStaticMesh* Mesh, UMaterial* Material, FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData =
        Mesh ? Mesh->GetAssetImportData() : nullptr;
    const auto* StaticImportData =
        Cast<UFbxStaticMeshImportData>(ImportData);
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
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    FString ImportError;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != MeshObjectPath || Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        !ValidateImportSettings(StaticImportData, ImportError) ||
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
        Mesh->bGenerateMeshDistanceField ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() !=
            ExpectedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() != ExpectedSectionCount ||
        !Mesh->NaniteSettings.bEnabled ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError) ||
        !Mesh->HasValidNaniteData())
    {
        OutError = ImportError.IsEmpty()
            ? TEXT("Outer-ground mesh lost its exact source, identity transform, one-LOD topology, full Nanite/raster settings, or distance-field exclusion.")
            : ImportError;
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (!Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = TEXT("Outer-ground mesh must have zero collision and no navigation data.");
        return false;
    }
    const FStaticMaterial& StaticMaterial = Mesh->GetStaticMaterials()[0];
    const FStaticMeshSection& RenderSection =
        RenderData->LODResources[0].Sections[0];
    const FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, 0);
    const FMeshSectionInfo Original =
        Mesh->GetOriginalSectionInfoMap().Get(0, 0);
    if (StaticMaterial.MaterialSlotName != FName(*MaterialName) ||
        StaticMaterial.ImportedMaterialSlotName != FName(*MaterialName) ||
        StaticMaterial.MaterialInterface != Material ||
        RenderSection.MaterialIndex != 0 ||
        RenderSection.NumTriangles != ExpectedTriangleCount ||
        Section.MaterialIndex != 0 || Original.MaterialIndex != 0 ||
        Section.bEnableCollision || Original.bEnableCollision ||
        Section.bCastShadow || Original.bCastShadow ||
        Section.bAffectDistanceFieldLighting ||
        Original.bAffectDistanceFieldLighting)
    {
        OutError = TEXT("Outer-ground exact material/section binding or no-collision/no-shadow/no-distance-field section policy drifted.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(ExpectedBoundsMin, 0.25) ||
        !BoundsMax.Equals(ExpectedBoundsMax, 0.25))
    {
        OutError = FString::Printf(
            TEXT("Outer-ground centimetre bounds drifted: min=(%.6f,%.6f,%.6f) max=(%.6f,%.6f,%.6f)."),
            BoundsMin.X, BoundsMin.Y, BoundsMin.Z,
            BoundsMax.X, BoundsMax.Y, BoundsMax.Z);
        return false;
    }
    if (!ValidateDescription(*Description, OutError) ||
        !ValidateUv0(RenderData->LODResources[0], OutError) ||
        !ValidateProvenance(Mesh, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateAllSourceHashes(OutReport) ||
        !ValidateRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UMaterial* Material = LoadExact<UMaterial>(MaterialObjectPath);
    UStaticMesh* Mesh = LoadExact<UStaticMesh>(MeshObjectPath);
    if (!ValidateMaterial(Material, OutReport) ||
        !ValidateMesh(Mesh, Material, OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_ASSETS_VALID assets=%d mesh=%s material=%s sourceFiles=7 objBytes=492909 objSha256=%s lockedSetSha256=06F77A61280C65DA1BB3539CEA404E70328FF303893CFBD8270D837138FE3C05 sourceCorners=%d importedVertices=%d importedVertexInstances=%d renderVertices=%d triangles=%d angularSectors=128 radialBands=5 boundsMinCm=(-125000,-125000,-51.51655292) boundsMaxCm=(125000,125000,201.68388265) materialSlot=%s materialGraph=exactSixNodeOpaqueWorldSpaceMacroNeutral noTextures=true noPhysicalMaterial=true physicalMetreUv0=true identityImportScale=true oneLod=true naniteFullMesh=true rasterFallbackFullMesh=true zeroCollision=true noNavigationData=true castShadow=false distanceField=false renderOnlyProviderLoadingVisual=true noActorOrMapChange=true noTerrainDemDtmSurveyAsBuiltRouteAccessOperationalSecuritySensorOcclusionRfGeometryOrRfMaterialAuthority=true liveMapOrProviderPolicyIntegrationIncluded=false cookedProvenanceObjects=1."),
        ExpectedAssetCount,
        *MeshObjectPath,
        *MaterialObjectPath,
        SourceSpecs[0].Sha256,
        ExpectedSourceCornerCount,
        ExpectedSourceCornerCount,
        ExpectedSourceCornerCount,
        ExpectedRenderVertexCount,
        ExpectedTriangleCount,
        *MaterialName);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory
{
const FString& GetAssetRootPath()
{
    return AssetRoot;
}

const FString& GetMeshObjectPath()
{
    return MeshObjectPath;
}

const FString& GetMaterialObjectPath()
{
    return MaterialObjectPath;
}

bool CreateFreshAssets(
    UStaticMesh*& OutMesh,
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMesh = nullptr;
    OutMaterial = nullptr;
    FString ExistingReport;
    if (ValidateAssets(ExistingReport))
    {
        OutMesh = LoadExact<UStaticMesh>(MeshObjectPath);
        OutMaterial = LoadExact<UMaterial>(MaterialObjectPath);
        OutError = ExistingReport;
        return OutMesh && OutMaterial;
    }
    if (!ValidateAllSourceHashes(OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty())
    {
        OutError = TEXT("Outer-ground import refuses an invalid or non-empty isolated asset root.");
        return false;
    }

    FScopedFreshRollback Rollback(OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    OutMaterial = CreateMaterial(AssetTools, OutError);
    if (!OutMaterial)
    {
        return false;
    }
    UAssetImportTask* Task = MakeImportTask();
    if (!Task || !ValidateImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate the exact outer-ground OBJ import task.");
        }
        OutMaterial = nullptr;
        return false;
    }
    AssetTools.ImportAssetTasks({Task});
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == MeshObjectPath)
        {
            OutMesh = Cast<UStaticMesh>(Object);
        }
    }
    if (!OutMesh)
    {
        OutMesh = LoadExact<UStaticMesh>(MeshObjectPath);
    }
    if (!OutMesh || !NormalizeMesh(OutMesh, OutMaterial, OutError) ||
        !StampProvenance(OutMesh, OutError) ||
        !ValidateMaterial(OutMaterial, OutError) ||
        !ValidateMesh(OutMesh, OutMaterial, OutError))
    {
        OutMesh = nullptr;
        OutMaterial = nullptr;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString PreSaveReport;
    if (!ValidateInternal(false, PreSaveReport))
    {
        OutError = TEXT("Fresh outer-ground validation failed before save: ") +
            PreSaveReport;
        OutMesh = nullptr;
        OutMaterial = nullptr;
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
} // namespace TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory
