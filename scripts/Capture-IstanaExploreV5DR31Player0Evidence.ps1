<#
.SYNOPSIS
Build, freshly cook, and capture exact Player0 R31 ProviderFallback evidence,
then explicitly accept a hash-pinned pending receipt after human review.

.DESCRIPTION
This wrapper is intentionally inert unless exactly one active mode is supplied.
The -Execute capture mode consumes a COMMITTED R31 transaction receipt,
transactionally promotes the two hash-pinned runtime capture-library sources,
builds the Development Game target, cooks into a new isolated sandbox, and
    drives five fixed Player0 views plus one unchanged-pose raster-fallback
    comparison through the runtime Remote Control API. It
writes only PENDING_VISUAL_REVIEW evidence; mechanical PNG checks can never
accept visual quality. The separate -AcceptVisualReview mode requires a
    caller-supplied SHA-256 for that pending receipt, an explicit five-image
    review attestation, and an explicit Nanite/raster-pair acceptance. It
    revalidates every immutable binding without launching
Unreal or writing native state, and only then writes the COMMITTED receipt.
Neither mode disables AirSim or AirSimTriadRuntime, and fallback pixels are
never classified as provider-ready proof.

Static contract check (no native-project access and no Unreal launch):
  .\Capture-IstanaExploreV5DR31Player0Evidence.ps1 -StaticSelfCheck

Live execution, only after a successful R31 native transaction:
  .\Capture-IstanaExploreV5DR31Player0Evidence.ps1 -Execute `
    -RunToken r31-player0-reviewed `
    -R31CommitReceipt D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1\<token>\commit.json `
    -ExpectedR31CommitReceiptSha256 <sha256>

Explicit acceptance, only after a human has reviewed all six pending PNGs
(the five baseline views plus the unchanged-pose Nanite/raster comparison):
  .\Capture-IstanaExploreV5DR31Player0Evidence.ps1 -AcceptVisualReview `
    -ConfirmFiveImagesReviewed `
    -ConfirmNaniteRasterPairReviewedAndAccepted `
    -RunToken r31-player0-reviewed `
    -PendingCaptureReceipt D:\triad\TRIAD_R31Evidence\r31-player0-reviewed\pending-visual-review.json `
    -ExpectedPendingCaptureReceiptSha256 <sha256>
#>
[CmdletBinding()]
param(
    [switch] $StaticSelfCheck,
    [switch] $Execute,
    [switch] $AcceptVisualReview,
    [switch] $ConfirmFiveImagesReviewed,
    [switch] $ConfirmNaniteRasterPairReviewedAndAccepted,
    [string] $RunToken = 'static-contract',
    [string] $R31CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR31CommitReceiptSha256 = '',
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

$schema = 'triad.istana_explore_v5d.r31_player0_capture.v2'
$r31TransactionSchema =
    'triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$privateMemoryCeilingBytes = 12884901888L # fixed 12 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # fixed continuous 6 GiB
$memoryWatchdogPollMilliseconds = 500
$memoryWatchdogPersistentBreachMilliseconds = 2000
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$visualReviewContract = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\r31_player0_visual_review.contract.json'))
$expectedVisualReviewContract = [pscustomobject] [ordered] @{
    Present=$true
    Bytes=1749L
    Sha256='5C07E6B0129AD67ED2999E23D1B2C38A23422097A7096718CE3FCB2FE9D5AAAC'
}
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$nativeEvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R31Evidence')
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
$r31ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31'))
$r30ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'))
$r29FacadeContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'))
$r29VegetationContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
$r29TerrainContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'))
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$contextFacadeR25ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25'))
$localFallbackSuppressionV2ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2'))
$contextTextureRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\Textures'))
$airSimRuntimeRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\AirSimTriadRuntime'))
$airSimRuntimeDescriptor = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'AirSimTriadRuntime.uplugin'))
$airSimRuntimeContentRoot = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'Content'))
$projectBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Binaries'))
$projectIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Intermediate'))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$airSimBinaryRoot = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'Binaries'))
$airSimIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $airSimRuntimeRoot 'Intermediate'))
$r31TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$runtimeCaptureLibrary = '/Script/TRIADSensorFusion.Default__TRIADIstanaExploreV5DR31Player0CaptureLibrary'
$script:activeOwnedProcess = $null

$captureSourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR31Player0CaptureLibrary.h'
        Bytes = 2629L
        Sha256 = '779B7974B1C70CD15334F47A68BEEFD338CAE9AD53F9F0049B101FFA0BA973B9'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR31Player0CaptureLibrary.cpp'
        Bytes = 40565L
        Sha256 = 'F41E6E4651B647C51B789C949B73119DEE688249CBA509436DD63D278C2A70C7'
    }
)

$expectedGroundHeader = [pscustomobject] [ordered] @{
    Present=$true
    Bytes=24562L
    Sha256='599358BC0E6290DAB276890BF118A6E5EE59AD62888B847330A9A991B6E2BDE3'
}
$expectedGroundSource = [pscustomobject] [ordered] @{
    Present=$true
    Bytes=246667L
    Sha256='3F00D319112C3E7C7F1172FD128C71360C025B43B6BE2FB138A63DE07C1FDB17'
}

$r31ContextPolicySourcePins = @(
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure\TRIADIstanaExploreV5DContextPolicyActor.h'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h'
        Bytes = 10649L
        Sha256 = '9114F728E337523DF3AD0FE5021685683C41BB048CB0049713C2ECC9342D962B'
    }
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Bytes = 134398L
        Sha256 = '58074AC6449BD6FBF7D86DDB876B9CD3B7BEFF03162DAA9EFE10C0A08E0DF4C4'
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

$r31ContentRelativePaths = @(
    'Materials\M_IPV5D_R31_BroadShellPBR_Master.uasset'
    'Materials\MI_IPV5D_R31_OfficialWall.uasset'
    'Materials\MI_IPV5D_R31_OfficialRoof.uasset'
    'Materials\MI_IPV5D_R31_FallbackWall.uasset'
    'Materials\MI_IPV5D_R31_FallbackRoof.uasset'
)

$requiredCookedDependencyRoots = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'
    'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2'
    'Content\TRIAD\IstanaPublicView\Textures'
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
            $path 'R31 capture-library source')
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
                RelativePath=[string] $_.RelativePath
                Bytes=[int64] $_.Bytes
                Sha256=[string] $_.Sha256
            }
        }
    )
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
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
            [void] (Assert-State $row.Before $row.Path 'restored capture journal file')
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
    }
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
                    Remove-Item -LiteralPath $file.FullName -Force
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
        throw "$Label refused by fixed 10 GiB FreeVirtualMemory gate: available=$freeBytes"
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
    if ($null -ne ('Triad.R31Capture.MemoryWatchdog' -as [type])) {
        return
    }
    $source = @'
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace Triad.R31Capture
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
                this.thread.Name = "TRIAD R31 capture memory watchdog";
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
        [string[]] $RequiredCommandLineTokens
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
    $handle = Start-Process -FilePath $FilePath `
        -ArgumentList $argumentLine `
        -WorkingDirectory $WorkingDirectory `
        -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog `
        -PassThru -WindowStyle Hidden
    $handle.Refresh()
    if ($handle.HasExited) {
        throw "$Label exited before exact identity registration."
    }
    $identity = Get-CimInstance Win32_Process `
        -Filter "ProcessId=$($handle.Id)" -ErrorAction Stop
    $expectedPath = [IO.Path]::GetFullPath($FilePath)
    if ($null -eq $identity -or -not $identity.ExecutablePath -or
        -not [IO.Path]::GetFullPath($identity.ExecutablePath).Equals(
            $expectedPath, [StringComparison]::OrdinalIgnoreCase)) {
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
    $watchdog = [Triad.R31Capture.MemoryWatchdog]::new(
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

function Wait-GuardedOwnedProcess {
    param($Owned, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $Owned.Handle.WaitForExit($memoryWatchdogPollMilliseconds)) {
        [void] (Assert-WatchdogHealthy $Owned.Watchdog $Owned.Label)
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "$($Owned.Label) exceeded its bounded timeout of $TimeoutSeconds seconds."
        }
    }
    [void] (Assert-WatchdogHealthy $Owned.Watchdog "$($Owned.Label) exit")
    if ($Owned.Handle.ExitCode -ne 0) {
        throw "$($Owned.Label) failed: exit=$($Owned.Handle.ExitCode)"
    }
}

function Close-GuardedOwnedProcess {
    param($Owned, [switch] $AllowContainment)
    if ($null -eq $Owned) { return $null }
    $error = $null
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
    }
    catch { $error = $_.Exception }
    $snapshot = $null
    try {
        $Owned.Watchdog.Stop()
        $snapshot = $Owned.Watchdog.GetSnapshot()
        $Owned.Watchdog.Dispose()
        if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError) -or
            -not [string]::IsNullOrWhiteSpace($snapshot.AlertKind) -or
            $snapshot.SampleCount -le 0) {
            throw "Memory watchdog final state is not healthy for $($Owned.Label)."
        }
    }
    catch {
        if ($null -eq $error) { $error = $_.Exception }
        else {
            $error = [InvalidOperationException]::new(
                "process={$($error.Message)} watchdog={$($_.Exception.Message)}")
        }
    }
    $script:activeOwnedProcess = $null
    if ($null -ne $error) { throw $error }
    [pscustomobject] [ordered] @{
        Label=$Owned.Label
        ProcessId=$Owned.ProcessId
        StartUtc=$Owned.StartUtc
        ExitCode=if ($Owned.Handle.HasExited) { $Owned.Handle.ExitCode } else { $null }
        CommandLine=$Owned.CommandLine
        Admission=$Owned.Admission
        ContinuousMemoryGuard=$snapshot
        StandardOutputLog=Get-FileState $Owned.StandardOutputLog
        StandardErrorLog=Get-FileState $Owned.StandardErrorLog
    }
}

function Wait-NativeMutationQuiescence {
    param([string] $Label, [int] $TimeoutSeconds=60)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $busy = @(Get-NativeMutatorProcesses)
        if ($busy.Count -eq 0 -and $null -eq $script:activeOwnedProcess) {
            return [pscustomobject] [ordered] @{
                Label=$Label; Status='QUIESCENT'; NativeMutatorProcessCount=0
            }
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "$Label did not reach exact native mutation quiescence."
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
        [int] $TimeoutSeconds
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
        }
        $owned = Start-GuardedOwnedProcess @startParameters
        Wait-GuardedOwnedProcess $owned $TimeoutSeconds
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
    [void] (Wait-NativeMutationQuiescence "after $Label" 60)
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

function Wait-RuntimeCaptureReady {
    param([int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = ''
    do {
        Start-Sleep -Seconds 2
        try {
            $response = Invoke-RcCall `
                'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
            if ($response.ReturnValue -eq $true -and
                ([string] $response.OutReport).Contains(
                    'ISTANA_EXPLORE_V5D_R31_PLAYER0_CAPTURE_STATE_VALID',
                    [StringComparison]::Ordinal)) {
                return $response
            }
            $lastError = [string] $response.OutReport
        }
        catch { $lastError = $_.Exception.Message }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "R31 runtime RC/state endpoint did not become ready: $lastError"
}

