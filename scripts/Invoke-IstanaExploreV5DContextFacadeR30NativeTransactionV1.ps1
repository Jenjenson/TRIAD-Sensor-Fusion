#requires -Version 7.0

<#
.SYNOPSIS
Runs the guarded V5D Context Facade Lookdev R30 native successor transaction.

.DESCRIPTION
The default invocation and -StaticSelfCheck are repository-only and never
write the native project. -Execute is the sole write authority and additionally
requires caller-supplied receipts for the exact facade-predecessor map and both
native editor DLLs. The map must contain the exact validated R29 facade
predecessor. The live transaction promotes exactly seven isolated code files
and one hash-bound R30 source contract, builds with forced header generation,
creates exactly twelve isolated R30 facade-lookdev packages, and performs
one guarded map save through the R30 editor library.

The map endpoint prepares R30 fully hidden beside R29, removes R29, and activates
R30 only afterward. It retains the exact R29 facade mesh and R28 connective
public realm/outer ground, changing only eleven slot-name-bound materials.
Context-policy/current-surroundings, R29 terrain, tree realism, vegetation,
provider, geospatial, simulation, collision, sensor and RF surfaces are outside
the mutation closure and are validated before and after. Failure restores the
map, promoted files, build surfaces and initially absent R30 content without a
broad or unresolved recursive delete.

.EXAMPLE
.\Invoke-IstanaExploreV5DContextFacadeR30NativeTransactionV1.ps1 -RunToken review -StaticSelfCheck

.EXAMPLE
.\Invoke-IstanaExploreV5DContextFacadeR30NativeTransactionV1.ps1 -RunToken reviewed -Execute -RequireR29FacadePredecessor -ExpectedVegetationOwner R29 -ExpectedMapBytes <r29-bytes> -ExpectedMapSha256 <r29-sha256> -ExpectedRuntimeDllBytes <r29-bytes> -ExpectedRuntimeDllSha256 <r29-sha256> -ExpectedEditorDllBytes <r29-bytes> -ExpectedEditorDllSha256 <r29-sha256>
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,
    [switch] $StaticSelfCheck,
    [Alias('RequireR29Predecessor')]
    [switch] $RequireR29FacadePredecessor,

    [ValidateSet('R29')]
    [string] $ExpectedVegetationOwner = 'R29',

    [long] $ExpectedMapBytes = 0L,
    [string] $ExpectedMapSha256 = '',
    [long] $ExpectedRuntimeDllBytes = 0L,
    [string] $ExpectedRuntimeDllSha256 = '',
    [long] $ExpectedEditorDllBytes = 0L,
    [string] $ExpectedEditorDllSha256 = '',

    [ValidateRange(120, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$privateMemoryCeilingBytes = 12884901888L # fixed 12 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # fixed continuous 6 GiB
$memoryWatchdogPollMilliseconds = 500
$memoryWatchdogPersistentBreachMilliseconds = 2000
$expectedExitIdentityLeaseMilliseconds = 35000
$script:memoryWatchdog = $null
$script:ownedNativeProcessHandles = [Collections.Generic.List[object]]::new()
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'))
$dotnet = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'))
$unrealBuildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'))
$engineSourceRoot = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Source'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath('C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$r30ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'))
$r29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'))
$r28ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28'))
$vegetationR29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
$landmarkVegetationR28ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28'))
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$terrainR29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'))
$contextFacadeR25ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25'))
$contextTextureRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\Textures'))
$heroV2ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV2'))
$heroV3ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV3'))
$heroV4ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV4'))
$heroV5ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV5'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$facadeLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary'
$treeLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DTreeRealismEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

$sourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR30FacadeLookdevActor.h'
        Bytes = 7179L
        Sha256 = '4A4E5BDDD6D66BE5013BE534FE157435DC9FCF80FCB2C2154EE85BB7DB46320A'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR30FacadeLookdevActor.cpp'
        Bytes = 34159L
        Sha256 = '1AA6E641A018E8137499B658878EC5C228F68EFE73DF88E246C5A394BC390A1B'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR30FacadeLookdevActorTests.cpp'
        Bytes = 5126L
        Sha256 = '27B1D8463087551C9DF62A6DEB7D51A673864F8D1517B2F38FCF6C3E480C42E3'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h'
        Bytes = 861L
        Sha256 = 'A83E7B04DD3F2BB5AF26F735312F29B6905A3C031661B869D5ACF231B639F59F'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.cpp'
        Bytes = 71395L
        Sha256 = '9941F3CD6E263683E4416569DAC4D8F6F3E879FD03D5CF8BB3E56E5E2B229A9D'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.h'
        Bytes = 2481L
        Sha256 = 'B67D37B2A61D0FE3A973CA9B6D3291D256257DAE4B9E84C65338225C29CB8B48'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.cpp'
        Bytes = 43276L
        Sha256 = 'FEFA20211AFE55057F26AB42B64D9351A48F9645853F903E9BEE62BBEFC4D470'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTreeRealismActor.h'
        Bytes = 9942L
        Sha256 = '5B1959612F4DE2C6A0BF585BF329EA6ECA77F923651C6F8FEE61EE444F0B1E14'
        NativeBeforePresent = $true
        NativeBeforeBytes = 8739L
        NativeBeforeSha256 = '0C3E68F1ACFDAD3D298B98F9D583EAF72C70D0AFFF5BF7F15200E7F3CBC8DA3E'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTreeRealismActor.cpp'
        Bytes = 101549L
        Sha256 = 'AB6083398877A62D6847D049ADD333FE4165A2B96C085B2EA47ACA11567B087E'
        NativeBeforePresent = $true
        NativeBeforeBytes = 79575L
        NativeBeforeSha256 = 'BC1B9A4D808681161245EB28981FDBA68C18AA6574D21556A08F196C5910C3A0'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTreeRealismEditorLibrary.h'
        Bytes = 1552L
        Sha256 = 'CBD331068DFF3CE42DDF6A50806528FBCB024AC30E6FD0ACF81EC486B6AEFB0D'
        NativeBeforePresent = $true
        NativeBeforeBytes = 1232L
        NativeBeforeSha256 = 'CC4325D272A5871EABA0A2AD9028AF5CB039DF9160B8F01FC3363BA5D5EC2BAD'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp'
        Bytes = 83826L
        Sha256 = '91EDEA04EF4D6E83551E3F4735F6CA4461059787177F7EDB94399F398CE1090F'
        NativeBeforePresent = $true
        NativeBeforeBytes = 25311L
        NativeBeforeSha256 = 'A4C9A01D44293D67CD09C2D71D284E6E7BAED2EE4F6035E0B2048AD341ADA445'
    }
)

$sourceAssetPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_facade_lookdev.contract.json'
        Bytes = 8277L
        Sha256 = 'BD11A517E8AC3F6EDBE0F6FE0912409D11180AD4FFAACD91E684391CC2B19F03'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_tree_material_response_v3.source_closure.json'
        Bytes = 5171L
        Sha256 = '8DA0BCB32D4066036A985EF6F283E07F967B0AAD2C931C6A94E0C33925A82253'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\istana_public_view_v5d_tree_realism.material_response_amendment.v3.json'
        Bytes = 21801L
        Sha256 = '1E53495FAB2356127CD4D0FDCF7458010F8347001ADA4BCD01811A06E7C87428'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\render_tree_material_response_audit.py'
        Bytes = 20525L
        Sha256 = '644CB68C3674A6DEDC12BC1993A5670243E3F5705FDBA4E762EFB2B97BD37764'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\README.md'
        Bytes = 708L
        Sha256 = 'A46673AE5AB0080E8E3EDEE76A9C8892DF821FA80A6A0AE500418FBFCEF663FA'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\tree_material_response_audit.json'
        Bytes = 29417L
        Sha256 = 'E917A7D616EE29608091D15F62CFFA127142F4E4452D4B2BA69667F58CA72326'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\tree_material_response_contact_sheet.png'
        Bytes = 641245L
        Sha256 = '3180643CAD43D679047CD6DE4BCCF747E2A9BACC57F6ACDA14B2947D52DDAE69'
        NativeBeforePresent = $false
    }
)

$r30ContentRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\M_IPV5D_R30_ContextFacadePBR_Master.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_GlassCool.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_GlassWarm.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_GlassNeutral.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_FrameLight.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_FrameDark.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_FrameBronze.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_SillLight.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_SillDark.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_RoofTrim.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_Canopy.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30\Materials\MI_IPV5D_R30_BalconyRail.uasset'
)

$treeResponseMaterialRelativePaths = @(
    'Materials\M_IPV5D_Tree_Umbrella_Trunk_Response.uasset'
    'Materials\M_IPV5D_Tree_Umbrella_Leaves_Response.uasset'
    'Materials\M_IPV5D_Tree_Umbrella_Branches_Response.uasset'
    'Materials\M_IPV5D_Tree_Dome_Branches_Response.uasset'
    'Materials\M_IPV5D_Tree_Dome_Leaves_Response.uasset'
    'Materials\M_IPV5D_Tree_Dome_Trunk_Response.uasset'
    'Materials\M_IPV5D_Tree_HighFork_Trunk_Response.uasset'
    'Materials\M_IPV5D_Tree_HighFork_Leaves_Response.uasset'
    'Materials\M_IPV5D_Tree_HighFork_Branches_Response.uasset'
    'Materials\M_IPV5D_Tree_Columnar_Branches_Response.uasset'
    'Materials\M_IPV5D_Tree_Columnar_Trunk_Response.uasset'
    'Materials\M_IPV5D_Tree_Columnar_Leaves_Response.uasset'
    'Materials\M_IPV5D_Tree_Palm_Composite_Response.uasset'
)

$treeDerivativeMeshRelativePaths = @(
    'Meshes\SM_IPV5D_Tree_Umbrella_NearLOD0.uasset'
    'Meshes\SM_IPV5D_Tree_Dome_NearLOD0.uasset'
    'Meshes\SM_IPV5D_Tree_HighForkRounded_NearLOD0.uasset'
    'Meshes\SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.uasset'
    'Meshes\SM_IPV5D_Tree_Palm_NearLOD0.uasset'
)

