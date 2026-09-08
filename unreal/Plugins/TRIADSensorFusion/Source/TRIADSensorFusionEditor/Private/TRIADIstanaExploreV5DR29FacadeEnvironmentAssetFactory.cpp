#include "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
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
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR29"));
const FString MeshRoot(AssetRoot + TEXT("/Meshes"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString FacadeMeshName(
    TEXT("SM_IPV5D_R29_ContextFacadeCoverage_Render"));

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const FString FacadeMeshObjectPath(ObjectPath(MeshRoot, FacadeMeshName));
const FString R28MasterObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR28/"
         "Materials/M_IPV5D_R28_Surface_Master."
         "M_IPV5D_R28_Surface_Master"));

const FName TintParameter(TEXT("Tint"));
const FName RoughnessParameter(TEXT("Roughness"));
const FName SpecularParameter(TEXT("Specular"));
const FName MacroStrengthParameter(TEXT("MacroStrength"));
const FName MacroScaleParameter(TEXT("MacroScaleCm"));
const FName MicroStrengthParameter(TEXT("MicroStrength"));
const FName MicroScaleParameter(TEXT("MicroScaleCm"));
const FName JointStrengthParameter(TEXT("JointStrength"));
const FName JointSpacingParameter(TEXT("JointSpacingCm"));
const FName RoughnessVariationParameter(TEXT("RoughnessVariation"));
const FName GlazingGrazingStrengthParameter(TEXT("GlazingGrazingStrength"));

struct FMaterialSpec
{
    const TCHAR* Name;
    FLinearColor Tint;
    float Roughness;
    float Specular;
    float MacroStrength;
    float MacroScaleCm;
    float MicroStrength;
    float MicroScaleCm;
    float JointStrength;
    float JointSpacingCm;
    float RoughnessVariation;
    float GlazingGrazingStrength;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("MI_IPV5D_R29_GlassCool"), FLinearColor(0.018f, 0.055f, 0.078f), 0.24f, 0.62f, 0.032f, 170.0f, 0.014f, 38.0f, 0.0f, 100.0f, 0.022f, 0.32f},
    {TEXT("MI_IPV5D_R29_GlassWarm"), FLinearColor(0.095f, 0.066f, 0.040f), 0.30f, 0.50f, 0.042f, 185.0f, 0.016f, 42.0f, 0.0f, 100.0f, 0.025f, 0.27f},
    {TEXT("MI_IPV5D_R29_GlassNeutral"), FLinearColor(0.060f, 0.073f, 0.075f), 0.27f, 0.56f, 0.036f, 176.0f, 0.015f, 40.0f, 0.0f, 100.0f, 0.023f, 0.29f},
    {TEXT("MI_IPV5D_R29_FrameLight"), FLinearColor(0.670f, 0.640f, 0.560f), 0.55f, 0.20f, 0.050f, 205.0f, 0.032f, 18.0f, 0.0f, 100.0f, 0.034f, 0.0f},
    {TEXT("MI_IPV5D_R29_FrameDark"), FLinearColor(0.060f, 0.069f, 0.074f), 0.47f, 0.31f, 0.044f, 180.0f, 0.030f, 17.0f, 0.0f, 100.0f, 0.033f, 0.0f},
    {TEXT("MI_IPV5D_R29_FrameBronze"), FLinearColor(0.245f, 0.155f, 0.085f), 0.43f, 0.34f, 0.052f, 192.0f, 0.034f, 19.0f, 0.0f, 100.0f, 0.036f, 0.0f},
    {TEXT("MI_IPV5D_R29_SillLight"), FLinearColor(0.510f, 0.490f, 0.440f), 0.72f, 0.15f, 0.070f, 255.0f, 0.045f, 27.0f, 0.018f, 145.0f, 0.045f, 0.0f},
    {TEXT("MI_IPV5D_R29_SillDark"), FLinearColor(0.160f, 0.170f, 0.170f), 0.68f, 0.18f, 0.064f, 242.0f, 0.042f, 25.0f, 0.018f, 145.0f, 0.043f, 0.0f},
    {TEXT("MI_IPV5D_R29_RoofTrim"), FLinearColor(0.120f, 0.130f, 0.135f), 0.77f, 0.15f, 0.080f, 290.0f, 0.058f, 35.0f, 0.020f, 160.0f, 0.048f, 0.0f},
    {TEXT("MI_IPV5D_R29_Canopy"), FLinearColor(0.310f, 0.160f, 0.085f), 0.59f, 0.22f, 0.070f, 225.0f, 0.047f, 24.0f, 0.0f, 100.0f, 0.044f, 0.0f},
    {TEXT("MI_IPV5D_R29_BalconyRail"), FLinearColor(0.055f, 0.062f, 0.065f), 0.42f, 0.36f, 0.042f, 175.0f, 0.030f, 16.0f, 0.0f, 100.0f, 0.033f, 0.0f}};

static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 11);

// Exact per-material triangle census from the hash-bound manifest. UE's OBJ
// importer deterministically orders the material slots by semantic name rather
// than by first `usemtl` occurrence, so validation binds each section's exact
// count to its saved material-slot name instead of assuming source-file order.
// Merely retaining eleven slots is insufficient because a tampered section map
// could otherwise render every triangle through one slot.
constexpr int32 FacadeMaterialTriangleCounts[] = {
    13686, // GlassCool
    13568, // GlassWarm
    12768, // GlassNeutral
    61044, // FrameLight
    58874, // FrameDark
    46710, // FrameBronze
    18592, // SillLight
    13156, // SillDark
    17916, // RoofTrim
    368,   // Canopy
    168};  // BalconyRail
static_assert(
    UE_ARRAY_COUNT(FacadeMaterialTriangleCounts) ==
    UE_ARRAY_COUNT(MaterialSpecs));

struct FSourceSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceSpec SourceSpecs[] = {
    {TEXT("Generated/SM_IPV5D_R29_ContextFacadeCoverage_Render.obj"),
     102051194,
     TEXT("8CE9659A991FC18369EC980FBCA8F9EFAF00B81784D9B2F423BEC935D13181C7")},
    {TEXT("Generated/IstanaPublicViewV5DR29ContextFacadeCoverage.mtl"),
     1580,
     TEXT("4989AFC4478B48C55E038F1E85233C8FA1405360EA19C7E623FDB98FD3B59ADD")},
    {TEXT("Generated/IstanaPublicViewV5DR29ContextFacadeCoverage.manifest.json"),
     11557,
     TEXT("DE2543394A10901FFA6025E325E8E92449CF866F4FC9934B99D9C9FCAEF0ECA2")}};

constexpr int32 FacadeTriangleCount = 256850;
constexpr int32 FacadeSourceCornerCount = 770550;

FString SourceRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
             "R29ContextFacadeCoverage")));
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
            TEXT("R29 facade source byte guard failed for '%s': expected=%lld actual=%d."),
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
        OutError = TEXT("R29 facade source SHA-256 guard failed for '") +
            Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R29 facade source admission requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

bool ValidateSources(FString& OutError)
{
    for (const FSourceSpec& Spec : SourceSpecs)
    {
        if (!ValidateSource(Spec, OutError))
        {
            return false;
        }
    }
    return true;
}

const FMaterialSpec* FindMaterialSpec(const FString& Name)
{
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        if (Name == Spec.Name)
        {
            return &Spec;
        }
    }
    return nullptr;
}

int32 ExpectedFacadeTriangleCountForMaterial(const FString& Name)
{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MaterialSpecs); ++Index)
    {
        if (Name == MaterialSpecs[Index].Name)
        {
            return FacadeMaterialTriangleCounts[Index];
        }
    }
    return INDEX_NONE;
}

UMaterialInstanceConstant* CreateMaterialInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
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
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR29FacadeAssets"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create isolated R29 facade material '%s'."),
            Spec.Name);
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(TintParameter), Spec.Tint);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(RoughnessParameter), Spec.Roughness);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(SpecularParameter), Spec.Specular);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MacroStrengthParameter), Spec.MacroStrength);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MacroScaleParameter), Spec.MacroScaleCm);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MicroStrengthParameter), Spec.MicroStrength);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MicroScaleParameter), Spec.MicroScaleCm);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(JointStrengthParameter), Spec.JointStrength);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(JointSpacingParameter), Spec.JointSpacingCm);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(RoughnessVariationParameter),
        Spec.RoughnessVariation);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(GlazingGrazingStrengthParameter),
        Spec.GlazingGrazingStrength);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    return Instance;
}

bool ValidateMaterialInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    bool bRequireSaved,
    FString& OutError)
{
    const FString Name(Spec.Name);
    const bool bGlazing = Name.StartsWith(TEXT("MI_IPV5D_R29_Glass"));
    if (!Instance || Instance->GetPathName() != MaterialObjectPath(Spec) ||
        Instance->Parent != Parent ||
        Instance->ScalarParameterValues.Num() != 10 ||
        Instance->VectorParameterValues.Num() != 1 ||
        !Instance->TextureParameterValues.IsEmpty() ||
        (bGlazing &&
         (Spec.GlazingGrazingStrength <= 0.0f ||
          Spec.GlazingGrazingStrength > 0.35f)) ||
        (!bGlazing && !FMath::IsNearlyZero(Spec.GlazingGrazingStrength)) ||
        (bRequireSaved &&
         (!Instance->GetOutermost() || Instance->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              FPackageName::ObjectPathToPackageName(
                  MaterialObjectPath(Spec))))))
    {
        OutError = FString::Printf(
            TEXT("R29 facade material '%s' lost its exact texture-free derivative contract."),
            Spec.Name);
        return false;
    }
    float Scalar = 0.0f;
    FLinearColor Tint;
    const auto ScalarMatches = [Instance, &Scalar](FName Parameter, float Expected)
    {
        return Instance->GetScalarParameterValue(
                   FHashedMaterialParameterInfo(Parameter), Scalar, true) &&
            FMath::IsNearlyEqual(Scalar, Expected, 0.000001f);
    };
    if (!Instance->GetVectorParameterValue(
            FHashedMaterialParameterInfo(TintParameter), Tint, true) ||
        !Tint.Equals(Spec.Tint, 0.000001f) ||
        !ScalarMatches(RoughnessParameter, Spec.Roughness) ||
        !ScalarMatches(SpecularParameter, Spec.Specular) ||
        !ScalarMatches(MacroStrengthParameter, Spec.MacroStrength) ||
        !ScalarMatches(MacroScaleParameter, Spec.MacroScaleCm) ||
        !ScalarMatches(MicroStrengthParameter, Spec.MicroStrength) ||
        !ScalarMatches(MicroScaleParameter, Spec.MicroScaleCm) ||
        !ScalarMatches(JointStrengthParameter, Spec.JointStrength) ||
        !ScalarMatches(JointSpacingParameter, Spec.JointSpacingCm) ||
        !ScalarMatches(RoughnessVariationParameter, Spec.RoughnessVariation) ||
        !ScalarMatches(
            GlazingGrazingStrengthParameter,
            Spec.GlazingGrazingStrength))
    {
        OutError = FString::Printf(
            TEXT("R29 facade material '%s' parameter values drifted."),
            Spec.Name);
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
    Task->Filename = SourcePath(SourceSpecs[0].RelativePath);
    Task->DestinationPath = MeshRoot;
    Task->DestinationName = FacadeMeshName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool NormalizeMesh(
    UStaticMesh* Mesh,
    const TMap<FString, UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetPathName() != FacadeMeshObjectPath ||
        !Description ||
        Description->Triangles().Num() != FacadeTriangleCount ||
        Description->VertexInstances().Num() != FacadeSourceCornerCount ||
        Mesh->GetStaticMaterials().Num() != UE_ARRAY_COUNT(MaterialSpecs))
    {
        OutError = TEXT("Imported R29 facade mesh lost exact source topology or material roster.");
        return false;
    }
    TArray<FStaticMaterial> BoundMaterials;
    TSet<FString> SeenNames;
    for (const FStaticMaterial& Imported : Mesh->GetStaticMaterials())
    {
        const FString Name = Imported.ImportedMaterialSlotName.IsNone()
            ? Imported.MaterialSlotName.ToString()
            : Imported.ImportedMaterialSlotName.ToString();
        UMaterialInstanceConstant* const* Material = Materials.Find(Name);
        if (!Material || !*Material || !FindMaterialSpec(Name) ||
            SeenNames.Contains(Name))
        {
            OutError = TEXT("R29 OBJ material groups did not match the exact eleven-slot semantic roster.");
            return false;
        }
        SeenNames.Add(Name);
        BoundMaterials.Emplace(*Material, FName(*Name), FName(*Name));
    }
    if (SeenNames.Num() != UE_ARRAY_COUNT(MaterialSpecs))
    {
        OutError = TEXT("R29 facade import omitted one or more semantic material groups.");
        return false;
    }

    Mesh->Modify();
    Mesh->SetStaticMaterials(BoundMaterials);
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
    Mesh->NaniteSettings.FallbackTarget =
        ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    for (int32 SectionIndex = 0;
         SectionIndex < UE_ARRAY_COUNT(MaterialSpecs);
         ++SectionIndex)
    {
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        Section.MaterialIndex = SectionIndex;
        Section.bEnableCollision = false;
        Section.bCastShadow = true;
        Section.bAffectDistanceFieldLighting = false;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Section);
    }
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->AggGeom.EmptyElements();
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        Body->InvalidatePhysicsData();
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

