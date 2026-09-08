<#
.SYNOPSIS
Build, freshly cook, and capture exact Player0 R30 ProviderFallback evidence.

.DESCRIPTION
This wrapper is intentionally inert unless -Execute or -AcceptVisualReview is
supplied. -Execute consumes a COMMITTED R30 transaction receipt,
transactionally promotes the two hash-pinned runtime capture-library sources,
builds the Development Game target, cooks into a new isolated sandbox, and
drives five fixed Player0 views through the runtime Remote Control API. It may
write only a PENDING_VISUAL_REVIEW receipt; mechanical PNG validation never
accepts visual quality. The separate -AcceptVisualReview mode requires an
explicit hash-pinned pending receipt and -ConfirmFiveImagesReviewed, re-decodes
and re-hashes all five PNGs, and revalidates the native read-only boundary
before it alone may publish COMMITTED. Neither path disables AirSim or
AirSimTriadRuntime or classifies fallback pixels as provider-ready proof.

Static contract check (no native-project access and no Unreal launch):
  .\Capture-IstanaExploreV5DR30Player0Evidence.ps1 -StaticSelfCheck

Live execution, only after a successful R30 native transaction:
  .\Capture-IstanaExploreV5DR30Player0Evidence.ps1 -Execute `
    -RunToken r30-player0-reviewed `
    -R30CommitReceipt D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\<token>\commit.json `
    -ExpectedR30CommitReceiptSha256 <sha256>

Explicit human acceptance, only after reviewing all five pending PNGs:
  .\Capture-IstanaExploreV5DR30Player0Evidence.ps1 -AcceptVisualReview `
    -ConfirmFiveImagesReviewed `
    -RunToken r30-player0-reviewed `
    -PendingCaptureReceipt D:\triad\TRIAD_R30Evidence\r30-player0-reviewed\pending-visual-review.json `
    -ExpectedPendingCaptureReceiptSha256 <sha256>
#>
[CmdletBinding()]
param(
    [switch] $StaticSelfCheck,
    [switch] $Execute,
    [switch] $AcceptVisualReview,
    [switch] $ConfirmFiveImagesReviewed,
    [string] $RunToken = 'static-contract',
    [string] $R30CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR30CommitReceiptSha256 = '',
    [string] $PendingCaptureReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedPendingCaptureReceiptSha256 = '',
    [ValidateRange(300, 3600)] [int] $BuildTimeoutSeconds = 1800,
    [ValidateRange(600, 7200)] [int] $CookTimeoutSeconds = 3600,
    [ValidateRange(120, 3600)] [int] $CaptureTimeoutSeconds = 1200,
    [ValidateRange(30, 300)] [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.r30_player0_capture.v1'
$r30TransactionSchema =
    'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 2147483648L # 2 GiB emergency commit reserve
$privateMemoryCeilingBytes = 9223372036854775807L # no fixed process-RAM ceiling
$minimumSystemFreeVirtualBytes = 2147483648L # 2 GiB emergency commit reserve
$memoryWatchdogPollMilliseconds = 500
$memoryWatchdogPersistentBreachMilliseconds = 2000
$restoreRetryCount = 8
$restoreRetryDelayMilliseconds = 250
$redirectedLogReleaseRetryCount = 40
$redirectedLogReleaseRetryDelayMilliseconds = 100
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$visualReviewContract = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_player0_visual_review.contract.json'))
$expectedVisualReviewContract = [pscustomobject] [ordered] @{
    Present=$true
    Bytes=2679L
    Sha256='2D93FF1CCBDD25EF532A47690531C5C19BB165D27040D7E2B8FE273744F266CC'
}
$treeMaterialResponseClosureContract = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_tree_material_response_v3.source_closure.json'))
$expectedTreeMaterialResponseClosureContract = [pscustomobject] [ordered] @{
    Present=$true
    Bytes=5171L
    Sha256='8DA0BCB32D4066036A985EF6F283E07F967B0AAD2C931C6A94E0C33925A82253'
}
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$nativeEvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence')
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$engineSourceRoot = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Source'))
$dotnet = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'))
$unrealBuildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'))
$unrealEditorCmd = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'))
$gameExe = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Binaries\Win64\TRIAD.exe'))
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeEditorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$nativePluginSourceRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Source'))
$groundHeader = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DGroundVegetationActor.h'))
$groundSource = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DGroundVegetationActor.cpp'))
$r30ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'))
$r29FacadeContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'))
$r29VegetationContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
$r29TerrainContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'))
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$contextFacadeR25ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25'))
$airSimRuntimeRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\AirSimTriadRuntime'))
$airSimRuntimeDescriptor = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'AirSimTriadRuntime.uplugin'))
$airSimRuntimeContentRoot = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'Content'))
$nativeProjectDdcRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\DerivedDataCache'))
$projectBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Binaries'))
$projectIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Intermediate'))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$airSimBinaryRoot = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'Binaries'))
$airSimIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'Intermediate'))
$r30TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$cookedRuntimeHotfixTransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DCookedRuntimeUE55V1'))
$cookedRuntimeHotfixTransactionSchema =
    'triad.istana_explore_v5d.cooked_runtime_ue55_hotfix.native_transaction.v1'
$cookedRuntimeHotfixCommitReceipt = [IO.Path]::GetFullPath((Join-Path `
    $cookedRuntimeHotfixTransactionBase `
    'r30-cooked-runtime-ue55-20260908-01\commit.json'))
$expectedCookedRuntimeHotfixCommitReceipt = [pscustomobject] [ordered] @{
    Present=$true
    Bytes=15166L
    Sha256='94A94303468D6045137CC74241117CC8C8047C9D9388E5672955B138745453EE'
}
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$rcPropertyUri = 'http://127.0.0.1:30010/remote/object/property'
$runtimeCaptureLibrary = '/Script/TRIADSensorFusion.Default__TRIADIstanaExploreV5DR30Player0CaptureLibrary'
$macDonaldRuntimeActorPath =
    '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid:PersistentLevel.TRIADIstanaExploreV5DR24MacDonaldHouse'
$temasekRuntimeActorPath =
    '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid:PersistentLevel.TRIADIstanaExploreV5DR24TemasekShophouse'
$script:activeOwnedProcess = $null

$captureSourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR30Player0CaptureLibrary.h'
        Bytes = 2046L
        Sha256 = 'B120B71629CB59BEC40C2E05DC18601F864CDFDA7DF45CCBAAB0B9700A920ADD'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR30Player0CaptureLibrary.cpp'
        Bytes = 26848L
        Sha256 = '0953E5D29CCD64FC7691FFA64A0FC68CC7377AEF10BD1C641A7C5FFAF6779878'
    }
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

# Largest-first is intentional. Each mesh receives its own guarded editor
# process so a cold 6.1 GiB static-mesh build cannot accumulate with another
# derivative mesh in the same process. The per-run DDC is shared only by these
# prewarm processes, their cache-only probe, the full cook, and the game.
$treeDerivativeMeshPrewarmPackages = @(
    '/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_ColumnarNarrow_NearLOD0'
    '/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0'
    '/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0'
    '/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0'
    '/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0'
)

# This exact filesystem-DDC record/content pair is a rebuildable acceleration
# seed, never an asset authority. It is copied byte-for-byte into the new
# evidence-run cache; Unreal never mounts or writes the shared source cache.
$columnarTreeDdcSeedPins = @(
    [pscustomobject] [ordered] @{
        RelativePath='Buckets\StaticMesh\a2\99\f84466d267d5711c7b95db355598c59dd3fa.udd'
        Bytes=178L
        Sha256='746286AA5339611E695ECEC63F44A4E12038F9C70529FB7FBF82F1F95979BEF3'
    }
    [pscustomobject] [ordered] @{
        RelativePath='Content\94\29\fa9aa87c0c793a75ce8aa546715d76d6b604.udd'
        Bytes=93363836L
        Sha256='CBC819BB03B671A17BB0CF6443F1D98472C8B844B4D981C665349F85856D19A1'
    }
)

$poses = @(
    [pscustomobject] [ordered] @{ Id='075m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-1.252968; Yaw=-90.0; Roll=0.0 }
    [pscustomobject] [ordered] @{ Id='020m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-4.703535; Yaw=-90.0; Roll=0.0 }
    [pscustomobject] [ordered] @{ Id='008m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-11.829499; Yaw=-90.0; Roll=0.0 }
    [pscustomobject] [ordered] @{ Id='002m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-55.084794; Yaw=-90.0; Roll=0.0 }
    [pscustomobject] [ordered] @{
        Id='surroundings_oblique_macdonald'
        X=40226.9640238642; Y=106152.591966384; Z=3900.0
        Pitch=-5.74137954693854; Yaw=-101.620267003074; Roll=0.0
    }
)

$r30ContentRelativePaths = @(
    'Materials\M_IPV5D_R30_ContextFacadePBR_Master.uasset'
    'Materials\MI_IPV5D_R30_GlassCool.uasset'
    'Materials\MI_IPV5D_R30_GlassWarm.uasset'
    'Materials\MI_IPV5D_R30_GlassNeutral.uasset'
    'Materials\MI_IPV5D_R30_FrameLight.uasset'
    'Materials\MI_IPV5D_R30_FrameDark.uasset'
    'Materials\MI_IPV5D_R30_FrameBronze.uasset'
    'Materials\MI_IPV5D_R30_SillLight.uasset'
    'Materials\MI_IPV5D_R30_SillDark.uasset'
    'Materials\MI_IPV5D_R30_RoofTrim.uasset'
    'Materials\MI_IPV5D_R30_Canopy.uasset'
    'Materials\MI_IPV5D_R30_BalconyRail.uasset'
)

$requiredCookedDependencyRoots = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'
    'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'
    'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25'
)

$forbiddenAirSimLaunchTokens = @(
    '-DisablePlugin=AirSim',
    '-DisablePlugins=AirSim',
    '-DisablePlugin=AirSimTriadRuntime',
    '-DisablePlugins=AirSimTriadRuntime',
    '-NoAirSim'
)

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $fullPath.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith(
            $fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-FileState {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{
            Present=$false; Bytes=0L; Sha256='ABSENT'; LastWriteUtc=$null
        }
    }
    $item = Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        LastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState $Path
    if ($actual.Present -ne $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label identity mismatch: path=$Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Test-ExactFileState {
    param($Expected, $Actual)
    if ([bool] $Actual.Present -ne [bool] $Expected.Present -or
        [int64] $Actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $Actual.Sha256 -cne [string] $Expected.Sha256) {
        return $false
    }
    if (-not [bool] $Expected.Present) { return $true }
    [string] $Actual.LastWriteUtc -ceq [string] $Expected.LastWriteUtc
}

function Test-ExactFileContentState {
    param($Expected, $Actual)
    [bool] $Actual.Present -eq [bool] $Expected.Present -and
        [int64] $Actual.Bytes -eq [int64] $Expected.Bytes -and
        [string] $Actual.Sha256 -ceq [string] $Expected.Sha256
}

function Assert-CaptureSourcePins {
    param([string] $Root)
    foreach ($pin in $captureSourcePins) {
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Capture source pin is not canonical: $($pin.RelativePath)"
        }
        $path = [IO.Path]::GetFullPath((Join-Path $Root $pin.RelativePath))
        if (-not (Test-ContainedPath $path $Root)) {
            throw "Capture source pin escaped root: $path"
        }
        [void] (Assert-State `
            ([pscustomobject] @{ Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256 }) `
            $path 'R30 capture-library source')
    }
}

function Get-TreeReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse |
            Sort-Object FullName |
            ForEach-Object {
                [pscustomobject] [ordered] @{
                    RelativePath = [IO.Path]::GetRelativePath($Root, $_.FullName)
                    Bytes = [int64] $_.Length
                    Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
                    LastWriteUtc = $_.LastWriteTimeUtc.ToString('o')
                }
            }
    )
}

function Get-TreeIdentityReceipt {
    param([string] $Root)
    @(
        Get-TreeReceipt $Root | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath = [string] $_.RelativePath
                Bytes = [int64] $_.Bytes
                Sha256 = [string] $_.Sha256
            }
        }
    )
}

function Assert-ColumnarTreeDdcSeedSource {
    $receipts = [Collections.Generic.List[object]]::new()
    foreach ($pin in $columnarTreeDdcSeedPins) {
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Columnar TreeRealism DDC seed pin is noncanonical: $($pin.RelativePath)"
        }
        $source = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectDdcRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $source $nativeProjectDdcRoot)) {
            throw "Columnar TreeRealism DDC seed escaped its source root: $source"
        }
        $state = Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        }) $source 'Columnar TreeRealism DDC acceleration seed source'
        $receipts.Add([pscustomobject] [ordered] @{
            Path=$source
            RelativePath=[string] $pin.RelativePath
            Present=[bool] $state.Present
            Bytes=[int64] $state.Bytes
            Sha256=[string] $state.Sha256
            LastWriteUtc=[string] $state.LastWriteUtc
        })
    }
    @($receipts)
}

function Assert-TreeMaterialResponseV3Receipt {
    param($Receipt, [string] $Label, [switch] $ValidateNative)
    if ([string] (Get-RequiredPropertyValue $Receipt 'Status' $Label) -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        [int] (Get-RequiredPropertyValue $Receipt 'ResponseMaterialPackageCount' $Label) -ne 13 -or
        [int] (Get-RequiredPropertyValue $Receipt 'ReboundManagedMeshPackageCount' $Label) -ne 5 -or
        [int] (Get-RequiredPropertyValue $Receipt 'RuntimeResponseMidCount' $Label) -ne 26 -or
        [bool] (Get-RequiredPropertyValue $Receipt 'TreePlacementGeometryOpacityWindAuthorityModified' $Label) -ne $false) {
        throw "$Label count or authority boundary drifted."
    }
    $nativePins = @(Get-RequiredPropertyValue $Receipt 'NativeSourcePins' $Label)
    $closurePins = @(Get-RequiredPropertyValue $Receipt 'SourceClosurePins' $Label)
    $before = @(Get-RequiredPropertyValue $Receipt 'ContentBefore' $Label)
    $after = @(Get-RequiredPropertyValue $Receipt 'ContentAfter' $Label)
    if ($nativePins.Count -ne 4 -or $closurePins.Count -ne 6 -or
        $after.Count -ne $before.Count + 13) {
        throw "$Label is not the exact four-source/six-evidence/thirteen-package closure."
    }
    $beforeByPath = @{}
    $afterByPath = @{}
    foreach ($row in $before) { $beforeByPath[[string] $row.RelativePath] = $row }
    foreach ($row in $after) { $afterByPath[[string] $row.RelativePath] = $row }
    $allowed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in @($treeResponseMaterialRelativePaths) + @($treeDerivativeMeshRelativePaths)) {
        if (-not $allowed.Add([string] $relative)) {
            throw "$Label has a duplicate managed TreeRealism path: $relative"
        }
    }
    foreach ($relative in $treeResponseMaterialRelativePaths) {
        if ($beforeByPath.ContainsKey($relative) -or -not $afterByPath.ContainsKey($relative)) {
            throw "$Label does not add the exact response-material package: $relative"
        }
    }
    foreach ($relative in $treeDerivativeMeshRelativePaths) {
        if (-not $beforeByPath.ContainsKey($relative) -or -not $afterByPath.ContainsKey($relative) -or
            [string] $beforeByPath[$relative].Sha256 -ceq [string] $afterByPath[$relative].Sha256) {
            throw "$Label does not rebind the exact managed tree mesh: $relative"
        }
    }
    foreach ($relative in @($beforeByPath.Keys)) {
        if (-not $allowed.Contains($relative) -and
            (-not $afterByPath.ContainsKey($relative) -or
             [int64] $beforeByPath[$relative].Bytes -ne [int64] $afterByPath[$relative].Bytes -or
             [string] $beforeByPath[$relative].Sha256 -cne [string] $afterByPath[$relative].Sha256)) {
            throw "$Label changes TreeRealism content outside the v3 closure: $relative"
        }
    }
    foreach ($relative in @($afterByPath.Keys)) {
        if (-not $beforeByPath.ContainsKey($relative) -and -not $allowed.Contains($relative)) {
            throw "$Label adds TreeRealism content outside the v3 closure: $relative"
        }
    }
    if ($ValidateNative) {
        foreach ($pin in @($nativePins) + @($closurePins)) {
            $relative = [string] (Get-RequiredPropertyValue $pin 'RelativePath' "$Label pin")
            if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('..')) {
                throw "$Label contains a noncanonical pin: $relative"
            }
            $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
            if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
                throw "$Label pin escaped the native project: $path"
            }
            [void] (Assert-State ([pscustomobject] @{
                Present=$true
                Bytes=[int64] (Get-RequiredPropertyValue $pin 'Bytes' "$Label pin")
                Sha256=[string] (Get-RequiredPropertyValue $pin 'Sha256' "$Label pin")
            }) $path "$Label promoted pin")
        }
        Assert-JsonIdentity @($after) @(Get-TreeIdentityReceipt $treeRealismContentRoot) `
            "$Label current TreeRealism content" 8
    }
    [pscustomobject] [ordered] @{
        Status='TREE_MATERIAL_RESPONSE_V3_RECEIPT_VALID'
        NativeSourcePins=@($nativePins)
        SourceClosurePins=@($closurePins)
        ContentBefore=@($before)
        ContentAfter=@($after)
        ResponseMaterialPackageCount=13
        ReboundManagedMeshPackageCount=5
        RuntimeResponseMidCount=26
        TreePlacementGeometryOpacityWindAuthorityModified=$false
    }
}

function Assert-TreeReceipt {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual = @(Get-TreeReceipt $Root)
    if ((ConvertTo-Json @($Expected) -Depth 6 -Compress) -cne
        (ConvertTo-Json @($actual) -Depth 6 -Compress)) {
        throw "$Label immutable tree changed: $Root"
    }
    @($actual)
}

function Get-PathBoundFileReceipt {
    param([string] $Path, $Expected, [string] $Label)
    $state = Assert-State $Expected $Path $Label
    [pscustomobject] [ordered] @{
        Path=[IO.Path]::GetFullPath($Path)
        Present=[bool] $state.Present
        Bytes=[int64] $state.Bytes
        Sha256=[string] $state.Sha256
        LastWriteUtc=[string] $state.LastWriteUtc
    }
}

function Assert-CaptureSourceTreeSnapshot {
    param([object[]] $Expected, [string] $Label)
    if (-not [IO.Directory]::Exists($nativePluginSourceRoot)) {
        throw "$Label native plugin source root is absent: $nativePluginSourceRoot"
    }
    $actual = if ($null -eq $Expected) {
        @(Get-TreeReceipt $nativePluginSourceRoot)
    }
    else {
        @(Assert-TreeReceipt $nativePluginSourceRoot @($Expected) $Label)
    }
    if ($actual.Count -eq 0) {
        throw "$Label native plugin source-tree receipt is empty."
    }
    @($actual)
}

