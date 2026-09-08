#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
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
#include "Materials/MaterialExpressionVertexNormalWS.h"
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
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "UObject/Package.h"

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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/R29CopernicusTerrainFallback"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MeshName(
    TEXT("SM_IPV5D_R29_CopernicusTerrainFallback_Render"));
const FString MaterialName(
    TEXT("M_IPV5D_R29_CopernicusTerrainFallback_ComplementaryCoreMask"));
const FString ImportedSlotName(
    TEXT("MI_IPV5D_R29_CopernicusTerrainProxy"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);
const FString MaterialObjectPath(
    MaterialRoot + TEXT("/") + MaterialName + TEXT(".") + MaterialName);

constexpr int32 ExpectedAssetCount = 2;
constexpr int32 ExpectedVertexCount = 16641;
constexpr int32 ExpectedVertexInstanceCount = 98304;
constexpr int32 ExpectedTriangleCount = 32768;
constexpr int32 ExpectedMaterialCount = 1;
constexpr int32 ExpectedSectionCount = 1;
constexpr float RequiredImportScale = 100.0f;
const FVector ExpectedBoundsMin(-100000.0, -100000.0, -3030.3865);
const FVector ExpectedBoundsMax(100000.0, 100000.0, 119.3868);

const FString WorldPositionNodeId(
    TEXT("R29Copernicus.AbsoluteWorldPositionNoOffsets"));
const FString VertexNormalNodeId(TEXT("R29Copernicus.VertexNormalWS"));
const FString AppearanceNodeId(
    TEXT("TRIAD_R29_COPERNICUS_DEM_TROPICAL_GROUND_RESPONSE_V1"));
const FString ComplementMaskNodeId(
    TEXT("TRIAD_R29_COPERNICUS_EXACT_COMPLEMENT_OF_V5D_CORE_MASK_V1"));
const FString BaseColorNodeId(TEXT("R29Copernicus.BaseColorRGB"));
const FString RoughnessNodeId(TEXT("R29Copernicus.RoughnessA"));
const FString MetallicNodeId(TEXT("R29Copernicus.NonMetal"));
const FString SpecularNodeId(TEXT("R29Copernicus.DielectricSpecular"));

const FString AppearanceCode(
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float2 p=(worldM+float2(31.7,-19.3))/42.0;\n")
    TEXT("float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f);\n")
    TEXT("float4 h=frac(sin(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+float2(1,1),float2(127.1,311.7))))*43758.5453);\n")
    TEXT("float broad=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);\n")
    TEXT("float2 q=(worldM+float2(-7.1,13.9))/7.5;\n")
    TEXT("float2 j=floor(q); float2 g=frac(q); g=g*g*(3.0-2.0*g);\n")
    TEXT("float4 k=frac(sin(float4(dot(j,float2(269.5,183.3)),dot(j+float2(1,0),float2(269.5,183.3)),dot(j+float2(0,1),float2(269.5,183.3)),dot(j+float2(1,1),float2(269.5,183.3))))*24634.6345);\n")
    TEXT("float meso=lerp(lerp(k.x,k.y,g.x),lerp(k.z,k.w,g.x),g.y);\n")
    TEXT("float slope=saturate(1.0-abs(normalize(VertexNormal).z));\n")
    TEXT("float low=saturate((-WorldPosition.z-250.0)/2200.0);\n")
    TEXT("float soil=saturate(0.58*slope+0.28*low+0.20*(meso-0.5));\n")
    TEXT("float3 humid=float3(0.105,0.165,0.070);\n")
    TEXT("float3 dry=float3(0.205,0.185,0.105);\n")
    TEXT("float3 base=lerp(humid,dry,soil)*lerp(0.90,1.10,0.62*broad+0.38*meso);\n")
    TEXT("float rough=clamp(0.84+0.08*soil+0.035*(broad-0.5),0.78,0.95);\n")
    TEXT("return float4(saturate(base),rough);"));

FString BuildComplementMaskCode()
{
    const FVector2D Center = ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipCenterCentimeters();
    const FVector2D Axes = ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipSemiAxesCentimeters();
    const FVector Amplitudes = ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipRippleAmplitudes();
    const FVector Phases = ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipRipplePhasesRadians();
    return FString::Printf(
        TEXT("const float2 coreCenterCm=float2(%.9f,%.9f);\n")
        TEXT("const float2 semiAxesCm=float2(%.9f,%.9f);\n")
        TEXT("const float opaqueCollarCm=%.9f;\n")
        TEXT("const float outwardFeatherCm=%.9f;\n")
        TEXT("const float ditherCellCm=%.9f;\n")
        TEXT("const float segmentParameterEpsilon=%.9f;\n")
        TEXT("const float twoPi=6.28318530717958647692;\n")
        TEXT("const int edgeCount=%d;\n")
        TEXT("const float sector=twoPi/(float)edgeCount;\n")
        TEXT("const float ripple3Amplitude=%.9f;\n")
        TEXT("const float ripple5Amplitude=%.9f;\n")
        TEXT("const float ripple7Amplitude=%.9f;\n")
        TEXT("const float ripple3Phase=%.9f;\n")
        TEXT("const float ripple5Phase=%.9f;\n")
        TEXT("const float ripple7Phase=%.9f;\n")
        TEXT("float2 worldCm=WorldPosition.xy;\n")
        TEXT("float2 centeredCm=worldCm-coreCenterCm;\n")
        TEXT("float radialDistanceCm=length(centeredCm);\n")
        TEXT("if(radialDistanceCm<0.001)return 0.0;\n")
        TEXT("float parametricAngle=atan2(centeredCm.y/semiAxesCm.y,centeredCm.x/semiAxesCm.x);\n")
        TEXT("parametricAngle=parametricAngle<0.0?parametricAngle+twoPi:parametricAngle;\n")
        TEXT("int sectorIndex=min(edgeCount-1,(int)floor(parametricAngle/sector));\n")
        TEXT("float theta0=(float)sectorIndex*sector;\n")
        TEXT("float theta1=(float)(sectorIndex+1)*sector;\n")
        TEXT("float scale0=1.0+ripple3Amplitude*sin(3.0*theta0+ripple3Phase)+ripple5Amplitude*sin(5.0*theta0+ripple5Phase)+ripple7Amplitude*sin(7.0*theta0+ripple7Phase);\n")
        TEXT("float scale1=1.0+ripple3Amplitude*sin(3.0*theta1+ripple3Phase)+ripple5Amplitude*sin(5.0*theta1+ripple5Phase)+ripple7Amplitude*sin(7.0*theta1+ripple7Phase);\n")
        TEXT("float2 p0=scale0*float2(semiAxesCm.x*cos(theta0),semiAxesCm.y*sin(theta0));\n")
        TEXT("float2 p1=scale1*float2(semiAxesCm.x*cos(theta1),semiAxesCm.y*sin(theta1));\n")
        TEXT("float2 edge=p1-p0; float2 ray=centeredCm/radialDistanceCm;\n")
        TEXT("float denominator=ray.x*edge.y-ray.y*edge.x;\n")
        TEXT("if(abs(denominator)<=0.0001)return 1.0;\n")
        TEXT("float rawSegmentParameter=(p0.x*ray.y-p0.y*ray.x)/denominator;\n")
        TEXT("float boundaryRayCm=(p0.x*edge.y-p0.y*edge.x)/denominator;\n")
        TEXT("if(rawSegmentParameter < -segmentParameterEpsilon || rawSegmentParameter > 1.0+segmentParameterEpsilon || boundaryRayCm<=0.0)return 1.0;\n")
        TEXT("float signedInwardDistanceCm=boundaryRayCm-radialDistanceCm;\n")
        TEXT("float coverage=saturate(1.0+(signedInwardDistanceCm+opaqueCollarCm)/outwardFeatherCm);\n")
        TEXT("float2 stableCell=floor(worldCm/ditherCellCm);\n")
        TEXT("float dither=frac(sin(dot(stableCell,float2(12.9898,78.233)))*43758.5453);\n")
        TEXT("float coreMask=coverage>0.0?step(dither,coverage):0.0;\n")
        TEXT("return 1.0-coreMask;"),
        Center.X, Center.Y, Axes.X, Axes.Y,
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOpaqueCollarMeters() * 100.0,
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOutwardFeatherMeters() * 100.0,
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayDitherCellMeters() * 100.0,
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSegmentParameterEpsilon(),
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSplinePoints(),
        Amplitudes.X, Amplitudes.Y, Amplitudes.Z,
        Phases.X, Phases.Y, Phases.Z);
}

const FString ComplementMaskCode(BuildComplementMaskCode());

struct FSourceSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceSpec SourceSpecs[] = {
    {TEXT("Generated/SM_IPV5D_CopernicusDEM2021_TerrainProxy_Visual.obj"),
        3349537,
        TEXT("6057AE3C23287E9AFFED72B1C0842177A980BFE7AAB4089BF51E6FD7B2A621DF")},
    {TEXT("Generated/IstanaCopernicusDEM2021TerrainProxy.mtl"), 233,
        TEXT("AA98AABD87F409748B24AD344EFC43F68C3A0840CD670EC503A67597ACBEB720")},
    {TEXT("Generated/IstanaCopernicusDEM2021Terrain.manifest.json"), 11112,
        TEXT("06FD4D5A2F3988DAC311601F8255255C632A4FE60FC7D923F1F2C62CF08AFC5D")},
    {TEXT("copernicus_dem_2021.contract.json"), 5208,
        TEXT("ED96EEE1B070E09F33F469747DDC27F482018959CF763A7D2B20755D6794A415")},
    {TEXT("copernicus_dem_2021.native_fallback.contract.json"), 3739,
        TEXT("36F00FB7AC870EE9411A1830817FD41EA6AFCE05AD6214E3E15F68E960849D3E")},
};
static_assert(UE_ARRAY_COUNT(SourceSpecs) == 5);

FString ToolRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
             "CopernicusDEM2021")));
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
T* LoadExact(const FString& Path)
{
    T* Object = LoadObject<T>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

bool HashMatches(const FSourceSpec& Spec, FString& OutError)
{
    const FString Filename = SourcePath(Spec.RelativePath);
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != Spec.Bytes)
    {
        OutError = FString::Printf(
            TEXT("R29 Copernicus source byte pin failed for '%s': expected=%lld actual=%d."),
            *Filename, Spec.Bytes, Bytes.Num());
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
            nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() != Spec.Sha256)
    {
        OutError = TEXT("R29 Copernicus SHA-256 pin failed for '") +
            Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R29 Copernicus source admission requires WITH_SSL.");
    return false;
#endif
    return true;
}

bool ValidateAllSourcePins(FString& OutError)
{
    for (const FSourceSpec& Spec : SourceSpecs)
    {
        if (!HashMatches(Spec, OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool GatherAssets(TArray<FAssetData>& OutAssets, FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    Registry.Get().SearchAllAssets(true);
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("R29 Copernicus asset discovery did not settle.");
        return false;
    }
    return true;
}

bool ValidateRootRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherAssets(Assets, OutError))
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
        OutError = TEXT("R29 Copernicus isolated asset root must contain exactly one mesh and one material.");
        return false;
    }
    if (bRequireSaved)
    {
        for (const FString& Path : Expected)
        {
            UObject* Object = LoadObject<UObject>(nullptr, *Path);
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (!Package || Package->IsDirty() ||
                !FPackageName::DoesPackageExist(Package->GetName()))
            {
                OutError = TEXT("R29 Copernicus assets must be saved and clean.");
                return false;
            }
        }
    }
    return true;
}

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

void ConnectCustomInput(
    UMaterialExpressionCustom* Custom,
    const TCHAR* Name,
    UMaterialExpression* Expression)
{
    FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
    Input.InputName = Name;
    Input.Input.Connect(0, Expression);
}

UMaterial* CreateMaterial(IAssetTools& AssetTools, FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MaterialName, MaterialRoot, UMaterial::StaticClass(), Factory,
              FName(TEXT("TRIAD.CreateR29CopernicusTerrainFallbackAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Material || !Data || !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh R29 Copernicus material.");
        return nullptr;
    }

    auto* World = AddExpression<UMaterialExpressionWorldPosition>(
        Material, WorldPositionNodeId, -1000, -120);
    auto* Normal = AddExpression<UMaterialExpressionVertexNormalWS>(
        Material, VertexNormalNodeId, -1000, 40);
    auto* Appearance = AddExpression<UMaterialExpressionCustom>(
        Material, AppearanceNodeId, -700, -160);
    auto* Mask = AddExpression<UMaterialExpressionCustom>(
        Material, ComplementMaskNodeId, -700, 170);
    auto* Base = AddExpression<UMaterialExpressionComponentMask>(
        Material, BaseColorNodeId, -360, -170);
    auto* Rough = AddExpression<UMaterialExpressionComponentMask>(
        Material, RoughnessNodeId, -360, -30);
    auto* Metallic = AddExpression<UMaterialExpressionConstant>(
        Material, MetallicNodeId, -360, 110);
    auto* Specular = AddExpression<UMaterialExpressionConstant>(
        Material, SpecularNodeId, -360, 220);
    if (!World || !Normal || !Appearance || !Mask || !Base || !Rough ||
        !Metallic || !Specular)
    {
        OutError = TEXT("Could not author the exact eight-node R29 Copernicus material graph.");
        return nullptr;
    }

    World->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    Appearance->Description = AppearanceNodeId;
    Appearance->Code = AppearanceCode;
    Appearance->OutputType = CMOT_Float4;
    Appearance->Inputs.Reset();
    ConnectCustomInput(Appearance, TEXT("WorldPosition"), World);
    ConnectCustomInput(Appearance, TEXT("VertexNormal"), Normal);
    Mask->Description = ComplementMaskNodeId;
    Mask->Code = ComplementMaskCode;
    Mask->OutputType = CMOT_Float1;
    Mask->Inputs.Reset();
    ConnectCustomInput(Mask, TEXT("WorldPosition"), World);
    Base->R = true;
    Base->G = true;
    Base->B = true;
    Base->A = false;
    Base->Input.Connect(0, Appearance);
    Rough->R = false;
    Rough->G = false;
    Rough->B = false;
    Rough->A = true;
    Rough->Input.Connect(0, Appearance);
    Metallic->R = 0.0f;
    Specular->R = 0.18f;

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Masked;
    Material->OpacityMaskClipValue = 0.5f;
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
    for (auto& PhysicalMaterial : Material->PhysicalMaterialMap)
    {
        PhysicalMaterial = nullptr;
    }
    Material->RenderTracePhysicalMaterialOutputs.Reset();
    Material->NaniteOverrideMaterial.bEnableOverride = false;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Data->BaseColor.Connect(0, Base);
    Data->Roughness.Connect(0, Rough);
    Data->Metallic.Connect(0, Metallic);
    Data->Specular.Connect(0, Specular);
    Data->OpacityMask.Connect(0, Mask);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutError.Reset();
    return Material;
}

bool InputIs(const FExpressionInput& Input, const UMaterialExpression* Expected)
{
    return Expected && Input.Expression == Expected && Input.OutputIndex == 0;
}

bool ValidateMaterial(UMaterial* Material, FString& OutError)
{
    const UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    const UMaterialExpressionWorldPosition* World = nullptr;
    const UMaterialExpressionVertexNormalWS* Normal = nullptr;
    const UMaterialExpressionCustom* Appearance = nullptr;
    const UMaterialExpressionCustom* Mask = nullptr;
    const UMaterialExpressionComponentMask* Base = nullptr;
    const UMaterialExpressionComponentMask* Rough = nullptr;
    const UMaterialExpressionConstant* Metallic = nullptr;
    const UMaterialExpressionConstant* Specular = nullptr;
    if (Data)
    {
        for (const UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (Expression->Desc == WorldPositionNodeId)
                World = Cast<UMaterialExpressionWorldPosition>(Expression);
            else if (Expression->Desc == VertexNormalNodeId)
                Normal = Cast<UMaterialExpressionVertexNormalWS>(Expression);
            else if (Expression->Desc == AppearanceNodeId)
                Appearance = Cast<UMaterialExpressionCustom>(Expression);
            else if (Expression->Desc == ComplementMaskNodeId)
                Mask = Cast<UMaterialExpressionCustom>(Expression);
            else if (Expression->Desc == BaseColorNodeId)
                Base = Cast<UMaterialExpressionComponentMask>(Expression);
            else if (Expression->Desc == RoughnessNodeId)
                Rough = Cast<UMaterialExpressionComponentMask>(Expression);
            else if (Expression->Desc == MetallicNodeId)
                Metallic = Cast<UMaterialExpressionConstant>(Expression);
            else if (Expression->Desc == SpecularNodeId)
                Specular = Cast<UMaterialExpressionConstant>(Expression);
        }
    }
    const bool bAppearanceInputs = Appearance &&
        Appearance->Inputs.Num() == 2 &&
        Appearance->Inputs[0].InputName == FName(TEXT("WorldPosition")) &&
        Appearance->Inputs[1].InputName == FName(TEXT("VertexNormal")) &&
        InputIs(Appearance->Inputs[0].Input, World) &&
        InputIs(Appearance->Inputs[1].Input, Normal);
    const bool bMaskInput = Mask && Mask->Inputs.Num() == 1 &&
        Mask->Inputs[0].InputName == FName(TEXT("WorldPosition")) &&
        InputIs(Mask->Inputs[0].Input, World);
    bool bPhysicalMaterialMapEmpty = Material != nullptr;
    if (Material)
    {
        for (const auto& PhysicalMaterial : Material->PhysicalMaterialMap)
        {
            if (PhysicalMaterial)
            {
                bPhysicalMaterialMapEmpty = false;
                break;
            }
        }
    }
    if (!Material || Material->GetPathName() != MaterialObjectPath || !Data ||
        Data->ExpressionCollection.Expressions.Num() != 8 ||
        !World || !Normal || !Appearance || !Mask || !Base || !Rough ||
        !Metallic || !Specular || !bAppearanceInputs || !bMaskInput ||
        World->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        Appearance->Description != AppearanceNodeId ||
        Appearance->Code != AppearanceCode ||
        Appearance->OutputType != CMOT_Float4 ||
        Mask->Description != ComplementMaskNodeId ||
        Mask->Code != ComplementMaskCode || Mask->OutputType != CMOT_Float1 ||
        !Base->R || !Base->G || !Base->B || Base->A ||
        !InputIs(Base->Input, Appearance) || Rough->R || Rough->G || Rough->B ||
        !Rough->A || !InputIs(Rough->Input, Appearance) ||
        !FMath::IsNearlyZero(Metallic->R) ||
        !FMath::IsNearlyEqual(Specular->R, 0.18f, 0.000001f) ||
        !InputIs(Data->BaseColor, Base) || !InputIs(Data->Roughness, Rough) ||
        !InputIs(Data->Metallic, Metallic) ||
        !InputIs(Data->Specular, Specular) ||
        !InputIs(Data->OpacityMask, Mask) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked ||
        !FMath::IsNearlyEqual(Material->GetOpacityMaskClipValue(), 0.5f) ||
        Material->TwoSided || !Material->bTangentSpaceNormal ||
        Material->bUseMaterialAttributes || Material->bScreenSpaceReflections ||
        Material->bCastRayTracedShadows || Material->PhysMaterial ||
        Material->PhysMaterialMask ||
        !bPhysicalMaterialMapEmpty ||
        !Material->RenderTracePhysicalMaterialOutputs.IsEmpty() ||
        Data->Normal.Expression || Data->WorldPositionOffset.Expression ||
        Data->PixelDepthOffset.Expression || Data->Displacement.Expression)
    {
        OutError = TEXT("R29 Copernicus material lost its exact tropical response, exact complementary core mask, or no-physics/no-displacement contract.");
        return false;
    }
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
    Options->StaticMeshImportData->ImportUniformScale = RequiredImportScale;
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
        !FMath::IsNearlyEqual(Data->ImportUniformScale, RequiredImportScale) ||
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
        OutError = TEXT("R29 Copernicus import policy must remain exact uniform x100, explicit-normal, no-collision/no-lightmap, one-LOD and full Nanite.");
        return false;
    }
    return true;
}

