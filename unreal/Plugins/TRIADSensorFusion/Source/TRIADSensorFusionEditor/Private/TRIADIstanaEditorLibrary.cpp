#include "TRIADIstanaEditorLibrary.h"

#include "AssetImportTask.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Cesium3DTileset.h"
#include "CesiumCartographicPolygon.h"
#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumPolygonRasterOverlay.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
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
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditorViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInterface.h"
#include "OriginPlacement.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/SceneViewport.h"
#include "StaticMeshCompiler.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADGeodesy.h"
#include "TRIADIstanaBuildingActor.h"
#include "TRIADIstanaExteriorMeshActor.h"
#include "TRIADIstanaRuntimePolicyActor.h"
#include "TRIADIstanaStudyAreaActor.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

namespace
{
const FString SourceMapPackage(TEXT("/Game/SDTH"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_1km"));
const FString DestinationMapV2Package(TEXT("/Game/Maps/Istana_1km_Context_v2"));
const FString LegacyIstanaExteriorMeshObjectPath(TEXT("/Game/TRIAD/Istana/Meshes/SM_IstanaExterior.SM_IstanaExterior"));
const FString IstanaExteriorMeshPackage(TEXT("/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined"));
const FString IstanaExteriorMeshObjectPath(TEXT("/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined.SM_IstanaExterior_Refined"));
const FString IstanaMaterialPackagePath(TEXT("/Game/TRIAD/Istana/Materials"));
const FString IstanaTexturePackagePath(TEXT("/Game/TRIAD/Istana/Textures"));
const FString IstanaTextureSourceRelativePath(TEXT("SourceAssets/Istana/Generated/Textures"));
const FString IstanaTextureManifestName(TEXT("IstanaPbrTextures.manifest.json"));
const FString ExternalAirSimGameModeClassPath(TEXT("/Script/AirSimTriadRuntime.AirSimGameMode"));
const FString IstanaAirSimGameModeClassPath(TEXT("/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode"));
const FName ClipComponentTag(TEXT("TRIADIstana1kmClip"));
const FName PolygonActorTag(TEXT("TRIADIstana1kmClipPolygon"));
const FName RuntimePolicyActorTag(TEXT("TRIADIstanaRuntimeV2Policy"));
const FName RuntimePlayerStartTag(TEXT("TRIADIstanaRuntimeV2PlayerStart"));
const FName RuntimeCameraActorTag(TEXT("TRIADIstanaRuntimeV2Camera"));
constexpr double CenterLongitudeDegrees = 103.84288055;
constexpr double CenterLatitudeDegrees = 1.30709615;
constexpr double ApproximateCenterHeightMeters = 47.0;
constexpr double StudyRadiusMeters = 1000.0;
constexpr int32 PolygonPointCount = 64;
constexpr double FootprintYawDegrees = 2.3;
constexpr double ImportedExteriorYawDegrees = 182.3;
constexpr double PlayerStartSouthOffsetMeters = 180.0;
constexpr double PlayerStartHeightAboveGroundMeters = 8.0;
constexpr double RuntimeCameraSouthOffsetMeters = 210.0;
constexpr double RuntimeCameraHeightAboveGroundMeters = 24.0;
constexpr double RuntimeCameraAimAboveGroundMeters = 13.0;
constexpr float RuntimeCameraFieldOfViewDegrees = 52.0f;
constexpr float MinimumCalibrationTilesetLoadProgress = 99.0f;
constexpr int32 MinimumCalibrationAcceptedSamples = 6;
constexpr int32 RefinedExteriorLod0VertexCount = 175974;
constexpr int32 RefinedExteriorLod0TriangleCount = 58658;
const FString RefinedExteriorLod0Sha256(TEXT("4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91"));

struct FIstanaMaterialSpec
{
    FName SlotName;
    const TCHAR* MaterialAssetName;
    const TCHAR* TextureStem;
    float MaterialCoordinateScale;
    bool bUsesHeight;
    bool bIsGlass;
};

enum class EIstanaTextureUsage : uint8
{
    BaseColor,
    Normal,
    PackedOrm,
    Height
};

struct FIstanaTextureSpec
{
    const TCHAR* AssetName;
    EIstanaTextureUsage Usage;
};

enum class EIstanaPbrAssetState : uint8
{
    Absent,
    CompleteValid,
    PartialOrInvalid
};

const TArray<FIstanaMaterialSpec>& GetIstanaMaterialSpecs()
{
    static const TArray<FIstanaMaterialSpec> Specs = {
        {TEXT("M_Istana_Plaster"), TEXT("M_Istana_Plaster"), TEXT("T_Istana_Plaster"), 0.5f, false, false},
        {TEXT("M_Istana_PlasterTrim"), TEXT("M_Istana_PlasterTrim"), TEXT("T_Istana_PlasterTrim"), 0.5f, false, false},
        {TEXT("M_Istana_Slate"), TEXT("M_Istana_Slate"), TEXT("T_Istana_Slate"), 0.25f, true, false},
        {TEXT("M_Istana_Shutter"), TEXT("M_Istana_Shutter"), TEXT("T_Istana_Shutter"), 0.5f, false, false},
        {TEXT("M_Istana_Glass"), TEXT("M_Istana_Glass"), nullptr, 1.0f, false, true},
        {TEXT("M_Istana_Stone"), TEXT("M_Istana_Stone"), TEXT("T_Istana_Stone"), 0.5f, false, false},
        {TEXT("M_Istana_Metal"), TEXT("M_Istana_Metal"), TEXT("T_Istana_Metal"), 0.5f, false, false},
        {TEXT("M_Istana_Door"), TEXT("M_Istana_Door"), TEXT("T_Istana_Door"), 0.5f, false, false}
    };
    return Specs;
}

const TArray<FIstanaTextureSpec>& GetIstanaTextureSpecs()
{
    static const TArray<FIstanaTextureSpec> Specs = {
        {TEXT("T_Istana_Door_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_Door_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_Door_ORM"), EIstanaTextureUsage::PackedOrm},
        {TEXT("T_Istana_Metal_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_Metal_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_Metal_ORM"), EIstanaTextureUsage::PackedOrm},
        {TEXT("T_Istana_Plaster_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_Plaster_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_Plaster_ORM"), EIstanaTextureUsage::PackedOrm},
        {TEXT("T_Istana_PlasterTrim_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_PlasterTrim_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_PlasterTrim_ORM"), EIstanaTextureUsage::PackedOrm},
        {TEXT("T_Istana_Shutter_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_Shutter_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_Shutter_ORM"), EIstanaTextureUsage::PackedOrm},
        {TEXT("T_Istana_Slate_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_Slate_Height"), EIstanaTextureUsage::Height},
        {TEXT("T_Istana_Slate_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_Slate_ORM"), EIstanaTextureUsage::PackedOrm},
        {TEXT("T_Istana_Stone_BaseColor"), EIstanaTextureUsage::BaseColor},
        {TEXT("T_Istana_Stone_Normal"), EIstanaTextureUsage::Normal},
        {TEXT("T_Istana_Stone_ORM"), EIstanaTextureUsage::PackedOrm}
    };
    return Specs;
}

FString MakeAssetObjectPath(const FString& PackagePath, const TCHAR* AssetName)
{
    return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, AssetName, AssetName);
}

FString MakeMaterialObjectPath(const FIstanaMaterialSpec& Spec)
{
    return MakeAssetObjectPath(IstanaMaterialPackagePath, Spec.MaterialAssetName);
}

FString MakeTextureObjectPath(const FIstanaTextureSpec& Spec)
{
    return MakeAssetObjectPath(IstanaTexturePackagePath, Spec.AssetName);
}

bool DoesAssetOrPackageExist(
    UEditorAssetSubsystem* AssetSubsystem,
    const FString& ObjectPath)
{
    return AssetSubsystem->DoesAssetExist(ObjectPath) ||
        FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath));
}

TextureCompressionSettings GetTextureCompression(EIstanaTextureUsage Usage)
{
    switch (Usage)
    {
        case EIstanaTextureUsage::Normal:
            return TC_Normalmap;
        case EIstanaTextureUsage::PackedOrm:
            return TC_Masks;
        case EIstanaTextureUsage::Height:
            return TC_Grayscale;
        default:
            return TC_Default;
    }
}

bool IsTextureSrgb(EIstanaTextureUsage Usage)
{
    return Usage == EIstanaTextureUsage::BaseColor;
}

TextureGroup GetTextureGroup(EIstanaTextureUsage Usage)
{
    return Usage == EIstanaTextureUsage::Normal
        ? TEXTUREGROUP_WorldNormalMap
        : TEXTUREGROUP_World;
}

FString MakeTextureAssetName(const FIstanaMaterialSpec& Material, const TCHAR* Suffix)
{
    return FString::Printf(TEXT("%s_%s"), Material.TextureStem, Suffix);
}

bool ValidatePbrSourceManifest(FString& OutError)
{
    const FString SourceDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        IstanaTextureSourceRelativePath));
    const FString ManifestPath = FPaths::Combine(SourceDirectory, IstanaTextureManifestName);
    FString ManifestJson;
    if (!FFileHelper::LoadFileToString(ManifestJson, *ManifestPath))
    {
        OutError = FString::Printf(
            TEXT("Required PBR manifest is missing or unreadable at '%s'."),
            *ManifestPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = FString::Printf(TEXT("PBR manifest '%s' is not valid JSON."), *ManifestPath);
        return false;
    }

    FString Schema;
    FString Maps;
    double Resolution = 0.0;
    if (!Root->TryGetStringField(TEXT("schema"), Schema) ||
        Schema != TEXT("triad.istana_pbr_textures.v1") ||
        !Root->TryGetNumberField(TEXT("resolution"), Resolution) ||
        !FMath::IsNearlyEqual(Resolution, 4096.0) ||
        !Root->TryGetStringField(TEXT("maps"), Maps) ||
        Maps != TEXT("baseColor_sRGB; normal_linear_Unreal_DirectX; ORM_linear_R_AO_G_roughness_B_metallic"))
    {
        OutError = TEXT("PBR manifest does not declare the expected v1 4096px sRGB/DirectX-normal/packed-ORM contract.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    if (!Root->TryGetArrayField(TEXT("outputs"), Outputs) || !Outputs)
    {
        OutError = TEXT("PBR manifest has no outputs array.");
        return false;
    }

    TSet<FString> ManifestOutputs;
    for (const TSharedPtr<FJsonValue>& Value : *Outputs)
    {
        const TSharedPtr<FJsonObject>* Output = nullptr;
        FString RelativePath;
        if (Value.IsValid() && Value->TryGetObject(Output) && Output && Output->IsValid() &&
            (*Output)->TryGetStringField(TEXT("path"), RelativePath))
        {
            ManifestOutputs.Add(RelativePath.Replace(TEXT("\\"), TEXT("/")));
        }
    }

    if (ManifestOutputs.Num() != GetIstanaTextureSpecs().Num())
    {
        OutError = FString::Printf(
            TEXT("PBR manifest declares %d outputs; exactly %d are required."),
            ManifestOutputs.Num(),
            GetIstanaTextureSpecs().Num());
        return false;
    }
    for (const FIstanaTextureSpec& Spec : GetIstanaTextureSpecs())
    {
        const FString RelativePath = FString::Printf(
            TEXT("Generated/Textures/%s.png"),
            Spec.AssetName);
        const FString SourcePath = FPaths::Combine(SourceDirectory, FString(Spec.AssetName) + TEXT(".png"));
        if (!ManifestOutputs.Contains(RelativePath) || !FPaths::FileExists(SourcePath))
        {
            OutError = FString::Printf(
                TEXT("PBR source '%s' is missing from the manifest or filesystem."),
                *SourcePath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateIstanaTextureAsset(
    UTexture2D* Texture,
    const FIstanaTextureSpec& Spec,
    FString& OutError)
{
    const FString ExpectedPath = MakeTextureObjectPath(Spec);
    if (!Texture || Texture->GetPathName() != ExpectedPath)
    {
        OutError = FString::Printf(TEXT("Texture '%s' is missing or has the wrong class/path."), *ExpectedPath);
        return false;
    }
    if (Texture->Source.GetSizeX() != 4096 || Texture->Source.GetSizeY() != 4096)
    {
        OutError = FString::Printf(TEXT("Texture '%s' is not 4096 x 4096."), *ExpectedPath);
        return false;
    }
    if (static_cast<bool>(Texture->SRGB) != IsTextureSrgb(Spec.Usage) ||
        Texture->CompressionSettings != GetTextureCompression(Spec.Usage) ||
        Texture->LODGroup != GetTextureGroup(Spec.Usage))
    {
        OutError = FString::Printf(
            TEXT("Texture '%s' does not match its color-space/compression/LOD-group contract."),
            *ExpectedPath);
        return false;
    }
    if (Spec.Usage == EIstanaTextureUsage::Normal && Texture->bFlipGreenChannel)
    {
        OutError = FString::Printf(
            TEXT("DirectX normal '%s' has green-channel flipping enabled; expected +Y data with no import flip."),
            *ExpectedPath);
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterialExpressionTextureSampleParameter2D* CreateIstanaTextureParameter(
    UMaterial* Material,
    UTexture2D* Texture,
    const FName ParameterName,
    EMaterialSamplerType SamplerType,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Expression =
        Cast<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::CreateMaterialExpressionEx(
                Material,
                nullptr,
                UMaterialExpressionTextureSampleParameter2D::StaticClass(),
                Texture,
                X,
                Y));
    if (Expression)
    {
        Expression->Texture = Texture;
        Expression->ParameterName = ParameterName;
        Expression->Group = FName(TEXT("Istana PBR"));
        Expression->SamplerType = SamplerType;
        Expression->AutoSetSampleType();
    }
    return Expression;
}

bool ConnectMaterialExpression(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    UMaterialExpression* To,
    const TCHAR* ToInput,
    const TCHAR* Description,
    FString& OutError)
{
    if (!UMaterialEditingLibrary::ConnectMaterialExpressions(
            From,
            FString(FromOutput),
            To,
            FString(ToInput)))
    {
        const TArray<FString> AvailableInputs =
            UMaterialEditingLibrary::GetMaterialExpressionInputNames(To);
        OutError = FString::Printf(
            TEXT("Could not connect material graph edge '%s' (%s output '%s' -> %s input '%s'; available destination pins: %s)."),
            Description,
            From ? *From->GetClass()->GetName() : TEXT("<null>"),
            FromOutput,
            To ? *To->GetClass()->GetName() : TEXT("<null>"),
            ToInput,
            *FString::Join(AvailableInputs, TEXT(", ")));
        return false;
    }
    return true;
}

bool ConnectMaterialProperty(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    EMaterialProperty Property,
    const TCHAR* Description,
    FString& OutError)
{
    if (!UMaterialEditingLibrary::ConnectMaterialProperty(From, FString(FromOutput), Property))
    {
        OutError = FString::Printf(TEXT("Could not connect material property '%s'."), Description);
        return false;
    }
    return true;
}

UMaterial* CreateOpaqueIstanaMaterial(
    const FIstanaMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    IAssetTools& AssetTools,
    FString& OutError)
{
    const FString BaseColorName = MakeTextureAssetName(Spec, TEXT("BaseColor"));
    const FString NormalName = MakeTextureAssetName(Spec, TEXT("Normal"));
    const FString OrmName = MakeTextureAssetName(Spec, TEXT("ORM"));
    UTexture2D* BaseColor = Textures.FindRef(FName(*BaseColorName));
    UTexture2D* Normal = Textures.FindRef(FName(*NormalName));
    UTexture2D* PackedOrm = Textures.FindRef(FName(*OrmName));
    if (!BaseColor || !Normal || !PackedOrm)
    {
        OutError = FString::Printf(TEXT("Material '%s' is missing one or more required imported textures."), Spec.MaterialAssetName);
        return nullptr;
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Cast<UMaterial>(AssetTools.CreateAsset(
        Spec.MaterialAssetName,
        IstanaMaterialPackagePath,
        UMaterial::StaticClass(),
        Factory,
        FName(TEXT("TRIAD.ImportIstanaPbrMaterials"))));
    if (!Material)
    {
        OutError = FString::Printf(TEXT("Could not create material '%s'."), *MakeMaterialObjectPath(Spec));
        return nullptr;
    }

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;

    UMaterialExpressionTextureCoordinate* TextureCoordinate =
        Cast<UMaterialExpressionTextureCoordinate>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionTextureCoordinate::StaticClass(),
                -1200,
                -50));
    UMaterialExpressionTextureSampleParameter2D* BaseColorSample =
        CreateIstanaTextureParameter(Material, BaseColor, TEXT("BaseColorTexture"), SAMPLERTYPE_Color, -350, -300);
    UMaterialExpressionTextureSampleParameter2D* NormalSample =
        CreateIstanaTextureParameter(Material, Normal, TEXT("NormalTexture"), SAMPLERTYPE_Normal, -350, 0);
    UMaterialExpressionTextureSampleParameter2D* OrmSample =
        CreateIstanaTextureParameter(Material, PackedOrm, TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, -350, 300);
    if (!TextureCoordinate || !BaseColorSample || !NormalSample || !OrmSample)
    {
        OutError = FString::Printf(TEXT("Could not allocate the PBR expression graph for '%s'."), Spec.MaterialAssetName);
        return nullptr;
    }
    TextureCoordinate->CoordinateIndex = 0;
    TextureCoordinate->UTiling = Spec.MaterialCoordinateScale;
    TextureCoordinate->VTiling = Spec.MaterialCoordinateScale;

    UMaterialExpression* SampleCoordinates = TextureCoordinate;
    if (Spec.bUsesHeight)
    {
        const FString HeightName = MakeTextureAssetName(Spec, TEXT("Height"));
        UTexture2D* Height = Textures.FindRef(FName(*HeightName));
        UMaterialExpressionTextureSampleParameter2D* HeightSample =
            CreateIstanaTextureParameter(
                Material,
                Height,
                TEXT("HeightTexture"),
                SAMPLERTYPE_LinearGrayscale,
                -900,
                500);
        UMaterialExpressionBumpOffset* BumpOffset =
            Cast<UMaterialExpressionBumpOffset>(
                UMaterialEditingLibrary::CreateMaterialExpression(
                    Material,
                    UMaterialExpressionBumpOffset::StaticClass(),
                    -600,
                    500));
        if (!Height || !HeightSample || !BumpOffset)
        {
            OutError = TEXT("Slate material could not allocate its conservative parallax-height graph.");
            return nullptr;
        }
        BumpOffset->HeightRatio = 0.006f;
        BumpOffset->ReferencePlane = 0.5f;
        // UE 5.5 shortens UMaterialExpressionTextureSample::Coordinates to the
        // public Material Editing Library pin name "UVs".
        if (!ConnectMaterialExpression(TextureCoordinate, TEXT(""), HeightSample, TEXT("UVs"), TEXT("UV to slate height"), OutError) ||
            !ConnectMaterialExpression(TextureCoordinate, TEXT(""), BumpOffset, TEXT("Coordinate"), TEXT("UV to slate bump offset"), OutError) ||
            !ConnectMaterialExpression(HeightSample, TEXT("R"), BumpOffset, TEXT("Height"), TEXT("slate height R to bump offset"), OutError))
        {
            return nullptr;
        }
        SampleCoordinates = BumpOffset;
    }

    if (!ConnectMaterialExpression(SampleCoordinates, TEXT(""), BaseColorSample, TEXT("UVs"), TEXT("UV to base color"), OutError) ||
        !ConnectMaterialExpression(SampleCoordinates, TEXT(""), NormalSample, TEXT("UVs"), TEXT("UV to normal"), OutError) ||
        !ConnectMaterialExpression(SampleCoordinates, TEXT(""), OrmSample, TEXT("UVs"), TEXT("UV to packed ORM"), OutError) ||
        !ConnectMaterialProperty(BaseColorSample, TEXT("RGB"), MP_BaseColor, TEXT("Base Color RGB"), OutError) ||
        !ConnectMaterialProperty(NormalSample, TEXT("RGB"), MP_Normal, TEXT("DirectX Normal RGB"), OutError) ||
        !ConnectMaterialProperty(OrmSample, TEXT("R"), MP_AmbientOcclusion, TEXT("ORM R to Ambient Occlusion"), OutError) ||
        !ConnectMaterialProperty(OrmSample, TEXT("G"), MP_Roughness, TEXT("ORM G to Roughness"), OutError) ||
        !ConnectMaterialProperty(OrmSample, TEXT("B"), MP_Metallic, TEXT("ORM B to Metallic"), OutError))
    {
        return nullptr;
    }

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->MarkPackageDirty();
    return Material;
}

UMaterial* CreateGlassIstanaMaterial(
    const FIstanaMaterialSpec& Spec,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Cast<UMaterial>(AssetTools.CreateAsset(
        Spec.MaterialAssetName,
        IstanaMaterialPackagePath,
        UMaterial::StaticClass(),
        Factory,
        FName(TEXT("TRIAD.ImportIstanaPbrMaterials"))));
    if (!Material)
    {
        OutError = FString::Printf(TEXT("Could not create glazing material '%s'."), *MakeMaterialObjectPath(Spec));
        return nullptr;
    }

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Translucent;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
    Material->RefractionMethod = RM_IndexOfRefraction;
    Material->TwoSided = true;
    Material->bScreenSpaceReflections = true;

    UMaterialExpressionConstant3Vector* Tint = Cast<UMaterialExpressionConstant3Vector>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionConstant3Vector::StaticClass(),
            -350,
            -250));
    UMaterialExpressionConstant* Opacity = Cast<UMaterialExpressionConstant>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionConstant::StaticClass(),
            -350,
            -100));
    UMaterialExpressionConstant* Roughness = Cast<UMaterialExpressionConstant>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionConstant::StaticClass(),
            -350,
            50));
    UMaterialExpressionConstant* Specular = Cast<UMaterialExpressionConstant>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionConstant::StaticClass(),
            -350,
            200));
    UMaterialExpressionConstant* Refraction = Cast<UMaterialExpressionConstant>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionConstant::StaticClass(),
            -350,
            350));
    if (!Tint || !Opacity || !Roughness || !Specular || !Refraction)
    {
        OutError = TEXT("Could not allocate the glazing material graph.");
        return nullptr;
    }
    Tint->Constant = FLinearColor(0.08f, 0.13f, 0.18f);
    Opacity->R = 0.32f;
    Roughness->R = 0.12f;
    Specular->R = 0.5f;
    Refraction->R = 1.52f;

    if (!ConnectMaterialProperty(Tint, TEXT(""), MP_BaseColor, TEXT("glass tint"), OutError) ||
        !ConnectMaterialProperty(Opacity, TEXT(""), MP_Opacity, TEXT("glass opacity"), OutError) ||
        !ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness, TEXT("glass roughness"), OutError) ||
        !ConnectMaterialProperty(Specular, TEXT(""), MP_Specular, TEXT("glass specular"), OutError) ||
        !ConnectMaterialProperty(Refraction, TEXT(""), MP_Refraction, TEXT("glass index of refraction"), OutError))
    {
        return nullptr;
    }

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->MarkPackageDirty();
    return Material;
}

bool ValidateTexturePropertyBinding(
    UMaterial* Material,
    EMaterialProperty Property,
    const TCHAR* OutputName,
    UTexture2D* ExpectedTexture,
    const FName ExpectedParameter,
    EMaterialSamplerType ExpectedSamplerType,
    FString& OutError)
{
    UMaterialExpressionTextureSampleParameter2D* Sample =
        Cast<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Property));
    if (!Sample || Sample->Texture != ExpectedTexture || Sample->ParameterName != ExpectedParameter ||
        Sample->SamplerType != ExpectedSamplerType ||
        UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(Material, Property) != OutputName)
    {
        OutError = FString::Printf(
            TEXT("Material '%s' property %d is not bound to %s output '%s'."),
            *Material->GetPathName(),
            static_cast<int32>(Property),
            *ExpectedTexture->GetPathName(),
            OutputName);
        return false;
    }
    return true;
}

bool ValidateIstanaMaterialAsset(
    UMaterial* Material,
    const FIstanaMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    const FString ExpectedPath = MakeMaterialObjectPath(Spec);
    if (!Material || Material->GetPathName() != ExpectedPath)
    {
        OutError = FString::Printf(TEXT("Material '%s' is missing or has the wrong class/path."), *ExpectedPath);
        return false;
    }

    if (Spec.bIsGlass)
    {
        if (Material->MaterialDomain != MD_Surface ||
            Material->BlendMode != BLEND_Translucent ||
            !Material->GetShadingModels().HasShadingModel(MSM_DefaultLit) ||
            !Material->TwoSided || !Material->bScreenSpaceReflections ||
            Material->TranslucencyLightingMode != TLM_SurfacePerPixelLighting ||
            Material->RefractionMethod != RM_IndexOfRefraction ||
            !Material->IsPropertyConnected(MP_BaseColor) ||
            !Material->IsPropertyConnected(MP_Opacity) ||
            !Material->IsPropertyConnected(MP_Roughness) ||
            !Material->IsPropertyConnected(MP_Specular) ||
            !Material->IsPropertyConnected(MP_Refraction))
        {
            OutError = TEXT("M_Istana_Glass is not the expected two-sided, lit translucent glazing material.");
            return false;
        }
        UMaterialExpressionConstant* Opacity = Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Opacity));
        UMaterialExpressionConstant* Roughness = Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Roughness));
        UMaterialExpressionConstant* Specular = Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Specular));
        UMaterialExpressionConstant* Refraction = Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Refraction));
        UMaterialExpressionConstant3Vector* Tint = Cast<UMaterialExpressionConstant3Vector>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_BaseColor));
        if (!Opacity || !Roughness || !Specular || !Refraction || !Tint ||
            !FMath::IsNearlyEqual(Opacity->R, 0.32f) ||
            !FMath::IsNearlyEqual(Roughness->R, 0.12f) ||
            !FMath::IsNearlyEqual(Specular->R, 0.5f) ||
            !Tint->Constant.Equals(FLinearColor(0.08f, 0.13f, 0.18f), KINDA_SMALL_NUMBER) ||
            !FMath::IsNearlyEqual(Refraction->R, 1.52f))
        {
            OutError = TEXT("M_Istana_Glass tint, opacity, roughness, specular, or IOR differs from the authored contract.");
            return false;
        }
        OutError.Reset();
        return true;
    }

    UTexture2D* BaseColor = Textures.FindRef(FName(*MakeTextureAssetName(Spec, TEXT("BaseColor"))));
    UTexture2D* Normal = Textures.FindRef(FName(*MakeTextureAssetName(Spec, TEXT("Normal"))));
    UTexture2D* PackedOrm = Textures.FindRef(FName(*MakeTextureAssetName(Spec, TEXT("ORM"))));
    if (Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->GetShadingModels().HasShadingModel(MSM_DefaultLit) ||
        !BaseColor || !Normal || !PackedOrm ||
        !ValidateTexturePropertyBinding(Material, MP_BaseColor, TEXT("RGB"), BaseColor, TEXT("BaseColorTexture"), SAMPLERTYPE_Color, OutError) ||
        !ValidateTexturePropertyBinding(Material, MP_Normal, TEXT("RGB"), Normal, TEXT("NormalTexture"), SAMPLERTYPE_Normal, OutError) ||
        !ValidateTexturePropertyBinding(Material, MP_AmbientOcclusion, TEXT("R"), PackedOrm, TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, OutError) ||
        !ValidateTexturePropertyBinding(Material, MP_Roughness, TEXT("G"), PackedOrm, TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, OutError) ||
        !ValidateTexturePropertyBinding(Material, MP_Metallic, TEXT("B"), PackedOrm, TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, OutError))
    {
        return false;
    }

    UMaterialExpressionTextureCoordinate* TextureCoordinate = nullptr;
    UMaterialExpressionBumpOffset* BumpOffset = nullptr;
    UMaterialExpressionTextureSampleParameter2D* HeightSample = nullptr;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        TextureCoordinate = TextureCoordinate ? TextureCoordinate : Cast<UMaterialExpressionTextureCoordinate>(Expression);
        BumpOffset = BumpOffset ? BumpOffset : Cast<UMaterialExpressionBumpOffset>(Expression);
        UMaterialExpressionTextureSampleParameter2D* TextureParameter =
            Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
        if (TextureParameter && TextureParameter->ParameterName == TEXT("HeightTexture"))
        {
            HeightSample = TextureParameter;
        }
    }
    if (!TextureCoordinate ||
        !FMath::IsNearlyEqual(TextureCoordinate->UTiling, Spec.MaterialCoordinateScale) ||
        !FMath::IsNearlyEqual(TextureCoordinate->VTiling, Spec.MaterialCoordinateScale))
    {
        OutError = FString::Printf(TEXT("Material '%s' has the wrong material-coordinate scale."), *ExpectedPath);
        return false;
    }

    TSet<UTexture*> ExpectedTextures;
    ExpectedTextures.Add(BaseColor);
    ExpectedTextures.Add(Normal);
    ExpectedTextures.Add(PackedOrm);
    if (Spec.bUsesHeight)
    {
        UTexture2D* Height = Textures.FindRef(FName(*MakeTextureAssetName(Spec, TEXT("Height"))));
        const TArray<UMaterialExpression*> BumpInputs = BumpOffset
            ? UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, BumpOffset)
            : TArray<UMaterialExpression*>();
        const TArray<UMaterialExpression*> HeightInputs = HeightSample
            ? UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, HeightSample)
            : TArray<UMaterialExpression*>();
        if (!Height || !HeightSample || HeightSample->Texture != Height ||
            HeightSample->SamplerType != SAMPLERTYPE_LinearGrayscale || !BumpOffset ||
            !BumpInputs.Contains(TextureCoordinate) || !BumpInputs.Contains(HeightSample) ||
            !HeightInputs.Contains(TextureCoordinate) ||
            !FMath::IsNearlyEqual(BumpOffset->HeightRatio, 0.006f) ||
            !FMath::IsNearlyEqual(BumpOffset->ReferencePlane, 0.5f))
        {
            OutError = TEXT("M_Istana_Slate is missing its conservative HeightTexture/BumpOffset parallax binding.");
            return false;
        }
        ExpectedTextures.Add(Height);
    }
    else if (HeightSample || BumpOffset)
    {
        OutError = FString::Printf(TEXT("Material '%s' unexpectedly contains a height/parallax graph."), *ExpectedPath);
        return false;
    }

    UMaterialExpression* ExpectedSampleCoordinates = Spec.bUsesHeight
        ? static_cast<UMaterialExpression*>(BumpOffset)
        : static_cast<UMaterialExpression*>(TextureCoordinate);
    UMaterialExpression* PropertySamples[] = {
        UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_BaseColor),
        UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Normal),
        UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_AmbientOcclusion)
    };
    for (UMaterialExpression* PropertySample : PropertySamples)
    {
        if (!PropertySample ||
            !UMaterialEditingLibrary::GetInputsForMaterialExpression(
                Material,
                PropertySample).Contains(ExpectedSampleCoordinates))
        {
            OutError = FString::Printf(
                TEXT("Material '%s' texture samples are not driven by the validated UV/parallax coordinates."),
                *ExpectedPath);
            return false;
        }
    }

    TSet<UTexture*> UsedTextures;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (UMaterialExpressionTextureSampleParameter2D* TextureParameter =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            UsedTextures.Add(TextureParameter->Texture);
        }
    }
    if (UsedTextures.Num() != ExpectedTextures.Num())
    {
        OutError = FString::Printf(TEXT("Material '%s' uses an unexpected number of textures."), *ExpectedPath);
        return false;
    }
    for (UTexture* ExpectedTexture : ExpectedTextures)
    {
        if (!UsedTextures.Contains(ExpectedTexture))
        {
            OutError = FString::Printf(TEXT("Material '%s' does not use expected texture '%s'."), *ExpectedPath, *ExpectedTexture->GetPathName());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

EIstanaPbrAssetState InspectIstanaPbrAssetSet(
    UEditorAssetSubsystem* AssetSubsystem,
    TMap<FName, UMaterialInterface*>* OutMaterials,
    FString& OutReport)
{
    if (OutMaterials)
    {
        OutMaterials->Reset();
    }

    int32 ExistingCount = 0;
    FString FirstExisting;
    FString FirstMissing;
    for (const FIstanaTextureSpec& Spec : GetIstanaTextureSpecs())
    {
        const FString Path = MakeTextureObjectPath(Spec);
        if (DoesAssetOrPackageExist(AssetSubsystem, Path))
        {
            ++ExistingCount;
            FirstExisting = FirstExisting.IsEmpty() ? Path : FirstExisting;
        }
        else
        {
            FirstMissing = FirstMissing.IsEmpty() ? Path : FirstMissing;
        }
    }
    for (const FIstanaMaterialSpec& Spec : GetIstanaMaterialSpecs())
    {
        const FString Path = MakeMaterialObjectPath(Spec);
        if (DoesAssetOrPackageExist(AssetSubsystem, Path))
        {
            ++ExistingCount;
            FirstExisting = FirstExisting.IsEmpty() ? Path : FirstExisting;
        }
        else
        {
            FirstMissing = FirstMissing.IsEmpty() ? Path : FirstMissing;
        }
    }

    const int32 ExpectedCount = GetIstanaTextureSpecs().Num() + GetIstanaMaterialSpecs().Num();
    if (ExistingCount == 0)
    {
        OutReport = TEXT("No Istana PBR texture or material target assets exist.");
        return EIstanaPbrAssetState::Absent;
    }
    if (ExistingCount != ExpectedCount)
    {
        OutReport = FString::Printf(
            TEXT("Refusing a partial Istana PBR asset set (%d/%d present). First existing: '%s'. First missing: '%s'. No target will be overwritten; remove or rename the partial set deliberately before retrying."),
            ExistingCount,
            ExpectedCount,
            *FirstExisting,
            *FirstMissing);
        return EIstanaPbrAssetState::PartialOrInvalid;
    }

    TMap<FName, UTexture2D*> Textures;
    FString Error;
    for (const FIstanaTextureSpec& Spec : GetIstanaTextureSpecs())
    {
        UTexture2D* Texture = Cast<UTexture2D>(AssetSubsystem->LoadAsset(MakeTextureObjectPath(Spec)));
        if (!ValidateIstanaTextureAsset(Texture, Spec, Error))
        {
            OutReport = TEXT("Existing target will not be overwritten: ") + Error;
            return EIstanaPbrAssetState::PartialOrInvalid;
        }
        Textures.Add(FName(Spec.AssetName), Texture);
    }
    for (const FIstanaMaterialSpec& Spec : GetIstanaMaterialSpecs())
    {
        UMaterial* Material = Cast<UMaterial>(AssetSubsystem->LoadAsset(MakeMaterialObjectPath(Spec)));
        if (!ValidateIstanaMaterialAsset(Material, Spec, Textures, Error))
        {
            OutReport = TEXT("Existing target will not be overwritten: ") + Error;
            return EIstanaPbrAssetState::PartialOrInvalid;
        }
        if (OutMaterials)
        {
            OutMaterials->Add(Spec.SlotName, Material);
        }
    }

    OutReport = FString::Printf(
        TEXT("Validated %d project-owned 4096px PBR textures and %d exact-slot materials; packed ORM is R=AO, G=roughness, B=metallic, normals are DirectX, and slate height drives conservative BumpOffset parallax."),
        GetIstanaTextureSpecs().Num(),
        GetIstanaMaterialSpecs().Num());
    return EIstanaPbrAssetState::CompleteValid;
}

bool BuildIstanaPbrAssetSet(
    IAssetTools& AssetTools,
    UEditorAssetSubsystem* AssetSubsystem,
    TArray<UObject*>& OutAssetsToSave,
    TMap<FName, UMaterialInterface*>& OutMaterials,
    FString& OutError)
{
    if (!ValidatePbrSourceManifest(OutError))
    {
        return false;
    }
    for (const FIstanaTextureSpec& Spec : GetIstanaTextureSpecs())
    {
        if (DoesAssetOrPackageExist(AssetSubsystem, MakeTextureObjectPath(Spec)))
        {
            OutError = FString::Printf(TEXT("Refusing to overwrite texture '%s'."), *MakeTextureObjectPath(Spec));
            return false;
        }
    }
    for (const FIstanaMaterialSpec& Spec : GetIstanaMaterialSpecs())
    {
        if (DoesAssetOrPackageExist(AssetSubsystem, MakeMaterialObjectPath(Spec)))
        {
            OutError = FString::Printf(TEXT("Refusing to overwrite material '%s'."), *MakeMaterialObjectPath(Spec));
            return false;
        }
    }

    const FString SourceDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        IstanaTextureSourceRelativePath));
    TArray<UAssetImportTask*> ImportTasks;
    TMap<FName, UAssetImportTask*> TaskByTextureName;
    for (const FIstanaTextureSpec& Spec : GetIstanaTextureSpecs())
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task)
        {
            OutError = TEXT("Could not allocate deterministic texture import tasks.");
            return false;
        }
        Factory->bCreateMaterial = false;
        Factory->CompressionSettings = GetTextureCompression(Spec.Usage);
        Factory->LODGroup = GetTextureGroup(Spec.Usage);
        Factory->MipGenSettings = TMGS_FromTextureGroup;
        Factory->bFlipNormalMapGreenChannel = false;
        Factory->ColorSpaceMode = IsTextureSrgb(Spec.Usage)
            ? ETextureSourceColorSpace::SRGB
            : ETextureSourceColorSpace::Linear;

        Task->Filename = FPaths::Combine(SourceDirectory, FString(Spec.AssetName) + TEXT(".png"));
        Task->DestinationPath = IstanaTexturePackagePath;
        Task->DestinationName = Spec.AssetName;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Task->Factory = Factory;
        Task->Options = Factory;
        ImportTasks.Add(Task);
        TaskByTextureName.Add(FName(Spec.AssetName), Task);
    }
    AssetTools.ImportAssetTasks(ImportTasks);

    TMap<FName, UTexture2D*> Textures;
    for (const FIstanaTextureSpec& Spec : GetIstanaTextureSpecs())
    {
        UTexture2D* Texture = nullptr;
        UAssetImportTask* const* Task = TaskByTextureName.Find(FName(Spec.AssetName));
        if (Task && *Task)
        {
            for (UObject* ImportedObject : (*Task)->GetObjects())
            {
                Texture = Cast<UTexture2D>(ImportedObject);
                if (Texture)
                {
                    break;
                }
            }
        }
        if (!Texture)
        {
            Texture = LoadObject<UTexture2D>(nullptr, *MakeTextureObjectPath(Spec));
        }
        if (!Texture)
        {
            OutError = FString::Printf(TEXT("Texture import produced no asset for '%s'."), Spec.AssetName);
            return false;
        }

        Texture->Modify();
        Texture->SRGB = IsTextureSrgb(Spec.Usage);
        Texture->CompressionSettings = GetTextureCompression(Spec.Usage);
        Texture->LODGroup = GetTextureGroup(Spec.Usage);
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->bFlipGreenChannel = false;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();

        if (!ValidateIstanaTextureAsset(Texture, Spec, OutError))
        {
            return false;
        }
        Textures.Add(FName(Spec.AssetName), Texture);
        OutAssetsToSave.Add(Texture);
    }

    for (const FIstanaMaterialSpec& Spec : GetIstanaMaterialSpecs())
    {
        UMaterial* Material = Spec.bIsGlass
            ? CreateGlassIstanaMaterial(Spec, AssetTools, OutError)
            : CreateOpaqueIstanaMaterial(Spec, Textures, AssetTools, OutError);
        if (!Material || !ValidateIstanaMaterialAsset(Material, Spec, Textures, OutError))
        {
            return false;
        }
        OutMaterials.Add(Spec.SlotName, Material);
        OutAssetsToSave.Add(Material);
    }
    return true;
}

