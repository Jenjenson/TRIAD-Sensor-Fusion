#include "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
const FString GrassCarrierMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5B/Vegetation/Meshes/SM_IPV5B_BermudaTurfCluster.SM_IPV5B_BermudaTurfCluster"));
const FString SourceGrassMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Manicured.M_IPV5D_Turf_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Humid.M_IPV5D_Turf_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Shade.M_IPV5D_Turf_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_DryEdge.M_IPV5D_Turf_DryEdge")};
const FString GrassMaterialAssetNames[] = {
    TEXT("M_IPV5D_LandmarkTurf_R27_Manicured"),
    TEXT("M_IPV5D_LandmarkTurf_R27_Humid"),
    TEXT("M_IPV5D_LandmarkTurf_R27_Shade"),
    TEXT("M_IPV5D_LandmarkTurf_R27_DryEdge")};
const FString GrassMaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/"));
const FString SourceStableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_STABLE_SPATIAL_VISIBILITY_20M_28M"));
const FString R27StableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R27_SINGLE_GATE_VISIBILITY_65M_90M"));
const FString SourceStableVisibilityCode(
    TEXT("float instanceVisibility=saturate(InstanceFade);\n")
    TEXT("float instanceGate=step(saturate(Random01),instanceVisibility);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float distanceVisibility=1.0-smoothstep(2000.0,2800.0,distanceCm);\n")
    TEXT("float2 cell=floor(WorldPosition.xy*0.02);\n")
    TEXT("float distanceSeed=frac(saturate(Random01)*0.754877666+dot(cell,float2(0.1031,0.11369)));\n")
    TEXT("distanceSeed=frac(distanceSeed*(distanceSeed+33.33)*(distanceSeed+distanceSeed+17.17));\n")
    TEXT("float distanceGate=step(distanceSeed,distanceVisibility);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("float calibrationGate=saturate(CalibrationRevision);\n")
    TEXT("return instanceGate*distanceGate*revisionGate*calibrationGate;"));
const FString R27StableVisibilityCode(
    TEXT("float componentVisibility=saturate(InstanceFade);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float calibratedVisibility=1.0-smoothstep(6500.0,9000.0,distanceCm);\n")
    TEXT("float effectiveVisibility=max(componentVisibility,calibratedVisibility);\n")
    TEXT("float stableGate=step(saturate(Random01),effectiveVisibility);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("float calibrationGate=saturate(CalibrationRevision);\n")
    TEXT("return stableGate*revisionGate*calibrationGate;"));
const FString R28SourceGrassMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Manicured.M_IPV5D_LandmarkTurf_R27_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Humid.M_IPV5D_LandmarkTurf_R27_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Shade.M_IPV5D_LandmarkTurf_R27_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_DryEdge.M_IPV5D_LandmarkTurf_R27_DryEdge")};
const FString R28GrassMaterialAssetNames[] = {
    TEXT("M_IPV5D_LandmarkTurf_R28_Manicured"),
    TEXT("M_IPV5D_LandmarkTurf_R28_Humid"),
    TEXT("M_IPV5D_LandmarkTurf_R28_Shade"),
    TEXT("M_IPV5D_LandmarkTurf_R28_DryEdge")};
const FString R28GrassMaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/"));
const FString R28StableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_SINGLE_GATE_VISIBILITY_65M_90M"));
// R28 changes the node label so provenance is explicit, but retains R27's
// exact accepted 65--90 m single visibility gate and component-fade merge.
const FString R28StableVisibilityCode(R27StableVisibilityCode);
const FString R23BGrassColorDescriptions[] = {
    TEXT("TRIAD_EXPLORE_V5D_GRASS_WORLD_STOCHASTIC_MANICURED_R23B"),
    TEXT("TRIAD_EXPLORE_V5D_GRASS_WORLD_STOCHASTIC_HUMID_R23B"),
    TEXT("TRIAD_EXPLORE_V5D_GRASS_WORLD_STOCHASTIC_SHADE_R23B"),
    TEXT("TRIAD_EXPLORE_V5D_GRASS_WORLD_STOCHASTIC_DRY_EDGE_R23B")};
const FString R28GrassColorDescriptions[] = {
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_MANICURED_HEALTHY_TROPICAL"),
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_HUMID_HEALTHY_TROPICAL"),
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_SHADE_HEALTHY_TROPICAL"),
    TEXT("TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_DRY_EDGE_HEALTHY_TROPICAL")};
static_assert(
    UE_ARRAY_COUNT(R28SourceGrassMaterialPaths) == 4 &&
        UE_ARRAY_COUNT(R28GrassMaterialAssetNames) == 4 &&
        UE_ARRAY_COUNT(R23BGrassColorDescriptions) == 4 &&
        UE_ARRAY_COUNT(R28GrassColorDescriptions) == 4,
    "R28 landmark material source, target and blade-colour rosters must stay exact and one-to-one.");
const FString UmbrellaTreeMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"));
const FString DomeTreeMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"));
const FString HighForkTreeMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"));
const FString ShrubMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Shrub04_A.SM_IPV4_Shrub04_A"));
const FString ShrubMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Shrub04_Wind.M_IPV4_Shrub04_Wind"));
const FString UnderstoreyMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Calathea_D.SM_IPV4_Calathea_D"));
const FString UnderstoreyMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Calathea_Wind.M_IPV4_Calathea_Wind"));
const FString FlowerMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Periwinkle06_F.SM_IPV4_Periwinkle06_F"));
const FString FlowerMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Periwinkle_Wind.M_IPV4_Periwinkle_Wind"));

template <typename TObjectType>
TObjectType* LoadExact(const FString& ObjectPath)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *ObjectPath);
    return IsValid(Object) && Object->GetPathName() == ObjectPath
        ? Object
        : nullptr;
}