function Write-JsonAtomic {
    param([Parameter(Mandatory = $true)] $Value,
          [Parameter(Mandatory = $true)] [string] $Path,
          [Parameter(Mandatory = $true)] [string] $AllowedRoot)
    if (-not (Test-ContainedPath $Path $AllowedRoot) -or
        [IO.File]::Exists($Path)) {
        throw "Atomic receipt refused unsafe or existing path: $Path"
    }
    [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))
    $temporary = "$Path.tmp.$PID"
    if ([IO.File]::Exists($temporary)) {
        throw "Atomic receipt temporary path already exists: $temporary"
    }
    try {
        $Value | ConvertTo-Json -Depth 16 |
            Set-Content -LiteralPath $temporary -Encoding utf8NoBOM
        Move-Item -LiteralPath $temporary -Destination $Path
    }
    finally {
        if ([IO.File]::Exists($temporary)) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
    Get-FileState $Path
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Journal target escaped native project: $path"
        }
        $before = Get-FileState $path
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $path)
        $backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $relative))
        if ($before.Present) {
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $path -Destination $backup
            [void] (Assert-State $before $backup 'capture journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path=$path; RelativePath=$relative; Before=$before
            Backup=if ($before.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function New-TreeJournal {
    param([string[]] $Roots, [string] $BackupRoot)
    $journals = [Collections.Generic.List[object]]::new()
    foreach ($root in @($Roots | Sort-Object -Unique)) {
        $fullRoot = [IO.Path]::GetFullPath($root)
        if ($fullRoot.Equals(
                $nativeProjectRoot,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-ContainedPath $fullRoot $nativeProjectRoot)) {
            throw "Tree journal refused unsafe native root: $fullRoot"
        }
        $directories = if ([IO.Directory]::Exists($fullRoot)) {
            @($fullRoot) + @(
                Get-ChildItem -LiteralPath $fullRoot -Directory -Recurse |
                    ForEach-Object FullName)
        } else { @() }
        $files = if ([IO.Directory]::Exists($fullRoot)) {
            @(
                Get-ChildItem -LiteralPath $fullRoot -File -Recurse |
                    ForEach-Object FullName)
        } else { @() }
        $journals.Add([pscustomobject] [ordered] @{
            Root=$fullRoot
            Directories=@($directories | Sort-Object -Unique)
            Files=@(New-FileJournal $files $BackupRoot)
            Before=@(Get-TreeReceipt $fullRoot)
        })
    }
    @($journals)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows)) {
        $restored = $false
        $lastRestoreError = $null
        for ($attempt = 1; $attempt -le $restoreRetryCount; ++$attempt) {
            try {
                $current = Get-FileState $row.Path
                if (Test-ExactFileState $row.Before $current) {
                    $restored = $true
                    break
                }

                if ([bool] $row.Before.Present) {
                    [void] [IO.Directory]::CreateDirectory(
                        [IO.Path]::GetDirectoryName($row.Path))
                    if (-not (Test-ExactFileContentState $row.Before $current)) {
                        Copy-Item -LiteralPath $row.Backup `
                            -Destination $row.Path -Force
                    }
                    $expectedLastWriteUtc = [DateTime]::Parse(
                        [string] $row.Before.LastWriteUtc,
                        [Globalization.CultureInfo]::InvariantCulture,
                        [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
                    [IO.File]::SetLastWriteTimeUtc(
                        [string] $row.Path, $expectedLastWriteUtc)
                }
                elseif ([IO.File]::Exists([string] $row.Path)) {
                    Remove-Item -LiteralPath $row.Path -Force
                }

                $current = Get-FileState $row.Path
                if (Test-ExactFileState $row.Before $current) {
                    $restored = $true
                    break
                }
                $lastRestoreError = [InvalidOperationException]::new(
                    "restored state remained different: expected=$($row.Before | ConvertTo-Json -Compress) actual=$($current | ConvertTo-Json -Compress)")
            }
            catch { $lastRestoreError = $_.Exception }

            if ($attempt -lt $restoreRetryCount) {
                Start-Sleep -Milliseconds $restoreRetryDelayMilliseconds
            }
        }
        if (-not $restored) {
            $detail = if ($null -ne $lastRestoreError) {
                $lastRestoreError.Message
            } else { 'unknown bounded restoration failure' }
            throw "Capture journal restore failed after $restoreRetryCount bounded attempts: path=$($row.Path) detail=$detail"
        }
    }
}

function Remove-JournalAdditionBounded {
    param([string] $Path, [string] $Root)
    if (-not (Test-ContainedPath $Path $Root)) {
        throw "Journal-addition cleanup escaped its validated root: path=$Path root=$Root"
    }
    $lastRemoveError = $null
    for ($attempt = 1; $attempt -le $restoreRetryCount; ++$attempt) {
        try {
            if (-not [IO.File]::Exists($Path)) { return }
            Remove-Item -LiteralPath $Path -Force
            if (-not [IO.File]::Exists($Path)) { return }
            $lastRemoveError = [InvalidOperationException]::new(
                'file remained present after removal')
        }
        catch { $lastRemoveError = $_.Exception }
        if ($attempt -lt $restoreRetryCount) {
            Start-Sleep -Milliseconds $restoreRetryDelayMilliseconds
        }
    }
    $detail = if ($null -ne $lastRemoveError) {
        $lastRemoveError.Message
    } else { 'unknown bounded removal failure' }
    throw "Journal-addition cleanup failed after $restoreRetryCount bounded attempts: path=$Path detail=$detail"
}

function Restore-TreeJournal {
    param([object[]] $Journals)
    foreach ($journal in @($Journals)) {
        $root = [IO.Path]::GetFullPath([string] $journal.Root)
        if ($root.Equals(
                $nativeProjectRoot,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-ContainedPath $root $nativeProjectRoot)) {
            throw "Tree rollback refused unsafe native root: $root"
        }
        $beforeFiles = [Collections.Generic.HashSet[string]]::new(
            [StringComparer]::OrdinalIgnoreCase)
        foreach ($row in @($journal.Files)) {
            [void] $beforeFiles.Add([string] $row.Path)
        }
        if ([IO.Directory]::Exists($root)) {
            foreach ($file in @(
                Get-ChildItem -LiteralPath $root -File -Recurse)) {
                if (-not $beforeFiles.Contains($file.FullName)) {
                    Remove-JournalAdditionBounded $file.FullName $root
                }
            }
        }
        Restore-FileJournal @($journal.Files)

        $beforeDirectories = [Collections.Generic.HashSet[string]]::new(
            [StringComparer]::OrdinalIgnoreCase)
        foreach ($directory in @($journal.Directories)) {
            [void] $beforeDirectories.Add([string] $directory)
        }
        if ([IO.Directory]::Exists($root)) {
            $currentDirectories = @($root) + @(
                Get-ChildItem -LiteralPath $root -Directory -Recurse |
                    ForEach-Object FullName)
            foreach ($directory in @(
                $currentDirectories | Sort-Object Length -Descending)) {
                if (-not $beforeDirectories.Contains($directory) -and
                    [IO.Directory]::Exists($directory) -and
                    @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
                    [IO.Directory]::Delete($directory, $false)
                }
            }
        }
        [void] (Assert-TreeReceipt $root @($journal.Before) 'restored build tree')
    }
}

function Remove-ContainedDirectory {
    param([string] $Path, [string] $AllowedRoot)
    $full = [IO.Path]::GetFullPath($Path)
    $root = [IO.Path]::GetFullPath($AllowedRoot)
    if ($full.Equals($root, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-ContainedPath $full $root)) {
        throw "Recursive cleanup refused non-child path: path=$full root=$root"
    }
    if ([IO.Directory]::Exists($full)) {
        Remove-Item -LiteralPath $full -Recurse -Force
    }
}

function Assert-LaunchLineRetainsAirSim {
    param([string] $ArgumentLine, [string] $Label)
    foreach ($token in $forbiddenAirSimLaunchTokens) {
        if ($ArgumentLine.Contains(
                $token, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$Label contains forbidden AirSim removal token: $token"
        }
    }
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $freeBytes = [int64] $os.FreeVirtualMemory * 1024L
    if ($freeBytes -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label refused by 2 GiB emergency FreeVirtualMemory reserve: available=$freeBytes"
    }
    [pscustomobject] [ordered] @{
        Label=$Label
        MinimumBytes=$minimumSystemFreeVirtualAtLaunchBytes
        ObservedBytes=$freeBytes
        Status='PASS'
    }
}

function Get-ProtectedProcesses {
    $protectedEditor = [IO.Path]::GetFullPath(
        'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
    $protectedProject = [IO.Path]::GetFullPath(
        'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object {
                $_.ExecutablePath -and $_.CommandLine -and
                [IO.Path]::GetFullPath($_.ExecutablePath).Equals(
                    $protectedEditor,
                    [StringComparison]::OrdinalIgnoreCase) -and
                $_.CommandLine.Contains(
                    $protectedProject,
                    [StringComparison]::OrdinalIgnoreCase)
            } |
            Sort-Object ProcessId |
            ForEach-Object {
                [pscustomobject] [ordered] @{
                    ProcessId=[uint32] $_.ProcessId
                    CreationDate=[string] $_.CreationDate
                    ExecutablePath=[IO.Path]::GetFullPath($_.ExecutablePath)
                    CommandLine=[string] $_.CommandLine
                }
            }
    )
}

function Assert-ProtectedUnchanged {
    param([object[]] $Before)
    if ((ConvertTo-Json @($Before) -Depth 4 -Compress) -cne
        (ConvertTo-Json @(Get-ProtectedProcesses) -Depth 4 -Compress)) {
        throw 'Protected UE5.4 CAPSTONE process set changed.'
    }
}

function Get-NativeMutatorProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object {
                $path = if ($_.ExecutablePath) {
                    [IO.Path]::GetFullPath($_.ExecutablePath)
                } else { '' }
                $command = [string] $_.CommandLine
                $isEngineUnreal = $path.StartsWith(
                        $engineRoot + '\',
                        [StringComparison]::OrdinalIgnoreCase) -and
                    $_.Name -like 'UnrealEditor*'
                $isExactGame = $path.Equals(
                    $gameExe, [StringComparison]::OrdinalIgnoreCase)
                $isExactProjectCommand = $command -and
                    ($command.Contains(
                        $nativeProjectFile,
                        [StringComparison]::OrdinalIgnoreCase) -or
                     $command.Contains(
                        $nativeProjectRoot,
                        [StringComparison]::OrdinalIgnoreCase))
                $isBuildTool = $_.Name -in @(
                    'dotnet.exe', 'UnrealBuildTool.exe',
                    'UnrealHeaderTool.exe', 'MSBuild.exe', 'cl.exe',
                    'link.exe', 'rc.exe', 'ShaderCompileWorker.exe')
                $isEngineUnreal -or $isExactGame -or
                    ($isExactProjectCommand -and $isBuildTool)
            } |
            Sort-Object ProcessId |
            ForEach-Object {
                [pscustomobject] [ordered] @{
                    ProcessId=[uint32] $_.ProcessId
                    ParentProcessId=[uint32] $_.ParentProcessId
                    CreationDate=[string] $_.CreationDate
                    Name=[string] $_.Name
                    ExecutablePath=if ($_.ExecutablePath) {
                        [IO.Path]::GetFullPath($_.ExecutablePath)
                    } else { '' }
                    CommandLine=[string] $_.CommandLine
                }
            }
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $busy = @(Get-NativeMutatorProcesses)
    if ($busy.Count -ne 0) {
        $summary = [string]::Join('; ', @($busy | ForEach-Object {
            "pid=$($_.ProcessId) name=$($_.Name)"
        }))
        throw "$Label refused because a native mutator is active: $summary"
    }
}

function Initialize-MemoryWatchdogType {
    if ($null -ne ('Triad.R30Capture.MemoryWatchdog' -as [type])) {
        return
    }
    $source = @'
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace Triad.R30Capture
{
    public sealed class MemoryWatchdogSnapshot
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

    public sealed class MemoryWatchdog : IDisposable
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

        public MemoryWatchdog(
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
                this.thread.Name = "TRIAD R30 capture memory watchdog";
                this.thread.Start();
            }
        }

        private bool TryGetExactProcess(
            out Process process,
            out bool processExited,
            out string failure)
        {
            process = null;
            processExited = false;
            failure = String.Empty;
            Process candidate = null;
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
                ProcessModule mainModule = candidate.MainModule;
                if (mainModule == null ||
                    String.IsNullOrWhiteSpace(mainModule.FileName))
                {
                    failure = "Exact owned-process executable lookup returned no path.";
                    return false;
                }
                string actualPath = Path.GetFullPath(mainModule.FileName);
                if (actualTicks != this.creationUtcTicks ||
                    !String.Equals(actualPath, this.executablePath,
                        StringComparison.OrdinalIgnoreCase))
                {
                    failure = "Exact owned-process identity changed while monitored.";
                    return false;
                }
                process = candidate;
                candidate = null;
                return true;
            }
            catch (ArgumentException)
            {
                processExited = true;
                return false;
            }
            catch (Exception ex)
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
            string failure;
            if (!this.TryGetExactProcess(
                    out process, out processExited, out failure))
            {
                if (!processExited)
                    this.SetMonitorError(failure);
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
                process.Refresh();
                long privateBytes = process.PrivateMemorySize64;
                ulong availableCommitBytes = memory.AvailablePageFile;
                DateTime now = DateTime.UtcNow;
                string kind = privateBytes >= this.privateCeilingBytes
                    ? "MEMORY_GUARD_PRIVATE_BYTES"
                    : availableCommitBytes < this.minimumAvailableCommitBytes
                        ? "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL"
                        : String.Empty;
                bool shouldContain = false;
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
                        if (!this.continuousBreachStartedUtc.HasValue)
                            this.continuousBreachStartedUtc = now;
                        if ((now - this.continuousBreachStartedUtc.Value).TotalMilliseconds >=
                                    this.persistentBreachMilliseconds)
                        {
                            if (String.IsNullOrEmpty(this.alertKind))
                            {
                                this.alertKind = kind;
                                this.alertObservedUtc = now.ToString("o");
                                this.alertPrivateBytes = privateBytes;
                                this.alertAvailableCommitBytes =
                                    availableCommitBytes;
                                this.exactIdentityVerifiedAtAlert = true;
                            }
                            shouldContain = true;
                        }
                    }
                    else
                    {
                        this.continuousBreachStartedUtc = null;
                    }
                }
                if (shouldContain)
                    this.ContainExactProcess();
            }
        }

        private void ContainExactProcess()
        {
            Process process;
            bool processExited;
            string failure;
            if (!this.TryGetExactProcess(
                    out process, out processExited, out failure))
            {
                lock (this.gate)
                    this.containmentRefused = true;
                if (!processExited)
                    this.SetMonitorError(failure);
                return;
            }
            using (process)
            {
                lock (this.gate)
                    this.exactIdentityVerifiedAtContainment = true;
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

        public MemoryWatchdogSnapshot GetSnapshot()
        {
            lock (this.gate)
            {
                return new MemoryWatchdogSnapshot
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

function Assert-WatchdogHealthy {
    param($Watchdog, [string] $Checkpoint)
    if ($null -eq $Watchdog) {
        throw "MEMORY_WATCHDOG_MISSING: checkpoint=$Checkpoint"
    }
    $snapshot = $Watchdog.GetSnapshot()
    if (-not $snapshot.Started -or $snapshot.SampleCount -le 0) {
        throw "MEMORY_WATCHDOG_NO_SAMPLES: checkpoint=$Checkpoint"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) {
        throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($snapshot.MonitorError)"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.AlertKind)) {
        throw "$($snapshot.AlertKind): checkpoint=$Checkpoint privateBytes=$($snapshot.AlertPrivateBytes) freeVirtualBytes=$($snapshot.AlertAvailableCommitBytes)"
    }
    $snapshot
}

function Join-NativeArgumentLine {
    param([string[]] $Arguments)
    [string]::Join(' ', @($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"{0}"' -f $_.Replace('"', '\"')
        }
        else { $_ }
    }))
}

function Start-GuardedOwnedProcess {
    param(
        [string] $Label,
        [string] $FilePath,
        [string[]] $Arguments,
        [string] $WorkingDirectory,
        [string] $StandardOutputLog,
        [string] $StandardErrorLog,
        [string[]] $RequiredCommandLineTokens,
        [hashtable] $ChildEnvironment
    )
    if ($null -ne $script:activeOwnedProcess) {
        throw "A guarded native process is already owned: $($script:activeOwnedProcess.Label)"
    }
    Assert-NativeIdle "before $Label"
    Assert-ProtectedUnchanged $script:protectedBefore
    $admission = Assert-LaunchAdmission "before $Label"
    foreach ($log in @($StandardOutputLog, $StandardErrorLog)) {
        if ([IO.File]::Exists($log)) {
            throw "$Label refused existing redirected log: $log"
        }
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($log))
    }
    $argumentLine = Join-NativeArgumentLine $Arguments
    Assert-LaunchLineRetainsAirSim $argumentLine $Label
    $validatedChildEnvironment = @{}
    if ($null -ne $ChildEnvironment) {
        foreach ($key in @($ChildEnvironment.Keys)) {
            if ([string] $key -cne 'UE-LocalDataCachePath') {
                throw "$Label requested a forbidden child-environment override: $key"
            }
            $value = [string] $ChildEnvironment[$key]
            if ([string]::IsNullOrWhiteSpace($value) -or
                -not [IO.Path]::IsPathFullyQualified($value)) {
                throw "$Label requested a noncanonical child-only DDC path."
            }
            $validatedChildEnvironment['UE-LocalDataCachePath'] =
                [IO.Path]::GetFullPath($value)
        }
    }
    $parentLocalDataCachePathBefore = [Environment]::GetEnvironmentVariable(
        'UE-LocalDataCachePath', [EnvironmentVariableTarget]::Process)
    $handle = Start-Process -FilePath $FilePath `
        -ArgumentList $argumentLine `
        -WorkingDirectory $WorkingDirectory `
        -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog `
        -Environment $validatedChildEnvironment `
        -PassThru -WindowStyle Hidden
    $parentLocalDataCachePathAfter = [Environment]::GetEnvironmentVariable(
        'UE-LocalDataCachePath', [EnvironmentVariableTarget]::Process)
    if (-not [object]::Equals(
            $parentLocalDataCachePathBefore,
            $parentLocalDataCachePathAfter)) {
        $handle.Kill($true)
        [void] $handle.WaitForExit(30000)
        $handle.Dispose()
        throw "$Label detected a parent-process DDC environment mutation."
    }
    $expectedPath = [IO.Path]::GetFullPath($FilePath)
    $identity = $null
    $identityDeadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $handle.Refresh()
        if ($handle.HasExited) { break }
        $candidateIdentity = Get-CimInstance Win32_Process `
            -Filter "ProcessId=$($handle.Id)" -ErrorAction SilentlyContinue
        if ($null -ne $candidateIdentity -and
            -not [string]::IsNullOrWhiteSpace(
                [string] $candidateIdentity.ExecutablePath) -and
            -not [string]::IsNullOrWhiteSpace(
                [string] $candidateIdentity.CommandLine)) {
            $candidatePath = [IO.Path]::GetFullPath(
                [string] $candidateIdentity.ExecutablePath)
            if (-not $candidatePath.Equals(
                    $expectedPath,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw "$Label started an unexpected executable path: $candidatePath"
            }
            $identity = $candidateIdentity
            break
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $identityDeadline)
    if ($null -eq $identity) {
        $handle.Refresh()
        if ($handle.HasExited) {
            throw "$Label exited before exact identity registration."
        }
        throw "$Label exact executable identity could not be proven."
    }
    foreach ($token in $RequiredCommandLineTokens) {
        if (-not ([string] $identity.CommandLine).Contains(
                $token, [StringComparison]::OrdinalIgnoreCase)) {
            $handle.Kill($true)
            throw "$Label command-line identity lacks required token: $token"
        }
    }
    Initialize-MemoryWatchdogType
    Initialize-BinaryMarkerScannerType
    $watchdog = [Triad.R30Capture.MemoryWatchdog]::new(
        [int] $handle.Id,
        [int64] $handle.StartTime.ToUniversalTime().Ticks,
        $expectedPath,
        $privateMemoryCeilingBytes,
        $minimumSystemFreeVirtualBytes,
        $memoryWatchdogPollMilliseconds,
        $memoryWatchdogPersistentBreachMilliseconds)
    $watchdog.Start()
    $script:activeOwnedProcess = [pscustomobject] [ordered] @{
        Label=$Label
        Handle=$handle
        ProcessId=[int] $handle.Id
        StartUtcTicks=[int64] $handle.StartTime.ToUniversalTime().Ticks
        StartUtc=$handle.StartTime.ToUniversalTime().ToString('o')
        ExpectedExecutablePath=$expectedPath
        CommandLine=[string] $identity.CommandLine
        Admission=$admission
        Watchdog=$watchdog
        StandardOutputLog=$StandardOutputLog
        StandardErrorLog=$StandardErrorLog
        ChildEnvironment=[pscustomobject] [ordered] @{
            UELocalDataCachePath=if (
                $validatedChildEnvironment.ContainsKey(
                    'UE-LocalDataCachePath')) {
                [string] $validatedChildEnvironment['UE-LocalDataCachePath']
            } else { $null }
            ParentProcessEnvironmentMutated=$false
        }
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $snapshot = $watchdog.GetSnapshot()
        if ($snapshot.SampleCount -gt 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    [void] (Assert-WatchdogHealthy $watchdog "$Label startup")
    $script:activeOwnedProcess
}

function Assert-ExactOwnedProcessIdentity {
    param($Owned)
    $Owned.Handle.Refresh()
    if ($Owned.Handle.HasExited) { return }
    $actualPath = [IO.Path]::GetFullPath($Owned.Handle.MainModule.FileName)
    if ($Owned.Handle.Id -ne $Owned.ProcessId -or
        [int64] $Owned.Handle.StartTime.ToUniversalTime().Ticks -ne
            [int64] $Owned.StartUtcTicks -or
        -not $actualPath.Equals(
            $Owned.ExpectedExecutablePath,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Exact owned process identity changed: $($Owned.Label)"
    }
}

function Read-RuntimeLogTextShared {
    param(
        [string] $Path,
        [string] $Label,
        [ValidateRange(1, 400)] [int] $RetryCount = 40,
        [ValidateRange(0, 1000)] [int] $RetryDelayMilliseconds = 100,
        [switch] $AllowMissing
    )
    if ([string]::IsNullOrWhiteSpace($Path) -or
        -not [IO.Path]::IsPathFullyQualified($Path)) {
        throw "$Label received an invalid runtime-log path."
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $lastReadError = $null
    for ($attempt = 1; $attempt -le $RetryCount; ++$attempt) {
        $stream = $null
        $reader = $null
        try {
            if (-not [IO.File]::Exists($fullPath)) {
                if ($AllowMissing) { return '' }
                throw [IO.FileNotFoundException]::new(
                    "$Label runtime log is absent.", $fullPath)
            }
            $share = [IO.FileShare] (
                [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
            $stream = [IO.File]::Open(
                $fullPath,
                [IO.FileMode]::Open,
                [IO.FileAccess]::Read,
                $share)
            $reader = [IO.StreamReader]::new(
                $stream, [Text.Encoding]::UTF8, $true, 65536)
            return $reader.ReadToEnd()
        }
        catch [IO.IOException] { $lastReadError = $_.Exception }
        catch [System.UnauthorizedAccessException] {
            $lastReadError = $_.Exception
        }
        finally {
            if ($null -ne $reader) { $reader.Dispose() }
            elseif ($null -ne $stream) { $stream.Dispose() }
        }
        if ($attempt -lt $RetryCount -and $RetryDelayMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $RetryDelayMilliseconds
        }
    }
    $detail = if ($null -ne $lastReadError) {
        $lastReadError.Message
    } else { 'unknown shared-read failure' }
    throw [IO.IOException]::new(
        "$Label runtime log could not be shared-read after $RetryCount bounded attempts: path=$fullPath detail=$detail",
        $lastReadError)
}

function Wait-GuardedOwnedProcess {
    param(
        $Owned,
        [int] $TimeoutSeconds,
        [string] $RuntimeLogPath = '',
        [string] $ForbiddenRuntimeLogPattern = ''
    )
    if (-not [string]::IsNullOrWhiteSpace($ForbiddenRuntimeLogPattern) -and
        ([string]::IsNullOrWhiteSpace($RuntimeLogPath) -or
         -not [IO.Path]::IsPathFullyQualified($RuntimeLogPath))) {
        throw "$($Owned.Label) received an invalid live runtime-log sentinel path."
    }
    $pollMilliseconds = if (
        [string]::IsNullOrWhiteSpace($ForbiddenRuntimeLogPattern)) {
        $memoryWatchdogPollMilliseconds
    } else { 100 }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $Owned.Handle.WaitForExit($pollMilliseconds)) {
        [void] (Assert-WatchdogHealthy $Owned.Watchdog $Owned.Label)
        if (-not [string]::IsNullOrWhiteSpace($ForbiddenRuntimeLogPattern) -and
            [IO.File]::Exists($RuntimeLogPath)) {
            $runtimeText = ''
            try {
                $runtimeText = Read-RuntimeLogTextShared `
                    -Path $RuntimeLogPath `
                    -Label "$($Owned.Label) live runtime log" `
                    -RetryCount 1 `
                    -RetryDelayMilliseconds 0 `
                    -AllowMissing
            }
            catch [IO.IOException] {
                # The next 100 ms poll retries while the memory guard continues.
            }
            if ([regex]::IsMatch($runtimeText, $ForbiddenRuntimeLogPattern)) {
                Assert-ExactOwnedProcessIdentity $Owned
                $Owned.Handle.Kill($true)
                [void] $Owned.Handle.WaitForExit(30000)
                throw "$($Owned.Label) hit its forbidden live runtime-log sentinel."
            }
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "$($Owned.Label) exceeded its bounded timeout of $TimeoutSeconds seconds."
        }
    }
    [void] (Assert-WatchdogHealthy $Owned.Watchdog "$($Owned.Label) exit")
    if ($Owned.Handle.ExitCode -ne 0) {
        throw "$($Owned.Label) failed: exit=$($Owned.Handle.ExitCode)"
    }
}

function Wait-RedirectedLogRelease {
    param([string[]] $Paths, [string] $Label)
    foreach ($path in @($Paths)) {
        $released = $false
        $lastReleaseError = $null
        for ($attempt = 1;
             $attempt -le $redirectedLogReleaseRetryCount;
             ++$attempt) {
            $stream = $null
            try {
                if (-not [IO.File]::Exists($path)) {
                    throw [IO.FileNotFoundException]::new(
                        "$Label redirected log is absent.", $path)
                }
                $stream = [IO.File]::Open(
                    $path,
                    [IO.FileMode]::Open,
                    [IO.FileAccess]::Read,
                    [IO.FileShare]::None)
                $released = $true
                break
            }
            catch { $lastReleaseError = $_.Exception }
            finally {
                if ($null -ne $stream) { $stream.Dispose() }
            }
            if ($attempt -lt $redirectedLogReleaseRetryCount) {
                Start-Sleep -Milliseconds $redirectedLogReleaseRetryDelayMilliseconds
            }
        }
        if (-not $released) {
            $detail = if ($null -ne $lastReleaseError) {
                $lastReleaseError.Message
            } else { 'unknown redirected-log release failure' }
            throw "$Label redirected log was not released after $redirectedLogReleaseRetryCount bounded attempts: path=$path detail=$detail"
        }
    }
}

function Close-GuardedOwnedProcess {
    param($Owned, [switch] $AllowContainment)
    if ($null -eq $Owned) { return $null }
    $closeErrors = [Collections.Generic.List[string]]::new()
    $processExited = $false
    $exitCode = $null
    try {
        $Owned.Handle.Refresh()
        if (-not $Owned.Handle.HasExited) {
            if (-not $AllowContainment) {
                throw "$($Owned.Label) is still running and containment was not authorized."
            }
            Assert-ExactOwnedProcessIdentity $Owned
            $Owned.Handle.Kill($true)
            if (-not $Owned.Handle.WaitForExit(30000)) {
                throw "$($Owned.Label) exact process tree did not exit after containment."
            }
        }
        $Owned.Handle.WaitForExit()
        $processExited = $true
        $exitCode = [int] $Owned.Handle.ExitCode
    }
    catch { $closeErrors.Add("process=$($_.Exception.Message)") }
    $snapshot = $null
    if ($processExited) {
        try {
            $Owned.Watchdog.Stop()
            $snapshot = $Owned.Watchdog.GetSnapshot()
            if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError) -or
                -not [string]::IsNullOrWhiteSpace($snapshot.AlertKind) -or
                $snapshot.SampleCount -le 0) {
                throw "Memory watchdog final state is not healthy for $($Owned.Label)."
            }
        }
        catch { $closeErrors.Add("watchdog=$($_.Exception.Message)") }
        finally {
            try { $Owned.Watchdog.Dispose() }
            catch { $closeErrors.Add("watchdogDispose=$($_.Exception.Message)") }
        }
        try { $Owned.Handle.Dispose() }
        catch { $closeErrors.Add("processDispose=$($_.Exception.Message)") }
        $script:activeOwnedProcess = $null
        try {
            [void] (Wait-RedirectedLogRelease `
                @($Owned.StandardOutputLog, $Owned.StandardErrorLog) `
                $Owned.Label)
        }
        catch { $closeErrors.Add("redirectedLogs=$($_.Exception.Message)") }
    }
    if ($closeErrors.Count -ne 0) {
        throw [InvalidOperationException]::new(
            [string]::Join(' ', @($closeErrors)))
    }
    [pscustomobject] [ordered] @{
        Label=$Owned.Label
        ProcessId=$Owned.ProcessId
        StartUtc=$Owned.StartUtc
        ExitCode=$exitCode
        CommandLine=$Owned.CommandLine
        ChildEnvironment=$Owned.ChildEnvironment
        Admission=$Owned.Admission
        ContinuousMemoryGuard=$snapshot
        StandardOutputLog=Get-FileState $Owned.StandardOutputLog
        StandardErrorLog=Get-FileState $Owned.StandardErrorLog
    }
}

function Wait-NativeMutationQuiescence {
    param([string] $Label, [int] $TimeoutSeconds=180)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $busy = @()
    do {
        $busy = @(Get-NativeMutatorProcesses)
        if ($busy.Count -eq 0 -and $null -eq $script:activeOwnedProcess) {
            return [pscustomobject] [ordered] @{
                Label=$Label; Status='QUIESCENT'; NativeMutatorProcessCount=0
            }
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $busySummary = if ($busy.Count -eq 0) {
        '<none>'
    } else {
        [string]::Join('; ', @($busy | ForEach-Object {
            "pid=$($_.ProcessId) parent=$($_.ParentProcessId) name=$($_.Name) path=$($_.ExecutablePath)"
        }))
    }
    $ownedSummary = if ($null -eq $script:activeOwnedProcess) {
        '<none>'
    } else {
        "pid=$($script:activeOwnedProcess.ProcessId) label=$($script:activeOwnedProcess.Label)"
    }
    throw "$Label did not reach exact native mutation quiescence after $TimeoutSeconds seconds: busy={$busySummary} activeOwned={$ownedSummary}."
}

function Invoke-GuardedCommand {
    param(
        [string] $Label,
        [string] $FilePath,
        [string[]] $Arguments,
        [string] $WorkingDirectory,
        [string] $StandardOutputLog,
        [string] $StandardErrorLog,
        [string[]] $RequiredCommandLineTokens,
        [hashtable] $ChildEnvironment,
        [int] $TimeoutSeconds,
        [string] $RuntimeLogPath = '',
        [string] $ForbiddenRuntimeLogPattern = ''
    )
    $owned = $null
    $primaryError = $null
    $receipt = $null
    try {
        $startParameters = @{
            Label=$Label
            FilePath=$FilePath
            Arguments=$Arguments
            WorkingDirectory=$WorkingDirectory
            StandardOutputLog=$StandardOutputLog
            StandardErrorLog=$StandardErrorLog
            RequiredCommandLineTokens=$RequiredCommandLineTokens
            ChildEnvironment=$ChildEnvironment
        }
        $owned = Start-GuardedOwnedProcess @startParameters
        Wait-GuardedOwnedProcess `
            $owned $TimeoutSeconds $RuntimeLogPath $ForbiddenRuntimeLogPattern
    }
    catch {
        $primaryError = $_.Exception
        if ($null -eq $owned -and
            $null -ne $script:activeOwnedProcess -and
            $script:activeOwnedProcess.Label -ceq $Label) {
            $owned = $script:activeOwnedProcess
        }
    }
    finally {
        if ($null -ne $owned) {
            try { $receipt = Close-GuardedOwnedProcess $owned -AllowContainment }
            catch {
                if ($null -eq $primaryError) { $primaryError = $_.Exception }
                else {
                    $primaryError = [InvalidOperationException]::new(
                        "command={$($primaryError.Message)} cleanup={$($_.Exception.Message)}")
                }
            }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Wait-NativeMutationQuiescence "after $Label" 180)
    if ($null -ne $primaryError) { throw $primaryError }
    $receipt
}

function Assert-RcPortUnowned {
    $listeners = @(
        Get-NetTCPConnection -LocalPort 30010 -State Listen `
            -ErrorAction SilentlyContinue)
    if ($listeners.Count -ne 0) {
        $owners = @($listeners | Select-Object -ExpandProperty OwningProcess -Unique)
        throw "RC port 30010 is already listening before Game launch: owners=$([string]::Join(',', $owners))"
    }
}

function Assert-RcPortOwnedByGame {
    if ($null -eq $script:activeOwnedProcess) {
        throw 'RC ownership check has no exact owned Game process.'
    }
    Assert-ExactOwnedProcessIdentity $script:activeOwnedProcess
    $listeners = @(
        Get-NetTCPConnection -LocalPort 30010 -State Listen `
            -ErrorAction SilentlyContinue)
    $owners = @(
        $listeners | Select-Object -ExpandProperty OwningProcess -Unique)
    if ($owners.Count -ne 1 -or
        [int] $owners[0] -ne [int] $script:activeOwnedProcess.ProcessId) {
        throw "RC port 30010 is not solely owned by the exact Game process: expected=$($script:activeOwnedProcess.ProcessId) owners=$([string]::Join(',', $owners))"
    }
}

function Invoke-RcCall {
    param(
        [string] $FunctionName,
        [hashtable] $Parameters=@{},
        [int] $TimeoutSeconds=120
    )
    if ($null -eq $script:activeOwnedProcess) {
        throw "Remote Control call has no exact owned Game process: $FunctionName"
    }
    Assert-RcPortOwnedByGame
    [void] (Assert-WatchdogHealthy `
        $script:activeOwnedProcess.Watchdog "RC $FunctionName")
    $script:activeOwnedProcess.Handle.Refresh()
    if ($script:activeOwnedProcess.Handle.HasExited) {
        throw "Owned Game exited before RC call: $FunctionName"
    }
    $body = [ordered] @{
        objectPath=$runtimeCaptureLibrary
        functionName=$FunctionName
        parameters=$Parameters
        generateTransaction=$false
    } | ConvertTo-Json -Depth 8 -Compress
    $result = Invoke-RestMethod -Method Put -Uri $rcUri `
        -ContentType 'application/json' -Body $body `
        -TimeoutSec $TimeoutSeconds
    Assert-RcPortOwnedByGame
    [void] (Assert-WatchdogHealthy `
        $script:activeOwnedProcess.Watchdog "RC $FunctionName response")
    $result
}

function Invoke-RcPropertyRead {
    param(
        [string] $ObjectPath,
        [string] $PropertyName,
        [int] $TimeoutSeconds=30
    )
    if ($null -eq $script:activeOwnedProcess) {
        throw "Remote Control property read has no exact owned Game process: $PropertyName"
    }
    if ([string]::IsNullOrWhiteSpace($ObjectPath) -or
        [string]::IsNullOrWhiteSpace($PropertyName)) {
        throw 'Remote Control property read received a blank object path or property name.'
    }
    Assert-RcPortOwnedByGame
    [void] (Assert-WatchdogHealthy `
        $script:activeOwnedProcess.Watchdog "RC property read $PropertyName")
    $script:activeOwnedProcess.Handle.Refresh()
    if ($script:activeOwnedProcess.Handle.HasExited) {
        throw "Owned Game exited before RC property read: $PropertyName"
    }
    $body = [ordered] @{
        objectPath=$ObjectPath
        propertyName=$PropertyName
        access='READ_ACCESS'
    } | ConvertTo-Json -Depth 6 -Compress
    $result = Invoke-RestMethod -Method Put -Uri $rcPropertyUri `
        -ContentType 'application/json' -Body $body `
        -TimeoutSec $TimeoutSeconds
    Assert-RcPortOwnedByGame
    [void] (Assert-WatchdogHealthy `
        $script:activeOwnedProcess.Watchdog `
        "RC property read $PropertyName response")
    $result
}

function Get-RcBooleanPropertyReceipt {
    param(
        [string] $ObjectPath,
        [string] $PropertyName,
        [string] $Label
    )
    $readback = Invoke-RcPropertyRead $ObjectPath $PropertyName 30
    $readbackJson = $readback | ConvertTo-Json -Compress -Depth 8
    $pattern = '"' + [regex]::Escape($PropertyName) +
        '"\s*:\s*(?<Value>true|false)'
    $match = [regex]::Match(
        $readbackJson,
        $pattern,
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        throw "$Label readback lacks exact boolean property $PropertyName."
    }
    [pscustomobject] [ordered] @{
        ObjectPath=$ObjectPath
        PropertyName=$PropertyName
        Value=($match.Groups['Value'].Value -ieq 'true')
        Access='READ_ACCESS'
        MutationAllowed=$false
    }
}

function Get-LandmarkRuntimeContractSnapshot {
    param([string] $ObjectPath, [string] $Label)
    $binding = Get-RcBooleanPropertyReceipt `
        $ObjectPath 'bRuntimeProviderTelemetryBindingValid' $Label
    $failedClosed = Get-RcBooleanPropertyReceipt `
        $ObjectPath 'bRuntimeAssetContractFailClosed' $Label
    $visible = Get-RcBooleanPropertyReceipt `
        $ObjectPath 'bDedicatedOverlayVisible' $Label
    [pscustomobject] [ordered] @{
        Label=$Label
        ObjectPath=$ObjectPath
        RuntimeProviderTelemetryBindingValid=$binding
        RuntimeAssetContractFailClosed=$failedClosed
        DedicatedOverlayVisible=$visible
    }
}

function Wait-LandmarkRuntimeContracts {
    param(
        [string] $RuntimeLogPath,
        [int] $TimeoutSeconds,
        [string] $Phase
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = ''
    do {
        Assert-LiveRuntimeCaptureCanContinue `
            $RuntimeLogPath "R30 landmark runtime audit $Phase"
        try {
            $macDonald = Get-LandmarkRuntimeContractSnapshot `
                $macDonaldRuntimeActorPath 'R24 MacDonald House'
            $temasek = Get-LandmarkRuntimeContractSnapshot `
                $temasekRuntimeActorPath 'R24 Temasek Shophouse'
            foreach ($snapshot in @($macDonald, $temasek)) {
                if ($snapshot.RuntimeAssetContractFailClosed.Value) {
                    throw "$($snapshot.Label) reports runtime fail-closed state."
                }
            }
            if ($macDonald.RuntimeProviderTelemetryBindingValid.Value -and
                $macDonald.DedicatedOverlayVisible.Value -and
                $temasek.RuntimeProviderTelemetryBindingValid.Value -and
                $temasek.DedicatedOverlayVisible.Value) {
                return [pscustomobject] [ordered] @{
                    Phase=$Phase
                    ReadOnlyPropertyEndpoint=$rcPropertyUri
                    ExactActorCount=2
                    RuntimeFullAuditPassed=$true
                    MacDonaldHouse=$macDonald
                    TemasekShophouse=$temasek
                }
            }
            $lastError = 'One or more landmark runtime bindings/overlays are not ready.'
        }
        catch {
            if ($_.Exception.Message.Contains(
                    'reports runtime fail-closed state.',
                    [StringComparison]::Ordinal)) {
                throw
            }
            $lastError = $_.Exception.Message
        }
        Assert-LiveRuntimeCaptureCanContinue `
            $RuntimeLogPath "R30 landmark runtime audit retry $Phase"
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "R30 landmark runtime contracts did not become valid during $Phase`: $lastError"
}

function Assert-LiveRuntimeCaptureCanContinue {
    param([string] $RuntimeLogPath, [string] $Label)
    if ($null -eq $script:activeOwnedProcess) {
        throw "$Label has no exact owned Game process."
    }
    $owned = $script:activeOwnedProcess
    $owned.Handle.Refresh()
    if ($owned.Handle.HasExited) {
        throw "$Label cannot continue because the exact owned Game exited: exit=$($owned.Handle.ExitCode)"
    }
    Assert-ExactOwnedProcessIdentity $owned
    [void] (Assert-WatchdogHealthy $owned.Watchdog $Label)
    if ([IO.File]::Exists($RuntimeLogPath)) {
        $runtimeText = Read-RuntimeLogTextShared `
            -Path $RuntimeLogPath `
            -Label "$Label live runtime log" `
            -AllowMissing
        Assert-NoFatalRuntimeLogText `
            -Text $runtimeText `
            -Label "$Label live runtime log"
    }
}

function Wait-RuntimeCaptureReady {
    param([string] $RuntimeLogPath, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = ''
    do {
        Assert-LiveRuntimeCaptureCanContinue `
            $RuntimeLogPath 'R30 runtime capture readiness'
        Start-Sleep -Seconds 2
        $response = $null
        try {
            $response = Invoke-RcCall `
                'GetIstanaExploreV5DR30Player0CaptureState' @{} 60
        }
        catch {
            $lastError = $_.Exception.Message
            Assert-LiveRuntimeCaptureCanContinue `
                $RuntimeLogPath 'R30 runtime capture readiness retry'
            continue
        }
        $lastError = [string] $response.OutReport
        Assert-NoFatalRuntimeLogText `
            -Text $lastError `
            -Label 'R30 runtime capture-state response'
        Assert-LiveRuntimeCaptureCanContinue `
            $RuntimeLogPath 'R30 runtime capture readiness response'
        if ($response.ReturnValue -eq $true -and
            $lastError.Contains(
                'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID',
                [StringComparison]::Ordinal)) {
            return $response
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "R30 runtime RC/state endpoint did not become ready: $lastError"
}

function Wait-StableFallbackPose {
    param($Pose, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $firstAcceptedUtc = $null
    $samples = [Collections.Generic.List[object]]::new()
    $lastReport = ''
    do {
        Start-Sleep -Seconds 2
        $response = Invoke-RcCall `
            'GetIstanaExploreV5DR30Player0CaptureState' @{} 60
        $lastReport = [string] $response.OutReport
        $accepted = $response.ReturnValue -eq $true -and
            $lastReport.Contains("poseId=$($Pose.Id)",
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('exactQaViewPose=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('providerFallbackVisualQa=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('providerReadyProofClaimed=false',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('localFallbackHidden=false',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('r30Visible=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('airSimTriadRuntimeLoaded=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('weatherActorResolved=true',
                [StringComparison]::Ordinal)
        if ($accepted) {
            $now = [DateTime]::UtcNow
            if ($null -eq $firstAcceptedUtc) { $firstAcceptedUtc = $now }
            $samples.Add([pscustomobject] [ordered] @{
                ObservedUtc=$now.ToString('o')
                Report=$lastReport
            })
            if ($samples.Count -ge 3 -and
                ($now - $firstAcceptedUtc).TotalSeconds -ge 12.0) {
                return [pscustomobject] [ordered] @{
                    PoseId=$Pose.Id
                    FirstAcceptedUtc=$firstAcceptedUtc.ToString('o')
                    LastAcceptedUtc=$now.ToString('o')
                    StableDurationSeconds=($now - $firstAcceptedUtc).TotalSeconds
                    Samples=@($samples)
                }
            }
        }
        else {
            $firstAcceptedUtc = $null
            $samples.Clear()
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "R30 pose did not hold exact ProviderFallback state for 12 seconds: pose=$($Pose.Id) last={$lastReport}"
}

function Get-PngReceipt {
    param([string] $Path)
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
    $image = $null
    $decoded = $null
    $graphics = $null
    $bitmapData = $null
    try {
        $image = [Drawing.Image]::FromFile($Path)
        if ($image.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or
            $image.Width -ne 2560 -or $image.Height -ne 1440) {
            throw "PNG dimensions/format are not exact: $Path $($image.Width)x$($image.Height)"
        }
        $decoded = [Drawing.Bitmap]::new(
            2560, 1440,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($decoded)
        $graphics.DrawImage($image, 0, 0, 2560, 1440)
        $rectangle = [Drawing.Rectangle]::new(0, 0, 2560, 1440)
        $bitmapData = $decoded.LockBits(
            $rectangle,
            [Drawing.Imaging.ImageLockMode]::ReadOnly,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $byteCount = [Math]::Abs($bitmapData.Stride) * 1440
        $pixels = [byte[]]::new($byteCount)
        [Runtime.InteropServices.Marshal]::Copy(
            $bitmapData.Scan0, $pixels, 0, $byteCount)
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $decodedHash = [Convert]::ToHexString(
                $sha.ComputeHash($pixels))
        }
        finally { $sha.Dispose() }
        $state = Get-FileState $Path
        [pscustomobject] [ordered] @{
            Path=[IO.Path]::GetFullPath($Path)
            Present=$true
            Bytes=$state.Bytes
            Sha256=$state.Sha256
            LastWriteUtc=$state.LastWriteUtc
            WidthPixels=2560
            HeightPixels=1440
            DecodedBgraSha256=$decodedHash
        }
    }
    finally {
        if ($null -ne $bitmapData -and $null -ne $decoded) {
            $decoded.UnlockBits($bitmapData)
        }
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $decoded) { $decoded.Dispose() }
        if ($null -ne $image) { $image.Dispose() }
    }
}

function Wait-StableDecodedPng {
    param([string] $Path, [DateTime] $NotBeforeUtc, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $stable = 0
    $lastIdentity = ''
    $lastError = ''
    do {
        Start-Sleep -Seconds 2
        [void] (Assert-WatchdogHealthy `
            $script:activeOwnedProcess.Watchdog "PNG $Path")
        try {
            $receipt = Get-PngReceipt $Path
            $written = [DateTime]::Parse(
                $receipt.LastWriteUtc,
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::RoundtripKind)
            if ($written -lt $NotBeforeUtc.AddSeconds(-1)) {
                throw "PNG predates its capture request: $Path"
            }
            $identity = "$($receipt.Bytes)|$($receipt.Sha256)|$($receipt.DecodedBgraSha256)"
            if ($identity -ceq $lastIdentity) { ++$stable }
            else { $lastIdentity = $identity; $stable = 1 }
            if ($stable -ge 3) { return $receipt }
        }
        catch {
            $lastError = $_.Exception.Message
            $stable = 0
            $lastIdentity = ''
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "PNG did not become a stable decoded 2560x1440 image: $Path error={$lastError}"
}

function Initialize-BinaryMarkerScannerType {
    if ($null -ne ('Triad.R30Capture.BinaryMarkerScanner' -as [type])) {
        return
    }
    $source = @'
using System;
using System.IO;

namespace Triad.R30Capture
{
    public static class BinaryMarkerScanner
    {
        public static bool Contains(string path, byte[] needle)
        {
            if (String.IsNullOrWhiteSpace(path))
                throw new ArgumentException("Binary path is required.", nameof(path));
            if (needle == null || needle.Length == 0)
                throw new ArgumentException("Binary marker is required.", nameof(needle));

            int[] prefix = new int[needle.Length];
            for (int i = 1, matched = 0; i < needle.Length; ++i)
            {
                while (matched > 0 && needle[i] != needle[matched])
                    matched = prefix[matched - 1];
                if (needle[i] == needle[matched])
                    ++matched;
                prefix[i] = matched;
            }

            byte[] buffer = new byte[4 * 1024 * 1024];
            int state = 0;
            using (FileStream stream = new FileStream(
                path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite,
                buffer.Length, FileOptions.SequentialScan))
            {
                int read;
                while ((read = stream.Read(buffer, 0, buffer.Length)) > 0)
                {
                    for (int i = 0; i < read; ++i)
                    {
                        while (state > 0 && buffer[i] != needle[state])
                            state = prefix[state - 1];
                        if (buffer[i] == needle[state])
                            ++state;
                        if (state == needle.Length)
                            return true;
                    }
                }
            }
            return false;
        }
    }
}
'@
    Add-Type -TypeDefinition $source -Language CSharp -ErrorAction Stop
}

function Test-FileContainsByteSequence {
    param([string] $Path, [byte[]] $Needle)
    Initialize-BinaryMarkerScannerType
    [Triad.R30Capture.BinaryMarkerScanner]::Contains($Path, $Needle)
}

function Assert-BinaryMarkers {
    param([string] $Path, [string[]] $Markers)
    foreach ($marker in $Markers) {
        $ascii = [Text.Encoding]::ASCII.GetBytes($marker)
        $unicode = [Text.Encoding]::Unicode.GetBytes($marker)
        if (-not (Test-FileContainsByteSequence $Path $ascii) -and
            -not (Test-FileContainsByteSequence $Path $unicode)) {
            throw "Game binary lacks current-source marker: $marker"
        }
    }
}

function Assert-NoFatalRuntimeLogText {
    param(
        [string] $Text,
        [string] $Label,
        [switch] $AllowExpectedColdCookShaderCompile
    )
    foreach ($pattern in @(
        'Fatal error:',
        'LogWindows: Error:',
        'Failed to compile Material',
        'ShaderCompileWorker terminated unexpectedly',
        'Falling back to directly compiling',
        'Missing shader map',
        'Failed to load package',
        "Can't find file for asset",
        'R30 facade provider mirror failed closed',
        'ISTANA_EXPLORE_V5_BEGIN_PLAY_REAPPLY_FAILED',
        'ISTANA_EXPLORE_V5D_HYBRID_INVALID',
        'ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID',
        'ISTANA_EXPLORE_V5D_R24_MACDONALD_ASSET_FAIL_CLOSED',
        'ISTANA_EXPLORE_V5D_R24_MACDONALD_INVALID',
        'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSET_FAIL_CLOSED',
        'ISTANA_EXPLORE_V5D_R24_TEMASEK_INVALID',
        'ISTANA_EXPLORE_V5D_TREE_REALISM_INVALID',
        'ISTANA_EXPLORE_V4_INVALID',
        'V5D ground/vegetation runtime failed closed',
        'V5D terrain/provider/turf/edge presentation failed closed',
        'V5D tree realism runtime failed closed',
        'V5D tree material response failed closed',
        'R30_CAPTURE_STATE_INVALID')) {
        if ($Text.Contains($pattern, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$Label contains fail-closed marker: $pattern"
        }
    }

    $cachedShaderMapMarker = 'Missing cached shadermap'
    if (-not $Text.Contains(
            $cachedShaderMapMarker, [StringComparison]::OrdinalIgnoreCase)) {
        return
    }
    if (-not $AllowExpectedColdCookShaderCompile) {
        throw "$Label contains fail-closed marker: $cachedShaderMapMarker"
    }

    $expectedColdCookShaderCompilePattern =
        '^\[[^\r\n]+\]\[\s*\d+\]LogMaterial: Display: ' +
        'Missing cached shadermap for [^\r\n]+ ' +
        '\(DDC key hash: [0-9A-Fa-f]{40}\), compiling\.' +
        '(?: Is special engine material\.)?\s*$'
    foreach ($line in [regex]::Split($Text, '\r?\n')) {
        if ($line.Contains(
                $cachedShaderMapMarker,
                [StringComparison]::OrdinalIgnoreCase) -and
            -not [regex]::IsMatch(
                $line,
                $expectedColdCookShaderCompilePattern,
                [Text.RegularExpressions.RegexOptions]::CultureInvariant)) {
            throw "$Label contains noncanonical cold-cook shadermap marker: $line"
        }
    }
}

function Assert-NoFatalRuntimeLog {
    param(
        [string] $Path,
        [string] $Label,
        [switch] $AllowExpectedColdCookShaderCompile
    )
    $text = Read-RuntimeLogTextShared -Path $Path -Label $Label
    Assert-NoFatalRuntimeLogText `
        -Text $text `
        -Label $Label `
        -AllowExpectedColdCookShaderCompile:$AllowExpectedColdCookShaderCompile
}

function Assert-RequiredPlayer0RuntimeContractMarkersText {
    param([string] $Text, [string] $Label)
    $requiredMarkers = @(
        'ISTANA_EXPLORE_V5D_FOUNTAIN_VALID',
        'ISTANA_EXPLORE_V5D_TREE_REALISM_VALID',
        'ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID')
    foreach ($marker in $requiredMarkers) {
        if (-not $Text.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Label lacks required packaged-runtime marker: $marker"
        }
    }
    [pscustomobject] [ordered] @{
        RequiredMarkerCount=$requiredMarkers.Count
        RequiredMarkers=@($requiredMarkers)
        AllRequiredMarkersPresent=$true
    }
}

function Assert-RequiredPlayer0RuntimeContractMarkers {
    param([string] $Path, [string] $Label)
    Assert-RequiredPlayer0RuntimeContractMarkersText `
        -Text (Read-RuntimeLogTextShared -Path $Path -Label $Label) `
        -Label $Label
}

function Assert-HybridCaptureStateResponse {
    param($Response, [string] $Label)
    $report = [string] (Get-RequiredPropertyValue `
        $Response 'OutReport' $Label)
    if ((Get-RequiredPropertyValue $Response 'ReturnValue' $Label) -ne $true -or
        -not $report.Contains(
            'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID',
            [StringComparison]::Ordinal) -or
        -not $report.Contains('contextPolicyOwner=1',
            [StringComparison]::Ordinal) -or
        -not $report.Contains('contextPolicyShell=R25Inherited',
            [StringComparison]::Ordinal)) {
        throw "$Label lacks the successful RC state that transitively executes ValidateHybridContext."
    }
    [pscustomobject] [ordered] @{
        CaptureStateMarker='ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID'
        ContextPolicyOwnerCount=1
        ContextPolicyShell='R25Inherited'
        ValidateHybridContextExecutedTransitively=$true
        Report=$report
    }
}

function ConvertTo-NormalizedRuntimePath {
    param([string] $Path, [string] $Label)
    $candidate = $Path.Trim().Trim('"')
    if ([string]::IsNullOrWhiteSpace($candidate) -or
        -not [IO.Path]::IsPathFullyQualified($candidate)) {
        throw "$Label is not a fully qualified runtime path: $Path"
    }
    [IO.Path]::GetFullPath($candidate).TrimEnd([char[]] @('\', '/'))
}

function Assert-IsolatedDdcRuntimeLogText {
    param(
        [string] $Text,
        [string] $Label,
        [string] $ExpectedDdcRoot
    )
    $ddcPattern =
        '(?im)^(?:\[[^\r\n]+\]\[\s*\d+\])?' +
        'LogDerivedDataCache: (?:Display: )?' +
        'Local: Using data cache path (?<CachePath>.+): Writable\s*$'
    $ddcMatches = [regex]::Matches($Text, $ddcPattern)
    if ($ddcMatches.Count -ne 1) {
        throw "$Label must contain exactly one writable local DDC selection; found=$($ddcMatches.Count)."
    }
    $expected = ConvertTo-NormalizedRuntimePath `
        $ExpectedDdcRoot "$Label expected isolated DDC root"
    $actual = ConvertTo-NormalizedRuntimePath `
        $ddcMatches[0].Groups['CachePath'].Value `
        "$Label selected local DDC root"
    if (-not $actual.Equals(
            $expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label selected a non-isolated writable local DDC: expected=$expected actual=$actual"
    }

    [pscustomobject] [ordered] @{
        Status='ISOLATED_DDC_RUNTIME_LOG_VALID'
        ExpectedDdcRoot=$expected
        SelectedDdcRoot=$actual
        ExactWritableLocalDdcSelectionCount=1
    }
}

function Assert-IsolatedDdcRuntimeLog {
    param(
        [string] $Path,
        [string] $Label,
        [string] $ExpectedDdcRoot
    )
    $text = Read-RuntimeLogTextShared -Path $Path -Label $Label
    Assert-IsolatedDdcRuntimeLogText `
        -Text $text `
        -Label $Label `
        -ExpectedDdcRoot $ExpectedDdcRoot
}

function Assert-DedicatedShaderWorkerRuntimeLogText {
    param(
        [string] $Text,
        [string] $Label,
        [switch] $AllowNoShaderJobs,
        [switch] $AllowCleanBoundedShaderBatchWithoutJobCacheStats
    )
    if ($AllowNoShaderJobs -and
        $AllowCleanBoundedShaderBatchWithoutJobCacheStats) {
        throw "$Label requested mutually exclusive shader-stat exceptions."
    }
    $commandPattern =
        '(?im)^LogInit: Command Line:\s+(?<Command>[^\r\n]+)\s*$'
    $commandMatches = [regex]::Matches($Text, $commandPattern)
    if ($commandMatches.Count -ne 1) {
        throw "$Label must contain exactly one Unreal command line; found=$($commandMatches.Count)."
    }
    $commandLine = $commandMatches[0].Groups['Command'].Value
    if ($commandLine.Contains(
            '-NoShaderWorker', [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label used the forbidden in-process shader-compiler debugging override."
    }
    if ($commandLine.Contains(
            'r.ShaderCompiler.JobCache=0',
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label disabled the shader job cache required by the external-worker path."
    }
    foreach ($fallbackMarker in @(
        'ShaderCompileWorker terminated unexpectedly',
        'Falling back to directly compiling')) {
        if ($Text.Contains(
                $fallbackMarker, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$Label contains external shader-worker fallback marker: $fallbackMarker"
        }
    }
    foreach ($required in @(
        '-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true',
        '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8',
        '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8')) {
        if (-not $commandLine.Contains(
                $required, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$Label command line lacks dedicated-worker safety token: $required"
        }
    }
    $workerPattern =
        '(?im)^(?:\[[^\r\n]+\]\[\s*\d+\])?' +
        'LogShaderCompilers: Display: Using Local Shader Compiler with 8 workers\.\s*$'
    $workerCount = [regex]::Matches($Text, $workerPattern).Count
    if ($workerCount -ne 1) {
        throw "$Label must prove exactly eight local shader workers; found=$workerCount."
    }
    $jobCacheStatsPattern =
        '(?im)^(?:\[[^\r\n]+\]\[\s*\d+\])?' +
        'LogShaderCompilers: Display: === FShaderJobCache stats ===\s*$'
    $jobCacheStatsCount = [regex]::Matches(
        $Text, $jobCacheStatsPattern).Count
    $missingShaderMapCount = [regex]::Matches(
        $Text, '(?im)^.*Missing cached shadermap .*$').Count
    $shadersLeftZeroCount = [regex]::Matches(
        $Text,
        '(?im)^.*LogShaderCompilers: Display: Shaders left to compile 0\s*$').Count
    $successZeroErrorsCount = [regex]::Matches(
        $Text,
        '(?im)^.*LogInit: Display: Success - 0 error\(s\), [0-9]+ warning\(s\)\s*$').Count
    $compileStatsWriteCount = [regex]::Matches(
        $Text,
        '(?im)^.*LogShaderCompilers: Wrote shader compile stats to file .*$').Count
    $usedNoShaderJobsException = $false
    $usedCleanBoundedBatchException = $false
    if ($jobCacheStatsCount -lt 1) {
        if ($AllowNoShaderJobs) {
            if ($missingShaderMapCount -ne 0 -or
                $shadersLeftZeroCount -ne 1 -or
                $successZeroErrorsCount -ne 1) {
                throw "$Label did not prove a clean zero-shader-job completion."
            }
            $usedNoShaderJobsException = $true
        }
        elseif ($AllowCleanBoundedShaderBatchWithoutJobCacheStats) {
            # UE 5.5 can omit the FShaderJobCache table for a tiny batch even
            # though external workers compiled it. Admit only 1..6 missing-map
            # requests, a written compiler-stat artifact, and a clean drain.
            if ($missingShaderMapCount -lt 1 -or
                $missingShaderMapCount -gt 6 -or
                $compileStatsWriteCount -lt 1 -or
                $shadersLeftZeroCount -ne 1 -or
                $successZeroErrorsCount -ne 1) {
                throw "$Label did not prove a clean bounded shader batch without a job-cache table."
            }
            $usedCleanBoundedBatchException = $true
        }
        else {
            throw "$Label lacks enabled shader-job-cache runtime statistics."
        }
    }
    [pscustomobject] [ordered] @{
        Status='DEDICATED_SHADER_WORKER_RUNTIME_LOG_VALID'
        InProcessNoShaderWorkerAbsent=$true
        ShaderJobCacheDisableOverrideAbsent=$true
        ShaderJobCacheEnabledRuntimeStatsCount=$jobCacheStatsCount
        MissingShaderMapCount=$missingShaderMapCount
        NoShaderJobsAllowed=[bool] $AllowNoShaderJobs
        UsedNoShaderJobsException=$usedNoShaderJobsException
        CleanBoundedShaderBatchWithoutJobCacheStatsAllowed=
            [bool] $AllowCleanBoundedShaderBatchWithoutJobCacheStats
        UsedCleanBoundedShaderBatchWithoutJobCacheStatsException=
            $usedCleanBoundedBatchException
        CompilingThroughWorkersExplicitlyEnabled=$true
        NumUnusedShaderCompilingThreads=8
        NumUnusedShaderCompilingThreadsDuringGame=8
        ExactEightWorkerLogMarkerCount=$workerCount
    }
}

function Assert-DedicatedShaderWorkerRuntimeLog {
    param(
        [string] $Path,
        [string] $Label,
        [switch] $AllowNoShaderJobs,
        [switch] $AllowCleanBoundedShaderBatchWithoutJobCacheStats
    )
    Assert-DedicatedShaderWorkerRuntimeLogText `
        -Text (Read-RuntimeLogTextShared -Path $Path -Label $Label) `
        -Label $Label `
        -AllowNoShaderJobs:$AllowNoShaderJobs `
        -AllowCleanBoundedShaderBatchWithoutJobCacheStats:$AllowCleanBoundedShaderBatchWithoutJobCacheStats
}

function Assert-DedicatedShaderWorkerLogPolicyContract {
    $positive = @'
LogInit: Command Line: D:\triad\TRIAD\TRIAD.uproject -run=Cook -ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8 -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8
[2026.09.08-08.30.00:000][  0]LogShaderCompilers: Display: Using Local Shader Compiler with 8 workers.
[2026.09.08-08.30.00:001][  0]LogShaderCompilers: Display: === FShaderJobCache stats ===
'@
    [void] (Assert-DedicatedShaderWorkerRuntimeLogText `
        -Text $positive `
        -Label 'dedicated shader-worker positive regression')
$noShaderJobs = @'
LogInit: Command Line: D:\triad\TRIAD\TRIAD.uproject -run=Cook -ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8 -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8
[2026.09.08-08.30.00:000][  0]LogShaderCompilers: Display: Using Local Shader Compiler with 8 workers.
[2026.09.08-08.30.00:001][  0]LogInit: Display: Success - 0 error(s), 1 warning(s)
[2026.09.08-08.30.00:002][  0]LogShaderCompilers: Display: Shaders left to compile 0
'@
    [void] (Assert-DedicatedShaderWorkerRuntimeLogText `
        -Text $noShaderJobs `
        -Label 'cache-only shader-worker positive regression' `
        -AllowNoShaderJobs)
    $cleanSmallBatch = @'
LogInit: Command Line: D:\triad\TRIAD\TRIAD.uproject -run=Cook -ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8 -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8
[2026.09.08-08.30.00:000][  0]LogShaderCompilers: Display: Using Local Shader Compiler with 8 workers.
[2026.09.08-08.30.00:001][  0]LogMaterial: Display: Missing cached shadermap for M_Example in PCD3D_SM6, Default, SM6, Game (DDC key hash: 0123456789012345678901234567890123456789), compiling.
[2026.09.08-08.30.00:002][  0]LogShaderCompilers: Wrote shader compile stats to file 'D:/triad/TRIAD/Saved/MaterialStats/Stats.csv'.
[2026.09.08-08.30.00:003][  0]LogInit: Display: Success - 0 error(s), 1 warning(s)
[2026.09.08-08.30.00:004][  0]LogShaderCompilers: Display: Shaders left to compile 0
'@
    [void] (Assert-DedicatedShaderWorkerRuntimeLogText `
        -Text $cleanSmallBatch `
        -Label 'bounded shader-batch positive regression' `
        -AllowCleanBoundedShaderBatchWithoutJobCacheStats)
    $rejections = @(
        $positive.Replace(
            '-run=Cook ', '-run=Cook -NoShaderWorker '),
        $positive.Replace(
            '-run=Cook ',
            '-run=Cook -ini:Engine:[ConsoleVariables]:r.ShaderCompiler.JobCache=0 '),
        $positive.Replace(
            'bAllowCompilingThroughWorkers=true ', ''),
        $positive.Replace(
            'NumUnusedShaderCompilingThreads=8 ', ''),
        $positive.Replace(
            'NumUnusedShaderCompilingThreadsDuringGame=8',
            'NumUnusedShaderCompilingThreadsDuringGame=7'),
        $positive.Replace('with 8 workers.', 'with 7 workers.'),
        $positive.Replace(
            '[2026.09.08-08.30.00:001][  0]LogShaderCompilers: Display: === FShaderJobCache stats ===',
            ''),
        ($positive + "`nLogShaderCompilers: Error: ShaderCompileWorker terminated unexpectedly"),
        ($positive + "`nLogShaderCompilers: Display: Falling back to directly compiling"),
        ($positive + "`n" +
            '[2026.09.08-08.30.00:002][  0]LogShaderCompilers: Display: Using Local Shader Compiler with 8 workers.')
    )
    for ($rejectionIndex = 0;
         $rejectionIndex -lt $rejections.Count;
         ++$rejectionIndex) {
        $text = $rejections[$rejectionIndex]
        $rejected = $false
        try {
            [void] (Assert-DedicatedShaderWorkerRuntimeLogText `
                -Text $text `
                -Label 'dedicated shader-worker negative regression')
        }
        catch { $rejected = $true }
        if (-not $rejected) {
            throw "Dedicated shader-worker policy accepted forbidden form index=$rejectionIndex."
        }
    }
    foreach ($narrowExceptionRejection in @(
        [pscustomobject] @{
            Text=($noShaderJobs + "`n" +
                '[2026.09.08-08.30.00:003][  0]LogMaterial: Display: Missing cached shadermap for M_Unexpected in PCD3D_SM6, Default, SM6, Game (DDC key hash: 0123456789012345678901234567890123456789), compiling.')
            Switch='NO_SHADER_JOBS'
        },
        [pscustomobject] @{
            Text=$cleanSmallBatch.Replace(
                "[2026.09.08-08.30.00:002][  0]LogShaderCompilers: Wrote shader compile stats to file 'D:/triad/TRIAD/Saved/MaterialStats/Stats.csv'.`n",
                '')
            Switch='BOUNDED_BATCH'
        })) {
        $rejected = $false
        try {
            if ($narrowExceptionRejection.Switch -ceq 'NO_SHADER_JOBS') {
                [void] (Assert-DedicatedShaderWorkerRuntimeLogText `
                    -Text $narrowExceptionRejection.Text `
                    -Label 'no-shader-jobs negative regression' `
                    -AllowNoShaderJobs)
            }
            else {
                [void] (Assert-DedicatedShaderWorkerRuntimeLogText `
                    -Text $narrowExceptionRejection.Text `
                    -Label 'bounded shader-batch negative regression' `
                    -AllowCleanBoundedShaderBatchWithoutJobCacheStats)
            }
        }
        catch { $rejected = $true }
        if (-not $rejected) {
            throw "Dedicated shader-worker policy accepted a broad exception: $($narrowExceptionRejection.Switch)."
        }
    }
    [pscustomobject] [ordered] @{
        PositiveFormsAccepted=3
        ForbiddenFormsRejected=($rejections.Count + 2)
        InProcessNoShaderWorkerForbidden=$true
        ShaderWorkerCrashAndDirectFallbackForbidden=$true
        ShaderJobCacheDisableOverrideForbidden=$true
        EnabledShaderJobCacheRuntimeStatsRequired=$true
        CacheOnlyNoShaderJobsExceptionNarrow=$true
        CleanBoundedShaderBatchWithoutStatsExceptionNarrow=$true
        CompilingThroughWorkersExplicitlyEnabled=$true
        ExactLocalShaderWorkerCount=8
    }
}

function Assert-TreeDerivativeMeshPrewarmBuildLogText {
    param(
        [string] $Text,
        [string] $PackagePath,
        [string] $Label
    )
    if ([string]::IsNullOrWhiteSpace($Text) -or
        [string]::IsNullOrWhiteSpace($PackagePath) -or
        $PackagePath -notmatch '^/Game/.+/SM_IPV5D_Tree_[A-Za-z0-9_]+_NearLOD0$') {
        throw "$Label received an invalid TreeRealism prewarm package."
    }
    $assetName = $PackagePath.Substring($PackagePath.LastIndexOf('/') + 1)
    $escapedAssetName = [regex]::Escape($assetName)
    $objectPath = "$PackagePath.$assetName"
    $escapedObjectPath = [regex]::Escape($objectPath)
    $buildMatches = [regex]::Matches(
        $Text,
        "(?m)LogStaticMesh: Display: Building static mesh $escapedAssetName \(Required Memory Estimate: (?<Megabytes>[0-9]+(?:\.[0-9]+)?) MB\)\.\.\.")
    $builtMatches = [regex]::Matches(
        $Text,
        "(?m)LogStaticMesh: Built static mesh \[[0-9]+(?:\.[0-9]+)?s\] $escapedObjectPath\r?$")
    if ($buildMatches.Count -ne 1 -or $builtMatches.Count -ne 1) {
        throw "$Label did not cold-build exactly one copy of $objectPath."
    }
    [pscustomobject] [ordered] @{
        Status='TREE_DERIVATIVE_MESH_PREWARM_BUILD_VALID'
        PackagePath=$PackagePath
        ObjectPath=$objectPath
        RequiredMemoryEstimateMegabytes=[double] $buildMatches[0].Groups['Megabytes'].Value
        BuildMarkerCount=$buildMatches.Count
        BuiltMarkerCount=$builtMatches.Count
    }
}

function Assert-TreeDerivativeMeshPrewarmBuildLog {
    param(
        [string] $Path,
        [string] $PackagePath,
        [string] $Label
    )
    Assert-TreeDerivativeMeshPrewarmBuildLogText `
        -Text (Read-RuntimeLogTextShared -Path $Path -Label $Label) `
        -PackagePath $PackagePath `
        -Label $Label
}

function Assert-TreeDerivativeMeshPrewarmResultLog {
    param(
        [string] $Path,
        [string] $PackagePath,
        [string] $Label
    )
    $text = Read-RuntimeLogTextShared -Path $Path -Label $Label
    $assetName = $PackagePath.Substring($PackagePath.LastIndexOf('/') + 1)
    $objectPath = "$PackagePath.$assetName"
    $escapedAssetName = [regex]::Escape($assetName)
    $escapedObjectPath = [regex]::Escape($objectPath)
    $buildMatches = [regex]::Matches(
        $text,
        "(?m)LogStaticMesh: Display: Building static mesh $escapedAssetName \(Required Memory Estimate: (?<Megabytes>[0-9]+(?:\.[0-9]+)?) MB\)\.\.\.")
    $builtMatches = [regex]::Matches(
        $text,
        "(?m)LogStaticMesh: Built static mesh \[[0-9]+(?:\.[0-9]+)?s\] $escapedObjectPath\r?$")
    $hitMatches = [regex]::Matches(
        $text,
        "(?m)LogStaticMesh: Verbose: Static mesh found in DDC \[[0-9]+(?:\.[0-9]+)?ms\] $escapedObjectPath\r?$")
    $mode = if ($buildMatches.Count -eq 1 -and
        $builtMatches.Count -eq 1) {
        if ($hitMatches.Count -gt 0) {
            'COLD_BUILD_AFTER_OPTIONAL_DDC_HIT'
        }
        else { 'COLD_BUILD' }
    }
    elseif ($buildMatches.Count -eq 0 -and
        $builtMatches.Count -eq 0 -and $hitMatches.Count -ge 1) {
        'EXACT_DDC_HIT'
    }
    else {
        throw "$Label lacks exactly one cold-build or exact-DDC-hit result for $objectPath."
    }
    [pscustomobject] [ordered] @{
        Status='TREE_DERIVATIVE_MESH_PREWARM_RESULT_VALID'
        Mode=$mode
        PackagePath=$PackagePath
        ObjectPath=$objectPath
        RequiredMemoryEstimateMegabytes=if ($buildMatches.Count -eq 1) {
            [double] $buildMatches[0].Groups['Megabytes'].Value
        } else { 0.0 }
        BuildMarkerCount=$buildMatches.Count
        BuiltMarkerCount=$builtMatches.Count
        DdcHitMarkerCount=$hitMatches.Count
    }
}

function Assert-TreeDerivativeMeshDdcHitLogText {
    param(
        [string] $Text,
        [string[]] $PackagePaths,
        [string] $Label
    )
    if ([string]::IsNullOrWhiteSpace($Text) -or
        $null -eq $PackagePaths -or $PackagePaths.Count -ne 5) {
        throw "$Label received an invalid TreeRealism DDC-hit roster."
    }
    $receipts = [Collections.Generic.List[object]]::new()
    foreach ($packagePath in $PackagePaths) {
        $assetName = $packagePath.Substring($packagePath.LastIndexOf('/') + 1)
        $escapedAssetName = [regex]::Escape($assetName)
        $objectPath = "$packagePath.$assetName"
        $hitMatches = [regex]::Matches(
            $Text,
            "(?m)LogStaticMesh: Verbose: Static mesh found in DDC \[[0-9]+(?:\.[0-9]+)?ms\] $([regex]::Escape($objectPath))\r?$")
        $buildMissMatches = [regex]::Matches(
            $Text,
            "(?m)LogStaticMesh: Display: Building static mesh $escapedAssetName ")
        if ($hitMatches.Count -lt 1 -or $buildMissMatches.Count -ne 0) {
            throw "$Label lacks an exact cache-only DDC hit for $objectPath."
        }
        $receipts.Add([pscustomobject] [ordered] @{
            PackagePath=$packagePath
            ObjectPath=$objectPath
            DdcHitMarkerCount=$hitMatches.Count
            BuildMissMarkerCount=$buildMissMatches.Count
        })
    }
    [pscustomobject] [ordered] @{
        Status='TREE_DERIVATIVE_MESH_DDC_HITS_VALID'
        PackageCount=$receipts.Count
        CacheOnly=$true
        Packages=@($receipts)
    }
}

function Assert-TreeDerivativeMeshDdcHitLog {
    param(
        [string] $Path,
        [string[]] $PackagePaths,
        [string] $Label
    )
    Assert-TreeDerivativeMeshDdcHitLogText `
        -Text (Read-RuntimeLogTextShared -Path $Path -Label $Label) `
        -PackagePaths $PackagePaths `
        -Label $Label
}

function Assert-TreeDerivativeMeshDdcPrewarmPolicyContract {
    $buildPackage = $treeDerivativeMeshPrewarmPackages[0]
    $buildName = $buildPackage.Substring($buildPackage.LastIndexOf('/') + 1)
    $buildText = @"
LogStaticMesh: Display: Building static mesh $buildName (Required Memory Estimate: 6119.359938 MB)...
LogStaticMesh: Built static mesh [12.34s] $buildPackage.$buildName
"@
    [void] (Assert-TreeDerivativeMeshPrewarmBuildLogText `
        -Text $buildText `
        -PackagePath $buildPackage `
        -Label 'TreeRealism prewarm-build positive regression')

    $hitLines = @($treeDerivativeMeshPrewarmPackages | ForEach-Object {
        $name = $_.Substring($_.LastIndexOf('/') + 1)
        "LogStaticMesh: Verbose: Static mesh found in DDC [0.25ms] $_.$name"
    })
    $hitText = [string]::Join([Environment]::NewLine, $hitLines)
    [void] (Assert-TreeDerivativeMeshDdcHitLogText `
        -Text $hitText `
        -PackagePaths $treeDerivativeMeshPrewarmPackages `
        -Label 'TreeRealism DDC-hit positive regression')

    $rejectedBuild = $false
    try {
        [void] (Assert-TreeDerivativeMeshPrewarmBuildLogText `
            -Text $buildText.Replace('Built static mesh', 'Skipped static mesh') `
            -PackagePath $buildPackage `
            -Label 'TreeRealism prewarm-build negative regression')
    }
    catch { $rejectedBuild = $true }
    $rejectedHit = $false
    try {
        [void] (Assert-TreeDerivativeMeshDdcHitLogText `
            -Text ($hitText + [Environment]::NewLine +
                "LogStaticMesh: Display: Building static mesh $buildName (Required Memory Estimate: 6119.359938 MB)...") `
            -PackagePaths $treeDerivativeMeshPrewarmPackages `
            -Label 'TreeRealism DDC-hit negative regression')
    }
    catch { $rejectedHit = $true }
    if (-not $rejectedBuild -or -not $rejectedHit) {
        throw 'TreeRealism DDC-prewarm negative regression was accepted.'
    }
    [pscustomobject] [ordered] @{
        IsolatedPerMeshProcessRequired=$true
        LargestMeshFirst=$true
        ExactPackageCount=5
        HashPinnedAccelerationSeedFileCount=2
        BoundedPopulationProofRequired=$true
        SeededLargestMeshColdBuildForbidden=$true
        LiveBuildMissSentinelPollMilliseconds=100
        CacheOnlyProbeRequired=$true
        FullCookCacheOnlyReuseRequired=$true
        NegativeFormsRejected=2
        VisualAssetMutationAllowed=$false
    }
}

function Assert-RuntimeIsolationLogPolicyContract {
    $expectedDdcRoot = [IO.Path]::GetFullPath(
        'C:\TRIAD_R30Evidence\static-contract\DDC')
    $ddcLine =
        '[2026.09.08-06.00.00:000][  0]' +
        'LogDerivedDataCache: Display: Local: Using data cache path ' +
        "${expectedDdcRoot}: Writable"
    [void] (Assert-IsolatedDdcRuntimeLogText `
        $ddcLine 'cook isolation positive regression' `
        $expectedDdcRoot)
    [void] (Assert-IsolatedDdcRuntimeLogText `
        $ddcLine 'game isolation positive regression' $expectedDdcRoot)

    $rejections = @(
        $ddcLine.Replace($expectedDdcRoot, 'C:\SharedDDC'),
        "$ddcLine`n$ddcLine",
        'LogInit: Display: no DDC line'
    )
    foreach ($text in $rejections) {
        $rejected = $false
        try {
            [void] (Assert-IsolatedDdcRuntimeLogText `
                $text 'runtime-isolation negative regression' `
                $expectedDdcRoot)
        }
        catch { $rejected = $true }
        if (-not $rejected) {
            throw 'Runtime-isolation log policy accepted a forbidden form.'
        }
    }
    [pscustomobject] [ordered] @{
        PositiveFormsAccepted=2
        ForbiddenFormsRejected=$rejections.Count
        ChildOnlyDdcSelectionRequired=$true
        ShaderJobCachePolicyDelegatedToDedicatedWorkerContract=$true
    }
}

function Assert-RuntimeLogPolicyContract {
    $ordinaryColdCook =
        '[2026.09.08-05.26.16:728][  0]LogMaterial: Display: ' +
        'Missing cached shadermap for M_ArrowColor in PCD3D_SM6, ' +
        'Default, SM6, Game (DDC key hash: ' +
        '0c660cfb9c643533c30e4e510d5d4c8fa5ca110b), compiling.'
    $specialEngineColdCook =
        '[2026.09.08-05.26.20:389][  0]LogMaterial: Display: ' +
        'Missing cached shadermap for WorldGridMaterial in PCD3D_SM6, ' +
        'Default, SM6, Game (DDC key hash: ' +
        '2d1499ed0513fb1359155dd3344ac44277b42408), compiling. ' +
        'Is special engine material.'
    Assert-NoFatalRuntimeLogText `
        $ordinaryColdCook 'ordinary cold-cook policy regression' `
        -AllowExpectedColdCookShaderCompile
    Assert-NoFatalRuntimeLogText `
        $specialEngineColdCook 'special-engine cold-cook policy regression' `
        -AllowExpectedColdCookShaderCompile

    $rejections = @(
        [pscustomobject] @{ Text=$ordinaryColdCook; AllowColdCook=$false },
        [pscustomobject] @{
            Text=$ordinaryColdCook.Replace(
                'LogMaterial: Display:', 'LogMaterial: Error:')
            AllowColdCook=$true
        },
        [pscustomobject] @{
            Text=$ordinaryColdCook.Replace(', compiling.', '')
            AllowColdCook=$true
        }
    )
    foreach ($pattern in @(
        'Fatal error:',
        'LogWindows: Error:',
        'Failed to compile Material',
        'ShaderCompileWorker terminated unexpectedly',
        'Falling back to directly compiling',
        'Missing shader map',
        'Failed to load package',
        "Can't find file for asset",
        'R30 facade provider mirror failed closed',
        'ISTANA_EXPLORE_V5_BEGIN_PLAY_REAPPLY_FAILED',
        'ISTANA_EXPLORE_V5D_HYBRID_INVALID',
        'ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID',
        'ISTANA_EXPLORE_V5D_R24_MACDONALD_ASSET_FAIL_CLOSED',
        'ISTANA_EXPLORE_V5D_R24_MACDONALD_INVALID',
        'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSET_FAIL_CLOSED',
        'ISTANA_EXPLORE_V5D_R24_TEMASEK_INVALID',
        'ISTANA_EXPLORE_V5D_TREE_REALISM_INVALID',
        'ISTANA_EXPLORE_V4_INVALID',
        'V5D ground/vegetation runtime failed closed',
        'V5D terrain/provider/turf/edge presentation failed closed',
        'V5D tree realism runtime failed closed',
        'V5D tree material response failed closed',
        'R30_CAPTURE_STATE_INVALID')) {
        $rejections += [pscustomobject] @{
            Text="LogMaterial: Error: $pattern"
            AllowColdCook=$true
        }
    }

    foreach ($case in $rejections) {
        $rejected = $false
        try {
            Assert-NoFatalRuntimeLogText `
                $case.Text 'negative runtime-log policy regression' `
                -AllowExpectedColdCookShaderCompile:$case.AllowColdCook
        }
        catch { $rejected = $true }
        if (-not $rejected) {
            throw "Runtime-log policy accepted forbidden text: $($case.Text)"
        }
    }

    $requiredRuntimeMarkers = @(
        'ISTANA_EXPLORE_V5D_FOUNTAIN_VALID',
        'ISTANA_EXPLORE_V5D_TREE_REALISM_VALID',
        'ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID')
    $requiredMarkerProof = Assert-RequiredPlayer0RuntimeContractMarkersText `
        -Text ([string]::Join("`n", $requiredRuntimeMarkers)) `
        -Label 'positive packaged-runtime marker policy regression'
    $missingRequiredMarkerRejected = $false
    try {
        [void] (Assert-RequiredPlayer0RuntimeContractMarkersText `
            -Text ([string]::Join("`n", $requiredRuntimeMarkers[0..1])) `
            -Label 'negative packaged-runtime marker policy regression')
    }
    catch { $missingRequiredMarkerRejected = $true }
    if (-not $missingRequiredMarkerRejected) {
        throw 'Runtime-log policy accepted a missing required packaged-runtime marker.'
    }

    [pscustomobject] [ordered] @{
        ExpectedColdCookFormsAccepted=2
        ForbiddenFormsRejected=$rejections.Count
        RuntimeCachedShaderMapCompileStillRejected=$true
        MissingShaderMapAlwaysRejected=$true
        OtherFatalMarkersStillRejected=$true
        RequiredPackagedRuntimeMarkers=$requiredMarkerProof
        MissingRequiredPackagedRuntimeMarkerRejected=$true
    }
}

function Get-RequiredPropertyValue {
    param($Object, [string] $Name, [string] $Label)
    if ($null -eq $Object) {
        throw "$Label is null while requiring property '$Name'."
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Label lacks required property '$Name'."
    }
    $property.Value
}

function Assert-ExactPathBoundReceipt {
    param($Receipt, [string] $ExpectedPath, [string] $Label)
    $receiptPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $Receipt 'Path' $Label))
    $fullExpectedPath = [IO.Path]::GetFullPath($ExpectedPath)
    if (-not $receiptPath.Equals(
            $fullExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label path mismatch: expected=$fullExpectedPath actual=$receiptPath"
    }
    $expectedState = [pscustomobject] [ordered] @{
        Present=[bool] (Get-RequiredPropertyValue $Receipt 'Present' $Label)
        Bytes=[int64] (Get-RequiredPropertyValue $Receipt 'Bytes' $Label)
        Sha256=[string] (Get-RequiredPropertyValue $Receipt 'Sha256' $Label)
    }
    $actual = Assert-State $expectedState $fullExpectedPath $Label
    [pscustomobject] [ordered] @{
        Path=$fullExpectedPath
        Present=[bool] $actual.Present
        Bytes=[int64] $actual.Bytes
        Sha256=[string] $actual.Sha256
        LastWriteUtc=[string] $actual.LastWriteUtc
    }
}

function Assert-ExactStateBinding {
    param($Binding, [string] $ExpectedPath, [string] $Label)
    $bindingPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $Binding 'Path' $Label))
    $fullExpectedPath = [IO.Path]::GetFullPath($ExpectedPath)
    if (-not $bindingPath.Equals(
            $fullExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label path mismatch: expected=$fullExpectedPath actual=$bindingPath"
    }
    $state = Get-RequiredPropertyValue $Binding 'State' $Label
    [void] (Assert-State $state $fullExpectedPath $Label)
    [pscustomobject] [ordered] @{
        Path=$fullExpectedPath
        State=$state
    }
}

function Assert-JsonIdentity {
    param($Expected, $Actual, [string] $Label, [int] $Depth=12)
    $expectedJson = ConvertTo-Json $Expected -Depth $Depth -Compress
    $actualJson = ConvertTo-Json $Actual -Depth $Depth -Compress
    if ($expectedJson -cne $actualJson) {
        throw "$Label JSON identity changed."
    }
}

function Assert-HybridRuntimeProofReceipt {
    param($Receipt, [string] $Label)
    $captureStateMarker = [string] (Get-RequiredPropertyValue `
        $Receipt 'CaptureStateMarker' $Label)
    $contextPolicyOwnerCount = [int] (Get-RequiredPropertyValue `
        $Receipt 'ContextPolicyOwnerCount' $Label)
    $contextPolicyShell = [string] (Get-RequiredPropertyValue `
        $Receipt 'ContextPolicyShell' $Label)
    $transitive = Get-RequiredPropertyValue `
        $Receipt 'ValidateHybridContextExecutedTransitively' $Label
    $report = [string] (Get-RequiredPropertyValue $Receipt 'Report' $Label)
    if ($captureStateMarker -cne
            'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID' -or
        $contextPolicyOwnerCount -ne 1 -or
        $contextPolicyShell -cne 'R25Inherited' -or
        $transitive -isnot [bool] -or
        [bool] $transitive -ne $true) {
        throw "$Label does not preserve the exact successful hybrid runtime proof."
    }
    foreach ($marker in @(
        'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID',
        'contextPolicyOwner=1',
        'contextPolicyShell=R25Inherited')) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Label report lacks required transitive hybrid marker: $marker"
        }
    }
    $Receipt
}

function Assert-LandmarkBooleanPropertyReceipt {
    param(
        $Receipt,
        [string] $ExpectedObjectPath,
        [string] $ExpectedPropertyName,
        [bool] $ExpectedValue,
        [string] $Label
    )
    $objectPath = [string] (Get-RequiredPropertyValue `
        $Receipt 'ObjectPath' $Label)
    $propertyName = [string] (Get-RequiredPropertyValue `
        $Receipt 'PropertyName' $Label)
    $value = Get-RequiredPropertyValue $Receipt 'Value' $Label
    $access = [string] (Get-RequiredPropertyValue $Receipt 'Access' $Label)
    $mutationAllowed = Get-RequiredPropertyValue `
        $Receipt 'MutationAllowed' $Label
    if ($objectPath -cne $ExpectedObjectPath -or
        $propertyName -cne $ExpectedPropertyName -or
        $value -isnot [bool] -or
        [bool] $value -ne $ExpectedValue -or
        $access -cne 'READ_ACCESS' -or
        $mutationAllowed -isnot [bool] -or
        [bool] $mutationAllowed -ne $false) {
        throw "$Label is not the exact read-only runtime property receipt."
    }
    $Receipt
}

function Assert-LandmarkRuntimeContractsReceipt {
    param($Receipt, [string] $ExpectedPhase, [string] $Label)
    $phase = [string] (Get-RequiredPropertyValue $Receipt 'Phase' $Label)
    $endpoint = [string] (Get-RequiredPropertyValue `
        $Receipt 'ReadOnlyPropertyEndpoint' $Label)
    $actorCount = [int] (Get-RequiredPropertyValue `
        $Receipt 'ExactActorCount' $Label)
    $fullAuditPassed = Get-RequiredPropertyValue `
        $Receipt 'RuntimeFullAuditPassed' $Label
    if ($phase -cne $ExpectedPhase -or
        $endpoint -cne $rcPropertyUri -or
        $actorCount -ne 2 -or
        $fullAuditPassed -isnot [bool] -or
        [bool] $fullAuditPassed -ne $true) {
        throw "$Label does not preserve the exact landmark runtime-audit envelope."
    }

    foreach ($landmark in @(
        [pscustomobject] [ordered] @{
            ReceiptName='MacDonaldHouse'
            ExpectedLabel='R24 MacDonald House'
            ExpectedPath=$macDonaldRuntimeActorPath
        },
        [pscustomobject] [ordered] @{
            ReceiptName='TemasekShophouse'
            ExpectedLabel='R24 Temasek Shophouse'
            ExpectedPath=$temasekRuntimeActorPath
        })) {
        $snapshot = Get-RequiredPropertyValue `
            $Receipt $landmark.ReceiptName $Label
        if ([string] (Get-RequiredPropertyValue `
                $snapshot 'Label' "$Label $($landmark.ReceiptName)") -cne
                $landmark.ExpectedLabel -or
            [string] (Get-RequiredPropertyValue `
                $snapshot 'ObjectPath' "$Label $($landmark.ReceiptName)") -cne
                $landmark.ExpectedPath) {
            throw "$Label $($landmark.ReceiptName) identity changed."
        }
        foreach ($property in @(
            [pscustomobject] @{
                ReceiptName='RuntimeProviderTelemetryBindingValid'
                PropertyName='bRuntimeProviderTelemetryBindingValid'
                ExpectedValue=$true
            },
            [pscustomobject] @{
                ReceiptName='RuntimeAssetContractFailClosed'
                PropertyName='bRuntimeAssetContractFailClosed'
                ExpectedValue=$false
            },
            [pscustomobject] @{
                ReceiptName='DedicatedOverlayVisible'
                PropertyName='bDedicatedOverlayVisible'
                ExpectedValue=$true
            })) {
            [void] (Assert-LandmarkBooleanPropertyReceipt `
                (Get-RequiredPropertyValue `
                    $snapshot $property.ReceiptName `
                    "$Label $($landmark.ReceiptName)") `
                $landmark.ExpectedPath `
                $property.PropertyName `
                $property.ExpectedValue `
                "$Label $($landmark.ReceiptName) $($property.ReceiptName)")
        }
    }
    $Receipt
}

function Assert-RuntimeEvidenceAcceptancePolicyContract {
    $hybrid = [pscustomobject] [ordered] @{
        CaptureStateMarker=
            'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID'
        ContextPolicyOwnerCount=1
        ContextPolicyShell='R25Inherited'
        ValidateHybridContextExecutedTransitively=$true
        Report=
            'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID contextPolicyOwner=1 contextPolicyShell=R25Inherited'
    }
    [void] (Assert-HybridRuntimeProofReceipt `
        $hybrid 'static valid hybrid runtime proof')

    function New-StaticLandmarkPropertyReceipt {
        param([string] $Path, [string] $Name, [bool] $Value)
        [pscustomobject] [ordered] @{
            ObjectPath=$Path
            PropertyName=$Name
            Value=$Value
            Access='READ_ACCESS'
            MutationAllowed=$false
        }
    }
    function New-StaticLandmarkSnapshot {
        param([string] $Name, [string] $Path)
        [pscustomobject] [ordered] @{
            Label=$Name
            ObjectPath=$Path
            RuntimeProviderTelemetryBindingValid=
                (New-StaticLandmarkPropertyReceipt `
                    $Path 'bRuntimeProviderTelemetryBindingValid' $true)
            RuntimeAssetContractFailClosed=
                (New-StaticLandmarkPropertyReceipt `
                    $Path 'bRuntimeAssetContractFailClosed' $false)
            DedicatedOverlayVisible=
                (New-StaticLandmarkPropertyReceipt `
                    $Path 'bDedicatedOverlayVisible' $true)
        }
    }
    $landmark = [pscustomobject] [ordered] @{
        Phase='PRE_CAPTURE'
        ReadOnlyPropertyEndpoint=$rcPropertyUri
        ExactActorCount=2
        RuntimeFullAuditPassed=$true
        MacDonaldHouse=(New-StaticLandmarkSnapshot `
            'R24 MacDonald House' $macDonaldRuntimeActorPath)
        TemasekShophouse=(New-StaticLandmarkSnapshot `
            'R24 Temasek Shophouse' $temasekRuntimeActorPath)
    }
    [void] (Assert-LandmarkRuntimeContractsReceipt `
        $landmark 'PRE_CAPTURE' 'static valid landmark runtime proof')

    $tamperedFormsRejected = 0
    $tamperedHybrid = $hybrid | ConvertTo-Json -Depth 8 |
        ConvertFrom-Json -Depth 8
    $tamperedHybrid.ValidateHybridContextExecutedTransitively = $false
    try {
        [void] (Assert-HybridRuntimeProofReceipt `
            $tamperedHybrid 'static tampered hybrid runtime proof')
    }
    catch { ++$tamperedFormsRejected }
    $tamperedLandmark = $landmark | ConvertTo-Json -Depth 12 |
        ConvertFrom-Json -Depth 12
    $tamperedLandmark.MacDonaldHouse.
        RuntimeProviderTelemetryBindingValid.Value = $false
    try {
        [void] (Assert-LandmarkRuntimeContractsReceipt `
            $tamperedLandmark 'PRE_CAPTURE' `
            'static tampered landmark runtime proof')
    }
    catch { ++$tamperedFormsRejected }
    if ($tamperedFormsRejected -ne 2) {
        throw 'R30 runtime-evidence acceptance policy did not reject both tampered forms.'
    }
    [pscustomobject] [ordered] @{
        PositiveHybridProofAccepted=$true
        PositiveLandmarkProofAccepted=$true
        TamperedFormsRejected=2
        PendingReceiptRevalidationRequired=$true
        CommittedReceiptCarryForwardRequired=$true
    }
}

function Assert-CookedRuntimeHotfixCommitReceipt {
    param($BaseR30Admission)
    $state = Assert-State `
        $expectedCookedRuntimeHotfixCommitReceipt `
        $cookedRuntimeHotfixCommitReceipt `
        'UE 5.5 cooked-runtime hotfix commit receipt'
    $receiptDirectory = [IO.Path]::GetDirectoryName(
        $cookedRuntimeHotfixCommitReceipt)
    if (-not [IO.Path]::GetDirectoryName($receiptDirectory).Equals(
            $cookedRuntimeHotfixTransactionBase,
            [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($cookedRuntimeHotfixCommitReceipt).Equals(
            'commit.json', [StringComparison]::Ordinal)) {
        throw 'Cooked-runtime hotfix receipt escaped its fixed transaction namespace.'
    }
    $receipt = Get-Content -LiteralPath $cookedRuntimeHotfixCommitReceipt -Raw |
        ConvertFrom-Json -Depth 32
    if ([string] (Get-RequiredPropertyValue `
            $receipt 'Schema' 'cooked-runtime hotfix receipt') -cne
            $cookedRuntimeHotfixTransactionSchema -or
        [string] (Get-RequiredPropertyValue `
            $receipt 'Status' 'cooked-runtime hotfix receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue `
            $receipt 'RunToken' 'cooked-runtime hotfix receipt') -cne
            [IO.Path]::GetFileName($receiptDirectory) -or
        [int] (Get-RequiredPropertyValue `
            $receipt 'SourceCount' 'cooked-runtime hotfix receipt') -ne 6 -or
        [int] (Get-RequiredPropertyValue `
            $receipt 'R30ContentPackageCount' 'cooked-runtime hotfix receipt') -ne 12) {
        throw 'Cooked-runtime hotfix receipt failed its schema/status/roster contract.'
    }

    $parent = Get-RequiredPropertyValue `
        $receipt 'ParentR30Commit' 'cooked-runtime hotfix receipt'
    $parentPath = [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue `
        $parent 'Path' 'cooked-runtime hotfix parent'))
    $parentFile = Get-RequiredPropertyValue `
        $parent 'File' 'cooked-runtime hotfix parent'
    if (-not $parentPath.Equals(
            $BaseR30Admission.Path,
            [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-ExactFileContentState $BaseR30Admission.File $parentFile)) {
        throw 'Cooked-runtime hotfix does not bind the admitted R30 commit exactly.'
    }
    [void] (Assert-State `
        $parentFile $parentPath 'cooked-runtime hotfix parent R30 receipt')

    $mapBefore = Get-RequiredPropertyValue `
        $receipt 'MapBefore' 'cooked-runtime hotfix receipt'
    $mapAfter = Get-RequiredPropertyValue `
        $receipt 'MapAfter' 'cooked-runtime hotfix receipt'
    if (-not (Test-ExactFileContentState $BaseR30Admission.SuccessorMap $mapBefore) -or
        -not (Test-ExactFileContentState $BaseR30Admission.SuccessorMap $mapAfter)) {
        throw 'Cooked-runtime hotfix map binding drifted from R30.'
    }
    [void] (Assert-State $mapAfter $mapFile 'cooked-runtime hotfix current R30 map')

    $runtimeBefore = Get-RequiredPropertyValue `
        $receipt 'RuntimeDllBefore' 'cooked-runtime hotfix receipt'
    $runtimeAfter = Get-RequiredPropertyValue `
        $receipt 'RuntimeDllAfter' 'cooked-runtime hotfix receipt'
    $editorBefore = Get-RequiredPropertyValue `
        $receipt 'EditorDllBefore' 'cooked-runtime hotfix receipt'
    $editorAfter = Get-RequiredPropertyValue `
        $receipt 'EditorDllAfter' 'cooked-runtime hotfix receipt'
    if (-not (Test-ExactFileContentState `
            $BaseR30Admission.RuntimeDllAfter $runtimeBefore) -or
        -not (Test-ExactFileContentState `
            $BaseR30Admission.EditorDllAfter $editorBefore) -or
        -not (Test-ExactFileContentState $editorBefore $editorAfter) -or
        (Test-ExactFileContentState $runtimeBefore $runtimeAfter)) {
        throw 'Cooked-runtime hotfix DLL predecessor/successor binding is invalid.'
    }
    [void] (Assert-State `
        $runtimeBefore ([string] $runtimeBefore.Path) `
        'cooked-runtime hotfix runtime DLL rollback preimage')
    [void] (Assert-State `
        $editorBefore ([string] $editorBefore.Path) `
        'cooked-runtime hotfix editor DLL rollback preimage')
    [void] (Assert-State `
        $runtimeAfter $runtimeEditorDll 'current cooked-runtime hotfix runtime DLL')
    [void] (Assert-State `
        $editorAfter $editorDll 'current cooked-runtime hotfix editor DLL')

    $sourcePins = @(Get-RequiredPropertyValue `
        $receipt 'SourcePins' 'cooked-runtime hotfix receipt')
    if ($sourcePins.Count -ne 6 -or
        @($sourcePins.RelativePath | Sort-Object -Unique).Count -ne 6) {
        throw 'Cooked-runtime hotfix source roster is not exactly six unique files.'
    }
    foreach ($pin in $sourcePins) {
        $relativePath = [string] (Get-RequiredPropertyValue `
            $pin 'RelativePath' 'cooked-runtime hotfix source pin')
        if ([IO.Path]::IsPathRooted($relativePath) -or
            $relativePath.Contains('..', [StringComparison]::Ordinal)) {
            throw "Cooked-runtime hotfix source path is unsafe: $relativePath"
        }
        $target = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relativePath))
        if (-not (Test-ContainedPath $target $nativePluginSourceRoot)) {
            throw "Cooked-runtime hotfix source escaped plugin source: $target"
        }
        $before = Get-RequiredPropertyValue `
            $pin 'Before' "cooked-runtime hotfix source $relativePath"
        $after = Get-RequiredPropertyValue `
            $pin 'After' "cooked-runtime hotfix source $relativePath"
        [void] (Assert-State `
            $before ([string] $before.Path) `
            "cooked-runtime hotfix rollback source $relativePath")
        [void] (Assert-State `
            $after $target "current cooked-runtime hotfix source $relativePath")
    }

    foreach ($truthRow in @(
        [pscustomobject] @{ Name='MapModified'; Expected=$false },
        [pscustomobject] @{ Name='ContentModified'; Expected=$false },
        [pscustomobject] @{ Name='EditorModuleDllModified'; Expected=$false },
        [pscustomobject] @{ Name='RuntimeModuleDllModified'; Expected=$true },
        [pscustomobject] @{ Name='ImportedMaterialSlotNameEditorOnlyGuarded'; Expected=$true },
        [pscustomobject] @{ Name='CookedProvenanceUsesCanonicalMaterialSlotNameSurrogate'; Expected=$true },
        [pscustomobject] @{ Name='TreeMaterialDelegationRequiresExactValidatedV5DOverride'; Expected=$true },
        [pscustomobject] @{ Name='SimulationCollisionNavigationSensorRfAuthorityModified'; Expected=$false },
        [pscustomobject] @{ Name='GeospatialLayoutModified'; Expected=$false },
        [pscustomobject] @{ Name='PackagedGameProofRequired'; Expected=$true },
        [pscustomobject] @{ Name='VisualAcceptanceClaimed'; Expected=$false },
        [pscustomobject] @{ Name='HyperrealismClaimed'; Expected=$false })) {
        if ([bool] (Get-RequiredPropertyValue `
                $receipt $truthRow.Name 'cooked-runtime hotfix receipt') -ne
            [bool] $truthRow.Expected) {
            throw "Cooked-runtime hotfix truth field drifted: $($truthRow.Name)"
        }
    }
    [pscustomobject] [ordered] @{
        Path=$cookedRuntimeHotfixCommitReceipt
        File=$state
        Receipt=$receipt
        ParentR30Commit=$parent
        SuccessorMap=$mapAfter
        RuntimeDllBefore=$runtimeBefore
        RuntimeDllAfter=$runtimeAfter
        EditorDllBefore=$editorBefore
        EditorDllAfter=$editorAfter
        SourcePins=@($sourcePins)
        SourceCount=6
        MapModified=$false
        ContentModified=$false
        SimulationCollisionNavigationSensorRfAuthorityModified=$false
    }
}