bool ReadManifestVector3(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FieldName,
    FVector& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!Object.IsValid() ||
        !Object->TryGetArrayField(FieldName, Values) || !Values ||
        Values->Num() != 3 ||
        !(*Values)[0].IsValid() || !(*Values)[0]->TryGetNumber(X) ||
        !(*Values)[1].IsValid() || !(*Values)[1]->TryGetNumber(Y) ||
        !(*Values)[2].IsValid() || !(*Values)[2]->TryGetNumber(Z) ||
        !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        return false;
    }
    OutValue = FVector(X, Y, Z);
    return true;
}

bool ValidateRefinedIstanaSourceManifest(
    const FString& SourceObjPath,
    FString& OutError)
{
    const FString ManifestPath = FPaths::Combine(
        FPaths::GetPath(SourceObjPath),
        TEXT("IstanaExterior.manifest.json"));
    FString ManifestText;
    if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath))
    {
        OutError = FString::Printf(
            TEXT("Refined exterior manifest is missing or unreadable at '%s'."),
            *ManifestPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(ManifestText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Refined exterior source manifest is not valid JSON.");
        return false;
    }

    FString Schema;
    FString CoordinateSystem;
    FString AssetName;
    FString AuthoringUnits;
    FString EncodedObjUnits;
    FString UnrealUnits;
    if (!Root->TryGetStringField(TEXT("schema"), Schema) ||
        Schema != TEXT("triad.istana_source_assets.v1") ||
        !Root->TryGetStringField(TEXT("coordinateSystem"), CoordinateSystem) ||
        CoordinateSystem != TEXT("right-handed Z-up; ceremonial approach +Y") ||
        !Root->TryGetStringField(TEXT("authoringUnits"), AuthoringUnits) ||
        AuthoringUnits != TEXT("metres") ||
        !Root->TryGetStringField(TEXT("encodedObjUnits"), EncodedObjUnits) ||
        EncodedObjUnits != TEXT("centimetres") ||
        !Root->TryGetStringField(TEXT("unrealUnits"), UnrealUnits) ||
        UnrealUnits != TEXT("centimetres (import scale 1.0)") ||
        !Root->TryGetStringField(TEXT("assetName"), AssetName) ||
        AssetName != TEXT("SM_IstanaExterior"))
    {
        OutError = TEXT("Refined exterior manifest does not match the expected v1 Z-up/source-+Y contract.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* MaterialSlots = nullptr;
    if (!Root->TryGetArrayField(TEXT("materialSlots"), MaterialSlots) ||
        !MaterialSlots ||
        MaterialSlots->Num() != GetIstanaMaterialSpecs().Num())
    {
        OutError = TEXT("Refined exterior manifest does not declare exactly eight material slots.");
        return false;
    }
    for (int32 Index = 0; Index < GetIstanaMaterialSpecs().Num(); ++Index)
    {
        FString SlotName;
        if (!(*MaterialSlots)[Index].IsValid() ||
            !(*MaterialSlots)[Index]->TryGetString(SlotName) ||
            SlotName != GetIstanaMaterialSpecs()[Index].SlotName.ToString())
        {
            OutError = FString::Printf(
                TEXT("Refined exterior manifest material slot %d is not exact ordered slot '%s'."),
                Index,
                *GetIstanaMaterialSpecs()[Index].SlotName.ToString());
            return false;
        }
    }

    const TSharedPtr<FJsonObject>* ReferenceMassing = nullptr;
    const TSharedPtr<FJsonObject>* FacadeSignature = nullptr;
    bool bHasCupolaOrDome = true;
    bool bHasExposedBrickFacade = true;
    FString FoundationForm;
    double EndPedimentCount = 0.0;
    double DeepEntranceArchCount = 0.0;
    double TowerLouvreBayCount = 0.0;
    double FrontDormerCount = 0.0;
    if (!Root->TryGetObjectField(TEXT("referenceMassing"), ReferenceMassing) ||
        !ReferenceMassing || !ReferenceMassing->IsValid() ||
        !(*ReferenceMassing)->TryGetStringField(TEXT("foundationForm"), FoundationForm) ||
        !FoundationForm.Contains(TEXT("0.15 m maximum visible")) ||
        !(*ReferenceMassing)->TryGetBoolField(TEXT("hasCupolaOrDome"), bHasCupolaOrDome) ||
        bHasCupolaOrDome ||
        !(*ReferenceMassing)->TryGetBoolField(TEXT("hasExposedBrickFacade"), bHasExposedBrickFacade) ||
        bHasExposedBrickFacade ||
        !(*ReferenceMassing)->TryGetObjectField(TEXT("ceremonialFacadeSignature"), FacadeSignature) ||
        !FacadeSignature || !FacadeSignature->IsValid() ||
        !(*FacadeSignature)->TryGetNumberField(TEXT("endPorticoTriangularPediments"), EndPedimentCount) ||
        !(*FacadeSignature)->TryGetNumberField(TEXT("deepEntranceRoundArches"), DeepEntranceArchCount) ||
        !(*FacadeSignature)->TryGetNumberField(TEXT("principalCentralTowerLouvreBaysLod0"), TowerLouvreBayCount) ||
        !(*FacadeSignature)->TryGetNumberField(TEXT("frontTowerDormers"), FrontDormerCount) ||
        EndPedimentCount != 2.0 || DeepEntranceArchCount != 3.0 ||
        TowerLouvreBayCount != 4.0 || FrontDormerCount != 1.0)
    {
        OutError = TEXT("Refined exterior manifest lacks the corrected low-plinth/pediment/deep-three-arch/four-louvre/one-front-dormer facade signature.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    TSharedPtr<FJsonObject> Lod0;
    int32 Lod0Count = 0;
    if (Root->TryGetArrayField(TEXT("files"), Files) && Files)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Files)
        {
            const TSharedPtr<FJsonObject>* FileObject = nullptr;
            FString Role;
            if (Value.IsValid() && Value->TryGetObject(FileObject) &&
                FileObject && FileObject->IsValid() &&
                (*FileObject)->TryGetStringField(TEXT("role"), Role) &&
                Role == TEXT("visual_lod_0"))
            {
                Lod0 = *FileObject;
                ++Lod0Count;
            }
        }
    }

    FString Lod0Path;
    FString DeclaredSha256;
    double VertexCount = 0.0;
    double TriangleCount = 0.0;
    FVector BoundsMin;
    FVector BoundsMax;
    if (Lod0Count != 1 || !Lod0.IsValid() ||
        !Lod0->TryGetStringField(TEXT("path"), Lod0Path) ||
        Lod0Path != TEXT("SM_IstanaExterior_LOD0.obj") ||
        !Lod0->TryGetNumberField(TEXT("vertices"), VertexCount) ||
        VertexCount != RefinedExteriorLod0VertexCount ||
        !Lod0->TryGetNumberField(TEXT("triangles"), TriangleCount) ||
        TriangleCount != RefinedExteriorLod0TriangleCount ||
        !ReadManifestVector3(Lod0, TEXT("boundsMinMeters"), BoundsMin) ||
        !ReadManifestVector3(Lod0, TEXT("boundsMaxMeters"), BoundsMax) ||
        !BoundsMin.Equals(FVector(-62.5, -52.0, -0.85), 0.000001) ||
        !BoundsMax.Equals(FVector(62.5, 65.69444444444444, 36.0), 0.000001) ||
        !Lod0->TryGetStringField(TEXT("sha256"), DeclaredSha256) ||
        !DeclaredSha256.Equals(RefinedExteriorLod0Sha256, ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Refined exterior LOD0 manifest metrics, bounds, path, or digest do not match the reviewed source contract.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool ValidateIstanaExteriorMeshAsset(
    UStaticMesh* Mesh,
    bool bRequireAssignedProjectMaterials,
    FString& OutError,
    const FString& ExpectedObjectPath = IstanaExteriorMeshObjectPath)
{
    if (!Mesh)
    {
        OutError = FString::Printf(
            TEXT("Required asset '%s' is missing. Run ImportIstanaExteriorRefinedLod0 first."),
            *ExpectedObjectPath);
        return false;
    }
    if (Mesh->GetPathName() != ExpectedObjectPath)
    {
        OutError = FString::Printf(
            TEXT("Imported exterior has unexpected object path '%s'; expected '%s'."),
            *Mesh->GetPathName(),
            *ExpectedObjectPath);
        return false;
    }

    const FVector SizeCentimeters = Mesh->GetBounds().BoxExtent * 2.0;
    const bool bRefinedAsset = ExpectedObjectPath == IstanaExteriorMeshObjectPath;
    const FVector ExpectedSizeCentimeters = bRefinedAsset
        ? FVector(12500.0, 11769.4444, 3685.0)
        : FVector(12400.0, 11819.4444, 3600.0);
    const FVector ToleranceCentimeters = bRefinedAsset
        ? FVector(25.0, 25.0, 10.0)
        : FVector(124.0, 120.0, 50.0);
    if (FMath::Abs(SizeCentimeters.X - ExpectedSizeCentimeters.X) > ToleranceCentimeters.X ||
        FMath::Abs(SizeCentimeters.Y - ExpectedSizeCentimeters.Y) > ToleranceCentimeters.Y ||
        FMath::Abs(SizeCentimeters.Z - ExpectedSizeCentimeters.Z) > ToleranceCentimeters.Z)
    {
        OutError = FString::Printf(
            TEXT("Istana mesh bounds are %.1f x %.1f x %.1f cm; expected approximately %.1f x %.1f x %.1f cm. Check the reviewed manifest, OBJ scale 1.0, and Z-up import."),
            SizeCentimeters.X,
            SizeCentimeters.Y,
            SizeCentimeters.Z,
            ExpectedSizeCentimeters.X,
            ExpectedSizeCentimeters.Y,
            ExpectedSizeCentimeters.Z);
        return false;
    }

    if (bRefinedAsset &&
        Mesh->GetNumTriangles(0) != RefinedExteriorLod0TriangleCount)
    {
        OutError = FString::Printf(
            TEXT("Refined Istana mesh has %d triangles; expected exact reviewed LOD0 metric %d. The source-manifest/hash check separately proves %d OBJ vertices."),
            Mesh->GetNumTriangles(0),
            RefinedExteriorLod0TriangleCount,
            RefinedExteriorLod0VertexCount);
        return false;
    }

    const UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (!BodySetup || BodySetup->AggGeom.GetElementCount() == 0)
    {
        OutError = TEXT("Istana mesh has no simple collision. Reimport the reviewed OBJ with Generate Missing Collision enabled.");
        return false;
    }

    const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
    if (StaticMaterials.Num() != GetIstanaMaterialSpecs().Num())
    {
        OutError = FString::Printf(
            TEXT("Istana mesh declares %d material slots; exactly %d named slots are required."),
            StaticMaterials.Num(),
            GetIstanaMaterialSpecs().Num());
        return false;
    }
    for (const FIstanaMaterialSpec& Spec : GetIstanaMaterialSpecs())
    {
        const int32 MaterialIndex = Mesh->GetMaterialIndex(Spec.SlotName);
        if (MaterialIndex == INDEX_NONE)
        {
            OutError = FString::Printf(
                TEXT("Istana mesh is missing required material slot '%s'."),
                *Spec.SlotName.ToString());
            return false;
        }
        if (bRequireAssignedProjectMaterials)
        {
            UMaterialInterface* Material = Mesh->GetMaterial(MaterialIndex);
            const FString ExpectedMaterialPath = MakeMaterialObjectPath(Spec);
            if (!Material || Material->GetPathName() != ExpectedMaterialPath)
            {
                OutError = FString::Printf(
                    TEXT("Slot '%s' is not assigned to exact project-owned material '%s'."),
                    *Spec.SlotName.ToString(),
                    *ExpectedMaterialPath);
                return false;
            }
        }
    }

    OutError.Reset();
    return true;
}

bool IsEditorOperationSafe(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("Unreal Editor is unavailable.");
        return false;
    }
    if (GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
    {
        OutError = TEXT("Stop or cancel Play-In-Editor before running Istana editor operations.");
        return false;
    }

    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (DirtyMaps.Num() > 0 || DirtyContent.Num() > 0)
    {
        TArray<FString> DirtyNames;
        for (UPackage* Package : DirtyMaps)
        {
            DirtyNames.Add(Package ? Package->GetName() : TEXT("<unknown map>"));
        }
        for (UPackage* Package : DirtyContent)
        {
            DirtyNames.Add(Package ? Package->GetName() : TEXT("<unknown content>"));
        }
        DirtyNames.Sort();
        OutError = FString::Printf(
            TEXT("Refusing to change maps while packages are dirty: %s"),
            *FString::Join(DirtyNames, TEXT(", ")));
        return false;
    }
    return true;
}

template <typename TActor>
TActor* SpawnNamedActor(UWorld* World, const FName Name, const TCHAR* Label)
{
    if (!World)
    {
        return nullptr;
    }
    FActorSpawnParameters Parameters;
    Parameters.Name = Name;
    Parameters.OverrideLevel = World->PersistentLevel;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TActor* Actor = World->SpawnActor<TActor>(
        TActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        Parameters);
#if WITH_EDITOR
    if (Actor)
    {
        Actor->SetActorLabel(Label, false);
        Actor->SetFolderPath(FName(TEXT("TRIAD/Istana 1km Study Area")));
    }
#endif
    return Actor;
}

ACesiumGeoreference* RecenterGeoreferences(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    ACesiumGeoreference* Default = nullptr;
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        ACesiumGeoreference* Georeference = *It;
        if (!Georeference || Georeference->GetLevel() != World->PersistentLevel)
        {
            continue;
        }
        Georeference->Modify();
        Georeference->SetOriginPlacement(EOriginPlacement::CartographicOrigin);
        Georeference->SetOriginLongitudeLatitudeHeight(FVector(
            CenterLongitudeDegrees,
            CenterLatitudeDegrees,
            ApproximateCenterHeightMeters));
        Default = Default ? Default : Georeference;
        ++OutCount;
    }
    if (!Default)
    {
        Default = ACesiumGeoreference::GetDefaultGeoreference(World);
        if (Default)
        {
            Default->Modify();
            Default->SetOriginPlacement(EOriginPlacement::CartographicOrigin);
            Default->SetOriginLongitudeLatitudeHeight(FVector(
                CenterLongitudeDegrees,
                CenterLatitudeDegrees,
                ApproximateCenterHeightMeters));
#if WITH_EDITOR
            Default->SetActorLabel(TEXT("CesiumGeoreference_Istana"), false);
#endif
            OutCount = 1;
        }
    }
    return Default;
}

ACesiumGeoreference* FindTemplateGeoreferenceWithoutModification(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    ACesiumGeoreference* Selected = nullptr;
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        ACesiumGeoreference* Georeference = *It;
        if (!Georeference || Georeference->GetLevel() != World->PersistentLevel)
        {
            continue;
        }
        Selected = Selected ? Selected : Georeference;
        ++OutCount;
    }
    return Selected;
}

FVector MakeSouthOffsetLongitudeLatitudeHeight(double SouthOffsetMeters, double HeightMeters)
{
    const FVector2D LongitudeLatitude = TRIAD::Geodesy::Wgs84DestinationDegrees(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        180.0,
        SouthOffsetMeters);
    return FVector(LongitudeLatitude.X, LongitudeLatitude.Y, HeightMeters);
}

bool IsSameLongitudeLatitudeHeight(
    const FVector& A,
    const FVector& B,
    double HorizontalToleranceMeters = 0.05,
    double HeightToleranceMeters = 0.05)
{
    return TRIAD::Geodesy::Wgs84DistanceMeters(A.X, A.Y, B.X, B.Y) <= HorizontalToleranceMeters &&
        FMath::Abs(A.Z - B.Z) <= HeightToleranceMeters;
}

bool IsImportedExteriorCeremonialFrontFacingSouth(
    const ATRIADIstanaExteriorMeshActor* Exterior,
    const ACesiumGeoreference* Georeference,
    double GroundHeightMeters,
    double& OutFrontAlignmentDot)
{
    OutFrontAlignmentDot = -1.0;
    if (!Exterior || !Georeference || !FMath::IsFinite(GroundHeightMeters))
    {
        return false;
    }

    // OBJ handedness maps the source model's ceremonial +Y facade to Unreal
    // local -Y. Prove that local -Y points toward geodetic south/front after
    // the actor's ESU rotation, rather than trusting a screenshot label.
    const FVector SouthGroundLongitudeLatitudeHeight =
        MakeSouthOffsetLongitudeLatitudeHeight(50.0, GroundHeightMeters);
    const FVector SouthGroundWorld =
        Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
            SouthGroundLongitudeLatitudeHeight);
    const FVector ExpectedFrontWorld =
        (SouthGroundWorld - Exterior->GetActorLocation()).GetSafeNormal();
    const FVector ExpectedFrontInActorSpace =
        Exterior->GetActorTransform()
            .InverseTransformVectorNoScale(ExpectedFrontWorld)
            .GetSafeNormal();
    const FVector ImportedCeremonialFrontLocal(0.0, -1.0, 0.0);
    OutFrontAlignmentDot = FVector::DotProduct(
        ExpectedFrontInActorSpace,
        ImportedCeremonialFrontLocal);
    return !ExpectedFrontWorld.ContainsNaN() &&
        !ExpectedFrontWorld.IsNearlyZero() &&
        !ExpectedFrontInActorSpace.ContainsNaN() &&
        !ExpectedFrontInActorSpace.IsNearlyZero() &&
        FMath::IsFinite(OutFrontAlignmentDot) &&
        OutFrontAlignmentDot >= 0.999;
}

bool SetCameraAutoActivateForPlayer(
    ACameraActor* Camera,
    EAutoReceiveInput::Type Player,
    FString& OutError)
{
    if (!Camera)
    {
        OutError = TEXT("Camera is unavailable.");
        return false;
    }
    FByteProperty* AutoActivateProperty = FindFProperty<FByteProperty>(
        ACameraActor::StaticClass(),
        TEXT("AutoActivateForPlayer"));
    if (!AutoActivateProperty)
    {
        OutError = TEXT("UE 5.5 ACameraActor AutoActivateForPlayer property is unavailable.");
        return false;
    }
    AutoActivateProperty->SetPropertyValue_InContainer(
        Camera,
        static_cast<uint8>(Player));
    return true;
}

UCesiumGlobeAnchorComponent* AddGlobeAnchor(
    AActor* Actor,
    ACesiumGeoreference* Georeference,
    const FVector& LongitudeLatitudeHeight)
{
    if (!Actor || !Georeference)
    {
        return nullptr;
    }

    const FName ComponentName = MakeUniqueObjectName(
        Actor,
        UCesiumGlobeAnchorComponent::StaticClass(),
        TEXT("TRIAD_GlobeAnchor"));
    UCesiumGlobeAnchorComponent* Anchor = NewObject<UCesiumGlobeAnchorComponent>(
        Actor,
        ComponentName,
        RF_Transactional);
    if (!Anchor)
    {
        return nullptr;
    }

    Anchor->CreationMethod = EComponentCreationMethod::Instance;
    Actor->AddInstanceComponent(Anchor);
    Anchor->RegisterComponent();
    Anchor->SetGeoreference(TSoftObjectPtr<ACesiumGeoreference>(Georeference));
    Anchor->SetAdjustOrientationForGlobeWhenMoving(true);
    Anchor->MoveToLongitudeLatitudeHeight(LongitudeLatitudeHeight);
    return Anchor;
}

bool SetPersistentAirSimGameModeOverride(UWorld* World, FString& OutError)
{
    if (!World || !World->PersistentLevel)
    {
        OutError = TEXT("World or persistent level is unavailable.");
        return false;
    }

    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* IstanaAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    FClassProperty* DefaultGameModeProperty = FindFProperty<FClassProperty>(
        AWorldSettings::StaticClass(),
        GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultGameMode));
    if (!ExternalAirSimGameModeClass ||
        !IstanaAirSimGameModeClass ||
        IstanaAirSimGameModeClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !IstanaAirSimGameModeClass->IsChildOf(ExternalAirSimGameModeClass) ||
        !WorldSettings ||
        !DefaultGameModeProperty)
    {
        OutError = FString::Printf(
            TEXT("TRIAD AirSim wrapper '%s', its external base '%s', or the UE 5.5 World Settings GameMode Override property is unavailable."),
            *IstanaAirSimGameModeClassPath,
            *ExternalAirSimGameModeClassPath);
        return false;
    }

    // Two independent disk round trips in this host stripped a direct native
    // AirSimTriadRuntime class reference to None. Persist the behavior-identical
    // TRIAD-owned subclass instead and prove that exact class after reload.
    WorldSettings->Modify();
    WorldSettings->PreEditChange(DefaultGameModeProperty);
    DefaultGameModeProperty->SetObjectPropertyValue_InContainer(
        WorldSettings,
        IstanaAirSimGameModeClass);
    FPropertyChangedEvent ChangedEvent(
        DefaultGameModeProperty,
        EPropertyChangeType::ValueSet);
    WorldSettings->PostEditChangeProperty(ChangedEvent);
    const bool bWorldSettingsPackageMarkedDirty = WorldSettings->MarkPackageDirty();
    World->PersistentLevel->MarkPackageDirty();
    World->MarkPackageDirty();

    if (!bWorldSettingsPackageMarkedDirty || !World->GetOutermost()->IsDirty())
    {
        OutError = TEXT("UE suppressed map-package dirtiness for the GameMode Override edit; refusing to save.");
        return false;
    }

    const UClass* PersistedClass = WorldSettings->DefaultGameMode.Get();
    if (!PersistedClass || PersistedClass->GetPathName() != IstanaAirSimGameModeClassPath)
    {
        OutError = TEXT("World Settings rejected the TRIAD-owned AirSim GameMode Override edit.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ConfigureAirSimRuntimeStart(
    UWorld* World,
    ACesiumGeoreference* Georeference,
    const FVector& PreservedGeoreferenceOriginLongitudeLatitudeHeight,
    FString& OutError)
{
    if (!World || !World->PersistentLevel || !Georeference)
    {
        OutError = TEXT("World or preserved Cesium georeference is unavailable.");
        return false;
    }

    if (!SetPersistentAirSimGameModeOverride(World, OutError))
    {
        return false;
    }

    TArray<APlayerStart*> PlayerStarts;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        if (It->GetLevel() == World->PersistentLevel)
        {
            PlayerStarts.Add(*It);
        }
    }
    APlayerStart* PlayerStart = PlayerStarts.Num() > 0
        ? PlayerStarts[0]
        : SpawnNamedActor<APlayerStart>(
            World,
            TEXT("TRIAD_Istana_AirSim_PlayerStart"),
            TEXT("TRIAD Istana AirSim Player Start"));
    if (!PlayerStart)
    {
        OutError = TEXT("Could not create the AirSim PlayerStart.");
        return false;
    }
    PlayerStart->Modify();
    PlayerStart->Tags.AddUnique(RuntimePlayerStartTag);
#if WITH_EDITOR
    PlayerStart->SetActorLabel(TEXT("TRIAD Istana AirSim Player Start"), false);
    PlayerStart->SetFolderPath(FName(TEXT("TRIAD/Istana 1km Runtime")));
#endif
    for (int32 Index = 1; Index < PlayerStarts.Num(); ++Index)
    {
        World->DestroyActor(PlayerStarts[Index], false, true);
    }

    const FVector PlayerStartLongitudeLatitudeHeight = MakeSouthOffsetLongitudeLatitudeHeight(
        PlayerStartSouthOffsetMeters,
        ApproximateCenterHeightMeters + PlayerStartHeightAboveGroundMeters);
    UCesiumGlobeAnchorComponent* PlayerStartAnchor = AddGlobeAnchor(
        PlayerStart,
        Georeference,
        PlayerStartLongitudeLatitudeHeight);
    if (!PlayerStartAnchor)
    {
        OutError = TEXT("Could not globe-anchor the AirSim PlayerStart.");
        return false;
    }
    const FVector PlayerStartWorld = PlayerStart->GetActorLocation();
    const FVector PlayerLookAtWorld = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters + PlayerStartHeightAboveGroundMeters));
    PlayerStart->SetActorRotation((PlayerLookAtWorld - PlayerStartWorld).Rotation());

    // Disable inherited auto-activation in the destination copy only. A single
    // deterministic camera becomes Player 0's view target, which is also the
    // fallback global-NED origin used by AirSim when no vehicle pawn is placed.
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->GetLevel() == World->PersistentLevel)
        {
            It->Modify();
            if (!SetCameraAutoActivateForPlayer(
                    *It,
                    EAutoReceiveInput::Disabled,
                    OutError))
            {
                return false;
            }
        }
    }
    ACameraActor* RuntimeCamera = SpawnNamedActor<ACameraActor>(
        World,
        TEXT("TRIAD_Istana_AirSim_RuntimeCamera"),
        TEXT("TRIAD Istana AirSim Runtime Camera"));
    if (!RuntimeCamera)
    {
        OutError = TEXT("Could not create the AirSim runtime camera.");
        return false;
    }
    RuntimeCamera->Modify();
    RuntimeCamera->Tags.AddUnique(RuntimeCameraActorTag);
    if (!SetCameraAutoActivateForPlayer(
            RuntimeCamera,
            EAutoReceiveInput::Player0,
            OutError))
    {
        return false;
    }
    if (RuntimeCamera->GetCameraComponent())
    {
        RuntimeCamera->GetCameraComponent()->SetFieldOfView(
            RuntimeCameraFieldOfViewDegrees);
    }
    const FVector CameraLongitudeLatitudeHeight = MakeSouthOffsetLongitudeLatitudeHeight(
        RuntimeCameraSouthOffsetMeters,
        ApproximateCenterHeightMeters + RuntimeCameraHeightAboveGroundMeters);
    UCesiumGlobeAnchorComponent* CameraAnchor = AddGlobeAnchor(
        RuntimeCamera,
        Georeference,
        CameraLongitudeLatitudeHeight);
    if (!CameraAnchor)
    {
        OutError = TEXT("Could not globe-anchor the AirSim runtime camera.");
        return false;
    }
    const FVector CenterWorld = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters + RuntimeCameraAimAboveGroundMeters));
    RuntimeCamera->SetActorRotation((CenterWorld - RuntimeCamera->GetActorLocation()).Rotation());

    ATRIADIstanaRuntimePolicyActor* RuntimePolicy =
        SpawnNamedActor<ATRIADIstanaRuntimePolicyActor>(
            World,
            TEXT("TRIAD_Istana_RuntimePolicy"),
            TEXT("TRIAD Istana Runtime Policy - Clear Weather / Cesium Recovery"));
    if (!RuntimePolicy)
    {
        OutError = TEXT("Could not create the Istana runtime policy actor.");
        return false;
    }
    RuntimePolicy->Modify();
    RuntimePolicy->Tags.AddUnique(RuntimePolicyActorTag);
    RuntimePolicy->ConfigureMapMetadata(
        PreservedGeoreferenceOriginLongitudeLatitudeHeight,
        FVector(CenterLongitudeDegrees, CenterLatitudeDegrees, ApproximateCenterHeightMeters));
    RuntimePolicy->ConfigureGroundCalibration(
        ApproximateCenterHeightMeters,
        0,
        false,
        TEXT("PUBLIC_SRTM30_APPROXIMATE_FALLBACK_NOT_SURVEY_GRADE"));
    return true;
}

ACesiumCartographicPolygon* BuildClipPolygon(
    UWorld* World,
    ACesiumGeoreference* Georeference)
{
    ACesiumCartographicPolygon* Polygon = SpawnNamedActor<ACesiumCartographicPolygon>(
        World,
        TEXT("TRIAD_Istana_1km_GeodesicClip"),
        TEXT("TRIAD Istana 1 km Geodesic Clip (64 points)"));
    if (!Polygon || !Polygon->Polygon || !Polygon->GlobeAnchor || !Georeference)
    {
        return nullptr;
    }
    Polygon->Tags.AddUnique(PolygonActorTag);
    Polygon->GlobeAnchor->SetGeoreference(TSoftObjectPtr<ACesiumGeoreference>(Georeference));
    Polygon->GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters));
    Polygon->Polygon->ClearSplinePoints(false);
    for (int32 PointIndex = 0; PointIndex < PolygonPointCount; ++PointIndex)
    {
        const double BearingDegrees = static_cast<double>(PointIndex) * 360.0 / PolygonPointCount;
        const FVector2D LongitudeLatitude = TRIAD::Geodesy::Wgs84DestinationDegrees(
            CenterLongitudeDegrees,
            CenterLatitudeDegrees,
            BearingDegrees,
            StudyRadiusMeters);
        const FVector WorldPosition = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
            LongitudeLatitude.X,
            LongitudeLatitude.Y,
            ApproximateCenterHeightMeters));
        Polygon->Polygon->AddSplinePoint(WorldPosition, ESplineCoordinateSpace::World, false);
        Polygon->Polygon->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
    }
    Polygon->Polygon->SetClosedLoop(true, false);
    Polygon->Polygon->UpdateSpline();
    Polygon->Modify();
    return Polygon;
}