function Get-FileState {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{
            Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState $Path
    if ($actual.Present -ne $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label state mismatch: $Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $fullPath.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Assert-Pins {
    param([object[]] $Pins, [string] $Root, [switch] $NativeAfter)
    foreach ($pin in $Pins) {
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Non-canonical promotion path: $($pin.RelativePath)"
        }
        $path = [IO.Path]::GetFullPath((Join-Path $Root $pin.RelativePath))
        if (-not (Test-ContainedPath $path $Root)) {
            throw "Promotion path escaped root: $path"
        }
        $expected = if ($NativeAfter) {
            [pscustomobject] @{ Present = $true; Bytes = [int64] $pin.Bytes; Sha256 = [string] $pin.Sha256 }
        }
        else {
            [pscustomobject] @{ Present = $true; Bytes = [int64] $pin.Bytes; Sha256 = [string] $pin.Sha256 }
        }
        [void] (Assert-State $expected $path 'pinned source')
    }
}

function Assert-NativePredecessorState {
    param([object[]] $Pins)
    foreach ($pin in $Pins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $pin.RelativePath))
        if ($pin.NativeBeforePresent) {
            if ([int64] $pin.NativeBeforeBytes -le 0 -or
                [string] $pin.NativeBeforeSha256 -notmatch '^[A-F0-9]{64}$') {
                throw "R30 replacement target lacks an exact native-preimage receipt: $path"
            }
            [void] (Assert-State ([pscustomobject] @{
                Present = $true
                Bytes = [int64] $pin.NativeBeforeBytes
                Sha256 = [string] $pin.NativeBeforeSha256
            }) $path 'R30 exact native predecessor')
        }
        elseif ([IO.File]::Exists($path)) {
            throw "R30 isolated native predecessor must be absent: $path"
        }
    }
}

function Get-TreeReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath = [IO.Path]::GetRelativePath($Root, $_.FullName)
                Bytes = [int64] $_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            }
        }
    )
}

function Assert-TreeReceipt {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual = @(Get-TreeReceipt $Root)
    $beforeJson = ConvertTo-Json @($Expected) -Depth 4 -Compress
    $afterJson = ConvertTo-Json @($actual) -Depth 4 -Compress
    if ($beforeJson -cne $afterJson) {
        throw "$Label immutable tree changed: $Root"
    }
}

function Assert-TreeRealismV3ContentDelta {
    param([object[]] $Before, [object[]] $After)
    $beforeByPath = @{}
    $afterByPath = @{}
    foreach ($row in @($Before)) { $beforeByPath[[string] $row.RelativePath] = $row }
    foreach ($row in @($After)) { $afterByPath[[string] $row.RelativePath] = $row }
    $allowed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in @($treeResponseMaterialRelativePaths) + @($treeDerivativeMeshRelativePaths)) {
        if (-not $allowed.Add([string] $relative)) {
            throw "Duplicate TreeRealism v3 content-delta path: $relative"
        }
    }
    foreach ($relative in $treeResponseMaterialRelativePaths) {
        if ($beforeByPath.ContainsKey($relative) -or -not $afterByPath.ContainsKey($relative)) {
            throw "TreeRealism v3 response material must be one newly created package: $relative"
        }
        $state = $afterByPath[$relative]
        if ([int64] $state.Bytes -le 0 -or [string] $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "TreeRealism v3 response material has no valid receipt: $relative"
        }
    }
    foreach ($relative in $treeDerivativeMeshRelativePaths) {
        if (-not $beforeByPath.ContainsKey($relative) -or -not $afterByPath.ContainsKey($relative)) {
            throw "TreeRealism v3 requires one exact pre-existing managed mesh package: $relative"
        }
        $beforeState = $beforeByPath[$relative]
        $afterState = $afterByPath[$relative]
        if ([string] $beforeState.Sha256 -ceq [string] $afterState.Sha256) {
            throw "TreeRealism v3 managed mesh did not receive its response-material binding: $relative"
        }
    }
    foreach ($relative in @($beforeByPath.Keys)) {
        if (-not $allowed.Contains($relative)) {
            if (-not $afterByPath.ContainsKey($relative) -or
                [int64] $beforeByPath[$relative].Bytes -ne [int64] $afterByPath[$relative].Bytes -or
                [string] $beforeByPath[$relative].Sha256 -cne [string] $afterByPath[$relative].Sha256) {
                throw "TreeRealism package outside the exact v3 response closure changed: $relative"
            }
        }
    }
    foreach ($relative in @($afterByPath.Keys)) {
        if (-not $beforeByPath.ContainsKey($relative) -and -not $allowed.Contains($relative)) {
            throw "Unexpected package was added to TreeRealism during the v3 response stage: $relative"
        }
    }
    if (@($After).Count -ne @($Before).Count + 13) {
        throw "TreeRealism v3 must add exactly thirteen response material packages."
    }
    [pscustomobject] [ordered] @{
        Status = 'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID'
        ResponseMaterialPackageCount = 13
        ReboundManagedMeshPackageCount = 5
        RuntimeResponseMidCount = 26
        Before = @($Before)
        After = @($After)
    }
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Journal target escaped native project: $path"
        }
        $state = Get-FileState $path
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $path)
        $backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $relative))
        if ($state.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $path -Destination $backup
            [void] (Assert-State $state $backup 'journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path = $path; RelativePath = $relative; Before = $state
            Backup = if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows)) {
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
            [void] (Assert-State $row.Before $row.Path 'restored file')
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
    }
}

function New-TreeJournal {
    param([string[]] $Roots, [string] $BackupRoot)
    $journals = [Collections.Generic.List[object]]::new()
    foreach ($root in $Roots) {
        $directories = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -Directory -Recurse | ForEach-Object FullName) + @($root)
        } else { @() }
        $files = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object FullName)
        } else { @() }
        $journals.Add([pscustomobject] [ordered] @{
            Root = $root
            Directories = @($directories | Sort-Object -Unique)
            Files = @(New-FileJournal $files $BackupRoot)
        })
    }
    @($journals)
}

function Restore-TreeJournal {
    param([object[]] $Journals)
    foreach ($journal in @($Journals)) {
        $beforeFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($row in @($journal.Files)) { [void] $beforeFiles.Add([string] $row.Path) }
        if ([IO.Directory]::Exists($journal.Root)) {
            foreach ($file in @(Get-ChildItem -LiteralPath $journal.Root -File -Recurse)) {
                if (-not $beforeFiles.Contains($file.FullName)) {
                    Remove-Item -LiteralPath $file.FullName -Force
                }
            }
        }
        Restore-FileJournal @($journal.Files)
        $beforeDirectories = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($directory in @($journal.Directories)) { [void] $beforeDirectories.Add([string] $directory) }
        if ([IO.Directory]::Exists($journal.Root)) {
            $currentDirectories = @(
                Get-ChildItem -LiteralPath $journal.Root -Directory -Recurse | ForEach-Object FullName
            ) + @($journal.Root)
            foreach ($directory in @($currentDirectories | Sort-Object Length -Descending)) {
                if (-not $beforeDirectories.Contains($directory) -and
                    @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
                    [IO.Directory]::Delete($directory, $false)
                }
            }
        }
    }
}

function Remove-IsolatedR30Content {
    if (-not [IO.Directory]::Exists($r30ContentRoot)) { return }
    if (-not (Test-ContainedPath $r30ContentRoot (Join-Path $nativeProjectRoot 'Content'))) {
        throw 'R30 content rollback root escaped native Content.'
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $r30ContentRoot -File -Recurse)) {
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $directories = @(Get-ChildItem -LiteralPath $r30ContentRoot -Directory -Recurse | ForEach-Object FullName) + @($r30ContentRoot)
    foreach ($directory in @($directories | Sort-Object Length -Descending)) {
        if (@(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
            [IO.Directory]::Delete($directory, $false)
        }
    }
}

function Get-ProtectedProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.ExecutablePath -and
            [IO.Path]::GetFullPath($_.ExecutablePath).Equals($protectedEditor, [StringComparison]::OrdinalIgnoreCase) -and
            $_.CommandLine -and $_.CommandLine.Contains($protectedProject, [StringComparison]::OrdinalIgnoreCase)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId = [uint32] $_.ProcessId
                CreationDate = [string] $_.CreationDate
                ExecutablePath = [IO.Path]::GetFullPath($_.ExecutablePath)
                CommandLine = [string] $_.CommandLine
            }
        }
    )
}

function Assert-ProtectedUnchanged {
    param([object[]] $Before)
    if ((ConvertTo-Json @($Before) -Compress) -cne
        (ConvertTo-Json @(Get-ProtectedProcesses) -Compress)) {
        throw 'Protected UE5.4 CAPSTONE process set changed.'
    }
}

function Get-NativeMutatorProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $isEngineEditor = $_.ExecutablePath -and
                [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith(
                    $engineRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
                $_.Name -like 'UnrealEditor*'
            $isExactProjectCommand = $_.CommandLine -and
                ($_.CommandLine.Contains(
                    $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -or
                 $_.CommandLine.Contains(
                    $nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase))
            $isBuildTool = $_.Name -in @(
                'dotnet.exe', 'UnrealBuildTool.exe',
                'UnrealHeaderTool.exe', 'MSBuild.exe',
                'cl.exe', 'link.exe', 'rc.exe',
                'ShaderCompileWorker.exe')
            $isExactProjectUbt = $isExactProjectCommand -and
                $_.CommandLine.Contains(
                    'UnrealBuildTool', [StringComparison]::OrdinalIgnoreCase)
            $isEngineEditor -or $isExactProjectUbt -or
                ($isExactProjectCommand -and $isBuildTool)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId = [uint32] $_.ProcessId
                ParentProcessId = [uint32] $_.ParentProcessId
                CreationDate = [string] $_.CreationDate
                Name = [string] $_.Name
                ExecutablePath = if ($_.ExecutablePath) {
                    [IO.Path]::GetFullPath($_.ExecutablePath)
                } else { '' }
                CommandLine = [string] $_.CommandLine
            }
        }
    )
}

function Register-OwnedNativeProcessHandle {
    param(
        [Parameter(Mandatory = $true)]
        [Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)]
        [string] $Label,
        [Parameter(Mandatory = $true)]
        [string] $ExpectedExecutablePath
    )

    $Handle.Refresh()
    if ($Handle.HasExited) {
        throw "Owned native process exited before registration: $Label"
    }
    $script:ownedNativeProcessHandles.Add(
        [pscustomobject] [ordered] @{
            Handle = $Handle
            ProcessId = [int] $Handle.Id
            StartUtcTicks = [int64] $Handle.StartTime.ToUniversalTime().Ticks
            ExpectedExecutablePath = [IO.Path]::GetFullPath(
                $ExpectedExecutablePath)
            Label = $Label
        })
}

