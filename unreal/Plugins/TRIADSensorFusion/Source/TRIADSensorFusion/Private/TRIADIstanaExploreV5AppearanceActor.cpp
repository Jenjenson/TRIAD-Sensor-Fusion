#include "TRIADIstanaExploreV5AppearanceActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADIstanaExploreV5Appearance,
    Log,
    All);

namespace
{
const FString V5ClaimLabel(
    TEXT("ISTANA_EXPLORE_V5_APPEARANCE_ONLY_PUBLIC_DATA_VISUAL_APPROXIMATION_NOT_ONE_TO_ONE_NOT_SURVEY_NOT_BOTANICAL_INVENTORY_NOT_SENSOR_TRUTH"));
const FString V5LawnMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_Grass001_Lawn.MI_IPV5_Grass001_Lawn"));
const FString V5FountainWaterMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/M_IPV5_FountainWater.M_IPV5_FountainWater"));
const FString V5HardscapeStoneMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone"));
const FString V5ContextRenderMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_ContextRender.MI_IPV5_ContextRender"));
const FString V5ContextRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_ContextRoof.MI_IPV5_ContextRoof"));
const FString V4CloseTurfBaseMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_CloseTurf_Wind.M_IPV4_CloseTurf_Wind"));
const FString V3CloseTurfBaseColorTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_Color.Foliage008_Color"));
const FString V3CloseTurfNormalTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_NormalGL.Foliage008_NormalGL"));
const FString V3CloseTurfRoughnessTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_Roughness.Foliage008_Roughness"));
const FString V3CloseTurfOpacityTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Foliage008_Opacity.Foliage008_Opacity"));

const FName DirectionalSunTag(TEXT("TRIADIstanaPublicViewLighting_v1"));
const FName V5BRenderSuccessorTag(TEXT("TRIADIstanaExploreV5BRenderSuccessor"));
constexpr float ContactShadowLengthCentimeters = 50.0f;
constexpr bool ContactShadowLengthInWorldSpace = true;
constexpr float ContactShadowCastingIntensity = 1.0f;
constexpr float ContactShadowNonCastingIntensity = 0.0f;

constexpr int32 ContactShadowEnabledComponentCount = 17;
constexpr int32 ContactShadowDisabledComponentCount = 11;
constexpr int32 ContactShadowComponentCount =
    ContactShadowEnabledComponentCount +
    ContactShadowDisabledComponentCount;

const FName LawnComponentName(TEXT("TerrainVisualCollision"));
const FName HardscapeComponentName(TEXT("PublicForecourtHardscape"));
const FName ContextComponentName(TEXT("ODbLMappingGradeContextBuildings"));
const FName WaterSlotName(TEXT("M_IPV_Water"));
const FName StoneSlotName(TEXT("M_IPV_Stone"));
const FName ContextRenderSlotName(TEXT("M_IPV_ContextRender"));
const FName ContextRoofSlotName(TEXT("M_IPV_ContextRoof"));
const FName CloseTurfComponentName(
    TEXT("V4AnimatedCloseTurfGeometryCardReplacement"));
const FName CloseTurfBaseColorParameter(TEXT("BaseColorTexture"));
const FName CloseTurfNormalParameter(TEXT("NormalTexture"));
const FName CloseTurfRoughnessParameter(TEXT("RoughnessTexture"));
const FName CloseTurfOpacityParameter(TEXT("OpacityTexture"));

bool ValidateInheritedSceneOrExactV5DCurrentSuccessor(
    const ATRIADIstanaPublicViewSceneActor* Scene,
    UWorld* World,
    FString& OutError)
{
    FString SceneReport;
    if (Scene && Scene->ValidatePublicViewScene(SceneReport, false))
    {
        OutError.Reset();
        return true;
    }

    const ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    int32 PolicyCount = 0;
    if (World)
    {
        for (TActorIterator<ATRIADIstanaExploreV5DContextPolicyActor> It(World);
             It;
             ++It)
        {
            if (IsValid(*It))
            {
                Policy = *It;
                ++PolicyCount;
            }
        }
    }

    FString PolicyReport;
    if (PolicyCount == 1 && Policy &&
        (Policy->ValidateCurrentSurroundingsSuccessorForInheritedScene(
             Scene,
             PolicyReport) ||
         Policy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
             Scene,
             PolicyReport)))
    {
        // The V5 material/transform checks below still validate their exact
        // inherited components. This alternate presentation gate admits only
        // a strict V5D suppression-V1 or suppression-V2 policy that owns its
        // exact current-context mesh and proves all legacy/V5C renderers are
        // hidden, render-only, and authority-free.
        OutError.Reset();
        return true;
    }

    OutError = FString::Printf(
        TEXT("Inherited public-view scene failed its original presentation and no exact V5D current-context successor was valid; scene='%s' policyCount=%d successor='%s'."),
        *SceneReport,
        PolicyCount,
        *PolicyReport);
    return false;
}

struct FResolvedAppearanceSlots
{
    int32 Water = INDEX_NONE;
    int32 Stone = INDEX_NONE;
    int32 ContextRender = INDEX_NONE;
    int32 ContextRoof = INDEX_NONE;
};

enum class EContactShadowRosterState : uint8
{
    InheritedPreMutation,
    Applied,
    AppliedWithExactV5DTreeRuntimeOverride
};

struct FContactShadowContract
{
    FName ComponentName;
    EComponentMobility::Type Mobility = EComponentMobility::Static;
    bool bCastShadow = true;
    bool bInheritedCastContactShadow = true;
    bool bAppliedCastContactShadow = false;
    bool bRequireVisibleCaster = false;
    bool bV5DTreeRuntimeSuppressionAdmitted = false;
};

const FContactShadowContract ContactShadowContracts[] = {
    {FName(TEXT("BuildingHeroVisual")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("PublicForecourtHardscape")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("V4UmbrellaTreeReclassification")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4DomeTreeReclassification")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4HighForkRoundedTreeReclassification")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4ColumnarNarrowTreeReclassification")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4PalmTreeReclassification")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4HeritageUmbrellaSilhouetteProxies")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4HeritageDomeSilhouetteProxies")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4HeritageHighForkSilhouetteProxies")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4HeritageColumnarSilhouetteProxies")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4HeritagePalmSilhouetteProxies")), EComponentMobility::Static, true, true, true, true, true},
    {FName(TEXT("V4LayeredShrubs")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("V4FloweringAccents")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("V4TropicalUnderstorey")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("V8CentralPorticoRenderOnlySuccessor")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("V5CCentralPorticoRenderOnlySuccessor")), EComponentMobility::Static, true, true, true, true},
    {FName(TEXT("BuildingCollision")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("TerrainVisualCollision")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("SyntheticTerrainBoundarySkirt")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("AnonymousContextBuildings")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("ODbLMappingGradeContextBuildings")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("VolumetricRainTrees")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("VolumetricPalmTrees")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("VolumetricFramingTrees")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("V4HeritageAnchorPawnOnlyBlockers")), EComponentMobility::Static, true, true, false, false},
    {FName(TEXT("V4TallerEdgeGeometryGrass")), EComponentMobility::Static, false, true, false, false},
    {FName(TEXT("V4AnimatedCloseTurfGeometryCardReplacement")), EComponentMobility::Static, false, true, false, false}};
static_assert(
    UE_ARRAY_COUNT(ContactShadowContracts) == ContactShadowComponentCount,
    "Explore V5 contact-shadow contract count must remain frozen at 28.");

const TCHAR* ContactShadowRosterStateLabel(EContactShadowRosterState State)
{
    switch (State)
    {
    case EContactShadowRosterState::InheritedPreMutation:
        return TEXT("inherited pre-mutation");
    case EContactShadowRosterState::AppliedWithExactV5DTreeRuntimeOverride:
        return TEXT("applied V5 with exact V5D tree runtime override");
    default:
        return TEXT("applied V5");
    }
}

bool ExpectedContactShadowForState(
    const FContactShadowContract& Contract,
    EContactShadowRosterState State)
{
    if (State == EContactShadowRosterState::InheritedPreMutation)
    {
        return Contract.bInheritedCastContactShadow;
    }
    if (State ==
            EContactShadowRosterState::AppliedWithExactV5DTreeRuntimeOverride &&
        Contract.bV5DTreeRuntimeSuppressionAdmitted)
    {
        return false;
    }
    return Contract.bAppliedCastContactShadow;
}

int32 ExpectedEnabledContactShadowCount(EContactShadowRosterState State)
{
    int32 Count = 0;
    for (const FContactShadowContract& Contract : ContactShadowContracts)
    {
        Count += ExpectedContactShadowForState(Contract, State) ? 1 : 0;
    }
    return Count;
}

struct FContactShadowRow
{
    UPrimitiveComponent* Component = nullptr;
    const FContactShadowContract* Contract = nullptr;
};

