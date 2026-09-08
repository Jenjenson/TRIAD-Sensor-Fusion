#include "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"
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
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5CSurroundingsAssetFactory.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings"));
const FString MeshName(TEXT("SM_IPV5D_OSMCurrentSurroundings_Render"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);

const FString V5COfficialWallPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OfficialContextRender.M_IPV5C_OfficialContextRender"));
const FString V5COfficialRoofPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OfficialContextRoof.M_IPV5C_OfficialContextRoof"));
const FString V5CFallbackWallPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OsmFallbackContextRender.M_IPV5C_OsmFallbackContextRender"));
const FString V5CFallbackRoofPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OsmFallbackContextRoof.M_IPV5C_OsmFallbackContextRoof"));

constexpr int32 ExpectedAssetCount = 1;
constexpr int32 ExpectedSourceVertexCount = 24522;
constexpr int32 ExpectedVertexInstanceCount = 130632;
constexpr int32 ExpectedTriangleCount = 43544;
constexpr int32 ExpectedMaterialCount = 17;

constexpr int64 ExpectedObjBytes = 6359246;
constexpr int64 ExpectedMtlBytes = 2645;
constexpr int64 ExpectedManifestBytes = 4533;
constexpr int64 ExpectedFeaturesBytes = 3248669;
constexpr int64 ExpectedGeometryBytes = 4899394;
constexpr int64 ExpectedContractBytes = 6328;

const FString ExpectedObjSha256(
    TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3"));
const FString ExpectedMtlSha256(
    TEXT("751DE195642892F781731EBD0F9EB3C05F23731CC674F76E7EF335E4F909342A"));
const FString ExpectedManifestSha256(
    TEXT("974583C86473D55CFFE3BF8A57660E787A946CD13E688D5DE5779A962DDC40F7"));
const FString ExpectedFeaturesSha256(
    TEXT("6EE2FAC283BC036F1FB67055D7CB9D2613D00C6B6F267D3329C88CCC5A5D7AE6"));
const FString ExpectedGeometrySha256(
    TEXT("E1C4432097C4360676E96C4B1ED4672FB5C11E37306544697A4576EC01256D62"));
const FString ExpectedContractSha256(
    TEXT("69C53E4E3E772A2DABE4BAF1A5097222B5CA6A7ADE11FBE3CE3544F4F9D61088"));

enum class EV5CMaterialRole : uint8
{
    OfficialWall = 0,
    OfficialRoof = 1,
    FallbackWall = 2,
    FallbackRoof = 3,
};

struct FMaterialSpec
{
    const TCHAR* Name;
    int32 Triangles;
    EV5CMaterialRole Role;
};

// This order is deliberately lexical and is the serialized static-material
// contract. Render sections are remapped to these indices by exact source
// names; source encounter order is not accepted as asset order.
const FMaterialSpec MaterialSpecs[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), 9457, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_COMMERCIAL_HINT"), 2188, EV5CMaterialRole::FallbackWall},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), 14704, EV5CMaterialRole::FallbackWall},
    {TEXT("MAT_HEALTHCARE_HINT"), 278, EV5CMaterialRole::FallbackWall},
    {TEXT("MAT_HOTEL_HINT"), 414, EV5CMaterialRole::OfficialWall},
    {TEXT("MAT_INDUSTRIAL_HINT"), 8, EV5CMaterialRole::FallbackWall},
    {TEXT("MAT_RELIGIOUS_HINT"), 150, EV5CMaterialRole::OfficialWall},
    {TEXT("MAT_RESIDENTIAL_HINT"), 6340, EV5CMaterialRole::FallbackWall},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), 1038, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), 5454, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), 131, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_HOTEL_HINT"), 169, EV5CMaterialRole::OfficialRoof},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), 2, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), 53, EV5CMaterialRole::OfficialRoof},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), 2694, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), 132, EV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_TRANSPORT_HINT"), 332, EV5CMaterialRole::FallbackWall},
};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == ExpectedMaterialCount);

FString CurrentSourcePath(const TCHAR* Filename)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent"),
        Filename));
}