function Get-ActiveOwnedNativeProcessHandles {
    @(
        foreach ($record in $script:ownedNativeProcessHandles) {
            $record.Handle.Refresh()
            if (-not $record.Handle.HasExited) { $record }
        }
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $busy = @(Get-NativeMutatorProcesses)
    if ($busy.Count -ne 0) {
        $summary = [string]::Join(
            '; ',
            @($busy | ForEach-Object {
                "pid=$($_.ProcessId) name=$($_.Name)"
            }))
        throw "$Label refused: UE5.5 editor/helper or exact-project build mutator is already present: $summary"
    }
}

function Wait-NativeMutationQuiescence {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Label,
        [ValidateRange(5, 120)]
        [int] $TimeoutSeconds = 60
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $busy = @(Get-NativeMutatorProcesses)
        $activeOwned = @(Get-ActiveOwnedNativeProcessHandles)
        if ($busy.Count -eq 0 -and $activeOwned.Count -eq 0) {
            return [pscustomobject] [ordered] @{
                Label = $Label
                Status = 'QUIESCENT'
                RegisteredOwnedProcessCount =
                    $script:ownedNativeProcessHandles.Count
                ActiveOwnedProcessCount = 0
                NativeMutatorProcessCount = 0
            }
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)

    $busy = @(Get-NativeMutatorProcesses)
    $activeOwned = @(Get-ActiveOwnedNativeProcessHandles)
    $summary = [string]::Join(
        '; ',
        @($busy | ForEach-Object {
            "mutatorPid=$($_.ProcessId) name=$($_.Name)"
        }) +
        @($activeOwned | ForEach-Object {
            "ownedPid=$($_.ProcessId) label=$($_.Label)"
        }))
    throw "$Label refused filesystem mutation because owned build/editor process trees are not quiescent: $summary"
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $freeBytes = [int64] $os.FreeVirtualMemory * 1024L
    if ($freeBytes -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label refused by fixed 10 GiB FreeVirtualMemory gate: available=$freeBytes"
    }
    [pscustomobject] [ordered] @{
        Label = $Label
        MinimumBytes = $minimumSystemFreeVirtualAtLaunchBytes
        ObservedBytes = $freeBytes
        Status = 'PASS'
    }
}

function Invoke-RcCall {
    param(
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters,
        [int] $TimeoutSeconds
    )
    $body = [ordered] @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
        generateTransaction = $false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' -Body $body -TimeoutSec $TimeoutSeconds
}

function Get-OwnedHelperIdentity {
    param([Diagnostics.Process] $Handle, [string] $Log)
    $record = Get-CimInstance Win32_Process -Filter "ProcessId=$($Handle.Id)" -ErrorAction Stop
    if ($null -eq $record -or -not $record.ExecutablePath -or
        -not [IO.Path]::GetFullPath($record.ExecutablePath).Equals($editor, [StringComparison]::OrdinalIgnoreCase) -or
        -not $record.CommandLine.Contains($mapPackage, [StringComparison]::Ordinal) -or
        -not $record.CommandLine.Contains($Log, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Could not prove exact launched UE5.5 helper identity.'
    }
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $record.ProcessId
        CreationDate = [string] $record.CreationDate
        ExecutablePath = [IO.Path]::GetFullPath($record.ExecutablePath)
        CommandLine = [string] $record.CommandLine
    }
}

function Stop-OwnedHelper {
    param(
        [Diagnostics.Process] $Handle,
        $Identity,
        [string] $Log
    )
    if ([uint32] $Handle.Id -ne [uint32] $Identity.ProcessId) {
        throw 'Process handle does not match exact owned helper.'
    }
    [void] (Get-OwnedHelperIdentity $Handle $Log)
    if ($null -ne $script:memoryWatchdog) {
        $script:memoryWatchdog.BeginExpectedExit(
            $expectedExitIdentityLeaseMilliseconds)
    }
    try { [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) } catch {}
    if (-not $Handle.WaitForExit($ShutdownTimeoutSeconds * 1000)) {
        [void] (Get-OwnedHelperIdentity $Handle $Log)
        # Forced containment is limited to the still-proven handle returned by
        # this wrapper. No process-name or broad Stop-Process action is used.
        $Handle.Kill()
        if (-not $Handle.WaitForExit(30000)) {
            throw 'Exact owned helper did not exit after bounded containment.'
        }
    }
}

function Initialize-ContinuousMemoryWatchdogType {
    if ($null -ne ('Triad.CesiumDiagnostics.ContinuousMemoryWatchdog' -as
            [type])) {
        return
    }

    # This monitor runs on its own CLR thread. That distinction is deliberate:
    # the main PowerShell runspace can be blocked in process discovery, CIM, or
    # an RC request while Unreal is still allocating. The watchdog therefore
    # covers the entire interval from immediately after Start-Process through
    # final teardown, rather than relying on cooperative sampling alone.
    $source = @'
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace Triad.CesiumDiagnostics
{
    public sealed class ContinuousMemoryWatchdogSnapshot
    {
        public bool Started { get; internal set; }
        public bool Stopped { get; internal set; }
        public long SampleCount { get; internal set; }
        public long PeakPrivateBytes { get; internal set; }
        public ulong MinimumAvailableCommitBytes { get; internal set; }
        public string LastSampleUtc { get; internal set; }
        public string AlertKind { get; internal set; }
        public string AlertObservedUtc { get; internal set; }
        public long AlertPrivateBytes { get; internal set; }
        public ulong AlertAvailableCommitBytes { get; internal set; }
        public bool ExactIdentityVerifiedAtAlert { get; internal set; }
        public bool ExactIdentityVerifiedAtContainment { get; internal set; }
        public bool ForceKillUsed { get; internal set; }
        public bool ContainmentRefused { get; internal set; }
        public string MonitorError { get; internal set; }
    }

    public sealed class ContinuousMemoryWatchdog : IDisposable
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        private sealed class MemoryStatusEx
        {
            public uint Length = (uint)Marshal.SizeOf(typeof(MemoryStatusEx));
            public uint MemoryLoad;
            public ulong TotalPhysical;
            public ulong AvailablePhysical;
            public ulong TotalPageFile;
            public ulong AvailablePageFile;
            public ulong TotalVirtual;
            public ulong AvailableVirtual;
            public ulong AvailableExtendedVirtual;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern bool GlobalMemoryStatusEx(
            [In, Out] MemoryStatusEx buffer);

        private readonly object gate = new object();
        private readonly int processId;
        private readonly long creationUtcTicks;
        private readonly string executablePath;
        private readonly long privateCeilingBytes;
        private readonly ulong minimumAvailableCommitBytes;
        private readonly int pollMilliseconds;
        private readonly int persistentBreachMilliseconds;
        private readonly ManualResetEvent stopEvent = new ManualResetEvent(false);
        private Thread thread;
        private bool started;
        private bool stopped;
        private long sampleCount;
        private long peakPrivateBytes;
        private ulong minimumObservedAvailableCommitBytes = ulong.MaxValue;
        private string lastSampleUtc = String.Empty;
        private string alertKind = String.Empty;
        private string alertObservedUtc = String.Empty;
        private long alertPrivateBytes;
        private ulong alertAvailableCommitBytes;
        private bool exactIdentityVerifiedAtAlert;
        private bool exactIdentityVerifiedAtContainment;
        private bool forceKillUsed;
        private bool containmentRefused;
        private string monitorError = String.Empty;
        private DateTime? continuousBreachStartedUtc;
        private bool expectedExit;
        private DateTime expectedExitIdentityLeaseDeadlineUtc =
            DateTime.MinValue;

        public ContinuousMemoryWatchdog(
            int processId,
            long creationUtcTicks,
            string executablePath,
            long privateCeilingBytes,
            long minimumAvailableCommitBytes,
            int pollMilliseconds,
            int persistentBreachMilliseconds)
        {
            this.processId = processId;
            this.creationUtcTicks = creationUtcTicks;
            this.executablePath = Path.GetFullPath(executablePath);
            this.privateCeilingBytes = privateCeilingBytes;
            this.minimumAvailableCommitBytes =
                checked((ulong)minimumAvailableCommitBytes);
            this.pollMilliseconds = pollMilliseconds;
            this.persistentBreachMilliseconds = persistentBreachMilliseconds;
        }

        public void Start()
        {
            lock (this.gate)
            {
                if (this.started)
                    throw new InvalidOperationException("Watchdog already started.");
                this.started = true;
                this.thread = new Thread(this.Run);
                this.thread.IsBackground = true;
                this.thread.Name = "TRIAD owned-process memory safety watchdog";
                this.thread.Start();
            }
        }

        public void BeginExpectedExit(int identityLeaseMilliseconds)
        {
            if (identityLeaseMilliseconds <= 0 ||
                identityLeaseMilliseconds > 60000)
            {
                throw new ArgumentOutOfRangeException(
                    "identityLeaseMilliseconds");
            }
            Process process;
            bool processExited;
            bool executablePathVerified;
            string failure;
            if (!this.TryGetExactProcess(
                    false,
                    out process,
                    out processExited,
                    out executablePathVerified,
                    out failure))
            {
                if (!processExited)
                {
                    throw new InvalidOperationException(
                        String.IsNullOrWhiteSpace(failure)
                            ? "Could not prove exact identity before expected exit."
                            : failure);
                }
            }
            else
            {
                using (process)
                {
                    if (!executablePathVerified)
                    {
                        throw new InvalidOperationException(
                            "Expected-exit lease requires full executable-path proof.");
                    }
                }
            }
            lock (this.gate)
            {
                this.expectedExit = true;
                this.expectedExitIdentityLeaseDeadlineUtc =
                    DateTime.UtcNow.AddMilliseconds(identityLeaseMilliseconds);
            }
        }

        private bool IsExpectedExitIdentityLeaseActive()
        {
            lock (this.gate)
                return this.expectedExit &&
                    DateTime.UtcNow <=
                        this.expectedExitIdentityLeaseDeadlineUtc;
        }

        private static bool IsConfirmedExited(Process candidate)
        {
            if (candidate == null)
                return false;
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                try
                {
                    candidate.Refresh();
                    if (candidate.HasExited)
                        return true;
                }
                catch (ArgumentException)
                {
                    return true;
                }
                catch (InvalidOperationException)
                {
                    return true;
                }
                Thread.Yield();
            }
            return false;
        }

        private bool TryGetExactProcess(
            bool allowExpectedExitIdentityLease,
            out Process process,
            out bool processExited,
            out bool executablePathVerified,
            out string failure)
        {
            process = null;
            processExited = false;
            executablePathVerified = false;
            failure = String.Empty;
            Process candidate = null;
            bool startIdentityMatched = false;
            try
            {
                candidate = Process.GetProcessById(this.processId);
                candidate.Refresh();
                if (candidate.HasExited)
                {
                    processExited = true;
                    return false;
                }
                long actualTicks = candidate.StartTime.ToUniversalTime().Ticks;
                if (actualTicks != this.creationUtcTicks)
                {
                    failure = "Exact owned-process identity changed while monitored.";
                    return false;
                }
                startIdentityMatched = true;
                ProcessModule mainModule = candidate.MainModule;
                if (mainModule == null ||
                    String.IsNullOrWhiteSpace(mainModule.FileName))
                {
                    // A graceful RC shutdown can begin between HasExited and
                    // MainModule.  A still-live process is accepted only under
                    // the short identity lease armed after a fresh, full
                    // PID/start/path proof immediately before QuitEditor.
                    if (IsConfirmedExited(candidate))
                    {
                        processExited = true;
                        return false;
                    }
                    if (allowExpectedExitIdentityLease &&
                        this.IsExpectedExitIdentityLeaseActive())
                    {
                        process = candidate;
                        candidate = null;
                        return true;
                    }
                    failure = "Exact owned-process executable lookup returned no path.";
                    return false;
                }
                string actualPath = Path.GetFullPath(mainModule.FileName);
                if (!String.Equals(actualPath, this.executablePath,
                        StringComparison.OrdinalIgnoreCase))
                {
                    failure = "Exact owned-process identity changed while monitored.";
                    return false;
                }
                executablePathVerified = true;
                process = candidate;
                candidate = null;
                return true;
            }
            catch (ArgumentException)
            {
                processExited = true;
                return false;
            }
            catch (InvalidOperationException ex)
            {
                if (IsConfirmedExited(candidate))
                {
                    processExited = true;
                    return false;
                }
                if (allowExpectedExitIdentityLease &&
                    this.IsExpectedExitIdentityLeaseActive() &&
                    startIdentityMatched &&
                    candidate != null)
                {
                    process = candidate;
                    candidate = null;
                    return true;
                }
                failure = "Exact owned-process lookup failed: " +
                    ex.GetType().Name;
                return false;
            }
            catch (System.ComponentModel.Win32Exception ex)
            {
                if (IsConfirmedExited(candidate))
                {
                    processExited = true;
                    return false;
                }
                if (allowExpectedExitIdentityLease &&
                    this.IsExpectedExitIdentityLeaseActive() &&
                    startIdentityMatched &&
                    candidate != null)
                {
                    process = candidate;
                    candidate = null;
                    return true;
                }
                failure = "Exact owned-process lookup failed: " +
                    ex.GetType().Name;
                return false;
            }
            catch (NotSupportedException ex)
            {
                failure = "Exact owned-process lookup failed: " +
                    ex.GetType().Name;
                return false;
            }
            finally
            {
                if (candidate != null)
                    candidate.Dispose();
            }
        }

        private void SetMonitorError(string value)
        {
            lock (this.gate)
            {
                if (String.IsNullOrEmpty(this.monitorError))
                    this.monitorError = value;
            }
        }

        private void Sample()
        {
            Process process;
            bool processExited;
            bool executablePathVerified;
            string failure;
            if (!this.TryGetExactProcess(
                    true,
                    out process,
                    out processExited,
                    out executablePathVerified,
                    out failure))
            {
                if (!processExited)
                {
                    this.SetMonitorError(String.IsNullOrWhiteSpace(failure)
                        ? "Exact owned-process lookup failed without a reason."
                        : failure);
                }
                return;
            }
            using (process)
            {
                MemoryStatusEx memory = new MemoryStatusEx();
                if (!GlobalMemoryStatusEx(memory))
                {
                    this.SetMonitorError(
                        "GlobalMemoryStatusEx failed with Win32 error " +
                        Marshal.GetLastWin32Error().ToString());
                    return;
                }
                long privateBytes;
                try
                {
                    process.Refresh();
                    if (process.HasExited)
                        return;
                    privateBytes = process.PrivateMemorySize64;
                }
                catch (InvalidOperationException ex)
                {
                    if (this.IsExpectedExitIdentityLeaseActive() &&
                        IsConfirmedExited(process))
                        return;
                    this.SetMonitorError(
                        "Exact owned-process memory sample failed: " +
                        ex.GetType().Name);
                    return;
                }
                catch (System.ComponentModel.Win32Exception ex)
                {
                    if (this.IsExpectedExitIdentityLeaseActive() &&
                        IsConfirmedExited(process))
                        return;
                    this.SetMonitorError(
                        "Exact owned-process memory sample failed: " +
                        ex.GetType().Name);
                    return;
                }
                ulong availableCommitBytes = memory.AvailablePageFile;
                DateTime now = DateTime.UtcNow;
                string kind = privateBytes >= this.privateCeilingBytes
                    ? "MEMORY_GUARD_PRIVATE_BYTES"
                    : availableCommitBytes < this.minimumAvailableCommitBytes
                        ? "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL"
                        : String.Empty;

                lock (this.gate)
                {
                    ++this.sampleCount;
                    if (privateBytes > this.peakPrivateBytes)
                        this.peakPrivateBytes = privateBytes;
                    if (availableCommitBytes <
                        this.minimumObservedAvailableCommitBytes)
                        this.minimumObservedAvailableCommitBytes =
                            availableCommitBytes;
                    this.lastSampleUtc = now.ToString("o");
                    if (!String.IsNullOrEmpty(kind))
                    {
                        if (String.IsNullOrEmpty(this.alertKind))
                        {
                            this.alertKind = kind;
                            this.alertObservedUtc = now.ToString("o");
                            this.alertPrivateBytes = privateBytes;
                            this.alertAvailableCommitBytes =
                                availableCommitBytes;
                            this.exactIdentityVerifiedAtAlert =
                                executablePathVerified;
                        }
                        if (!this.continuousBreachStartedUtc.HasValue)
                            this.continuousBreachStartedUtc = now;
                    }
                    else
                    {
                        this.continuousBreachStartedUtc = null;
                    }
                }

                DateTime? breachStarted;
                lock (this.gate)
                    breachStarted = this.continuousBreachStartedUtc;
                if (breachStarted.HasValue &&
                    (now - breachStarted.Value).TotalMilliseconds >=
                        this.persistentBreachMilliseconds)
                {
                    this.ContainExactProcess();
                }
            }
        }

        private void ContainExactProcess()
        {
            Process process;
            bool processExited;
            bool executablePathVerified;
            string failure;
            if (!this.TryGetExactProcess(
                    false,
                    out process,
                    out processExited,
                    out executablePathVerified,
                    out failure))
            {
                lock (this.gate)
                    this.containmentRefused = true;
                if (!processExited)
                {
                    this.SetMonitorError(String.IsNullOrWhiteSpace(failure)
                        ? "Exact owned-process containment lookup failed without a reason."
                        : failure);
                }
                return;
            }
            using (process)
            {
                lock (this.gate)
                    this.exactIdentityVerifiedAtContainment =
                        executablePathVerified;
                try
                {
                    process.Kill(true);
                    lock (this.gate)
                        this.forceKillUsed = true;
                }
                catch (Exception ex)
                {
                    this.SetMonitorError(
                        "Exact-process containment failed: " +
                        ex.GetType().Name);
                }
            }
        }

        private void Run()
        {
            try
            {
                while (!this.stopEvent.WaitOne(0))
                {
                    this.Sample();
                    if (this.stopEvent.WaitOne(this.pollMilliseconds))
                        break;
                }
            }
            catch (Exception ex)
            {
                this.SetMonitorError(
                    "Watchdog thread failed: " + ex.GetType().Name);
            }
            finally
            {
                lock (this.gate)
                    this.stopped = true;
            }
        }

        public ContinuousMemoryWatchdogSnapshot GetSnapshot()
        {
            lock (this.gate)
            {
                return new ContinuousMemoryWatchdogSnapshot
                {
                    Started = this.started,
                    Stopped = this.stopped,
                    SampleCount = this.sampleCount,
                    PeakPrivateBytes = this.peakPrivateBytes,
                    MinimumAvailableCommitBytes =
                        this.minimumObservedAvailableCommitBytes == ulong.MaxValue
                            ? 0UL
                            : this.minimumObservedAvailableCommitBytes,
                    LastSampleUtc = this.lastSampleUtc,
                    AlertKind = this.alertKind,
                    AlertObservedUtc = this.alertObservedUtc,
                    AlertPrivateBytes = this.alertPrivateBytes,
                    AlertAvailableCommitBytes =
                        this.alertAvailableCommitBytes,
                    ExactIdentityVerifiedAtAlert =
                        this.exactIdentityVerifiedAtAlert,
                    ExactIdentityVerifiedAtContainment =
                        this.exactIdentityVerifiedAtContainment,
                    ForceKillUsed = this.forceKillUsed,
                    ContainmentRefused = this.containmentRefused,
                    MonitorError = this.monitorError
                };
            }
        }

        public void Stop()
        {
            this.stopEvent.Set();
            Thread local;
            lock (this.gate)
                local = this.thread;
            if (local != null && local != Thread.CurrentThread)
                local.Join(5000);
        }

        public void Dispose()
        {
            this.Stop();
            this.stopEvent.Dispose();
        }
    }
}
'@
    Add-Type -TypeDefinition $source -Language CSharp -ErrorAction Stop
}