int32 ConfigureTilesets(
    UWorld* World,
    ACesiumCartographicPolygon* Polygon)
{
    int32 TilesetCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        if (!Tileset || !Polygon)
        {
            continue;
        }
        Tileset->Modify();
        Tileset->SetCreatePhysicsMeshes(true);
        Tileset->EnableFrustumCulling = false;
        Tileset->EnableFogCulling = false;
        Tileset->EnforceCulledScreenSpaceError = true;
        Tileset->CulledScreenSpaceError = 32.0;

        const FName ComponentName = MakeUniqueObjectName(
            Tileset,
            UCesiumPolygonRasterOverlay::StaticClass(),
            TEXT("TRIAD_Istana_1km_OutsideClip"));
        UCesiumPolygonRasterOverlay* Overlay = NewObject<UCesiumPolygonRasterOverlay>(
            Tileset,
            ComponentName,
            RF_Transactional);
        if (!Overlay)
        {
            continue;
        }
        Overlay->CreationMethod = EComponentCreationMethod::Instance;
        Overlay->ComponentTags.AddUnique(ClipComponentTag);
        Overlay->Polygons.Add(TSoftObjectPtr<ACesiumCartographicPolygon>(Polygon));
        Overlay->InvertSelection = true;
        Overlay->ExcludeSelectedTiles = true;
        Tileset->AddInstanceComponent(Overlay);
        Overlay->RegisterComponent();
        ++TilesetCount;
    }
    return TilesetCount;
}

bool ValidateWorld(UWorld* World, bool bRequireDestinationPackage, FString& OutReport)
{
    TArray<FString> Errors;
    TArray<FString> Notes;
    if (!World)
    {
        OutReport = TEXT("No editor world is loaded.");
        return false;
    }

    const FString LoadedPackage = World->GetOutermost()->GetName();
    if (bRequireDestinationPackage && LoadedPackage != DestinationMapPackage)
    {
        Errors.Add(FString::Printf(
            TEXT("Loaded map is '%s', expected '%s'."),
            *LoadedPackage,
            *DestinationMapPackage));
    }

    TArray<ACesiumGeoreference*> Georeferences;
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        if (It->GetLevel() == World->PersistentLevel)
        {
            Georeferences.Add(*It);
            const FVector Origin = It->GetOriginLongitudeLatitudeHeight();
            if (TRIAD::Geodesy::Wgs84DistanceMeters(
                    CenterLongitudeDegrees,
                    CenterLatitudeDegrees,
                    Origin.X,
                    Origin.Y) > 0.10 ||
                FMath::Abs(Origin.Z - ApproximateCenterHeightMeters) > 0.10)
            {
                Errors.Add(FString::Printf(
                    TEXT("Georeference '%s' is not centered on the Istana fallback LLH."),
                    *It->GetName()));
            }
        }
    }
    if (Georeferences.Num() == 0)
    {
        Errors.Add(TEXT("No persistent CesiumGeoreference found."));
    }
    ACesiumGeoreference* Georeference = Georeferences.Num() > 0 ? Georeferences[0] : nullptr;

    TArray<ATRIADIstanaBuildingActor*> Buildings;
    for (TActorIterator<ATRIADIstanaBuildingActor> It(World); It; ++It)
    {
        Buildings.Add(*It);
    }
    if (Buildings.Num() != 1)
    {
        Errors.Add(FString::Printf(TEXT("Expected exactly one Istana building actor; found %d."), Buildings.Num()));
    }
    else if (!Buildings[0]->GlobeAnchor)
    {
        Errors.Add(TEXT("Istana building is missing its Cesium globe anchor."));
    }
    else
    {
        const FVector BuildingLLH = Buildings[0]->GlobeAnchor->GetLongitudeLatitudeHeight();
        const double CenterOffsetMeters = TRIAD::Geodesy::Wgs84DistanceMeters(
            CenterLongitudeDegrees,
            CenterLatitudeDegrees,
            BuildingLLH.X,
            BuildingLLH.Y);
        if (CenterOffsetMeters > 1.0)
        {
            Errors.Add(FString::Printf(TEXT("Istana building anchor is %.3f m from AOI center."), CenterOffsetMeters));
        }
        if (!FMath::IsFinite(BuildingLLH.Z) ||
            FMath::Abs(BuildingLLH.Z - Buildings[0]->HeightMeters) > 0.25)
        {
            Errors.Add(TEXT("Istana building height metadata does not match its globe anchor."));
        }
    }

    TArray<ATRIADIstanaStudyAreaActor*> StudyAreas;
    for (TActorIterator<ATRIADIstanaStudyAreaActor> It(World); It; ++It)
    {
        StudyAreas.Add(*It);
    }
    if (StudyAreas.Num() != 1)
    {
        Errors.Add(FString::Printf(TEXT("Expected exactly one Istana study-area actor; found %d."), StudyAreas.Num()));
    }
    else if (FMath::Abs(StudyAreas[0]->RadiusMeters - StudyRadiusMeters) > 0.001 ||
             StudyAreas[0]->BoundarySegments != PolygonPointCount)
    {
        Errors.Add(TEXT("Study-area actor is not the exact configured 1000 m / 64-segment boundary."));
    }
    else if (!StudyAreas[0]->ApproximateTerrainFallback ||
             !StudyAreas[0]->ApproximateTerrainFallback->GetStaticMesh() ||
             StudyAreas[0]->ApproximateTerrainFallback->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        Errors.Add(TEXT("Study-area actor is missing its explicit non-survey terrain collision fallback."));
    }
    else
    {
        Notes.Add(TEXT("Validated the invisible 47 m simulation collision fallback; this is not survey terrain."));
    }

    TArray<ACesiumCartographicPolygon*> Polygons;
    for (TActorIterator<ACesiumCartographicPolygon> It(World); It; ++It)
    {
        if (It->ActorHasTag(PolygonActorTag))
        {
            Polygons.Add(*It);
        }
    }
    ACesiumCartographicPolygon* Polygon = Polygons.Num() == 1 ? Polygons[0] : nullptr;
    if (!Polygon)
    {
        Errors.Add(FString::Printf(TEXT("Expected one tagged geodesic clip polygon; found %d."), Polygons.Num()));
    }
    else if (!Polygon->Polygon || Polygon->Polygon->GetNumberOfSplinePoints() != PolygonPointCount ||
             !Polygon->Polygon->IsClosedLoop())
    {
        Errors.Add(TEXT("Clip polygon must be a closed 64-point spline."));
    }
    else if (Georeference)
    {
        double MaximumRadiusErrorMeters = 0.0;
        for (int32 Index = 0; Index < PolygonPointCount; ++Index)
        {
            const FVector PointLLH = Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(
                Polygon->Polygon->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
            MaximumRadiusErrorMeters = FMath::Max(
                MaximumRadiusErrorMeters,
                FMath::Abs(TRIAD::Geodesy::Wgs84DistanceMeters(
                    CenterLongitudeDegrees,
                    CenterLatitudeDegrees,
                    PointLLH.X,
                    PointLLH.Y) - StudyRadiusMeters));
        }
        if (MaximumRadiusErrorMeters > 0.05)
        {
            Errors.Add(FString::Printf(
                TEXT("Clip polygon maximum geodesic radius error is %.4f m."),
                MaximumRadiusErrorMeters));
        }
        else
        {
            Notes.Add(FString::Printf(
                TEXT("64-point clip maximum geodesic radius error %.4f m."),
                MaximumRadiusErrorMeters));
        }
    }

    int32 TilesetCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        ++TilesetCount;
        if (!Tileset->GetCreatePhysicsMeshes() || Tileset->EnableFrustumCulling ||
            Tileset->EnableFogCulling || !Tileset->EnforceCulledScreenSpaceError ||
            Tileset->CulledScreenSpaceError > 32.0)
        {
            Errors.Add(FString::Printf(
                TEXT("Tileset '%s' does not have placement-survey collision/culling settings."),
                *Tileset->GetName()));
        }
        TArray<UCesiumPolygonRasterOverlay*> Overlays;
        Tileset->GetComponents<UCesiumPolygonRasterOverlay>(Overlays);
        const int32 MatchingOverlays = Overlays.FilterByPredicate(
            [Polygon](const UCesiumPolygonRasterOverlay* Overlay)
            {
                return Overlay &&
                    Overlay->ComponentHasTag(ClipComponentTag) &&
                    Overlay->InvertSelection &&
                    Overlay->ExcludeSelectedTiles &&
                    Polygon &&
                    Overlay->Polygons.Contains(TSoftObjectPtr<ACesiumCartographicPolygon>(Polygon));
            }).Num();
        if (MatchingOverlays != 1)
        {
            Errors.Add(FString::Printf(
                TEXT("Tileset '%s' has %d valid tagged outside-clip overlays; expected 1."),
                *Tileset->GetName(),
                MatchingOverlays));
        }
    }
    if (TilesetCount == 0)
    {
        Errors.Add(TEXT("No Cesium 3D Tileset found to provide terrain/building context."));
    }

    TArray<UPackage*> DirtyMaps;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    for (UPackage* Package : DirtyMaps)
    {
        if (Package && Package->GetName() == SourceMapPackage)
        {
            Errors.Add(TEXT("Source map /Game/SDTH is dirty; builder safety invariant failed."));
        }
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Istana map validation FAILED:\n - ") + FString::Join(Errors, TEXT("\n - "));
        if (Notes.Num() > 0)
        {
            OutReport += TEXT("\nNotes:\n - ") + FString::Join(Notes, TEXT("\n - "));
        }
        return false;
    }

    Notes.Add(FString::Printf(TEXT("Validated %d Cesium tileset(s)."), TilesetCount));
    Notes.Add(TEXT("Public exterior and 47 m SRTM fallback remain explicit non-survey approximations."));
    OutReport = TEXT("Istana map validation PASSED:\n - ") + FString::Join(Notes, TEXT("\n - "));
    return true;
}

bool ValidateRuntimeWorldV2(
    UWorld* World,
    bool bRequireDestinationPackage,
    FString& OutReport,
    bool bRequireAirSimGameMode = true,
    bool bRequireRuntimeCamera = true,
    bool bRequireImportedExteriorOrientation = true,
    bool bRequireRefinedExteriorAsset = true)
{
    TArray<FString> Errors;
    TArray<FString> Notes;
    if (!World)
    {
        OutReport = TEXT("No editor world is loaded.");
        return false;
    }

    const FString LoadedPackage = World->GetOutermost()->GetName();
    if (bRequireDestinationPackage && LoadedPackage != DestinationMapV2Package)
    {
        Errors.Add(FString::Printf(
            TEXT("Loaded map is '%s', expected '%s'."),
            *LoadedPackage,
            *DestinationMapV2Package));
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    const UClass* ConfiguredGameMode = WorldSettings ? WorldSettings->DefaultGameMode.Get() : nullptr;
    if (bRequireAirSimGameMode &&
        (!ConfiguredGameMode || ConfiguredGameMode->GetPathName() != IstanaAirSimGameModeClassPath))
    {
        Errors.Add(FString::Printf(
            TEXT("World Settings does not persist the required TRIAD-owned AirSim GameMode wrapper '%s'."),
            *IstanaAirSimGameModeClassPath));
    }
    else if (!bRequireAirSimGameMode)
    {
        Notes.Add(TEXT("GameMode Override validation was deferred only for the narrow persistence-repair preflight."));
    }

    int32 GeoreferenceCount = 0;
    ACesiumGeoreference* Georeference =
        FindTemplateGeoreferenceWithoutModification(World, GeoreferenceCount);
    if (!Georeference)
    {
        Errors.Add(TEXT("No persistent CesiumGeoreference found."));
    }

    TArray<ATRIADIstanaRuntimePolicyActor*> RuntimePolicies;
    for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimePolicyActorTag))
        {
            RuntimePolicies.Add(*It);
        }
    }
    ATRIADIstanaRuntimePolicyActor* RuntimePolicy =
        RuntimePolicies.Num() == 1 ? RuntimePolicies[0] : nullptr;
    double ExpectedGroundHeightMeters = ApproximateCenterHeightMeters;
    if (!RuntimePolicy)
    {
        Errors.Add(FString::Printf(
            TEXT("Expected one tagged Istana runtime policy; found %d."),
            RuntimePolicies.Num()));
    }
    else
    {
        if (!FMath::IsFinite(RuntimePolicy->ExteriorGroundHeightMeters) ||
            RuntimePolicy->ExteriorGroundHeightMeters < -50.0 ||
            RuntimePolicy->ExteriorGroundHeightMeters > 150.0)
        {
            Errors.Add(TEXT("Runtime policy exterior ground-height metadata is invalid."));
        }
        else
        {
            ExpectedGroundHeightMeters = RuntimePolicy->ExteriorGroundHeightMeters;
        }
        if (!RuntimePolicy->bSuppressAirSimVisualWeather)
        {
            Errors.Add(TEXT("Runtime policy does not suppress AirSim visual weather."));
        }
        if (!RuntimePolicy->bSuppressInheritedExponentialHeightFog ||
            RuntimePolicy->MaximumExponentialFogSuppressionAttempts < 1 ||
            RuntimePolicy->MaximumExponentialFogSuppressionAttempts > 12)
        {
            Errors.Add(TEXT("Runtime policy does not provide bounded inherited exponential-height-fog suppression."));
        }
        if (!RuntimePolicy->bRequireIstanaAirSimGameMode ||
            RuntimePolicy->RequiredGameModeClassPath != IstanaAirSimGameModeClassPath)
        {
            Errors.Add(TEXT("Runtime policy does not require the exact persisted TRIAD-owned AirSim GameMode wrapper."));
        }
        if (!RuntimePolicy->bEnforceTaggedRuntimeCameraForPlayer0 ||
            RuntimePolicy->RequiredRuntimeCameraTag != RuntimeCameraActorTag ||
            RuntimePolicy->MaximumRuntimeCameraEnforcementAttempts < 1 ||
            RuntimePolicy->MaximumRuntimeCameraEnforcementAttempts > 12)
        {
            Errors.Add(TEXT("Runtime policy does not provide bounded Player 0 enforcement for the tagged Istana camera."));
        }
        if (!RuntimePolicy->bApplyRuntimeCameraQualityProfile)
        {
            Errors.Add(TEXT("Runtime policy does not require the transient Istana camera-quality profile."));
        }
        if (!FMath::IsFinite(RuntimePolicy->StreamedPrimaryEvaluationIntervalSeconds) ||
            RuntimePolicy->StreamedPrimaryEvaluationIntervalSeconds < 0.25f ||
            RuntimePolicy->StreamedPrimaryEvaluationIntervalSeconds > 5.0f ||
            !FMath::IsFinite(RuntimePolicy->StreamedPrimaryRestoreBelowLoadProgress) ||
            RuntimePolicy->StreamedPrimaryRestoreBelowLoadProgress < 95.0f ||
            RuntimePolicy->StreamedPrimaryRestoreBelowLoadProgress > 99.9f)
        {
            Errors.Add(TEXT("Runtime policy streamed-primary evaluation interval or restoration hysteresis is invalid."));
        }
        if (!RuntimePolicy->bPreferStreamedIstanaVisualWhenReady)
        {
            Notes.Add(TEXT("Refined authored exterior is the persisted visual primary; streamed-building substitution remains explicitly opted out."));
        }
        if (!RuntimePolicy->bRefreshUnreadyCesiumTilesets ||
            RuntimePolicy->MaximumTilesetRefreshAttempts < 1 ||
            RuntimePolicy->MaximumTilesetRefreshAttempts > 3)
        {
            Errors.Add(TEXT("Runtime policy does not provide a bounded Cesium zero-progress refresh."));
        }
        if (Georeference && !IsSameLongitudeLatitudeHeight(
                Georeference->GetOriginLongitudeLatitudeHeight(),
                RuntimePolicy->PreservedGeoreferenceOriginLongitudeLatitudeHeight,
                0.001,
                0.001))
        {
            Errors.Add(TEXT("Template georeference origin changed after v2 metadata was captured."));
        }
        if (!IsSameLongitudeLatitudeHeight(
                RuntimePolicy->StudyCenterLongitudeLatitudeHeight,
                FVector(CenterLongitudeDegrees, CenterLatitudeDegrees, ExpectedGroundHeightMeters)))
        {
            Errors.Add(TEXT("Runtime policy study-center metadata does not match the Istana LLH."));
        }
        if (RuntimePolicy->bExteriorGroundHeightCalibrated)
        {
            if (RuntimePolicy->ExteriorGroundAcceptedSampleCount < MinimumCalibrationAcceptedSamples ||
                RuntimePolicy->ExteriorGroundHeightSource !=
                    TEXT("CESIUM_COLLISION_EXTERIOR_RING_MEDIAN_WGS84_ELLIPSOID"))
            {
                Errors.Add(TEXT("Calibrated ground-height metadata lacks sufficient accepted Cesium ring samples/provenance."));
            }
            else
            {
                Notes.Add(FString::Printf(
                    TEXT("Ground-calibrated to %.3f m WGS84 ellipsoid height from %d accepted exterior-ring Cesium collision samples."),
                    ExpectedGroundHeightMeters,
                    RuntimePolicy->ExteriorGroundAcceptedSampleCount));
            }
        }
        else if (FMath::Abs(ExpectedGroundHeightMeters - ApproximateCenterHeightMeters) > 0.001 ||
                 RuntimePolicy->ExteriorGroundAcceptedSampleCount != 0)
        {
            Errors.Add(TEXT("Uncalibrated runtime policy must retain the explicit 47 m / zero-sample fallback."));
        }
    }

    TArray<APlayerStart*> PlayerStarts;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        if (It->GetLevel() == World->PersistentLevel)
        {
            PlayerStarts.Add(*It);
        }
    }
    if (PlayerStarts.Num() != 1 || !PlayerStarts[0]->ActorHasTag(RuntimePlayerStartTag))
    {
        Errors.Add(FString::Printf(
            TEXT("Expected exactly one tagged AirSim PlayerStart; found %d persistent PlayerStart actor(s)."),
            PlayerStarts.Num()));
    }
    else
    {
        const UCesiumGlobeAnchorComponent* Anchor =
            PlayerStarts[0]->FindComponentByClass<UCesiumGlobeAnchorComponent>();
        const FVector Expected = MakeSouthOffsetLongitudeLatitudeHeight(
            PlayerStartSouthOffsetMeters,
            ExpectedGroundHeightMeters + PlayerStartHeightAboveGroundMeters);
        if (!Anchor || !IsSameLongitudeLatitudeHeight(Anchor->GetLongitudeLatitudeHeight(), Expected))
        {
            Errors.Add(TEXT("AirSim PlayerStart is not globe-anchored at its deterministic Istana approach LLH."));
        }
    }

    if (!bRequireRuntimeCamera)
    {
        Notes.Add(TEXT("Runtime-camera pose/FOV validation was deferred only for the narrow camera-migration preflight."));
    }
    else
    {
        TArray<ACameraActor*> RuntimeCameras;
        for (TActorIterator<ACameraActor> It(World); It; ++It)
        {
            if (It->ActorHasTag(RuntimeCameraActorTag))
            {
                RuntimeCameras.Add(*It);
            }
        }
        if (RuntimeCameras.Num() != 1)
        {
            Errors.Add(FString::Printf(
                TEXT("Expected one tagged AirSim runtime camera; found %d."),
                RuntimeCameras.Num()));
        }
        else
        {
            ACameraActor* Camera = RuntimeCameras[0];
            const UCesiumGlobeAnchorComponent* Anchor =
                Camera->FindComponentByClass<UCesiumGlobeAnchorComponent>();
            const FVector Expected = MakeSouthOffsetLongitudeLatitudeHeight(
                RuntimeCameraSouthOffsetMeters,
                ExpectedGroundHeightMeters + RuntimeCameraHeightAboveGroundMeters);
            if (Camera->GetAutoActivatePlayerIndex() != 0)
            {
                Errors.Add(TEXT("Istana runtime camera is not the deterministic Player 0 view target."));
            }
            const UCameraComponent* CameraComponent = Camera->GetCameraComponent();
            if (!CameraComponent ||
                !FMath::IsFinite(CameraComponent->FieldOfView) ||
                !FMath::IsNearlyEqual(
                    CameraComponent->FieldOfView,
                    RuntimeCameraFieldOfViewDegrees,
                    0.01f))
            {
                Errors.Add(FString::Printf(
                    TEXT("Istana runtime camera must use the deterministic %.1f degree facade-view FOV."),
                    RuntimeCameraFieldOfViewDegrees));
            }
            if (!Anchor || !IsSameLongitudeLatitudeHeight(Anchor->GetLongitudeLatitudeHeight(), Expected))
            {
                Errors.Add(TEXT("Istana runtime camera is not globe-anchored at its configured LLH."));
            }
            if (Georeference)
            {
                const FVector TargetWorld = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
                    CenterLongitudeDegrees,
                    CenterLatitudeDegrees,
                    ExpectedGroundHeightMeters + RuntimeCameraAimAboveGroundMeters));
                const FVector ToTarget = (TargetWorld - Camera->GetActorLocation()).GetSafeNormal();
                if (FVector::DotProduct(Camera->GetActorForwardVector(), ToTarget) < 0.999)
                {
                    Errors.Add(TEXT("Istana runtime camera is not aimed at the main-building elevation."));
                }
            }
        }
    }

    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorMeshActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
    {
        ExteriorMeshActors.Add(*It);
    }
    if (ExteriorMeshActors.Num() != 1 || !ExteriorMeshActors[0]->GlobeAnchor ||
        !ExteriorMeshActors[0]->ExteriorMeshComponent ||
        !ExteriorMeshActors[0]->bRequiredExteriorMeshLoaded ||
        ExteriorMeshActors[0]->ExteriorMeshComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        !IsSameLongitudeLatitudeHeight(
            ExteriorMeshActors[0]->GlobeAnchor->GetLongitudeLatitudeHeight(),
            FVector(CenterLongitudeDegrees, CenterLatitudeDegrees, ExpectedGroundHeightMeters),
            1.0,
            0.25) ||
        !FMath::IsNearlyEqual(
            ExteriorMeshActors[0]->StreamedReplacementHorizontalScale,
            1.004,
            0.0001) ||
        !ExteriorMeshActors[0]->ExteriorMeshComponent->GetRelativeTransform().Equals(
            FTransform(
                FQuat::Identity,
                FVector::ZeroVector,
                FVector(1.004, 1.004, 1.0)),
            0.0001) ||
        !ExteriorMeshActors[0]->ExteriorMeshComponent->IsVisible() ||
        ExteriorMeshActors[0]->ExteriorMeshComponent->bHiddenInGame)
    {
        Errors.Add(TEXT("Expected one loaded and correctly globe-anchored project Istana exterior mesh actor."));
    }
    else
    {
        UStaticMesh* ExteriorStaticMesh =
            ExteriorMeshActors[0]->ExteriorMeshComponent->GetStaticMesh();
        const FString ExistingObjectPath = ExteriorStaticMesh
            ? ExteriorStaticMesh->GetPathName()
            : FString();
        const FString ExistingSoftObjectPath =
            ExteriorMeshActors[0]->RequiredExteriorMeshAsset
                .ToSoftObjectPath()
                .ToString();
        FString MeshValidationError;
        if (!bRequireRefinedExteriorAsset)
        {
            if (ExistingObjectPath != LegacyIstanaExteriorMeshObjectPath &&
                ExistingObjectPath != IstanaExteriorMeshObjectPath)
            {
                Errors.Add(FString::Printf(
                    TEXT("Istana exterior-asset migration preflight found unsupported mesh '%s'."),
                    *ExistingObjectPath));
            }
            else if (!ValidateIstanaExteriorMeshAsset(
                    ExteriorStaticMesh,
                    true,
                    MeshValidationError,
                    ExistingObjectPath))
            {
                Errors.Add(TEXT("Istana exterior-asset migration preflight rejected the current legacy/refined mesh: ") +
                    MeshValidationError);
            }
            else
            {
                Notes.Add(TEXT("Exact refined-exterior asset-path validation was deferred only for the narrow exterior-asset migration preflight."));
            }

            const bool bExactSupportedPair =
                ExistingSoftObjectPath == ExistingObjectPath;
            const bool bLegacyHardWithRefinedSoftDefault =
                ExistingObjectPath == LegacyIstanaExteriorMeshObjectPath &&
                ExistingSoftObjectPath == IstanaExteriorMeshObjectPath;
            // A refined hard mesh paired with the legacy soft path is not a
            // class-default transition and remains fail-closed as a partial or
            // unsupported edit. No arbitrary hard/soft mismatch is accepted.
            if (!bExactSupportedPair &&
                !bLegacyHardWithRefinedSoftDefault)
            {
                Errors.Add(FString::Printf(
                    TEXT("Istana exterior-asset migration preflight rejected hard/soft mismatch '%s' / '%s'; only an exact supported pair or the legacy-hard/refined-soft class-default transition is allowed."),
                    *ExistingObjectPath,
                    *ExistingSoftObjectPath));
            }
            else if (bLegacyHardWithRefinedSoftDefault)
            {
                Notes.Add(TEXT("Accepted only the legacy-hard/refined-soft class-default transition for the narrow exterior-asset migration; normal validation still requires refined/refined."));
            }
        }
        else if (!ValidateIstanaExteriorMeshAsset(
                ExteriorStaticMesh,
                true,
                MeshValidationError))
        {
            Errors.Add(TEXT("Istana fidelity asset validation failed: ") + MeshValidationError);
        }
        if (bRequireRefinedExteriorAsset &&
            ExistingSoftObjectPath != IstanaExteriorMeshObjectPath)
        {
            Errors.Add(TEXT("Istana exterior actor hard and soft asset references must both match the exact refined mesh."));
        }
        if (!bRequireImportedExteriorOrientation)
        {
            Notes.Add(TEXT("Imported-exterior ceremonial-front orientation validation was deferred only for the narrow camera/orientation migration preflight."));
        }
        else
        {
            double FrontAlignmentDot = -1.0;
            if (!FMath::IsNearlyEqual(
                    ExteriorMeshActors[0]->FootprintYawDegrees,
                    ImportedExteriorYawDegrees,
                    0.001) ||
                !IsImportedExteriorCeremonialFrontFacingSouth(
                    ExteriorMeshActors[0],
                    Georeference,
                    ExpectedGroundHeightMeters,
                    FrontAlignmentDot))
            {
                Errors.Add(FString::Printf(
                    TEXT("Imported Istana ceremonial front is reversed or unproven (ESU yaw %.3f, expected %.3f; inverse-transform local-front alignment %.4f)."),
                    ExteriorMeshActors[0]->FootprintYawDegrees,
                    ImportedExteriorYawDegrees,
                    FrontAlignmentDot));
            }
            else
            {
                Notes.Add(FString::Printf(
                    TEXT("Imported OBJ local -Y ceremonial front is proven broadly geodetic south (inverse-transform alignment %.4f)."),
                    FrontAlignmentDot));
            }
        }
    }

    int32 ProceduralBuildingCount = 0;
    for (TActorIterator<ATRIADIstanaBuildingActor> It(World); It; ++It)
    {
        ++ProceduralBuildingCount;
    }
    if (ProceduralBuildingCount > 0)
    {
        Errors.Add(TEXT("V2 contains a procedural/HISM Istana fallback; fidelity validation refuses mixed or silently substituted geometry."));
    }

    TArray<ATRIADIstanaStudyAreaActor*> StudyAreas;
    for (TActorIterator<ATRIADIstanaStudyAreaActor> It(World); It; ++It)
    {
        StudyAreas.Add(*It);
    }
    if (StudyAreas.Num() != 1 ||
        FMath::Abs(StudyAreas[0]->RadiusMeters - StudyRadiusMeters) > 0.001 ||
        StudyAreas[0]->BoundarySegments != PolygonPointCount ||
        FMath::Abs(StudyAreas[0]->CenterHeightMeters - ExpectedGroundHeightMeters) > 0.05)
    {
        Errors.Add(TEXT("Expected one exact 1000 m / 64-segment visible study boundary."));
    }

    TArray<ACesiumCartographicPolygon*> Polygons;
    for (TActorIterator<ACesiumCartographicPolygon> It(World); It; ++It)
    {
        if (It->ActorHasTag(PolygonActorTag))
        {
            Polygons.Add(*It);
        }
    }
    ACesiumCartographicPolygon* Polygon = Polygons.Num() == 1 ? Polygons[0] : nullptr;
    if (!Polygon || !Polygon->Polygon ||
        Polygon->Polygon->GetNumberOfSplinePoints() != PolygonPointCount ||
        !Polygon->Polygon->IsClosedLoop())
    {
        Errors.Add(TEXT("Expected one closed tagged 64-point Cesium clip polygon."));
    }
    else if (Georeference)
    {
        double MaximumRadiusErrorMeters = 0.0;
        for (int32 Index = 0; Index < PolygonPointCount; ++Index)
        {
            const FVector PointLongitudeLatitudeHeight =
                Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(
                    Polygon->Polygon->GetLocationAtSplinePoint(
                        Index,
                        ESplineCoordinateSpace::World));
            MaximumRadiusErrorMeters = FMath::Max(
                MaximumRadiusErrorMeters,
                FMath::Abs(TRIAD::Geodesy::Wgs84DistanceMeters(
                    CenterLongitudeDegrees,
                    CenterLatitudeDegrees,
                    PointLongitudeLatitudeHeight.X,
                    PointLongitudeLatitudeHeight.Y) - StudyRadiusMeters));
        }
        if (MaximumRadiusErrorMeters > 0.05)
        {
            Errors.Add(FString::Printf(
                TEXT("Clip polygon maximum geodesic radius error is %.4f m."),
                MaximumRadiusErrorMeters));
        }
        else
        {
            Notes.Add(FString::Printf(
                TEXT("64-point clip maximum geodesic radius error %.4f m."),
                MaximumRadiusErrorMeters));
        }
    }

    int32 TilesetCount = 0;
    int32 ZeroProgressTilesetCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        ++TilesetCount;
        if (!Tileset->GetCreatePhysicsMeshes() || Tileset->EnableFrustumCulling ||
            Tileset->EnableFogCulling || !Tileset->EnforceCulledScreenSpaceError ||
            Tileset->CulledScreenSpaceError > 32.0)
        {
            Errors.Add(FString::Printf(
                TEXT("Tileset '%s' is not configured to retain complete in-AOI context/collision."),
                *Tileset->GetName()));
        }
        if (Tileset->GetLoadProgress() <= KINDA_SMALL_NUMBER)
        {
            ++ZeroProgressTilesetCount;
        }

        TArray<UCesiumPolygonRasterOverlay*> Overlays;
        Tileset->GetComponents<UCesiumPolygonRasterOverlay>(Overlays);
        const int32 MatchingOverlays = Overlays.FilterByPredicate(
            [Polygon](const UCesiumPolygonRasterOverlay* Overlay)
            {
                return Overlay &&
                    Overlay->ComponentHasTag(ClipComponentTag) &&
                    Overlay->InvertSelection &&
                    Overlay->ExcludeSelectedTiles &&
                    Polygon &&
                    Overlay->Polygons.Contains(TSoftObjectPtr<ACesiumCartographicPolygon>(Polygon));
            }).Num();
        if (MatchingOverlays != 1)
        {
            Errors.Add(FString::Printf(
                TEXT("Tileset '%s' has %d valid outside-only clip overlay(s); expected 1."),
                *Tileset->GetName(),
                MatchingOverlays));
        }
    }
    if (TilesetCount == 0)
    {
        Errors.Add(TEXT("No Cesium 3D Tileset found to provide the 1 km context."));
    }
    if (ZeroProgressTilesetCount > 0)
    {
        Notes.Add(FString::Printf(
            TEXT("%d tileset(s) currently report zero progress; PIE runtime policy will perform one credential-free refresh after its grace period."),
            ZeroProgressTilesetCount));
    }

    TArray<UPackage*> DirtyMaps;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    for (UPackage* Package : DirtyMaps)
    {
        if (Package && Package->GetName() == SourceMapPackage)
        {
            Errors.Add(TEXT("Source map /Game/SDTH is dirty; builder safety invariant failed."));
        }
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Istana v2 runtime-map validation FAILED:\n - ") +
            FString::Join(Errors, TEXT("\n - "));
        if (Notes.Num() > 0)
        {
            OutReport += TEXT("\nNotes:\n - ") + FString::Join(Notes, TEXT("\n - "));
        }
        return false;
    }

    Notes.Add(FString::Printf(
        TEXT("Preserved the SDTH georeference and validated %d Cesium tileset(s)."),
        TilesetCount));
    Notes.Add(TEXT("Outside-only clipping is configured to retain the 1 km interior; use the separate readiness validator before claiming that its context has finished streaming."));
    Notes.Add(RuntimePolicy && RuntimePolicy->bExteriorGroundHeightCalibrated
        ? TEXT("Project-owned public exterior reconstruction is loaded at a collision-derived median height; neither geometry nor height is survey/BIM data.")
        : TEXT("Project-owned public exterior reconstruction is loaded at the explicit 47 m public-SRTM fallback; geometry/height are not survey/BIM data."));
    Notes.Add(TEXT("The opaque project shell uses a 0.4% XY depth-occluding expansion to cover the streamed Istana without clipping any surrounding Cesium context."));
    OutReport = TEXT("Istana v2 runtime-map validation PASSED:\n - ") +
        FString::Join(Notes, TEXT("\n - "));
    return true;
}

