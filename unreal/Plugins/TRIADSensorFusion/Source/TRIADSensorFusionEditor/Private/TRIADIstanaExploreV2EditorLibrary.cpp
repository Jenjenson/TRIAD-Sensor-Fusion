#include "TRIADIstanaExploreV2EditorLibrary.h"

#include "AssetToolsModule.h"
#include "AssetCompilingManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Slate/SceneViewport.h"
#include "StaticMeshCompiler.h"
#include "TRIADIstanaExploreEditorLibrary.h"
#include "TRIADIstanaExploreLandscapeActor.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaFreeRoamPawn.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UnrealClient.h"

namespace
{
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v1"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v2"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v2.Istana_PublicView_Explore_v2"));
const FString V5HeroObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewV5/Building/SM_IstanaPublicViewV5_Building_Hero.SM_IstanaPublicViewV5_Building_Hero"));
const FString V1CollisionObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision"));
const FString V1TerrainObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Terrain.SM_IstanaPublicView_Terrain"));
const FString V1HardscapeObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Hardscape.SM_IstanaPublicView_Hardscape"));
const FString V2AssetPath(TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation"));
const FString TrunkWindName(TEXT("M_IPVExploreV2_BroadleafTrunk_Wind"));
const FString BranchWindName(TEXT("M_IPVExploreV2_BroadleafBranches_Wind"));
const FString LeafWindName(TEXT("M_IPVExploreV2_BroadleafLeaves_Wind"));
const FString GrassWindName(TEXT("M_IPVExploreV2_Turf_Wind"));
const FString TrunkWindObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafTrunk_Wind.M_IPVExploreV2_BroadleafTrunk_Wind"));
const FString BranchWindObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafBranches_Wind.M_IPVExploreV2_BroadleafBranches_Wind"));
const FString LeafWindObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafLeaves_Wind.M_IPVExploreV2_BroadleafLeaves_Wind"));
const FString GrassWindObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_Turf_Wind.M_IPVExploreV2_Turf_Wind"));
const FString V1LeafMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves.jacaranda_tree_leaves"));
const FString V1TrunkMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_trunk.jacaranda_tree_trunk"));
const FString V1BranchMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_branches.jacaranda_tree_branches"));
const FString V1GrassMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/M_IPVExplore_Lawn.M_IPVExplore_Lawn"));
const FString BroadleafObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FString PalmObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Palm.SM_IstanaPublicView_Tree_Palm"));
const FString ShrubAPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_A.SM_Urb_Str_Shrub_Common_Set_01_A"));
const FString ShrubBPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_D.SM_Urb_Str_Shrub_Common_Set_01_D"));
const FString HedgePath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_F.SM_Urb_Str_Shrub_Common_Set_01_F"));
const FString GroundcoverPath(
    TEXT("/Game/Scene_RoadsideConstruction/Assets/MS/3D_Plants/Urb_Str_Shrub_Common_Set_01/SM_Urb_Str_Shrub_Common_Set_01_H.SM_Urb_Str_Shrub_Common_Set_01_H"));
const FString GrassObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_GrassClump.SM_IstanaPublicViewExploreV1_GrassClump"));
const FString CylinderObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
const FName V1LandscapeTag(TEXT("TRIADIstanaExploreLandscapeV1"));
const FName V2LandscapeTag(TEXT("TRIADIstanaExploreLandscapeV2"));
constexpr int32 ExpectedOuterLegacyTrees = 560;

const FString WindCustomDescription(TEXT("TRIAD_EXPLORE_V2_DIRECTIONAL_GUST_WPO_V1"));
const FString WindCustomCode(
    TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\n")
    TEXT("float3 delta = WorldPosition - ObjectPosition;\n")
    TEXT("float h = saturate(max(delta.z, 0.0) / max(HeightCm, 1.0));\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023)) + InstanceRandom * 6.28318530718;\n")
    TEXT("float wave = sin(phase) + 0.35 * sin(phase * 2.31 + InstanceRandom * 3.7) + 0.12 * sin(phase * 7.7 + delta.z * 0.015);\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float response = max(ResponseScale, 0.0);\n")
    TEXT("float bend = WindStrengthCm * response * wave * h * h;\n")
    TEXT("return float3(direction * bend, abs(WindStrengthCm) * response * 0.035 * sin(phase * 0.73) * h);"));
const FString ColourCustomDescription(TEXT("TRIAD_EXPLORE_V2_PER_INSTANCE_LEAF_VARIATION_V1"));
const FString ColourCustomCode(
    TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\n")
    TEXT("float3 cool = float3(0.88, 0.98, 0.86);\n")
    TEXT("float3 warm = float3(1.04, 0.94, 0.78);\n")
    TEXT("return Diffuse * lerp(cool, warm, saturate(InstanceRandom));"));

struct FProtectedPackageBytes
{
    FString PackageName;
    FString Filename;
    TArray<uint8> Bytes;
};

template <typename T>
T* LoadExact(const FString& ObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

bool HasDirtyPackages(FString& OutError)
{
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (!DirtyMaps.IsEmpty() || !DirtyContent.IsEmpty())
    {
        OutError = TEXT("Close or save all dirty map/content packages before additive Explore V2 migration.");
        return true;
    }
    OutError.Reset();
    return false;
}

const TArray<FString>& ProtectedPackages()
{
    static const TArray<FString> Packages = {
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v3"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v4"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v5"),
        SourceMapPackage};
    return Packages;
}

bool CaptureProtectedPackages(
    TArray<FProtectedPackageBytes>& OutRecords,
    FString& OutError)
{
    OutRecords.Reset();
    for (const FString& PackageName : ProtectedPackages())
    {
        FProtectedPackageBytes Record;
        Record.PackageName = PackageName;
        if (!FPackageName::DoesPackageExist(PackageName, &Record.Filename) ||
            !FFileHelper::LoadFileToArray(Record.Bytes, *Record.Filename) ||
            Record.Bytes.IsEmpty())
        {
            OutError = TEXT("Could not freeze exact protected map bytes for ") + PackageName;
            return false;
        }
        OutRecords.Add(MoveTemp(Record));
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedPackages(
    const TArray<FProtectedPackageBytes>& Records,
    FString& OutError)
{
    if (Records.Num() != ProtectedPackages().Num())
    {
        OutError = TEXT("The protected V1-V5/Explore-V1 package roster changed.");
        return false;
    }
    for (const FProtectedPackageBytes& Record : Records)
    {
        FString Filename;
        TArray<uint8> Current;
        if (!ProtectedPackages().Contains(Record.PackageName) ||
            !FPackageName::DoesPackageExist(Record.PackageName, &Filename) ||
            !FPaths::IsSamePath(Filename, Record.Filename) ||
            !FFileHelper::LoadFileToArray(Current, *Filename) ||
            Current != Record.Bytes)
        {
            OutError = TEXT("A protected map changed during Explore V2 creation: ") +
                Record.PackageName;
            return false;
        }
    }
    OutError.Reset();
    return true;
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

ATRIADIstanaExploreLandscapeActor* FindV1Landscape(UWorld* World)
{
    ATRIADIstanaExploreLandscapeActor* Result = nullptr;
    for (TActorIterator<ATRIADIstanaExploreLandscapeActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(V1LandscapeTag))
        {
            continue;
        }
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaExploreV2LandscapeActor* FindV2Landscape(UWorld* World)
{
    ATRIADIstanaExploreV2LandscapeActor* Result = nullptr;
    for (TActorIterator<ATRIADIstanaExploreV2LandscapeActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(V2LandscapeTag))
        {
            continue;
        }
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

int32 CountLegacyTrees(const ATRIADIstanaPublicViewSceneActor* Scene)
{
    return Scene
        ? Scene->RainTreeInstances->GetInstanceCount() +
            Scene->PalmTreeInstances->GetInstanceCount() +
            Scene->FramingTreeInstances->GetInstanceCount()
        : 0;
}

bool CollectLegacyTreeWorldTransforms(
    const ATRIADIstanaPublicViewSceneActor* Scene,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    OutTransforms.Reset();
    if (!Scene)
    {
        OutError = TEXT("The inherited public-view scene actor is absent.");
        return false;
    }
    const TArray<const UHierarchicalInstancedStaticMeshComponent*> Components = {
        Scene->RainTreeInstances,
        Scene->PalmTreeInstances,
        Scene->FramingTreeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            OutError = TEXT("An inherited outer-tree component/mesh is absent.");
            return false;
        }
        for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
        {
            FTransform WorldTransform;
            if (!Component->GetInstanceTransform(Index, WorldTransform, true) ||
                WorldTransform.ContainsNaN())
            {
                OutError = TEXT("An inherited outer-tree world transform is missing or non-finite.");
                return false;
            }
            const FVector LocationMeters =
                WorldTransform.GetTranslation() / 100.0;
            const double RadiusMeters = FVector2D(LocationMeters).Size();
            if (!FMath::IsFinite(RadiusMeters) ||
                RadiusMeters < 249.0 || RadiusMeters > 1001.0)
            {
                OutError = TEXT("An inherited outer-tree position is outside the exact 250-1000 m upgrade envelope.");
                return false;
            }
            OutTransforms.Add(WorldTransform);
        }
    }
    if (OutTransforms.Num() != ExpectedOuterLegacyTrees)
    {
        OutError = FString::Printf(
            TEXT("Expected exactly %d inherited outer-tree positions, found %d."),
            ExpectedOuterLegacyTrees,
            OutTransforms.Num());
        return false;
    }
    OutError.Reset();
    return true;
}

bool ClearLegacyTreeVisuals(
    ATRIADIstanaPublicViewSceneActor* Scene,
    FString& OutError)
{
    if (!Scene)
    {
        OutError = TEXT("The duplicated scene is absent before legacy-tree replacement.");
        return false;
    }
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        Scene->RainTreeInstances,
        Scene->PalmTreeInstances,
        Scene->FramingTreeInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component)
        {
            OutError = TEXT("An inherited tree component is absent before replacement.");
            return false;
        }
        Component->Modify();
        Component->bAutoRebuildTreeOnInstanceChanges = false;
        Component->ClearInstances();
        Component->bAutoRebuildTreeOnInstanceChanges = true;
    }
    if (CountLegacyTrees(Scene) != 0)
    {
        OutError = TEXT("The duplicated low-detail outer-tree visuals were not fully replaced.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 FindMaterialSlot(const UStaticMesh* Mesh, const FName SlotName)
{
    if (!Mesh)
    {
        return INDEX_NONE;
    }
    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        if (Materials[Index].ImportedMaterialSlotName == SlotName ||
            Materials[Index].MaterialSlotName == SlotName)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

template <typename T>
T* AddMaterialExpression(UMaterial* Material, int32 EditorX, int32 EditorY)
{
    T* Expression = Material ? NewObject<T>(Material) : nullptr;
    if (Expression)
    {
        Material->GetExpressionCollection().AddExpression(Expression);
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
    }
    return Expression;
}

void ConnectCustomInput(
    UMaterialExpressionCustom* Custom,
    const FName Name,
    UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
    Input.InputName = Name;
    Input.Input.Connect(OutputIndex, Expression);
}

bool AddWindGraph(
    UMaterial* Material,
    float DefaultStrengthCm,
    float DefaultSpeed,
    float DefaultHeightCm,
    float ResponseScaleValue,
    float MaximumDisplacementCm,
    bool bAddLeafColourVariation,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpression* OriginalDiffuse = EditorOnly
        ? EditorOnly->BaseColor.Expression
        : nullptr;
    const int32 OriginalDiffuseOutput = EditorOnly
        ? EditorOnly->BaseColor.OutputIndex
        : 0;
    if (!Material || !EditorOnly || EditorOnly->WorldPositionOffset.Expression ||
        (bAddLeafColourVariation && !OriginalDiffuse))
    {
        OutError = TEXT("The V2 wind material source graph is absent or already has WPO state.");
        return false;
    }

    UMaterialExpressionWorldPosition* WorldPosition =
        AddMaterialExpression<UMaterialExpressionWorldPosition>(Material, -1200, 560);
    UMaterialExpressionObjectPositionWS* ObjectPosition =
        AddMaterialExpression<UMaterialExpressionObjectPositionWS>(Material, -1200, 650);
    UMaterialExpressionTime* Time =
        AddMaterialExpression<UMaterialExpressionTime>(Material, -1200, 740);
    UMaterialExpressionScalarParameter* Strength =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -920, 560);
    UMaterialExpressionScalarParameter* Speed =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -920, 650);
    UMaterialExpressionScalarParameter* Height =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -920, 740);
    UMaterialExpressionScalarParameter* ResponseScale =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -920, 830);
    UMaterialExpressionVectorParameter* Direction =
        AddMaterialExpression<UMaterialExpressionVectorParameter>(Material, -920, 920);
    UMaterialExpressionCustom* Wind =
        AddMaterialExpression<UMaterialExpressionCustom>(Material, -520, 650);
    if (!WorldPosition || !ObjectPosition || !Time || !Strength ||
        !Speed || !Height || !ResponseScale || !Direction || !Wind)
    {
        OutError = TEXT("Could not allocate the complete Explore V2 wind graph.");
        return false;
    }

    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    Strength->ParameterName = TEXT("TRIAD_WindStrengthCm");
    Strength->DefaultValue = DefaultStrengthCm;
    Speed->ParameterName = TEXT("TRIAD_WindSpeed");
    Speed->DefaultValue = DefaultSpeed;
    Height->ParameterName = TEXT("TRIAD_WindHeightCm");
    Height->DefaultValue = DefaultHeightCm;
    ResponseScale->ParameterName = TEXT("TRIAD_WindResponseScale");
    ResponseScale->DefaultValue = ResponseScaleValue;
    Direction->ParameterName = TEXT("TRIAD_WindDirection");
    Direction->DefaultValue = FLinearColor(0.94f, 0.342f, 0.0f, 0.0f);
    Wind->Description = WindCustomDescription;
    Wind->OutputType = CMOT_Float3;
    Wind->Code = WindCustomCode;
    // UMaterialExpressionCustom starts with one empty input in UE 5.5.
    // Replace that editor placeholder instead of appending the exact roster
    // behind it; otherwise the saved material round-trips with nine inputs.
    Wind->Inputs.Reset();
    ConnectCustomInput(Wind, TEXT("WorldPosition"), WorldPosition);
    ConnectCustomInput(Wind, TEXT("ObjectPosition"), ObjectPosition);
    ConnectCustomInput(Wind, TEXT("TimeSeconds"), Time);
    ConnectCustomInput(Wind, TEXT("WindStrengthCm"), Strength);
    ConnectCustomInput(Wind, TEXT("WindSpeed"), Speed);
    ConnectCustomInput(Wind, TEXT("WindDirection"), Direction);
    ConnectCustomInput(Wind, TEXT("HeightCm"), Height);
    ConnectCustomInput(Wind, TEXT("ResponseScale"), ResponseScale);
    EditorOnly->WorldPositionOffset.Connect(0, Wind);

    if (bAddLeafColourVariation)
    {
        UMaterialExpressionCustom* Colour =
            AddMaterialExpression<UMaterialExpressionCustom>(Material, -280, -260);
        if (!Colour)
        {
            OutError = TEXT("Could not allocate the V2 per-instance leaf-colour node.");
            return false;
        }
        Colour->Description = ColourCustomDescription;
        Colour->OutputType = CMOT_Float3;
        Colour->Code = ColourCustomCode;
        Colour->Inputs.Reset();
        ConnectCustomInput(Colour, TEXT("Diffuse"), OriginalDiffuse, OriginalDiffuseOutput);
        EditorOnly->BaseColor.Connect(0, Colour);
        EditorOnly->SubsurfaceColor.Connect(0, Colour);
    }

    Material->Modify();
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->MaxWorldPositionOffsetDisplacement = MaximumDisplacementCm;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return true;
}

bool ValidateWindMaterial(
    UMaterial* Material,
    const FString& ExactPath,
    float ExpectedMaximumDisplacement,
    float ExpectedResponseScale,
    float ExpectedDefaultStrength,
    float ExpectedDefaultSpeed,
    float ExpectedDefaultHeight,
    bool bRequireLeafPolicy,
    FString& OutError)
{
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionCustom* Wind = EditorOnly
        ? Cast<UMaterialExpressionCustom>(
            EditorOnly->WorldPositionOffset.Expression)
        : nullptr;
    bool bStrength = false;
    bool bSpeed = false;
    bool bDirection = false;
    bool bHeight = false;
    bool bResponseScale = false;
    float ResponseScaleDefault = -1.0f;
    int32 WindNodeCount = 0;
    int32 ColourNodeCount = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
            EditorOnly->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionScalarParameter* Scalar =
                    Cast<UMaterialExpressionScalarParameter>(Expression))
            {
                bStrength |= Scalar->ParameterName == TEXT("TRIAD_WindStrengthCm");
                bSpeed |= Scalar->ParameterName == TEXT("TRIAD_WindSpeed");
                bHeight |= Scalar->ParameterName == TEXT("TRIAD_WindHeightCm");
                if (Scalar->ParameterName == TEXT("TRIAD_WindResponseScale"))
                {
                    bResponseScale = true;
                    ResponseScaleDefault = Scalar->DefaultValue;
                }
            }
            if (const UMaterialExpressionVectorParameter* Vector =
                    Cast<UMaterialExpressionVectorParameter>(Expression))
            {
                bDirection |= Vector->ParameterName == TEXT("TRIAD_WindDirection");
            }
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                WindNodeCount += Custom->Description == WindCustomDescription ? 1 : 0;
                ColourNodeCount += Custom->Description == ColourCustomDescription ? 1 : 0;
            }
        }
    }
    const TArray<FName> ExpectedInputNames = {
        TEXT("WorldPosition"),
        TEXT("ObjectPosition"),
        TEXT("TimeSeconds"),
        TEXT("WindStrengthCm"),
        TEXT("WindSpeed"),
        TEXT("WindDirection"),
        TEXT("HeightCm"),
        TEXT("ResponseScale")};
    bool bExactInputs = Wind && Wind->Inputs.Num() == ExpectedInputNames.Num();
    if (bExactInputs)
    {
        for (int32 Index = 0; Index < ExpectedInputNames.Num(); ++Index)
        {
            bExactInputs &=
                Wind->Inputs[Index].InputName == ExpectedInputNames[Index] &&
                Wind->Inputs[Index].Input.Expression != nullptr &&
                Wind->Inputs[Index].Input.OutputIndex == 0;
        }
    }
    const UMaterialExpressionWorldPosition* WorldInput = bExactInputs
        ? Cast<UMaterialExpressionWorldPosition>(
            Wind->Inputs[0].Input.Expression)
        : nullptr;
    const UMaterialExpressionObjectPositionWS* ObjectInput = bExactInputs
        ? Cast<UMaterialExpressionObjectPositionWS>(
            Wind->Inputs[1].Input.Expression)
        : nullptr;
    const UMaterialExpressionTime* TimeInput = bExactInputs
        ? Cast<UMaterialExpressionTime>(Wind->Inputs[2].Input.Expression)
        : nullptr;
    const UMaterialExpressionScalarParameter* StrengthInput = bExactInputs
        ? Cast<UMaterialExpressionScalarParameter>(
            Wind->Inputs[3].Input.Expression)
        : nullptr;
    const UMaterialExpressionScalarParameter* SpeedInput = bExactInputs
        ? Cast<UMaterialExpressionScalarParameter>(
            Wind->Inputs[4].Input.Expression)
        : nullptr;
    const UMaterialExpressionVectorParameter* DirectionInput = bExactInputs
        ? Cast<UMaterialExpressionVectorParameter>(
            Wind->Inputs[5].Input.Expression)
        : nullptr;
    const UMaterialExpressionScalarParameter* HeightInput = bExactInputs
        ? Cast<UMaterialExpressionScalarParameter>(
            Wind->Inputs[6].Input.Expression)
        : nullptr;
    const UMaterialExpressionScalarParameter* ResponseInput = bExactInputs
        ? Cast<UMaterialExpressionScalarParameter>(
            Wind->Inputs[7].Input.Expression)
        : nullptr;
    const auto FailPolicy = [&OutError, &ExactPath](const FString& Detail)
    {
        OutError = FString::Printf(
            TEXT("An Explore V2 wind material no longer matches its exact WPO/instancing policy: %s [%s]."),
            *Detail,
            *ExactPath);
        return false;
    };
    if (!Material) return FailPolicy(TEXT("material-missing"));
    if (Material->GetPathName() != ExactPath) return FailPolicy(TEXT("object-path"));
    if (!EditorOnly) return FailPolicy(TEXT("editor-only-data"));
    if (!Material->bUsedWithInstancedStaticMeshes) return FailPolicy(TEXT("instanced-static-mesh-usage"));
    if (Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5)) return FailPolicy(TEXT("shader-compiling-or-error"));
    if (!FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            ExpectedMaximumDisplacement,
            0.001f))
    {
        return FailPolicy(FString::Printf(
            TEXT("max-wpo-displacement actual=%.6f expected=%.6f"),
            Material->MaxWorldPositionOffsetDisplacement,
            ExpectedMaximumDisplacement));
    }
    if (!Wind) return FailPolicy(TEXT("wpo-custom-node"));
    if (Wind->Description != WindCustomDescription) return FailPolicy(TEXT("wpo-description"));
    if (Wind->Code != WindCustomCode) return FailPolicy(TEXT("wpo-code"));
    if (Wind->OutputType != CMOT_Float3) return FailPolicy(TEXT("wpo-output-type"));
    if (!bExactInputs)
    {
        FString Signature = FString::Printf(
            TEXT("wpo-input-roster-or-connection count=%d"),
            Wind ? Wind->Inputs.Num() : -1);
        if (Wind)
        {
            for (int32 Index = 0; Index < Wind->Inputs.Num(); ++Index)
            {
                const FCustomInput& Input = Wind->Inputs[Index];
                Signature += FString::Printf(
                    TEXT(" input[%d]={name=%s,class=%s,output=%d}"),
                    Index,
                    *Input.InputName.ToString(),
                    Input.Input.Expression
                        ? *Input.Input.Expression->GetClass()->GetName()
                        : TEXT("null"),
                    Input.Input.OutputIndex);
            }
        }
        return FailPolicy(Signature);
    }
    if (!WorldInput) return FailPolicy(TEXT("world-position-input"));
    if (!ObjectInput) return FailPolicy(TEXT("object-position-input"));
    if (!TimeInput) return FailPolicy(TEXT("time-input"));
    if (!StrengthInput) return FailPolicy(TEXT("strength-input"));
    if (!SpeedInput) return FailPolicy(TEXT("speed-input"));
    if (!DirectionInput) return FailPolicy(TEXT("direction-input"));
    if (!HeightInput) return FailPolicy(TEXT("height-input"));
    if (!ResponseInput) return FailPolicy(TEXT("response-input"));
    if (WorldInput->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets) return FailPolicy(TEXT("world-position-offset-mode"));
    if (StrengthInput->ParameterName != TEXT("TRIAD_WindStrengthCm")) return FailPolicy(TEXT("strength-name"));
    if (!FMath::IsNearlyEqual(StrengthInput->DefaultValue, ExpectedDefaultStrength, 0.0001f)) return FailPolicy(TEXT("strength-default"));
    if (SpeedInput->ParameterName != TEXT("TRIAD_WindSpeed")) return FailPolicy(TEXT("speed-name"));
    if (!FMath::IsNearlyEqual(SpeedInput->DefaultValue, ExpectedDefaultSpeed, 0.0001f)) return FailPolicy(TEXT("speed-default"));
    if (DirectionInput->ParameterName != TEXT("TRIAD_WindDirection")) return FailPolicy(TEXT("direction-name"));
    if (!DirectionInput->DefaultValue.Equals(FLinearColor(0.94f, 0.342f, 0.0f, 0.0f), 0.0001f)) return FailPolicy(TEXT("direction-default"));
    if (HeightInput->ParameterName != TEXT("TRIAD_WindHeightCm")) return FailPolicy(TEXT("height-name"));
    if (!FMath::IsNearlyEqual(HeightInput->DefaultValue, ExpectedDefaultHeight, 0.0001f)) return FailPolicy(TEXT("height-default"));
    if (ResponseInput->ParameterName != TEXT("TRIAD_WindResponseScale")) return FailPolicy(TEXT("response-name"));
    if (!FMath::IsNearlyEqual(ResponseInput->DefaultValue, ExpectedResponseScale, 0.0001f)) return FailPolicy(TEXT("response-default"));
    if (WindNodeCount != 1) return FailPolicy(TEXT("wind-node-count"));
    if (!bStrength || !bSpeed || !bDirection || !bHeight || !bResponseScale) return FailPolicy(TEXT("parameter-roster"));
    if (!FMath::IsNearlyEqual(ResponseScaleDefault, ExpectedResponseScale, 0.0001f)) return FailPolicy(TEXT("response-roster-default"));
    if (bRequireLeafPolicy && Material->GetBlendMode() != BLEND_Masked) return FailPolicy(TEXT("leaf-blend-mode"));
    if (bRequireLeafPolicy && !Material->IsTwoSided()) return FailPolicy(TEXT("leaf-two-sided"));
    if (bRequireLeafPolicy && !Material->GetShadingModels().HasOnlyShadingModel(MSM_TwoSidedFoliage)) return FailPolicy(TEXT("leaf-shading-model"));
    if (bRequireLeafPolicy && ColourNodeCount != 1) return FailPolicy(TEXT("leaf-colour-node-count"));
    if (!bRequireLeafPolicy && ColourNodeCount != 0) return FailPolicy(TEXT("unexpected-colour-node"));
    OutError.Reset();
    return true;
}

