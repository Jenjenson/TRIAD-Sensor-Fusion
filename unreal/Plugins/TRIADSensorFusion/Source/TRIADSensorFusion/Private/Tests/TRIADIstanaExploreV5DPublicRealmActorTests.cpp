#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5DPublicRealmActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DPublicRealmDefaultsTest,
    "TRIAD.Istana.ExploreV5D.PublicRealm.Defaults",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DPublicRealmDefaultsTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const ATRIADIstanaExploreV5DPublicRealmActor* Defaults =
        GetDefault<ATRIADIstanaExploreV5DPublicRealmActor>();
    TestNotNull(TEXT("V5D public-realm class default object"), Defaults);
    if (!Defaults)
    {
        return false;
    }

    TestFalse(TEXT("Public realm never ticks"), Defaults->PrimaryActorTick.bCanEverTick);
    TestFalse(TEXT("Public realm actor collision is disabled"), Defaults->GetActorEnableCollision());
    TestFalse(TEXT("Public realm defaults unconfigured"), Defaults->bConfigured);
    TestFalse(TEXT("Provider defaults not ready"), Defaults->bProviderReady);
    TestTrue(TEXT("Public realm is render-only"), Defaults->bRenderOnly);
    TestFalse(TEXT("Public realm has no collision/navigation authority"), Defaults->bCollisionOrNavigationAuthority);
    TestFalse(TEXT("Public realm has no sensor/RF material authority"), Defaults->bSensorOrRfMaterialAuthority);
    TestFalse(TEXT("Public realm generates no runtime geometry"), Defaults->bRuntimeGeometryGenerated);
    TestFalse(TEXT("Public realm modifies no R15 ground packages"), Defaults->bR15GroundMaterialPackagesModified);
    TestFalse(TEXT("Public realm modifies no existing RF inputs"), Defaults->bExistingRfInputsModified);
    TestEqual(
        TEXT("Unconfigured class default remains the immutable planning-material predecessor"),
        Defaults->AppearanceRevision,
        1);
    TestEqual(
        TEXT("Validated configured presentation targets appearance revision 2"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedAppearanceRevision(),
        2);
    TestFalse(
        TEXT("Public realm actor is not a screenshot-suppressed debug overlay"),
        Defaults->ActorHasTag(FName(TEXT("TRIADHumanOnlyOverlay"))));
    TestEqual(
        TEXT("Core mesh object path"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedCoreMeshObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/SM_IPV5D_PublicRealm_Core_Render.SM_IPV5D_PublicRealm_Core_Render")));
    TestEqual(
        TEXT("Fallback mesh object path"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedFallbackMeshObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/SM_IPV5D_PublicRealm_Fallback_Render.SM_IPV5D_PublicRealm_Fallback_Render")));
    TestEqual(
        TEXT("Road-base material object path"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedRoadBaseMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmVisualR2/Materials/M_IPV5D_PublicRealm_AsphaltDry_R2.M_IPV5D_PublicRealm_AsphaltDry_R2")));
    TestEqual(
        TEXT("Official-planning-graphic presentation uses the fully clipped object path"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedRoadGraphicMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmVisualR2/Materials/M_IPV5D_PublicRealm_RoadGraphicFullyClipped_R2.M_IPV5D_PublicRealm_RoadGraphicFullyClipped_R2")));
    TestNotEqual(
        TEXT("Road-base and official-graphic presentation materials are isolated"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedRoadBaseMaterialObjectPath(),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedRoadGraphicMaterialObjectPath());
    TestEqual(
        TEXT("Concrete material object path"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedConcreteMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/Materials/MI_IPV5D_PublicRealmConcrete.MI_IPV5D_PublicRealmConcrete")));
    TestEqual(
        TEXT("Provenance schema revision"),
        ATRIADIstanaExploreV5DPublicRealmActor::ExpectedProvenanceSchemaRevision(),
        FString(TEXT("TRIAD_IPV5D_PUBLIC_REALM_PROVENANCE_V1")));

    const UStaticMeshComponent* Components[] = {
        Defaults->CorePublicRealmRenderOnly,
        Defaults->FallbackPublicRealmRenderOnly};
    const FName Names[] = {
        TEXT("CorePublicRealmRenderOnly"),
        TEXT("FallbackPublicRealmRenderOnly")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Components); ++Index)
    {
        const UStaticMeshComponent* Component = Components[Index];
        TestNotNull(TEXT("Owned public-realm component"), Component);
        if (!Component)
        {
            continue;
        }
        TestEqual(TEXT("Stable native component name"), Component->GetFName(), Names[Index]);
        TestEqual(TEXT("Component attaches to root"), Component->GetAttachParent(), Defaults->SceneRoot.Get());
        TestTrue(TEXT("Component defaults identity"), Component->GetRelativeTransform().Equals(FTransform::Identity, 0.0));
        TestEqual(TEXT("Component is static"), Component->Mobility, EComponentMobility::Static);
        TestNull(TEXT("Unconfigured component has no mesh"), Component->GetStaticMesh());
        TestEqual(TEXT("Component is NoCollision"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestTrue(TEXT("Component ignores every channel"), Component->GetCollisionResponseToChannels() == FCollisionResponseContainer(ECR_Ignore));
        TestFalse(TEXT("Component generates no overlaps"), Component->GetGenerateOverlapEvents());
        TestFalse(TEXT("Component affects no navigation"), Component->CanEverAffectNavigation());
        TestTrue(TEXT("Component is excluded from scene-capture sensors"), Component->bHiddenInSceneCapture);
        TestFalse(TEXT("Component does not enter custom depth"), Component->bRenderCustomDepth);
        TestFalse(TEXT("Component is not a screenshot-suppressed debug overlay"), Component->ComponentHasTag(FName(TEXT("TRIADHumanOnlyOverlay"))));
        TestFalse(TEXT("Unconfigured component defaults invisible"), Component->IsVisible());
        TestTrue(TEXT("Unconfigured component defaults hidden in game"), Component->bHiddenInGame);
    }

    FTRIADIstanaExploreV5DPublicRealmProvenance Provenance;
    Provenance.SourceEpoch = TEXT("2026-08-31");
    Provenance.CoreSourceIdentifier = TEXT("core-source");
    Provenance.CoreSourceSha256 = FString::ChrN(64, TEXT('A'));
    Provenance.FallbackSourceIdentifier = TEXT("fallback-source");
    Provenance.FallbackSourceSha256 = FString::ChrN(64, TEXT('0'));
    FString Error;
    TestTrue(
        TEXT("Canonical source-specific provenance is admitted"),
        ATRIADIstanaExploreV5DPublicRealmActor::
            ValidateProvenanceContract(Provenance, Error));
    Provenance.bExistingRfInputsUntouched = false;
    TestFalse(
        TEXT("RF-mutating provenance fails closed"),
        ATRIADIstanaExploreV5DPublicRealmActor::
            ValidateProvenanceContract(Provenance, Error));
    return true;
}

#endif