function Start-ContinuousMemoryWatchdog {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)]
        [string] $ExpectedExecutablePath,
        [Parameter(Mandatory = $true)]
        [string] $OwnedProcessLabel
    )

    Initialize-ContinuousMemoryWatchdogType
    $Handle.Refresh()
    if ($Handle.HasExited) {
        throw "Owned $OwnedProcessLabel exited before watchdog startup."
    }
    $watchdog =
        [Triad.CesiumDiagnostics.ContinuousMemoryWatchdog]::new(
            [int] $Handle.Id,
            [int64] $Handle.StartTime.ToUniversalTime().Ticks,
            [IO.Path]::GetFullPath($ExpectedExecutablePath),
            [int64] $privateMemoryCeilingBytes,
            [int64] $minimumSystemFreeVirtualBytes,
            [int] $memoryWatchdogPollMilliseconds,
            [int] $memoryWatchdogPersistentBreachMilliseconds)
    $watchdog.Start()
    $sampleDeadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $snapshot = $watchdog.GetSnapshot()
        if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) {
            $watchdog.Stop()
            $watchdog.Dispose()
            throw "MEMORY_WATCHDOG_FAILURE: process=$OwnedProcessLabel detail=$($snapshot.MonitorError)"
        }
        if ([int64] $snapshot.SampleCount -gt 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $sampleDeadline)
    if ([int64] $snapshot.SampleCount -le 0 -or
        [string]::IsNullOrWhiteSpace($snapshot.LastSampleUtc)) {
        $watchdog.Stop()
        $watchdog.Dispose()
        throw "MEMORY_WATCHDOG_NO_SAMPLES: process=$OwnedProcessLabel"
    }
    $watchdog
}

