#include "TRIADIstanaExploreV5DR29VegetationAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Ssl.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29"));
const FString MeshRoot(AssetRoot + TEXT("/Meshes"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

struct FMaterialSpec
{
    const TCHAR* Name;
    const TCHAR* SourceObjectPath;
    const TCHAR* SourceColorDescription;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("M_IPV5D_R29_Turf_Manicured"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_Manicured.M_IPV5D_LandmarkTurf_R28_Manicured"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_MANICURED_HEALTHY_TROPICAL")},
    {TEXT("M_IPV5D_R29_Turf_Humid"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_Humid.M_IPV5D_LandmarkTurf_R28_Humid"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_HUMID_HEALTHY_TROPICAL")},
    {TEXT("M_IPV5D_R29_Turf_Shade"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_Shade.M_IPV5D_LandmarkTurf_R28_Shade"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_SHADE_HEALTHY_TROPICAL")},
    {TEXT("M_IPV5D_R29_Turf_DryEdge"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_DryEdge.M_IPV5D_LandmarkTurf_R28_DryEdge"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_DRY_EDGE_HEALTHY_TROPICAL")}};

const FString SourceStableDescription(
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_SINGLE_GATE_VISIBILITY_65M_90M"));
const FString SourceRoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_PHOTOGRAPHIC_DEPTH_ROUGHNESS"));
const FString SourceNormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_12M_24M_DEPTH_NORMAL_ALPHA"));
const FString R29StableDescription(
    TEXT("TRIAD_EXPLORE_V5D_R29_MODELED_BLADE_STABLE_VISIBILITY_65M_90M"));
const FString R29RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_R29_18M_65M_MULTI_SCALE_ROUGHNESS"));
const FString R29NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_R29_18M_60M_MODELED_BLADE_NORMAL_ALPHA"));

const FString StableCode(
    TEXT("float componentVisibility=saturate(InstanceFade);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float calibratedVisibility=1.0-smoothstep(6500.0,9000.0,distanceCm);\n")
    TEXT("float effectiveVisibility=max(componentVisibility,calibratedVisibility);\n")
    TEXT("float stableGate=step(saturate(Random01),effectiveVisibility);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("float calibrationGate=saturate(CalibrationRevision);\n")
    TEXT("return stableGate*revisionGate*calibrationGate;"));

const FString RoughnessCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1800.0,6500.0,distanceCm);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float blade=frac(saturate(BladeUV.x)*31.416+saturate(Random01)*0.7548777);\n")
    TEXT("float2 cell=floor(WorldPosition.xy/240.0);\n")
    TEXT("float macro=frac(dot(cell,float2(0.1031,0.11369))+saturate(Random01)*0.618033989);\n")
    TEXT("macro=frac(macro*(macro+33.33)*(macro+macro+17.17));\n")
    TEXT("float rootMask=1.0-smoothstep(0.05,0.32,h);\n")
    TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
    TEXT("float nearRough=BaseRoughness+lerp(-0.030,0.034,blade)+lerp(-0.018,0.020,macro)+0.032*rootMask+0.016*cutMask;\n")
    TEXT("float farRough=0.82+lerp(-0.012,0.014,macro);\n")
    TEXT("return clamp(lerp(farRough,nearRough,detail),0.66,0.90);"));

const FString NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1800.0,6000.0,distanceCm);\n")
    TEXT("float blade=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float bladeWeight=lerp(0.92,1.10,blade);\n")
    TEXT("float heightWeight=lerp(0.72,1.0,smoothstep(0.06,0.66,saturate(BladeUV.y)));\n")
    TEXT("float retainedFarResponse=0.10;\n")
    TEXT("return saturate((retainedFarResponse+0.34*detail)*bladeWeight*heightWeight*saturate(InstanceFade));"));

struct FSourceSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceSpec MaterialLibrarySource = {
    TEXT("Generated/IstanaPublicViewV5DR29TropicalVegetation.mtl"),
    211,
    TEXT("AD38CAA9165558EA9051DF18E762885C62E9E7D65AD77086A5B72D5E774C0C34")};