function Assert-R30CommitReceipt {
    param([string] $Path, [string] $ExpectedSha256)
    if ([string]::IsNullOrWhiteSpace($Path) -or
        [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        throw 'Live capture requires a path and explicit SHA-256 for the committed R30 receipt.'
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $receiptDirectory = [IO.Path]::GetDirectoryName($fullPath)
    $receiptParent = [IO.Path]::GetDirectoryName($receiptDirectory)
    $receiptToken = [IO.Path]::GetFileName($receiptDirectory)
    if (-not [IO.Path]::GetFileName($fullPath).Equals(
            'commit.json', [StringComparison]::Ordinal) -or
        -not $receiptParent.Equals(
            $r30TransactionBase,
            [StringComparison]::OrdinalIgnoreCase) -or
        $receiptToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "R30 receipt must be the direct safe-token child commit.json below the R30 transaction base: $fullPath"
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or
        $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "R30 commit receipt hash mismatch: expected=$($ExpectedSha256.ToUpperInvariant()) actual=$($state.Sha256)"
    }
    $receipt = Get-Content -LiteralPath $fullPath -Raw |
        ConvertFrom-Json -Depth 32
    if ((Get-RequiredPropertyValue $receipt 'Schema' 'R30 commit receipt') -cne
            $r30TransactionSchema -or
        (Get-RequiredPropertyValue $receipt 'Status' 'R30 commit receipt') -cne
            'COMMITTED' -or
        (Get-RequiredPropertyValue $receipt 'RunToken' 'R30 commit receipt') -cne
            $receiptToken -or
        [int] (Get-RequiredPropertyValue $receipt 'R30ContentPackageCount' 'R30 commit receipt') -ne 12 -or
        [string] (Get-RequiredPropertyValue $receipt 'PredecessorVegetationOwner' 'R30 commit receipt') -cne 'R29' -or
        [bool] (Get-RequiredPropertyValue $receipt 'VegetationMutationAllowed' 'R30 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'R29FacadeMeshRetained' 'R30 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'R28PublicRealmRetained' 'R30 commit receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $receipt 'R29EnvironmentActiveRenderers' 'R30 commit receipt') -ne 0 -or
        [bool] (Get-RequiredPropertyValue $receipt 'R29EnvironmentConcurrentRenderingAllowed' 'R30 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeRealismValidated' 'R30 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Promoted' 'R30 commit receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeResponseMaterialPackageCount' 'R30 commit receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeDerivativeMeshPackageCount' 'R30 commit receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeRuntimeResponseMidCount' 'R30 commit receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreePlacementGeometryOpacityWindAuthorityModified' 'R30 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TerrainR29Validated' 'R30 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ContextPolicyShellValidatedBeforeAndAfter' 'R30 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'SimulationCollisionNavigationSensorRfAuthority' 'R30 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualCaptureAccepted' 'R30 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CaptureRevalidationRequired' 'R30 commit receipt') -ne $true) {
        throw 'R30 commit receipt failed its exact R30/R25-shell/R29-owner admission contract.'
    }
    $treeMaterialResponseV3 = Assert-TreeMaterialResponseV3Receipt `
        (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3' 'R30 commit receipt') `
        'R30 commit TreeRealism v3 closure' -ValidateNative
    $closureState = Assert-State `
        $expectedTreeMaterialResponseClosureContract `
        $treeMaterialResponseClosureContract `
        'repository R30 TreeRealism v3 source-closure contract'
    $closure = Get-Content -LiteralPath $treeMaterialResponseClosureContract -Raw |
        ConvertFrom-Json -Depth 32
    $expectedNativePins = @($closure.nativeSourcePromotion)
    $actualNativePins = @($treeMaterialResponseV3.NativeSourcePins)
    for ($index = 0; $index -lt $expectedNativePins.Count; ++$index) {
        $expectedPin = $expectedNativePins[$index]
        $actualPin = $actualNativePins[$index]
        if ([string] $actualPin.RelativePath -cne ([string] $expectedPin.relativePath).Replace('/', '\') -or
            [int64] $actualPin.Bytes -ne [int64] $expectedPin.bytes -or
            [string] $actualPin.Sha256 -cne [string] $expectedPin.sha256 -or
            [bool] $actualPin.NativeBeforePresent -ne $true -or
            [int64] $actualPin.NativeBeforeBytes -ne [int64] $expectedPin.nativePreimageBytes -or
            [string] $actualPin.NativeBeforeSha256 -cne [string] $expectedPin.nativePreimageSha256) {
            throw "R30 commit TreeRealism v3 native-source pin $index drifted from its source-closure contract."
        }
    }
    $actualClosurePins = @($treeMaterialResponseV3.SourceClosurePins)
    if ([string] $actualClosurePins[0].RelativePath -cne
            'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_tree_material_response_v3.source_closure.json' -or
        [int64] $actualClosurePins[0].Bytes -ne [int64] $closureState.Bytes -or
        [string] $actualClosurePins[0].Sha256 -cne [string] $closureState.Sha256) {
        throw 'R30 commit does not bind the exact TreeRealism v3 source-closure contract.'
    }
    for ($index = 0; $index -lt @($closure.sourceEvidence).Count; ++$index) {
        $expectedPin = @($closure.sourceEvidence)[$index]
        $actualPin = $actualClosurePins[$index + 1]
        if ([string] $actualPin.RelativePath -cne ([string] $expectedPin.relativePath).Replace('/', '\') -or
            [int64] $actualPin.Bytes -ne [int64] $expectedPin.bytes -or
            [string] $actualPin.Sha256 -cne [string] $expectedPin.sha256) {
            throw "R30 commit TreeRealism v3 evidence pin $index drifted from its source-closure contract."
        }
    }
    $successorMap = Get-RequiredPropertyValue `
        $receipt 'SuccessorMap' 'R30 commit receipt'
    $runtimeAfter = Get-RequiredPropertyValue `
        $receipt 'RuntimeDllAfter' 'R30 commit receipt'
    $editorAfter = Get-RequiredPropertyValue `
        $receipt 'EditorDllAfter' 'R30 commit receipt'
    [void] (Assert-State $successorMap $mapFile 'current R30 successor map')
    [void] (Assert-State $editorAfter $editorDll 'current R30 editor DLL')
    $baseAdmission = [pscustomobject] [ordered] @{
        Path=$fullPath
        File=$state
        RunToken=$receiptToken
        SuccessorMap=$successorMap
        RuntimeDllAfter=$runtimeAfter
        EditorDllAfter=$editorAfter
        TreeMaterialResponseV3=$treeMaterialResponseV3
        R30ContentPackageCount=12
        PredecessorVegetationOwner='R29'
        ContextPolicyShell='R25_INHERITED'
        VisualCaptureAccepted=$false
        CaptureRevalidationRequired=$true
    }
    $hotfixAdmission = Assert-CookedRuntimeHotfixCommitReceipt $baseAdmission
    [pscustomobject] [ordered] @{
        Path=$baseAdmission.Path
        File=$baseAdmission.File
        RunToken=$baseAdmission.RunToken
        SuccessorMap=$baseAdmission.SuccessorMap
        RuntimeDllAfter=$hotfixAdmission.RuntimeDllAfter
        EditorDllAfter=$hotfixAdmission.EditorDllAfter
        OriginalR30RuntimeDllAfter=$baseAdmission.RuntimeDllAfter
        OriginalR30EditorDllAfter=$baseAdmission.EditorDllAfter
        TreeMaterialResponseV3=$baseAdmission.TreeMaterialResponseV3
        R30ContentPackageCount=12
        PredecessorVegetationOwner='R29'
        ContextPolicyShell='R25_INHERITED'
        VisualCaptureAccepted=$false
        CaptureRevalidationRequired=$true
        BaseR30Admission=$baseAdmission
        CookedRuntimeHotfixAdmission=$hotfixAdmission
    }
}

function Assert-AirSimProjectConfiguration {
    if (-not [IO.File]::Exists($nativeProjectFile)) {
        throw "Native project descriptor is absent: $nativeProjectFile"
    }
    $project = Get-Content -LiteralPath $nativeProjectFile -Raw |
        ConvertFrom-Json -Depth 32
    $plugins = @(Get-RequiredPropertyValue `
        $project 'Plugins' 'native project descriptor')
    $expected = [ordered] @{
        RemoteControl=$true
        AirSim=$false
        AirSimTriadRuntime=$true
        TRIADSensorFusion=$true
        CesiumForUnreal=$true
    }
    $states = [Collections.Generic.List[object]]::new()
    foreach ($entry in $expected.GetEnumerator()) {
        $matches = @($plugins | Where-Object {
            [string] $_.Name -ceq [string] $entry.Key
        })
        if ($matches.Count -ne 1 -or
            [bool] $matches[0].Enabled -ne [bool] $entry.Value) {
            throw "Native project plugin state mismatch: plugin=$($entry.Key) expectedEnabled=$($entry.Value) matches=$($matches.Count)"
        }
        $states.Add([pscustomobject] [ordered] @{
            Name=[string] $entry.Key
            Enabled=[bool] $entry.Value
        })
    }
    $descriptor = Get-FileState $airSimRuntimeDescriptor
    $weatherAsset = Get-FileState (
        Join-Path $airSimRuntimeContentRoot `
            'Weather\WeatherFX\WeatherActor.uasset')
    if (-not $descriptor.Present -or $descriptor.Bytes -le 0 -or
        -not $weatherAsset.Present -or $weatherAsset.Bytes -le 0) {
        throw 'AirSimTriadRuntime descriptor or WeatherActor content is absent.'
    }
    [pscustomobject] [ordered] @{
        ProjectFile=Get-FileState $nativeProjectFile
        PluginStates=@($states)
        LegacyAirSimConfiguredEnabled=$false
        AirSimTriadRuntimeConfiguredEnabled=$true
        AirSimTriadRuntimeDescriptor=$descriptor
        WeatherActorSourcePackage=$weatherAsset
    }
}

function Assert-R30NativeContent {
    if (-not [IO.Directory]::Exists($r30ContentRoot)) {
        throw "R30 native content root is absent: $r30ContentRoot"
    }
    $actual = @(
        Get-ChildItem -LiteralPath $r30ContentRoot -File -Recurse |
            Sort-Object FullName)
    $relative = @($actual | ForEach-Object {
        [IO.Path]::GetRelativePath($r30ContentRoot, $_.FullName)
    })
    if ($actual.Count -ne 12 -or
        (ConvertTo-Json @($relative) -Compress) -cne
        (ConvertTo-Json @($r30ContentRelativePaths | Sort-Object) -Compress)) {
        throw 'Native R30 namespace is not the exact committed twelve-package roster.'
    }
    foreach ($file in $actual) {
        $state = Get-FileState $file.FullName
        if (-not $state.Present -or $state.Bytes -le 0 -or
            $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "Native R30 package lacks a non-empty SHA-256 identity: $($file.FullName)"
        }
    }
    @(Get-TreeReceipt $r30ContentRoot)
}

function Assert-FreshFileState {
    param([string] $Path, [DateTime] $NotBeforeUtc, [string] $Label)
    $state = Get-FileState $Path
    if (-not $state.Present -or $state.Bytes -le 0 -or
        $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
        throw "$Label is absent, empty, or unhashed: $Path"
    }
    $written = [DateTime]::Parse(
        $state.LastWriteUtc,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::RoundtripKind)
    if ($written -lt $NotBeforeUtc.AddSeconds(-2)) {
        throw "$Label predates this isolated operation: path=$Path written=$written notBefore=$NotBeforeUtc"
    }
    [pscustomobject] [ordered] @{
        Path=[IO.Path]::GetFullPath($Path)
        Present=$state.Present
        Bytes=$state.Bytes
        Sha256=$state.Sha256
        LastWriteUtc=$state.LastWriteUtc
    }
}

function Assert-CookedClosure {
    param(
        [string] $EvidenceRoot,
        [string] $CookedPlatformRoot,
        [DateTime] $CookStartedUtc
    )
    $fullPlatformRoot = [IO.Path]::GetFullPath($CookedPlatformRoot)
    if (-not (Test-ContainedPath $fullPlatformRoot $EvidenceRoot) -or
        -not [IO.Directory]::Exists($fullPlatformRoot)) {
        throw "Isolated cooked platform root is absent or escaped evidence: $fullPlatformRoot"
    }
    $cookedGameRoot = [IO.Path]::GetFullPath(
        (Join-Path $fullPlatformRoot 'TRIAD'))
    $cookedContentRoot = Join-Path $cookedGameRoot 'Content'
    $cookedMapBase = Join-Path $cookedContentRoot `
        'Maps\Istana_PublicView_Explore_v5d_hybrid'
    $mapReceipt = @(
        Assert-FreshFileState "$cookedMapBase.umap" $CookStartedUtc 'cooked R30 successor map package'
        Assert-FreshFileState "$cookedMapBase.uexp" $CookStartedUtc 'cooked R30 successor map export'
    )

    $cookedR30Root = Join-Path $cookedContentRoot `
        'TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'
    $cookedR30Uassets = @(
        Get-ChildItem -LiteralPath $cookedR30Root -File -Recurse `
            -Filter '*.uasset' -ErrorAction Stop |
            Sort-Object FullName)
    $cookedR30Relative = @($cookedR30Uassets | ForEach-Object {
        [IO.Path]::GetRelativePath($cookedR30Root, $_.FullName)
    })
    if ($cookedR30Uassets.Count -ne 12 -or
        (ConvertTo-Json @($cookedR30Relative) -Compress) -cne
        (ConvertTo-Json @($r30ContentRelativePaths | Sort-Object) -Compress)) {
        throw 'Cooked R30 namespace is not the exact twelve-package roster.'
    }
    $r30Receipt = [Collections.Generic.List[object]]::new()
    foreach ($asset in $cookedR30Uassets) {
        $r30Receipt.Add((Assert-FreshFileState `
            $asset.FullName $CookStartedUtc 'cooked R30 package'))
        $exportPath = [IO.Path]::ChangeExtension($asset.FullName, '.uexp')
        $r30Receipt.Add((Assert-FreshFileState `
            $exportPath $CookStartedUtc 'cooked R30 package export'))
    }

    $dependencyReceipts = [Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $requiredCookedDependencyRoots) {
        $root = [IO.Path]::GetFullPath(
            (Join-Path $cookedGameRoot $relativeRoot))
        $files = @(
            Get-ChildItem -LiteralPath $root -File -Recurse `
                -ErrorAction Stop | Sort-Object FullName)
        if ($files.Count -eq 0) {
            throw "Required cooked R30/R29/R25 dependency root is empty: $root"
        }
        $states = [Collections.Generic.List[object]]::new()
        foreach ($file in $files) {
            $states.Add((Assert-FreshFileState `
                $file.FullName $CookStartedUtc `
                "cooked dependency $relativeRoot"))
        }
        $dependencyReceipts.Add([pscustomobject] [ordered] @{
            RelativeRoot=$relativeRoot
            FileCount=$states.Count
            Files=@($states)
        })
    }

    $cookedWeatherRoot = Join-Path $cookedGameRoot `
        'Plugins\AirSimTriadRuntime\Content\Weather\WeatherFX'
    $weatherReceipt = @(
        Assert-FreshFileState `
            (Join-Path $cookedWeatherRoot 'WeatherActor.uasset') `
            $CookStartedUtc 'cooked AirSimTriadRuntime WeatherActor package'
        Assert-FreshFileState `
            (Join-Path $cookedWeatherRoot 'WeatherActor.uexp') `
            $CookStartedUtc 'cooked AirSimTriadRuntime WeatherActor export'
    )
    $assetRegistries = @(
        Get-ChildItem -LiteralPath $fullPlatformRoot -File -Recurse `
            -Filter 'AssetRegistry.bin' -ErrorAction Stop |
            Sort-Object FullName)
    if ($assetRegistries.Count -lt 1) {
        throw 'Fresh isolated cook produced no AssetRegistry.bin.'
    }
    $assetRegistryReceipt = @($assetRegistries | ForEach-Object {
        Assert-FreshFileState $_.FullName $CookStartedUtc 'cooked asset registry'
    })
    $shaderLibraries = @(
        Get-ChildItem -LiteralPath $fullPlatformRoot -File -Recurse `
            -Filter '*.ushaderbytecode' -ErrorAction Stop |
            Sort-Object FullName)
    if ($shaderLibraries.Count -lt 1) {
        throw 'Fresh isolated cook produced no shader bytecode library.'
    }
    $shaderReceipt = @($shaderLibraries | ForEach-Object {
        Assert-FreshFileState $_.FullName $CookStartedUtc 'cooked shader bytecode'
    })
    [pscustomobject] [ordered] @{
        PlatformRoot=$fullPlatformRoot
        GameRoot=$cookedGameRoot
        MapFiles=$mapReceipt
        R30Files=@($r30Receipt)
        R30PackageCount=12
        DependencyRoots=@($dependencyReceipts)
        AirSimWeatherActorFiles=$weatherReceipt
        AssetRegistries=$assetRegistryReceipt
        ShaderLibraries=$shaderReceipt
        FreshCookValidated=$true
    }
}

function Assert-CookedClosureUnchanged {
    param($Closure)
    $states = [Collections.Generic.List[object]]::new()
    foreach ($state in @($Closure.MapFiles)) { $states.Add($state) }
    foreach ($state in @($Closure.R30Files)) { $states.Add($state) }
    foreach ($root in @($Closure.DependencyRoots)) {
        foreach ($state in @($root.Files)) { $states.Add($state) }
    }
    foreach ($state in @($Closure.AirSimWeatherActorFiles)) {
        $states.Add($state)
    }
    foreach ($state in @($Closure.AssetRegistries)) { $states.Add($state) }
    foreach ($state in @($Closure.ShaderLibraries)) { $states.Add($state) }
    if ($states.Count -eq 0) {
        throw 'Cooked closure has no hash-bound files to revalidate.'
    }
    foreach ($state in $states) {
        [void] (Assert-State $state $state.Path 'post-Game cooked file')
    }
    [pscustomobject] [ordered] @{
        Status='UNCHANGED'
        RevalidatedFileCount=$states.Count
    }
}

function Assert-StaticContract {
    Assert-CaptureSourcePins $repositoryUnrealRoot
    $visualReviewContractState = Assert-State `
        $expectedVisualReviewContract $visualReviewContract `
        'R30 Player0 two-phase visual-review contract'
    $treeClosureContractState = Assert-State `
        $expectedTreeMaterialResponseClosureContract `
        $treeMaterialResponseClosureContract `
        'R30 TreeRealism v3 source-closure contract'
    $visualReviewContractJson = Get-Content -LiteralPath $visualReviewContract -Raw |
        ConvertFrom-Json -Depth 16
    $treeClosureContractJson = Get-Content -LiteralPath $treeMaterialResponseClosureContract -Raw |
        ConvertFrom-Json -Depth 32
    if ([string] $visualReviewContractJson.schema -cne
            'triad.istana_explore_v5d.r30_player0_visual_review.v1' -or
        [string] $visualReviewContractJson.strictNativeOrder -cne
            'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' -or
        [string] $visualReviewContractJson.executeStatus -cne
            'PENDING_VISUAL_REVIEW' -or
        [string] $visualReviewContractJson.acceptedStatus -cne 'COMMITTED' -or
        [bool] $visualReviewContractJson.automaticVisualAcceptanceAllowed -ne $false -or
        [bool] $visualReviewContractJson.pendingTruth.treeMaterialResponseV3Reviewed -ne $false -or
        [bool] $visualReviewContractJson.acceptedTruth.treeMaterialResponseV3Reviewed -ne $true -or
        [bool] $visualReviewContractJson.acceptanceRevalidation.treeMaterialResponseV3SourceAndContentClosure -ne $true -or
        [bool] $visualReviewContractJson.acceptanceRevalidation.treeResponseMaterials13AndRuntimeMids26 -ne $true -or
        [bool] $visualReviewContractJson.treeVisualReview.required -ne $true -or
        [bool] $visualReviewContractJson.treeVisualReview.sourceOrCpuAuditMayAuthorizeAcceptance -ne $false -or
        [string]::Join('|', @($visualReviewContractJson.exactOrderedPoseIds)) -cne
            '075m|020m|008m|002m|surroundings_oblique_macdonald') {
        throw 'R30 Player0 two-phase visual-review contract truth drifted.'
    }
    if ([string] $treeClosureContractJson.schema -cne
            'triad.istana_explore_v5d.r30_tree_material_response_v3.source_closure.v1' -or
        [string] $treeClosureContractJson.nativeOrder -cne
            'R30_COMMIT_AND_CAPTURE_THEN_R31_COMMIT_AND_CAPTURE_THEN_R32_COMMIT_AND_CAPTURE_THEN_R33_COMMIT_AND_CAPTURE' -or
        [bool] $treeClosureContractJson.newStageCreated -ne $false -or
        [bool] $treeClosureContractJson.r34Created -ne $false -or
        @($treeClosureContractJson.nativeSourcePromotion).Count -ne 4 -or
        @($treeClosureContractJson.sourceEvidence).Count -ne 5 -or
        [int] $treeClosureContractJson.nativeContentMutation.responseMaterialPackagesCreated -ne 13 -or
        [int] $treeClosureContractJson.nativeContentMutation.managedMeshPackagesRebound -ne 5 -or
        [int] $treeClosureContractJson.runtimeResponse.runtimeResponseMids -ne 26 -or
        [bool] $treeClosureContractJson.captureGate.humanTreeReviewRequired -ne $true -or
        [bool] $treeClosureContractJson.captureGate.automaticVisualAcceptanceAllowed -ne $false) {
        throw 'R30 TreeRealism v3 source-closure contract truth drifted.'
    }
    Initialize-MemoryWatchdogType
    Initialize-BinaryMarkerScannerType
    $runtimeLogPolicy = Assert-RuntimeLogPolicyContract
    $runtimeIsolationLogPolicy = Assert-RuntimeIsolationLogPolicyContract
    $dedicatedShaderWorkerLogPolicy =
        Assert-DedicatedShaderWorkerLogPolicyContract
    $treeDerivativeMeshDdcPrewarmPolicy =
        Assert-TreeDerivativeMeshDdcPrewarmPolicyContract
    $runtimeEvidenceAcceptancePolicy =
        Assert-RuntimeEvidenceAcceptancePolicyContract
    if ($poses.Count -ne 5 -or
        ([string]::Join('|', @($poses.Id))) -cne
            '075m|020m|008m|002m|surroundings_oblique_macdonald' -or
        @($poses.Id | Sort-Object -Unique).Count -ne 5 -or
        $r30ContentRelativePaths.Count -ne 12 -or
        $treeResponseMaterialRelativePaths.Count -ne 13 -or
        $treeDerivativeMeshRelativePaths.Count -ne 5 -or
        $treeDerivativeMeshPrewarmPackages.Count -ne 5 -or
        @($treeDerivativeMeshPrewarmPackages | Sort-Object -Unique).Count -ne 5 -or
        $columnarTreeDdcSeedPins.Count -ne 2 -or
        $requiredCookedDependencyRoots.Count -ne 6 -or
        $minimumSystemFreeVirtualAtLaunchBytes -ne 2147483648L -or
        $minimumSystemFreeVirtualBytes -ne 2147483648L -or
        $privateMemoryCeilingBytes -ne 9223372036854775807L -or
        $memoryWatchdogPollMilliseconds -ne 500 -or
        $memoryWatchdogPersistentBreachMilliseconds -ne 2000 -or
        $rcPropertyUri -cne
            'http://127.0.0.1:30010/remote/object/property' -or
        $macDonaldRuntimeActorPath -cne
            '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid:PersistentLevel.TRIADIstanaExploreV5DR24MacDonaldHouse' -or
        $temasekRuntimeActorPath -cne
            '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid:PersistentLevel.TRIADIstanaExploreV5DR24TemasekShophouse' -or
        $restoreRetryCount -ne 8 -or
        $restoreRetryDelayMilliseconds -ne 250 -or
        $redirectedLogReleaseRetryCount -ne 40 -or
        $redirectedLogReleaseRetryDelayMilliseconds -ne 100) {
        throw 'R30 capture static roster or fixed memory guard drifted.'
    }
    $header = [IO.File]::ReadAllText(
        (Join-Path $repositoryUnrealRoot $captureSourcePins[0].RelativePath))
    $implementation = [IO.File]::ReadAllText(
        (Join-Path $repositoryUnrealRoot $captureSourcePins[1].RelativePath))
    foreach ($marker in @(
        'UBlueprintFunctionLibrary',
        'GetIstanaExploreV5DR30Player0CaptureState',
        'SetIstanaExploreV5DR30Player0CapturePose',
        'CaptureIstanaExploreV5DR30Player0FallbackView',
        'FinishIstanaExploreV5DR30Player0CaptureRun',
        'treeMaterialResponseV3=true',
        'treeResponseMaterials=13',
        'treeRuntimeResponseMids=26',
        'treePlacementGeometryOpacityWindAuthorityModified=false')) {
        if (-not ($header.Contains($marker, [StringComparison]::Ordinal) -or
                  $implementation.Contains($marker, [StringComparison]::Ordinal))) {
            throw "R30 runtime capture source lacks marker: $marker"
        }
    }
    foreach ($forbidden in @('GEditor', 'UnrealEd', 'R31')) {
        if ($header.Contains($forbidden, [StringComparison]::Ordinal) -or
            $implementation.Contains($forbidden, [StringComparison]::Ordinal)) {
            throw "R30 runtime capture source contains forbidden dependency: $forbidden"
        }
    }
    [pscustomobject] [ordered] @{
        Schema=$schema
        Status='STATIC_SELF_CHECK_PASS'
        CaptureSourceCount=2
        ExactPoseCount=5
        R30ContentPackageCount=12
        ContextPolicyShell='R25_INHERITED'
        PredecessorVegetationOwner='R29'
        VisualReviewContract=$visualReviewContractState
        TreeMaterialResponseV3SourceClosure=$treeClosureContractState
        TreeResponseMaterialPackageCount=13
        TreeDerivativeMeshPackageCount=5
        TreeRuntimeResponseMidCount=26
        TreeHumanVisualReviewRequired=$true
        NativePluginSourceTreeRequired=$true
        GroundHeaderAndSourceRequired=$true
        TwoPhaseVisualReviewRequired=$true
        PendingCaptureStatus='PENDING_VISUAL_REVIEW'
        AcceptedCaptureStatus='COMMITTED'
        AutomaticVisualAcceptanceAllowed=$false
        ExplicitFiveImageReviewConfirmationRequired=$true
        ProviderFallbackOnly=$true
        ProviderReadyProofClaimed=$false
        AirSimTriadRuntimeRetained=$true
        EmergencyLaunchFreeVirtualReserveBytes=2147483648L
        EmergencyContinuousFreeVirtualReserveBytes=2147483648L
        FixedPrivateMemoryCeilingDisabled=$true
        MemoryWatchdogTypeCompiled=$true
        BinaryMarkerScannerTypeCompiled=$true
        RuntimeLogPolicy=$runtimeLogPolicy
        HybridRuntimeProof='SUCCESSFUL_RC_CAPTURE_STATE_TRANSITIVE_VALIDATE_HYBRID'
        RuntimeEvidenceAcceptancePolicy=$runtimeEvidenceAcceptancePolicy
        LandmarkRuntimePropertyAudit=[pscustomobject] [ordered] @{
            Endpoint=$rcPropertyUri
            ExactActorCount=2
            ReadOnly=$true
            RequiredProperties=@(
                'bRuntimeProviderTelemetryBindingValid',
                'bRuntimeAssetContractFailClosed',
                'bDedicatedOverlayVisible')
            PreCaptureRequired=$true
            PostFifthCaptureRequired=$true
            PendingReceiptRevalidationRequired=$true
            CommittedReceiptCarryForwardRequired=$true
        }
        RuntimeIsolationLogPolicy=$runtimeIsolationLogPolicy
        DedicatedShaderWorkerLogPolicy=$dedicatedShaderWorkerLogPolicy
        TreeDerivativeMeshDdcPrewarmPolicy=$treeDerivativeMeshDdcPrewarmPolicy
        ProcessHandlesDisposedBeforeLogInspection=$true
        IdempotentBoundedJournalRestore=$true
        NativeProjectAccessed=$false
        NativeTreeWritten=$false
        UnrealLaunched=$false
        R31DependencyAllowed=$false
    }
}

function Assert-ExactFallbackStateResponse {
    param($Response, $Pose, [string] $Label)
    $report = [string] (Get-RequiredPropertyValue `
        $Response 'OutReport' $Label)
    if ((Get-RequiredPropertyValue $Response 'ReturnValue' $Label) -ne $true) {
        throw "$Label returned false: $report"
    }
    foreach ($marker in @(
        'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID',
        "poseId=$($Pose.Id)",
        'exactQaViewPose=true',
        'contextPolicyShell=R25Inherited',
        'r29VegetationOwner=1',
        'r29TerrainOwner=1',
        'treeRealismOwner=1',
        'treeMaterialResponseV3=true',
        'treeResponseMaterials=13',
        'treeRuntimeResponseMids=26',
        'treePlacementGeometryOpacityWindAuthorityModified=false',
        'providerReadyForProof=false',
        'localFallbackHidden=false',
        'r30Visible=true',
        'providerFallbackVisualQa=true',
        'providerReadyProofClaimed=false',
        'airSimTriadRuntimeLoaded=true',
        'weatherActorResolved=true',
        'visualCaptureAccepted=false',
        'captureRevalidationRequired=true')) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Label lacks exact ProviderFallback marker '$marker': $report"
        }
    }
    $report
}

function Assert-CaptureImmutableState {
    param($Before, $R30Admission, $R30ReceiptFileBefore)
    $currentProject = Assert-AirSimProjectConfiguration
    [void] (Assert-State `
        $Before.ProjectFile $nativeProjectFile 'native project descriptor')
    [void] (Assert-State `
        $Before.AirSimDescriptor $airSimRuntimeDescriptor `
        'AirSimTriadRuntime descriptor')
    [void] (Assert-State `
        $R30Admission.SuccessorMap $mapFile 'R30 successor map')
    [void] (Assert-State `
        $R30Admission.RuntimeDllAfter $runtimeEditorDll `
        'R30 runtime editor DLL')
    [void] (Assert-State `
        $R30Admission.EditorDllAfter $editorDll 'R30 editor DLL')
    [void] (Assert-State `
        $R30ReceiptFileBefore $R30Admission.Path 'R30 source commit receipt')
    [void] (Assert-State `
        $Before.CookedRuntimeHotfixCommit `
        $cookedRuntimeHotfixCommitReceipt `
        'UE 5.5 cooked-runtime hotfix commit receipt')
    [void] (Assert-TreeReceipt `
        $r30ContentRoot @($Before.R30) 'R30 content')
    [void] (Assert-TreeReceipt `
        $r29FacadeContentRoot @($Before.R29Facade) `
        'R29 facade predecessor content')
    [void] (Assert-TreeReceipt `
        $r29VegetationContentRoot @($Before.R29Vegetation) `
        'R29 vegetation content')
    [void] (Assert-TreeReceipt `
        $r29TerrainContentRoot @($Before.R29Terrain) `
        'R29 terrain content')
    [void] (Assert-TreeReceipt `
        $treeRealismContentRoot @($Before.TreeRealism) `
        'tree realism content')
    Assert-JsonIdentity `
        @($R30Admission.TreeMaterialResponseV3.ContentAfter) `
        @(Get-TreeIdentityReceipt $treeRealismContentRoot) `
        'R30 transaction/capture TreeRealism v3 content binding' 8
    [void] (Assert-TreeReceipt `
        $contextFacadeR25ContentRoot @($Before.ContextFacadeR25) `
        'R25 inherited context shell content')
    [void] (Assert-TreeReceipt `
        $airSimRuntimeContentRoot @($Before.AirSimContent) `
        'AirSimTriadRuntime content')
    Assert-CaptureSourcePins $nativeProjectRoot
    [pscustomobject] [ordered] @{
        Status='UNCHANGED'
        Project=$currentProject
        R30Map=$R30Admission.SuccessorMap
        R30RuntimeEditorDll=$R30Admission.RuntimeDllAfter
        R30EditorDll=$R30Admission.EditorDllAfter
        CookedRuntimeHotfixCommit=$Before.CookedRuntimeHotfixCommit
        R30ContentPackageCount=12
        R29VegetationRetained=$true
        R29TerrainRetained=$true
        TreeRealismRetained=$true
        TreeMaterialResponseV3Retained=$true
        TreeResponseMaterialPackageCount=13
        TreeRuntimeResponseMidCount=26
        R25ContextShellRetained=$true
        AirSimTriadRuntimeRetained=$true
        RuntimeCaptureSourcesHashPinned=$true
    }
}

function Assert-R30PendingCaptureReceipt {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $ExpectedRunToken
    )
    if ([string]::IsNullOrWhiteSpace($Path) -or
        [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        throw 'Visual acceptance requires a pending receipt path and an explicit SHA-256.'
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $evidenceRoot = [IO.Path]::GetDirectoryName($fullPath)
    $evidenceParent = [IO.Path]::GetDirectoryName($evidenceRoot)
    $evidenceToken = [IO.Path]::GetFileName($evidenceRoot)
    if (-not [IO.Path]::GetFileName($fullPath).Equals(
            'pending-visual-review.json', [StringComparison]::Ordinal) -or
        -not $evidenceParent.Equals(
            $nativeEvidenceBase, [StringComparison]::OrdinalIgnoreCase) -or
        $evidenceToken -cne $ExpectedRunToken -or
        $evidenceToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "Pending receipt must be the exact direct run-token child below the R30 evidence base: $fullPath"
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or
        $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "Pending R30 capture receipt hash mismatch: expected=$($ExpectedSha256.ToUpperInvariant()) actual=$($state.Sha256)"
    }
    $receipt = Get-Content -LiteralPath $fullPath -Raw |
        ConvertFrom-Json -Depth 64
    if ((Get-RequiredPropertyValue $receipt 'Schema' 'pending R30 capture receipt') -cne
            $schema -or
        (Get-RequiredPropertyValue $receipt 'Status' 'pending R30 capture receipt') -cne
            'PENDING_VISUAL_REVIEW' -or
        (Get-RequiredPropertyValue $receipt 'RunToken' 'pending R30 capture receipt') -cne
            $ExpectedRunToken -or
        (Get-RequiredPropertyValue $receipt 'NativeOrder' 'pending R30 capture receipt') -cne
            'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' -or
        [bool] (Get-RequiredPropertyValue $receipt 'R31DependencyAllowed' 'pending R30 capture receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactPoseCount' 'pending R30 capture receipt') -ne 5 -or
        [bool] (Get-RequiredPropertyValue $receipt 'MechanicalCaptureValidationPassed' 'pending R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ExplicitHumanReviewAcceptance' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanVisualReviewAttested' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ConfirmedFiveImagesReviewed' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'AutomaticVisualAcceptanceAllowed' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewRequired' 'pending R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewAccepted' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderFallbackVisualQaAccepted' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Reviewed' 'pending R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'R31AdmissionAuthorized' 'pending R30 capture receipt') -ne $false) {
        throw 'Pending R30 capture receipt is not the exact unaccepted mechanical-capture result.'
    }
    foreach ($acceptedOnlyProperty in @(
        'MapModifiedByCapture',
        'SimulationCollisionNavigationSensorRfModified')) {
        if ($null -ne $receipt.PSObject.Properties[$acceptedOnlyProperty]) {
            throw "Pending receipt illegally exposes accepted-only field: $acceptedOnlyProperty"
        }
    }
    foreach ($truthRow in @(
        [pscustomobject] @{ Name='ProviderReadyProofClaimed'; Expected=$false },
        [pscustomobject] @{ Name='ProviderReadyCaptureAccepted'; Expected=$false },
        [pscustomobject] @{ Name='HyperrealismClaimed'; Expected=$false },
        [pscustomobject] @{ Name='CesiumProviderEvidencePresent'; Expected=$true },
        [pscustomobject] @{ Name='CesiumProviderReadyEvidencePresent'; Expected=$false },
        [pscustomobject] @{ Name='AirSimTriadRuntimeRetained'; Expected=$true },
        [pscustomobject] @{ Name='LegacyAirSimProjectStateChanged'; Expected=$false },
        [pscustomobject] @{ Name='AirSimDisableArgumentsUsed'; Expected=$false })) {
        if ([bool] (Get-RequiredPropertyValue `
                $receipt $truthRow.Name 'pending R30 capture receipt') -ne
            [bool] $truthRow.Expected) {
            throw "Pending R30 capture truth field drifted: $($truthRow.Name)"
        }
    }
    $staticEvidence = Get-RequiredPropertyValue `
        $receipt 'StaticReceipt' 'pending R30 capture receipt'
    if ((Get-RequiredPropertyValue `
            $staticEvidence 'Status' 'pending R30 static receipt') -cne
                'STATIC_SELF_CHECK_PASS' -or
        [bool] (Get-RequiredPropertyValue `
            $staticEvidence 'TwoPhaseVisualReviewRequired' `
            'pending R30 static receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $staticEvidence 'AutomaticVisualAcceptanceAllowed' `
            'pending R30 static receipt') -ne $false) {
        throw 'Pending R30 capture does not embed the exact two-phase static receipt.'
    }
    $hybridRuntimeProof = Get-RequiredPropertyValue `
        $receipt 'GameStartupHybridContract' 'pending R30 capture receipt'
    [void] (Assert-HybridRuntimeProofReceipt `
        $hybridRuntimeProof 'pending R30 startup hybrid runtime proof')
    $landmarkRuntimePre = Get-RequiredPropertyValue `
        $receipt 'LandmarkRuntimeContractsPreCapture' `
        'pending R30 capture receipt'
    $landmarkRuntimePost = Get-RequiredPropertyValue `
        $receipt 'LandmarkRuntimeContractsPostCapture' `
        'pending R30 capture receipt'
    [void] (Assert-LandmarkRuntimeContractsReceipt `
        $landmarkRuntimePre 'PRE_CAPTURE' `
        'pending R30 pre-capture landmark runtime proof')
    [void] (Assert-LandmarkRuntimeContractsReceipt `
        $landmarkRuntimePost 'POST_FIFTH_CAPTURE' `
        'pending R30 post-capture landmark runtime proof')
    foreach ($landmarkReceiptName in @(
        'MacDonaldHouse', 'TemasekShophouse')) {
        Assert-JsonIdentity `
            (Get-RequiredPropertyValue `
                $landmarkRuntimePre $landmarkReceiptName `
                'pending R30 pre-capture landmark runtime proof') `
            (Get-RequiredPropertyValue `
                $landmarkRuntimePost $landmarkReceiptName `
                'pending R30 post-capture landmark runtime proof') `
            "pending R30 pre/post $landmarkReceiptName runtime property receipts" `
            10
    }
    $noMutation = Get-RequiredPropertyValue `
        $receipt 'MechanicalNoMutationEvidence' 'pending R30 capture receipt'
    if ([bool] (Get-RequiredPropertyValue `
            $noMutation 'MapUnchanged' 'pending R30 no-mutation evidence') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $noMutation 'SimulationCollisionNavigationSensorRfUnchanged' `
            'pending R30 no-mutation evidence') -ne $true) {
        throw 'Pending R30 capture no-mutation evidence is not exact.'
    }
    $postflight = Get-RequiredPropertyValue `
        $receipt 'Postflight' 'pending R30 capture receipt'
    if ((Get-RequiredPropertyValue $postflight 'Status' 'pending R30 postflight') -cne
            'UNCHANGED' -or
        [bool] (Get-RequiredPropertyValue $postflight 'R29VegetationRetained' 'pending R30 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue $postflight 'R29TerrainRetained' 'pending R30 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue $postflight 'TreeRealismRetained' 'pending R30 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue $postflight 'TreeMaterialResponseV3Retained' 'pending R30 postflight') -ne $true -or
        [int] (Get-RequiredPropertyValue $postflight 'TreeResponseMaterialPackageCount' 'pending R30 postflight') -ne 13 -or
        [int] (Get-RequiredPropertyValue $postflight 'TreeRuntimeResponseMidCount' 'pending R30 postflight') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $postflight 'R25ContextShellRetained' 'pending R30 postflight') -ne $true) {
        throw 'Pending R30 capture postflight does not preserve the exact R30/R29/R25 scene.'
    }
    $cookedPostflight = Get-RequiredPropertyValue `
        $receipt 'CookedClosurePostflight' 'pending R30 capture receipt'
    if ((Get-RequiredPropertyValue `
            $cookedPostflight 'Status' 'pending R30 cooked postflight') -cne
            'UNCHANGED') {
        throw 'Pending R30 cooked closure was not revalidated unchanged.'
    }

    $r30Commit = Get-RequiredPropertyValue `
        $receipt 'R30CommitAdmission' 'pending R30 capture receipt'
    $r30CommitPath = [string] (Get-RequiredPropertyValue `
        $r30Commit 'Path' 'pending R30 commit admission')
    $r30CommitFile = Get-RequiredPropertyValue `
        $r30Commit 'File' 'pending R30 commit admission'
    $currentR30Admission = Assert-R30CommitReceipt `
        $r30CommitPath ([string] (Get-RequiredPropertyValue `
            $r30CommitFile 'Sha256' 'pending R30 commit receipt file'))
    $embeddedHotfixAdmission = Get-RequiredPropertyValue `
        $receipt 'CookedRuntimeHotfixAdmission' 'pending R30 capture receipt'
    Assert-JsonIdentity `
        $currentR30Admission.CookedRuntimeHotfixAdmission `
        $embeddedHotfixAdmission `
        'pending/current cooked-runtime hotfix admission' 20

    $binding = Get-RequiredPropertyValue `
        $receipt 'BindingEvidence' 'pending R30 capture receipt'
    $mapBinding = Get-RequiredPropertyValue `
        $binding 'MapCaptureBinding' 'pending R30 binding evidence'
    $runtimeBinding = Get-RequiredPropertyValue `
        $binding 'RuntimeEditorDllCaptureBinding' 'pending R30 binding evidence'
    $editorBinding = Get-RequiredPropertyValue `
        $binding 'EditorDllCaptureBinding' 'pending R30 binding evidence'
    [void] (Assert-ExactStateBinding $mapBinding $mapFile 'pending/current R30 map binding')
    [void] (Assert-ExactStateBinding $runtimeBinding $runtimeEditorDll 'pending/current R30 runtime DLL binding')
    [void] (Assert-ExactStateBinding $editorBinding $editorDll 'pending/current R30 editor DLL binding')
    Assert-JsonIdentity `
        $currentR30Admission.SuccessorMap `
        (Get-RequiredPropertyValue $mapBinding 'State' 'pending R30 map binding') `
        'R30 transaction/capture map binding'
    Assert-JsonIdentity `
        $currentR30Admission.RuntimeDllAfter `
        (Get-RequiredPropertyValue $runtimeBinding 'State' 'pending R30 runtime binding') `
        'R30 transaction/capture runtime DLL binding'
    Assert-JsonIdentity `
        $currentR30Admission.EditorDllAfter `
        (Get-RequiredPropertyValue $editorBinding 'State' 'pending R30 editor binding') `
        'R30 transaction/capture editor DLL binding'

    foreach ($groundRow in @(
        [pscustomobject] @{ Name='GroundHeaderCaptureBinding'; Path=$groundHeader; Label='Ground header capture binding' },
        [pscustomobject] @{ Name='GroundSourceCaptureBinding'; Path=$groundSource; Label='Ground source capture binding' })) {
        $groundBinding = Get-RequiredPropertyValue `
            $binding $groundRow.Name 'pending R30 binding evidence'
        if ([bool] (Get-RequiredPropertyValue `
                $groundBinding 'Unchanged' $groundRow.Label) -ne $true -or
            [bool] (Get-RequiredPropertyValue `
                $groundBinding 'Present' $groundRow.Label) -ne $true -or
            [int64] (Get-RequiredPropertyValue `
                $groundBinding 'Bytes' $groundRow.Label) -le 0 -or
            [string] (Get-RequiredPropertyValue `
                $groundBinding 'Sha256' $groundRow.Label) -notmatch '^[A-F0-9]{64}$') {
            throw "$($groundRow.Label) is absent, empty, unhashed, or not marked unchanged."
        }
        [void] (Assert-ExactPathBoundReceipt `
            $groundBinding $groundRow.Path $groundRow.Label)
        [void] (Assert-ExactPathBoundReceipt `
            (Get-RequiredPropertyValue $groundBinding 'ImmediatePreCapture' $groundRow.Label) `
            $groundRow.Path "$($groundRow.Label) immediate pre-capture")
        [void] (Assert-ExactPathBoundReceipt `
            (Get-RequiredPropertyValue $groundBinding 'ImmediatePostCapture' $groundRow.Label) `
            $groundRow.Path "$($groundRow.Label) immediate post-capture")
    }

    $sourceTreeBinding = Get-RequiredPropertyValue `
        $binding 'NativePluginSourceTreeCaptureBinding' `
        'pending R30 binding evidence'
    $sourceTreeRoot = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $sourceTreeBinding 'Root' `
            'native plugin source-tree capture binding'))
    if (-not $sourceTreeRoot.Equals(
            $nativePluginSourceRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [bool] (Get-RequiredPropertyValue `
            $sourceTreeBinding 'Unchanged' 'native plugin source-tree capture binding') -ne $true) {
        throw 'Pending native plugin source-tree root or unchanged marker is invalid.'
    }
    $sourceTreeBaseline = @(Get-RequiredPropertyValue `
        $sourceTreeBinding 'BaselineAfterCaptureSourcePromotion' `
        'native plugin source-tree capture binding')
    $sourceTreePre = @(Get-RequiredPropertyValue `
        $sourceTreeBinding 'ImmediatePreCapture' `
        'native plugin source-tree capture binding')
    $sourceTreePost = @(Get-RequiredPropertyValue `
        $sourceTreeBinding 'ImmediatePostCapture' `
        'native plugin source-tree capture binding')
    if ($sourceTreeBaseline.Count -eq 0 -or
        [int] (Get-RequiredPropertyValue `
            $sourceTreeBinding 'FileCount' 'native plugin source-tree capture binding') -ne
            $sourceTreeBaseline.Count) {
        throw 'Pending native plugin source-tree receipt is empty or has the wrong count.'
    }
    Assert-JsonIdentity @($sourceTreeBaseline) @($sourceTreePre) `
        'native plugin source tree baseline/pre-capture' 8
    Assert-JsonIdentity @($sourceTreeBaseline) @($sourceTreePost) `
        'native plugin source tree baseline/post-capture' 8
    [void] (Assert-CaptureSourceTreeSnapshot `
        $sourceTreePost 'visual-acceptance native plugin source tree')
    Assert-CaptureSourcePins $nativeProjectRoot

    $immutable = Get-RequiredPropertyValue `
        $receipt 'ImmutableNativeState' 'pending R30 capture receipt'
    [void] (Assert-CaptureImmutableState `
        $immutable $currentR30Admission $currentR30Admission.File)

    $captures = @(Get-RequiredPropertyValue `
        $receipt 'Captures' 'pending R30 capture receipt')
    if ($captures.Count -ne 5) {
        throw "Pending R30 capture receipt has $($captures.Count) captures instead of five."
    }
    $fileHashes = [Collections.Generic.List[string]]::new()
    $pixelHashes = [Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $poses.Count; ++$index) {
        $expectedPose = $poses[$index]
        $capture = $captures[$index]
        $pose = Get-RequiredPropertyValue $capture 'Pose' "pending capture row $index"
        if ([string] (Get-RequiredPropertyValue `
                $pose 'Id' "pending capture pose $index") -cne $expectedPose.Id) {
            throw "Pending capture pose order mismatch at index $index."
        }
        foreach ($coordinate in @('X', 'Y', 'Z', 'Pitch', 'Yaw', 'Roll')) {
            if ([double] (Get-RequiredPropertyValue `
                    $pose $coordinate "pending capture pose $index") -ne
                [double] $expectedPose.$coordinate) {
                throw "Pending capture pose $index coordinate $coordinate drifted."
            }
        }
        if ([bool] (Get-RequiredPropertyValue `
                $capture 'ProviderFallbackVisualQa' "pending capture row $index") -ne $true -or
            [bool] (Get-RequiredPropertyValue `
                $capture 'ProviderReadyProofClaimed' "pending capture row $index") -ne $false) {
            throw "Pending capture row $index does not preserve ProviderFallback truth."
        }
        $imageReceipt = Get-RequiredPropertyValue `
            $capture 'Image' "pending capture row $index"
        $expectedImagePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            "explore_v5d_r30_player0_$($expectedPose.Id)_$ExpectedRunToken.png"))
        $imagePath = [IO.Path]::GetFullPath([string] (
            Get-RequiredPropertyValue $imageReceipt 'Path' `
                "pending capture image $index"))
        if (-not $imagePath.Equals(
                $expectedImagePath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Pending capture image $index has a noncanonical path: $imagePath"
        }
        [void] (Assert-State `
            $imageReceipt $expectedImagePath "pending capture PNG $index")
        $decoded = Get-PngReceipt $expectedImagePath
        foreach ($identityProperty in @('Sha256', 'DecodedBgraSha256')) {
            if ([string] (Get-RequiredPropertyValue `
                    $imageReceipt $identityProperty "pending capture image $index") -cne
                [string] $decoded.$identityProperty) {
                throw "Pending capture image $index $identityProperty changed."
            }
        }
        if ([int64] (Get-RequiredPropertyValue `
                $imageReceipt 'Bytes' "pending capture image $index") -ne
                [int64] $decoded.Bytes -or
            [int] (Get-RequiredPropertyValue `
                $imageReceipt 'WidthPixels' "pending capture image $index") -ne 2560 -or
            [int] (Get-RequiredPropertyValue `
                $imageReceipt 'HeightPixels' "pending capture image $index") -ne 1440) {
            throw "Pending capture image $index decode binding changed."
        }
        $fileHashes.Add([string] $decoded.Sha256)
        $pixelHashes.Add([string] $decoded.DecodedBgraSha256)
    }
    if (@($fileHashes | Sort-Object -Unique).Count -ne 5 -or
        @($pixelHashes | Sort-Object -Unique).Count -ne 5) {
        throw 'Pending R30 capture PNG file/pixel identities are not five-way distinct.'
    }

    [pscustomobject] [ordered] @{
        Path=$fullPath
        File=$state
        EvidenceRoot=$evidenceRoot
        Receipt=$receipt
        BindingEvidence=$binding
        ImmutableNativeState=$immutable
        CurrentR30Admission=$currentR30Admission
        GameStartupHybridContract=$hybridRuntimeProof
        LandmarkRuntimeContractsPreCapture=$landmarkRuntimePre
        LandmarkRuntimeContractsPostCapture=$landmarkRuntimePost
        CaptureCount=5
        MechanicalValidationReplayed=$true
        HybridRuntimeProofRevalidated=$true
        LandmarkRuntimePropertyReceiptsRevalidated=$true
        VisualReviewAccepted=$false
    }
}

function Get-R30AcceptanceNativeBoundary {
    param($PendingAdmission)
    $binding = $PendingAdmission.BindingEvidence
    $mapBinding = Get-RequiredPropertyValue `
        $binding 'MapCaptureBinding' 'acceptance binding evidence'
    $runtimeBinding = Get-RequiredPropertyValue `
        $binding 'RuntimeEditorDllCaptureBinding' 'acceptance binding evidence'
    $editorBinding = Get-RequiredPropertyValue `
        $binding 'EditorDllCaptureBinding' 'acceptance binding evidence'
    $groundHeaderBinding = Get-RequiredPropertyValue `
        $binding 'GroundHeaderCaptureBinding' 'acceptance binding evidence'
    $groundSourceBinding = Get-RequiredPropertyValue `
        $binding 'GroundSourceCaptureBinding' 'acceptance binding evidence'
    $sourceTreeBinding = Get-RequiredPropertyValue `
        $binding 'NativePluginSourceTreeCaptureBinding' 'acceptance binding evidence'
    [void] (Assert-ExactStateBinding $mapBinding $mapFile 'acceptance R30 map')
    [void] (Assert-ExactStateBinding $runtimeBinding $runtimeEditorDll 'acceptance R30 runtime DLL')
    [void] (Assert-ExactStateBinding $editorBinding $editorDll 'acceptance R30 editor DLL')
    [void] (Assert-ExactPathBoundReceipt $groundHeaderBinding $groundHeader 'acceptance Ground header')
    [void] (Assert-ExactPathBoundReceipt $groundSourceBinding $groundSource 'acceptance Ground source')
    $expectedTree = @(Get-RequiredPropertyValue `
        $sourceTreeBinding 'ImmediatePostCapture' 'acceptance native plugin source tree')
    $currentTree = @(Assert-CaptureSourceTreeSnapshot `
        $expectedTree 'acceptance native plugin source tree')
    $preservedScene = Assert-CaptureImmutableState `
        $PendingAdmission.ImmutableNativeState `
        $PendingAdmission.CurrentR30Admission `
        $PendingAdmission.CurrentR30Admission.File
    [pscustomobject] [ordered] @{
        Map=Get-FileState $mapFile
        RuntimeEditorDll=Get-FileState $runtimeEditorDll
        EditorDll=Get-FileState $editorDll
        GroundHeader=Get-PathBoundFileReceipt `
            $groundHeader $groundHeaderBinding 'acceptance Ground header boundary'
        GroundSource=Get-PathBoundFileReceipt `
            $groundSource $groundSourceBinding 'acceptance Ground source boundary'
        NativePluginSourceTree=@($currentTree)
        R30R29R25Preservation=$preservedScene
    }
}

function Invoke-R30VisualReviewAcceptance {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $ExpectedRunToken
    )
    Assert-NativeIdle 'before explicit R30 visual-review acceptance'
    $protectedBefore = @(Get-ProtectedProcesses)
    Assert-ProtectedUnchanged $protectedBefore
    $pendingAdmission = Assert-R30PendingCaptureReceipt `
        $Path $ExpectedSha256 $ExpectedRunToken
    $pending = $pendingAdmission.Receipt
    $binding = $pendingAdmission.BindingEvidence
    $nativeBoundaryBefore = Get-R30AcceptanceNativeBoundary $pendingAdmission
    $nativeBoundaryAfter = Get-R30AcceptanceNativeBoundary $pendingAdmission
    Assert-JsonIdentity `
        $nativeBoundaryBefore $nativeBoundaryAfter `
        'native state across explicit visual acceptance' 10
    [void] (Assert-State `
        $pendingAdmission.File $pendingAdmission.Path `
        'hash-pinned pending receipt before acceptance publication')
    Assert-ProtectedUnchanged $protectedBefore
    Assert-NativeIdle 'before explicit R30 visual acceptance publication'

    $acceptedReceipt = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='COMMITTED'
        RunToken=$ExpectedRunToken
        NativeOrder='R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31'
        R31DependencyAllowed=$false
        PendingCaptureAdmission=[pscustomobject] [ordered] @{
            Path=$pendingAdmission.Path
            File=$pendingAdmission.File
            CallerSha256=$ExpectedSha256.ToUpperInvariant()
            Status='PENDING_VISUAL_REVIEW'
        }
        StaticReceipt=(Get-RequiredPropertyValue $pending 'StaticReceipt' 'pending R30 capture receipt')
        PrewriteAdmission=(Get-RequiredPropertyValue $pending 'PrewriteAdmission' 'pending R30 capture receipt')
        R30CommitAdmission=(Get-RequiredPropertyValue $pending 'R30CommitAdmission' 'pending R30 capture receipt')
        ImmutableNativeState=$pendingAdmission.ImmutableNativeState
        BindingEvidence=$binding
        Map=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'MapCaptureBinding' 'pending R30 binding evidence') `
            'State' 'pending R30 map binding')
        RuntimeEditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'RuntimeEditorDllCaptureBinding' 'pending R30 binding evidence') `
            'State' 'pending R30 runtime binding')
        EditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'EditorDllCaptureBinding' 'pending R30 binding evidence') `
            'State' 'pending R30 editor binding')
        GroundHeader=(Get-RequiredPropertyValue $binding 'GroundHeaderCaptureBinding' 'pending R30 binding evidence')
        GroundSource=(Get-RequiredPropertyValue $binding 'GroundSourceCaptureBinding' 'pending R30 binding evidence')
        NativePluginSourceTree=(Get-RequiredPropertyValue `
            $binding 'NativePluginSourceTreeCaptureBinding' 'pending R30 binding evidence')
        SourcePromotion=(Get-RequiredPropertyValue $pending 'SourcePromotion' 'pending R30 capture receipt')
        GameBinaryBefore=(Get-RequiredPropertyValue $pending 'GameBinaryBefore' 'pending R30 capture receipt')
        GameBinaryAfter=(Get-RequiredPropertyValue $pending 'GameBinaryAfter' 'pending R30 capture receipt')
        Build=(Get-RequiredPropertyValue $pending 'Build' 'pending R30 capture receipt')
        Cook=(Get-RequiredPropertyValue $pending 'Cook' 'pending R30 capture receipt')
        CookedClosure=(Get-RequiredPropertyValue $pending 'CookedClosure' 'pending R30 capture receipt')
        CookedClosurePostflight=(Get-RequiredPropertyValue $pending 'CookedClosurePostflight' 'pending R30 capture receipt')
        Game=(Get-RequiredPropertyValue $pending 'Game' 'pending R30 capture receipt')
        GameStartupHybridContract=
            $pendingAdmission.GameStartupHybridContract
        LandmarkRuntimeContractsPreCapture=
            $pendingAdmission.LandmarkRuntimeContractsPreCapture
        LandmarkRuntimeContractsPostCapture=
            $pendingAdmission.LandmarkRuntimeContractsPostCapture
        FinishTransportErrorAfterExit=(Get-RequiredPropertyValue `
            $pending 'FinishTransportErrorAfterExit' 'pending R30 capture receipt')
        Captures=(Get-RequiredPropertyValue $pending 'Captures' 'pending R30 capture receipt')
        ExactPoseCount=5
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$true
        HumanVisualReviewAttested=$true
        ConfirmedFiveImagesReviewed=$true
        AutomaticVisualAcceptanceAllowed=$false
        VisualReviewRequired=$false
        VisualReviewAccepted=$true
        ProviderFallbackVisualQaAccepted=$true
        TreeMaterialResponseV3Reviewed=$true
        R31AdmissionAuthorized=$true
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        HyperrealismClaimed=$false
        CesiumProviderEvidencePresent=$true
        CesiumProviderReadyEvidencePresent=$false
        AirSimTriadRuntimeRetained=$true
        LegacyAirSimProjectStateChanged=$false
        AirSimDisableArgumentsUsed=$false
        MapModifiedByCapture=$false
        SimulationCollisionNavigationSensorRfModified=$false
        NativeStateMutatedByAcceptance=$false
        UnrealLaunchedByAcceptance=$false
        AcceptanceRevalidation=[pscustomobject] [ordered] @{
            ImmediatePreAcceptance=$nativeBoundaryBefore
            ImmediatePostAcceptance=$nativeBoundaryAfter
            Unchanged=$true
            PendingReceiptHashPinned=$true
            FivePngsRedecodedAndRehashed=$true
            MapRuntimeEditorDllRevalidated=$true
            GroundHeaderAndSourceRevalidated=$true
            FullNativePluginSourceTreeRevalidated=$true
            R30R29R25PreservationRevalidated=$true
            TreeMaterialResponseV3SourceAndContentClosureRevalidated=$true
            TreeResponseMaterials13AndRuntimeMids26Revalidated=$true
            HybridTransitiveRuntimeProofRevalidated=$true
            LandmarkRuntimePropertyReceiptsRevalidated=$true
            LandmarkRuntimePropertyReceiptsCarriedForward=$true
            NativeReceiptWriteAllowed=$false
            EvidenceReceiptWriteOnly=$true
        }
        Postflight=(Get-RequiredPropertyValue $pending 'Postflight' 'pending R30 capture receipt')
        AcceptedUtc=[DateTime]::UtcNow.ToString('o')
    }

    $evidenceRoot = $pendingAdmission.EvidenceRoot
    $candidatePath = Join-Path $evidenceRoot ".commit.candidate.$PID.json"
    $commitPath = Join-Path $evidenceRoot 'commit.json'
    if ([IO.File]::Exists($candidatePath) -or [IO.File]::Exists($commitPath)) {
        throw 'Explicit R30 visual acceptance refuses an existing candidate or commit receipt.'
    }
    try {
        [void] (Write-JsonAtomic $acceptedReceipt $candidatePath $evidenceRoot)
        $nativeBoundaryAfterCandidateWrite = `
            Get-R30AcceptanceNativeBoundary $pendingAdmission
        Assert-JsonIdentity `
            $nativeBoundaryBefore $nativeBoundaryAfterCandidateWrite `
            'native state after bounded acceptance-candidate evidence write' 10
        [void] (Assert-State `
            $pendingAdmission.File $pendingAdmission.Path `
            'hash-pinned pending receipt after acceptance-candidate write')
        Assert-ProtectedUnchanged $protectedBefore
        Assert-NativeIdle 'before atomic R30 acceptance publication'
        Move-Item -LiteralPath $candidatePath -Destination $commitPath
        [void] (Get-FileState $commitPath)
    }
    finally {
        if ([IO.File]::Exists($candidatePath)) {
            Remove-Item -LiteralPath $candidatePath -Force
        }
    }
    $acceptedReceipt
}