function Get-ContinuousMemoryWatchdogSnapshot {
    if ($null -eq $script:memoryWatchdog) {
        return $null
    }
    $script:memoryWatchdog.GetSnapshot()
}

function Test-ContinuousMemoryWatchdogMemoryAlert {
    param([AllowNull()] $Snapshot)

    $null -ne $Snapshot -and
        -not [string]::IsNullOrWhiteSpace($Snapshot.AlertKind) -and
        $Snapshot.AlertKind.StartsWith(
            'MEMORY_GUARD_', [StringComparison]::Ordinal)
}

function Assert-ContinuousMemoryWatchdogHealthy {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    $snapshot = Get-ContinuousMemoryWatchdogSnapshot
    if ($null -eq $snapshot) {
        throw "MEMORY_WATCHDOG_MISSING: checkpoint=$Checkpoint"
    }
    if (-not $snapshot.Started -or [int64] $snapshot.SampleCount -le 0 -or
        [string]::IsNullOrWhiteSpace($snapshot.LastSampleUtc)) {
        throw "MEMORY_WATCHDOG_NO_SAMPLES: checkpoint=$Checkpoint sampleCount=$($snapshot.SampleCount)"
    }
    $lastSampleUtc = [DateTime]::MinValue
    if (-not [DateTime]::TryParse(
            [string] $snapshot.LastSampleUtc,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind,
            [ref] $lastSampleUtc) -or
        ([DateTime]::UtcNow - $lastSampleUtc.ToUniversalTime()).TotalMilliseconds -gt
            [Math]::Max(5000, $memoryWatchdogPollMilliseconds * 6)) {
        throw "MEMORY_WATCHDOG_STALE_SAMPLE: checkpoint=$Checkpoint lastSampleUtc=$($snapshot.LastSampleUtc)"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) {
        throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($snapshot.MonitorError)"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.AlertKind)) {
        throw "$($snapshot.AlertKind): checkpoint=$Checkpoint source=continuous_watchdog privateBytes=$($snapshot.AlertPrivateBytes) freeVirtualBytes=$($snapshot.AlertAvailableCommitBytes)"
    }
}

function Assert-ContinuousMemoryWatchdogSnapshotComplete {
    param(
        [Parameter(Mandatory = $true)] $Snapshot,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    if (-not $Snapshot.Started -or -not $Snapshot.Stopped -or
        [int64] $Snapshot.SampleCount -le 0 -or
        [string]::IsNullOrWhiteSpace($Snapshot.LastSampleUtc)) {
        throw "MEMORY_WATCHDOG_INCOMPLETE: checkpoint=$Checkpoint started=$($Snapshot.Started) stopped=$($Snapshot.Stopped) sampleCount=$($Snapshot.SampleCount)"
    }
    $lastSampleUtc = [DateTime]::MinValue
    if (-not [DateTime]::TryParse(
            [string] $Snapshot.LastSampleUtc,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind,
            [ref] $lastSampleUtc) -or
        ([DateTime]::UtcNow - $lastSampleUtc.ToUniversalTime()).TotalMilliseconds -gt
            [Math]::Max(5000, $memoryWatchdogPollMilliseconds * 6)) {
        throw "MEMORY_WATCHDOG_STALE_SAMPLE: checkpoint=$Checkpoint lastSampleUtc=$($Snapshot.LastSampleUtc)"
    }
    if (-not [string]::IsNullOrWhiteSpace($Snapshot.MonitorError)) {
        throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($Snapshot.MonitorError)"
    }
    if (-not [string]::IsNullOrWhiteSpace($Snapshot.AlertKind)) {
        throw "$($Snapshot.AlertKind): checkpoint=$Checkpoint source=continuous_watchdog privateBytes=$($Snapshot.AlertPrivateBytes) freeVirtualBytes=$($Snapshot.AlertAvailableCommitBytes)"
    }
}

function Stop-ContinuousMemoryWatchdog {
    if ($null -eq $script:memoryWatchdog) {
        return $null
    }
    $script:memoryWatchdog.Stop()
    $snapshot = $script:memoryWatchdog.GetSnapshot()
    $script:memoryWatchdog.Dispose()
    $script:memoryWatchdog = $null
    $snapshot
}

function Invoke-GuardedOwnedBuild {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments,
        [Parameter(Mandatory = $true)]
        [string] $StandardOutputLog,
        [Parameter(Mandatory = $true)]
        [string] $StandardErrorLog
    )

    Assert-NativeIdle 'before guarded owned UBT build'
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission 'before guarded owned UBT build')
    foreach ($logPath in @($StandardOutputLog, $StandardErrorLog)) {
        if ([IO.File]::Exists($logPath)) {
            throw "Guarded owned UBT build log already exists: $logPath"
        }
    }
    $escapedArguments = @(
        ('"{0}"' -f $unrealBuildTool)
        $Arguments | ForEach-Object {
            if ($_ -match '[\s"]') {
                '"{0}"' -f $_.Replace('"', '\"')
            }
            else { $_ }
        }
    )
    $argumentLine = [string]::Join(' ', $escapedArguments)
    $handle = Start-Process `
        -FilePath $dotnet `
        -ArgumentList $argumentLine `
        -WorkingDirectory $engineSourceRoot `
        -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog `
        -PassThru `
        -WindowStyle Hidden
    Register-OwnedNativeProcessHandle `
        -Handle $handle `
        -Label 'UnrealBuildTool dotnet host' `
        -ExpectedExecutablePath $dotnet
    $ownedStartTicks = [int64] $handle.StartTime.ToUniversalTime().Ticks
    $buildError = $null
    $memorySnapshot = $null
    $exitCode = $null
    try {
        $script:memoryWatchdog = Start-ContinuousMemoryWatchdog `
            -Handle $handle `
            -ExpectedExecutablePath $dotnet `
            -OwnedProcessLabel 'UnrealBuildTool dotnet host'
        do {
            Assert-ContinuousMemoryWatchdogHealthy `
                -Checkpoint 'guarded owned UBT build'
        } while (-not $handle.WaitForExit($memoryWatchdogPollMilliseconds))
        Assert-ContinuousMemoryWatchdogHealthy `
            -Checkpoint 'guarded owned UBT build process exit'
        $exitCode = $handle.ExitCode
        if ($exitCode -ne 0) {
            throw "R30 facade lookdev build failed: exit=$exitCode"
        }
    }
    catch {
        $buildError = $_.Exception
    }
    finally {
        if (-not $handle.HasExited) {
            try {
                $handle.Refresh()
                $actualExecutable = [IO.Path]::GetFullPath(
                    $handle.MainModule.FileName)
                if ([int64] $handle.StartTime.ToUniversalTime().Ticks -ne
                        $ownedStartTicks -or
                    -not $actualExecutable.Equals(
                        $dotnet, [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Owned UBT containment refused because exact identity could not be re-proven.'
                }
                $handle.Kill($true)
                if (-not $handle.WaitForExit(30000)) {
                    throw 'Exact owned UBT process tree did not exit after bounded containment.'
                }
            }
            catch {
                if ($null -eq $buildError) { $buildError = $_.Exception }
                else {
                    $buildError = [InvalidOperationException]::new(
                        "build={$($buildError.Message)} containment={$($_.Exception.Message)}")
                }
            }
        }
        try {
            $memorySnapshot = Stop-ContinuousMemoryWatchdog
            if ($null -eq $memorySnapshot) {
                throw 'MEMORY_WATCHDOG_MISSING: checkpoint=guarded owned UBT final snapshot'
            }
            Assert-ContinuousMemoryWatchdogSnapshotComplete `
                -Snapshot $memorySnapshot `
                -Checkpoint 'guarded owned UBT final snapshot'
        }
        catch {
            if ($null -eq $buildError) { $buildError = $_.Exception }
            else {
                $buildError = [InvalidOperationException]::new(
                    "build={$($buildError.Message)} memoryGuard={$($_.Exception.Message)}")
            }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'after guarded owned UBT build'
    if ($null -ne $buildError) { throw $buildError }
    if (-not [IO.File]::Exists($StandardOutputLog) -or
        (Get-Item -LiteralPath $StandardOutputLog).Length -le 0) {
        throw 'Guarded owned UBT build did not persist a non-empty stdout log.'
    }
    [pscustomobject] [ordered] @{
        ProcessId = [int] $handle.Id
        ExecutablePath = $dotnet
        UnrealBuildTool = $unrealBuildTool
        ExitCode = [int] $exitCode
        ContinuousMemoryGuard = $memorySnapshot
        StandardOutputLog = Get-FileState $StandardOutputLog
        StandardErrorLog = Get-FileState $StandardErrorLog
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix,
        [hashtable] $Parameters = @{},
        [string] $Library = $facadeLibrary
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission "before helper $Stage")
    $logRoot = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logRoot)
    $log = [IO.Path]::GetFullPath((Join-Path $logRoot "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine = "`"$nativeProjectFile`" $mapPackage -d3d12 -sm6 " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    Register-OwnedNativeProcessHandle `
        -Handle $handle `
        -Label "UE5.5 helper $Stage" `
        -ExpectedExecutablePath $editor
    $identity = $null
    $response = $null
    $stageError = $null
    $memorySnapshot = $null
    try {
        $script:memoryWatchdog = Start-ContinuousMemoryWatchdog `
            -Handle $handle `
            -ExpectedExecutablePath $editor `
            -OwnedProcessLabel "UE5.5 helper $Stage"
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage watchdog startup"
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage identity wait"
            try { $identity = Get-OwnedHelperIdentity $handle $log } catch { $identity = $null }
            if ($null -ne $identity) { break }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }
        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        do {
            Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage RC readiness wait"
            try {
                $ready = Invoke-RcCall $identityLibrary 'ValidateIstanaExploreRemoteControlProject' @{ ExpectedProjectPath = $nativeProjectRoot } 30
            } catch { $ready = $null }
            if ($null -ne $ready -and $ready.ReturnValue -eq $true) { break }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $ready -or $ready.ReturnValue -ne $true) {
            throw "Remote-control identity did not become ready for $Stage"
        }
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage before endpoint"
        [void] (Get-OwnedHelperIdentity $handle $log)
        $response = Invoke-RcCall $Library $FunctionName $Parameters $EditorTimeoutSeconds
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage after endpoint"
        $text = [string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or
            -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: response=$text"
        }
    } catch { $stageError = $_.Exception }
    finally {
        if ($null -eq $identity -and -not $handle.HasExited) {
            try { $identity = Get-OwnedHelperIdentity $handle $log }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} identityCleanup={$($_.Exception.Message)}") }
            }
        }
        if ($null -ne $identity -and -not $handle.HasExited) {
            $teardownGuardError = $null
            try {
                Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage before helper teardown"
            }
            catch {
                # A breached guard must still allow the exact owned helper to
                # receive graceful shutdown (and bounded containment if
                # required).  Deferring this error until after teardown keeps
                # rollback fail-closed without orphaning the mutator that
                # caused the guard failure.
                $teardownGuardError = $_.Exception
            }
            try {
                Stop-OwnedHelper $handle $identity $log
            }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} cleanup={$($_.Exception.Message)}") }
            }
            if ($null -ne $teardownGuardError) {
                if ($null -eq $stageError) { $stageError = $teardownGuardError }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} memoryGuardBeforeTeardown={$($teardownGuardError.Message)}") }
            }
        }
        try {
            $memorySnapshot = Stop-ContinuousMemoryWatchdog
            if ($null -eq $memorySnapshot) {
                throw "MEMORY_WATCHDOG_MISSING: checkpoint=$Stage final snapshot"
            }
            Assert-ContinuousMemoryWatchdogSnapshotComplete `
                -Snapshot $memorySnapshot `
                -Checkpoint "$Stage final snapshot"
        } catch {
            if ($null -eq $stageError) { $stageError = $_.Exception }
            else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} memoryGuard={$($_.Exception.Message)}") }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle "after $Stage"
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item -LiteralPath $log).Length -le 0) {
        throw "Stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        Function = $FunctionName
        ExpectedPrefix = $ExpectedPrefix
        Message = [string] $response.$TextProperty
        ProcessIdentity = $identity
        ContinuousMemoryGuard = $memorySnapshot
        Log = Get-FileState $log
    }
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 11 -or $sourceAssetPins.Count -ne 7 -or
        $r30ContentRelativePaths.Count -ne 12 -or
        $treeResponseMaterialRelativePaths.Count -ne 13 -or
        $treeDerivativeMeshRelativePaths.Count -ne 5) {
        throw 'R30 closure must be exactly 7 facade code + 4 TreeRealism v3 code + 7 source/evidence files + 12 facade packages + 13 tree response packages + 5 rebound managed tree meshes.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        if (-not $seen.Add([string] $pin.RelativePath)) {
            throw "Duplicate R30 facade lookdev promotion path: $($pin.RelativePath)"
        }
        $isTreeSource = $pin.RelativePath.Contains(
            'TRIADIstanaExploreV5DTreeRealism', [StringComparison]::Ordinal)
        if ($pin.NativeBeforePresent -ne $isTreeSource) {
            throw "Only the exact four TreeRealism v3 code destinations may replace native R29 source: $($pin.RelativePath)"
        }
        if ($pin.NativeBeforePresent -and
            ([int64] $pin.NativeBeforeBytes -le 0 -or
             [string] $pin.NativeBeforeSha256 -notmatch '^[A-F0-9]{64}$')) {
            throw "TreeRealism v3 source replacement lacks an exact native preimage: $($pin.RelativePath)"
        }
        if ($pin.RelativePath.Contains('R29Vegetation', [StringComparison]::Ordinal) -or
            $pin.RelativePath.Contains('LandmarkVegetation', [StringComparison]::Ordinal)) {
            throw "Vegetation source escaped into R30 facade lookdev mutation closure: $($pin.RelativePath)"
        }
    }
    Assert-Pins $sourcePins $repositoryUnrealRoot
    Assert-Pins $sourceAssetPins $repositoryUnrealRoot
    $actor = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[1].RelativePath) -Raw
    $factory = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[4].RelativePath) -Raw
    $editorHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[5].RelativePath) -Raw
    $editorSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[6].RelativePath) -Raw
    $treeActorHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[7].RelativePath) -Raw
    $treeActorSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[8].RelativePath) -Raw
    $treeEditorHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[9].RelativePath) -Raw
    $treeEditorSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[10].RelativePath) -Raw
    $treeClosure = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourceAssetPins[1].RelativePath) -Raw | ConvertFrom-Json -Depth 32
    if ([string] $treeClosure.schema -cne
            'triad.istana_explore_v5d.r30_tree_material_response_v3.source_closure.v1' -or
        [string] $treeClosure.nativeOrder -cne
            'R30_COMMIT_AND_CAPTURE_THEN_R31_COMMIT_AND_CAPTURE_THEN_R32_COMMIT_AND_CAPTURE_THEN_R33_COMMIT_AND_CAPTURE' -or
        [bool] $treeClosure.newStageCreated -ne $false -or
        [bool] $treeClosure.r34Created -ne $false -or
        [int] $treeClosure.runtimeResponse.isolatedResponseMaterials -ne 13 -or
        [int] $treeClosure.runtimeResponse.runtimeResponseMids -ne 26 -or
        @($treeClosure.nativeSourcePromotion).Count -ne 4 -or
        @($treeClosure.sourceEvidence).Count -ne 5 -or
        [bool] $treeClosure.preservation.geographyModified -ne $false -or
        [bool] $treeClosure.preservation.collisionModified -ne $false -or
        [bool] $treeClosure.preservation.rfAuthorityModified -ne $false -or
        [bool] $treeClosure.preservation.sensorAuthorityModified -ne $false) {
        throw 'R30 TreeRealism v3 source-closure contract truth drifted.'
    }
    foreach ($marker in @(
        'ConfigurePreparedR30FacadeLookdevHandoff',
        'ActivateAfterR29FacadeRemoval',
        'SynchronizeOwnedVisibilityWithContextPolicy',
        'bProviderReadinessObservedWithoutPolicyMutation',
        'bR28OrR29ArchitectureRetainedOrCoRendered',
        'collision=false', 'simulationAuthority=false',
        'sensorAuthority=false', 'rfAuthority=false')) {
        if (-not $actor.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R30 facade lookdev actor marker missing: $marker"
        }
    }
    foreach ($marker in @(
        'RuntimeTreeResponseMaterialCount = TreeMaterialCount * 2',
        'ValidateRuntimeTreeMaterialResponse',
        'isolatedResponseMaterials=13 responseMids=26',
        'runtimeResponseMids=26',
        'opacityMaskAndClipPreserved=true',
        'windWpoCodePreserved=true')) {
        if (-not ($treeActorHeader.Contains($marker, [StringComparison]::Ordinal) -or
                  $treeActorSource.Contains($marker, [StringComparison]::Ordinal))) {
            throw "TreeRealism v3 runtime source marker missing: $marker"
        }
    }
    foreach ($marker in @(
        'EnsureTreeMaterialResponseAssets',
        'EnsureTreeCanopyRealismMeshAssets',
        'EnsureTreeCanopyRealismMeshAsset',
        'TreeMaterialCount = 13',
        'UE_ARRAY_COUNT(TreeResponseSpecs) == TreeMaterialCount',
        'isolatedResponseMaterials=13',
        'opacityAndWindWpoExact=true')) {
        if (-not ($treeEditorHeader.Contains($marker, [StringComparison]::Ordinal) -or
                  $treeEditorSource.Contains($marker, [StringComparison]::Ordinal))) {
            throw "TreeRealism v3 editor source marker missing: $marker"
        }
    }
    foreach ($receipt in @(
        'BD11A517E8AC3F6EDBE0F6FE0912409D11180AD4FFAACD91E684391CC2B19F03',
        'ExpectedMasterExpressionCount = 52',
        'textureBackedOverrides=8',
        'proceduralOpaqueGlassOverrides=3',
        'namedStaticBoolParameters=1',
        'plainStaticSwitches=5',
        'duplicateProneStaticSwitchParameterNodes=0',
        'exactStaticBranchTopology=true',
        'uniqueImmutableTextureDependencies=12',
        'HeroMaterials')) {
        if (-not $factory.Contains($receipt, [StringComparison]::Ordinal)) {
            throw "R30 facade lookdev source/topology receipt missing: $receipt"
        }
    }
    foreach ($endpoint in @(
        'EnsureR30FacadeLookdevAssets',
        'ValidateR30FacadeLookdevAssets',
        'ApplyR30FacadeLookdevReplacementToLoadedV5DHybridMap',
        'CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap',
        'ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap')) {
        if (-not $editorHeader.Contains($endpoint, [StringComparison]::Ordinal) -or
            -not $editorSource.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "R30 facade lookdev editor endpoint missing: $endpoint"
        }
    }
    foreach ($marker in @(
        'ValidateComposableVegetationOwner',
        'ValidateLandmarkVegetationR28',
        'ValidateR29Vegetation',
        'Composable vegetation owner requires exact R28 xor R29',
        'vegetationOwnerMode=EXACT_R29',
        'ValidateContextPolicyShell',
        'ValidateCopernicusTerrainFallback',
        'ValidateTreeRealism',
        'UndoTransaction(false)',
        'packageDirtyStateRestored=true',
        'vegetationActorsOrAssetsModified=false')) {
        if (-not $editorSource.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R30 facade lookdev composable vegetation-owner marker missing: $marker"
        }
    }
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'STATIC_SELF_CHECK_PASS'
        CodeSourceCount = $sourcePins.Count
        FacadeCodeSourceCount = 7
        TreeMaterialResponseV3CodeSourceCount = 4
        SourceEvidenceCount = $sourceAssetPins.Count
        SourceContractCount = 3
        TreeAuditEvidenceCount = 4
        NewFacadeContentPackageCount = $r30ContentRelativePaths.Count
        NewTreeResponseMaterialPackageCount = $treeResponseMaterialRelativePaths.Count
        ReboundTreeDerivativeMeshPackageCount = $treeDerivativeMeshRelativePaths.Count
        RuntimeTreeResponseMidCount = 26
        NativeTreeWritten = $false
        UnrealBuildOrEditorLaunched = $false
        R29FacadeMeshRetained = $true
        R28PublicRealmRetained = $true
        R29EnvironmentConcurrentRenderingAllowed = $false
        R29VegetationMutationAllowed = $false
        TreePlacementOrGeometryMutationAllowed = $false
        TreeMaterialResponseV3PromotionPrepared = $true
        TerrainMutationAllowed = $false
        ProviderPolicyMutationAllowed = $false
        SimulationSensorRfAuthority = $false
    }
}