bool ValidateMesh(
    UStaticMesh* Mesh,
    const TMap<FString, UMaterialInstanceConstant*>& Materials,
    bool bRequireSaved,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    const FStaticMeshSourceModel* SourceModel =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? &Mesh->GetSourceModel(0)
        : nullptr;
    int32 RenderTriangles = 0;
    FString RenderSectionReport;
    bool bExactRenderSectionCensus =
        RenderData && RenderData->LODResources.Num() == 1 &&
        RenderData->LODResources[0].Sections.Num() ==
            UE_ARRAY_COUNT(MaterialSpecs);
    if (RenderData && RenderData->LODResources.Num() == 1)
    {
        int32 ExpectedFirstIndex = 0;
        for (int32 SectionIndex = 0;
             SectionIndex < RenderData->LODResources[0].Sections.Num();
             ++SectionIndex)
        {
            const FStaticMeshSection& Section =
                RenderData->LODResources[0].Sections[SectionIndex];
            const int32 MaterialIndex =
                static_cast<int32>(Section.MaterialIndex);
            const FString SlotName =
                MaterialIndex >= 0 &&
                MaterialIndex < Mesh->GetStaticMaterials().Num()
                ? Mesh->GetStaticMaterials()[MaterialIndex]
                      .MaterialSlotName.ToString()
                : FString();
            const int32 ExpectedSectionTriangles =
                ExpectedFacadeTriangleCountForMaterial(SlotName);
            RenderTriangles += Section.NumTriangles;
            RenderSectionReport += FString::Printf(
                TEXT("%d:material=%d,slot=%s,triangles=%u,expected=%d,firstIndex=%u;"),
                SectionIndex,
                MaterialIndex,
                *SlotName,
                Section.NumTriangles,
                ExpectedSectionTriangles,
                Section.FirstIndex);
            if (MaterialIndex != SectionIndex ||
                ExpectedSectionTriangles == INDEX_NONE ||
                Section.NumTriangles !=
                    static_cast<uint32>(ExpectedSectionTriangles) ||
                Section.FirstIndex !=
                    static_cast<uint32>(ExpectedFirstIndex))
            {
                bExactRenderSectionCensus = false;
            }
            if (ExpectedSectionTriangles != INDEX_NONE)
            {
                ExpectedFirstIndex += ExpectedSectionTriangles * 3;
            }
        }
    }
    if (!Mesh || Mesh->GetPathName() != FacadeMeshObjectPath ||
        !Description || !SourceModel ||
        Description->Triangles().Num() != FacadeTriangleCount ||
        Description->VertexInstances().Num() != FacadeSourceCornerCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        !bExactRenderSectionCensus ||
        RenderTriangles != FacadeTriangleCount ||
        Mesh->GetStaticMaterials().Num() != UE_ARRAY_COUNT(MaterialSpecs) ||
        !Mesh->NaniteSettings.bEnabled || !Mesh->HasValidNaniteData() ||
        !FMath::IsNearlyEqual(Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError) ||
        SourceModel->BuildSettings.bGenerateLightmapUVs ||
        !SourceModel->BuildSettings.bUseFullPrecisionUVs ||
        SourceModel->BuildSettings.bRecomputeNormals ||
        !SourceModel->BuildSettings.bRecomputeTangents ||
        !SourceModel->BuildSettings.bUseMikkTSpace ||
        Mesh->bGenerateMeshDistanceField || Mesh->bHasNavigationData ||
        Mesh->GetNavCollision() || !Body ||
        Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag == CTF_UseComplexAsSimple ||
        (bRequireSaved &&
         (!Mesh->GetOutermost() || Mesh->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              FPackageName::ObjectPathToPackageName(
                  FacadeMeshObjectPath)))))
    {
        OutError = FString::Printf(
            TEXT("R29 facade mesh lost its exact topology, full-topology Nanite/raster, or negative collision/navigation contract. path=%s sourceModels=%d sourceTriangles=%d sourceCorners=%d renderLods=%d renderSections=%d renderTriangles=%d exactSectionCensus=%s materials=%d naniteEnabled=%s naniteValid=%s keepPercent=%.9g trimError=%.9g fallbackTarget=%d fallbackPercent=%.9g fallbackError=%.9g generateLightmapUvs=%s fullPrecisionUvs=%s recomputeNormals=%s recomputeTangents=%s mikk=%s distanceField=%s hasNavigation=%s navCollision=%s body=%s bodyElements=%d collisionTrace=%d requireSaved=%s dirty=%s packageExists=%s sections={%s}"),
            Mesh ? *Mesh->GetPathName() : TEXT("null"),
            Mesh ? Mesh->GetNumSourceModels() : -1,
            Description ? Description->Triangles().Num() : -1,
            Description ? Description->VertexInstances().Num() : -1,
            RenderData ? RenderData->LODResources.Num() : -1,
            RenderData && RenderData->LODResources.Num() == 1
                ? RenderData->LODResources[0].Sections.Num() : -1,
            RenderTriangles,
            bExactRenderSectionCensus ? TEXT("true") : TEXT("false"),
            Mesh ? Mesh->GetStaticMaterials().Num() : -1,
            Mesh && Mesh->NaniteSettings.bEnabled ? TEXT("true") : TEXT("false"),
            Mesh && Mesh->HasValidNaniteData() ? TEXT("true") : TEXT("false"),
            Mesh ? Mesh->NaniteSettings.KeepPercentTriangles : -1.0f,
            Mesh ? Mesh->NaniteSettings.TrimRelativeError : -1.0f,
            Mesh ? static_cast<int32>(Mesh->NaniteSettings.FallbackTarget) : -1,
            Mesh ? Mesh->NaniteSettings.FallbackPercentTriangles : -1.0f,
            Mesh ? Mesh->NaniteSettings.FallbackRelativeError : -1.0f,
            SourceModel && SourceModel->BuildSettings.bGenerateLightmapUVs ? TEXT("true") : TEXT("false"),
            SourceModel && SourceModel->BuildSettings.bUseFullPrecisionUVs ? TEXT("true") : TEXT("false"),
            SourceModel && SourceModel->BuildSettings.bRecomputeNormals ? TEXT("true") : TEXT("false"),
            SourceModel && SourceModel->BuildSettings.bRecomputeTangents ? TEXT("true") : TEXT("false"),
            SourceModel && SourceModel->BuildSettings.bUseMikkTSpace ? TEXT("true") : TEXT("false"),
            Mesh && Mesh->bGenerateMeshDistanceField ? TEXT("true") : TEXT("false"),
            Mesh && Mesh->bHasNavigationData ? TEXT("true") : TEXT("false"),
            Mesh && Mesh->GetNavCollision() ? TEXT("true") : TEXT("false"),
            Body ? TEXT("true") : TEXT("false"),
            Body ? Body->AggGeom.GetElementCount() : -1,
            Body ? static_cast<int32>(Body->CollisionTraceFlag) : -1,
            bRequireSaved ? TEXT("true") : TEXT("false"),
            Mesh && Mesh->GetOutermost() && Mesh->GetOutermost()->IsDirty()
                ? TEXT("true") : TEXT("false"),
            FPackageName::DoesPackageExist(
                FPackageName::ObjectPathToPackageName(FacadeMeshObjectPath))
                ? TEXT("true") : TEXT("false"),
            *RenderSectionReport);
        return false;
    }
    TSet<FString> Seen;
    for (int32 SectionIndex = 0;
         SectionIndex < Mesh->GetStaticMaterials().Num();
         ++SectionIndex)
    {
        const FStaticMaterial& Slot = Mesh->GetStaticMaterials()[SectionIndex];
        const FString Name = Slot.MaterialSlotName.ToString();
        UMaterialInstanceConstant* const* Expected = Materials.Find(Name);
        const FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        const FMeshSectionInfo OriginalSection =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!Expected || Slot.ImportedMaterialSlotName != Slot.MaterialSlotName ||
            Slot.MaterialInterface != *Expected || Seen.Contains(Name) ||
            Section.MaterialIndex != SectionIndex ||
            OriginalSection.MaterialIndex != SectionIndex ||
            Section.bEnableCollision || !Section.bCastShadow ||
            Section.bAffectDistanceFieldLighting ||
            OriginalSection.bEnableCollision || !OriginalSection.bCastShadow ||
            OriginalSection.bAffectDistanceFieldLighting)
        {
            OutError = TEXT("R29 facade mesh semantic material or render-only section policy drifted.");
            return false;
        }
        Seen.Add(Name);
    }
    OutError.Reset();
    return Seen.Num() == UE_ARRAY_COUNT(MaterialSpecs);
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