function Wait-StableFallbackPose {
    param(
        $Pose,
        [int] $TimeoutSeconds,
        [string] $RenderPathId='NANITE_ON'
    )
    $naniteValue = if ($RenderPathId -ceq 'NANITE_ON') { 1 } else { 0 }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $firstAcceptedUtc = $null
    $samples = [Collections.Generic.List[object]]::new()
    $lastReport = ''
    do {
        Start-Sleep -Seconds 2
        $response = Invoke-RcCall `
            'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
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
            $lastReport.Contains('r31BroadShellVisible=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('airSimTriadRuntimeLoaded=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('weatherActorResolved=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains("renderPath=$RenderPathId",
                [StringComparison]::Ordinal) -and
            $lastReport.Contains("r.Nanite=$naniteValue",
                [StringComparison]::Ordinal) -and
            $lastReport.Contains("r.Nanite.ProxyRenderMode=$naniteValue",
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('naniteMeshEnabled=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('naniteDataValid=true',
                [StringComparison]::Ordinal) -and
            $lastReport.Contains('fallbackPercentTriangles=1.0',
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
                    RenderPathId=$RenderPathId
                    NaniteConsoleValue=$naniteValue
                    NaniteProxyRenderMode=$naniteValue
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
    throw "R31 pose/render path did not hold exact ProviderFallback state for 12 seconds: pose=$($Pose.Id) renderPath=$RenderPathId last={$lastReport}"
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
        $sampledColors = [Collections.Generic.HashSet[int]]::new()
        $minimumLuminance = 255.0
        $maximumLuminance = 0.0
        for ($sampleY = 0; $sampleY -lt 36; ++$sampleY) {
            $pixelY = [Math]::Min(1439, 20 + ($sampleY * 40))
            for ($sampleX = 0; $sampleX -lt 64; ++$sampleX) {
                $pixelX = [Math]::Min(2559, 20 + ($sampleX * 40))
                $color = $decoded.GetPixel($pixelX, $pixelY)
                [void] $sampledColors.Add($color.ToArgb())
                $luminance = (0.2126 * $color.R) +
                    (0.7152 * $color.G) + (0.0722 * $color.B)
                $minimumLuminance = [Math]::Min(
                    $minimumLuminance, $luminance)
                $maximumLuminance = [Math]::Max(
                    $maximumLuminance, $luminance)
            }
        }
        $luminanceRange = $maximumLuminance - $minimumLuminance
        if ($sampledColors.Count -lt 8 -or $luminanceRange -lt 12.0) {
            throw "PNG decoded successfully but is blank/near-uniform: path=$Path sampledColors=$($sampledColors.Count) luminanceRange=$luminanceRange"
        }
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
            NonBlank=$true
            SampledDistinctColorCount=$sampledColors.Count
            SampledLuminanceRange=$luminanceRange
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

function Test-FileContainsByteSequence {
    param([string] $Path, [byte[]] $Needle)
    $stream = [IO.File]::Open(
        $Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite)
    try {
        $chunk = [byte[]]::new(1048576)
        $carry = [byte[]]::new(0)
        while (($read = $stream.Read($chunk, 0, $chunk.Length)) -gt 0) {
            $haystack = [byte[]]::new($carry.Length + $read)
            if ($carry.Length -gt 0) {
                [Array]::Copy($carry, 0, $haystack, 0, $carry.Length)
            }
            [Array]::Copy($chunk, 0, $haystack, $carry.Length, $read)
            for ($index=0; $index -le $haystack.Length-$Needle.Length; ++$index) {
                $match = $true
                for ($offset=0; $offset -lt $Needle.Length; ++$offset) {
                    if ($haystack[$index+$offset] -ne $Needle[$offset]) {
                        $match = $false
                        break
                    }
                }
                if ($match) { return $true }
            }
            $keep = [Math]::Min($Needle.Length-1, $haystack.Length)
            $carry = [byte[]]::new($keep)
            if ($keep -gt 0) {
                [Array]::Copy(
                    $haystack, $haystack.Length-$keep,
                    $carry, 0, $keep)
            }
        }
        $false
    }
    finally { $stream.Dispose() }
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

function Assert-NoFatalRuntimeLog {
    param([string] $Path, [string] $Label)
    $text = [IO.File]::ReadAllText($Path)
    foreach ($pattern in @(
        'Fatal error:',
        'LogWindows: Error:',
        'Missing cached shadermap',
        'Missing shader map',
        'Failed to load package',
        "Can't find file for asset",
        'R30 facade provider mirror failed closed',
        'R31_CAPTURE_STATE_INVALID')) {
        if ($text.Contains($pattern, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$Label contains fail-closed marker: $pattern"
        }
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

function Assert-R31ContextPolicySourceClosure {
    param($Receipt)
    $receiptRows = @(
        Get-RequiredPropertyValue `
            $Receipt 'NativeSourceClosureFiles' 'R31 commit receipt')
    if ($r31ContextPolicySourcePins.Count -ne 2 -or
        $receiptRows.Count -ne $r31ContextPolicySourcePins.Count -or
        [bool] (Get-RequiredPropertyValue `
            $Receipt 'R33SourceOrDeclarationAllowed' 'R31 commit receipt') -ne
                $false) {
        throw 'R31 commit receipt lacks the exact two-file pre-R33 ContextPolicy source closure.'
    }
    $accepted = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $r31ContextPolicySourcePins.Count; ++$index) {
        $pin = $r31ContextPolicySourcePins[$index]
        $row = $receiptRows[$index]
        $repositoryRelative = [string] (Get-RequiredPropertyValue `
            $row 'RepositoryRelativePath' "R31 source-closure row $index")
        $nativeRelative = [string] (Get-RequiredPropertyValue `
            $row 'NativeRelativePath' "R31 source-closure row $index")
        $receiptState = Get-RequiredPropertyValue `
            $row 'State' "R31 source-closure row $index"
        if ($repositoryRelative -cne $pin.RepositoryRelativePath -or
            $nativeRelative -cne $pin.NativeRelativePath -or
            [bool] (Get-RequiredPropertyValue `
                $receiptState 'Present' "R31 source-closure state $index") -ne
                    $true -or
            [int64] (Get-RequiredPropertyValue `
                $receiptState 'Bytes' "R31 source-closure state $index") -ne
                    [int64] $pin.Bytes -or
            [string] (Get-RequiredPropertyValue `
                $receiptState 'Sha256' "R31 source-closure state $index") -cne
                    [string] $pin.Sha256) {
            throw "R31 commit source-closure row $index does not match its frozen repository/native identity."
        }
        $expected = [pscustomobject] [ordered] @{
            Present=$true
            Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        }
        $repositoryPath = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RepositoryRelativePath))
        $nativePath = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $pin.NativeRelativePath))
        if (-not (Test-ContainedPath $repositoryPath $repositoryUnrealRoot) -or
            -not (Test-ContainedPath $nativePath $nativeProjectRoot)) {
            throw "R31 ContextPolicy source-closure row escaped its bounded root: $index"
        }
        [void] (Assert-State $expected $repositoryPath "repository R31 ContextPolicy source $index")
        [void] (Assert-State $expected $nativePath "native R31 ContextPolicy source $index")
        $accepted.Add([pscustomobject] [ordered] @{
            RepositoryRelativePath=$repositoryRelative
            NativeRelativePath=$nativeRelative
            State=$expected
        })
    }
    @($accepted)
}

function Assert-R31CommitReceipt {
    param([string] $Path, [string] $ExpectedSha256)
    if ([string]::IsNullOrWhiteSpace($Path) -or
        [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        throw 'Live capture requires a path and explicit SHA-256 for the committed R31 receipt.'
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $receiptDirectory = [IO.Path]::GetDirectoryName($fullPath)
    $receiptParent = [IO.Path]::GetDirectoryName($receiptDirectory)
    $receiptToken = [IO.Path]::GetFileName($receiptDirectory)
    if (-not [IO.Path]::GetFileName($fullPath).Equals(
            'commit.json', [StringComparison]::Ordinal) -or
        -not $receiptParent.Equals(
            $r31TransactionBase,
            [StringComparison]::OrdinalIgnoreCase) -or
        $receiptToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "R31 receipt must be the direct safe-token child commit.json below the R31 transaction base: $fullPath"
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or
        $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "R31 commit receipt hash mismatch: expected=$($ExpectedSha256.ToUpperInvariant()) actual=$($state.Sha256)"
    }
    $receipt = Get-Content -LiteralPath $fullPath -Raw |
        ConvertFrom-Json -Depth 32
    if ((Get-RequiredPropertyValue $receipt 'Schema' 'R31 commit receipt') -cne
            $r31TransactionSchema -or
        (Get-RequiredPropertyValue $receipt 'Status' 'R31 commit receipt') -cne
            'COMMITTED' -or
        (Get-RequiredPropertyValue $receipt 'RunToken' 'R31 commit receipt') -cne
            $receiptToken -or
        [int] (Get-RequiredPropertyValue $receipt 'R31ContentPackageCount' 'R31 commit receipt') -ne 5 -or
        [bool] (Get-RequiredPropertyValue $receipt 'R30FacadeLookdevAssetsModified' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'R30FacadeCueCoexists' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'R29FacadeEnvironmentAssetsModified' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'V2BroadShellMeshRetained' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'R28PublicRealmRetained' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'R29EnvironmentAssetsModified' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VegetationMutationAllowed' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeRealismValidated' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3PromotedAtR30' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Preserved' 'R31 commit receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeResponseMaterialPackageCount' 'R31 commit receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeDerivativeMeshPackageCount' 'R31 commit receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeRuntimeResponseMidCount' 'R31 commit receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreePlacementGeometryOpacityWindAuthorityModified' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TerrainR29Validated' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ContextPolicyShellValidatedBeforeAndAfter' 'R31 commit receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ContextTextureAssetsModified' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HeroMaterialsDependencyAllowed' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SimulationCollisionNavigationSensorRfAuthority' 'R31 commit receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $receipt 'ContractReferencedFileCount' 'R31 commit receipt') -ne 15 -or
        [int] (Get-RequiredPropertyValue $receipt 'PromotedContractReferencedFileCount' 'R31 commit receipt') -ne 4 -or
        [int] (Get-RequiredPropertyValue $receipt 'RetainedContractReferencedFileCount' 'R31 commit receipt') -ne 11 -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualCaptureAccepted' 'R31 commit receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CaptureRevalidationRequired' 'R31 commit receipt') -ne $true) {
        throw 'R31 commit receipt failed its exact R31 broad-shell/R30-owner/R29-inherited-state admission contract.'
    }
    $treeMaterialResponseV3 = Get-RequiredPropertyValue `
        $receipt 'TreeMaterialResponseV3' 'R31 commit receipt'
    $treeNativePins = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'NativeSourcePins' 'R31 TreeRealism v3 receipt')
    $treeClosurePins = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'SourceClosurePins' 'R31 TreeRealism v3 receipt')
    $treeBefore = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentBefore' 'R31 TreeRealism v3 receipt')
    $treeAfter = @(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentAfter' 'R31 TreeRealism v3 receipt')
    if ([string] (Get-RequiredPropertyValue $treeMaterialResponseV3 'Status' 'R31 TreeRealism v3 receipt') -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        $treeNativePins.Count -ne 4 -or $treeClosurePins.Count -ne 6 -or
        $treeAfter.Count -ne $treeBefore.Count + 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ResponseMaterialPackageCount' 'R31 TreeRealism v3 receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ReboundManagedMeshPackageCount' 'R31 TreeRealism v3 receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'RuntimeResponseMidCount' 'R31 TreeRealism v3 receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $treeMaterialResponseV3 'TreePlacementGeometryOpacityWindAuthorityModified' 'R31 TreeRealism v3 receipt') -ne $false) {
        throw 'R31 commit TreeRealism v3 receipt is not the exact promoted R30 closure.'
    }
    foreach ($pin in @($treeNativePins) + @($treeClosurePins)) {
        $relative = [string] (Get-RequiredPropertyValue $pin 'RelativePath' 'R31 TreeRealism v3 pin')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('..')) {
            throw "R31 TreeRealism v3 pin escaped its native root: $relative"
        }
        $pinPath = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $pinPath $nativeProjectRoot)) {
            throw "R31 TreeRealism v3 pin escaped its native root: $relative"
        }
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] (Get-RequiredPropertyValue $pin 'Bytes' 'R31 TreeRealism v3 pin')
            Sha256=[string] (Get-RequiredPropertyValue $pin 'Sha256' 'R31 TreeRealism v3 pin')
        }) $pinPath 'R31 retained TreeRealism v3 source pin')
    }
    Assert-JsonIdentity @($treeAfter) @(Get-TreeIdentityReceipt $treeRealismContentRoot) `
        'R31 commit/current TreeRealism v3 content' 8
    $successorMap = Get-RequiredPropertyValue `
        $receipt 'SuccessorMap' 'R31 commit receipt'
    $runtimeAfter = Get-RequiredPropertyValue `
        $receipt 'RuntimeDllAfter' 'R31 commit receipt'
    $editorAfter = Get-RequiredPropertyValue `
        $receipt 'EditorDllAfter' 'R31 commit receipt'
    [void] (Assert-State $successorMap $mapFile 'current R31 successor map')
    [void] (Assert-State $runtimeAfter $runtimeEditorDll 'current R31 runtime editor DLL')
    [void] (Assert-State $editorAfter $editorDll 'current R31 editor DLL')
    $sourceClosure = @(
        Assert-R31ContextPolicySourceClosure $receipt)
    [pscustomobject] [ordered] @{
        Path=$fullPath
        File=$state
        RunToken=$receiptToken
        SuccessorMap=$successorMap
        RuntimeDllAfter=$runtimeAfter
        EditorDllAfter=$editorAfter
        ContextPolicySourceClosure=@($sourceClosure)
        TreeMaterialResponseV3=$treeMaterialResponseV3
        R31ContentPackageCount=5
        R30Owner='EXACT_R30_FACADE_LOOKDEV'
        PredecessorVegetationOwner='R29'
        ContextPolicyShell='R31_BROAD_SHELL'
        VisualCaptureAccepted=$false
        CaptureRevalidationRequired=$true
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

function Assert-R31NativeContent {
    if (-not [IO.Directory]::Exists($r31ContentRoot)) {
        throw "R31 native content root is absent: $r31ContentRoot"
    }
    $actual = @(
        Get-ChildItem -LiteralPath $r31ContentRoot -File -Recurse |
            Sort-Object FullName)
    $relative = @($actual | ForEach-Object {
        [IO.Path]::GetRelativePath($r31ContentRoot, $_.FullName)
    })
    if ($actual.Count -ne 5 -or
        (ConvertTo-Json @($relative) -Compress) -cne
        (ConvertTo-Json @($r31ContentRelativePaths | Sort-Object) -Compress)) {
        throw 'Native R31 namespace is not the exact committed five-package roster.'
    }
    foreach ($file in $actual) {
        $state = Get-FileState $file.FullName
        if (-not $state.Present -or $state.Bytes -le 0 -or
            $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "Native R31 package lacks a non-empty SHA-256 identity: $($file.FullName)"
        }
    }
    @(Get-TreeReceipt $r31ContentRoot)
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
        Assert-FreshFileState "$cookedMapBase.umap" $CookStartedUtc 'cooked R31 successor map package'
        Assert-FreshFileState "$cookedMapBase.uexp" $CookStartedUtc 'cooked R31 successor map export'
    )

    $cookedR31Root = Join-Path $cookedContentRoot `
        'TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31'
    $cookedR31Uassets = @(
        Get-ChildItem -LiteralPath $cookedR31Root -File -Recurse `
            -Filter '*.uasset' -ErrorAction Stop |
            Sort-Object FullName)
    $cookedR31Relative = @($cookedR31Uassets | ForEach-Object {
        [IO.Path]::GetRelativePath($cookedR31Root, $_.FullName)
    })
    if ($cookedR31Uassets.Count -ne 5 -or
        (ConvertTo-Json @($cookedR31Relative) -Compress) -cne
        (ConvertTo-Json @($r31ContentRelativePaths | Sort-Object) -Compress)) {
        throw 'Cooked R31 namespace is not the exact five-package roster.'
    }
    $r31Receipt = [Collections.Generic.List[object]]::new()
    foreach ($asset in $cookedR31Uassets) {
        $r31Receipt.Add((Assert-FreshFileState `
            $asset.FullName $CookStartedUtc 'cooked R31 package'))
        $exportPath = [IO.Path]::ChangeExtension($asset.FullName, '.uexp')
        $r31Receipt.Add((Assert-FreshFileState `
            $exportPath $CookStartedUtc 'cooked R31 package export'))
    }

    $dependencyReceipts = [Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $requiredCookedDependencyRoots) {
        $root = [IO.Path]::GetFullPath(
            (Join-Path $cookedGameRoot $relativeRoot))
        $files = @(
            Get-ChildItem -LiteralPath $root -File -Recurse `
                -ErrorAction Stop | Sort-Object FullName)
        if ($files.Count -eq 0) {
            throw "Required cooked R31/R30/R29/V2/texture dependency root is empty: $root"
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
        R31Files=@($r31Receipt)
        R31PackageCount=5
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
    foreach ($state in @($Closure.R31Files)) { $states.Add($state) }
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
        'R31 Player0 human visual-review contract'
    $visualReviewContractJson = Get-Content -LiteralPath $visualReviewContract -Raw |
        ConvertFrom-Json -Depth 16
    if ([string] $visualReviewContractJson.schema -cne
            'triad.istana_explore_v5d.r31_player0_visual_review.v2' -or
        [string] $visualReviewContractJson.strictNativeOrder -cne
            'R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32' -or
        [bool] $visualReviewContractJson.automaticVisualAcceptanceAllowed -ne $false -or
        [string] $visualReviewContractJson.pendingReceipt.status -cne
            'PENDING_VISUAL_REVIEW' -or
        [bool] $visualReviewContractJson.pendingReceipt.humanVisualReviewAttested -ne $false -or
        [bool] $visualReviewContractJson.pendingReceipt.humanNaniteRasterComparisonAttested -ne $false -or
        [bool] $visualReviewContractJson.pendingReceipt.naniteRasterAppearanceParityAccepted -ne $false -or
        [string] $visualReviewContractJson.acceptedReceipt.status -cne 'COMMITTED' -or
        [bool] $visualReviewContractJson.acceptedReceipt.humanVisualReviewAttested -ne $true -or
        [bool] $visualReviewContractJson.acceptedReceipt.r31BroadShellVisualQaAccepted -ne $true -or
        [bool] $visualReviewContractJson.acceptedReceipt.confirmedNaniteRasterPairReviewed -ne $true -or
        [bool] $visualReviewContractJson.acceptedReceipt.humanNaniteRasterComparisonAttested -ne $true -or
        [bool] $visualReviewContractJson.acceptedReceipt.naniteRasterAppearanceParityAccepted -ne $true -or
        [int] $visualReviewContractJson.acceptedReceipt.exactImageCount -ne 6) {
        throw 'R31 Player0 human visual-review contract truth drifted.'
    }
    foreach ($pin in $r31ContextPolicySourcePins) {
        $expected = [pscustomobject] [ordered] @{
            Present=$true
            Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        }
        [void] (Assert-State $expected ([IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RepositoryRelativePath))) `
            'repository R31 ContextPolicy source closure')
    }
    Initialize-MemoryWatchdogType
    if ($poses.Count -ne 5 -or
        ([string]::Join('|', @($poses.Id))) -cne
            '075m|020m|008m|002m|surroundings_oblique_macdonald' -or
        @($poses.Id | Sort-Object -Unique).Count -ne 5 -or
        $r31ContentRelativePaths.Count -ne 5 -or
        $requiredCookedDependencyRoots.Count -ne 8 -or
        $r31ContextPolicySourcePins.Count -ne 2 -or
        $expectedGroundHeader.Bytes -ne 24562L -or
        $expectedGroundHeader.Sha256 -cne
            '599358BC0E6290DAB276890BF118A6E5EE59AD62888B847330A9A991B6E2BDE3' -or
        $expectedGroundSource.Bytes -ne 246667L -or
        $expectedGroundSource.Sha256 -cne
            '3F00D319112C3E7C7F1172FD128C71360C025B43B6BE2FB138A63DE07C1FDB17' -or
        $minimumSystemFreeVirtualAtLaunchBytes -ne 10737418240L -or
        $minimumSystemFreeVirtualBytes -ne 6442450944L -or
        $privateMemoryCeilingBytes -ne 12884901888L -or
        $memoryWatchdogPollMilliseconds -ne 500 -or
        $memoryWatchdogPersistentBreachMilliseconds -ne 2000) {
        throw 'R31 capture static roster or fixed memory guard drifted.'
    }
    $header = [IO.File]::ReadAllText(
        (Join-Path $repositoryUnrealRoot $captureSourcePins[0].RelativePath))
    $implementation = [IO.File]::ReadAllText(
        (Join-Path $repositoryUnrealRoot $captureSourcePins[1].RelativePath))
    foreach ($marker in @(
        'UBlueprintFunctionLibrary',
        'GetIstanaExploreV5DR31Player0CaptureState',
        'SetIstanaExploreV5DR31Player0CapturePose',
        'SetIstanaExploreV5DR31Player0CaptureRenderPath',
        'CaptureIstanaExploreV5DR31Player0FallbackView',
        'FinishIstanaExploreV5DR31Player0CaptureRun')) {
        if (-not ($header.Contains($marker, [StringComparison]::Ordinal) -or
                  $implementation.Contains($marker, [StringComparison]::Ordinal))) {
            throw "R31 runtime capture source lacks marker: $marker"
        }
    }
    foreach ($forbidden in @('GEditor', 'UnrealEd', 'R32', 'R33')) {
        if ($header.Contains($forbidden, [StringComparison]::Ordinal) -or
            $implementation.Contains($forbidden, [StringComparison]::Ordinal)) {
            throw "R31 runtime capture source contains forbidden dependency: $forbidden"
        }
    }
    [pscustomobject] [ordered] @{
        Schema=$schema
        Status='STATIC_SELF_CHECK_PASS'
        CaptureSourceCount=2
        ExactPoseCount=5
        ExactImageCount=6
        R31ContentPackageCount=5
        R30Owner='EXACT_R30_FACADE_LOOKDEV'
        ContextPolicyShell='R31_BROAD_SHELL'
        PredecessorVegetationOwner='R29'
        TreeMaterialResponseV3PreservationRequired=$true
        TreeResponseMaterialPackageCount=13
        TreeRuntimeResponseMidCount=26
        VisualReviewContract=$visualReviewContractState
        GroundHeader=$expectedGroundHeader
        GroundSource=$expectedGroundSource
        NativePluginSourceTreeRequired=$true
        TwoPhaseVisualReviewRequired=$true
        PendingCaptureStatus='PENDING_VISUAL_REVIEW'
        AcceptedCaptureStatus='COMMITTED'
        AutomaticVisualAcceptanceAllowed=$false
        ExplicitFiveImageReviewConfirmationRequired=$true
        ExplicitNaniteRasterPairReviewRequired=$true
        HumanVisualReviewAttestationRequired=$true
        ProviderFallbackOnly=$true
        ProviderReadyProofClaimed=$false
        AirSimTriadRuntimeRetained=$true
        FixedLaunchFreeVirtualBytes=10737418240L
        FixedContinuousFreeVirtualBytes=6442450944L
        FixedPrivateMemoryCeilingBytes=12884901888L
        MemoryWatchdogTypeCompiled=$true
        NativeProjectAccessed=$false
        NativeTreeWritten=$false
        UnrealLaunched=$false
        R32DependencyAllowed=$false
    }
}

