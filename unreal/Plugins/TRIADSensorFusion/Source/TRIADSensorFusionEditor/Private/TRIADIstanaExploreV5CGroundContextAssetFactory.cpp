#include "TRIADIstanaExploreV5CGroundContextAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "Ssl.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MeshName(TEXT("SM_IPV5C_OfficialPlanningGroundContext"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);
const FString ParentMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/"
         "MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone"));

constexpr int32 ExpectedAssetCount = 3;
constexpr int32 ExpectedSourceTriangleCount = 30408;
constexpr int32 ExpectedImportedTriangleCount = 30403;
constexpr int32 ExpectedMaterialCount = 2;
constexpr int64 ExpectedObjBytes = 11861751;
constexpr int64 ExpectedMtlBytes = 473;
constexpr int64 ExpectedFeaturesBytes = 181449;
constexpr int64 ExpectedManifestBytes = 13093;
constexpr int64 ExpectedContractBytes = 8514;
const FString ExpectedObjSha256(
    TEXT("49334DD702E12E82EEAD1C70C4C8BB5984D6F2C375262CF368102E363677D12C"));
const FString ExpectedMtlSha256(
    TEXT("9F1ED025A49BB0DBBF28BDAE9E12010B4600842BF847FDC1552D1F1351BF467B"));
const FString ExpectedFeaturesSha256(
    TEXT("D79B7D7B602AEA593DE65E8C5368CFC04967AC02C024ABBF375D95724C1153A9"));
const FString ExpectedManifestSha256(
    TEXT("FC1F00F91B6CCAD820A3D3B2D354AAD691E08FE4F9BD70F7AA28E45EFBAF501F"));
const FString ExpectedContractSha256(
    TEXT("7E642EC331F013CECB5251012592CA5509814B9F71E2CD989134719B8981E06E"));

struct FMaterialSpec
{
    const TCHAR* Name;
    int32 SourceTriangles;
    int32 ImportedTriangles;
    FLinearColor LookdevTint;
    float TileMeters;
    float DetailTileMeters;
    float MacroTileMeters;
    float NormalStrength;
    float DetailNormalStrength;
    float RoughnessBias;
    float MacroAlbedoStrength;
    float MacroRoughnessStrength;
};

const FMaterialSpec MaterialSpecs[] = {
    {
        TEXT("MI_IPV5C_OfficialPlanningRoadZone"),
        23338,
        23336,
        FLinearColor(0.12f, 0.13f, 0.14f, 1.0f),
        5.0f,
        0.22f,
        36.0f,
        0.22f,
        0.20f,
        0.16f,
        0.055f,
        0.045f,
    },
    {
        TEXT("MI_IPV5C_OfficialPlanningRoadGraphic"),
        7070,
        7067,
        FLinearColor(0.22f, 0.23f, 0.24f, 1.0f),
        5.0f,
        0.22f,
        36.0f,
        0.10f,
        0.10f,
        0.12f,
        0.055f,
        0.045f,
    },
};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == ExpectedMaterialCount);

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
        "IstanaPublicViewExploreV5C/GroundContext/Generated/"
        "SM_IPV5C_OfficialPlanningGroundContext.obj"));
}

FString MtlSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/GroundContext/Generated/"
        "SM_IPV5C_OfficialPlanningGroundContext.mtl"));
}

FString FeaturesSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/GroundContext/Generated/"
        "IstanaPublicViewV5CGroundContext.features.json"));
}

FString ManifestSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/GroundContext/Generated/"
        "IstanaPublicViewV5CGroundContext.manifest.json"));
}

FString ContractSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/GroundContext/"
        "istana_public_view_v5c_ground_context.contract.json"));
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
            TEXT("V5C ground-context source byte guard failed for '%s': expected=%lld actual=%d."),
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
        OutError = TEXT("V5C ground-context source SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5C ground-context SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5C ground-context source SHA-256 guard failed for '%s': expected=%s actual=%s."),
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
               FeaturesSourcePath(), ExpectedFeaturesBytes,
               ExpectedFeaturesSha256, OutError) &&
        ValidateSourceHash(
               ManifestSourcePath(), ExpectedManifestBytes,
               ExpectedManifestSha256, OutError) &&
        ValidateSourceHash(
               ContractSourcePath(), ExpectedContractBytes,
               ExpectedContractSha256, OutError);
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
        OutError = TEXT("V5C ground-context Asset Registry discovery did not complete.");
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
            TEXT("V5C GroundContext root must contain exactly three assets; actual=[%s]."),
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
                OutError = TEXT("A V5C ground-context asset is not persisted: ") +
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
            Error += TEXT(" V5C_GROUND_CONTEXT_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
    }

    void Commit() { bCommitted = true; }