$staticReceipt = Assert-StaticContract
if ($StaticSelfCheck) {
    $staticReceipt | ConvertTo-Json -Depth 8
    exit 0
}
if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'READ_ONLY_REPOSITORY_PREFLIGHT_PASS'
        ExecuteRequiredForNativeWrites = $true
        ExplicitPredecessorAndDllReceiptsRequired = $true
        NativeTreeWritten = $false
        UnrealBuildOrEditorLaunched = $false
        StaticReceipt = $staticReceipt
    } | ConvertTo-Json -Depth 8
    exit 0
}

foreach ($name in @(
    'ExpectedVegetationOwner',
    'ExpectedMapBytes', 'ExpectedMapSha256',
    'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
    'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256')) {
    if (-not $PSBoundParameters.ContainsKey($name)) {
        throw "Live R30 facade lookdev execution requires explicit caller-supplied parameter: $name"
    }
}
if (-not $RequireR29FacadePredecessor) {
    throw 'Live R30 facade lookdev execution requires -RequireR29FacadePredecessor (alias: -RequireR29Predecessor).'
}
foreach ($pair in @(
    @($ExpectedMapBytes, $ExpectedMapSha256),
    @($ExpectedRuntimeDllBytes, $ExpectedRuntimeDllSha256),
    @($ExpectedEditorDllBytes, $ExpectedEditorDllSha256))) {
    if ([int64] $pair[0] -le 0 -or [string] $pair[1] -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'Live R30 facade lookdev receipts require positive bytes and exact SHA-256.'
    }
}
foreach ($required in @(
    $nativeProjectFile, $buildTool, $dotnet, $unrealBuildTool, $editor,
    $mapFile, $runtimeDll, $editorDll)) {
    if (-not [IO.File]::Exists($required)) { throw "Required native file missing: $required" }
}
if ([IO.Directory]::Exists($transactionRoot)) {
    throw "Transaction run token already exists: $transactionRoot"
}
if ([IO.Directory]::Exists($r30ContentRoot)) {
    throw "Initially absent R30 facade lookdev content root already exists: $r30ContentRoot"
}
Assert-NativePredecessorState $sourcePins
Assert-NativePredecessorState $sourceAssetPins
foreach ($relative in $treeResponseMaterialRelativePaths) {
    $path = [IO.Path]::GetFullPath((Join-Path $treeRealismContentRoot $relative))
    if ([IO.File]::Exists($path)) {
        throw "Initially absent TreeRealism v3 response material already exists: $path"
    }
}
$expectedMap = [pscustomobject] @{ Present = $true; Bytes = $ExpectedMapBytes; Sha256 = $ExpectedMapSha256.ToUpperInvariant() }
$expectedRuntime = [pscustomobject] @{ Present = $true; Bytes = $ExpectedRuntimeDllBytes; Sha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant() }
$expectedEditor = [pscustomobject] @{ Present = $true; Bytes = $ExpectedEditorDllBytes; Sha256 = $ExpectedEditorDllSha256.ToUpperInvariant() }
[void] (Assert-State $expectedMap $mapFile 'explicit facade-predecessor map')
[void] (Assert-State $expectedRuntime $runtimeDll 'runtime DLL predecessor')
[void] (Assert-State $expectedEditor $editorDll 'editor DLL predecessor')
$script:protectedBefore = @(Get-ProtectedProcesses)
Assert-NativeIdle 'prewrite boundary'
Assert-ProtectedUnchanged $script:protectedBefore
$prewriteAdmission = Assert-LaunchAdmission 'before first native write'

