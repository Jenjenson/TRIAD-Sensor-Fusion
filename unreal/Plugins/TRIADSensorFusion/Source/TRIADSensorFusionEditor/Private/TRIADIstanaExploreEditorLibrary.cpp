#include "TRIADIstanaExploreEditorLibrary.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialInterface.h"
#include "MeshReductionSettings.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreLandscapeActor.h"
#include "TRIADIstanaFreeRoamPawn.h"
#include "TRIADIstanaPublicViewHeroV5EditorLibrary.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v5"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v1"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v1.Istana_PublicView_Explore_v1"));
const FString ExploreGameModeClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreGameMode"));
const FString BroadleafPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation"));
const FString BroadleafName(
    TEXT("SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FString BroadleafObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FString BroadleafTrunkMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_trunk.jacaranda_tree_trunk"));
const FString BroadleafBranchMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_branches.jacaranda_tree_branches"));
const FString BroadleafLeafMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves.jacaranda_tree_leaves"));
const FString BroadleafLeafAlphaTextureObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_alpha_1k.jacaranda_tree_leaves_alpha_1k"));
const FName BroadleafTrunkSlotName(TEXT("jacaranda_tree_trunk"));
const FName BroadleafBranchSlotName(TEXT("jacaranda_tree_branches"));
const FName BroadleafLeafSlotName(TEXT("jacaranda_tree_leaves"));
constexpr float BroadleafLeafOpacityMaskClipValue = 0.333f;
const FString BroadleafTextureSourceRelativeDirectory(
    TEXT("Sources/PolyHaven/jacaranda_tree_1k/textures"));

enum class EBroadleafTextureUsage : uint8
{
    Diffuse,
    NormalOpenGl,
    Roughness,
    AlphaMask
};

struct FBroadleafTextureSpec
{
    const TCHAR* AssetName;
    int64 ExpectedBytes;
    const TCHAR* ExpectedMd5;
    EBroadleafTextureUsage Usage;
};

const TArray<FBroadleafTextureSpec>& BroadleafTextureSpecs()
{
    static const TArray<FBroadleafTextureSpec> Specs = {
        {TEXT("jacaranda_tree_branches_diff_1k"), 5223386, TEXT("ad4488598993a114236807d60136fc7b"), EBroadleafTextureUsage::Diffuse},
        {TEXT("jacaranda_tree_branches_nor_gl_1k"), 5400459, TEXT("df3d91a9c29266f44b0b17da135a2899"), EBroadleafTextureUsage::NormalOpenGl},
        {TEXT("jacaranda_tree_branches_rough_1k"), 1625206, TEXT("8bd9e066f93c242ef468c7c6ea6dc382"), EBroadleafTextureUsage::Roughness},
        {TEXT("jacaranda_tree_leaves_alpha_1k"), 506043, TEXT("740214de7b900e30737009ff4ee2c01a"), EBroadleafTextureUsage::AlphaMask},
        {TEXT("jacaranda_tree_leaves_diff_1k"), 2364526, TEXT("879ef43acb3443e247a2d32713fec86c"), EBroadleafTextureUsage::Diffuse},
        {TEXT("jacaranda_tree_leaves_nor_gl_1k"), 4154090, TEXT("c968c79960303470e124875a111bd5a4"), EBroadleafTextureUsage::NormalOpenGl},
        {TEXT("jacaranda_tree_leaves_rough_1k"), 992522, TEXT("4cbffd1596f7ad63de8e170ac54fb907"), EBroadleafTextureUsage::Roughness},
        {TEXT("jacaranda_tree_trunk_diff_1k"), 5450061, TEXT("eb97b09e36a6683ab5c15496d08a65b2"), EBroadleafTextureUsage::Diffuse},
        {TEXT("jacaranda_tree_trunk_nor_gl_1k"), 5486270, TEXT("98bae75f6c5805b4fd22907d74c7d2c0"), EBroadleafTextureUsage::NormalOpenGl},
        {TEXT("jacaranda_tree_trunk_rough_1k"), 1747236, TEXT("d3682e710029d485344b9c2c5f819e37"), EBroadleafTextureUsage::Roughness}};
    return Specs;
}

struct FBroadleafMaterialSpec
{
    FName SlotName;
    const TCHAR* AssetName;
    const TCHAR* DiffuseTextureName;
    const TCHAR* NormalTextureName;
    const TCHAR* RoughnessTextureName;
    bool bLeaves;
};

const TArray<FBroadleafMaterialSpec>& BroadleafMaterialSpecs()
{
    static const TArray<FBroadleafMaterialSpec> Specs = {
        {BroadleafTrunkSlotName, TEXT("jacaranda_tree_trunk"), TEXT("jacaranda_tree_trunk_diff_1k"), TEXT("jacaranda_tree_trunk_nor_gl_1k"), TEXT("jacaranda_tree_trunk_rough_1k"), false},
        {BroadleafBranchSlotName, TEXT("jacaranda_tree_branches"), TEXT("jacaranda_tree_branches_diff_1k"), TEXT("jacaranda_tree_branches_nor_gl_1k"), TEXT("jacaranda_tree_branches_rough_1k"), false},
        {BroadleafLeafSlotName, TEXT("jacaranda_tree_leaves"), TEXT("jacaranda_tree_leaves_diff_1k"), TEXT("jacaranda_tree_leaves_nor_gl_1k"), TEXT("jacaranda_tree_leaves_rough_1k"), true}};
    return Specs;
}
const FString GrassName(TEXT("SM_IstanaPublicViewExploreV1_GrassClump"));
const FString GrassObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_GrassClump.SM_IstanaPublicViewExploreV1_GrassClump"));
const FString SharedLawnMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Lawn.M_IPV_Lawn"));
const FString ExploreLawnMaterialName(TEXT("M_IPVExplore_Lawn"));
const FString ExploreLawnMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/M_IPVExplore_Lawn.M_IPVExplore_Lawn"));
const FName GeneratedMeshUvMetadataSchemaKey(
    TEXT("TRIAD.IstanaExploreV1.GeneratedMeshUvMetadataSchema"));
const FString BroadleafUvMetadataSchemaValue(
    TEXT("schema=1;asset=SM_IstanaPublicViewExploreV1_Broadleaf_A;slots=3;source_lods=4;uv_density=render_derived"));
const FString GrassUvMetadataSchemaValue(
    TEXT("schema=1;asset=SM_IstanaPublicViewExploreV1_GrassClump;slots=1;source_lods=1;uv_density=render_derived"));
constexpr int32 BroadleafSourceLodCount = 4;
constexpr int32 BroadleafRuntimeMinLod = 1;

struct FBroadleafRuntimeLodSpec
{
    int32 Index;
    float PercentTriangles;
    uint32 MaximumTriangles;
    float ScreenSize;
};

const TArray<FBroadleafRuntimeLodSpec>& BroadleafRuntimeLodSpecs()
{
    static const TArray<FBroadleafRuntimeLodSpec> Specs = {
        {1, 0.08f, 350000u, 1.0f},
        {2, 0.03f, 120000u, 0.32f},
        {3, 0.01f, 40000u, 0.12f}};
    return Specs;
}
const FString ShrubAPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_A.SM_Urb_Str_Shrub_Common_Set_01_A"));
const FString ShrubBPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_D.SM_Urb_Str_Shrub_Common_Set_01_D"));
const FString GroundcoverPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_H.SM_Urb_Str_Shrub_Common_Set_01_H"));
const FString CylinderPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
const FName ExploreLandscapeTag(TEXT("TRIADIstanaExploreLandscapeV1"));
const FName ExplorePlayerStartTag(TEXT("TRIADIstanaExplorePlayerStartV1"));
constexpr double InnerLegacyTreeRadiusCentimeters = 25000.0;
constexpr int32 ExpectedPreservedOuterLegacyTrees = 560;

struct FProtectedMapBytes
{
    FString PackageName;
    FString Filename;
    TArray<uint8> Bytes;
};

struct FDistantContextSnapshot
{
    FString MeshPath;
    TArray<FString> MaterialPaths;
    FTransform RelativeTransform = FTransform::Identity;
    FTransform WorldTransform = FTransform::Identity;
    ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
    TArray<TEnumAsByte<ECollisionResponse>> CollisionResponses;
    bool bVisible = false;
    bool bHiddenInGame = true;
    bool bActive = false;
    bool bAutoActivate = false;
    bool bGenerateOverlapEvents = true;
};

const TArray<FString>& ProtectedMapPackages()
{
    static const TArray<FString> Packages = {
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v3"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v4"),
        SourceMapPackage};
    return Packages;
}