private:
    TArray<UObject*>& Assets;
    FString& Error;
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

void GatherSpecScalars(
    const FMaterialSpec& Spec,
    TArray<TPair<FName, float>>& OutValues)
{
    OutValues = {
        {TEXT("TileMeters"), Spec.TileMeters},
        {TEXT("DetailTileMeters"), Spec.DetailTileMeters},
        {TEXT("MacroTileMeters"), Spec.MacroTileMeters},
        {TEXT("NormalStrength"), Spec.NormalStrength},
        {TEXT("DetailNormalStrength"), Spec.DetailNormalStrength},
        {TEXT("RoughnessBias"), Spec.RoughnessBias},
        {TEXT("MacroAlbedoStrength"), Spec.MacroAlbedoStrength},
        {TEXT("MacroRoughnessStrength"), Spec.MacroRoughnessStrength},
        {TEXT("BumpOffsetStrength"), 0.0f},
        {TEXT("ExposedMetalMaskStrength"), 0.0f},
    };
}

void GatherSpecVectors(
    const FMaterialSpec& Spec,
    TArray<TPair<FName, FLinearColor>>& OutValues)
{
    OutValues = {{TEXT("LookdevTint"), Spec.LookdevTint}};
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

bool ValidateParentMaterial(
    UMaterialInstanceConstant* Parent,
    FString& OutError)
{
    UMaterial* Base = Parent ? Parent->GetMaterial() : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Base ? Base->GetEditorOnlyData() : nullptr;
    if (!Parent || Parent->GetPathName() != ParentMaterialObjectPath ||
        !Base || !EditorOnly || Base->MaterialDomain != MD_Surface ||
        Base->BlendMode != BLEND_Opaque || Base->TwoSided ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->Displacement.Expression ||
        !FMath::IsNearlyZero(Base->MaxWorldPositionOffsetDisplacement))
    {
        OutError = TEXT("The exact opaque, one-sided, displacement-free MI_IPV5_HardscapeStone parent is absent or changed.");
        return false;
    }
    TArray<TPair<FName, float>> Scalars;
    TArray<TPair<FName, FLinearColor>> Vectors;
    GatherSpecScalars(MaterialSpecs[0], Scalars);
    GatherSpecVectors(MaterialSpecs[0], Vectors);
    for (const TPair<FName, float>& Pair : Scalars)
    {
        float Value = 0.0f;
        if (!Parent->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Value, true))
        {
            OutError = TEXT("MI_IPV5_HardscapeStone lacks required scalar parameter '") +
                Pair.Key.ToString() + TEXT("'.");
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : Vectors)
    {
        FLinearColor Value;
        if (!Parent->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Value, true))
        {
            OutError = TEXT("MI_IPV5_HardscapeStone lacks required vector parameter '") +
                Pair.Key.ToString() + TEXT("'.");
            return false;
        }
    }
    const FName ForbiddenSiteSpecificStrengths[] = {
        TEXT("WeatheringStrength"),
        TEXT("SillDirtStrength"),
        TEXT("CorniceRunoffStrength"),
        TEXT("GroundContactDampStrength"),
        TEXT("CavityDirtStrength"),
    };
    for (const FName& Name : ForbiddenSiteSpecificStrengths)
    {
        float Value = 1.0f;
        if (!Parent->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Name), Value, true) ||
            !FMath::IsNearlyZero(Value, 0.000001f))
        {
            OutError = TEXT("MI_IPV5_HardscapeStone enables forbidden site-specific response '") +
                Name.ToString() + TEXT("'.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

UMaterialInstanceConstant* CreateMaterialInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
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
              Spec.Name,
              MaterialRoot,
              UMaterialInstanceConstant::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5CGroundContextAssets"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create exact V5C ground-context material '%s'."),
            Spec.Name);
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Instance->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    TArray<TPair<FName, float>> Scalars;
    TArray<TPair<FName, FLinearColor>> Vectors;
    GatherSpecScalars(Spec, Scalars);
    GatherSpecVectors(Spec, Vectors);
    for (const TPair<FName, float>& Pair : Scalars)
    {
        Instance->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    for (const TPair<FName, FLinearColor>& Pair : Vectors)
    {
        Instance->SetVectorParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    OutError.Reset();
    return Instance;
}

bool ValidateMaterialInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
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
        Instance->GetPathName() != ObjectPath(MaterialRoot, Spec.Name) ||
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
        OutError = FString::Printf(
            TEXT("V5C ground-context child material drifted: %s baseOverrides=[%s]."),
            Spec.Name,
            *BaseOverrides);
        return false;
    }
    TArray<TPair<FName, float>> ExpectedScalars;
    TArray<TPair<FName, FLinearColor>> ExpectedVectors;
    GatherSpecScalars(Spec, ExpectedScalars);
    GatherSpecVectors(Spec, ExpectedVectors);
    TSet<FName> ExpectedScalarNames;
    TSet<FName> ActualScalarNames;
    TSet<FName> ExpectedVectorNames;
    TSet<FName> ActualVectorNames;
    for (const TPair<FName, float>& Pair : ExpectedScalars)
    {
        ExpectedScalarNames.Add(Pair.Key);
    }
    for (const TPair<FName, FLinearColor>& Pair : ExpectedVectors)
    {
        ExpectedVectorNames.Add(Pair.Key);
    }
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualScalarNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("V5C ground-context child has an invalid or duplicate scalar override.");
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("V5C ground-context child has an invalid or duplicate vector override.");
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameNames(ExpectedScalarNames, ActualScalarNames) ||
        !SameNames(ExpectedVectorNames, ActualVectorNames))
    {
        OutError = FString::Printf(
            TEXT("V5C ground-context material '%s' override roster changed."),
            Spec.Name);
        return false;
    }
    for (const TPair<FName, float>& Pair : ExpectedScalars)
    {
        float Actual = 0.0f;
        if (!Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !FMath::IsNearlyEqual(Actual, Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V5C ground-context material '%s' scalar '%s' changed."),
                Spec.Name,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : ExpectedVectors)
    {
        FLinearColor Actual;
        if (!Instance->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !Actual.Equals(Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V5C ground-context material '%s' vector '%s' changed."),
                Spec.Name,
                *Pair.Key.ToString());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledInstance(
    UMaterialInstanceConstant* Instance,
    FString& OutError)
{
    if (Instance)
    {
        Instance->EnsureIsComplete();
    }
    FMaterialResource* Resource = Instance
        ? Instance->GetMaterialResource(ERHIFeatureLevel::SM5)
        : nullptr;
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Instance || !Resource || Instance->IsCompiling() ||
        !Instance->IsComplete() || !Resource->IsCompilationFinished() ||
        !Resource->GetGameThreadShaderMap() ||
        !Resource->IsGameThreadShaderMapComplete() || !Errors.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("V5C ground-context material '%s' has no complete SM5 shader map: [%s]."),
            Instance ? *Instance->GetPathName() : TEXT("<null>"),
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
    Options->StaticMeshImportData->bBuildNanite = true;
    // The hash-pinned official overlay contains no zero-area triangles, but a
    // few very narrow display-stroke triangles fall below FBX's generic edge
    // tolerance. Preserve the source census instead of silently deleting
    // legitimate planning-graphic coverage at import time.
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
        !Data->bBuildNanite || Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The exact V5C ground-context identity OBJ import policy changed.");
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
    if (Mesh->GetNumSourceModels() == 1)
    {
        Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs = true;
    }
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
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
}

bool NormalizeAndBindMaterials(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Materials.Num() != ExpectedMaterialCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("V5C ground-context normalization requires the exact two-slot/two-section imported mesh.");
        return false;
    }
    TMap<FName, int32> CanonicalOrder;
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        const FName Name(MaterialSpecs[Index].Name);
        if (!Materials[Index] || CanonicalOrder.Contains(Name))
        {
            OutError = TEXT("V5C ground-context canonical material roster is incomplete or ambiguous.");
            return false;
        }
        CanonicalOrder.Add(Name, Index);
    }
    const TArray<FStaticMaterial> ImportedMaterials = Mesh->GetStaticMaterials();
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
            SeenCanonical[*CanonicalIndex])
        {
            OutError = FString::Printf(
                TEXT("V5C ground-context OBJ material slots are not an exact unique canonical permutation at imported index %d: slot='%s' imported='%s'."),
                ImportedIndex,
                *Imported.MaterialSlotName.ToString(),
                *Imported.ImportedMaterialSlotName.ToString());
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

    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    TSet<int32> SeenSectionMaterials;
    TArray<FString> Digest;
    bool bSemanticSectionCensusExact = true;
    for (int32 SectionIndex = 0;
         SectionIndex < Lod.Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection = Lod.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        const FString ImportedName = ImportedMaterials.IsValidIndex(ImportedIndex)
            ? ImportedMaterials[ImportedIndex].MaterialSlotName.ToString()
            : TEXT("INVALID");
        Digest.Add(FString::Printf(
            TEXT("%d:%d:%s:%u"),
            SectionIndex,
            ImportedIndex,
            *ImportedName,
            RenderSection.NumTriangles));
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToCanonical.IsValidIndex(ImportedIndex) ||
            ImportedToCanonical[ImportedIndex] == INDEX_NONE ||
            Section.MaterialIndex != ImportedIndex ||
            Original.MaterialIndex != ImportedIndex)
        {
            OutError = TEXT("A V5C ground-context section references an invalid imported material index.");
            return false;
        }
        const int32 CanonicalIndex = ImportedToCanonical[ImportedIndex];
        if (SeenSectionMaterials.Contains(CanonicalIndex) ||
            RenderSection.NumTriangles !=
                MaterialSpecs[CanonicalIndex].ImportedTriangles)
        {
            bSemanticSectionCensusExact = false;
        }
        SeenSectionMaterials.Add(CanonicalIndex);
        Section.MaterialIndex = CanonicalIndex;
        Original.MaterialIndex = CanonicalIndex;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    if (!bSemanticSectionCensusExact ||
        SeenSectionMaterials.Num() != ExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5C ground-context semantic section census changed: [%s]."),
            *FString::Join(Digest, TEXT(",")));
        return false;
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
    FString& OutError)
{
    const FStaticMeshVertexBuffer& Buffer =
        Lod.VertexBuffers.StaticMeshVertexBuffer;
    if (Buffer.GetNumVertices() == 0 || Buffer.GetNumTexCoords() < 1)
    {
        OutError = TEXT("V5C ground-context mesh has no imported UV0.");
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
            OutError = TEXT("V5C ground-context UV0 contains a non-finite value.");
            return false;
        }
        Minimum.X = FMath::Min(Minimum.X, Uv.X);
        Minimum.Y = FMath::Min(Minimum.Y, Uv.Y);
        Maximum.X = FMath::Max(Maximum.X, Uv.X);
        Maximum.Y = FMath::Max(Maximum.Y, Uv.Y);
    }
    if (!FMath::IsNearlyEqual(Minimum.X, -998.385847f, 0.001f) ||
        !FMath::IsNearlyEqual(Minimum.Y, -999.576768f, 0.001f) ||
        !FMath::IsNearlyEqual(Maximum.X, 998.929611f, 0.001f) ||
        !FMath::IsNearlyEqual(Maximum.Y, 999.663390f, 0.001f))
    {
        OutError = FString::Printf(
            TEXT("V5C ground-context continuous source-metre UV0 bounds drifted: min=(%.6f,%.6f) max=(%.6f,%.6f)."),
            Minimum.X,
            Minimum.Y,
            Maximum.X,
            Maximum.Y);
        return false;
    }
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
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        TriangleCount(Mesh) != ExpectedImportedTriangleCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount ||
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
        OutError = TEXT("V5C ground-context mesh lost exact source, identity scale, full-precision UV0, stable 30,403 imported-triangle, one-LOD, two-slot, or full-fidelity Nanite/fallback state.");
        return false;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("V5C ground-context mesh must have no simple or complex-as-simple collision.");
        return false;
    }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-99838.5847, -99957.6768, -128.003865), 0.25) ||
        !BoundsMax.Equals(
            FVector(99892.9611, 99966.3390, 191.882529), 0.25))
    {
        OutError = FString::Printf(
            TEXT("V5C ground-context imported centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }
    if (!ValidateUv0(RenderData->LODResources[0], OutError))
    {
        return false;
    }

    TSet<int32> SeenSections;
    TArray<FString> SectionDigest;
    bool bSectionCensusExact = true;
    const auto& Sections = RenderData->LODResources[0].Sections;
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
                MaterialSpecs[Section.MaterialIndex].ImportedTriangles)
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
            TEXT("V5C ground-context section:material:triangle census drifted: [%s]."),
            *FString::Join(SectionDigest, TEXT(",")));
        return false;
    }
    for (int32 Slot = 0; Slot < ExpectedMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[Slot];
        if (StaticMaterial.MaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            (bRequireMaterials &&
             (!Mesh->GetMaterial(Slot) ||
              Mesh->GetMaterial(Slot)->GetPathName() !=
                  OrderedMaterialPaths()[Slot])))
        {
            OutError = FString::Printf(
                TEXT("V5C ground-context material slot/order/binding drifted at %d."),
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
    UMaterialInstanceConstant* Parent =
        LoadExact<UMaterialInstanceConstant>(ParentMaterialObjectPath);
    if (!ValidateParentMaterial(Parent, OutReport))
    {
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(
                ObjectPath(MaterialRoot, Spec.Name));
        if (!ValidateMaterialInstance(Instance, Spec, OutReport) ||
            !ValidateCompiledInstance(Instance, OutReport))
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
        TEXT("ISTANA_EXPLORE_V5C_GROUND_CONTEXT_ASSETS_VALID assets=3 meshTriangles=%d sourceTriangles=%d roadZoneTriangles=%d sourceRoadZoneTriangles=%d roadGraphicTriangles=%d sourceRoadGraphicTriangles=%d importerCanonicalizedNarrowFaces=5 selectedRoadFeatures=199 selectedAbovegroundGraphics=53 excludedUndergroundGraphics=31 materialSlots=2 radiusMeters=299.909656..1000.000000 objBytes=%lld objSha256=%s manifestSha256=%s contractSha256=%s fullPrecisionProjection=true terrainRings=12..40 substrateTriangles=7168 fullFidelityNanite=true inheritedPbrNoNewTextures=true renderOnly=true noPhysicalRoadWidthMaterialElevationClaim=true noCollisionNavigationSensorOrRfAuthority=true."),
        ExpectedImportedTriangleCount,
        ExpectedSourceTriangleCount,
        MaterialSpecs[0].ImportedTriangles,
        MaterialSpecs[0].SourceTriangles,
        MaterialSpecs[1].ImportedTriangles,
        MaterialSpecs[1].SourceTriangles,
        ExpectedObjBytes,
        *ExpectedObjSha256,
        *ExpectedManifestSha256,
        *ExpectedContractSha256);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5CGroundContextAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }

const FString& GetGroundContextMeshObjectPath() { return MeshObjectPath; }

const TArray<FString>& GetOrderedMaterialObjectPaths()
{
    return OrderedMaterialPaths();
}

int32 GetExpectedImportedTriangleCount()
{
    return ExpectedImportedTriangleCount;
}

int32 GetExpectedMaterialCount() { return ExpectedMaterialCount; }

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
        OutError = TEXT("CreateFreshAssets requires an empty exact V5C GroundContext asset root.");
        return false;
    }
    UMaterialInstanceConstant* Parent =
        LoadExact<UMaterialInstanceConstant>(ParentMaterialObjectPath);
    if (!ValidateParentMaterial(Parent, OutError))
    {
        return false;
    }

    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    TArray<UMaterialInstanceConstant*> Materials;
    Materials.Reserve(ExpectedMaterialCount);
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Material =
            CreateMaterialInstance(AssetTools, Spec, Parent, OutError);
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
            OutError = TEXT("Could not allocate the exact V5C ground-context OBJ import task.");
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
        OutError = TEXT("Fresh V5C ground-context validation failed before save: ") +
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
} // namespace TRIADIstanaExploreV5CGroundContextAssetFactory