function Assert-ExactFallbackStateResponse {
    param(
        $Response,
        $Pose,
        [string] $Label,
        [string] $RenderPathId='NANITE_ON'
    )
    $naniteValue = if ($RenderPathId -ceq 'NANITE_ON') { 1 } else { 0 }
    $report = [string] (Get-RequiredPropertyValue `
        $Response 'OutReport' $Label)
    if ((Get-RequiredPropertyValue $Response 'ReturnValue' $Label) -ne $true) {
        throw "$Label returned false: $report"
    }
    foreach ($marker in @(
        'ISTANA_EXPLORE_V5D_R31_PLAYER0_CAPTURE_STATE_VALID',
        "poseId=$($Pose.Id)",
        'exactQaViewPose=true',
        'contextPolicyShell=R31BroadShell',
        'r30Owner=1',
        'r29VegetationOwner=1',
        'r29TerrainOwner=1',
        'treeRealismOwner=1',
        'providerReadyForProof=false',
        'localFallbackHidden=false',
        'r30Visible=true',
        'r31BroadShellVisible=true',
        'providerFallbackVisualQa=true',
        'providerReadyProofClaimed=false',
        'airSimTriadRuntimeLoaded=true',
        'weatherActorResolved=true',
        "renderPath=$RenderPathId",
        "r.Nanite=$naniteValue",
        "r.Nanite.ProxyRenderMode=$naniteValue",
        'naniteMeshEnabled=true',
        'naniteDataValid=true',
        'naniteKeepPercentTriangles=1.0',
        'naniteTrimRelativeError=0.0',
        'fallbackTargetPercentTriangles=true',
        'fallbackPercentTriangles=1.0',
        'fallbackRelativeError=0.0',
        'visualCaptureAccepted=false',
        'captureRevalidationRequired=true')) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Label lacks exact ProviderFallback marker '$marker': $report"
        }
    }
    $report
}

function Assert-RenderPathResponse {
    param($Response, [string] $RenderPathId, [string] $Label)
    $naniteValue = if ($RenderPathId -ceq 'NANITE_ON') { 1 } else { 0 }
    $message = [string] (Get-RequiredPropertyValue `
        $Response 'OutMessage' $Label)
    if ((Get-RequiredPropertyValue $Response 'ReturnValue' $Label) -ne $true) {
        throw "$Label returned false: $message"
    }
    foreach ($marker in @(
        'ISTANA_EXPLORE_V5D_R31_RENDER_PATH_PASS',
        "renderPath=$RenderPathId",
        "r.Nanite=$naniteValue",
        "r.Nanite.ProxyRenderMode=$naniteValue",
        'transientProcessStateOnly=true',
        'mapModified=false',
        'simulationSensorRfModified=false')) {
        if (-not $message.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Label lacks exact render-path marker '$marker': $message"
        }
    }
    $message
}