bool CaptureProtectedMapBytes(
    TArray<FProtectedMapBytes>& OutRecords,
    FString& OutError)
{
    OutRecords.Reset();
    for (const FString& PackageName : ProtectedMapPackages())
    {
        FProtectedMapBytes Record;
        Record.PackageName = PackageName;
        if (!FPackageName::DoesPackageExist(PackageName, &Record.Filename) ||
            !FFileHelper::LoadFileToArray(Record.Bytes, *Record.Filename) ||
            Record.Bytes.IsEmpty())
        {
            OutError = TEXT("Could not capture exact protected V1-V5 map bytes for ") +
                PackageName;
            return false;
        }
        OutRecords.Add(MoveTemp(Record));
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedMapBytesUnchanged(
    const TArray<FProtectedMapBytes>& Records,
    FString& OutError)
{
    if (Records.Num() != ProtectedMapPackages().Num())
    {
        OutError = TEXT("The V1-V5 protected-map byte roster changed.");
        return false;
    }
    for (const FProtectedMapBytes& Record : Records)
    {
        FString Filename;
        TArray<uint8> CurrentBytes;
        if (!ProtectedMapPackages().Contains(Record.PackageName) ||
            !FPackageName::DoesPackageExist(Record.PackageName, &Filename) ||
            !FPaths::IsSamePath(Filename, Record.Filename) ||
            !FFileHelper::LoadFileToArray(CurrentBytes, *Filename) ||
            CurrentBytes != Record.Bytes)
        {
            OutError = TEXT("A protected V1-V5 map changed during Explore creation: ") +
                Record.PackageName;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool SetCameraAutoActivationDisabled(
    ACameraActor* Camera,
    FString& OutError)
{
    FByteProperty* Property = Camera
        ? FindFProperty<FByteProperty>(
            ACameraActor::StaticClass(),
            TEXT("AutoActivateForPlayer"))
        : nullptr;
    if (!Camera || !Property)
    {
        OutError = TEXT("UE 5.5 camera auto-activation property is unavailable.");
        return false;
    }
    Property->SetPropertyValue_InContainer(
        Camera,
        static_cast<uint8>(EAutoReceiveInput::Disabled));
    OutError.Reset();
    return true;
}

bool IsCameraAutoActivationDisabled(const ACameraActor* Camera)
{
    const FByteProperty* Property = Camera
        ? FindFProperty<FByteProperty>(
            ACameraActor::StaticClass(),
            TEXT("AutoActivateForPlayer"))
        : nullptr;
    return Property &&
        Property->GetPropertyValue_InContainer(Camera) ==
            static_cast<uint8>(EAutoReceiveInput::Disabled);
}

bool CaptureDistantContext(
    const UStaticMeshComponent* Component,
    FDistantContextSnapshot& OutSnapshot,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh())
    {
        OutError = TEXT("The distant OSM/HDB component or mesh is absent.");
        return false;
    }
    OutSnapshot.MeshPath = Component->GetStaticMesh()->GetPathName();
    OutSnapshot.MaterialPaths.Reset();
    for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material)
        {
            OutError = TEXT("The distant OSM/HDB component has a null material.");
            return false;
        }
        OutSnapshot.MaterialPaths.Add(Material->GetPathName());
    }
    OutSnapshot.RelativeTransform = Component->GetRelativeTransform();
    OutSnapshot.WorldTransform = Component->GetComponentTransform();
    OutSnapshot.CollisionEnabled = Component->GetCollisionEnabled();
    OutSnapshot.CollisionResponses.Reset();
    for (int32 Channel = 0;
         Channel <= static_cast<int32>(ECC_GameTraceChannel18);
         ++Channel)
    {
        OutSnapshot.CollisionResponses.Add(
            Component->GetCollisionResponseToChannel(
                static_cast<ECollisionChannel>(Channel)));
    }
    OutSnapshot.bVisible = Component->IsVisible();
    OutSnapshot.bHiddenInGame = Component->bHiddenInGame;
    OutSnapshot.bActive = Component->IsActive();
    OutSnapshot.bAutoActivate = Component->bAutoActivate;
    OutSnapshot.bGenerateOverlapEvents = Component->GetGenerateOverlapEvents();
    OutError.Reset();
    return true;
}

bool AreDistantContextsIdentical(
    const FDistantContextSnapshot& Expected,
    const FDistantContextSnapshot& Actual,
    FString& OutError)
{
    if (Expected.MeshPath != Actual.MeshPath ||
        Expected.MaterialPaths != Actual.MaterialPaths ||
        !Expected.RelativeTransform.Equals(Actual.RelativeTransform, 0.001f) ||
        !Expected.WorldTransform.Equals(Actual.WorldTransform, 0.001f) ||
        Expected.CollisionEnabled != Actual.CollisionEnabled ||
        Expected.CollisionResponses != Actual.CollisionResponses ||
        Expected.bVisible != Actual.bVisible ||
        Expected.bHiddenInGame != Actual.bHiddenInGame ||
        Expected.bActive != Actual.bActive ||
        Expected.bAutoActivate != Actual.bAutoActivate ||
        Expected.bGenerateOverlapEvents != Actual.bGenerateOverlapEvents)
    {
        OutError = TEXT("The duplicated distant OSM/HDB mesh, materials, transform, activation, visibility, or collision state differs from V5.");
        return false;
    }
    OutError.Reset();
    return true;
}

FString ProjectSourcePath(const TCHAR* Relative)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("SourceAssets/IstanaPublicViewExploreV1"), Relative));
}

bool HasDirtyPackages(FString& OutError)
{
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (!DirtyMaps.IsEmpty() || !DirtyContent.IsEmpty())
    {
        OutError = TEXT("Close or save all dirty map/content packages before additive Explore migration.");
        return true;
    }
    OutError.Reset();
    return false;
}