const FSourceSpec MeshSources[] = {
    {TEXT("Generated/SM_IPV5D_R29_GrassFineCluster_Render.obj"),
     152684,
     TEXT("F2EE9137C885D9EFDB50B8E4542A7EF40979FA59A549F5F15B14F7DB47A8F527")},
    {TEXT("Generated/SM_IPV5D_R29_GrassBroadCluster_Render.obj"),
     133033,
     TEXT("D90E8436E1F9607AFDCF484C70AA944E12612979730853B5F1F61736061343D3")},
    {TEXT("Generated/SM_IPV5D_R29_GrassMixedCluster_Render.obj"),
     172938,
     TEXT("BBA5D2912A7AFAEE0DB710A977094FB821786507DFC42AC56BB7C984B7399FB9")}};

struct FMeshSpec
{
    const TCHAR* Name;
    const FSourceSpec* Source;
    int32 Triangles;
    int32 SourceCorners;
    double MinimumHeightCm;
    double MaximumHeightCm;
};

const FMeshSpec MeshSpecs[] = {
    {TEXT("SM_IPV5D_R29_GrassFineCluster_Render"),
     &MeshSources[0], 1024, 3072, 13.8, 13.9},
    {TEXT("SM_IPV5D_R29_GrassBroadCluster_Render"),
     &MeshSources[1], 896, 2688, 18.4, 18.5},
    {TEXT("SM_IPV5D_R29_GrassMixedCluster_Render"),
     &MeshSources[2], 1152, 3456, 19.6, 19.7}};

static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 4);
static_assert(UE_ARRAY_COUNT(MeshSpecs) == 3);

FString SourceRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
             "R29TropicalDetail")));
}

FString SourcePath(const TCHAR* RelativePath)
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(SourceRoot(), RelativePath));
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& Path)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *Path);
    return IsValid(Object) && Object->GetPathName() == Path ? Object : nullptr;
}

FString MaterialObjectPath(const FMaterialSpec& Spec)
{
    return ObjectPath(MaterialRoot, Spec.Name);
}