function Assert-CaptureImmutableState {
    param($Before, $R31Admission, $R31ReceiptFileBefore)
    $currentProject = Assert-AirSimProjectConfiguration
    [void] (Assert-State `
        $Before.ProjectFile $nativeProjectFile 'native project descriptor')
    [void] (Assert-State `
        $Before.AirSimDescriptor $airSimRuntimeDescriptor `
        'AirSimTriadRuntime descriptor')
    [void] (Assert-State `
        $R31Admission.SuccessorMap $mapFile 'R31 successor map')
    [void] (Assert-State `
        $R31Admission.RuntimeDllAfter $runtimeEditorDll `
        'R31 runtime editor DLL')
    [void] (Assert-State `
        $R31Admission.EditorDllAfter $editorDll 'R31 editor DLL')
    [void] (Assert-State `
        $R31ReceiptFileBefore $R31Admission.Path 'R31 source commit receipt')
    [void] (Assert-TreeReceipt `
        $r31ContentRoot @($Before.R31) 'R31 content')
    [void] (Assert-TreeReceipt `
        $r30ContentRoot @($Before.R30) 'R30 facade-lookdev content')
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
        @($R31Admission.TreeMaterialResponseV3.ContentAfter) `
        @(Get-TreeIdentityReceipt $treeRealismContentRoot) `
        'R31 capture retained R30 TreeRealism v3 content' 8
    [void] (Assert-TreeReceipt `
        $contextFacadeR25ContentRoot @($Before.ContextFacadeR25) `
        'retained R25 material dependency content')
    [void] (Assert-TreeReceipt `
        $localFallbackSuppressionV2ContentRoot `
        @($Before.LocalFallbackSuppressionV2) 'R31 exact V2 shell content')
    [void] (Assert-TreeReceipt `
        $contextTextureRoot @($Before.ContextTextures) `
        'R31 source-admitted context texture content')
    [void] (Assert-TreeReceipt `
        $airSimRuntimeContentRoot @($Before.AirSimContent) `
        'AirSimTriadRuntime content')
    Assert-CaptureSourcePins $nativeProjectRoot
    foreach ($pin in $r31ContextPolicySourcePins) {
        $expected = [pscustomobject] [ordered] @{
            Present=$true; Bytes=[int64] $pin.Bytes
            Sha256=[string] $pin.Sha256
        }
        [void] (Assert-State $expected ([IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RepositoryRelativePath))) `
            'repository R31 ContextPolicy source')
        [void] (Assert-State $expected ([IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $pin.NativeRelativePath))) `
            'native R31 ContextPolicy source')
    }
    [pscustomobject] [ordered] @{
        Status='UNCHANGED'
        Project=$currentProject
        R31Map=$R31Admission.SuccessorMap
        R31RuntimeEditorDll=$R31Admission.RuntimeDllAfter
        R31EditorDll=$R31Admission.EditorDllAfter
        R31ContentPackageCount=5
        R30FacadeCueRetained=$true
        R29VegetationRetained=$true
        R29TerrainRetained=$true
        TreeRealismRetained=$true
        TreeMaterialResponseV3Retained=$true
        TreeResponseMaterialPackageCount=13
        TreeRuntimeResponseMidCount=26
        R31BroadShellRetained=$true
        V2BroadShellMeshRetained=$true
        ContextPolicySourceClosureHashPinned=$true
        AirSimTriadRuntimeRetained=$true
        RuntimeCaptureSourcesHashPinned=$true
    }
}