bool NormalizeMesh(UStaticMesh* Mesh, UMaterial* Material, FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FMeshDescription* Description = Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || !Material || !Description ||
        Description->Vertices().Num() != ExpectedVertexCount ||
        Description->VertexInstances().Num() != ExpectedVertexInstanceCount ||
        Description->Triangles().Num() != ExpectedTriangleCount ||
        Description->Polygons().Num() != ExpectedTriangleCount ||
        Description->PolygonGroups().Num() != ExpectedMaterialCount ||
        Mesh->GetStaticMaterials().Num() != 1)
    {
        OutError = TEXT("R29 Copernicus normalization refused unexpected 129x129 source topology.");
        return false;
    }
    FStaticMaterial Slot = Mesh->GetStaticMaterials()[0];
    if (Slot.MaterialSlotName != FName(*ImportedSlotName) ||
        Slot.ImportedMaterialSlotName != FName(*ImportedSlotName))
    {
        OutError = TEXT("R29 Copernicus imported material slot drifted.");
        return false;
    }
    Slot.MaterialInterface = Material;
    Mesh->Modify();
    const TArray<FStaticMaterial> ExactMaterials = {Slot};
    Mesh->SetStaticMaterials(ExactMaterials);
    FStaticMeshSourceModel& Source = Mesh->GetSourceModel(0);
    Source.BuildSettings.BuildScale3D = FVector::OneVector;
    Source.BuildSettings.bGenerateLightmapUVs = false;
    Source.BuildSettings.bUseFullPrecisionUVs = true;
    Source.BuildSettings.bRecomputeNormals = false;
    Source.BuildSettings.bRecomputeTangents = true;
    Source.BuildSettings.bUseMikkTSpace = true;
    Source.BuildSettings.bRemoveDegenerates = false;
    Mesh->SetLightMapCoordinateIndex(0);
    Mesh->bGenerateMeshDistanceField = false;
    Mesh->NaniteSettings.bEnabled = true;
    Mesh->NaniteSettings.KeepPercentTriangles = 1.0f;
    Mesh->NaniteSettings.TrimRelativeError = 0.0f;
    Mesh->NaniteSettings.FallbackTarget =
        ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, 0);
    FMeshSectionInfo Original = Mesh->GetOriginalSectionInfoMap().Get(0, 0);
    Section.MaterialIndex = 0;
    Section.bEnableCollision = false;
    Section.bCastShadow = true;
    Section.bAffectDistanceFieldLighting = false;
    Original = Section;
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
    const auto* StaticImportData = Cast<UFbxStaticMeshImportData>(ImportData);
    const TArray<FString> Sources = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    const FMeshDescription* Description = Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    FString ImportError;
    if (!Mesh || Mesh->GetPathName() != MeshObjectPath || Sources.Num() != 1 ||
        !FPaths::IsSamePath(Sources[0], ObjSourcePath()) ||
        !ValidateImportSettings(StaticImportData, ImportError) ||
        Mesh->GetNumSourceModels() != 1 || !Description ||
        Description->Vertices().Num() != ExpectedVertexCount ||
        Description->VertexInstances().Num() != ExpectedVertexInstanceCount ||
        Description->Triangles().Num() != ExpectedTriangleCount ||
        Description->Polygons().Num() != ExpectedTriangleCount ||
        Description->PolygonGroups().Num() != ExpectedMaterialCount ||
        Mesh->GetStaticMaterials().Num() != 1 || !RenderData ||
        RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() != ExpectedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() != ExpectedSectionCount ||
        RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.
            GetNumTexCoords() != 1 ||
        !Mesh->NaniteSettings.bEnabled || !Mesh->HasValidNaniteData() ||
        Mesh->bGenerateMeshDistanceField || Mesh->bHasNavigationData ||
        Mesh->GetNavCollision())
    {
        OutError = ImportError.IsEmpty()
            ? TEXT("R29 Copernicus mesh lost its exact source, x100 import, 129x129 topology, one LOD/UV, Nanite, or no-navigation contract.")
            : ImportError;
        return false;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    const FStaticMaterial& Slot = Mesh->GetStaticMaterials()[0];
    const FStaticMeshSection& RenderSection =
        RenderData->LODResources[0].Sections[0];
    const FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, 0);
    const FMeshSectionInfo OriginalSection =
        Mesh->GetOriginalSectionInfoMap().Get(0, 0);
    const FBox Box = Mesh->GetBoundingBox();
    if (!Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        RenderSection.FirstIndex != 0 ||
        RenderSection.NumTriangles != ExpectedTriangleCount ||
        RenderSection.MaterialIndex != 0 ||
        Slot.MaterialSlotName != FName(*ImportedSlotName) ||
        Slot.ImportedMaterialSlotName != FName(*ImportedSlotName) ||
        Slot.MaterialInterface != Material || Section.MaterialIndex != 0 ||
        Section.bEnableCollision || !Section.bCastShadow ||
        Section.bAffectDistanceFieldLighting ||
        OriginalSection.MaterialIndex != Section.MaterialIndex ||
        OriginalSection.bEnableCollision != Section.bEnableCollision ||
        OriginalSection.bCastShadow != Section.bCastShadow ||
        OriginalSection.bAffectDistanceFieldLighting !=
            Section.bAffectDistanceFieldLighting ||
        !Box.Min.Equals(ExpectedBoundsMin, 0.02) ||
        !Box.Max.Equals(ExpectedBoundsMax, 0.02))
    {
        OutError = FString::Printf(
            TEXT("R29 Copernicus post-import guard failed: expected one complete material-zero render section, matching current/original no-collision policy, exact x100 XY +/-100000cm and Z [-3030.3865,119.3868]cm; actual min=%s max=%s."),
            *Box.Min.ToString(), *Box.Max.ToString());
        return false;
    }
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateAllSourcePins(OutReport) ||
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
        TEXT("R29_COPERNICUS_TERRAIN_ASSETS_VALID assets=2 sourcePins=5 objBytes=3349537 objSha256=%s vertices=16641 normals=16641 uv0=16641 triangles=32768 importUniformScale=100 boundsXcm=[-100000,100000] boundsYcm=[-100000,100000] boundsZcm=[-3030.3865,119.3868] materialGraph=exactEightNodeTropicalResponseAndComplementary64EdgeMask collar=50m feather=8m dither=0.25m cesiumPreferred=true noCollision=true noNavigation=true noSensorRfGeospatialSurveyAbsoluteHeightOrBareEarthAuthority=true"),
        SourceSpecs[0].Sha256);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetMeshObjectPath() { return MeshObjectPath; }