bool ValidateRuntimeWorldV2Readiness(
    UWorld* World,
    bool bRequireDestinationPackage,
    FString& OutReport)
{
    FString StructuralReport;
    if (!ValidateRuntimeWorldV2(World, bRequireDestinationPackage, StructuralReport))
    {
        OutReport = TEXT("Istana v2 readiness FAILED because structural validation failed:\n") +
            StructuralReport;
        return false;
    }

    TArray<FString> UnreadyTilesets;
    int32 TilesetCount = 0;
    float MinimumLoadProgress = 100.0f;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        ++TilesetCount;
        const float LoadProgress = Tileset->GetLoadProgress();
        MinimumLoadProgress = FMath::Min(MinimumLoadProgress, LoadProgress);
        if (LoadProgress < MinimumCalibrationTilesetLoadProgress ||
            !Tileset->GetCreatePhysicsMeshes())
        {
            UnreadyTilesets.Add(FString::Printf(
                TEXT("%s (%.1f%%, physics=%s)"),
                *Tileset->GetName(),
                LoadProgress,
                Tileset->GetCreatePhysicsMeshes() ? TEXT("true") : TEXT("false")));
        }
    }
    if (TilesetCount == 0 || UnreadyTilesets.Num() > 0)
    {
        OutReport = FString::Printf(
            TEXT("Istana v2 context readiness FAILED: %d/%d Cesium tileset(s) are below %.0f%% load or lack physics meshes (minimum progress %.1f%%): %s. Wait for streaming or resolve the provider session; no endpoint, token, or request details were inspected."),
            UnreadyTilesets.Num(),
            TilesetCount,
            MinimumCalibrationTilesetLoadProgress,
            MinimumLoadProgress,
            *FString::Join(UnreadyTilesets, TEXT(", ")));
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Istana v2 context readiness PASSED: all %d Cesium tileset(s) report >= %.0f%% load with physics meshes.\n%s"),
        TilesetCount,
        MinimumCalibrationTilesetLoadProgress,
        *StructuralReport);
    return true;
}

bool ValidatePlayWorldV2Readiness(UWorld* PlayWorld, FString& OutReport)
{
    TArray<FString> Errors;
    TArray<FString> Notes;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        OutReport = TEXT("Istana PIE readiness FAILED: no Play-In-Editor world exists.");
        return false;
    }
    if (!PlayWorld->IsGameWorld() || !PlayWorld->HasBegunPlay() ||
        (GEditor && GEditor->IsSimulatingInEditor()))
    {
        OutReport = TEXT("Istana PIE readiness FAILED: PlayWorld has not begun authoritative non-simulated play.");
        return false;
    }
    const FString SourcePackageName = UWorld::RemovePIEPrefix(
        PlayWorld->GetOutermost()->GetName());
    if (SourcePackageName != DestinationMapV2Package)
    {
        OutReport = FString::Printf(
            TEXT("Istana PIE readiness FAILED: PlayWorld source package is '%s', expected '%s'."),
            *SourcePackageName,
            *DestinationMapV2Package);
        return false;
    }

    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* ExpectedGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    AGameModeBase* ActiveGameMode = PlayWorld->GetAuthGameMode();
    if (!ExternalAirSimGameModeClass ||
        !ExpectedGameModeClass ||
        ExternalAirSimGameModeClass->GetPathName() != ExternalAirSimGameModeClassPath ||
        ExpectedGameModeClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !ExpectedGameModeClass->IsChildOf(ExternalAirSimGameModeClass))
    {
        Errors.Add(TEXT("The TRIAD-owned AirSim GameMode wrapper or its external AirSim lineage is unavailable."));
    }
    else if (!ActiveGameMode ||
        ActiveGameMode->GetWorld() != PlayWorld ||
        ActiveGameMode->GetClass() != ExpectedGameModeClass)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE instantiated GameMode '%s' instead of exact wrapper '%s'."),
            ActiveGameMode ? *ActiveGameMode->GetClass()->GetPathName() : TEXT("<none>"),
            *IstanaAirSimGameModeClassPath));
    }

    TArray<ATRIADIstanaRuntimePolicyActor*> RuntimePolicies;
    for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(PlayWorld); It; ++It)
    {
        if (It->ActorHasTag(RuntimePolicyActorTag))
        {
            RuntimePolicies.Add(*It);
        }
    }
    ATRIADIstanaRuntimePolicyActor* RuntimePolicy =
        RuntimePolicies.Num() == 1 ? RuntimePolicies[0] : nullptr;
    if (!RuntimePolicy)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE contains %d tagged runtime policy actor(s); expected exactly one."),
            RuntimePolicies.Num()));
    }
    else
    {
        if (!RuntimePolicy->HasActorBegunPlay() ||
            RuntimePolicy->GetWorld() != PlayWorld)
        {
            Errors.Add(TEXT("PIE runtime policy has not begun play in the inspected PlayWorld."));
        }
        if (!RuntimePolicy->bSuppressAirSimVisualWeather ||
            !RuntimePolicy->IsVisualWeatherSuppressionActive())
        {
            Errors.Add(TEXT("PIE runtime policy has not verified current AirSim rain/fog suppression."));
        }
        int32 LiveFogComponentCount = 0;
        const bool bLiveFogSuppressed =
            RuntimePolicy->IsExponentialHeightFogSuppressionActive(
                LiveFogComponentCount);
        if (!RuntimePolicy->bSuppressInheritedExponentialHeightFog ||
            !RuntimePolicy->bExponentialHeightFogSuppressionVerifiedAtRuntime ||
            LiveFogComponentCount !=
                RuntimePolicy->ExponentialHeightFogComponentCountAtRuntime ||
            !bLiveFogSuppressed)
        {
            Errors.Add(FString::Printf(
                TEXT("PIE inherited exponential-height fog is not fully suppressed: live=%d component(s), last-verified=%d; every component must have exact zero primary/secondary density, zero max opacity, and volumetric fog disabled."),
                LiveFogComponentCount,
                RuntimePolicy->ExponentialHeightFogComponentCountAtRuntime));
        }
        else
        {
            Notes.Add(FString::Printf(
                TEXT("Verified exact zero-density/zero-opacity/non-volumetric state across %d exponential-height-fog component(s); sky and cloud actors remain intact."),
                LiveFogComponentCount));
        }
        if (!RuntimePolicy->bGameModeOverrideVerifiedAtRuntime)
        {
            Errors.Add(TEXT("PIE runtime policy did not verify the instantiated GameMode at BeginPlay."));
        }
        FString CameraQualityReason;
        if (!RuntimePolicy->bApplyRuntimeCameraQualityProfile ||
            !RuntimePolicy->bRuntimeCameraQualityProfileVerifiedAtRuntime ||
            !RuntimePolicy->IsRuntimeCameraQualityProfileActive(
                CameraQualityReason))
        {
            Errors.Add(TEXT("PIE tagged-camera quality profile is unverified: ") +
                CameraQualityReason);
        }
        else
        {
            Notes.Add(CameraQualityReason);
        }
        if (!RuntimePolicy->bEnforceTaggedRuntimeCameraForPlayer0 ||
            RuntimePolicy->RequiredRuntimeCameraTag != RuntimeCameraActorTag ||
            !RuntimePolicy->bRuntimeCameraEnforcementComplete ||
            !RuntimePolicy->bRuntimeCameraViewTargetVerifiedAtRuntime)
        {
            Errors.Add(TEXT("Bounded post-AirSim Player 0 camera enforcement is incomplete or unverified."));
        }
    }

    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(PlayWorld); It; ++It)
    {
        ExteriorActors.Add(*It);
    }
    ATRIADIstanaExteriorMeshActor* Exterior =
        ExteriorActors.Num() == 1 ? ExteriorActors[0] : nullptr;
    if (!Exterior || !Exterior->bRequiredExteriorMeshLoaded ||
        !Exterior->ExteriorMeshComponent ||
        !Exterior->ExteriorMeshComponent->GetStaticMesh() ||
        Exterior->ExteriorMeshComponent->GetStaticMesh()->GetPathName() !=
            IstanaExteriorMeshObjectPath)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE contains %d imported Istana exterior actor(s), but not exactly one valid loaded exterior."),
            ExteriorActors.Num()));
    }

    if (RuntimePolicy && Exterior && Exterior->ExteriorMeshComponent)
    {
        int32 StreamedTilesetCount = 0;
        float StreamedMinimumProgress = 0.0f;
        FString StreamedVisualReason;
        const bool bStreamedPrimaryActive =
            RuntimePolicy->IsStreamedPrimaryVisualActive(
                StreamedTilesetCount,
                StreamedMinimumProgress,
                StreamedVisualReason);
        const bool bFallbackVisible =
            RuntimePolicy->bAuthoredExteriorFallbackVisibleAtRuntime &&
            RuntimePolicy->bAuthoredExteriorCollisionPreservedAtRuntime &&
            Exterior->ExteriorMeshComponent->IsVisible() &&
            !Exterior->ExteriorMeshComponent->bHiddenInGame &&
            Exterior->ExteriorMeshComponent->GetCollisionEnabled() !=
                ECollisionEnabled::NoCollision;
        if (bStreamedPrimaryActive)
        {
            Notes.Add(TEXT("Istana visual source is STREAMED_PHOTOGRAMMETRY_PRIMARY_EXPLICIT_OPT_IN with the refined authored exterior retained as invisible collision fallback: ") +
                StreamedVisualReason);
        }
        else if (bFallbackVisible)
        {
            Notes.Add(RuntimePolicy->bPreferStreamedIstanaVisualWhenReady
                ? TEXT("Istana visual source is AUTHORED_REFINED_FALLBACK_PENDING_STREAMED_OPT_IN: ") +
                    StreamedVisualReason
                : TEXT("Istana visual source is AUTHORED_REFINED_PRIMARY; streamed photogrammetry supplies surrounding context only: ") +
                    StreamedVisualReason);
        }
        else
        {
            Errors.Add(TEXT("Neither streamed-primary nor visible authored-fallback Istana state is safely active: ") +
                StreamedVisualReason);
        }
    }

    TArray<ACameraActor*> RuntimeCameras;
    for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
    {
        if (It->ActorHasTag(RuntimeCameraActorTag))
        {
            RuntimeCameras.Add(*It);
        }
    }
    ACameraActor* RuntimeCamera = RuntimeCameras.Num() == 1 ? RuntimeCameras[0] : nullptr;
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(PlayWorld, 0);
    if (PlayerController &&
        (PlayerController->GetWorld() != PlayWorld || !PlayerController->IsLocalController()))
    {
        PlayerController = nullptr;
    }
    if (!RuntimeCamera || !PlayerController)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE requires exactly one tagged runtime camera and Player 0; found %d camera(s), controller=%s."),
            RuntimeCameras.Num(),
            PlayerController ? TEXT("true") : TEXT("false")));
    }
    else if (PlayerController->GetViewTarget() != RuntimeCamera)
    {
        const AActor* ActualViewTarget = PlayerController->GetViewTarget();
        Errors.Add(FString::Printf(
            TEXT("Player 0 view target is '%s', not the tagged Istana runtime camera '%s'."),
            ActualViewTarget ? *ActualViewTarget->GetName() : TEXT("<none>"),
            *RuntimeCamera->GetName()));
    }

    if (Exterior && RuntimeCamera && PlayerController)
    {
        const FVector ExteriorUp = Exterior->GetActorUpVector().GetSafeNormal();
        const FVector ExteriorToCamera =
            RuntimeCamera->GetActorLocation() - Exterior->GetActorLocation();
        const double VerticalMeters = FVector::DotProduct(ExteriorToCamera, ExteriorUp) / 100.0;
        const FVector HorizontalOffset =
            ExteriorToCamera - ExteriorUp * FVector::DotProduct(ExteriorToCamera, ExteriorUp);
        const double HorizontalMeters = HorizontalOffset.Size() / 100.0;
        const double DistanceMeters = ExteriorToCamera.Size() / 100.0;
        const FVector AimPoint = Exterior->GetActorLocation() +
            ExteriorUp * RuntimeCameraAimAboveGroundMeters * 100.0;
        const FVector CameraToAim =
            (AimPoint - RuntimeCamera->GetActorLocation()).GetSafeNormal();
        const double AimDot = FVector::DotProduct(
            RuntimeCamera->GetActorForwardVector(),
            CameraToAim);
        if (Exterior->GetActorLocation().ContainsNaN() ||
            RuntimeCamera->GetActorLocation().ContainsNaN() ||
            RuntimeCamera->GetActorRotation().ContainsNaN() ||
            ExteriorUp.IsNearlyZero() ||
            !FMath::IsFinite(VerticalMeters) ||
            !FMath::IsFinite(HorizontalMeters) ||
            !FMath::IsFinite(DistanceMeters) ||
            FMath::Abs(HorizontalMeters - RuntimeCameraSouthOffsetMeters) > 5.0 ||
            FMath::Abs(VerticalMeters - RuntimeCameraHeightAboveGroundMeters) > 5.0 ||
            FMath::Abs(DistanceMeters - FMath::Sqrt(
                FMath::Square(RuntimeCameraSouthOffsetMeters) +
                FMath::Square(RuntimeCameraHeightAboveGroundMeters))) > 7.0)
        {
            Errors.Add(FString::Printf(
                TEXT("Tagged runtime camera relative pose is invalid (horizontal=%.1f m, height=%.1f m, distance=%.1f m)."),
                HorizontalMeters,
                VerticalMeters,
                DistanceMeters));
        }
        if (!FMath::IsFinite(AimDot) || AimDot < 0.999)
        {
            Errors.Add(FString::Printf(
                TEXT("Tagged runtime camera does not aim at the exterior elevation (forward dot %.4f)."),
                AimDot));
        }

        FVector PlayerViewLocation;
        FRotator PlayerViewRotation;
        PlayerController->GetPlayerViewPoint(PlayerViewLocation, PlayerViewRotation);
        const UCameraComponent* CameraComponent = RuntimeCamera->GetCameraComponent();
        if (!CameraComponent ||
            !FMath::IsFinite(CameraComponent->FieldOfView) ||
            !FMath::IsNearlyEqual(
                CameraComponent->FieldOfView,
                RuntimeCameraFieldOfViewDegrees,
                0.01f))
        {
            Errors.Add(FString::Printf(
                TEXT("Tagged runtime camera does not use the required %.1f degree facade-view FOV."),
                RuntimeCameraFieldOfViewDegrees));
        }
        const FVector CameraViewLocation = CameraComponent
            ? CameraComponent->GetComponentLocation()
            : RuntimeCamera->GetActorLocation();
        const FVector CameraViewForward = CameraComponent
            ? CameraComponent->GetForwardVector()
            : RuntimeCamera->GetActorForwardVector();
        const double PlayerViewOffsetMeters =
            FVector::Distance(PlayerViewLocation, CameraViewLocation) / 100.0;
        const double PlayerViewForwardDot = FVector::DotProduct(
            PlayerViewRotation.Vector(),
            CameraViewForward);
        if (PlayerViewLocation.ContainsNaN() ||
            PlayerViewRotation.ContainsNaN() ||
            CameraViewLocation.ContainsNaN() ||
            CameraViewForward.ContainsNaN() ||
            !FMath::IsFinite(PlayerViewOffsetMeters) ||
            !FMath::IsFinite(PlayerViewForwardDot) ||
            PlayerViewOffsetMeters > 0.5 || PlayerViewForwardDot < 0.999)
        {
            Errors.Add(FString::Printf(
                TEXT("Player 0 rendered view does not match the tagged camera (offset=%.2f m, forward dot=%.4f)."),
                PlayerViewOffsetMeters,
                PlayerViewForwardDot));
        }

        APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
        if (!CameraManager ||
            !FMath::IsFinite(CameraManager->GetCameraCacheTime()) ||
            CameraManager->GetCameraCacheTime() <= 0.0f)
        {
            Errors.Add(TEXT("Player 0 camera-manager cache has not produced a rendered POV yet."));
        }
        else
        {
            const FMinimalViewInfo& CachedPov = CameraManager->GetCameraCacheView();
            const double CachedOffsetMeters =
                FVector::Distance(CachedPov.Location, CameraViewLocation) / 100.0;
            const double CachedForwardDot = FVector::DotProduct(
                CachedPov.Rotation.Vector(),
                CameraViewForward);
            const float ExpectedFieldOfView = CameraComponent
                ? CameraComponent->FieldOfView
                : RuntimeCameraFieldOfViewDegrees;
            if (CachedPov.Location.ContainsNaN() ||
                CachedPov.Rotation.ContainsNaN() ||
                !FMath::IsFinite(CachedPov.FOV) ||
                !FMath::IsFinite(CachedOffsetMeters) ||
                !FMath::IsFinite(CachedForwardDot) ||
                !FMath::IsFinite(ExpectedFieldOfView) ||
                CachedPov.FOV <= 1.0f || CachedPov.FOV >= 170.0f ||
                CachedOffsetMeters > 0.5 ||
                CachedForwardDot < 0.999 ||
                FMath::Abs(CachedPov.FOV - ExpectedFieldOfView) > 0.25f)
            {
                Errors.Add(FString::Printf(
                    TEXT("Player 0 cached POV does not match the tagged camera (time=%.3f, offset=%.2f m, forward dot=%.4f, FOV=%.2f/%.2f)."),
                    CameraManager->GetCameraCacheTime(),
                    CachedOffsetMeters,
                    CachedForwardDot,
                    CachedPov.FOV,
                    ExpectedFieldOfView));
            }
        }
        Notes.Add(FString::Printf(
            TEXT("Runtime-origin-safe relative camera pose: horizontal %.1f m, height %.1f m, distance %.1f m, aim dot %.4f."),
            HorizontalMeters,
            VerticalMeters,
            DistanceMeters,
            AimDot));
    }

    TArray<FString> UnreadyTilesets;
    int32 TilesetCount = 0;
    float MinimumLoadProgress = 100.0f;
    for (TActorIterator<ACesium3DTileset> It(PlayWorld); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        ++TilesetCount;
        const float LoadProgress = Tileset->GetLoadProgress();
        if (FMath::IsFinite(LoadProgress))
        {
            MinimumLoadProgress = FMath::Min(MinimumLoadProgress, LoadProgress);
        }
        if (!FMath::IsFinite(LoadProgress) ||
            LoadProgress < MinimumCalibrationTilesetLoadProgress ||
            !Tileset->GetCreatePhysicsMeshes())
        {
            UnreadyTilesets.Add(FString::Printf(
                TEXT("%s (%.1f%%, physics=%s)"),
                *Tileset->GetName(),
                LoadProgress,
                Tileset->GetCreatePhysicsMeshes() ? TEXT("true") : TEXT("false")));
        }
    }
    if (TilesetCount == 0 || UnreadyTilesets.Num() > 0)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE Cesium readiness is incomplete: %d/%d tileset(s) below %.0f%% or without physics (minimum %.1f%%): %s."),
            UnreadyTilesets.Num(),
            TilesetCount,
            MinimumCalibrationTilesetLoadProgress,
            MinimumLoadProgress,
            *FString::Join(UnreadyTilesets, TEXT(", "))));
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Istana PIE readiness FAILED:\n - ") +
            FString::Join(Errors, TEXT("\n - "));
        if (Notes.Num() > 0)
        {
            OutReport += TEXT("\nNotes:\n - ") + FString::Join(Notes, TEXT("\n - "));
        }
        return false;
    }

    Notes.Add(FString::Printf(
        TEXT("Exact TRIAD AirSim wrapper, runtime policy, Player 0 tagged-camera view, and %d Cesium tileset(s) are ready (minimum %.1f%%)."),
        TilesetCount,
        MinimumLoadProgress));
    OutReport = TEXT("Istana PIE readiness PASSED:\n - ") +
        FString::Join(Notes, TEXT("\n - "));
    return true;
}

bool ReloadAndValidatePersistedRuntimeMapV2(FString& OutReport)
{
    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapV2Package,
            &DestinationFilename))
    {
        OutReport = FString::Printf(
            TEXT("Saved destination '%s' cannot be resolved for persistence validation."),
            *DestinationMapV2Package);
        return false;
    }

    UWorld* ReloadedWorld = UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (!ReloadedWorld || ReloadedWorld->GetOutermost()->GetName() != DestinationMapV2Package)
    {
        OutReport = FString::Printf(
            TEXT("Could not reopen '%s' from disk for persistence validation."),
            *DestinationMapV2Package);
        return false;
    }

    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* ExpectedGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    AWorldSettings* ReloadedWorldSettings = ReloadedWorld->GetWorldSettings();
    const UClass* ReloadedGameMode = ReloadedWorldSettings
        ? ReloadedWorldSettings->DefaultGameMode.Get()
        : nullptr;
    if (!ExternalAirSimGameModeClass ||
        !ExpectedGameMode ||
        ExpectedGameMode->GetPathName() != IstanaAirSimGameModeClassPath ||
        !ExpectedGameMode->IsChildOf(ExternalAirSimGameModeClass))
    {
        OutReport = FString::Printf(
            TEXT("Reopened the destination, but TRIAD wrapper '%s' is unavailable or no longer derives from external AirSim class '%s'."),
            *IstanaAirSimGameModeClassPath,
            *ExternalAirSimGameModeClassPath);
        return false;
    }
    if (!ReloadedGameMode || ReloadedGameMode->GetPathName() != IstanaAirSimGameModeClassPath)
    {
        OutReport = FString::Printf(
            TEXT("Reopened the destination with the TRIAD AirSim wrapper loadable, but its serialized GameMode Override is '%s' instead of '%s'."),
            ReloadedGameMode ? *ReloadedGameMode->GetPathName() : TEXT("<none>"),
            *IstanaAirSimGameModeClassPath);
        return false;
    }

    FString ReloadedValidation;
    if (!ValidateRuntimeWorldV2(ReloadedWorld, true, ReloadedValidation))
    {
        OutReport = TEXT("Reloaded destination failed persistence validation: ") +
            ReloadedValidation;
        return false;
    }

    OutReport = TEXT("Reloaded the saved destination from disk and revalidated its serialized World Settings and v2 contracts.\n") +
        ReloadedValidation;
    return true;
}

struct FSurveyPoint
{
    FString Id;
    FVector LongitudeLatitudeHeight = FVector::ZeroVector;
    FVector WorldPosition = FVector::ZeroVector;
    double EastMeters = 0.0;
    double NorthMeters = 0.0;
    double RadiusMeters = 0.0;
    double OutwardBearingDegrees = 0.0;
    double AltitudeAglMeters = 0.0;
    bool bSurfaceResolved = false;
    FString SurfaceActor;
};

struct FSurveyScenario
{
    FString ScenarioId;
    FString WeatherProfile;
    bool bRfEmitting = true;
};

struct FSurveyRequest
{
    FString RequestId;
    FString PackageId;
    double PackageCostUnits = 1.0;
    double CameraFieldOfViewDegrees = 90.0;
    double SearchRadarRangeMeters = 5000.0;
    double RgbRangeMeters = 1000.0;
    bool bHasSearchRadar = false;
    bool bHasRgb = false;
    int32 MaximumSampleCount = 100000;
    TArray<double> TargetAltitudeBandsAglMeters;
    TArray<FSurveyScenario> Scenarios;
    TSharedPtr<FJsonObject> UnrealNodeTemplate;
};

bool ResolveSafePlacementRequestPath(
    const FString& RequestJsonPath,
    FString& OutResolvedPath,
    FString& OutError)
{
    if (RequestJsonPath.IsEmpty() || RequestJsonPath.Contains(TEXT("..")))
    {
        OutError = TEXT("RequestJsonPath is empty or contains '..'.");
        return false;
    }

    const FString ConfigRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
    const FString SavedRequestRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("TRIAD"), TEXT("PlacementRequests")));
    const FString PluginResourceRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectPluginsDir(), TEXT("TRIADSensorFusion"), TEXT("Resources")));

    TArray<FString> Candidates;
    if (FPaths::IsRelative(RequestJsonPath))
    {
        if (FPaths::GetCleanFilename(RequestJsonPath) != RequestJsonPath)
        {
            OutError = TEXT("Relative RequestJsonPath must be one filename; use an absolute allowlisted path otherwise.");
            return false;
        }
        Candidates.Add(FPaths::Combine(ConfigRoot, RequestJsonPath));
        Candidates.Add(FPaths::Combine(SavedRequestRoot, RequestJsonPath));
        Candidates.Add(FPaths::Combine(PluginResourceRoot, RequestJsonPath));
    }
    else
    {
        Candidates.Add(RequestJsonPath);
    }

    for (FString Candidate : Candidates)
    {
        Candidate = FPaths::ConvertRelativePathToFull(Candidate);
        FPaths::NormalizeFilename(Candidate);
        const bool bAllowlisted = FPaths::IsUnderDirectory(Candidate, ConfigRoot) ||
            FPaths::IsUnderDirectory(Candidate, SavedRequestRoot) ||
            FPaths::IsUnderDirectory(Candidate, PluginResourceRoot);
        if (bAllowlisted && IFileManager::Get().FileExists(*Candidate) &&
            Candidate.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
        {
            OutResolvedPath = Candidate;
            return true;
        }
    }

    OutError = TEXT("Placement request must be a .json file under project Config, Saved/TRIAD/PlacementRequests, or TRIADSensorFusion/Resources.");
    return false;
}

bool ParseSurveyRequest(
    const FString& RequestJsonPath,
    FSurveyRequest& OutRequest,
    FString& OutError)
{
    FString ResolvedPath;
    if (!ResolveSafePlacementRequestPath(RequestJsonPath, ResolvedPath, OutError))
    {
        return false;
    }
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
    {
        OutError = TEXT("Could not read the allowlisted placement request.");
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Placement request is not valid JSON.");
        return false;
    }
    FString SchemaVersion;
    if (!Root->TryGetStringField(TEXT("schemaVersion"), SchemaVersion) ||
        SchemaVersion != TEXT("triad.placement_request.v1") ||
        !Root->TryGetStringField(TEXT("requestId"), OutRequest.RequestId) ||
        OutRequest.RequestId.IsEmpty())
    {
        OutError = TEXT("Placement request requires schemaVersion triad.placement_request.v1 and a non-empty requestId.");
        return false;
    }

    const TSharedPtr<FJsonObject>* Aoi = nullptr;
    const TSharedPtr<FJsonObject>* Center = nullptr;
    double RadiusMeters = 0.0;
    double CandidateBufferMeters = 0.0;
    double LongitudeDegrees = 0.0;
    double LatitudeDegrees = 0.0;
    if (!Root->TryGetObjectField(TEXT("aoi"), Aoi) || !Aoi || !Aoi->IsValid() ||
        !(*Aoi)->TryGetObjectField(TEXT("center"), Center) || !Center || !Center->IsValid() ||
        !(*Center)->TryGetNumberField(TEXT("longitudeDegrees"), LongitudeDegrees) ||
        !(*Center)->TryGetNumberField(TEXT("latitudeDegrees"), LatitudeDegrees) ||
        !(*Aoi)->TryGetNumberField(TEXT("radiusMeters"), RadiusMeters))
    {
        OutError = TEXT("Placement request AOI center/radius is missing.");
        return false;
    }
    (*Aoi)->TryGetNumberField(TEXT("candidateBufferMeters"), CandidateBufferMeters);
    if (TRIAD::Geodesy::Wgs84DistanceMeters(
            CenterLongitudeDegrees,
            CenterLatitudeDegrees,
            LongitudeDegrees,
            LatitudeDegrees) > 0.10 ||
        FMath::Abs(RadiusMeters - StudyRadiusMeters) > 0.01 ||
        FMath::Abs(CandidateBufferMeters) > 0.001)
    {
        OutError = TEXT("Request AOI must match the loaded 1000 m Istana geodesic circle with candidateBufferMeters=0.");
        return false;
    }

    const TSharedPtr<FJsonObject>* Sampling = nullptr;
    if (Root->TryGetObjectField(TEXT("sampling"), Sampling) && Sampling && Sampling->IsValid())
    {
        double MaximumSampleCount = 100000.0;
        (*Sampling)->TryGetNumberField(TEXT("maximumSampleCount"), MaximumSampleCount);
        OutRequest.MaximumSampleCount = FMath::Clamp(static_cast<int32>(MaximumSampleCount), 1, 100000);
        const TArray<TSharedPtr<FJsonValue>>* Bands = nullptr;
        if ((*Sampling)->TryGetArrayField(TEXT("targetAltitudeBandsAglMeters"), Bands) && Bands)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Bands)
            {
                double BandMeters = 0.0;
                if (Value.IsValid() && Value->TryGetNumber(BandMeters) &&
                    FMath::IsFinite(BandMeters) && BandMeters > 0.0 && BandMeters <= 1000.0)
                {
                    OutRequest.TargetAltitudeBandsAglMeters.AddUnique(BandMeters);
                }
                if (OutRequest.TargetAltitudeBandsAglMeters.Num() >= 8)
                {
                    break;
                }
            }
        }
    }
    if (OutRequest.TargetAltitudeBandsAglMeters.Num() == 0)
    {
        OutRequest.TargetAltitudeBandsAglMeters.Add(20.0);
    }
    OutRequest.TargetAltitudeBandsAglMeters.Sort();

    const TArray<TSharedPtr<FJsonValue>>* Packages = nullptr;
    if (!Root->TryGetArrayField(TEXT("sensorPackages"), Packages) || !Packages || Packages->Num() != 1)
    {
        OutError = TEXT("This bounded exporter currently requires exactly one sensor package.");
        return false;
    }
    const TSharedPtr<FJsonObject> Package = (*Packages)[0].IsValid()
        ? (*Packages)[0]->AsObject()
        : nullptr;
    if (!Package.IsValid() || !Package->TryGetStringField(TEXT("packageId"), OutRequest.PackageId) ||
        OutRequest.PackageId.IsEmpty())
    {
        OutError = TEXT("Sensor package requires a non-empty packageId.");
        return false;
    }
    Package->TryGetNumberField(TEXT("costUnits"), OutRequest.PackageCostUnits);
    const TArray<TSharedPtr<FJsonValue>>* Modalities = nullptr;
    if (Package->TryGetArrayField(TEXT("modalities"), Modalities) && Modalities)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Modalities)
        {
            FString Modality;
            if (Value.IsValid() && Value->TryGetString(Modality))
            {
                OutRequest.bHasSearchRadar |= Modality == TEXT("SEARCH_RADAR");
                OutRequest.bHasRgb |= Modality.Equals(TEXT("rgb"), ESearchCase::IgnoreCase);
            }
        }
    }
    if (!OutRequest.bHasSearchRadar && !OutRequest.bHasRgb)
    {
        OutError = TEXT("Sensor package must expose SEARCH_RADAR and/or rgb for this geometry survey.");
        return false;
    }
    const TSharedPtr<FJsonObject>* Template = nullptr;
    if (!Package->TryGetObjectField(TEXT("unrealNodeTemplate"), Template) || !Template || !Template->IsValid())
    {
        OutError = TEXT("Sensor package requires unrealNodeTemplate.");
        return false;
    }
    OutRequest.UnrealNodeTemplate = *Template;
    OutRequest.UnrealNodeTemplate->TryGetNumberField(
        TEXT("CameraFieldOfViewDegrees"), OutRequest.CameraFieldOfViewDegrees);
    OutRequest.UnrealNodeTemplate->TryGetNumberField(
        TEXT("SearchRadarRangeMeters"), OutRequest.SearchRadarRangeMeters);
    OutRequest.UnrealNodeTemplate->TryGetNumberField(
        TEXT("DetectionRangeMeters"), OutRequest.RgbRangeMeters);
    OutRequest.CameraFieldOfViewDegrees = FMath::Clamp(OutRequest.CameraFieldOfViewDegrees, 5.0, 170.0);
    OutRequest.SearchRadarRangeMeters = FMath::Max(OutRequest.SearchRadarRangeMeters, 5000.0);
    OutRequest.RgbRangeMeters = FMath::Max(OutRequest.RgbRangeMeters, 1.0);

    const TArray<TSharedPtr<FJsonValue>>* Scenarios = nullptr;
    if (!Root->TryGetArrayField(TEXT("scenarios"), Scenarios) || !Scenarios ||
        Scenarios->Num() == 0 || Scenarios->Num() > 16)
    {
        OutError = TEXT("Placement request must contain 1..16 scenarios.");
        return false;
    }
    TSet<FString> ScenarioIds;
    for (const TSharedPtr<FJsonValue>& Value : *Scenarios)
    {
        const TSharedPtr<FJsonObject> ScenarioJson = Value.IsValid() ? Value->AsObject() : nullptr;
        FSurveyScenario Scenario;
        if (!ScenarioJson.IsValid() ||
            !ScenarioJson->TryGetStringField(TEXT("scenarioId"), Scenario.ScenarioId) ||
            Scenario.ScenarioId.IsEmpty() || ScenarioIds.Contains(Scenario.ScenarioId))
        {
            OutError = TEXT("Every scenario requires a unique non-empty scenarioId.");
            return false;
        }
        ScenarioJson->TryGetStringField(TEXT("weatherProfile"), Scenario.WeatherProfile);
        ScenarioJson->TryGetBoolField(TEXT("rfEmitting"), Scenario.bRfEmitting);
        ScenarioIds.Add(Scenario.ScenarioId);
        OutRequest.Scenarios.Add(Scenario);
    }
    return true;
}

TSharedPtr<FJsonObject> MakeLocationJson(const FVector& LongitudeLatitudeHeight)
{
    TSharedPtr<FJsonObject> Location = MakeShared<FJsonObject>();
    Location->SetNumberField(TEXT("latitudeDegrees"), LongitudeLatitudeHeight.Y);
    Location->SetNumberField(TEXT("longitudeDegrees"), LongitudeLatitudeHeight.X);
    Location->SetNumberField(TEXT("heightMeters"), LongitudeLatitudeHeight.Z);
    Location->SetStringField(TEXT("heightReference"), TEXT("WGS84_ELLIPSOID"));
    return Location;
}

