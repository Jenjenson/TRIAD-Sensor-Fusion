<#
.SYNOPSIS
Runs the guarded R24 repository-to-native sync, UE 5.5 build, migration, and tests.

.DESCRIPTION
DryRun is the default and performs no writes. Apply copies only the reviewed
allowlist, Build invokes the proven module-scoped UE 5.5 command, Migrate runs
the ordered MacDonald House then Temasek Shophouse import/map transactions in
separate rendering-capable cold editor processes, and Test runs one hidden cold
process per allowlisted filter.

.EXAMPLE
.\Invoke-R24NativeTransaction.ps1 -Phase DryRun

.EXAMPLE
.\Invoke-R24NativeTransaction.ps1 -Phase Apply,Build,Migrate,Test

.EXAMPLE
.\Invoke-R24NativeTransaction.ps1 -Phase All
#>
[CmdletBinding()]
param(
    [ValidateSet('DryRun', 'Apply', 'Build', 'Migrate', 'Test', 'All')]
    [string[]] $Phase = @('DryRun'),

    [ValidateRange(60, 1800)]
    [int] $AutomationTimeoutSeconds = 900,

    [string[]] $TestFilter = @(
        'TRIAD.SensorFusion.Istana.GeodesicCircle',
        'TRIAD.RF.IndexedGeometryQuery.Contract',
        'TRIAD.SensorFusion.RF.GeometryInteractionModel',
        'TRIAD.Istana.ExploreV5D.MacDonaldHouse',
        'TRIAD.Istana.ExploreV5D.TemasekShophouse'
    )
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# This transaction is intentionally host-specific. Changing any root requires a
# reviewed script change; command-line root overrides are deliberately absent.
$repositoryUnrealRoot = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal')
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath('D:\triad\TRIAD\TRIAD.uproject')
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath(
    (Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'))
$editorCommand = [IO.Path]::GetFullPath(
    (Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'))
$buildVersionFile = [IO.Path]::GetFullPath(
    (Join-Path $engineRoot 'Engine\Build\Build.version'))

$protectedExpectedState = 'ABSENT'
$protectedProcessId = [uint32] 0
$protectedCreationUtcTicks = [int64] 0
$protectedExecutable = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')

$tightGeometryPath = [IO.Path]::GetFullPath(
    (Join-Path $repositoryUnrealRoot `
        'SourceAssets\IstanaPublicViewRF\TightV1\Generated\IstanaPublicViewRFTightV1.geometry.json'))
$tightGeometryBytes = [int64] 2091703
$tightGeometrySha256 =
    'A3705E22B47FBE4FD38936294890E1AD620BB3C618AC3C93CA6C4719C6EDC62A'
$tightCatalogPath = [IO.Path]::GetFullPath(
    (Join-Path $repositoryUnrealRoot `
        'SourceAssets\IstanaPublicViewRF\TightV1\istana_rf_materials_tight_v1.catalog.json'))
$tightCatalogBytes = [int64] 12554
$tightCatalogSha256 =
    '0089FED936494ECD6938DC14D34EA8D5540E0896C2DC814414E071B6D1081A63'
$oneKilometreV2GeometryPath = [IO.Path]::GetFullPath(
    (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Resources\RF\IstanaPublicViewRFOneKilometreV2.geometry.json'))
$oneKilometreV2GeometryBytes = [int64] 12506346
$oneKilometreV2GeometrySha256 =
    '85E654FBA602B2DBC51EB64C6B66FF234C8EA1F948152612C755EDC22C65DA51'
$oneKilometreV2CatalogPath = [IO.Path]::GetFullPath(
    (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_materials_one_kilometre_v2.catalog.json'))
$oneKilometreV2CatalogBytes = [int64] 28674
$oneKilometreV2CatalogSha256 =
    '210CB26DDB9B531ADEB3C917606A736AEFC857EB6696DA485A2E63DBB8B31662'
$oneKilometreV2ContractPath = [IO.Path]::GetFullPath(
    (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_scene_one_kilometre_v2.contract.json'))
$oneKilometreV2ContractBytes = [int64] 14783
$oneKilometreV2ContractSha256 =
    'FE509917AE59BE0918BCD799F23DC981E00A394C6C328A7342F56B371C40BCC2'
$expectedSourceAssetsTarget = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD\Saved\TRIAD\CDriveRelief\20260830_workspace\TRIAD-Sensor-Fusion-Repo_unreal_SourceAssets')
$migratableV5DMap = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot `
        'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$macDonaldNativeAssetRoot = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse'))
$macDonaldExpectedAssetRelativeFiles = @(
    'SM_IPV5D_R24_MacDonaldHouse_Render.uasset',
    'Materials\M_MD_BalconyConcrete_PBR_R24.uasset',
    'Materials\M_MD_DarkMetal_PBR_R24.uasset',
    'Materials\M_MD_DarkWindowGlass_PBR_R24.uasset',
    'Materials\M_MD_GreenGlazedRoofTile_PBR_R24.uasset',
    'Materials\M_MD_HeritagePlaque_PBR_R24.uasset',
    'Materials\M_MD_MarbleColumn_PBR_R24.uasset',
    'Materials\M_MD_RedSandFacedBrick_PBR_R24.uasset',
    'Materials\M_MD_ShadowRecess_PBR_R24.uasset',
    'Materials\M_MD_WhitePaintedFrame_PBR_R24.uasset'
)
$macDonaldMigrationFilter =
    'TRIAD.Istana.ExploreV5D.MacDonaldHouse.EndToEndContract'
$temasekNativeAssetRoot = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse'))
$temasekExpectedAssetRelativeFiles = @(
    'SM_IPV5D_R24_TemasekShophouse_Render.uasset',
    'Materials\M_TSH_Bark_PBR_R24.uasset',
    'Materials\M_TSH_Brass_PBR_R24.uasset',
    'Materials\M_TSH_CharcoalTrim_PBR_R24.uasset',
    'Materials\M_TSH_DarkWindowGlass_PBR_R24.uasset',
    'Materials\M_TSH_LeafDeep_PBR_R24.uasset',
    'Materials\M_TSH_LeafLight_PBR_R24.uasset',
    'Materials\M_TSH_PaverDark_PBR_R24.uasset',
    'Materials\M_TSH_PaverLight_PBR_R24.uasset',
    'Materials\M_TSH_PinkGreyMosaic_PBR_R24.uasset',
    'Materials\M_TSH_PollinatorBloom_PBR_R24.uasset',
    'Materials\M_TSH_PrecastConcrete_PBR_R24.uasset',
    'Materials\M_TSH_Rainwater_PBR_R24.uasset',
    'Materials\M_TSH_ReusedTimber_PBR_R24.uasset',
    'Materials\M_TSH_ShadowRecess_PBR_R24.uasset',
    'Materials\M_TSH_SoilMulch_PBR_R24.uasset',
    'Materials\M_TSH_SolarPanel_PBR_R24.uasset',
    'Materials\M_TSH_Terracotta_PBR_R24.uasset',
    'Materials\M_TSH_Timber_PBR_R24.uasset',
    'Materials\M_TSH_WhiteShanghaiPlaster_PBR_R24.uasset'
)
$temasekMigrationFilter =
    'TRIAD.Istana.ExploreV5D.TemasekShophouse.EndToEndContract'
$buildProductPaths = @(
    (Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'),
    (Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll')
)

# Only these files may be copied into the native project. Documentation and
# Python-only tests are intentionally excluded from native synchronization.
$rfAndScenarioSyncAllowlist = @(
    [pscustomobject] [ordered] @{
        Area = 'RF_GEODESY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADGeodesy.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_GEODESY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADGeodesy.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_GEODESY_TEST'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADGeodesyTests.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_POLYHEDRON'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADRFIndexedGeometryQuery.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_POLYHEDRON_AND_TRACE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADRFIndexedGeometryQuery.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_POLYHEDRON_TEST'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADRFIndexedGeometryQueryTests.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_INTERACTION_TRACE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADRFInteractionModel.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_INTERACTION_TRACE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADRFInteractionModel.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_INTERACTION_TRACE_TEST'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADRFInteractionModelTests.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_SCENARIO_BINDING'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADSensorFusionScenarioManager.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_SCENARIO_BINDING'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADSensorFusionScenarioManager.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_SCENARIO_BINDING'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADSensorFusionTypes.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_RESOURCE_STAGING'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\TRIADSensorFusion.Build.cs'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_SCENARIO_BINDING'
        RelativePath = 'Plugins\TRIADSensorFusion\Resources\IstanaHighFidelityRF.example.json'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_SCENE_CONTRACT'
        RelativePath = 'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_scene_tight_v1.contract.json'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_ONE_KILOMETRE_V2_CONFIG'
        RelativePath = 'Plugins\TRIADSensorFusion\Resources\IstanaOneKilometreV2RF.example.json'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_ONE_KILOMETRE_V2_GEOMETRY'
        RelativePath = 'Plugins\TRIADSensorFusion\Resources\RF\IstanaPublicViewRFOneKilometreV2.geometry.json'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_ONE_KILOMETRE_V2_CATALOG'
        RelativePath = 'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_materials_one_kilometre_v2.catalog.json'
    },
    [pscustomobject] [ordered] @{
        Area = 'RF_ONE_KILOMETRE_V2_CONTRACT'
        RelativePath = 'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_scene_one_kilometre_v2.contract.json'
    }
)

$macDonaldSyncAllowlist = @(
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DMacDonaldHouseActor.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DMacDonaldHouseActor.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_PROVENANCE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DMacDonaldHouseProvenance.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_PROVENANCE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DMacDonaldHouseProvenance.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_RUNTIME_TEST'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DMacDonaldHouseRuntimeTests.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_EDITOR_LIBRARY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_EDITOR_LIBRARY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_HYBRID_MAP_INTEGRATION'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_HYBRID_MAP_INTEGRATION'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\SM_IPV5D_R24_MacDonaldHouse_Render.obj'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\SM_IPV5D_R24_MacDonaldHouse_Render.mtl'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\IstanaPublicViewV5DR24MacDonaldHouse.geometry.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\IstanaPublicViewV5DR24MacDonaldHouse.features.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\IstanaPublicViewV5DR24MacDonaldHouse.manifest.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\macdonald_house_r24.contract.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Sources\public_sources.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'MACDONALD_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\macdonald_house_r24.unreal_placement.json'
        SourceRootKind = 'SOURCE_ASSETS'
    }
)
$temasekSyncAllowlist = @(
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseActor.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseActor.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_PROVENANCE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseProvenance.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_PROVENANCE'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_RUNTIME_TEST'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_EDITOR_LIBRARY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_EDITOR_LIBRARY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\SM_IPV5D_R24_TemasekShophouse_Render.obj'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\SM_IPV5D_R24_TemasekShophouse_Render.mtl'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.geometry.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.features.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.materials.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.manifest.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\temasek_shophouse_r24.contract.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Sources\public_sources.json'
        SourceRootKind = 'SOURCE_ASSETS'
    },
    [pscustomobject] [ordered] @{
        Area = 'TEMASEK_SOURCE_INPUT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\temasek_shophouse_r24.unreal_placement.json'
        SourceRootKind = 'SOURCE_ASSETS'
    }
)
$syncAllowlist = @($rfAndScenarioSyncAllowlist) +
    @($macDonaldSyncAllowlist) +
    @($temasekSyncAllowlist)

# This receipt is deliberately independent from each invocation's fresh
# snapshots. Any drift from the reviewed handoff aborts before a native write.
$reviewedSyncPins = [ordered] @{
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADGeodesy.h' = @{ Bytes = 1559; Sha256 = 'D5B97F36CB4137E5ECC040D8BA474663BBB74D2F978CB04FC4C42B4A7747A870' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADGeodesy.cpp' = @{ Bytes = 15556; Sha256 = 'C77F224CE6AEF250F4B2587665DAEA878370AC3C7254AD86057B83660518352A' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADGeodesyTests.cpp' = @{ Bytes = 5797; Sha256 = '7774370C2220F2A92D141046BBB7C37C4ED8AC87D74726090138C03DCC36B76C' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADRFIndexedGeometryQuery.h' = @{ Bytes = 11003; Sha256 = '21F93D53084396E36BDC3967D6C3FE383092155352AEE9AC30D6819FB847E367' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADRFIndexedGeometryQuery.cpp' = @{ Bytes = 166878; Sha256 = 'A56719963B602795F5B8A4C2715C51BA39650D9F124686C71FDE418BCF4D3C1D' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADRFIndexedGeometryQueryTests.cpp' = @{ Bytes = 69037; Sha256 = 'A17DF12D9B4B365E39290D80B4461F79B03315181E978E10184258D487805003' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADRFInteractionModel.h' = @{ Bytes = 10303; Sha256 = '4DCABE5D6A38D184990CCF462E96694DD5F9EC79211594DFA086D7D626B204F1' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADRFInteractionModel.cpp' = @{ Bytes = 39642; Sha256 = 'DDE6D1DD80728367B794EAB643038719A4664434A9A0A784595E7ACB0DE00AB2' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADRFInteractionModelTests.cpp' = @{ Bytes = 28877; Sha256 = '2F9631C0EE5E3E9090F2E4E82EB85EC8E2B788527EC3F93D34F6EEB96B6E7D87' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADSensorFusionScenarioManager.h' = @{ Bytes = 8620; Sha256 = 'CB402C39430C5087CDB2AD81F978DB4C45C9AB0106DC9D2EE5F98ADDF6E8CB4B' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADSensorFusionScenarioManager.cpp' = @{ Bytes = 186224; Sha256 = 'B70150E26F4F84475A47F3FD60CA6E59E64411A93BB27A78E39735AE1E347A64' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADSensorFusionTypes.h' = @{ Bytes = 37179; Sha256 = '9C60F1D560A6230AC8BD83E6D6202C5E137D5FF731DF2EDF232A40E4F2599ED1' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\TRIADSensorFusion.Build.cs' = @{ Bytes = 2736; Sha256 = 'E066028EC940568E2927ADA63F684ABA5866786B99B6F494D02713DC0969DEB2' }
    'Plugins\TRIADSensorFusion\Resources\IstanaHighFidelityRF.example.json' = @{ Bytes = 4130; Sha256 = '3D3A5F86C22D65D3A9328E9F5B5083D35332E403376CB8165D19D4CE7B4C9C2F' }
    'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_scene_tight_v1.contract.json' = @{ Bytes = 13162; Sha256 = '19002F898C34C23793038C42892F122642F112BD514A0EBB05E4969DCB9AE591' }
    'Plugins\TRIADSensorFusion\Resources\IstanaOneKilometreV2RF.example.json' = @{ Bytes = 3091; Sha256 = '45D33E674088DDCD9075AE84B99B02B14D6BB361B10068397F9054A301DEBE31' }
    'Plugins\TRIADSensorFusion\Resources\RF\IstanaPublicViewRFOneKilometreV2.geometry.json' = @{ Bytes = 12506346; Sha256 = '85E654FBA602B2DBC51EB64C6B66FF234C8EA1F948152612C755EDC22C65DA51' }
    'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_materials_one_kilometre_v2.catalog.json' = @{ Bytes = 28674; Sha256 = '210CB26DDB9B531ADEB3C917606A736AEFC857EB6696DA485A2E63DBB8B31662' }
    'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_scene_one_kilometre_v2.contract.json' = @{ Bytes = 14783; Sha256 = 'FE509917AE59BE0918BCD799F23DC981E00A394C6C328A7342F56B371C40BCC2' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DMacDonaldHouseActor.h' = @{ Bytes = 3701; Sha256 = '06CE7A0C3F29C79817C44DDB9EFD9AFA51FEEE9F26CDAB4F0073F3A5537E1500' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DMacDonaldHouseActor.cpp' = @{ Bytes = 21885; Sha256 = '5CA2DD206CBB12749A740DDDEC6A5CC5AE59D5B4FD43D4E42CDA7F209DF0B5DE' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DMacDonaldHouseProvenance.h' = @{ Bytes = 4267; Sha256 = 'EEB569D3A9C32DF521F022DBAEDA635152A7C3927BDE5A530FC268D9FEE8A143' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DMacDonaldHouseProvenance.cpp' = @{ Bytes = 13282; Sha256 = '2C319D1FEEF70973F3B0B5529889195C5466808C46AE3CEA4E98722EBA83E772' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DMacDonaldHouseRuntimeTests.cpp' = @{ Bytes = 8817; Sha256 = 'A06AF71381236DF41460676BAC7E7854B164934D6DAB961C00C4BE75A64FEF7A' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.h' = @{ Bytes = 1133; Sha256 = '7686E750E285D85692F851C13DA2FF47E49D58D42D3C6B3556F3A7AB894764A5' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.cpp' = @{ Bytes = 61875; Sha256 = '232C77FBA57580484DF52EA82BA7DB9A74F5C32857278085EC7B2B0B7D3900A0' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.h' = @{ Bytes = 1042; Sha256 = '4AD17FE7D2D01F85EB7B136DE7F0FA37D97FE59897B885996488C1B37B0C7A07' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp' = @{ Bytes = 9128; Sha256 = '2576619861D0D2F90391F34F23C314550BF2F87B37163D52B3323834D8E21F01' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h' = @{ Bytes = 7000; Sha256 = 'B36240169864F54895692AFEBEEE33CBB1B768EDF8960A5234A5FBA2DEF6C90B' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp' = @{ Bytes = 267685; Sha256 = '514C1F9320A50B233116F4D6758752470A803072B76176605293ED5E984F13D4' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\SM_IPV5D_R24_MacDonaldHouse_Render.obj' = @{ Bytes = 293759; Sha256 = 'BE049C7E1A8AB2D9DED98C027478EA5A5183F0C2CF2FFC206289D1FF47F955DE' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\SM_IPV5D_R24_MacDonaldHouse_Render.mtl' = @{ Bytes = 1487; Sha256 = 'E35EE31D1B2ABB047FE6CA134510D32EDFC1E13FC98A0C338EACB5AEB7DE1784' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\IstanaPublicViewV5DR24MacDonaldHouse.geometry.json' = @{ Bytes = 1078589; Sha256 = '9F629115A98BE5A257D06ACD864CE40170C8EFCE57BF60D7D145B85D7682C69C' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\IstanaPublicViewV5DR24MacDonaldHouse.features.json' = @{ Bytes = 3800; Sha256 = '26EB68961BB66BFCC25B39AF86FFFFE5DCE9491F14AF5454316D88D5A7D24C78' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Generated\IstanaPublicViewV5DR24MacDonaldHouse.manifest.json' = @{ Bytes = 4656; Sha256 = 'E1949EFEBD51A18D9919BB4174A0C95038D7028ADD13D7BE46070E46C773F8C2' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\macdonald_house_r24.contract.json' = @{ Bytes = 6608; Sha256 = '30FDEDD2E7EBBB4E4E4133D0E2D383399FA5F17AA264B73AD422094F2D00DC77' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\Sources\public_sources.json' = @{ Bytes = 2728; Sha256 = '5292CAA6133BC0B2F69CCB52377DA62D778D4405F3213D585E6908184D24A570' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\macdonald_house_r24.unreal_placement.json' = @{ Bytes = 4192; Sha256 = 'E5B40D02A3D712303A2D99734A42E9ADD68F4E528DED31DD763F37034961ED81' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseActor.h' = @{ Bytes = 4064; Sha256 = 'D72DD0458B10D3EE5038B2E7623D405BCECB55B944B0BCCA271DDB25140F89FC' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseActor.cpp' = @{ Bytes = 22824; Sha256 = '89EA99CC0888C006EBC495D4302009F1B381958BE4074E8DA89580D289FF4EA1' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseProvenance.h' = @{ Bytes = 4316; Sha256 = '1126B52219E0AC5320ED4F40863EFE147C0616F3520DD05349928D2166120EB8' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp' = @{ Bytes = 13306; Sha256 = '41F1FB40C0EF75246DDE770015E5210E18D37DCCEBCC4C3DDA4F2E4A9BE4B368' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp' = @{ Bytes = 9076; Sha256 = '7C1556C72F608624CC0657339CDE053D217215B8047C5A2A55D558B46E0B9044' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h' = @{ Bytes = 496; Sha256 = '1CCF3D7F68B74325ABFBCBACCEA9ECD099EC8FA71D9A5A53C499DEE0157A4FB0' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp' = @{ Bytes = 68078; Sha256 = '020599213B1177819B0926F2E2A5BF1AE631DA62D6F71506A314636B13CF5706' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h' = @{ Bytes = 1058; Sha256 = '3225071442862102AB6B7005ACCA61B0E385B47D9834819584554C9C8F8CC95A' }
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp' = @{ Bytes = 8682; Sha256 = '4DED8BD31A1A0CC1EBCCABA3BBF71D82E6F06A66C42A6B0B1FB804BEFCDCC481' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\SM_IPV5D_R24_TemasekShophouse_Render.obj' = @{ Bytes = 980675; Sha256 = '81720789E5D96B32E00FF5744B1407730B2548C5CDF3818BE7998063E5E5676B' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\SM_IPV5D_R24_TemasekShophouse_Render.mtl' = @{ Bytes = 2938; Sha256 = '4647F2B9BEA229C016B5FFEA0136C40D8CB5EBC286BA095DC97E8C90E020C2FC' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.geometry.json' = @{ Bytes = 3276163; Sha256 = '433645DF13FCA45C0BC5392DC1D7EB3A72F0F8395FE431540696194942CEE9DC' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.features.json' = @{ Bytes = 4720; Sha256 = 'C09D37505ED46073E91A799D59D845CA857E2164FB1E26F76A84C7614F4300C5' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.materials.json' = @{ Bytes = 8265; Sha256 = 'AE7600271DC222AE1E727E23241232C1179E8337B74541892EC177489110DE6A' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\IstanaPublicViewV5DR24TemasekShophouse.manifest.json' = @{ Bytes = 7095; Sha256 = '495BBDFF95FB6507E816301E514577FF27E697CCA650729F451D9F3BE68C4ECA' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\temasek_shophouse_r24.contract.json' = @{ Bytes = 9474; Sha256 = '6EC251EF53D28B8D5D97CA9F93A380FBB1916E9F2AFA974EAC383CEBA71F345F' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Sources\public_sources.json' = @{ Bytes = 5870; Sha256 = 'E4D1B0F3648B87E577A004F158821EBD716E0288511C51A468E272D475DE009E' }
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Generated\temasek_shophouse_r24.unreal_placement.json' = @{ Bytes = 4517; Sha256 = '61C9CFB4D055401B5AE44CBD19A382DD1DCB2516F0766A0E6B928A706EA72987' }
}

$allowedTestFilters = @(
    'TRIAD.SensorFusion.Istana.GeodesicCircle',
    'TRIAD.RF.IndexedGeometryQuery.Contract',
    'TRIAD.SensorFusion.RF.GeometryInteractionModel',
    'TRIAD.Istana.ExploreV5D.MacDonaldHouse',
    'TRIAD.Istana.ExploreV5D.TemasekShophouse'
)

$immutableNativeFiles = @(
    $nativeProjectFile,
    (Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v4.umap'),
    (Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5.umap'),
    (Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5b.umap'),
    $migratableV5DMap
)

function Test-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    return $fullPath.StartsWith(
        $fullRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

function Assert-ExactDirectory {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Expected,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $fullExpected = [IO.Path]::GetFullPath($Expected).TrimEnd('\', '/')
    if ($fullPath -cne $fullExpected) {
        throw "$Label root changed: expected=$fullExpected actual=$fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath -ErrorAction Stop
    if (-not $item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label must be an existing, non-reparse directory: $fullPath"
    }
}

function Assert-NoReparseAncestor {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if (-not (Test-ContainedPath -Path $Path -Root $Root)) {
        throw "$Label escaped its reviewed root: $Path"
    }
    $relative = [IO.Path]::GetFullPath($Path).Substring(
        [IO.Path]::GetFullPath($Root).TrimEnd('\', '/').Length).TrimStart('\', '/')
    $segments = @($relative -split '[\\/]')
    $cursor = [IO.Path]::GetFullPath($Root)
    for ($index = 0; $index -lt ($segments.Count - 1); ++$index) {
        $cursor = Join-Path $cursor $segments[$index]
        if (-not (Test-Path -LiteralPath $cursor)) {
            break
        }
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
        if (-not $item.PSIsContainer -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label traverses a non-directory or reparse ancestor: $cursor"
        }
    }
}

function Get-FileSnapshot {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [switch] $AllowMissing
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath)) {
        if (-not $AllowMissing) {
            throw "Required file is missing: $fullPath"
        }
        return [pscustomobject] [ordered] @{
            Path = $fullPath
            Exists = $false
            Bytes = $null
            Sha256 = $null
            LastWriteUtc = $null
        }
    }
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if ($item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Expected a regular, non-reparse file: $fullPath"
    }
    return [pscustomobject] [ordered] @{
        Path = $fullPath
        Exists = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
        LastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

function Assert-SameSnapshot {
    param(
        [Parameter(Mandatory = $true)] [object] $Expected,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-FileSnapshot -Path $Path -AllowMissing
    if ($actual.Exists -ne $Expected.Exists -or
        ($actual.Exists -and
         ($actual.Bytes -ne $Expected.Bytes -or
          $actual.Sha256 -cne $Expected.Sha256))) {
        throw "$Label identity drifted: path=$Path expected=$($Expected.Bytes):$($Expected.Sha256) actual=$($actual.Bytes):$($actual.Sha256)"
    }
    return $actual
}

function Assert-PinnedFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $snapshot = Get-FileSnapshot -Path $Path
    if ($snapshot.Bytes -ne $Bytes -or $snapshot.Sha256 -cne $Sha256) {
        throw "$Label pin failed: path=$Path expected=$Bytes`:$Sha256 actual=$($snapshot.Bytes)`:$($snapshot.Sha256)"
    }
    return $snapshot
}

function Get-ProtectedUE54Identity {
    if ($protectedExpectedState -ceq 'ABSENT') {
        $protectedEngineRoot = [IO.Path]::GetFullPath(
            'C:\Program Files\Epic Games\UE_5.4').TrimEnd('\', '/') +
            [IO.Path]::DirectorySeparatorChar
        $normalisedProject = $protectedProject.Replace('/', '\')
        $unexpected = @(Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object {
                if ($_.Name -notin @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe')) {
                    return $false
                }
                $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                        [string] $_.ExecutablePath)) {
                    ''
                }
                else {
                    [IO.Path]::GetFullPath([string] $_.ExecutablePath)
                }
                $normalisedCommandLine = ([string] $_.CommandLine).Replace('/', '\')
                $actualExecutable.StartsWith(
                    $protectedEngineRoot,
                    [StringComparison]::OrdinalIgnoreCase) -or
                    $normalisedCommandLine.IndexOf(
                        $normalisedProject,
                        [StringComparison]::OrdinalIgnoreCase) -ge 0
            })
        if ($unexpected.Count -ne 0) {
            $unexpectedIds = @($unexpected | ForEach-Object { $_.ProcessId }) -join ','
            throw "UE5.4/Capstone was expected absent but appeared (PIDs=$unexpectedIds). No R24 phase may continue."
        }
        return [pscustomobject] [ordered] @{
            State = 'ABSENT'
            ProcessId = [uint32] 0
            CreationUtcTicks = [int64] 0
            ExecutablePath = $protectedExecutable
            ProjectPath = $protectedProject
            CommandLine = ''
        }
    }

    $process = Get-CimInstance Win32_Process `
        -Filter "ProcessId = $protectedProcessId"
    if ($null -eq $process) {
        throw "Protected UE5.4 PID $protectedProcessId is absent. No R24 phase may continue."
    }
    $creationTicks = ([DateTimeOffset] $process.CreationDate).UtcTicks
    $actualExecutable = if ([string]::IsNullOrWhiteSpace(
        [string] $process.ExecutablePath)) {
        ''
    }
    else {
        [IO.Path]::GetFullPath([string] $process.ExecutablePath)
    }
    $projectPattern = '(?i)(?:^|\s)"?' +
        [regex]::Escape($protectedProject) + '"?(?:\s|$)'
    if ($process.Name -cne 'UnrealEditor.exe' -or
        $actualExecutable -cne $protectedExecutable -or
        [int64] $creationTicks -ne $protectedCreationUtcTicks -or
        [string]::IsNullOrWhiteSpace([string] $process.CommandLine) -or
        -not [regex]::IsMatch([string] $process.CommandLine, $projectPattern)) {
        throw 'Protected UE5.4 PID/executable/start/project identity changed. No R24 phase may continue.'
    }
    return [pscustomobject] [ordered] @{
        State = 'PRESENT'
        ProcessId = [uint32] $process.ProcessId
        CreationUtcTicks = [int64] $creationTicks
        ExecutablePath = $actualExecutable
        ProjectPath = $protectedProject
        CommandLine = [string] $process.CommandLine
    }
}

function Assert-ProtectedUE54Unchanged {
    param([Parameter(Mandatory = $true)] [object] $Before)

    $after = Get-ProtectedUE54Identity
    if ($after.State -cne $Before.State -or
        $after.ProcessId -ne $Before.ProcessId -or
        $after.CreationUtcTicks -ne $Before.CreationUtcTicks -or
        $after.ExecutablePath -cne $Before.ExecutablePath -or
        $after.ProjectPath -cne $Before.ProjectPath -or
        $after.CommandLine -cne $Before.CommandLine) {
        throw 'Protected UE5.4 identity changed during the R24 transaction.'
    }
    return $after
}

function Get-UE55EditorProcesses {
    $expectedPrefix = $engineRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar
    return @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -and
        -not [string]::IsNullOrWhiteSpace([string] $_.ExecutablePath) -and
        [IO.Path]::GetFullPath([string] $_.ExecutablePath).StartsWith(
            $expectedPrefix, [StringComparison]::OrdinalIgnoreCase)
    })
}

function Assert-NativeProjectIdle {
    $editors = @(Get-UE55EditorProcesses)
    $remoteControlListeners = @(Get-NetTCPConnection -State Listen `
        -LocalPort 30010 -ErrorAction SilentlyContinue)
    if ($editors.Count -ne 0 -or $remoteControlListeners.Count -ne 0) {
        $editorIds = @($editors | ForEach-Object { $_.ProcessId }) -join ','
        throw "R24 native work requires zero UE5.5 editors and zero Remote Control port 30010 listeners. ue55Pids=$editorIds rcListeners=$($remoteControlListeners.Count)"
    }
}

function Write-JsonFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [object] $Value
    )

    $parent = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Path))
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        [void] (New-Item -ItemType Directory -Path $parent -Force)
    }
    $json = $Value | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText(
        [IO.Path]::GetFullPath($Path),
        $json + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
}

function Get-SyncRows {
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($entry in $syncAllowlist) {
        $relative = [string] $entry.RelativePath
        if ([string]::IsNullOrWhiteSpace($relative) -or
            [IO.Path]::IsPathRooted($relative) -or
            (@($relative -split '[\\/]') -contains '..') -or
            -not $seen.Add($relative)) {
            throw "Invalid or duplicate R24 sync allowlist entry: $relative"
        }
        $sourceRootKind = if ($null -ne
            $entry.PSObject.Properties['SourceRootKind']) {
            [string] $entry.SourceRootKind
        }
        else {
            'UNREAL_ROOT'
        }
        if ($sourceRootKind -ceq 'SOURCE_ASSETS') {
            $sourceAssetsPrefix = 'SourceAssets\'
            if (-not $relative.StartsWith(
                $sourceAssetsPrefix,
                [StringComparison]::OrdinalIgnoreCase)) {
                throw "SOURCE_ASSETS entry lacks its required prefix: $relative"
            }
            $sourceRelative = $relative.Substring($sourceAssetsPrefix.Length)
            $source = [IO.Path]::GetFullPath(
                (Join-Path $expectedSourceAssetsTarget $sourceRelative))
            Assert-NoReparseAncestor -Path $source `
                -Root $expectedSourceAssetsTarget `
                -Label 'R24 physical repository SourceAssets input'
        }
        elseif ($sourceRootKind -ceq 'UNREAL_ROOT') {
            $source = [IO.Path]::GetFullPath(
                (Join-Path $repositoryUnrealRoot $relative))
            Assert-NoReparseAncestor -Path $source `
                -Root $repositoryUnrealRoot -Label 'R24 repository source'
        }
        else {
            throw "Unknown R24 source-root kind: $sourceRootKind"
        }
        $destination = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $relative))
        Assert-NoReparseAncestor -Path $destination -Root $nativeProjectRoot `
            -Label 'R24 native destination'
        $sourceSnapshot = Get-FileSnapshot -Path $source
        $reviewedPin = $reviewedSyncPins[$relative]
        if ($null -eq $reviewedPin -or
            [int64] $reviewedPin.Bytes -ne $sourceSnapshot.Bytes -or
            [string] $reviewedPin.Sha256 -cne $sourceSnapshot.Sha256) {
            throw "R24 source does not match its frozen reviewed byte/hash receipt: $relative"
        }
        $destinationSnapshot = Get-FileSnapshot -Path $destination -AllowMissing
        $rows.Add([pscustomobject] [ordered] @{
            Area = [string] $entry.Area
            RelativePath = $relative
            SourceRootKind = $sourceRootKind
            Source = $source
            Destination = $destination
            SourceSnapshot = $sourceSnapshot
            DestinationBefore = $destinationSnapshot
            NeedsCopy = (-not $destinationSnapshot.Exists -or
                $destinationSnapshot.Bytes -ne $sourceSnapshot.Bytes -or
                $destinationSnapshot.Sha256 -cne $sourceSnapshot.Sha256)
        })
    }
    if ($seen.Count -ne $reviewedSyncPins.Count) {
        throw "R24 frozen pin roster and sync allowlist differ: allowlist=$($seen.Count) pins=$($reviewedSyncPins.Count)"
    }
    return @($rows)
}

function Assert-NativeSourcesMatch {
    param([Parameter(Mandatory = $true)] [object[]] $Rows)

    foreach ($row in $Rows) {
        [void] (Assert-SameSnapshot -Expected $row.SourceSnapshot `
            -Path $row.Source -Label 'reviewed R24 repository source')
        $native = Get-FileSnapshot -Path $row.Destination
        if ($native.Bytes -ne $row.SourceSnapshot.Bytes -or
            $native.Sha256 -cne $row.SourceSnapshot.Sha256) {
            throw "Native source is not synchronized for phase execution: $($row.RelativePath)"
        }
    }
}

function Get-ImmutableSnapshots {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in $immutableNativeFiles) {
        $rows.Add((Get-FileSnapshot -Path $path))
    }
    return @($rows)
}

function Assert-ImmutableSnapshots {
    param([Parameter(Mandatory = $true)] [object[]] $Snapshots)

    foreach ($snapshot in $Snapshots) {
        [void] (Assert-SameSnapshot -Expected $snapshot -Path $snapshot.Path `
            -Label 'protected native project/content input')
    }
}

function Get-MigrationImmutableSnapshots {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in $immutableNativeFiles) {
        if ([IO.Path]::GetFullPath($path) -cne $migratableV5DMap) {
            $rows.Add((Get-FileSnapshot -Path $path))
        }
    }
    return @($rows)
}

function Get-MacDonaldAssetInventory {
    param([switch] $RequireComplete)

    $expectedPrimaryPaths = @($macDonaldExpectedAssetRelativeFiles |
        ForEach-Object {
            [IO.Path]::GetFullPath((Join-Path $macDonaldNativeAssetRoot $_))
        })
    $expectedPackageStems = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $expectedPrimaryPaths) {
        if (-not (Test-ContainedPath -Path $path -Root $macDonaldNativeAssetRoot)) {
            throw "MacDonald asset path escaped its exact root: $path"
        }
        [void] $expectedPackageStems.Add(
            [IO.Path]::ChangeExtension($path, $null))
    }

    $rootExists = Test-Path -LiteralPath $macDonaldNativeAssetRoot
    if ($rootExists) {
        $rootItem = Get-Item -LiteralPath $macDonaldNativeAssetRoot -Force
        if (-not $rootItem.PSIsContainer -or
            ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "MacDonald native asset root is not a regular directory: $macDonaldNativeAssetRoot"
        }
    }

    $primary = [Collections.Generic.List[object]]::new()
    foreach ($path in $expectedPrimaryPaths) {
        Assert-NoReparseAncestor -Path $path -Root $nativeProjectRoot `
            -Label 'MacDonald generated asset'
        if ($RequireComplete) {
            $primary.Add((Get-FileSnapshot -Path $path))
        }
        else {
            $primary.Add((Get-FileSnapshot -Path $path -AllowMissing))
        }
    }

    $allowedExtensions = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($extension in @('.uasset', '.uexp', '.ubulk', '.uptnl')) {
        [void] $allowedExtensions.Add($extension)
    }
    $artifacts = [Collections.Generic.List[object]]::new()
    if ($rootExists) {
        foreach ($file in @(Get-ChildItem -LiteralPath $macDonaldNativeAssetRoot `
                -File -Recurse -Force | Sort-Object FullName)) {
            $full = [IO.Path]::GetFullPath($file.FullName)
            Assert-NoReparseAncestor -Path $full -Root $nativeProjectRoot `
                -Label 'MacDonald generated package artifact'
            $extension = [IO.Path]::GetExtension($full)
            $stem = [IO.Path]::ChangeExtension($full, $null)
            if (-not $allowedExtensions.Contains($extension) -or
                -not $expectedPackageStems.Contains($stem)) {
                throw "Unexpected file in the exact MacDonald generated asset root: $full"
            }
            $artifacts.Add((Get-FileSnapshot -Path $full))
        }
    }

    $complete = $rootExists -and
        @($primary | Where-Object { -not $_.Exists }).Count -eq 0
    if ($RequireComplete -and -not $complete) {
        throw 'The exact ten-package MacDonald generated asset roster is incomplete.'
    }
    return [pscustomobject] [ordered] @{
        AssetRoot = $macDonaldNativeAssetRoot
        RootExists = [bool] $rootExists
        Complete = [bool] $complete
        ExpectedPrimaryAssetCount = $expectedPrimaryPaths.Count
        PrimaryAssets = @($primary)
        PackageArtifacts = @($artifacts)
    }
}

function Assert-MacDonaldAssetInventorySame {
    param([Parameter(Mandatory = $true)] [object] $Expected)

    if (-not $Expected.Complete) {
        throw 'Cannot compare against an incomplete MacDonald asset inventory.'
    }
    $actual = Get-MacDonaldAssetInventory -RequireComplete
    $expectedPaths = @($Expected.PackageArtifacts | ForEach-Object { $_.Path } |
        Sort-Object)
    $actualPaths = @($actual.PackageArtifacts | ForEach-Object { $_.Path } |
        Sort-Object)
    if ($expectedPaths.Count -ne $actualPaths.Count -or
        ($expectedPaths -join "`n") -cne ($actualPaths -join "`n")) {
        throw 'The exact MacDonald generated package artifact roster changed.'
    }
    foreach ($snapshot in $Expected.PackageArtifacts) {
        [void] (Assert-SameSnapshot -Expected $snapshot -Path $snapshot.Path `
            -Label 'MacDonald generated asset during cold validation')
    }
    return $actual
}

function Get-TemasekAssetInventory {
    param([switch] $RequireComplete)

    $expectedPrimaryPaths = @($temasekExpectedAssetRelativeFiles |
        ForEach-Object { [IO.Path]::GetFullPath((Join-Path $temasekNativeAssetRoot $_)) })
    $expectedPackageStems = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $expectedPrimaryPaths) {
        if (-not (Test-ContainedPath -Path $path -Root $temasekNativeAssetRoot)) {
            throw "Temasek asset path escaped its exact root: $path"
        }
        [void] $expectedPackageStems.Add([IO.Path]::ChangeExtension($path, $null))
    }

    $rootExists = Test-Path -LiteralPath $temasekNativeAssetRoot
    if ($rootExists) {
        $rootItem = Get-Item -LiteralPath $temasekNativeAssetRoot -Force
        if (-not $rootItem.PSIsContainer -or
            ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Temasek native asset root is not a regular directory: $temasekNativeAssetRoot"
        }
    }

    $primary = [Collections.Generic.List[object]]::new()
    foreach ($path in $expectedPrimaryPaths) {
        Assert-NoReparseAncestor -Path $path -Root $nativeProjectRoot `
            -Label 'Temasek generated asset'
        $primary.Add((Get-FileSnapshot -Path $path -AllowMissing:(!$RequireComplete)))
    }

    $allowedExtensions = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($extension in @('.uasset', '.uexp', '.ubulk', '.uptnl')) {
        [void] $allowedExtensions.Add($extension)
    }
    $artifacts = [Collections.Generic.List[object]]::new()
    if ($rootExists) {
        foreach ($file in @(Get-ChildItem -LiteralPath $temasekNativeAssetRoot `
                -File -Recurse -Force | Sort-Object FullName)) {
            $full = [IO.Path]::GetFullPath($file.FullName)
            Assert-NoReparseAncestor -Path $full -Root $nativeProjectRoot `
                -Label 'Temasek generated package artifact'
            $extension = [IO.Path]::GetExtension($full)
            $stem = [IO.Path]::ChangeExtension($full, $null)
            if (-not $allowedExtensions.Contains($extension) -or
                -not $expectedPackageStems.Contains($stem)) {
                throw "Unexpected file in the exact Temasek generated asset root: $full"
            }
            $artifacts.Add((Get-FileSnapshot -Path $full))
        }
    }

    $complete = $rootExists -and
        @($primary | Where-Object { -not $_.Exists }).Count -eq 0
    if ($RequireComplete -and -not $complete) {
        throw 'The exact twenty-package Temasek generated asset roster is incomplete.'
    }
    return [pscustomobject] [ordered] @{
        AssetRoot = $temasekNativeAssetRoot
        RootExists = [bool] $rootExists
        Complete = [bool] $complete
        ExpectedPrimaryAssetCount = $expectedPrimaryPaths.Count
        PrimaryAssets = @($primary)
        PackageArtifacts = @($artifacts)
    }
}

function Assert-TemasekAssetInventorySame {
    param([Parameter(Mandatory = $true)] [object] $Expected)

    if (-not $Expected.Complete) {
        throw 'Cannot compare against an incomplete Temasek asset inventory.'
    }
    $actual = Get-TemasekAssetInventory -RequireComplete
    $expectedPaths = @($Expected.PackageArtifacts | ForEach-Object { $_.Path } | Sort-Object)
    $actualPaths = @($actual.PackageArtifacts | ForEach-Object { $_.Path } | Sort-Object)
    if ($expectedPaths.Count -ne $actualPaths.Count -or
        ($expectedPaths -join "`n") -cne ($actualPaths -join "`n")) {
        throw 'The exact Temasek generated package artifact roster changed.'
    }
    foreach ($snapshot in $Expected.PackageArtifacts) {
        [void] (Assert-SameSnapshot -Expected $snapshot -Path $snapshot.Path `
            -Label 'Temasek generated asset during cold validation')
    }
    return $actual
}

function New-MigrationRollbackJournal {
    param(
        [Parameter(Mandatory = $true)] [string] $EvidenceRoot,
        [Parameter(Mandatory = $true)] [object] $MapSnapshot,
        [Parameter(Mandatory = $true)] [object] $MacDonaldInventory,
        [Parameter(Mandatory = $true)] [object] $TemasekInventory
    )

    $backupRoot = [IO.Path]::GetFullPath(
        (Join-Path $EvidenceRoot 'migration_before'))
    if (-not (Test-ContainedPath -Path $backupRoot -Root $EvidenceRoot) -or
        (Test-Path -LiteralPath $backupRoot)) {
        throw "Migration rollback root is not fresh and contained: $backupRoot"
    }
    [void] (New-Item -ItemType Directory -Path $backupRoot)
    Assert-NoReparseAncestor -Path $backupRoot -Root $EvidenceRoot `
        -Label 'migration rollback root'

    $ownedSnapshots = @(
        @($MacDonaldInventory.PackageArtifacts) +
        @($TemasekInventory.PackageArtifacts))
    $sourceRows = @($MapSnapshot) + $ownedSnapshots
    $backups = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $sourceRows.Count; ++$index) {
        $sourceSnapshot = $sourceRows[$index]
        if (-not $sourceSnapshot.Exists) {
            throw "Migration rollback cannot back up an absent listed file: $($sourceSnapshot.Path)"
        }
        $backup = [IO.Path]::GetFullPath((Join-Path $backupRoot `
            ('{0:D4}.bin' -f $index)))
        Assert-NoReparseAncestor -Path $sourceSnapshot.Path `
            -Root $nativeProjectRoot -Label 'migration predecessor artifact'
        Assert-NoReparseAncestor -Path $backup -Root $EvidenceRoot `
            -Label 'migration rollback copy'
        [void] (Assert-SameSnapshot -Expected $sourceSnapshot `
            -Path $sourceSnapshot.Path -Label 'migration predecessor artifact')
        Copy-Item -LiteralPath $sourceSnapshot.Path -Destination $backup
        $backupSnapshot = Get-FileSnapshot -Path $backup
        if ($backupSnapshot.Bytes -ne $sourceSnapshot.Bytes -or
            $backupSnapshot.Sha256 -cne $sourceSnapshot.Sha256) {
            throw "Migration rollback backup verification failed: $backup"
        }
        $backups.Add([pscustomobject] [ordered] @{
            Destination = $sourceSnapshot.Path
            Before = $sourceSnapshot
            Backup = $backupSnapshot
        })
    }
    $journal = [pscustomobject] [ordered] @{
        MapBefore = $MapSnapshot
        MacDonaldBefore = $MacDonaldInventory
        TemasekBefore = $TemasekInventory
        Backups = @($backups)
    }
    Write-JsonFile -Path (Join-Path $backupRoot 'manifest.json') -Value $journal
    return $journal
}

function Assert-MigrationInventoryRestored {
    param(
        [Parameter(Mandatory = $true)] [object] $Expected,
        [Parameter(Mandatory = $true)] [object] $Actual,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $expectedArtifacts = @($Expected.PackageArtifacts | Sort-Object Path)
    $actualArtifacts = @($Actual.PackageArtifacts | Sort-Object Path)
    if ($expectedArtifacts.Count -ne $actualArtifacts.Count -or
        (@($expectedArtifacts.Path) -join "`n") -cne
        (@($actualArtifacts.Path) -join "`n")) {
        throw "$Label artifact roster was not restored exactly."
    }
    foreach ($snapshot in $expectedArtifacts) {
        [void] (Assert-SameSnapshot -Expected $snapshot -Path $snapshot.Path `
            -Label "$Label restored artifact")
    }
}

function Restore-MigrationRollbackJournal {
    param([Parameter(Mandatory = $true)] [object] $Journal)

    Assert-NativeProjectIdle
    $beforePaths = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $Journal.Backups) {
        [void] $beforePaths.Add([IO.Path]::GetFullPath($row.Destination))
    }

    foreach ($inventory in @(
            (Get-MacDonaldAssetInventory),
            (Get-TemasekAssetInventory))) {
        foreach ($artifact in $inventory.PackageArtifacts) {
            $path = [IO.Path]::GetFullPath($artifact.Path)
            if (-not $beforePaths.Contains($path)) {
                Assert-NoReparseAncestor -Path $path -Root $inventory.AssetRoot `
                    -Label 'new migration-owned package artifact'
                $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
                if ($item.PSIsContainer -or
                    ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                    throw "Refusing non-regular migration rollback target: $path"
                }
                Remove-Item -LiteralPath $path -Force
            }
        }
    }

    foreach ($row in $Journal.Backups) {
        [void] (Assert-SameSnapshot -Expected $row.Backup `
            -Path $row.Backup.Path -Label 'migration rollback backup')
        Assert-NoReparseAncestor -Path $row.Destination `
            -Root $nativeProjectRoot -Label 'migration rollback destination'
        Copy-Item -LiteralPath $row.Backup.Path `
            -Destination $row.Destination -Force
        [void] (Assert-SameSnapshot -Expected $row.Before `
            -Path $row.Destination -Label 'restored migration predecessor')
    }

    [void] (Assert-SameSnapshot -Expected $Journal.MapBefore `
        -Path $Journal.MapBefore.Path -Label 'restored migration map')
    Assert-MigrationInventoryRestored -Expected $Journal.MacDonaldBefore `
        -Actual (Get-MacDonaldAssetInventory) -Label 'MacDonald'
    Assert-MigrationInventoryRestored -Expected $Journal.TemasekBefore `
        -Actual (Get-TemasekAssetInventory) -Label 'Temasek'
    return 'ROLLBACK_PASS exactMapAndOwnedPackageArtifactsRestored=true'
}

function Invoke-ApplyPhase {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Rows,
        [Parameter(Mandatory = $true)] [string] $EvidenceRoot
    )

    $changedRows = @($Rows | Where-Object { $_.NeedsCopy })
    $backupRoot = Join-Path $EvidenceRoot 'native_before'
    $applied = [Collections.Generic.List[object]]::new()

    # Snapshot and verify every predecessor backup before the first destination
    # changes. New files are recorded explicitly and need no placeholder file.
    foreach ($row in $changedRows) {
        [void] (Assert-SameSnapshot -Expected $row.SourceSnapshot `
            -Path $row.Source -Label 'reviewed R24 source before backup')
        [void] (Assert-SameSnapshot -Expected $row.DestinationBefore `
            -Path $row.Destination -Label 'native predecessor before backup')
        if ($row.DestinationBefore.Exists) {
            $backup = [IO.Path]::GetFullPath(
                (Join-Path $backupRoot $row.RelativePath))
            if (-not (Test-ContainedPath -Path $backup -Root $backupRoot)) {
                throw "Backup path escaped the R24 evidence root: $backup"
            }
            $backupParent = [IO.Path]::GetDirectoryName($backup)
            if (-not (Test-Path -LiteralPath $backupParent -PathType Container)) {
                [void] (New-Item -ItemType Directory -Path $backupParent -Force)
            }
            Copy-Item -LiteralPath $row.Destination -Destination $backup
            [void] (Assert-SameSnapshot -Expected $row.DestinationBefore `
                -Path $backup -Label 'R24 predecessor backup')
            $row | Add-Member -NotePropertyName Backup -NotePropertyValue $backup
        }
        else {
            $row | Add-Member -NotePropertyName Backup -NotePropertyValue $null
        }
    }

    try {
        foreach ($row in $changedRows) {
            [void] (Assert-SameSnapshot -Expected $row.SourceSnapshot `
                -Path $row.Source -Label 'reviewed R24 source at copy time')
            [void] (Assert-SameSnapshot -Expected $row.DestinationBefore `
                -Path $row.Destination -Label 'native predecessor at copy time')
            $destinationParent = [IO.Path]::GetDirectoryName($row.Destination)
            if (-not (Test-Path -LiteralPath $destinationParent -PathType Container)) {
                [void] (New-Item -ItemType Directory `
                    -Path $destinationParent -Force)
            }
            # The exact destination becomes transaction-owned before the first
            # mutating call, so a failed or partial Copy-Item is still rolled
            # back from its verified predecessor receipt.
            $applied.Add($row)
            Copy-Item -LiteralPath $row.Source `
                -Destination $row.Destination -Force
            [void] (Assert-SameSnapshot -Expected $row.SourceSnapshot `
                -Path $row.Destination -Label 'R24 native readback')
        }
    }
    catch {
        $primaryError = $_.Exception.Message
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        for ($index = $applied.Count - 1; $index -ge 0; --$index) {
            $row = $applied[$index]
            try {
                if ($row.DestinationBefore.Exists) {
                    [void] (Assert-SameSnapshot `
                        -Expected $row.DestinationBefore -Path $row.Backup `
                        -Label 'R24 rollback backup')
                    Copy-Item -LiteralPath $row.Backup `
                        -Destination $row.Destination -Force
                    [void] (Assert-SameSnapshot `
                        -Expected $row.DestinationBefore `
                        -Path $row.Destination -Label 'R24 rollback readback')
                }
                else {
                    # Exact, non-recursive removal is allowed only for a new
                    # allowlisted file whose mutation attempt is ledgered by
                    # this failed transaction. It may be absent or partial.
                    if (Test-Path -LiteralPath $row.Destination) {
                        [void] (Get-FileSnapshot -Path $row.Destination)
                        Remove-Item -LiteralPath $row.Destination -Force
                    }
                    if (Test-Path -LiteralPath $row.Destination) {
                        throw "New allowlisted file remained after rollback: $($row.Destination)"
                    }
                }
            }
            catch {
                $rollbackErrors.Add($_.Exception.Message)
            }
        }
        throw "R24 allowlisted copy failed; rollbackErrors=$($rollbackErrors -join '; ') original=$primaryError"
    }

    foreach ($row in $Rows) {
        $row.NeedsCopy = $false
        [void] (Assert-SameSnapshot -Expected $row.SourceSnapshot `
            -Path $row.Destination -Label 'complete R24 native sync')
    }
    return [pscustomobject] [ordered] @{
        Status = 'PASS'
        ChangedFiles = $changedRows.Count
        UnchangedFiles = $Rows.Count - $changedRows.Count
        BackupRoot = if ($changedRows.Count -ne 0) { $backupRoot } else { $null }
        RecursiveDeleteUsed = $false
    }
}

function Invoke-BuildPhase {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Rows,
        [Parameter(Mandatory = $true)] [string] $EvidenceRoot
    )

    Assert-NativeSourcesMatch -Rows $Rows
    Assert-NativeProjectIdle
    $protectedBefore = Get-ProtectedUE54Identity
    $immutableBefore = Get-ImmutableSnapshots
    $buildLog = Join-Path $EvidenceRoot 'build.log'
    $buildArguments = @(
        'UnrealEditor',
        'Win64',
        'Development',
        $nativeProjectFile,
        '-DisablePlugin=AirSim',
        '-Module=TRIADSensorFusion',
        '-Module=TRIADSensorFusionEditor',
        '-WaitMutex',
        '-NoHotReloadFromIDE',
        '-MaxParallelActions=1',
        '-NoUBA',
        '-NoUBALocal'
    )
    & $buildTool @buildArguments *> $buildLog
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        throw "R24 module-scoped UE5.5 build failed with exit code $buildExitCode. Log: $buildLog"
    }
    if (-not (Test-Path -LiteralPath $buildLog -PathType Leaf)) {
        throw 'R24 build did not persist its evidence log.'
    }
    Assert-NativeSourcesMatch -Rows $Rows
    Assert-ImmutableSnapshots -Snapshots $immutableBefore
    Assert-NativeProjectIdle
    $protectedAfter = Assert-ProtectedUE54Unchanged -Before $protectedBefore
    $buildProducts = @($buildProductPaths | ForEach-Object {
        Get-FileSnapshot -Path $_
    })
    return [pscustomobject] [ordered] @{
        Status = 'PASS'
        ExitCode = [int] $buildExitCode
        Log = $buildLog
        BuildProducts = $buildProducts
        ProtectedUE54Before = $protectedBefore
        ProtectedUE54After = $protectedAfter
        ProtectedContentHashesUnchanged = $true
    }
}

function Get-LaunchedProcessIdentity {
    param(
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Process,
        [Parameter(Mandatory = $true)] [string] $Filter
    )

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    $native = $null
    do {
        $candidate = Get-CimInstance Win32_Process `
            -Filter "ProcessId = $($Process.Id)" -ErrorAction SilentlyContinue
        if ($null -ne $candidate -and
            -not [string]::IsNullOrWhiteSpace(
                [string] $candidate.ExecutablePath) -and
            -not [string]::IsNullOrWhiteSpace(
                [string] $candidate.CommandLine)) {
            $native = $candidate
            break
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($null -eq $native) {
        throw "Could not capture the launched R24 automation identity for PID $($Process.Id)."
    }
    $creationTicks = ([DateTimeOffset] $native.CreationDate).UtcTicks
    $actualExecutable = [IO.Path]::GetFullPath([string] $native.ExecutablePath)
    if ([uint32] $native.ProcessId -eq $protectedProcessId -or
        $native.Name -cne 'UnrealEditor-Cmd.exe' -or
        $actualExecutable -cne $editorCommand -or
        [string]::IsNullOrWhiteSpace([string] $native.CommandLine) -or
        [string] $native.CommandLine -notlike "*$nativeProjectFile*" -or
        [string] $native.CommandLine -notlike "*$Filter*" -or
        [Math]::Abs(
            $creationTicks - $Process.StartTime.ToUniversalTime().Ticks) -gt
            [TimeSpan]::FromSeconds(2).Ticks) {
        throw 'Launched R24 automation failed exact PID/executable/project/filter/start identity capture.'
    }
    return [pscustomobject] [ordered] @{
        ProcessId = [uint32] $native.ProcessId
        CreationUtcTicks = [int64] $creationTicks
        ExecutablePath = $actualExecutable
        CommandLine = [string] $native.CommandLine
    }
}

function Stop-OwnedTimedOutAutomation {
    param([Parameter(Mandatory = $true)] [object] $Identity)

    [void] (Get-ProtectedUE54Identity)
    $live = Get-CimInstance Win32_Process `
        -Filter "ProcessId = $($Identity.ProcessId)" -ErrorAction SilentlyContinue
    if ($null -eq $live) {
        return
    }
    $creationTicks = ([DateTimeOffset] $live.CreationDate).UtcTicks
    $actualExecutable = if ([string]::IsNullOrWhiteSpace(
        [string] $live.ExecutablePath)) {
        ''
    }
    else {
        [IO.Path]::GetFullPath([string] $live.ExecutablePath)
    }
    if ([uint32] $live.ProcessId -eq $protectedProcessId -or
        $live.Name -cne 'UnrealEditor-Cmd.exe' -or
        $actualExecutable -cne $editorCommand -or
        [int64] $creationTicks -ne $Identity.CreationUtcTicks -or
        [string] $live.CommandLine -cne $Identity.CommandLine -or
        [string] $live.CommandLine -notlike "*$nativeProjectFile*") {
        throw 'Refused to stop a timed-out process whose exact R24 UE5.5 identity cannot be reproved.'
    }
    Stop-Process -Id $Identity.ProcessId -Force -ErrorAction Stop
    [void] (Get-ProtectedUE54Identity)
}

function Invoke-OneAutomationFilter {
    param(
        [Parameter(Mandatory = $true)] [string] $Filter,
        [Parameter(Mandatory = $true)] [string] $EvidenceRoot,
        [ValidateRange(1, 100)] [int] $ExpectedSuccessfulCompletions = 1,
        [switch] $RenderCapable
    )

    Assert-NativeProjectIdle
    $filterSlug = $Filter -replace '[^A-Za-z0-9._-]', '_'
    $filterRoot = Join-Path (Join-Path $EvidenceRoot 'automation') $filterSlug
    $reportRoot = Join-Path $filterRoot 'report'
    $logFile = Join-Path $filterRoot 'automation.log'
    [void] (New-Item -ItemType Directory -Path $reportRoot -Force)
    $localDdcPath = Join-Path $nativeProjectRoot 'Saved\DerivedDataCache'
    $taskTempPath = Join-Path $nativeProjectRoot `
        'Saved\TRIAD\Temp\R24NativeTransaction'
    [void] (New-Item -ItemType Directory -Path $localDdcPath -Force)
    [void] (New-Item -ItemType Directory -Path $taskTempPath -Force)

    $renderArguments = if ($RenderCapable) {
        '-NoSound '
    }
    else {
        '-NullRHI -NoSound '
    }
    $argumentLine = '"' + $nativeProjectFile + '" /Game/Maps/Entry ' +
        '-DisablePlugin=AirSim -unattended -nop4 -NoSplash ' +
        $renderArguments +
        '-DDC=InstalledNoZenLocalFallback ' +
        '-LocalDataCachePath="' + $localDdcPath + '" ' +
        '-TRIADRFTightGeometry="' + $tightGeometryPath + '" ' +
        '-TRIADRFTightCatalog="' + $tightCatalogPath + '" ' +
        '-ExecCmds="Automation RunTests ' + $Filter + '" ' +
        '-TestExit="Automation Test Queue Empty" ' +
        '-ReportOutputPath="' + $reportRoot + '" ' +
        '-abslog="' + $logFile + '"'

    $previousTemp = [Environment]::GetEnvironmentVariable('TEMP', 'Process')
    $previousTmp = [Environment]::GetEnvironmentVariable('TMP', 'Process')
    $previousDdc = [Environment]::GetEnvironmentVariable(
        'UE-LocalDataCachePath', 'Process')
    $process = $null
    $identity = $null
    try {
        [Environment]::SetEnvironmentVariable('TEMP', $taskTempPath, 'Process')
        [Environment]::SetEnvironmentVariable('TMP', $taskTempPath, 'Process')
        [Environment]::SetEnvironmentVariable(
            'UE-LocalDataCachePath', $localDdcPath, 'Process')
        $process = Start-Process -FilePath $editorCommand `
            -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot `
            -PassThru -WindowStyle Hidden
        $identity = Get-LaunchedProcessIdentity -Process $process -Filter $Filter
        if (-not $process.WaitForExit($AutomationTimeoutSeconds * 1000)) {
            Stop-OwnedTimedOutAutomation -Identity $identity
            [void] $process.WaitForExit(30000)
            throw "R24 automation filter timed out after $AutomationTimeoutSeconds seconds: $Filter"
        }
        if ($process.ExitCode -ne 0) {
            throw "R24 automation filter exited with code $($process.ExitCode): $Filter"
        }
    }
    catch {
        $primaryError = $_.Exception.Message
        if ($null -ne $process -and -not $process.HasExited) {
            try {
                if ($null -eq $identity) {
                    $identity = Get-LaunchedProcessIdentity `
                        -Process $process -Filter $Filter
                }
                Stop-OwnedTimedOutAutomation -Identity $identity
                [void] $process.WaitForExit(30000)
            }
            catch {
                throw "R24 automation failed and exact owned-process cleanup also failed; original={$primaryError} cleanup={$($_.Exception.Message)}"
            }
        }
        throw $primaryError
    }
    finally {
        [Environment]::SetEnvironmentVariable('TEMP', $previousTemp, 'Process')
        [Environment]::SetEnvironmentVariable('TMP', $previousTmp, 'Process')
        [Environment]::SetEnvironmentVariable(
            'UE-LocalDataCachePath', $previousDdc, 'Process')
    }

    if (-not (Test-Path -LiteralPath $logFile -PathType Leaf)) {
        throw "R24 automation did not persist its log for filter: $Filter"
    }
    $logText = Get-Content -LiteralPath $logFile -Raw
    $successfulCompletions = [regex]::Matches(
        $logText, 'Test Completed\. Result=\{Success\}').Count
    if ($logText -notmatch 'Automation Test Queue Empty' -or
        $successfulCompletions -ne $ExpectedSuccessfulCompletions -or
        $logText -match 'Test Completed\. Result=\{Fail' -or
        $logText -match 'Automation Test Failed') {
        throw "R24 automation log lacks the exact $ExpectedSuccessfulCompletions clean success completions for filter: $Filter; actual=$successfulCompletions log=$logFile"
    }
    Assert-NativeProjectIdle
    return [pscustomobject] [ordered] @{
        Status = 'PASS'
        Filter = $Filter
        ProcessId = $identity.ProcessId
        ExitCode = [int] $process.ExitCode
        ExpectedSuccessfulTestCompletions = $ExpectedSuccessfulCompletions
        SuccessfulTestCompletions = $successfulCompletions
        RenderCapable = [bool] $RenderCapable
        Log = $logFile
        Report = $reportRoot
    }
}

function Invoke-MigratePhase {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Rows,
        [Parameter(Mandatory = $true)] [string] $EvidenceRoot
    )

    Assert-NativeSourcesMatch -Rows $Rows
    Assert-NativeProjectIdle
    $protectedBefore = Get-ProtectedUE54Identity
    $immutableBefore = Get-MigrationImmutableSnapshots
    $mapBefore = Get-FileSnapshot -Path $migratableV5DMap
    $macDonaldAssetsBefore = Get-MacDonaldAssetInventory
    $temasekAssetsBefore = Get-TemasekAssetInventory
    $rollbackJournal = New-MigrationRollbackJournal `
        -EvidenceRoot $EvidenceRoot -MapSnapshot $mapBefore `
        -MacDonaldInventory $macDonaldAssetsBefore `
        -TemasekInventory $temasekAssetsBefore
    try {
        $macDonaldResult = Invoke-OneAutomationFilter `
            -Filter $macDonaldMigrationFilter `
            -EvidenceRoot $EvidenceRoot `
            -ExpectedSuccessfulCompletions 1 `
            -RenderCapable
        [void] (Assert-ProtectedUE54Unchanged -Before $protectedBefore)
        Assert-NativeSourcesMatch -Rows $Rows
        Assert-ImmutableSnapshots -Snapshots $immutableBefore
        $macDonaldAssetsAfterMacMigration = Get-MacDonaldAssetInventory -RequireComplete
        $temasekResult = Invoke-OneAutomationFilter `
            -Filter $temasekMigrationFilter `
            -EvidenceRoot $EvidenceRoot `
            -ExpectedSuccessfulCompletions 1 `
            -RenderCapable
        [void] (Assert-ProtectedUE54Unchanged -Before $protectedBefore)
        Assert-NativeSourcesMatch -Rows $Rows
        Assert-ImmutableSnapshots -Snapshots $immutableBefore
        $macDonaldAssetsAfter = Assert-MacDonaldAssetInventorySame `
            -Expected $macDonaldAssetsAfterMacMigration
        $temasekAssetsAfter = Get-TemasekAssetInventory -RequireComplete
        $mapAfter = Get-FileSnapshot -Path $migratableV5DMap
        Assert-NativeProjectIdle
        $protectedAfter = Assert-ProtectedUE54Unchanged -Before $protectedBefore
    }
    catch {
        $migrationFailure = $_
        $rollbackStatus = try {
            Restore-MigrationRollbackJournal -Journal $rollbackJournal
        }
        catch {
            'ROLLBACK_FAILED ' + $_.Exception.Message
        }
        throw "R24 migration transaction failed: $($migrationFailure.Exception.Message); externalRollback={$rollbackStatus}"
    }
    return [pscustomobject] [ordered] @{
        Status = 'PASS'
        Filters = @($macDonaldResult, $temasekResult)
        HybridMapBefore = $mapBefore
        HybridMapAfter = $mapAfter
        HybridMapChanged = ($mapBefore.Bytes -ne $mapAfter.Bytes -or
            $mapBefore.Sha256 -cne $mapAfter.Sha256)
        MacDonaldGeneratedAssetsBefore = $macDonaldAssetsBefore
        MacDonaldGeneratedAssetsAfter = $macDonaldAssetsAfter
        TemasekGeneratedAssetsBefore = $temasekAssetsBefore
        TemasekGeneratedAssetsAfter = $temasekAssetsAfter
        ProtectedUE54Before = $protectedBefore
        ProtectedUE54After = $protectedAfter
        ProtectedNonMigratedContentHashesUnchanged = $true
    }
}

function Invoke-TestPhase {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Rows,
        [Parameter(Mandatory = $true)] [string] $EvidenceRoot
    )

    Assert-NativeSourcesMatch -Rows $Rows
    Assert-NativeProjectIdle
    $protectedBefore = Get-ProtectedUE54Identity
    $immutableBefore = Get-ImmutableSnapshots
    $macDonaldAssetsBefore = Get-MacDonaldAssetInventory -RequireComplete
    $temasekAssetsBefore = Get-TemasekAssetInventory -RequireComplete
    $results = [Collections.Generic.List[object]]::new()
    foreach ($filter in $TestFilter) {
        $expectedSuccessfulCompletions = if ($filter -ceq
            'TRIAD.Istana.ExploreV5D.MacDonaldHouse' -or $filter -ceq
            'TRIAD.Istana.ExploreV5D.TemasekShophouse') {
            3
        }
        else {
            1
        }
        $results.Add((Invoke-OneAutomationFilter `
            -Filter $filter `
            -EvidenceRoot $EvidenceRoot `
            -ExpectedSuccessfulCompletions $expectedSuccessfulCompletions))
        [void] (Assert-ProtectedUE54Unchanged -Before $protectedBefore)
    }
    Assert-NativeSourcesMatch -Rows $Rows
    Assert-ImmutableSnapshots -Snapshots $immutableBefore
    $macDonaldAssetsAfter = Assert-MacDonaldAssetInventorySame `
        -Expected $macDonaldAssetsBefore
    $temasekAssetsAfter = Assert-TemasekAssetInventorySame `
        -Expected $temasekAssetsBefore
    Assert-NativeProjectIdle
    $protectedAfter = Assert-ProtectedUE54Unchanged -Before $protectedBefore
    return [pscustomobject] [ordered] @{
        Status = 'PASS'
        Filters = @($results)
        MacDonaldGeneratedAssetsBefore = $macDonaldAssetsBefore
        MacDonaldGeneratedAssetsAfter = $macDonaldAssetsAfter
        TemasekGeneratedAssetsBefore = $temasekAssetsBefore
        TemasekGeneratedAssetsAfter = $temasekAssetsAfter
        ProtectedUE54Before = $protectedBefore
        ProtectedUE54After = $protectedAfter
        ProtectedContentHashesUnchanged = $true
    }
}

# Normalize and validate the requested phase set before any filesystem write.
$requestedPhases = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($requestedPhase in $Phase) {
    if ($requestedPhase -ieq 'All') {
        [void] $requestedPhases.Add('Apply')
        [void] $requestedPhases.Add('Build')
        [void] $requestedPhases.Add('Migrate')
        [void] $requestedPhases.Add('Test')
    }
    else {
        [void] $requestedPhases.Add($requestedPhase)
    }
}
if ($requestedPhases.Contains('DryRun') -and $requestedPhases.Count -ne 1) {
    throw 'DryRun is exclusive. Use -Phase Apply,Build,Migrate,Test or -Phase All for execution.'
}
if ($TestFilter.Count -ne $allowedTestFilters.Count) {
    throw "Test requires the exact ordered five-filter gate roster; expected=$($allowedTestFilters.Count) actual=$($TestFilter.Count)"
}
$seenFilters = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::Ordinal)
for ($filterIndex = 0; $filterIndex -lt $TestFilter.Count; ++$filterIndex) {
    $filter = $TestFilter[$filterIndex]
    if ($filter -notmatch '^[A-Za-z0-9_.]+$' -or
        $filter -cnotin $allowedTestFilters -or
        -not $seenFilters.Add($filter) -or
        $filter -cne $allowedTestFilters[$filterIndex]) {
        throw "Automation filter roster is not exact, unique, allowlisted, and ordered at index $filterIndex`: $filter"
    }
}

Assert-ExactDirectory -Path $repositoryUnrealRoot `
    -Expected 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal' `
    -Label 'repository Unreal'
Assert-ExactDirectory -Path $nativeProjectRoot -Expected 'D:\triad\TRIAD' `
    -Label 'native project'
Assert-ExactDirectory -Path $engineRoot `
    -Expected 'C:\Program Files\Epic Games\UE_5.5' -Label 'UE5.5 engine'
[void] (Get-FileSnapshot -Path $nativeProjectFile)
[void] (Get-FileSnapshot -Path $buildTool)
[void] (Get-FileSnapshot -Path $editorCommand)
$buildVersion = Get-Content -LiteralPath $buildVersionFile -Raw |
    ConvertFrom-Json
if ([int] $buildVersion.MajorVersion -ne 5 -or
    [int] $buildVersion.MinorVersion -ne 5) {
    throw 'The reviewed engine root no longer reports Unreal Engine 5.5.'
}

# The TightV1 inputs intentionally live behind one reviewed workspace relief
# junction. Verify that junction target before trusting the pinned artifacts.
$sourceAssetsItem = Get-Item -LiteralPath `
    (Join-Path $repositoryUnrealRoot 'SourceAssets') -Force
if (-not $sourceAssetsItem.PSIsContainer -or
    $sourceAssetsItem.LinkType -cne 'Junction' -or
    @($sourceAssetsItem.Target).Count -ne 1 -or
    [IO.Path]::GetFullPath([string] @($sourceAssetsItem.Target)[0]) -cne
        $expectedSourceAssetsTarget) {
    throw 'The reviewed repository SourceAssets junction identity changed.'
}
$tightGeometrySnapshot = Assert-PinnedFile -Path $tightGeometryPath `
    -Bytes $tightGeometryBytes -Sha256 $tightGeometrySha256 `
    -Label 'TightV1 geometry'
$tightCatalogSnapshot = Assert-PinnedFile -Path $tightCatalogPath `
    -Bytes $tightCatalogBytes -Sha256 $tightCatalogSha256 `
    -Label 'TightV1 material catalog'
$oneKilometreV2GeometrySnapshot = Assert-PinnedFile `
    -Path $oneKilometreV2GeometryPath `
    -Bytes $oneKilometreV2GeometryBytes `
    -Sha256 $oneKilometreV2GeometrySha256 `
    -Label 'OneKilometreV2 staged geometry'
$oneKilometreV2CatalogSnapshot = Assert-PinnedFile `
    -Path $oneKilometreV2CatalogPath `
    -Bytes $oneKilometreV2CatalogBytes `
    -Sha256 $oneKilometreV2CatalogSha256 `
    -Label 'OneKilometreV2 staged material catalog'
$oneKilometreV2ContractSnapshot = Assert-PinnedFile `
    -Path $oneKilometreV2ContractPath `
    -Bytes $oneKilometreV2ContractBytes `
    -Sha256 $oneKilometreV2ContractSha256 `
    -Label 'OneKilometreV2 staged scene contract'
$protectedAtPreflight = Get-ProtectedUE54Identity
Assert-NativeProjectIdle
$syncRows = @(Get-SyncRows)
$immutableAtPreflight = @(Get-ImmutableSnapshots)
$macDonaldAssetsAtPreflight = Get-MacDonaldAssetInventory
$temasekAssetsAtPreflight = Get-TemasekAssetInventory

$preflight = [pscustomobject] [ordered] @{
    SchemaVersion = 'triad.r24_native_transaction.v2'
    TimestampUtc = [DateTime]::UtcNow.ToString('o')
    RequestedPhases = @($requestedPhases | Sort-Object)
    RepositoryUnrealRoot = $repositoryUnrealRoot
    NativeProjectRoot = $nativeProjectRoot
    NativeProjectFile = $nativeProjectFile
    EngineRoot = $engineRoot
    EngineVersion = [pscustomobject] [ordered] @{
        Major = [int] $buildVersion.MajorVersion
        Minor = [int] $buildVersion.MinorVersion
        Patch = [int] $buildVersion.PatchVersion
        Changelist = [int64] $buildVersion.Changelist
    }
    ProtectedUE54 = $protectedAtPreflight
    TightV1Geometry = $tightGeometrySnapshot
    TightV1Catalog = $tightCatalogSnapshot
    OneKilometreV2Geometry = $oneKilometreV2GeometrySnapshot
    OneKilometreV2Catalog = $oneKilometreV2CatalogSnapshot
    OneKilometreV2SceneContract = $oneKilometreV2ContractSnapshot
    Files = $syncRows
    ProtectedNativeInputs = $immutableAtPreflight
    MacDonaldGeneratedAssets = $macDonaldAssetsAtPreflight
    TemasekGeneratedAssets = $temasekAssetsAtPreflight
    FilesNeedingCopy = @($syncRows | Where-Object { $_.NeedsCopy }).Count
    RecursiveDeletePermitted = $false
}

if ($requestedPhases.Contains('DryRun')) {
    [pscustomobject] [ordered] @{
        Status = 'DRY_RUN_PASS'
        NativeMutated = $false
        Preflight = $preflight
        Next = 'Run with -Phase Apply,Build,Migrate,Test (or -Phase All) after reviewing this allowlist and hash snapshot.'
    } | ConvertTo-Json -Depth 12
    exit 0
}

$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff')
$evidenceRoot = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot `
        "Saved\TRIAD\Automation\R24NativeTransaction\$($timestamp)_$PID"))
if (-not (Test-ContainedPath -Path $evidenceRoot -Root $nativeProjectRoot) -or
    (Test-Path -LiteralPath $evidenceRoot)) {
    throw "R24 evidence root is not fresh and contained: $evidenceRoot"
}
[void] (New-Item -ItemType Directory -Path $evidenceRoot)
Write-JsonFile -Path (Join-Path $evidenceRoot 'preflight.json') `
    -Value $preflight

$phaseResults = [Collections.Generic.List[object]]::new()
try {
    if ($requestedPhases.Contains('Apply')) {
        $protectedBefore = Get-ProtectedUE54Identity
        Assert-NativeProjectIdle
        $result = Invoke-ApplyPhase -Rows $syncRows `
            -EvidenceRoot $evidenceRoot
        $protectedAfter = Assert-ProtectedUE54Unchanged -Before $protectedBefore
        Assert-ImmutableSnapshots -Snapshots $immutableAtPreflight
        $result | Add-Member -NotePropertyName Phase -NotePropertyValue 'Apply'
        $result | Add-Member -NotePropertyName ProtectedUE54After `
            -NotePropertyValue $protectedAfter
        $phaseResults.Add($result)
    }
    if ($requestedPhases.Contains('Build')) {
        $result = Invoke-BuildPhase -Rows $syncRows `
            -EvidenceRoot $evidenceRoot
        $result | Add-Member -NotePropertyName Phase -NotePropertyValue 'Build'
        $phaseResults.Add($result)
    }
    if ($requestedPhases.Contains('Migrate')) {
        $result = Invoke-MigratePhase -Rows $syncRows `
            -EvidenceRoot $evidenceRoot
        $result | Add-Member -NotePropertyName Phase -NotePropertyValue 'Migrate'
        $phaseResults.Add($result)
    }
    if ($requestedPhases.Contains('Test')) {
        $result = Invoke-TestPhase -Rows $syncRows `
            -EvidenceRoot $evidenceRoot
        $result | Add-Member -NotePropertyName Phase -NotePropertyValue 'Test'
        $phaseResults.Add($result)
    }
    $protectedAtEnd = Assert-ProtectedUE54Unchanged `
        -Before $protectedAtPreflight
    $receipt = [pscustomobject] [ordered] @{
        SchemaVersion = 'triad.r24_native_transaction.v2'
        Status = 'PASS'
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
        EvidenceRoot = $evidenceRoot
        Phases = @($phaseResults)
        ProtectedUE54 = $protectedAtEnd
        SourceAndNativeHashesMatch = $true
        RecursiveDeleteUsed = $false
    }
    Assert-NativeSourcesMatch -Rows $syncRows
    Write-JsonFile -Path (Join-Path $evidenceRoot 'receipt.json') `
        -Value $receipt
    $receipt | ConvertTo-Json -Depth 12
}
catch {
    $failure = [pscustomobject] [ordered] @{
        SchemaVersion = 'triad.r24_native_transaction.v2'
        Status = 'FAIL'
        FailedUtc = [DateTime]::UtcNow.ToString('o')
        EvidenceRoot = $evidenceRoot
        CompletedPhases = @($phaseResults)
        Error = $_.Exception.Message
        ProtectedUE54AtFailure = try {
            Get-ProtectedUE54Identity
        }
        catch {
            [pscustomobject] @{ Error = $_.Exception.Message }
        }
    }
    Write-JsonFile -Path (Join-Path $evidenceRoot 'failure.json') `
        -Value $failure
    throw
}