FString ObjSourcePath()
{
    return CurrentSourcePath(
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.obj"));
}

FString MtlSourcePath()
{
    return CurrentSourcePath(
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.mtl"));
}

FString ManifestSourcePath()
{
    return CurrentSourcePath(
        TEXT("IstanaPublicViewV5DSurroundings.manifest.json"));
}

FString FeaturesSourcePath()
{
    return CurrentSourcePath(
        TEXT("IstanaPublicViewV5DSurroundings.features.json"));
}

FString GeometrySourcePath()
{
    return CurrentSourcePath(
        TEXT("IstanaPublicViewV5DSurroundings.geometry.json"));
}

FString ContractSourcePath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
             "istana_public_view_v5d_surroundings.contract.json")));
}

template <typename T>
T* LoadExact(const FString& ObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
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
            TEXT("V5D current-surroundings source byte guard failed for '%s': expected=%lld actual=%d."),
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
        OutError = TEXT("V5D current-surroundings SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5D current-surroundings SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings SHA-256 guard failed for '%s': expected=%s actual=%s."),
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
               ExpectedManifestSha256, OutError) &&
        ValidateSourceHash(
               FeaturesSourcePath(), ExpectedFeaturesBytes,
               ExpectedFeaturesSha256, OutError) &&
        ValidateSourceHash(
               GeometrySourcePath(), ExpectedGeometryBytes,
               ExpectedGeometrySha256, OutError) &&
        ValidateSourceHash(
               ContractSourcePath(), ExpectedContractBytes,
               ExpectedContractSha256, OutError);
}

const TArray<FString>& ExpectedV5CMaterialPaths()
{
    static const TArray<FString> Paths = {
        V5COfficialWallPath,
        V5COfficialRoofPath,
        V5CFallbackWallPath,
        V5CFallbackRoofPath,
    };
    return Paths;
}

bool ValidateV5CMaterialDependencies(
    TArray<UMaterialInstanceConstant*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    // This importer depends only on the four exact persisted material
    // instances. The V5C factory's full cold validator also requires an SM5
    // shader resource, which is unrelated to the asset identity and is absent
    // in a valid SM6-only editor session. Keep this admission platform-neutral
    // and validate the exact paths, classes, packages, cleanliness, and order.
    if (TRIADIstanaExploreV5CSurroundingsAssetFactory::
            GetOrderedMaterialObjectPaths() != ExpectedV5CMaterialPaths())
    {
        OutError = TEXT("The V5C surroundings material dependency order changed.");
        return false;
    }
    for (const FString& Path : ExpectedV5CMaterialPaths())
    {
        UMaterialInstanceConstant* Material =
            LoadExact<UMaterialInstanceConstant>(Path);
        if (!Material ||
            Material->GetClass() != UMaterialInstanceConstant::StaticClass() ||
            !Material->GetOutermost() ||
            !FPackageName::DoesPackageExist(Material->GetOutermost()->GetName()) ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("A required exact, persisted, clean V5C material dependency is invalid: ") +
                Path;
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    if (OutMaterials.Num() != 4 || OutMaterials.Contains(nullptr))
    {
        OutError = TEXT("The exact four-material V5C dependency roster is incomplete.");
        OutMaterials.Reset();
        return false;
    }
    OutError.Reset();
    return true;
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
    // Independently owned landmark packages live below this namespace.
    // Canonical-mesh cardinality applies only to direct children.
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, false, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("V5D current-surroundings Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
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
    const TArray<FString> Expected = {MeshObjectPath};
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings root must contain exactly one mesh asset; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(MeshObjectPath);
        if (!Mesh || !Mesh->GetOutermost() ||
            !FPackageName::DoesPackageExist(Mesh->GetOutermost()->GetName()) ||
            Mesh->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The exact V5D current-surroundings mesh is not persisted and clean.");
            return false;
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
            Error += TEXT(" V5D_CURRENT_SURROUNDINGS_ROLLBACK_INCOMPLETE");
        }
    }

    void Commit() { bCommitted = true; }

private:
    FString& Error;
    bool bCommitted = false;
};

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
    // The source contract is explicitly metre-valued while Unreal stores
    // centimetres. Bake only this unit conversion during import; persisted
    // source-model BuildScale3D remains identity.
    Options->StaticMeshImportData->ImportUniformScale = 100.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    // UV0 is source-authored in metres. No generated light-map channel is
    // allowed to obscure the one-channel provenance contract.
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ComputeNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
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
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 100.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ComputeNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        !Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The exact V5D current-surroundings identity OBJ import policy changed.");
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
        // UE's OBJ interchange path mirrors source Y while converting from
        // right- to left-handed coordinates. The generator already emits the
        // admitted Istana-local Unreal frame, so bake the inverse reflection
        // into the source description. ApplyTransform also reverses polygon
        // facing and correctly transforms normals/tangents for this mirror.
        if (FMeshDescription* Description = Mesh->GetMeshDescription(0))
        {
            const FTransform RestoreIstanaLocalHandedness(
                FQuat::Identity,
                FVector::ZeroVector,
                FVector(1.0, -1.0, 1.0));
            FStaticMeshOperations::ApplyTransform(
                *Description,
                RestoreIstanaLocalHandedness,
                true);
            Mesh->CommitMeshDescription(0);
        }
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
        SourceModel.BuildSettings.bGenerateLightmapUVs = false;
        SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
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
        // With an empty simple aggregate this prevents the source triangle
        // mesh from becoming query collision if a future component policy is
        // accidentally loosened.
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
}

int32 CountProvenanceObjects(const UStaticMesh* Mesh)
{
    const TArray<UAssetUserData*>* UserData =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    int32 Count = 0;
    if (UserData)
    {
        for (const UAssetUserData* Datum : *UserData)
        {
            Count += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                    StaticClass());
        }
    }
    return Count;
}