$stageResults = [Collections.Generic.List[object]]::new()
$committed = $false
$failure = $null
$transactionStarted = $false
$sourceJournal = @()
$sourceAssetJournal = @()
$mapJournal = @()
$buildJournal = @()
$treeRealismJournal = @()
$backupMap = ''
try {
    [void] [IO.Directory]::CreateDirectory($transactionRoot)
    $transactionStarted = $true
    $journalRoot = Join-Path $transactionRoot 'rollback'
    $sourceDestinations = @($sourcePins | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath)) })
    $sourceAssetDestinations = @($sourceAssetPins | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath)) })
    $sourceJournal = @(New-FileJournal $sourceDestinations (Join-Path $journalRoot 'source'))
    $sourceAssetJournal = @(New-FileJournal $sourceAssetDestinations (Join-Path $journalRoot 'source-assets'))
    $mapJournal = @(New-FileJournal @($mapFile) (Join-Path $journalRoot 'map'))
    $buildJournal = @(New-TreeJournal @($pluginBinaryRoot, $pluginIntermediateRoot) (Join-Path $journalRoot 'build'))
    $treeRealismJournal = @(New-TreeJournal @($treeRealismContentRoot) (Join-Path $journalRoot 'tree-realism'))
    $immutableBefore = [ordered] @{
        R29FacadeEnvironment = @(Get-TreeReceipt $r29ContentRoot)
        R28Environment = @(Get-TreeReceipt $r28ContentRoot)
        VegetationR29 = @(Get-TreeReceipt $vegetationR29ContentRoot)
        LandmarkVegetationR28 = @(Get-TreeReceipt $landmarkVegetationR28ContentRoot)
        TreeRealism = @(Get-TreeReceipt $treeRealismContentRoot)
        TerrainR29 = @(Get-TreeReceipt $terrainR29ContentRoot)
        ContextFacadeR25 = @(Get-TreeReceipt $contextFacadeR25ContentRoot)
        ContextTextures = @(Get-TreeReceipt $contextTextureRoot)
        HeroMaterialsV2 = @(Get-TreeReceipt $heroV2ContentRoot)
        HeroMaterialsV3 = @(Get-TreeReceipt $heroV3ContentRoot)
        HeroMaterialsV4 = @(Get-TreeReceipt $heroV4ContentRoot)
        HeroMaterialsV5 = @(Get-TreeReceipt $heroV5ContentRoot)
    }
    $backupMap = [IO.Path]::GetFullPath((Join-Path $transactionRoot 'Istana_PublicView_Explore_v5d_hybrid.facade-predecessor.umap'))
    Copy-Item -LiteralPath $mapFile -Destination $backupMap
    [void] (Assert-State $expectedMap $backupMap 'external map backup')

    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        $source = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot $pin.RelativePath))
        $destination = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $pin.RelativePath))
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
    Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter
    Assert-Pins $sourceAssetPins $nativeProjectRoot -NativeAfter
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before build'
    $buildAdmission = Assert-LaunchAdmission 'before forced R30 facade lookdev build'
    $buildLog = Join-Path $transactionRoot 'build.stdout.log'
    $buildErrorLog = Join-Path $transactionRoot 'build.stderr.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development',
        "-Project=$nativeProjectFile", '-WaitMutex', '-NoHotReloadFromIDE',
        '-Module=TRIADSensorFusion', '-Module=TRIADSensorFusionEditor',
        '-ForceHeaderGeneration', '-NoUBTMakefiles',
        '-MaxParallelActions=1',
        '-NoUBA', '-NoUBALocal'
    )
    $buildGuard = Invoke-GuardedOwnedBuild `
        -Arguments $buildArguments `
        -StandardOutputLog $buildLog `
        -StandardErrorLog $buildErrorLog
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Sha256 -ceq $expectedRuntime.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditor.Sha256) {
        throw 'Forced build did not produce fresh runtime and editor DLL receipts.'
    }
    $runtimeBytes = [IO.File]::ReadAllBytes($runtimeDll)
    $editorBytes = [IO.File]::ReadAllBytes($editorDll)
    $runtimeText = [Text.Encoding]::ASCII.GetString($runtimeBytes) + [Text.Encoding]::Unicode.GetString($runtimeBytes)
    $editorText = [Text.Encoding]::ASCII.GetString($editorBytes) + [Text.Encoding]::Unicode.GetString($editorBytes)
    foreach ($marker in @('ATRIADIstanaExploreV5DR30FacadeLookdevActor', 'ActivateAfterR29FacadeRemoval', 'SynchronizeOwnedVisibilityWithContextPolicy', 'isolatedResponseMaterials=13 responseMids=26')) {
        if (-not $runtimeText.Contains($marker, [StringComparison]::Ordinal)) { throw "Runtime DLL lacks R30 facade lookdev marker: $marker" }
    }
    foreach ($marker in @('CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap', 'ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap', 'EnsureTreeMaterialResponseAssets', 'EnsureTreeCanopyRealismMeshAssets', 'EnsureTreeCanopyRealismMeshAsset', 'isolatedResponseMaterials=13')) {
        if (-not $editorText.Contains($marker, [StringComparison]::Ordinal)) { throw "Editor DLL lacks R30 facade lookdev marker: $marker" }
    }

    $stageResults.Add((Invoke-ColdStage `
        -Stage '01_ensure_tree_material_response_v3_assets' `
        -FunctionName 'EnsureTreeMaterialResponseAssets' `
        -TextProperty 'OutReport' `
        -ExpectedPrefix 'V5D_TREE_RESPONSE_MATERIAL_ASSETS_VALID' `
        -Library $treeLibrary))
    $treeFormLabels = @('umbrella', 'dome', 'high_fork', 'columnar', 'palm')
    for ($treeFormIndex = 0; $treeFormIndex -lt $treeFormLabels.Count; $treeFormIndex++) {
        $stageResults.Add((Invoke-ColdStage `
            -Stage "01b_ensure_tree_mesh_response_v3_$($treeFormLabels[$treeFormIndex])" `
            -FunctionName 'EnsureTreeCanopyRealismMeshAsset' `
            -TextProperty 'OutReport' `
            -ExpectedPrefix 'V5D_TREE_REALISM_MESH_ASSET_VALID' `
            -Parameters @{ FormIndex = [int] $treeFormIndex } `
            -Library $treeLibrary))
    }
    $treeRealismAfterEnsure = @(Get-TreeReceipt $treeRealismContentRoot)
    $treeMaterialResponseV3 = Assert-TreeRealismV3ContentDelta `
        -Before @($immutableBefore.TreeRealism) `
        -After $treeRealismAfterEnsure

    $stageResults.Add((Invoke-ColdStage '02_ensure_r30_facade_lookdev_assets' 'EnsureR30FacadeLookdevAssets' 'OutMessage' 'EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_PASS'))
    foreach ($relative in $r30ContentRelativePaths) {
        $state = Get-FileState ([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
        if (-not $state.Present -or $state.Bytes -le 0 -or $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "R30 facade lookdev content package missing after asset stage: $relative"
        }
    }
    if (@(Get-ChildItem -LiteralPath $r30ContentRoot -File -Recurse).Count -ne 12) {
        throw 'R30 facade lookdev content root is not the exact twelve-file namespace.'
    }
    $stageResults.Add((Invoke-ColdStage '03_validate_r30_facade_lookdev_assets' 'ValidateR30FacadeLookdevAssets' 'OutReport' 'ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSETS_VALID'))
    $stageResults.Add((Invoke-ColdStage '04_commit_r30_facade_lookdev_replacement' 'CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap' 'OutReport' 'ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_PASS' @{
        ExpectedPredecessorBytes = [int64] $ExpectedMapBytes
        ExpectedPredecessorSha256 = $ExpectedMapSha256.ToUpperInvariant()
        ExpectedVegetationOwner = $ExpectedVegetationOwner.ToUpperInvariant()
        VerifiedExternalBackupFilename = $backupMap
    }))
    $stageResults.Add((Invoke-ColdStage '05_cold_validate_r30_facade_lookdev_replacement' 'ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap' 'OutReport' 'ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_REPLACEMENT_MAP_VALID'))
    $stageResults.Add((Invoke-ColdStage `
        -Stage '06_cold_validate_tree_material_response_v3_assets' `
        -FunctionName 'EnsureTreeMaterialResponseAssets' `
        -TextProperty 'OutReport' `
        -ExpectedPrefix 'V5D_TREE_RESPONSE_MATERIAL_ASSETS_VALID' `
        -Library $treeLibrary))
    for ($treeFormIndex = 0; $treeFormIndex -lt $treeFormLabels.Count; $treeFormIndex++) {
        $stageResults.Add((Invoke-ColdStage `
            -Stage "06b_cold_validate_tree_mesh_response_v3_$($treeFormLabels[$treeFormIndex])" `
            -FunctionName 'EnsureTreeCanopyRealismMeshAsset' `
            -TextProperty 'OutReport' `
            -ExpectedPrefix 'V5D_TREE_REALISM_MESH_ASSET_VALID' `
            -Parameters @{ FormIndex = [int] $treeFormIndex } `
            -Library $treeLibrary))
    }
    $successQuiescence = Wait-NativeMutationQuiescence `
        -Label 'before R30 success receipts and commit receipt' `
        -TimeoutSeconds 60

    Assert-TreeReceipt $r29ContentRoot $immutableBefore.R29FacadeEnvironment 'R29 facade environment'
    Assert-TreeReceipt $r28ContentRoot $immutableBefore.R28Environment 'R28 environment/public realm'
    Assert-TreeReceipt $vegetationR29ContentRoot $immutableBefore.VegetationR29 'R29 vegetation'
    Assert-TreeReceipt $landmarkVegetationR28ContentRoot $immutableBefore.LandmarkVegetationR28 'R28 landmark vegetation'
    $treeRealismAfter = @(Get-TreeReceipt $treeRealismContentRoot)
    $treeMaterialResponseV3 = Assert-TreeRealismV3ContentDelta `
        -Before @($immutableBefore.TreeRealism) `
        -After $treeRealismAfter
    Assert-TreeReceipt $terrainR29ContentRoot $immutableBefore.TerrainR29 'R29 Copernicus terrain fallback'
    Assert-TreeReceipt $contextFacadeR25ContentRoot $immutableBefore.ContextFacadeR25 'current-surroundings context facade R25'
    Assert-TreeReceipt $contextTextureRoot $immutableBefore.ContextTextures 'context-admitted public-view textures'
    Assert-TreeReceipt $heroV2ContentRoot $immutableBefore.HeroMaterialsV2 'HeroMaterialsV2'
    Assert-TreeReceipt $heroV3ContentRoot $immutableBefore.HeroMaterialsV3 'HeroMaterialsV3'
    Assert-TreeReceipt $heroV4ContentRoot $immutableBefore.HeroMaterialsV4 'HeroMaterialsV4'
    Assert-TreeReceipt $heroV5ContentRoot $immutableBefore.HeroMaterialsV5 'HeroMaterialsV5'
    [void] (Assert-State $expectedMap $backupMap 'preserved external map backup')
    $successorMap = Get-FileState $mapFile
    if (-not $successorMap.Present -or $successorMap.Bytes -le 0 -or
        $successorMap.Sha256 -notmatch '^[A-F0-9]{64}$' -or
        ($successorMap.Bytes -eq $expectedMap.Bytes -and $successorMap.Sha256 -ceq $expectedMap.Sha256)) {
        throw 'R30 successor map receipt did not change from exact R29 predecessor.'
    }
    $treeMaterialResponseV3Receipt = [pscustomobject] [ordered] @{
        Status = $treeMaterialResponseV3.Status
        NativeSourcePins = @($sourcePins[7..10])
        SourceClosurePins = @($sourceAssetPins[1..6])
        ResponseMaterialPackageCount = 13
        ReboundManagedMeshPackageCount = 5
        RuntimeResponseMidCount = 26
        ContentBefore = @($treeMaterialResponseV3.Before)
        ContentAfter = @($treeMaterialResponseV3.After)
        TreePlacementGeometryOpacityWindAuthorityModified = $false
    }
    $commit = [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'COMMITTED'
        RunToken = $RunToken
        PrewriteAdmission = $prewriteAdmission
        BuildAdmission = $buildAdmission
        BuildContinuousMemoryGuard = $buildGuard
        NativeMutationQuiescence = $successQuiescence
        PredecessorMap = $expectedMap
        SuccessorMap = $successorMap
        ExternalBackup = Get-FileState $backupMap
        RuntimeDllBefore = $expectedRuntime
        RuntimeDllAfter = $runtimeAfter
        EditorDllBefore = $expectedEditor
        EditorDllAfter = $editorAfter
        StageResults = @($stageResults)
        R30ContentPackageCount = 12
        TreeMaterialResponseV3 = $treeMaterialResponseV3Receipt
        TreeMaterialResponseV3Promoted = $true
        TreeResponseMaterialPackageCount = 13
        TreeDerivativeMeshPackageCount = 5
        TreeRuntimeResponseMidCount = 26
        R29FacadeEnvironmentAssetsModified = $false
        R29FacadeMeshRetained = $true
        R28PublicRealmRetained = $true
        R29EnvironmentActiveRenderers = 0
        R29EnvironmentConcurrentRenderingAllowed = $false
        PredecessorVegetationOwner = $ExpectedVegetationOwner.ToUpperInvariant()
        VegetationMutationAllowed = $false
        TreeRealismValidated = $true
        TreePlacementGeometryOpacityWindAuthorityModified = $false
        TerrainR29Validated = $true
        ContextPolicyShellValidatedBeforeAndAfter = $true
        ContextTextureAssetsModified = $false
        HeroMaterialsDependencyAllowed = $false
        SimulationCollisionNavigationSensorRfAuthority = $false
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM
    $committed = $true
    $commit | ConvertTo-Json -Depth 12
}
catch {
    $failure = $_.Exception
}
finally {
    if (-not $committed -and $transactionStarted) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        $rollbackMayMutateFilesystem = $false
        try {
            [void] (Wait-NativeMutationQuiescence `
                -Label 'before R30 filesystem rollback restoration' `
                -TimeoutSeconds 60)
            $rollbackMayMutateFilesystem = $true
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackMayMutateFilesystem) {
            try { Restore-FileJournal $mapJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Remove-IsolatedR30Content } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-TreeJournal $treeRealismJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-TreeJournal $buildJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $sourceAssetJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $sourceJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        else {
            $rollbackErrors.Add(
                'Filesystem rollback was intentionally not attempted while an owned build/editor process tree remained active; exact journals were retained for guarded recovery.')
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore } catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback = [pscustomobject] [ordered] @{
            Schema = $schema
            Status = if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            Failure = if ($null -eq $failure) { 'unknown' } else { $failure.Message }
            RollbackErrors = @($rollbackErrors)
        }
        $rollback | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $transactionRoot 'rollback.json') -Encoding utf8NoBOM
        if ($rollbackErrors.Count -ne 0) {
            throw "R30 facade lookdev transaction failed and rollback was incomplete: failure={$($rollback.Failure)} rollback={$([string]::Join(' | ', @($rollbackErrors)))}"
        }
    }
}
if ($null -ne $failure) { throw $failure }