$staticReceipt = Assert-StaticContract
$activeModeCount = 0
foreach ($mode in @($StaticSelfCheck, $Execute, $AcceptVisualReview)) {
    if ($mode) { ++$activeModeCount }
}
if ($activeModeCount -gt 1) {
    throw '-StaticSelfCheck, -Execute, and -AcceptVisualReview are mutually exclusive.'
}
if ($ConfirmFiveImagesReviewed -and -not $AcceptVisualReview) {
    throw '-ConfirmFiveImagesReviewed is valid only with -AcceptVisualReview.'
}
if ($StaticSelfCheck) {
    $staticReceipt | ConvertTo-Json -Depth 12
    exit 0
}
if ($AcceptVisualReview) {
    foreach ($requiredParameter in @(
        'RunToken',
        'PendingCaptureReceipt',
        'ExpectedPendingCaptureReceiptSha256',
        'ConfirmFiveImagesReviewed')) {
        if (-not $PSBoundParameters.ContainsKey($requiredParameter)) {
            throw "Explicit R30 visual acceptance requires parameter: $requiredParameter"
        }
    }
    if ($RunToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$' -or
        $RunToken -ceq 'static-contract') {
        throw 'Explicit R30 visual acceptance requires the pending run token.'
    }
    if (-not $ConfirmFiveImagesReviewed) {
        throw 'Explicit R30 visual acceptance requires confirmation that all five PNGs were human-reviewed.'
    }
    foreach ($captureOnlyParameter in @(
        'R30CommitReceipt', 'ExpectedR30CommitReceiptSha256')) {
        if ($PSBoundParameters.ContainsKey($captureOnlyParameter)) {
            throw "Explicit visual acceptance refuses capture-only parameter: $captureOnlyParameter"
        }
    }
    Invoke-R30VisualReviewAcceptance `
        $PendingCaptureReceipt `
        $ExpectedPendingCaptureReceiptSha256 `
        $RunToken | ConvertTo-Json -Depth 24
    exit 0
}
if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema=$schema
        Status='READ_ONLY_REPOSITORY_PREFLIGHT_PASS'
        ExecuteRequiredForNativeAccess=$true
        NativeProjectAccessed=$false
        NativeTreeWritten=$false
        UnrealLaunched=$false
        TwoPhaseVisualReviewRequired=$true
        AutomaticVisualAcceptanceAllowed=$false
        StaticReceipt=$staticReceipt
    } | ConvertTo-Json -Depth 12
    exit 0
}

foreach ($acceptanceOnlyParameter in @(
    'PendingCaptureReceipt',
    'ExpectedPendingCaptureReceiptSha256',
    'ConfirmFiveImagesReviewed')) {
    if ($PSBoundParameters.ContainsKey($acceptanceOnlyParameter)) {
        throw "Live capture refuses acceptance-only parameter: $acceptanceOnlyParameter"
    }
}

foreach ($requiredParameter in @(
    'RunToken',
    'R30CommitReceipt',
    'ExpectedR30CommitReceiptSha256')) {
    if (-not $PSBoundParameters.ContainsKey($requiredParameter)) {
        throw "Live R30 Player0 capture requires explicit parameter: $requiredParameter"
    }
}
if ($RunToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$' -or
    $RunToken -ceq 'static-contract') {
    throw 'Live R30 Player0 capture requires a new safe run token.'
}

$requiredNativeFiles = @(
    $nativeProjectFile,
    $mapFile,
    $runtimeEditorDll,
    $editorDll,
    $cookedRuntimeHotfixCommitReceipt,
    $groundHeader,
    $groundSource,
    $dotnet,
    $unrealBuildTool,
    $unrealEditorCmd,
    $airSimRuntimeDescriptor)
foreach ($requiredFile in $requiredNativeFiles) {
    if (-not [IO.File]::Exists($requiredFile)) {
        throw "Live capture prerequisite is absent: $requiredFile"
    }
}
if (-not [IO.Directory]::Exists($engineSourceRoot)) {
    throw "UE 5.5 source root is absent: $engineSourceRoot"
}

$script:protectedBefore = @(Get-ProtectedProcesses)
Assert-ProtectedUnchanged $script:protectedBefore
Assert-NativeIdle 'R30 capture preflight'
Assert-RcPortUnowned
$r30Admission = Assert-R30CommitReceipt `
    $R30CommitReceipt $ExpectedR30CommitReceiptSha256
$r30ReceiptFileBefore = Get-FileState $r30Admission.Path
$hotfixReceiptFileBefore = Get-FileState `
    $r30Admission.CookedRuntimeHotfixAdmission.Path
$airSimProjectBefore = Assert-AirSimProjectConfiguration
$r30TreeBefore = @(Assert-R30NativeContent)
$columnarDdcSeedSourceBefore = @(Assert-ColumnarTreeDdcSeedSource)
$immutableBefore = [pscustomobject] [ordered] @{
    ProjectFile=$airSimProjectBefore.ProjectFile
    AirSimDescriptor=$airSimProjectBefore.AirSimTriadRuntimeDescriptor
    R30=@($r30TreeBefore)
    R29Facade=@(Get-TreeReceipt $r29FacadeContentRoot)
    R29Vegetation=@(Get-TreeReceipt $r29VegetationContentRoot)
    R29Terrain=@(Get-TreeReceipt $r29TerrainContentRoot)
    TreeRealism=@(Get-TreeReceipt $treeRealismContentRoot)
    ContextFacadeR25=@(Get-TreeReceipt $contextFacadeR25ContentRoot)
    AirSimContent=@(Get-TreeReceipt $airSimRuntimeContentRoot)
    CookedRuntimeHotfixCommit=$hotfixReceiptFileBefore
}
foreach ($treeName in @(
    'R29Facade', 'R29Vegetation', 'R29Terrain', 'TreeRealism',
    'ContextFacadeR25', 'AirSimContent')) {
    if (@($immutableBefore.$treeName).Count -eq 0) {
        throw "Required immutable native dependency tree is empty: $treeName"
    }
}

$evidenceRoot = [IO.Path]::GetFullPath(
    (Join-Path $nativeEvidenceBase $RunToken))
if (-not [IO.Path]::GetDirectoryName($evidenceRoot).Equals(
        $nativeEvidenceBase, [StringComparison]::OrdinalIgnoreCase) -or
    -not [IO.Path]::GetFileName($evidenceRoot).Equals(
        $RunToken, [StringComparison]::Ordinal) -or
    [IO.Directory]::Exists($evidenceRoot) -or
    [IO.File]::Exists($evidenceRoot)) {
    throw "Evidence root must be a new direct run-token child: $evidenceRoot"
}
$prewriteAdmission = Assert-LaunchAdmission `
    'before first isolated R30 capture write'
Assert-ProtectedUnchanged $script:protectedBefore
Assert-NativeIdle 'before first isolated R30 capture write'

$transactionStarted = $false
$captureCompleted = $false
$primaryFailure = $null
$rollbackErrors = [Collections.Generic.List[string]]::new()
$sourceJournal = @()
$buildJournal = @()
$buildReceipt = $null
$treePrewarmProcessReceipts = [Collections.Generic.List[object]]::new()
$treePrewarmRuntimeIsolationReceipts = [Collections.Generic.List[object]]::new()
$treePrewarmShaderWorkerReceipts = [Collections.Generic.List[object]]::new()
$treePrewarmBuildReceipts = [Collections.Generic.List[object]]::new()
$treeDdcProbeProcessReceipt = $null
$treeDdcProbeRuntimeIsolation = $null
$treeDdcProbeShaderWorkerMode = $null
$treeDdcProbeHits = $null
$treeDdcScratchCleanupReceipt = $null
$columnarDdcSeedDestinationReceipt = [Collections.Generic.List[object]]::new()
$columnarDdcSeedSourceAfter = @()
$cookProcessReceipt = $null
$gameProcessReceipt = $null
$cookClosure = $null
$captureReceipts = [Collections.Generic.List[object]]::new()
$gameBefore = Get-FileState $gameExe
$finishTransportError = ''
$nativePluginSourceTreeBaseline = @()
$nativePluginSourceTreeImmediatePreCapture = @()
$nativePluginSourceTreeImmediatePostCapture = @()
$groundHeaderBefore = $null
$groundSourceBefore = $null
$groundHeaderImmediatePreCapture = $null
$groundSourceImmediatePreCapture = $null
$groundHeaderImmediatePostCapture = $null
$groundSourceImmediatePostCapture = $null
$gameStartupHybridContract = $null
$landmarkRuntimeContractsPreCapture = $null
$landmarkRuntimeContractsPostCapture = $null

try {
    [void] [IO.Directory]::CreateDirectory($evidenceRoot)
    $transactionStarted = $true
    $logsRoot = Join-Path $evidenceRoot 'logs'
    $journalRoot = Join-Path $evidenceRoot 'rollback'
    [void] [IO.Directory]::CreateDirectory($logsRoot)

    $sourceDestinations = @($captureSourcePins | ForEach-Object {
        [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $_.RelativePath))
    })
    $sourceJournal = @(New-FileJournal `
        $sourceDestinations (Join-Path $journalRoot 'source'))
    $buildJournal = @(New-TreeJournal @(
        $projectBinaryRoot,
        $projectIntermediateRoot,
        $pluginBinaryRoot,
        $pluginIntermediateRoot,
        $airSimBinaryRoot,
        $airSimIntermediateRoot) (Join-Path $journalRoot 'build'))
    $admissionReceipt = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='ADMITTED_BEFORE_NATIVE_SOURCE_PROMOTION'
        RunToken=$RunToken
        PrewriteAdmission=$prewriteAdmission
        R30Commit=$r30Admission
        ImmutableNativeState=$immutableBefore
        GameBinaryBefore=$gameBefore
        SourceJournalCount=$sourceJournal.Count
        BuildTreeJournalCount=$buildJournal.Count
        AirSimDisableArgumentsAllowed=$false
        ProviderReadyProofClaimed=$false
        R31DependencyAllowed=$false
    }
    [void] (Write-JsonAtomic `
        $admissionReceipt (Join-Path $evidenceRoot 'admission.json') `
        $evidenceRoot)

    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before hash-pinned runtime source promotion'
    foreach ($pin in $captureSourcePins) {
        $source = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RelativePath))
        $destination = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $destination $nativeProjectRoot)) {
            throw "Capture source promotion escaped native project: $destination"
        }
        [void] [IO.Directory]::CreateDirectory(
            [IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination -Force
        [IO.File]::SetLastWriteTimeUtc($destination, [DateTime]::UtcNow)
    }
    Assert-CaptureSourcePins $nativeProjectRoot
    $nativePluginSourceTreeBaseline = @(
        Assert-CaptureSourceTreeSnapshot `
            $null 'after R30 capture-source promotion')
    $groundHeaderBefore = Get-PathBoundFileReceipt `
        $groundHeader (Get-FileState $groundHeader) `
        'GroundVegetation header after R30 capture-source promotion'
    $groundSourceBefore = Get-PathBoundFileReceipt `
        $groundSource (Get-FileState $groundSource) `
        'GroundVegetation source after R30 capture-source promotion'

    $buildStartedUtc = [DateTime]::UtcNow
    $buildStdout = Join-Path $logsRoot 'build.stdout.log'
    $buildStderr = Join-Path $logsRoot 'build.stderr.log'
    $buildArguments = @(
        $unrealBuildTool,
        'TRIAD',
        'Win64',
        'Development',
        "-Project=$nativeProjectFile",
        '-WaitMutex',
        '-NoHotReloadFromIDE',
        '-ForceHeaderGeneration',
        '-NoUBTMakefiles',
        '-MaxParallelActions=1',
        '-NoUBA',
        '-NoUBALocal')
    $buildReceipt = Invoke-GuardedCommand `
        -Label 'R30 standalone Development Game build' `
        -FilePath $dotnet `
        -Arguments $buildArguments `
        -WorkingDirectory $nativeProjectRoot `
        -StandardOutputLog $buildStdout `
        -StandardErrorLog $buildStderr `
        -RequiredCommandLineTokens @(
            $unrealBuildTool, 'TRIAD', 'Win64', 'Development',
            $nativeProjectFile) `
        -TimeoutSeconds $BuildTimeoutSeconds
    Assert-CaptureSourcePins $nativeProjectRoot
    $gameAfterBuild = Assert-FreshFileState `
        $gameExe $buildStartedUtc 'standalone R30 Development Game executable'
    Assert-BinaryMarkers $gameExe @(
        'UTRIADIstanaExploreV5DR30Player0CaptureLibrary',
        'GetIstanaExploreV5DR30Player0CaptureState',
        'ISTANA_EXPLORE_V5D_R30_PLAYER0_FALLBACK_CAPTURE_ACCEPTED',
        'ATRIADIstanaExploreV5DR30FacadeLookdevActor',
        'AirSimTriadRuntime',
        'WeatherActor')
    [void] (Assert-CaptureImmutableState `
        $immutableBefore $r30Admission $r30ReceiptFileBefore)

    $isolatedDdcRoot = Join-Path $evidenceRoot 'DDC'
    $isolatedDdcChildEnvironment = @{
        'UE-LocalDataCachePath'=$isolatedDdcRoot
    }
    if (-not (Test-ContainedPath $isolatedDdcRoot $evidenceRoot)) {
        throw "Per-run DDC escaped its evidence root: $isolatedDdcRoot"
    }
    foreach ($pin in $columnarTreeDdcSeedPins) {
        $source = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectDdcRoot $pin.RelativePath))
        $destination = [IO.Path]::GetFullPath(
            (Join-Path $isolatedDdcRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $source $nativeProjectDdcRoot) -or
            -not (Test-ContainedPath $destination $isolatedDdcRoot)) {
            throw 'Columnar TreeRealism DDC seed copy escaped an exact root.'
        }
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        }) $source 'Columnar TreeRealism DDC seed copy source')
        [void] [IO.Directory]::CreateDirectory(
            [IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination
        $destinationState = Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        }) $destination 'per-run Columnar TreeRealism DDC seed copy'
        $columnarDdcSeedDestinationReceipt.Add([pscustomobject] [ordered] @{
            Path=$destination
            RelativePath=[string] $pin.RelativePath
            Present=[bool] $destinationState.Present
            Bytes=[int64] $destinationState.Bytes
            Sha256=[string] $destinationState.Sha256
            LastWriteUtc=[string] $destinationState.LastWriteUtc
        })
    }
    Assert-JsonIdentity `
        $columnarDdcSeedSourceBefore `
        @(Assert-ColumnarTreeDdcSeedSource) `
        'Columnar TreeRealism DDC acceleration seed source after copy' 8

    $treePrewarmOutputRoot = Join-Path $evidenceRoot 'TreeDdcPrewarmCooked'
    for ($prewarmIndex = 0;
         $prewarmIndex -lt $treeDerivativeMeshPrewarmPackages.Count;
         ++$prewarmIndex) {
        $prewarmPackage = $treeDerivativeMeshPrewarmPackages[$prewarmIndex]
        $prewarmAssetName = $prewarmPackage.Substring(
            $prewarmPackage.LastIndexOf('/') + 1)
        $prewarmOrdinal = '{0:D2}' -f ($prewarmIndex + 1)
        $prewarmLabel =
            "R30 isolated TreeRealism DDC prewarm $prewarmOrdinal $prewarmAssetName"
        $prewarmRuntimeLog = Join-Path $logsRoot `
            "tree-ddc-prewarm-$prewarmOrdinal.runtime.log"
        $prewarmStdout = Join-Path $logsRoot `
            "tree-ddc-prewarm-$prewarmOrdinal.stdout.log"
        $prewarmStderr = Join-Path $logsRoot `
            "tree-ddc-prewarm-$prewarmOrdinal.stderr.log"
        $prewarmOutputPattern = Join-Path $treePrewarmOutputRoot `
            "$prewarmOrdinal\[Platform]"
        $prewarmArguments = @(
            $nativeProjectFile,
            '-run=Cook',
            '-TargetPlatform=Windows',
            "-Package=$prewarmPackage",
            '-cooksinglepackage',
            "-OutputDir=$prewarmOutputPattern",
            '-NullRHI',
            '-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true',
            '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8',
            '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8',
            '-asyncstaticmeshcompilation=0',
            '-LogCmds=LogStaticMesh Verbose',
            '-DDC=InstalledNoZenLocalFallback',
            '-unattended',
            '-nop4',
            '-NoSplash',
            '-NoSound',
            '-stdout',
            '-FullStdOutLogOutput',
            "-abslog=$prewarmRuntimeLog")
        $forbiddenPrewarmBuildPattern = if ($prewarmIndex -eq 0) {
            [regex]::Escape(
                "LogStaticMesh: Display: Building static mesh $prewarmAssetName ")
        }
        else { '' }
        $prewarmProcessReceipt = Invoke-GuardedCommand `
            -Label $prewarmLabel `
            -FilePath $unrealEditorCmd `
            -Arguments $prewarmArguments `
            -WorkingDirectory $nativeProjectRoot `
            -StandardOutputLog $prewarmStdout `
            -StandardErrorLog $prewarmStderr `
            -RequiredCommandLineTokens @(
                $nativeProjectFile, '-run=Cook', '-TargetPlatform=Windows',
                "-Package=$prewarmPackage", '-cooksinglepackage',
                "-OutputDir=$prewarmOutputPattern",
                '-DDC=InstalledNoZenLocalFallback',
                '-asyncstaticmeshcompilation=0',
                'LogStaticMesh Verbose') `
            -ChildEnvironment $isolatedDdcChildEnvironment `
            -TimeoutSeconds $CookTimeoutSeconds `
            -RuntimeLogPath $prewarmRuntimeLog `
            -ForbiddenRuntimeLogPattern $forbiddenPrewarmBuildPattern
        Assert-NoFatalRuntimeLog `
            $prewarmRuntimeLog $prewarmLabel `
            -AllowExpectedColdCookShaderCompile
        $treePrewarmProcessReceipts.Add($prewarmProcessReceipt)
        $treePrewarmRuntimeIsolationReceipts.Add((
            Assert-IsolatedDdcRuntimeLog `
                -Path $prewarmRuntimeLog `
                -Label $prewarmLabel `
                -ExpectedDdcRoot $isolatedDdcRoot))
        $treePrewarmShaderWorkerReceipts.Add((
            Assert-DedicatedShaderWorkerRuntimeLog `
                -Path $prewarmRuntimeLog `
                -Label $prewarmLabel `
                -AllowCleanBoundedShaderBatchWithoutJobCacheStats:($prewarmIndex -gt 0)))
        $prewarmResult = Assert-TreeDerivativeMeshPrewarmResultLog `
            -Path $prewarmRuntimeLog `
            -PackagePath $prewarmPackage `
            -Label $prewarmLabel
        if ($prewarmIndex -eq 0 -and
            [string] $prewarmResult.Mode -cne 'EXACT_DDC_HIT') {
            throw 'The seeded largest TreeRealism mesh was not an exact cache-only DDC hit.'
        }
        $treePrewarmBuildReceipts.Add($prewarmResult)
        [void] (Assert-CaptureImmutableState `
            $immutableBefore $r30Admission $r30ReceiptFileBefore)
    }

    $treeDdcProbePackageArgument = [string]::Join(
        '+', $treeDerivativeMeshPrewarmPackages)
    $treeDdcProbeOutputPattern = Join-Path $evidenceRoot `
        'TreeDdcProbeCooked\[Platform]'
    $treeDdcProbeRuntimeLog = Join-Path $logsRoot `
        'tree-ddc-probe.runtime.log'
    $treeDdcProbeStdout = Join-Path $logsRoot 'tree-ddc-probe.stdout.log'
    $treeDdcProbeStderr = Join-Path $logsRoot 'tree-ddc-probe.stderr.log'
    $treeDdcProbeLabel = 'R30 isolated TreeRealism cache-only DDC probe'
    $treeDdcProbeArguments = @(
        $nativeProjectFile,
        '-run=Cook',
        '-TargetPlatform=Windows',
        "-Package=$treeDdcProbePackageArgument",
        '-cooksinglepackage',
        "-OutputDir=$treeDdcProbeOutputPattern",
        '-NullRHI',
        '-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true',
        '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8',
        '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8',
        '-asyncstaticmeshcompilation=0',
        '-LogCmds=LogStaticMesh Verbose',
        '-DDC=InstalledNoZenLocalFallback',
        '-unattended',
        '-nop4',
        '-NoSplash',
        '-NoSound',
        '-stdout',
        '-FullStdOutLogOutput',
        "-abslog=$treeDdcProbeRuntimeLog")
    $treeDdcProbeProcessReceipt = Invoke-GuardedCommand `
        -Label $treeDdcProbeLabel `
        -FilePath $unrealEditorCmd `
        -Arguments $treeDdcProbeArguments `
        -WorkingDirectory $nativeProjectRoot `
        -StandardOutputLog $treeDdcProbeStdout `
        -StandardErrorLog $treeDdcProbeStderr `
        -RequiredCommandLineTokens @(
            $nativeProjectFile, '-run=Cook', '-TargetPlatform=Windows',
            "-Package=$treeDdcProbePackageArgument", '-cooksinglepackage',
            "-OutputDir=$treeDdcProbeOutputPattern",
            '-DDC=InstalledNoZenLocalFallback',
            '-asyncstaticmeshcompilation=0',
            'LogStaticMesh Verbose') `
        -ChildEnvironment $isolatedDdcChildEnvironment `
        -TimeoutSeconds $CookTimeoutSeconds
    Assert-NoFatalRuntimeLog `
        $treeDdcProbeRuntimeLog $treeDdcProbeLabel
    $treeDdcProbeRuntimeIsolation = Assert-IsolatedDdcRuntimeLog `
        -Path $treeDdcProbeRuntimeLog `
        -Label $treeDdcProbeLabel `
        -ExpectedDdcRoot $isolatedDdcRoot
    $treeDdcProbeShaderWorkerMode =
        Assert-DedicatedShaderWorkerRuntimeLog `
            -Path $treeDdcProbeRuntimeLog `
            -Label $treeDdcProbeLabel `
            -AllowNoShaderJobs
    $treeDdcProbeHits = Assert-TreeDerivativeMeshDdcHitLog `
        -Path $treeDdcProbeRuntimeLog `
        -PackagePaths $treeDerivativeMeshPrewarmPackages `
        -Label $treeDdcProbeLabel
    $treeDdcProbeOutputRoot = Join-Path $evidenceRoot 'TreeDdcProbeCooked'
    Remove-ContainedDirectory $treePrewarmOutputRoot $evidenceRoot
    Remove-ContainedDirectory $treeDdcProbeOutputRoot $evidenceRoot
    if ([IO.Directory]::Exists($treePrewarmOutputRoot) -or
        [IO.Directory]::Exists($treeDdcProbeOutputRoot)) {
        throw 'Bounded TreeRealism prewarm/probe cooked-output cleanup failed.'
    }
    $treeDdcScratchCleanupReceipt = [pscustomobject] [ordered] @{
        TreeDdcPrewarmCookedRemoved=$true
        TreeDdcProbeCookedRemoved=$true
        DdcRetainedForFullCook=$true
        ExactEvidenceChildrenOnly=$true
    }
    [void] (Assert-CaptureImmutableState `
        $immutableBefore $r30Admission $r30ReceiptFileBefore)

    $cookStartedUtc = [DateTime]::UtcNow
    $cookOutputPattern = Join-Path $evidenceRoot 'Cooked\[Platform]'
    $cookedPlatformRoot = Join-Path $evidenceRoot 'Cooked\Windows'
    $cookRuntimeLog = Join-Path $logsRoot 'cook.runtime.log'
    $cookStdout = Join-Path $logsRoot 'cook.stdout.log'
    $cookStderr = Join-Path $logsRoot 'cook.stderr.log'
    $fullCookLabel =
        'R30 full closure cook after bounded per-run TreeRealism DDC prewarm'
    $cookArguments = @(
        $nativeProjectFile,
        '-run=Cook',
        '-TargetPlatform=Windows',
        "-Map=$mapPackage",
        "-OutputDir=$cookOutputPattern",
        "-COOKDIR=$airSimRuntimeContentRoot",
        '-NullRHI',
        '-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true',
        '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8',
        '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8',
        '-ini:Editor:[CookSettings]:PackagesPerGC=32',
        '-asyncstaticmeshcompilation=0',
        '-LogCmds=LogStaticMesh Verbose',
        '-DDC=InstalledNoZenLocalFallback',
        '-unattended',
        '-nop4',
        '-NoSplash',
        '-NoSound',
        '-stdout',
        '-FullStdOutLogOutput',
        "-abslog=$cookRuntimeLog")
    $cookProcessReceipt = Invoke-GuardedCommand `
        -Label $fullCookLabel `
        -FilePath $unrealEditorCmd `
        -Arguments $cookArguments `
        -WorkingDirectory $nativeProjectRoot `
        -StandardOutputLog $cookStdout `
        -StandardErrorLog $cookStderr `
        -RequiredCommandLineTokens @(
            $nativeProjectFile, '-run=Cook', '-TargetPlatform=Windows',
            "-Map=$mapPackage", "-OutputDir=$cookOutputPattern",
            "-COOKDIR=$airSimRuntimeContentRoot",
            '-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true',
            '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8',
            '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8',
            '-ini:Editor:[CookSettings]:PackagesPerGC=32',
            '-DDC=InstalledNoZenLocalFallback',
            '-asyncstaticmeshcompilation=0',
            'LogStaticMesh Verbose') `
        -ChildEnvironment $isolatedDdcChildEnvironment `
        -TimeoutSeconds $CookTimeoutSeconds
    Assert-NoFatalRuntimeLog `
        $cookRuntimeLog $fullCookLabel `
        -AllowExpectedColdCookShaderCompile
    $cookRuntimeIsolation = Assert-IsolatedDdcRuntimeLog `
        -Path $cookRuntimeLog `
        -Label $fullCookLabel `
        -ExpectedDdcRoot $isolatedDdcRoot
    $cookShaderWorkerMode = Assert-DedicatedShaderWorkerRuntimeLog `
        -Path $cookRuntimeLog `
        -Label $fullCookLabel `
        -AllowNoShaderJobs
    $cookTreeDerivativeMeshDdcHits =
        Assert-TreeDerivativeMeshDdcHitLog `
            -Path $cookRuntimeLog `
            -PackagePaths $treeDerivativeMeshPrewarmPackages `
            -Label $fullCookLabel
    $cookClosure = Assert-CookedClosure `
        $evidenceRoot $cookedPlatformRoot $cookStartedUtc
    [void] (Assert-CaptureImmutableState `
        $immutableBefore $r30Admission $r30ReceiptFileBefore)
    $nativePluginSourceTreeImmediatePreCapture = @(
        Assert-CaptureSourceTreeSnapshot `
            $nativePluginSourceTreeBaseline 'immediate pre-capture')
    $groundHeaderImmediatePreCapture = Get-PathBoundFileReceipt `
        $groundHeader $groundHeaderBefore `
        'immediate pre-capture GroundVegetation header'
    $groundSourceImmediatePreCapture = Get-PathBoundFileReceipt `
        $groundSource $groundSourceBefore `
        'immediate pre-capture GroundVegetation source'

    Assert-RcPortUnowned
    $gameRuntimeLog = Join-Path $logsRoot 'game.runtime.log'
    $gameStdout = Join-Path $logsRoot 'game.stdout.log'
    $gameStderr = Join-Path $logsRoot 'game.stderr.log'
    $sandboxRoot = [IO.Path]::GetFullPath($cookedPlatformRoot)
    $gameArguments = @(
        $nativeProjectFile,
        $mapPackage,
        '-game',
        "-Sandbox=$sandboxRoot",
        '-unattended',
        '-NoSplash',
        '-NoSound',
        '-NoAutoSave',
        '-RenderOffscreen',
        '-dx12',
        '-sm6',
        '-ResX=2560',
        '-ResY=1440',
        '-Windowed',
        '-ini:Engine:[HTTP]:HttpMaxConnectionsPerServer=12',
        '-ini:Engine:[/Script/CesiumRuntime.CesiumRuntimeSettings]:MaxCacheItems=32768',
        '-ini:Engine:[ConsoleVariables]:r.Streaming.PoolSize=768',
        '-DDC=InstalledNoZenLocalFallback',
        '-RemoteControlHttpServer',
        '-RCWebControlEnable',
        '-ExecCmds=WebControl.StartServer',
        "-TRIADR30CaptureRun=$RunToken",
        "-TRIADR30CaptureOutputRoot=$evidenceRoot",
        "-abslog=$gameRuntimeLog")
    $gameStartParameters = @{
        Label='R30 standalone cooked Player0 Game capture'
        FilePath=$gameExe
        Arguments=$gameArguments
        WorkingDirectory=$nativeProjectRoot
        StandardOutputLog=$gameStdout
        StandardErrorLog=$gameStderr
        RequiredCommandLineTokens=@(
            $nativeProjectFile, $mapPackage, '-game',
            "-Sandbox=$sandboxRoot",
            '-DDC=InstalledNoZenLocalFallback',
            "-TRIADR30CaptureRun=$RunToken",
            "-TRIADR30CaptureOutputRoot=$evidenceRoot")
        ChildEnvironment=$isolatedDdcChildEnvironment
    }
    $gameOwned = Start-GuardedOwnedProcess @gameStartParameters
    $gameStartupState = Wait-RuntimeCaptureReady `
        $gameRuntimeLog $CaptureTimeoutSeconds
    $gameStartupHybridContract = Assert-HybridCaptureStateResponse `
        $gameStartupState 'standalone R30 capture Game startup state'
    $gameStartupRuntimeContract =
        Assert-RequiredPlayer0RuntimeContractMarkers `
            $gameRuntimeLog 'standalone R30 capture Game startup'
    $landmarkRuntimeContractsPreCapture = Wait-LandmarkRuntimeContracts `
        $gameRuntimeLog 120 'PRE_CAPTURE'

    foreach ($pose in $poses) {
        $setPose = Invoke-RcCall `
            'SetIstanaExploreV5DR30Player0CapturePose' `
            @{ PoseId=[string] $pose.Id } 60
        $setMessage = [string] (Get-RequiredPropertyValue `
            $setPose 'OutMessage' "set pose $($pose.Id)")
        if ((Get-RequiredPropertyValue `
                $setPose 'ReturnValue' "set pose $($pose.Id)") -ne $true -or
            -not $setMessage.Contains(
                'ISTANA_EXPLORE_V5D_R30_PLAYER0_POSE_PASS',
                [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                "poseId=$($pose.Id)", [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                'simulationSensorRfModified=false',
                [StringComparison]::Ordinal)) {
            throw "Unexpected exact pose acknowledgement: $setMessage"
        }
        $stableState = Wait-StableFallbackPose `
            $pose $CaptureTimeoutSeconds
        $immediatePreCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR30Player0CaptureState' @{} 60
        $preCaptureReport = Assert-ExactFallbackStateResponse `
            $immediatePreCapture $pose "pre-capture $($pose.Id)"
        $captureRequestedUtc = [DateTime]::UtcNow
        $capture = Invoke-RcCall `
            'CaptureIstanaExploreV5DR30Player0FallbackView' `
            @{ PoseId=[string] $pose.Id } $CaptureTimeoutSeconds
        $captureMessage = [string] (Get-RequiredPropertyValue `
            $capture 'OutMessage' "capture $($pose.Id)")
        if ((Get-RequiredPropertyValue `
                $capture 'ReturnValue' "capture $($pose.Id)") -ne $true -or
            -not $captureMessage.Contains(
                'ISTANA_EXPLORE_V5D_R30_PLAYER0_FALLBACK_CAPTURE_ACCEPTED',
                [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                "poseId=$($pose.Id)", [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                'providerFallbackVisualQa=true',
                [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                'providerReadyProofClaimed=false',
                [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                'airSimTriadRuntimeLoaded=true',
                [StringComparison]::Ordinal)) {
            throw "Unexpected ProviderFallback capture acknowledgement: $captureMessage"
        }
        $capturePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            "explore_v5d_r30_player0_$($pose.Id)_$RunToken.png"))
        if (-not (Test-ContainedPath $capturePath $evidenceRoot)) {
            throw "Expected capture path escaped evidence root: $capturePath"
        }
        $imageReceipt = Wait-StableDecodedPng `
            $capturePath $captureRequestedUtc $CaptureTimeoutSeconds
        $immediatePostCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR30Player0CaptureState' @{} 60
        $postCaptureReport = Assert-ExactFallbackStateResponse `
            $immediatePostCapture $pose "post-capture $($pose.Id)"
        $captureReceipts.Add([pscustomobject] [ordered] @{
            Pose=[pscustomobject] [ordered] @{
                Id=$pose.Id
                X=$pose.X; Y=$pose.Y; Z=$pose.Z
                Pitch=$pose.Pitch; Yaw=$pose.Yaw; Roll=$pose.Roll
            }
            SetPoseAcknowledgement=$setMessage
            StableProviderFallbackWindow=$stableState
            ImmediatePreCaptureState=$preCaptureReport
            CaptureAcknowledgement=$captureMessage
            Image=$imageReceipt
            ImmediatePostCaptureState=$postCaptureReport
            ProviderFallbackVisualQa=$true
            ProviderReadyProofClaimed=$false
        })
    }

    if ($captureReceipts.Count -ne 5 -or
        @($captureReceipts.Image.Sha256 | Sort-Object -Unique).Count -ne 5 -or
        @($captureReceipts.Image.DecodedBgraSha256 |
            Sort-Object -Unique).Count -ne 5) {
        throw 'The exact five Player0 images are absent or are not distinct at both file and decoded-pixel identity.'
    }
    $landmarkRuntimeContractsPostCapture = Wait-LandmarkRuntimeContracts `
        $gameRuntimeLog 120 'POST_FIFTH_CAPTURE'

    try {
        $finish = Invoke-RcCall `
            'FinishIstanaExploreV5DR30Player0CaptureRun' @{} 60
        $finishMessage = [string] (Get-RequiredPropertyValue `
            $finish 'OutMessage' 'finish capture run')
        if ((Get-RequiredPropertyValue `
                $finish 'ReturnValue' 'finish capture run') -ne $true -or
            -not $finishMessage.Contains(
                'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_EXIT_ACCEPTED',
                [StringComparison]::Ordinal)) {
            throw "Unexpected clean-exit acknowledgement: $finishMessage"
        }
    }
    catch {
        $gameOwned.Handle.Refresh()
        if (-not $gameOwned.Handle.HasExited) { throw }
        $finishTransportError = $_.Exception.Message
    }
    Wait-GuardedOwnedProcess $gameOwned $ShutdownTimeoutSeconds
    $gameProcessReceipt = Close-GuardedOwnedProcess $gameOwned
    $gameOwned = $null
    [void] (Wait-NativeMutationQuiescence `
        'after clean standalone Game exit' 60)
    Assert-NoFatalRuntimeLog $gameRuntimeLog 'standalone R30 capture Game'
    $gameFinalRuntimeContract =
        Assert-RequiredPlayer0RuntimeContractMarkers `
            $gameRuntimeLog 'standalone R30 capture Game final log'
    $gameRuntimeIsolation = Assert-IsolatedDdcRuntimeLog `
        -Path $gameRuntimeLog `
        -Label 'standalone R30 capture Game' `
        -ExpectedDdcRoot $isolatedDdcRoot
    $gameLogText = Read-RuntimeLogTextShared `
        -Path $gameRuntimeLog `
        -Label 'standalone R30 capture Game final log'
    if (-not $gameLogText.Contains(
            'ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_EXIT_ACCEPTED',
            [StringComparison]::Ordinal)) {
        throw 'Standalone Game log lacks the clean capture-exit marker.'
    }
    foreach ($captureReceipt in $captureReceipts) {
        [void] (Assert-State `
            $captureReceipt.Image $captureReceipt.Image.Path `
            "final Player0 PNG $($captureReceipt.Pose.Id)")
    }
    $cookedPostflight = Assert-CookedClosureUnchanged $cookClosure
    [void] (Assert-State `
        $gameAfterBuild $gameExe 'post-capture standalone Game executable')
    $postflight = Assert-CaptureImmutableState `
        $immutableBefore $r30Admission $r30ReceiptFileBefore
    $nativePluginSourceTreeImmediatePostCapture = @(
        Assert-CaptureSourceTreeSnapshot `
            $nativePluginSourceTreeBaseline 'immediate post-capture')
    $groundHeaderImmediatePostCapture = Get-PathBoundFileReceipt `
        $groundHeader $groundHeaderBefore `
        'immediate post-capture GroundVegetation header'
    $groundSourceImmediatePostCapture = Get-PathBoundFileReceipt `
        $groundSource $groundSourceBefore `
        'immediate post-capture GroundVegetation source'
    $columnarDdcSeedSourceAfter = @(Assert-ColumnarTreeDdcSeedSource)
    Assert-JsonIdentity `
        $columnarDdcSeedSourceBefore `
        $columnarDdcSeedSourceAfter `
        'Columnar TreeRealism DDC acceleration seed source post-capture' 8
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before R30 pending visual-review receipt'

    $bindingEvidence = [pscustomobject] [ordered] @{
        MapCaptureBinding=[pscustomobject] [ordered] @{
            Path=$mapFile
            State=$r30Admission.SuccessorMap
        }
        RuntimeEditorDllCaptureBinding=[pscustomobject] [ordered] @{
            Path=$runtimeEditorDll
            State=$r30Admission.RuntimeDllAfter
        }
        EditorDllCaptureBinding=[pscustomobject] [ordered] @{
            Path=$editorDll
            State=$r30Admission.EditorDllAfter
        }
        GroundHeaderCaptureBinding=[pscustomobject] [ordered] @{
            Path=$groundHeader
            Present=$groundHeaderBefore.Present
            Bytes=$groundHeaderBefore.Bytes
            Sha256=$groundHeaderBefore.Sha256
            LastWriteUtc=$groundHeaderBefore.LastWriteUtc
            ImmediatePreCapture=$groundHeaderImmediatePreCapture
            ImmediatePostCapture=$groundHeaderImmediatePostCapture
            Unchanged=$true
        }
        GroundSourceCaptureBinding=[pscustomobject] [ordered] @{
            Path=$groundSource
            Present=$groundSourceBefore.Present
            Bytes=$groundSourceBefore.Bytes
            Sha256=$groundSourceBefore.Sha256
            LastWriteUtc=$groundSourceBefore.LastWriteUtc
            ImmediatePreCapture=$groundSourceImmediatePreCapture
            ImmediatePostCapture=$groundSourceImmediatePostCapture
            Unchanged=$true
        }
        NativePluginSourceTreeCaptureBinding=[pscustomobject] [ordered] @{
            Root=$nativePluginSourceRoot
            BaselineAfterCaptureSourcePromotion=@(
                $nativePluginSourceTreeBaseline)
            ImmediatePreCapture=@(
                $nativePluginSourceTreeImmediatePreCapture)
            ImmediatePostCapture=@(
                $nativePluginSourceTreeImmediatePostCapture)
            FileCount=$nativePluginSourceTreeBaseline.Count
            Unchanged=$true
        }
    }
    $pendingCapture = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='PENDING_VISUAL_REVIEW'
        RunToken=$RunToken
        NativeOrder='R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31'
        R31DependencyAllowed=$false
        StaticReceipt=$staticReceipt
        PrewriteAdmission=$prewriteAdmission
        R30CommitAdmission=$r30Admission
        CookedRuntimeHotfixAdmission=$r30Admission.CookedRuntimeHotfixAdmission
        ImmutableNativeState=$immutableBefore
        BindingEvidence=$bindingEvidence
        SourcePromotion=[pscustomobject] [ordered] @{
            Transactional=$true
            SourceCount=2
            Pins=$captureSourcePins
            Before=@($sourceJournal | ForEach-Object {
                [pscustomobject] [ordered] @{
                    RelativePath=$_.RelativePath
                    State=$_.Before
                }
            })
            After=@($captureSourcePins | ForEach-Object {
                [pscustomobject] [ordered] @{
                    RelativePath=$_.RelativePath
                    State=Get-FileState (
                        Join-Path $nativeProjectRoot $_.RelativePath)
                }
            })
        }
        GameBinaryBefore=$gameBefore
        GameBinaryAfter=$gameAfterBuild
        Build=$buildReceipt
        TreeDerivativeMeshDdcPrewarm=[pscustomobject] [ordered] @{
            Strategy='PER_MESH_ISOLATED_PROCESS_LARGEST_FIRST'
            VisualAssetsModified=$false
            DdcScope='CURRENT_EVIDENCE_RUN_ONLY'
            AccelerationSeedKind='HASH_PINNED_SOURCE_STATIC_MESH_PAIR'
            AccelerationSeedSourceBefore=$columnarDdcSeedSourceBefore
            AccelerationSeedDestination=@($columnarDdcSeedDestinationReceipt)
            AccelerationSeedSourceAfter=$columnarDdcSeedSourceAfter
            ProcessReceipts=@($treePrewarmProcessReceipts)
            RuntimeIsolation=@($treePrewarmRuntimeIsolationReceipts)
            ShaderWorkerMode=@($treePrewarmShaderWorkerReceipts)
            BoundedPopulationProof=@($treePrewarmBuildReceipts)
            CacheOnlyProbeProcess=$treeDdcProbeProcessReceipt
            CacheOnlyProbeRuntimeIsolation=$treeDdcProbeRuntimeIsolation
            CacheOnlyProbeShaderWorkerMode=$treeDdcProbeShaderWorkerMode
            CacheOnlyProbeHits=$treeDdcProbeHits
            ScratchCookedOutputCleanup=$treeDdcScratchCleanupReceipt
        }
        Cook=$cookProcessReceipt
        CookRuntimeIsolation=$cookRuntimeIsolation
        CookShaderWorkerMode=$cookShaderWorkerMode
        CookTreeDerivativeMeshDdcHits=$cookTreeDerivativeMeshDdcHits
        CookedClosure=$cookClosure
        CookedClosurePostflight=$cookedPostflight
        Game=$gameProcessReceipt
        GameRuntimeIsolation=$gameRuntimeIsolation
        GameStartupRuntimeContract=$gameStartupRuntimeContract
        GameStartupHybridContract=$gameStartupHybridContract
        LandmarkRuntimeContractsPreCapture=$landmarkRuntimeContractsPreCapture
        LandmarkRuntimeContractsPostCapture=$landmarkRuntimeContractsPostCapture
        GameFinalRuntimeContract=$gameFinalRuntimeContract
        FinishTransportErrorAfterExit=$finishTransportError
        Captures=@($captureReceipts)
        ExactPoseCount=5
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$false
        HumanVisualReviewAttested=$false
        ConfirmedFiveImagesReviewed=$false
        AutomaticVisualAcceptanceAllowed=$false
        VisualReviewRequired=$true
        VisualReviewAccepted=$false
        ProviderFallbackVisualQaAccepted=$false
        TreeMaterialResponseV3Reviewed=$false
        R31AdmissionAuthorized=$false
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        HyperrealismClaimed=$false
        CesiumProviderEvidencePresent=$true
        CesiumProviderReadyEvidencePresent=$false
        AirSimTriadRuntimeRetained=$true
        LegacyAirSimProjectStateChanged=$false
        AirSimDisableArgumentsUsed=$false
        MechanicalNoMutationEvidence=[pscustomobject] [ordered] @{
            MapUnchanged=$true
            SimulationCollisionNavigationSensorRfUnchanged=$true
        }
        Postflight=$postflight
    }
    [void] (Write-JsonAtomic `
        $pendingCapture `
        (Join-Path $evidenceRoot 'pending-visual-review.json') `
        $evidenceRoot)
    $captureCompleted = $true
    $pendingCapture | ConvertTo-Json -Depth 24
}
catch {
    $primaryFailure = $_.Exception
}
finally {
    if (-not $captureCompleted -and $transactionStarted) {
        if ($null -ne $script:activeOwnedProcess) {
            try {
                [void] (Close-GuardedOwnedProcess `
                    $script:activeOwnedProcess -AllowContainment)
            }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        $rollbackMayMutate = $false
        try {
            [void] (Wait-NativeMutationQuiescence `
                'before R30 capture rollback' 60)
            $rollbackMayMutate = $true
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackMayMutate) {
            try { Restore-TreeJournal @($buildJournal) }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal @($sourceJournal) }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            foreach ($isolatedName in @(
                'Cooked', 'captures', 'DDC', 'TreeDdcPrewarmCooked',
                'TreeDdcProbeCooked')) {
                try {
                    Remove-ContainedDirectory `
                        (Join-Path $evidenceRoot $isolatedName) `
                        $evidenceRoot
                }
                catch { $rollbackErrors.Add($_.Exception.Message) }
            }
        }
        else {
            $rollbackErrors.Add(
                'Filesystem rollback was intentionally refused without exact native mutation quiescence; journals were retained.')
        }
        try {
            [void] (Assert-State `
                $r30Admission.SuccessorMap $mapFile 'rollback R30 map')
            [void] (Assert-State `
                $r30Admission.RuntimeDllAfter $runtimeEditorDll `
                'rollback R30 runtime editor DLL')
            [void] (Assert-State `
                $r30Admission.EditorDllAfter $editorDll `
                'rollback R30 editor DLL')
            [void] (Assert-TreeReceipt `
                $r30ContentRoot @($immutableBefore.R30) `
                'rollback R30 content')
            [void] (Assert-TreeReceipt `
                $airSimRuntimeContentRoot @($immutableBefore.AirSimContent) `
                'rollback AirSimTriadRuntime content')
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Assert-ProtectedUnchanged $script:protectedBefore }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        try {
            Assert-JsonIdentity `
                $columnarDdcSeedSourceBefore `
                @(Assert-ColumnarTreeDdcSeedSource) `
                'rollback Columnar TreeRealism DDC acceleration seed source' 8
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback = [pscustomobject] [ordered] @{
            Schema=$schema
            Status=if ($rollbackErrors.Count -eq 0) {
                'ROLLED_BACK'
            } else { 'ROLLBACK_INCOMPLETE' }
            RunToken=$RunToken
            Failure=if ($null -ne $primaryFailure) {
                $primaryFailure.Message
            } else { 'Unknown failure before commit.' }
            NativeMutationQuiescenceRequired=$true
            NativeMutationQuiescenceProven=$rollbackMayMutate
            ExactOwnedProcessContainmentOnly=$true
            SourceRestorationAttempted=$rollbackMayMutate
            BuildTreeRestorationAttempted=$rollbackMayMutate
            IsolatedCookCaptureDdcCleanupAttempted=$rollbackMayMutate
            R30MapAndContentPreserved=$true
            AirSimTriadRuntimePreserved=$true
            RollbackErrors=@($rollbackErrors)
        }
        try {
            [void] (Write-JsonAtomic `
                $rollback (Join-Path $evidenceRoot 'rollback.json') `
                $evidenceRoot)
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
    }
}

if ($null -ne $primaryFailure) {
    if ($rollbackErrors.Count -ne 0) {
        throw [InvalidOperationException]::new(
            "R30 capture failed and rollback was incomplete: capture={$($primaryFailure.Message)} rollback={$([string]::Join('; ', @($rollbackErrors)))}",
            $primaryFailure)
    }
    throw $primaryFailure
}