struct FExploreV5Configuration
{
    ATRIADIstanaPublicViewSceneActor* SceneActor = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4Actor = nullptr;
    ADirectionalLight* SunActor = nullptr;
    UMaterialInterface* Lawn = nullptr;
    UMaterialInterface* FountainWater = nullptr;
    UMaterialInterface* HardscapeStone = nullptr;
    UMaterialInterface* ContextRender = nullptr;
    UMaterialInterface* ContextRoof = nullptr;
    UTexture2D* CloseTurfBaseColor = nullptr;
    UTexture2D* CloseTurfNormal = nullptr;
    UTexture2D* CloseTurfRoughness = nullptr;
    UTexture2D* CloseTurfOpacity = nullptr;
    bool bSunBaselineRecorded = false;
    FTransform RecordedSunTransform = FTransform::Identity;
    FLinearColor RecordedSunColor = FLinearColor::White;
    float RecordedSunIntensity = 0.0f;
};

struct FSunContactShadowState
{
    float Length = 0.0f;
    bool bLengthInWorldSpace = false;
    float CastingIntensity = 1.0f;
    float NonCastingIntensity = 0.0f;
};

struct FContactShadowState
{
    UPrimitiveComponent* Component = nullptr;
    bool bCastContactShadow = false;
};

struct FAppearanceMutationSnapshot
{
    UMaterialInterface* Lawn = nullptr;
    UMaterialInterface* FountainWater = nullptr;
    UMaterialInterface* HardscapeStone = nullptr;
    UMaterialInterface* ContextRender = nullptr;
    UMaterialInterface* ContextRoof = nullptr;
    TArray<FContactShadowState> ContactShadows;
    FSunContactShadowState Sun;
};

bool HasExactObjectPath(const UObject* Object, const FString& ExpectedPath)
{
    return Object && Object->GetPathName() == ExpectedPath;
}

bool HasIdentityRelativeTransform(const USceneComponent* Component)
{
    return Component && Component->GetRelativeTransform().Equals(
        FTransform::Identity, 0.001f);
}

FExploreV5Configuration ConfigurationFromActor(
    const ATRIADIstanaExploreV5AppearanceActor* Actor)
{
    FExploreV5Configuration Configuration;
    if (!Actor)
    {
        return Configuration;
    }
    Configuration.SceneActor = Actor->PublicViewSceneActor;
    Configuration.V4Actor = Actor->ExploreV4LandscapeActor;
    Configuration.SunActor = Actor->DirectionalSunActor;
    Configuration.Lawn = Actor->LawnMaterial;
    Configuration.FountainWater = Actor->FountainWaterMaterial;
    Configuration.HardscapeStone = Actor->HardscapeStoneMaterial;
    Configuration.ContextRender = Actor->ContextRenderMaterial;
    Configuration.ContextRoof = Actor->ContextRoofMaterial;
    Configuration.CloseTurfBaseColor = Actor->CloseTurfBaseColorTexture;
    Configuration.CloseTurfNormal = Actor->CloseTurfNormalTexture;
    Configuration.CloseTurfRoughness = Actor->CloseTurfRoughnessTexture;
    Configuration.CloseTurfOpacity = Actor->CloseTurfOpacityTexture;
    Configuration.bSunBaselineRecorded =
        Actor->bInheritedSunPoseColorIntensityRecorded;
    Configuration.RecordedSunTransform = Actor->RecordedInheritedSunTransform;
    Configuration.RecordedSunColor = Actor->RecordedInheritedSunColor;
    Configuration.RecordedSunIntensity = Actor->RecordedInheritedSunIntensity;
    return Configuration;
}

bool ResolveUniqueTaggedDirectionalSun(
    UWorld* World,
    ADirectionalLight*& OutSun,
    FString& OutError)
{
    OutSun = nullptr;
    int32 MatchCount = 0;
    bool bMatchUsesExactDirectionalLightClass = false;
    if (World)
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
        {
            if (!It->ActorHasTag(DirectionalSunTag))
            {
                continue;
            }
            OutSun = *It;
            bMatchUsesExactDirectionalLightClass =
                It->GetClass() == ADirectionalLight::StaticClass();
            ++MatchCount;
        }
    }
    if (MatchCount != 1 || !OutSun ||
        !bMatchUsesExactDirectionalLightClass)
    {
        OutSun = nullptr;
        OutError = FString::Printf(
            TEXT("Explore V5 requires exactly one exact ADirectionalLight tagged '%s'; found %d tagged directional-light actors (exactClass=%s)."),
            *DirectionalSunTag.ToString(),
            MatchCount,
            bMatchUsesExactDirectionalLightClass ? TEXT("true") : TEXT("false"));
        return false;
    }
    OutError.Reset();
    return true;
}

UDirectionalLightComponent* ResolveDirectionalLightComponent(
    ADirectionalLight* SunActor)
{
    return SunActor
        ? Cast<UDirectionalLightComponent>(SunActor->GetLightComponent())
        : nullptr;
}

bool ValidateContactShadowConsoleState(
    FString& OutError,
    FString* OutStateReport = nullptr)
{
    IConsoleManager& Console = IConsoleManager::Get();
    const IConsoleVariable* Enabled = Console.FindConsoleVariable(
        TEXT("r.ContactShadows"));
    const IConsoleVariable* OverrideLength = Console.FindConsoleVariable(
        TEXT("r.ContactShadows.OverrideLength"));
    const IConsoleVariable* OverrideLengthInWs = Console.FindConsoleVariable(
        TEXT("r.ContactShadows.OverrideLengthInWS"));
    const IConsoleVariable* OverrideCasting = Console.FindConsoleVariable(
        TEXT("r.ContactShadows.OverrideShadowCastingIntensity"));
    const IConsoleVariable* OverrideNonCasting = Console.FindConsoleVariable(
        TEXT("r.ContactShadows.OverrideNonShadowCastingIntensity"));
    const IConsoleVariable* IntensityMode = Console.FindConsoleVariable(
        TEXT("r.ContactShadows.Intensity.Mode"));
    if (!Enabled || !OverrideLength || !OverrideCasting ||
        !OverrideNonCasting || !IntensityMode)
    {
        OutError = TEXT("Explore V5 could not resolve every effective UE5.5 contact-shadow renderer CVar.");
        return false;
    }

    const FString OverrideLengthInWsState = OverrideLengthInWs
        ? FString::FromInt(OverrideLengthInWs->GetInt())
        : FString(TEXT("<unavailable>"));
    const FString StateReport = FString::Printf(
        TEXT("r.ContactShadows=%d OverrideLength=%g OverrideLengthInWS=%s(inert while OverrideLength<0) OverrideShadowCastingIntensity=%g OverrideNonShadowCastingIntensity=%g Intensity.Mode=%d"),
        Enabled->GetInt(),
        OverrideLength->GetFloat(),
        *OverrideLengthInWsState,
        OverrideCasting->GetFloat(),
        OverrideNonCasting->GetFloat(),
        IntensityMode->GetInt());
    if (OutStateReport)
    {
        *OutStateReport = StateReport;
    }
    if (Enabled->GetInt() != 1 ||
        OverrideLength->GetFloat() >= 0.0f ||
        OverrideCasting->GetFloat() >= 0.0f ||
        OverrideNonCasting->GetFloat() >= 0.0f ||
        IntensityMode->GetInt() != 0)
    {
        OutError = TEXT("Explore V5 requires contact shadows enabled, primitive-flag intensity mode and every effective directional-light length/intensity override disabled; actual ") +
            StateReport;
        return false;
    }
    OutError.Reset();
    return true;
}

FSunContactShadowState CaptureSunContactShadowState(
    const UDirectionalLightComponent* Component)
{
    FSunContactShadowState State;
    if (Component)
    {
        State.Length = Component->ContactShadowLength;
        State.bLengthInWorldSpace = Component->ContactShadowLengthInWS;
        State.CastingIntensity = Component->ContactShadowCastingIntensity;
        State.NonCastingIntensity =
            Component->ContactShadowNonCastingIntensity;
    }
    return State;
}

bool HasExpectedSunContactShadowState(
    const UDirectionalLightComponent* Component)
{
    return Component &&
        FMath::IsNearlyEqual(
            Component->ContactShadowLength,
            ContactShadowLengthCentimeters,
            0.001f) &&
        static_cast<bool>(Component->ContactShadowLengthInWS) ==
            ContactShadowLengthInWorldSpace &&
        FMath::IsNearlyEqual(
            Component->ContactShadowCastingIntensity,
            ContactShadowCastingIntensity,
            0.0001f) &&
        FMath::IsNearlyEqual(
            Component->ContactShadowNonCastingIntensity,
            ContactShadowNonCastingIntensity,
            0.0001f);
}

void ApplySunContactShadowState(
    UDirectionalLightComponent* Component,
    const FSunContactShadowState& State)
{
    if (!Component)
    {
        return;
    }
    Component->Modify();
    Component->ContactShadowLength = State.Length;
    Component->ContactShadowLengthInWS = State.bLengthInWorldSpace;
    Component->ContactShadowCastingIntensity = State.CastingIntensity;
    Component->ContactShadowNonCastingIntensity = State.NonCastingIntensity;
    Component->MarkRenderStateDirty();
}

void ApplyExpectedSunContactShadowState(
    UDirectionalLightComponent* Component)
{
    FSunContactShadowState Expected;
    Expected.Length = ContactShadowLengthCentimeters;
    Expected.bLengthInWorldSpace = ContactShadowLengthInWorldSpace;
    Expected.CastingIntensity = ContactShadowCastingIntensity;
    Expected.NonCastingIntensity = ContactShadowNonCastingIntensity;
    ApplySunContactShadowState(Component, Expected);
}