template <typename T>
T* LoadExact(const FString& ObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

UStaticMesh* ImportedMeshFromTask(
    UAssetImportTask* Task,
    const FString& ExactObjectPath)
{
    if (Task)
    {
        for (UObject* Object : Task->GetObjects())
        {
            if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
            {
                if (Mesh->GetPathName() == ExactObjectPath)
                {
                    return Mesh;
                }
            }
        }
    }
    return LoadExact<UStaticMesh>(ExactObjectPath);
}

struct FBroadleafMaterialRoster
{
    UMaterial* Trunk = nullptr;
    UMaterial* Branches = nullptr;
    UMaterial* Leaves = nullptr;
};

FString BroadleafTextureSourcePath(const FBroadleafTextureSpec& Spec)
{
    return ProjectSourcePath(*FPaths::Combine(
        BroadleafTextureSourceRelativeDirectory,
        FString(Spec.AssetName) + TEXT(".png")));
}

FString BroadleafTextureObjectPath(const FBroadleafTextureSpec& Spec)
{
    return FString::Printf(
        TEXT("%s/%s.%s"), *BroadleafPath, Spec.AssetName, Spec.AssetName);
}

FString BroadleafMaterialObjectPath(const FBroadleafMaterialSpec& Spec)
{
    return FString::Printf(
        TEXT("%s/%s.%s"), *BroadleafPath, Spec.AssetName, Spec.AssetName);
}

const FBroadleafTextureSpec* FindBroadleafTextureSpec(const TCHAR* AssetName)
{
    return BroadleafTextureSpecs().FindByPredicate(
        [AssetName](const FBroadleafTextureSpec& Spec)
        {
            return FCString::Strcmp(Spec.AssetName, AssetName) == 0;
        });
}

bool ValidateBroadleafTextureSourceRoster(FString& OutError)
{
    if (BroadleafTextureSpecs().Num() != 10)
    {
        OutError = TEXT("The locked Poly Haven PNG roster must contain exactly ten files.");
        return false;
    }
    TSet<FString> UniqueNames;
    for (const FBroadleafTextureSpec& Spec : BroadleafTextureSpecs())
    {
        const FString Source = BroadleafTextureSourcePath(Spec);
        const FMD5Hash Hash = FMD5Hash::HashFile(*Source);
        if (UniqueNames.Contains(Spec.AssetName) ||
            IFileManager::Get().FileSize(*Source) != Spec.ExpectedBytes ||
            !Hash.IsValid() ||
            !LexToString(Hash).Equals(Spec.ExpectedMd5, ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT("Locked Poly Haven PNG is missing or changed: %s"),
                *Source);
            return false;
        }
        UniqueNames.Add(Spec.AssetName);
    }
    OutError.Reset();
    return true;
}

bool BroadleafTextureIsSrgb(EBroadleafTextureUsage Usage)
{
    return Usage == EBroadleafTextureUsage::Diffuse;
}

TextureCompressionSettings BroadleafTextureCompression(
    EBroadleafTextureUsage Usage)
{
    if (Usage == EBroadleafTextureUsage::NormalOpenGl)
    {
        return TC_Normalmap;
    }
    if (Usage == EBroadleafTextureUsage::Roughness ||
        Usage == EBroadleafTextureUsage::AlphaMask)
    {
        return TC_Masks;
    }
    return TC_Default;
}

TextureGroup BroadleafTextureGroup(EBroadleafTextureUsage Usage)
{
    if (Usage == EBroadleafTextureUsage::NormalOpenGl)
    {
        return TEXTUREGROUP_WorldNormalMap;
    }
    if (Usage == EBroadleafTextureUsage::Roughness ||
        Usage == EBroadleafTextureUsage::AlphaMask)
    {
        return TEXTUREGROUP_WorldSpecular;
    }
    return TEXTUREGROUP_World;
}

bool ValidateBroadleafTexture(
    UTexture2D* Texture,
    const FBroadleafTextureSpec& Spec,
    FString& OutError)
{
    const FString ExpectedSource = BroadleafTextureSourcePath(Spec);
    TArray<FString> ImportedFilenames = Texture && Texture->AssetImportData
        ? Texture->AssetImportData->ExtractFilenames()
        : TArray<FString>();
    FString ActualSource = ImportedFilenames.Num() == 1
        ? FPaths::ConvertRelativePathToFull(ImportedFilenames[0])
        : FString();
    FString NormalizedExpected = FPaths::ConvertRelativePathToFull(ExpectedSource);
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(NormalizedExpected);
    const FMD5Hash CurrentHash = FMD5Hash::HashFile(*NormalizedExpected);
    const FMD5Hash* ImportedHash = Texture && Texture->AssetImportData &&
        Texture->AssetImportData->GetSourceData().SourceFiles.Num() == 1
        ? &Texture->AssetImportData->GetSourceData().SourceFiles[0].FileHash
        : nullptr;
    if (!Texture || Texture->GetPathName() != BroadleafTextureObjectPath(Spec) ||
        Texture->Source.GetSizeX() != 1024 ||
        Texture->Source.GetSizeY() != 1024 ||
        Texture->SRGB != BroadleafTextureIsSrgb(Spec.Usage) ||
        Texture->CompressionSettings != BroadleafTextureCompression(Spec.Usage) ||
        Texture->LODGroup != BroadleafTextureGroup(Spec.Usage) ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        Texture->bFlipGreenChannel !=
            (Spec.Usage == EBroadleafTextureUsage::NormalOpenGl) ||
        Texture->VirtualTextureStreaming || ImportedFilenames.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, NormalizedExpected) ||
        !CurrentHash.IsValid() || !ImportedHash || !ImportedHash->IsValid() ||
        CurrentHash != *ImportedHash)
    {
        OutError = FString::Printf(
            TEXT("Imported locked broadleaf texture is invalid: %s"),
            Spec.AssetName);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ResolveBroadleafMaterialRoster(
    UStaticMesh* Broadleaf,
    FBroadleafMaterialRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FBroadleafMaterialRoster();
    if (!Broadleaf || Broadleaf->GetStaticMaterials().Num() != 3)
    {
        OutError = TEXT("The CC0 broadleaf must retain exactly three imported trunk/branch/leaf material slots.");
        return false;
    }
    for (const FStaticMaterial& Slot : Broadleaf->GetStaticMaterials())
    {
        if (Slot.MaterialSlotName != Slot.ImportedMaterialSlotName)
        {
            OutError = TEXT("A CC0 broadleaf render slot no longer matches its exact imported material slot name.");
            return false;
        }
        UMaterial* Material = Cast<UMaterial>(Slot.MaterialInterface);
        if (Slot.MaterialSlotName == BroadleafTrunkSlotName &&
            Material && Material->GetPathName() == BroadleafTrunkMaterialObjectPath &&
            !OutRoster.Trunk)
        {
            OutRoster.Trunk = Material;
        }
        else if (Slot.MaterialSlotName == BroadleafBranchSlotName &&
            Material && Material->GetPathName() == BroadleafBranchMaterialObjectPath &&
            !OutRoster.Branches)
        {
            OutRoster.Branches = Material;
        }
        else if (Slot.MaterialSlotName == BroadleafLeafSlotName &&
            Material && Material->GetPathName() == BroadleafLeafMaterialObjectPath &&
            !OutRoster.Leaves)
        {
            OutRoster.Leaves = Material;
        }
        else
        {
            OutError = TEXT("The CC0 broadleaf material roster, exact slot names, or exact imported material bindings changed.");
            return false;
        }
    }
    if (!OutRoster.Trunk || !OutRoster.Branches || !OutRoster.Leaves)
    {
        OutError = TEXT("The CC0 broadleaf exact trunk/branch/leaf material roster is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateBroadleafMaterialInput(
    const FExpressionInput& Input,
    UTexture2D* ExpectedTexture,
    int32 ExpectedOutputIndex,
    EMaterialSamplerType ExpectedSampler,
    const TCHAR* Label,
    const UMaterialExpressionTextureSample*& OutSample,
    FString& OutError)
{
    OutSample = Cast<UMaterialExpressionTextureSample>(Input.Expression);
    if (!OutSample || !ExpectedTexture ||
        OutSample->Texture != ExpectedTexture ||
        Input.OutputIndex != ExpectedOutputIndex ||
        OutSample->SamplerType != ExpectedSampler ||
        OutSample->Coordinates.Expression != nullptr ||
        OutSample->SamplerSource != SSM_FromTextureAsset)
    {
        OutError = FString::Printf(
            TEXT("Broadleaf material %s input is not canonical."), Label);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateBroadleafMaterial(
    UMaterial* Material,
    const FBroadleafMaterialSpec& Spec,
    FString& OutError)
{
    const FBroadleafTextureSpec* DiffuseSpec =
        FindBroadleafTextureSpec(Spec.DiffuseTextureName);
    const FBroadleafTextureSpec* NormalSpec =
        FindBroadleafTextureSpec(Spec.NormalTextureName);
    const FBroadleafTextureSpec* RoughnessSpec =
        FindBroadleafTextureSpec(Spec.RoughnessTextureName);
    const FBroadleafTextureSpec* AlphaSpec = Spec.bLeaves
        ? FindBroadleafTextureSpec(TEXT("jacaranda_tree_leaves_alpha_1k"))
        : nullptr;
    UTexture2D* Diffuse = DiffuseSpec
        ? LoadExact<UTexture2D>(BroadleafTextureObjectPath(*DiffuseSpec))
        : nullptr;
    UTexture2D* Normal = NormalSpec
        ? LoadExact<UTexture2D>(BroadleafTextureObjectPath(*NormalSpec))
        : nullptr;
    UTexture2D* Roughness = RoughnessSpec
        ? LoadExact<UTexture2D>(BroadleafTextureObjectPath(*RoughnessSpec))
        : nullptr;
    UTexture2D* Alpha = AlphaSpec
        ? LoadExact<UTexture2D>(BroadleafTextureObjectPath(*AlphaSpec))
        : nullptr;
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionTextureSample* DiffuseSample = nullptr;
    const UMaterialExpressionTextureSample* NormalSample = nullptr;
    const UMaterialExpressionTextureSample* RoughnessSample = nullptr;
    const UMaterialExpressionTextureSample* AlphaSample = nullptr;
    if (!Material || Material->GetPathName() != BroadleafMaterialObjectPath(Spec) ||
        !Material->bUsedWithInstancedStaticMeshes ||
        !EditorOnly || !Diffuse || !Normal || !Roughness ||
        !ValidateBroadleafMaterialInput(
            EditorOnly->BaseColor, Diffuse, 0, SAMPLERTYPE_Color,
            TEXT("BaseColor"), DiffuseSample, OutError) ||
        !ValidateBroadleafMaterialInput(
            EditorOnly->Normal, Normal, 0, SAMPLERTYPE_Normal,
            TEXT("Normal"), NormalSample, OutError) ||
        !ValidateBroadleafMaterialInput(
            EditorOnly->Roughness, Roughness, 1, SAMPLERTYPE_Masks,
            TEXT("Roughness"), RoughnessSample, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Broadleaf material identity/PBR graph is invalid: %s"),
                Spec.AssetName);
        }
        return false;
    }
    TSet<const UMaterialExpression*> ExpectedExpressions = {
        DiffuseSample, NormalSample, RoughnessSample};
    if (Spec.bLeaves)
    {
        if (!Alpha ||
            !ValidateBroadleafMaterialInput(
                EditorOnly->OpacityMask, Alpha, 1, SAMPLERTYPE_Masks,
                TEXT("OpacityMask"), AlphaSample, OutError) ||
            EditorOnly->SubsurfaceColor.Expression != DiffuseSample ||
            EditorOnly->SubsurfaceColor.OutputIndex != 0 ||
            Material->GetBlendMode() != BLEND_Masked ||
            !Material->IsTwoSided() ||
            !Material->GetShadingModels().HasOnlyShadingModel(
                MSM_TwoSidedFoliage) ||
            !FMath::IsNearlyEqual(
                Material->GetOpacityMaskClipValue(),
                BroadleafLeafOpacityMaskClipValue))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("The canonical jacaranda leaf alpha/two-sided-foliage graph is invalid.");
            }
            return false;
        }
        ExpectedExpressions.Add(AlphaSample);
    }
    else if (EditorOnly->OpacityMask.Expression ||
        EditorOnly->SubsurfaceColor.Expression ||
        Material->GetBlendMode() != BLEND_Opaque || Material->IsTwoSided() ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit))
    {
        OutError = TEXT("The canonical trunk/branch PBR material policy changed.");
        return false;
    }
    if (Material->MaterialDomain != MD_Surface || Material->bUseMaterialAttributes ||
        EditorOnly->Opacity.Expression || EditorOnly->Metallic.Expression ||
        EditorOnly->EmissiveColor.Expression ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedExpressions.Num())
    {
        OutError = TEXT("The canonical broadleaf material gained non-contract graph state.");
        return false;
    }
    for (const UMaterialExpression* Expression :
        EditorOnly->ExpressionCollection.Expressions)
    {
        if (!ExpectedExpressions.Contains(Expression))
        {
            OutError = TEXT("The canonical broadleaf material contains an unexpected expression.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateBroadleafMaterialRoster(
    UStaticMesh* Broadleaf,
    FString& OutError)
{
    FBroadleafMaterialRoster Roster;
    if (!ResolveBroadleafMaterialRoster(Broadleaf, Roster, OutError))
    {
        return false;
    }
    for (const FBroadleafMaterialSpec& Spec : BroadleafMaterialSpecs())
    {
        UMaterial* Material = Spec.SlotName == BroadleafTrunkSlotName
            ? Roster.Trunk
            : (Spec.SlotName == BroadleafBranchSlotName
                ? Roster.Branches
                : Roster.Leaves);
        if (!ValidateBroadleafMaterial(Material, Spec, OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateBroadleafTextureRoster(FString& OutError)
{
    if (!ValidateBroadleafTextureSourceRoster(OutError))
    {
        return false;
    }
    for (const FBroadleafTextureSpec& Spec : BroadleafTextureSpecs())
    {
        UTexture2D* Texture = LoadExact<UTexture2D>(
            BroadleafTextureObjectPath(Spec));
        if (!ValidateBroadleafTexture(Texture, Spec, OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ImportBroadleafTextures(
    IAssetTools& AssetTools,
    TMap<FName, UTexture2D*>& OutTextures,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutTextures.Reset();
    if (!ValidateBroadleafTextureSourceRoster(OutError))
    {
        return false;
    }
    TArray<UAssetImportTask*> Tasks;
    TMap<FName, UAssetImportTask*> TaskByName;
    for (const FBroadleafTextureSpec& Spec : BroadleafTextureSpecs())
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task)
        {
            OutError = TEXT("Could not allocate the ten locked broadleaf PNG import tasks.");
            return false;
        }
        Factory->bCreateMaterial = false;
        Factory->NoAlpha = false;
        Factory->bDeferCompression = false;
        Factory->CompressionSettings = BroadleafTextureCompression(Spec.Usage);
        Factory->LODGroup = BroadleafTextureGroup(Spec.Usage);
        Factory->MipGenSettings = TMGS_FromTextureGroup;
        Factory->bFlipNormalMapGreenChannel =
            Spec.Usage == EBroadleafTextureUsage::NormalOpenGl;
        Factory->ColorSpaceMode = BroadleafTextureIsSrgb(Spec.Usage)
            ? ETextureSourceColorSpace::SRGB
            : ETextureSourceColorSpace::Linear;
        Task->Filename = BroadleafTextureSourcePath(Spec);
        Task->DestinationPath = BroadleafPath;
        Task->DestinationName = Spec.AssetName;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Task->Factory = Factory;
        Task->Options = Factory;
        Tasks.Add(Task);
        TaskByName.Add(FName(Spec.AssetName), Task);
    }
    if (Tasks.Num() != 10 || TaskByName.Num() != 10)
    {
        OutError = TEXT("The broadleaf PNG import-task roster is not exactly ten unique assets.");
        return false;
    }
    AssetTools.ImportAssetTasks(Tasks);

    for (const FBroadleafTextureSpec& Spec : BroadleafTextureSpecs())
    {
        UAssetImportTask* Task = TaskByName.FindRef(FName(Spec.AssetName));
        UTexture2D* Texture = nullptr;
        if (Task)
        {
            for (UObject* Object : Task->GetObjects())
            {
                UTexture2D* Candidate = Cast<UTexture2D>(Object);
                if (Candidate &&
                    Candidate->GetPathName() == BroadleafTextureObjectPath(Spec))
                {
                    if (Texture)
                    {
                        OutError = TEXT("A locked broadleaf PNG import returned duplicate exact assets.");
                        return false;
                    }
                    Texture = Candidate;
                }
            }
        }
        if (!Texture)
        {
            Texture = LoadExact<UTexture2D>(BroadleafTextureObjectPath(Spec));
        }
        if (!Texture)
        {
            OutError = FString::Printf(
                TEXT("Locked broadleaf PNG import produced no exact asset: %s"),
                Spec.AssetName);
            return false;
        }
        Texture->Modify();
        Texture->SRGB = BroadleafTextureIsSrgb(Spec.Usage);
        Texture->CompressionSettings = BroadleafTextureCompression(Spec.Usage);
        Texture->LODGroup = BroadleafTextureGroup(Spec.Usage);
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        Texture->Filter = TF_Default;
        Texture->bFlipGreenChannel =
            Spec.Usage == EBroadleafTextureUsage::NormalOpenGl;
        Texture->VirtualTextureStreaming = false;
        Texture->NeverStream = false;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (!ValidateBroadleafTexture(Texture, Spec, OutError))
        {
            return false;
        }
        OutTextures.Add(FName(Spec.AssetName), Texture);
        OutAssetsToSave.Add(Texture);
    }
    if (OutTextures.Num() != 10)
    {
        OutError = TEXT("The imported broadleaf texture map is not exactly ten assets.");
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterialExpressionTextureSample* AddBroadleafTextureSample(
    UMaterial* Material,
    UTexture2D* Texture,
    EMaterialSamplerType SamplerType,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionTextureSample* Sample =
        Material && Texture
        ? NewObject<UMaterialExpressionTextureSample>(Material)
        : nullptr;
    if (Sample)
    {
        Material->GetExpressionCollection().AddExpression(Sample);
        Sample->Texture = Texture;
        Sample->SamplerType = SamplerType;
        Sample->SamplerSource = SSM_FromTextureAsset;
        Sample->MaterialExpressionEditorX = EditorX;
        Sample->MaterialExpressionEditorY = EditorY;
    }
    return Sample;
}

UMaterial* CreateBroadleafMaterial(
    IAssetTools& AssetTools,
    const FBroadleafMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    UTexture2D* Diffuse = Textures.FindRef(FName(Spec.DiffuseTextureName));
    UTexture2D* Normal = Textures.FindRef(FName(Spec.NormalTextureName));
    UTexture2D* Roughness = Textures.FindRef(FName(Spec.RoughnessTextureName));
    UTexture2D* Alpha = Spec.bLeaves
        ? Textures.FindRef(FName(TEXT("jacaranda_tree_leaves_alpha_1k")))
        : nullptr;
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Spec.AssetName,
            BroadleafPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV1Assets"))))
        : nullptr;
    if (!Material || Material->GetPathName() != BroadleafMaterialObjectPath(Spec) ||
        !Diffuse || !Normal || !Roughness || (Spec.bLeaves && !Alpha))
    {
        OutError = FString::Printf(
            TEXT("Could not create exact canonical broadleaf material: %s"),
            Spec.AssetName);
        return nullptr;
    }
    UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    if (!EditorOnly || !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("New broadleaf material did not start with an empty editable graph.");
        return nullptr;
    }

    UMaterialExpressionTextureSample* DiffuseSample =
        AddBroadleafTextureSample(Material, Diffuse, SAMPLERTYPE_Color, -560, -180);
    UMaterialExpressionTextureSample* NormalSample =
        AddBroadleafTextureSample(Material, Normal, SAMPLERTYPE_Normal, -560, 40);
    UMaterialExpressionTextureSample* RoughnessSample =
        AddBroadleafTextureSample(Material, Roughness, SAMPLERTYPE_Masks, -560, 240);
    UMaterialExpressionTextureSample* AlphaSample = Spec.bLeaves
        ? AddBroadleafTextureSample(Material, Alpha, SAMPLERTYPE_Masks, -560, 430)
        : nullptr;
    if (!DiffuseSample || !NormalSample || !RoughnessSample ||
        (Spec.bLeaves && !AlphaSample))
    {
        OutError = TEXT("Could not allocate the complete canonical broadleaf PBR graph.");
        return nullptr;
    }

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->BlendMode = Spec.bLeaves ? BLEND_Masked : BLEND_Opaque;
    Material->TwoSided = Spec.bLeaves;
    Material->OpacityMaskClipValue = BroadleafLeafOpacityMaskClipValue;
    Material->SetShadingModel(
        Spec.bLeaves ? MSM_TwoSidedFoliage : MSM_DefaultLit);
    EditorOnly->BaseColor.Connect(0, DiffuseSample);
    EditorOnly->Normal.Connect(0, NormalSample);
    EditorOnly->Roughness.Connect(1, RoughnessSample);
    if (Spec.bLeaves)
    {
        EditorOnly->OpacityMask.Connect(1, AlphaSample);
        EditorOnly->SubsurfaceColor.Connect(0, DiffuseSample);
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (!ValidateBroadleafMaterial(Material, Spec, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

bool CreateBroadleafMaterials(
    IAssetTools& AssetTools,
    const TMap<FName, UTexture2D*>& Textures,
    TMap<FName, UMaterial*>& OutMaterials,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutMaterials.Reset();
    if (BroadleafMaterialSpecs().Num() != 3 || Textures.Num() != 10)
    {
        OutError = TEXT("Canonical broadleaf material creation requires 3 materials and 10 textures.");
        return false;
    }
    for (const FBroadleafMaterialSpec& Spec : BroadleafMaterialSpecs())
    {
        UMaterial* Material = CreateBroadleafMaterial(
            AssetTools, Spec, Textures, OutError);
        if (!Material || OutMaterials.Contains(Spec.SlotName))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Canonical broadleaf material roster contains a duplicate slot.");
            }
            return false;
        }
        OutMaterials.Add(Spec.SlotName, Material);
        OutAssetsToSave.Add(Material);
    }
    OutError.Reset();
    return true;
}

bool ValidateExploreLawnMaterial(
    UMaterial* ExploreLawn,
    FString& OutError)
{
    UMaterial* SharedLawn =
        LoadExact<UMaterial>(SharedLawnMaterialObjectPath);
    if (!ExploreLawn || !SharedLawn || ExploreLawn == SharedLawn ||
        ExploreLawn->GetPathName() != ExploreLawnMaterialObjectPath ||
        !ExploreLawn->bUsedWithInstancedStaticMeshes ||
        SharedLawn->GetOutermost()->IsDirty())
    {
        OutError = TEXT("The Explore-owned instanced lawn material is missing, invalid, or the protected shared lawn package became dirty.");
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateExploreLawnMaterial(
    IAssetTools& AssetTools,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    UMaterial* SharedLawn =
        LoadExact<UMaterial>(SharedLawnMaterialObjectPath);
    if (!SharedLawn || SharedLawn->GetOutermost()->IsDirty())
    {
        OutError = TEXT("The exact protected shared lawn material is missing or dirty before duplication.");
        return nullptr;
    }
    UMaterial* ExploreLawn = Cast<UMaterial>(AssetTools.DuplicateAsset(
        ExploreLawnMaterialName, BroadleafPath, SharedLawn));
    if (!ExploreLawn ||
        ExploreLawn->GetPathName() != ExploreLawnMaterialObjectPath)
    {
        OutError = TEXT("Could not create the exact Explore-owned lawn material duplicate.");
        return nullptr;
    }
    ExploreLawn->Modify();
    ExploreLawn->bUsedWithInstancedStaticMeshes = true;
    UMaterialEditingLibrary::RecompileMaterial(ExploreLawn);
    ExploreLawn->PostEditChange();
    ExploreLawn->MarkPackageDirty();
    if (!ValidateExploreLawnMaterial(ExploreLawn, OutError))
    {
        return nullptr;
    }
    OutAssetsToSave.Add(ExploreLawn);
    OutError.Reset();
    return ExploreLawn;
}

bool BindBroadleafMaterials(
    UStaticMesh* Broadleaf,
    const TMap<FName, UMaterial*>& Materials,
    FString& OutError)
{
    if (!Broadleaf || Broadleaf->GetStaticMaterials().Num() != 3 ||
        Materials.Num() != 3)
    {
        OutError = TEXT("Imported broadleaf mesh/material slot roster is not exactly three.");
        return false;
    }
    TSet<int32> BoundIndices;
    Broadleaf->Modify();
    for (const FBroadleafMaterialSpec& Spec : BroadleafMaterialSpecs())
    {
        const int32 Index = Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
            Spec.SlotName);
        UMaterial* Material = Materials.FindRef(Spec.SlotName);
        if (Index == INDEX_NONE || BoundIndices.Contains(Index) || !Material ||
            Broadleaf->GetStaticMaterials()[Index].ImportedMaterialSlotName !=
                Spec.SlotName)
        {
            OutError = FString::Printf(
                TEXT("FBX mesh lost exact imported material slot '%s'."),
                *Spec.SlotName.ToString());
            return false;
        }
        BoundIndices.Add(Index);
        FStaticMaterial& Slot = Broadleaf->GetStaticMaterials()[Index];
        Slot.MaterialSlotName = Spec.SlotName;
        Slot.ImportedMaterialSlotName = Spec.SlotName;
        Slot.MaterialInterface = Material;
    }
    Broadleaf->PostEditChange();
    Broadleaf->MarkPackageDirty();
    return ValidateBroadleafMaterialRoster(Broadleaf, OutError);
}

TArray<FString> ExploreAssetObjectPaths()
{
    TArray<FString> ObjectPaths = {
        BroadleafObjectPath, GrassObjectPath, ExploreLawnMaterialObjectPath};
    for (const FBroadleafTextureSpec& Spec : BroadleafTextureSpecs())
    {
        ObjectPaths.Add(BroadleafTextureObjectPath(Spec));
    }
    for (const FBroadleafMaterialSpec& Spec : BroadleafMaterialSpecs())
    {
        ObjectPaths.Add(BroadleafMaterialObjectPath(Spec));
    }
    return ObjectPaths;
}

bool ValidateExactExploreAssetOutputRoster(FString& OutError)
{
    const TArray<FString> ObjectPaths = ExploreAssetObjectPaths();
    if (ObjectPaths.Num() != 16)
    {
        OutError = TEXT("The compiled Explore asset roster is not exactly 16 packages.");
        return false;
    }

    TSet<FString> ExpectedFilenames;
    for (const FString& ObjectPath : ObjectPaths)
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        FString Filename;
        if (!FPackageName::DoesPackageExist(PackageName, &Filename))
        {
            OutError = FString::Printf(
                TEXT("The exact Explore asset package is missing: %s"),
                *PackageName);
            return false;
        }
        Filename = FPaths::ConvertRelativePathToFull(Filename);
        FPaths::NormalizeFilename(Filename);
        Filename.ToLowerInline();
        if (ExpectedFilenames.Contains(Filename))
        {
            OutError = TEXT("The exact Explore asset roster resolves to a duplicate package file.");
            return false;
        }
        ExpectedFilenames.Add(Filename);
    }

    TArray<FString> ActualFiles;
    const FString OutputDirectory =
        FPackageName::LongPackageNameToFilename(BroadleafPath);
    IFileManager::Get().FindFilesRecursive(
        ActualFiles,
        *OutputDirectory,
        TEXT("*.uasset"),
        true,
        false,
        false);
    if (ActualFiles.Num() != ExpectedFilenames.Num())
    {
        OutError = FString::Printf(
            TEXT("The Explore destination contains %d package files; exactly %d are required."),
            ActualFiles.Num(), ExpectedFilenames.Num());
        return false;
    }
    for (FString ActualFile : ActualFiles)
    {
        ActualFile = FPaths::ConvertRelativePathToFull(ActualFile);
        FPaths::NormalizeFilename(ActualFile);
        ActualFile.ToLowerInline();
        if (!ExpectedFilenames.Contains(ActualFile))
        {
            OutError = FString::Printf(
                TEXT("The Explore destination contains an unexpected package file: %s"),
                *ActualFile);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateGeneratedMeshMaterialUvChannelData(
    UStaticMesh* Mesh,
    const TCHAR* MeshRole,
    FString& OutError)
{
    if (!Mesh || !Mesh->GetRenderData() || Mesh->GetStaticMaterials().IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("The generated %s mesh has no render data or material slots."),
            MeshRole);
        return false;
    }
    for (int32 MaterialIndex = 0;
         MaterialIndex < Mesh->GetStaticMaterials().Num();
         ++MaterialIndex)
    {
        const FStaticMaterial& Slot =
            Mesh->GetStaticMaterials()[MaterialIndex];
        if (!Slot.UVChannelData.bInitialized)
        {
            OutError = FString::Printf(
                TEXT("The generated %s mesh material slot %d has uninitialized UV-channel density metadata."),
                MeshRole, MaterialIndex);
            return false;
        }
        if (Slot.UVChannelData.bOverrideDensities)
        {
            OutError = FString::Printf(
                TEXT("The generated %s mesh material slot %d uses overridden rather than render-derived UV-channel densities."),
                MeshRole, MaterialIndex);
            return false;
        }
        for (const float Density : Slot.UVChannelData.LocalUVDensities)
        {
            if (!FMath::IsFinite(Density) || Density < 0.0f)
            {
                OutError = FString::Printf(
                    TEXT("The generated %s mesh material slot %d has a non-finite or negative UV-channel density."),
                    MeshRole, MaterialIndex);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool RebuildGeneratedMeshMaterialUvChannelData(
    UStaticMesh* Mesh,
    const TCHAR* MeshRole,
    FString& OutError)
{
    if (!Mesh || !Mesh->GetRenderData() || Mesh->GetStaticMaterials().IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("The generated %s mesh cannot rebuild UV-channel metadata without render data and material slots."),
            MeshRole);
        return false;
    }
    for (FStaticMaterial& Slot : Mesh->GetStaticMaterials())
    {
        Slot.UVChannelData.bInitialized = false;
        Slot.UVChannelData.bOverrideDensities = false;
        for (float& Density : Slot.UVChannelData.LocalUVDensities)
        {
            Density = 0.0f;
        }
    }
    Mesh->UpdateUVChannelData(true);
    return ValidateGeneratedMeshMaterialUvChannelData(
        Mesh, MeshRole, OutError);
}

enum class EGeneratedMeshUvMetadataMarkerStatus : uint8
{
    Missing,
    Exact,
    Foreign
};

const FString* ExpectedGeneratedMeshUvMetadataSchema(UStaticMesh* Mesh)
{
    if (Mesh && Mesh->GetPathName() == BroadleafObjectPath)
    {
        return &BroadleafUvMetadataSchemaValue;
    }
    if (Mesh && Mesh->GetPathName() == GrassObjectPath)
    {
        return &GrassUvMetadataSchemaValue;
    }
    return nullptr;
}

EGeneratedMeshUvMetadataMarkerStatus GetGeneratedMeshUvMetadataMarkerStatus(
    UStaticMesh* Mesh)
{
    const FString* Expected = ExpectedGeneratedMeshUvMetadataSchema(Mesh);
    UPackage* Package = Mesh ? Mesh->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Expected || !Metadata)
    {
        return EGeneratedMeshUvMetadataMarkerStatus::Foreign;
    }
    const FString* Actual = Metadata->FindValue(
        Mesh, GeneratedMeshUvMetadataSchemaKey);
    if (!Actual)
    {
        return EGeneratedMeshUvMetadataMarkerStatus::Missing;
    }
    return *Actual == *Expected
        ? EGeneratedMeshUvMetadataMarkerStatus::Exact
        : EGeneratedMeshUvMetadataMarkerStatus::Foreign;
}

bool ValidateGeneratedMeshUvMetadataState(
    UStaticMesh* Mesh,
    const TCHAR* MeshRole,
    bool bAllowMissingMarker,
    FString& OutError)
{
    const EGeneratedMeshUvMetadataMarkerStatus MarkerStatus =
        GetGeneratedMeshUvMetadataMarkerStatus(Mesh);
    if (MarkerStatus == EGeneratedMeshUvMetadataMarkerStatus::Foreign)
    {
        OutError = FString::Printf(
            TEXT("The generated %s mesh has a foreign UV-metadata schema marker."),
            MeshRole);
        return false;
    }
    if (MarkerStatus == EGeneratedMeshUvMetadataMarkerStatus::Missing)
    {
        if (bAllowMissingMarker)
        {
            OutError.Reset();
            return true;
        }
        OutError = FString::Printf(
            TEXT("The generated %s mesh is missing its persisted UV-metadata schema marker."),
            MeshRole);
        return false;
    }
    return ValidateGeneratedMeshMaterialUvChannelData(
        Mesh, MeshRole, OutError);
}

bool StampGeneratedMeshUvMetadataSchema(
    UStaticMesh* Mesh,
    FString& OutError)
{
    const FString* Expected = ExpectedGeneratedMeshUvMetadataSchema(Mesh);
    UPackage* Package = Mesh ? Mesh->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Expected || !Metadata)
    {
        OutError = TEXT("A generated mesh is outside the exact UV-metadata schema allowlist.");
        return false;
    }
    Metadata->Modify();
    Metadata->SetValue(
        Mesh, GeneratedMeshUvMetadataSchemaKey, *(*Expected));
    if (Metadata->GetValue(Mesh, GeneratedMeshUvMetadataSchemaKey) != *Expected)
    {
        OutError = TEXT("The exact generated mesh UV-metadata schema marker did not persist in memory.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool AnyExploreAssetOutputExists()
{
    const TArray<FString> ObjectPaths = ExploreAssetObjectPaths();
    for (const FString& ObjectPath : ObjectPaths)
    {
        if (LoadObject<UObject>(nullptr, *ObjectPath) ||
            FPackageName::DoesPackageExist(
                FPackageName::ObjectPathToPackageName(ObjectPath)))
        {
            return true;
        }
    }
    TArray<FString> ActualFiles;
    const FString OutputDirectory =
        FPackageName::LongPackageNameToFilename(BroadleafPath);
    IFileManager::Get().FindFilesRecursive(
        ActualFiles,
        *OutputDirectory,
        TEXT("*.uasset"),
        true,
        false,
        false);
    if (!ActualFiles.IsEmpty())
    {
        return true;
    }
    return false;
}

bool ValidateBroadleafRuntimeLods(
    UStaticMesh* Broadleaf,
    FString& OutError)
{
    if (!Broadleaf || Broadleaf->GetNumSourceModels() != BroadleafSourceLodCount ||
        Broadleaf->GetDefaultMinLOD() != BroadleafRuntimeMinLod ||
        Broadleaf->GetMinLODIdx() != BroadleafRuntimeMinLod ||
        Broadleaf->bAutoComputeLODScreenSize ||
        Broadleaf->NaniteSettings.bEnabled ||
        BroadleafRuntimeLodSpecs().Num() != BroadleafSourceLodCount - 1)
    {
        OutError = TEXT("The broadleaf runtime LOD/MinLOD policy is absent or changed.");
        return false;
    }
    if (!FMath::IsNearlyEqual(
            Broadleaf->GetSourceModel(0).ScreenSize.Default, 1.0f, 0.0001f))
    {
        OutError = TEXT("The non-renderable broadleaf source LOD screen-size policy changed.");
        return false;
    }
    for (const FBroadleafRuntimeLodSpec& Spec : BroadleafRuntimeLodSpecs())
    {
        const FStaticMeshSourceModel& SourceModel =
            Broadleaf->GetSourceModel(Spec.Index);
        const FMeshReductionSettings& Reduction =
            SourceModel.ReductionSettings;
        if (Reduction.TerminationCriterion !=
                EStaticMeshReductionTerimationCriterion::Triangles ||
            !FMath::IsNearlyEqual(
                Reduction.PercentTriangles, Spec.PercentTriangles, 0.000001f) ||
            Reduction.MaxNumOfTriangles != Spec.MaximumTriangles ||
            Reduction.BaseLODModel != 0 || Reduction.bRecalculateNormals ||
            !FMath::IsNearlyEqual(
                SourceModel.ScreenSize.Default, Spec.ScreenSize, 0.0001f))
        {
            OutError = FString::Printf(
                TEXT("Broadleaf generated LOD%d settings changed."), Spec.Index);
            return false;
        }
    }

    const FStaticMeshRenderData* RenderData = Broadleaf->GetRenderData();
    if (!RenderData ||
        RenderData->LODResources.Num() != BroadleafSourceLodCount ||
        RenderData->LODResources[0].GetNumTriangles() < 3000000)
    {
        OutError = TEXT("The broadleaf built render-resource LOD roster is invalid.");
        return false;
    }
    int32 PreviousTriangles = RenderData->LODResources[0].GetNumTriangles();
    for (const FBroadleafRuntimeLodSpec& Spec : BroadleafRuntimeLodSpecs())
    {
        const FStaticMeshLODResources& Lod =
            RenderData->LODResources[Spec.Index];
        const int32 Triangles = Lod.GetNumTriangles();
        TSet<uint32> MaterialIndices;
        for (const FStaticMeshSection& Section : Lod.Sections)
        {
            if (Section.NumTriangles == 0)
            {
                OutError = TEXT("A generated broadleaf LOD contains an empty render section.");
                return false;
            }
            MaterialIndices.Add(Section.MaterialIndex);
        }
        const uint32 VertexCount = Lod.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
        if (Triangles <= 0 || Triangles > static_cast<int32>(Spec.MaximumTriangles) ||
            Triangles >= PreviousTriangles || Lod.GetNumTexCoords() < 1 ||
            VertexCount == 0 || VertexCount != static_cast<uint32>(Lod.GetNumVertices()) ||
            MaterialIndices.Num() != 3 || !MaterialIndices.Contains(0) ||
            !MaterialIndices.Contains(1) || !MaterialIndices.Contains(2))
        {
            OutError = FString::Printf(
                TEXT("Rendered broadleaf LOD%d exceeds its triangle ceiling or lost UV/normal/material data (triangles=%d, ceiling=%u)."),
                Spec.Index, Triangles, Spec.MaximumTriangles);
            return false;
        }
        const uint32 SampleVertex = VertexCount / 2;
        if (Lod.VertexBuffers.StaticMeshVertexBuffer
                .GetVertexUV(SampleVertex, 0).ContainsNaN() ||
            Lod.VertexBuffers.StaticMeshVertexBuffer
                .VertexTangentZ(SampleVertex).ContainsNaN())
        {
            OutError = TEXT("A rendered broadleaf LOD contains non-finite sampled UV/normal data.");
            return false;
        }
        PreviousTriangles = Triangles;
    }
    OutError.Reset();
    return true;
}

bool ConfigureBroadleafRuntimeLods(
    UStaticMesh* Broadleaf,
    FString& OutError)
{
    if (!Broadleaf || Broadleaf->GetNumSourceModels() != 1)
    {
        OutError = TEXT("The imported broadleaf must begin with exactly one source LOD.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Broadleaf});
    Broadleaf->Modify();
    Broadleaf->SetNumSourceModels(BroadleafSourceLodCount);
    Broadleaf->bAutoComputeLODScreenSize = false;
    Broadleaf->GetSourceModel(0).ScreenSize = FPerPlatformFloat(1.0f);
    for (const FBroadleafRuntimeLodSpec& Spec : BroadleafRuntimeLodSpecs())
    {
        FStaticMeshSourceModel& SourceModel =
            Broadleaf->GetSourceModel(Spec.Index);
        SourceModel.ScreenSize = FPerPlatformFloat(Spec.ScreenSize);
        FMeshReductionSettings& Reduction = SourceModel.ReductionSettings;
        Reduction.TerminationCriterion =
            EStaticMeshReductionTerimationCriterion::Triangles;
        Reduction.PercentTriangles = Spec.PercentTriangles;
        Reduction.MaxNumOfTriangles = Spec.MaximumTriangles;
        Reduction.PercentVertices = 1.0f;
        Reduction.MaxNumOfVerts = MAX_uint32;
        Reduction.MaxDeviation = 0.0f;
        Reduction.WeldingThreshold = 0.0f;
        Reduction.BaseLODModel = 0;
        Reduction.SilhouetteImportance = EMeshFeatureImportance::Highest;
        Reduction.TextureImportance = EMeshFeatureImportance::Highest;
        Reduction.ShadingImportance = EMeshFeatureImportance::Highest;
        Reduction.bRecalculateNormals = false;
        Reduction.bGenerateUniqueLightmapUVs = false;
    }
    Broadleaf->SetMinLODIdx(BroadleafRuntimeMinLod);
    Broadleaf->PostEditChange();
    Broadleaf->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Broadleaf});
    return ValidateBroadleafRuntimeLods(Broadleaf, OutError);
}

bool ValidateImportedAssets(
    FString& OutError,
    bool bAllowMissingPersistedUvMetadataMarker = false)
{
    if (!ValidateExactExploreAssetOutputRoster(OutError))
    {
        return false;
    }
    UStaticMesh* Broadleaf = LoadExact<UStaticMesh>(BroadleafObjectPath);
    UStaticMesh* Grass = LoadExact<UStaticMesh>(GrassObjectPath);
    UMaterial* Lawn = LoadExact<UMaterial>(ExploreLawnMaterialObjectPath);
    if (!Broadleaf || !Grass || !Lawn ||
        !LoadExact<UStaticMesh>(ShrubAPath) ||
        !LoadExact<UStaticMesh>(ShrubBPath) ||
        !LoadExact<UStaticMesh>(GroundcoverPath) ||
        !LoadExact<UStaticMesh>(CylinderPath))
    {
        OutError = TEXT("Explore assets or exact project shrub/lawn dependencies are missing.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Broadleaf, Grass});
    if (!ValidateGeneratedMeshUvMetadataState(
            Broadleaf,
            TEXT("broadleaf"),
            bAllowMissingPersistedUvMetadataMarker,
            OutError) ||
        !ValidateGeneratedMeshUvMetadataState(
            Grass,
            TEXT("grass"),
            bAllowMissingPersistedUvMetadataMarker,
            OutError))
    {
        return false;
    }
    const FVector TreeExtent = Broadleaf->GetBounds().BoxExtent;
    const FVector GrassExtent = Grass->GetBounds().BoxExtent;
    const bool bGrassMaterialExact = Grass->GetStaticMaterials().Num() == 1 &&
        Grass->GetStaticMaterials()[0].MaterialInterface == Lawn;
    if (TreeExtent.ContainsNaN() || TreeExtent.X < 700.0 ||
        TreeExtent.Y < 700.0 || TreeExtent.Z < 700.0 || TreeExtent.Z > 1600.0 ||
        GrassExtent.ContainsNaN() || GrassExtent.X < 55.0 ||
        GrassExtent.X > 90.0 || GrassExtent.Y < 55.0 ||
        GrassExtent.Y > 90.0 || GrassExtent.Z < 4.0 || GrassExtent.Z > 15.0 ||
        !bGrassMaterialExact || Broadleaf->NaniteSettings.bEnabled ||
        Grass->NaniteSettings.bEnabled)
    {
        OutError = FString::Printf(
            TEXT("Explore imported mesh bounds/material policy changed (tree=%s, grass=%s)."),
            *TreeExtent.ToString(), *GrassExtent.ToString());
        return false;
    }
    if (!ValidateExploreLawnMaterial(Lawn, OutError) ||
        !ValidateBroadleafRuntimeLods(Broadleaf, OutError) ||
        !ValidateBroadleafTextureRoster(OutError) ||
        !ValidateBroadleafMaterialRoster(Broadleaf, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

enum class EGeneratedMeshUvMetadataRepairResult : uint8
{
    NotEligible,
    Repaired,
    Failed
};

EGeneratedMeshUvMetadataRepairResult TryRepairGeneratedMeshUvMetadata(
    FString& OutError)
{
    FString PreflightError;
    if (!ValidateImportedAssets(PreflightError, true))
    {
        OutError = PreflightError;
        return EGeneratedMeshUvMetadataRepairResult::NotEligible;
    }

    UStaticMesh* Broadleaf = LoadExact<UStaticMesh>(BroadleafObjectPath);
    UStaticMesh* Grass = LoadExact<UStaticMesh>(GrassObjectPath);
    if (!Broadleaf || !Grass)
    {
        OutError = TEXT("The exact generated mesh pair disappeared during UV-metadata repair.");
        return EGeneratedMeshUvMetadataRepairResult::Failed;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Broadleaf, Grass});

    const TSet<FString> RepairableObjectPaths = {
        BroadleafObjectPath, GrassObjectPath};
    const TArray<UStaticMesh*> GeneratedMeshes = {Broadleaf, Grass};
    TArray<UStaticMesh*> ChangedMeshes;
    for (UStaticMesh* Mesh : GeneratedMeshes)
    {
        const EGeneratedMeshUvMetadataMarkerStatus MarkerStatus =
            GetGeneratedMeshUvMetadataMarkerStatus(Mesh);
        if (MarkerStatus == EGeneratedMeshUvMetadataMarkerStatus::Exact)
        {
            continue;
        }
        if (MarkerStatus != EGeneratedMeshUvMetadataMarkerStatus::Missing ||
            !RepairableObjectPaths.Contains(Mesh->GetPathName()))
        {
            OutError = TEXT("UV-metadata repair selected a foreign marker or mesh outside the exact two-package allowlist.");
            return EGeneratedMeshUvMetadataRepairResult::Failed;
        }
        Mesh->Modify();
        if (!RebuildGeneratedMeshMaterialUvChannelData(
                Mesh,
                Mesh == Broadleaf ? TEXT("broadleaf") : TEXT("grass"),
                OutError) ||
            !StampGeneratedMeshUvMetadataSchema(Mesh, OutError) ||
            !ValidateGeneratedMeshUvMetadataState(
                Mesh,
                Mesh == Broadleaf ? TEXT("broadleaf") : TEXT("grass"),
                false,
                OutError))
        {
            return EGeneratedMeshUvMetadataRepairResult::Failed;
        }
        Mesh->MarkPackageDirty();
        ChangedMeshes.Add(Mesh);
    }
    if (ChangedMeshes.IsEmpty())
    {
        OutError = TEXT("The exact roster passed missing-marker preflight but contains no repairable UV-metadata marker defect.");
        return EGeneratedMeshUvMetadataRepairResult::Failed;
    }

    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!Assets)
    {
        OutError = TEXT("The editor asset subsystem is unavailable for exact mesh UV-metadata repair.");
        return EGeneratedMeshUvMetadataRepairResult::Failed;
    }
    for (UStaticMesh* Mesh : ChangedMeshes)
    {
        if (!RepairableObjectPaths.Contains(Mesh->GetPathName()) ||
            !Assets->SaveLoadedAsset(Mesh, false))
        {
            OutError = TEXT("Saving an exact changed mesh package after UV-metadata repair failed.");
            return EGeneratedMeshUvMetadataRepairResult::Failed;
        }
    }
    if (!ValidateImportedAssets(OutError))
    {
        return EGeneratedMeshUvMetadataRepairResult::Failed;
    }
    OutError = FString::Printf(
        TEXT("Rebuilt UV-channel density metadata and persisted schema markers in %d exact generated mesh package(s)."),
        ChangedMeshes.Num());
    return EGeneratedMeshUvMetadataRepairResult::Repaired;
}

bool ValidateOrRepairImportedAssets(
    FString& OutError,
    bool& OutRepairedUvMetadata)
{
    OutRepairedUvMetadata = false;
    if (ValidateImportedAssets(OutError))
    {
        return true;
    }
    const EGeneratedMeshUvMetadataRepairResult RepairResult =
        TryRepairGeneratedMeshUvMetadata(OutError);
    if (RepairResult == EGeneratedMeshUvMetadataRepairResult::Repaired)
    {
        OutRepairedUvMetadata = true;
        return true;
    }
    return false;
}

UAssetImportTask* MakeMeshImportTask(
    const FString& Filename,
    const FString& DestinationName,
    bool bConvertSceneUnit)
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
    Options->StaticMeshImportData->bConvertScene = bConvertSceneUnit;
    Options->StaticMeshImportData->bConvertSceneUnit = bConvertSceneUnit;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->bCombineMeshes = true;
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

    Task->Filename = Filename;
    Task->DestinationPath = BroadleafPath;
    Task->DestinationName = DestinationName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

ATRIADIstanaPublicViewSceneActor* FindScene(UWorld* World)
{
    ATRIADIstanaPublicViewSceneActor* Result = nullptr;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(World); It; ++It)
    {
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaPublicViewRuntimePolicyActor* FindPolicy(UWorld* World)
{
    ATRIADIstanaPublicViewRuntimePolicyActor* Result = nullptr;
    for (TActorIterator<ATRIADIstanaPublicViewRuntimePolicyActor> It(World); It; ++It)
    {
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaExploreLandscapeActor* FindLandscape(UWorld* World)
{
    ATRIADIstanaExploreLandscapeActor* Result = nullptr;
    for (TActorIterator<ATRIADIstanaExploreLandscapeActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(ExploreLandscapeTag) || Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

int32 CountTreeInstances(const ATRIADIstanaPublicViewSceneActor* Scene)
{
    return Scene
        ? Scene->RainTreeInstances->GetInstanceCount() +
            Scene->PalmTreeInstances->GetInstanceCount() +
            Scene->FramingTreeInstances->GetInstanceCount()
        : 0;
}

int32 PreserveOnlyOuterLegacyTrees(ATRIADIstanaPublicViewSceneActor* Scene)
{
    int32 PreservedCount = 0;
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        Scene->RainTreeInstances.Get(), Scene->PalmTreeInstances.Get(),
        Scene->FramingTreeInstances.Get()};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        TArray<FTransform> Preserved;
        for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
        {
            FTransform Transform;
            if (Component->GetInstanceTransform(Index, Transform, false) &&
                Transform.GetTranslation().Size2D() >= InnerLegacyTreeRadiusCentimeters)
            {
                Preserved.Add(Transform);
            }
        }
        Component->Modify();
        Component->ClearInstances();
        for (const FTransform& Transform : Preserved)
        {
            Component->AddInstance(Transform, false);
        }
        PreservedCount += Preserved.Num();
    }
    return PreservedCount;
}

bool ValidateWorld(UWorld* World, FString& OutReport)
{
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutReport = TEXT("Open exact additive map /Game/Maps/Istana_PublicView_Explore_v1.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(World);
    ATRIADIstanaExploreLandscapeActor* Landscape = FindLandscape(World);
    FString LandscapeReport;
    UClass* ExploreGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(), nullptr, *ExploreGameModeClassPath);
    const AGameModeBase* ExploreGameModeDefaults = ExploreGameMode
        ? Cast<AGameModeBase>(ExploreGameMode->GetDefaultObject())
        : nullptr;
    if (!Scene || !Policy || !Landscape || !ExploreGameMode ||
        !ExploreGameModeDefaults ||
        ExploreGameModeDefaults->DefaultPawnClass !=
            ATRIADIstanaFreeRoamPawn::StaticClass() ||
        ExploreGameModeDefaults->HUDClass != nullptr ||
        World->GetWorldSettings()->DefaultGameMode.Get() != ExploreGameMode ||
        Policy->bEnforceFixedPrimaryCamera ||
        !Policy->bRequireIstanaAirSimGameMode ||
        Policy->RequiredGameModeClassPath != ExploreGameModeClassPath ||
        CountTreeInstances(Scene) != ExpectedPreservedOuterLegacyTrees ||
        !Landscape->ValidateExploreLandscape(LandscapeReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V1_MAP_INVALID: game mode, runtime policy, preserved outer trees, or landscape failed. ") +
            LandscapeReport;
        return false;
    }
    FString DistantContextError;
    if (!Landscape->ValidatePreservedDistantContext(
            Scene->OSMContextBuildingsComponent.Get(),
            DistantContextError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V1_MAP_INVALID: distant OSM/HDB context was changed. ") +
            DistantContextError;
        return false;
    }

    int32 StartCount = 0;
    int32 CameraCount = 0;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        if (It->Tags.Contains(ExplorePlayerStartTag))
        {
            ++StartCount;
            if (!It->GetActorLocation().Equals(
                    FVector(0.0, 21000.0, 2400.0), 0.1) ||
                !It->GetActorRotation().Equals(
                    FRotator(-5.0f, -90.0f, 0.0f), 0.01f))
            {
                OutReport = TEXT("ISTANA_EXPLORE_V1_MAP_INVALID: explorer start transform moved.");
                return false;
            }
        }
    }
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        ++CameraCount;
        if (!IsCameraAutoActivationDisabled(*It))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V1_MAP_INVALID: a retained fixed QA camera still auto-activates in Explore.");
            return false;
        }
    }
    if (StartCount != 1 || CameraCount < 1)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V1_MAP_INVALID: exact explorer start or retained QA cameras are absent.");
        return false;
    }
    OutReport = TEXT("Validated additive Istana Explore V1 map: V5 hero/terrain/hardscape and distant OSM buildings remain, 560 outer legacy context trees are preserved, deterministic high-detail inner landscaping is active, fixed QA cameras are retained but not auto-active, and Player 0 starts in the collision-aware free-roam pawn.");
    return true;
}
}

bool UTRIADIstanaExploreEditorLibrary::ValidateIstanaExploreRemoteControlProject(
    const FString& ExpectedProjectPath,
    FString& OutReport)
{
    if (ExpectedProjectPath.IsEmpty())
    {
        OutReport = TEXT("ExpectedProjectPath is required.");
        return false;
    }
    FString Expected = FPaths::ConvertRelativePathToFull(ExpectedProjectPath);
    FPaths::NormalizeDirectoryName(Expected);
    if (Expected.EndsWith(TEXT(".uproject"), ESearchCase::IgnoreCase))
    {
        Expected = FPaths::GetPath(Expected);
        FPaths::NormalizeDirectoryName(Expected);
    }
    FString Actual = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Actual);
    if (!FPaths::IsSamePath(Actual, Expected) ||
        !FPaths::FileExists(FPaths::Combine(Actual, TEXT("TRIAD.uproject"))))
    {
        OutReport = FString::Printf(
            TEXT("Remote editor project mismatch: expected '%s', actual '%s'."),
            *Expected, *Actual);
        return false;
    }
    OutReport = TEXT("Remote editor is the exact requested TRIAD project.");
    return true;
}

bool UTRIADIstanaExploreEditorLibrary::ImportIstanaExploreV1Assets(
    FString& OutMessage)
{
    FString Error;
    bool bRepairedUvMetadata = false;
    if (ValidateOrRepairImportedAssets(Error, bRepairedUvMetadata))
    {
        OutMessage = bRepairedUvMetadata
            ? TEXT("REPAIRED_EXPLORE_V1_ASSET_UV_METADATA: ") + Error
            : TEXT("IDEMPOTENT_EXPLORE_V1_ASSETS_ALREADY_VALID: ") + Error;
        return true;
    }
    if (AnyExploreAssetOutputExists())
    {
        OutMessage = TEXT("EXPLORE_V1_IMPORT_REFUSED: partial or invalid 16-asset destination roster already exists. ") + Error;
        return false;
    }
    const FString TreeSource = ProjectSourcePath(
        TEXT("Sources/PolyHaven/jacaranda_tree_1k/jacaranda_tree_1k.fbx"));
    const FString GrassSource = ProjectSourcePath(
        TEXT("Generated/SM_IstanaPublicViewExploreV1_GrassClump.obj"));
    if (!FPaths::FileExists(TreeSource) || !FPaths::FileExists(GrassSource) ||
        !ValidateBroadleafTextureSourceRoster(Error))
    {
        OutMessage = TEXT("EXPLORE_V1_IMPORT_REFUSED: validated tree, grass, or exact ten-PNG source roster is missing/changed. ") + Error;
        return false;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UObject*> AssetsToSave;
    TMap<FName, UTexture2D*> Textures;
    if (!ImportBroadleafTextures(
            AssetTools, Textures, AssetsToSave, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: exact ten-PNG import failed. ") + Error;
        return false;
    }
    TMap<FName, UMaterial*> Materials;
    if (!CreateBroadleafMaterials(
            AssetTools, Textures, Materials, AssetsToSave, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: canonical three-material creation failed. ") + Error;
        return false;
    }
    UMaterial* ExploreLawn = CreateExploreLawnMaterial(
        AssetTools, AssetsToSave, Error);
    if (!ExploreLawn)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: Explore-owned instanced lawn material creation failed. ") + Error;
        return false;
    }
    UAssetImportTask* TreeTask = MakeMeshImportTask(
        TreeSource, BroadleafName, true);
    UAssetImportTask* GrassTask = MakeMeshImportTask(
        GrassSource, GrassName, false);
    if (!TreeTask || !GrassTask)
    {
        OutMessage = TEXT("EXPLORE_V1_IMPORT_REFUSED: could not allocate import tasks.");
        return false;
    }
    TArray<UAssetImportTask*> Tasks = {TreeTask, GrassTask};
    AssetTools.ImportAssetTasks(Tasks);
    UStaticMesh* Broadleaf = ImportedMeshFromTask(TreeTask, BroadleafObjectPath);
    UStaticMesh* Grass = ImportedMeshFromTask(GrassTask, GrassObjectPath);
    if (!Broadleaf || !Grass || !ExploreLawn)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: import produced no exact tree/grass mesh or lawn dependency.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Broadleaf, Grass});
    if (!BindBroadleafMaterials(Broadleaf, Materials, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: exact FBX material-slot binding failed. ") + Error;
        return false;
    }
    Broadleaf->Modify();
    Broadleaf->NaniteSettings.bEnabled = false;
    if (!ConfigureBroadleafRuntimeLods(Broadleaf, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: deterministic broadleaf runtime LOD generation failed. ") + Error;
        return false;
    }
    Broadleaf->CreateBodySetup();
    if (UBodySetup* Body = Broadleaf->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Grass->Modify();
    Grass->NaniteSettings.bEnabled = false;
    Grass->GetStaticMaterials().Reset();
    Grass->GetStaticMaterials().Add(FStaticMaterial(
        ExploreLawn, TEXT("M_IPVExplore_Grass"), TEXT("M_IPVExplore_Grass")));
    if (!RebuildGeneratedMeshMaterialUvChannelData(
            Broadleaf, TEXT("broadleaf"), Error) ||
        !RebuildGeneratedMeshMaterialUvChannelData(
            Grass, TEXT("grass"), Error) ||
        !StampGeneratedMeshUvMetadataSchema(Broadleaf, Error) ||
        !StampGeneratedMeshUvMetadataSchema(Grass, Error) ||
        !ValidateGeneratedMeshUvMetadataState(
            Broadleaf, TEXT("broadleaf"), false, Error) ||
        !ValidateGeneratedMeshUvMetadataState(
            Grass, TEXT("grass"), false, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_ASSETS: generated mesh material UV-channel metadata initialization or schema stamping failed. ") + Error;
        return false;
    }
    Grass->CreateBodySetup();
    if (UBodySetup* Body = Grass->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Broadleaf->MarkPackageDirty();
    Grass->MarkPackageDirty();
    AssetsToSave.Add(Broadleaf);
    AssetsToSave.Add(Grass);

    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    TSet<FString> UniqueSavePaths;
    if (!Assets || AssetsToSave.Num() != 16)
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V1_ASSETS: the exact 10-texture/4-material/2-mesh save roster is incomplete.");
        return false;
    }
    for (UObject* Object : AssetsToSave)
    {
        if (!Object || UniqueSavePaths.Contains(Object->GetPathName()) ||
            !Assets->SaveLoadedAsset(Object, false))
        {
            OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V1_ASSETS: a duplicate or unsaved object exists in the exact 16-asset roster.");
            return false;
        }
        UniqueSavePaths.Add(Object->GetPathName());
    }
    if (!ValidateImportedAssets(Error))
    {
        OutMessage = TEXT("EXPLORE_V1_IMPORT_INVALID: ") + Error;
        return false;
    }
    OutMessage = TEXT("Imported and validated additive Explore V1 CC0 broadleaf and deterministic near-field grass assets; no V1-V5 package was edited.");
    return true;
}

bool UTRIADIstanaExploreEditorLibrary::BuildIstanaExploreV1Map(
    FString& OutMessage)
{
    FString Error;
    if (HasDirtyPackages(Error))
    {
        OutMessage = TEXT("EXPLORE_V1_BUILD_REFUSED: ") + Error;
        return false;
    }
    bool bRepairedUvMetadata = false;
    if (!ValidateOrRepairImportedAssets(Error, bRepairedUvMetadata))
    {
        OutMessage = TEXT("EXPLORE_V1_BUILD_REFUSED: ") + Error;
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage))
    {
        FString Existing;
        if (ValidateIstanaExploreV1Map(Existing))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V1_MAP_ALREADY_VALID: ") + Existing;
            return true;
        }
        OutMessage = TEXT("EXPLORE_V1_BUILD_REFUSED: destination exists and is not the open exact validated map. ") + Existing;
        return false;
    }
    UWorld* SourceWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    ATRIADIstanaPublicViewSceneActor* SourceScene = FindScene(SourceWorld);
    FString SourceV5Validation;
    if (!SourceWorld || !SourceWorld->GetOutermost() ||
        SourceWorld->GetOutermost()->GetName() != SourceMapPackage ||
        !SourceScene || CountTreeInstances(SourceScene) != 720 ||
        !UTRIADIstanaPublicViewHeroV5EditorLibrary::
            ValidateIstanaPublicViewHeroV5Map(SourceV5Validation))
    {
        OutMessage = TEXT("EXPLORE_V1_BUILD_REFUSED: open the exact frozen/validated V5 public-view source map with 720 legacy trees. ") +
            SourceV5Validation;
        return false;
    }
    TArray<FProtectedMapBytes> ProtectedMapBytes;
    FDistantContextSnapshot SourceDistantContext;
    if (!CaptureProtectedMapBytes(ProtectedMapBytes, Error) ||
        !CaptureDistantContext(
            SourceScene->OSMContextBuildingsComponent.Get(),
            SourceDistantContext,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V1_BUILD_REFUSED: could not freeze V1-V5 maps or the exact distant-building state. ") +
            Error;
        return false;
    }
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    UWorld* TargetWorld = Assets
        ? Cast<UWorld>(Assets->DuplicateLoadedAsset(SourceWorld, DestinationMapPackage))
        : nullptr;
    if (!TargetWorld || !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: V5 duplication failed.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(TargetWorld);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(TargetWorld);
    FDistantContextSnapshot TargetDistantContext;
    UClass* ExploreGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(), nullptr, *ExploreGameModeClassPath);
    if (!Scene || !Policy || !ExploreGameMode ||
        !CaptureDistantContext(
            Scene ? Scene->OSMContextBuildingsComponent.Get() : nullptr,
            TargetDistantContext,
            Error) ||
        !AreDistantContextsIdentical(
            SourceDistantContext,
            TargetDistantContext,
            Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: scene, runtime policy, Explore GameMode, or exact distant-building preservation failed. ") +
            Error;
        return false;
    }
    if (PreserveOnlyOuterLegacyTrees(Scene) != ExpectedPreservedOuterLegacyTrees)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: inner/outer legacy-tree partition changed.");
        return false;
    }

    Policy->Modify();
    Policy->bEnforceFixedPrimaryCamera = false;
    Policy->bRequireIstanaAirSimGameMode = true;
    Policy->RequiredGameModeClassPath = ExploreGameModeClassPath;

    for (TActorIterator<ACameraActor> It(TargetWorld); It; ++It)
    {
        It->Modify();
        if (!SetCameraAutoActivationDisabled(*It, Error))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: ") + Error;
            return false;
        }
    }
    TargetWorld->GetWorldSettings()->Modify();
    TargetWorld->GetWorldSettings()->DefaultGameMode = ExploreGameMode;

    FActorSpawnParameters Parameters;
    Parameters.Name = TEXT("TRIADIstanaExploreLandscapeV1");
    Parameters.OverrideLevel = TargetWorld->PersistentLevel;
    Parameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreLandscapeActor* Landscape =
        TargetWorld->SpawnActor<ATRIADIstanaExploreLandscapeActor>(
            ATRIADIstanaExploreLandscapeActor::StaticClass(),
            FTransform::Identity,
            Parameters);
    if (!Landscape)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: landscape actor could not be spawned.");
        return false;
    }
    Landscape->Tags.AddUnique(ExploreLandscapeTag);
    Landscape->SetActorLabel(TEXT("TRIAD Istana Explore V1 Landscape"));
    if (!Landscape->RecordPreservedDistantContext(
            Scene->OSMContextBuildingsComponent.Get(),
            Error) ||
        !Landscape->ConfigureExploreAssets(
            LoadExact<UStaticMesh>(BroadleafObjectPath),
            LoadExact<UStaticMesh>(ShrubAPath),
            LoadExact<UStaticMesh>(ShrubBPath),
            LoadExact<UStaticMesh>(GroundcoverPath),
            LoadExact<UStaticMesh>(GrassObjectPath),
            LoadExact<UStaticMesh>(CylinderPath),
            Error) ||
        !Landscape->PopulateDeterministicLandscape(Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: ") + Error;
        return false;
    }

    FActorSpawnParameters StartParameters;
    StartParameters.Name = TEXT("TRIADIstanaExplorePlayerStartV1");
    StartParameters.OverrideLevel = TargetWorld->PersistentLevel;
    StartParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FRotator StartRotation(-5.0f, -90.0f, 0.0f);
    APlayerStart* Start = TargetWorld->SpawnActor<APlayerStart>(
        APlayerStart::StaticClass(),
        FVector(0.0, 21000.0, 2400.0),
        StartRotation,
        StartParameters);
    if (!Start)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V1_MAP: player start could not be spawned.");
        return false;
    }
    Start->Tags.AddUnique(ExplorePlayerStartTag);
    Start->SetActorLabel(TEXT("TRIAD Istana Explore Player Start"));
    TargetWorld->MarkPackageDirty();

    FString BeforeSave;
    if (!ValidateWorld(TargetWorld, BeforeSave) ||
        !Assets->SaveLoadedAsset(TargetWorld, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V1_MAP: ") + BeforeSave;
        return false;
    }
    FString Filename;
    if (!FPackageName::DoesPackageExist(DestinationMapPackage, &Filename))
    {
        OutMessage = TEXT("Explore map save returned success but no package exists.");
        return false;
    }
    UWorld* Reloaded = UEditorLoadingAndSavingUtils::LoadMap(Filename);
    FString Persisted;
    if (!ValidateWorld(Reloaded, Persisted) ||
        !ValidateProtectedMapBytesUnchanged(ProtectedMapBytes, Error))
    {
        OutMessage = TEXT("Explore map persisted but exact validation or V1-V5 byte preservation failed: ") +
            (!Error.IsEmpty() ? Error : Persisted);
        return false;
    }
    OutMessage = TEXT("Created additive /Game/Maps/Istana_PublicView_Explore_v1 from V5 with realistic CC0 broadleaf framing, layered project shrubs, clipped hedges, foundation planting, 3D near grass, 560 preserved outer context trees and default free-roam Play; V1-V5 and distant OSM buildings were not overwritten. ") + Persisted;
    return true;
}

bool UTRIADIstanaExploreEditorLibrary::ValidateIstanaExploreV1Map(
    FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateWorld(World, OutReport);
}

bool UTRIADIstanaExploreEditorLibrary::ValidateIstanaExploreV1PlayWorld(
    FString& OutReport)
{
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorReport;
    if (!ValidateWorld(EditorWorld, EditorReport))
    {
        OutReport = TEXT("Explore PIE editor-map validation failed: ") +
            EditorReport;
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE ||
        !PlayWorld->IsGameWorld() || !PlayWorld->HasBegunPlay() ||
        (GEditor && GEditor->IsSimulatingInEditor()) ||
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()) !=
            DestinationMapPackage)
    {
        OutReport = TEXT("Explore PIE is absent, simulated rather than played, not begun, or sourced from the wrong map.");
        return false;
    }

    UClass* ExploreGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(), nullptr, *ExploreGameModeClassPath);
    AGameModeBase* ActiveGameMode = PlayWorld->GetAuthGameMode();
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    ATRIADIstanaFreeRoamPawn* ExplorePawn = PlayerController
        ? Cast<ATRIADIstanaFreeRoamPawn>(PlayerController->GetPawn())
        : nullptr;
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(PlayWorld);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(PlayWorld);
    ATRIADIstanaExploreLandscapeActor* Landscape = FindLandscape(PlayWorld);
    UCameraComponent* ExploreCamera = ExplorePawn
        ? ExplorePawn->GetExploreCameraComponent()
        : nullptr;

    int32 HudCount = 0;
    for (TActorIterator<AHUD> It(PlayWorld); It; ++It)
    {
        ++HudCount;
    }
    FString LandscapeReport;
    FString DistantContextError;
    if (!ExploreGameMode || !ActiveGameMode ||
        ActiveGameMode->GetClass() != ExploreGameMode ||
        ActiveGameMode->HUDClass != nullptr || HudCount != 0 ||
        !PlayerController || !ExplorePawn ||
        PlayerController->GetViewTarget() != ExplorePawn ||
        !ExplorePawn->HasActorBegunPlay() ||
        !ExploreCamera || !ExploreCamera->IsActive() ||
        !ExplorePawn->HasExpectedExploreCameraProfile() ||
        ExploreCamera->GetComponentLocation().ContainsNaN() ||
        ExploreCamera->GetComponentRotation().ContainsNaN() ||
        !Scene || !Policy || !Policy->HasActorBegunPlay() ||
        Policy->bEnforceFixedPrimaryCamera ||
        Policy->RequiredGameModeClassPath != ExploreGameModeClassPath ||
        !Landscape || !Landscape->HasActorBegunPlay() ||
        !Landscape->ValidateExploreLandscape(LandscapeReport) ||
        !Landscape->ValidatePreservedDistantContext(
            Scene ? Scene->OSMContextBuildingsComponent.Get() : nullptr,
            DistantContextError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V1_PLAY_INVALID: free-roam possession/view/manual exposure, visual-only GameMode, policy, greenery, or distant buildings failed. ") +
            (!DistantContextError.IsEmpty()
                ? DistantContextError
                : LandscapeReport);
        return false;
    }

    OutReport = TEXT("Explore PIE is ready: Player 0 owns the collision-aware free-roam pawn and its calibrated manual-exposure camera; WASD, mouse, E/Q, Shift and Ctrl controls are active; the AirSim HUD/vehicle-possession path is disabled only in Explore; deterministic greenery and the preserved distant OSM/HDB layer both validate.");
    return true;
}

bool UTRIADIstanaExploreEditorLibrary::
    QuiesceIstanaExploreV1PlayWorldForStop(FString& OutMessage)
{
    FString Readiness;
    if (!ValidateIstanaExploreV1PlayWorld(Readiness))
    {
        OutMessage = TEXT("Refusing scripted Explore PIE stop because exact runtime identity failed. ") +
            Readiness;
        return false;
    }
    // Explore deliberately has no AirSim HUD or SimMode, so there are no
    // AirSim sensor workers to pause/drain before EditorRequestEndPlay.
    OutMessage = TEXT("Explore PIE identity is exact and no AirSim HUD/SimMode exists; scripted stop may proceed immediately.");
    return true;
}
