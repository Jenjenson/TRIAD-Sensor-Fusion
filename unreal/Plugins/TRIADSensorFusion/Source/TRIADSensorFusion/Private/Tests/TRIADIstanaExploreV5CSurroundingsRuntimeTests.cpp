#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaPublicViewSceneActor.h"

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5CSurroundingsRuntimeContractTest,
    "TRIAD.Istana.ExploreV5C.Surroundings.RuntimeContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5CSurroundingsRuntimeContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const ATRIADIstanaPublicViewSceneActor* Defaults =
        GetDefault<ATRIADIstanaPublicViewSceneActor>();
    TestNotNull(TEXT("Public-view class default object"), Defaults);
    if (!Defaults)
    {
        return false;
    }

    TestNotNull(
        TEXT("Legacy ODbL surroundings component"),
        Defaults->OSMContextBuildingsComponent.Get());
    if (Defaults->OSMContextBuildingsComponent)
    {
        TestTrue(
            TEXT("Legacy ODbL fallback defaults auto-active"),
            Defaults->OSMContextBuildingsComponent->bAutoActivate);
    }
    TestNotNull(
        TEXT("V5C render-only surroundings sibling"),
        Defaults->V5CSurroundingsRenderOnlyComponent.Get());
    TestNotNull(
        TEXT("V5C render-only official planning ground-context sibling"),
        Defaults->V5CGroundContextRenderOnlyComponent.Get());
    TestFalse(
        TEXT("V5C surroundings default presentation is inactive"),
        Defaults->IsV5CSurroundingsPresentationActive());
    TestTrue(
        TEXT("V5C surroundings are explicitly render-only"),
        Defaults->bV5CSurroundingsRenderOnly);
    TestFalse(
        TEXT("V5C surroundings have no collision/navigation/sensor/RF authority"),
        Defaults->bV5CSurroundingsCollisionNavigationSensorOrRfAuthority);
    TestFalse(
        TEXT("V5C surroundings claim no measured heights"),
        Defaults->bV5CSurroundingsMeasuredHeightClaimed);
    TestFalse(
        TEXT("V5C surroundings claim no facade or roof-form fidelity"),
        Defaults->bV5CSurroundingsFacadeOrRoofFormClaimed);
    TestFalse(
        TEXT("V5C surroundings claim no physical terrain grade or foundation"),
        Defaults->bV5CSurroundingsTerrainGradeOrFoundationClaimed);
    TestFalse(
        TEXT("V5C surroundings claim no visual acceptance"),
        Defaults->bV5CSurroundingsVisualAcceptanceClaimed);
    TestTrue(
        TEXT("V5C official planning ground context is explicitly render-only"),
        Defaults->bV5CGroundContextRenderOnly);
    TestFalse(
        TEXT("V5C ground context claims no road-width or road-material authority"),
        Defaults->bV5CGroundContextRoadWidthOrMaterialClaimed);
    TestFalse(
        TEXT("V5C ground context claims no elevation/Z or survey authority"),
        Defaults->bV5CGroundContextElevationOrSurveyClaimed);
    TestFalse(
        TEXT("V5C ground context has no collision/navigation/sensor/RF authority"),
        Defaults->bV5CGroundContextCollisionNavigationSensorOrRfAuthority);

    const UStaticMeshComponent* Component =
        Defaults->V5CSurroundingsRenderOnlyComponent;
    if (Component)
    {
        TestEqual(
            TEXT("V5C sibling attaches directly to scene root"),
            Component->GetAttachParent(),
            Defaults->SceneRoot.Get());
        TestNull(
            TEXT("V5C sibling has no default mesh"),
            Component->GetStaticMesh());
        TestFalse(
            TEXT("V5C sibling defaults hidden"),
            Component->IsVisible());
        TestTrue(
            TEXT("V5C sibling defaults hidden in game"),
            Component->bHiddenInGame);
        TestFalse(
            TEXT("V5C sibling does not auto-activate"),
            Component->bAutoActivate);
        TestFalse(
            TEXT("V5C sibling defaults inactive"),
            Component->IsActive());
        TestEqual(
            TEXT("V5C sibling defaults NoCollision"),
            Component->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        TestFalse(
            TEXT("V5C sibling generates no overlaps"),
            Component->GetGenerateOverlapEvents());
        TestFalse(
            TEXT("V5C sibling affects no navigation"),
            Component->CanEverAffectNavigation());
        TestTrue(
            TEXT("V5C sibling is hidden from scene-capture sensors"),
            Component->bHiddenInSceneCapture);
        TestTrue(
            TEXT("V5C sibling is excluded from sensor capture and target bounds"),
            Component->ComponentHasTag(
                FName(TEXT("TRIADHumanOnlyOverlay"))));
        TestFalse(
            TEXT("V5C sibling adds no contact-shadow presentation authority"),
            Component->bCastContactShadow);
        TestTrue(
            TEXT("V5C sibling defaults identity"),
            Component->GetRelativeTransform().Equals(
                FTransform::Identity, 0.0));
        TestTrue(
            TEXT("V5C sibling ignores every collision/trace channel"),
            Component->GetCollisionResponseToChannels() ==
                FCollisionResponseContainer(ECR_Ignore));
    }

    const UStaticMeshComponent* GroundComponent =
        Defaults->V5CGroundContextRenderOnlyComponent;
    if (GroundComponent)
    {
        TestTrue(
            TEXT("V5C ground sibling has stable native subobject name"),
            GroundComponent->GetFName() ==
                FName(TEXT("V5COfficialPlanningGroundContextRenderOnlySuccessor")));
        TestEqual(
            TEXT("V5C ground sibling attaches directly to scene root"),
            GroundComponent->GetAttachParent(),
            Defaults->SceneRoot.Get());
        TestNull(
            TEXT("V5C ground sibling has no default mesh"),
            GroundComponent->GetStaticMesh());
        TestFalse(
            TEXT("V5C ground sibling defaults hidden"),
            GroundComponent->IsVisible());
        TestTrue(
            TEXT("V5C ground sibling defaults hidden in game"),
            GroundComponent->bHiddenInGame);
        TestFalse(
            TEXT("V5C ground sibling does not auto-activate"),
            GroundComponent->bAutoActivate);
        TestFalse(
            TEXT("V5C ground sibling defaults inactive"),
            GroundComponent->IsActive());
        TestEqual(
            TEXT("V5C ground sibling defaults NoCollision"),
            GroundComponent->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        TestFalse(
            TEXT("V5C ground sibling generates no overlaps"),
            GroundComponent->GetGenerateOverlapEvents());
        TestFalse(
            TEXT("V5C ground sibling affects no navigation"),
            GroundComponent->CanEverAffectNavigation());
        TestTrue(
            TEXT("V5C ground sibling is hidden from scene-capture sensors"),
            GroundComponent->bHiddenInSceneCapture);
        TestTrue(
            TEXT("V5C ground sibling is excluded from sensor capture and target bounds"),
            GroundComponent->ComponentHasTag(
                FName(TEXT("TRIADHumanOnlyOverlay"))));
        TestFalse(
            TEXT("V5C ground sibling casts no shadows"),
            GroundComponent->CastShadow);
        TestFalse(
            TEXT("V5C ground sibling adds no contact shadows"),
            GroundComponent->bCastContactShadow);
        TestTrue(
            TEXT("V5C ground sibling defaults identity"),
            GroundComponent->GetRelativeTransform().Equals(
                FTransform::Identity, 0.0));
        TestTrue(
            TEXT("V5C ground sibling ignores every collision/trace channel"),
            GroundComponent->GetCollisionResponseToChannels() ==
                FCollisionResponseContainer(ECR_Ignore));
    }

    TestEqual(
        TEXT("Frozen V5C surroundings OBJ digest"),
        Defaults->FrozenV5CSurroundingsObjSha256,
        FString(TEXT("774F7E30456B989D0BF9EEB013C10D2688A2B3DE87936529BBB154E83D344C74")));
    TestEqual(
        TEXT("Frozen V5C surroundings MTL digest"),
        Defaults->FrozenV5CSurroundingsMtlSha256,
        FString(TEXT("6BBDA30E125F92EEF36D060404CA6EBB3F7DFC9DF95D7895766A57A99A737CA7")));
    TestEqual(
        TEXT("Frozen V5C surroundings feature-sidecar digest"),
        Defaults->FrozenV5CSurroundingsFeaturesSha256,
        FString(TEXT("8825DDC93E7C6465B01AD5A5AD23B2E8368F2C825B6F8F8EB1FBBD2DA8138793")));
    TestEqual(
        TEXT("Frozen V5C surroundings manifest digest"),
        Defaults->FrozenV5CSurroundingsManifestSha256,
        FString(TEXT("CEBFA56EC84E697305A35DA9CCD06B29619AB4500701B4A806B958B415F04F20")));
    TestEqual(
        TEXT("Frozen V5C surroundings contract digest"),
        Defaults->FrozenV5CSurroundingsContractSha256,
        FString(TEXT("52587014FC80459732B1C943056D1724684287B67CA0DE8556AF7E1E2F8C7FBD")));

    return true;
}

#endif