function Assert-R31PendingCaptureReceipt {
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
        throw "Pending receipt must be the exact direct run-token child below the R31 evidence base: $fullPath"
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or
        $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "Pending R31 capture receipt hash mismatch: expected=$($ExpectedSha256.ToUpperInvariant()) actual=$($state.Sha256)"
    }
    $receipt = Get-Content -LiteralPath $fullPath -Raw |
        ConvertFrom-Json -Depth 64
    if ((Get-RequiredPropertyValue $receipt 'Schema' 'pending R31 capture receipt') -cne
            $schema -or
        (Get-RequiredPropertyValue $receipt 'Status' 'pending R31 capture receipt') -cne
            'PENDING_VISUAL_REVIEW' -or
        (Get-RequiredPropertyValue $receipt 'RunToken' 'pending R31 capture receipt') -cne
            $ExpectedRunToken -or
        (Get-RequiredPropertyValue $receipt 'NativeOrder' 'pending R31 capture receipt') -cne
            'R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32' -or
        [bool] (Get-RequiredPropertyValue $receipt 'R32DependencyAllowed' 'pending R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'MechanicalCaptureValidationPassed' 'pending R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanVisualReviewAttested' 'pending R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewRequired' 'pending R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewAccepted' 'pending R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Preserved' 'pending R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'R32AdmissionAuthorized' 'pending R31 capture receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactPoseCount' 'pending R31 capture receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactImageCount' 'pending R31 capture receipt') -ne 6 -or
        [bool] (Get-RequiredPropertyValue $receipt 'ConfirmedNaniteRasterPairReviewed' 'pending R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanNaniteRasterComparisonAttested' 'pending R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NaniteRasterAppearanceParityAccepted' 'pending R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NaniteConsoleStateRestored' 'pending R31 capture receipt') -ne $true) {
        throw 'Pending R31 capture receipt is not the exact unaccepted mechanical-capture result.'
    }
    foreach ($acceptedOnlyProperty in @(
        'GroundHeader',
        'GroundSource',
        'NativePluginSourceTree',
        'R31BroadShellVisualQaAccepted',
        'MapModifiedByCapture',
        'SimulationCollisionNavigationSensorRfModified')) {
        if ($null -ne $receipt.PSObject.Properties[$acceptedOnlyProperty]) {
            throw "Pending receipt illegally exposes accepted-only R32 field: $acceptedOnlyProperty"
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
                $receipt $truthRow.Name 'pending R31 capture receipt') -ne
            [bool] $truthRow.Expected) {
            throw "Pending R31 capture truth field drifted: $($truthRow.Name)"
        }
    }
    $noMutation = Get-RequiredPropertyValue `
        $receipt 'MechanicalNoMutationEvidence' 'pending R31 capture receipt'
    if ([bool] (Get-RequiredPropertyValue `
            $noMutation 'MapUnchanged' 'pending R31 no-mutation evidence') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $noMutation 'SimulationCollisionNavigationSensorRfUnchanged' `
            'pending R31 no-mutation evidence') -ne $true) {
        throw 'Pending R31 capture no-mutation evidence is not exact.'
    }
    $staticEvidence = Get-RequiredPropertyValue `
        $receipt 'StaticReceipt' 'pending R31 capture receipt'
    if ((Get-RequiredPropertyValue `
            $staticEvidence 'Status' 'pending R31 static receipt') -cne
                'STATIC_SELF_CHECK_PASS' -or
        [bool] (Get-RequiredPropertyValue `
            $staticEvidence 'TwoPhaseVisualReviewRequired' `
            'pending R31 static receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $staticEvidence 'AutomaticVisualAcceptanceAllowed' `
            'pending R31 static receipt') -ne $false) {
        throw 'Pending R31 capture does not embed the exact two-phase static receipt.'
    }
    $postflightEvidence = Get-RequiredPropertyValue `
        $receipt 'Postflight' 'pending R31 capture receipt'
    if ((Get-RequiredPropertyValue `
            $postflightEvidence 'Status' 'pending R31 postflight') -cne
                'UNCHANGED' -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'R30FacadeCueRetained' `
            'pending R31 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'R29VegetationRetained' `
            'pending R31 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'R29TerrainRetained' `
            'pending R31 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'TreeRealismRetained' `
            'pending R31 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'TreeMaterialResponseV3Retained' `
            'pending R31 postflight') -ne $true -or
        [int] (Get-RequiredPropertyValue `
            $postflightEvidence 'TreeResponseMaterialPackageCount' `
            'pending R31 postflight') -ne 13 -or
        [int] (Get-RequiredPropertyValue `
            $postflightEvidence 'TreeRuntimeResponseMidCount' `
            'pending R31 postflight') -ne 26 -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'R31BroadShellRetained' `
            'pending R31 postflight') -ne $true -or
        [bool] (Get-RequiredPropertyValue `
            $postflightEvidence 'V2BroadShellMeshRetained' `
            'pending R31 postflight') -ne $true) {
        throw 'Pending R31 capture postflight does not preserve the exact R31/R30/R29 scene.'
    }
    $cookedPostflightEvidence = Get-RequiredPropertyValue `
        $receipt 'CookedClosurePostflight' 'pending R31 capture receipt'
    if ((Get-RequiredPropertyValue `
            $cookedPostflightEvidence 'Status' `
            'pending R31 cooked-closure postflight') -cne 'UNCHANGED') {
        throw 'Pending R31 cooked closure was not revalidated unchanged.'
    }

    $r31Commit = Get-RequiredPropertyValue `
        $receipt 'R31CommitAdmission' 'pending R31 capture receipt'
    $r31CommitPath = [string] (Get-RequiredPropertyValue `
        $r31Commit 'Path' 'pending R31 commit admission')
    $r31CommitFile = Get-RequiredPropertyValue `
        $r31Commit 'File' 'pending R31 commit admission'
    $currentR31Admission = Assert-R31CommitReceipt `
        $r31CommitPath ([string] (Get-RequiredPropertyValue `
            $r31CommitFile 'Sha256' 'pending R31 commit receipt file'))

    $binding = Get-RequiredPropertyValue `
        $receipt 'BindingEvidence' 'pending R31 capture receipt'
    $mapBinding = Get-RequiredPropertyValue `
        $binding 'MapCaptureBinding' 'pending R31 binding evidence'
    $runtimeBinding = Get-RequiredPropertyValue `
        $binding 'RuntimeEditorDllCaptureBinding' 'pending R31 binding evidence'
    $editorBinding = Get-RequiredPropertyValue `
        $binding 'EditorDllCaptureBinding' 'pending R31 binding evidence'
    [void] (Assert-ExactStateBinding `
        $mapBinding $mapFile 'pending/current R31 map binding')
    [void] (Assert-ExactStateBinding `
        $runtimeBinding $runtimeEditorDll 'pending/current R31 runtime DLL binding')
    [void] (Assert-ExactStateBinding `
        $editorBinding $editorDll 'pending/current R31 editor DLL binding')
    Assert-JsonIdentity `
        $currentR31Admission.SuccessorMap `
        (Get-RequiredPropertyValue $mapBinding 'State' 'pending R31 map binding') `
        'R31 transaction/capture map binding'
    Assert-JsonIdentity `
        $currentR31Admission.RuntimeDllAfter `
        (Get-RequiredPropertyValue $runtimeBinding 'State' 'pending R31 runtime DLL binding') `
        'R31 transaction/capture runtime DLL binding'
    Assert-JsonIdentity `
        $currentR31Admission.EditorDllAfter `
        (Get-RequiredPropertyValue $editorBinding 'State' 'pending R31 editor DLL binding') `
        'R31 transaction/capture editor DLL binding'

    $groundHeaderBinding = Get-RequiredPropertyValue `
        $binding 'GroundHeaderCaptureBinding' 'pending R31 binding evidence'
    $groundSourceBinding = Get-RequiredPropertyValue `
        $binding 'GroundSourceCaptureBinding' 'pending R31 binding evidence'
    foreach ($groundRow in @(
        [pscustomobject] @{ Receipt=$groundHeaderBinding; Path=$groundHeader; Expected=$expectedGroundHeader; Label='Ground header capture binding' },
        [pscustomobject] @{ Receipt=$groundSourceBinding; Path=$groundSource; Expected=$expectedGroundSource; Label='Ground source capture binding' })) {
        if ([bool] (Get-RequiredPropertyValue `
                $groundRow.Receipt 'Unchanged' $groundRow.Label) -ne $true) {
            throw "$($groundRow.Label) is not marked unchanged."
        }
        [void] (Assert-State $groundRow.Expected $groundRow.Path $groundRow.Label)
        [void] (Assert-ExactPathBoundReceipt `
            $groundRow.Receipt $groundRow.Path $groundRow.Label)
        [void] (Assert-ExactPathBoundReceipt `
            (Get-RequiredPropertyValue $groundRow.Receipt `
                'ImmediatePreCapture' $groundRow.Label) `
            $groundRow.Path "$($groundRow.Label) immediate pre-capture")
        [void] (Assert-ExactPathBoundReceipt `
            (Get-RequiredPropertyValue $groundRow.Receipt `
                'ImmediatePostCapture' $groundRow.Label) `
            $groundRow.Path "$($groundRow.Label) immediate post-capture")
    }

    $sourceTreeBinding = Get-RequiredPropertyValue `
        $binding 'NativePluginSourceTreeCaptureBinding' `
        'pending R31 binding evidence'
    $sourceTreeRoot = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $sourceTreeBinding 'Root' `
            'native plugin source-tree capture binding'))
    if (-not $sourceTreeRoot.Equals(
            $nativePluginSourceRoot,
            [StringComparison]::OrdinalIgnoreCase) -or
        [bool] (Get-RequiredPropertyValue $sourceTreeBinding 'Unchanged' `
            'native plugin source-tree capture binding') -ne $true) {
        throw 'Pending native plugin source-tree root or unchanged marker is invalid.'
    }
    $sourceTreeBaseline = @(
        Get-RequiredPropertyValue $sourceTreeBinding `
            'BaselineAfterCaptureSourcePromotion' `
            'native plugin source-tree capture binding')
    $sourceTreePre = @(
        Get-RequiredPropertyValue $sourceTreeBinding 'ImmediatePreCapture' `
            'native plugin source-tree capture binding')
    $sourceTreePost = @(
        Get-RequiredPropertyValue $sourceTreeBinding 'ImmediatePostCapture' `
            'native plugin source-tree capture binding')
    if ($sourceTreeBaseline.Count -eq 0 -or
        [int] (Get-RequiredPropertyValue $sourceTreeBinding 'FileCount' `
            'native plugin source-tree capture binding') -ne
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
    [void] (Assert-R31NativeContent)

    $captures = @(
        Get-RequiredPropertyValue $receipt 'Captures' 'pending R31 capture receipt')
    if ($captures.Count -ne 5) {
        throw "Pending R31 capture receipt has $($captures.Count) captures instead of five."
    }
    $fileHashes = [Collections.Generic.List[string]]::new()
    $pixelHashes = [Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $poses.Count; ++$index) {
        $expectedPose = $poses[$index]
        $capture = $captures[$index]
        $pose = Get-RequiredPropertyValue `
            $capture 'Pose' "pending capture row $index"
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
                $capture 'ProviderReadyProofClaimed' "pending capture row $index") -ne $false -or
            [string] (Get-RequiredPropertyValue `
                $capture 'RenderPathId' "pending capture row $index") -cne 'NANITE_ON' -or
            [int] (Get-RequiredPropertyValue `
                $capture 'NaniteConsoleValue' "pending capture row $index") -ne 1 -or
            [int] (Get-RequiredPropertyValue `
                $capture 'NaniteProxyRenderMode' "pending capture row $index") -ne 1) {
            throw "Pending capture row $index does not preserve ProviderFallback truth."
        }
        foreach ($stateProperty in @(
            'ImmediatePreCaptureState', 'ImmediatePostCaptureState')) {
            $report = [string] (Get-RequiredPropertyValue `
                $capture $stateProperty "pending capture row $index")
            foreach ($marker in @(
                "poseId=$($expectedPose.Id)",
                'contextPolicyShell=R31BroadShell',
                'r30Owner=1',
                'r29VegetationOwner=1',
                'r29TerrainOwner=1',
                'treeRealismOwner=1',
                'providerFallbackVisualQa=true',
                'providerReadyProofClaimed=false',
                'airSimTriadRuntimeLoaded=true',
                'weatherActorResolved=true',
                'renderPath=NANITE_ON',
                'r.Nanite=1',
                'r.Nanite.ProxyRenderMode=1',
                'naniteMeshEnabled=true',
                'naniteDataValid=true',
                'fallbackPercentTriangles=1.0',
                'localFallbackHidden=false',
                'r30Visible=true',
                'r31BroadShellVisible=true')) {
                if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
                    throw "Pending capture row $index lacks runtime marker '$marker'."
                }
            }
        }
        $imageReceipt = Get-RequiredPropertyValue `
            $capture 'Image' "pending capture row $index"
        $expectedImagePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            "explore_v5d_r31_player0_$($expectedPose.Id)_$ExpectedRunToken.png"))
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
                $imageReceipt 'HeightPixels' "pending capture image $index") -ne 1440 -or
            [bool] (Get-RequiredPropertyValue `
                $imageReceipt 'NonBlank' "pending capture image $index") -ne $true -or
            [int] (Get-RequiredPropertyValue `
                $imageReceipt 'SampledDistinctColorCount' "pending capture image $index") -ne
                [int] $decoded.SampledDistinctColorCount -or
            [double] (Get-RequiredPropertyValue `
                $imageReceipt 'SampledLuminanceRange' "pending capture image $index") -ne
                [double] $decoded.SampledLuminanceRange) {
            throw "Pending capture image $index mechanical decode/nonblank binding changed."
        }
        $fileHashes.Add([string] $decoded.Sha256)
        $pixelHashes.Add([string] $decoded.DecodedBgraSha256)
    }
    if (@($fileHashes | Sort-Object -Unique).Count -ne 5 -or
        @($pixelHashes | Sort-Object -Unique).Count -ne 5) {
        throw 'Pending R31 capture PNG file/pixel identities are not five-way distinct.'
    }

    $comparison = Get-RequiredPropertyValue `
        $receipt 'NaniteRasterHighOccupancyComparison' `
        'pending R31 capture receipt'
    if ([bool] (Get-RequiredPropertyValue $comparison 'Required' `
            'Nanite/raster comparison') -ne $true -or
        [string] (Get-RequiredPropertyValue $comparison 'PoseId' `
            'Nanite/raster comparison') -cne 'surroundings_oblique_macdonald' -or
        [bool] (Get-RequiredPropertyValue $comparison 'SamePose' `
            'Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameGameProcess' `
            'Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameCookedClosure' `
            'Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'MechanicalValidationPassed' `
            'Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison `
            'HumanNaniteRasterComparisonAttested' `
            'Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison `
            'NaniteRasterAppearanceParityAccepted' `
            'Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison `
            'AutomaticVisualAcceptanceAllowed' `
            'Nanite/raster comparison') -ne $false) {
        throw 'Pending Nanite/raster comparison truth is not fail-closed.'
    }
    $naniteSide = Get-RequiredPropertyValue `
        $comparison 'NaniteOn' 'Nanite/raster comparison'
    Assert-JsonIdentity `
        (Get-RequiredPropertyValue $naniteSide 'Capture' `
            'Nanite comparison side') `
        $captures[4] 'Nanite comparison baseline capture' 16
    if ([string] (Get-RequiredPropertyValue $naniteSide 'RenderPathId' `
            'Nanite comparison side') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $naniteSide 'NaniteConsoleValue' `
            'Nanite comparison side') -ne 1 -or
        [int] (Get-RequiredPropertyValue $naniteSide 'NaniteProxyRenderMode' `
            'Nanite comparison side') -ne 1) {
        throw 'Pending Nanite comparison side lacks exact CVar readback.'
    }

    $rasterSide = Get-RequiredPropertyValue `
        $comparison 'RasterFallback' 'Nanite/raster comparison'
    $rasterPose = Get-RequiredPropertyValue `
        $rasterSide 'Pose' 'raster-fallback comparison side'
    $expectedComparisonPose = $poses[4]
    if ([string] (Get-RequiredPropertyValue $rasterPose 'Id' `
            'raster comparison pose') -cne $expectedComparisonPose.Id -or
        [string] (Get-RequiredPropertyValue $rasterSide 'RenderPathId' `
            'raster comparison side') -cne 'RASTER_FALLBACK' -or
        [int] (Get-RequiredPropertyValue $rasterSide 'NaniteConsoleValue' `
            'raster comparison side') -ne 0 -or
        [int] (Get-RequiredPropertyValue $rasterSide 'NaniteProxyRenderMode' `
            'raster comparison side') -ne 0) {
        throw 'Pending raster comparison side lacks exact pose or CVar readback.'
    }
    foreach ($coordinate in @('X', 'Y', 'Z', 'Pitch', 'Yaw', 'Roll')) {
        if ([double] (Get-RequiredPropertyValue $rasterPose $coordinate `
                'raster comparison pose') -ne
            [double] $expectedComparisonPose.$coordinate) {
            throw "Pending raster comparison pose coordinate drifted: $coordinate"
        }
    }
    foreach ($stateProperty in @(
        'ImmediatePreCaptureState', 'ImmediatePostCaptureState')) {
        $stateReport = [string] (Get-RequiredPropertyValue `
            $rasterSide $stateProperty 'raster comparison side')
        foreach ($marker in @(
            'poseId=surroundings_oblique_macdonald',
            'exactQaViewPose=true',
            'providerFallbackVisualQa=true',
            'renderPath=RASTER_FALLBACK',
            'r.Nanite=0',
            'r.Nanite.ProxyRenderMode=0',
            'naniteMeshEnabled=true',
            'naniteDataValid=true',
            'fallbackPercentTriangles=1.0')) {
            if (-not $stateReport.Contains($marker, [StringComparison]::Ordinal)) {
                throw "Raster comparison state lacks marker '$marker'."
            }
        }
    }
    $rasterImage = Get-RequiredPropertyValue `
        $rasterSide 'Image' 'raster comparison side'
    $expectedRasterPath = [IO.Path]::GetFullPath((Join-Path `
        (Join-Path $evidenceRoot 'captures') `
        "explore_v5d_r31_player0_surroundings_oblique_macdonald_${ExpectedRunToken}_raster_fallback.png"))
    $actualRasterPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $rasterImage 'Path' 'raster comparison image'))
    if (-not $actualRasterPath.Equals(
            $expectedRasterPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Raster comparison image path is noncanonical: $actualRasterPath"
    }
    [void] (Assert-State `
        $rasterImage $expectedRasterPath 'pending raster comparison PNG')
    $decodedRaster = Get-PngReceipt $expectedRasterPath
    foreach ($identityProperty in @('Sha256', 'DecodedBgraSha256')) {
        if ([string] (Get-RequiredPropertyValue $rasterImage $identityProperty `
                'raster comparison image') -cne
            [string] $decodedRaster.$identityProperty) {
            throw "Raster comparison image $identityProperty changed."
        }
    }
    if ([int] (Get-RequiredPropertyValue $rasterImage 'WidthPixels' `
            'raster comparison image') -ne 2560 -or
        [int] (Get-RequiredPropertyValue $rasterImage 'HeightPixels' `
            'raster comparison image') -ne 1440 -or
        [bool] (Get-RequiredPropertyValue $rasterImage 'NonBlank' `
            'raster comparison image') -ne $true) {
        throw 'Raster comparison image failed exact decode/nonblank validation.'
    }
    $restore = Get-RequiredPropertyValue `
        $comparison 'NaniteRestore' 'Nanite/raster comparison'
    $restoreState = [string] (Get-RequiredPropertyValue `
        $restore 'State' 'Nanite restore evidence')
    if ([bool] (Get-RequiredPropertyValue $restore 'Restored' `
            'Nanite restore evidence') -ne $true -or
        [string] (Get-RequiredPropertyValue $restore 'RenderPathId' `
            'Nanite restore evidence') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteConsoleValue' `
            'Nanite restore evidence') -ne 1 -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteProxyRenderMode' `
            'Nanite restore evidence') -ne 1 -or
        -not $restoreState.Contains(
            'renderPath=NANITE_ON', [StringComparison]::Ordinal) -or
        -not $restoreState.Contains(
            'r.Nanite=1', [StringComparison]::Ordinal) -or
        -not $restoreState.Contains(
            'r.Nanite.ProxyRenderMode=1', [StringComparison]::Ordinal)) {
        throw 'Pending comparison does not prove exact Nanite CVar restoration.'
    }

    [pscustomobject] [ordered] @{
        Path=$fullPath
        File=$state
        EvidenceRoot=$evidenceRoot
        Receipt=$receipt
        BindingEvidence=$binding
        CurrentR31Admission=$currentR31Admission
        CaptureCount=5
        ImageCount=6
        NaniteRasterComparison=$comparison
        MechanicalValidationReplayed=$true
        VisualReviewAccepted=$false
    }
}

function Get-R31AcceptanceNativeBoundary {
    param($Binding)
    $mapBinding = Get-RequiredPropertyValue `
        $Binding 'MapCaptureBinding' 'acceptance binding evidence'
    $runtimeBinding = Get-RequiredPropertyValue `
        $Binding 'RuntimeEditorDllCaptureBinding' 'acceptance binding evidence'
    $editorBinding = Get-RequiredPropertyValue `
        $Binding 'EditorDllCaptureBinding' 'acceptance binding evidence'
    $groundHeaderBinding = Get-RequiredPropertyValue `
        $Binding 'GroundHeaderCaptureBinding' 'acceptance binding evidence'
    $groundSourceBinding = Get-RequiredPropertyValue `
        $Binding 'GroundSourceCaptureBinding' 'acceptance binding evidence'
    $sourceTreeBinding = Get-RequiredPropertyValue `
        $Binding 'NativePluginSourceTreeCaptureBinding' `
        'acceptance binding evidence'
    [void] (Assert-ExactStateBinding `
        $mapBinding $mapFile 'acceptance R31 map')
    [void] (Assert-ExactStateBinding `
        $runtimeBinding $runtimeEditorDll 'acceptance R31 runtime DLL')
    [void] (Assert-ExactStateBinding `
        $editorBinding $editorDll 'acceptance R31 editor DLL')
    [void] (Assert-ExactPathBoundReceipt `
        $groundHeaderBinding $groundHeader 'acceptance Ground header')
    [void] (Assert-ExactPathBoundReceipt `
        $groundSourceBinding $groundSource 'acceptance Ground source')
    $expectedTree = @(
        Get-RequiredPropertyValue $sourceTreeBinding 'ImmediatePostCapture' `
            'acceptance native plugin source tree')
    $currentTree = @(
        Assert-CaptureSourceTreeSnapshot `
            $expectedTree 'acceptance native plugin source tree')
    [pscustomobject] [ordered] @{
        Map=Get-FileState $mapFile
        RuntimeEditorDll=Get-FileState $runtimeEditorDll
        EditorDll=Get-FileState $editorDll
        GroundHeader=Get-PathBoundFileReceipt `
            $groundHeader $groundHeaderBinding 'acceptance Ground header boundary'
        GroundSource=Get-PathBoundFileReceipt `
            $groundSource $groundSourceBinding 'acceptance Ground source boundary'
        NativePluginSourceTree=@($currentTree)
    }
}

function Invoke-R31VisualReviewAcceptance {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $ExpectedRunToken
    )
    Assert-NativeIdle 'before explicit R31 visual-review acceptance'
    $protectedBefore = @(Get-ProtectedProcesses)
    Assert-ProtectedUnchanged $protectedBefore
    $pendingAdmission = Assert-R31PendingCaptureReceipt `
        $Path $ExpectedSha256 $ExpectedRunToken
    $pending = $pendingAdmission.Receipt
    $binding = $pendingAdmission.BindingEvidence
    $nativeBoundaryBefore = Get-R31AcceptanceNativeBoundary $binding
    $nativeBoundaryAfter = Get-R31AcceptanceNativeBoundary $binding
    Assert-JsonIdentity `
        $nativeBoundaryBefore $nativeBoundaryAfter `
        'native state across explicit visual acceptance' 10
    [void] (Assert-State `
        $pendingAdmission.File $pendingAdmission.Path `
        'hash-pinned pending receipt before acceptance publication')
    Assert-ProtectedUnchanged $protectedBefore
    Assert-NativeIdle 'before explicit R31 visual acceptance publication'

    $acceptedReceipt = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='COMMITTED'
        RunToken=$ExpectedRunToken
        NativeOrder='R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32'
        R32DependencyAllowed=$false
        PendingCaptureAdmission=[pscustomobject] [ordered] @{
            Path=$pendingAdmission.Path
            File=$pendingAdmission.File
            CallerSha256=$ExpectedSha256.ToUpperInvariant()
            Status='PENDING_VISUAL_REVIEW'
        }
        StaticReceipt=(Get-RequiredPropertyValue `
            $pending 'StaticReceipt' 'pending R31 capture receipt')
        PrewriteAdmission=(Get-RequiredPropertyValue `
            $pending 'PrewriteAdmission' 'pending R31 capture receipt')
        R31CommitAdmission=(Get-RequiredPropertyValue `
            $pending 'R31CommitAdmission' 'pending R31 capture receipt')
        Map=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'MapCaptureBinding' `
                'pending R31 binding evidence') `
            'State' 'pending R31 map binding')
        RuntimeEditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'RuntimeEditorDllCaptureBinding' `
                'pending R31 binding evidence') `
            'State' 'pending R31 runtime DLL binding')
        EditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'EditorDllCaptureBinding' `
                'pending R31 binding evidence') `
            'State' 'pending R31 editor DLL binding')
        GroundHeader=(Get-RequiredPropertyValue `
            $binding 'GroundHeaderCaptureBinding' 'pending R31 binding evidence')
        GroundSource=(Get-RequiredPropertyValue `
            $binding 'GroundSourceCaptureBinding' 'pending R31 binding evidence')
        NativePluginSourceTree=(Get-RequiredPropertyValue `
            $binding 'NativePluginSourceTreeCaptureBinding' `
            'pending R31 binding evidence')
        SourcePromotion=(Get-RequiredPropertyValue `
            $pending 'SourcePromotion' 'pending R31 capture receipt')
        GameBinaryBefore=(Get-RequiredPropertyValue `
            $pending 'GameBinaryBefore' 'pending R31 capture receipt')
        GameBinaryAfter=(Get-RequiredPropertyValue `
            $pending 'GameBinaryAfter' 'pending R31 capture receipt')
        Build=(Get-RequiredPropertyValue `
            $pending 'Build' 'pending R31 capture receipt')
        Cook=(Get-RequiredPropertyValue `
            $pending 'Cook' 'pending R31 capture receipt')
        CookedClosure=(Get-RequiredPropertyValue `
            $pending 'CookedClosure' 'pending R31 capture receipt')
        CookedClosurePostflight=(Get-RequiredPropertyValue `
            $pending 'CookedClosurePostflight' 'pending R31 capture receipt')
        Game=(Get-RequiredPropertyValue `
            $pending 'Game' 'pending R31 capture receipt')
        FinishTransportErrorAfterExit=(Get-RequiredPropertyValue `
            $pending 'FinishTransportErrorAfterExit' 'pending R31 capture receipt')
        Captures=(Get-RequiredPropertyValue `
            $pending 'Captures' 'pending R31 capture receipt')
        NaniteRasterHighOccupancyComparison=(Get-RequiredPropertyValue `
            $pending 'NaniteRasterHighOccupancyComparison' `
            'pending R31 capture receipt')
        ExactPoseCount=5
        ExactImageCount=6
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$true
        HumanVisualReviewAttested=$true
        ConfirmedFiveImagesReviewed=$true
        ConfirmedNaniteRasterPairReviewed=$true
        HumanNaniteRasterComparisonAttested=$true
        NaniteRasterAppearanceParityAccepted=$true
        AutomaticVisualAcceptanceAllowed=$false
        VisualReviewRequired=$false
        VisualReviewAccepted=$true
        R31BroadShellVisualQaAccepted=$true
        ProviderFallbackVisualQaAccepted=$true
        TreeMaterialResponseV3Preserved=$true
        R32AdmissionAuthorized=$true
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
            SixPngsRedecodedAndRehashed=$true
            TreeMaterialResponseV3SourceAndContentClosureRevalidated=$true
            NativeReceiptWriteAllowed=$false
        }
        Postflight=(Get-RequiredPropertyValue `
            $pending 'Postflight' 'pending R31 capture receipt')
        AcceptedUtc=[DateTime]::UtcNow.ToString('o')
    }

    $evidenceRoot = $pendingAdmission.EvidenceRoot
    $candidatePath = Join-Path $evidenceRoot ".commit.candidate.$PID.json"
    $commitPath = Join-Path $evidenceRoot 'commit.json'
    if ([IO.File]::Exists($candidatePath) -or [IO.File]::Exists($commitPath)) {
        throw 'Explicit R31 visual acceptance refuses an existing candidate or commit receipt.'
    }
    try {
        [void] (Write-JsonAtomic `
            $acceptedReceipt $candidatePath $evidenceRoot)
        $nativeBoundaryAfterCandidateWrite = `
            Get-R31AcceptanceNativeBoundary $binding
        Assert-JsonIdentity `
            $nativeBoundaryBefore $nativeBoundaryAfterCandidateWrite `
            'native state after bounded acceptance-candidate evidence write' 10
        [void] (Assert-State `
            $pendingAdmission.File $pendingAdmission.Path `
            'hash-pinned pending receipt after acceptance-candidate write')
        Assert-ProtectedUnchanged $protectedBefore
        Assert-NativeIdle 'before atomic R31 acceptance publication'
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
if (($ConfirmFiveImagesReviewed -or
     $ConfirmNaniteRasterPairReviewedAndAccepted) -and
    -not $AcceptVisualReview) {
    throw 'Visual-review confirmation switches are valid only with -AcceptVisualReview.'
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
        'ConfirmFiveImagesReviewed',
        'ConfirmNaniteRasterPairReviewedAndAccepted')) {
        if (-not $PSBoundParameters.ContainsKey($requiredParameter)) {
            throw "Explicit R31 visual acceptance requires parameter: $requiredParameter"
        }
    }
    if ($RunToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$' -or
        $RunToken -ceq 'static-contract') {
        throw 'Explicit R31 visual acceptance requires the pending run token.'
    }
    if (-not $ConfirmFiveImagesReviewed) {
        throw 'Explicit R31 visual acceptance requires confirmation that all five PNGs were human-reviewed.'
    }
    if (-not $ConfirmNaniteRasterPairReviewedAndAccepted) {
        throw 'Explicit R31 visual acceptance requires human review and acceptance of the unchanged-pose Nanite/raster pair.'
    }
    foreach ($captureOnlyParameter in @(
        'R31CommitReceipt', 'ExpectedR31CommitReceiptSha256')) {
        if ($PSBoundParameters.ContainsKey($captureOnlyParameter)) {
            throw "Explicit visual acceptance refuses capture-only parameter: $captureOnlyParameter"
        }
    }
    Invoke-R31VisualReviewAcceptance `
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
    'ConfirmFiveImagesReviewed',
    'ConfirmNaniteRasterPairReviewedAndAccepted')) {
    if ($PSBoundParameters.ContainsKey($acceptanceOnlyParameter)) {
        throw "Live capture refuses acceptance-only parameter: $acceptanceOnlyParameter"
    }
}

foreach ($requiredParameter in @(
    'RunToken',
    'R31CommitReceipt',
    'ExpectedR31CommitReceiptSha256')) {
    if (-not $PSBoundParameters.ContainsKey($requiredParameter)) {
        throw "Live R31 Player0 capture requires explicit parameter: $requiredParameter"
    }
}
if ($RunToken -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$' -or
    $RunToken -ceq 'static-contract') {
    throw 'Live R31 Player0 capture requires a new safe run token.'
}

$requiredNativeFiles = @(
    $nativeProjectFile,
    $mapFile,
    $runtimeEditorDll,
    $editorDll,
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
Assert-NativeIdle 'R31 capture preflight'
Assert-RcPortUnowned
$r31Admission = Assert-R31CommitReceipt `
    $R31CommitReceipt $ExpectedR31CommitReceiptSha256
$r31ReceiptFileBefore = Get-FileState $r31Admission.Path
$groundHeaderBefore = Get-PathBoundFileReceipt `
    $groundHeader $expectedGroundHeader 'R31 pre-capture GroundVegetation header'
$groundSourceBefore = Get-PathBoundFileReceipt `
    $groundSource $expectedGroundSource 'R31 pre-capture GroundVegetation source'
$airSimProjectBefore = Assert-AirSimProjectConfiguration
$r31TreeBefore = @(Assert-R31NativeContent)
$immutableBefore = [pscustomobject] [ordered] @{
    ProjectFile=$airSimProjectBefore.ProjectFile
    AirSimDescriptor=$airSimProjectBefore.AirSimTriadRuntimeDescriptor
    R31=@($r31TreeBefore)
    R30=@(Get-TreeReceipt $r30ContentRoot)
    R29Facade=@(Get-TreeReceipt $r29FacadeContentRoot)
    R29Vegetation=@(Get-TreeReceipt $r29VegetationContentRoot)
    R29Terrain=@(Get-TreeReceipt $r29TerrainContentRoot)
    TreeRealism=@(Get-TreeReceipt $treeRealismContentRoot)
    ContextFacadeR25=@(Get-TreeReceipt $contextFacadeR25ContentRoot)
    LocalFallbackSuppressionV2=@(
        Get-TreeReceipt $localFallbackSuppressionV2ContentRoot)
    ContextTextures=@(Get-TreeReceipt $contextTextureRoot)
    AirSimContent=@(Get-TreeReceipt $airSimRuntimeContentRoot)
}
foreach ($treeName in @(
    'R31', 'R30', 'R29Facade', 'R29Vegetation', 'R29Terrain',
    'TreeRealism', 'ContextFacadeR25', 'LocalFallbackSuppressionV2',
    'ContextTextures', 'AirSimContent')) {
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
    'before first isolated R31 capture write'
Assert-ProtectedUnchanged $script:protectedBefore
Assert-NativeIdle 'before first isolated R31 capture write'

$transactionStarted = $false
$captureCompleted = $false
$primaryFailure = $null
$rollbackErrors = [Collections.Generic.List[string]]::new()
$sourceJournal = @()
$buildJournal = @()
$buildReceipt = $null
$cookProcessReceipt = $null
$gameProcessReceipt = $null
$cookClosure = $null
$captureReceipts = [Collections.Generic.List[object]]::new()
$gameBefore = Get-FileState $gameExe
$finishTransportError = ''
$nativePluginSourceTreeBaseline = @()
$nativePluginSourceTreeImmediatePreCapture = @()
$nativePluginSourceTreeImmediatePostCapture = @()
$groundHeaderImmediatePreCapture = $null
$groundSourceImmediatePreCapture = $null
$groundHeaderImmediatePostCapture = $null
$groundSourceImmediatePostCapture = $null

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
        R31Commit=$r31Admission
        ImmutableNativeState=$immutableBefore
        GameBinaryBefore=$gameBefore
        SourceJournalCount=$sourceJournal.Count
        BuildTreeJournalCount=$buildJournal.Count
        AirSimDisableArgumentsAllowed=$false
        ProviderReadyProofClaimed=$false
        R32DependencyAllowed=$false
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
        Get-TreeReceipt $nativePluginSourceRoot)
    if ($nativePluginSourceTreeBaseline.Count -eq 0) {
        throw 'Native plugin source-tree receipt is empty after R31 capture-source promotion.'
    }
    [void] (Get-PathBoundFileReceipt `
        $groundHeader $expectedGroundHeader `
        'GroundVegetation header after R31 capture-source promotion')
    [void] (Get-PathBoundFileReceipt `
        $groundSource $expectedGroundSource `
        'GroundVegetation source after R31 capture-source promotion')

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
        -Label 'R31 standalone Development Game build' `
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
        $gameExe $buildStartedUtc 'standalone R31 Development Game executable'
    Assert-BinaryMarkers $gameExe @(
        'UTRIADIstanaExploreV5DR31Player0CaptureLibrary',
        'GetIstanaExploreV5DR31Player0CaptureState',
        'ISTANA_EXPLORE_V5D_R31_PLAYER0_FALLBACK_CAPTURE_ACCEPTED',
        'ATRIADIstanaExploreV5DR30FacadeLookdevActor',
        'AirSimTriadRuntime',
        'WeatherActor')
    [void] (Assert-CaptureImmutableState `
        $immutableBefore $r31Admission $r31ReceiptFileBefore)

    $cookStartedUtc = [DateTime]::UtcNow
    $cookOutputPattern = Join-Path $evidenceRoot 'Cooked\[Platform]'
    $cookedPlatformRoot = Join-Path $evidenceRoot 'Cooked\Windows'
    $isolatedDdcRoot = Join-Path $evidenceRoot 'DDC'
    $cookRuntimeLog = Join-Path $logsRoot 'cook.runtime.log'
    $cookStdout = Join-Path $logsRoot 'cook.stdout.log'
    $cookStderr = Join-Path $logsRoot 'cook.stderr.log'
    $cookArguments = @(
        $nativeProjectFile,
        '-run=Cook',
        '-TargetPlatform=Windows',
        "-Map=$mapPackage",
        "-OutputDir=$cookOutputPattern",
        "-COOKDIR=$airSimRuntimeContentRoot",
        '-NullRHI',
        '-NoShaderWorker',
        '-asyncstaticmeshcompilation=0',
        '-DDC=InstalledNoZenLocalFallback',
        "-LocalDataCachePath=$isolatedDdcRoot",
        '-unattended',
        '-nop4',
        '-NoSplash',
        '-NoSound',
        '-stdout',
        '-FullStdOutLogOutput',
        "-abslog=$cookRuntimeLog")
    $cookProcessReceipt = Invoke-GuardedCommand `
        -Label 'R31 fresh isolated Windows cook' `
        -FilePath $unrealEditorCmd `
        -Arguments $cookArguments `
        -WorkingDirectory $nativeProjectRoot `
        -StandardOutputLog $cookStdout `
        -StandardErrorLog $cookStderr `
        -RequiredCommandLineTokens @(
            $nativeProjectFile, '-run=Cook', '-TargetPlatform=Windows',
            "-Map=$mapPackage", "-OutputDir=$cookOutputPattern",
            "-COOKDIR=$airSimRuntimeContentRoot") `
        -TimeoutSeconds $CookTimeoutSeconds
    Assert-NoFatalRuntimeLog $cookRuntimeLog 'fresh isolated R31 cook'
    $cookClosure = Assert-CookedClosure `
        $evidenceRoot $cookedPlatformRoot $cookStartedUtc
    [void] (Assert-CaptureImmutableState `
        $immutableBefore $r31Admission $r31ReceiptFileBefore)
    $nativePluginSourceTreeImmediatePreCapture = @(
        Assert-CaptureSourceTreeSnapshot `
            $nativePluginSourceTreeBaseline 'immediate pre-capture')
    $groundHeaderImmediatePreCapture = Get-PathBoundFileReceipt `
        $groundHeader $expectedGroundHeader 'immediate pre-capture GroundVegetation header'
    $groundSourceImmediatePreCapture = Get-PathBoundFileReceipt `
        $groundSource $expectedGroundSource 'immediate pre-capture GroundVegetation source'

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
        '-ini:Engine:[ConsoleVariables]:r.Nanite=1',
        '-ini:Engine:[ConsoleVariables]:r.Nanite.ProxyRenderMode=1',
        '-DDC=InstalledNoZenLocalFallback',
        "-LocalDataCachePath=$isolatedDdcRoot",
        '-RemoteControlHttpServer',
        '-RCWebControlEnable',
        '-ExecCmds=WebControl.StartServer',
        "-TRIADR31CaptureRun=$RunToken",
        "-TRIADR31CaptureOutputRoot=$evidenceRoot",
        "-abslog=$gameRuntimeLog")
    $gameStartParameters = @{
        Label='R31 standalone cooked Player0 Game capture'
        FilePath=$gameExe
        Arguments=$gameArguments
        WorkingDirectory=$nativeProjectRoot
        StandardOutputLog=$gameStdout
        StandardErrorLog=$gameStderr
        RequiredCommandLineTokens=@(
            $nativeProjectFile, $mapPackage, '-game',
            "-Sandbox=$sandboxRoot",
            "-TRIADR31CaptureRun=$RunToken",
            "-TRIADR31CaptureOutputRoot=$evidenceRoot",
            '-ini:Engine:[ConsoleVariables]:r.Nanite=1',
            '-ini:Engine:[ConsoleVariables]:r.Nanite.ProxyRenderMode=1')
    }
    $gameOwned = Start-GuardedOwnedProcess @gameStartParameters
    [void] (Wait-RuntimeCaptureReady $CaptureTimeoutSeconds)
    $initialNaniteRenderPath = Assert-RenderPathResponse `
        (Invoke-RcCall `
            'SetIstanaExploreV5DR31Player0CaptureRenderPath' `
            @{ RenderPathId='NANITE_ON' } 60) `
        'NANITE_ON' 'initial Nanite render path'

    foreach ($pose in $poses) {
        $setPose = Invoke-RcCall `
            'SetIstanaExploreV5DR31Player0CapturePose' `
            @{ PoseId=[string] $pose.Id } 60
        $setMessage = [string] (Get-RequiredPropertyValue `
            $setPose 'OutMessage' "set pose $($pose.Id)")
        if ((Get-RequiredPropertyValue `
                $setPose 'ReturnValue' "set pose $($pose.Id)") -ne $true -or
            -not $setMessage.Contains(
                'ISTANA_EXPLORE_V5D_R31_PLAYER0_POSE_PASS',
                [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                "poseId=$($pose.Id)", [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                'simulationSensorRfModified=false',
                [StringComparison]::Ordinal)) {
            throw "Unexpected exact pose acknowledgement: $setMessage"
        }
        $stableState = Wait-StableFallbackPose `
            $pose $CaptureTimeoutSeconds 'NANITE_ON'
        $immediatePreCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
        $preCaptureReport = Assert-ExactFallbackStateResponse `
            $immediatePreCapture $pose "pre-capture $($pose.Id)" 'NANITE_ON'
        $captureRequestedUtc = [DateTime]::UtcNow
        $capture = Invoke-RcCall `
            'CaptureIstanaExploreV5DR31Player0FallbackView' `
            @{ PoseId=[string] $pose.Id; RenderPathId='NANITE_ON' } `
            $CaptureTimeoutSeconds
        $captureMessage = [string] (Get-RequiredPropertyValue `
            $capture 'OutMessage' "capture $($pose.Id)")
        if ((Get-RequiredPropertyValue `
                $capture 'ReturnValue' "capture $($pose.Id)") -ne $true -or
            -not $captureMessage.Contains(
                'ISTANA_EXPLORE_V5D_R31_PLAYER0_FALLBACK_CAPTURE_ACCEPTED',
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
                [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                'renderPath=NANITE_ON',
                [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                'r.Nanite=1',
                [StringComparison]::Ordinal) -or
            -not $captureMessage.Contains(
                'r.Nanite.ProxyRenderMode=1',
                [StringComparison]::Ordinal)) {
            throw "Unexpected ProviderFallback capture acknowledgement: $captureMessage"
        }
        $capturePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            "explore_v5d_r31_player0_$($pose.Id)_$RunToken.png"))
        if (-not (Test-ContainedPath $capturePath $evidenceRoot)) {
            throw "Expected capture path escaped evidence root: $capturePath"
        }
        $imageReceipt = Wait-StableDecodedPng `
            $capturePath $captureRequestedUtc $CaptureTimeoutSeconds
        $immediatePostCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
        $postCaptureReport = Assert-ExactFallbackStateResponse `
            $immediatePostCapture $pose "post-capture $($pose.Id)" 'NANITE_ON'
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
            RenderPathId='NANITE_ON'
            NaniteConsoleValue=1
            NaniteProxyRenderMode=1
            ProviderFallbackVisualQa=$true
            ProviderReadyProofClaimed=$false
        })
    }

    # The five baseline images and their canonical filenames remain unchanged.
    # The sole sixth image is a raster-fallback twin of the existing high-
    # occupancy oblique, captured without moving Player0 in the same Game run.
    $comparisonPose = $poses[4]
    $rasterPathAcknowledgement = Assert-RenderPathResponse `
        (Invoke-RcCall `
            'SetIstanaExploreV5DR31Player0CaptureRenderPath' `
            @{ RenderPathId='RASTER_FALLBACK' } 60) `
        'RASTER_FALLBACK' 'raster-fallback comparison path'
    $rasterStableState = Wait-StableFallbackPose `
        $comparisonPose $CaptureTimeoutSeconds 'RASTER_FALLBACK'
    $rasterImmediatePreCapture = Invoke-RcCall `
        'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
    $rasterPreCaptureReport = Assert-ExactFallbackStateResponse `
        $rasterImmediatePreCapture $comparisonPose `
        'pre-capture raster-fallback comparison' 'RASTER_FALLBACK'
    $rasterCaptureRequestedUtc = [DateTime]::UtcNow
    $rasterCapture = Invoke-RcCall `
        'CaptureIstanaExploreV5DR31Player0FallbackView' `
        @{ PoseId=[string] $comparisonPose.Id; RenderPathId='RASTER_FALLBACK' } `
        $CaptureTimeoutSeconds
    $rasterCaptureMessage = [string] (Get-RequiredPropertyValue `
        $rasterCapture 'OutMessage' 'raster-fallback comparison capture')
    foreach ($marker in @(
        'ISTANA_EXPLORE_V5D_R31_PLAYER0_FALLBACK_CAPTURE_ACCEPTED',
        "poseId=$($comparisonPose.Id)",
        'providerFallbackVisualQa=true',
        'providerReadyProofClaimed=false',
        'airSimTriadRuntimeLoaded=true',
        'renderPath=RASTER_FALLBACK',
        'r.Nanite=0',
        'r.Nanite.ProxyRenderMode=0')) {
        if ((Get-RequiredPropertyValue `
                $rasterCapture 'ReturnValue' `
                'raster-fallback comparison capture') -ne $true -or
            -not $rasterCaptureMessage.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "Unexpected raster-fallback comparison acknowledgement; missing '$marker': $rasterCaptureMessage"
        }
    }
    $rasterCapturePath = [IO.Path]::GetFullPath((Join-Path `
        (Join-Path $evidenceRoot 'captures') `
        "explore_v5d_r31_player0_$($comparisonPose.Id)_${RunToken}_raster_fallback.png"))
    if (-not (Test-ContainedPath $rasterCapturePath $evidenceRoot)) {
        throw "Raster-fallback comparison path escaped evidence root: $rasterCapturePath"
    }
    $rasterImageReceipt = Wait-StableDecodedPng `
        $rasterCapturePath $rasterCaptureRequestedUtc $CaptureTimeoutSeconds
    $rasterImmediatePostCapture = Invoke-RcCall `
        'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
    $rasterPostCaptureReport = Assert-ExactFallbackStateResponse `
        $rasterImmediatePostCapture $comparisonPose `
        'post-capture raster-fallback comparison' 'RASTER_FALLBACK'

    $restoredNaniteAcknowledgement = Assert-RenderPathResponse `
        (Invoke-RcCall `
            'SetIstanaExploreV5DR31Player0CaptureRenderPath' `
            @{ RenderPathId='NANITE_ON' } 60) `
        'NANITE_ON' 'restored Nanite render path'
    $restoredNaniteStableState = Wait-StableFallbackPose `
        $comparisonPose $CaptureTimeoutSeconds 'NANITE_ON'
    $restoredNaniteStateResponse = Invoke-RcCall `
        'GetIstanaExploreV5DR31Player0CaptureState' @{} 60
    $restoredNaniteState = Assert-ExactFallbackStateResponse `
        $restoredNaniteStateResponse $comparisonPose `
        'restored Nanite render path' 'NANITE_ON'

    $naniteRasterComparison = [pscustomobject] [ordered] @{
        Required=$true
        PoseId='surroundings_oblique_macdonald'
        SamePose=$true
        SameGameProcess=$true
        SameCookedClosure=$true
        NaniteOn=[pscustomobject] [ordered] @{
            Capture=$captureReceipts[4]
            RenderPathId='NANITE_ON'
            NaniteConsoleValue=1
            NaniteProxyRenderMode=1
        }
        RasterFallback=[pscustomobject] [ordered] @{
            Pose=[pscustomobject] [ordered] @{
                Id=$comparisonPose.Id
                X=$comparisonPose.X; Y=$comparisonPose.Y; Z=$comparisonPose.Z
                Pitch=$comparisonPose.Pitch; Yaw=$comparisonPose.Yaw
                Roll=$comparisonPose.Roll
            }
            RenderPathAcknowledgement=$rasterPathAcknowledgement
            StableProviderFallbackWindow=$rasterStableState
            ImmediatePreCaptureState=$rasterPreCaptureReport
            CaptureAcknowledgement=$rasterCaptureMessage
            Image=$rasterImageReceipt
            ImmediatePostCaptureState=$rasterPostCaptureReport
            RenderPathId='RASTER_FALLBACK'
            NaniteConsoleValue=0
            NaniteProxyRenderMode=0
        }
        NaniteRestore=[pscustomobject] [ordered] @{
            Acknowledgement=$restoredNaniteAcknowledgement
            StableProviderFallbackWindow=$restoredNaniteStableState
            State=$restoredNaniteState
            RenderPathId='NANITE_ON'
            NaniteConsoleValue=1
            NaniteProxyRenderMode=1
            Restored=$true
        }
        MechanicalValidationPassed=$true
        HumanNaniteRasterComparisonAttested=$false
        NaniteRasterAppearanceParityAccepted=$false
        AutomaticVisualAcceptanceAllowed=$false
    }

    if ($captureReceipts.Count -ne 5 -or
        $false -in @($captureReceipts.Image.NonBlank) -or
        @($captureReceipts.Image.Sha256 | Sort-Object -Unique).Count -ne 5 -or
        @($captureReceipts.Image.DecodedBgraSha256 |
            Sort-Object -Unique).Count -ne 5 -or
        -not $rasterImageReceipt.NonBlank) {
        throw 'The exact five Player0 images are absent, blank, or not distinct at both file and decoded-pixel identity.'
    }

    try {
        $finish = Invoke-RcCall `
            'FinishIstanaExploreV5DR31Player0CaptureRun' @{} 60
        $finishMessage = [string] (Get-RequiredPropertyValue `
            $finish 'OutMessage' 'finish capture run')
        if ((Get-RequiredPropertyValue `
                $finish 'ReturnValue' 'finish capture run') -ne $true -or
            -not $finishMessage.Contains(
                'ISTANA_EXPLORE_V5D_R31_PLAYER0_CAPTURE_EXIT_ACCEPTED',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'renderPath=NANITE_ON',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'r.Nanite=1',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'r.Nanite.ProxyRenderMode=1',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'naniteConsoleStateRestored=true',
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
    Assert-NoFatalRuntimeLog $gameRuntimeLog 'standalone R31 capture Game'
    $gameLogText = [IO.File]::ReadAllText($gameRuntimeLog)
    if (-not $gameLogText.Contains(
            'ISTANA_EXPLORE_V5D_R31_PLAYER0_CAPTURE_EXIT_ACCEPTED',
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
        $immutableBefore $r31Admission $r31ReceiptFileBefore
    $nativePluginSourceTreeImmediatePostCapture = @(
        Assert-CaptureSourceTreeSnapshot `
            $nativePluginSourceTreeBaseline 'immediate post-capture')
    $groundHeaderImmediatePostCapture = Get-PathBoundFileReceipt `
        $groundHeader $expectedGroundHeader 'immediate post-capture GroundVegetation header'
    $groundSourceImmediatePostCapture = Get-PathBoundFileReceipt `
        $groundSource $expectedGroundSource 'immediate post-capture GroundVegetation source'
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before R31 pending visual-review receipt'

    $bindingEvidence = [pscustomobject] [ordered] @{
        MapCaptureBinding=[pscustomobject] [ordered] @{
            Path=$mapFile
            State=$r31Admission.SuccessorMap
        }
        RuntimeEditorDllCaptureBinding=[pscustomobject] [ordered] @{
            Path=$runtimeEditorDll
            State=$r31Admission.RuntimeDllAfter
        }
        EditorDllCaptureBinding=[pscustomobject] [ordered] @{
            Path=$editorDll
            State=$r31Admission.EditorDllAfter
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
        NativeOrder='R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32'
        R32DependencyAllowed=$false
        StaticReceipt=$staticReceipt
        PrewriteAdmission=$prewriteAdmission
        R31CommitAdmission=$r31Admission
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
        Cook=$cookProcessReceipt
        CookedClosure=$cookClosure
        CookedClosurePostflight=$cookedPostflight
        Game=$gameProcessReceipt
        FinishTransportErrorAfterExit=$finishTransportError
        Captures=@($captureReceipts)
        NaniteRasterHighOccupancyComparison=$naniteRasterComparison
        ExactPoseCount=5
        ExactImageCount=6
        MechanicalCaptureValidationPassed=$true
        HumanVisualReviewAttested=$false
        ConfirmedNaniteRasterPairReviewed=$false
        HumanNaniteRasterComparisonAttested=$false
        NaniteRasterAppearanceParityAccepted=$false
        VisualReviewRequired=$true
        VisualReviewAccepted=$false
        AutomaticVisualAcceptanceAllowed=$false
        TreeMaterialResponseV3Preserved=$true
        R32AdmissionAuthorized=$false
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        HyperrealismClaimed=$false
        CesiumProviderEvidencePresent=$true
        CesiumProviderReadyEvidencePresent=$false
        AirSimTriadRuntimeRetained=$true
        LegacyAirSimProjectStateChanged=$false
        AirSimDisableArgumentsUsed=$false
        NaniteConsoleStateRestored=$true
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
                'before R31 capture rollback' 60)
            $rollbackMayMutate = $true
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackMayMutate) {
            try { Restore-TreeJournal @($buildJournal) }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal @($sourceJournal) }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            foreach ($isolatedName in @('Cooked', 'captures', 'DDC')) {
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
                $r31Admission.SuccessorMap $mapFile 'rollback R31 map')
            [void] (Assert-State `
                $r31Admission.RuntimeDllAfter $runtimeEditorDll `
                'rollback R31 runtime editor DLL')
            [void] (Assert-State `
                $r31Admission.EditorDllAfter $editorDll `
                'rollback R31 editor DLL')
            [void] (Assert-TreeReceipt `
                $r31ContentRoot @($immutableBefore.R31) `
                'rollback R31 content')
            [void] (Assert-TreeReceipt `
                $airSimRuntimeContentRoot @($immutableBefore.AirSimContent) `
                'rollback AirSimTriadRuntime content')
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Assert-ProtectedUnchanged $script:protectedBefore }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback = [pscustomobject] [ordered] @{
            Schema=$schema
            Status=if ($rollbackErrors.Count -eq 0) {
                'ROLLED_BACK'
            } else { 'ROLLBACK_INCOMPLETE' }
            RunToken=$RunToken
            Failure=if ($null -ne $primaryFailure) {
                $primaryFailure.Message
            } else { 'Unknown failure before pending visual-review receipt.' }
            NativeMutationQuiescenceRequired=$true
            NativeMutationQuiescenceProven=$rollbackMayMutate
            ExactOwnedProcessContainmentOnly=$true
            SourceRestorationAttempted=$rollbackMayMutate
            BuildTreeRestorationAttempted=$rollbackMayMutate
            IsolatedCookCaptureDdcCleanupAttempted=$rollbackMayMutate
            R31MapAndContentPreserved=$true
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
            "R31 capture failed and rollback was incomplete: capture={$($primaryFailure.Message)} rollback={$([string]::Join('; ', @($rollbackErrors)))}",
            $primaryFailure)
    }
    throw $primaryFailure
}