bool LoadAndValidateMaterials(
    UMaterial*& OutParent,
    TMap<FString, UMaterialInstanceConstant*>& OutMaterials,
    bool bRequireSaved,
    FString& OutError)
{
    OutParent = LoadExact<UMaterial>(R28MasterObjectPath);
    if (!OutParent)
    {
        OutError = TEXT("R29 facade materials require the exact validated R28 surface master.");
        return false;
    }
    OutMaterials.Reset();
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(MaterialObjectPath(Spec));
        if (!ValidateMaterialInstance(
                Instance, Spec, OutParent, bRequireSaved, OutError))
        {
            return false;
        }
        OutMaterials.Add(Spec.Name, Instance);
    }
    return true;
}

bool ValidateInternal(FString& OutReport)
{
    if (!ValidateSources(OutReport))
    {
        return false;
    }
    FString R28Report;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets(
            R28Report))
    {
        OutReport = TEXT("R29 facade refused invalid retained R28 presentation assets: ") +
            R28Report;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<FAssetData> RootAssets;
    GatherRootAssets(RootAssets);
    constexpr int32 ExpectedAssetCount = UE_ARRAY_COUNT(MaterialSpecs) + 1;
    if (RootAssets.Num() != ExpectedAssetCount)
    {
        OutReport = FString::Printf(
            TEXT("R29 facade isolated asset-root roster changed: expected=%d actual=%d."),
            ExpectedAssetCount,
            RootAssets.Num());
        return false;
    }
    UMaterial* Parent = nullptr;
    TMap<FString, UMaterialInstanceConstant*> Materials;
    if (!LoadAndValidateMaterials(
            Parent, Materials, true, OutReport) ||
        !ValidateMesh(
            LoadExact<UStaticMesh>(FacadeMeshObjectPath),
            Materials,
            true,
            OutReport))
    {
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_FACADE_ASSETS_VALID exactAssets=12 facadeMeshes=1 materialInstances=11 renderSections=11 exactPerMaterialTriangleCensus=true exactSectionMaterialMapping=true exactR28SurfaceMasterRetained=true r29ObjBytes=102051194 r29ObjSha256=8CE9659A991FC18369EC980FBCA8F9EFAF00B81784D9B2F423BEC935D13181C7 r29MtlBytes=1580 r29MtlSha256=4989AFC4478B48C55E038F1E85233C8FA1405360EA19C7E623FDB98FD3B59ADD r29ManifestBytes=11557 r29ManifestSha256=DE2543394A10901FFA6025E325E8E92449CF866F4FC9934B99D9C9FCAEF0ECA2 triangles=256850 sourceCorners=770550 selectedBuildingParts=1174 sourceBuildingParts=1305 apertureGroups=20011 bottomFrameRails=20011 everyGroupedApertureFourSided=true bottomRailIntersectsContinuousSillTop=true sillLedges=7881 awnings=46 balconyProxies=28 fullTopologyNanite=true textureInputsAdded=0 collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false surveyClaim=false asBuiltClaim=false currentCompleteClaim=false physicalMaterialClaim=false visualCaptureAccepted=false captureRevalidationRequired=true.");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory
{
const FString& GetAssetRootPath()
{
    return AssetRoot;
}

const FString& GetFacadeMeshObjectPath()
{
    return FacadeMeshObjectPath;
}

const TArray<FString>& GetExpectedAssetObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result = {FacadeMeshObjectPath};
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
    if (!ValidateSources(OutError))
    {
        return false;
    }
    FString R28Report;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets(
            R28Report))
    {
        OutError = TEXT("R29 facade creation refused invalid retained R28 assets: ") +
            R28Report;
        return false;
    }
    TArray<FAssetData> Existing;
    GatherRootAssets(Existing);
    if (!Existing.IsEmpty())
    {
        OutError = TEXT("R29 facade creation refuses an invalid or partially populated isolated asset root.");
        return false;
    }

    UMaterial* Parent = LoadExact<UMaterial>(R28MasterObjectPath);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    TMap<FString, UMaterialInstanceConstant*> Materials;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance = CreateMaterialInstance(
            AssetTools, Spec, Parent, OutError);
        if (!Instance ||
            !ValidateMaterialInstance(
                Instance, Spec, Parent, false, OutError))
        {
            return false;
        }
        Materials.Add(Spec.Name, Instance);
        OutAssets.Add(Instance);
    }
    UAssetImportTask* Task = MakeImportTask();
    if (!Task)
    {
        OutError = TEXT("Could not allocate the exact R29 facade OBJ import task.");
        return false;
    }
    AssetTools.ImportAssetTasks({Task});
    UStaticMesh* Facade = nullptr;
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == FacadeMeshObjectPath)
        {
            Facade = Cast<UStaticMesh>(Object);
            break;
        }
    }
    if (!Facade)
    {
        Facade = LoadExact<UStaticMesh>(FacadeMeshObjectPath);
    }
    if (!NormalizeMesh(Facade, Materials, OutError) ||
        !ValidateMesh(Facade, Materials, false, OutError))
    {
        return false;
    }
    OutAssets.Add(Facade);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (OutAssets.Num() != 12)
    {
        OutError = TEXT("R29 facade fresh asset roster is not exactly eleven materials plus one mesh.");
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
    FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets{};
    if (!ValidateInternal(OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DR28EnvironmentAssets R28Assets;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::
            LoadValidatedRuntimeContract(R28Assets, OutError))
    {
        return false;
    }
    OutAssets.RetainedR28ConnectivePublicRealmMesh =
        R28Assets.ConnectivePublicRealmMesh;
    OutAssets.ContextFacadeCoverageMesh =
        LoadExact<UStaticMesh>(FacadeMeshObjectPath);
    OutAssets.RetainedOuterGroundMesh = R28Assets.OuterGroundMesh;
    OutAssets.RetainedR28OuterGroundMaterial =
        R28Assets.OuterGroundMaterial;
    if (!ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
            ValidateAssetRoster(OutAssets, OutError))
    {
        OutAssets = FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets{};
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory
