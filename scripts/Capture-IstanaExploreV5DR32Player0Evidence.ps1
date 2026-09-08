<#
.SYNOPSIS
Build, freshly cook, and capture exact Player0 R32 turf evidence, then accept
only a separately hash-pinned pending receipt after human review.

.DESCRIPTION
Exactly one mode is required. -Execute consumes one caller-hash-pinned,
COMMITTED R32 native transaction receipt. That receipt must itself bind the
complete accepted R30/R31 chain. Execute promotes only the two R32 runtime
capture-library sources, performs a serial Development Game build and an
isolated fresh cook, captures ten fixed 2560x1440 SDR views, and emits only
PENDING_VISUAL_REVIEW. It never mechanically accepts visual quality.

-AcceptVisualReview requires the caller-pinned pending receipt and an explicit
ten-image human attestation. It re-decodes and re-hashes every PNG and
rechecks the map, DLL, Ground source, whole plugin source tree, transaction,
and predecessor receipts without launching Unreal or writing the native
project. Only that mode may emit the COMMITTED visual-QA receipt.

Repository-only self-check:
  .\Capture-IstanaExploreV5DR32Player0Evidence.ps1 -StaticSelfCheck

Live capture after the exact R32 transaction commits:
  .\Capture-IstanaExploreV5DR32Player0Evidence.ps1 -Execute `
    -RunToken r32-player0-reviewed `
    -R32CommitReceipt D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DMediumDistanceTurfR32V1\<token>\commit.json `
    -ExpectedR32CommitReceiptSha256 <sha256>

Human acceptance after reviewing all ten PNGs:
  .\Capture-IstanaExploreV5DR32Player0Evidence.ps1 -AcceptVisualReview `
    -ConfirmTenImagesReviewed `
    -RunToken r32-player0-reviewed `
    -PendingCaptureReceipt D:\triad\TRIAD_R32Evidence\r32-player0-reviewed\pending-visual-review.json `
    -ExpectedPendingCaptureReceiptSha256 <sha256>
#>
[CmdletBinding()]
param(
    [switch] $StaticSelfCheck,
    [switch] $Execute,
    [switch] $AcceptVisualReview,
    [switch] $ConfirmTenImagesReviewed,
    [string] $RunToken = 'static-contract',
    [string] $R32CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR32CommitReceiptSha256 = '',
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

$schema = 'triad.istana_explore_v5d.r32_player0_capture.v1'
$r32TransactionSchema =
    'triad.istana_explore_v5d.r32_medium_distance_turf.native_transaction.v1'
$r31TransactionSchema =
    'triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1'
$r31CaptureSchema = 'triad.istana_explore_v5d.r31_player0_capture.v2'
$r30TransactionSchema =
    'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1'
$r30CaptureSchema = 'triad.istana_explore_v5d.r30_player0_capture.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$privateMemoryCeilingBytes = 12884901888L # fixed 12 GiB owned tree
$minimumSystemFreeVirtualBytes = 6442450944L # fixed continuous 6 GiB
$memoryWatchdogPollMilliseconds = 500
$memoryWatchdogPersistentBreachMilliseconds = 2000
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot 'unreal'))
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$nativeEvidenceBase = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD_R32Evidence')
$engineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$dotnet = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'))
$unrealBuildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'))
$unrealEditorCmd = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'))
$gameExe = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot 'Binaries\Win64\TRIAD.exe'))
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeEditorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$nativePluginSourceRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Source'))
$r32MaterialContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32'))
$groundHeader = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DGroundVegetationActor.h'))
$groundSource = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DGroundVegetationActor.cpp'))
$r32TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DMediumDistanceTurfR32V1'))
$r31TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1'))
$r31EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R31Evidence')
$r30TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$r30EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence')
$airSimRuntimeRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\AirSimTriadRuntime'))
$airSimRuntimeContentRoot = [IO.Path]::GetFullPath(
    (Join-Path $airSimRuntimeRoot 'Content'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$runtimeCaptureLibrary =
    '/Script/TRIADSensorFusion.Default__TRIADIstanaExploreV5DR32Player0CaptureLibrary'
$script:activeOwnedProcess = $null

$captureSourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR32Player0CaptureLibrary.h'
        Bytes = 1939L
        Sha256 = '2864A8C922EA1ACBD0E16AB69ECA7EB9DF24D4795DC0F4752DA17D759393616D'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR32Player0CaptureLibrary.cpp'
        Bytes = 34338L
        Sha256 = 'D85453E2DFE51BDEC7F9B03BD0DC5A7599A77749EBA454339A0360BFCAA4FDA3'
    }
)

$poses = @(
    [pscustomobject] [ordered] @{ Id='075m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-1.252968; Yaw=-90.0; Roll=0.0; TurfProbe=$false; DistanceMeters=0.0 }
    [pscustomobject] [ordered] @{ Id='020m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-4.703535; Yaw=-90.0; Roll=0.0; TurfProbe=$false; DistanceMeters=0.0 }
    [pscustomobject] [ordered] @{ Id='008m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-11.829499; Yaw=-90.0; Roll=0.0; TurfProbe=$false; DistanceMeters=0.0 }
    [pscustomobject] [ordered] @{ Id='002m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-55.084794; Yaw=-90.0; Roll=0.0; TurfProbe=$false; DistanceMeters=0.0 }
    [pscustomobject] [ordered] @{
        Id='surroundings_oblique_macdonald'
        X=40226.9640238642; Y=106152.591966384; Z=3900.0
        Pitch=-5.74137954693854; Yaw=-101.620267003074; Roll=0.0
        TurfProbe=$false; DistanceMeters=0.0
    }
    [pscustomobject] [ordered] @{ Id='012m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-7.782210724; Yaw=-90.0; Roll=0.0; TurfProbe=$true; DistanceMeters=12.0 }
    [pscustomobject] [ordered] @{ Id='050m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-1.878628060; Yaw=-90.0; Roll=0.0; TurfProbe=$true; DistanceMeters=50.0 }
    [pscustomobject] [ordered] @{ Id='065m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-1.445309952; Yaw=-90.0; Roll=0.0; TurfProbe=$true; DistanceMeters=65.0 }
    [pscustomobject] [ordered] @{ Id='090m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-1.043940890; Yaw=-90.0; Roll=0.0; TurfProbe=$true; DistanceMeters=90.0 }
    [pscustomobject] [ordered] @{ Id='095m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-0.989007849; Yaw=-90.0; Roll=0.0; TurfProbe=$true; DistanceMeters=95.0 }
)

$immutableContentRoots = [ordered] @{
    R32MediumDistanceTurfMaterials = $r32MaterialContentRoot
    R31BroadShell = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31'))
    R30Lookdev = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'))
    R29Vegetation = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
    R29Terrain = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'))
    TreeRealism = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
        'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
    AirSimTriadRuntime = $airSimRuntimeContentRoot
}

$forbiddenAirSimLaunchTokens = @(
    '-DisablePlugin=AirSim', '-DisablePlugins=AirSim',
    '-DisablePlugin=AirSimTriadRuntime',
    '-DisablePlugins=AirSimTriadRuntime', '-NoAirSim')

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
        Present=$true
        Bytes=[int64] $item.Length
        Sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        LastWriteUtc=$item.LastWriteTimeUtc.ToString('o')
    }
}

function Get-RequiredPropertyValue {
    param($Object, [string] $Name, [string] $Label)
    if ($null -eq $Object) { throw "$Label is null." }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { throw "$Label lacks required property '$Name'." }
    $property.Value
}

function Test-StateMatches {
    param($State, $Expected)
    $null -ne $State -and $null -ne $Expected -and
        [bool] (Get-RequiredPropertyValue $State 'Present' 'state') -eq
            [bool] (Get-RequiredPropertyValue $Expected 'Present' 'expected state') -and
        [int64] (Get-RequiredPropertyValue $State 'Bytes' 'state') -eq
            [int64] (Get-RequiredPropertyValue $Expected 'Bytes' 'expected state') -and
        [string] (Get-RequiredPropertyValue $State 'Sha256' 'state') -ceq
            [string] (Get-RequiredPropertyValue $Expected 'Sha256' 'expected state')
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState $Path
    if (-not (Test-StateMatches $actual $Expected)) {
        throw "$Label identity mismatch: path=$Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Assert-R31NaniteRasterCaptureAdmission {
    param($Receipt, [string] $CaptureToken)
    $captures = @(Get-RequiredPropertyValue $Receipt 'Captures' 'R31 capture')
    $expectedPoseIds = @(
        '075m','020m','008m','002m','surroundings_oblique_macdonald')
    if ($captures.Count -ne 5) {
        throw 'Accepted R31 capture must retain exactly five baseline poses.'
    }
    $fileHashes = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::Ordinal)
    $pixelHashes = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::Ordinal)
    for ($index = 0; $index -lt $expectedPoseIds.Count; ++$index) {
        $capture = $captures[$index]
        $pose = Get-RequiredPropertyValue $capture 'Pose' "R31 capture row $index"
        $poseId = [string] (Get-RequiredPropertyValue $pose 'Id' "R31 capture pose $index")
        $image = Get-RequiredPropertyValue $capture 'Image' "R31 capture row $index"
        $expectedImagePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path (Join-Path $r31EvidenceBase $CaptureToken) 'captures') `
            "explore_v5d_r31_player0_${poseId}_${CaptureToken}.png"))
        if ($poseId -cne $expectedPoseIds[$index] -or
            [string] (Get-RequiredPropertyValue $capture 'RenderPathId' "R31 capture row $index") -cne 'NANITE_ON' -or
            [int] (Get-RequiredPropertyValue $capture 'NaniteConsoleValue' "R31 capture row $index") -ne 1 -or
            [int] (Get-RequiredPropertyValue $capture 'NaniteProxyRenderMode' "R31 capture row $index") -ne 1 -or
            [bool] (Get-RequiredPropertyValue $capture 'ProviderFallbackVisualQa' "R31 capture row $index") -ne $true -or
            [bool] (Get-RequiredPropertyValue $capture 'ProviderReadyProofClaimed' "R31 capture row $index") -ne $false -or
            -not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $image 'Path' "R31 capture image $index")).Equals($expectedImagePath,[StringComparison]::OrdinalIgnoreCase) -or
            [int] (Get-RequiredPropertyValue $image 'WidthPixels' "R31 capture image $index") -ne 2560 -or
            [int] (Get-RequiredPropertyValue $image 'HeightPixels' "R31 capture image $index") -ne 1440 -or
            [bool] (Get-RequiredPropertyValue $image 'NonBlank' "R31 capture image $index") -ne $true -or
            -not $fileHashes.Add([string] (Get-RequiredPropertyValue $image 'Sha256' "R31 capture image $index")) -or
            -not $pixelHashes.Add([string] (Get-RequiredPropertyValue $image 'DecodedBgraSha256' "R31 capture image $index"))) {
            throw "R31 Nanite baseline capture row $index failed exact predecessor admission."
        }
        [void] (Assert-State $image $expectedImagePath "R31 baseline PNG $index")
    }

    $comparison = Get-RequiredPropertyValue $Receipt `
        'NaniteRasterHighOccupancyComparison' 'R31 capture'
    if ([bool] (Get-RequiredPropertyValue $comparison 'Required' 'R31 Nanite/raster comparison') -ne $true -or
        [string] (Get-RequiredPropertyValue $comparison 'PoseId' 'R31 Nanite/raster comparison') -cne 'surroundings_oblique_macdonald' -or
        [bool] (Get-RequiredPropertyValue $comparison 'SamePose' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameGameProcess' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameCookedClosure' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'MechanicalValidationPassed' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'HumanNaniteRasterComparisonAttested' 'R31 Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison 'NaniteRasterAppearanceParityAccepted' 'R31 Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison 'AutomaticVisualAcceptanceAllowed' 'R31 Nanite/raster comparison') -ne $false) {
        throw 'R31 Nanite/raster comparison is missing or not fail-closed.'
    }
    $nanite = Get-RequiredPropertyValue $comparison 'NaniteOn' 'R31 Nanite/raster comparison'
    if ((ConvertTo-Json (Get-RequiredPropertyValue $nanite 'Capture' 'R31 Nanite comparison side') -Depth 20 -Compress) -cne
            (ConvertTo-Json $captures[4] -Depth 20 -Compress) -or
        [string] (Get-RequiredPropertyValue $nanite 'RenderPathId' 'R31 Nanite comparison side') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $nanite 'NaniteConsoleValue' 'R31 Nanite comparison side') -ne 1 -or
        [int] (Get-RequiredPropertyValue $nanite 'NaniteProxyRenderMode' 'R31 Nanite comparison side') -ne 1) {
        throw 'R31 Nanite comparison side is not the exact fifth baseline capture.'
    }
    $raster = Get-RequiredPropertyValue $comparison 'RasterFallback' 'R31 Nanite/raster comparison'
    $rasterPose = Get-RequiredPropertyValue $raster 'Pose' 'R31 raster comparison side'
    $baselinePose = Get-RequiredPropertyValue $captures[4] 'Pose' 'R31 fifth baseline capture'
    if ((ConvertTo-Json $rasterPose -Depth 8 -Compress) -cne
            (ConvertTo-Json $baselinePose -Depth 8 -Compress) -or
        [string] (Get-RequiredPropertyValue $raster 'RenderPathId' 'R31 raster comparison side') -cne 'RASTER_FALLBACK' -or
        [int] (Get-RequiredPropertyValue $raster 'NaniteConsoleValue' 'R31 raster comparison side') -ne 0 -or
        [int] (Get-RequiredPropertyValue $raster 'NaniteProxyRenderMode' 'R31 raster comparison side') -ne 0) {
        throw 'R31 raster comparison side changed pose or lacks exact zero CVar readback.'
    }
    foreach ($stateName in @('ImmediatePreCaptureState','ImmediatePostCaptureState')) {
        $stateReport = [string] (Get-RequiredPropertyValue $raster $stateName 'R31 raster comparison side')
        foreach ($marker in @(
            'poseId=surroundings_oblique_macdonald', 'exactQaViewPose=true',
            'renderPath=RASTER_FALLBACK', 'r.Nanite=0',
            'r.Nanite.ProxyRenderMode=0', 'naniteMeshEnabled=true',
            'naniteDataValid=true', 'naniteKeepPercentTriangles=1.0',
            'fallbackPercentTriangles=1.0')) {
            if (-not $stateReport.Contains($marker,[StringComparison]::Ordinal)) {
                throw "R31 raster comparison $stateName lacks '$marker'."
            }
        }
    }
    $rasterImage = Get-RequiredPropertyValue $raster 'Image' 'R31 raster comparison side'
    $expectedRasterPath = [IO.Path]::GetFullPath((Join-Path `
        (Join-Path (Join-Path $r31EvidenceBase $CaptureToken) 'captures') `
        "explore_v5d_r31_player0_surroundings_oblique_macdonald_${CaptureToken}_raster_fallback.png"))
    if (-not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $rasterImage 'Path' 'R31 raster comparison image')).Equals($expectedRasterPath,[StringComparison]::OrdinalIgnoreCase) -or
        [int] (Get-RequiredPropertyValue $rasterImage 'WidthPixels' 'R31 raster comparison image') -ne 2560 -or
        [int] (Get-RequiredPropertyValue $rasterImage 'HeightPixels' 'R31 raster comparison image') -ne 1440 -or
        [bool] (Get-RequiredPropertyValue $rasterImage 'NonBlank' 'R31 raster comparison image') -ne $true -or
        [string]::IsNullOrWhiteSpace([string] (Get-RequiredPropertyValue $rasterImage 'DecodedBgraSha256' 'R31 raster comparison image'))) {
        throw 'R31 raster comparison image is not the exact canonical decoded sixth image.'
    }
    [void] (Assert-State $rasterImage $expectedRasterPath 'R31 raster comparison PNG')
    $restore = Get-RequiredPropertyValue $comparison 'NaniteRestore' 'R31 Nanite/raster comparison'
    $restoreState = [string] (Get-RequiredPropertyValue $restore 'State' 'R31 Nanite restore')
    if ([bool] (Get-RequiredPropertyValue $restore 'Restored' 'R31 Nanite restore') -ne $true -or
        [string] (Get-RequiredPropertyValue $restore 'RenderPathId' 'R31 Nanite restore') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteConsoleValue' 'R31 Nanite restore') -ne 1 -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteProxyRenderMode' 'R31 Nanite restore') -ne 1 -or
        -not $restoreState.Contains('renderPath=NANITE_ON',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite=1',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite.ProxyRenderMode=1',[StringComparison]::Ordinal)) {
        throw 'R31 capture does not prove exact Nanite CVar restoration.'
    }
    $comparison
}

function Get-TreeReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse -Force |
            ForEach-Object {
                $relative = [IO.Path]::GetRelativePath($Root, $_.FullName)
                $state = Get-FileState $_.FullName
                [pscustomobject] [ordered] @{
                    RelativePath=$relative.Replace('/', '\')
                    Bytes=$state.Bytes
                    Sha256=$state.Sha256
                    LastWriteUtc=$state.LastWriteUtc
                }
            } | Sort-Object RelativePath
    )
}

function ConvertTo-TreeIdentity {
    param([object[]] $Rows)
    @(
        $Rows | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath=[string] $_.RelativePath
                Bytes=[int64] $_.Bytes
                Sha256=[string] $_.Sha256
            }
        } | Sort-Object RelativePath
    )
}

function Assert-TreeReceipt {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual = @(Get-TreeReceipt $Root)
    if ((ConvertTo-Json $actual -Depth 6 -Compress) -cne
        (ConvertTo-Json @($Expected) -Depth 6 -Compress)) {
        throw "$Label tree identity mismatch: root=$Root"
    }
    @($actual)
}

function Get-PathBoundReceipt {
    param([string] $Path, $Expected, [string] $Label)
    $state = Assert-State $Expected $Path $Label
    [pscustomobject] [ordered] @{
        Path=$Path
        Present=$state.Present
        Bytes=$state.Bytes
        Sha256=$state.Sha256
        LastWriteUtc=$state.LastWriteUtc
    }
}

function Write-JsonAtomic {
    param($Value, [string] $Path, [string] $BoundedRoot)
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-ContainedPath $fullPath $BoundedRoot)) {
        throw "Receipt path escaped bounded evidence root: $fullPath"
    }
    $directory = [IO.Path]::GetDirectoryName($fullPath)
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    if ([IO.File]::Exists($fullPath)) { throw "Receipt already exists: $fullPath" }
    $temporary = "$fullPath.tmp-$([Guid]::NewGuid().ToString('N'))"
    try {
        [IO.File]::WriteAllText(
            $temporary,
            ($Value | ConvertTo-Json -Depth 32),
            [Text.UTF8Encoding]::new($false))
        [IO.File]::Move($temporary, $fullPath)
    }
    finally {
        if ([IO.File]::Exists($temporary)) {
            [IO.File]::Delete($temporary)
        }
    }
    Get-FileState $fullPath
}

function Read-HashPinnedDirectReceipt {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $Base,
        [string] $Label)
    if ([string]::IsNullOrWhiteSpace($Path) -or
        $ExpectedSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        throw "$Label requires an explicit path and SHA-256."
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $directory = [IO.Path]::GetDirectoryName($fullPath)
    $token = [IO.Path]::GetFileName($directory)
    if (-not [IO.Path]::GetFileName($fullPath).Equals(
            'commit.json', [StringComparison]::Ordinal) -or
        -not [IO.Path]::GetDirectoryName($directory).Equals(
            $Base, [StringComparison]::OrdinalIgnoreCase) -or
        $token -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "$Label must be a direct safe-token child commit.json below $Base."
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or
        $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "$Label hash mismatch."
    }
    [pscustomobject] [ordered] @{
        Path=$fullPath
        Token=$token
        State=$state
        Receipt=(Get-Content -LiteralPath $fullPath -Raw |
            ConvertFrom-Json -Depth 64)
    }
}

function Assert-EmbeddedReceiptAdmission {
    param($Embedded, [string] $Base, [string] $Label)
    $path = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $Embedded 'Path' $Label))
    $state = Get-RequiredPropertyValue $Embedded 'File' $Label
    $directory = [IO.Path]::GetDirectoryName($path)
    $token = [IO.Path]::GetFileName($directory)
    if (-not [IO.Path]::GetFileName($path).Equals(
            'commit.json', [StringComparison]::Ordinal) -or
        -not [IO.Path]::GetDirectoryName($directory).Equals(
            $Base, [StringComparison]::OrdinalIgnoreCase) -or
        $token -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "$Label embedded receipt path is not canonical."
    }
    [void] (Assert-State $state $path $Label)
    [pscustomobject] [ordered] @{
        Path=$path
        Token=$token
        State=$state
        Receipt=(Get-Content -LiteralPath $path -Raw |
            ConvertFrom-Json -Depth 64)
    }
}

function Assert-R32CommitReceipt {
    param([string] $Path, [string] $ExpectedSha256)
    $admission = Read-HashPinnedDirectReceipt `
        $Path $ExpectedSha256 $r32TransactionBase 'R32 transaction receipt'
    $receipt = $admission.Receipt
    if ([string] (Get-RequiredPropertyValue $receipt 'Schema' 'R32 receipt') -cne
            $r32TransactionSchema -or
        [string] (Get-RequiredPropertyValue $receipt 'Status' 'R32 receipt') -cne
            'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $receipt 'RunToken' 'R32 receipt') -cne
            $admission.Token -or
        [int] (Get-RequiredPropertyValue $receipt 'R32ContentPackageCount' 'R32 receipt') -ne 4 -or
        [int] (Get-RequiredPropertyValue $receipt 'SelectedTransformCount' 'R32 receipt') -ne 4608 -or
        [int] (Get-RequiredPropertyValue $receipt 'OwnedHismCount' 'R32 receipt') -ne 12 -or
        [bool] (Get-RequiredPropertyValue $receipt 'R33SourceOrDeclarationAllowed' 'R32 receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $receipt 'CommitR32EndpointInvocationCount' 'R32 receipt') -ne 1 -or
        [int] (Get-RequiredPropertyValue $receipt 'StandaloneApplyR32EndpointInvocationCount' 'R32 receipt') -ne 0 -or
        [bool] (Get-RequiredPropertyValue $receipt 'SourceGroundVegetationActorModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SourceGroundVegetationComponentsModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SourceR29AssetPackagesModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TerrainModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'BuildingOrSurroundingsModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreePresentationModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3PromotedAtR30' 'R32 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Preserved' 'R32 receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeResponseMaterialPackageCount' 'R32 receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeDerivativeMeshPackageCount' 'R32 receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeRuntimeResponseMidCount' 'R32 receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreePlacementGeometryOpacityWindAuthorityModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CollisionModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NavigationModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SensorPlacementModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'DetectionSimulationModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'RfTruthModified' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'GeospatialAuthority' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HyperrealismClaimed' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualCaptureAccepted' 'R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CaptureRevalidationRequired' 'R32 receipt') -ne $true) {
        throw 'R32 transaction receipt failed the exact committed source-only contract.'
    }
    $r32Materials=@(Get-RequiredPropertyValue $receipt 'R32MaterialPackages' 'R32 receipt')
    if ($r32Materials.Count -ne 4) {
        throw 'R32 transaction receipt lacks the exact four-material content receipt.'
    }
    Assert-TreeReceipt $r32MaterialContentRoot $r32Materials 'R32 material packages bound by transaction'

    $r30Transaction = Assert-EmbeddedReceiptAdmission `
        (Get-RequiredPropertyValue $receipt 'R30TransactionAdmission' 'R32 receipt') `
        $r30TransactionBase 'embedded R30 transaction receipt'
    $r30Capture = Assert-EmbeddedReceiptAdmission `
        (Get-RequiredPropertyValue $receipt 'R30CaptureAdmission' 'R32 receipt') `
        $r30EvidenceBase 'embedded R30 capture receipt'
    $r31Transaction = Assert-EmbeddedReceiptAdmission `
        (Get-RequiredPropertyValue $receipt 'R31TransactionAdmission' 'R32 receipt') `
        $r31TransactionBase 'embedded R31 transaction receipt'
    $r31Capture = Assert-EmbeddedReceiptAdmission `
        (Get-RequiredPropertyValue $receipt 'R31CaptureAdmission' 'R32 receipt') `
        $r31EvidenceBase 'embedded R31 capture receipt'

    if ([string] (Get-RequiredPropertyValue $r30Transaction.Receipt 'Schema' 'R30 transaction') -cne $r30TransactionSchema -or
        [string] (Get-RequiredPropertyValue $r30Transaction.Receipt 'Status' 'R30 transaction') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r30Capture.Receipt 'Schema' 'R30 capture') -cne $r30CaptureSchema -or
        [string] (Get-RequiredPropertyValue $r30Capture.Receipt 'Status' 'R30 capture') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r30Capture.Receipt 'RunToken' 'R30 capture') -cne $r30Capture.Token -or
        [string] (Get-RequiredPropertyValue $r30Capture.Receipt 'NativeOrder' 'R30 capture') -cne 'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'R31DependencyAllowed' 'R30 capture') -ne $false -or
        [int] (Get-RequiredPropertyValue $r30Capture.Receipt 'ExactPoseCount' 'R30 capture') -ne 5 -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'MechanicalCaptureValidationPassed' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'ExplicitHumanReviewAcceptance' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'ConfirmedFiveImagesReviewed' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'HumanVisualReviewAttested' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'AutomaticVisualAcceptanceAllowed' 'R30 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'VisualReviewRequired' 'R30 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'VisualReviewAccepted' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'ProviderFallbackVisualQaAccepted' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'TreeMaterialResponseV3Reviewed' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'R31AdmissionAuthorized' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'ProviderReadyProofClaimed' 'R30 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'HyperrealismClaimed' 'R30 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'MapModifiedByCapture' 'R30 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'SimulationCollisionNavigationSensorRfModified' 'R30 capture') -ne $false -or
        [string] (Get-RequiredPropertyValue $r31Transaction.Receipt 'Schema' 'R31 transaction') -cne $r31TransactionSchema -or
        [string] (Get-RequiredPropertyValue $r31Transaction.Receipt 'Status' 'R31 transaction') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r31Capture.Receipt 'Schema' 'R31 capture') -cne $r31CaptureSchema -or
        [string] (Get-RequiredPropertyValue $r31Capture.Receipt 'Status' 'R31 capture') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r31Capture.Receipt 'RunToken' 'R31 capture') -cne $r31Capture.Token -or
        [string] (Get-RequiredPropertyValue $r31Capture.Receipt 'NativeOrder' 'R31 capture') -cne 'R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32' -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'R32DependencyAllowed' 'R31 capture') -ne $false -or
        [int] (Get-RequiredPropertyValue $r31Capture.Receipt 'ExactPoseCount' 'R31 capture') -ne 5 -or
        [int] (Get-RequiredPropertyValue $r31Capture.Receipt 'ExactImageCount' 'R31 capture') -ne 6 -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'MechanicalCaptureValidationPassed' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'ExplicitHumanReviewAcceptance' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'ConfirmedFiveImagesReviewed' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'ConfirmedNaniteRasterPairReviewed' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'HumanVisualReviewAttested' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'HumanNaniteRasterComparisonAttested' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'NaniteRasterAppearanceParityAccepted' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'NaniteConsoleStateRestored' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'AutomaticVisualAcceptanceAllowed' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'VisualReviewRequired' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'VisualReviewAccepted' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'R31BroadShellVisualQaAccepted' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'ProviderFallbackVisualQaAccepted' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'TreeMaterialResponseV3Preserved' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'R32AdmissionAuthorized' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'ProviderReadyProofClaimed' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'ProviderReadyCaptureAccepted' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'HyperrealismClaimed' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'NativeStateMutatedByAcceptance' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'UnrealLaunchedByAcceptance' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'MapModifiedByCapture' 'R31 capture') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'SimulationCollisionNavigationSensorRfModified' 'R31 capture') -ne $false) {
        throw 'R32 receipt does not bind the full committed and accepted R30/R31 chain.'
    }

    $treeMaterialResponseV3 = Get-RequiredPropertyValue `
        $receipt 'TreeMaterialResponseV3' 'R32 receipt'
    $treeNativePins = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'NativeSourcePins' 'R32 TreeRealism v3 receipt')
    $treeClosurePins = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'SourceClosurePins' 'R32 TreeRealism v3 receipt')
    $treeBefore = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentBefore' 'R32 TreeRealism v3 receipt')
    $treeAfter = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentAfter' 'R32 TreeRealism v3 receipt')
    if ([string] (Get-RequiredPropertyValue $treeMaterialResponseV3 'Status' 'R32 TreeRealism v3 receipt') -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        $treeNativePins.Count -ne 4 -or $treeClosurePins.Count -ne 6 -or
        $treeAfter.Count -ne $treeBefore.Count + 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ResponseMaterialPackageCount' 'R32 TreeRealism v3 receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ReboundManagedMeshPackageCount' 'R32 TreeRealism v3 receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'RuntimeResponseMidCount' 'R32 TreeRealism v3 receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $treeMaterialResponseV3 'TreePlacementGeometryOpacityWindAuthorityModified' 'R32 TreeRealism v3 receipt') -ne $false) {
        throw 'R32 TreeRealism v3 receipt failed its exact promoted-state contract.'
    }
    foreach ($upstreamTree in @(
        (Get-RequiredPropertyValue $r30Transaction.Receipt 'TreeMaterialResponseV3' 'R30 transaction'),
        (Get-RequiredPropertyValue $r31Transaction.Receipt 'TreeMaterialResponseV3' 'R31 transaction'))) {
        if ((ConvertTo-Json $treeMaterialResponseV3 -Depth 16 -Compress) -cne
            (ConvertTo-Json $upstreamTree -Depth 16 -Compress)) {
            throw 'R30/R31/R32 TreeRealism v3 identity chain drifted.'
        }
    }
    foreach ($pin in @($treeNativePins) + @($treeClosurePins)) {
        $relative=[string] (Get-RequiredPropertyValue $pin 'RelativePath' 'R32 TreeRealism v3 pin')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('..')) {
            throw "R32 TreeRealism v3 contains a noncanonical pin: $relative"
        }
        $path=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "R32 TreeRealism v3 pin escaped native root: $path"
        }
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] (Get-RequiredPropertyValue $pin 'Bytes' 'R32 TreeRealism v3 pin')
            Sha256=[string] (Get-RequiredPropertyValue $pin 'Sha256' 'R32 TreeRealism v3 pin')
        }) $path 'R32 retained TreeRealism v3 source pin')
    }
    $currentTreeIdentity = @(ConvertTo-TreeIdentity `
        (Get-TreeReceipt $immutableContentRoots.TreeRealism))
    if ((ConvertTo-Json @($treeAfter) -Depth 8 -Compress) -cne
        (ConvertTo-Json $currentTreeIdentity -Depth 8 -Compress)) {
        throw 'Current TreeRealism content no longer matches the R30 v3 receipt admitted by R32.'
    }

    $r30Acceptance = Get-RequiredPropertyValue `
        $r30Capture.Receipt 'AcceptanceRevalidation' 'R30 capture'
    if ([bool] (Get-RequiredPropertyValue $r30Acceptance 'Unchanged' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Acceptance 'PendingReceiptHashPinned' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Acceptance 'FivePngsRedecodedAndRehashed' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Acceptance 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Acceptance 'TreeResponseMaterials13AndRuntimeMids26Revalidated' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Acceptance 'NativeReceiptWriteAllowed' 'R30 acceptance') -ne $false) {
        throw 'Accepted R30 capture lacks immutable two-phase review evidence.'
    }

    $r31Acceptance = Get-RequiredPropertyValue `
        $r31Capture.Receipt 'AcceptanceRevalidation' 'R31 capture'
    if ([bool] (Get-RequiredPropertyValue $r31Acceptance 'Unchanged' 'R31 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Acceptance 'PendingReceiptHashPinned' 'R31 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Acceptance 'SixPngsRedecodedAndRehashed' 'R31 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Acceptance 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' 'R31 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Acceptance 'NativeReceiptWriteAllowed' 'R31 acceptance') -ne $false) {
        throw 'Accepted R31 capture lacks immutable two-phase review evidence.'
    }
    [void] (Assert-R31NaniteRasterCaptureAdmission `
        $r31Capture.Receipt $r31Capture.Token)

    foreach ($embedded in @(
        [pscustomobject] @{
            Binding=Get-RequiredPropertyValue $r30Capture.Receipt `
                'R30CommitAdmission' 'R30 capture'
            Admission=$r30Transaction
            Label='R30 capture transaction admission'
        }
        [pscustomobject] @{
            Binding=Get-RequiredPropertyValue $r31Transaction.Receipt `
                'R30TransactionAdmission' 'R31 transaction'
            Admission=$r30Transaction
            Label='R31 embedded R30 transaction admission'
        }
        [pscustomobject] @{
            Binding=Get-RequiredPropertyValue $r31Transaction.Receipt `
                'R30CaptureAdmission' 'R31 transaction'
            Admission=$r30Capture
            Label='R31 embedded R30 capture admission'
        }
        [pscustomobject] @{
            Binding=Get-RequiredPropertyValue $r31Capture.Receipt `
                'R31CommitAdmission' 'R31 capture'
            Admission=$r31Transaction
            Label='R31 capture transaction admission'
        })) {
        $bindingPath = [IO.Path]::GetFullPath([string] (
            Get-RequiredPropertyValue $embedded.Binding 'Path' $embedded.Label))
        $bindingFile = Get-RequiredPropertyValue `
            $embedded.Binding 'File' $embedded.Label
        if (-not $bindingPath.Equals(
                $embedded.Admission.Path,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-StateMatches $bindingFile $embedded.Admission.State)) {
            throw "$($embedded.Label) drifted from the hash-bound predecessor."
        }
    }

    $r30Pending = Get-RequiredPropertyValue `
        $r30Capture.Receipt 'PendingCaptureAdmission' 'R30 capture'
    $r30PendingPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $r30Pending 'Path' 'R30 pending admission'))
    $r30PendingState = Get-RequiredPropertyValue `
        $r30Pending 'File' 'R30 pending admission'
    $expectedR30PendingPath = [IO.Path]::GetFullPath((Join-Path `
        (Join-Path $r30EvidenceBase $r30Capture.Token) `
        'pending-visual-review.json'))
    if (-not $r30PendingPath.Equals(
            $expectedR30PendingPath, [StringComparison]::OrdinalIgnoreCase) -or
        [string] (Get-RequiredPropertyValue $r30Pending 'Status' 'R30 pending admission') -cne 'PENDING_VISUAL_REVIEW' -or
        [string] (Get-RequiredPropertyValue $r30Pending 'CallerSha256' 'R30 pending admission') -cne [string] $r30PendingState.Sha256) {
        throw 'Accepted R30 capture is not bound to its exact pending receipt.'
    }
    [void] (Assert-State `
        $r30PendingState $r30PendingPath 'R30 pending visual-review receipt')

    $r31Pending = Get-RequiredPropertyValue `
        $r31Capture.Receipt 'PendingCaptureAdmission' 'R31 capture'
    $r31PendingPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $r31Pending 'Path' 'R31 pending admission'))
    $r31PendingState = Get-RequiredPropertyValue `
        $r31Pending 'File' 'R31 pending admission'
    $expectedR31PendingPath = [IO.Path]::GetFullPath((Join-Path `
        (Join-Path $r31EvidenceBase $r31Capture.Token) `
        'pending-visual-review.json'))
    if (-not $r31PendingPath.Equals(
            $expectedR31PendingPath, [StringComparison]::OrdinalIgnoreCase) -or
        [string] (Get-RequiredPropertyValue $r31Pending 'Status' 'R31 pending admission') -cne 'PENDING_VISUAL_REVIEW' -or
        [string] (Get-RequiredPropertyValue $r31Pending 'CallerSha256' 'R31 pending admission') -cne [string] $r31PendingState.Sha256) {
        throw 'Accepted R31 capture is not bound to its exact pending receipt.'
    }
    [void] (Assert-State `
        $r31PendingState $r31PendingPath 'R31 pending visual-review receipt')

    $map = Get-RequiredPropertyValue $receipt 'SuccessorMap' 'R32 receipt'
    $runtimeDll = Get-RequiredPropertyValue $receipt 'RuntimeDllAfter' 'R32 receipt'
    $editorDllState = Get-RequiredPropertyValue $receipt 'EditorDllAfter' 'R32 receipt'
    $groundHeaderState = Get-RequiredPropertyValue $receipt 'GroundHeaderAfter' 'R32 receipt'
    $groundSourceState = Get-RequiredPropertyValue $receipt 'GroundSourceAfter' 'R32 receipt'
    $sourceTree = @(Get-RequiredPropertyValue `
        $receipt 'NativePluginSourceTreeAfter' 'R32 receipt')
    if ($sourceTree.Count -le 0 -or
        @($sourceTree | Where-Object {
            [string] $_.RelativePath -match 'R33'
        }).Count -ne 0) {
        throw 'R32 committed source tree is empty or successor-contaminated.'
    }
    [void] (Assert-State $map $mapFile 'R32 committed map')
    [void] (Assert-State $runtimeDll $runtimeEditorDll 'R32 runtime editor DLL')
    [void] (Assert-State $editorDllState $editorDll 'R32 editor DLL')
    [void] (Assert-State $groundHeaderState $groundHeader 'R32 Ground header')
    [void] (Assert-State $groundSourceState $groundSource 'R32 Ground source')

    $r31Map = Get-RequiredPropertyValue `
        $r31Transaction.Receipt 'SuccessorMap' 'R31 transaction'
    $r31Runtime = Get-RequiredPropertyValue `
        $r31Transaction.Receipt 'RuntimeDllAfter' 'R31 transaction'
    $r31Editor = Get-RequiredPropertyValue `
        $r31Transaction.Receipt 'EditorDllAfter' 'R31 transaction'
    if (-not (Test-StateMatches `
            (Get-RequiredPropertyValue $receipt 'PredecessorMap' 'R32 receipt') `
            $r31Map) -or
        -not (Test-StateMatches `
            (Get-RequiredPropertyValue $receipt 'RuntimeDllBefore' 'R32 receipt') `
            $r31Runtime) -or
        -not (Test-StateMatches `
            (Get-RequiredPropertyValue $receipt 'EditorDllBefore' 'R32 receipt') `
            $r31Editor)) {
        throw 'R32 receipt is not cross-bound to the exact R31 map/DLL predecessor.'
    }
    foreach ($crossBinding in @(
        [pscustomobject] @{
            Actual=Get-RequiredPropertyValue $r31Capture.Receipt 'Map' 'R31 capture'
            Expected=$r31Map; Label='R31 capture map'
        }
        [pscustomobject] @{
            Actual=Get-RequiredPropertyValue $r31Capture.Receipt 'RuntimeEditorDll' 'R31 capture'
            Expected=$r31Runtime; Label='R31 capture runtime DLL'
        }
        [pscustomobject] @{
            Actual=Get-RequiredPropertyValue $r31Capture.Receipt 'EditorDll' 'R31 capture'
            Expected=$r31Editor; Label='R31 capture editor DLL'
        }
        [pscustomobject] @{
            Actual=Get-RequiredPropertyValue $r31Capture.Receipt 'GroundHeader' 'R31 capture'
            Expected=Get-RequiredPropertyValue $receipt 'GroundHeaderBefore' 'R32 receipt'
            Label='R31 capture Ground header'
        }
        [pscustomobject] @{
            Actual=Get-RequiredPropertyValue $r31Capture.Receipt 'GroundSource' 'R31 capture'
            Expected=Get-RequiredPropertyValue $receipt 'GroundSourceBefore' 'R32 receipt'
            Label='R31 capture Ground source'
        })) {
        if (-not (Test-StateMatches `
                $crossBinding.Actual $crossBinding.Expected)) {
            throw "$($crossBinding.Label) does not cross-bind into R32."
        }
    }
    $r31TreeBinding = Get-RequiredPropertyValue `
        $r31Capture.Receipt 'NativePluginSourceTree' 'R31 capture'
    $r31Tree = @(Get-RequiredPropertyValue `
        $r31TreeBinding 'BaselineAfterCaptureSourcePromotion' 'R31 tree binding')
    $r32BeforeTree = @(Get-RequiredPropertyValue `
        $receipt 'NativePluginSourceTreeBefore' 'R32 receipt')
    if ([bool] (Get-RequiredPropertyValue $r31TreeBinding 'Unchanged' 'R31 tree binding') -ne $true -or
        (ConvertTo-Json $r31Tree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($r31TreeBinding.ImmediatePreCapture) -Depth 6 -Compress) -or
        (ConvertTo-Json $r31Tree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($r31TreeBinding.ImmediatePostCapture) -Depth 6 -Compress) -or
        (ConvertTo-Json $r31Tree -Depth 6 -Compress) -cne
            (ConvertTo-Json $r32BeforeTree -Depth 6 -Compress)) {
        throw 'R31 accepted plugin Source tree does not cross-bind into R32.'
    }

    [pscustomobject] [ordered] @{
        R32=$admission
        R30Transaction=$r30Transaction
        R30Capture=$r30Capture
        R31Transaction=$r31Transaction
        R31Capture=$r31Capture
        Map=$map
        RuntimeDll=$runtimeDll
        EditorDll=$editorDllState
        GroundHeader=$groundHeaderState
        GroundSource=$groundSourceState
        NativePluginSourceTree=@($sourceTree)
        TreeMaterialResponseV3=$treeMaterialResponseV3
    }
}

function Assert-CaptureSourcePins {
    foreach ($pin in $captureSourcePins) {
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "R32 capture source pin is not canonical: $($pin.RelativePath)"
        }
        $path = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $path $repositoryUnrealRoot)) {
            throw "R32 capture source escaped repository Unreal root: $path"
        }
        [void] (Assert-State ([pscustomobject] [ordered] @{
            Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256
        }) $path 'repository R32 capture source')
    }
}

function Get-CaptureTreeRelativePath {
    param($Pin)
    $prefix = 'Plugins\TRIADSensorFusion\Source\'
    if (-not $Pin.RelativePath.StartsWith(
            $prefix, [StringComparison]::Ordinal)) {
        throw "Capture source is outside the plugin Source tree: $($Pin.RelativePath)"
    }
    $Pin.RelativePath.Substring($prefix.Length)
}

function Get-ExpectedCaptureSourceTree {
    param([object[]] $R32Tree)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($row in @($R32Tree)) {
        $relative = [string] (
            Get-RequiredPropertyValue $row 'RelativePath' 'R32 source row')
        if ($relative -match 'R33') {
            throw "Successor source is forbidden in R32 capture baseline: $relative"
        }
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath=$relative.Replace('/', '\')
            Bytes=[int64] (Get-RequiredPropertyValue $row 'Bytes' 'R32 source row')
            Sha256=[string] (Get-RequiredPropertyValue $row 'Sha256' 'R32 source row')
        })
    }
    foreach ($pin in $captureSourcePins) {
        $relative = Get-CaptureTreeRelativePath $pin
        if (@($rows | Where-Object {
                $_.RelativePath -ceq $relative
            }).Count -ne 0) {
            throw "R32 transaction source tree unexpectedly owns capture source: $relative"
        }
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath=$relative
            Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        })
    }
    @($rows | Sort-Object RelativePath)
}

function Install-CaptureSourceClosure {
    param([object[]] $R32Tree)
    Assert-CaptureSourcePins
    $expectedIdentity = @(Get-ExpectedCaptureSourceTree $R32Tree)
    $current = @(Get-TreeReceipt $nativePluginSourceRoot)
    $baseJson = ConvertTo-Json `
        @(ConvertTo-TreeIdentity $R32Tree) -Depth 6 -Compress
    $expectedJson = ConvertTo-Json $expectedIdentity -Depth 6 -Compress
    $currentJson = ConvertTo-Json `
        @(ConvertTo-TreeIdentity $current) -Depth 6 -Compress
    if ($currentJson -cne $baseJson -and $currentJson -cne $expectedJson) {
        throw 'Native plugin Source tree is neither the exact R32 commit tree nor its exact idempotent capture-source successor.'
    }

    $journal = [Collections.Generic.List[object]]::new()
    foreach ($pin in $captureSourcePins) {
        $source = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RelativePath))
        $destination = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $destination $nativePluginSourceRoot)) {
            throw "Native R32 capture source escaped plugin Source: $destination"
        }
        $before = Get-FileState $destination
        if ($before.Present) {
            [void] (Assert-State ([pscustomobject] [ordered] @{
                Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256
            }) $destination 'idempotent native R32 capture source')
        }
        else {
            [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($destination)) | Out-Null
            [IO.File]::Copy($source, $destination, $false)
        }
        [void] (Assert-State ([pscustomobject] [ordered] @{
            Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256
        }) $destination 'promoted native R32 capture source')
        $journal.Add([pscustomobject] [ordered] @{
            Path=$destination
            Before=$before
            Created=(-not $before.Present)
        })
    }
    $promotedTree = @(Get-TreeReceipt $nativePluginSourceRoot)
    if ((ConvertTo-Json @(ConvertTo-TreeIdentity $promotedTree) -Depth 6 -Compress) -cne
        $expectedJson) {
        throw 'Post-promotion R32 capture Source-tree identity drifted.'
    }
    [pscustomobject] [ordered] @{
        ExpectedTree=@($promotedTree)
        Journal=@($journal)
    }
}