FString MeshObjectPath(const FMeshSpec& Spec)
{
    return ObjectPath(MeshRoot, Spec.Name);
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

bool ValidateSource(const FSourceSpec& Spec, FString& OutError)
{
    const FString Filename = SourcePath(Spec.RelativePath);
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != Spec.Bytes)
    {
        OutError = FString::Printf(
            TEXT("R29 vegetation source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            Spec.Bytes,
            Bytes.Num());
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH) != Spec.Sha256)
    {
        OutError = TEXT("R29 vegetation source SHA-256 guard failed for '") +
            Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R29 vegetation source validation requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

bool ValidateSources(FString& OutError)
{
    if (!ValidateSource(MaterialLibrarySource, OutError))
    {
        return false;
    }
    for (const FSourceSpec& Spec : MeshSources)
    {
        if (!ValidateSource(Spec, OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

UMaterialExpressionCustom* FindCustom(
    UMaterial* Material,
    const FString& Description,
    int32& OutCount)
{
    OutCount = 0;
    UMaterialExpressionCustom* Result = nullptr;
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    for (UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionCustom* Custom =
            Cast<UMaterialExpressionCustom>(Expression);
        if (Custom && Custom->Description == Description)
        {
            ++OutCount;
            Result = Custom;
        }
    }
    return Result;
}

bool ValidateMaterialPair(
    const FMaterialSpec& Spec,
    UMaterial* Source,
    UMaterial* Target,
    bool bRequireSaved,
    FString& OutError)
{
    const FString ExpectedTargetPath = MaterialObjectPath(Spec);
    const UMaterialEditorOnlyData* SourceData = Source
        ? Source->GetEditorOnlyData()
        : nullptr;
    const UMaterialEditorOnlyData* TargetData = Target
        ? Target->GetEditorOnlyData()
        : nullptr;
    int32 SourceStableCount = 0;
    int32 SourceRoughnessCount = 0;
    int32 SourceNormalCount = 0;
    int32 SourceColorCount = 0;
    int32 TargetStableCount = 0;
    int32 TargetRoughnessCount = 0;
    int32 TargetNormalCount = 0;
    int32 TargetColorCount = 0;
    UMaterialExpressionCustom* SourceStable = FindCustom(
        Source, SourceStableDescription, SourceStableCount);
    UMaterialExpressionCustom* SourceRoughness = FindCustom(
        Source, SourceRoughnessDescription, SourceRoughnessCount);
    UMaterialExpressionCustom* SourceNormal = FindCustom(
        Source, SourceNormalAlphaDescription, SourceNormalCount);
    UMaterialExpressionCustom* SourceColor = FindCustom(
        Source, Spec.SourceColorDescription, SourceColorCount);
    UMaterialExpressionCustom* TargetStable = FindCustom(
        Target, R29StableDescription, TargetStableCount);
    UMaterialExpressionCustom* TargetRoughness = FindCustom(
        Target, R29RoughnessDescription, TargetRoughnessCount);
    UMaterialExpressionCustom* TargetNormal = FindCustom(
        Target, R29NormalAlphaDescription, TargetNormalCount);
    UMaterialExpressionCustom* TargetColor = FindCustom(
        Target, Spec.SourceColorDescription, TargetColorCount);
    if (!Source || !Target || !SourceData || !TargetData ||
        Source->GetPathName() != Spec.SourceObjectPath ||
        Target->GetPathName() != ExpectedTargetPath ||
        SourceData->ExpressionCollection.Expressions.Num() != 32 ||
        TargetData->ExpressionCollection.Expressions.Num() != 32 ||
        SourceStableCount != 1 || SourceRoughnessCount != 1 ||
        SourceNormalCount != 1 || SourceColorCount != 1 ||
        TargetStableCount != 1 || TargetRoughnessCount != 1 ||
        TargetNormalCount != 1 || TargetColorCount != 1 ||
        !SourceStable || !SourceRoughness || !SourceNormal || !SourceColor ||
        !TargetStable || !TargetRoughness || !TargetNormal || !TargetColor ||
        SourceStable->Code != StableCode || TargetStable->Code != StableCode ||
        TargetRoughness->Code != RoughnessCode ||
        TargetNormal->Code != NormalAlphaCode ||
        SourceColor->Code != TargetColor->Code ||
        SourceData->OpacityMask.Expression != SourceStable ||
        TargetData->OpacityMask.Expression != TargetStable ||
        SourceData->Roughness.Expression != SourceRoughness ||
        TargetData->Roughness.Expression != TargetRoughness ||
        Source->MaterialDomain != Target->MaterialDomain ||
        Source->BlendMode != Target->BlendMode ||
        Source->TwoSided != Target->TwoSided ||
        Source->bTangentSpaceNormal != Target->bTangentSpaceNormal ||
        Source->bUsedWithInstancedStaticMeshes !=
            Target->bUsedWithInstancedStaticMeshes ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty() ||
        (bRequireSaved &&
            (!Target->GetOutermost() || Target->GetOutermost()->IsDirty() ||
             !FPackageName::DoesPackageExist(
                 FPackageName::ObjectPathToPackageName(ExpectedTargetPath)))))
    {
        OutError = FString::Printf(
            TEXT("R29 grass material '%s' is not the exact admitted R28 derivative with only visibility provenance, multi-scale roughness, and retained-distance normal response changed."),
            Spec.Name);
        return false;
    }

    for (int32 Index = 0;
         Index < SourceData->ExpressionCollection.Expressions.Num();
         ++Index)
    {
        const UMaterialExpression* SourceExpression =
            SourceData->ExpressionCollection.Expressions[Index];
        const UMaterialExpression* TargetExpression =
            TargetData->ExpressionCollection.Expressions[Index];
        if (!SourceExpression || !TargetExpression ||
            SourceExpression->GetClass() != TargetExpression->GetClass())
        {
            OutError = TEXT("R29 grass material changed expression topology.");
            return false;
        }
        const UMaterialExpressionCustom* SourceCustom =
            Cast<UMaterialExpressionCustom>(SourceExpression);
        const UMaterialExpressionCustom* TargetCustom =
            Cast<UMaterialExpressionCustom>(TargetExpression);
        const bool bAllowed =
            SourceCustom == SourceStable ||
            SourceCustom == SourceRoughness ||
            SourceCustom == SourceNormal;
        if (SourceCustom && !bAllowed &&
            (!TargetCustom || SourceCustom->Description != TargetCustom->Description ||
             SourceCustom->Code != TargetCustom->Code ||
             SourceCustom->OutputType != TargetCustom->OutputType ||
             SourceCustom->Inputs.Num() != TargetCustom->Inputs.Num() ||
             SourceCustom->AdditionalOutputs.Num() !=
                 TargetCustom->AdditionalOutputs.Num() ||
             SourceCustom->AdditionalDefines.Num() !=
                 TargetCustom->AdditionalDefines.Num() ||
             SourceCustom->IncludeFilePaths.Num() !=
                 TargetCustom->IncludeFilePaths.Num()))
        {
            OutError = TEXT("R29 grass material changed a forbidden custom-expression body.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

UMaterial* DuplicateMaterial(
    const FMaterialSpec& Spec,
    UMaterial* Source,
    FString& OutError)
{
    const FString PackagePath = MaterialRoot + TEXT("/") + Spec.Name;
    if (!Source || FPackageName::DoesPackageExist(PackagePath) ||
        FindPackage(nullptr, *PackagePath))
    {
        OutError = TEXT("R29 material destination is not fresh: ") + PackagePath;
        return nullptr;
    }
    UPackage* Package = CreatePackage(*PackagePath);
    UMaterial* Target = Package
        ? Cast<UMaterial>(StaticDuplicateObject(
              Source,
              Package,
              FName(Spec.Name),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    int32 StableCount = 0;
    int32 RoughnessCount = 0;
    int32 NormalCount = 0;
    UMaterialExpressionCustom* Stable = FindCustom(
        Target, SourceStableDescription, StableCount);
    UMaterialExpressionCustom* Roughness = FindCustom(
        Target, SourceRoughnessDescription, RoughnessCount);
    UMaterialExpressionCustom* Normal = FindCustom(
        Target, SourceNormalAlphaDescription, NormalCount);
    if (!Target || Target->GetPathName() != MaterialObjectPath(Spec) ||
        StableCount != 1 || RoughnessCount != 1 || NormalCount != 1 ||
        !Stable || !Roughness || !Normal || Stable->Code != StableCode)
    {
        OutError = TEXT("Could not duplicate the exact R28 grass graph into R29.");
        return nullptr;
    }
    Target->Modify();
    Target->PreEditChange(nullptr);
    Stable->Modify();
    Stable->Description = R29StableDescription;
    Roughness->Modify();
    Roughness->Description = R29RoughnessDescription;
    Roughness->Code = RoughnessCode;
    Normal->Modify();
    Normal->Description = R29NormalAlphaDescription;
    Normal->Code = NormalAlphaCode;
    UMaterialEditingLibrary::RecompileMaterial(Target);
    Target->PostEditChange();
    Target->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Target);
    if (!ValidateMaterialPair(Spec, Source, Target, false, OutError))
    {
        return nullptr;
    }
    return Target;
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
    Options->StaticMeshImportData->bBuildNanite = false;
    Options->StaticMeshImportData->bRemoveDegenerates = false;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = SourcePath(Spec.Source->RelativePath);
    Task->DestinationPath = MeshRoot;
    Task->DestinationName = Spec.Name;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool NormalizeMesh(UStaticMesh* Mesh, const FMeshSpec& Spec, FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetPathName() != MeshObjectPath(Spec) || !Description ||
        Description->Triangles().Num() != Spec.Triangles ||
        Description->VertexInstances().Num() != Spec.SourceCorners ||
        Mesh->GetStaticMaterials().Num() != 1)
    {
        OutError = TEXT("An imported R29 grass mesh lost exact source topology.");
        return false;
    }
    Mesh->Modify();
    TArray<FStaticMaterial> MaterialSlots;
    MaterialSlots.Emplace(
        nullptr,
        FName(TEXT("R29Grass")),
        FName(TEXT("R29Grass")));
    Mesh->SetStaticMaterials(MaterialSlots);
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
    Mesh->NaniteSettings.bEnabled = false;
    Mesh->CreateBodySetup();
    if (Mesh->GetBodySetup())
    {
        Mesh->GetBodySetup()->Modify();
        Mesh->GetBodySetup()->AggGeom.EmptyElements();
        Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseDefault;
        Mesh->GetBodySetup()->InvalidatePhysicsData();
    }
    FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, 0);
    Section.bEnableCollision = false;
    Section.bCastShadow = false;
    Mesh->GetSectionInfoMap().Set(0, 0, Section);
    Mesh->GetOriginalSectionInfoMap().Set(0, 0, Section);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    return true;
}

bool ValidateMesh(UStaticMesh* Mesh, const FMeshSpec& Spec, FString& OutError)
{
    if (!Mesh)
    {
        OutError = FString::Printf(
            TEXT("R29 grass mesh '%s' is missing."),
            Spec.Name);
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    const FMeshDescription* Description =
        Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const double Height = Bounds.BoxExtent.Z * 2.0;
    const FStaticMeshSourceModel* SourceModel =
        Mesh->GetNumSourceModels() == 1
        ? &Mesh->GetSourceModel(0)
        : nullptr;
    if (!Mesh || !Description || !SourceModel ||
        Mesh->GetPathName() != MeshObjectPath(Spec) ||
        Description->Triangles().Num() != Spec.Triangles ||
        Description->VertexInstances().Num() != Spec.SourceCorners ||
        Mesh->GetStaticMaterials().Num() != 1 ||
        Mesh->GetStaticMaterials()[0].MaterialSlotName !=
            FName(TEXT("R29Grass")) ||
        !FMath::IsWithinInclusive(
            Height, Spec.MinimumHeightCm, Spec.MaximumHeightCm) ||
        Mesh->NaniteSettings.bEnabled || Mesh->bGenerateMeshDistanceField ||
        SourceModel->BuildSettings.bGenerateLightmapUVs ||
        !SourceModel->BuildSettings.bUseFullPrecisionUVs ||
        SourceModel->BuildSettings.bRecomputeNormals ||
        !SourceModel->BuildSettings.bRecomputeTangents ||
        !SourceModel->BuildSettings.bUseMikkTSpace ||
        !Mesh->GetBodySetup() ||
        Mesh->GetBodySetup()->AggGeom.GetElementCount() != 0 ||
        Mesh->GetSectionInfoMap().Get(0, 0).bEnableCollision ||
        Mesh->GetSectionInfoMap().Get(0, 0).bCastShadow ||
        !Mesh->GetOutermost() || Mesh->GetOutermost()->IsDirty() ||
        !FPackageName::DoesPackageExist(
            FPackageName::ObjectPathToPackageName(MeshObjectPath(Spec))))
    {
        OutError = FString::Printf(
            TEXT("R29 grass mesh '%s' failed exact topology, bounds, or render-only validation."),
            Spec.Name);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(FString& OutReport)
{
    if (!ValidateSources(OutReport))
    {
        return false;
    }
    FString R28SourceReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateLandmarkGrassMaterialsR28(R28SourceReport))
    {
        OutReport = TEXT("R29 vegetation refused non-canonical R28 grass sources: ") +
            R28SourceReport;
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Source = LoadExact<UMaterial>(Spec.SourceObjectPath);
        UMaterial* Target = LoadExact<UMaterial>(MaterialObjectPath(Spec));
        if (!ValidateMaterialPair(Spec, Source, Target, true, OutReport))
        {
            return false;
        }
    }
    for (const FMeshSpec& Spec : MeshSpecs)
    {
        if (!ValidateMesh(
                LoadExact<UStaticMesh>(MeshObjectPath(Spec)),
                Spec,
                OutReport))
        {
            return false;
        }
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSETS_VALID exactAssets=7 isolatedGrassMeshes=3 modeledBladeMeshTriangles=3072 modeledBladesPerCarrier=56,64,72 authoredNormalsFaceAligned=true minimumTriangleVertexNormalDot=0.987537 isolatedGrassMaterials=4 exactR28Sources=4 allowedGraphDeltas=visibilityProvenance,roughnessResponse,normalResponse roughnessResponseMeters=18,65 normalResponseMeters=18,60 retainedFarNormalResponse=0.10 stableVisibilityMeters=65,90 textureInputsAdded=0 sourceMaterialsModified=false sourceMeshesModified=false mapsSaved=0 collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false botanicalClaim=false visualCaptureAccepted=false captureRevalidationRequired=true");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR29VegetationAssetFactory
{
const TArray<FString>& GetExpectedAssetObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result;
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Result.Add(MaterialObjectPath(Spec));
        }
        for (const FMeshSpec& Spec : MeshSpecs)
        {
            Result.Add(MeshObjectPath(Spec));
        }
        Result.Sort();
        return Result;
    }();
    return Paths;
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    if (!ValidateSources(OutError))
    {
        return false;
    }
    FString R28SourceReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateLandmarkGrassMaterialsR28(R28SourceReport))
    {
        OutError = TEXT("R29 vegetation refused non-canonical R28 grass sources: ") +
            R28SourceReport;
        return false;
    }
    int32 ExistingCount = 0;
    for (const FString& Path : GetExpectedAssetObjectPaths())
    {
        ExistingCount += FindObject<UObject>(nullptr, *Path) ||
            FPackageName::DoesPackageExist(
                FPackageName::ObjectPathToPackageName(Path))
            ? 1
            : 0;
    }
    if (ExistingCount == 7)
    {
        FString ExistingReport;
        if (!ValidateInternal(ExistingReport))
        {
            OutError = TEXT("Existing complete R29 asset roster failed exact validation: ") +
                ExistingReport;
            return false;
        }
        for (const FString& Path : GetExpectedAssetObjectPaths())
        {
            OutAssets.Add(LoadObject<UObject>(nullptr, *Path));
        }
        OutError = ExistingReport;
        return true;
    }
    if (ExistingCount != 0)
    {
        OutError = FString::Printf(
            TEXT("R29 creation refuses an invalid or partially populated isolated root: existing=%d expected=0-or-7."),
            ExistingCount);
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    TArray<UMaterial*> Materials;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Source = LoadExact<UMaterial>(Spec.SourceObjectPath);
        UMaterial* Target = DuplicateMaterial(Spec, Source, OutError);
        if (!Target)
        {
            return false;
        }
        Materials.Add(Target);
        OutAssets.Add(Target);
    }
    TArray<UAssetImportTask*> Tasks;
    for (const FMeshSpec& Spec : MeshSpecs)
    {
        UAssetImportTask* Task = MakeImportTask(Spec);
        if (!Task)
        {
            OutError = TEXT("Could not allocate all three R29 grass OBJ import tasks.");
            return false;
        }
        Tasks.Add(Task);
    }
    AssetTools.ImportAssetTasks(Tasks);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MeshSpecs); ++Index)
    {
        UStaticMesh* Mesh = nullptr;
        for (UObject* Object : Tasks[Index]->GetObjects())
        {
            if (Object && Object->GetPathName() == MeshObjectPath(MeshSpecs[Index]))
            {
                Mesh = Cast<UStaticMesh>(Object);
                break;
            }
        }
        if (!Mesh)
        {
            Mesh = LoadExact<UStaticMesh>(MeshObjectPath(MeshSpecs[Index]));
        }
        if (!NormalizeMesh(Mesh, MeshSpecs[Index], OutError))
        {
            return false;
        }
        OutAssets.Add(Mesh);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem || OutAssets.Num() != 7 ||
        !AssetSubsystem->SaveLoadedAssets(OutAssets, false))
    {
        OutError = TEXT("R29 vegetation failed to save the exact seven-asset roster.");
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Report;
    if (!ValidateInternal(Report))
    {
        OutError = TEXT("Fresh R29 vegetation assets failed exact validation: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(OutReport);
}

bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR29VegetationAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DR29VegetationAssets{};
    if (!ValidateInternal(OutError))
    {
        return false;
    }
    for (const FMeshSpec& Spec : MeshSpecs)
    {
        OutAssets.GrassMeshVariants.Add(
            LoadExact<UStaticMesh>(MeshObjectPath(Spec)));
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        OutAssets.GrassProfileMaterials.Add(
            LoadExact<UMaterialInterface>(MaterialObjectPath(Spec)));
    }
    const TCHAR* TreePaths[] = {
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.SM_IPV5D_Tree_ColumnarNarrow_NearLOD0"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0.SM_IPV5D_Tree_Palm_NearLOD0")};
    for (const TCHAR* Path : TreePaths)
    {
        OutAssets.TropicalTreeMeshes.Add(LoadExact<UStaticMesh>(Path));
    }
    OutAssets.ShrubMesh = LoadExact<UStaticMesh>(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Shrub04_A.SM_IPV4_Shrub04_A"));
    OutAssets.ShrubMaterial = LoadExact<UMaterialInterface>(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Shrub04_Wind.M_IPV4_Shrub04_Wind"));
    OutAssets.UnderstoreyMesh = LoadExact<UStaticMesh>(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Calathea_D.SM_IPV4_Calathea_D"));
    OutAssets.UnderstoreyMaterial = LoadExact<UMaterialInterface>(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Calathea_Wind.M_IPV4_Calathea_Wind"));
    OutAssets.FlowerMesh = LoadExact<UStaticMesh>(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Periwinkle06_F.SM_IPV4_Periwinkle06_F"));
    OutAssets.FlowerMaterial = LoadExact<UMaterialInterface>(
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Periwinkle_Wind.M_IPV4_Periwinkle_Wind"));
    if (!ATRIADIstanaExploreV5DR29VegetationActor::
            ValidateAssetRoster(OutAssets, OutError))
    {
        OutAssets = FTRIADIstanaExploreV5DR29VegetationAssets{};
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DR29VegetationAssetFactory