FString GrassMaterialPackagePath(int32 ProfileIndex)
{
    return GrassMaterialRoot + GrassMaterialAssetNames[ProfileIndex];
}

FString GrassMaterialObjectPath(int32 ProfileIndex)
{
    const FString PackagePath = GrassMaterialPackagePath(ProfileIndex);
    return PackagePath + TEXT(".") + GrassMaterialAssetNames[ProfileIndex];
}

FString R28GrassMaterialPackagePath(int32 ProfileIndex)
{
    return R28GrassMaterialRoot + R28GrassMaterialAssetNames[ProfileIndex];
}

FString R28GrassMaterialObjectPath(int32 ProfileIndex)
{
    const FString PackagePath = R28GrassMaterialPackagePath(ProfileIndex);
    return PackagePath + TEXT(".") +
        R28GrassMaterialAssetNames[ProfileIndex];
}

UMaterialExpressionCustom* FindExactCustomNode(
    UMaterial* Material,
    const FString& Description,
    int32& OutCount)
{
    OutCount = 0;
    UMaterialExpressionCustom* Result = nullptr;
    const auto* Data = Material ? Material->GetEditorOnlyData() : nullptr;
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

bool BuildR28GrassColorCode(
    const FString& R23BCode,
    FString& OutCode,
    FString& OutError)
{
    OutCode = R23BCode;
    const bool bExactReplacements =
        ReplaceExactlyOnce(
            OutCode,
            TEXT("nearDetailedColor=lerp(nearDetailedColor,dryColor,0.76*dryNear*smoothstep(0.18,0.82,h));"),
            TEXT("nearDetailedColor=lerp(nearDetailedColor,dryColor,0.26*dryNear*smoothstep(0.18,0.82,h));")) &&
        ReplaceExactlyOnce(
            OutCode,
            TEXT("nearDetailedColor=lerp(nearDetailedColor,thatchColor,0.88*thatchBand*thatchNear);"),
            TEXT("nearDetailedColor=lerp(nearDetailedColor,thatchColor,0.46*thatchBand*thatchNear);")) &&
        ReplaceExactlyOnce(
            OutCode,
            TEXT("float3 farColor=lerp(body,dryColor,0.48*dryFraction);"),
            TEXT("float3 farColor=lerp(body,dryColor,0.12*dryFraction);")) &&
        ReplaceExactlyOnce(
            OutCode,
            TEXT("farColor=lerp(farColor,thatchColor,0.060*thatchFraction);"),
            TEXT("farColor=lerp(farColor,thatchColor,0.025*thatchFraction);")) &&
        ReplaceExactlyOnce(
            OutCode,
            TEXT("result*=localVariation*bladeTint*float3(1.06,0.94,0.92);"),
            TEXT("result*=localVariation*bladeTint*float3(0.90,1.12,0.90);")) &&
        ReplaceExactlyOnce(
            OutCode,
            TEXT("result=lerp(finalLuma.xxx,result,0.90);"),
            TEXT("result=lerp(finalLuma.xxx,result,0.94);"));
    if (!bExactReplacements || OutCode == R23BCode)
    {
        OutCode.Reset();
        OutError = TEXT("R28 blade-colour calibration refused a non-canonical R23B predecessor code body.");
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterialExpressionCustom* FindStableVisibilityNode(
    UMaterial* Material,
    bool bR27,
    int32& OutSourceCount,
    int32& OutR27Count)
{
    OutSourceCount = 0;
    OutR27Count = 0;
    UMaterialExpressionCustom* Result = nullptr;
    const auto* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    for (UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionCustom* Custom =
            Cast<UMaterialExpressionCustom>(Expression);
        if (!Custom)
        {
            continue;
        }
        if (Custom->Description == SourceStableVisibilityDescription)
        {
            ++OutSourceCount;
            if (!bR27)
            {
                Result = Custom;
            }
        }
        if (Custom->Description == R27StableVisibilityDescription)
        {
            ++OutR27Count;
            if (bR27)
            {
                Result = Custom;
            }
        }
    }
    return Result;
}

bool HasExactStableInputs(const UMaterialExpressionCustom* Stable)
{
    const FName ExpectedNames[] = {
        TEXT("InstanceFade"),
        TEXT("Random01"),
        TEXT("WorldPosition"),
        TEXT("CameraPosition"),
        TEXT("MaterialRevision"),
        TEXT("CalibrationRevision")};
    if (!Stable || Stable->Inputs.Num() != UE_ARRAY_COUNT(ExpectedNames))
    {
        return false;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedNames); ++Index)
    {
        if (Stable->Inputs[Index].InputName != ExpectedNames[Index] ||
            !Stable->Inputs[Index].Input.Expression)
        {
            return false;
        }
    }
    return Stable->AdditionalOutputs.IsEmpty() &&
        Stable->AdditionalDefines.IsEmpty() &&
        Stable->IncludeFilePaths.IsEmpty();
}

bool ValidateSourceGrassMaterial(
    UMaterial* Source,
    int32 ProfileIndex,
    FString& OutError)
{
    int32 SourceLegacyCount = 0;
    int32 SourceR27Count = 0;
    UMaterialExpressionCustom* SourceStable = FindStableVisibilityNode(
        Source, false, SourceLegacyCount, SourceR27Count);
    const auto* SourceData = Source ? Source->GetEditorOnlyData() : nullptr;
    if (!Source || !SourceData ||
        Source->GetPathName() != SourceGrassMaterialPaths[ProfileIndex] ||
        SourceLegacyCount != 1 || SourceR27Count != 0 ||
        !SourceStable ||
        SourceStable->Code != SourceStableVisibilityCode ||
        SourceStable->OutputType != CMOT_Float1 ||
        !HasExactStableInputs(SourceStable) ||
        SourceData->OpacityMask.Expression != SourceStable ||
        SourceData->OpacityMask.OutputIndex != 0 ||
        SourceData->ExpressionCollection.Expressions.Num() != 32 ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutError = FString::Printf(
            TEXT("Protected R23B grass profile %d is not the exact clean 20--28 m source graph."),
            ProfileIndex);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMaterialPair(
    UMaterial* Source,
    UMaterial* Target,
    int32 ProfileIndex,
    bool bRequireSaved,
    FString& OutError)
{
    if (!ValidateSourceGrassMaterial(Source, ProfileIndex, OutError))
    {
        return false;
    }
    int32 SourceLegacyCount = 0;
    int32 SourceR27Count = 0;
    UMaterialExpressionCustom* SourceStable = FindStableVisibilityNode(
        Source, false, SourceLegacyCount, SourceR27Count);
    int32 TargetLegacyCount = 0;
    int32 TargetR27Count = 0;
    UMaterialExpressionCustom* TargetStable = FindStableVisibilityNode(
        Target, true, TargetLegacyCount, TargetR27Count);
    const auto* SourceData = Source ? Source->GetEditorOnlyData() : nullptr;
    const auto* TargetData = Target ? Target->GetEditorOnlyData() : nullptr;
    if (!Source || !Target || !SourceData || !TargetData ||
        Source->GetPathName() != SourceGrassMaterialPaths[ProfileIndex] ||
        Target->GetPathName() != GrassMaterialObjectPath(ProfileIndex) ||
        TargetLegacyCount != 0 || TargetR27Count != 1 ||
        !SourceStable || !TargetStable ||
        SourceStable->Code != SourceStableVisibilityCode ||
        TargetStable->Code != R27StableVisibilityCode ||
        SourceStable->OutputType != CMOT_Float1 ||
        TargetStable->OutputType != CMOT_Float1 ||
        !HasExactStableInputs(SourceStable) ||
        !HasExactStableInputs(TargetStable) ||
        SourceData->OpacityMask.Expression != SourceStable ||
        SourceData->OpacityMask.OutputIndex != 0 ||
        TargetData->OpacityMask.Expression != TargetStable ||
        TargetData->OpacityMask.OutputIndex != 0 ||
        SourceData->ExpressionCollection.Expressions.Num() != 32 ||
        TargetData->ExpressionCollection.Expressions.Num() !=
            SourceData->ExpressionCollection.Expressions.Num() ||
        Source->MaterialDomain != Target->MaterialDomain ||
        Source->BlendMode != Target->BlendMode ||
        Source->TwoSided != Target->TwoSided ||
        Source->bTangentSpaceNormal != Target->bTangentSpaceNormal ||
        Source->bUseMaterialAttributes != Target->bUseMaterialAttributes ||
        Source->bUsedWithInstancedStaticMeshes !=
            Target->bUsedWithInstancedStaticMeshes ||
        Source->GetShadingModels() != Target->GetShadingModels() ||
        !FMath::IsNearlyEqual(
            Source->OpacityMaskClipValue,
            Target->OpacityMaskClipValue,
            0.000001f) ||
        !FMath::IsNearlyEqual(
            Source->MaxWorldPositionOffsetDisplacement,
            Target->MaxWorldPositionOffsetDisplacement,
            0.000001f) ||
        !Source->GetOutermost() || !Target->GetOutermost() ||
        (bRequireSaved &&
            (Source->GetOutermost()->IsDirty() ||
             Target->GetOutermost()->IsDirty() ||
             !FPackageName::DoesPackageExist(
                 FPackageName::ObjectPathToPackageName(
                     SourceGrassMaterialPaths[ProfileIndex])) ||
             !FPackageName::DoesPackageExist(
                 GrassMaterialPackagePath(ProfileIndex)))))
    {
        OutError = FString::Printf(
            TEXT("R27 grass material profile %d is not an exact disk-backed R23B derivative with only the 20--28 m double gate replaced by one integrated 65--90 m visibility gate."),
            ProfileIndex);
        return false;
    }

    FString ExactDerivativeReport;
    if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
            ValidateExactR23BDerivativeGrassMaterial(
                GrassMaterialObjectPath(ProfileIndex),
                ProfileIndex,
                R27StableVisibilityDescription,
                R27StableVisibilityCode,
                bRequireSaved,
                ExactDerivativeReport))
    {
        OutError = FString::Printf(
            TEXT("R27 grass profile %d failed the complete canonical R23B derivative and compiled-material contract: %s"),
            ProfileIndex,
            *ExactDerivativeReport);
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
            OutError = FString::Printf(
                TEXT("R27 grass profile %d changed expression topology at index %d."),
                ProfileIndex,
                Index);
            return false;
        }
        const UMaterialExpressionCustom* SourceCustom =
            Cast<UMaterialExpressionCustom>(SourceExpression);
        const UMaterialExpressionCustom* TargetCustom =
            Cast<UMaterialExpressionCustom>(TargetExpression);
        if (SourceCustom && SourceCustom != SourceStable &&
            (!TargetCustom ||
             SourceCustom->Description != TargetCustom->Description ||
             SourceCustom->Code != TargetCustom->Code ||
             SourceCustom->OutputType != TargetCustom->OutputType ||
             SourceCustom->Inputs.Num() != TargetCustom->Inputs.Num()))
        {
            OutError = FString::Printf(
                TEXT("R27 grass profile %d changed a non-visibility custom expression at index %d."),
                ProfileIndex,
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateR27GrassMaterialsInternal(
    bool bRequireSaved,
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(GrassMaterialAssetNames);
         ++Index)
    {
        UMaterial* Source = LoadExact<UMaterial>(
            SourceGrassMaterialPaths[Index]);
        UMaterial* Target = LoadExact<UMaterial>(
            GrassMaterialObjectPath(Index));
        if (!ValidateMaterialPair(
                Source, Target, Index, bRequireSaved, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Target);
    }
    OutError.Reset();
    return OutMaterials.Num() == 4;
}

UMaterial* DuplicateAndConfigureR27Material(
    UMaterial* Source,
    int32 ProfileIndex,
    FString& OutError)
{
    const FString PackagePath = GrassMaterialPackagePath(ProfileIndex);
    if (!Source || FPackageName::DoesPackageExist(PackagePath) ||
        FindPackage(nullptr, *PackagePath))
    {
        OutError = TEXT("R27 landmark grass destination is not fresh: ") +
            PackagePath;
        return nullptr;
    }
    UPackage* Package = CreatePackage(*PackagePath);
    UMaterial* Target = Package
        ? Cast<UMaterial>(StaticDuplicateObject(
              Source,
              Package,
              FName(*GrassMaterialAssetNames[ProfileIndex]),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    int32 LegacyCount = 0;
    int32 R27Count = 0;
    UMaterialExpressionCustom* Stable = FindStableVisibilityNode(
        Target, false, LegacyCount, R27Count);
    if (!Target || Target->GetPathName() != GrassMaterialObjectPath(ProfileIndex) ||
        LegacyCount != 1 || R27Count != 0 || !Stable ||
        Stable->Code != SourceStableVisibilityCode ||
        !HasExactStableInputs(Stable))
    {
        OutError = FString::Printf(
            TEXT("Could not duplicate exact R23B grass profile %d into the isolated R27 namespace."),
            ProfileIndex);
        return nullptr;
    }
    Target->Modify();
    Target->PreEditChange(nullptr);
    Stable->Modify();
    Stable->Description = R27StableVisibilityDescription;
    Stable->Code = R27StableVisibilityCode;
    Stable->AdditionalOutputs.Reset();
    Stable->AdditionalDefines.Reset();
    Stable->IncludeFilePaths.Reset();
    UMaterialEditingLibrary::RecompileMaterial(Target);
    Target->PostEditChange();
    Target->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Target);
    if (!ValidateMaterialPair(
            Source, Target, ProfileIndex, false, OutError))
    {
        return nullptr;
    }
    return Target;
}

bool LoadReusableAssets(
    FTRIADIstanaExploreV5DLandmarkVegetationAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DLandmarkVegetationAssets();
    OutAssets.GrassCarrierMesh = LoadExact<UStaticMesh>(GrassCarrierMeshPath);
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(GrassMaterialAssetNames);
         ++Index)
    {
        OutAssets.GrassProfileMaterials.Add(
            LoadExact<UMaterialInterface>(GrassMaterialObjectPath(Index)));
    }
    OutAssets.UmbrellaTreeMesh =
        LoadExact<UStaticMesh>(UmbrellaTreeMeshPath);
    OutAssets.DomeTreeMesh = LoadExact<UStaticMesh>(DomeTreeMeshPath);
    OutAssets.HighForkTreeMesh =
        LoadExact<UStaticMesh>(HighForkTreeMeshPath);
    OutAssets.ShrubMesh = LoadExact<UStaticMesh>(ShrubMeshPath);
    OutAssets.ShrubMaterial =
        LoadExact<UMaterialInterface>(ShrubMaterialPath);
    OutAssets.UnderstoreyMesh =
        LoadExact<UStaticMesh>(UnderstoreyMeshPath);
    OutAssets.UnderstoreyMaterial =
        LoadExact<UMaterialInterface>(UnderstoreyMaterialPath);
    OutAssets.FlowerMesh = LoadExact<UStaticMesh>(FlowerMeshPath);
    OutAssets.FlowerMaterial =
        LoadExact<UMaterialInterface>(FlowerMaterialPath);
    return ATRIADIstanaExploreV5DLandmarkVegetationActor::
        ValidateAssetRoster(OutAssets, OutError);
}

bool ValidateR28MaterialPair(
    UMaterial* SourceR27,
    UMaterial* TargetR28,
    int32 ProfileIndex,
    bool bRequireSaved,
    FString& OutError)
{
    UMaterial* CanonicalR23B = LoadExact<UMaterial>(
        SourceGrassMaterialPaths[ProfileIndex]);
    if (!ValidateMaterialPair(
            CanonicalR23B,
            SourceR27,
            ProfileIndex,
            true,
            OutError))
    {
        OutError = TEXT("R28 source is not the exact clean R27 derivative: ") +
            OutError;
        return false;
    }

    int32 SourceStableCount = 0;
    int32 SourceColorCount = 0;
    int32 TargetStableCount = 0;
    int32 TargetColorCount = 0;
    int32 TargetR27StableCount = 0;
    int32 TargetR23BColorCount = 0;
    UMaterialExpressionCustom* SourceStable = FindExactCustomNode(
        SourceR27,
        R27StableVisibilityDescription,
        SourceStableCount);
    UMaterialExpressionCustom* SourceColor = FindExactCustomNode(
        SourceR27,
        R23BGrassColorDescriptions[ProfileIndex],
        SourceColorCount);
    UMaterialExpressionCustom* TargetStable = FindExactCustomNode(
        TargetR28,
        R28StableVisibilityDescription,
        TargetStableCount);
    UMaterialExpressionCustom* TargetColor = FindExactCustomNode(
        TargetR28,
        R28GrassColorDescriptions[ProfileIndex],
        TargetColorCount);
    FindExactCustomNode(
        TargetR28,
        R27StableVisibilityDescription,
        TargetR27StableCount);
    FindExactCustomNode(
        TargetR28,
        R23BGrassColorDescriptions[ProfileIndex],
        TargetR23BColorCount);
    const auto* SourceData =
        SourceR27 ? SourceR27->GetEditorOnlyData() : nullptr;
    const auto* TargetData =
        TargetR28 ? TargetR28->GetEditorOnlyData() : nullptr;
    FString ExpectedR28ColorCode;
    if (!SourceColor ||
        !BuildR28GrassColorCode(
            SourceColor->Code,
            ExpectedR28ColorCode,
            OutError))
    {
        return false;
    }

    if (!SourceR27 || !TargetR28 || !SourceData || !TargetData ||
        SourceR27->GetPathName() !=
            R28SourceGrassMaterialPaths[ProfileIndex] ||
        TargetR28->GetPathName() !=
            R28GrassMaterialObjectPath(ProfileIndex) ||
        SourceStableCount != 1 || SourceColorCount != 1 ||
        TargetStableCount != 1 || TargetColorCount != 1 ||
        TargetR27StableCount != 0 || TargetR23BColorCount != 0 ||
        !SourceStable || !SourceColor || !TargetStable || !TargetColor ||
        SourceStable->Code != R27StableVisibilityCode ||
        TargetStable->Code != R28StableVisibilityCode ||
        TargetColor->Code != ExpectedR28ColorCode ||
        !HasExactStableInputs(SourceStable) ||
        !HasExactStableInputs(TargetStable) ||
        SourceData->OpacityMask.Expression != SourceStable ||
        TargetData->OpacityMask.Expression != TargetStable ||
        SourceData->OpacityMask.OutputIndex != 0 ||
        TargetData->OpacityMask.OutputIndex != 0 ||
        SourceData->ExpressionCollection.Expressions.Num() != 32 ||
        TargetData->ExpressionCollection.Expressions.Num() != 32 ||
        SourceR27->MaterialDomain != TargetR28->MaterialDomain ||
        SourceR27->BlendMode != TargetR28->BlendMode ||
        SourceR27->TwoSided != TargetR28->TwoSided ||
        SourceR27->bTangentSpaceNormal != TargetR28->bTangentSpaceNormal ||
        SourceR27->bUseMaterialAttributes !=
            TargetR28->bUseMaterialAttributes ||
        SourceR27->bUsedWithInstancedStaticMeshes !=
            TargetR28->bUsedWithInstancedStaticMeshes ||
        SourceR27->GetShadingModels() != TargetR28->GetShadingModels() ||
        !FMath::IsNearlyEqual(
            SourceR27->OpacityMaskClipValue,
            TargetR28->OpacityMaskClipValue,
            0.000001f) ||
        !FMath::IsNearlyEqual(
            SourceR27->MaxWorldPositionOffsetDisplacement,
            TargetR28->MaxWorldPositionOffsetDisplacement,
            0.000001f) ||
        !SourceR27->GetOutermost() || !TargetR28->GetOutermost() ||
        SourceR27->GetOutermost()->IsDirty() ||
        !FPackageName::DoesPackageExist(
            FPackageName::ObjectPathToPackageName(
                R28SourceGrassMaterialPaths[ProfileIndex])) ||
        (bRequireSaved &&
            (TargetR28->GetOutermost()->IsDirty() ||
             !FPackageName::DoesPackageExist(
                 R28GrassMaterialPackagePath(ProfileIndex)))))
    {
        OutError = FString::Printf(
            TEXT("R28 grass profile %d is not an exact R27 derivative with only the provenance label and blade-colour response changed."),
            ProfileIndex);
        return false;
    }

    FString ExactDerivativeReport;
    if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
            ValidateExactR23BVisualDerivativeGrassMaterial(
                R28GrassMaterialObjectPath(ProfileIndex),
                ProfileIndex,
                R28StableVisibilityDescription,
                R28StableVisibilityCode,
                R28GrassColorDescriptions[ProfileIndex],
                ExpectedR28ColorCode,
                bRequireSaved,
                ExactDerivativeReport))
    {
        OutError = FString::Printf(
            TEXT("R28 grass profile %d failed the complete canonical R23B visual-derivative contract: %s"),
            ProfileIndex,
            *ExactDerivativeReport);
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
            OutError = FString::Printf(
                TEXT("R28 grass profile %d changed expression topology at index %d."),
                ProfileIndex,
                Index);
            return false;
        }
        const UMaterialExpressionCustom* SourceCustom =
            Cast<UMaterialExpressionCustom>(SourceExpression);
        const UMaterialExpressionCustom* TargetCustom =
            Cast<UMaterialExpressionCustom>(TargetExpression);
        const bool bAllowedStableDelta =
            SourceCustom == SourceStable && TargetCustom == TargetStable;
        const bool bAllowedColorDelta =
            SourceCustom == SourceColor && TargetCustom == TargetColor;
        if (SourceCustom && !bAllowedStableDelta && !bAllowedColorDelta &&
            (!TargetCustom ||
             SourceCustom->Description != TargetCustom->Description ||
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
            OutError = FString::Printf(
                TEXT("R28 grass profile %d changed a forbidden custom expression at index %d."),
                ProfileIndex,
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateR28GrassMaterialsInternal(
    bool bRequireSaved,
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(R28GrassMaterialAssetNames);
         ++Index)
    {
        UMaterial* SourceR27 = LoadExact<UMaterial>(
            R28SourceGrassMaterialPaths[Index]);
        UMaterial* TargetR28 = LoadExact<UMaterial>(
            R28GrassMaterialObjectPath(Index));
        if (!ValidateR28MaterialPair(
                SourceR27,
                TargetR28,
                Index,
                bRequireSaved,
                OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(TargetR28);
    }
    OutError.Reset();
    return OutMaterials.Num() == 4;
}

UMaterial* DuplicateAndConfigureR28Material(
    UMaterial* SourceR27,
    int32 ProfileIndex,
    FString& OutError)
{
    const FString PackagePath = R28GrassMaterialPackagePath(ProfileIndex);
    if (!SourceR27 || FPackageName::DoesPackageExist(PackagePath) ||
        FindPackage(nullptr, *PackagePath))
    {
        OutError = TEXT("R28 landmark grass destination is not fresh: ") +
            PackagePath;
        return nullptr;
    }
    UPackage* Package = CreatePackage(*PackagePath);
    UMaterial* Target = Package
        ? Cast<UMaterial>(StaticDuplicateObject(
              SourceR27,
              Package,
              FName(*R28GrassMaterialAssetNames[ProfileIndex]),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    int32 StableCount = 0;
    int32 ColorCount = 0;
    UMaterialExpressionCustom* Stable = FindExactCustomNode(
        Target,
        R27StableVisibilityDescription,
        StableCount);
    UMaterialExpressionCustom* Color = FindExactCustomNode(
        Target,
        R23BGrassColorDescriptions[ProfileIndex],
        ColorCount);
    FString R28ColorCode;
    if (!Target ||
        Target->GetPathName() != R28GrassMaterialObjectPath(ProfileIndex) ||
        StableCount != 1 || ColorCount != 1 || !Stable || !Color ||
        Stable->Code != R27StableVisibilityCode ||
        !HasExactStableInputs(Stable) ||
        !BuildR28GrassColorCode(Color->Code, R28ColorCode, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Could not duplicate exact R27 grass profile %d into the isolated R28 namespace."),
                ProfileIndex);
        }
        return nullptr;
    }

    Target->Modify();
    Target->PreEditChange(nullptr);
    Stable->Modify();
    Stable->Description = R28StableVisibilityDescription;
    Stable->Code = R28StableVisibilityCode;
    Stable->AdditionalOutputs.Reset();
    Stable->AdditionalDefines.Reset();
    Stable->IncludeFilePaths.Reset();
    Color->Modify();
    Color->Description = R28GrassColorDescriptions[ProfileIndex];
    Color->Code = R28ColorCode;
    UMaterialEditingLibrary::RecompileMaterial(Target);
    Target->PostEditChange();
    Target->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Target);
    if (!ValidateR28MaterialPair(
            SourceR27,
            Target,
            ProfileIndex,
            false,
            OutError))
    {
        return nullptr;
    }
    return Target;
}

bool LoadReusableAssetsR28(
    FTRIADIstanaExploreV5DLandmarkVegetationAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DLandmarkVegetationAssets();
    OutAssets.GrassCarrierMesh = LoadExact<UStaticMesh>(GrassCarrierMeshPath);
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(R28GrassMaterialAssetNames);
         ++Index)
    {
        OutAssets.GrassProfileMaterials.Add(
            LoadExact<UMaterialInterface>(
                R28GrassMaterialObjectPath(Index)));
    }
    OutAssets.UmbrellaTreeMesh =
        LoadExact<UStaticMesh>(UmbrellaTreeMeshPath);
    OutAssets.DomeTreeMesh = LoadExact<UStaticMesh>(DomeTreeMeshPath);
    OutAssets.HighForkTreeMesh =
        LoadExact<UStaticMesh>(HighForkTreeMeshPath);
    OutAssets.ShrubMesh = LoadExact<UStaticMesh>(ShrubMeshPath);
    OutAssets.ShrubMaterial =
        LoadExact<UMaterialInterface>(ShrubMaterialPath);
    OutAssets.UnderstoreyMesh =
        LoadExact<UStaticMesh>(UnderstoreyMeshPath);
    OutAssets.UnderstoreyMaterial =
        LoadExact<UMaterialInterface>(UnderstoreyMaterialPath);
    OutAssets.FlowerMesh = LoadExact<UStaticMesh>(FlowerMeshPath);
    OutAssets.FlowerMaterial =
        LoadExact<UMaterialInterface>(FlowerMaterialPath);
    return ATRIADIstanaExploreV5DLandmarkVegetationActor::
        ValidateAssetRosterR28(OutAssets, OutError);
}
} // namespace

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    BuildOrValidateLandmarkGrassMaterialsR27(FString& OutReport)
{
    int32 ExistingPackages = 0;
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(GrassMaterialAssetNames);
         ++Index)
    {
        const FString PackagePath = GrassMaterialPackagePath(Index);
        ExistingPackages +=
            FPackageName::DoesPackageExist(PackagePath) ||
            FindPackage(nullptr, *PackagePath)
            ? 1
            : 0;
    }
    if (ExistingPackages != 0 && ExistingPackages != 4)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_REFUSED_PARTIAL_NAMESPACE existingPackages=%d expectedPackages=4"),
            ExistingPackages);
        return false;
    }

    if (ExistingPackages == 4)
    {
        TArray<UMaterial*> Materials;
        FString Error;
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (!ValidateR27GrassMaterialsInternal(true, Materials, Error))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_IDEMPOTENT_VALIDATION_FAILED: ") +
                Error;
            return false;
        }
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_IDEMPOTENT_PASS createdPackages=0 exactSavedPackages=4 exactDerivativeGraph=true compiledMaterials=4 sourceMaterialsModified=false sourceStableVisibilityMeters=20,28 targetStableVisibilityMeters=65,90 visibilityGateCount=1 componentFadeIntegrated=true evidenceRangeMeters=%.1f materialVisibilityAtEvidenceRange=%.3f tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip visualCaptureAccepted=false captureRevalidationRequired=true"),
            ATRIADIstanaExploreV5DLandmarkVegetationActor::
                ExpectedEvidenceViewDistanceCm() / 100.0,
            ATRIADIstanaExploreV5DLandmarkVegetationActor::
                ExpectedMaterialVisibilityAtEvidenceView(),
            ATRIADIstanaExploreV5DLandmarkVegetationActor::
                ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView());
        return true;
    }

    TArray<UMaterial*> SourceMaterials;
    FString Error;
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(SourceGrassMaterialPaths);
         ++Index)
    {
        UMaterial* Source = LoadExact<UMaterial>(SourceGrassMaterialPaths[Index]);
        if (!ValidateSourceGrassMaterial(Source, Index, Error))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_REFUSED_SOURCE: ") +
                Error;
            return false;
        }
        SourceMaterials.Add(Source);
    }

    TArray<UMaterial*> CreatedMaterials;
    for (int32 Index = 0;
         Index < SourceMaterials.Num();
         ++Index)
    {
        UMaterial* Created = DuplicateAndConfigureR27Material(
            SourceMaterials[Index], Index, Error);
        if (!Created)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_FAILED_UNSAVED: ") +
                Error;
            return false;
        }
        CreatedMaterials.Add(Created);
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UMaterial*> PreSaveMaterials;
    if (CreatedMaterials.Num() != 4 ||
        !ValidateR27GrassMaterialsInternal(false, PreSaveMaterials, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_FAILED_PRE_SAVE: ") +
            Error;
        return false;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    TArray<UObject*> AssetsToSave;
    for (UMaterial* Material : CreatedMaterials)
    {
        AssetsToSave.Add(Material);
    }
    if (!AssetSubsystem || AssetsToSave.Num() != 4 ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_FAILED_SAVE exactSaveRoster=4");
        return false;
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UMaterial*> SavedMaterials;
    if (!ValidateR27GrassMaterialsInternal(true, SavedMaterials, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_FAILED_POST_SAVE: ") +
            Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_PASS createdPackages=4 exactSavedPackages=4 exactDerivativeGraph=true compiledMaterials=4 sourceMaterialsModified=false sourceStableVisibilityMeters=20,28 targetStableVisibilityMeters=65,90 visibilityGateCount=1 componentFadeIntegrated=true evidenceRangeMeters=%.1f materialVisibilityAtEvidenceRange=%.3f tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip visualCaptureAccepted=false captureRevalidationRequired=true"),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedEvidenceViewDistanceCm() / 100.0,
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedMaterialVisibilityAtEvidenceView(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView());
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    ValidateLandmarkGrassMaterialsR27(FString& OutReport)
{
    TArray<UMaterial*> Materials;
    FString Error;
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateR27GrassMaterialsInternal(true, Materials, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_VALID exactSavedPackages=4 isolatedDerivativePackages=4 exactDerivativeGraph=true compiledMaterials=4 sourceMaterialsModified=false sourceStableVisibilityMeters=20,28 targetStableVisibilityMeters=65,90 visibilityGateCount=1 componentFadeIntegrated=true evidenceRangeMeters=%.1f materialVisibilityAtEvidenceRange=%.3f tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip visualCaptureAccepted=false captureRevalidationRequired=true"),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedEvidenceViewDistanceCm() / 100.0,
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedMaterialVisibilityAtEvidenceView(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView());
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    ValidateReusableLandmarkVegetationAssets(FString& OutReport)
{
    FString MaterialReport;
    if (!ValidateLandmarkGrassMaterialsR27(MaterialReport))
    {
        OutReport = MaterialReport;
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationAssets Assets;
    FString Error;
    if (!LoadReusableAssets(Assets, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_ASSET_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationLayout Layout;
    if (!ATRIADIstanaExploreV5DLandmarkVegetationActor::
            BuildDeterministicLayout(Layout, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_LAYOUT_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_ASSETS_VALID exactAssets=14 reusedUnmodifiedAssets=10 isolatedR27GrassMaterials=4 exactDerivativeGraph=true compiledMaterials=4 macDonaldGrass=%d temasekGrass=%d maximumGrassPerSite=%d evidenceRangeMeters=%.1f materialVisibilityAtEvidenceRange=%.3f visibilityGateCount=1 componentFadeIntegrated=true tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip visualCaptureAccepted=false captureRevalidationRequired=true mapMutation=false assetsCreated=false renderOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false"),
        Layout.MacDonaldHouse.GrassTotal(),
        Layout.TemasekShophouse.GrassTotal(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            MaximumGrassInstancesPerSite(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedEvidenceViewDistanceCm() / 100.0,
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedMaterialVisibilityAtEvidenceView(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView());
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    ConfigureLandmarkVegetationActor(
        ATRIADIstanaExploreV5DLandmarkVegetationActor* Actor,
        FString& OutReport)
{
    if (!IsValid(Actor))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_CONFIGURE_REFUSED: actor is null or invalid.");
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationAssets Assets;
    FString Error;
    if (!LoadReusableAssets(Assets, Error) ||
        !Actor->ConfigureLandmarkVegetation(Assets, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_CONFIGURE_FAILED: ") +
            Error;
        return false;
    }
    if (!Actor->ValidateLandmarkVegetation(OutReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_CONFIGURE_FAILED_POST_VALIDATION: ") +
            OutReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_CONFIGURE_PASS mapMutation=false assetsCreated=false ") +
        OutReport;
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    BuildOrValidateLandmarkGrassMaterialsR28(FString& OutReport)
{
    int32 ExistingPackages = 0;
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(R28GrassMaterialAssetNames);
         ++Index)
    {
        const FString PackagePath = R28GrassMaterialPackagePath(Index);
        ExistingPackages +=
            FPackageName::DoesPackageExist(PackagePath) ||
            FindPackage(nullptr, *PackagePath)
            ? 1
            : 0;
    }
    if (ExistingPackages != 0 && ExistingPackages != 4)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_REFUSED_PARTIAL_NAMESPACE existingPackages=%d expectedPackages=4"),
            ExistingPackages);
        return false;
    }

    if (ExistingPackages == 4)
    {
        TArray<UMaterial*> Materials;
        FString Error;
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (!ValidateR28GrassMaterialsInternal(true, Materials, Error))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_IDEMPOTENT_VALIDATION_FAILED: ") +
                Error;
            return false;
        }
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_IDEMPOTENT_PASS createdPackages=0 exactSavedPackages=4 exactR27Sources=4 allowedGraphDeltas=visibilityLabel,colorCode dryNearBlend=0.26 thatchNearBlend=0.46 farDryBlend=0.12 farThatchBlend=0.025 finalTint=0.90,1.12,0.90 finalChroma=0.94 sourceMaterialsModified=false mapsSaved=0 visualCaptureAccepted=false captureRevalidationRequired=true");
        return true;
    }

    TArray<UMaterial*> SourceMaterials;
    FString Error;
    if (!ValidateR27GrassMaterialsInternal(true, SourceMaterials, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_REFUSED_SOURCE: ") +
            Error;
        return false;
    }

    TArray<UMaterial*> CreatedMaterials;
    for (int32 Index = 0; Index < SourceMaterials.Num(); ++Index)
    {
        UMaterial* Created = DuplicateAndConfigureR28Material(
            SourceMaterials[Index],
            Index,
            Error);
        if (!Created)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_FAILED_UNSAVED: ") +
                Error;
            return false;
        }
        CreatedMaterials.Add(Created);
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UMaterial*> PreSaveMaterials;
    if (CreatedMaterials.Num() != 4 ||
        !ValidateR28GrassMaterialsInternal(
            false,
            PreSaveMaterials,
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_FAILED_PRE_SAVE: ") +
            Error;
        return false;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    TArray<UObject*> AssetsToSave;
    for (UMaterial* Material : CreatedMaterials)
    {
        AssetsToSave.Add(Material);
    }
    if (!AssetSubsystem || AssetsToSave.Num() != 4 ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_FAILED_SAVE exactSaveRoster=4");
        return false;
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UMaterial*> SavedMaterials;
    if (!ValidateR28GrassMaterialsInternal(true, SavedMaterials, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_FAILED_POST_SAVE: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_PASS createdPackages=4 exactSavedPackages=4 exactR27Sources=4 allowedGraphDeltas=visibilityLabel,colorCode dryNearBlend=0.26 thatchNearBlend=0.46 farDryBlend=0.12 farThatchBlend=0.025 finalTint=0.90,1.12,0.90 finalChroma=0.94 sourceMaterialsModified=false mapsSaved=0 visualCaptureAccepted=false captureRevalidationRequired=true");
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    ValidateLandmarkGrassMaterialsR28(FString& OutReport)
{
    TArray<UMaterial*> Materials;
    FString Error;
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateR28GrassMaterialsInternal(true, Materials, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_VALID exactSavedPackages=4 isolatedDerivativePackages=4 exactR27Sources=4 completeCanonicalR23BGraph=true allowedGraphDeltas=visibilityLabel,colorCode compiledMaterials=4 sourceMaterialsModified=false mapsSaved=0 visualCaptureAccepted=false captureRevalidationRequired=true");
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    ValidateReusableLandmarkVegetationAssetsR28(FString& OutReport)
{
    FString MaterialReport;
    if (!ValidateLandmarkGrassMaterialsR28(MaterialReport))
    {
        OutReport = MaterialReport;
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationAssets Assets;
    FString Error;
    if (!LoadReusableAssetsR28(Assets, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_ASSET_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationLayout Layout;
    if (!ATRIADIstanaExploreV5DLandmarkVegetationActor::
            BuildDeterministicLayoutR28(Layout, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_LAYOUT_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_ASSETS_VALID exactAssets=14 reusedUnmodifiedAssets=10 isolatedR28GrassMaterials=4 exactDerivativeGraph=true compiledMaterials=4 macDonaldGrass=%d temasekGrass=%d maximumGrassPerSite=%d hardscapeRenderClearanceCm=%.1f minimumNominalCarrierCoverage=%.2f temasekTreeRoster=umbrella,dome,umbrella sourceTreeAnchorTranslationsPreserved=true treeScalesAnisotropic=true mapMutation=false assetsCreated=false renderOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false botanicalClaim=false seasonalClaim=false visualCaptureAccepted=false captureRevalidationRequired=true"),
        Layout.MacDonaldHouse.GrassTotal(),
        Layout.TemasekShophouse.GrassTotal(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            MaximumR28GrassInstancesPerSite(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedR28HardscapeClearanceCm(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedR28NominalCarrierCoverageMinimum());
    return true;
}

bool UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
    ConfigureLandmarkVegetationActorR28(
        ATRIADIstanaExploreV5DLandmarkVegetationActor* Actor,
        FString& OutReport)
{
    if (!IsValid(Actor))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_CONFIGURE_REFUSED: actor is null or invalid.");
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationAssets Assets;
    FString Error;
    if (!LoadReusableAssetsR28(Assets, Error) ||
        !Actor->ConfigureLandmarkVegetationR28(Assets, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_CONFIGURE_FAILED: ") +
            Error;
        return false;
    }
    if (!Actor->ValidateLandmarkVegetationR28(OutReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_CONFIGURE_FAILED_POST_VALIDATION: ") +
            OutReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_CONFIGURE_PASS mapMutation=false assetsCreated=false ") +
        OutReport;
    return true;
}