FSurveyPoint ResolveSurveyPoint(
    UWorld* World,
    ACesiumGeoreference* Georeference,
    const FString& Id,
    double EastMeters,
    double NorthMeters,
    double AltitudeAglMeters)
{
    FSurveyPoint Point;
    Point.Id = Id;
    Point.EastMeters = EastMeters;
    Point.NorthMeters = NorthMeters;
    Point.RadiusMeters = FMath::Sqrt(EastMeters * EastMeters + NorthMeters * NorthMeters);
    Point.OutwardBearingDegrees = Point.RadiusMeters > UE_DOUBLE_SMALL_NUMBER
        ? FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(EastMeters, NorthMeters)) + 360.0, 360.0)
        : 0.0;
    Point.AltitudeAglMeters = AltitudeAglMeters;

    const FVector2D Destination = TRIAD::Geodesy::Wgs84DestinationDegrees(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        Point.OutwardBearingDegrees,
        Point.RadiusMeters);
    const FVector TraceStart = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
        Destination.X,
        Destination.Y,
        ApproximateCenterHeightMeters + 1000.0));
    const FVector TraceEnd = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
        Destination.X,
        Destination.Y,
        ApproximateCenterHeightMeters - 500.0));
    FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(TRIADIstanaSurveySurface), true);
    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParameters))
    {
        Point.bSurfaceResolved = true;
        Point.SurfaceActor = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("<component-only-hit>");
        Point.WorldPosition = Hit.ImpactPoint + FVector::UpVector * AltitudeAglMeters * 100.0;
        Point.LongitudeLatitudeHeight =
            Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(Point.WorldPosition);
    }
    else
    {
        Point.WorldPosition = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
            Destination.X,
            Destination.Y,
            ApproximateCenterHeightMeters + AltitudeAglMeters));
        Point.LongitudeLatitudeHeight = FVector(
            Destination.X,
            Destination.Y,
            ApproximateCenterHeightMeters + AltitudeAglMeters);
        Point.SurfaceActor = TEXT("UNRESOLVED");
    }
    return Point;
}

void AddSurveyRing(
    UWorld* World,
    ACesiumGeoreference* Georeference,
    const FString& Prefix,
    double RadiusMeters,
    int32 Count,
    double AltitudeAglMeters,
    TArray<FSurveyPoint>& OutPoints)
{
    if (Count <= 0)
    {
        return;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const double BearingDegrees = static_cast<double>(Index) * 360.0 / Count;
        const double BearingRadians = FMath::DegreesToRadians(BearingDegrees);
        const double EastMeters = RadiusMeters * FMath::Sin(BearingRadians);
        const double NorthMeters = RadiusMeters * FMath::Cos(BearingRadians);
        OutPoints.Add(ResolveSurveyPoint(
            World,
            Georeference,
            FString::Printf(TEXT("%s-%03d"), *Prefix, Index),
            EastMeters,
            NorthMeters,
            AltitudeAglMeters));
    }
}

double MedianOfSortedHeights(const TArray<double>& SortedHeights)
{
    if (SortedHeights.Num() == 0)
    {
        return 0.0;
    }
    const int32 Middle = SortedHeights.Num() / 2;
    return SortedHeights.Num() % 2 == 0
        ? (SortedHeights[Middle - 1] + SortedHeights[Middle]) * 0.5
        : SortedHeights[Middle];
}

bool SampleIstanaExteriorRingGroundHeight(
    UWorld* World,
    ACesiumGeoreference* Georeference,
    ATRIADIstanaExteriorMeshActor* ExteriorMeshActor,
    ATRIADIstanaStudyAreaActor* StudyArea,
    double& OutMedianGroundHeightMeters,
    int32& OutRawSampleCount,
    int32& OutAcceptedSampleCount,
    FString& OutError)
{
    OutMedianGroundHeightMeters = ApproximateCenterHeightMeters;
    OutRawSampleCount = 0;
    OutAcceptedSampleCount = 0;
    if (!World || !Georeference || !ExteriorMeshActor || !StudyArea)
    {
        OutError = TEXT("Ground calibration requires the v2 world, preserved georeference, exterior mesh, and study-area actor.");
        return false;
    }

    int32 TilesetCount = 0;
    int32 UnreadyTilesetCount = 0;
    float MinimumLoadProgress = 100.0f;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ++TilesetCount;
        MinimumLoadProgress = FMath::Min(MinimumLoadProgress, It->GetLoadProgress());
        if (It->GetLoadProgress() < MinimumCalibrationTilesetLoadProgress ||
            !It->GetCreatePhysicsMeshes())
        {
            ++UnreadyTilesetCount;
        }
    }
    if (TilesetCount == 0 || UnreadyTilesetCount > 0)
    {
        OutError = FString::Printf(
            TEXT("Cesium calibration is not ready: %d/%d tileset(s) lack >= %.0f%% load progress plus physics meshes (minimum observed %.1f%%). Wait for streaming; no endpoint/token details were inspected or logged."),
            UnreadyTilesetCount,
            TilesetCount,
            MinimumCalibrationTilesetLoadProgress,
            MinimumLoadProgress);
        return false;
    }

    FCollisionQueryParams QueryParameters(
        SCENE_QUERY_STAT(TRIADIstanaGroundCalibration),
        true);
    QueryParameters.AddIgnoredActor(ExteriorMeshActor);
    QueryParameters.AddIgnoredActor(StudyArea);
    QueryParameters.bReturnPhysicalMaterial = false;

    // Both 95 m and 125 m rings lie beyond the ~86 m circumscribed radius of
    // the 124 x 118 m mapping-grade footprint. No center/roof sample is taken.
    static const double SampleRadiiMeters[] = {95.0, 125.0};
    TArray<double> RawHeights;
    for (double RadiusMeters : SampleRadiiMeters)
    {
        for (int32 BearingIndex = 0; BearingIndex < 8; ++BearingIndex)
        {
            const double BearingDegrees = static_cast<double>(BearingIndex) * 45.0;
            const FVector2D LongitudeLatitude = TRIAD::Geodesy::Wgs84DestinationDegrees(
                CenterLongitudeDegrees,
                CenterLatitudeDegrees,
                BearingDegrees,
                RadiusMeters);
            const FVector TraceStart = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
                LongitudeLatitude.X,
                LongitudeLatitude.Y,
                250.0));
            const FVector TraceEnd = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
                LongitudeLatitude.X,
                LongitudeLatitude.Y,
                -100.0));
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(
                    Hit,
                    TraceStart,
                    TraceEnd,
                    ECC_Visibility,
                    QueryParameters) ||
                !Cast<ACesium3DTileset>(Hit.GetActor()))
            {
                continue;
            }
            const FVector HitLongitudeLatitudeHeight =
                Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.ImpactPoint);
            if (FMath::IsFinite(HitLongitudeLatitudeHeight.Z) &&
                HitLongitudeLatitudeHeight.Z >= -50.0 &&
                HitLongitudeLatitudeHeight.Z <= 150.0)
            {
                RawHeights.Add(HitLongitudeLatitudeHeight.Z);
            }
        }
    }
    RawHeights.Sort();
    OutRawSampleCount = RawHeights.Num();
    if (RawHeights.Num() < MinimumCalibrationAcceptedSamples)
    {
        OutError = FString::Printf(
            TEXT("Only %d/16 exterior-ring traces resolved against Cesium collision; at least %d are required. The 47 m fallback was retained."),
            RawHeights.Num(),
            MinimumCalibrationAcceptedSamples);
        return false;
    }

    // Select the densest <=4 m vertical cluster. This rejects isolated trees,
    // roofs, and other raised geometry; equal-size clusters prefer the lower
    // one as the conservative ground candidate.
    int32 BestStart = 0;
    int32 BestCount = 0;
    for (int32 Start = 0; Start < RawHeights.Num(); ++Start)
    {
        int32 End = Start;
        while (End + 1 < RawHeights.Num() &&
               RawHeights[End + 1] - RawHeights[Start] <= 4.0)
        {
            ++End;
        }
        const int32 Count = End - Start + 1;
        if (Count > BestCount)
        {
            BestStart = Start;
            BestCount = Count;
        }
    }
    if (BestCount < MinimumCalibrationAcceptedSamples)
    {
        OutError = FString::Printf(
            TEXT("Exterior-ring collision heights do not contain a stable <=4 m ground cluster (%d/%d best); the 47 m fallback was retained."),
            BestCount,
            RawHeights.Num());
        return false;
    }

    TArray<double> AcceptedHeights;
    for (int32 Index = 0; Index < BestCount; ++Index)
    {
        AcceptedHeights.Add(RawHeights[BestStart + Index]);
    }
    OutAcceptedSampleCount = AcceptedHeights.Num();
    OutMedianGroundHeightMeters = MedianOfSortedHeights(AcceptedHeights);
    if (!FMath::IsFinite(OutMedianGroundHeightMeters) ||
        OutMedianGroundHeightMeters < -50.0 ||
        OutMedianGroundHeightMeters > 150.0 ||
        FMath::Abs(OutMedianGroundHeightMeters - ApproximateCenterHeightMeters) > 75.0)
    {
        OutError = FString::Printf(
            TEXT("Median ground candidate %.3f m failed the conservative WGS84-height guard; the 47 m fallback was retained."),
            OutMedianGroundHeightMeters);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ResolveNewSavedFile(
    const FString& Subdirectory,
    const FString& OutputFileName,
    const FString& RequiredExtension,
    FString& OutPath,
    FString& OutError)
{
    if (OutputFileName.IsEmpty() ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        OutputFileName.Contains(TEXT("..")) ||
        !OutputFileName.EndsWith(RequiredExtension, ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(
            TEXT("Output must be one filename ending in %s; directories and '..' are rejected."),
            *RequiredExtension);
        return false;
    }
    const FString OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("TRIAD"), Subdirectory);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    OutPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(OutputDirectory, OutputFileName));
    if (IFileManager::Get().FileExists(*OutPath))
    {
        OutError = FString::Printf(TEXT("Refusing to overwrite existing file '%s'."), *OutPath);
        return false;
    }
    return true;
}

bool WriteJsonWithoutOverwrite(
    const FString& DestinationPath,
    const TSharedRef<FJsonObject>& Root,
    FString& OutError)
{
    FString JsonText;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonText);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("Could not serialize placement survey JSON.");
        return false;
    }
    const FString TemporaryPath = DestinationPath + TEXT(".") +
        FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(JsonText, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("Could not write temporary survey file '%s'."), *TemporaryPath);
        return false;
    }
    if (!IFileManager::Get().Move(*DestinationPath, *TemporaryPath, false, true))
    {
        IFileManager::Get().Delete(*TemporaryPath, false, true);
        OutError = FString::Printf(TEXT("Could not atomically publish new survey '%s'."), *DestinationPath);
        return false;
    }
    return true;
}

bool IsLowercaseSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!((Character >= TEXT('0') && Character <= TEXT('9')) ||
              (Character >= TEXT('a') && Character <= TEXT('f'))))
        {
            return false;
        }
    }
    return true;
}

bool IsDigitalTwinPlaceholder(const FString& Value)
{
    const FString Normalized = Value.TrimStartAndEnd().ToUpper().Replace(TEXT("-"), TEXT("_"));
    return Normalized.IsEmpty() ||
        Normalized == TEXT("TBD") ||
        Normalized == TEXT("TODO") ||
        Normalized == TEXT("UNKNOWN") ||
        Normalized == TEXT("UNSPECIFIED") ||
        Normalized == TEXT("PLACEHOLDER") ||
        Normalized.Contains(TEXT("REPLACE_ME"));
}

bool ResolveDigitalTwinReleaseManifest(
    const FString& ManifestPath,
    FString& OutResolvedPath,
    FString& OutError)
{
    if (ManifestPath.IsEmpty() || ManifestPath.Contains(TEXT("..")))
    {
        OutError = TEXT("Digital-twin ManifestPath is empty or contains '..'.");
        return false;
    }

    FString ReleaseRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets"),
        TEXT("IstanaDigitalTwin"),
        TEXT("Releases")));
    FPaths::NormalizeFilename(ReleaseRoot);

    FString Candidate = ManifestPath;
    if (FPaths::IsRelative(Candidate))
    {
        Candidate = FPaths::Combine(ReleaseRoot, Candidate);
    }
    Candidate = FPaths::ConvertRelativePathToFull(Candidate);
    FPaths::NormalizeFilename(Candidate);

    if (!FPaths::IsUnderDirectory(Candidate, ReleaseRoot) ||
        FPaths::GetCleanFilename(Candidate) != TEXT("manifest.json") ||
        !IFileManager::Get().FileExists(*Candidate))
    {
        OutError = FString::Printf(
            TEXT("Digital-twin metadata must be an existing <releaseId>/manifest.json below '%s'."),
            *ReleaseRoot);
        return false;
    }
    const int64 FileSize = IFileManager::Get().FileSize(*Candidate);
    if (FileSize < 2 || FileSize > 4 * 1024 * 1024)
    {
        OutError = TEXT("Digital-twin manifest size is invalid or exceeds the 4 MiB safety limit.");
        return false;
    }
    OutResolvedPath = Candidate;
    return true;
}

bool ValidateDigitalTwinReleaseMetadataFile(
    const FString& ManifestPath,
    FString& OutResolvedPath,
    FString& OutError)
{
    if (!ResolveDigitalTwinReleaseManifest(ManifestPath, OutResolvedPath, OutError))
    {
        return false;
    }

    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *OutResolvedPath))
    {
        OutError = TEXT("Could not read the allowlisted digital-twin release manifest.");
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Digital-twin release manifest is not valid JSON.");
        return false;
    }

    FString SchemaVersion;
    FString ReleaseId;
    FString SiteId;
    FString ClaimStatus;
    FString ReleaseState;
    FString ReferenceEpochUtc;
    FString DataClassification;
    FString ReleaseInventorySha256;
    FString AcceptancePayloadSha256;
    if (!Root->TryGetStringField(TEXT("schemaVersion"), SchemaVersion) ||
        SchemaVersion != TEXT("triad.istana_digital_twin_release.v1") ||
        !Root->TryGetStringField(TEXT("releaseId"), ReleaseId) ||
        !Root->TryGetStringField(TEXT("siteId"), SiteId) ||
        SiteId != TEXT("istana-main-building") ||
        !Root->TryGetStringField(TEXT("claimStatus"), ClaimStatus) ||
        (ClaimStatus != TEXT("SURVEY_CONTROLLED") && ClaimStatus != TEXT("AS_BUILT_AUTHORIZED")) ||
        !Root->TryGetStringField(TEXT("releaseState"), ReleaseState) ||
        ReleaseState != TEXT("APPROVED_FOR_STAGE0_INTEGRITY_REVIEW") ||
        !Root->TryGetStringField(TEXT("referenceEpochUtc"), ReferenceEpochUtc) ||
        !Root->TryGetStringField(TEXT("dataClassification"), DataClassification) ||
        !Root->TryGetStringField(TEXT("releaseInventorySha256"), ReleaseInventorySha256) ||
        !Root->TryGetStringField(TEXT("acceptancePayloadSha256"), AcceptancePayloadSha256) ||
        !IsLowercaseSha256(ReleaseInventorySha256) ||
        !IsLowercaseSha256(AcceptancePayloadSha256) ||
        IsDigitalTwinPlaceholder(ReleaseId) ||
        IsDigitalTwinPlaceholder(ReferenceEpochUtc) ||
        IsDigitalTwinPlaceholder(DataClassification))
    {
        OutError = TEXT("Release metadata requires the v1 schema, exact Istana site ID, the Stage-0 integrity-review state, a survey/as-built claim, stable release ID, classification, and explicit reference epoch.");
        return false;
    }

    const TSharedPtr<FJsonObject>* Provenance = nullptr;
    const TSharedPtr<FJsonObject>* Crs = nullptr;
    const TSharedPtr<FJsonObject>* Accuracy = nullptr;
    const TSharedPtr<FJsonObject>* Criteria = nullptr;
    const TSharedPtr<FJsonObject>* Qa = nullptr;
    const TSharedPtr<FJsonObject>* ColorQa = nullptr;
    if (!Root->TryGetObjectField(TEXT("provenance"), Provenance) || !Provenance || !Provenance->IsValid() ||
        !Root->TryGetObjectField(TEXT("coordinateReferenceSystem"), Crs) || !Crs || !Crs->IsValid() ||
        !Root->TryGetObjectField(TEXT("declaredAccuracy"), Accuracy) || !Accuracy || !Accuracy->IsValid() ||
        !Root->TryGetObjectField(TEXT("acceptanceCriteria"), Criteria) || !Criteria || !Criteria->IsValid() ||
        !Root->TryGetObjectField(TEXT("qaResults"), Qa) || !Qa || !Qa->IsValid() ||
        !Root->TryGetObjectField(TEXT("colorQa"), ColorQa) || !ColorQa || !ColorQa->IsValid())
    {
        OutError = TEXT("Stage-0 review metadata requires provenance, CRS/vertical transform, declared accuracy, acceptance criteria, and measured QA objects.");
        return false;
    }
    for (const TCHAR* Field : {
             TEXT("rightsHolder"),
             TEXT("acquisitionAuthority"),
             TEXT("licenseId"),
             TEXT("redistribution"),
             TEXT("securityReviewId")})
    {
        FString Value;
        if (!(*Provenance)->TryGetStringField(Field, Value) ||
            IsDigitalTwinPlaceholder(Value) ||
            (FCString::Stricmp(Field, TEXT("redistribution")) == 0 &&
             Value != TEXT("PRIVATE_REPOSITORY_ONLY") && Value != TEXT("AUTHORIZED")))
        {
            OutError = FString::Printf(
                TEXT("Stage-0 provenance field '%s' is missing, placeholder, or unauthorized."),
                Field);
            return false;
        }
    }

    const TSharedPtr<FJsonObject>* Horizontal = nullptr;
    const TSharedPtr<FJsonObject>* Vertical = nullptr;
    const TSharedPtr<FJsonObject>* UnrealFrame = nullptr;
    if (!(*Crs)->TryGetObjectField(TEXT("sourceHorizontal"), Horizontal) || !Horizontal || !Horizontal->IsValid() ||
        !(*Crs)->TryGetObjectField(TEXT("sourceVertical"), Vertical) || !Vertical || !Vertical->IsValid() ||
        !(*Crs)->TryGetObjectField(TEXT("unrealFrame"), UnrealFrame) || !UnrealFrame || !UnrealFrame->IsValid())
    {
        OutError = TEXT("CRS metadata must separate source horizontal CRS, source vertical datum, and Unreal frame.");
        return false;
    }
    FString HorizontalId;
    FString VerticalId;
    FString VerticalTransform;
    FString AxisConvention;
    FString UnrealUnits;
    const TArray<TSharedPtr<FJsonValue>>* SourceToUnreal = nullptr;
    if (!(*Horizontal)->TryGetStringField(TEXT("id"), HorizontalId) || IsDigitalTwinPlaceholder(HorizontalId) ||
        !(*Vertical)->TryGetStringField(TEXT("id"), VerticalId) || IsDigitalTwinPlaceholder(VerticalId) ||
        !(*Vertical)->TryGetStringField(TEXT("transformationToWgs84Ellipsoid"), VerticalTransform) || IsDigitalTwinPlaceholder(VerticalTransform) ||
        !(*UnrealFrame)->TryGetStringField(TEXT("axisConvention"), AxisConvention) || AxisConvention != TEXT("EAST_SOUTH_UP") ||
        !(*UnrealFrame)->TryGetStringField(TEXT("units"), UnrealUnits) || UnrealUnits != TEXT("centimetres") ||
        !(*UnrealFrame)->TryGetArrayField(TEXT("sourceToUnrealMatrix4x4"), SourceToUnreal) || !SourceToUnreal || SourceToUnreal->Num() != 16)
    {
        OutError = TEXT("CRS metadata is placeholder/incomplete or lacks the exact EAST_SOUTH_UP centimetre transform contract.");
        return false;
    }
    for (const TCHAR* Field : {TEXT("horizontalRmseMeters"), TEXT("verticalRmseMeters"), TEXT("confidencePercent")})
    {
        double Value = 0.0;
        const bool bIsRmse = FCString::Stricmp(Field, TEXT("confidencePercent")) != 0;
        if (!(*Accuracy)->TryGetNumberField(Field, Value) ||
            !FMath::IsFinite(Value) ||
            Value <= 0.0 ||
            (bIsRmse && Value > 0.03))
        {
            OutError = FString::Printf(TEXT("Declared accuracy field '%s' must be finite and positive."), Field);
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ControlPoints = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ReferencePhotos = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Approvals = nullptr;
    if (!Root->TryGetArrayField(TEXT("assets"), Assets) || !Assets ||
        !Root->TryGetArrayField(TEXT("groundControlPoints"), ControlPoints) || !ControlPoints ||
        !Root->TryGetArrayField(TEXT("referencePhotos"), ReferencePhotos) || !ReferencePhotos ||
        !Root->TryGetArrayField(TEXT("approvals"), Approvals) || !Approvals)
    {
        OutError = TEXT("Stage-0 review metadata requires asset, ground-control, calibrated-photo, and approval arrays.");
        return false;
    }

    const TSet<FString> RequiredRoles = {
        TEXT("BUILDING_RENDER_MESH"),
        TEXT("BUILDING_COLLISION_MESH"),
        TEXT("SOURCE_POINT_CLOUD"),
        TEXT("SOURCE_POINT_CLOUD_ARCHIVE"),
        TEXT("TERRAIN_DTM_SOURCE"),
        TEXT("TERRAIN_RENDER_MESH"),
        TEXT("VEGETATION_INVENTORY"),
        TEXT("VEGETATION_GEOMETRY_LIBRARY"),
        TEXT("CONTEXT_GEOMETRY"),
        TEXT("REFERENCE_PHOTO"),
        TEXT("REFERENCE_PHOTO_RAW"),
        TEXT("MATERIAL_TEXTURE"),
        TEXT("COLOR_CHART_CAPTURE"),
        TEXT("CAMERA_COLOR_PROFILE"),
        TEXT("LIGHTING_MEASUREMENT"),
        TEXT("UNREAL_ACCEPTANCE_RENDER_CONFIG"),
        TEXT("COLOR_TARGET_RENDER"),
        TEXT("OCIO_CONFIG"),
        TEXT("LIGHT_RIG_CONFIG"),
        TEXT("TONEMAPPER_CONFIG"),
        TEXT("COLOR_QA_EVIDENCE"),
        TEXT("COLOR_QA_RESULT")};
    TSet<FString> Roles;
    const FString ReleaseDirectory = FPaths::GetPath(OutResolvedPath);
    for (int32 Index = 0; Index < Assets->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Asset = nullptr;
        FString AssetId;
        FString Role;
        FString SourceClass;
        FString RelativePath;
        FString Sha256;
        if (!(*Assets)[Index].IsValid() ||
            !(*Assets)[Index]->TryGetObject(Asset) || !Asset || !Asset->IsValid() ||
            !(*Asset)->TryGetStringField(TEXT("assetId"), AssetId) || IsDigitalTwinPlaceholder(AssetId) ||
            !(*Asset)->TryGetStringField(TEXT("role"), Role) || IsDigitalTwinPlaceholder(Role) ||
            !(*Asset)->TryGetStringField(TEXT("sourceClass"), SourceClass) || IsDigitalTwinPlaceholder(SourceClass) ||
            !(*Asset)->TryGetStringField(TEXT("path"), RelativePath) || RelativePath.IsEmpty() ||
            !(*Asset)->TryGetStringField(TEXT("sha256"), Sha256) || !IsLowercaseSha256(Sha256))
        {
            OutError = FString::Printf(TEXT("Asset %d lacks an exact ID/role/source class/safe path/SHA-256 declaration."), Index);
            return false;
        }
        if (Sha256 == RefinedExteriorLod0Sha256 ||
            Sha256 == TEXT("77fbaeaf7e62891301cfe520a55569e31a427f252a5c6b3b2034463ae7697ac4") ||
            Sha256 == TEXT("51ef6841ec7d65ecb14ba71cfc0240dbd7b8f4c99ca79184a9f483deae67a6d0") ||
            Sha256 == TEXT("d8b50c4681fec96e843a6989b6e25eeea7f4c2cce68c03400c6dccb4032fa083") ||
            Sha256 == TEXT("3a83db5c9b9618c3708c838f2e059fa7b8361ed79aeb140395b7b5c6e7058c61"))
        {
            OutError = FString::Printf(
                TEXT("Asset %d matches known rejected generated/reference geometry bytes; renaming cannot grant survey authority."),
                Index);
            return false;
        }
        const FString Combined = AssetId + TEXT("|") + Role + TEXT("|") + SourceClass + TEXT("|") + RelativePath;
        for (const TCHAR* Forbidden : {
                 TEXT("SM_IstanaExterior"),
                 TEXT("SourceAssets/Istana/Generated"),
                 TEXT("BILLBOARD"),
                 TEXT("IMAGE_PLANE"),
                 TEXT("PLACEHOLDER"),
                 TEXT("STREAMED_UNVERIFIED"),
                 TEXT("SRTM_FALLBACK")})
        {
            if (Combined.Contains(Forbidden, ESearchCase::IgnoreCase))
            {
                OutError = FString::Printf(
                    TEXT("Asset %d contains forbidden approximation/final-authority marker '%s'."),
                    Index,
                    Forbidden);
                return false;
            }
        }
        if (!FPaths::IsRelative(RelativePath) || RelativePath.Contains(TEXT("..")))
        {
            OutError = FString::Printf(TEXT("Asset %d path must remain within the immutable release directory."), Index);
            return false;
        }
        FString ResolvedAsset = FPaths::ConvertRelativePathToFull(FPaths::Combine(ReleaseDirectory, RelativePath));
        FPaths::NormalizeFilename(ResolvedAsset);
        if (!FPaths::IsUnderDirectory(ResolvedAsset, ReleaseDirectory) ||
            !IFileManager::Get().FileExists(*ResolvedAsset))
        {
            OutError = FString::Printf(TEXT("Asset %d declared file is absent or outside the release directory."), Index);
            return false;
        }
        Roles.Add(Role);
    }
    for (const FString& RequiredRole : RequiredRoles)
    {
        if (!Roles.Contains(RequiredRole))
        {
            OutError = FString::Printf(TEXT("Digital-twin release is missing required asset role '%s'."), *RequiredRole);
            return false;
        }
    }
    if (ClaimStatus == TEXT("AS_BUILT_AUTHORIZED") && !Roles.Contains(TEXT("BIM_MODEL")))
    {
        OutError = TEXT("AS_BUILT_AUTHORIZED metadata requires an authorized BIM_MODEL asset.");
        return false;
    }

    int32 ControlCount = 0;
    int32 CheckCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : *ControlPoints)
    {
        const TSharedPtr<FJsonObject>* Point = nullptr;
        FString Usage;
        if (Value.IsValid() && Value->TryGetObject(Point) && Point && Point->IsValid() &&
            (*Point)->TryGetStringField(TEXT("usage"), Usage))
        {
            ControlCount += Usage == TEXT("CONTROL") ? 1 : 0;
            CheckCount += Usage == TEXT("CHECK") ? 1 : 0;
        }
    }
    if (ControlCount < 4 || CheckCount < 2)
    {
        OutError = TEXT("Digital-twin metadata requires at least four control points and two independent withheld check points.");
        return false;
    }

    TSet<FString> PhotoViewpoints;
    for (const TSharedPtr<FJsonValue>& Value : *ReferencePhotos)
    {
        const TSharedPtr<FJsonObject>* Photo = nullptr;
        FString Viewpoint;
        if (Value.IsValid() && Value->TryGetObject(Photo) && Photo && Photo->IsValid() &&
            (*Photo)->TryGetStringField(TEXT("viewpoint"), Viewpoint))
        {
            PhotoViewpoints.Add(Viewpoint);
            const TSharedPtr<FJsonObject>* MaterialCalibration = nullptr;
            if (!(*Photo)->TryGetObjectField(TEXT("materialCalibration"), MaterialCalibration) ||
                !MaterialCalibration || !MaterialCalibration->IsValid())
            {
                OutError = TEXT("Every calibrated reference photo requires chart/profile/white-balance/exposure/lighting materialCalibration metadata.");
                return false;
            }
        }
    }
    for (const TCHAR* RequiredViewpoint : {TEXT("FRONT"), TEXT("REAR"), TEXT("EAST"), TEXT("WEST")})
    {
        if (!PhotoViewpoints.Contains(RequiredViewpoint))
        {
            OutError = FString::Printf(TEXT("Calibrated reference-photo coverage is missing '%s'."), RequiredViewpoint);
            return false;
        }
    }

    TSet<FString> ApprovalRoles;
    for (const TSharedPtr<FJsonValue>& Value : *Approvals)
    {
        const TSharedPtr<FJsonObject>* Approval = nullptr;
        FString Role;
        if (Value.IsValid() && Value->TryGetObject(Approval) && Approval && Approval->IsValid() &&
            (*Approval)->TryGetStringField(TEXT("role"), Role))
        {
            ApprovalRoles.Add(Role);
        }
    }
    for (const TCHAR* RequiredApproval : {
             TEXT("SURVEY_AUTHORITY"),
             TEXT("DATA_RIGHTS"),
             TEXT("SECURITY_REVIEW"),
             TEXT("PROJECT_ACCEPTANCE")})
    {
        if (!ApprovalRoles.Contains(RequiredApproval))
        {
            OutError = FString::Printf(TEXT("Digital-twin release is missing '%s' approval."), RequiredApproval);
            return false;
        }
    }
    FString QaStatus;
    FString QaReportSha256;
    if (!(*Qa)->TryGetStringField(TEXT("status"), QaStatus) || QaStatus != TEXT("PASSED") ||
        !(*Qa)->TryGetStringField(TEXT("reportSha256"), QaReportSha256) || !IsLowercaseSha256(QaReportSha256))
    {
        OutError = TEXT("Digital-twin metadata requires PASSED measured QA and a lowercase QA-report SHA-256.");
        return false;
    }
    for (const TCHAR* Field : {
             TEXT("evidenceAssetId"),
             TEXT("resultAssetId"),
             TEXT("sourceAssetId"),
             TEXT("targetAssetId"),
             TEXT("ocioConfigAssetId"),
             TEXT("renderSettingsAssetId"),
             TEXT("lightRigAssetId"),
             TEXT("tonemapperSettingsAssetId")})
    {
        FString Value;
        if (!(*ColorQa)->TryGetStringField(Field, Value) || IsDigitalTwinPlaceholder(Value))
        {
            OutError = FString::Printf(TEXT("Mandatory ColorQA binding '%s' is missing or placeholder."), Field);
            return false;
        }
    }

    struct FDigitalTwinMetricLimit
    {
        const TCHAR* MeasuredField;
        const TCHAR* MaximumField;
        double HardMaximum;
    };
    const FDigitalTwinMetricLimit MetricLimits[] = {
        {TEXT("materialColorDeltaE2000Median"), TEXT("materialColorDeltaE2000MedianMax"), 3.0},
        {TEXT("materialColorDeltaE2000P95"), TEXT("materialColorDeltaE2000P95Max"), 6.0}};
    for (const FDigitalTwinMetricLimit& Metric : MetricLimits)
    {
        double Measured = 0.0;
        double Maximum = 0.0;
        if (!(*Qa)->TryGetNumberField(Metric.MeasuredField, Measured) || !FMath::IsFinite(Measured) || Measured < 0.0 ||
            !(*Criteria)->TryGetNumberField(Metric.MaximumField, Maximum) || !FMath::IsFinite(Maximum) || Maximum <= 0.0 ||
            Measured > Maximum ||
            Measured > Metric.HardMaximum ||
            Maximum > Metric.HardMaximum)
        {
            OutError = FString::Printf(
                TEXT("Material colour QA '%s' must be finite and no greater than approved '%s' or project hard maximum %.3f."),
                Metric.MeasuredField,
                Metric.MaximumField,
                Metric.HardMaximum);
            return false;
        }
    }
    double ContextCoverage = 0.0;
    double ContextCoverageMinimum = 0.0;
    if (!(*Qa)->TryGetNumberField(TEXT("contextCoveragePercent"), ContextCoverage) ||
        !(*Criteria)->TryGetNumberField(TEXT("contextCoverageMinPercent"), ContextCoverageMinimum) ||
        ContextCoverage != 100.0 || ContextCoverageMinimum != 100.0)
    {
        OutError = TEXT("Stage-0 metadata requires exact declared 100% context coverage; semantic coverage remains externally unverified.");
        return false;
    }

    OutError.Reset();
    return true;
}
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaRemoteControlProject(
    const FString& ExpectedProjectPath,
    FString& OutReport)
{
    if (ExpectedProjectPath.IsEmpty())
    {
        OutReport = TEXT("ExpectedProjectPath is required.");
        return false;
    }

    FString ExpectedDirectory = FPaths::ConvertRelativePathToFull(ExpectedProjectPath);
    FPaths::NormalizeDirectoryName(ExpectedDirectory);
    if (ExpectedDirectory.EndsWith(TEXT(".uproject"), ESearchCase::IgnoreCase))
    {
        ExpectedDirectory = FPaths::GetPath(ExpectedDirectory);
        FPaths::NormalizeDirectoryName(ExpectedDirectory);
    }
    FString ActualDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ActualDirectory);
    if (!FPaths::IsSamePath(ExpectedDirectory, ActualDirectory))
    {
        OutReport = FString::Printf(
            TEXT("Remote Control project mismatch: connected editor project is '%s', expected '%s'. No operation was run."),
            *ActualDirectory,
            *ExpectedDirectory);
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Remote Control project identity verified: '%s'."),
        *ActualDirectory);
    return true;
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaDigitalTwinReleaseMetadata(
    const FString& ManifestPath,
    FString& OutReport)
{
    FString ResolvedManifest;
    FString ValidationError;
    if (!ValidateDigitalTwinReleaseMetadataFile(
            ManifestPath,
            ResolvedManifest,
            ValidationError))
    {
        OutReport = TEXT("DIGITAL_TWIN_METADATA_INVALID: ") + ValidationError +
            TEXT(" No asset or map was created or modified.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("DIGITAL_TWIN_METADATA_ONLY_NON_IMPORTABLE: '%s' has the required Stage-0 metadata shape and local file declarations. Unreal did not hash bytes, run ColorQA, semantically inspect geometry/terrain/vegetation, verify receipt signatures, import an asset, open a map, or grant a fidelity claim. Run scripts/Validate-IstanaDigitalTwinRelease.ps1 successfully and retain its exact-release evidence before future reviewed format-specific and trust-root gates may be enabled."),
        *ResolvedManifest);
    return true;
}

bool UTRIADIstanaEditorLibrary::ImportIstanaDigitalTwinRelease(
    const FString& ManifestPath,
    FString& OutMessage)
{
    FString MetadataReport;
    if (!ValidateIstanaDigitalTwinReleaseMetadata(ManifestPath, MetadataReport))
    {
        OutMessage = MetadataReport;
        return false;
    }

    OutMessage = TEXT("DIGITAL_TWIN_IMPORT_LOCKED: metadata inspection passed, but format-specific semantic survey validation, cryptographic receipt verification, and a transaction-safe Unreal importer have not been reviewed. No package was created, saved, overwritten, or modified. The current generated/refined mesh is not a permitted substitute.");
    return false;
}

bool UTRIADIstanaEditorLibrary::BuildIstanaDigitalTwinMapV3(
    const FString& ReleaseId,
    FString& OutMessage)
{
    OutMessage = FString::Printf(
        TEXT("DIGITAL_TWIN_V3_BUILD_LOCKED: release '%s' cannot be built until a strict byte-validated release has been imported by a reviewed format-specific pipeline and its immutable manifest digest is bound to project-owned assets. This stub never opens, duplicates, saves, or modifies /Game/SDTH, /Game/Maps/Istana_1km, or /Game/Maps/Istana_1km_Context_v2."),
        *ReleaseId);
    return false;
}

bool UTRIADIstanaEditorLibrary::BuildIstanaStudyMap(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing destination map '%s'."),
            *DestinationMapPackage);
        return false;
    }

    FString SourceFilename;
    if (!FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename))
    {
        OutMessage = FString::Printf(TEXT("Source map '%s' does not exist."), *SourceMapPackage);
        return false;
    }

    UWorld* PreviousWorld = GEditor->GetEditorWorldContext().World();
    UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(SourceFilename, false);
    if (!World || !World->PersistentLevel || World == PreviousWorld ||
        World->GetOutermost()->GetName() == SourceMapPackage)
    {
        OutMessage = TEXT("Could not load SDTH as an untitled template; source was not saved or modified.");
        return false;
    }
    World->Modify();
    World->PersistentLevel->Modify();

    int32 GeoreferenceCount = 0;
    ACesiumGeoreference* Georeference = RecenterGeoreferences(World, GeoreferenceCount);
    if (!Georeference)
    {
        OutMessage = TEXT("Could not find or create a CesiumGeoreference in the destination template.");
        return false;
    }

    ACesiumCartographicPolygon* Polygon = BuildClipPolygon(World, Georeference);
    if (!Polygon)
    {
        OutMessage = TEXT("Could not create the deterministic 64-point Cesium clip polygon.");
        return false;
    }
    const int32 TilesetCount = ConfigureTilesets(World, Polygon);
    if (TilesetCount == 0)
    {
        OutMessage = TEXT("The SDTH template contains no Cesium 3D Tileset to clip.");
        return false;
    }

    ATRIADIstanaBuildingActor* Building = SpawnNamedActor<ATRIADIstanaBuildingActor>(
        World,
        TEXT("TRIAD_Istana_MainBuilding_PublicExterior"),
        TEXT("TRIAD Istana Main Building - Public Exterior Approximation"));
    ATRIADIstanaStudyAreaActor* StudyArea = SpawnNamedActor<ATRIADIstanaStudyAreaActor>(
        World,
        TEXT("TRIAD_Istana_1km_StudyArea"),
        TEXT("TRIAD Istana 1 km Simulation Study Area"));
    if (!Building || !StudyArea)
    {
        OutMessage = TEXT("Could not spawn the deterministic Istana building/study-area actors.");
        return false;
    }
    Building->ConfigureGeodeticPlacement(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters,
        FootprintYawDegrees,
        Georeference);
    Building->RebuildExterior();
    StudyArea->ConfigureStudyArea(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters,
        StudyRadiusMeters,
        PolygonPointCount,
        Georeference);

    FString PreSaveValidation;
    if (!ValidateWorld(World, false, PreSaveValidation))
    {
        OutMessage = TEXT("Refusing to save an invalid destination template. ") + PreSaveValidation;
        return false;
    }
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapPackage))
    {
        OutMessage = FString::Printf(
            TEXT("Istana world passed validation but could not be saved to '%s'."),
            *DestinationMapPackage);
        return false;
    }

    FString PostSaveValidation;
    if (!ValidateWorld(World, true, PostSaveValidation))
    {
        OutMessage = TEXT("Destination saved but post-save validation failed: ") + PostSaveValidation;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Created %s from an untitled SDTH template. Recentered %d georeference(s), clipped %d tileset(s) to an exact 64-point/1000 m geodesic AOI, and saved no source map package.\n%s"),
        *DestinationMapPackage,
        GeoreferenceCount,
        TilesetCount,
        *PostSaveValidation);
    return true;
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaStudyMap(FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateWorld(World, true, OutReport);
}

bool UTRIADIstanaEditorLibrary::BuildIstanaRuntimeMapV2(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapV2Package))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing destination map '%s'."),
            *DestinationMapV2Package);
        return false;
    }

    UStaticMesh* RequiredExteriorMesh = LoadObject<UStaticMesh>(
        nullptr,
        *IstanaExteriorMeshObjectPath);
    if (RequiredExteriorMesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation(
            {RequiredExteriorMesh});
    }
    FString ExteriorMeshValidationError;
    if (!ValidateIstanaExteriorMeshAsset(
            RequiredExteriorMesh,
            true,
            ExteriorMeshValidationError))
    {
        OutMessage = TEXT("V2 requires the project-owned Istana fidelity asset and will not use the procedural fallback. ") +
            ExteriorMeshValidationError +
            TEXT(" Run ImportIstanaExteriorRefinedLod0 after generating/copying SourceAssets/Istana.");
        return false;
    }

    FString SourceFilename;
    if (!FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename))
    {
        OutMessage = FString::Printf(TEXT("Source map '%s' does not exist."), *SourceMapPackage);
        return false;
    }

    UWorld* PreviousWorld = GEditor->GetEditorWorldContext().World();
    UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(SourceFilename, false);
    if (!World || !World->PersistentLevel || World == PreviousWorld ||
        World->GetOutermost()->GetName() == SourceMapPackage)
    {
        OutMessage = TEXT("Could not load SDTH as an untitled template; source was not saved or modified.");
        return false;
    }
    World->Modify();
    World->PersistentLevel->Modify();

    int32 GeoreferenceCount = 0;
    ACesiumGeoreference* Georeference =
        FindTemplateGeoreferenceWithoutModification(World, GeoreferenceCount);
    if (!Georeference)
    {
        OutMessage = TEXT("SDTH contains no persistent CesiumGeoreference to preserve.");
        return false;
    }
    const FVector PreservedGeoreferenceOriginLongitudeLatitudeHeight =
        Georeference->GetOriginLongitudeLatitudeHeight();

    ACesiumCartographicPolygon* Polygon = BuildClipPolygon(World, Georeference);
    if (!Polygon)
    {
        OutMessage = TEXT("Could not create the deterministic 64-point Cesium clip polygon.");
        return false;
    }
    const int32 TilesetCount = ConfigureTilesets(World, Polygon);
    if (TilesetCount == 0)
    {
        OutMessage = TEXT("The SDTH template contains no Cesium 3D Tileset to preserve inside the AOI.");
        return false;
    }

    ATRIADIstanaExteriorMeshActor* ExteriorMeshActor =
        SpawnNamedActor<ATRIADIstanaExteriorMeshActor>(
        World,
        TEXT("TRIAD_Istana_MainBuilding_ProjectExterior"),
        TEXT("TRIAD Istana Main Building - Project Public Exterior Reconstruction"));
    ATRIADIstanaStudyAreaActor* StudyArea = SpawnNamedActor<ATRIADIstanaStudyAreaActor>(
        World,
        TEXT("TRIAD_Istana_1km_StudyArea"),
        TEXT("TRIAD Istana 1 km Simulation Study Area"));
    if (!ExteriorMeshActor || !StudyArea)
    {
        OutMessage = TEXT("Could not spawn the Istana exterior-mesh/study-area actors.");
        return false;
    }
    if (!ExteriorMeshActor->SetExteriorMesh(RequiredExteriorMesh, ExteriorMeshValidationError))
    {
        OutMessage = TEXT("Could not assign the required Istana exterior mesh: ") +
            ExteriorMeshValidationError;
        return false;
    }
    ExteriorMeshActor->ConfigureGeodeticPlacement(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters,
        ImportedExteriorYawDegrees,
        Georeference);
    StudyArea->ConfigureStudyArea(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters,
        StudyRadiusMeters,
        PolygonPointCount,
        Georeference);

    FString RuntimeConfigurationError;
    if (!ConfigureAirSimRuntimeStart(
            World,
            Georeference,
            PreservedGeoreferenceOriginLongitudeLatitudeHeight,
            RuntimeConfigurationError))
    {
        OutMessage = TEXT("Could not configure the v2 AirSim runtime: ") + RuntimeConfigurationError;
        return false;
    }

    FString PreSaveValidation;
    if (!ValidateRuntimeWorldV2(World, false, PreSaveValidation))
    {
        OutMessage = TEXT("Refusing to save an invalid v2 destination template. ") + PreSaveValidation;
        return false;
    }
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapV2Package))
    {
        OutMessage = FString::Printf(
            TEXT("Istana v2 world passed validation but could not be saved to '%s'."),
            *DestinationMapV2Package);
        return false;
    }
    if (World->GetOutermost()->IsDirty())
    {
        OutMessage = TEXT("Istana v2 save returned success but the destination package remains dirty; refusing reload-based persistence validation.");
        return false;
    }

    FString PersistedValidation;
    if (!ReloadAndValidatePersistedRuntimeMapV2(PersistedValidation))
    {
        OutMessage = TEXT("Destination saved but reload-based v2 persistence validation failed: ") +
            PersistedValidation;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Created %s from an untitled SDTH template. Preserved %d georeference(s), loaded the required project-owned Istana exterior (no HISM fallback), retained the complete interior of an exact 64-point/1000 m Cesium AOI across %d tileset(s), persisted the TRIAD-owned AirSim GameMode wrapper with globe-anchored start/camera actors, reopened the destination from disk, and saved no source/current map package.\n%s"),
        *DestinationMapV2Package,
        GeoreferenceCount,
        TilesetCount,
        *PersistedValidation);
    return true;
}

bool UTRIADIstanaEditorLibrary::RepairIstanaRuntimeMapV2GameMode(
    FString& OutBackupFile,
    FString& OutMessage)
{
    OutBackupFile.Reset();
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World || World->GetOutermost()->GetName() != DestinationMapV2Package)
    {
        OutMessage = FString::Printf(
            TEXT("Repair requires the loaded exact v2 destination '%s'; no other map may be changed."),
            *DestinationMapV2Package);
        return false;
    }

    FString NonGameModeValidation;
    if (!ValidateRuntimeWorldV2(
            World,
            true,
            NonGameModeValidation,
            false))
    {
        OutMessage = TEXT("Refusing narrow GameMode repair because another v2 structural contract failed. ") +
            NonGameModeValidation;
        return false;
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    const UClass* CurrentGameMode = WorldSettings
        ? WorldSettings->DefaultGameMode.Get()
        : nullptr;
    if (CurrentGameMode && CurrentGameMode->GetPathName() == IstanaAirSimGameModeClassPath)
    {
        FString ExistingValidation;
        if (!ValidateRuntimeWorldV2(World, true, ExistingValidation))
        {
            OutMessage = TEXT("GameMode is already correct, but full v2 validation failed: ") +
                ExistingValidation;
            return false;
        }
        OutMessage = TEXT("The loaded v2 destination already has the persisted TRIAD-owned AirSim GameMode Override; no backup, edit, or save was needed.\n") +
            ExistingValidation;
        return true;
    }

    UClass* PreviousGameMode = WorldSettings
        ? WorldSettings->DefaultGameMode.Get()
        : nullptr;
    UPackage* DestinationPackage = World->GetOutermost();
    const bool bDestinationWasDirty = DestinationPackage && DestinationPackage->IsDirty();
    const auto RestoreUnsavedGameModeEdit = [&]() -> bool
    {
        FClassProperty* Property = FindFProperty<FClassProperty>(
            AWorldSettings::StaticClass(),
            GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultGameMode));
        if (!WorldSettings || !Property || !DestinationPackage)
        {
            return false;
        }
        WorldSettings->PreEditChange(Property);
        Property->SetObjectPropertyValue_InContainer(WorldSettings, PreviousGameMode);
        FPropertyChangedEvent RestoreEvent(Property, EPropertyChangeType::ValueSet);
        WorldSettings->PostEditChangeProperty(RestoreEvent);
        if (!bDestinationWasDirty)
        {
            DestinationPackage->SetDirtyFlag(false);
        }
        return WorldSettings->DefaultGameMode.Get() == PreviousGameMode &&
            DestinationPackage->IsDirty() == bDestinationWasDirty;
    };

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapV2Package,
            &DestinationFilename))
    {
        OutMessage = TEXT("The exact v2 destination file disappeared before its repair backup could be created.");
        return false;
    }
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD"),
        TEXT("IstanaMapBackups")));
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutMessage = FString::Printf(
            TEXT("Could not create the v2 repair-backup directory '%s'."),
            *BackupDirectory);
        return false;
    }
    const FString BackupLeaf = FString::Printf(
        TEXT("Istana_1km_Context_v2.pre_gamemode_repair.%s.%s%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits),
        *FPackageName::GetMapPackageExtension());
    const FString BackupFilename = FPaths::Combine(BackupDirectory, BackupLeaf);
    if (IFileManager::Get().FileExists(*BackupFilename) ||
        IFileManager::Get().Copy(
            *BackupFilename,
            *DestinationFilename,
            false,
            true) != COPY_OK)
    {
        OutMessage = FString::Printf(
            TEXT("Could not create the non-overwriting v2 map-package backup '%s'; repair was not saved."),
            *BackupFilename);
        return false;
    }
    OutBackupFile = BackupFilename;

    FString SetError;
    if (!SetPersistentAirSimGameModeOverride(World, SetError))
    {
        const bool bRestored = RestoreUnsavedGameModeEdit();
        OutMessage = FString::Printf(
            TEXT("Could not apply the persistent TRIAD-owned AirSim GameMode Override; unsaved state restoration %s. The untouched pre-repair map-package backup remains at '%s'. %s"),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile,
            *SetError);
        return false;
    }

    FString PreSaveValidation;
    if (!ValidateRuntimeWorldV2(World, true, PreSaveValidation))
    {
        const bool bRestored = RestoreUnsavedGameModeEdit();
        OutMessage = FString::Printf(
            TEXT("Refusing to save the repaired v2 map because full validation failed; unsaved state restoration %s. The untouched pre-repair map-package backup remains at '%s'. %s"),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile,
            *PreSaveValidation);
        return false;
    }

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapV2Package))
    {
        const bool bRestored = RestoreUnsavedGameModeEdit();
        OutMessage = FString::Printf(
            TEXT("Could not save the exact repaired v2 destination; unsaved state restoration %s. The pre-repair map-package backup remains at '%s'."),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile);
        return false;
    }
    if (World->GetOutermost()->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("The exact v2 save returned success but its package is still dirty, so the tool refused to reload it. Treat disk state as committed and inspect manually; pre-repair backup: '%s'."),
            *OutBackupFile);
        return false;
    }

    FString PersistedValidation;
    if (!ReloadAndValidatePersistedRuntimeMapV2(PersistedValidation))
    {
        OutMessage = FString::Printf(
            TEXT("The repaired destination was saved but failed reload-based persistence validation. The pre-repair map-package backup remains at '%s'. %s"),
            *OutBackupFile,
            *PersistedValidation);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Repaired only the serialized TRIAD-owned AirSim GameMode Override in '%s', saved and reopened that exact destination, and left SDTH/v1 untouched. Pre-repair map-package backup: '%s'.\n%s"),
        *DestinationMapV2Package,
        *OutBackupFile,
        *PersistedValidation);
    return true;
}

bool UTRIADIstanaEditorLibrary::RepairIstanaRuntimeMapV2Camera(
    FString& OutBackupFile,
    FString& OutMessage)
{
    OutBackupFile.Reset();
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World || World->GetOutermost()->GetName() != DestinationMapV2Package)
    {
        OutMessage = FString::Printf(
            TEXT("Camera migration requires the loaded exact v2 destination '%s'; no other map may be changed."),
            *DestinationMapV2Package);
        return false;
    }

    // The legacy pose/FOV and imported-front heading are expected to fail the
    // current full validator. Defer only those two migration targets; every
    // other v2 invariant is still required before the backup or first edit.
    FString NonMigrationValidation;
    if (!ValidateRuntimeWorldV2(
            World,
            true,
            NonMigrationValidation,
            true,
            false,
            false))
    {
        OutMessage = TEXT("Refusing narrow camera/orientation migration because another v2 structural contract failed. ") +
            NonMigrationValidation;
        return false;
    }

    TArray<ATRIADIstanaRuntimePolicyActor*> RuntimePolicies;
    for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimePolicyActorTag))
        {
            RuntimePolicies.Add(*It);
        }
    }
    TArray<ACameraActor*> RuntimeCameras;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimeCameraActorTag))
        {
            RuntimeCameras.Add(*It);
        }
    }
    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
    {
        ExteriorActors.Add(*It);
    }
    ATRIADIstanaRuntimePolicyActor* RuntimePolicy =
        RuntimePolicies.Num() == 1 ? RuntimePolicies[0] : nullptr;
    ACameraActor* RuntimeCamera =
        RuntimeCameras.Num() == 1 ? RuntimeCameras[0] : nullptr;
    ATRIADIstanaExteriorMeshActor* Exterior =
        ExteriorActors.Num() == 1 ? ExteriorActors[0] : nullptr;
    UCesiumGlobeAnchorComponent* CameraAnchor = RuntimeCamera
        ? RuntimeCamera->FindComponentByClass<UCesiumGlobeAnchorComponent>()
        : nullptr;
    UCameraComponent* CameraComponent = RuntimeCamera
        ? RuntimeCamera->GetCameraComponent()
        : nullptr;
    UCesiumGlobeAnchorComponent* ExteriorAnchor = Exterior
        ? Exterior->GlobeAnchor.Get()
        : nullptr;
    UStaticMeshComponent* ExteriorMeshComponent = Exterior
        ? Exterior->ExteriorMeshComponent.Get()
        : nullptr;
    FByteProperty* AutoActivateProperty = FindFProperty<FByteProperty>(
        ACameraActor::StaticClass(),
        TEXT("AutoActivateForPlayer"));
    UPackage* DestinationPackage = World->GetOutermost();
    int32 GeoreferenceCount = 0;
    ACesiumGeoreference* Georeference =
        FindTemplateGeoreferenceWithoutModification(World, GeoreferenceCount);
    if (!RuntimePolicy || !RuntimeCamera || !Exterior ||
        RuntimeCamera->GetLevel() != World->PersistentLevel ||
        Exterior->GetLevel() != World->PersistentLevel ||
        !CameraAnchor || CameraAnchor->GetOwner() != RuntimeCamera ||
        !CameraComponent || CameraComponent->GetOwner() != RuntimeCamera ||
        !ExteriorAnchor || ExteriorAnchor->GetOwner() != Exterior ||
        !ExteriorMeshComponent || ExteriorMeshComponent->GetOwner() != Exterior ||
        !AutoActivateProperty || !Georeference || !DestinationPackage)
    {
        OutMessage = FString::Printf(
            TEXT("Camera/orientation migration requires exactly one tagged persistent camera, one imported exterior, their owned anchor/components, one tagged runtime policy, and a preserved georeference (policy=%d, camera=%d, exterior=%d, georeference=%d)."),
            RuntimePolicies.Num(),
            RuntimeCameras.Num(),
            ExteriorActors.Num(),
            GeoreferenceCount);
        return false;
    }
    if (RuntimeCamera->GetPackage() != DestinationPackage ||
        CameraAnchor->GetPackage() != DestinationPackage ||
        CameraComponent->GetPackage() != DestinationPackage ||
        Exterior->GetPackage() != DestinationPackage ||
        ExteriorAnchor->GetPackage() != DestinationPackage ||
        ExteriorMeshComponent->GetPackage() != DestinationPackage)
    {
        OutMessage = TEXT("Camera/orientation migration refuses external/package-split camera or exterior objects because the single-map backup would not cover every mutation.");
        return false;
    }

    const double GroundHeightMeters = RuntimePolicy->ExteriorGroundHeightMeters;
    const FVector PreviousLongitudeLatitudeHeight =
        CameraAnchor->GetLongitudeLatitudeHeight();
    const FRotator PreviousRotation = RuntimeCamera->GetActorRotation();
    const float PreviousFieldOfView = CameraComponent->FieldOfView;
    const uint8 PreviousAutoActivateValue =
        AutoActivateProperty->GetPropertyValue_InContainer(RuntimeCamera);
    const double PreviousExteriorLongitudeDegrees = Exterior->LongitudeDegrees;
    const double PreviousExteriorLatitudeDegrees = Exterior->LatitudeDegrees;
    const double PreviousExteriorHeightMeters = Exterior->HeightMeters;
    const double PreviousExteriorYawDegrees = Exterior->FootprintYawDegrees;
    if (!FMath::IsFinite(GroundHeightMeters) ||
        PreviousLongitudeLatitudeHeight.ContainsNaN() ||
        PreviousRotation.ContainsNaN() ||
        !FMath::IsFinite(PreviousFieldOfView) ||
        !FMath::IsFinite(PreviousExteriorLongitudeDegrees) ||
        !FMath::IsFinite(PreviousExteriorLatitudeDegrees) ||
        !FMath::IsFinite(PreviousExteriorHeightMeters) ||
        !FMath::IsFinite(PreviousExteriorYawDegrees))
    {
        OutMessage = TEXT("Camera/orientation migration refused non-finite policy, camera, or exterior state because it could not be restored safely after a failed edit.");
        return false;
    }

    const FVector TargetLongitudeLatitudeHeight =
        MakeSouthOffsetLongitudeLatitudeHeight(
            RuntimeCameraSouthOffsetMeters,
            GroundHeightMeters + RuntimeCameraHeightAboveGroundMeters);
    const FVector TargetWorld =
        Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
            CenterLongitudeDegrees,
            CenterLatitudeDegrees,
            GroundHeightMeters + RuntimeCameraAimAboveGroundMeters));
    const FVector CurrentAimDirection =
        (TargetWorld - RuntimeCamera->GetActorLocation()).GetSafeNormal();
    const bool bCameraAlreadyCurrent =
        RuntimeCamera->GetAutoActivatePlayerIndex() == 0 &&
        IsSameLongitudeLatitudeHeight(
            PreviousLongitudeLatitudeHeight,
            TargetLongitudeLatitudeHeight) &&
        FMath::IsNearlyEqual(
            PreviousFieldOfView,
            RuntimeCameraFieldOfViewDegrees,
            0.01f) &&
        !CurrentAimDirection.IsNearlyZero() &&
        FVector::DotProduct(
            RuntimeCamera->GetActorForwardVector(),
            CurrentAimDirection) >= 0.999;
    double CurrentExteriorFrontAlignmentDot = -1.0;
    const bool bExteriorAlreadyCurrent =
        FMath::IsNearlyEqual(
            Exterior->FootprintYawDegrees,
            ImportedExteriorYawDegrees,
            0.001) &&
        IsImportedExteriorCeremonialFrontFacingSouth(
            Exterior,
            Georeference,
            GroundHeightMeters,
            CurrentExteriorFrontAlignmentDot);
    if (bCameraAlreadyCurrent && bExteriorAlreadyCurrent)
    {
        FString ExistingValidation;
        if (!ValidateRuntimeWorldV2(World, true, ExistingValidation))
        {
            OutMessage = TEXT("The tagged camera already matches the current facade-view contract, but full v2 validation failed: ") +
                ExistingValidation;
            return false;
        }
        OutMessage = TEXT("The tagged v2 runtime camera and imported fallback already match the 210 m / 24 m / 13 m / 52 degree facade-view and 182.3 degree ceremonial-front contracts; no backup, edit, or save was needed.\n") +
            ExistingValidation;
        return true;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapV2Package,
            &DestinationFilename))
    {
        OutMessage = TEXT("The exact v2 destination file disappeared before its camera-migration backup could be created.");
        return false;
    }
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD"),
        TEXT("IstanaMapBackups")));
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutMessage = FString::Printf(
            TEXT("Could not create the v2 camera-migration backup directory '%s'."),
            *BackupDirectory);
        return false;
    }
    const FString BackupLeaf = FString::Printf(
        TEXT("Istana_1km_Context_v2.pre_camera_repair.%s.%s%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits),
        *FPackageName::GetMapPackageExtension());
    const FString BackupFilename = FPaths::Combine(BackupDirectory, BackupLeaf);
    if (IFileManager::Get().FileExists(*BackupFilename) ||
        IFileManager::Get().Copy(
            *BackupFilename,
            *DestinationFilename,
            false,
            true) != COPY_OK)
    {
        OutMessage = FString::Printf(
            TEXT("Could not create the non-overwriting v2 camera-migration backup '%s'; the map was not edited."),
            *BackupFilename);
        return false;
    }
    OutBackupFile = BackupFilename;

    const bool bDestinationWasDirty =
        DestinationPackage && DestinationPackage->IsDirty();
    const auto RestoreUnsavedCameraAndOrientationEdit = [&]() -> bool
    {
        if (!RuntimeCamera || !CameraAnchor || !CameraComponent ||
            !Exterior || !ExteriorAnchor || !AutoActivateProperty ||
            !DestinationPackage)
        {
            return false;
        }
        RuntimeCamera->Modify();
        CameraAnchor->Modify();
        CameraComponent->Modify();
        Exterior->Modify();
        ExteriorAnchor->Modify();
        AutoActivateProperty->SetPropertyValue_InContainer(
            RuntimeCamera,
            PreviousAutoActivateValue);
        CameraComponent->SetFieldOfView(PreviousFieldOfView);
        CameraAnchor->MoveToLongitudeLatitudeHeight(
            PreviousLongitudeLatitudeHeight);
        RuntimeCamera->SetActorRotation(PreviousRotation);
        Exterior->ConfigureGeodeticPlacement(
            PreviousExteriorLongitudeDegrees,
            PreviousExteriorLatitudeDegrees,
            PreviousExteriorHeightMeters,
            PreviousExteriorYawDegrees,
            Georeference);
        if (!bDestinationWasDirty)
        {
            DestinationPackage->SetDirtyFlag(false);
        }
        return AutoActivateProperty->GetPropertyValue_InContainer(RuntimeCamera) ==
                PreviousAutoActivateValue &&
            FMath::IsNearlyEqual(
                CameraComponent->FieldOfView,
                PreviousFieldOfView,
                0.001f) &&
            IsSameLongitudeLatitudeHeight(
                CameraAnchor->GetLongitudeLatitudeHeight(),
                PreviousLongitudeLatitudeHeight,
                0.001,
                0.001) &&
            RuntimeCamera->GetActorRotation().Equals(PreviousRotation, 0.001f) &&
            FMath::IsNearlyEqual(
                Exterior->FootprintYawDegrees,
                PreviousExteriorYawDegrees,
                0.001) &&
            DestinationPackage->IsDirty() == bDestinationWasDirty;
    };

    DestinationPackage->Modify();
    RuntimeCamera->Modify();
    CameraAnchor->Modify();
    CameraComponent->Modify();
    Exterior->Modify();
    ExteriorAnchor->Modify();
    FString CameraEditError;
    if (!SetCameraAutoActivateForPlayer(
            RuntimeCamera,
            EAutoReceiveInput::Player0,
            CameraEditError))
    {
        const bool bRestored = RestoreUnsavedCameraAndOrientationEdit();
        OutMessage = FString::Printf(
            TEXT("Could not set the migrated Player 0 camera; unsaved state restoration %s. Backup: '%s'. %s"),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile,
            *CameraEditError);
        return false;
    }
    CameraComponent->SetFieldOfView(RuntimeCameraFieldOfViewDegrees);
    CameraAnchor->MoveToLongitudeLatitudeHeight(TargetLongitudeLatitudeHeight);
    RuntimeCamera->SetActorRotation(
        (TargetWorld - RuntimeCamera->GetActorLocation()).Rotation());
    Exterior->ConfigureGeodeticPlacement(
        PreviousExteriorLongitudeDegrees,
        PreviousExteriorLatitudeDegrees,
        PreviousExteriorHeightMeters,
        ImportedExteriorYawDegrees,
        Georeference);
    CameraComponent->MarkPackageDirty();
    CameraAnchor->MarkPackageDirty();
    RuntimeCamera->MarkPackageDirty();
    ExteriorAnchor->MarkPackageDirty();
    Exterior->MarkPackageDirty();
    World->PersistentLevel->MarkPackageDirty();
    World->MarkPackageDirty();
    if (!DestinationPackage->IsDirty())
    {
        const bool bRestored = RestoreUnsavedCameraAndOrientationEdit();
        OutMessage = FString::Printf(
            TEXT("UE suppressed map-package dirtiness for the camera/orientation migration; unsaved state restoration %s. Backup: '%s'."),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile);
        return false;
    }

    FString PreSaveValidation;
    if (!ValidateRuntimeWorldV2(World, true, PreSaveValidation))
    {
        const bool bRestored = RestoreUnsavedCameraAndOrientationEdit();
        OutMessage = FString::Printf(
            TEXT("Refusing to save the migrated camera/orientation because full v2 validation failed; unsaved state restoration %s. Backup: '%s'. %s"),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile,
            *PreSaveValidation);
        return false;
    }

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapV2Package))
    {
        const bool bRestored = RestoreUnsavedCameraAndOrientationEdit();
        OutMessage = FString::Printf(
            TEXT("Could not save the exact v2 destination after camera/orientation migration; unsaved state restoration %s. Backup: '%s'."),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile);
        return false;
    }
    if (World->GetOutermost()->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("The exact v2 save returned success but its package remains dirty, so reload was refused. Treat disk state as committed and inspect manually; backup: '%s'."),
            *OutBackupFile);
        return false;
    }

    FString PersistedValidation;
    if (!ReloadAndValidatePersistedRuntimeMapV2(PersistedValidation))
    {
        OutMessage = FString::Printf(
            TEXT("The camera/orientation-migrated destination was saved but failed full reload validation. Backup: '%s'. %s"),
            *OutBackupFile,
            *PersistedValidation);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Migrated the tagged v2 runtime camera to 210 m south / 24 m AGL / 13 m aim / 52 degree FOV and corrected the imported fallback ceremonial-front ESU yaw to 182.3 degrees using persisted ground height %.3f m, saved and reopened only '%s', and left SDTH/v1 untouched. Pre-migration backup: '%s'.\n%s"),
        GroundHeightMeters,
        *DestinationMapV2Package,
        *OutBackupFile,
        *PersistedValidation);
    return true;
}

bool UTRIADIstanaEditorLibrary::RepairIstanaRuntimeMapV2ExteriorAsset(
    FString& OutBackupFile,
    FString& OutMessage)
{
    OutBackupFile.Reset();
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World || World->GetOutermost()->GetName() != DestinationMapV2Package)
    {
        OutMessage = FString::Printf(
            TEXT("Refined-exterior migration requires the loaded exact v2 destination '%s'; no other map may be changed."),
            *DestinationMapV2Package);
        return false;
    }

    // Defer only the exact exterior asset path. Camera, heading, georeference,
    // clip, policy, context, collision, and every other v2 invariant remain
    // mandatory before the backup or first edit.
    FString NonExteriorAssetValidation;
    if (!ValidateRuntimeWorldV2(
            World,
            true,
            NonExteriorAssetValidation,
            true,
            true,
            true,
            false))
    {
        OutMessage = TEXT("Refusing narrow refined-exterior migration because another v2 structural contract failed. ") +
            NonExteriorAssetValidation;
        return false;
    }

    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
    {
        ExteriorActors.Add(*It);
    }
    ATRIADIstanaExteriorMeshActor* Exterior =
        ExteriorActors.Num() == 1 ? ExteriorActors[0] : nullptr;
    UStaticMeshComponent* ExteriorMeshComponent = Exterior
        ? Exterior->ExteriorMeshComponent.Get()
        : nullptr;
    UCesiumGlobeAnchorComponent* ExteriorAnchor = Exterior
        ? Exterior->GlobeAnchor.Get()
        : nullptr;
    UPackage* DestinationPackage = World->GetOutermost();
    if (!Exterior || Exterior->GetLevel() != World->PersistentLevel ||
        !ExteriorMeshComponent || ExteriorMeshComponent->GetOwner() != Exterior ||
        !ExteriorAnchor || ExteriorAnchor->GetOwner() != Exterior ||
        !DestinationPackage ||
        Exterior->GetPackage() != DestinationPackage ||
        ExteriorAnchor->GetPackage() != DestinationPackage ||
        ExteriorMeshComponent->GetPackage() != DestinationPackage)
    {
        OutMessage = FString::Printf(
            TEXT("Refined-exterior migration requires exactly one persistent package-local imported exterior actor/component; found %d actor(s)."),
            ExteriorActors.Num());
        return false;
    }

    UStaticMesh* RefinedMesh = LoadObject<UStaticMesh>(
        nullptr,
        *IstanaExteriorMeshObjectPath);
    if (RefinedMesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({RefinedMesh});
    }
    FString RefinedValidationError;
    if (!ValidateIstanaExteriorMeshAsset(
            RefinedMesh,
            true,
            RefinedValidationError))
    {
        OutMessage = TEXT("Required separately imported refined exterior asset is unavailable or invalid. Run ImportIstanaExteriorRefinedLod0 first. ") +
            RefinedValidationError;
        return false;
    }

    UStaticMesh* PreviousMesh = ExteriorMeshComponent->GetStaticMesh();
    const TSoftObjectPtr<UStaticMesh> PreviousRequiredAsset =
        Exterior->RequiredExteriorMeshAsset;
    const bool bPreviousRequiredMeshLoaded = Exterior->bRequiredExteriorMeshLoaded;
    const FTransform PreviousRelativeTransform =
        ExteriorMeshComponent->GetRelativeTransform();
    const ECollisionEnabled::Type PreviousCollisionEnabled =
        ExteriorMeshComponent->GetCollisionEnabled();
    const FName PreviousCollisionProfile =
        ExteriorMeshComponent->GetCollisionProfileName();
    const ECollisionChannel PreviousCollisionObjectType =
        ExteriorMeshComponent->GetCollisionObjectType();
    const FCollisionResponseContainer PreviousCollisionResponses =
        ExteriorMeshComponent->GetCollisionResponseToChannels();
    const bool bPreviousGenerateOverlapEvents =
        ExteriorMeshComponent->GetGenerateOverlapEvents();
    const bool bPreviousVisible = ExteriorMeshComponent->IsVisible();
    const bool bPreviousHiddenInGame = ExteriorMeshComponent->bHiddenInGame;
    const FTransform PreviousActorTransform = Exterior->GetActorTransform();
    const FVector PreviousAnchorLongitudeLatitudeHeight =
        ExteriorAnchor->GetLongitudeLatitudeHeight();
    const FQuat PreviousAnchorEastSouthUpRotation =
        ExteriorAnchor->GetEastSouthUpRotation();
    const double PreviousLongitudeDegrees = Exterior->LongitudeDegrees;
    const double PreviousLatitudeDegrees = Exterior->LatitudeDegrees;
    const double PreviousHeightMeters = Exterior->HeightMeters;
    const double PreviousYawDegrees = Exterior->FootprintYawDegrees;
    const FString PreviousHardObjectPath = PreviousMesh
        ? PreviousMesh->GetPathName()
        : FString();
    const FString PreviousSoftObjectPath =
        PreviousRequiredAsset.ToSoftObjectPath().ToString();
    const bool bExactSupportedPair =
        PreviousSoftObjectPath == PreviousHardObjectPath;
    const bool bLegacyHardWithRefinedSoftDefault =
        PreviousHardObjectPath == LegacyIstanaExteriorMeshObjectPath &&
        PreviousSoftObjectPath == IstanaExteriorMeshObjectPath;
    if (!PreviousMesh ||
        (PreviousHardObjectPath != LegacyIstanaExteriorMeshObjectPath &&
         PreviousHardObjectPath != IstanaExteriorMeshObjectPath) ||
        (!bExactSupportedPair && !bLegacyHardWithRefinedSoftDefault) ||
        PreviousCollisionEnabled != ECollisionEnabled::QueryAndPhysics ||
        PreviousActorTransform.ContainsNaN() ||
        PreviousAnchorLongitudeLatitudeHeight.ContainsNaN() ||
        PreviousAnchorEastSouthUpRotation.ContainsNaN() ||
        !FMath::IsFinite(PreviousLongitudeDegrees) ||
        !FMath::IsFinite(PreviousLatitudeDegrees) ||
        !FMath::IsFinite(PreviousHeightMeters) ||
        !FMath::IsFinite(PreviousYawDegrees))
    {
        OutMessage = TEXT("Refined-exterior migration refused unsupported or non-restorable current mesh/soft-reference/collision state.");
        return false;
    }

    if (PreviousMesh == RefinedMesh &&
        PreviousMesh->GetPathName() == IstanaExteriorMeshObjectPath)
    {
        FString ExistingValidation;
        if (!ValidateRuntimeWorldV2(World, true, ExistingValidation))
        {
            OutMessage = TEXT("The refined exterior is already assigned, but full v2 validation failed: ") +
                ExistingValidation;
            return false;
        }
        OutMessage = TEXT("The exact v2 exterior already uses the validated SM_IstanaExterior_Refined asset with collision and ceremonial-front heading intact; no backup, edit, or save was needed.\n") +
            ExistingValidation;
        return true;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapV2Package,
            &DestinationFilename))
    {
        OutMessage = TEXT("The exact v2 destination file disappeared before its refined-exterior backup could be created.");
        return false;
    }
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD"),
        TEXT("IstanaMapBackups")));
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutMessage = FString::Printf(
            TEXT("Could not create the refined-exterior backup directory '%s'."),
            *BackupDirectory);
        return false;
    }
    const FString BackupLeaf = FString::Printf(
        TEXT("Istana_1km_Context_v2.pre_refined_exterior_repair.%s.%s%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits),
        *FPackageName::GetMapPackageExtension());
    const FString BackupFilename = FPaths::Combine(BackupDirectory, BackupLeaf);
    if (IFileManager::Get().FileExists(*BackupFilename) ||
        IFileManager::Get().Copy(
            *BackupFilename,
            *DestinationFilename,
            false,
            true) != COPY_OK)
    {
        OutMessage = FString::Printf(
            TEXT("Could not create the non-overwriting refined-exterior backup '%s'; the map was not edited."),
            *BackupFilename);
        return false;
    }
    OutBackupFile = BackupFilename;

    const bool bDestinationWasDirty = DestinationPackage->IsDirty();
    const auto RestoreUnsavedExteriorAssetEdit = [&]() -> bool
    {
        if (!Exterior || !ExteriorMeshComponent || !PreviousMesh ||
            !DestinationPackage)
        {
            return false;
        }
        Exterior->Modify();
        ExteriorMeshComponent->Modify();
        ExteriorMeshComponent->SetStaticMesh(PreviousMesh);
        ExteriorMeshComponent->SetRelativeTransform(PreviousRelativeTransform);
        ExteriorMeshComponent->SetCollisionEnabled(PreviousCollisionEnabled);
        ExteriorMeshComponent->SetCollisionProfileName(PreviousCollisionProfile);
        ExteriorMeshComponent->SetCollisionObjectType(PreviousCollisionObjectType);
        ExteriorMeshComponent->SetCollisionResponseToChannels(PreviousCollisionResponses);
        ExteriorMeshComponent->SetGenerateOverlapEvents(
            bPreviousGenerateOverlapEvents);
        ExteriorMeshComponent->SetVisibility(bPreviousVisible, false);
        ExteriorMeshComponent->SetHiddenInGame(bPreviousHiddenInGame, false);
        Exterior->RequiredExteriorMeshAsset = PreviousRequiredAsset;
        Exterior->bRequiredExteriorMeshLoaded = bPreviousRequiredMeshLoaded;
        if (!bDestinationWasDirty)
        {
            DestinationPackage->SetDirtyFlag(false);
        }
        return ExteriorMeshComponent->GetStaticMesh() == PreviousMesh &&
            ExteriorMeshComponent->GetRelativeTransform().Equals(
                PreviousRelativeTransform,
                0.001) &&
            ExteriorMeshComponent->GetCollisionEnabled() ==
                PreviousCollisionEnabled &&
            ExteriorMeshComponent->GetCollisionProfileName() ==
                PreviousCollisionProfile &&
            ExteriorMeshComponent->GetCollisionObjectType() ==
                PreviousCollisionObjectType &&
            ExteriorMeshComponent->GetCollisionResponseToChannels() ==
                PreviousCollisionResponses &&
            ExteriorMeshComponent->GetGenerateOverlapEvents() ==
                bPreviousGenerateOverlapEvents &&
            ExteriorMeshComponent->IsVisible() == bPreviousVisible &&
            static_cast<bool>(ExteriorMeshComponent->bHiddenInGame) ==
                bPreviousHiddenInGame &&
            Exterior->RequiredExteriorMeshAsset.ToSoftObjectPath() ==
                PreviousRequiredAsset.ToSoftObjectPath() &&
            Exterior->bRequiredExteriorMeshLoaded ==
                bPreviousRequiredMeshLoaded &&
            DestinationPackage->IsDirty() == bDestinationWasDirty;
    };

    DestinationPackage->Modify();
    Exterior->Modify();
    ExteriorMeshComponent->Modify();
    FString AssignmentError;
    if (!Exterior->SetExteriorMesh(RefinedMesh, AssignmentError) ||
        ExteriorMeshComponent->GetStaticMesh() != RefinedMesh ||
        Exterior->RequiredExteriorMeshAsset.ToSoftObjectPath().ToString() !=
            IstanaExteriorMeshObjectPath ||
        !Exterior->bRequiredExteriorMeshLoaded ||
        ExteriorMeshComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        ExteriorMeshComponent->GetCollisionProfileName() !=
            PreviousCollisionProfile ||
        ExteriorMeshComponent->GetCollisionObjectType() !=
            PreviousCollisionObjectType ||
        ExteriorMeshComponent->GetCollisionResponseToChannels() !=
            PreviousCollisionResponses ||
        ExteriorMeshComponent->GetGenerateOverlapEvents() !=
            bPreviousGenerateOverlapEvents ||
        ExteriorMeshComponent->IsVisible() != bPreviousVisible ||
        static_cast<bool>(ExteriorMeshComponent->bHiddenInGame) !=
            bPreviousHiddenInGame ||
        !Exterior->GetActorTransform().Equals(PreviousActorTransform, 0.001) ||
        !IsSameLongitudeLatitudeHeight(
            ExteriorAnchor->GetLongitudeLatitudeHeight(),
            PreviousAnchorLongitudeLatitudeHeight,
            0.001,
            0.001) ||
        !ExteriorAnchor->GetEastSouthUpRotation().Equals(
            PreviousAnchorEastSouthUpRotation,
            0.000001) ||
        !FMath::IsNearlyEqual(Exterior->LongitudeDegrees, PreviousLongitudeDegrees, 0.0000001) ||
        !FMath::IsNearlyEqual(Exterior->LatitudeDegrees, PreviousLatitudeDegrees, 0.0000001) ||
        !FMath::IsNearlyEqual(Exterior->HeightMeters, PreviousHeightMeters, 0.001) ||
        !FMath::IsNearlyEqual(Exterior->FootprintYawDegrees, PreviousYawDegrees, 0.001))
    {
        const bool bRestored = RestoreUnsavedExteriorAssetEdit();
        OutMessage = FString::Printf(
            TEXT("Could not assign the refined exterior with collision intact; unsaved state restoration %s. Backup: '%s'. %s"),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile,
            *AssignmentError);
        return false;
    }

    ExteriorMeshComponent->MarkPackageDirty();
    Exterior->MarkPackageDirty();
    World->PersistentLevel->MarkPackageDirty();
    World->MarkPackageDirty();
    if (!DestinationPackage->IsDirty())
    {
        const bool bRestored = RestoreUnsavedExteriorAssetEdit();
        OutMessage = FString::Printf(
            TEXT("UE suppressed map-package dirtiness for refined-exterior migration; unsaved state restoration %s. Backup: '%s'."),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile);
        return false;
    }

    FString PreSaveValidation;
    if (!ValidateRuntimeWorldV2(World, true, PreSaveValidation))
    {
        const bool bRestored = RestoreUnsavedExteriorAssetEdit();
        OutMessage = FString::Printf(
            TEXT("Refusing to save the refined-exterior migration because full v2 validation failed; unsaved state restoration %s. Backup: '%s'. %s"),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile,
            *PreSaveValidation);
        return false;
    }

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapV2Package))
    {
        const bool bRestored = RestoreUnsavedExteriorAssetEdit();
        OutMessage = FString::Printf(
            TEXT("Could not save exact v2 after refined-exterior migration; unsaved state restoration %s. Backup: '%s'."),
            bRestored ? TEXT("succeeded") : TEXT("FAILED; close without saving"),
            *OutBackupFile);
        return false;
    }
    if (World->GetOutermost()->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("The refined-exterior save returned success but exact v2 remains dirty, so reload was refused. Treat disk state as committed and inspect manually; backup: '%s'."),
            *OutBackupFile);
        return false;
    }

    FString PersistedValidation;
    if (!ReloadAndValidatePersistedRuntimeMapV2(PersistedValidation))
    {
        OutMessage = FString::Printf(
            TEXT("The refined-exterior destination was saved but failed full disk-reload validation. Backup: '%s'. %s"),
            *OutBackupFile,
            *PersistedValidation);
        return false;
    }

    // ReloadAndValidatePersistedRuntimeMapV2 deliberately invalidates every
    // map-owned pointer above. Reacquire the actor/component and additionally
    // prove that the narrow migration preserved the exact pre-save collision,
    // visibility, relative-transform, and geodetic-placement state on disk.
    UWorld* ReloadedWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    TArray<ATRIADIstanaExteriorMeshActor*> ReloadedExteriorActors;
    if (ReloadedWorld)
    {
        for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(ReloadedWorld); It; ++It)
        {
            ReloadedExteriorActors.Add(*It);
        }
    }
    ATRIADIstanaExteriorMeshActor* ReloadedExterior =
        ReloadedExteriorActors.Num() == 1 ? ReloadedExteriorActors[0] : nullptr;
    UStaticMeshComponent* ReloadedMeshComponent = ReloadedExterior
        ? ReloadedExterior->ExteriorMeshComponent.Get()
        : nullptr;
    UCesiumGlobeAnchorComponent* ReloadedAnchor = ReloadedExterior
        ? ReloadedExterior->GlobeAnchor.Get()
        : nullptr;
    if (!ReloadedWorld ||
        ReloadedWorld->GetOutermost()->GetName() != DestinationMapV2Package ||
        !ReloadedExterior || !ReloadedMeshComponent || !ReloadedAnchor ||
        !ReloadedMeshComponent->GetStaticMesh() ||
        ReloadedMeshComponent->GetStaticMesh()->GetPathName() !=
            IstanaExteriorMeshObjectPath ||
        ReloadedExterior->RequiredExteriorMeshAsset.ToSoftObjectPath().ToString() !=
            IstanaExteriorMeshObjectPath ||
        !ReloadedMeshComponent->GetRelativeTransform().Equals(
            PreviousRelativeTransform,
            0.001) ||
        ReloadedMeshComponent->GetCollisionEnabled() != PreviousCollisionEnabled ||
        ReloadedMeshComponent->GetCollisionProfileName() != PreviousCollisionProfile ||
        ReloadedMeshComponent->GetCollisionObjectType() != PreviousCollisionObjectType ||
        ReloadedMeshComponent->GetCollisionResponseToChannels() !=
            PreviousCollisionResponses ||
        ReloadedMeshComponent->GetGenerateOverlapEvents() !=
            bPreviousGenerateOverlapEvents ||
        ReloadedMeshComponent->IsVisible() != bPreviousVisible ||
        static_cast<bool>(ReloadedMeshComponent->bHiddenInGame) !=
            bPreviousHiddenInGame ||
        !IsSameLongitudeLatitudeHeight(
            ReloadedAnchor->GetLongitudeLatitudeHeight(),
            PreviousAnchorLongitudeLatitudeHeight,
            0.001,
            0.001) ||
        !ReloadedAnchor->GetEastSouthUpRotation().Equals(
            PreviousAnchorEastSouthUpRotation,
            0.000001) ||
        !FMath::IsNearlyEqual(
            ReloadedExterior->LongitudeDegrees,
            PreviousLongitudeDegrees,
            0.0000001) ||
        !FMath::IsNearlyEqual(
            ReloadedExterior->LatitudeDegrees,
            PreviousLatitudeDegrees,
            0.0000001) ||
        !FMath::IsNearlyEqual(
            ReloadedExterior->HeightMeters,
            PreviousHeightMeters,
            0.001) ||
        !FMath::IsNearlyEqual(
            ReloadedExterior->FootprintYawDegrees,
            PreviousYawDegrees,
            0.001))
    {
        OutMessage = FString::Printf(
            TEXT("The refined exterior passed full map validation after save/reload, but exact persisted placement/collision/visibility preservation could not be proven. Disk state is committed; inspect against backup '%s' without automatically overwriting it."),
            *OutBackupFile);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Migrated only exact v2 to AUTHORED_REFINED_PRIMARY asset '%s', preserved its 182.3-degree ceremonial-front heading and QueryAndPhysics collision, saved/reopened that destination, and left legacy SM_IstanaExterior, SDTH, and v1 untouched. Pre-migration backup: '%s'.\n%s"),
        *IstanaExteriorMeshObjectPath,
        *OutBackupFile,
        *PersistedValidation);
    return true;
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaRuntimeMapV2(FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateRuntimeWorldV2(World, true, OutReport);
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaRuntimeMapV2Readiness(FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateRuntimeWorldV2Readiness(World, true, OutReport);
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaPlayWorldReadiness(FString& OutReport)
{
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString EditorStructuralReport;
    if (!ValidateRuntimeWorldV2(EditorWorld, true, EditorStructuralReport))
    {
        OutReport = TEXT("Istana PIE readiness FAILED because the editor map is structurally invalid:\n") +
            EditorStructuralReport;
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    FString PlayReadinessReport;
    if (!ValidatePlayWorldV2Readiness(PlayWorld, PlayReadinessReport))
    {
        OutReport = PlayReadinessReport +
            TEXT("\nEditor-map structural validation passed separately; no PlayWorld LLH/geodesic validation was attempted.");
        return false;
    }

    OutReport = PlayReadinessReport +
        TEXT("\nEditor-map structural validation passed separately; PlayWorld checks used runtime-relative geometry only.");
    return true;
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaVisualAcceptancePlayWorldReadiness(FString& OutReport)
{
    FString StandardReadiness;
    if (!ValidateIstanaPlayWorldReadiness(StandardReadiness))
    {
        OutReport = TEXT("Visual-acceptance readiness FAILED at the unchanged production readiness gate:\n") +
            StandardReadiness;
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    TArray<ATRIADIstanaRuntimePolicyActor*> RuntimePolicies;
    if (PlayWorld)
    {
        for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(PlayWorld); It; ++It)
        {
            if (It->ActorHasTag(RuntimePolicyActorTag))
            {
                RuntimePolicies.Add(*It);
            }
        }
    }
    if (RuntimePolicies.Num() != 1)
    {
        OutReport = FString::Printf(
            TEXT("Visual-acceptance readiness FAILED: expected one tagged runtime policy; found %d."),
            RuntimePolicies.Num());
        return false;
    }

    FString ProfileReason;
    if (!RuntimePolicies[0]->IsVisualAcceptanceAirSimProfileActive(ProfileReason))
    {
        OutReport = TEXT("Visual-acceptance readiness FAILED: ") + ProfileReason;
        return false;
    }

    TArray<FString> UnreadyTilesets;
    int32 TilesetCount = 0;
    for (TActorIterator<ACesium3DTileset> It(PlayWorld); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        ++TilesetCount;
        const float LoadProgress = Tileset->GetLoadProgress();
        if (!FMath::IsFinite(LoadProgress) ||
            LoadProgress != 100.0f ||
            !Tileset->GetCreatePhysicsMeshes() ||
            !Tileset->EnableFrustumCulling ||
            Tileset->EnableFogCulling)
        {
            UnreadyTilesets.Add(FString::Printf(
                TEXT("%s (progress=%.3f%%, physics=%s, frustum=%s, fog=%s)"),
                *Tileset->GetName(),
                LoadProgress,
                Tileset->GetCreatePhysicsMeshes() ? TEXT("true") : TEXT("false"),
                Tileset->EnableFrustumCulling ? TEXT("true") : TEXT("false"),
                Tileset->EnableFogCulling ? TEXT("true") : TEXT("false")));
        }
    }
    if (TilesetCount == 0 || UnreadyTilesets.Num() > 0)
    {
        OutReport = FString::Printf(
            TEXT("Visual-acceptance readiness FAILED: current-view Cesium must report exactly 100%% with physics, runtime-only frustum culling, and fog culling disabled (%d/%d unready): %s. This does not claim that the complete offscreen 1 km sensor-physics AOI is resident."),
            UnreadyTilesets.Num(),
            TilesetCount,
            *FString::Join(UnreadyTilesets, TEXT(", ")));
        return false;
    }

    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(PlayWorld); It; ++It)
    {
        ExteriorActors.Add(*It);
    }
    ATRIADIstanaExteriorMeshActor* Exterior =
        ExteriorActors.Num() == 1 ? ExteriorActors[0] : nullptr;
    UStaticMeshComponent* ExteriorMesh = Exterior
        ? Exterior->ExteriorMeshComponent.Get()
        : nullptr;
    UStaticMesh* ExteriorStaticMesh = ExteriorMesh
        ? ExteriorMesh->GetStaticMesh()
        : nullptr;
    const bool bAuthoredRefinedRendererActive =
        Exterior && Exterior->bRequiredExteriorMeshLoaded &&
        ExteriorStaticMesh &&
        ExteriorStaticMesh->GetPathName() == IstanaExteriorMeshObjectPath &&
        Exterior->RequiredExteriorMeshAsset.ToSoftObjectPath().ToString() ==
            IstanaExteriorMeshObjectPath &&
        ExteriorMesh->IsVisible() && !ExteriorMesh->bHiddenInGame &&
        ExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision &&
        RuntimePolicies[0]->bAuthoredExteriorFallbackVisibleAtRuntime &&
        RuntimePolicies[0]->bAuthoredExteriorCollisionPreservedAtRuntime &&
        !RuntimePolicies[0]->bStreamedPrimaryVisualActiveAtRuntime;
    if (RuntimePolicies[0]->bPreferStreamedIstanaVisualWhenReady ||
        !bAuthoredRefinedRendererActive)
    {
        OutReport = FString::Printf(
            TEXT("Visual-acceptance readiness FAILED: exact-100 Cesium context is loaded, but AUTHORED_REFINED_PRIMARY is not visibly active with collision (exterior-count=%d, streamed-opt-in=%s). Provider data is context only and must not substitute the Istana building."),
            ExteriorActors.Num(),
            RuntimePolicies[0]->bPreferStreamedIstanaVisualWhenReady
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }

    int32 HiddenStudyOverlayCount = 0;
    if (!RuntimePolicies[0]->bVisualAcceptanceStudyOverlaysHiddenAtRuntime ||
        !RuntimePolicies[0]->IsVisualAcceptanceStudyOverlaySuppressionActive(
            HiddenStudyOverlayCount) ||
        HiddenStudyOverlayCount !=
            RuntimePolicies[0]->VisualAcceptanceHiddenStudyOverlayCountAtRuntime)
    {
        OutReport = FString::Printf(
            TEXT("Visual-acceptance readiness FAILED: TRIADHumanOnlyOverlay components on the study-area actor are not all hidden (live=%d, recorded=%d). AOI/clip state was not changed."),
            HiddenStudyOverlayCount,
            RuntimePolicies[0]->VisualAcceptanceHiddenStudyOverlayCountAtRuntime);
        return false;
    }

    OutReport = FString::Printf(
        TEXT("NON-PRODUCTION VISUAL ACCEPTANCE readiness PASSED with AUTHORED_REFINED_PRIMARY visible and colliding; %d current-view Cesium context tileset(s) are exactly 100%% ready, and %d study-area human-overlay component(s) are hidden. Provider data is retained as surrounding context only and is not claimed as the Istana building. %s\n%s"),
        TilesetCount,
        HiddenStudyOverlayCount,
        *ProfileReason,
        *StandardReadiness);
    return true;
}

bool UTRIADIstanaEditorLibrary::QuiesceIstanaPlayWorldForStop(FString& OutMessage)
{
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        OutMessage = TEXT("No PIE PlayWorld exists to quiesce.");
        return false;
    }

    TArray<ATRIADIstanaRuntimePolicyActor*> RuntimePolicies;
    for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(PlayWorld); It; ++It)
    {
        if (It->ActorHasTag(RuntimePolicyActorTag))
        {
            RuntimePolicies.Add(*It);
        }
    }
    if (RuntimePolicies.Num() != 1)
    {
        OutMessage = FString::Printf(
            TEXT("Expected one tagged Istana runtime policy for AirSim quiescence; found %d. PIE was not stopped."),
            RuntimePolicies.Num());
        return false;
    }

    int32 SimModeCount = 0;
    if (!RuntimePolicies[0]->RequestAirSimQuiescenceForTeardown(SimModeCount))
    {
        OutMessage = FString::Printf(
            TEXT("AirSim quiescence could not be verified across %d active simulation mode(s). PIE was not stopped."),
            SimModeCount);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Requested and verified pause for %d AirSim simulation mode(s). Keep PIE alive for the caller's bounded worker-drain interval before explicit stop."),
        SimModeCount);
    return true;
}

bool UTRIADIstanaEditorLibrary::PrepareIstanaRuntimeMapV2Streaming(FString& OutMessage)
{
    OutMessage.Reset();
    if (!GEditor)
    {
        OutMessage = TEXT("Unreal Editor is unavailable.");
        return false;
    }
    if (GEditor->PlayWorld)
    {
        OutMessage = TEXT("Stop Play-In-Editor before preparing v2 editor streaming.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    FString StructuralReport;
    if (!ValidateRuntimeWorldV2(World, true, StructuralReport))
    {
        OutMessage = TEXT("Streaming preparation requires the structurally valid loaded v2 destination map. ") +
            StructuralReport;
        return false;
    }

    FViewport* Viewport = GEditor->GetActiveViewport();
    if (!GCurrentLevelEditingViewportClient || !Viewport ||
        Viewport->GetClient() != GCurrentLevelEditingViewportClient)
    {
        OutMessage = TEXT("Focus an active Level Editor viewport before preparing Cesium streaming.");
        return false;
    }

    ACameraActor* RuntimeCamera = nullptr;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimeCameraActorTag))
        {
            RuntimeCamera = *It;
            break;
        }
    }
    if (!RuntimeCamera)
    {
        OutMessage = TEXT("The tagged v2 runtime camera is unavailable.");
        return false;
    }

    const float CameraFieldOfView = RuntimeCamera->GetCameraComponent()
        ? RuntimeCamera->GetCameraComponent()->FieldOfView
        : RuntimeCameraFieldOfViewDegrees;
    GCurrentLevelEditingViewportClient->SetViewportType(LVT_Perspective);
    GCurrentLevelEditingViewportClient->SetRealtime(true);
    GCurrentLevelEditingViewportClient->SetViewLocation(RuntimeCamera->GetActorLocation());
    GCurrentLevelEditingViewportClient->SetViewRotation(RuntimeCamera->GetActorRotation());
    GCurrentLevelEditingViewportClient->ViewFOV = CameraFieldOfView;
    GCurrentLevelEditingViewportClient->FOVAngle = CameraFieldOfView;
    GCurrentLevelEditingViewportClient->Invalidate(true, true);
    Viewport->Draw(true);

    int32 TilesetCount = 0;
    int32 RefreshedZeroProgressCount = 0;
    float MinimumProgressBeforeRefresh = 100.0f;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        ++TilesetCount;
        const float LoadProgress = Tileset->GetLoadProgress();
        MinimumProgressBeforeRefresh = FMath::Min(
            MinimumProgressBeforeRefresh,
            LoadProgress);
        if (LoadProgress <= KINDA_SMALL_NUMBER)
        {
            // Exactly one refresh per zero-progress tileset in this call. This
            // reacquires provider state without reading/logging endpoints or
            // tokens and does not modify or save the map package.
            Tileset->RefreshTileset();
            ++RefreshedZeroProgressCount;
        }
    }
    if (TilesetCount == 0)
    {
        OutMessage = TEXT("The structurally valid v2 map unexpectedly contains no Cesium tileset.");
        return false;
    }

    // Draw once more after any refresh so the newly created native tileset
    // immediately receives a current perspective view for tile selection.
    if (RefreshedZeroProgressCount > 0)
    {
        GCurrentLevelEditingViewportClient->Invalidate(true, true);
        Viewport->Draw(true);
    }
    OutMessage = FString::Printf(
        TEXT("Prepared transient v2 Cesium streaming from the active real-time perspective viewport at the tagged runtime camera (FOV %.1f). Drew %d frame(s), checked %d tileset(s), and refreshed %d that were at zero progress (minimum before refresh %.1f%%). No readiness claim, screenshot, package edit, or save was performed; keep the viewport focused and rerun the readiness validator after streaming advances."),
        CameraFieldOfView,
        RefreshedZeroProgressCount > 0 ? 2 : 1,
        TilesetCount,
        RefreshedZeroProgressCount,
        MinimumProgressBeforeRefresh);
    return true;
}

bool UTRIADIstanaEditorLibrary::CalibrateIstanaRuntimeMapV2Ground(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString Validation;
    if (!ValidateRuntimeWorldV2(World, true, Validation))
    {
        OutMessage = TEXT("Ground calibration requires the clean, validated v2 destination map. ") + Validation;
        return false;
    }

    int32 GeoreferenceCount = 0;
    ACesiumGeoreference* Georeference =
        FindTemplateGeoreferenceWithoutModification(World, GeoreferenceCount);
    ATRIADIstanaRuntimePolicyActor* RuntimePolicy = nullptr;
    ATRIADIstanaExteriorMeshActor* ExteriorMeshActor = nullptr;
    ATRIADIstanaStudyAreaActor* StudyArea = nullptr;
    APlayerStart* PlayerStart = nullptr;
    ACameraActor* RuntimeCamera = nullptr;
    for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimePolicyActorTag))
        {
            RuntimePolicy = *It;
        }
    }
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
    {
        ExteriorMeshActor = *It;
    }
    for (TActorIterator<ATRIADIstanaStudyAreaActor> It(World); It; ++It)
    {
        StudyArea = *It;
    }
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimePlayerStartTag))
        {
            PlayerStart = *It;
        }
    }
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RuntimeCameraActorTag))
        {
            RuntimeCamera = *It;
        }
    }
    if (!Georeference || !RuntimePolicy || !ExteriorMeshActor || !StudyArea ||
        !PlayerStart || !RuntimeCamera)
    {
        OutMessage = TEXT("V2 ground-calibration actors are incomplete; no map changes were made.");
        return false;
    }
    if (RuntimePolicy->bExteriorGroundHeightCalibrated)
    {
        OutMessage = FString::Printf(
            TEXT("V2 ground height is already calibrated at %.3f m WGS84 ellipsoid from %d accepted exterior-ring samples; no map changes were made."),
            RuntimePolicy->ExteriorGroundHeightMeters,
            RuntimePolicy->ExteriorGroundAcceptedSampleCount);
        return true;
    }

    double MedianGroundHeightMeters = ApproximateCenterHeightMeters;
    int32 RawSampleCount = 0;
    int32 AcceptedSampleCount = 0;
    if (!SampleIstanaExteriorRingGroundHeight(
            World,
            Georeference,
            ExteriorMeshActor,
            StudyArea,
            MedianGroundHeightMeters,
            RawSampleCount,
            AcceptedSampleCount,
            OutMessage))
    {
        return false;
    }

    UCesiumGlobeAnchorComponent* PlayerStartAnchor =
        PlayerStart->FindComponentByClass<UCesiumGlobeAnchorComponent>();
    UCesiumGlobeAnchorComponent* RuntimeCameraAnchor =
        RuntimeCamera->FindComponentByClass<UCesiumGlobeAnchorComponent>();
    if (!PlayerStartAnchor || !RuntimeCameraAnchor)
    {
        OutMessage = TEXT("V2 start/camera globe anchors are unavailable; no calibration was applied.");
        return false;
    }

    ExteriorMeshActor->Modify();
    StudyArea->Modify();
    PlayerStart->Modify();
    PlayerStartAnchor->Modify();
    RuntimeCamera->Modify();
    RuntimeCameraAnchor->Modify();
    RuntimePolicy->Modify();

    ExteriorMeshActor->ConfigureGeodeticPlacement(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        MedianGroundHeightMeters,
        ExteriorMeshActor->FootprintYawDegrees,
        Georeference);
    StudyArea->ConfigureStudyArea(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        MedianGroundHeightMeters,
        StudyRadiusMeters,
        PolygonPointCount,
        Georeference);
    StudyArea->TerrainStatement = FString::Printf(
        TEXT("Fallback collision disk aligned to %.3f m WGS84 ellipsoid from a median of %d/%d loaded Cesium exterior-ring collision samples; still not survey-grade."),
        MedianGroundHeightMeters,
        AcceptedSampleCount,
        RawSampleCount);

    PlayerStartAnchor->MoveToLongitudeLatitudeHeight(MakeSouthOffsetLongitudeLatitudeHeight(
        PlayerStartSouthOffsetMeters,
        MedianGroundHeightMeters + PlayerStartHeightAboveGroundMeters));
    const FVector PlayerLookAtWorld = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        MedianGroundHeightMeters + PlayerStartHeightAboveGroundMeters));
    PlayerStart->SetActorRotation((PlayerLookAtWorld - PlayerStart->GetActorLocation()).Rotation());

    RuntimeCameraAnchor->MoveToLongitudeLatitudeHeight(MakeSouthOffsetLongitudeLatitudeHeight(
        RuntimeCameraSouthOffsetMeters,
        MedianGroundHeightMeters + RuntimeCameraHeightAboveGroundMeters));
    const FVector CameraLookAtWorld = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        MedianGroundHeightMeters + RuntimeCameraAimAboveGroundMeters));
    RuntimeCamera->SetActorRotation((CameraLookAtWorld - RuntimeCamera->GetActorLocation()).Rotation());

    RuntimePolicy->ConfigureGroundCalibration(
        MedianGroundHeightMeters,
        AcceptedSampleCount,
        true,
        TEXT("CESIUM_COLLISION_EXTERIOR_RING_MEDIAN_WGS84_ELLIPSOID"));

    FString PreSaveValidation;
    if (!ValidateRuntimeWorldV2(World, false, PreSaveValidation))
    {
        OutMessage = TEXT("Calibration was not saved because the adjusted v2 map failed validation: ") + PreSaveValidation;
        return false;
    }
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapV2Package))
    {
        OutMessage = TEXT("Calibration validated but the v2 destination could not be saved; /Game/SDTH was not touched.");
        return false;
    }

    FString PostSaveValidation;
    if (!ValidateRuntimeWorldV2(World, true, PostSaveValidation))
    {
        OutMessage = TEXT("V2 destination saved, but post-save calibration validation failed: ") + PostSaveValidation;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Calibrated only %s to %.3f m WGS84 ellipsoid height using the median of %d accepted samples from %d Cesium collision hits on 95/125 m exterior rings. Moved the imported exterior/fallback foundation, AirSim PlayerStart (+%.1f m), and Player 0 camera (+%.1f m) consistently; no center/roof sample, source-map write, endpoint/token read, or surrounding-context clip was performed.\n%s"),
        *DestinationMapV2Package,
        MedianGroundHeightMeters,
        AcceptedSampleCount,
        RawSampleCount,
        PlayerStartHeightAboveGroundMeters,
        RuntimeCameraHeightAboveGroundMeters,
        *PostSaveValidation);
    return true;
}

