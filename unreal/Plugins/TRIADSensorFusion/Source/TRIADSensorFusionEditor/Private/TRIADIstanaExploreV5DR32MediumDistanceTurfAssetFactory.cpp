#include "TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR29VegetationAssetFactory.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
const FString AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/MediumDistanceTurfR32"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));

const FString R29RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_R29_18M_65M_MULTI_SCALE_ROUGHNESS"));
const FString R32RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_R32_20M_65M_CONTINUOUS_TURF_ROUGHNESS"));

// The response is deliberately continuous and low frequency. Two rotated,
// smooth value-noise fields retain a restrained organic read after blade
// widths become subpixel without introducing mowing stripes or checkerboards.
// It is fully band-limited by screen-space world-position derivatives and is
// a render-only approximation, not physical or site-measured material truth.
const FString R32RoughnessCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1800.0,6500.0,distanceCm);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float blade=frac(saturate(BladeUV.x)*31.416+saturate(Random01)*0.7548777);\n")
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float2 broadP=float2(0.819152*worldM.x-0.573576*worldM.y,0.573576*worldM.x+0.819152*worldM.y);\n")
    TEXT("broadP=(broadP+float2(31.7,-19.3))/11.0;\n")
    TEXT("float2 broadI=floor(broadP); float2 broadF=frac(broadP);\n")
    TEXT("broadF=broadF*broadF*(3.0-2.0*broadF);\n")
    TEXT("float4 broadHash=frac(float4(dot(broadI,float2(127.1,311.7)),dot(broadI+float2(1,0),float2(127.1,311.7)),dot(broadI+float2(0,1),float2(127.1,311.7)),dot(broadI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
    TEXT("broadHash=frac(broadHash*(broadHash+33.33)*(broadHash+broadHash+17.17));\n")
    TEXT("float broadNoise=lerp(lerp(broadHash.x,broadHash.y,broadF.x),lerp(broadHash.z,broadHash.w,broadF.x),broadF.y);\n")
    TEXT("float2 mesoP=float2(0.342020*worldM.x+0.939693*worldM.y,-0.939693*worldM.x+0.342020*worldM.y);\n")
    TEXT("mesoP=(mesoP+float2(-7.1,13.9))/3.4;\n")
    TEXT("float2 mesoI=floor(mesoP); float2 mesoF=frac(mesoP);\n")
    TEXT("mesoF=mesoF*mesoF*(3.0-2.0*mesoF);\n")
    TEXT("float4 mesoHash=frac(float4(dot(mesoI,float2(269.5,183.3)),dot(mesoI+float2(1,0),float2(269.5,183.3)),dot(mesoI+float2(0,1),float2(269.5,183.3)),dot(mesoI+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
    TEXT("mesoHash=frac(mesoHash*(mesoHash+19.19)*(mesoHash+mesoHash+7.77));\n")
    TEXT("float mesoNoise=lerp(lerp(mesoHash.x,mesoHash.y,mesoF.x),lerp(mesoHash.z,mesoHash.w,mesoF.x),mesoF.y);\n")
    TEXT("float organic=saturate(0.68*broadNoise+0.32*mesoNoise);\n")
    TEXT("float rootMask=1.0-smoothstep(0.05,0.32,h);\n")
    TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
    TEXT("float nearRough=BaseRoughness+lerp(-0.030,0.034,blade)+lerp(-0.022,0.024,organic)+0.032*rootMask+0.016*cutMask;\n")
    TEXT("float farRough=0.82+lerp(-0.026,0.030,organic);\n")
    TEXT("float mediumGate=smoothstep(1400.0,2000.0,distanceCm)*(1.0-smoothstep(6500.0,7600.0,distanceCm));\n")
    TEXT("float footprintM=max(length(ddx(worldM)),length(ddy(worldM)));\n")
    TEXT("float bandLimit=1.0-smoothstep(2.0,5.5,footprintM);\n")
    TEXT("float baseRough=lerp(farRough,nearRough,detail);\n")
    TEXT("return clamp(baseRough+mediumGate*bandLimit*lerp(-0.018,0.020,organic),0.66,0.90);"));

struct FMaterialSpec
{
    const TCHAR* Name;
    const TCHAR* SourceObjectPath;
    const TCHAR* SourceColorDescription;
    const TCHAR* TargetColorDescription;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("M_IPV5D_R32_Turf_Manicured"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_Manicured.M_IPV5D_R29_Turf_Manicured"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_MANICURED_HEALTHY_TROPICAL"),
     TEXT("TRIAD_EXPLORE_V5D_R32_TURF_MANICURED_CONTINUOUS_MACRO_MESO_COLOUR")},
    {TEXT("M_IPV5D_R32_Turf_Humid"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_Humid.M_IPV5D_R29_Turf_Humid"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_HUMID_HEALTHY_TROPICAL"),
     TEXT("TRIAD_EXPLORE_V5D_R32_TURF_HUMID_CONTINUOUS_MACRO_MESO_COLOUR")},
    {TEXT("M_IPV5D_R32_Turf_Shade"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_Shade.M_IPV5D_R29_Turf_Shade"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_SHADE_HEALTHY_TROPICAL"),
     TEXT("TRIAD_EXPLORE_V5D_R32_TURF_SHADE_CONTINUOUS_MACRO_MESO_COLOUR")},
    {TEXT("M_IPV5D_R32_Turf_DryEdge"),
     TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_DryEdge.M_IPV5D_R29_Turf_DryEdge"),
     TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_DRY_EDGE_HEALTHY_TROPICAL"),
     TEXT("TRIAD_EXPLORE_V5D_R32_TURF_DRY_EDGE_CONTINUOUS_MACRO_MESO_COLOUR")}};