bool ResolveUniqueNamedMaterialSlot(
    const UStaticMeshComponent* Component,
    FName ExpectedComponentName,
    FName ExpectedSlotName,
    int32& OutSlotIndex,
    FString& OutError)
{
    OutSlotIndex = INDEX_NONE;
    const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    if (!Component || Component->GetFName() != ExpectedComponentName || !Mesh)
    {
        OutError = FString::Printf(
            TEXT("Explore V5 requires component '%s' with a saved static mesh."),
            *ExpectedComponentName.ToString());
        return false;
    }

    int32 MatchCount = 0;
    const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
    for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
    {
        const FStaticMaterial& StaticMaterial = StaticMaterials[Index];
        if (StaticMaterial.MaterialSlotName == ExpectedSlotName ||
            StaticMaterial.ImportedMaterialSlotName == ExpectedSlotName)
        {
            OutSlotIndex = Index;
            ++MatchCount;
        }
    }
    if (MatchCount != 1 || OutSlotIndex == INDEX_NONE ||
        OutSlotIndex >= Component->GetNumMaterials())
    {
        OutSlotIndex = INDEX_NONE;
        OutError = FString::Printf(
            TEXT("Component '%s' must expose exactly one usable named slot '%s'."),
            *ExpectedComponentName.ToString(),
            *ExpectedSlotName.ToString());
        return false;
    }

    OutError.Reset();
    return true;
}

bool ResolveAppearanceSlots(
    const ATRIADIstanaPublicViewSceneActor* SceneActor,
    FResolvedAppearanceSlots& OutSlots,
    FString& OutError)
{
    OutSlots = FResolvedAppearanceSlots();
    if (!SceneActor || !SceneActor->TerrainComponent ||
        SceneActor->TerrainComponent->GetFName() != LawnComponentName ||
        !SceneActor->TerrainComponent->GetStaticMesh() ||
        SceneActor->TerrainComponent->GetNumMaterials() < 1 ||
        SceneActor->TerrainComponent->GetStaticMesh()->
            GetStaticMaterials().IsEmpty())
    {
        OutError = TEXT("TerrainVisualCollision must retain a usable material slot 0 for the V5 lawn override.");
        return false;
    }
    if (!ResolveUniqueNamedMaterialSlot(
            SceneActor->HardscapeComponent,
            HardscapeComponentName,
            WaterSlotName,
            OutSlots.Water,
            OutError) ||
        !ResolveUniqueNamedMaterialSlot(
            SceneActor->HardscapeComponent,
            HardscapeComponentName,
            StoneSlotName,
            OutSlots.Stone,
            OutError) ||
        OutSlots.Water == OutSlots.Stone ||
        !ResolveUniqueNamedMaterialSlot(
            SceneActor->OSMContextBuildingsComponent,
            ContextComponentName,
            ContextRenderSlotName,
            OutSlots.ContextRender,
            OutError) ||
        !ResolveUniqueNamedMaterialSlot(
            SceneActor->OSMContextBuildingsComponent,
            ContextComponentName,
            ContextRoofSlotName,
            OutSlots.ContextRoof,
            OutError) ||
        OutSlots.ContextRender == OutSlots.ContextRoof)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Explore V5 material roles must resolve to four distinct role-appropriate named slots.");
        }
        return false;
    }

    OutError.Reset();
    return true;
}

TArray<FContactShadowRow> BuildContactShadowRows(
    ATRIADIstanaPublicViewSceneActor* SceneActor,
    ATRIADIstanaExploreV4LandscapeActor* V4Actor)
{
    TArray<FContactShadowRow> Rows;
    Rows.Reserve(ContactShadowComponentCount);
    if (!SceneActor || !V4Actor)
    {
        return Rows;
    }

    UPrimitiveComponent* Components[] = {
        SceneActor->BuildingHeroVisualComponent.Get(),
        SceneActor->HardscapeComponent.Get(),
        V4Actor->UmbrellaTreeInstances.Get(),
        V4Actor->DomeTreeInstances.Get(),
        V4Actor->HighForkRoundedTreeInstances.Get(),
        V4Actor->ColumnarNarrowTreeInstances.Get(),
        V4Actor->PalmTreeInstances.Get(),
        V4Actor->HeritageUmbrellaInstances.Get(),
        V4Actor->HeritageDomeInstances.Get(),
        V4Actor->HeritageHighForkRoundedInstances.Get(),
        V4Actor->HeritageColumnarNarrowInstances.Get(),
        V4Actor->HeritagePalmInstances.Get(),
        V4Actor->ShrubInstances.Get(),
        V4Actor->FlowerInstances.Get(),
        V4Actor->UnderstoreyInstances.Get(),
        V4Actor->PorticoV8RenderOnlyComponent.Get(),
        V4Actor->PorticoV5CRenderOnlyComponent.Get(),
        SceneActor->BuildingCollisionComponent.Get(),
        SceneActor->TerrainComponent.Get(),
        SceneActor->TerrainSkirtComponent.Get(),
        SceneActor->ContextBuildingsComponent.Get(),
        SceneActor->OSMContextBuildingsComponent.Get(),
        SceneActor->RainTreeInstances.Get(),
        SceneActor->PalmTreeInstances.Get(),
        SceneActor->FramingTreeInstances.Get(),
        V4Actor->HeritageAnchorPawnBlockers.Get(),
        V4Actor->GeometryGrassInstances.Get(),
        V4Actor->CloseTurfInstances.Get()};
    static_assert(
        UE_ARRAY_COUNT(Components) == ContactShadowComponentCount,
        "Explore V5 contact-shadow component binding count changed.");
    for (int32 Index = 0; Index < ContactShadowComponentCount; ++Index)
    {
        Rows.Add(FContactShadowRow{
            Components[Index], &ContactShadowContracts[Index]});
    }
    return Rows;
}

bool ResolveAppliedContactShadowRosterState(
    const ATRIADIstanaExploreV4LandscapeActor* V4Actor,
    EContactShadowRosterState& OutState,
    FString& OutError)
{
    OutState = EContactShadowRosterState::Applied;
    if (!V4Actor || !V4Actor->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::RuntimeOverrideTag()))
    {
        OutError.Reset();
        return true;
    }

    FString OverrideReport;
    if (!ATRIADIstanaExploreV5DTreeRealismActor::
            ValidateActiveRuntimeOverrideForSourceActor(
                V4Actor,
                OverrideReport))
    {
        OutError = TEXT(
            "A tagged V5D tree contact-shadow suppression failed its exact "
            "owner/source/census/LOD/material/simulation-isolation gate: ") +
            OverrideReport;
        return false;
    }

    OutState =
        EContactShadowRosterState::AppliedWithExactV5DTreeRuntimeOverride;
    OutError.Reset();
    return true;
}

bool ValidateContactShadowRowIdentity(
    const TArray<FContactShadowRow>& Rows,
    FString& OutError)
{
    if (Rows.Num() != ContactShadowComponentCount)
    {
        OutError = FString::Printf(
            TEXT("The exact Explore V5 contact-shadow roster requires %d rows; found %d."),
            ContactShadowComponentCount,
            Rows.Num());
        return false;
    }

    int32 EnabledCount = 0;
    int32 DisabledCount = 0;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const FContactShadowRow& Row = Rows[Index];
        const FContactShadowContract& Expected =
            ContactShadowContracts[Index];
        if (!Row.Component || Row.Contract != &Expected ||
            Row.Component->GetFName() != Expected.ComponentName)
        {
            OutError = FString::Printf(
                TEXT("Explore V5 contact-shadow row %d must remain exact component '%s'."),
                Index,
                *Expected.ComponentName.ToString());
            return false;
        }
        if (Row.Component->Mobility != Expected.Mobility ||
            static_cast<bool>(Row.Component->CastShadow) !=
                Expected.bCastShadow)
        {
            OutError = FString::Printf(
                TEXT("Contact-shadow component '%s' inherited-state drift: mobility expected=%d actual=%d, CastShadow expected=%s actual=%s."),
                *Expected.ComponentName.ToString(),
                static_cast<int32>(Expected.Mobility),
                static_cast<int32>(Row.Component->Mobility.GetValue()),
                Expected.bCastShadow ? TEXT("true") : TEXT("false"),
                Row.Component->CastShadow ? TEXT("true") : TEXT("false"));
            return false;
        }
        if (Expected.bAppliedCastContactShadow)
        {
            ++EnabledCount;
            // V5B may replace only the exact inherited hardscape and planting
            // renderers. Their ordinary/contact-shadow flags remain frozen,
            // while the exact tagged owner requires the original renderer to
            // be hidden so its render successor does not double-render. Every
            // other V5 caster still has to remain visibly enabled.
            const bool bHiddenByExactV5BRenderSuccessor =
                Row.Component->GetOwner() &&
                Row.Component->GetOwner()->Tags.Contains(
                    V5BRenderSuccessorTag) &&
                (Expected.ComponentName == HardscapeComponentName ||
                 Expected.ComponentName == FName(TEXT("V4LayeredShrubs")) ||
                 Expected.ComponentName == FName(TEXT("V4FloweringAccents")) ||
                 Expected.ComponentName == FName(TEXT("V4TropicalUnderstorey")));
            const ATRIADIstanaExploreV4LandscapeActor* PorticoOwner =
                Cast<ATRIADIstanaExploreV4LandscapeActor>(
                    Row.Component->GetOwner());
            const bool bHiddenByExactV5CPorticoPresentation = PorticoOwner &&
                ((Expected.ComponentName ==
                      FName(TEXT("V8CentralPorticoRenderOnlySuccessor")) &&
                  PorticoOwner->IsPorticoV5CPresentationActive()) ||
                 (Expected.ComponentName ==
                      FName(TEXT("V5CCentralPorticoRenderOnlySuccessor")) &&
                  !PorticoOwner->IsPorticoV5CPresentationActive()));
            const bool bHasExactVisibilityState =
                bHiddenByExactV5BRenderSuccessor ||
                    bHiddenByExactV5CPorticoPresentation
                ? !Row.Component->IsVisible() && Row.Component->bHiddenInGame
                : Row.Component->IsVisible() && !Row.Component->bHiddenInGame;
            if (!Expected.bRequireVisibleCaster ||
                !bHasExactVisibilityState || !Row.Component->CastShadow)
            {
                OutError = FString::Printf(
                    TEXT("Enabled contact-shadow component '%s' is not in its exact visible V5 or tagged hidden V5B successor state as an ordinary-shadow caster."),
                    *Expected.ComponentName.ToString());
                return false;
            }
        }
        else
        {
            ++DisabledCount;
        }
    }
    if (EnabledCount != ContactShadowEnabledComponentCount ||
        DisabledCount != ContactShadowDisabledComponentCount)
    {
        OutError = TEXT("The exact Explore V5 enabled/disabled contact-shadow census changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateContactShadowRowsInState(
    const TArray<FContactShadowRow>& Rows,
    EContactShadowRosterState State,
    FString& OutError)
{
    if (!ValidateContactShadowRowIdentity(Rows, OutError))
    {
        return false;
    }
    for (const FContactShadowRow& Row : Rows)
    {
        const bool bExpected = ExpectedContactShadowForState(
            *Row.Contract,
            State);
        const bool bActual =
            static_cast<bool>(Row.Component->bCastContactShadow);
        if (bActual != bExpected)
        {
            OutError = FString::Printf(
                TEXT("Contact-shadow component '%s' %s-state drift: bCastContactShadow expected=%s actual=%s."),
                *Row.Contract->ComponentName.ToString(),
                ContactShadowRosterStateLabel(State),
                bExpected ? TEXT("true") : TEXT("false"),
                bActual ? TEXT("true") : TEXT("false"));
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ApplyContactShadowRows(
    const TArray<FContactShadowRow>& Rows,
    EContactShadowRosterState ExpectedPreState,
    FString& OutError)
{
    if (!ValidateContactShadowRowsInState(
            Rows, ExpectedPreState, OutError))
    {
        return false;
    }
    const EContactShadowRosterState TargetState =
        ExpectedPreState ==
            EContactShadowRosterState::AppliedWithExactV5DTreeRuntimeOverride
        ? EContactShadowRosterState::AppliedWithExactV5DTreeRuntimeOverride
        : EContactShadowRosterState::Applied;
    for (const FContactShadowRow& Row : Rows)
    {
        Row.Component->SetCastContactShadow(
            ExpectedContactShadowForState(*Row.Contract, TargetState));
    }
    return ValidateContactShadowRowsInState(
        Rows, TargetState, OutError);
}

bool ValidateAppliedContactShadowRows(
    const TArray<FContactShadowRow>& Rows,
    const ATRIADIstanaExploreV4LandscapeActor* V4Actor,
    EContactShadowRosterState& OutState,
    FString& OutError)
{
    if (!ResolveAppliedContactShadowRosterState(
            V4Actor,
            OutState,
            OutError))
    {
        return false;
    }
    return ValidateContactShadowRowsInState(
        Rows, OutState, OutError);
}

#if WITH_DEV_AUTOMATION_TESTS
bool ValidateContactShadowRosterValuesForAutomation(
    const TArray<FName>& ComponentNames,
    const TArray<uint8>& Mobilities,
    const TArray<bool>& CastShadows,
    const TArray<bool>& CastContactShadows,
    EContactShadowRosterState State,
    FString& OutError)
{
    if (ComponentNames.Num() != ContactShadowComponentCount ||
        Mobilities.Num() != ContactShadowComponentCount ||
        CastShadows.Num() != ContactShadowComponentCount ||
        CastContactShadows.Num() != ContactShadowComponentCount)
    {
        OutError = TEXT("Automation contact-shadow roster must contain exactly 28 values in every field.");
        return false;
    }

    for (int32 Index = 0; Index < ContactShadowComponentCount; ++Index)
    {
        const FContactShadowContract& Expected =
            ContactShadowContracts[Index];
        const bool bExpectedContactShadow = ExpectedContactShadowForState(
            Expected,
            State);
        if (ComponentNames[Index] != Expected.ComponentName ||
            Mobilities[Index] != static_cast<uint8>(Expected.Mobility) ||
            CastShadows[Index] != Expected.bCastShadow ||
            CastContactShadows[Index] != bExpectedContactShadow)
        {
            OutError = FString::Printf(
                TEXT("Automation contact-shadow row %d ('%s') does not match the frozen %s state."),
                Index,
                *Expected.ComponentName.ToString(),
                ContactShadowRosterStateLabel(State));
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ApplyContactShadowRosterValuesForAutomation(
    const TArray<FName>& ComponentNames,
    const TArray<uint8>& Mobilities,
    const TArray<bool>& CastShadows,
    TArray<bool>& InOutCastContactShadows,
    FString& OutError)
{
    if (!ValidateContactShadowRosterValuesForAutomation(
            ComponentNames,
            Mobilities,
            CastShadows,
            InOutCastContactShadows,
            EContactShadowRosterState::InheritedPreMutation,
            OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < ContactShadowComponentCount; ++Index)
    {
        InOutCastContactShadows[Index] =
            ContactShadowContracts[Index].bAppliedCastContactShadow;
    }
    return ValidateContactShadowRosterValuesForAutomation(
        ComponentNames,
        Mobilities,
        CastShadows,
        InOutCastContactShadows,
        EContactShadowRosterState::Applied,
        OutError);
}
#endif

UMaterialInstanceDynamic* ResolveExactCloseTurfRuntimeMid(
    const ATRIADIstanaExploreV4LandscapeActor* V4Actor,
    FString& OutError)
{
    const UHierarchicalInstancedStaticMeshComponent* CloseTurf =
        V4Actor ? V4Actor->CloseTurfInstances.Get() : nullptr;
    UMaterialInstanceDynamic* Mid = CloseTurf
        ? Cast<UMaterialInstanceDynamic>(CloseTurf->GetMaterial(0))
        : nullptr;
    if (!CloseTurf || CloseTurf->GetFName() != CloseTurfComponentName ||
        CloseTurf->GetNumMaterials() != 1 || !Mid || !Mid->Parent ||
        Mid->Parent->GetPathName() != V4CloseTurfBaseMaterialPath)
    {
        OutError = TEXT("Close-turf texture rebind requires material(0) to be a runtime MID whose direct parent is the exact V4 close-turf wind base.");
        return nullptr;
    }
    OutError.Reset();
    return Mid;
}

bool HasExactCloseTurfTextureBindings(
    const UMaterialInstanceDynamic* Mid,
    const UTexture2D* BaseColor,
    const UTexture2D* Normal,
    const UTexture2D* Roughness,
    const UTexture2D* Opacity,
    FString& OutError)
{
    struct FTextureBinding
    {
        FName Parameter;
        const UTexture2D* ExpectedTexture = nullptr;
    };
    const FTextureBinding Bindings[] = {
        {CloseTurfBaseColorParameter, BaseColor},
        {CloseTurfNormalParameter, Normal},
        {CloseTurfRoughnessParameter, Roughness},
        {CloseTurfOpacityParameter, Opacity}};
    for (const FTextureBinding& Binding : Bindings)
    {
        UTexture* ActualTexture = nullptr;
        if (!Mid || !Mid->GetTextureParameterValue(
                FMaterialParameterInfo(Binding.Parameter), ActualTexture) ||
            ActualTexture != Binding.ExpectedTexture)
        {
            OutError = FString::Printf(
                TEXT("Close-turf runtime MID parameter '%s' does not retain its exact V5 texture reference."),
                *Binding.Parameter.ToString());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateExploreV5Configuration(
    const ATRIADIstanaExploreV5AppearanceActor* Owner,
    const FExploreV5Configuration& Configuration,
    FString& OutError)
{
    if (!Owner ||
        !Owner->GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        !Owner->SceneRoot ||
        Owner->GetRootComponent() != Owner->SceneRoot.Get() ||
        Owner->GetComponents().Num() != 1 ||
        !Owner->GetComponents().Contains(Owner->SceneRoot.Get()) ||
        Owner->SceneRoot->Mobility != EComponentMobility::Static ||
        Owner->SceneRoot->GetAttachParent() != nullptr ||
        !Owner->SceneRoot->GetRelativeTransform().Equals(
            FTransform::Identity, 0.001f) ||
        Owner->ClaimLabel !=
            ATRIADIstanaExploreV5AppearanceActor::ExpectedClaimLabel() ||
        !Owner->bAppearanceOnly || Owner->bGeometryOrTransformAuthority ||
        Owner->bCollisionOrNavigationAuthority ||
        Owner->bSurveyOrAsBuiltClaimed || Owner->bBotanicalInventoryClaimed ||
        Owner->bSensorOrRfMaterialAuthority ||
        Owner->bGoogleOrOneMapContentUsed ||
        Owner->bSunPoseColorOrIntensityModified)
    {
        OutError = TEXT("Explore V5 identity, exact one-component non-primitive root census or appearance-only truth boundary changed.");
        return false;
    }

    if (!Configuration.SceneActor || !Configuration.V4Actor ||
        !Configuration.SunActor ||
        !HasExactObjectPath(Configuration.Lawn, V5LawnMaterialPath) ||
        !HasExactObjectPath(
            Configuration.FountainWater, V5FountainWaterMaterialPath) ||
        !HasExactObjectPath(
            Configuration.HardscapeStone, V5HardscapeStoneMaterialPath) ||
        !HasExactObjectPath(
            Configuration.ContextRender, V5ContextRenderMaterialPath) ||
        !HasExactObjectPath(
            Configuration.ContextRoof, V5ContextRoofMaterialPath) ||
        !HasExactObjectPath(
            Configuration.CloseTurfBaseColor,
            V3CloseTurfBaseColorTexturePath) ||
        !HasExactObjectPath(
            Configuration.CloseTurfNormal,
            V3CloseTurfNormalTexturePath) ||
        !HasExactObjectPath(
            Configuration.CloseTurfRoughness,
            V3CloseTurfRoughnessTexturePath) ||
        !HasExactObjectPath(
            Configuration.CloseTurfOpacity,
            V3CloseTurfOpacityTexturePath))
    {
        OutError = TEXT("Explore V5 lost an exact persisted target, material, close-turf texture or directional-sun reference.");
        return false;
    }

    UWorld* const OwnerWorld = Owner->GetWorld();
    if (!OwnerWorld || Configuration.SceneActor->GetWorld() != OwnerWorld ||
        Configuration.V4Actor->GetWorld() != OwnerWorld ||
        Configuration.SunActor->GetWorld() != OwnerWorld)
    {
        OutError = TEXT("Explore V5 owner, inherited targets and directional sun must belong to the same world.");
        return false;
    }

    ADirectionalLight* UniqueTaggedSun = nullptr;
    if (!ResolveUniqueTaggedDirectionalSun(
            OwnerWorld, UniqueTaggedSun, OutError) ||
        UniqueTaggedSun != Configuration.SunActor)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Explore V5 persisted directional sun is not the unique exact-tag match.");
        }
        return false;
    }

    const UDirectionalLightComponent* SunComponent =
        ResolveDirectionalLightComponent(Configuration.SunActor);
    if (!Configuration.bSunBaselineRecorded || !SunComponent ||
        !Configuration.SunActor->GetActorTransform().Equals(
            Configuration.RecordedSunTransform, 0.001f) ||
        !SunComponent->GetLightColor().Equals(
            Configuration.RecordedSunColor, 0.0001f) ||
        !FMath::IsNearlyEqual(
            SunComponent->Intensity,
            Configuration.RecordedSunIntensity,
            0.001f) ||
        SunComponent->Mobility != EComponentMobility::Movable ||
        !SunComponent->bAffectsWorld || !SunComponent->IsVisible() ||
        !SunComponent->CastShadows || !SunComponent->CastDynamicShadows)
    {
        OutError = TEXT("Explore V5 requires the unique tagged movable directional sun and its recorded inherited pose, colour and intensity to remain unchanged and shadow-capable.");
        return false;
    }
    if (!ValidateContactShadowConsoleState(OutError))
    {
        return false;
    }

    if (!Configuration.SceneActor->GetActorTransform().Equals(
            FTransform::Identity, 0.001f) ||
        !Configuration.V4Actor->GetActorTransform().Equals(
            FTransform::Identity, 0.001f) ||
        !HasIdentityRelativeTransform(Configuration.SceneActor->SceneRoot) ||
        !HasIdentityRelativeTransform(
            Configuration.SceneActor->TerrainComponent) ||
        !HasIdentityRelativeTransform(
            Configuration.SceneActor->HardscapeComponent) ||
        !HasIdentityRelativeTransform(
            Configuration.SceneActor->OSMContextBuildingsComponent))
    {
        OutError = TEXT("Explore V5 target actor/component transforms must remain exact inherited identities.");
        return false;
    }

    FString InheritedReport;
    if (!ValidateInheritedSceneOrExactV5DCurrentSuccessor(
            Configuration.SceneActor,
            OwnerWorld,
            InheritedReport))
    {
        OutError = TEXT("Inherited public-view scene is invalid: ") +
            InheritedReport;
        return false;
    }
    if (!Configuration.V4Actor->ValidateExploreV4Landscape(InheritedReport))
    {
        OutError = TEXT("Inherited Explore V4 landscape is invalid: ") +
            InheritedReport;
        return false;
    }

    FResolvedAppearanceSlots Slots;
    if (!ResolveAppearanceSlots(Configuration.SceneActor, Slots, OutError))
    {
        return false;
    }
    const TArray<FContactShadowRow> ContactRows = BuildContactShadowRows(
        Configuration.SceneActor, Configuration.V4Actor);
    if (!ValidateContactShadowRowIdentity(ContactRows, OutError))
    {
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* CloseTurf =
        Configuration.V4Actor->CloseTurfInstances;
    const UMaterialInterface* CloseTurfMaterial = CloseTurf
        ? CloseTurf->GetMaterial(0)
        : nullptr;
    const UMaterialInstanceDynamic* CloseTurfMid =
        Cast<UMaterialInstanceDynamic>(CloseTurfMaterial);
    const bool bHasExactSavedBase =
        HasExactObjectPath(CloseTurfMaterial, V4CloseTurfBaseMaterialPath);
    const bool bHasExactRuntimeParent = CloseTurfMid && CloseTurfMid->Parent &&
        CloseTurfMid->Parent->GetPathName() == V4CloseTurfBaseMaterialPath;
    // V5B is an additive render successor. The inherited V4 validator admits
    // its exact tagged state only after all close-turf and planting transforms
    // are preserved and their five renderers receive full render-only
    // replacements. The close-turf mesh, count, transform, material, culling,
    // collision, navigation, LOD, and shadow contracts below remain mandatory.
    const bool bHiddenByExactV5BRenderSuccessor =
        Configuration.V4Actor->Tags.Contains(V5BRenderSuccessorTag);
    const bool bExactVisibilityState = CloseTurf &&
        (bHiddenByExactV5BRenderSuccessor
            ? !CloseTurf->IsVisible() && CloseTurf->bHiddenInGame
            : CloseTurf->IsVisible() && !CloseTurf->bHiddenInGame);
    if (!CloseTurf || CloseTurf->GetFName() != CloseTurfComponentName ||
        CloseTurf->GetNumMaterials() != 1 ||
        CloseTurf->GetInstanceCount() != 18432 ||
        !CloseTurf->GetRelativeTransform().Equals(
            FTransform::Identity, 0.001f) ||
        CloseTurf->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        CloseTurf->GetGenerateOverlapEvents() ||
        CloseTurf->CanEverAffectNavigation() ||
        CloseTurf->InstanceStartCullDistance != 800 ||
        CloseTurf->InstanceEndCullDistance != 10000 ||
        CloseTurf->WorldPositionOffsetDisableDistance != 9000 ||
        !CloseTurf->bOverrideMinLOD || CloseTurf->MinLOD != 1 ||
        CloseTurf->ForcedLodModel != 0 || CloseTurf->CastShadow ||
        !bExactVisibilityState ||
        (!bHasExactSavedBase && !bHasExactRuntimeParent))
    {
        OutError = TEXT("Explore V5 close turf must retain its exact V4 mesh/18,432 instances, identity component transform, NoCollision/non-navigation state, 800/10000 cm cull, 9000 cm WPO-disable, MinLOD1, shadow/material state, and either its visible V5 state or the exact tagged hidden V5B render-successor state.");
        return false;
    }
    if (CloseTurf->GetCollisionResponseToChannels() !=
        FCollisionResponseContainer(ECR_Ignore))
    {
        OutError = TEXT("Explore V5 close turf must keep ignoring every inherited collision and trace channel.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool CaptureAppearanceMutationSnapshot(
    const FExploreV5Configuration& Configuration,
    const FResolvedAppearanceSlots& Slots,
    const TArray<FContactShadowRow>& Rows,
    EContactShadowRosterState ExpectedContactShadowPreState,
    FAppearanceMutationSnapshot& OutSnapshot,
    FString& OutError)
{
    OutSnapshot = FAppearanceMutationSnapshot();
    if (!Configuration.SceneActor || !Configuration.SunActor ||
        !Configuration.SceneActor->TerrainComponent ||
        !Configuration.SceneActor->HardscapeComponent ||
        !Configuration.SceneActor->OSMContextBuildingsComponent ||
        !ResolveDirectionalLightComponent(Configuration.SunActor) ||
        !ValidateContactShadowRowsInState(
            Rows, ExpectedContactShadowPreState, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Explore V5 could not capture a complete pre-mutation appearance snapshot.");
        }
        return false;
    }

    OutSnapshot.Lawn =
        Configuration.SceneActor->TerrainComponent->GetMaterial(0);
    OutSnapshot.FountainWater =
        Configuration.SceneActor->HardscapeComponent->GetMaterial(Slots.Water);
    OutSnapshot.HardscapeStone =
        Configuration.SceneActor->HardscapeComponent->GetMaterial(Slots.Stone);
    OutSnapshot.ContextRender =
        Configuration.SceneActor->OSMContextBuildingsComponent->GetMaterial(
            Slots.ContextRender);
    OutSnapshot.ContextRoof =
        Configuration.SceneActor->OSMContextBuildingsComponent->GetMaterial(
            Slots.ContextRoof);
    OutSnapshot.ContactShadows.Reserve(Rows.Num());
    for (const FContactShadowRow& Row : Rows)
    {
        OutSnapshot.ContactShadows.Add(FContactShadowState{
            Row.Component,
            static_cast<bool>(Row.Component->bCastContactShadow)});
    }
    OutSnapshot.Sun = CaptureSunContactShadowState(
        ResolveDirectionalLightComponent(Configuration.SunActor));
    OutError.Reset();
    return true;
}

bool RestoreAppearanceMutationSnapshot(
    const FExploreV5Configuration& Configuration,
    const FResolvedAppearanceSlots& Slots,
    const FAppearanceMutationSnapshot& Snapshot,
    FString& OutError)
{
    if (!Configuration.SceneActor || !Configuration.SunActor ||
        !Configuration.SceneActor->TerrainComponent ||
        !Configuration.SceneActor->HardscapeComponent ||
        !Configuration.SceneActor->OSMContextBuildingsComponent ||
        !ResolveDirectionalLightComponent(Configuration.SunActor) ||
        Snapshot.ContactShadows.Num() != ContactShadowComponentCount)
    {
        OutError = TEXT("Explore V5 rollback snapshot or target roster is incomplete.");
        return false;
    }

    Configuration.SceneActor->TerrainComponent->SetMaterial(0, Snapshot.Lawn);
    Configuration.SceneActor->HardscapeComponent->SetMaterial(
        Slots.Water, Snapshot.FountainWater);
    Configuration.SceneActor->HardscapeComponent->SetMaterial(
        Slots.Stone, Snapshot.HardscapeStone);
    Configuration.SceneActor->OSMContextBuildingsComponent->SetMaterial(
        Slots.ContextRender, Snapshot.ContextRender);
    Configuration.SceneActor->OSMContextBuildingsComponent->SetMaterial(
        Slots.ContextRoof, Snapshot.ContextRoof);
    for (const FContactShadowState& State : Snapshot.ContactShadows)
    {
        if (State.Component)
        {
            State.Component->SetCastContactShadow(State.bCastContactShadow);
        }
    }
    ApplySunContactShadowState(
        ResolveDirectionalLightComponent(Configuration.SunActor),
        Snapshot.Sun);

    bool bRestored =
        Configuration.SceneActor->TerrainComponent->GetMaterial(0) ==
            Snapshot.Lawn &&
        Configuration.SceneActor->HardscapeComponent->GetMaterial(
            Slots.Water) == Snapshot.FountainWater &&
        Configuration.SceneActor->HardscapeComponent->GetMaterial(
            Slots.Stone) == Snapshot.HardscapeStone &&
        Configuration.SceneActor->OSMContextBuildingsComponent->GetMaterial(
            Slots.ContextRender) == Snapshot.ContextRender &&
        Configuration.SceneActor->OSMContextBuildingsComponent->GetMaterial(
            Slots.ContextRoof) == Snapshot.ContextRoof;
    for (const FContactShadowState& State : Snapshot.ContactShadows)
    {
        bRestored = bRestored && State.Component &&
            static_cast<bool>(State.Component->bCastContactShadow) ==
                State.bCastContactShadow;
    }
    const FSunContactShadowState RestoredSun = CaptureSunContactShadowState(
        ResolveDirectionalLightComponent(Configuration.SunActor));
    bRestored = bRestored &&
        FMath::IsNearlyEqual(
            RestoredSun.Length, Snapshot.Sun.Length, 0.001f) &&
        RestoredSun.bLengthInWorldSpace == Snapshot.Sun.bLengthInWorldSpace &&
        FMath::IsNearlyEqual(
            RestoredSun.CastingIntensity,
            Snapshot.Sun.CastingIntensity,
            0.0001f) &&
        FMath::IsNearlyEqual(
            RestoredSun.NonCastingIntensity,
            Snapshot.Sun.NonCastingIntensity,
            0.0001f);
    if (!bRestored)
    {
        OutError = TEXT("Explore V5 failed to restore the exact pre-mutation materials, component flags or sun contact-shadow fields.");
        return false;
    }

    OutError.Reset();
    return true;
}
}

ATRIADIstanaExploreV5AppearanceActor::ATRIADIstanaExploreV5AppearanceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("ExploreV5AppearanceOnlyRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    ClaimLabel = ExpectedClaimLabel();
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedClaimLabel()
{
    return V5ClaimLabel;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedLawnMaterialPath()
{
    return V5LawnMaterialPath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedFountainWaterMaterialPath()
{
    return V5FountainWaterMaterialPath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedHardscapeStoneMaterialPath()
{
    return V5HardscapeStoneMaterialPath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedContextRenderMaterialPath()
{
    return V5ContextRenderMaterialPath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedContextRoofMaterialPath()
{
    return V5ContextRoofMaterialPath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfBaseMaterialPath()
{
    return V4CloseTurfBaseMaterialPath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfBaseColorTexturePath()
{
    return V3CloseTurfBaseColorTexturePath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfNormalTexturePath()
{
    return V3CloseTurfNormalTexturePath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfRoughnessTexturePath()
{
    return V3CloseTurfRoughnessTexturePath;
}

const FString& ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfOpacityTexturePath()
{
    return V3CloseTurfOpacityTexturePath;
}

FName ATRIADIstanaExploreV5AppearanceActor::ExpectedDirectionalSunTag()
{
    return DirectionalSunTag;
}

float ATRIADIstanaExploreV5AppearanceActor::ExpectedContactShadowLengthCentimeters()
{
    return ContactShadowLengthCentimeters;
}

bool ATRIADIstanaExploreV5AppearanceActor::ExpectedContactShadowLengthInWorldSpace()
{
    return ContactShadowLengthInWorldSpace;
}

float ATRIADIstanaExploreV5AppearanceActor::ExpectedContactShadowCastingIntensity()
{
    return ContactShadowCastingIntensity;
}

float ATRIADIstanaExploreV5AppearanceActor::ExpectedContactShadowNonCastingIntensity()
{
    return ContactShadowNonCastingIntensity;
}

int32 ATRIADIstanaExploreV5AppearanceActor::ExpectedContactShadowEnabledComponentCount()
{
    return ContactShadowEnabledComponentCount;
}

int32 ATRIADIstanaExploreV5AppearanceActor::ExpectedContactShadowDisabledComponentCount()
{
    return ContactShadowDisabledComponentCount;
}

#if WITH_DEV_AUTOMATION_TESTS
bool ATRIADIstanaExploreV5AppearanceActor::
    ValidateContactShadowRosterStateForAutomation(
        const TArray<FName>& ComponentNames,
        const TArray<uint8>& Mobilities,
        const TArray<bool>& CastShadows,
        const TArray<bool>& CastContactShadows,
        bool bRequireAppliedState,
        FString& OutError)
{
    return ValidateContactShadowRosterValuesForAutomation(
        ComponentNames,
        Mobilities,
        CastShadows,
        CastContactShadows,
        bRequireAppliedState
            ? EContactShadowRosterState::Applied
            : EContactShadowRosterState::InheritedPreMutation,
        OutError);
}

bool ATRIADIstanaExploreV5AppearanceActor::
    ApplyContactShadowRosterStateForAutomation(
        const TArray<FName>& ComponentNames,
        const TArray<uint8>& Mobilities,
        const TArray<bool>& CastShadows,
        TArray<bool>& InOutCastContactShadows,
        FString& OutError)
{
    return ApplyContactShadowRosterValuesForAutomation(
        ComponentNames,
        Mobilities,
        CastShadows,
        InOutCastContactShadows,
        OutError);
}

bool ATRIADIstanaExploreV5AppearanceActor::
    ValidateV5DTreeRuntimeContactShadowRosterForAutomation(
        const TArray<FName>& ComponentNames,
        const TArray<uint8>& Mobilities,
        const TArray<bool>& CastShadows,
        const TArray<bool>& CastContactShadows,
        FString& OutError)
{
    return ValidateContactShadowRosterValuesForAutomation(
        ComponentNames,
        Mobilities,
        CastShadows,
        CastContactShadows,
        EContactShadowRosterState::
            AppliedWithExactV5DTreeRuntimeOverride,
        OutError);
}
#endif

bool ATRIADIstanaExploreV5AppearanceActor::ConfigureExploreV5Appearance(
    ATRIADIstanaPublicViewSceneActor* InPublicViewSceneActor,
    ATRIADIstanaExploreV4LandscapeActor* InExploreV4LandscapeActor,
    UMaterialInterface* InLawnMaterial,
    UMaterialInterface* InFountainWaterMaterial,
    UMaterialInterface* InHardscapeStoneMaterial,
    UMaterialInterface* InContextRenderMaterial,
    UMaterialInterface* InContextRoofMaterial,
    UTexture2D* InCloseTurfBaseColorTexture,
    UTexture2D* InCloseTurfNormalTexture,
    UTexture2D* InCloseTurfRoughnessTexture,
    UTexture2D* InCloseTurfOpacityTexture,
    FString& OutError)
{
    ADirectionalLight* CandidateSun = nullptr;
    if (!ResolveUniqueTaggedDirectionalSun(
            GetWorld(), CandidateSun, OutError))
    {
        return false;
    }

    const UDirectionalLightComponent* CandidateSunComponent =
        ResolveDirectionalLightComponent(CandidateSun);
    if (!CandidateSunComponent)
    {
        OutError = TEXT("Explore V5 unique tagged directional sun has no directional-light component.");
        return false;
    }

    FExploreV5Configuration Candidate;
    Candidate.SceneActor = InPublicViewSceneActor;
    Candidate.V4Actor = InExploreV4LandscapeActor;
    Candidate.SunActor = CandidateSun;
    Candidate.Lawn = InLawnMaterial;
    Candidate.FountainWater = InFountainWaterMaterial;
    Candidate.HardscapeStone = InHardscapeStoneMaterial;
    Candidate.ContextRender = InContextRenderMaterial;
    Candidate.ContextRoof = InContextRoofMaterial;
    Candidate.CloseTurfBaseColor = InCloseTurfBaseColorTexture;
    Candidate.CloseTurfNormal = InCloseTurfNormalTexture;
    Candidate.CloseTurfRoughness = InCloseTurfRoughnessTexture;
    Candidate.CloseTurfOpacity = InCloseTurfOpacityTexture;
    Candidate.bSunBaselineRecorded = true;
    if (bInheritedSunPoseColorIntensityRecorded &&
        DirectionalSunActor == CandidateSun)
    {
        Candidate.RecordedSunTransform = RecordedInheritedSunTransform;
        Candidate.RecordedSunColor = RecordedInheritedSunColor;
        Candidate.RecordedSunIntensity = RecordedInheritedSunIntensity;
    }
    else
    {
        Candidate.RecordedSunTransform = CandidateSun->GetActorTransform();
        Candidate.RecordedSunColor = CandidateSunComponent->GetLightColor();
        Candidate.RecordedSunIntensity = CandidateSunComponent->Intensity;
    }

    if (!ValidateExploreV5Configuration(this, Candidate, OutError))
    {
        return false;
    }

    const bool bReapplyingSameInheritedTargets =
        PublicViewSceneActor == Candidate.SceneActor &&
        ExploreV4LandscapeActor == Candidate.V4Actor &&
        DirectionalSunActor == Candidate.SunActor;
    const TArray<FContactShadowRow> CandidateContactRows =
        BuildContactShadowRows(Candidate.SceneActor, Candidate.V4Actor);
    EContactShadowRosterState CandidateContactShadowState =
        EContactShadowRosterState::InheritedPreMutation;
    if (bReapplyingSameInheritedTargets &&
        !ResolveAppliedContactShadowRosterState(
            Candidate.V4Actor,
            CandidateContactShadowState,
            OutError))
    {
        return false;
    }
    if (!ValidateContactShadowRowsInState(
            CandidateContactRows,
            CandidateContactShadowState,
            OutError))
    {
        return false;
    }
    const FExploreV5Configuration Previous = ConfigurationFromActor(this);
    const bool bPreviousRuntimeCloseTurfTextureRebindApplied =
        bRuntimeCloseTurfTextureRebindApplied;

    PublicViewSceneActor = Candidate.SceneActor;
    ExploreV4LandscapeActor = Candidate.V4Actor;
    DirectionalSunActor = Candidate.SunActor;
    LawnMaterial = Candidate.Lawn;
    FountainWaterMaterial = Candidate.FountainWater;
    HardscapeStoneMaterial = Candidate.HardscapeStone;
    ContextRenderMaterial = Candidate.ContextRender;
    ContextRoofMaterial = Candidate.ContextRoof;
    CloseTurfBaseColorTexture = Candidate.CloseTurfBaseColor;
    CloseTurfNormalTexture = Candidate.CloseTurfNormal;
    CloseTurfRoughnessTexture = Candidate.CloseTurfRoughness;
    CloseTurfOpacityTexture = Candidate.CloseTurfOpacity;
    bInheritedSunPoseColorIntensityRecorded =
        Candidate.bSunBaselineRecorded;
    RecordedInheritedSunTransform = Candidate.RecordedSunTransform;
    RecordedInheritedSunColor = Candidate.RecordedSunColor;
    RecordedInheritedSunIntensity = Candidate.RecordedSunIntensity;
    bRuntimeCloseTurfTextureRebindApplied = false;

    if (!ReapplyExploreV5AppearanceWithExpectedContactShadowPreState(
            !bReapplyingSameInheritedTargets,
            OutError))
    {
        PublicViewSceneActor = Previous.SceneActor;
        ExploreV4LandscapeActor = Previous.V4Actor;
        DirectionalSunActor = Previous.SunActor;
        LawnMaterial = Previous.Lawn;
        FountainWaterMaterial = Previous.FountainWater;
        HardscapeStoneMaterial = Previous.HardscapeStone;
        ContextRenderMaterial = Previous.ContextRender;
        ContextRoofMaterial = Previous.ContextRoof;
        CloseTurfBaseColorTexture = Previous.CloseTurfBaseColor;
        CloseTurfNormalTexture = Previous.CloseTurfNormal;
        CloseTurfRoughnessTexture = Previous.CloseTurfRoughness;
        CloseTurfOpacityTexture = Previous.CloseTurfOpacity;
        bInheritedSunPoseColorIntensityRecorded =
            Previous.bSunBaselineRecorded;
        RecordedInheritedSunTransform = Previous.RecordedSunTransform;
        RecordedInheritedSunColor = Previous.RecordedSunColor;
        RecordedInheritedSunIntensity = Previous.RecordedSunIntensity;
        bRuntimeCloseTurfTextureRebindApplied =
            bPreviousRuntimeCloseTurfTextureRebindApplied;
        return false;
    }

    if (HasActorBegunPlay())
    {
        GetWorldTimerManager().SetTimerForNextTick(
            this,
            &ATRIADIstanaExploreV5AppearanceActor::
                ApplyDeferredRuntimeBindings);
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5AppearanceActor::ValidatePersistentInputsAndTargets(
    FString& OutError) const
{
    return ValidateExploreV5Configuration(
        this, ConfigurationFromActor(this), OutError);
}

bool ATRIADIstanaExploreV5AppearanceActor::ReapplyExploreV5Appearance(
    FString& OutError)
{
    return ReapplyExploreV5AppearanceWithExpectedContactShadowPreState(
        false,
        OutError);
}

bool ATRIADIstanaExploreV5AppearanceActor::
    ReapplyExploreV5AppearanceWithExpectedContactShadowPreState(
        bool bRequireInheritedContactShadowPreState,
        FString& OutError)
{
    if (!ValidatePersistentInputsAndTargets(OutError))
    {
        return false;
    }

    FResolvedAppearanceSlots Slots;
    if (!ResolveAppearanceSlots(PublicViewSceneActor, Slots, OutError))
    {
        return false;
    }
    const TArray<FContactShadowRow> ContactRows = BuildContactShadowRows(
        PublicViewSceneActor, ExploreV4LandscapeActor);

    const FExploreV5Configuration Configuration =
        ConfigurationFromActor(this);
    EContactShadowRosterState ExpectedContactShadowPreState =
        EContactShadowRosterState::InheritedPreMutation;
    if (!bRequireInheritedContactShadowPreState &&
        !ResolveAppliedContactShadowRosterState(
            ExploreV4LandscapeActor,
            ExpectedContactShadowPreState,
            OutError))
    {
        return false;
    }
    FAppearanceMutationSnapshot Snapshot;
    if (!CaptureAppearanceMutationSnapshot(
            Configuration,
            Slots,
            ContactRows,
            ExpectedContactShadowPreState,
            Snapshot,
            OutError))
    {
        return false;
    }
    if (!ApplyContactShadowRows(
            ContactRows, ExpectedContactShadowPreState, OutError))
    {
        const FString ApplyError = OutError;
        FString RollbackError;
        if (!RestoreAppearanceMutationSnapshot(
                Configuration, Slots, Snapshot, RollbackError))
        {
            OutError = ApplyError + TEXT(" Rollback failure: ") +
                RollbackError;
            return false;
        }
        OutError = ApplyError;
        return false;
    }

    PublicViewSceneActor->TerrainComponent->SetMaterial(0, LawnMaterial);
    PublicViewSceneActor->HardscapeComponent->SetMaterial(
        Slots.Water, FountainWaterMaterial);
    PublicViewSceneActor->HardscapeComponent->SetMaterial(
        Slots.Stone, HardscapeStoneMaterial);
    PublicViewSceneActor->OSMContextBuildingsComponent->SetMaterial(
        Slots.ContextRender, ContextRenderMaterial);
    PublicViewSceneActor->OSMContextBuildingsComponent->SetMaterial(
        Slots.ContextRoof, ContextRoofMaterial);
    ApplyExpectedSunContactShadowState(
        ResolveDirectionalLightComponent(DirectionalSunActor));

    FString ValidationReport;
    if (!ValidateExploreV5Appearance(ValidationReport, false))
    {
        FString RollbackError;
        if (!RestoreAppearanceMutationSnapshot(
                Configuration, Slots, Snapshot, RollbackError))
        {
            OutError = ValidationReport + TEXT(" Rollback failure: ") +
                RollbackError;
            return false;
        }
        OutError = ValidationReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5AppearanceActor::ValidateExploreV5Appearance(
    FString& OutReport,
    bool bRequireRuntimeCloseTurfRebind) const
{
    FString Error;
    if (!ValidatePersistentInputsAndTargets(Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID: ") + Error;
        return false;
    }

    FResolvedAppearanceSlots Slots;
    if (!ResolveAppearanceSlots(PublicViewSceneActor, Slots, Error) ||
        PublicViewSceneActor->TerrainComponent->GetMaterial(0) !=
            LawnMaterial ||
        PublicViewSceneActor->HardscapeComponent->GetMaterial(Slots.Water) !=
            FountainWaterMaterial ||
        PublicViewSceneActor->HardscapeComponent->GetMaterial(Slots.Stone) !=
            HardscapeStoneMaterial ||
        PublicViewSceneActor->OSMContextBuildingsComponent->GetMaterial(
            Slots.ContextRender) != ContextRenderMaterial ||
        PublicViewSceneActor->OSMContextBuildingsComponent->GetMaterial(
            Slots.ContextRoof) != ContextRoofMaterial)
    {
        if (Error.IsEmpty())
        {
            Error = TEXT("An exact V5 material override is missing from its admitted slot.");
        }
        OutReport = TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID: ") + Error;
        return false;
    }

    const TArray<FContactShadowRow> ContactRows = BuildContactShadowRows(
        PublicViewSceneActor, ExploreV4LandscapeActor);
    EContactShadowRosterState AppliedContactShadowState =
        EContactShadowRosterState::Applied;
    if (!ValidateAppliedContactShadowRows(
            ContactRows,
            ExploreV4LandscapeActor,
            AppliedContactShadowState,
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID: ") + Error;
        return false;
    }
    if (!DirectionalSunActor ||
        !HasExpectedSunContactShadowState(
            ResolveDirectionalLightComponent(DirectionalSunActor)))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID: The unique exact-tag directional sun did not read back the required 50 cm world-space contact length and 1/0 casting/non-casting intensities.");
        return false;
    }
    FString ContactShadowConsoleReport;
    if (!ValidateContactShadowConsoleState(
            Error, &ContactShadowConsoleReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID: ") + Error;
        return false;
    }

    const bool bRequireExactRuntimeTextures =
        bRequireRuntimeCloseTurfRebind ||
        bRuntimeCloseTurfTextureRebindApplied;
    if (bRequireExactRuntimeTextures)
    {
        UMaterialInstanceDynamic* CloseTurfMid =
            ResolveExactCloseTurfRuntimeMid(
                ExploreV4LandscapeActor, Error);
        if (!bRuntimeCloseTurfTextureRebindApplied || !CloseTurfMid ||
            !HasExactCloseTurfTextureBindings(
                CloseTurfMid,
                CloseTurfBaseColorTexture,
                CloseTurfNormalTexture,
                CloseTurfRoughnessTexture,
                CloseTurfOpacityTexture,
                Error))
        {
            if (Error.IsEmpty())
            {
                Error = TEXT("The required next-tick close-turf texture rebind has not completed.");
            }
            OutReport = TEXT("ISTANA_EXPLORE_V5_APPEARANCE_INVALID: ") + Error;
            return false;
        }
    }

    const int32 EffectiveContactShadowEnabledCount =
        ExpectedEnabledContactShadowCount(AppliedContactShadowState);
    const int32 EffectiveContactShadowDisabledCount =
        ContactShadowComponentCount - EffectiveContactShadowEnabledCount;
    const bool bExactV5DTreeRuntimeOverride =
        AppliedContactShadowState ==
            EContactShadowRosterState::
                AppliedWithExactV5DTreeRuntimeOverride;
    OutReport = FString::Printf(
        TEXT("Validated additive Explore V5 appearance-only bindings: five exact material references; base contact-shadow contract %d enabled/%d disabled, effective runtime %d enabled/%d disabled, exactV5DTreeRuntimeSuppression=%s; unique exact-tag sun at 50 cm world-space contact length with casting/non-casting intensities 1/0; renderer CVar readback [%s]; close-turf V4 wind base, mesh/count/transforms/cull/WPO/collision preserved%s. Public-data visual approximation only; no geometry, transform, collision, navigation, survey, botanical, sensor or RF authority. Inherited sun pose, colour and intensity remain untouched; only bounded contact-shadow state is admitted."),
        ContactShadowEnabledComponentCount,
        ContactShadowDisabledComponentCount,
        EffectiveContactShadowEnabledCount,
        EffectiveContactShadowDisabledCount,
        bExactV5DTreeRuntimeOverride ? TEXT("true") : TEXT("false"),
        *ContactShadowConsoleReport,
        bRequireExactRuntimeTextures
            ? TEXT(" with four exact next-tick runtime texture parameters")
            : TEXT(" awaiting runtime MID texture rebind"));
    return true;
}

bool ATRIADIstanaExploreV5AppearanceActor::ApplyCloseTurfRuntimeTextureRebind(
    FString& OutError)
{
    if (!ValidatePersistentInputsAndTargets(OutError))
    {
        return false;
    }
    UMaterialInstanceDynamic* Mid = ResolveExactCloseTurfRuntimeMid(
        ExploreV4LandscapeActor, OutError);
    if (!Mid)
    {
        return false;
    }

    const FName Parameters[] = {
        CloseTurfBaseColorParameter,
        CloseTurfNormalParameter,
        CloseTurfRoughnessParameter,
        CloseTurfOpacityParameter};
    UTexture* PreviousTextures[UE_ARRAY_COUNT(Parameters)] = {};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Parameters); ++Index)
    {
        if (!Mid->GetTextureParameterValue(
                FMaterialParameterInfo(Parameters[Index]),
                PreviousTextures[Index]))
        {
            OutError = FString::Printf(
                TEXT("Exact V4 close-turf wind base lacks required texture parameter '%s'; no V5 texture mutation was applied."),
                *Parameters[Index].ToString());
            return false;
        }
    }

    Mid->SetTextureParameterValue(
        CloseTurfBaseColorParameter, CloseTurfBaseColorTexture);
    Mid->SetTextureParameterValue(
        CloseTurfNormalParameter, CloseTurfNormalTexture);
    Mid->SetTextureParameterValue(
        CloseTurfRoughnessParameter, CloseTurfRoughnessTexture);
    Mid->SetTextureParameterValue(
        CloseTurfOpacityParameter, CloseTurfOpacityTexture);
    if (!HasExactCloseTurfTextureBindings(
            Mid,
            CloseTurfBaseColorTexture,
            CloseTurfNormalTexture,
            CloseTurfRoughnessTexture,
            CloseTurfOpacityTexture,
            OutError))
    {
        const FString ValidationError = OutError;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Parameters); ++Index)
        {
            Mid->SetTextureParameterValue(
                Parameters[Index], PreviousTextures[Index]);
        }
        OutError = ValidationError;
        return false;
    }
    bRuntimeCloseTurfTextureRebindApplied = true;
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5AppearanceActor::ApplyDeferredRuntimeBindings()
{
    FString Error;
    if (!ApplyCloseTurfRuntimeTextureRebind(Error))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5Appearance,
            Error,
            TEXT("ISTANA_EXPLORE_V5_CLOSE_TURF_REBIND_FAILED: %s"),
            *Error);
        return;
    }

    FString Report;
    if (!ValidateExploreV5Appearance(Report, true))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5Appearance,
            Error,
            TEXT("ISTANA_EXPLORE_V5_RUNTIME_VALIDATION_FAILED: %s"),
            *Report);
        return;
    }
    UE_LOG(
        LogTRIADIstanaExploreV5Appearance,
        Display,
        TEXT("ISTANA_EXPLORE_V5_RUNTIME_APPEARANCE_PASS %s"),
        *Report);
}

void ATRIADIstanaExploreV5AppearanceActor::BeginPlay()
{
    Super::BeginPlay();
    bRuntimeCloseTurfTextureRebindApplied = false;

    FString Error;
    if (!ReapplyExploreV5Appearance(Error))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5Appearance,
            Error,
            TEXT("ISTANA_EXPLORE_V5_BEGIN_PLAY_REAPPLY_FAILED: %s"),
            *Error);
        return;
    }
    GetWorldTimerManager().SetTimerForNextTick(
        this,
        &ATRIADIstanaExploreV5AppearanceActor::ApplyDeferredRuntimeBindings);
}