bool ValidateWindMaterials(FString& OutError)
{
    return ValidateWindMaterial(
            LoadExact<UMaterial>(TrunkWindObjectPath),
            TrunkWindObjectPath,
            20.0f,
            0.08f,
            5.0f,
            0.52f,
            2200.0f,
            false,
            OutError) &&
        ValidateWindMaterial(
            LoadExact<UMaterial>(BranchWindObjectPath),
            BranchWindObjectPath,
            60.0f,
            0.35f,
            5.0f,
            0.52f,
            2200.0f,
            false,
            OutError) &&
        ValidateWindMaterial(
            LoadExact<UMaterial>(LeafWindObjectPath),
            LeafWindObjectPath,
            160.0f,
            1.0f,
            5.0f,
            0.52f,
            2200.0f,
            true,
            OutError) &&
        ValidateWindMaterial(
            LoadExact<UMaterial>(GrassWindObjectPath),
            GrassWindObjectPath,
            18.0f,
            0.12f,
            5.0f,
            1.35f,
            80.0f,
            false,
            OutError);
}

bool AnyV2AssetExists()
{
    return FPackageName::DoesPackageExist(
            V2AssetPath + TEXT("/") + TrunkWindName) ||
        FPackageName::DoesPackageExist(
            V2AssetPath + TEXT("/") + BranchWindName) ||
        FPackageName::DoesPackageExist(
            V2AssetPath + TEXT("/") + LeafWindName) ||
        FPackageName::DoesPackageExist(
            V2AssetPath + TEXT("/") + GrassWindName);
}

bool CreateWindMaterials(FString& OutMessage)
{
    FString Error;
    if (ValidateWindMaterials(Error))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V2_WIND_MATERIALS_ALREADY_VALID");
        return true;
    }
    if (AnyV2AssetExists())
    {
        OutMessage = TEXT("EXPLORE_V2_MATERIAL_CREATION_REFUSED: partial or invalid V2 wind-material roster exists. ") + Error;
        return false;
    }
    UMaterial* V1Trunk = LoadExact<UMaterial>(V1TrunkMaterialObjectPath);
    UMaterial* V1Branches = LoadExact<UMaterial>(V1BranchMaterialObjectPath);
    UMaterial* V1Leaves = LoadExact<UMaterial>(V1LeafMaterialObjectPath);
    UMaterial* V1Grass = LoadExact<UMaterial>(V1GrassMaterialObjectPath);
    if (!V1Trunk || !V1Branches || !V1Leaves || !V1Grass ||
        V1Trunk->GetOutermost()->IsDirty() ||
        V1Branches->GetOutermost()->IsDirty() ||
        V1Leaves->GetOutermost()->IsDirty() ||
        V1Grass->GetOutermost()->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V2_MATERIAL_CREATION_REFUSED: exact validated V1 material dependencies are absent or dirty.");
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterial* Trunk = Cast<UMaterial>(AssetTools.DuplicateAsset(
        TrunkWindName, V2AssetPath, V1Trunk));
    UMaterial* Branches = Cast<UMaterial>(AssetTools.DuplicateAsset(
        BranchWindName, V2AssetPath, V1Branches));
    UMaterial* Leaves = Cast<UMaterial>(AssetTools.DuplicateAsset(
        LeafWindName, V2AssetPath, V1Leaves));
    UMaterial* Grass = Cast<UMaterial>(AssetTools.DuplicateAsset(
        GrassWindName, V2AssetPath, V1Grass));
    if (!Trunk || !Branches || !Leaves || !Grass ||
        Trunk->GetPathName() != TrunkWindObjectPath ||
        Branches->GetPathName() != BranchWindObjectPath ||
        Leaves->GetPathName() != LeafWindObjectPath ||
        Grass->GetPathName() != GrassWindObjectPath ||
        !AddWindGraph(Trunk, 5.0f, 0.52f, 2200.0f, 0.08f, 20.0f, false, Error) ||
        !AddWindGraph(Branches, 5.0f, 0.52f, 2200.0f, 0.35f, 60.0f, false, Error) ||
        !AddWindGraph(Leaves, 5.0f, 0.52f, 2200.0f, 1.0f, 160.0f, true, Error) ||
        !AddWindGraph(Grass, 5.0f, 1.35f, 80.0f, 0.12f, 18.0f, false, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MATERIALS: ") + Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!Assets || !Assets->SaveLoadedAsset(Trunk, false) ||
        !Assets->SaveLoadedAsset(Branches, false) ||
        !Assets->SaveLoadedAsset(Leaves, false) ||
        !Assets->SaveLoadedAsset(Grass, false) ||
        !ValidateWindMaterials(Error))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V2_MATERIALS: ") + Error;
        return false;
    }
    OutMessage = TEXT("Created and validated four additive V2 instanced WPO materials: low-amplitude trunk flex, medium branch sway, high-detail leaf flutter/colour variation, and bounded turf/meadow motion with runtime spring-driven recovery; V1 assets remain unchanged.");
    return true;
}

bool ValidateAuthoritativeSensorMeshSplit(
    const ATRIADIstanaPublicViewSceneActor* Scene,
    FString& OutError)
{
    if (!Scene)
    {
        OutError = TEXT("Public-view scene actor is absent.");
        return false;
    }

    struct FExpectedMeshBinding
    {
        const TCHAR* Role;
        const UStaticMeshComponent* Component;
        const FString* ObjectPath;
        bool bCollisionAuthority;
    };
    const FExpectedMeshBinding Bindings[] = {
        {TEXT("RGB_AND_SCENE_DEPTH_VISUAL"), Scene->BuildingHeroVisualComponent.Get(), &V5HeroObjectPath, false},
        {TEXT("BUILDING_RAY_AND_PAWN_COLLISION"), Scene->BuildingCollisionComponent.Get(), &V1CollisionObjectPath, true},
        {TEXT("TERRAIN_VISUAL_COLLISION"), Scene->TerrainComponent.Get(), &V1TerrainObjectPath, true},
        {TEXT("HARDSCAPE_VISUAL_COLLISION"), Scene->HardscapeComponent.Get(), &V1HardscapeObjectPath, true},
    };
    for (const FExpectedMeshBinding& Binding : Bindings)
    {
        const FString& ExpectedPath = *Binding.ObjectPath;
        const UStaticMesh* Mesh = Binding.Component
            ? Binding.Component->GetStaticMesh()
            : nullptr;
        if (!Mesh || Mesh->GetPathName() != ExpectedPath)
        {
            OutError = FString::Printf(
                TEXT("Authoritative %s mesh binding changed; expected exact asset '%s'."),
                Binding.Role,
                *ExpectedPath);
            return false;
        }
        if (!Binding.bCollisionAuthority)
        {
            if (Binding.Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
            {
                OutError = TEXT("The V5 visual mesh must remain render-only; it cannot become ray or pawn collision authority.");
                return false;
            }
            continue;
        }

        const UBodySetup* BodySetup = Mesh->GetBodySetup();
        if (!BodySetup || BodySetup->CollisionTraceFlag != CTF_UseComplexAsSimple ||
            Binding.Component->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics ||
            Binding.Component->GetCollisionObjectType() != ECC_WorldStatic ||
            Binding.Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block ||
            Binding.Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
        {
            OutError = FString::Printf(
                TEXT("Authoritative %s mesh must remain complex-as-simple WorldStatic QueryAndPhysics collision that blocks both Visibility rays and Pawn sweeps."),
                Binding.Role);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateWorld(UWorld* World, FString& OutReport)
{
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutReport = TEXT("Open exact additive map /Game/Maps/Istana_PublicView_Explore_v2.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(World);
    ATRIADIstanaExploreV2LandscapeActor* Landscape = FindV2Landscape(World);
    UClass* ExploreGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreGameMode"));
    FString SceneReport;
    FString SensorMeshReport;
    FString LandscapeReport;
    if (!Scene || !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !ValidateAuthoritativeSensorMeshSplit(Scene, SensorMeshReport) ||
        !Policy || FindV1Landscape(World) || !Landscape ||
        !ExploreGameMode ||
        World->GetWorldSettings()->DefaultGameMode.Get() != ExploreGameMode ||
        Policy->bEnforceFixedPrimaryCamera ||
        CountLegacyTrees(Scene) != 0 ||
        !Landscape->ValidateExploreV2Landscape(LandscapeReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_MAP_INVALID: public-view scene, authoritative visual/collision split, map policy, source replacement, low-detail outer-tree removal or V2 landscape failed. ") +
            SceneReport + TEXT(" ") + SensorMeshReport + TEXT(" ") + LandscapeReport;
        return false;
    }
    FString ContextError;
    if (!Landscape->ValidatePreservedDistantContext(
            Scene->OSMContextBuildingsComponent.Get(), ContextError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_MAP_INVALID: distant OSM/HDB context changed. ") +
            ContextError;
        return false;
    }
    OutReport = TEXT("Validated additive Explore V2: all 560 inherited 250-1000 m tree positions are deterministically replanted into the 720-tree safe-LOD V2 canopy system (532 outer high-detail broadleaf positions and 28 sparse procedural palm-proxy positions); evidence-aligned tropical planting, tiered turf, trunk/branch/leaf gust recovery, free-roam play and unchanged distant OSM/HDB context are live. Exact species, individual-tree inventory and survey-grade one-to-one claims remain false.");
    return true;
}
}

bool UTRIADIstanaExploreV2EditorLibrary::CreateIstanaExploreV2WindMaterials(
    FString& OutMessage)
{
    return CreateWindMaterials(OutMessage);
}

bool UTRIADIstanaExploreV2EditorLibrary::BuildIstanaExploreV2Map(
    FString& OutMessage)
{
    FString Error;
    if (HasDirtyPackages(Error))
    {
        OutMessage = TEXT("EXPLORE_V2_BUILD_REFUSED: ") + Error;
        return false;
    }
    if (!ValidateWindMaterials(Error))
    {
        OutMessage = TEXT("EXPLORE_V2_BUILD_REFUSED: exact V2 wind materials are absent or invalid. ") + Error;
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage))
    {
        FString Existing;
        if (ValidateIstanaExploreV2Map(Existing))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V2_MAP_ALREADY_VALID: ") + Existing;
            return true;
        }
        OutMessage = TEXT("EXPLORE_V2_BUILD_REFUSED: destination exists and is not the open exact validated V2 map. ") + Existing;
        return false;
    }

    UWorld* SourceWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    ATRIADIstanaPublicViewSceneActor* SourceScene = FindScene(SourceWorld);
    ATRIADIstanaExploreLandscapeActor* SourceLandscape = FindV1Landscape(SourceWorld);
    FString SourceReport;
    if (!SourceWorld || !SourceWorld->GetOutermost() ||
        SourceWorld->GetOutermost()->GetName() != SourceMapPackage ||
        !SourceScene || !SourceLandscape ||
        CountLegacyTrees(SourceScene) != ExpectedOuterLegacyTrees ||
        !UTRIADIstanaExploreEditorLibrary::ValidateIstanaExploreV1Map(
            SourceReport) ||
        !SourceLandscape->ValidateExploreLandscape(SourceReport))
    {
        OutMessage = TEXT("EXPLORE_V2_BUILD_REFUSED: open the exact validated Explore V1 source map. ") + SourceReport;
        return false;
    }

    TArray<FProtectedPackageBytes> Protected;
    if (!CaptureProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V2_BUILD_REFUSED: ") + Error;
        return false;
    }
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    UWorld* TargetWorld = Assets
        ? Cast<UWorld>(Assets->DuplicateLoadedAsset(
            SourceWorld, DestinationMapPackage))
        : nullptr;
    if (!TargetWorld || !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: Explore V1 duplication failed.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(TargetWorld);
    ATRIADIstanaExploreLandscapeActor* OldLandscape = FindV1Landscape(TargetWorld);
    if (!Scene || !OldLandscape || CountLegacyTrees(Scene) != ExpectedOuterLegacyTrees ||
        !SourceLandscape->ValidatePreservedDistantContext(
            Scene ? Scene->OSMContextBuildingsComponent.Get() : nullptr,
            Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: duplicated source scene/landscape/outer trees or exact distant context are invalid. ") + Error;
        return false;
    }
    TArray<FTransform> OuterTreeTransforms;
    if (!CollectLegacyTreeWorldTransforms(Scene, OuterTreeTransforms, Error) ||
        !ClearLegacyTreeVisuals(Scene, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: low-detail outer-tree position capture/replacement failed. ") + Error;
        return false;
    }
    OldLandscape->Modify();
    if (!TargetWorld->DestroyActor(OldLandscape, true, true))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: only the duplicated V1 landscape actor could not be removed.");
        return false;
    }

    UStaticMesh* Broadleaf = LoadExact<UStaticMesh>(BroadleafObjectPath);
    UStaticMesh* Grass = LoadExact<UStaticMesh>(GrassObjectPath);
    UStaticMesh* Palm = LoadExact<UStaticMesh>(PalmObjectPath);
    UStaticMesh* ShrubA = LoadExact<UStaticMesh>(ShrubAPath);
    UStaticMesh* ShrubB = LoadExact<UStaticMesh>(ShrubBPath);
    UStaticMesh* Hedge = LoadExact<UStaticMesh>(HedgePath);
    UStaticMesh* Groundcover = LoadExact<UStaticMesh>(GroundcoverPath);
    UStaticMesh* Cylinder = LoadExact<UStaticMesh>(CylinderObjectPath);
    TArray<UStaticMesh*> RequiredMeshes = {
        Broadleaf, Grass, Palm, ShrubA, ShrubB, Hedge, Groundcover, Cylinder};
    if (RequiredMeshes.Contains(nullptr))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: an exact V2 vegetation/collision mesh dependency is absent.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(RequiredMeshes);
    for (const UStaticMesh* Mesh : RequiredMeshes)
    {
        if (!Mesh || Mesh->IsCompiling())
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: an exact vegetation mesh remained in asynchronous compilation.");
            return false;
        }
    }
    const int32 TrunkSlot = FindMaterialSlot(
        Broadleaf, TEXT("jacaranda_tree_trunk"));
    const int32 BranchSlot = FindMaterialSlot(
        Broadleaf, TEXT("jacaranda_tree_branches"));
    const int32 LeafSlot = FindMaterialSlot(
        Broadleaf, TEXT("jacaranda_tree_leaves"));
    if (TrunkSlot == INDEX_NONE || BranchSlot == INDEX_NONE ||
        LeafSlot == INDEX_NONE || TrunkSlot == BranchSlot ||
        TrunkSlot == LeafSlot || BranchSlot == LeafSlot)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: the exact distinct trunk/branch/leaf slot roster is absent.");
        return false;
    }

    FActorSpawnParameters Parameters;
    Parameters.Name = TEXT("TRIADIstanaExploreLandscapeV2");
    Parameters.OverrideLevel = TargetWorld->PersistentLevel;
    Parameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV2LandscapeActor* Landscape =
        TargetWorld->SpawnActor<ATRIADIstanaExploreV2LandscapeActor>(
            ATRIADIstanaExploreV2LandscapeActor::StaticClass(),
            FTransform::Identity,
            Parameters);
    if (!Landscape)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: V2 landscape actor could not be spawned.");
        return false;
    }
    Landscape->Tags.AddUnique(V2LandscapeTag);
    Landscape->SetActorLabel(TEXT("TRIAD Istana Explore V2 Reference-Aligned Landscape Prototype"));
    if (!Landscape->RecordPreservedDistantContext(
            Scene->OSMContextBuildingsComponent.Get(), Error) ||
        !Landscape->ConfigureExploreV2Assets(
            Broadleaf,
            Broadleaf,
            Broadleaf,
            Palm,
            ShrubA,
            ShrubB,
            Hedge,
            Groundcover,
            Grass,
            Grass,
            Cylinder,
            Error) ||
        !Landscape->ConfigureWindMaterials(
            LoadExact<UMaterialInterface>(TrunkWindObjectPath),
            TrunkSlot,
            LoadExact<UMaterialInterface>(BranchWindObjectPath),
            BranchSlot,
            LoadExact<UMaterialInterface>(LeafWindObjectPath),
            LeafSlot,
            LoadExact<UMaterialInterface>(GrassWindObjectPath),
            0,
            Error) ||
        !Landscape->PopulateDeterministicLandscape(
            OuterTreeTransforms, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V2_MAP: ") + Error;
        return false;
    }
    TargetWorld->MarkPackageDirty();
    FString BeforeSave;
    if (!ValidateWorld(TargetWorld, BeforeSave) ||
        !Assets->SaveLoadedAsset(TargetWorld, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V2_MAP: ") + BeforeSave;
        return false;
    }
    FString Filename;
    UWorld* Reloaded = FPackageName::DoesPackageExist(
            DestinationMapPackage, &Filename)
        ? UEditorLoadingAndSavingUtils::LoadMap(Filename)
        : nullptr;
    FString Persisted;
    if (!Reloaded || !ValidateWorld(Reloaded, Persisted) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V2_PERSISTENCE_FAILED: ") +
            (!Error.IsEmpty() ? Error : Persisted);
        return false;
    }
    OutMessage = TEXT("Created additive /Game/Maps/Istana_PublicView_Explore_v2 from Explore V1 with 720 safe-LOD trees (deterministically replanted at all 560 inherited 250-1000 m positions, including 532 outer high-detail broadleaf positions and 28 sparse procedural palm-proxy positions), taller varied tropical canopy habits, tiered turf/edge grass, trunk-branch-leaf under-damped gust recovery, unchanged free-roam controls and preserved OSM/HDB context. This is reference-aligned, not a hyperreal, botanical-inventory or survey-grade reconstruction. ") + Persisted;
    return true;
}

bool UTRIADIstanaExploreV2EditorLibrary::ValidateIstanaExploreV2Map(
    FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateWorld(World, OutReport);
}

bool UTRIADIstanaExploreV2EditorLibrary::ValidateIstanaExploreV2PlayWorld(
    FString& OutReport)
{
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorReport;
    if (!ValidateWorld(EditorWorld, EditorReport))
    {
        OutReport = TEXT("Explore V2 PIE editor-map validation failed: ") + EditorReport;
        return false;
    }
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE ||
        !PlayWorld->IsGameWorld() || !PlayWorld->HasBegunPlay() ||
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()) !=
            DestinationMapPackage)
    {
        OutReport = TEXT("Explore V2 PIE is absent, not begun, or sourced from the wrong map.");
        return false;
    }
    APlayerController* Player = UGameplayStatics::GetPlayerController(
        PlayWorld, 0);
    ATRIADIstanaFreeRoamPawn* Pawn = Player
        ? Cast<ATRIADIstanaFreeRoamPawn>(Player->GetPawn())
        : nullptr;
    ATRIADIstanaExploreV2LandscapeActor* Landscape =
        FindV2Landscape(PlayWorld);
    FString LandscapeReport;
    if (!Player || !Pawn || Player->GetViewTarget() != Pawn ||
        !Landscape || !Landscape->HasActorBegunPlay() ||
        !Landscape->IsWindRuntimeActive() ||
        !Landscape->ValidateExploreV2Landscape(LandscapeReport))
    {
        OutReport = TEXT("EXPLORE_V2_PIE_INVALID: free-roam possession or live wind/landscape contract failed. ") +
            LandscapeReport;
        return false;
    }
    OutReport = TEXT("Explore V2 PIE is live: Player 0 owns the free-roam pawn and the finite deterministic gust/recovery vegetation controller is updating instanced wind materials.");
    return true;
}

bool UTRIADIstanaExploreV2EditorLibrary::CaptureIstanaExploreV2PlayView(
    const FString& OutputFileName,
    FString& OutMessage)
{
    if (!OutputFileName.StartsWith(TEXT("explore_v2_"), ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName)
    {
        OutMessage = TEXT("Explore V2 PIE captures require one clean 'explore_v2_*.png' filename.");
        return false;
    }

    FString Validation;
    if (!ValidateIstanaExploreV2PlayWorld(Validation))
    {
        OutMessage = TEXT("Explore V2 PIE capture requires the validated live free-roam world. ") + Validation;
        return false;
    }
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    UGameViewportClient* ViewportClient = PlayWorld ? PlayWorld->GetGameViewport() : nullptr;
    FSceneViewport* Viewport = ViewportClient ? ViewportClient->GetGameViewport() : nullptr;
    if (!Viewport)
    {
        OutMessage = TEXT("The validated Explore V2 Player 0 viewport is unavailable.");
        return false;
    }

    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV2"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        OutMessage = TEXT("Could not create the Explore V2 QA capture directory.");
        return false;
    }
    const FString Destination = FPaths::Combine(Directory, OutputFileName);
    if (IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested())
    {
        OutMessage = TEXT("Explore V2 capture refused an overwrite or overlapping screenshot request.");
        return false;
    }

    FScreenshotRequest::RequestScreenshot(Destination, false, false, false);
    Viewport->Draw(false);
    OutMessage = FString::Printf(
        TEXT("Captured the validated Explore V2 Player 0 view to '%s'."),
        *Destination);
    return true;
}

bool UTRIADIstanaExploreV2EditorLibrary::
    QuiesceIstanaExploreV2PlayWorldForStop(FString& OutMessage)
{
    FString Readiness;
    if (!ValidateIstanaExploreV2PlayWorld(Readiness))
    {
        OutMessage = TEXT("Refusing scripted Explore V2 PIE stop because exact runtime identity failed. ") +
            Readiness;
        return false;
    }
    OutMessage = TEXT("Explore V2 PIE identity, free-roam possession and wind runtime are exact; no AirSim HUD/SimMode exists, so scripted stop may proceed immediately.");
    return true;
}