const FString& GetMaterialObjectPath() { return MaterialObjectPath; }

bool CreateFreshAssets(
    UStaticMesh*& OutMesh,
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMesh = nullptr;
    OutMaterial = nullptr;
    FString Existing;
    if (ValidateAssets(Existing))
    {
        OutMesh = LoadExact<UStaticMesh>(MeshObjectPath);
        OutMaterial = LoadExact<UMaterial>(MaterialObjectPath);
        OutError = Existing;
        return OutMesh && OutMaterial;
    }
    if (!ValidateAllSourcePins(OutError))
    {
        return false;
    }
    TArray<FAssetData> ExistingAssets;
    if (!GatherAssets(ExistingAssets, OutError) || !ExistingAssets.IsEmpty())
    {
        OutError = TEXT("R29 Copernicus import refuses a partial, dirty, or non-empty isolated asset root.");
        return false;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    OutMaterial = CreateMaterial(AssetTools, OutError);
    UAssetImportTask* Task = MakeImportTask();
    if (!OutMaterial || !Task)
    {
        return false;
    }
    FString ImportPolicyError;
    if (!ValidateImportSettings(
            Cast<UFbxImportUI>(Task->Options)->StaticMeshImportData,
            ImportPolicyError))
    {
        OutError = ImportPolicyError;
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
        !ValidateMaterial(OutMaterial, OutError) ||
        !ValidateMesh(OutMesh, OutMaterial, OutError) ||
        !ValidateInternal(false, Existing))
    {
        TArray<FAssetData> Registered;
        FString Ignored;
        if (GatherAssets(Registered, Ignored))
        {
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
            if (!Disposable.IsEmpty())
            {
                ObjectTools::DeleteObjectsUnchecked(Disposable);
            }
        }
        OutMesh = nullptr;
        OutMaterial = nullptr;
        if (OutError.IsEmpty())
        {
            OutError = Existing;
        }
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    TArray<UObject*> FreshAssets = {OutMaterial, OutMesh};
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAssets(FreshAssets, false) ||
        !ValidateInternal(true, Existing))
    {
        OutError = TEXT("R29 Copernicus assets could not be saved and cold-validated: ") + Existing;
        OutMesh = nullptr;
        OutMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory
