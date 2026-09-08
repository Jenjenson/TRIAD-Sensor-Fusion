from __future__ import annotations

import json
import re
import unittest
from collections import Counter, defaultdict
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV2LandscapeActor.h"
RUNTIME_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV2LandscapeActor.cpp"
PUBLIC_VIEW_SCENE_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaPublicViewSceneActor.h"
PUBLIC_VIEW_SCENE_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaPublicViewSceneActor.cpp"
EDITOR_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV2EditorLibrary.h"
EDITOR_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV2EditorLibrary.cpp"
CONTRACT = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV2/explore_v2.contract.json"
EXPLORE_GAME_MODE_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreGameMode.cpp"
V1_BUILDING_GENERATOR = REPO / "unreal/SourceAssets/IstanaPublicView/generate_public_view_building.py"
V1_COLLISION_OBJ = REPO / "unreal/SourceAssets/IstanaPublicView/Generated/SM_IstanaPublicView_Building_Collision.obj"
V5_BUILDING_GENERATOR = REPO / "unreal/SourceAssets/IstanaPublicViewV5/generate_hero_v5.py"
V6_CONTRACT = REPO / "unreal/SourceAssets/IstanaPublicViewV6/istana_public_view_hybrid_v6.contract.json"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class IstanaExploreV2MapEditorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = text(RUNTIME_H)
        cls.runtime_cpp = text(RUNTIME_CPP)
        cls.public_view_scene_h = text(PUBLIC_VIEW_SCENE_H)
        cls.public_view_scene_cpp = text(PUBLIC_VIEW_SCENE_CPP)
        cls.editor_h = text(EDITOR_H)
        cls.editor_cpp = text(EDITOR_CPP)
        cls.contract = json.loads(text(CONTRACT))
        cls.explore_game_mode_cpp = text(EXPLORE_GAME_MODE_CPP)
        cls.v1_building_generator = text(V1_BUILDING_GENERATOR)
        cls.v5_building_generator = text(V5_BUILDING_GENERATOR)
        cls.v6_contract = json.loads(text(V6_CONTRACT))

    def test_additive_namespace_and_map_chain_are_exact(self) -> None:
        self.assertIn('/Game/Maps/Istana_PublicView_Explore_v1', self.editor_cpp)
        self.assertIn('/Game/Maps/Istana_PublicView_Explore_v2', self.editor_cpp)
        self.assertIn('/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation', self.editor_cpp)
        self.assertIn('DuplicateLoadedAsset', self.editor_cpp)
        self.assertIn('ValidateProtectedPackages', self.editor_cpp)
        self.assertIn('CollectLegacyTreeWorldTransforms', self.editor_cpp)
        self.assertIn('ClearLegacyTreeVisuals', self.editor_cpp)
        self.assertIn('CountLegacyTrees(Scene) != 0', self.editor_cpp)
        self.assertNotIn('Istana_PublicView_Exterior_v5", DestinationMapPackage', self.editor_cpp)

    def test_map_revalidates_exact_visual_collision_authorities_and_channels(self) -> None:
        validate_world = self.editor_cpp[self.editor_cpp.index('bool ValidateWorld('):]
        self.assertIn('Scene->ValidatePublicViewScene(SceneReport, false)', validate_world)
        self.assertIn(
            'bool bRequireLegacyTreeInstances = true',
            self.public_view_scene_h,
        )
        self.assertEqual(
            self.public_view_scene_cpp.count('bRequireLegacyTreeInstances &&'),
            2,
        )
        self.assertIn(
            'ValidateAuthoritativeSensorMeshSplit(Scene, SensorMeshReport)',
            validate_world,
        )
        for path in (
            '/Game/TRIAD/IstanaPublicViewV5/Building/SM_IstanaPublicViewV5_Building_Hero.SM_IstanaPublicViewV5_Building_Hero',
            '/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision',
            '/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Terrain.SM_IstanaPublicView_Terrain',
            '/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Hardscape.SM_IstanaPublicView_Hardscape',
        ):
            self.assertIn(path, self.editor_cpp)
        for marker in (
            'CTF_UseComplexAsSimple',
            'ECollisionEnabled::QueryAndPhysics',
            'ECC_WorldStatic',
            'GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block',
            'GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block',
        ):
            self.assertIn(marker, self.editor_cpp)

    def test_retained_v1_collision_gap_has_an_exact_visual_ray_witness(self) -> None:
        # The lower collision authority ends at 14.00 m and the tower authority
        # begins at 14.20 m.  V5's opaque foundation plinth spans the gap, so a
        # horizontal world ray at z=14.10 m sees render geometry but no V1 hull.
        self.assertIn(
            '((0.0, -0.8, 7.0), (115.0, 14.0, 14.0))',
            self.v1_building_generator,
        )
        self.assertIn(
            '((0.0, -3.0, 18.7), (29.0, 29.0, 9.0))',
            self.v1_building_generator,
        )
        for marker in (
            'UPPER_TOWER_BODY_WIDTH = 21.70',
            'UPPER_TOWER_BODY_FRONT_Y = 12.22',
            'f"{GROUP_PREFIX}UpperTowerFoundationPlinth"',
            '14.475',
            '1.65',
        ):
            self.assertIn(marker, self.v5_building_generator)

        lower_collision_top = 7.0 + 14.0 * 0.5
        tower_collision_bottom = 18.7 - 9.0 * 0.5
        probe_z = 14.10
        visual_plinth_bottom = 14.475 - 1.65 * 0.5
        visual_plinth_top = 14.475 + 1.65 * 0.5
        self.assertAlmostEqual(tower_collision_bottom - lower_collision_top, 0.20)
        self.assertLess(lower_collision_top, probe_z)
        self.assertLess(probe_z, tower_collision_bottom)
        self.assertLess(visual_plinth_bottom, probe_z)
        self.assertLess(probe_z, visual_plinth_top)
        self.assertAlmostEqual(100.0 - 12.22, 87.78)  # +Y origin, unit -Y ray.

        # This known proxy mismatch is legal only while Explore remains visual
        # navigation and V6 remains uninstalled with every depth/LiDAR claim off.
        self.assertIn(
            'visual navigation mode, not a sensor simulation',
            self.explore_game_mode_cpp,
        )
        self.assertFalse(
            self.v6_contract['claimBoundary']['depthLidarOrMultisensorTwinClaimed']
        )
        self.assertFalse(self.v6_contract['receiverContract']['collisionEnabled'])
        self.assertTrue(
            self.v6_contract['receiverContract']['visualCollisionDistanceAuditRequired']
        )
        self.assertEqual(
            self.v6_contract['collisionAuthority']['authorityRevision'],
            'V1_UNCHANGED',
        )
        self.assertFalse(self.v6_contract['isolation']['liveInstallAuthorized'])

    def test_retained_v1_collision_hulls_are_individually_closed_and_wound(self) -> None:
        vertices: list[tuple[float, float, float]] = []
        faces_by_group: dict[str, list[tuple[int, int, int]]] = defaultdict(list)
        current_group = ''
        for line in text(V1_COLLISION_OBJ).splitlines():
            fields = line.split()
            if not fields:
                continue
            if fields[0] == 'v':
                vertices.append(tuple(float(value) for value in fields[1:4]))
            elif fields[0] == 'g':
                current_group = fields[1]
            elif fields[0] == 'f':
                self.assertTrue(current_group)
                indices = tuple(int(token.split('/')[0]) - 1 for token in fields[1:])
                self.assertEqual(len(indices), 3)
                faces_by_group[current_group].append(indices)

        self.assertEqual(len(faces_by_group), 7)
        signed_volumes: list[float] = []
        for group, faces in faces_by_group.items():
            self.assertRegex(group, r'^IPV_CollisionHull_[0-9]{2}$')
            self.assertEqual(len(faces), 12)
            undirected_edges: Counter[tuple[tuple[float, ...], tuple[float, ...]]] = Counter()
            directed_edges: Counter[tuple[tuple[float, ...], tuple[float, ...]]] = Counter()
            signed_volume = 0.0
            for indices in faces:
                a, b, c = (vertices[index] for index in indices)
                signed_volume += (
                    a[0] * (b[1] * c[2] - b[2] * c[1])
                    + a[1] * (b[2] * c[0] - b[0] * c[2])
                    + a[2] * (b[0] * c[1] - b[1] * c[0])
                ) / 6.0
                for start, end in ((a, b), (b, c), (c, a)):
                    undirected_edges[tuple(sorted((start, end)))] += 1
                    directed_edges[(start, end)] += 1
            self.assertEqual(len(undirected_edges), 18)
            self.assertTrue(all(count == 2 for count in undirected_edges.values()))
            for start, end in undirected_edges:
                self.assertEqual(directed_edges[(start, end)], 1)
                self.assertEqual(directed_edges[(end, start)], 1)
            signed_volumes.append(signed_volume)
        self.assertTrue(all(volume > 0.0 for volume in signed_volumes))

    def test_exact_deterministic_census_is_bound_in_source_and_contract(self) -> None:
        expected = {
            'ExpectedNearUmbrellaBroadleafCount': 48,
            'ExpectedNearColumnarBroadleafCount': 64,
            'ExpectedNearDomeBroadleafCount': 42,
            'ExpectedNearPalmCount': 6,
            'ExpectedOuterUmbrellaBroadleafCount': 224,
            'ExpectedOuterColumnarBroadleafCount': 168,
            'ExpectedOuterDomeBroadleafCount': 140,
            'ExpectedOuterPalmCount': 28,
            'ExpectedOuterTreeCount': 560,
            'ExpectedShrubACount': 512,
            'ExpectedShrubBCount': 320,
            'ExpectedHedgeCount': 192,
            'ExpectedGroundcoverCount': 768,
            'ExpectedNearTurfCount': 6144,
            'ExpectedMeadowSedgeCount': 1536,
        }
        for name, value in expected.items():
            self.assertRegex(
                self.runtime_cpp,
                rf'constexpr int32 {name} = {value};',
            )
        self.assertIn('static_assert(ExpectedTreeCount == 720);', self.runtime_cpp)
        census = self.contract['vegetationPolicy']['exactInstanceCensus']
        self.assertEqual(census['v2VisibleTrees'], 720)
        self.assertEqual(census['v2VisibleInstances'], 10192)
        self.assertEqual(census['hiddenPawnTreeBlockers'], 720)
        self.assertEqual(census['outerInheritedTreePositionsReplanted'], 560)
        self.assertEqual(census['outerInheritedPositionsUsingHighDetailBroadleaf'], 532)
        self.assertEqual(census['outerInheritedPositionsUsingProceduralPalmProxy'], 28)
        self.assertEqual(census['lowDetailLegacySceneTreeInstancesRemaining'], 0)
        self.assertGreater(census['nearTurf'] + census['meadowSedge'], 7000)

    def test_tall_varied_tree_habits_and_sparse_palms_are_explicit(self) -> None:
        for component in (
            'UmbrellaBroadleafInstances',
            'ColumnarBroadleafInstances',
            'DomeBroadleafInstances',
            'PalmInstances',
        ):
            self.assertIn(component, self.runtime_h)
            self.assertIn(component, self.runtime_cpp)
        self.assertIn('MaximumTreeScaleZ - MinimumTreeScaleZ < 0.90f', self.runtime_cpp)
        self.assertIn('MaximumTreeScaleZ < 1.88f', self.runtime_cpp)
        self.assertIn('MinimumTreeHeightMeters < 17.95', self.runtime_cpp)
        self.assertIn('MaximumTreeHeightMeters > 37.05', self.runtime_cpp)
        self.assertIn('required 18-37 m physical height/scale range', self.runtime_cpp)
        self.assertIn('OuterMixRandom', self.runtime_cpp)
        self.assertIn('SortedOuterTransforms.Swap', self.runtime_cpp)
        self.assertNotIn('Hyperreal Landscape', self.editor_cpp)
        self.assertIn('28 sparse procedural palm-proxy positions', self.editor_cpp)
        self.assertLess(
            self.contract['vegetationPolicy']['exactInstanceCensus']['palm'],
            self.contract['vegetationPolicy']['exactInstanceCensus']['v2VisibleTrees'] * 0.1,
        )

    def test_wind_graph_and_aftereffect_spring_are_not_a_static_loop(self) -> None:
        for parameter in (
            'TRIAD_WindStrengthCm',
            'TRIAD_WindSpeed',
            'TRIAD_WindDirection',
            'TRIAD_WindHeightCm',
            'TRIAD_WindResponseScale',
        ):
            self.assertIn(parameter, self.editor_cpp)
        self.assertIn('GetPerInstanceRandom(Parameters)', self.editor_cpp)
        self.assertIn('WorldPosition.xy', self.editor_cpp)
        self.assertIn('Wind->Inputs.Reset();', self.editor_cpp)
        self.assertIn('Colour->Inputs.Reset();', self.editor_cpp)
        self.assertIn('float RecoveryDampingRatio = 0.32f;', self.runtime_h)
        self.assertIn('const FVector2D DirectionAcceleration', self.runtime_cpp)
        self.assertIn('WindStrengthVelocityCmPerSecond', self.runtime_cpp)
        self.assertIn('TargetStrengthCm - CurrentWindStrengthCm', self.runtime_cpp)
        self.assertIn('sin(phase * 7.7', self.editor_cpp)
        for material in (
            'M_IPVExploreV2_BroadleafTrunk_Wind',
            'M_IPVExploreV2_BroadleafBranches_Wind',
            'M_IPVExploreV2_BroadleafLeaves_Wind',
            'M_IPVExploreV2_Turf_Wind',
        ):
            self.assertIn(material, self.editor_cpp)
        self.assertIn('Material->IsCompilingOrHadCompileError', self.editor_cpp)
        self.assertTrue(self.contract['windContract']['recoveryAftereffectRequired'])

    def test_runtime_materials_are_transient_mids_and_finite(self) -> None:
        self.assertIn('UPROPERTY(Transient)', self.runtime_h)
        self.assertIn('TArray<TObjectPtr<UMaterialInstanceDynamic>>', self.runtime_h)
        self.assertIn('CreateDynamicMaterialInstance', self.runtime_cpp)
        self.assertIn('WindMaterialInstances.Num() != 11', self.runtime_cpp)
        self.assertIn('IsWindRuntimeActive() const', self.runtime_h)
        self.assertIn('Landscape->IsWindRuntimeActive()', self.editor_cpp)

    def test_play_view_capture_is_validated_and_non_overwriting(self) -> None:
        self.assertIn('CaptureIstanaExploreV2PlayView', self.editor_h)
        self.assertIn('ValidateIstanaExploreV2PlayWorld(Validation)', self.editor_cpp)
        self.assertIn('FScreenshotRequest::IsScreenshotRequested()', self.editor_cpp)
        self.assertIn('IFileManager::Get().FileSize(*Destination) >= 0', self.editor_cpp)
        self.assertIn('TRIAD/IstanaPreviews/ExploreV2', self.editor_cpp)

    def test_hism_population_defers_cluster_rebuilds(self) -> None:
        self.assertIn(
            'Component->bAutoRebuildTreeOnInstanceChanges = false;',
            self.runtime_cpp,
        )
        self.assertEqual(
            self.runtime_cpp.count('Component->bAutoRebuildTreeOnInstanceChanges = false;'),
            1,
        )
        self.assertIn(
            'Component->BuildTreeIfOutdated(false, true)',
            self.runtime_cpp,
        )
        self.assertIn('FStaticMeshCompilingManager::Get().FinishCompilation', self.editor_cpp)

    def test_high_detail_tree_lod_zero_is_fail_closed(self) -> None:
        self.assertIn('bOverrideMinLOD = true', self.runtime_cpp)
        self.assertIn('MinLOD = 1', self.runtime_cpp)

    def test_visible_plants_are_noncolliding_with_pawn_only_trunks(self) -> None:
        self.assertIn('SetCollisionEnabled(ECollisionEnabled::NoCollision)', self.runtime_cpp)
        self.assertIn('SetCollisionEnabled(ECollisionEnabled::QueryOnly)', self.runtime_cpp)
        self.assertIn('SetCollisionResponseToChannel(ECC_Pawn, ECR_Block)', self.runtime_cpp)
        self.assertIn('SetHiddenInGame(true, true)', self.runtime_cpp)
        self.assertIn('bVegetationUsedForSensorTruth = false', self.runtime_h)

    def test_truth_boundary_rejects_one_to_one_and_botanical_claims(self) -> None:
        self.assertIn('bOneToOneKilometerReplicaClaimed = false', self.runtime_h)
        self.assertIn('bExactSpeciesOrCultivarsClaimed = false', self.runtime_h)
        self.assertIn('bExactIndividualTreePlacementClaimed = false', self.runtime_h)
        self.assertFalse(self.contract['claimBoundary']['oneToOneOneKilometerClaimed'])
        self.assertFalse(self.contract['claimBoundary']['hiddenEstateSurveyClaimed'])
        self.assertTrue(self.contract['claimBoundary']['referenceAlignedVisualReconstruction'])

    def test_editor_entrypoints_and_pie_free_roam_gate_are_exposed(self) -> None:
        for function in (
            'CreateIstanaExploreV2WindMaterials',
            'BuildIstanaExploreV2Map',
            'ValidateIstanaExploreV2Map',
            'ValidateIstanaExploreV2PlayWorld',
            'QuiesceIstanaExploreV2PlayWorldForStop',
        ):
            self.assertIn(function, self.editor_h)
            self.assertIn(function, self.editor_cpp)
        self.assertIn('ATRIADIstanaFreeRoamPawn', self.editor_cpp)
        self.assertIn('Player->GetViewTarget() != Pawn', self.editor_cpp)


if __name__ == '__main__':
    unittest.main()