bool UTRIADIstanaEditorLibrary::ImportIstanaPbrMaterials(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }

    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("Editor Asset Subsystem is unavailable.");
        return false;
    }

    TMap<FName, UMaterialInterface*> Materials;
    FString ExistingReport;
    const EIstanaPbrAssetState ExistingState =
        InspectIstanaPbrAssetSet(AssetSubsystem, &Materials, ExistingReport);
    if (ExistingState == EIstanaPbrAssetState::CompleteValid)
    {
        OutMessage = TEXT("Istana PBR import is already complete; no assets were changed. ") + ExistingReport;
        return true;
    }
    if (ExistingState == EIstanaPbrAssetState::PartialOrInvalid)
    {
        OutMessage = ExistingReport;
        return false;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UObject*> AssetsToSave;
    if (!BuildIstanaPbrAssetSet(
            AssetTools,
            AssetSubsystem,
            AssetsToSave,
            Materials,
            OutMessage))
    {
        OutMessage = TEXT("Istana PBR import stopped before saving. Discard any unsaved import packages (or restart the editor) before retrying. ") + OutMessage;
        return false;
    }

    FString PreSaveReport;
    if (InspectIstanaPbrAssetSet(AssetSubsystem, nullptr, PreSaveReport) !=
        EIstanaPbrAssetState::CompleteValid)
    {
        OutMessage = TEXT("Istana PBR assets failed pre-save validation; nothing was intentionally saved. ") + PreSaveReport;
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutMessage = TEXT("The complete validated Istana PBR set could not be saved in full; inspect package save errors before retrying.");
        return false;
    }

    FString PostSaveReport;
    if (InspectIstanaPbrAssetSet(AssetSubsystem, nullptr, PostSaveReport) !=
        EIstanaPbrAssetState::CompleteValid)
    {
        OutMessage = TEXT("Istana PBR packages saved, but post-save validation failed: ") + PostSaveReport;
        return false;
    }
    OutMessage = TEXT("Imported the generated Istana texture set into /Game/TRIAD/Istana/Textures and created eight exact-slot project-owned materials under /Game/TRIAD/Istana/Materials. ") + PostSaveReport;
    return true;
}

bool UTRIADIstanaEditorLibrary::ValidateIstanaPbrMaterials(FString& OutReport)
{
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutReport = TEXT("Editor Asset Subsystem is unavailable.");
        return false;
    }
    const EIstanaPbrAssetState State =
        InspectIstanaPbrAssetSet(AssetSubsystem, nullptr, OutReport);
    if (State == EIstanaPbrAssetState::Absent)
    {
        OutReport += TEXT(" Run ImportIstanaPbrMaterials to create the non-overwriting project-owned set.");
    }
    return State == EIstanaPbrAssetState::CompleteValid;
}

bool UTRIADIstanaEditorLibrary::ImportIstanaExteriorLod0(FString& OutMessage)
{
    OutMessage = TEXT("The legacy SM_IstanaExterior import target is retired and will not be overwritten or recreated from the refined source. Run ImportIstanaExteriorRefinedLod0 instead.");
    return false;
}

bool UTRIADIstanaEditorLibrary::ImportIstanaExteriorRefinedLod0(
    FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }

    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("Editor Asset Subsystem is unavailable.");
        return false;
    }
    if (FPackageName::DoesPackageExist(IstanaExteriorMeshPackage) ||
        AssetSubsystem->DoesAssetExist(IstanaExteriorMeshObjectPath))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing Istana exterior asset '%s'."),
            *IstanaExteriorMeshObjectPath);
        return false;
    }

    const FString SourceObjPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/Istana/Generated/SM_IstanaExterior_LOD0.obj")));
    if (!FPaths::FileExists(SourceObjPath))
    {
        OutMessage = FString::Printf(
            TEXT("Required generated LOD0 OBJ is missing at '%s'. Run SourceAssets/Istana/Build-IstanaSourceAssets.ps1 first."),
            *SourceObjPath);
        return false;
    }
    FString SourceManifestError;
    if (!ValidateRefinedIstanaSourceManifest(
            SourceObjPath,
            SourceManifestError))
    {
        OutMessage = TEXT("Refined LOD0 import refused its generated source contract: ") +
            SourceManifestError;
        return false;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UObject*> AssetsToSave;
    TMap<FName, UMaterialInterface*> Materials;
    FString PbrReport;
    const EIstanaPbrAssetState PbrState =
        InspectIstanaPbrAssetSet(AssetSubsystem, &Materials, PbrReport);
    if (PbrState != EIstanaPbrAssetState::CompleteValid)
    {
        OutMessage = TEXT("Refined LOD0 import requires the complete existing eight-material PBR set and will not create or overwrite it. Run/validate ImportIstanaPbrMaterials first. ") +
            PbrReport;
        return false;
    }

    UFbxImportUI* ImportOptions = NewObject<UFbxImportUI>();
    UFbxFactory* ImportFactory = NewObject<UFbxFactory>();
    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    if (!ImportOptions || !ImportFactory || !ImportTask || !ImportOptions->StaticMeshImportData)
    {
        OutMessage = TEXT("Could not allocate deterministic OBJ import options.");
        return false;
    }

    ImportOptions->bImportAsSkeletal = false;
    ImportOptions->MeshTypeToImport = FBXIT_StaticMesh;
    ImportOptions->bAutomatedImportShouldDetectType = false;
    ImportOptions->bImportMesh = true;
    ImportOptions->bImportMaterials = false;
    ImportOptions->bImportTextures = false;
    ImportOptions->StaticMeshImportData->ImportUniformScale = 1.0f;
    ImportOptions->StaticMeshImportData->bCombineMeshes = true;
    ImportOptions->StaticMeshImportData->bAutoGenerateCollision = true;
    ImportOptions->StaticMeshImportData->bGenerateLightmapUVs = true;
    ImportOptions->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    ImportOptions->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    ImportOptions->StaticMeshImportData->bBuildNanite = false;
    ImportOptions->StaticMeshImportData->bRemoveDegenerates = true;
    ImportFactory->ImportUI = ImportOptions;
    ImportFactory->SetDetectImportTypeOnImport(false);

    ImportTask->Filename = SourceObjPath;
    ImportTask->DestinationPath = TEXT("/Game/TRIAD/Istana/Meshes");
    ImportTask->DestinationName = TEXT("SM_IstanaExterior_Refined");
    ImportTask->bReplaceExisting = false;
    ImportTask->bReplaceExistingSettings = false;
    ImportTask->bAutomated = true;
    ImportTask->bSave = false;
    ImportTask->bAsync = false;
    ImportTask->Factory = ImportFactory;
    ImportTask->Options = ImportOptions;

    TArray<UAssetImportTask*> ImportTasks;
    ImportTasks.Add(ImportTask);
    AssetTools.ImportAssetTasks(ImportTasks);

    UStaticMesh* ImportedMesh = nullptr;
    for (UObject* ImportedObject : ImportTask->GetObjects())
    {
        if (UStaticMesh* Candidate = Cast<UStaticMesh>(ImportedObject))
        {
            ImportedMesh = Candidate;
            break;
        }
    }
    if (!ImportedMesh)
    {
        ImportedMesh = LoadObject<UStaticMesh>(nullptr, *IstanaExteriorMeshObjectPath);
    }
    if (ImportedMesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({ImportedMesh});
    }

    FString ValidationError;
    if (!ValidateIstanaExteriorMeshAsset(ImportedMesh, false, ValidationError))
    {
        OutMessage = TEXT("Refined LOD0 import did not meet the source-asset contract: ") + ValidationError;
        return false;
    }

    AssetsToSave.Add(ImportedMesh);
    ImportedMesh->Modify();
    for (const FIstanaMaterialSpec& Spec : GetIstanaMaterialSpecs())
    {
        UMaterialInterface* Material = Materials.FindRef(Spec.SlotName);
        if (!Material)
        {
            OutMessage = FString::Printf(
                TEXT("Validated PBR set did not provide exact material slot '%s'."),
                *Spec.SlotName.ToString());
            return false;
        }
        const int32 MaterialIndex = ImportedMesh->GetMaterialIndex(Spec.SlotName);
        ImportedMesh->SetMaterial(MaterialIndex, Material);
    }
    ImportedMesh->PostEditChange();
    ImportedMesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({ImportedMesh});

    if (!ValidateIstanaExteriorMeshAsset(ImportedMesh, true, ValidationError))
    {
        OutMessage = TEXT("Imported refined mesh/material validation failed: ") + ValidationError;
        return false;
    }
    FString PbrValidation;
    if (InspectIstanaPbrAssetSet(AssetSubsystem, nullptr, PbrValidation) !=
        EIstanaPbrAssetState::CompleteValid)
    {
        OutMessage = TEXT("Imported mesh is valid, but the authored PBR set failed pre-save validation: ") + PbrValidation;
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutMessage = TEXT("Validated the refined Istana mesh but could not save its new package.");
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Imported and validated the reviewed regenerated LOD0 as new non-overwriting asset %s: 58,658 imported triangles, reviewed bounds, simple collision, and eight exact existing project-owned PBR materials passed. Its manifest declares 175,974 source vertices, the corrected facade signature, and the reviewed digest; the narrow PowerShell caller separately proves the OBJ byte SHA-256 before this call. Legacy SM_IstanaExterior was not read, edited, or overwritten. LOD1/LOD2 remain an explicit follow-up import."),
        *IstanaExteriorMeshObjectPath);
    return true;
}

bool UTRIADIstanaEditorLibrary::GenerateIstanaPlacementSurvey(
    const FString& RequestJsonPath,
    const FString& OutputFileName,
    FString& OutMessage)
{
    FString Validation;
    if (!ValidateIstanaStudyMap(Validation))
    {
        OutMessage = TEXT("Placement survey requires a valid loaded Istana map. ") + Validation;
        return false;
    }

    FSurveyRequest Request;
    if (!ParseSurveyRequest(RequestJsonPath, Request, OutMessage))
    {
        return false;
    }

    FString DestinationPath;
    if (!ResolveNewSavedFile(
            TEXT("PlacementSurveys"),
            OutputFileName,
            TEXT(".json"),
            DestinationPath,
            OutMessage))
    {
        return false;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ACesiumGeoreference* Georeference = World
        ? ACesiumGeoreference::GetDefaultGeoreference(World)
        : nullptr;
    if (!World || !Georeference)
    {
        OutMessage = TEXT("Editor world or default Cesium georeference is unavailable.");
        return false;
    }

    // Deterministic exact-AOI grid. Candidate mounts are 8 m AGL and target
    // samples are 20 m AGL; vertical traces resolve actual loaded collision.
    TArray<FSurveyPoint> Options;
    Options.Add(ResolveSurveyPoint(World, Georeference, TEXT("site-center"), 0.0, 0.0, 8.0));
    AddSurveyRing(World, Georeference, TEXT("site-r200"), 200.0, 8, 8.0, Options);
    AddSurveyRing(World, Georeference, TEXT("site-r450"), 450.0, 12, 8.0, Options);
    AddSurveyRing(World, Georeference, TEXT("site-r750"), 750.0, 16, 8.0, Options);
    // Perimeter-facing coverage needs candidates outside the 900 m sample
    // ring so fixed RGB cameras can point inward without creating an
    // unsurveyed annulus. These sites remain 50 m inside the exact AOI.
    AddSurveyRing(World, Georeference, TEXT("site-r950"), 950.0, 16, 8.0, Options);

    TArray<FSurveyPoint> Samples;
    for (double AltitudeAglMeters : Request.TargetAltitudeBandsAglMeters)
    {
        const FString AltitudeSuffix = FString::Printf(TEXT("agl%04d"), FMath::RoundToInt(AltitudeAglMeters));
        Samples.Add(ResolveSurveyPoint(
            World,
            Georeference,
            FString::Printf(TEXT("sample-%s-center"), *AltitudeSuffix),
            0.0,
            0.0,
            AltitudeAglMeters));
        AddSurveyRing(World, Georeference, FString::Printf(TEXT("sample-%s-r150"), *AltitudeSuffix),
            150.0, 8, AltitudeAglMeters, Samples);
        AddSurveyRing(World, Georeference, FString::Printf(TEXT("sample-%s-r400"), *AltitudeSuffix),
            400.0, 8, AltitudeAglMeters, Samples);
        AddSurveyRing(World, Georeference, FString::Printf(TEXT("sample-%s-r650"), *AltitudeSuffix),
            650.0, 8, AltitudeAglMeters, Samples);
        AddSurveyRing(World, Georeference, FString::Printf(TEXT("sample-%s-r900"), *AltitudeSuffix),
            900.0, 8, AltitudeAglMeters, Samples);
    }
    if (Samples.Num() > Request.MaximumSampleCount)
    {
        Samples.SetNum(Request.MaximumSampleCount, EAllowShrinking::No);
    }

    int32 ResolvedOptionCount = 0;
    int32 ResolvedSampleCount = 0;
    TArray<TSharedPtr<FJsonValue>> UnresolvedOptionIds;
    TArray<TSharedPtr<FJsonValue>> UnresolvedSampleIds;
    TArray<TSharedPtr<FJsonValue>> OptionValues;
    TArray<TSharedPtr<FJsonValue>> SampleValues;
    OptionValues.Reserve(Options.Num());
    SampleValues.Reserve(Samples.Num());

    for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
    {
        const FSurveyPoint& Option = Options[OptionIndex];
        ResolvedOptionCount += Option.bSurfaceResolved ? 1 : 0;
        if (!Option.bSurfaceResolved)
        {
            UnresolvedOptionIds.Add(MakeShared<FJsonValueString>(Option.Id));
        }
        const double InwardBearingDegrees = FMath::Fmod(Option.OutwardBearingDegrees + 180.0, 360.0);
        const double UnrealYawDegrees = FRotator::NormalizeAxis(InwardBearingDegrees - 90.0);
        TSharedPtr<FJsonObject> Rotation = MakeShared<FJsonObject>();
        Rotation->SetNumberField(TEXT("pitchDegrees"), 0.0);
        Rotation->SetNumberField(TEXT("yawDegrees"), UnrealYawDegrees);
        Rotation->SetNumberField(TEXT("rollDegrees"), 0.0);

        TSharedPtr<FJsonObject> Overrides = Request.UnrealNodeTemplate;

        TSharedPtr<FJsonObject> OptionJson = MakeShared<FJsonObject>();
        OptionJson->SetStringField(TEXT("optionId"), FString::Printf(TEXT("opt-%03d"), OptionIndex));
        OptionJson->SetStringField(TEXT("siteId"), Option.Id);
        OptionJson->SetStringField(TEXT("packageId"), Request.PackageId);
        OptionJson->SetStringField(TEXT("orientationId"), TEXT("default"));
        OptionJson->SetObjectField(TEXT("location"), MakeLocationJson(Option.LongitudeLatitudeHeight));
        OptionJson->SetStringField(
            TEXT("failureDomainId"),
            Option.RadiusMeters < 1.0
                ? TEXT("istana-main-building")
                : FString::Printf(TEXT("sector-%d"), static_cast<int32>(Option.OutwardBearingDegrees / 45.0) % 8));
        OptionJson->SetNumberField(TEXT("siteCostUnits"), Request.PackageCostUnits);
        OptionJson->SetBoolField(TEXT("feasible"), Option.bSurfaceResolved);
        TArray<TSharedPtr<FJsonValue>> RejectionReasons;
        if (!Option.bSurfaceResolved)
        {
            RejectionReasons.Add(MakeShared<FJsonValueString>(TEXT("UNRESOLVED_SURFACE_COLLISION")));
        }
        OptionJson->SetArrayField(TEXT("rejectionReasons"), RejectionReasons);
        OptionJson->SetObjectField(TEXT("cameraRotation"), Rotation);
        OptionJson->SetObjectField(TEXT("unrealNodeOverrides"), Overrides);
        OptionValues.Add(MakeShared<FJsonValueObject>(OptionJson));
    }

    for (const FSurveyPoint& Sample : Samples)
    {
        ResolvedSampleCount += Sample.bSurfaceResolved ? 1 : 0;
        if (!Sample.bSurfaceResolved)
        {
            UnresolvedSampleIds.Add(MakeShared<FJsonValueString>(Sample.Id));
        }
        TSharedPtr<FJsonObject> SampleJson = MakeShared<FJsonObject>();
        SampleJson->SetStringField(TEXT("sampleId"), Sample.Id);
        SampleJson->SetObjectField(TEXT("location"), MakeLocationJson(Sample.LongitudeLatitudeHeight));
        SampleJson->SetNumberField(TEXT("eastMeters"), Sample.EastMeters);
        SampleJson->SetNumberField(TEXT("northMeters"), Sample.NorthMeters);
        SampleJson->SetNumberField(TEXT("altitudeAglMeters"), Sample.AltitudeAglMeters);
        SampleJson->SetNumberField(TEXT("weight"), Sample.RadiusMeters <= 300.0 ? 2.0 : 1.0);
        SampleJson->SetBoolField(TEXT("critical"), Sample.RadiusMeters <= 300.0);
        SampleValues.Add(MakeShared<FJsonValueObject>(SampleJson));
    }

    TArray<TSharedPtr<FJsonValue>> EvaluationValues;
    EvaluationValues.Reserve(Options.Num() * Samples.Num() * Request.Scenarios.Num() * 2);
    for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
    {
        const FSurveyPoint& Option = Options[OptionIndex];
        const FString OptionId = FString::Printf(TEXT("opt-%03d"), OptionIndex);
        const double CameraBearingDegrees = FMath::Fmod(Option.OutwardBearingDegrees + 180.0, 360.0);
        for (const FSurveyPoint& Sample : Samples)
        {
            const FVector DeltaWorld = Sample.WorldPosition - Option.WorldPosition;
            const double RangeMeters = DeltaWorld.Size() / 100.0;
            const double HorizontalMeters = FVector2D(DeltaWorld.X, DeltaWorld.Y).Size() / 100.0;
            const double BearingDegrees = FMath::Fmod(
                FMath::RadiansToDegrees(FMath::Atan2(DeltaWorld.X, -DeltaWorld.Y)) + 360.0,
                360.0);
            const double ElevationDegrees = FMath::RadiansToDegrees(FMath::Atan2(
                DeltaWorld.Z / 100.0,
                FMath::Max(HorizontalMeters, 0.001)));
            const bool bWithinVisualFov = FMath::Abs(FMath::FindDeltaAngleDegrees(
                CameraBearingDegrees,
                BearingDegrees)) <= Request.CameraFieldOfViewDegrees * 0.5;

            FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(TRIADIstanaSurveyVisibility), true);
            FHitResult BlockingHit;
            const bool bBlocked = World->LineTraceSingleByChannel(
                BlockingHit,
                Option.WorldPosition,
                Sample.WorldPosition,
                ECC_Visibility,
                QueryParameters);
            const bool bLineOfSight = !bBlocked;
            const FString BlockingActor = bBlocked && BlockingHit.GetActor()
                ? BlockingHit.GetActor()->GetName()
                : bBlocked ? TEXT("<component-only-hit>") : TEXT("");

            for (const FSurveyScenario& Scenario : Request.Scenarios)
            {
                struct FModalityDefinition
                {
                    const TCHAR* Modality;
                    const TCHAR* Family;
                    double MaximumRangeMeters;
                    bool bWithinFov;
                    double WeatherFactor;
                };
                const bool bMonsoon = Scenario.WeatherProfile.Equals(TEXT("Monsoon"), ESearchCase::IgnoreCase);
                const bool bHaze = Scenario.WeatherProfile.Equals(TEXT("Haze"), ESearchCase::IgnoreCase);
                const double VisualWeatherFactor = bMonsoon ? 0.55 : bHaze ? 0.35 : 1.0;
                const double RadarWeatherFactor = bMonsoon ? 0.85 : bHaze ? 0.95 : 1.0;
                TArray<FModalityDefinition, TInlineAllocator<2>> Modalities;
                if (Request.bHasSearchRadar)
                {
                    Modalities.Add(FModalityDefinition{
                        TEXT("SEARCH_RADAR"), TEXT("active_radar"),
                        Request.SearchRadarRangeMeters, true, RadarWeatherFactor});
                }
                if (Request.bHasRgb)
                {
                    Modalities.Add(FModalityDefinition{
                        TEXT("rgb"), TEXT("visual"),
                        Request.RgbRangeMeters, bWithinVisualFov, VisualWeatherFactor});
                }
                for (const FModalityDefinition& Definition : Modalities)
                {
                    const bool bWithinRange = RangeMeters <= Definition.MaximumRangeMeters;
                    const bool bEligible = Option.bSurfaceResolved && Sample.bSurfaceResolved &&
                        bWithinRange && Definition.bWithinFov && bLineOfSight;
                    const double RangeQuality = FMath::Clamp(
                        1.0 - RangeMeters / Definition.MaximumRangeMeters,
                        0.0,
                        1.0);
                    const double Quality = bEligible
                        ? FMath::Clamp(RangeQuality * Definition.WeatherFactor, 0.0, 1.0)
                        : 0.0;
                    TSharedPtr<FJsonObject> Evaluation = MakeShared<FJsonObject>();
                    Evaluation->SetStringField(TEXT("optionId"), OptionId);
                    Evaluation->SetStringField(TEXT("sampleId"), Sample.Id);
                    Evaluation->SetStringField(TEXT("scenarioId"), Scenario.ScenarioId);
                    Evaluation->SetStringField(TEXT("modality"), Definition.Modality);
                    Evaluation->SetStringField(TEXT("family"), Definition.Family);
                    Evaluation->SetNumberField(TEXT("quality"), Quality);
                    Evaluation->SetBoolField(TEXT("eligible"), bEligible);
                    Evaluation->SetNumberField(TEXT("rangeMeters"), RangeMeters);
                    Evaluation->SetNumberField(TEXT("maximumRangeMeters"), Definition.MaximumRangeMeters);
                    Evaluation->SetNumberField(TEXT("bearingDegrees"), BearingDegrees);
                    Evaluation->SetNumberField(TEXT("elevationDegrees"), ElevationDegrees);
                    Evaluation->SetBoolField(TEXT("withinRange"), bWithinRange);
                    Evaluation->SetBoolField(TEXT("withinFov"), Definition.bWithinFov);
                    Evaluation->SetBoolField(TEXT("lineOfSight"), bLineOfSight);
                    Evaluation->SetBoolField(TEXT("lineOfSightRequired"), true);
                    Evaluation->SetStringField(TEXT("source"), TEXT("UNREAL_PLACEMENT_SURVEY"));
                    Evaluation->SetStringField(TEXT("blockingActor"), BlockingActor);
                    Evaluation->SetBoolField(TEXT("traceComplex"), true);
                    EvaluationValues.Add(MakeShared<FJsonValueObject>(Evaluation));
                }
            }
        }
    }

    const bool bGeometryAuthoritative =
        ResolvedOptionCount == Options.Num() && ResolvedSampleCount == Samples.Num();
    TSharedPtr<FJsonObject> WorldJson = MakeShared<FJsonObject>();
    WorldJson->SetStringField(TEXT("mapPackage"), DestinationMapPackage);
    WorldJson->SetStringField(TEXT("generator"), TEXT("TRIADSensorFusionEditor.IstanaSurvey.v1"));
    WorldJson->SetObjectField(TEXT("center"), MakeLocationJson(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        ApproximateCenterHeightMeters)));
    WorldJson->SetNumberField(TEXT("radiusMeters"), StudyRadiusMeters);
    WorldJson->SetStringField(TEXT("aoiGeometry"), TEXT("WGS84_GEODESIC_CIRCLE"));
    WorldJson->SetStringField(TEXT("traceChannel"), TEXT("ECC_Visibility"));
    WorldJson->SetBoolField(TEXT("traceComplex"), true);
    WorldJson->SetBoolField(TEXT("geometryAuthoritative"), bGeometryAuthoritative);
    WorldJson->SetStringField(TEXT("geometryAuthorityScope"), TEXT("LOADED_SIMULATION_COLLISION_ONLY"));
    WorldJson->SetBoolField(TEXT("surveyGrade"), false);
    WorldJson->SetBoolField(TEXT("approximateTerrainFallbackEnabled"), true);
    WorldJson->SetStringField(
        TEXT("readinessSemantics"),
        TEXT("Authoritative only when every candidate/sample vertical trace resolves against loaded collision; run after Cesium tiles finish loading."));
    WorldJson->SetNumberField(TEXT("resolvedOptionCount"), ResolvedOptionCount);
    WorldJson->SetNumberField(TEXT("optionCount"), Options.Num());
    WorldJson->SetNumberField(TEXT("resolvedSampleCount"), ResolvedSampleCount);
    WorldJson->SetNumberField(TEXT("sampleCount"), Samples.Num());
    WorldJson->SetArrayField(TEXT("unresolvedOptionIds"), UnresolvedOptionIds);
    WorldJson->SetArrayField(TEXT("unresolvedSampleIds"), UnresolvedSampleIds);
    WorldJson->SetStringField(
        TEXT("fidelityStatement"),
        TEXT("Public exterior/terrain collision survey; custom Istana model and 30 m SRTM fallback are visual/physics approximations, not survey/BIM data."));

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schemaVersion"), TEXT("triad.placement_survey.v1"));
    Root->SetStringField(TEXT("requestId"), Request.RequestId);
    Root->SetObjectField(TEXT("world"), WorldJson);
    Root->SetArrayField(TEXT("options"), OptionValues);
    Root->SetArrayField(TEXT("samples"), SampleValues);
    Root->SetArrayField(TEXT("evaluations"), EvaluationValues);

    if (!WriteJsonWithoutOverwrite(DestinationPath, Root, OutMessage))
    {
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Wrote %s with %d options, %d samples, and %d modality/scenario evaluations. Geometry authoritative: %s (%d/%d option surfaces, %d/%d sample surfaces resolved)."),
        *DestinationPath,
        Options.Num(),
        Samples.Num(),
        EvaluationValues.Num(),
        bGeometryAuthoritative ? TEXT("true") : TEXT("false"),
        ResolvedOptionCount,
        Options.Num(),
        ResolvedSampleCount,
        Samples.Num());
    return true;
}

bool UTRIADIstanaEditorLibrary::CaptureIstanaPreview(
    const FString& PresetName,
    const FString& OutputFileName,
    FString& OutMessage)
{
    FString Validation;
    if (!ValidateIstanaRuntimeMapV2Readiness(Validation))
    {
        OutMessage = TEXT("V2 preview capture requires the validated loaded imported-mesh runtime map. ") + Validation;
        return false;
    }
    if (!OutputFileName.StartsWith(TEXT("v2_editor_"), ESearchCase::IgnoreCase))
    {
        OutMessage = TEXT("V2 editor-preview filenames must start with 'v2_editor_' so editor, PIE, and legacy/procedural captures cannot be mislabeled.");
        return false;
    }

    FString DestinationPath;
    if (!ResolveNewSavedFile(
            TEXT("IstanaPreviews/V2"),
            OutputFileName,
            TEXT(".png"),
            DestinationPath,
            OutMessage))
    {
        return false;
    }

    FString NormalizedPreset = PresetName.ToUpper();
    FVector LocalCameraCentimeters;
    if (NormalizedPreset == TEXT("FRONT"))
    {
        LocalCameraCentimeters = FVector(0.0, 28000.0, 8500.0);
    }
    else if (NormalizedPreset == TEXT("OBLIQUE"))
    {
        LocalCameraCentimeters = FVector(22000.0, 24000.0, 11500.0);
    }
    else if (NormalizedPreset == TEXT("SIDE"))
    {
        LocalCameraCentimeters = FVector(29000.0, 0.0, 9000.0);
    }
    else
    {
        OutMessage = TEXT("PresetName must be FRONT, OBLIQUE, or SIDE.");
        return false;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorMeshActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
    {
        ExteriorMeshActors.Add(*It);
    }
    if (ExteriorMeshActors.Num() != 1 ||
        !ExteriorMeshActors[0]->bRequiredExteriorMeshLoaded ||
        !ExteriorMeshActors[0]->ExteriorMeshComponent ||
        ExteriorMeshActors[0]->ExteriorMeshComponent->GetStaticMesh() == nullptr ||
        !GCurrentLevelEditingViewportClient)
    {
        OutMessage = TEXT("Exactly one loaded v2 Istana imported-mesh actor and an active level-editor viewport are required.");
        return false;
    }

    FViewport* Viewport = GEditor->GetActiveViewport();
    if (!Viewport || Viewport->GetClient() != GCurrentLevelEditingViewportClient)
    {
        OutMessage = TEXT("Focus a Level Editor viewport before requesting an Istana preview.");
        return false;
    }

    ATRIADIstanaExteriorMeshActor* ExteriorMeshActor = ExteriorMeshActors[0];
    const FTransform BuildingTransform = ExteriorMeshActor->GetActorTransform();
    const FVector CameraLocation = BuildingTransform.TransformPosition(LocalCameraCentimeters);
    const FVector LookAtLocation = BuildingTransform.TransformPosition(FVector(0.0, 500.0, 1300.0));
    GCurrentLevelEditingViewportClient->SetViewportType(LVT_Perspective);
    GCurrentLevelEditingViewportClient->SetViewLocation(CameraLocation);
    GCurrentLevelEditingViewportClient->SetViewRotation((LookAtLocation - CameraLocation).Rotation());
    GCurrentLevelEditingViewportClient->SetLookAtLocation(LookAtLocation, false);
    GCurrentLevelEditingViewportClient->ViewFOV = 50.0f;
    GCurrentLevelEditingViewportClient->FOVAngle = 50.0f;
    GCurrentLevelEditingViewportClient->Invalidate(true, true);

    FScreenshotRequest::RequestScreenshot(DestinationPath, false, false, false);
    Viewport->Draw(false);
    OutMessage = FString::Printf(
        TEXT("Captured deterministic V2_IMPORTED_MESH %s Istana preview to '%s'. If the renderer is still flushing, the PNG will appear on the next editor frame."),
        *NormalizedPreset,
        *DestinationPath);
    return true;
}

bool UTRIADIstanaEditorLibrary::CaptureIstanaPlaySpawnPreview(
    const FString& OutputFileName,
    FString& OutMessage)
{
    if (!OutputFileName.StartsWith(TEXT("v2_play_"), ESearchCase::IgnoreCase))
    {
        OutMessage = TEXT("PIE spawn preview filenames must start with 'v2_play_'.");
        return false;
    }

    const bool bVisualAcceptance = FParse::Param(
        FCommandLine::Get(),
        TEXT("TRIADIstanaVisualAcceptance"));
    FString Validation;
    const bool bReady = bVisualAcceptance
        ? ValidateIstanaVisualAcceptancePlayWorldReadiness(Validation)
        : ValidateIstanaPlayWorldReadiness(Validation);
    if (!bReady)
    {
        OutMessage = TEXT("PIE spawn capture requires the active origin-safe v2 PlayWorld readiness mode. ") + Validation;
        return false;
    }
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;

    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorMeshActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(PlayWorld); It; ++It)
    {
        ExteriorMeshActors.Add(*It);
    }
    APlayerController* PlayerController = PlayWorld->GetFirstPlayerController();
    if (ExteriorMeshActors.Num() != 1 || !PlayerController)
    {
        OutMessage = TEXT("PIE does not contain exactly one v2 exterior mesh actor and a Player 0 controller.");
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const double SpawnDistanceMeters = FVector::Distance(
        ViewLocation,
        ExteriorMeshActors[0]->GetActorLocation()) / 100.0;
    if (!FMath::IsFinite(SpawnDistanceMeters) || SpawnDistanceMeters > StudyRadiusMeters)
    {
        OutMessage = FString::Printf(
            TEXT("Player 0 view is %.1f m from the Istana exterior, outside the exact 1 km acceptance AOI; refusing a misleading spawn screenshot."),
            SpawnDistanceMeters);
        return false;
    }

    UGameViewportClient* GameViewportClient = PlayWorld->GetGameViewport();
    FSceneViewport* GameViewport = GameViewportClient
        ? GameViewportClient->GetGameViewport()
        : nullptr;
    if (!GameViewport)
    {
        OutMessage = TEXT("The active PIE game viewport is unavailable; focus the Player 0 PIE viewport and retry.");
        return false;
    }

    FString DestinationPath;
    if (!ResolveNewSavedFile(
            TEXT("IstanaPreviews/V2"),
            OutputFileName,
            TEXT(".png"),
            DestinationPath,
            OutMessage))
    {
        return false;
    }

    FScreenshotRequest::RequestScreenshot(DestinationPath, false, false, false);
    GameViewport->Draw(false);
    OutMessage = FString::Printf(
        TEXT("Captured V2_PIE_PLAYER0_SPAWN at %.1f m from the imported Istana exterior to '%s'. View location=(%.1f, %.1f, %.1f) cm, rotation=(P=%.1f Y=%.1f R=%.1f)."),
        SpawnDistanceMeters,
        *DestinationPath,
        ViewLocation.X,
        ViewLocation.Y,
        ViewLocation.Z,
        ViewRotation.Pitch,
        ViewRotation.Yaw,
        ViewRotation.Roll);
    return true;
}