bool ValidateProvenance(const UStaticMesh* Mesh, FString& OutError)
{
    const TArray<UAssetUserData*>* UserData =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    const UTRIADIstanaExploreV5DCurrentSurroundingsProvenance* Provenance =
        nullptr;
    int32 MatchingCount = 0;
    if (UserData)
    {
        for (const UAssetUserData* Datum : *UserData)
        {
            if (Datum && Datum->IsA(
                    UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                        StaticClass()))
            {
                ++MatchingCount;
                Provenance = Cast<
                    UTRIADIstanaExploreV5DCurrentSurroundingsProvenance>(
                        Datum);
            }
        }
    }
    if (!Mesh || MatchingCount != 1 || !Provenance ||
        Provenance->GetClass() !=
            UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                StaticClass() ||
        Provenance->GetOuter() != Mesh ||
        Provenance->HasAnyFlags(RF_Transient) ||
        !Provenance->IsCanonicalContract())
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings mesh must own exactly one non-transient exact-class cooked provenance contract; matches=%d."),
            MatchingCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool StampCanonicalProvenance(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh)
    {
        OutError = TEXT("Cannot stamp V5D current-surroundings provenance on a null mesh.");
        return false;
    }
    Mesh->Modify();
    const int32 ExistingCount = CountProvenanceObjects(Mesh);
    for (int32 Index = 0; Index < ExistingCount; ++Index)
    {
        Mesh->RemoveUserDataOfClass(
            UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                StaticClass());
    }
    if (CountProvenanceObjects(Mesh) != 0)
    {
        OutError = TEXT("Could not deterministically remove every prior V5D current-surroundings provenance object.");
        return false;
    }
    UTRIADIstanaExploreV5DCurrentSurroundingsProvenance* Provenance =
        NewObject<UTRIADIstanaExploreV5DCurrentSurroundingsProvenance>(
            Mesh, NAME_None, RF_Transactional);
    if (!Provenance)
    {
        OutError = TEXT("Could not allocate the exact V5D current-surroundings provenance object.");
        return false;
    }
    Provenance->SetCanonicalContract();
    Mesh->AddAssetUserData(Provenance);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    if (!ValidateProvenance(Mesh, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool NormalizeAndBindMaterials(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& V5CMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || V5CMaterials.Num() != 4 || V5CMaterials.Contains(nullptr) ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("V5D current-surroundings normalization requires the exact 17-slot/17-section imported mesh and four V5C materials.");
        return false;
    }

    TMap<FName, int32> CanonicalOrder;
    for (int32 CanonicalIndex = 0;
         CanonicalIndex < ExpectedMaterialCount;
         ++CanonicalIndex)
    {
        const FName Name(MaterialSpecs[CanonicalIndex].Name);
        if (Name.IsNone() || CanonicalOrder.Contains(Name))
        {
            OutError = TEXT("The V5D current-surroundings canonical material roster is ambiguous.");
            return false;
        }
        CanonicalOrder.Add(Name, CanonicalIndex);
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
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
                TEXT("V5D current-surroundings imported slot %d is not an exact unique semantic-name permutation: slot='%s' imported='%s'."),
                ImportedIndex,
                *Imported.MaterialSlotName.ToString(),
                *Imported.ImportedMaterialSlotName.ToString());
            return false;
        }
        const int32 DependencyIndex =
            static_cast<int32>(MaterialSpecs[*CanonicalIndex].Role);
        if (!V5CMaterials.IsValidIndex(DependencyIndex))
        {
            OutError = TEXT("A V5D current-surroundings semantic role has no V5C material binding.");
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
            V5CMaterials[DependencyIndex];
    }

    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    TSet<int32> SeenSections;
    TArray<FString> Digest;
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
            OutError = TEXT("A V5D current-surroundings section references an invalid imported material index.");
            return false;
        }
        const int32 CanonicalIndex = ImportedToCanonical[ImportedIndex];
        if (SeenSections.Contains(CanonicalIndex) ||
            RenderSection.NumTriangles !=
                MaterialSpecs[CanonicalIndex].Triangles)
        {
            OutError = FString::Printf(
                TEXT("V5D current-surroundings semantic section census changed: expected='%s':%d actual=%u sections=[%s]."),
                MaterialSpecs[CanonicalIndex].Name,
                MaterialSpecs[CanonicalIndex].Triangles,
                RenderSection.NumTriangles,
                *FString::Join(Digest, TEXT(",")));
            return false;
        }
        SeenSections.Add(CanonicalIndex);
        Section.MaterialIndex = CanonicalIndex;
        Original.MaterialIndex = CanonicalIndex;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    if (SeenSections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("The V5D current-surroundings import omitted one or more semantic sections.");
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
    const FMeshDescription& Description,
    const FStaticMeshLODResources& Lod,
    FString& OutError)
{
    const FStaticMeshConstAttributes Attributes(Description);
    const TVertexInstanceAttributesConstRef<FVector2f> Uvs =
        Attributes.GetVertexInstanceUVs();
    const FStaticMeshVertexBuffer& RenderBuffer =
        Lod.VertexBuffers.StaticMeshVertexBuffer;
    if (Description.VertexInstances().Num() != ExpectedVertexInstanceCount ||
        Uvs.GetNumElements() != ExpectedVertexInstanceCount ||
        Uvs.GetNumChannels() != 1 ||
        RenderBuffer.GetNumVertices() == 0 ||
        RenderBuffer.GetNumTexCoords() != 1)
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings source UV0 structure changed: vertexInstances=%d uvElements=%d uvChannels=%d renderUvChannels=%d."),
            Description.VertexInstances().Num(),
            Uvs.GetNumElements(),
            Uvs.GetNumChannels(),
            RenderBuffer.GetNumTexCoords());
        return false;
    }

    FVector2f Minimum(FLT_MAX, FLT_MAX);
    FVector2f Maximum(-FLT_MAX, -FLT_MAX);
    for (const FVertexInstanceID VertexInstanceId :
         Description.VertexInstances().GetElementIDs())
    {
        const FVector2f Uv = Uvs.Get(VertexInstanceId, 0);
        if (!FMath::IsFinite(Uv.X) || !FMath::IsFinite(Uv.Y))
        {
            OutError = TEXT("V5D current-surroundings source UV0 contains a non-finite value.");
            return false;
        }
        Minimum.X = FMath::Min(Minimum.X, Uv.X);
        Minimum.Y = FMath::Min(Minimum.Y, Uv.Y);
        Maximum.X = FMath::Max(Maximum.X, Uv.X);
        Maximum.Y = FMath::Max(Maximum.Y, Uv.Y);
    }
    // UE's OBJ importer performs its stable V -> 1-V conversion. These are
    // therefore the exact admitted source-metre bounds after that conversion.
    if (!FMath::IsNearlyEqual(Minimum.X, -984.383058f, 0.002f) ||
        !FMath::IsNearlyEqual(Minimum.Y, -992.398856f, 0.002f) ||
        !FMath::IsNearlyEqual(Maximum.X, 998.751074f, 0.002f) ||
        !FMath::IsNearlyEqual(Maximum.Y, 999.319761f, 0.002f))
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings source-metre UV0 bounds drifted: min=(%.6f,%.6f) max=(%.6f,%.6f)."),
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
    FString& OutError)
{
    if (Description.Vertices().Num() != ExpectedSourceVertexCount ||
        Description.VertexInstances().Num() != ExpectedVertexInstanceCount ||
        Description.Triangles().Num() != ExpectedTriangleCount ||
        Description.PolygonGroups().Num() != ExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings source mesh-description census changed: vertices=%d vertexInstances=%d triangles=%d polygonGroups=%d."),
            Description.Vertices().Num(),
            Description.VertexInstances().Num(),
            Description.Triangles().Num(),
            Description.PolygonGroups().Num());
        return false;
    }

    const FStaticMeshConstAttributes Attributes(Description);
    const TPolygonGroupAttributesConstRef<FName> PolygonGroupSlots =
        Attributes.GetPolygonGroupMaterialSlotNames();
    TMap<FName, int32> CanonicalIndices;
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        CanonicalIndices.Add(FName(MaterialSpecs[Index].Name), Index);
    }
    TArray<int32> Counts;
    Counts.Init(0, ExpectedMaterialCount);
    TSet<FName> SeenGroups;
    for (const FPolygonGroupID GroupId :
         Description.PolygonGroups().GetElementIDs())
    {
        const FName Slot = PolygonGroupSlots[GroupId];
        const int32* Index = CanonicalIndices.Find(Slot);
        if (!Index || SeenGroups.Contains(Slot))
        {
            OutError = TEXT("V5D current-surroundings source polygon-group names are incomplete or ambiguous.");
            return false;
        }
        SeenGroups.Add(Slot);
    }
    for (const FTriangleID TriangleId :
         Description.Triangles().GetElementIDs())
    {
        const FPolygonGroupID GroupId =
            Description.GetTrianglePolygonGroup(TriangleId);
        const int32* Index = CanonicalIndices.Find(PolygonGroupSlots[GroupId]);
        if (!Index)
        {
            OutError = TEXT("A V5D current-surroundings source triangle has no canonical semantic material group.");
            return false;
        }
        ++Counts[*Index];
    }
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        if (Counts[Index] != MaterialSpecs[Index].Triangles)
        {
            OutError = FString::Printf(
                TEXT("V5D current-surroundings source triangle census drifted for '%s': expected=%d actual=%d."),
                MaterialSpecs[Index].Name,
                MaterialSpecs[Index].Triangles,
                Counts[Index]);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMesh(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& V5CMaterials,
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
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != MeshObjectPath || Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        Mesh->GetNumSourceModels() != 1 || !Description ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        Mesh->GetLightMapCoordinateIndex() != 0 ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() !=
            ExpectedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() !=
            ExpectedMaterialCount ||
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
        OutError = TEXT("V5D current-surroundings mesh lost its exact source, identity scale, one-source/one-LOD structure, source-only UV0 policy, 43,544-triangle/17-slot census, or full-fidelity Nanite/fallback state.");
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (!Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = TEXT("V5D current-surroundings render mesh must have an empty simple aggregate, simple-as-complex zero-collision policy, and no navigation data.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-98438.3058, -99831.9761, 0.0), 0.25) ||
        !BoundsMax.Equals(
            FVector(99875.1074, 99339.8856, 15200.0), 0.25))
    {
        OutError = FString::Printf(
            TEXT("V5D current-surroundings imported centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }
    if (!ValidateMeshDescriptionCensus(*Description, OutError) ||
        !ValidateUv0(*Description, RenderData->LODResources[0], OutError) ||
        !ValidateProvenance(Mesh, OutError))
    {
        return false;
    }

    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    TSet<int32> SeenSectionMaterials;
    TArray<FString> SectionDigest;
    for (int32 SectionIndex = 0;
         SectionIndex < RenderData->LODResources[0].Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& Section =
            RenderData->LODResources[0].Sections[SectionIndex];
        SectionDigest.Add(FString::Printf(
            TEXT("%d:%d:%u"),
            SectionIndex,
            Section.MaterialIndex,
            Section.NumTriangles));
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedMaterialCount ||
            SeenSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                MaterialSpecs[Section.MaterialIndex].Triangles ||
            Mesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Section.MaterialIndex ||
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Section.MaterialIndex)
        {
            OutError = FString::Printf(
                TEXT("V5D current-surroundings render section census/mapping drifted: [%s]."),
                *FString::Join(SectionDigest, TEXT(",")));
            return false;
        }
        SeenSectionMaterials.Add(Section.MaterialIndex);
    }
    if (SeenSectionMaterials.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("V5D current-surroundings render section roster is incomplete.");
        return false;
    }

    for (int32 Slot = 0; Slot < ExpectedMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial = Materials[Slot];
        const int32 DependencyIndex =
            static_cast<int32>(MaterialSpecs[Slot].Role);
        if (!V5CMaterials.IsValidIndex(DependencyIndex) ||
            StaticMaterial.MaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            StaticMaterial.MaterialInterface !=
                V5CMaterials[DependencyIndex])
        {
            OutError = FString::Printf(
                TEXT("V5D current-surroundings canonical material slot/order/binding drifted at %d ('%s')."),
                Slot,
                MaterialSpecs[Slot].Name);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    TArray<UMaterialInstanceConstant*> V5CMaterials;
    if (!ValidateAllSourceHashes(OutReport) ||
        !ValidateV5CMaterialDependencies(V5CMaterials, OutReport) ||
        !ValidateExactRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateMesh(
            LoadExact<UStaticMesh>(MeshObjectPath),
            V5CMaterials,
            OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_CURRENT_SURROUNDINGS_ASSET_VALID assets=%d sourceDate=2026-08-31 sourceVertices=%d sourceVertexInstances=%d meshTriangles=%d materialSlots=%d polygonParts=1391 selectedFeatures=1389 wallTriangles=24414 roofTriangles=9673 bottomTriangles=9457 objBytes=%lld objSha256=%s mtlSha256=%s manifestSha256=%s featuresSha256=%s geometrySha256=%s contractSha256=%s canonicalLexicalSlots=true v5cBindings=officialHotelReligious+fallbackAllOtherRoles sourceUv0Channels=1 generatedLightmapUv=false fullPrecisionUv=true naniteFullMesh=true cookedProvenanceObjects=1 cookedOutputSetSha256=27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD renderOnly=true zeroCollision=true noNavigationData=true noActorOrMapChange=true publicVolunteeredApproximation=true notSurveyAsBuiltOrHyperreal=true noSensorPropagationRfMaterialOrRfOcclusionAuthority=true."),
        ExpectedAssetCount,
        ExpectedSourceVertexCount,
        ExpectedVertexInstanceCount,
        ExpectedTriangleCount,
        ExpectedMaterialCount,
        ExpectedObjBytes,
        *ExpectedObjSha256,
        *ExpectedMtlSha256,
        *ExpectedManifestSha256,
        *ExpectedFeaturesSha256,
        *ExpectedGeometrySha256,
        *ExpectedContractSha256);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }

const FString& GetSurroundingsMeshObjectPath() { return MeshObjectPath; }

bool CreateFreshAsset(UStaticMesh*& OutAsset, FString& OutError)
{
    OutAsset = nullptr;
    if (!ValidateAllSourceHashes(OutError))
    {
        return false;
    }
    TArray<UMaterialInstanceConstant*> V5CMaterials;
    if (!ValidateV5CMaterialDependencies(V5CMaterials, OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty())
    {
        OutError = TEXT("CreateFreshAsset requires an empty exact V5D current-surroundings asset root.");
        return false;
    }

    FScopedFreshRollback Rollback(OutError);
    UAssetImportTask* Task = MakeImportTask();
    if (!Task || !ValidateImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate the exact V5D current-surroundings OBJ import task.");
        }
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    AssetTools.ImportAssetTasks({Task});
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == MeshObjectPath)
        {
            OutAsset = Cast<UStaticMesh>(Object);
        }
    }
    if (!OutAsset)
    {
        OutAsset = LoadExact<UStaticMesh>(MeshObjectPath);
    }
    MakeRenderOnly(OutAsset);
    if (!NormalizeAndBindMaterials(
            OutAsset, V5CMaterials, OutError) ||
        !StampCanonicalProvenance(OutAsset, OutError) ||
        !ValidateMesh(OutAsset, V5CMaterials, OutError))
    {
        OutAsset = nullptr;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString PreSaveReport;
    if (!ValidateInternal(false, PreSaveReport))
    {
        OutError = TEXT("Fresh V5D current-surroundings validation failed before save: ") +
            PreSaveReport;
        OutAsset = nullptr;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateAsset(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory
