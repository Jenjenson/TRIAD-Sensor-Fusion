#include "TRIADIstanaExploreV5DTreeRealismEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/UnrealMemory.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace
{
class FScopedTreeAssetSaveValidationSuppression final
{
public:
    bool Initialize(FString& OutError)
    {
        UClass* SettingsClass = FindObject<UClass>(
            nullptr,
            TEXT("/Script/DataValidation.DataValidationSettings"));
        SettingsObject = SettingsClass
            ? SettingsClass->GetDefaultObject()
            : nullptr;
        ValidateOnSaveProperty = SettingsClass
            ? FindFProperty<FBoolProperty>(
                  SettingsClass,
                  TEXT("bValidateOnSave"))
            : nullptr;
        if (!SettingsObject || !ValidateOnSaveProperty)
        {
            OutError = TEXT("The exact DataValidation save-hook setting could not be resolved; bounded duplicate validation suppression is refused.");
            return false;
        }
        bPriorValidateOnSave =
            ValidateOnSaveProperty->GetPropertyValue_InContainer(
                SettingsObject);
        ValidateOnSaveProperty->SetPropertyValue_InContainer(
            SettingsObject,
            false);
        if (ValidateOnSaveProperty->GetPropertyValue_InContainer(
                SettingsObject))
        {
            OutError = TEXT("The duplicate validate-on-save hook remained enabled; bounded tree asset creation is refused.");
            SettingsObject = nullptr;
            ValidateOnSaveProperty = nullptr;
            return false;
        }
        bMustRestore = true;
        OutError.Reset();
        return true;
    }

    ~FScopedTreeAssetSaveValidationSuppression()
    {
        if (bMustRestore && SettingsObject && ValidateOnSaveProperty)
        {
            ValidateOnSaveProperty->SetPropertyValue_InContainer(
                SettingsObject,
                bPriorValidateOnSave);
        }
    }

private:
    UObject* SettingsObject = nullptr;
    FBoolProperty* ValidateOnSaveProperty = nullptr;
    bool bPriorValidateOnSave = true;
    bool bMustRestore = false;
};

const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString MeshNamespace(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes"));
const FString MaterialNamespace(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials"));
const FString TreeBaseTransitionMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_SoilMulch_Layered.M_IPV5D_SoilMulch_Layered"));

bool ReleaseStaticMeshDescriptionCacheMemoryBounded(
    UStaticMesh* Mesh,
    const FString& Role,
    int32 FormIndex,
    FString& OutError)
{
    if (!Mesh || !Mesh->GetOutermost())
    {
        OutError = FString::Printf(
            TEXT("V5D tree %s %d is null or outerless during cache release."),
            *Role,
            FormIndex);
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (Mesh->IsCompiling() || !RenderData ||
        RenderData->LODResources.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("V5D tree %s %d did not reach synchronous render readiness before cache release."),
            *Role,
            FormIndex);
        return false;
    }
    const bool bWasDirty = Mesh->GetOutermost()->IsDirty();
    Mesh->ClearMeshDescriptions();
    Mesh->ClearHiResMeshDescription();
    if (Mesh->GetOutermost()->IsDirty() != bWasDirty)
    {
        OutError = FString::Printf(
            TEXT("V5D tree %s %d changed package dirtiness while releasing cache-only MeshDescriptions."),
            *Role,
            FormIndex);
        return false;
    }
    FMemory::Trim(true);
    OutError.Reset();
    return true;
}

bool SaveStaticMeshPackageDirectWithoutThumbnail(
    UStaticMesh* Mesh,
    int32 FormIndex,
    FString& OutError)
{
    UPackage* Package = Mesh ? Mesh->GetOutermost() : nullptr;
    if (!Mesh || !Package ||
        FormIndex < 0 || FormIndex >= 5 ||
        Package->GetName() != MeshNamespace + TEXT("/") + Mesh->GetName() ||
        FPackageName::IsTempPackage(Package->GetName()))
    {
        OutError = TEXT("The bounded V5D tree package failed its exact direct-save boundary.");
        return false;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(),
        FPackageName::GetAssetPackageExtension());
    if (Filename.IsEmpty())
    {
        OutError = TEXT("The bounded V5D tree package has no exact native asset filename.");
        return false;
    }

    // UEditorAssetSubsystem routes through InternalPromptForCheckoutAndSave,
    // which renders a new thumbnail.  The 114 MiB columnar canopy's thumbnail
    // transient alone can exceed the fixed 12 GiB private-memory ceiling.
    // Saving the already-loaded package directly preserves every serialized
    // mesh payload and its existing thumbnail while avoiding that unrelated
    // editor preview render.
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_None;
    SaveArgs.bSlowTask = false;
    if (!UPackage::SavePackage(Package, Mesh, *Filename, SaveArgs))
    {
        OutError = TEXT("The exact bounded V5D tree package direct save failed.");
        return false;
    }
    UPackage::WaitForAsyncFileWrites();
    OutError.Reset();
    return true;
}

const FString MeshAssetNames[] = {
    TEXT("SM_IPV5D_Tree_Umbrella_NearLOD0"),
    TEXT("SM_IPV5D_Tree_Dome_NearLOD0"),
    TEXT("SM_IPV5D_Tree_HighForkRounded_NearLOD0"),
    TEXT("SM_IPV5D_Tree_ColumnarNarrow_NearLOD0"),
    TEXT("SM_IPV5D_Tree_Palm_NearLOD0")};
static_assert(UE_ARRAY_COUNT(MeshAssetNames) == 5);

constexpr int32 TreeMaterialCount = 13;
constexpr int32 TreeFormCount = 5;
const int32 TreeMaterialOffsets[] = {0, 3, 6, 9, 12, 13};
static_assert(UE_ARRAY_COUNT(TreeMaterialOffsets) == TreeFormCount + 1);

enum class ETreeMaterialResponseKind : uint8
{
    Bark,
    Canopy,
    PalmComposite
};

struct FTreeMaterialResponseSpec
{
    const TCHAR* AssetName;
    const TCHAR* SourceObjectPath;
    ETreeMaterialResponseKind Kind;
    FLinearColor TintLow;
    FLinearColor TintHigh;
    float LumaLow;
    float LumaHigh;
    float DesaturationFraction;
    float RoughnessMin;
    float RoughnessMax;
    float BaseColorMax;
};

// Exact mesh-form/slot order: umbrella 3, dome 3, high-fork 3,
// columnar 3, palm 1. Tint values are luminance-normalized again at graph
// construction; they are visual response assumptions, never species claims.
const FTreeMaterialResponseSpec TreeResponseSpecs[] = {
    {TEXT("M_IPV5D_Tree_Umbrella_Trunk_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree02_Trunk_Wind.M_IPV4_IslandTree02_Trunk_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(0.95f, 1.00f, 1.03f), FLinearColor(1.02f, 1.00f, 0.96f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_Umbrella_Leaves_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree02_Leaves_Wind.M_IPV4_IslandTree02_Leaves_Wind"), ETreeMaterialResponseKind::Canopy, FLinearColor(0.91f, 1.08f, 0.91f), FLinearColor(0.98f, 1.04f, 0.94f), 0.96f, 1.04f, -0.07f, 0.58f, 0.82f, 0.80f},
    {TEXT("M_IPV5D_Tree_Umbrella_Branches_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree02_Branches_Wind.M_IPV4_IslandTree02_Branches_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(0.95f, 1.00f, 1.03f), FLinearColor(1.02f, 1.00f, 0.96f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_Dome_Branches_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_TreeSmall02_Branches_Wind.M_IPV4_TreeSmall02_Branches_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(1.00f, 0.99f, 0.95f), FLinearColor(1.03f, 1.00f, 0.97f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_Dome_Leaves_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_TreeSmall02_Leaves_Wind.M_IPV4_TreeSmall02_Leaves_Wind"), ETreeMaterialResponseKind::Canopy, FLinearColor(0.88f, 1.07f, 0.95f), FLinearColor(0.94f, 1.02f, 0.98f), 0.96f, 1.04f, -0.07f, 0.58f, 0.82f, 0.80f},
    {TEXT("M_IPV5D_Tree_Dome_Trunk_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_TreeSmall02_Trunk_Wind.M_IPV4_TreeSmall02_Trunk_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(1.00f, 0.99f, 0.95f), FLinearColor(1.03f, 1.00f, 0.97f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_HighFork_Trunk_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree01_Trunk_Wind.M_IPV4_IslandTree01_Trunk_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(0.97f, 0.99f, 0.98f), FLinearColor(1.03f, 1.00f, 0.95f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_HighFork_Leaves_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree01_Leaves_Wind.M_IPV4_IslandTree01_Leaves_Wind"), ETreeMaterialResponseKind::Canopy, FLinearColor(0.94f, 1.08f, 0.86f), FLinearColor(1.01f, 1.02f, 0.91f), 0.96f, 1.04f, -0.07f, 0.58f, 0.82f, 0.80f},
    {TEXT("M_IPV5D_Tree_HighFork_Branches_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree01_Branches_Wind.M_IPV4_IslandTree01_Branches_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(0.97f, 0.99f, 0.98f), FLinearColor(1.03f, 1.00f, 0.95f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_Columnar_Branches_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_BroadleafBranches_Wind.M_IPV4_BroadleafBranches_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(0.96f, 0.99f, 1.02f), FLinearColor(1.02f, 1.00f, 0.97f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_Columnar_Trunk_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_BroadleafTrunk_Wind.M_IPV4_BroadleafTrunk_Wind"), ETreeMaterialResponseKind::Bark, FLinearColor(0.96f, 0.99f, 1.02f), FLinearColor(1.02f, 1.00f, 0.97f), 0.97f, 1.03f, 0.00f, 0.68f, 0.90f, 0.75f},
    {TEXT("M_IPV5D_Tree_Columnar_Leaves_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_BroadleafLeavesDark_Wind.M_IPV4_BroadleafLeavesDark_Wind"), ETreeMaterialResponseKind::Canopy, FLinearColor(0.85f, 1.07f, 0.93f), FLinearColor(0.91f, 1.01f, 0.97f), 0.96f, 1.04f, -0.07f, 0.58f, 0.82f, 0.80f},
    {TEXT("M_IPV5D_Tree_Palm_Composite_Response"), TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_QuaterniusPalm_Atlas_Wind.M_IPV4_QuaterniusPalm_Atlas_Wind"), ETreeMaterialResponseKind::PalmComposite, FLinearColor(0.95f, 1.03f, 0.93f), FLinearColor(0.99f, 1.01f, 0.96f), 0.98f, 1.02f, -0.02f, 0.62f, 0.88f, 0.82f}};
static_assert(UE_ARRAY_COUNT(TreeResponseSpecs) == TreeMaterialCount);

const FString WindCustomDescription(
    TEXT("TRIAD_EXPLORE_V4_INSTANCE_LOCAL_PIVOT_UNDERDAMPED_WPO_V1"));
const FString WindCustomCode(
    TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\n")
    TEXT("float h = saturate(max(InstanceLocalPosition.z, 0.0) / max(HeightCm, 1.0));\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + InstanceRandom * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023));\n")
    TEXT("float wave = sin(phase) + 0.31 * sin(phase * 1.73 + 1.2);\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float bend = clamp(WindStrengthCm * ResponseScale * wave * h * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float lift = clamp(abs(WindStrengthCm) * ResponseScale * 0.025 * sin(phase * 0.71) * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("return float3(direction * bend, lift);"));

FString PackagePath(const FString& AssetName)
{
    return MeshNamespace + TEXT("/") + AssetName;
}

FString ObjectPath(const FString& AssetName)
{
    return PackagePath(AssetName) + TEXT(".") + AssetName;
}

FString MaterialPackagePath(const FString& AssetName)
{
    return MaterialNamespace + TEXT("/") + AssetName;
}

FString MaterialObjectPath(const FString& AssetName)
{
    return MaterialPackagePath(AssetName) + TEXT(".") + AssetName;
}

int32 FlattenedTreeMaterialIndex(int32 FormIndex, int32 Slot)
{
    if (FormIndex < 0 || FormIndex >= TreeFormCount || Slot < 0 ||
        Slot >= TreeMaterialOffsets[FormIndex + 1] -
            TreeMaterialOffsets[FormIndex])
    {
        return INDEX_NONE;
    }
    return TreeMaterialOffsets[FormIndex] + Slot;
}

FLinearColor NormalizeTintLuminance(const FLinearColor& Tint)
{
    const float Luminance =
        Tint.R * 0.2126f + Tint.G * 0.7152f + Tint.B * 0.0722f;
    return Luminance > 0.0001f
        ? FLinearColor(
              Tint.R / Luminance,
              Tint.G / Luminance,
              Tint.B / Luminance,
              1.0f)
        : FLinearColor::White;
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& Path)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

template <typename TActorType>
TActorType* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    TActorType* Match = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActorType> It(World); It; ++It)
    {
        ++OutCount;
        Match = *It;
    }
    return OutCount == 1 ? Match : nullptr;
}

void GetSourceMeshes(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    TArray<UStaticMesh*>& OutMeshes)
{
    OutMeshes.Reset();
    if (!V4)
    {
        return;
    }
    const UHierarchicalInstancedStaticMeshComponent* Components[] = {
        V4->UmbrellaTreeInstances,
        V4->DomeTreeInstances,
        V4->HighForkRoundedTreeInstances,
        V4->ColumnarNarrowTreeInstances,
        V4->PalmTreeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        OutMeshes.Add(Component ? Component->GetStaticMesh() : nullptr);
    }
}

bool GetSourceTreeMaterials(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    const UHierarchicalInstancedStaticMeshComponent* SourceComponents[] = {
        V4 ? V4->UmbrellaTreeInstances.Get() : nullptr,
        V4 ? V4->DomeTreeInstances.Get() : nullptr,
        V4 ? V4->HighForkRoundedTreeInstances.Get() : nullptr,
        V4 ? V4->ColumnarNarrowTreeInstances.Get() : nullptr,
        V4 ? V4->PalmTreeInstances.Get() : nullptr};
    static_assert(UE_ARRAY_COUNT(SourceComponents) == TreeFormCount);
    TArray<UStaticMesh*> Sources;
    GetSourceMeshes(V4, Sources);
    OutMaterials.Reset();
    if (Sources.Num() != TreeFormCount || Sources.Contains(nullptr))
    {
        OutError = TEXT("The five exact V4 tree meshes are required before material-response authoring.");
        return false;
    }
    for (int32 FormIndex = 0; FormIndex < TreeFormCount; ++FormIndex)
    {
        if (!SourceComponents[FormIndex] ||
            SourceComponents[FormIndex]->GetStaticMesh() != Sources[FormIndex])
        {
            OutError = FString::Printf(
                TEXT("V4 tree form %d no longer has its exact effective source component/mesh binding."),
                FormIndex);
            return false;
        }
        const int32 ExpectedSlots =
            TreeMaterialOffsets[FormIndex + 1] -
            TreeMaterialOffsets[FormIndex];
        if (Sources[FormIndex]->GetStaticMaterials().Num() != ExpectedSlots)
        {
            OutError = FString::Printf(
                TEXT("V4 tree form %d no longer has its exact material-slot count."),
                FormIndex);
            return false;
        }
        for (int32 Slot = 0; Slot < ExpectedSlots; ++Slot)
        {
            const int32 MaterialIndex =
                FlattenedTreeMaterialIndex(FormIndex, Slot);
            UMaterialInterface* Interface =
                SourceComponents[FormIndex]->GetMaterial(Slot);
            UMaterial* Material = Cast<UMaterial>(Interface);
            if (MaterialIndex == INDEX_NONE || !Material ||
                Material->GetPathName() !=
                    TreeResponseSpecs[MaterialIndex].SourceObjectPath)
            {
                OutError = FString::Printf(
                    TEXT("V4 tree form %d slot %d is not the admitted exact wind-material source: actual=%s class=%s expected=%s."),
                    FormIndex,
                    Slot,
                    Interface ? *Interface->GetPathName() : TEXT("<null>"),
                    Interface ? *Interface->GetClass()->GetPathName() : TEXT("<null>"),
                    MaterialIndex != INDEX_NONE
                        ? TreeResponseSpecs[MaterialIndex].SourceObjectPath
                        : TEXT("<invalid-index>"));
                return false;
            }
            OutMaterials.Add(Material);
        }
    }
    if (OutMaterials.Num() != TreeMaterialCount)
    {
        OutError = TEXT("The exact 13-material V4 tree roster is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

template <typename TExpression>
TExpression* AddResponseExpression(
    UMaterial* Material,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    TExpression* Expression = EditorOnly
        ? NewObject<TExpression>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpression* AddResponseExpressionByClassPath(
    UMaterial* Material,
    const TCHAR* ClassPath,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UClass* ExpressionClass = EditorOnly
        ? FindObject<UClass>(nullptr, ClassPath)
        : nullptr;
    if (!ExpressionClass && EditorOnly)
    {
        ExpressionClass = LoadObject<UClass>(nullptr, ClassPath);
    }
    UMaterialExpression* Expression =
        ExpressionClass &&
            ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? NewObject<UMaterialExpression>(
              Material,
              ExpressionClass,
              NAME_None,
              RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpressionScalarParameter* AddResponseScalar(
    UMaterial* Material,
    const TCHAR* Description,
    const TCHAR* ParameterName,
    float DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            Description,
            X,
            Y);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = TEXT("Istana Explore V5D Tree Response");
        Parameter->DefaultValue = DefaultValue;
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* AddResponseVector(
    UMaterial* Material,
    const TCHAR* Description,
    const TCHAR* ParameterName,
    const FLinearColor& DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionVectorParameter* Parameter =
        AddResponseExpression<UMaterialExpressionVectorParameter>(
            Material,
            Description,
            X,
            Y);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = TEXT("Istana Explore V5D Tree Response");
        Parameter->DefaultValue = DefaultValue;
    }
    return Parameter;
}

bool HasExactPreservedWindGraph(
    const UMaterial* Material,
    FString& OutError)
{
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionCustom* Wind = nullptr;
    int32 CustomCount = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                ++CustomCount;
                if (Custom->Description == WindCustomDescription &&
                    Custom->Code == WindCustomCode)
                {
                    Wind = Custom;
                }
            }
        }
    }
    const FName Scalars[] = {
        TEXT("TRIAD_WindStrengthCm"),
        TEXT("TRIAD_WindSpeed"),
        TEXT("TRIAD_WindHeightCm"),
        TEXT("TRIAD_WindResponseScale"),
        TEXT("TRIAD_MaxWpoCm")};
    float ScalarValue = 0.0f;
    FLinearColor Direction = FLinearColor::Black;
    if (!EditorOnly || !Wind || CustomCount != 1 ||
        EditorOnly->WorldPositionOffset.Expression != Wind ||
        !Material->GetVectorParameterValue(
            FMaterialParameterInfo(TEXT("TRIAD_WindDirection")),
            Direction))
    {
        OutError = TEXT("A tree material lost the exact sole V4 WPO graph or wind-direction parameter.");
        return false;
    }
    for (const FName Parameter : Scalars)
    {
        if (!Material->GetScalarParameterValue(
                FMaterialParameterInfo(Parameter),
                ScalarValue) ||
            !FMath::IsFinite(ScalarValue))
        {
            OutError = TEXT("A tree material lost one of the exact five scalar wind parameters.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool WrapExactTreeMaterialResponse(
    UMaterial* Material,
    const FTreeMaterialResponseSpec& Spec,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->BaseColor.Expression ||
        !EditorOnly->Roughness.Expression ||
        Material->GetPathName() != MaterialObjectPath(Spec.AssetName) ||
        !FMath::IsNearlyEqual(Material->OpacityMaskClipValue, 0.333f, 0.0001f) ||
        !HasExactPreservedWindGraph(Material, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("An isolated tree response material cannot wrap the exact admitted outputs.");
        }
        return false;
    }

    UMaterialExpression* OriginalBase = EditorOnly->BaseColor.Expression;
    const int32 OriginalBaseOutput = EditorOnly->BaseColor.OutputIndex;
    UMaterialExpression* OriginalRoughness =
        EditorOnly->Roughness.Expression;
    const int32 OriginalRoughnessOutput =
        EditorOnly->Roughness.OutputIndex;
    UMaterialExpression* OriginalOpacity =
        EditorOnly->OpacityMask.Expression;
    const int32 OriginalOpacityOutput =
        EditorOnly->OpacityMask.OutputIndex;
    UMaterialExpression* OriginalWpo =
        EditorOnly->WorldPositionOffset.Expression;
    const int32 OriginalWpoOutput =
        EditorOnly->WorldPositionOffset.OutputIndex;
    UMaterialExpression* OriginalSubsurface =
        EditorOnly->SubsurfaceColor.Expression;
    const int32 OriginalSubsurfaceOutput =
        EditorOnly->SubsurfaceColor.OutputIndex;

    UMaterialExpression* InstanceRandom = AddResponseExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"),
        TEXT("TRIAD_V5D_TREE_RESPONSE_PER_INSTANCE_RANDOM"),
        320,
        -640);
    UMaterialExpressionVectorParameter* TintLow = AddResponseVector(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_TINT_LOW"),
        TEXT("TRIAD_TreeResponseTintLow"),
        NormalizeTintLuminance(Spec.TintLow),
        320,
        -540);
    UMaterialExpressionVectorParameter* TintHigh = AddResponseVector(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_TINT_HIGH"),
        TEXT("TRIAD_TreeResponseTintHigh"),
        NormalizeTintLuminance(Spec.TintHigh),
        320,
        -440);
    UMaterialExpressionLinearInterpolate* TintBlend =
        AddResponseExpression<UMaterialExpressionLinearInterpolate>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_TINT_BLEND"),
            560,
            -500);
    UMaterialExpressionScalarParameter* LumaLow = AddResponseScalar(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_LUMA_LOW"),
        TEXT("TRIAD_TreeResponseLumaLow"),
        Spec.LumaLow,
        320,
        -330);
    UMaterialExpressionScalarParameter* LumaHigh = AddResponseScalar(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_LUMA_HIGH"),
        TEXT("TRIAD_TreeResponseLumaHigh"),
        Spec.LumaHigh,
        320,
        -240);
    UMaterialExpressionLinearInterpolate* LumaBlend =
        AddResponseExpression<UMaterialExpressionLinearInterpolate>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_LUMA_BLEND"),
            560,
            -285);
    UMaterialExpressionMultiply* TintedBase =
        AddResponseExpression<UMaterialExpressionMultiply>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_TINTED_BASE"),
            790,
            -500);
    UMaterialExpressionMultiply* VariedBase =
        AddResponseExpression<UMaterialExpressionMultiply>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_LUMA_VARIED_BASE"),
            1010,
            -500);
    UMaterialExpressionScalarParameter* DesaturationAmount =
        AddResponseScalar(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_DESATURATION"),
            TEXT("TRIAD_TreeResponseDesaturationFraction"),
            Spec.DesaturationFraction,
            790,
            -330);
    UMaterialExpressionDesaturation* ChromaResponse =
        AddResponseExpression<UMaterialExpressionDesaturation>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_CHROMA"),
            1230,
            -500);
    UMaterialExpressionScalarParameter* BaseColorMax = AddResponseScalar(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_BASE_COLOR_MAX"),
        TEXT("TRIAD_TreeResponseBaseColorMax"),
        Spec.BaseColorMax,
        1010,
        -330);
    UMaterialExpressionClamp* ClampedBase =
        AddResponseExpression<UMaterialExpressionClamp>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_BASE_COLOR_CLAMP"),
            1450,
            -500);
    UMaterialExpressionScalarParameter* RoughnessMin = AddResponseScalar(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_ROUGHNESS_MIN"),
        TEXT("TRIAD_TreeResponseRoughnessMin"),
        Spec.RoughnessMin,
        790,
        -80);
    UMaterialExpressionScalarParameter* RoughnessMax = AddResponseScalar(
        Material,
        TEXT("TRIAD_V5D_TREE_RESPONSE_ROUGHNESS_MAX"),
        TEXT("TRIAD_TreeResponseRoughnessMax"),
        Spec.RoughnessMax,
        790,
        20);
    UMaterialExpressionClamp* ClampedRoughness =
        AddResponseExpression<UMaterialExpressionClamp>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_ROUGHNESS_CLAMP"),
            1230,
            -30);
    if (!InstanceRandom || !TintLow || !TintHigh || !TintBlend ||
        !LumaLow || !LumaHigh || !LumaBlend || !TintedBase || !VariedBase ||
        !DesaturationAmount || !ChromaResponse || !BaseColorMax ||
        !ClampedBase || !RoughnessMin || !RoughnessMax ||
        !ClampedRoughness)
    {
        OutError = TEXT("Could not allocate the complete bounded tree response graph.");
        return false;
    }

    TintBlend->A.Connect(0, TintLow);
    TintBlend->B.Connect(0, TintHigh);
    TintBlend->Alpha.Connect(0, InstanceRandom);
    LumaBlend->A.Connect(0, LumaLow);
    LumaBlend->B.Connect(0, LumaHigh);
    LumaBlend->Alpha.Connect(0, InstanceRandom);
    TintedBase->A.Connect(OriginalBaseOutput, OriginalBase);
    TintedBase->B.Connect(0, TintBlend);
    VariedBase->A.Connect(0, TintedBase);
    VariedBase->B.Connect(0, LumaBlend);
    ChromaResponse->LuminanceFactors =
        FLinearColor(0.2126f, 0.7152f, 0.0722f, 0.0f);
    ChromaResponse->Input.Connect(0, VariedBase);
    ChromaResponse->Fraction.Connect(0, DesaturationAmount);
    ClampedBase->Input.Connect(0, ChromaResponse);
    ClampedBase->ClampMode = CMODE_Clamp;
    ClampedBase->MinDefault = 0.0f;
    ClampedBase->Max.Connect(0, BaseColorMax);
    ClampedRoughness->Input.Connect(
        OriginalRoughnessOutput,
        OriginalRoughness);
    ClampedRoughness->ClampMode = CMODE_Clamp;
    ClampedRoughness->Min.Connect(0, RoughnessMin);
    ClampedRoughness->Max.Connect(0, RoughnessMax);
    EditorOnly->BaseColor.Connect(0, ClampedBase);
    EditorOnly->Roughness.Connect(0, ClampedRoughness);

    if (Spec.Kind == ETreeMaterialResponseKind::Canopy)
    {
        UMaterialExpressionVectorParameter* SubsurfaceTint =
            AddResponseVector(
                Material,
                TEXT("TRIAD_V5D_TREE_RESPONSE_SUBSURFACE_TINT"),
                TEXT("TRIAD_TreeResponseSubsurfaceTint"),
                FLinearColor(0.72f, 0.94f, 0.68f, 1.0f),
                1230,
                -690);
        UMaterialExpressionMultiply* SubsurfaceResponse =
            AddResponseExpression<UMaterialExpressionMultiply>(
                Material,
                TEXT("TRIAD_V5D_TREE_RESPONSE_SUBSURFACE"),
                1450,
                -650);
        if (!SubsurfaceTint || !SubsurfaceResponse || !OriginalSubsurface)
        {
            OutError = TEXT("A canopy response could not preserve and tint its exact subsurface output.");
            return false;
        }
        SubsurfaceResponse->A.Connect(
            OriginalSubsurfaceOutput,
            OriginalSubsurface);
        SubsurfaceResponse->B.Connect(0, SubsurfaceTint);
        EditorOnly->SubsurfaceColor.Connect(0, SubsurfaceResponse);
    }

    // These outputs are explicitly immutable in the amendment.
    if (EditorOnly->OpacityMask.Expression != OriginalOpacity ||
        EditorOnly->OpacityMask.OutputIndex != OriginalOpacityOutput ||
        EditorOnly->WorldPositionOffset.Expression != OriginalWpo ||
        EditorOnly->WorldPositionOffset.OutputIndex != OriginalWpoOutput)
    {
        OutError = TEXT("Tree response authoring touched opacity or WPO and was refused.");
        return false;
    }

    Material->Modify();
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutError.Reset();
    return true;
}

template <typename TExpression>
const TExpression* FindSingleResponseExpression(
    const UMaterial* Material,
    const TCHAR* Description)
{
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const TExpression* Match = nullptr;
    int32 Count = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const TExpression* Candidate = Cast<TExpression>(Expression);
                Candidate && Candidate->Desc == Description)
            {
                Match = Candidate;
                ++Count;
            }
        }
    }
    return Count == 1 ? Match : nullptr;
}

bool InputsHaveEquivalentHeads(
    const FExpressionInput& Candidate,
    const FExpressionInput& Source)
{
    if (!Candidate.Expression || !Source.Expression)
    {
        return Candidate.Expression == Source.Expression;
    }
    return Candidate.OutputIndex == Source.OutputIndex &&
        Candidate.Expression->GetClass()->GetPathName() ==
            Source.Expression->GetClass()->GetPathName() &&
        Candidate.Expression->Desc == Source.Expression->Desc;
}

bool ValidateTreeMaterialResponse(
    const UMaterial* Material,
    const UMaterial* Source,
    const FTreeMaterialResponseSpec& Spec,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialEditorOnlyData* SourceEditorOnly = Source
        ? Source->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionClamp* BaseClamp =
        FindSingleResponseExpression<UMaterialExpressionClamp>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_BASE_COLOR_CLAMP"));
    const UMaterialExpressionClamp* RoughnessClamp =
        FindSingleResponseExpression<UMaterialExpressionClamp>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_ROUGHNESS_CLAMP"));
    const UMaterialExpressionVectorParameter* TintLow =
        FindSingleResponseExpression<UMaterialExpressionVectorParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_TINT_LOW"));
    const UMaterialExpressionVectorParameter* TintHigh =
        FindSingleResponseExpression<UMaterialExpressionVectorParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_TINT_HIGH"));
    const UMaterialExpressionScalarParameter* LumaLow =
        FindSingleResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_LUMA_LOW"));
    const UMaterialExpressionScalarParameter* LumaHigh =
        FindSingleResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_LUMA_HIGH"));
    const UMaterialExpressionScalarParameter* Desaturation =
        FindSingleResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_DESATURATION"));
    const UMaterialExpressionScalarParameter* RoughnessMin =
        FindSingleResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_ROUGHNESS_MIN"));
    const UMaterialExpressionScalarParameter* RoughnessMax =
        FindSingleResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_ROUGHNESS_MAX"));
    const UMaterialExpressionScalarParameter* BaseColorMax =
        FindSingleResponseExpression<UMaterialExpressionScalarParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_BASE_COLOR_MAX"));
    int32 PerInstanceRandomCount = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            PerInstanceRandomCount += Expression &&
                Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom") &&
                Expression->Desc ==
                    TEXT("TRIAD_V5D_TREE_RESPONSE_PER_INSTANCE_RANDOM")
                ? 1
                : 0;
        }
    }

    FString WindError;
    if (!Material || !Source || Material == Source || !EditorOnly ||
        !SourceEditorOnly ||
        Material->GetPathName() != MaterialObjectPath(Spec.AssetName) ||
        Source->GetPathName() != Spec.SourceObjectPath ||
        Material->MaterialDomain != Source->MaterialDomain ||
        Material->GetBlendMode() != Source->GetBlendMode() ||
        Material->IsTwoSided() != Source->IsTwoSided() ||
        Material->GetShadingModels() != Source->GetShadingModels() ||
        Material->bUsedWithInstancedStaticMeshes !=
            Source->bUsedWithInstancedStaticMeshes ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            Source->MaxWorldPositionOffsetDisplacement,
            0.0001f) ||
        !FMath::IsNearlyEqual(Material->OpacityMaskClipValue, 0.333f, 0.0001f) ||
        !FMath::IsNearlyEqual(
            Source->OpacityMaskClipValue,
            0.333f,
            0.0001f) ||
        !HasExactPreservedWindGraph(Material, WindError) ||
        !HasExactPreservedWindGraph(Source, WindError) ||
        !BaseClamp || !RoughnessClamp || !TintLow || !TintHigh ||
        !LumaLow || !LumaHigh || !Desaturation || !RoughnessMin ||
        !RoughnessMax || !BaseColorMax || PerInstanceRandomCount != 1 ||
        EditorOnly->BaseColor.Expression != BaseClamp ||
        EditorOnly->Roughness.Expression != RoughnessClamp ||
        !InputsHaveEquivalentHeads(
            EditorOnly->Normal,
            SourceEditorOnly->Normal) ||
        !InputsHaveEquivalentHeads(
            EditorOnly->OpacityMask,
            SourceEditorOnly->OpacityMask) ||
        TintLow->ParameterName != TEXT("TRIAD_TreeResponseTintLow") ||
        TintHigh->ParameterName != TEXT("TRIAD_TreeResponseTintHigh") ||
        !TintLow->DefaultValue.Equals(
            NormalizeTintLuminance(Spec.TintLow),
            0.0001f) ||
        !TintHigh->DefaultValue.Equals(
            NormalizeTintLuminance(Spec.TintHigh),
            0.0001f) ||
        LumaLow->ParameterName != TEXT("TRIAD_TreeResponseLumaLow") ||
        LumaHigh->ParameterName != TEXT("TRIAD_TreeResponseLumaHigh") ||
        Desaturation->ParameterName !=
            TEXT("TRIAD_TreeResponseDesaturationFraction") ||
        RoughnessMin->ParameterName !=
            TEXT("TRIAD_TreeResponseRoughnessMin") ||
        RoughnessMax->ParameterName !=
            TEXT("TRIAD_TreeResponseRoughnessMax") ||
        BaseColorMax->ParameterName !=
            TEXT("TRIAD_TreeResponseBaseColorMax") ||
        !FMath::IsNearlyEqual(LumaLow->DefaultValue, Spec.LumaLow, 0.0001f) ||
        !FMath::IsNearlyEqual(LumaHigh->DefaultValue, Spec.LumaHigh, 0.0001f) ||
        !FMath::IsNearlyEqual(
            Desaturation->DefaultValue,
            Spec.DesaturationFraction,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            RoughnessMin->DefaultValue,
            Spec.RoughnessMin,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            RoughnessMax->DefaultValue,
            Spec.RoughnessMax,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            BaseColorMax->DefaultValue,
            Spec.BaseColorMax,
            0.0001f))
    {
        OutError = TEXT("An isolated V5D tree response material failed exact graph/property validation: ") +
            FString(Spec.AssetName) + TEXT(" ") + WindError;
        return false;
    }

    const bool bCanopy = Spec.Kind == ETreeMaterialResponseKind::Canopy;
    const UMaterialExpressionVectorParameter* SubsurfaceTint =
        FindSingleResponseExpression<UMaterialExpressionVectorParameter>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_SUBSURFACE_TINT"));
    const UMaterialExpressionMultiply* SubsurfaceResponse =
        FindSingleResponseExpression<UMaterialExpressionMultiply>(
            Material,
            TEXT("TRIAD_V5D_TREE_RESPONSE_SUBSURFACE"));
    if (bCanopy)
    {
        if (!Material->IsTwoSided() || !SubsurfaceTint ||
            !SubsurfaceResponse ||
            EditorOnly->SubsurfaceColor.Expression != SubsurfaceResponse ||
            !InputsHaveEquivalentHeads(
                SubsurfaceResponse->A,
                SourceEditorOnly->SubsurfaceColor) ||
            SubsurfaceResponse->B.Expression != SubsurfaceTint ||
            SubsurfaceResponse->B.OutputIndex != 0 ||
            SubsurfaceTint->ParameterName !=
                TEXT("TRIAD_TreeResponseSubsurfaceTint") ||
            !SubsurfaceTint->DefaultValue.Equals(
                FLinearColor(0.72f, 0.94f, 0.68f, 1.0f),
                0.0001f))
        {
            OutError = TEXT("A canopy response lost its bounded subsurface response.");
            return false;
        }
    }
    else if (SubsurfaceTint || SubsurfaceResponse ||
             !InputsHaveEquivalentHeads(
                 EditorOnly->SubsurfaceColor,
                 SourceEditorOnly->SubsurfaceColor))
    {
        OutError = TEXT("A bark/palm response invented a subsurface channel.");
        return false;
    }

    const FName WindScalars[] = {
        TEXT("TRIAD_WindStrengthCm"),
        TEXT("TRIAD_WindSpeed"),
        TEXT("TRIAD_WindHeightCm"),
        TEXT("TRIAD_WindResponseScale"),
        TEXT("TRIAD_MaxWpoCm")};
    for (const FName Parameter : WindScalars)
    {
        float SourceValue = 0.0f;
        float ResponseValue = 0.0f;
        if (!Source->GetScalarParameterValue(
                FMaterialParameterInfo(Parameter),
                SourceValue) ||
            !Material->GetScalarParameterValue(
                FMaterialParameterInfo(Parameter),
                ResponseValue) ||
            !FMath::IsNearlyEqual(SourceValue, ResponseValue, 0.000001f))
        {
            OutError = TEXT("An isolated response changed a scalar wind default.");
            return false;
        }
    }
    FLinearColor SourceDirection = FLinearColor::Black;
    FLinearColor ResponseDirection = FLinearColor::Black;
    if (!Source->GetVectorParameterValue(
            FMaterialParameterInfo(TEXT("TRIAD_WindDirection")),
            SourceDirection) ||
        !Material->GetVectorParameterValue(
            FMaterialParameterInfo(TEXT("TRIAD_WindDirection")),
            ResponseDirection) ||
        !SourceDirection.Equals(ResponseDirection, 0.000001f))
    {
        OutError = TEXT("An isolated response changed the wind-direction default.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool LoadAndValidateTreeResponseMaterials(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    TArray<UMaterial*> Sources;
    if (!GetSourceTreeMaterials(V4, Sources, OutError))
    {
        return false;
    }
    OutMaterials.Reset();
    for (int32 Index = 0; Index < TreeMaterialCount; ++Index)
    {
        UMaterial* Material = LoadExact<UMaterial>(
            MaterialObjectPath(TreeResponseSpecs[Index].AssetName));
        if (!ValidateTreeMaterialResponse(
                Material,
                Sources[Index],
                TreeResponseSpecs[Index],
                OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    OutError.Reset();
    return true;
}

bool EnsureTreeResponseMaterials(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    TArray<UMaterial*> Sources;
    if (!GetSourceTreeMaterials(V4, Sources, OutError))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    int32 ExistingCount = 0;
    for (const FTreeMaterialResponseSpec& Spec : TreeResponseSpecs)
    {
        const FString Package = MaterialPackagePath(Spec.AssetName);
        ExistingCount += FPackageName::DoesPackageExist(Package) ||
            FindPackage(nullptr, *Package)
            ? 1
            : 0;
    }
    if (ExistingCount == TreeMaterialCount)
    {
        return LoadAndValidateTreeResponseMaterials(
            V4,
            OutMaterials,
            OutError);
    }
    if (ExistingCount != 0)
    {
        OutError = FString::Printf(
            TEXT("The isolated V5D tree-response namespace is partial (%d/13); overwrite or repair is refused."),
            ExistingCount);
        return false;
    }

    TArray<UObject*> FreshAssets;
    for (int32 Index = 0; Index < TreeMaterialCount; ++Index)
    {
        const FTreeMaterialResponseSpec& Spec = TreeResponseSpecs[Index];
        UPackage* Package = CreatePackage(
            *MaterialPackagePath(Spec.AssetName));
        UMaterial* Duplicate = Package
            ? Cast<UMaterial>(StaticDuplicateObject(
                  Sources[Index],
                  Package,
                  FName(Spec.AssetName),
                  RF_Public | RF_Standalone | RF_Transactional))
            : nullptr;
        if (!Duplicate || Duplicate == Sources[Index] ||
            !WrapExactTreeMaterialResponse(Duplicate, Spec, OutError) ||
            !ValidateTreeMaterialResponse(
                Duplicate,
                Sources[Index],
                Spec,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Could not create an exact isolated V5D tree response material.");
            }
            return false;
        }
        FAssetRegistryModule::AssetCreated(Duplicate);
        FreshAssets.Add(Duplicate);
    }
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!Assets || FreshAssets.Num() != TreeMaterialCount ||
        !Assets->SaveLoadedAssets(FreshAssets, false) ||
        !LoadAndValidateTreeResponseMaterials(
            V4,
            OutMaterials,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Only the exact 13 isolated response materials were offered to save, but validation failed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool LoadAndValidateMeshes(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    TArray<UStaticMesh*>& OutMeshes,
    FString& OutError)
{
    OutMeshes.Reset();
    for (const FString& AssetName : MeshAssetNames)
    {
        OutMeshes.Add(LoadExact<UStaticMesh>(ObjectPath(AssetName)));
    }
    FTRIADIstanaExploreV5DTreeRealismAssets Roster;
    for (UStaticMesh* Mesh : OutMeshes)
    {
        Roster.NearLodZeroTreeMeshes.Add(Mesh);
    }
    // These two fields are not part of mesh validation, but the runtime roster
    // validator deliberately requires the complete pass. Use exact V5B assets
    // at the call sites instead of weakening that gate here.
    int32 V5BCount = 0;
    ATRIADIstanaExploreV5BVisualActor* V5B =
        V4 ? FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(
                 V4->GetWorld(), V5BCount)
           : nullptr;
    if (V5BCount != 1 || !V5B || !V5B->TreeBaseMulchInstances)
    {
        OutError = TEXT("The exact V5B tree-base source is required to cold-validate V5D tree meshes.");
        return false;
    }
    Roster.TreeBaseTransitionMesh =
        V5B->TreeBaseMulchInstances->GetStaticMesh();
    Roster.TreeBaseTransitionMaterial =
        LoadExact<UMaterialInterface>(TreeBaseTransitionMaterialPath);
    return ATRIADIstanaExploreV5DTreeRealismActor::ValidateAssetRoster(
        Roster,
        V4,
        OutError);
}

bool RehomeExactSourceMeshDescriptions(
    UStaticMesh* Source,
    UStaticMesh* Duplicate,
    int32 FormIndex,
    FString& OutError)
{
    if (!Source || !Duplicate || Source == Duplicate ||
        Source->GetNumSourceModels() <= 0 ||
        Duplicate->GetNumSourceModels() != Source->GetNumSourceModels())
    {
        OutError = FString::Printf(
            TEXT("V5D tree derivative %d cannot own the exact source LOD roster."),
            FormIndex);
        return false;
    }

    int32 InstalledDescriptions = 0;
    for (int32 Lod = 0; Lod < Source->GetNumSourceModels(); ++Lod)
    {
        if (!Source->IsMeshDescriptionValid(Lod))
        {
            if (Lod == 0)
            {
                OutError = FString::Printf(
                    TEXT("V5D tree derivative %d has no exact source LOD0 MeshDescription."),
                    FormIndex);
                return false;
            }
            continue;
        }

        FMeshDescription SourceDescription;
        if (!Source->CloneMeshDescription(Lod, SourceDescription) ||
            SourceDescription.Vertices().Num() <= 0 ||
            SourceDescription.Triangles().Num() <= 0)
        {
            OutError = FString::Printf(
                TEXT("V5D tree derivative %d could not clone exact source LOD %d geometry."),
                FormIndex,
                Lod);
            return false;
        }
        const int32 SourceVertexCount = SourceDescription.Vertices().Num();
        const int32 SourceTriangleCount = SourceDescription.Triangles().Num();
        FMeshDescription* Installed = Duplicate->CreateMeshDescription(
            Lod,
            MoveTemp(SourceDescription));
        if (!Installed || Installed->Vertices().Num() != SourceVertexCount ||
            Installed->Triangles().Num() != SourceTriangleCount)
        {
            OutError = FString::Printf(
                TEXT("V5D tree derivative %d could not install exact source LOD %d geometry."),
                FormIndex,
                Lod);
            return false;
        }
        UStaticMesh::FCommitMeshDescriptionParams CommitParams;
        CommitParams.bMarkPackageDirty = false;
        CommitParams.bUseHashAsGuid = true;
        Duplicate->CommitMeshDescription(Lod, CommitParams);
        ++InstalledDescriptions;
    }
    if (InstalledDescriptions <= 0)
    {
        OutError = FString::Printf(
            TEXT("V5D tree derivative %d installed no owned source geometry."),
            FormIndex);
        return false;
    }

    // Do not rely on low-level duplication of lazily loaded bulk-payload state.
    // Recommitting every explicit source LOD gives the V5D asset package its own
    // serialized payload. Reassert source slots before the intentional build.
    Duplicate->SetStaticMaterials(Source->GetStaticMaterials());
    Duplicate->GetSectionInfoMap().CopyFrom(Source->GetSectionInfoMap());
    Duplicate->GetOriginalSectionInfoMap().CopyFrom(
        Source->GetOriginalSectionInfoMap());
    OutError.Reset();
    return true;
}

bool MatchesExactManagedDerivativeStateExceptBounds(
    const UStaticMesh* Candidate,
    const UStaticMesh* Source,
    int32 FormIndex,
    FString& OutError)
{
    const FStaticMeshRenderData* CandidateRender = Candidate
        ? Candidate->GetRenderData()
        : nullptr;
    const FStaticMeshRenderData* SourceRender = Source
        ? Source->GetRenderData()
        : nullptr;
    if (!Candidate || !Source || Candidate == Source ||
        FormIndex < 0 || FormIndex >= UE_ARRAY_COUNT(MeshAssetNames) ||
        Candidate->GetPathName() != ObjectPath(MeshAssetNames[FormIndex]) ||
        Candidate->GetMinLODIdx() != 0 || Source->GetMinLODIdx() != 1 ||
        Candidate->GetNumSourceModels() != Source->GetNumSourceModels() ||
        !Candidate->GetPositiveBoundsExtension().Equals(
            Source->GetPositiveBoundsExtension(), 0.0) ||
        !Candidate->GetNegativeBoundsExtension().Equals(
            Source->GetNegativeBoundsExtension(), 0.0) ||
        !CandidateRender || !SourceRender ||
        CandidateRender->LODResources.Num() != SourceRender->LODResources.Num() ||
        Candidate->GetStaticMaterials().Num() !=
            Source->GetStaticMaterials().Num())
    {
        OutError = FString::Printf(
            TEXT("Managed V5D tree derivative %d changed outside its restorable bounds."),
            FormIndex);
        return false;
    }

    for (int32 Lod = 0; Lod < CandidateRender->LODResources.Num(); ++Lod)
    {
        const FStaticMeshLODResources& CandidateLod =
            CandidateRender->LODResources[Lod];
        const FStaticMeshLODResources& SourceLod = SourceRender->LODResources[Lod];
        if (CandidateLod.GetNumTriangles() != SourceLod.GetNumTriangles() ||
            CandidateLod.Sections.Num() != SourceLod.Sections.Num())
        {
            OutError = FString::Printf(
                TEXT("Managed V5D tree derivative %d changed LOD %d topology; bounds-only repair is refused."),
                FormIndex,
                Lod);
            return false;
        }
        for (int32 Section = 0; Section < CandidateLod.Sections.Num(); ++Section)
        {
            const FStaticMeshSection& CandidateSection =
                CandidateLod.Sections[Section];
            const FStaticMeshSection& SourceSection = SourceLod.Sections[Section];
            if (CandidateSection.MaterialIndex != SourceSection.MaterialIndex ||
                CandidateSection.NumTriangles != SourceSection.NumTriangles)
            {
                OutError = FString::Printf(
                    TEXT("Managed V5D tree derivative %d changed LOD %d section %d; bounds-only repair is refused."),
                    FormIndex,
                    Lod,
                    Section);
                return false;
            }
        }
    }
    for (int32 Slot = 0; Slot < Candidate->GetStaticMaterials().Num(); ++Slot)
    {
        const int32 MaterialIndex =
            FlattenedTreeMaterialIndex(FormIndex, Slot);
        const FStaticMaterial& CandidateMaterial =
            Candidate->GetStaticMaterials()[Slot];
        const FStaticMaterial& SourceMaterial = Source->GetStaticMaterials()[Slot];
        const UMaterialInterface* CandidateInterface =
            Candidate->GetMaterial(Slot);
        const UMaterialInterface* SourceInterface = Source->GetMaterial(Slot);
        const bool bExactSourceBinding = MaterialIndex != INDEX_NONE &&
            CandidateInterface && SourceInterface &&
            CandidateInterface == SourceInterface;
        const bool bExactResponseBinding = MaterialIndex != INDEX_NONE &&
            CandidateInterface &&
            CandidateInterface->GetPathName() ==
                MaterialObjectPath(TreeResponseSpecs[MaterialIndex].AssetName);
        if (CandidateMaterial.MaterialSlotName != SourceMaterial.MaterialSlotName ||
            CandidateMaterial.ImportedMaterialSlotName !=
                SourceMaterial.ImportedMaterialSlotName ||
            (!bExactSourceBinding && !bExactResponseBinding))
        {
            OutError = FString::Printf(
                TEXT("Managed V5D tree derivative %d changed material slot %d outside the exact source-to-response migration; repair is refused."),
                FormIndex,
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateExactManagedDerivativeStateForForm(
    UStaticMesh* Source,
    UStaticMesh* Candidate,
    int32 FormIndex,
    FString& OutError)
{
    if (!MatchesExactManagedDerivativeStateExceptBounds(
            Candidate,
            Source,
            FormIndex,
            OutError))
    {
        return false;
    }
    const FBoxSphereBounds& SourceBounds = Source->GetExtendedBounds();
    const FBoxSphereBounds& CandidateBounds = Candidate->GetExtendedBounds();
    if (!CandidateBounds.Origin.Equals(SourceBounds.Origin, 0.001) ||
        !CandidateBounds.BoxExtent.Equals(SourceBounds.BoxExtent, 0.001))
    {
        OutError = FString::Printf(
            TEXT("Managed V5D tree derivative %d changed exact source bounds."),
            FormIndex);
        return false;
    }
    for (int32 Slot = 0; Slot < Candidate->GetStaticMaterials().Num(); ++Slot)
    {
        const int32 MaterialIndex =
            FlattenedTreeMaterialIndex(FormIndex, Slot);
        const UMaterialInterface* Material = Candidate->GetMaterial(Slot);
        if (MaterialIndex == INDEX_NONE || !Material ||
            Material->GetPathName() != MaterialObjectPath(
                TreeResponseSpecs[MaterialIndex].AssetName))
        {
            OutError = FString::Printf(
                TEXT("Managed V5D tree derivative %d slot %d lacks its exact isolated response material."),
                FormIndex,
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool EnsureExistingMeshForForm(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    int32 FormIndex,
    UStaticMesh*& OutMesh,
    FString& OutError)
{
    OutMesh = nullptr;
    if (!V4 || FormIndex < 0 ||
        FormIndex >= UE_ARRAY_COUNT(MeshAssetNames))
    {
        OutError = TEXT("The requested V5D tree form index is outside the exact five-form roster.");
        return false;
    }
    TArray<UStaticMesh*> Sources;
    GetSourceMeshes(V4, Sources);
    if (Sources.Num() != UE_ARRAY_COUNT(MeshAssetNames) ||
        Sources.Contains(nullptr))
    {
        OutError = TEXT("The exact five V4 source meshes are incomplete during bounded one-form migration.");
        return false;
    }
    int32 ExistingCount = 0;
    for (const FString& AssetName : MeshAssetNames)
    {
        ExistingCount +=
            (FPackageName::DoesPackageExist(PackagePath(AssetName)) ||
             FindPackage(nullptr, *PackagePath(AssetName)))
            ? 1
            : 0;
    }
    if (ExistingCount != UE_ARRAY_COUNT(MeshAssetNames))
    {
        OutError = FString::Printf(
            TEXT("Bounded one-form migration requires the complete preexisting five-mesh namespace; found %d/5."),
            ExistingCount);
        return false;
    }
    UStaticMesh* Source = Sources[FormIndex];
    // Evict the map-resident source's editor-only MeshDescription before the
    // matching derivative is loaded.  The columnar form expands to millions
    // of triangles in memory, so overlapping the two caches is unnecessary
    // and can breach the fixed process ceiling even on an idempotent pass.
    if (!ReleaseStaticMeshDescriptionCacheMemoryBounded(
            Source,
            TEXT("bounded source before candidate load"),
            FormIndex,
            OutError))
    {
        return false;
    }
    UStaticMesh* Candidate = LoadExact<UStaticMesh>(
        ObjectPath(MeshAssetNames[FormIndex]));
    if (!Candidate)
    {
        OutError = TEXT("The exact requested managed V5D tree derivative is absent.");
        return false;
    }
    if (!ReleaseStaticMeshDescriptionCacheMemoryBounded(
            Candidate,
            TEXT("bounded candidate before validation"),
            FormIndex,
            OutError))
    {
        return false;
    }
    FString InitialValidationError;
    if (ValidateExactManagedDerivativeStateForForm(
            Source,
            Candidate,
            FormIndex,
            InitialValidationError))
    {
        OutMesh = Candidate;
        return ReleaseStaticMeshDescriptionCacheMemoryBounded(
            Candidate,
            TEXT("idempotent managed candidate"),
            FormIndex,
            OutError);
    }
    if (!MatchesExactManagedDerivativeStateExceptBounds(
            Candidate,
            Source,
            FormIndex,
            OutError))
    {
        OutError = TEXT("The requested managed V5D derivative failed exact validation and bounded one-form repair was refused: ") +
            InitialValidationError + TEXT(" ") + OutError;
        return false;
    }

    TArray<FStaticMaterial> ResponseSlots = Candidate->GetStaticMaterials();
    for (int32 Slot = 0; Slot < ResponseSlots.Num(); ++Slot)
    {
        const int32 MaterialIndex =
            FlattenedTreeMaterialIndex(FormIndex, Slot);
        UMaterial* ResponseMaterial = MaterialIndex != INDEX_NONE
            ? LoadExact<UMaterial>(MaterialObjectPath(
                  TreeResponseSpecs[MaterialIndex].AssetName))
            : nullptr;
        if (!ResponseMaterial)
        {
            OutError = TEXT("An exact V5D response material was absent during bounded one-form migration.");
            return false;
        }
        ResponseSlots[Slot].MaterialInterface = ResponseMaterial;
    }
    Candidate->SetStaticMaterials(ResponseSlots);
    Candidate->SetExtendedBounds(Source->GetExtendedBounds());
    Candidate->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ReleaseStaticMeshDescriptionCacheMemoryBounded(
            Source,
            TEXT("bounded source"),
            FormIndex,
            OutError) ||
        !ReleaseStaticMeshDescriptionCacheMemoryBounded(
            Candidate,
            TEXT("bounded candidate before save"),
            FormIndex,
            OutError))
    {
        return false;
    }
    if (!SaveStaticMeshPackageDirectWithoutThumbnail(
            Candidate,
            FormIndex,
            OutError))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ReleaseStaticMeshDescriptionCacheMemoryBounded(
            Candidate,
            TEXT("bounded candidate after save"),
            FormIndex,
            OutError) ||
        !ValidateExactManagedDerivativeStateForForm(
            Source,
            Candidate,
            FormIndex,
            OutError))
    {
        return false;
    }
    OutMesh = Candidate;
    OutError.Reset();
    return true;
}

bool RestoreExactSourceExtendedBounds(
    const TArray<UStaticMesh*>& Sources,
    const TArray<UStaticMesh*>& Candidates,
    bool bRequireAtLeastOneMismatch,
    FString& OutError)
{
    if (Sources.Num() != UE_ARRAY_COUNT(MeshAssetNames) ||
        Candidates.Num() != UE_ARRAY_COUNT(MeshAssetNames) ||
        Sources.Contains(nullptr) || Candidates.Contains(nullptr))
    {
        OutError = TEXT("The exact five-mesh source/derivative bounds roster is incomplete.");
        return false;
    }

    bool bFoundMismatch = false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MeshAssetNames); ++Index)
    {
        if (!MatchesExactManagedDerivativeStateExceptBounds(
                Candidates[Index], Sources[Index], Index, OutError))
        {
            return false;
        }
        const FBoxSphereBounds& SourceBounds = Sources[Index]->GetExtendedBounds();
        const FBoxSphereBounds& CandidateBounds =
            Candidates[Index]->GetExtendedBounds();
        bFoundMismatch = bFoundMismatch ||
            !CandidateBounds.Origin.Equals(SourceBounds.Origin, 0.001) ||
            !CandidateBounds.BoxExtent.Equals(SourceBounds.BoxExtent, 0.001);
        for (int32 Slot = 0;
             Slot < Candidates[Index]->GetStaticMaterials().Num();
             ++Slot)
        {
            const int32 MaterialIndex =
                FlattenedTreeMaterialIndex(Index, Slot);
            bFoundMismatch = bFoundMismatch || MaterialIndex == INDEX_NONE ||
                !Candidates[Index]->GetMaterial(Slot) ||
                Candidates[Index]->GetMaterial(Slot)->GetPathName() !=
                    MaterialObjectPath(
                        TreeResponseSpecs[MaterialIndex].AssetName);
        }
    }
    if (bRequireAtLeastOneMismatch && !bFoundMismatch)
    {
        OutError = TEXT("Managed V5D derivatives have no bounds-only mismatch; repair is refused.");
        return false;
    }

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MeshAssetNames); ++Index)
    {
        Candidates[Index]->Modify();
        TArray<FStaticMaterial> ResponseSlots =
            Candidates[Index]->GetStaticMaterials();
        for (int32 Slot = 0; Slot < ResponseSlots.Num(); ++Slot)
        {
            const int32 MaterialIndex =
                FlattenedTreeMaterialIndex(Index, Slot);
            UMaterial* ResponseMaterial = MaterialIndex != INDEX_NONE
                ? LoadExact<UMaterial>(MaterialObjectPath(
                      TreeResponseSpecs[MaterialIndex].AssetName))
                : nullptr;
            if (!ResponseMaterial)
            {
                OutError = TEXT("An exact V5D response material was absent during managed-mesh migration.");
                return false;
            }
            ResponseSlots[Slot].MaterialInterface = ResponseMaterial;
        }
        Candidates[Index]->SetStaticMaterials(ResponseSlots);
        Candidates[Index]->SetExtendedBounds(
            Sources[Index]->GetExtendedBounds());
        Candidates[Index]->MarkPackageDirty();
        // Material rebinding invalidates derived static-mesh work.  Complete
        // each exact derivative before mutating the next one so five large
        // tree meshes cannot accumulate distance-field/card build work in a
        // single editor process and breach the fixed native memory ceiling.
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (!ReleaseStaticMeshDescriptionCacheMemoryBounded(
                Sources[Index],
                TEXT("source"),
                Index,
                OutError) ||
            !ReleaseStaticMeshDescriptionCacheMemoryBounded(
                Candidates[Index],
                TEXT("candidate"),
                Index,
                OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool EnsureMeshes(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    TArray<UStaticMesh*>& OutMeshes,
    FString& OutError)
{
    if (!V4)
    {
        OutError = TEXT("The exact V4 source actor must be loaded before building isolated V5D tree derivatives.");
        return false;
    }
    TArray<UStaticMesh*> Sources;
    GetSourceMeshes(V4, Sources);
    if (Sources.Num() != UE_ARRAY_COUNT(MeshAssetNames) ||
        Sources.Contains(nullptr))
    {
        OutError = TEXT("The five exact V4 broad-form source meshes are incomplete.");
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    int32 ExistingCount = 0;
    for (const FString& AssetName : MeshAssetNames)
    {
        if (FPackageName::DoesPackageExist(PackagePath(AssetName)) ||
            FindPackage(nullptr, *PackagePath(AssetName)))
        {
            ++ExistingCount;
        }
    }
    if (ExistingCount == UE_ARRAY_COUNT(MeshAssetNames))
    {
        if (LoadAndValidateMeshes(V4, OutMeshes, OutError))
        {
            return true;
        }
        const FString InitialValidationError = OutError;
        if (!RestoreExactSourceExtendedBounds(
                Sources,
                OutMeshes,
                true,
                OutError))
        {
            OutError = TEXT("Existing managed V5D tree derivatives failed exact validation and the bounded source-material-to-response/bounds repair was refused: ") +
                InitialValidationError + TEXT(" ") + OutError;
            return false;
        }
        UEditorAssetSubsystem* Assets = GEditor
            ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
            : nullptr;
        if (!Assets)
        {
            OutError = TEXT("The editor asset subsystem is unavailable while saving repaired V5D tree derivatives.");
            return false;
        }
        for (UStaticMesh* Mesh : OutMeshes)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
            if (!Mesh || !Assets->SaveLoadedAsset(Mesh, false))
            {
                OutError = TEXT("An exact managed V5D tree derivative could not be saved serially.");
                return false;
            }
            FAssetCompilingManager::Get().FinishAllCompilation();
            const int32 FormIndex = OutMeshes.IndexOfByKey(Mesh);
            if (FormIndex == INDEX_NONE ||
                !ReleaseStaticMeshDescriptionCacheMemoryBounded(
                    Mesh,
                    TEXT("saved managed candidate"),
                    FormIndex,
                    OutError))
            {
                return false;
            }
        }
        if (!LoadAndValidateMeshes(V4, OutMeshes, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("The exact five managed V5D derivative response bindings/bounds could not be repaired and saved.");
            }
            return false;
        }
        return true;
    }
    if (ExistingCount != 0)
    {
        OutError = FString::Printf(
            TEXT("The isolated V5D tree-realism mesh namespace is partial (%d/5); overwrite or repair is refused."),
            ExistingCount);
        return false;
    }

    TArray<UObject*> FreshAssets;
    TArray<UStaticMesh*> FreshMeshes;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MeshAssetNames); ++Index)
    {
        const FString NewPackageName = PackagePath(MeshAssetNames[Index]);
        UPackage* Package = CreatePackage(*NewPackageName);
        UStaticMesh* Duplicate = Package
            ? Cast<UStaticMesh>(StaticDuplicateObject(
                  Sources[Index],
                  Package,
                  FName(*MeshAssetNames[Index]),
                  RF_Public | RF_Standalone | RF_Transactional))
            : nullptr;
        if (!Duplicate ||
            Duplicate->GetPathName() != ObjectPath(MeshAssetNames[Index]) ||
            !RehomeExactSourceMeshDescriptions(
                Sources[Index],
                Duplicate,
                Index,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Could not duplicate an exact V4 tree source into the isolated V5D namespace.");
            }
            return false;
        }
        Duplicate->Modify();
        Duplicate->SetMinLODIdx(0);
        Duplicate->PostEditChange();
        Duplicate->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Duplicate);
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (!ReleaseStaticMeshDescriptionCacheMemoryBounded(
                Sources[Index],
                TEXT("fresh source"),
                Index,
                OutError) ||
            !ReleaseStaticMeshDescriptionCacheMemoryBounded(
                Duplicate,
                TEXT("fresh candidate"),
                Index,
                OutError))
        {
            return false;
        }
        FreshAssets.Add(Duplicate);
        FreshMeshes.Add(Duplicate);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!RestoreExactSourceExtendedBounds(
            Sources,
            FreshMeshes,
            false,
            OutError) ||
        !Assets || FreshAssets.Num() != UE_ARRAY_COUNT(MeshAssetNames))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Only five isolated V5D tree derivatives were prepared, but serial save preconditions failed.");
        }
        return false;
    }
    for (UStaticMesh* Mesh : FreshMeshes)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (!Mesh || !Assets->SaveLoadedAsset(Mesh, false))
        {
            OutError = TEXT("An exact fresh V5D tree derivative could not be saved serially.");
            return false;
        }
        FAssetCompilingManager::Get().FinishAllCompilation();
        const int32 FormIndex = FreshMeshes.IndexOfByKey(Mesh);
        if (FormIndex == INDEX_NONE ||
            !ReleaseStaticMeshDescriptionCacheMemoryBounded(
                Mesh,
                TEXT("saved fresh candidate"),
                FormIndex,
                OutError))
        {
            return false;
        }
    }
    if (!LoadAndValidateMeshes(V4, OutMeshes, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The five serially saved V5D tree derivatives failed exact cold validation.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildAssetRoster(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    ATRIADIstanaExploreV5BVisualActor* V5B,
    const TArray<UStaticMesh*>& Meshes,
    FTRIADIstanaExploreV5DTreeRealismAssets& OutRoster,
    FString& OutError)
{
    OutRoster = FTRIADIstanaExploreV5DTreeRealismAssets();
    if (!V4 || !V5B || Meshes.Num() != 5 || Meshes.Contains(nullptr) ||
        !V5B->TreeBaseMulchInstances ||
        !V5B->TreeBaseMulchInstances->GetStaticMesh() ||
        !V5B->TreeBaseMulchInstances->GetMaterial(0))
    {
        OutError = TEXT("The exact V4 tree and V5B tree-base source roster is incomplete.");
        return false;
    }
    for (UStaticMesh* Mesh : Meshes)
    {
        OutRoster.NearLodZeroTreeMeshes.Add(Mesh);
    }
    OutRoster.TreeBaseTransitionMesh =
        V5B->TreeBaseMulchInstances->GetStaticMesh();
    OutRoster.TreeBaseTransitionMaterial =
        LoadExact<UMaterialInterface>(TreeBaseTransitionMaterialPath);
    return ATRIADIstanaExploreV5DTreeRealismActor::ValidateAssetRoster(
        OutRoster,
        V4,
        OutError);
}

bool ValidateLoadedWorld(
    UWorld* World,
    ATRIADIstanaExploreV5DTreeRealismActor*& OutActor,
    FString& OutReport)
{
    OutActor = nullptr;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != TargetMapPackage)
    {
        OutReport = TEXT("The current editor world is not /Game/Maps/Istana_PublicView_Explore_v5d_hybrid.");
        return false;
    }
    int32 Count = 0;
    OutActor = FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>(
        World,
        Count);
    if (Count != 1 || !OutActor ||
        !OutActor->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()) ||
        !OutActor->ValidateTreeRealism(OutReport))
    {
        if (OutReport.IsEmpty())
        {
            OutReport = FString::Printf(
                TEXT("V5D requires exactly one tagged tree-realism actor; found %d."),
                Count);
        }
        return false;
    }
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
    EnsureTreeMaterialResponseAssets(FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    int32 V4Count = 0;
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    TArray<UMaterial*> ResponseMaterials;
    FString Error;
    {
        FScopedTreeAssetSaveValidationSuppression SaveValidationSuppression;
        if (V4Count != 1 || !V4 ||
            !SaveValidationSuppression.Initialize(Error) ||
            !EnsureTreeResponseMaterials(V4, ResponseMaterials, Error))
        {
            OutReport = TEXT("V5D_TREE_RESPONSE_MATERIAL_ASSETS_FAILED: ") +
                Error;
            return false;
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    ResponseMaterials.Reset();
    CollectGarbage(RF_NoFlags);
    OutReport = TEXT("V5D_TREE_RESPONSE_MATERIAL_ASSETS_VALID isolatedResponseMaterials=13 sourceOpacityAndWindWpoExact=true duplicateGenericSaveValidationSuppressed=true exactInProcessAndColdValidationRequired=true");
    return true;
}

bool UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
    EnsureTreeCanopyRealismMeshAssets(FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    int32 V4Count = 0;
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    TArray<UStaticMesh*> Meshes;
    FString Error;
    {
        FScopedTreeAssetSaveValidationSuppression SaveValidationSuppression;
        if (V4Count != 1 || !V4 ||
            !SaveValidationSuppression.Initialize(Error) ||
            !EnsureMeshes(V4, Meshes, Error))
        {
            OutReport = TEXT("V5D_TREE_REALISM_ASSETS_FAILED: ") + Error;
            return false;
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    Meshes.Reset();
    CollectGarbage(RF_NoFlags);
    OutReport = TEXT("V5D_TREE_REALISM_ASSETS_VALID meshes=5 isolatedResponseMaterials=13 sourceLodChainsTopologyAndSlotNamesExact=true opacityAndWindWpoExact=true isolatedMinLod0=true sourcePackagesUntouched=true runtimeMinimumLod=0 runtimeForcedLodModel=0 automaticScreenSizeLod=true runtimeLodDistanceScale=1.8 sourceLOD0AvailableNearCamera=true mediumRangeCrownDetailRetained=true allTreesForcedToLod0=false duplicateGenericSaveValidationSuppressed=true exactInProcessAndColdValidationRequired=true materialAndMeshCreationProcessesSeparated=true");
    return true;
}

bool UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
    EnsureTreeCanopyRealismMeshAsset(
        int32 FormIndex,
        FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    int32 V4Count = 0;
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    UStaticMesh* Mesh = nullptr;
    FString Error;
    {
        FScopedTreeAssetSaveValidationSuppression SaveValidationSuppression;
        if (V4Count != 1 || !V4 ||
            !SaveValidationSuppression.Initialize(Error) ||
            !EnsureExistingMeshForForm(V4, FormIndex, Mesh, Error))
        {
            OutReport = TEXT("V5D_TREE_REALISM_MESH_ASSET_FAILED: ") +
                Error;
            return false;
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    Mesh = nullptr;
    CollectGarbage(RF_NoFlags);
    OutReport = FString::Printf(
        TEXT("V5D_TREE_REALISM_MESH_ASSET_VALID formIndex=%d exactFiveMeshNamespaceRequired=true isolatedResponseMaterialsExact=true sourceTopologyAndBoundsExact=true oneFormPerColdProcess=true"),
        FormIndex);
    return true;
}

bool UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
    ApplyTreeCanopyRealismPassToLoadedV5DHybridMap(FString& OutMessage)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != TargetMapPackage)
    {
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_REFUSED: load the exact V5D hybrid destination map first.");
        return false;
    }
    return ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder(
        World,
        false,
        OutMessage);
}

bool UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
    ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder(
        UWorld* World,
        bool bTrustedUntitledHybridBuilder,
        FString& OutMessage)
{
    const bool bExactTarget = World && World->GetOutermost() &&
        World->GetOutermost()->GetName() == TargetMapPackage;
    const bool bTrustedTemp = World && World->GetOutermost() &&
        bTrustedUntitledHybridBuilder &&
        FPackageName::IsTempPackage(World->GetOutermost()->GetName());
    if (!bExactTarget && !bTrustedTemp)
    {
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_REFUSED_WORLD: only the exact target or an explicitly trusted untitled hybrid-builder world is admitted.");
        return false;
    }

    int32 V4Count = 0;
    int32 V5BCount = 0;
    int32 ExistingCount = 0;
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    ATRIADIstanaExploreV5DTreeRealismActor* Existing =
        FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>(
            World,
            ExistingCount);
    FString V4Report;
    FString V5BReport;
    if (V4Count != 1 || V5BCount != 1 || !V4 || !V5B ||
        !V4->ValidateExploreV4Landscape(V4Report) ||
        !V5B->ValidateExploreV5BVisuals(V5BReport))
    {
        OutMessage = FString::Printf(
            TEXT("V5D_TREE_REALISM_APPLY_REFUSED_SOURCES: V4=%d V5B=%d %s %s"),
            V4Count,
            V5BCount,
            *V4Report,
            *V5BReport);
        return false;
    }

    TArray<UMaterial*> ResponseMaterials;
    TArray<UStaticMesh*> Meshes;
    FString Error;
    if (!EnsureTreeResponseMaterials(V4, ResponseMaterials, Error))
    {
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_REFUSED_MATERIALS: ") + Error;
        return false;
    }
    if (!EnsureMeshes(V4, Meshes, Error))
    {
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_REFUSED_MESHES: ") + Error;
        return false;
    }
    if (ExistingCount == 1 && Existing)
    {
        FString ExistingReport;
        if (Existing->Tags.Contains(
                ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()) &&
            Existing->ValidateTreeRealism(ExistingReport))
        {
            OutMessage = TEXT("IDEMPOTENT_V5D_TREE_REALISM_ALREADY_VALID: ") +
                ExistingReport;
            return true;
        }
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_REFUSED_EXISTING_INVALID: ") +
            ExistingReport;
        return false;
    }
    if (ExistingCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("V5D_TREE_REALISM_APPLY_REFUSED_DUPLICATES: found %d actors."),
            ExistingCount);
        return false;
    }

    FTRIADIstanaExploreV5DTreeRealismAssets Roster;
    if (!BuildAssetRoster(V4, V5B, Meshes, Roster, Error))
    {
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_REFUSED_ROSTER: ") + Error;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DTreeRealism");
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV5DTreeRealismActor* Actor =
        World->SpawnActor<ATRIADIstanaExploreV5DTreeRealismActor>(
            ATRIADIstanaExploreV5DTreeRealismActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Actor)
    {
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_FAILED_SPAWN: identity visual actor could not be spawned.");
        return false;
    }
    Actor->Tags.AddUnique(
        ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag());
    Actor->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D Tree and Canopy Visual Assumption"));
    FString Report;
    if (!Actor->ConfigureTreeRealism(V4, V5B, Roster, Error) ||
        !Actor->ValidateTreeRealism(Report))
    {
        Actor->Destroy();
        OutMessage = TEXT("V5D_TREE_REALISM_APPLY_FAILED_CONFIGURATION: ") +
            Error + TEXT(" ") + Report;
        return false;
    }
    World->MarkPackageDirty();
    OutMessage = TEXT("V5D_TREE_REALISM_APPLIED_CALLER_MUST_SAVE_MAP: ") +
        Report;
    return true;
}

bool UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
    ValidateTreeCanopyRealismPassInLoadedV5DHybridMap(FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    ATRIADIstanaExploreV5DTreeRealismActor* Actor = nullptr;
    return ValidateLoadedWorld(World, Actor, OutReport);
}