function Restore-CaptureSourceClosure {
    param([object[]] $Journal)
    foreach ($entry in @($Journal)) {
        if ([bool] $entry.Created) {
            $current = Get-FileState $entry.Path
            $pin = $captureSourcePins | Where-Object {
                [IO.Path]::GetFullPath(
                    (Join-Path $nativeProjectRoot $_.RelativePath)).Equals(
                        $entry.Path, [StringComparison]::OrdinalIgnoreCase)
            } | Select-Object -First 1
            if ($null -eq $pin -or -not $current.Present -or
                $current.Bytes -ne $pin.Bytes -or
                $current.Sha256 -cne $pin.Sha256) {
                throw "Refused unsafe capture-source rollback: $($entry.Path)"
            }
            [IO.File]::Delete($entry.Path)
        }
        [void] (Assert-State $entry.Before $entry.Path `
            'rolled-back R32 capture source')
    }
}

function Get-ImmutableContentSnapshot {
    $snapshot = [ordered] @{}
    foreach ($entry in $immutableContentRoots.GetEnumerator()) {
        $snapshot[$entry.Key] = @(Get-TreeReceipt $entry.Value)
    }
    [pscustomobject] $snapshot
}

function Assert-ImmutableContentSnapshot {
    param($Snapshot)
    foreach ($entry in $immutableContentRoots.GetEnumerator()) {
        [void] (Assert-TreeReceipt $entry.Value `
            @($Snapshot.($entry.Key)) "immutable $($entry.Key) content")
    }
}

function Assert-CaptureBindings {
    param($Admission, [object[]] $ExpectedCaptureTree)
    $map = Get-PathBoundReceipt `
        $mapFile $Admission.Map 'R32 capture-bound map'
    $runtime = Get-PathBoundReceipt `
        $runtimeEditorDll $Admission.RuntimeDll 'R32 capture-bound runtime DLL'
    $editor = Get-PathBoundReceipt `
        $editorDll $Admission.EditorDll 'R32 capture-bound editor DLL'
    $groundH = Get-PathBoundReceipt `
        $groundHeader $Admission.GroundHeader 'R32 capture-bound Ground header'
    $groundC = Get-PathBoundReceipt `
        $groundSource $Admission.GroundSource 'R32 capture-bound Ground source'
    $tree = @(Assert-TreeReceipt `
        $nativePluginSourceRoot $ExpectedCaptureTree `
        'R32 capture-bound native plugin Source')
    [pscustomobject] [ordered] @{
        Map=$map
        RuntimeEditorDll=$runtime
        EditorDll=$editor
        GroundHeader=$groundH
        GroundSource=$groundC
        NativePluginSourceTree=@($tree)
    }
}

function Assert-NoSuccessorNativeSource {
    foreach ($relative in @(
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h',
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.cpp')) {
        $path = Join-Path $nativeProjectRoot $relative
        if ([IO.File]::Exists($path)) {
            throw "Successor source is forbidden during R32 capture: $path"
        }
    }
}

function Initialize-MemoryProbeType {
    if ($null -ne ('Triad.R32Capture.MemoryProbe' -as [type])) { return }
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Triad.R32Capture
{
    public static class MemoryProbe
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
        public static ulong AvailableCommitBytes()
        {
            MemoryStatusEx memory = new MemoryStatusEx();
            if (!GlobalMemoryStatusEx(memory))
                throw new System.ComponentModel.Win32Exception(
                    Marshal.GetLastWin32Error());
            return memory.AvailablePageFile;
        }
    }
}
'@ -Language CSharp -ErrorAction Stop
}

function Get-FreeVirtualBytes {
    Initialize-MemoryProbeType
    [uint64] [Triad.R32Capture.MemoryProbe]::AvailableCommitBytes()
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $free = Get-FreeVirtualBytes
    if ($free -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "R32_CAPTURE_LAUNCH_HEADROOM_REFUSED label=$Label freeVirtualBytes=$free required=$minimumSystemFreeVirtualAtLaunchBytes"
    }
    [pscustomobject] [ordered] @{
        Label=$Label
        ObservedUtc=[DateTime]::UtcNow.ToString('o')
        FreeVirtualBytes=$free
        RequiredFreeVirtualBytes=$minimumSystemFreeVirtualAtLaunchBytes
        Admitted=$true
    }
}

function Get-NativeMutatorProcesses {
    @(
        Get-CimInstance Win32_Process | Where-Object {
            $name = [string] $_.Name
            $line = [string] $_.CommandLine
            $projectBound = $line.Contains(
                $nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase) -or
                $line.Contains(
                    $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)
            $projectBound -and $name -in @(
                'UnrealEditor.exe', 'UnrealEditor-Cmd.exe', 'TRIAD.exe',
                'dotnet.exe', 'UnrealBuildTool.exe',
                'UnrealHeaderTool.exe', 'MSBuild.exe', 'cl.exe',
                'link.exe', 'rc.exe', 'ShaderCompileWorker.exe')
        }
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $ownedId = if ($null -ne $script:activeOwnedProcess) {
        [int] $script:activeOwnedProcess.Handle.Id
    } else { -1 }
    $busy = @(Get-NativeMutatorProcesses | Where-Object {
        [int] $_.ProcessId -ne $ownedId
    })
    if ($busy.Count -ne 0) {
        throw "$Label refused because an exact-project native mutator is active: $([string]::Join('; ', @($busy | ForEach-Object { "pid=$($_.ProcessId) name=$($_.Name)" })))"
    }
}

function Join-NativeArgumentLine {
    param([string[]] $Arguments)
    [string]::Join(' ', @($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"' + $_.Replace('"', '\"') + '"'
        } else { $_ }
    }))
}

function Assert-LaunchLineRetainsAirSim {
    param([string[]] $Arguments)
    foreach ($token in $forbiddenAirSimLaunchTokens) {
        if ($token -in $Arguments) {
            throw "R32 capture launch attempted to disable AirSim: $token"
        }
    }
}

function Start-GuardedOwnedProcess {
    param(
        [string] $Label,
        [string] $FilePath,
        [string[]] $Arguments,
        [string] $WorkingDirectory,
        [string] $StandardOutputLog,
        [string] $StandardErrorLog)
    Assert-NativeIdle "before $Label"
    $launch = Assert-LaunchAdmission "before $Label"
    Assert-LaunchLineRetainsAirSim $Arguments
    foreach ($log in @($StandardOutputLog, $StandardErrorLog)) {
        if ([IO.File]::Exists($log)) { throw "$Label log already exists: $log" }
        [IO.Directory]::CreateDirectory(
            [IO.Path]::GetDirectoryName($log)) | Out-Null
    }
    $started = [DateTime]::UtcNow
    $handle = Start-Process -FilePath $FilePath `
        -ArgumentList (Join-NativeArgumentLine $Arguments) `
        -WorkingDirectory $WorkingDirectory -PassThru `
        -WindowStyle Hidden `
        -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog
    $handle.Refresh()
    if ($handle.HasExited -or
        -not [IO.Path]::GetFullPath(
            $handle.MainModule.FileName).Equals(
                [IO.Path]::GetFullPath($FilePath),
                [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label failed exact process identity verification."
    }
    $owned = [pscustomobject] [ordered] @{
        Label=$Label
        Handle=$handle
        FilePath=[IO.Path]::GetFullPath($FilePath)
        Arguments=@($Arguments)
        CommandLine=Join-NativeArgumentLine $Arguments
        StartedUtc=$started.ToString('o')
        StartTimeUtc=$handle.StartTime.ToUniversalTime().ToString('o')
        LaunchAdmission=$launch
        PeakOwnedPrivateBytes=0L
        MinimumFreeVirtualBytes=[uint64]::MaxValue
        SampleCount=0L
        BreachStartedUtc=$null
        MemoryGuardAlert=''
    }
    $script:activeOwnedProcess = $owned
    $owned
}

function Get-OwnedProcessTree {
    param([int] $RootPid)
    $all = @(Get-CimInstance Win32_Process)
    $ids = [Collections.Generic.HashSet[int]]::new()
    [void] $ids.Add($RootPid)
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($row in $all) {
            if ($ids.Contains([int] $row.ParentProcessId) -and
                $ids.Add([int] $row.ProcessId)) {
                $changed = $true
            }
        }
    }
    @($all | Where-Object { $ids.Contains([int] $_.ProcessId) })
}

function Update-OwnedMemoryGuard {
    param($Owned, [string] $Checkpoint)
    $Owned.Handle.Refresh()
    if ($Owned.Handle.HasExited) { return }
    if ($Owned.Handle.StartTime.ToUniversalTime().ToString('o') -cne
        $Owned.StartTimeUtc -or
        -not [IO.Path]::GetFullPath($Owned.Handle.MainModule.FileName).Equals(
            $Owned.FilePath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "R32 capture owned-process identity changed at $Checkpoint."
    }
    $tree = @(Get-OwnedProcessTree $Owned.Handle.Id)
    $privateBytes = 0L
    foreach ($row in $tree) {
        try {
            $process = Get-Process -Id ([int] $row.ProcessId) -ErrorAction Stop
            $privateBytes += [int64] $process.PrivateMemorySize64
        }
        catch { }
    }
    $free = Get-FreeVirtualBytes
    $Owned.SampleCount = [int64] $Owned.SampleCount + 1L
    if ($privateBytes -gt [int64] $Owned.PeakOwnedPrivateBytes) {
        $Owned.PeakOwnedPrivateBytes = $privateBytes
    }
    if ($free -lt [uint64] $Owned.MinimumFreeVirtualBytes) {
        $Owned.MinimumFreeVirtualBytes = $free
    }
    $breached = $privateBytes -ge $privateMemoryCeilingBytes -or
        $free -lt $minimumSystemFreeVirtualBytes
    if ($breached) {
        if ($null -eq $Owned.BreachStartedUtc) {
            $Owned.BreachStartedUtc = [DateTime]::UtcNow
        }
        if (([DateTime]::UtcNow -
                [DateTime] $Owned.BreachStartedUtc).TotalMilliseconds -ge
                    $memoryWatchdogPersistentBreachMilliseconds) {
            $Owned.MemoryGuardAlert = if (
                $privateBytes -ge $privateMemoryCeilingBytes) {
                'MEMORY_GUARD_OWNED_TREE_PRIVATE_BYTES'
            } else { 'MEMORY_GUARD_SYSTEM_FREE_VIRTUAL' }
            $Owned.Handle.Kill($true)
            throw "$($Owned.MemoryGuardAlert) checkpoint=$Checkpoint ownedPrivateBytes=$privateBytes freeVirtualBytes=$free"
        }
    }
    else { $Owned.BreachStartedUtc = $null }
}

function Wait-GuardedOwnedProcess {
    param($Owned, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ($true) {
        $Owned.Handle.Refresh()
        if ($Owned.Handle.HasExited) { break }
        Update-OwnedMemoryGuard $Owned $Owned.Label
        if ([DateTime]::UtcNow -ge $deadline) {
            $Owned.Handle.Kill($true)
            throw "$($Owned.Label) exceeded bounded timeout."
        }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    $Owned.Handle.WaitForExit()
    $exitCode = $Owned.Handle.ExitCode
    $script:activeOwnedProcess = $null
    if ($exitCode -ne 0) {
        throw "$($Owned.Label) failed with exit code $exitCode."
    }
    [pscustomobject] [ordered] @{
        Label=$Owned.Label
        FilePath=$Owned.FilePath
        Arguments=@($Owned.Arguments)
        CommandLine=$Owned.CommandLine
        StartedUtc=$Owned.StartedUtc
        ExitedUtc=[DateTime]::UtcNow.ToString('o')
        ExitCode=$exitCode
        LaunchAdmission=$Owned.LaunchAdmission
        ContinuousMemoryGuard=[pscustomobject] [ordered] @{
            FixedPrivateMemoryCeilingBytes=$privateMemoryCeilingBytes
            FixedMinimumFreeVirtualBytes=$minimumSystemFreeVirtualBytes
            PollMilliseconds=$memoryWatchdogPollMilliseconds
            PersistentBreachMilliseconds=$memoryWatchdogPersistentBreachMilliseconds
            SampleCount=$Owned.SampleCount
            PeakOwnedTreePrivateBytes=$Owned.PeakOwnedPrivateBytes
            MinimumObservedFreeVirtualBytes=$Owned.MinimumFreeVirtualBytes
            Alert=$Owned.MemoryGuardAlert
            Passed=[string]::IsNullOrWhiteSpace($Owned.MemoryGuardAlert)
        }
    }
}

function Close-OwnedProcessOnFailure {
    if ($null -eq $script:activeOwnedProcess) { return }
    $owned = $script:activeOwnedProcess
    $handle = $owned.Handle
    $handle.Refresh()
    if (-not $handle.HasExited) {
        if ($handle.StartTime.ToUniversalTime().ToString('o') -ne
            $owned.StartTimeUtc) {
            throw 'Refused failure containment after owned PID identity changed.'
        }
        $handle.Kill($true)
        if (-not $handle.WaitForExit(30000)) {
            throw 'Exact owned process tree did not quiesce after containment.'
        }
    }
    $script:activeOwnedProcess = $null
}

function Invoke-GuardedCommand {
    param(
        [string] $Label,
        [string] $FilePath,
        [string[]] $Arguments,
        [string] $WorkingDirectory,
        [string] $StandardOutputLog,
        [string] $StandardErrorLog,
        [int] $TimeoutSeconds)
    $owned = Start-GuardedOwnedProcess `
        $Label $FilePath $Arguments $WorkingDirectory `
        $StandardOutputLog $StandardErrorLog
    Wait-GuardedOwnedProcess $owned $TimeoutSeconds
}

function Assert-FreshFileState {
    param([string] $Path, [DateTime] $StartedUtc, [string] $Label)
    $state = Get-FileState $Path
    if (-not $state.Present -or $state.Bytes -le 0 -or
        [DateTime]::Parse($state.LastWriteUtc).ToUniversalTime() -lt
            $StartedUtc.AddSeconds(-2)) {
        throw "$Label was not freshly produced: $Path"
    }
    $state
}

function Test-FileContainsMarker {
    param([string] $Path, [string] $Marker)
    $bytes = [IO.File]::ReadAllBytes($Path)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    if ($ascii.Contains($Marker, [StringComparison]::Ordinal)) {
        return $true
    }
    $unicode = [Text.Encoding]::Unicode.GetString($bytes)
    $unicode.Contains($Marker, [StringComparison]::Ordinal)
}

function Assert-GameBinaryMarkers {
    foreach ($marker in @(
        'UTRIADIstanaExploreV5DR32Player0CaptureLibrary',
        'GetIstanaExploreV5DR32Player0CaptureState',
        'SetIstanaExploreV5DR32Player0CapturePose',
        'CaptureIstanaExploreV5DR32Player0TurfView',
        'ISTANA_EXPLORE_V5D_R32_PLAYER0_TURF_CAPTURE_ACCEPTED',
        'ATRIADIstanaExploreV5DR32MediumDistanceTurfActor',
        'ValidateR32MediumDistanceTurf',
        'AirSimTriadRuntime', 'WeatherActor')) {
        if (-not (Test-FileContainsMarker $gameExe $marker)) {
            throw "Fresh R32 Game binary lacks marker: $marker"
        }
    }
    foreach ($marker in @(
        'TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor',
        'ConfigureR33CesiumWorldTerrainReference')) {
        if (Test-FileContainsMarker $gameExe $marker) {
            throw "Fresh R32 Game binary contains forbidden successor marker: $marker"
        }
    }
}

function Assert-NoFatalRuntimeLog {
    param([string] $Path, [string] $Label)
    if (-not [IO.File]::Exists($Path)) {
        throw "$Label runtime log is absent: $Path"
    }
    $text = [IO.File]::ReadAllText($Path)
    foreach ($marker in @(
        'Fatal error:', 'Unhandled Exception:',
        'Assertion failed:', 'LowLevelFatalError')) {
        if ($text.Contains($marker, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$Label runtime log contains fatal marker: $marker"
        }
    }
}

function Assert-RcPortUnowned {
    $listeners = @(Get-NetTCPConnection -LocalPort 30010 `
        -State Listen -ErrorAction SilentlyContinue)
    if ($listeners.Count -ne 0) {
        throw 'Remote Control port 30010 is already owned before R32 capture launch.'
    }
}

function Assert-RcPortOwnedByGame {
    param($Owned)
    $listeners = @(Get-NetTCPConnection -LocalPort 30010 `
        -State Listen -ErrorAction SilentlyContinue)
    if ($listeners.Count -ne 1 -or
        [int] $listeners[0].OwningProcess -ne [int] $Owned.Handle.Id) {
        throw 'Remote Control port 30010 is not owned by the exact R32 Game process.'
    }
}

function Invoke-RcCall {
    param(
        [string] $FunctionName,
        [hashtable] $Parameters,
        [int] $TimeoutSeconds = 5)
    $body = [ordered] @{
        objectPath=$runtimeCaptureLibrary
        functionName=$FunctionName
        parameters=$Parameters
        generateTransaction=$false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Uri $rcUri -Method Put `
        -ContentType 'application/json' -Body $body `
        -TimeoutSec $TimeoutSeconds
}

function Assert-ExactStateResponse {
    param($Response, $Pose, [string] $Label)
    if ([bool] (Get-RequiredPropertyValue $Response 'ReturnValue' $Label) -ne
        $true) {
        throw "$Label returned false."
    }
    $report = [string] (
        Get-RequiredPropertyValue $Response 'OutReport' $Label)
    $markers = @(
        'ISTANA_EXPLORE_V5D_R32_PLAYER0_CAPTURE_STATE_VALID',
        'exactPlayer0=true', 'r32MediumDistanceTurfOwner=1',
        'r32SelectedTransforms=4608', 'r32OwnedHismCount=12',
        'r32CullMeters=65-90', 'groundVegetationOwner=1',
        'r31BroadShellVisible=true', 'r30Owner=1',
        'r29VegetationOwner=1', 'r29TerrainOwner=1',
        'treeRealismOwner=1', 'contextPolicyOwner=1',
        'ionAssetId=2275207', 'providerFallbackVisualQa=true',
        'providerReadyProofClaimed=false',
        'airSimTriadRuntimeLoaded=true', 'weatherActorResolved=true',
        "poseId=$($Pose.Id)", 'exactQaViewPose=true',
        "turfIntersectionDistanceProbe=$($Pose.TurfProbe.ToString().ToLowerInvariant())",
        'visualCaptureAccepted=false', 'performanceAccepted=false',
        'surveyClaim=false', 'botanicalClaim=false',
        'currentConditionClaim=false')
    $markers += [string]::Format(
        [Globalization.CultureInfo]::InvariantCulture,
        'turfIntersectionDistanceMeters={0:F3}',
        [double] $Pose.DistanceMeters)
    foreach ($marker in $markers) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Label lacks exact marker '$marker': $report"
        }
    }
    $report
}

function Wait-RuntimeCaptureReady {
    param($Owned, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $last = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        Update-OwnedMemoryGuard $Owned 'waiting for R32 capture API'
        $Owned.Handle.Refresh()
        if ($Owned.Handle.HasExited) {
            throw "R32 Game exited before capture API readiness: code=$($Owned.Handle.ExitCode)"
        }
        try {
            Assert-RcPortOwnedByGame $Owned
            $response = Invoke-RcCall `
                'GetIstanaExploreV5DR32Player0CaptureState' @{} 5
            if ([bool] $response.ReturnValue -eq $true -and
                [string] $response.OutReport -match
                    'ISTANA_EXPLORE_V5D_R32_PLAYER0_CAPTURE_STATE_VALID') {
                return [string] $response.OutReport
            }
            $last = [string] $response.OutReport
        }
        catch { $last = $_.Exception.Message }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    throw "R32 capture API did not become ready: $last"
}

function Wait-StablePose {
    param($Owned, $Pose, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $stableStarted = $null
    $samples = 0
    $last = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        Update-OwnedMemoryGuard $Owned "holding pose $($Pose.Id)"
        try {
            $response = Invoke-RcCall `
                'GetIstanaExploreV5DR32Player0CaptureState' @{} 5
            $last = Assert-ExactStateResponse `
                $response $Pose "stable pose $($Pose.Id)"
            ++$samples
            if ($null -eq $stableStarted) { $stableStarted = [DateTime]::UtcNow }
            if (([DateTime]::UtcNow - $stableStarted).TotalSeconds -ge 12.0) {
                return [pscustomobject] [ordered] @{
                    PoseId=$Pose.Id
                    StableSeconds=12
                    Samples=$samples
                    FinalReport=$last
                }
            }
        }
        catch {
            $last = $_.Exception.Message
            $stableStarted = $null
            $samples = 0
        }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    throw "R32 pose did not hold exact state for 12 seconds: pose=$($Pose.Id) last={$last}"
}

function Get-PngReceipt {
    param([string] $Path)
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
    $stream = $null
    $bitmap = $null
    $decoded = $null
    $bitmapData = $null
    try {
        $stream = [IO.File]::Open(
            $Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            [IO.FileShare]::Read)
        $bitmap = [Drawing.Bitmap]::FromStream($stream, $true, $true)
        if ($bitmap.Width -ne 2560 -or $bitmap.Height -ne 1440) {
            throw "PNG dimensions are not exactly 2560x1440: $Path"
        }
        $decoded = [Drawing.Bitmap]::new(
            2560, 1440, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($decoded)
        try { $graphics.DrawImageUnscaled($bitmap, 0, 0) }
        finally { $graphics.Dispose() }

        $colors = [Collections.Generic.HashSet[int]]::new()
        $minimumLuminance = 255.0
        $maximumLuminance = 0.0
        for ($y = 40; $y -lt 1440; $y += 80) {
            for ($x = 40; $x -lt 2560; $x += 80) {
                $color = $decoded.GetPixel($x, $y)
                [void] $colors.Add($color.ToArgb())
                $luminance = 0.2126 * $color.R +
                    0.7152 * $color.G + 0.0722 * $color.B
                $minimumLuminance = [Math]::Min($minimumLuminance, $luminance)
                $maximumLuminance = [Math]::Max($maximumLuminance, $luminance)
            }
        }
        $range = $maximumLuminance - $minimumLuminance
        if ($colors.Count -lt 32 -or $range -lt 12.0) {
            throw "PNG decoded but is blank/near-uniform: colors=$($colors.Count) luminanceRange=$range path=$Path"
        }
        $rectangle = [Drawing.Rectangle]::new(0, 0, 2560, 1440)
        $bitmapData = $decoded.LockBits(
            $rectangle,
            [Drawing.Imaging.ImageLockMode]::ReadOnly,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $decodedBytes = [Math]::Abs($bitmapData.Stride) * 1440
        $pixels = [byte[]]::new($decodedBytes)
        [Runtime.InteropServices.Marshal]::Copy(
            $bitmapData.Scan0, $pixels, 0, $decodedBytes)
        $pixelHash = [Convert]::ToHexString(
            [Security.Cryptography.SHA256]::HashData($pixels))
        $state = Get-FileState $Path
        [pscustomobject] [ordered] @{
            Path=[IO.Path]::GetFullPath($Path)
            Present=$state.Present
            Bytes=$state.Bytes
            Sha256=$state.Sha256
            LastWriteUtc=$state.LastWriteUtc
            WidthPixels=2560
            HeightPixels=1440
            PixelFormat='BGRA8_SDR'
            DecodedBgraSha256=$pixelHash
            SampledDistinctColorCount=$colors.Count
            SampledLuminanceRange=$range
            NonBlank=$true
            DecodedSuccessfully=$true
        }
    }
    finally {
        if ($null -ne $bitmapData -and $null -ne $decoded) {
            $decoded.UnlockBits($bitmapData)
        }
        if ($null -ne $decoded) { $decoded.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
        if ($null -ne $stream) { $stream.Dispose() }
    }
}

function Wait-StableDecodedPng {
    param(
        $Owned,
        [string] $Path,
        [DateTime] $RequestedUtc,
        [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $prior = $null
    $lastError = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        Update-OwnedMemoryGuard $Owned "waiting for PNG $Path"
        try {
            $receipt = Get-PngReceipt $Path
            if ([DateTime]::Parse($receipt.LastWriteUtc).ToUniversalTime() -lt
                $RequestedUtc.AddSeconds(-2)) {
                throw 'PNG predates its capture request.'
            }
            if ($null -ne $prior -and
                $receipt.Bytes -eq $prior.Bytes -and
                $receipt.Sha256 -ceq $prior.Sha256 -and
                $receipt.DecodedBgraSha256 -ceq $prior.DecodedBgraSha256) {
                return $receipt
            }
            $prior = $receipt
        }
        catch { $lastError = $_.Exception.Message }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    throw "PNG did not become a stable decoded 2560x1440 image: $Path error={$lastError}"
}

function Assert-CookedClosure {
    param(
        [string] $CookedPlatformRoot,
        [DateTime] $CookStartedUtc)
    if (-not [IO.Directory]::Exists($CookedPlatformRoot)) {
        throw "Fresh isolated cook root is absent: $CookedPlatformRoot"
    }
    $mapCandidates = @(
        Get-ChildItem -LiteralPath $CookedPlatformRoot -Recurse -File -Force |
            Where-Object {
                $_.Name -eq 'Istana_PublicView_Explore_v5d_hybrid.umap'
            })
    if ($mapCandidates.Count -ne 1 -or
        $mapCandidates[0].LastWriteTimeUtc -lt $CookStartedUtc.AddSeconds(-2)) {
        throw 'Fresh isolated cook did not produce exactly one current target map.'
    }
    $receipt = @(Get-TreeReceipt $CookedPlatformRoot)
    if ($receipt.Count -le 0) { throw 'Fresh isolated cooked closure is empty.' }
    [pscustomobject] [ordered] @{
        Root=[IO.Path]::GetFullPath($CookedPlatformRoot)
        TargetMapRelativePath=[IO.Path]::GetRelativePath(
            $CookedPlatformRoot, $mapCandidates[0].FullName)
        FileCount=$receipt.Count
        Files=@($receipt)
    }
}

function Assert-StaticContract {
    Assert-CaptureSourcePins
    if ($poses.Count -ne 10 -or
        ([string]::Join(',', @($poses.Id))) -cne
            '075m,020m,008m,002m,surroundings_oblique_macdonald,012m,050m,065m,090m,095m') {
        throw 'R32 capture pose roster drifted from the exact ten reviewed views.'
    }
    foreach ($probe in @($poses | Where-Object TurfProbe)) {
        if ($probe.X -ne 3500.0 -or $probe.Y -ne 15000.0 -or
            $probe.Z -ne 164.0 -or $probe.Yaw -ne -90.0 -or
            $probe.Roll -ne 0.0) {
            throw "R32 turf probe changed the inherited camera rig: $($probe.Id)"
        }
        $expectedPitch = -[Math]::Atan2(
            1.64, [double] $probe.DistanceMeters) * 180.0 / [Math]::PI
        if ([Math]::Abs($probe.Pitch - $expectedPitch) -gt 0.0000000005) {
            throw "R32 turf probe pitch no longer intersects Z=0 at its exact distance: $($probe.Id)"
        }
    }
    $headerPath = Join-Path $repositoryUnrealRoot `
        $captureSourcePins[0].RelativePath
    $sourcePath = Join-Path $repositoryUnrealRoot `
        $captureSourcePins[1].RelativePath
    $header = [IO.File]::ReadAllText($headerPath)
    $source = [IO.File]::ReadAllText($sourcePath)
    foreach ($marker in @(
        'GetIstanaExploreV5DR32Player0CaptureState',
        'SetIstanaExploreV5DR32Player0CapturePose',
        'CaptureIstanaExploreV5DR32Player0TurfView',
        'FinishIstanaExploreV5DR32Player0CaptureRun')) {
        if (-not $header.Contains($marker, [StringComparison]::Ordinal) -or
            -not $source.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R32 capture library lacks endpoint marker: $marker"
        }
    }
    foreach ($marker in @(
        'static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 10)',
        'ExactV2ComponentCount != 1',
        'CurrentSurroundingsRenderOnlyComponent',
        'R28FacadeTagCount != 0', 'R29FacadeTagCount != 0',
        'LandmarkVegetationTagCount != 0',
        'ATRIADIstanaExploreV5DR32MediumDistanceTurfActor',
        'ValidateR32MediumDistanceTurf',
        'ExpectedBucketCount()', 'ExpectedInstanceCount()',
        'ValidateCurrentSurroundingsBroadShellR31ForInheritedScene',
        'ValidateGroundVegetationRealism', 'ValidateR29Vegetation',
        'ValidateCopernicusTerrainFallback', 'ValidateTreeRealism',
        'GooglePhotorealistic3DTilesIonAssetId = 2275207',
        'AirSimTriadRuntime', 'WeatherActor',
        'SetResolution(2560, 1440, 1.0f)',
        'performanceAccepted=false', 'surveyClaim=false',
        'botanicalClaim=false', 'currentConditionClaim=false')) {
        if (-not $source.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R32 capture source lacks fail-closed marker: $marker"
        }
    }
    if ($header.Contains('R33', [StringComparison]::Ordinal) -or
        $source.Contains('R33', [StringComparison]::Ordinal)) {
        throw 'R32 capture C++ source contains a forbidden successor reference.'
    }

    $contractPath = Join-Path $repositoryUnrealRoot `
        'SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R32MediumDistanceTurf\r32_medium_distance_turf.contract.json'
    $contract = Get-Content -LiteralPath $contractPath -Raw |
        ConvertFrom-Json -Depth 64
    $gate = Get-RequiredPropertyValue `
        $contract 'nativePlayer0CaptureGate' 'R32 contract'
    if ([string] $gate.schema -cne $schema -or
        [string] $gate.wrapper -cne
            'scripts/Capture-IstanaExploreV5DR32Player0Evidence.ps1' -or
        [int] $gate.exactPoseCount -ne 10 -or
        [bool] $gate.twoPhaseHumanVisualReviewRequired -ne $true -or
        [bool] $gate.executeCanAcceptVisualQuality -ne $false -or
        [bool] $gate.acceptanceLaunchesUnreal -ne $false -or
        [bool] $gate.acceptanceWritesNativeProject -ne $false -or
        [bool] $gate.fullNativePluginSourceTreeReceiptRequired -ne $true -or
        [bool] $gate.r33SourceAllowed -ne $false -or
        [int64] $gate.fixedLaunchFreeVirtualBytes -ne
            $minimumSystemFreeVirtualAtLaunchBytes -or
        [int64] $gate.fixedOwnedTreePrivateMemoryCeilingBytes -ne
            $privateMemoryCeilingBytes -or
        [int64] $gate.fixedContinuousFreeVirtualBytes -ne
            $minimumSystemFreeVirtualBytes) {
        throw 'R32 contract capture gate drifted from the guarded wrapper.'
    }
    [pscustomobject] [ordered] @{
        Schema=$schema
        Status='STATIC_SELF_CHECK_PASS'
        RepositoryOnly=$true
        NativeAccess=$false
        UnrealLaunched=$false
        ExactPoseCount=10
        PoseIds=@($poses.Id)
        TurfReadabilityReviewPoseIds=@('012m','020m','050m')
        TurfIntersectionProbeIds=@('012m','050m','065m','090m','095m')
        TurfIntersectionProbeDistancesMeters=@(12.0,50.0,65.0,90.0,95.0)
        ExactResolution='2560x1440'
        SdrRequired=$true
        DecodedDistinctPngProofRequired=$true
        R32CommitReceiptRequired=$true
        FullAcceptedR31ChainRequired=$true
        FullNativePluginSourceTreeReceiptRequired=$true
        MapDllGroundNoMutationBindingsRequired=$true
        R33DependencyAllowed=$false
        FixedLaunchFreeVirtualBytes=$minimumSystemFreeVirtualAtLaunchBytes
        FixedOwnedTreePrivateMemoryCeilingBytes=$privateMemoryCeilingBytes
        FixedContinuousFreeVirtualBytes=$minimumSystemFreeVirtualBytes
        SerialFreshBuild=$true
        FreshIsolatedCook=$true
        TwoPhaseHumanVisualReview=$true
        ExecuteCanAcceptVisualQuality=$false
        PerformanceAcceptanceClaimed=$false
        HyperrealismClaimed=$false
        BotanicalSurveyOrCurrentConditionClaimed=$false
    }
}

function Assert-PendingCaptureReceipt {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $ExpectedRunToken)
    if ($ExpectedRunToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw 'Acceptance requires one safe run token.'
    }
    $evidenceRoot = [IO.Path]::GetFullPath(
        (Join-Path $nativeEvidenceBase $ExpectedRunToken))
    $expectedPath = [IO.Path]::GetFullPath(
        (Join-Path $evidenceRoot 'pending-visual-review.json'))
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not $fullPath.Equals(
            $expectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Pending R32 receipt path is not the exact run-token evidence child.'
    }
    $file = Get-FileState $fullPath
    if (-not $file.Present -or
        $file.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw 'Pending R32 visual-review receipt hash mismatch.'
    }
    $receipt = Get-Content -LiteralPath $fullPath -Raw |
        ConvertFrom-Json -Depth 64
    if ([string] (Get-RequiredPropertyValue $receipt 'Schema' 'pending R32 receipt') -cne $schema -or
        [string] (Get-RequiredPropertyValue $receipt 'Status' 'pending R32 receipt') -cne 'PENDING_VISUAL_REVIEW' -or
        [string] (Get-RequiredPropertyValue $receipt 'RunToken' 'pending R32 receipt') -cne $ExpectedRunToken -or
        [string] (Get-RequiredPropertyValue $receipt 'NativeOrder' 'pending R32 receipt') -cne 'R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33' -or
        [bool] (Get-RequiredPropertyValue $receipt 'R33DependencyAllowed' 'pending R32 receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactPoseCount' 'pending R32 receipt') -ne 10 -or
        [bool] (Get-RequiredPropertyValue $receipt 'MechanicalCaptureValidationPassed' 'pending R32 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ExplicitHumanReviewAcceptance' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ConfirmedTenImagesReviewed' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewRequired' 'pending R32 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewAccepted' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanVisualReviewAttested' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'R32MediumDistanceTurfVisualQaAccepted' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'R33AdmissionAuthorized' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Preserved' 'pending R32 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'MapModifiedByCapture' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SimulationCollisionNavigationSensorRfModified' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'PerformanceAcceptanceClaimed' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HyperrealismClaimed' 'pending R32 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'BotanicalSurveyOrCurrentConditionClaimed' 'pending R32 receipt') -ne $false) {
        throw 'Pending R32 receipt failed the exact two-phase contract.'
    }

    $commitAdmission = Get-RequiredPropertyValue `
        $receipt 'R32CommitAdmission' 'pending R32 receipt'
    $commitPath = [string] (
        Get-RequiredPropertyValue $commitAdmission 'Path' 'R32 commit admission')
    $commitFile = Get-RequiredPropertyValue `
        $commitAdmission 'File' 'R32 commit admission'
    $admission = Assert-R32CommitReceipt `
        $commitPath ([string] $commitFile.Sha256)
    if (-not [IO.Path]::GetFullPath($commitPath).Equals(
            $admission.R32.Path, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-StateMatches $commitFile $admission.R32.State) -or
        [string] (Get-RequiredPropertyValue $commitAdmission 'CallerSha256' 'R32 commit admission') -cne [string] $commitFile.Sha256 -or
        [string] (Get-RequiredPropertyValue $commitAdmission 'Schema' 'R32 commit admission') -cne $r32TransactionSchema -or
        [string] (Get-RequiredPropertyValue $commitAdmission 'Status' 'R32 commit admission') -cne 'COMMITTED') {
        throw 'Pending receipt no longer binds its exact R32 commit receipt.'
    }
    foreach ($commitBinding in @(
        [pscustomobject] @{ Name='SuccessorMap'; State=$admission.Map }
        [pscustomobject] @{ Name='RuntimeDllAfter'; State=$admission.RuntimeDll }
        [pscustomobject] @{ Name='EditorDllAfter'; State=$admission.EditorDll }
        [pscustomobject] @{ Name='GroundHeaderAfter'; State=$admission.GroundHeader }
        [pscustomobject] @{ Name='GroundSourceAfter'; State=$admission.GroundSource })) {
        if (-not (Test-StateMatches `
                (Get-RequiredPropertyValue $commitAdmission `
                    $commitBinding.Name 'R32 commit admission') `
                $commitBinding.State)) {
            throw "Pending R32 commit admission drifted at $($commitBinding.Name)."
        }
    }
    if ((ConvertTo-Json @(Get-RequiredPropertyValue $commitAdmission `
                'NativePluginSourceTreeAfter' 'R32 commit admission') `
            -Depth 6 -Compress) -cne
        (ConvertTo-Json @($admission.NativePluginSourceTree) `
            -Depth 6 -Compress)) {
        throw 'Pending R32 commit admission plugin Source tree drifted.'
    }
    if ((ConvertTo-Json (Get-RequiredPropertyValue $commitAdmission `
                'TreeMaterialResponseV3' 'R32 commit admission') `
            -Depth 16 -Compress) -cne
        (ConvertTo-Json $admission.TreeMaterialResponseV3 `
            -Depth 16 -Compress)) {
        throw 'Pending R32 commit admission TreeRealism v3 identity drifted.'
    }
    foreach ($chainBinding in @(
        [pscustomobject] @{ Name='R30TransactionAdmission'; Admission=$admission.R30Transaction }
        [pscustomobject] @{ Name='R30CaptureAdmission'; Admission=$admission.R30Capture }
        [pscustomobject] @{ Name='R31TransactionAdmission'; Admission=$admission.R31Transaction }
        [pscustomobject] @{ Name='R31CaptureAdmission'; Admission=$admission.R31Capture })) {
        $embedded = Get-RequiredPropertyValue `
            $commitAdmission $chainBinding.Name 'R32 commit admission'
        if (-not [IO.Path]::GetFullPath([string] (
                    Get-RequiredPropertyValue $embedded 'Path' `
                        $chainBinding.Name)).Equals(
                $chainBinding.Admission.Path,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-StateMatches `
                (Get-RequiredPropertyValue $embedded 'File' `
                    $chainBinding.Name) $chainBinding.Admission.State)) {
            throw "Pending R32 commit admission drifted at $($chainBinding.Name)."
        }
    }

    $binding = Get-RequiredPropertyValue `
        $receipt 'BindingEvidence' 'pending R32 receipt'
    $mapBinding = Get-RequiredPropertyValue `
        $binding 'MapCaptureBinding' 'pending R32 binding'
    $runtimeBinding = Get-RequiredPropertyValue `
        $binding 'RuntimeEditorDllCaptureBinding' 'pending R32 binding'
    $editorBinding = Get-RequiredPropertyValue `
        $binding 'EditorDllCaptureBinding' 'pending R32 binding'
    foreach ($row in @(
        [pscustomobject] @{ Binding=$mapBinding; Path=$mapFile; State=$admission.Map; Label='map' }
        [pscustomobject] @{ Binding=$runtimeBinding; Path=$runtimeEditorDll; State=$admission.RuntimeDll; Label='runtime DLL' }
        [pscustomobject] @{ Binding=$editorBinding; Path=$editorDll; State=$admission.EditorDll; Label='editor DLL' })) {
        if (-not [IO.Path]::GetFullPath([string] $row.Binding.Path).Equals(
                $row.Path, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-StateMatches $row.Binding.State $row.State)) {
            throw "Pending R32 $($row.Label) binding drifted."
        }
        [void] (Assert-State $row.State $row.Path `
            "pending R32 $($row.Label)")
    }

    $groundHeaderBinding = Get-RequiredPropertyValue `
        $binding 'GroundHeaderCaptureBinding' 'pending R32 binding'
    $groundSourceBinding = Get-RequiredPropertyValue `
        $binding 'GroundSourceCaptureBinding' 'pending R32 binding'
    foreach ($row in @(
        [pscustomobject] @{ Binding=$groundHeaderBinding; Path=$groundHeader; State=$admission.GroundHeader; Label='Ground header' }
        [pscustomobject] @{ Binding=$groundSourceBinding; Path=$groundSource; State=$admission.GroundSource; Label='Ground source' })) {
        if (-not [IO.Path]::GetFullPath([string] $row.Binding.Path).Equals(
                $row.Path, [StringComparison]::OrdinalIgnoreCase) -or
            [bool] $row.Binding.Unchanged -ne $true -or
            -not (Test-StateMatches $row.Binding $row.State) -or
            -not (Test-StateMatches $row.Binding.ImmediatePreCapture $row.State) -or
            -not (Test-StateMatches $row.Binding.ImmediatePostCapture $row.State)) {
            throw "Pending R32 $($row.Label) no-mutation binding drifted."
        }
        [void] (Assert-State $row.State $row.Path `
            "pending R32 $($row.Label)")
    }

    $treeBinding = Get-RequiredPropertyValue `
        $binding 'NativePluginSourceTreeCaptureBinding' 'pending R32 binding'
    $expectedTree = @(Get-RequiredPropertyValue `
        $treeBinding 'BaselineAfterCaptureSourcePromotion' 'pending R32 tree')
    if (-not [IO.Path]::GetFullPath([string] $treeBinding.Root).Equals(
            $nativePluginSourceRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [bool] $treeBinding.Unchanged -ne $true -or
        [int] $treeBinding.FileCount -ne $expectedTree.Count -or
        (ConvertTo-Json $expectedTree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($treeBinding.ImmediatePreCapture) -Depth 6 -Compress) -or
        (ConvertTo-Json $expectedTree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($treeBinding.ImmediatePostCapture) -Depth 6 -Compress)) {
        throw 'Pending R32 native plugin Source-tree no-mutation binding drifted.'
    }
    [void] (Assert-TreeReceipt `
        $nativePluginSourceRoot $expectedTree 'pending R32 plugin Source')

    $captures = @(Get-RequiredPropertyValue `
        $receipt 'Captures' 'pending R32 receipt')
    if ($captures.Count -ne 10) {
        throw 'Pending R32 capture must contain exactly ten image rows.'
    }
    $fileHashes = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::Ordinal)
    $pixelHashes = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::Ordinal)
    for ($index = 0; $index -lt $poses.Count; ++$index) {
        $row = $captures[$index]
        $pose = Get-RequiredPropertyValue `
            $row 'Pose' "pending R32 capture row $index"
        $expectedPose = $poses[$index]
        $image = Get-RequiredPropertyValue `
            $row 'Image' "pending R32 capture row $index"
        $expectedImagePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            "explore_v5d_r32_player0_$($expectedPose.Id)_$ExpectedRunToken.png"))
        if ([string] $pose.Id -cne $expectedPose.Id -or
            [double] $pose.X -ne [double] $expectedPose.X -or
            [double] $pose.Y -ne [double] $expectedPose.Y -or
            [double] $pose.Z -ne [double] $expectedPose.Z -or
            [double] $pose.Pitch -ne [double] $expectedPose.Pitch -or
            [double] $pose.Yaw -ne [double] $expectedPose.Yaw -or
            [double] $pose.Roll -ne [double] $expectedPose.Roll -or
            [bool] $pose.TurfIntersectionDistanceProbe -ne
                [bool] $expectedPose.TurfProbe -or
            [double] $pose.TurfIntersectionDistanceMeters -ne
                [double] $expectedPose.DistanceMeters -or
            [bool] $row.R32MediumDistanceTurfObserved -ne $true -or
            [bool] $row.ProviderReadyProofClaimed -ne $false -or
            -not [IO.Path]::GetFullPath([string] $image.Path).Equals(
                $expectedImagePath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Pending R32 capture row $index identity drifted."
        }
        $decoded = Get-PngReceipt $expectedImagePath
        if (-not (Test-StateMatches $decoded $image) -or
            [string] $decoded.DecodedBgraSha256 -cne
                [string] $image.DecodedBgraSha256 -or
            [int] $decoded.WidthPixels -ne 2560 -or
            [int] $decoded.HeightPixels -ne 1440 -or
            [bool] $decoded.NonBlank -ne $true -or
            -not $fileHashes.Add([string] $decoded.Sha256) -or
            -not $pixelHashes.Add([string] $decoded.DecodedBgraSha256)) {
            throw "Pending R32 PNG row $index failed decode/hash/distinctness replay."
        }
    }
    if ($fileHashes.Count -ne 10 -or $pixelHashes.Count -ne 10) {
        throw 'Pending R32 images are not ten-way distinct by file and decoded pixels.'
    }
    Assert-NoSuccessorNativeSource
    [void] (Assert-State $file $fullPath `
        'hash-pinned pending R32 visual-review receipt')
    [pscustomobject] [ordered] @{
        Path=$fullPath
        File=$file
        EvidenceRoot=$evidenceRoot
        Receipt=$receipt
        Admission=$admission
        BindingEvidence=$binding
        CurrentTree=@($expectedTree)
    }
}

function Invoke-VisualReviewAcceptance {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $ExpectedRunToken)
    Assert-NativeIdle 'before explicit R32 visual-review acceptance'
    $pendingAdmission = Assert-PendingCaptureReceipt `
        $Path $ExpectedSha256 $ExpectedRunToken
    $pending = $pendingAdmission.Receipt
    $binding = $pendingAdmission.BindingEvidence
    $immutable = Get-RequiredPropertyValue `
        $pending 'ImmutableContent' 'pending R32 receipt'
    Assert-ImmutableContentSnapshot $immutable
    $before = Assert-CaptureBindings `
        $pendingAdmission.Admission $pendingAdmission.CurrentTree
    $after = Assert-CaptureBindings `
        $pendingAdmission.Admission $pendingAdmission.CurrentTree
    if ((ConvertTo-Json $before -Depth 16 -Compress) -cne
        (ConvertTo-Json $after -Depth 16 -Compress)) {
        throw 'Native state changed during explicit R32 visual acceptance.'
    }
    [void] (Assert-State `
        $pendingAdmission.File $pendingAdmission.Path `
        'pending R32 receipt immediately before acceptance publication')
    Assert-ImmutableContentSnapshot $immutable
    Assert-NativeIdle 'before explicit R32 visual acceptance publication'

    $accepted = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='COMMITTED'
        RunToken=$ExpectedRunToken
        NativeOrder='R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33'
        R33DependencyAllowed=$false
        PendingVisualReviewReceipt=[pscustomobject] [ordered] @{
            Path=$pendingAdmission.Path
            State=$pendingAdmission.File
            File=$pendingAdmission.File
            CallerSha256=$ExpectedSha256.ToUpperInvariant()
            Status='PENDING_VISUAL_REVIEW'
        }
        StaticReceipt=(Get-RequiredPropertyValue `
            $pending 'StaticReceipt' 'pending R32 receipt')
        R32CommitAdmission=(Get-RequiredPropertyValue `
            $pending 'R32CommitAdmission' 'pending R32 receipt')
        Map=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'MapCaptureBinding' 'R32 binding') `
            'State' 'R32 map binding')
        RuntimeEditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'RuntimeEditorDllCaptureBinding' 'R32 binding') `
            'State' 'R32 runtime DLL binding')
        EditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'EditorDllCaptureBinding' 'R32 binding') `
            'State' 'R32 editor DLL binding')
        GroundHeader=(Get-RequiredPropertyValue `
            $binding 'GroundHeaderCaptureBinding' 'R32 binding')
        GroundSource=(Get-RequiredPropertyValue `
            $binding 'GroundSourceCaptureBinding' 'R32 binding')
        NativePluginSourceTree=(Get-RequiredPropertyValue `
            $binding 'NativePluginSourceTreeCaptureBinding' 'R32 binding')
        SourcePromotion=(Get-RequiredPropertyValue `
            $pending 'SourcePromotion' 'pending R32 receipt')
        GameBinaryBefore=(Get-RequiredPropertyValue `
            $pending 'GameBinaryBefore' 'pending R32 receipt')
        GameBinaryAfter=(Get-RequiredPropertyValue `
            $pending 'GameBinaryAfter' 'pending R32 receipt')
        Build=(Get-RequiredPropertyValue `
            $pending 'Build' 'pending R32 receipt')
        Cook=(Get-RequiredPropertyValue `
            $pending 'Cook' 'pending R32 receipt')
        CookedClosure=(Get-RequiredPropertyValue `
            $pending 'CookedClosure' 'pending R32 receipt')
        Game=(Get-RequiredPropertyValue `
            $pending 'Game' 'pending R32 receipt')
        Captures=(Get-RequiredPropertyValue `
            $pending 'Captures' 'pending R32 receipt')
        ExactPoseCount=10
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$true
        ConfirmedTenImagesReviewed=$true
        HumanVisualReviewAttested=$true
        AutomaticVisualAcceptanceAllowed=$false
        VisualReviewRequired=$false
        VisualReviewAccepted=$true
        R32MediumDistanceTurfVisualQaAccepted=$true
        TreeMaterialResponseV3Preserved=$true
        R33AdmissionAuthorized=$true
        ProviderFallbackVisualQaAccepted=$true
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        MapModifiedByCapture=$false
        SimulationCollisionNavigationSensorRfModified=$false
        PerformanceAcceptanceClaimed=$false
        HyperrealismClaimed=$false
        BotanicalSurveyOrCurrentConditionClaimed=$false
        NativeStateMutatedByAcceptance=$false
        UnrealLaunchedByAcceptance=$false
        AcceptanceRevalidation=[pscustomobject] [ordered] @{
            ImmediatePreAcceptance=$before
            ImmediatePostAcceptance=$after
            Unchanged=$true
            PendingReceiptHashPinned=$true
            TenPngsRedecodedAndRehashed=$true
            TreeMaterialResponseV3SourceAndContentClosureRevalidated=$true
            NativeReceiptWriteAllowed=$false
            NativeProjectWriteAllowed=$false
            UnrealLaunchAllowed=$false
        }
        ImmutableContent=$immutable
        AcceptedUtc=[DateTime]::UtcNow.ToString('o')
    }
    $commitPath = Join-Path $pendingAdmission.EvidenceRoot 'commit.json'
    [void] (Write-JsonAtomic `
        $accepted $commitPath $pendingAdmission.EvidenceRoot)
    [void] (Assert-State `
        $pendingAdmission.File $pendingAdmission.Path `
        'pending R32 receipt after acceptance publication')
    $accepted | ConvertTo-Json -Depth 32
}

$modeCount = @($StaticSelfCheck, $Execute, $AcceptVisualReview |
    Where-Object { $_ }).Count
if ($modeCount -ne 1) {
    throw '-StaticSelfCheck, -Execute, and -AcceptVisualReview are mutually exclusive and exactly one is required.'
}
if ($ConfirmTenImagesReviewed -and -not $AcceptVisualReview) {
    throw '-ConfirmTenImagesReviewed is valid only with -AcceptVisualReview.'
}
if ($RunToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
    throw 'RunToken must be 1-64 safe ASCII letters, digits, underscore, or hyphen.'
}

$staticReceipt = Assert-StaticContract
if ($StaticSelfCheck) {
    $staticReceipt | ConvertTo-Json -Depth 16
    exit 0
}
if ($AcceptVisualReview) {
    if (-not $ConfirmTenImagesReviewed) {
        throw 'R32 visual acceptance requires -ConfirmTenImagesReviewed after human review of all ten PNGs.'
    }
    if ([string]::IsNullOrWhiteSpace($PendingCaptureReceipt) -or
        $ExpectedPendingCaptureReceiptSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'R32 visual acceptance requires the pending receipt path and caller-pinned SHA-256.'
    }
    Invoke-VisualReviewAcceptance `
        $PendingCaptureReceipt `
        $ExpectedPendingCaptureReceiptSha256 `
        $RunToken
    exit 0
}

if ([string]::IsNullOrWhiteSpace($R32CommitReceipt) -or
    $ExpectedR32CommitReceiptSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
    throw 'Live R32 capture requires the committed R32 transaction receipt path and caller-pinned SHA-256.'
}

$evidenceRoot = [IO.Path]::GetFullPath(
    (Join-Path $nativeEvidenceBase $RunToken))
if (-not [IO.Path]::GetDirectoryName($evidenceRoot).Equals(
        $nativeEvidenceBase, [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($evidenceRoot) -cne $RunToken) {
    throw 'R32 evidence root is not the exact direct run-token child.'
}
$captureSourceJournal = @()
$transactionStarted = $false
$captureCompleted = $false
$primaryFailure = $null
$rollbackErrors = [Collections.Generic.List[string]]::new()
$admission = $null
$immutableBefore = $null
$r32CommitBefore = $null

try {
    if ([IO.Directory]::Exists($evidenceRoot)) {
        throw "R32 evidence token already exists: $evidenceRoot"
    }
    foreach ($required in @(
        $nativeProjectFile, $dotnet, $unrealBuildTool, $unrealEditorCmd,
        $mapFile, $runtimeEditorDll, $editorDll)) {
        if (-not [IO.File]::Exists($required)) {
            throw "Required R32 capture input is absent: $required"
        }
    }
    if (-not [IO.Directory]::Exists($airSimRuntimeContentRoot)) {
        throw 'AirSimTriadRuntime content is required for R32 capture.'
    }
    Assert-NativeIdle 'before R32 capture preflight'
    $prewriteAdmission = Assert-LaunchAdmission `
        'before any R32 capture write'
    $admission = Assert-R32CommitReceipt `
        $R32CommitReceipt $ExpectedR32CommitReceiptSha256
    $r32CommitBefore = Get-FileState $admission.R32.Path
    Assert-NoSuccessorNativeSource
    $currentTree = @(Get-TreeReceipt $nativePluginSourceRoot)
    $baseIdentity = ConvertTo-Json `
        @(ConvertTo-TreeIdentity $admission.NativePluginSourceTree) `
        -Depth 6 -Compress
    $currentIdentity = ConvertTo-Json `
        @(ConvertTo-TreeIdentity $currentTree) -Depth 6 -Compress
    $expectedCaptureIdentity = ConvertTo-Json `
        @(Get-ExpectedCaptureSourceTree $admission.NativePluginSourceTree) `
        -Depth 6 -Compress
    if ($currentIdentity -cne $baseIdentity -and
        $currentIdentity -cne $expectedCaptureIdentity) {
        throw 'R32 capture preflight found an unreceipted native plugin Source tree.'
    }
    $immutableBefore = Get-ImmutableContentSnapshot
    $bindingBefore = [pscustomobject] [ordered] @{
        Map=Get-PathBoundReceipt `
            $mapFile $admission.Map 'pre-capture R32 map'
        RuntimeEditorDll=Get-PathBoundReceipt `
            $runtimeEditorDll $admission.RuntimeDll 'pre-capture R32 runtime DLL'
        EditorDll=Get-PathBoundReceipt `
            $editorDll $admission.EditorDll 'pre-capture R32 editor DLL'
        GroundHeader=Get-PathBoundReceipt `
            $groundHeader $admission.GroundHeader 'pre-capture R32 Ground header'
        GroundSource=Get-PathBoundReceipt `
            $groundSource $admission.GroundSource 'pre-capture R32 Ground source'
    }
    Assert-NativeIdle 'immediately before R32 evidence-root creation'
    [void] (Assert-LaunchAdmission `
        'immediately before R32 evidence-root creation')
    [IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
    $logsRoot = Join-Path $evidenceRoot 'logs'
    [IO.Directory]::CreateDirectory($logsRoot) | Out-Null
    $transactionStarted = $true

    $promotion = Install-CaptureSourceClosure `
        $admission.NativePluginSourceTree
    $captureSourceJournal = @($promotion.Journal)
    $expectedCaptureTree = @($promotion.ExpectedTree)
    $pluginSourceTreeBaseline = @(Assert-TreeReceipt `
        $nativePluginSourceRoot $expectedCaptureTree `
        'capture-source-promoted R32 plugin Source')
    Assert-NoSuccessorNativeSource

    $gameBefore = Get-FileState $gameExe
    $buildStartedUtc = [DateTime]::UtcNow
    $buildStdout = Join-Path $logsRoot 'build.stdout.log'
    $buildStderr = Join-Path $logsRoot 'build.stderr.log'
    $buildArguments = @(
        $unrealBuildTool,
        'TRIAD', 'Win64', 'Development',
        "-Project=$nativeProjectFile",
        '-TargetType=Game',
        '-Progress', '-WaitMutex', '-NoHotReloadFromIDE',
        '-ForceHeaderGeneration', '-NoUBTMakefiles',
        '-MaxParallelActions=1', '-NoUBA', '-NoUBALocal')
    $buildReceipt = Invoke-GuardedCommand `
        'R32 serial fresh Development Game build' `
        $dotnet $buildArguments $nativeProjectRoot `
        $buildStdout $buildStderr $BuildTimeoutSeconds
    $gameAfter = Assert-FreshFileState `
        $gameExe $buildStartedUtc 'R32 Development Game executable'
    Assert-GameBinaryMarkers
    [void] (Assert-CaptureBindings $admission $expectedCaptureTree)
    Assert-ImmutableContentSnapshot $immutableBefore
    [void] (Assert-State `
        $r32CommitBefore $admission.R32.Path 'R32 transaction receipt after build')

    $cookStartedUtc = [DateTime]::UtcNow
    $cookOutputPattern = Join-Path $evidenceRoot 'Cooked\[Platform]'
    $cookedPlatformRoot = Join-Path $evidenceRoot 'Cooked\Windows'
    $isolatedDdcRoot = Join-Path $evidenceRoot 'DDC'
    $cookRuntimeLog = Join-Path $logsRoot 'cook.runtime.log'
    $cookStdout = Join-Path $logsRoot 'cook.stdout.log'
    $cookStderr = Join-Path $logsRoot 'cook.stderr.log'
    $cookArguments = @(
        $nativeProjectFile,
        '-run=Cook', '-TargetPlatform=Windows',
        "-Map=$mapPackage", "-OutputDir=$cookOutputPattern",
        "-COOKDIR=$airSimRuntimeContentRoot",
        '-NullRHI', '-NoShaderWorker',
        '-asyncstaticmeshcompilation=0',
        '-DDC=InstalledNoZenLocalFallback',
        "-LocalDataCachePath=$isolatedDdcRoot",
        '-unattended', '-nop4', '-NoSplash', '-NoSound',
        '-stdout', '-FullStdOutLogOutput',
        "-abslog=$cookRuntimeLog")
    if (@($cookArguments | Where-Object {
            $_ -match 'Iterate|CookPartialgc'
        }).Count -ne 0) {
        throw 'R32 capture cook must be fresh and non-iterative.'
    }
    $cookReceipt = Invoke-GuardedCommand `
        'R32 fresh isolated Windows cook' `
        $unrealEditorCmd $cookArguments $nativeProjectRoot `
        $cookStdout $cookStderr $CookTimeoutSeconds
    Assert-NoFatalRuntimeLog $cookRuntimeLog 'R32 fresh isolated cook'
    $cookedClosure = Assert-CookedClosure `
        $cookedPlatformRoot $cookStartedUtc
    [void] (Assert-CaptureBindings $admission $expectedCaptureTree)
    Assert-ImmutableContentSnapshot $immutableBefore
    [void] (Assert-State `
        $r32CommitBefore $admission.R32.Path 'R32 transaction receipt after cook')

    $pluginSourceTreeImmediatePreCapture = @(
        Assert-TreeReceipt $nativePluginSourceRoot $expectedCaptureTree `
            'immediate pre-capture R32 plugin Source')
    $groundHeaderImmediatePreCapture = Get-PathBoundReceipt `
        $groundHeader $admission.GroundHeader `
        'immediate pre-capture R32 Ground header'
    $groundSourceImmediatePreCapture = Get-PathBoundReceipt `
        $groundSource $admission.GroundSource `
        'immediate pre-capture R32 Ground source'

    Assert-RcPortUnowned
    $gameRuntimeLog = Join-Path $logsRoot 'game.runtime.log'
    $gameStdout = Join-Path $logsRoot 'game.stdout.log'
    $gameStderr = Join-Path $logsRoot 'game.stderr.log'
    $sandboxRoot = [IO.Path]::GetFullPath($cookedPlatformRoot)
    $gameArguments = @(
        $nativeProjectFile, $mapPackage, '-game',
        "-Sandbox=$sandboxRoot", '-unattended',
        '-NoSplash', '-NoSound', '-NoAutoSave',
        '-RenderOffscreen', '-dx12', '-sm6',
        '-ResX=2560', '-ResY=1440', '-Windowed',
        '-ini:Engine:[HTTP]:HttpMaxConnectionsPerServer=12',
        '-ini:Engine:[/Script/CesiumRuntime.CesiumRuntimeSettings]:MaxCacheItems=32768',
        '-ini:Engine:[ConsoleVariables]:r.Streaming.PoolSize=768',
        '-DDC=InstalledNoZenLocalFallback',
        "-LocalDataCachePath=$isolatedDdcRoot",
        '-RemoteControlHttpServer', '-RCWebControlEnable',
        '-ExecCmds=WebControl.StartServer',
        "-TRIADR32CaptureRun=$RunToken",
        "-TRIADR32CaptureOutputRoot=$evidenceRoot",
        "-abslog=$gameRuntimeLog")
    $gameOwned = Start-GuardedOwnedProcess `
        'R32 standalone cooked Player0 Game capture' `
        $gameExe $gameArguments $nativeProjectRoot `
        $gameStdout $gameStderr
    [void] (Wait-RuntimeCaptureReady $gameOwned $CaptureTimeoutSeconds)
    $captureReceipts = [Collections.Generic.List[object]]::new()
    foreach ($pose in $poses) {
        $turfProbeMarker =
            "turfIntersectionDistanceProbe=$($pose.TurfProbe.ToString().ToLowerInvariant())"
        $turfDistanceMarker = [string]::Format(
            [Globalization.CultureInfo]::InvariantCulture,
            'turfIntersectionDistanceMeters={0:F3}',
            [double] $pose.DistanceMeters)
        Update-OwnedMemoryGuard $gameOwned "before pose $($pose.Id)"
        $setPose = Invoke-RcCall `
            'SetIstanaExploreV5DR32Player0CapturePose' `
            @{ PoseId=[string] $pose.Id } 5
        $setMessage = [string] (
            Get-RequiredPropertyValue $setPose 'OutMessage' `
                "set R32 pose $($pose.Id)")
        if ([bool] $setPose.ReturnValue -ne $true -or
            -not $setMessage.Contains(
                'ISTANA_EXPLORE_V5D_R32_PLAYER0_POSE_PASS',
                [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                "poseId=$($pose.Id)", [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                $turfProbeMarker, [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                $turfDistanceMarker, [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                'simulationCollisionNavigationSensorRfModified=false',
                [StringComparison]::Ordinal)) {
            throw "Unexpected R32 pose acknowledgement: $setMessage"
        }
        $stable = Wait-StablePose `
            $gameOwned $pose $CaptureTimeoutSeconds
        $preCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR32Player0CaptureState' @{} 5
        $preCaptureReport = Assert-ExactStateResponse `
            $preCapture $pose "pre-capture $($pose.Id)"
        $requestedUtc = [DateTime]::UtcNow
        $capture = Invoke-RcCall `
            'CaptureIstanaExploreV5DR32Player0TurfView' `
            @{ PoseId=[string] $pose.Id } 5
        $captureMessage = [string] (
            Get-RequiredPropertyValue $capture 'OutMessage' `
                "capture R32 pose $($pose.Id)")
        foreach ($marker in @(
            'ISTANA_EXPLORE_V5D_R32_PLAYER0_TURF_CAPTURE_ACCEPTED',
            "poseId=$($pose.Id)", 'width=2560', 'height=1440',
            'hdr=false', 'r32MediumDistanceTurfVisible=true',
            'r32SelectedTransforms=4608', 'r32OwnedHismCount=12',
            'r32CullMeters=65-90', $turfProbeMarker, $turfDistanceMarker,
            'providerReadyProofClaimed=false',
            'airSimTriadRuntimeLoaded=true',
            'simulationCollisionNavigationSensorRfModified=false',
            'performanceAccepted=false', 'surveyClaim=false',
            'botanicalClaim=false', 'currentConditionClaim=false')) {
            if ([bool] $capture.ReturnValue -ne $true -or
                -not $captureMessage.Contains(
                    $marker, [StringComparison]::Ordinal)) {
                throw "Unexpected R32 capture acknowledgement marker '$marker': $captureMessage"
            }
        }
        $capturePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            "explore_v5d_r32_player0_$($pose.Id)_$RunToken.png"))
        if (-not (Test-ContainedPath $capturePath $evidenceRoot)) {
            throw "R32 capture path escaped evidence root: $capturePath"
        }
        $image = Wait-StableDecodedPng `
            $gameOwned $capturePath $requestedUtc $CaptureTimeoutSeconds
        $postCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR32Player0CaptureState' @{} 5
        $postCaptureReport = Assert-ExactStateResponse `
            $postCapture $pose "post-capture $($pose.Id)"
        $captureReceipts.Add([pscustomobject] [ordered] @{
            Pose=[pscustomobject] [ordered] @{
                Id=$pose.Id
                X=$pose.X; Y=$pose.Y; Z=$pose.Z
                Pitch=$pose.Pitch; Yaw=$pose.Yaw; Roll=$pose.Roll
                TurfIntersectionDistanceProbe=$pose.TurfProbe
                TurfIntersectionDistanceMeters=$pose.DistanceMeters
            }
            SetPoseAcknowledgement=$setMessage
            StableStateWindow=$stable
            ImmediatePreCaptureState=$preCaptureReport
            CaptureAcknowledgement=$captureMessage
            Image=$image
            ImmediatePostCaptureState=$postCaptureReport
            R32MediumDistanceTurfObserved=$true
            ProviderFallbackVisualQa=$true
            ProviderReadyProofClaimed=$false
        })
    }
    if ($captureReceipts.Count -ne 10 -or
        $false -in @($captureReceipts.Image.NonBlank) -or
        @($captureReceipts.Image.Sha256 | Sort-Object -Unique).Count -ne 10 -or
        @($captureReceipts.Image.DecodedBgraSha256 |
            Sort-Object -Unique).Count -ne 10) {
        throw 'The exact ten R32 Player0 PNGs are absent, blank, or not distinct.'
    }

    $finishTransportError = ''
    try {
        $finish = Invoke-RcCall `
            'FinishIstanaExploreV5DR32Player0CaptureRun' @{} 5
        $finishMessage = [string] $finish.OutMessage
        if ([bool] $finish.ReturnValue -ne $true -or
            -not $finishMessage.Contains(
                'ISTANA_EXPLORE_V5D_R32_PLAYER0_CAPTURE_EXIT_ACCEPTED',
                [StringComparison]::Ordinal)) {
            throw "Unexpected R32 clean-exit acknowledgement: $finishMessage"
        }
    }
    catch {
        $gameOwned.Handle.Refresh()
        if (-not $gameOwned.Handle.HasExited) { throw }
        $finishTransportError = $_.Exception.Message
    }
    $gameReceipt = Wait-GuardedOwnedProcess `
        $gameOwned $ShutdownTimeoutSeconds
    Assert-NativeIdle 'after clean R32 Game exit'
    Assert-NoFatalRuntimeLog $gameRuntimeLog 'standalone R32 capture Game'
    $gameLogText = [IO.File]::ReadAllText($gameRuntimeLog)
    if (-not $gameLogText.Contains(
            'ISTANA_EXPLORE_V5D_R32_PLAYER0_CAPTURE_EXIT_ACCEPTED',
            [StringComparison]::Ordinal)) {
        throw 'R32 Game log lacks the clean capture-exit marker.'
    }
    foreach ($row in $captureReceipts) {
        [void] (Assert-State $row.Image $row.Image.Path `
            "final R32 Player0 PNG $($row.Pose.Id)")
    }
    [void] (Assert-State $gameAfter $gameExe `
        'post-capture R32 Development Game executable')
    $cookedClosurePostflight = @(
        Assert-TreeReceipt $cookedPlatformRoot $cookedClosure.Files `
            'post-capture cooked closure')
    $bindingAfter = Assert-CaptureBindings `
        $admission $expectedCaptureTree
    Assert-ImmutableContentSnapshot $immutableBefore
    [void] (Assert-State `
        $r32CommitBefore $admission.R32.Path 'post-capture R32 transaction receipt')
    Assert-NoSuccessorNativeSource
    $pluginSourceTreeImmediatePostCapture = @(
        Assert-TreeReceipt $nativePluginSourceRoot $expectedCaptureTree `
            'immediate post-capture R32 plugin Source')
    $groundHeaderImmediatePostCapture = Get-PathBoundReceipt `
        $groundHeader $admission.GroundHeader `
        'immediate post-capture R32 Ground header'
    $groundSourceImmediatePostCapture = Get-PathBoundReceipt `
        $groundSource $admission.GroundSource `
        'immediate post-capture R32 Ground source'

    $bindingEvidence = [pscustomobject] [ordered] @{
        MapCaptureBinding=[pscustomobject] [ordered] @{
            Path=$mapFile; State=$admission.Map
        }
        RuntimeEditorDllCaptureBinding=[pscustomobject] [ordered] @{
            Path=$runtimeEditorDll; State=$admission.RuntimeDll
        }
        EditorDllCaptureBinding=[pscustomobject] [ordered] @{
            Path=$editorDll; State=$admission.EditorDll
        }
        GroundHeaderCaptureBinding=[pscustomobject] [ordered] @{
            Path=$groundHeader
            Present=$bindingBefore.GroundHeader.Present
            Bytes=$bindingBefore.GroundHeader.Bytes
            Sha256=$bindingBefore.GroundHeader.Sha256
            LastWriteUtc=$bindingBefore.GroundHeader.LastWriteUtc
            ImmediatePreCapture=$groundHeaderImmediatePreCapture
            ImmediatePostCapture=$groundHeaderImmediatePostCapture
            Unchanged=$true
        }
        GroundSourceCaptureBinding=[pscustomobject] [ordered] @{
            Path=$groundSource
            Present=$bindingBefore.GroundSource.Present
            Bytes=$bindingBefore.GroundSource.Bytes
            Sha256=$bindingBefore.GroundSource.Sha256
            LastWriteUtc=$bindingBefore.GroundSource.LastWriteUtc
            ImmediatePreCapture=$groundSourceImmediatePreCapture
            ImmediatePostCapture=$groundSourceImmediatePostCapture
            Unchanged=$true
        }
        NativePluginSourceTreeCaptureBinding=[pscustomobject] [ordered] @{
            Root=$nativePluginSourceRoot
            BaselineAfterCaptureSourcePromotion=@($pluginSourceTreeBaseline)
            ImmediatePreCapture=@($pluginSourceTreeImmediatePreCapture)
            ImmediatePostCapture=@($pluginSourceTreeImmediatePostCapture)
            FileCount=$pluginSourceTreeBaseline.Count
            Unchanged=$true
        }
    }
    $r32CommitAdmission = [pscustomobject] [ordered] @{
        Path=$admission.R32.Path
        File=$admission.R32.State
        CallerSha256=$ExpectedR32CommitReceiptSha256.ToUpperInvariant()
        Schema=$r32TransactionSchema
        Status='COMMITTED'
        SuccessorMap=$admission.Map
        RuntimeDllAfter=$admission.RuntimeDll
        EditorDllAfter=$admission.EditorDll
        GroundHeaderAfter=$admission.GroundHeader
        GroundSourceAfter=$admission.GroundSource
        NativePluginSourceTreeAfter=@($admission.NativePluginSourceTree)
        TreeMaterialResponseV3=$admission.TreeMaterialResponseV3
        R30TransactionAdmission=[pscustomobject] [ordered] @{
            Path=$admission.R30Transaction.Path
            File=$admission.R30Transaction.State
        }
        R30CaptureAdmission=[pscustomobject] [ordered] @{
            Path=$admission.R30Capture.Path
            File=$admission.R30Capture.State
        }
        R31TransactionAdmission=[pscustomobject] [ordered] @{
            Path=$admission.R31Transaction.Path
            File=$admission.R31Transaction.State
        }
        R31CaptureAdmission=[pscustomobject] [ordered] @{
            Path=$admission.R31Capture.Path
            File=$admission.R31Capture.State
        }
    }
    $pending = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='PENDING_VISUAL_REVIEW'
        RunToken=$RunToken
        NativeOrder='R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33'
        R33DependencyAllowed=$false
        StaticReceipt=$staticReceipt
        PrewriteAdmission=$prewriteAdmission
        R32CommitAdmission=$r32CommitAdmission
        BindingEvidence=$bindingEvidence
        SourcePromotion=[pscustomobject] [ordered] @{
            Transactional=$true
            SourceCount=2
            Pins=$captureSourcePins
            Before=@($captureSourceJournal | ForEach-Object {
                [pscustomobject] [ordered] @{
                    Path=$_.Path; State=$_.Before; Created=$_.Created
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
        GameBinaryAfter=$gameAfter
        Build=$buildReceipt
        Cook=$cookReceipt
        CookedClosure=$cookedClosure
        CookedClosurePostflight=@($cookedClosurePostflight)
        Game=$gameReceipt
        FinishTransportErrorAfterExit=$finishTransportError
        Captures=@($captureReceipts)
        ExactPoseCount=10
        TurfReadabilityReviewPoseIds=@('012m','020m','050m')
        TurfIntersectionProbeIds=@('012m','050m','065m','090m','095m')
        TurfIntersectionProbeDistancesMeters=@(12.0,50.0,65.0,90.0,95.0)
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$false
        ConfirmedTenImagesReviewed=$false
        VisualReviewRequired=$true
        VisualReviewAccepted=$false
        HumanVisualReviewAttested=$false
        AutomaticVisualAcceptanceAllowed=$false
        R32MediumDistanceTurfVisualQaAccepted=$false
        TreeMaterialResponseV3Preserved=$true
        R33AdmissionAuthorized=$false
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        MapModifiedByCapture=$false
        SimulationCollisionNavigationSensorRfModified=$false
        PerformanceAcceptanceClaimed=$false
        HyperrealismClaimed=$false
        BotanicalSurveyOrCurrentConditionClaimed=$false
        ImmutableContent=$immutableBefore
        MechanicalNoMutationEvidence=[pscustomobject] [ordered] @{
            Before=$bindingBefore
            After=$bindingAfter
            MapUnchanged=$true
            RuntimeEditorDllUnchanged=$true
            EditorDllUnchanged=$true
            GroundHeaderUnchanged=$true
            GroundSourceUnchanged=$true
            NativePluginSourceTreeUnchanged=$true
            SimulationCollisionNavigationSensorRfUnchanged=$true
        }
        CompletedUtc=[DateTime]::UtcNow.ToString('o')
    }
    [void] (Write-JsonAtomic `
        $pending (Join-Path $evidenceRoot 'pending-visual-review.json') `
        $evidenceRoot)
    $captureCompleted = $true
    $pending | ConvertTo-Json -Depth 32
}
catch {
    $primaryFailure = $_.Exception
}
finally {
    if (-not $captureCompleted -and $transactionStarted) {
        try { Close-OwnedProcessOnFailure }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Assert-NativeIdle 'before R32 capture source rollback' }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackErrors.Count -eq 0) {
            try { Restore-CaptureSourceClosure $captureSourceJournal }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        if ($null -ne $admission) {
            try {
                [void] (Assert-State $admission.Map $mapFile 'rollback R32 map')
                [void] (Assert-State $admission.RuntimeDll $runtimeEditorDll `
                    'rollback R32 runtime editor DLL')
                [void] (Assert-State $admission.EditorDll $editorDll `
                    'rollback R32 editor DLL')
                [void] (Assert-State $admission.GroundHeader $groundHeader `
                    'rollback R32 Ground header')
                [void] (Assert-State $admission.GroundSource $groundSource `
                    'rollback R32 Ground source')
                [void] (Assert-State $r32CommitBefore $admission.R32.Path `
                    'rollback R32 transaction receipt')
            }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        if ($null -ne $immutableBefore) {
            try { Assert-ImmutableContentSnapshot $immutableBefore }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        $rollback = [pscustomobject] [ordered] @{
            Schema=$schema
            Status=if ($rollbackErrors.Count -eq 0) {
                'ROLLED_BACK'
            } else { 'ROLLBACK_INCOMPLETE' }
            RunToken=$RunToken
            Failure=if ($null -ne $primaryFailure) {
                $primaryFailure.Message
            } else { 'Unknown R32 capture failure.' }
            ExactOwnedProcessContainmentOnly=$true
            CaptureSourceRestorationAttempted=$true
            MapAndAuthoritativeContentPreserved=$true
            BuildOutputsMayRequireNextFreshBuild=$true
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
        throw "R32 capture failed and rollback was incomplete: failure={$($primaryFailure.Message)} rollback={$([string]::Join(' | ', @($rollbackErrors)))}"
    }
    throw $primaryFailure
}