static_assert(UE_ARRAY_COUNT(MaterialSpecs) == 4);
constexpr int32 ExpectedExpressionCount = 32;

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

FString MaterialObjectPath(const FMaterialSpec& Spec)
{
    return ObjectPath(MaterialRoot, Spec.Name);
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& Path)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *Path);
    return IsValid(Object) && Object->GetPathName() == Path ? Object : nullptr;
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

bool ReplaceExactlyOnce(
    FString& InOutCode,
    const TCHAR* From,
    const TCHAR* To)
{
    return InOutCode.ReplaceInline(
               From,
               To,
               ESearchCase::CaseSensitive) == 1;
}

bool BuildR32ColorCode(
    const FString& R29Code,
    FString& OutCode,
    FString& OutError)
{
    OutCode = R29Code;
    const bool bReplaced = ReplaceExactlyOnce(
        OutCode,
        TEXT("result=lerp(finalLuma.xxx,result,0.94);\nreturn saturate(result);"),
        TEXT("result=lerp(finalLuma.xxx,result,0.94);\n")
        TEXT("float r32MediumGate=smoothstep(1400.0,2000.0,distanceCm)*(1.0-smoothstep(6500.0,7600.0,distanceCm));\n")
        TEXT("float2 r32BroadP=float2(0.819152*worldM.x-0.573576*worldM.y,0.573576*worldM.x+0.819152*worldM.y);\n")
        TEXT("r32BroadP=(r32BroadP+float2(31.7,-19.3))/11.0;\n")
        TEXT("float2 r32BroadI=floor(r32BroadP); float2 r32BroadF=frac(r32BroadP);\n")
        TEXT("r32BroadF=r32BroadF*r32BroadF*(3.0-2.0*r32BroadF);\n")
        TEXT("float4 r32BroadHash=frac(float4(dot(r32BroadI,float2(127.1,311.7)),dot(r32BroadI+float2(1,0),float2(127.1,311.7)),dot(r32BroadI+float2(0,1),float2(127.1,311.7)),dot(r32BroadI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("r32BroadHash=frac(r32BroadHash*(r32BroadHash+33.33)*(r32BroadHash+r32BroadHash+17.17));\n")
        TEXT("float r32BroadNoise=lerp(lerp(r32BroadHash.x,r32BroadHash.y,r32BroadF.x),lerp(r32BroadHash.z,r32BroadHash.w,r32BroadF.x),r32BroadF.y);\n")
        TEXT("float2 r32MesoP=float2(0.342020*worldM.x+0.939693*worldM.y,-0.939693*worldM.x+0.342020*worldM.y);\n")
        TEXT("r32MesoP=(r32MesoP+float2(-7.1,13.9))/3.4;\n")
        TEXT("float2 r32MesoI=floor(r32MesoP); float2 r32MesoF=frac(r32MesoP);\n")
        TEXT("r32MesoF=r32MesoF*r32MesoF*(3.0-2.0*r32MesoF);\n")
        TEXT("float4 r32MesoHash=frac(float4(dot(r32MesoI,float2(269.5,183.3)),dot(r32MesoI+float2(1,0),float2(269.5,183.3)),dot(r32MesoI+float2(0,1),float2(269.5,183.3)),dot(r32MesoI+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
        TEXT("r32MesoHash=frac(r32MesoHash*(r32MesoHash+19.19)*(r32MesoHash+r32MesoHash+7.77));\n")
        TEXT("float r32MesoNoise=lerp(lerp(r32MesoHash.x,r32MesoHash.y,r32MesoF.x),lerp(r32MesoHash.z,r32MesoHash.w,r32MesoF.x),r32MesoF.y);\n")
        TEXT("float r32Organic=saturate(0.68*r32BroadNoise+0.32*r32MesoNoise);\n")
        TEXT("float r32Signed=r32Organic*2.0-1.0;\n")
        TEXT("float r32FootprintM=max(length(ddx(worldM)),length(ddy(worldM)));\n")
        TEXT("float r32BandLimit=1.0-smoothstep(2.0,5.5,r32FootprintM);\n")
        TEXT("float r32Response=r32MediumGate*r32BandLimit;\n")
        TEXT("float3 r32Tint=lerp(float3(0.955,1.025,0.970),float3(1.045,0.990,0.955),r32Organic);\n")
        TEXT("result*=lerp(float3(1.0,1.0,1.0),r32Tint,0.62*r32Response);\n")
        TEXT("result*=1.0+0.090*r32Response*r32Signed;\n")
        TEXT("return saturate(result);"));
    if (!bReplaced || OutCode == R29Code)
    {
        OutCode.Reset();
        OutError = TEXT("R32 turf colour refinement refused a non-canonical R29 predecessor code body.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CustomMetadataEqual(
    const UMaterialExpressionCustom* Source,
    const UMaterialExpressionCustom* Target)
{
    return Source && Target &&
        Source->OutputType == Target->OutputType &&
        Source->Inputs.Num() == Target->Inputs.Num() &&
        Source->AdditionalOutputs.Num() == Target->AdditionalOutputs.Num() &&
        Source->AdditionalDefines.Num() == Target->AdditionalDefines.Num() &&
        Source->IncludeFilePaths.Num() == Target->IncludeFilePaths.Num();
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
    int32 SourceRoughnessCount = 0;
    int32 TargetRoughnessCount = 0;
    int32 SourceColorCount = 0;
    int32 TargetColorCount = 0;
    UMaterialExpressionCustom* SourceRoughness = FindCustom(
        Source, R29RoughnessDescription, SourceRoughnessCount);
    UMaterialExpressionCustom* TargetRoughness = FindCustom(
        Target, R32RoughnessDescription, TargetRoughnessCount);
    UMaterialExpressionCustom* SourceColor = FindCustom(
        Source, Spec.SourceColorDescription, SourceColorCount);
    UMaterialExpressionCustom* TargetColor = FindCustom(
        Target, Spec.TargetColorDescription, TargetColorCount);
    FString ExpectedColorCode;
    FString ColorError;
    if (!Source || !Target || !SourceData || !TargetData ||
        !BuildR32ColorCode(
            SourceColor ? SourceColor->Code : FString(),
            ExpectedColorCode,
            ColorError) ||
        Source->GetPathName() != Spec.SourceObjectPath ||
        Target->GetPathName() != ExpectedTargetPath ||
        SourceData->ExpressionCollection.Expressions.Num() !=
            ExpectedExpressionCount ||
        TargetData->ExpressionCollection.Expressions.Num() !=
            ExpectedExpressionCount ||
        SourceRoughnessCount != 1 || TargetRoughnessCount != 1 ||
        SourceColorCount != 1 || TargetColorCount != 1 ||
        !SourceRoughness || !TargetRoughness ||
        !SourceColor || !TargetColor ||
        TargetRoughness->Code != R32RoughnessCode ||
        TargetColor->Code != ExpectedColorCode ||
        !CustomMetadataEqual(SourceRoughness, TargetRoughness) ||
        !CustomMetadataEqual(SourceColor, TargetColor) ||
        SourceData->BaseColor.Expression != SourceColor ||
        TargetData->BaseColor.Expression != TargetColor ||
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
                 FPackageName::ObjectPathToPackageName(
                     ExpectedTargetPath)))))
    {
        OutError = FString::Printf(
            TEXT("R32 material '%s' is not the exact saved two-node derivative of its clean R29 source. %s"),
            Spec.Name,
            *ColorError);
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
            OutError = TEXT("R32 turf material changed expression topology.");
            return false;
        }
        const UMaterialExpressionCustom* SourceCustom =
            Cast<UMaterialExpressionCustom>(SourceExpression);
        const UMaterialExpressionCustom* TargetCustom =
            Cast<UMaterialExpressionCustom>(TargetExpression);
        const bool bAllowed =
            SourceCustom == SourceRoughness || SourceCustom == SourceColor;
        if (SourceCustom && !bAllowed &&
            (!TargetCustom ||
             SourceCustom->Description != TargetCustom->Description ||
             SourceCustom->Code != TargetCustom->Code ||
             !CustomMetadataEqual(SourceCustom, TargetCustom)))
        {
            OutError = TEXT("R32 turf material changed a forbidden custom-expression body.");
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
        OutError = TEXT("R32 material destination is not fresh: ") +
            PackagePath;
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
    int32 RoughnessCount = 0;
    int32 ColorCount = 0;
    UMaterialExpressionCustom* Roughness = FindCustom(
        Target, R29RoughnessDescription, RoughnessCount);
    UMaterialExpressionCustom* Color = FindCustom(
        Target, Spec.SourceColorDescription, ColorCount);
    FString RefinedColorCode;
    if (!Target || Target->GetPathName() != MaterialObjectPath(Spec) ||
        RoughnessCount != 1 || ColorCount != 1 || !Roughness || !Color ||
        !BuildR32ColorCode(Color->Code, RefinedColorCode, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not duplicate the exact R29 grass graph into R32.");
        }
        return nullptr;
    }

    Target->Modify();
    Target->PreEditChange(nullptr);
    Roughness->Modify();
    Roughness->Description = R32RoughnessDescription;
    Roughness->Code = R32RoughnessCode;
    Color->Modify();
    Color->Description = Spec.TargetColorDescription;
    Color->Code = RefinedColorCode;
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

bool ValidateInternal(FString& OutReport)
{
    FString R29Report;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::
            ValidateAssets(R29Report))
    {
        OutReport = TEXT("R32 turf materials require the exact clean seven-package R29 vegetation source: ") +
            R29Report;
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        if (!ValidateMaterialPair(
                Spec,
                LoadExact<UMaterial>(Spec.SourceObjectPath),
                LoadExact<UMaterial>(MaterialObjectPath(Spec)),
                true,
                OutReport))
        {
            return false;
        }
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_MATERIALS_VALID exactSavedPackages=4 exactCleanR29SourcePackages=7 isolatedDerivatives=4 expressionCountPerMaterial=32 allowedGraphDeltas=baseColourBodyAndLabel,roughnessBodyAndLabel continuousBroadScaleMeters=11.0 continuousMesoScaleMeters=3.4 mediumResponseMeters=20,65 responseFadeOutMeters=65,76 derivativeBandLimitFootprintMeters=2.0,5.5 texturesAdded=0 meshGeometryModified=false opacityModified=false wpoModified=false normalResponseModified=false sourcePackagesModified=false mapsSaved=0 collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false pbrTruthClaimed=false siteMeasurementClaimed=false visualCaptureAccepted=false captureRevalidationRequired=true");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory
{
const FString& GetAssetRootPath()
{
    return AssetRoot;
}

const TArray<FString>& GetExpectedMaterialObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result;
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
    FString R29Report;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::
            ValidateAssets(R29Report))
    {
        OutError = TEXT("R32 material creation refused non-canonical R29 sources: ") +
            R29Report;
        return false;
    }
    int32 ExistingCount = 0;
    for (const FString& Path : GetExpectedMaterialObjectPaths())
    {
        ExistingCount += FindObject<UObject>(nullptr, *Path) ||
            FPackageName::DoesPackageExist(
                FPackageName::ObjectPathToPackageName(Path))
            ? 1
            : 0;
    }
    if (ExistingCount == UE_ARRAY_COUNT(MaterialSpecs))
    {
        FString ExistingReport;
        if (!ValidateInternal(ExistingReport))
        {
            OutError = TEXT("Existing complete R32 turf-material roster failed exact validation: ") +
                ExistingReport;
            return false;
        }
        for (const FString& Path : GetExpectedMaterialObjectPaths())
        {
            OutAssets.Add(LoadExact<UObject>(Path));
        }
        OutError = ExistingReport;
        return true;
    }
    if (ExistingCount != 0)
    {
        OutError = FString::Printf(
            TEXT("R32 material creation refuses a partial isolated root: existing=%d expected=0-or-4."),
            ExistingCount);
        return false;
    }

    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Target = DuplicateMaterial(
            Spec,
            LoadExact<UMaterial>(Spec.SourceObjectPath),
            OutError);
        if (!Target)
        {
            return false;
        }
        OutAssets.Add(Target);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem || OutAssets.Num() != UE_ARRAY_COUNT(MaterialSpecs) ||
        !AssetSubsystem->SaveLoadedAssets(OutAssets, false))
    {
        OutError = TEXT("R32 turf refinement failed to save the exact four-material derivative roster.");
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Report;
    if (!ValidateInternal(Report))
    {
        OutError = TEXT("Fresh R32 turf material assets failed exact validation: ") +
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
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets{};
    if (!ValidateInternal(OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DR29VegetationAssets R29Assets;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::
            LoadValidatedRuntimeContract(R29Assets, OutError) ||
        R29Assets.GrassMeshVariants.Num() != 3)
    {
        OutError = TEXT("R32 turf could not load exact R29 mesh sources: ") +
            OutError;
        return false;
    }
    OutAssets.GrassMeshVariants = R29Assets.GrassMeshVariants;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        OutAssets.GrassProfileMaterials.Add(
            LoadExact<UMaterialInterface>(MaterialObjectPath(Spec)));
    }
    if (!ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            ValidateAssetRoster(OutAssets, OutError))
    {
        OutAssets = FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets{};
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory
