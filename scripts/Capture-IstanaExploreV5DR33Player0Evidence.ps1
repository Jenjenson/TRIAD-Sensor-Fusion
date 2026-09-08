<#
.SYNOPSIS
Build, freshly cook, and capture exact Player0 R33 Google/CWT comparison
evidence, then accept only a separately hash-pinned pending receipt after
human review.

.DESCRIPTION
Exactly one mode is required. -Execute consumes one caller-hash-pinned,
COMMITTED R33 native transaction receipt. That receipt must itself bind and
permit replay of the complete accepted R30/R31/R32 chain. Execute promotes
only two R33 runtime capture-library sources, performs a serial Development
Game build and isolated fresh cook, captures the same four fixed Player0 poses
in GooglePrimary and CwtPresented, and emits only PENDING_VISUAL_REVIEW. It
never mechanically accepts visual quality or provider readiness.

-AcceptVisualReview requires the caller-pinned pending receipt and an explicit
eight-image human attestation. It re-decodes and re-hashes every PNG and
rechecks the map, DLL, Ground source, whole plugin source tree, R33 transaction,
and all six predecessor receipts without launching Unreal or writing the
native project. It may emit COMMITTED visual QA but never authorizes R34.

Repository-only self-check:
  .\Capture-IstanaExploreV5DR33Player0Evidence.ps1 -StaticSelfCheck

Live capture after the exact R33 transaction commits:
  .\Capture-IstanaExploreV5DR33Player0Evidence.ps1 -Execute `
    -RunToken r33-player0-reviewed `
    -R33CommitReceipt D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DCesiumWorldTerrainReferenceR33V1\<token>\commit.json `
    -ExpectedR33CommitReceiptSha256 <sha256>

Human acceptance after reviewing all eight PNGs:
  .\Capture-IstanaExploreV5DR33Player0Evidence.ps1 -AcceptVisualReview `
    -ConfirmEightImagesReviewed `
    -RunToken r33-player0-reviewed `
    -PendingCaptureReceipt D:\triad\TRIAD_R33Evidence\r33-player0-reviewed\pending-visual-review.json `
    -ExpectedPendingCaptureReceiptSha256 <sha256>
#>
[CmdletBinding()]
param(
    [switch] $StaticSelfCheck,
    [switch] $Execute,
    [switch] $AcceptVisualReview,
    [switch] $ConfirmEightImagesReviewed,
    [string] $RunToken = 'static-contract',
    [string] $R33CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR33CommitReceiptSha256 = '',
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

$schema = 'triad.istana_explore_v5d.r33_player0_capture.v1'
$r33TransactionSchema =
    'triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1'
$r32TransactionSchema =
    'triad.istana_explore_v5d.r32_medium_distance_turf.native_transaction.v1'
$r32CaptureSchema = 'triad.istana_explore_v5d.r32_player0_capture.v1'
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
    'D:\triad\TRIAD_R33Evidence')
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
$groundHeader = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DGroundVegetationActor.h'))
$groundSource = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DGroundVegetationActor.cpp'))
$r33TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DCesiumWorldTerrainReferenceR33V1'))
$r32TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DMediumDistanceTurfR32V1'))
$r32EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R32Evidence')
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
    '/Script/TRIADSensorFusion.Default__TRIADIstanaExploreV5DR33Player0CaptureLibrary'
$script:activeOwnedProcess = $null
$r32MaterialContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32'))

$captureSourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR33Player0CaptureLibrary.h'
        Bytes = 2161L
        Sha256 = 'B5F0F8B9496448070530F85EA087EF47979E97A3B823F6615E3D392A0ABC329D'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR33Player0CaptureLibrary.cpp'
        Bytes = 28764L
        Sha256 = '471EB8435EABE2170A58C97FC32677E4F450580BE931E65CB3894A32CB54E131'
    }
)

$poses = @(
    [pscustomobject] [ordered] @{ Id='075m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-1.252968; Yaw=-90.0; Roll=0.0; ReviewPurpose='terrain_wide' }
    [pscustomobject] [ordered] @{ Id='020m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-4.703535; Yaw=-90.0; Roll=0.0; ReviewPurpose='terrain_mid' }
    [pscustomobject] [ordered] @{ Id='095m'; X=3500.0; Y=15000.0; Z=164.0; Pitch=-0.989007849; Yaw=-90.0; Roll=0.0; ReviewPurpose='grazing_hole_double_render' }
    [pscustomobject] [ordered] @{
        Id='surroundings_oblique_macdonald'
        X=40226.9640238642; Y=106152.591966384; Z=3900.0
        Pitch=-5.74137954693854; Yaw=-101.620267003074; Roll=0.0
        ReviewPurpose='surroundings_oblique'
    }
)
$presentations = @('GooglePrimary', 'CwtPresented')
$capturePlan = @(
    foreach ($presentation in $presentations) {
        foreach ($pose in $poses) {
            [pscustomobject] [ordered] @{
                Presentation=$presentation
                Pose=$pose
                Id=('{0}__{1}' -f $presentation, $pose.Id)
            }
        }
    }
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

function Assert-AdmissionBinding {
    param($Binding, $Admission, [string] $Label)
    $path = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $Binding 'Path' $Label))
    $file = Get-RequiredPropertyValue $Binding 'File' $Label
    if (-not $path.Equals(
            $Admission.Path, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-StateMatches $file $Admission.State)) {
        throw "$Label does not bind the exact admitted receipt."
    }
}

function Assert-AcceptedCaptureTruth {
    param(
        $Admission,
        [string] $ExpectedSchema,
        [string] $ExpectedOrder,
        [int] $ExpectedPoseCount,
        [string] $RehashField,
        [string] $ConfirmedField,
        [string] $DependencyField,
        [string] $QaField,
        [string] $NextAdmissionField,
        [string] $PendingAdmissionField,
        [string] $EvidenceBase,
        [string] $Label)
    $receipt = $Admission.Receipt
    if ([string] (Get-RequiredPropertyValue $receipt 'Schema' $Label) -cne $ExpectedSchema -or
        [string] (Get-RequiredPropertyValue $receipt 'Status' $Label) -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $receipt 'RunToken' $Label) -cne $Admission.Token -or
        [string] (Get-RequiredPropertyValue $receipt 'NativeOrder' $Label) -cne $ExpectedOrder -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactPoseCount' $Label) -ne $ExpectedPoseCount -or
        [bool] (Get-RequiredPropertyValue $receipt $DependencyField $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'MechanicalCaptureValidationPassed' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ExplicitHumanReviewAcceptance' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt $ConfirmedField $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanVisualReviewAttested' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'AutomaticVisualAcceptanceAllowed' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewRequired' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewAccepted' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt $QaField $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt $NextAdmissionField $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderReadyProofClaimed' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HyperrealismClaimed' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'MapModifiedByCapture' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SimulationCollisionNavigationSensorRfModified' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NativeStateMutatedByAcceptance' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'UnrealLaunchedByAcceptance' $Label) -ne $false) {
        throw "$Label is not the exact explicit human-accepted no-mutation receipt."
    }
    $acceptance = Get-RequiredPropertyValue $receipt 'AcceptanceRevalidation' $Label
    if ([bool] (Get-RequiredPropertyValue $acceptance 'Unchanged' "$Label acceptance") -ne $true -or
        [bool] (Get-RequiredPropertyValue $acceptance 'PendingReceiptHashPinned' "$Label acceptance") -ne $true -or
        [bool] (Get-RequiredPropertyValue $acceptance $RehashField "$Label acceptance") -ne $true -or
        [bool] (Get-RequiredPropertyValue $acceptance 'NativeReceiptWriteAllowed' "$Label acceptance") -ne $false) {
        throw "$Label lacks exact two-phase acceptance revalidation."
    }
    $pending = Get-RequiredPropertyValue $receipt $PendingAdmissionField $Label
    $pendingPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredPropertyValue $pending 'Path' "$Label pending admission"))
    $pendingFile = Get-RequiredPropertyValue $pending 'File' "$Label pending admission"
    $expectedPending = [IO.Path]::GetFullPath((Join-Path (Join-Path $EvidenceBase $Admission.Token) 'pending-visual-review.json'))
    if (-not $pendingPath.Equals(
            $expectedPending, [StringComparison]::OrdinalIgnoreCase) -or
        [string] (Get-RequiredPropertyValue $pending 'Status' "$Label pending admission") -cne 'PENDING_VISUAL_REVIEW' -or
        [string] (Get-RequiredPropertyValue $pending 'CallerSha256' "$Label pending admission") -cne [string] $pendingFile.Sha256) {
        throw "$Label does not bind its exact pending receipt."
    }
    [void] (Assert-State $pendingFile $pendingPath "$Label pending receipt")
}

function Assert-R31V2NaniteRasterAcceptedReceipt {
    param($Admission)
    $receipt=$Admission.Receipt
    if ([int] (Get-RequiredPropertyValue $receipt 'ExactImageCount' 'R31 capture') -ne 6 -or
        [bool] (Get-RequiredPropertyValue $receipt 'ConfirmedNaniteRasterPairReviewed' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanNaniteRasterComparisonAttested' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'NaniteRasterAppearanceParityAccepted' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'NaniteConsoleStateRestored' 'R31 capture') -ne $true) {
        throw 'R31 predecessor lacks accepted six-image Nanite/raster review truth.'
    }
    $captures=@(Get-RequiredPropertyValue $receipt 'Captures' 'R31 capture')
    if ($captures.Count -ne 5) {
        throw 'R31 predecessor must retain exactly five baseline pose rows.'
    }
    $comparison=Get-RequiredPropertyValue $receipt `
        'NaniteRasterHighOccupancyComparison' 'R31 capture'
    if ([bool] (Get-RequiredPropertyValue $comparison 'Required' 'R31 Nanite/raster comparison') -ne $true -or
        [string] (Get-RequiredPropertyValue $comparison 'PoseId' 'R31 Nanite/raster comparison') -cne 'surroundings_oblique_macdonald' -or
        [bool] (Get-RequiredPropertyValue $comparison 'SamePose' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameGameProcess' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameCookedClosure' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'MechanicalValidationPassed' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'HumanNaniteRasterComparisonAttested' 'R31 mechanical comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison 'NaniteRasterAppearanceParityAccepted' 'R31 mechanical comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison 'AutomaticVisualAcceptanceAllowed' 'R31 mechanical comparison') -ne $false) {
        throw 'R31 nested Nanite/raster comparison is not the exact fail-closed mechanical proof.'
    }
    $nanite=Get-RequiredPropertyValue $comparison 'NaniteOn' 'R31 Nanite/raster comparison'
    if ((ConvertTo-Json (Get-RequiredPropertyValue $nanite 'Capture' 'R31 Nanite side') -Depth 20 -Compress) -cne
            (ConvertTo-Json $captures[4] -Depth 20 -Compress) -or
        [string] (Get-RequiredPropertyValue $nanite 'RenderPathId' 'R31 Nanite side') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $nanite 'NaniteConsoleValue' 'R31 Nanite side') -ne 1 -or
        [int] (Get-RequiredPropertyValue $nanite 'NaniteProxyRenderMode' 'R31 Nanite side') -ne 1) {
        throw 'R31 Nanite comparison side is not the exact fifth baseline capture.'
    }
    $raster=Get-RequiredPropertyValue $comparison 'RasterFallback' 'R31 Nanite/raster comparison'
    if ((ConvertTo-Json (Get-RequiredPropertyValue $raster 'Pose' 'R31 raster side') -Depth 8 -Compress) -cne
            (ConvertTo-Json (Get-RequiredPropertyValue $captures[4] 'Pose' 'R31 fifth baseline') -Depth 8 -Compress) -or
        [string] (Get-RequiredPropertyValue $raster 'RenderPathId' 'R31 raster side') -cne 'RASTER_FALLBACK' -or
        [int] (Get-RequiredPropertyValue $raster 'NaniteConsoleValue' 'R31 raster side') -ne 0 -or
        [int] (Get-RequiredPropertyValue $raster 'NaniteProxyRenderMode' 'R31 raster side') -ne 0) {
        throw 'R31 raster comparison side changed pose or lacks exact zero CVar readback.'
    }
    $rasterImage=Get-RequiredPropertyValue $raster 'Image' 'R31 raster side'
    $expectedRasterPath=[IO.Path]::GetFullPath((Join-Path `
        (Join-Path (Join-Path $r31EvidenceBase $Admission.Token) 'captures') `
        "explore_v5d_r31_player0_surroundings_oblique_macdonald_$($Admission.Token)_raster_fallback.png"))
    if (-not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $rasterImage 'Path' 'R31 raster image')).Equals($expectedRasterPath,[StringComparison]::OrdinalIgnoreCase) -or
        [int] (Get-RequiredPropertyValue $rasterImage 'WidthPixels' 'R31 raster image') -ne 2560 -or
        [int] (Get-RequiredPropertyValue $rasterImage 'HeightPixels' 'R31 raster image') -ne 1440 -or
        [bool] (Get-RequiredPropertyValue $rasterImage 'NonBlank' 'R31 raster image') -ne $true -or
        [string]::IsNullOrWhiteSpace([string] (Get-RequiredPropertyValue $rasterImage 'DecodedBgraSha256' 'R31 raster image'))) {
        throw 'R31 raster comparison image is not the canonical decoded sixth image.'
    }
    [void] (Assert-State $rasterImage $expectedRasterPath 'R31 raster comparison PNG')
    $restore=Get-RequiredPropertyValue $comparison 'NaniteRestore' 'R31 comparison'
    $restoreState=[string] (Get-RequiredPropertyValue $restore 'State' 'R31 Nanite restore')
    if ([bool] (Get-RequiredPropertyValue $restore 'Restored' 'R31 Nanite restore') -ne $true -or
        [string] (Get-RequiredPropertyValue $restore 'RenderPathId' 'R31 Nanite restore') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteConsoleValue' 'R31 Nanite restore') -ne 1 -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteProxyRenderMode' 'R31 Nanite restore') -ne 1 -or
        -not $restoreState.Contains('renderPath=NANITE_ON',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite=1',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite.ProxyRenderMode=1',[StringComparison]::Ordinal)) {
        throw 'R31 predecessor does not prove Nanite state restoration.'
    }
}

function Assert-R33CommitReceipt {
    param([string] $Path, [string] $ExpectedSha256)
    $admission = Read-HashPinnedDirectReceipt $Path $ExpectedSha256 $r33TransactionBase 'R33 transaction receipt'
    $receipt = $admission.Receipt
    if ([string] (Get-RequiredPropertyValue $receipt 'Schema' 'R33 receipt') -cne $r33TransactionSchema -or
        [string] (Get-RequiredPropertyValue $receipt 'Status' 'R33 receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $receipt 'RunToken' 'R33 receipt') -cne $admission.Token -or
        [string] (Get-RequiredPropertyValue $receipt 'NativeOrder' 'R33 receipt') -cne 'R30_COMMIT_HUMAN_CAPTURE_THEN_R31_COMMIT_HUMAN_CAPTURE_THEN_R32_COMMIT_HUMAN_CAPTURE_THEN_R33' -or
        [int] (Get-RequiredPropertyValue $receipt 'PromotedCodeFileCount' 'R33 receipt') -ne 9 -or
        [int] (Get-RequiredPropertyValue $receipt 'PromotedContractFileCount' 'R33 receipt') -ne 1 -or
        [int] (Get-RequiredPropertyValue $receipt 'PromotedContentPackageCount' 'R33 receipt') -ne 0 -or
        [int] (Get-RequiredPropertyValue $receipt 'CommitR33EndpointInvocationCount' 'R33 receipt') -ne 1 -or
        [int] (Get-RequiredPropertyValue $receipt 'StandaloneApplyR33EndpointInvocationCount' 'R33 receipt') -ne 0 -or
        [int64] (Get-RequiredPropertyValue $receipt 'GooglePhotorealisticIonAssetId' 'R33 receipt') -ne 2275207L -or
        [int64] (Get-RequiredPropertyValue $receipt 'CesiumWorldTerrainIonAssetId' 'R33 receipt') -ne 1L -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactCesiumTilesetCount' 'R33 receipt') -ne 2 -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactCesiumGeoreferenceCount' 'R33 receipt') -ne 1 -or
        [bool] (Get-RequiredPropertyValue $receipt 'SameGeoreferenceAndIonServerRequired' 'R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'SourceTerrainModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3PromotedAtR30' 'R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Preserved' 'R33 receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeResponseMaterialPackageCount' 'R33 receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeDerivativeMeshPackageCount' 'R33 receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $receipt 'TreeRuntimeResponseMidCount' 'R33 receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreePlacementGeometryOpacityWindAuthorityModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CollisionModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NavigationModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'LineOfSightAuthorityModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SensorPlacementModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'DetectionSimulationModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'RfTruthModified' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SimulationCollisionNavigationSensorRfAuthority' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReferenceOnly' 'R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'GeospatialAuthority' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'AccurateRealWorldTerrainClaimed' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SurveyAccuracyClaimed' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VerticalDatumResolved' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HeightCheckpointValidationComplete' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderTermsAndEntitlementVerifiedByThisTransaction' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderCreditsMustRemainVisible' 'R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualCaptureAccepted' 'R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CaptureRevalidationRequired' 'R33 receipt') -ne $true) {
        throw 'R33 transaction receipt failed the exact committed visual-reference contract.'
    }
    $states = @(Get-RequiredPropertyValue $receipt 'PresentationStates' 'R33 receipt')
    if ((ConvertTo-Json $states -Compress) -cne
        (ConvertTo-Json @('GooglePrimary','CwtWarming','CwtPresented','SafeLocal') -Compress)) {
        throw 'R33 transaction presentation-state roster drifted.'
    }

    $r30Transaction = Assert-EmbeddedReceiptAdmission (Get-RequiredPropertyValue $receipt 'R30TransactionAdmission' 'R33 receipt') $r30TransactionBase 'embedded R30 transaction receipt'
    $r30Capture = Assert-EmbeddedReceiptAdmission (Get-RequiredPropertyValue $receipt 'R30CaptureAdmission' 'R33 receipt') $r30EvidenceBase 'embedded R30 capture receipt'
    $r31Transaction = Assert-EmbeddedReceiptAdmission (Get-RequiredPropertyValue $receipt 'R31TransactionAdmission' 'R33 receipt') $r31TransactionBase 'embedded R31 transaction receipt'
    $r31Capture = Assert-EmbeddedReceiptAdmission (Get-RequiredPropertyValue $receipt 'R31CaptureAdmission' 'R33 receipt') $r31EvidenceBase 'embedded R31 capture receipt'
    $r32Transaction = Assert-EmbeddedReceiptAdmission (Get-RequiredPropertyValue $receipt 'R32TransactionAdmission' 'R33 receipt') $r32TransactionBase 'embedded R32 transaction receipt'
    $r32Capture = Assert-EmbeddedReceiptAdmission (Get-RequiredPropertyValue $receipt 'R32CaptureAdmission' 'R33 receipt') $r32EvidenceBase 'embedded R32 capture receipt'

    if ([string] $r30Transaction.Receipt.Schema -cne $r30TransactionSchema -or
        [string] $r30Transaction.Receipt.Status -cne 'COMMITTED' -or
        [string] $r30Transaction.Receipt.RunToken -cne $r30Transaction.Token -or
        [int] $r30Transaction.Receipt.R30ContentPackageCount -ne 12 -or
        [bool] $r30Transaction.Receipt.TreeMaterialResponseV3Promoted -ne $true -or
        [int] $r30Transaction.Receipt.TreeResponseMaterialPackageCount -ne 13 -or
        [int] $r30Transaction.Receipt.TreeRuntimeResponseMidCount -ne 26 -or
        [bool] $r30Transaction.Receipt.VisualCaptureAccepted -ne $false -or
        [bool] $r30Transaction.Receipt.CaptureRevalidationRequired -ne $true -or
        [string] $r31Transaction.Receipt.Schema -cne $r31TransactionSchema -or
        [string] $r31Transaction.Receipt.Status -cne 'COMMITTED' -or
        [string] $r31Transaction.Receipt.RunToken -cne $r31Transaction.Token -or
        [int] $r31Transaction.Receipt.R31ContentPackageCount -ne 5 -or
        [bool] $r31Transaction.Receipt.TreeMaterialResponseV3Preserved -ne $true -or
        [int] $r31Transaction.Receipt.TreeResponseMaterialPackageCount -ne 13 -or
        [int] $r31Transaction.Receipt.TreeRuntimeResponseMidCount -ne 26 -or
        [bool] $r31Transaction.Receipt.R33SourceOrDeclarationAllowed -ne $false -or
        [bool] $r31Transaction.Receipt.VisualCaptureAccepted -ne $false -or
        [bool] $r31Transaction.Receipt.CaptureRevalidationRequired -ne $true -or
        [string] $r32Transaction.Receipt.Schema -cne $r32TransactionSchema -or
        [string] $r32Transaction.Receipt.Status -cne 'COMMITTED' -or
        [string] $r32Transaction.Receipt.RunToken -cne $r32Transaction.Token -or
        [int] $r32Transaction.Receipt.PromotedCodeFileCount -ne 8 -or
        [int] $r32Transaction.Receipt.PromotedContractFileCount -ne 1 -or
        [int] $r32Transaction.Receipt.R32ContentPackageCount -ne 4 -or
        [bool] $r32Transaction.Receipt.TreeMaterialResponseV3Preserved -ne $true -or
        [int] $r32Transaction.Receipt.TreeResponseMaterialPackageCount -ne 13 -or
        [int] $r32Transaction.Receipt.TreeRuntimeResponseMidCount -ne 26 -or
        [int] $r32Transaction.Receipt.CommitR32EndpointInvocationCount -ne 1 -or
        [int] $r32Transaction.Receipt.StandaloneApplyR32EndpointInvocationCount -ne 0 -or
        [bool] $r32Transaction.Receipt.R33SourceOrDeclarationAllowed -ne $false -or
        [bool] $r32Transaction.Receipt.VisualCaptureAccepted -ne $false -or
        [bool] $r32Transaction.Receipt.CaptureRevalidationRequired -ne $true -or
        [bool] $r32Transaction.Receipt.SimulationCollisionNavigationSensorRfAuthority -ne $false) {
        throw 'R33 capture rejected an invalid R30/R31/R32 transaction receipt.'
    }
    $r32Materials=@(Get-RequiredPropertyValue $r32Transaction.Receipt 'R32MaterialPackages' 'R32 transaction receipt')
    if ($r32Materials.Count -ne 4) {
        throw 'R33 capture rejected an incomplete R32 material-content receipt.'
    }
    Assert-TreeReceipt $r32MaterialContentRoot $r32Materials 'R32 material packages bound by transaction'

    Assert-AcceptedCaptureTruth $r30Capture $r30CaptureSchema 'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' 5 'FivePngsRedecodedAndRehashed' 'ConfirmedFiveImagesReviewed' 'R31DependencyAllowed' 'ProviderFallbackVisualQaAccepted' 'R31AdmissionAuthorized' 'PendingCaptureAdmission' $r30EvidenceBase 'R30 capture'
    Assert-AcceptedCaptureTruth $r31Capture $r31CaptureSchema 'R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32' 5 'SixPngsRedecodedAndRehashed' 'ConfirmedFiveImagesReviewed' 'R32DependencyAllowed' 'R31BroadShellVisualQaAccepted' 'R32AdmissionAuthorized' 'PendingCaptureAdmission' $r31EvidenceBase 'R31 capture'
    Assert-R31V2NaniteRasterAcceptedReceipt $r31Capture
    Assert-AcceptedCaptureTruth $r32Capture $r32CaptureSchema 'R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33' 10 'TenPngsRedecodedAndRehashed' 'ConfirmedTenImagesReviewed' 'R33DependencyAllowed' 'R32MediumDistanceTurfVisualQaAccepted' 'R33AdmissionAuthorized' 'PendingVisualReviewReceipt' $r32EvidenceBase 'R32 capture'

    Assert-AdmissionBinding $r30Capture.Receipt.R30CommitAdmission $r30Transaction 'R30 capture transaction admission'
    Assert-AdmissionBinding $r31Transaction.Receipt.R30TransactionAdmission $r30Transaction 'R31 embedded R30 transaction admission'
    Assert-AdmissionBinding $r31Transaction.Receipt.R30CaptureAdmission $r30Capture 'R31 embedded R30 capture admission'
    Assert-AdmissionBinding $r31Capture.Receipt.R31CommitAdmission $r31Transaction 'R31 capture transaction admission'
    Assert-AdmissionBinding $r32Transaction.Receipt.R30TransactionAdmission $r30Transaction 'R32 embedded R30 transaction admission'
    Assert-AdmissionBinding $r32Transaction.Receipt.R30CaptureAdmission $r30Capture 'R32 embedded R30 capture admission'
    Assert-AdmissionBinding $r32Transaction.Receipt.R31TransactionAdmission $r31Transaction 'R32 embedded R31 transaction admission'
    Assert-AdmissionBinding $r32Transaction.Receipt.R31CaptureAdmission $r31Capture 'R32 embedded R31 capture admission'
    Assert-AdmissionBinding $r32Capture.Receipt.R32CommitAdmission $r32Transaction 'R32 capture transaction admission'

    if ([bool] (Get-RequiredPropertyValue $r30Capture.Receipt 'TreeMaterialResponseV3Reviewed' 'R30 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt 'TreeMaterialResponseV3Preserved' 'R31 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r32Capture.Receipt 'TreeMaterialResponseV3Preserved' 'R32 capture') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt.AcceptanceRevalidation 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture.Receipt.AcceptanceRevalidation 'TreeResponseMaterials13AndRuntimeMids26Revalidated' 'R30 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture.Receipt.AcceptanceRevalidation 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' 'R31 acceptance') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r32Capture.Receipt.AcceptanceRevalidation 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' 'R32 acceptance') -ne $true) {
        throw 'R30-R32 accepted captures do not preserve the exact TreeRealism v3 review chain.'
    }
    $treeMaterialResponseV3=Get-RequiredPropertyValue `
        $receipt 'TreeMaterialResponseV3' 'R33 receipt'
    $treeNativePins=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'NativeSourcePins' 'R33 TreeRealism v3 receipt')
    $treeClosurePins=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'SourceClosurePins' 'R33 TreeRealism v3 receipt')
    $treeBefore=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentBefore' 'R33 TreeRealism v3 receipt')
    $treeAfter=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentAfter' 'R33 TreeRealism v3 receipt')
    if ([string] (Get-RequiredPropertyValue $treeMaterialResponseV3 'Status' 'R33 TreeRealism v3 receipt') -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        $treeNativePins.Count -ne 4 -or $treeClosurePins.Count -ne 6 -or
        $treeAfter.Count -ne $treeBefore.Count + 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ResponseMaterialPackageCount' 'R33 TreeRealism v3 receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ReboundManagedMeshPackageCount' 'R33 TreeRealism v3 receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'RuntimeResponseMidCount' 'R33 TreeRealism v3 receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $treeMaterialResponseV3 'TreePlacementGeometryOpacityWindAuthorityModified' 'R33 TreeRealism v3 receipt') -ne $false) {
        throw 'R33 TreeRealism v3 receipt failed its exact promoted-state contract.'
    }
    foreach ($upstreamTree in @(
        $r30Transaction.Receipt.TreeMaterialResponseV3,
        $r31Transaction.Receipt.TreeMaterialResponseV3,
        $r32Transaction.Receipt.TreeMaterialResponseV3,
        $r32Capture.Receipt.R32CommitAdmission.TreeMaterialResponseV3)) {
        if ((ConvertTo-Json $treeMaterialResponseV3 -Depth 16 -Compress) -cne
            (ConvertTo-Json $upstreamTree -Depth 16 -Compress)) {
            throw 'R30-R33 TreeRealism v3 receipt identity drifted.'
        }
    }
    foreach ($pin in @($treeNativePins)+@($treeClosurePins)) {
        $relative=[string] (Get-RequiredPropertyValue $pin 'RelativePath' 'R33 TreeRealism v3 pin')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('..')) {
            throw "R33 TreeRealism v3 contains a noncanonical pin: $relative"
        }
        $pinPath=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $pinPath $nativeProjectRoot)) {
            throw "R33 TreeRealism v3 pin escaped native root: $pinPath"
        }
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] (Get-RequiredPropertyValue $pin 'Bytes' 'R33 TreeRealism v3 pin')
            Sha256=[string] (Get-RequiredPropertyValue $pin 'Sha256' 'R33 TreeRealism v3 pin')
        }) $pinPath 'R33 retained TreeRealism v3 source pin')
    }
    if ((ConvertTo-Json @(ConvertTo-TreeIdentity $treeAfter) -Depth 8 -Compress) -cne
        (ConvertTo-Json @(ConvertTo-TreeIdentity (Get-TreeReceipt $immutableContentRoots.TreeRealism)) -Depth 8 -Compress)) {
        throw 'Current TreeRealism content no longer matches the R30 v3 identity preserved through R33.'
    }

    $map = Get-RequiredPropertyValue $receipt 'SuccessorMap' 'R33 receipt'
    $runtimeDll = Get-RequiredPropertyValue $receipt 'RuntimeDllAfter' 'R33 receipt'
    $editorDllState = Get-RequiredPropertyValue $receipt 'EditorDllAfter' 'R33 receipt'
    $groundHeaderState = Get-RequiredPropertyValue $receipt 'GroundHeaderAfter' 'R33 receipt'
    $groundSourceState = Get-RequiredPropertyValue $receipt 'GroundSourceAfter' 'R33 receipt'
    $sourceTree = @(Get-RequiredPropertyValue $receipt 'NativePluginSourceTreeAfter' 'R33 receipt')
    if ($sourceTree.Count -le 0 -or
        @($sourceTree | Where-Object {
            [string] $_.RelativePath -match 'R34'
        }).Count -ne 0) {
        throw 'R33 committed source tree is empty or R34-contaminated.'
    }
    [void] (Assert-State $map $mapFile 'R33 committed map')
    [void] (Assert-State $runtimeDll $runtimeEditorDll 'R33 runtime editor DLL')
    [void] (Assert-State $editorDllState $editorDll 'R33 editor DLL')
    [void] (Assert-State $groundHeaderState $groundHeader 'R33 Ground header')
    [void] (Assert-State $groundSourceState $groundSource 'R33 Ground source')
    [void] (Assert-TreeReceipt $nativePluginSourceRoot $sourceTree 'R33 committed plugin source')

    $r32Map = Get-RequiredPropertyValue $r32Transaction.Receipt 'SuccessorMap' 'R32 transaction'
    $r32Runtime = Get-RequiredPropertyValue $r32Transaction.Receipt 'RuntimeDllAfter' 'R32 transaction'
    $r32Editor = Get-RequiredPropertyValue $r32Transaction.Receipt 'EditorDllAfter' 'R32 transaction'
    $r32GroundH = Get-RequiredPropertyValue $r32Transaction.Receipt 'GroundHeaderAfter' 'R32 transaction'
    $r32GroundC = Get-RequiredPropertyValue $r32Transaction.Receipt 'GroundSourceAfter' 'R32 transaction'
    $r32CaptureTreeBinding = Get-RequiredPropertyValue $r32Capture.Receipt 'NativePluginSourceTree' 'R32 capture'
    $r32CaptureTree = @(Get-RequiredPropertyValue $r32CaptureTreeBinding 'BaselineAfterCaptureSourcePromotion' 'R32 capture source tree')
    $r33BeforeTree = @(Get-RequiredPropertyValue $receipt 'NativePluginSourceTreeBefore' 'R33 receipt')
    if (-not (Test-StateMatches $r32Capture.Receipt.Map $r32Map) -or
        -not (Test-StateMatches $r32Capture.Receipt.RuntimeEditorDll $r32Runtime) -or
        -not (Test-StateMatches $r32Capture.Receipt.EditorDll $r32Editor) -or
        -not (Test-StateMatches $r32Capture.Receipt.GroundHeader $r32GroundH) -or
        -not (Test-StateMatches $r32Capture.Receipt.GroundSource $r32GroundC) -or
        -not (Test-StateMatches $receipt.PredecessorMap $r32Map) -or
        -not (Test-StateMatches $receipt.RuntimeDllBefore $r32Runtime) -or
        -not (Test-StateMatches $receipt.EditorDllBefore $r32Editor) -or
        -not (Test-StateMatches $receipt.GroundHeaderBefore $r32GroundH) -or
        -not (Test-StateMatches $receipt.GroundSourceBefore $r32GroundC) -or
        [bool] $r32CaptureTreeBinding.Unchanged -ne $true -or
        (ConvertTo-Json $r32CaptureTree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($r32CaptureTreeBinding.ImmediatePreCapture) -Depth 6 -Compress) -or
        (ConvertTo-Json $r32CaptureTree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($r32CaptureTreeBinding.ImmediatePostCapture) -Depth 6 -Compress) -or
        (ConvertTo-Json $r32CaptureTree -Depth 6 -Compress) -cne
            (ConvertTo-Json $r33BeforeTree -Depth 6 -Compress)) {
        throw 'R33 receipt is not cross-bound to the exact accepted R32 boundary.'
    }

    [pscustomobject] [ordered] @{
        R33=$admission
        R30Transaction=$r30Transaction
        R30Capture=$r30Capture
        R31Transaction=$r31Transaction
        R31Capture=$r31Capture
        R32Transaction=$r32Transaction
        R32Capture=$r32Capture
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
            throw "R33 capture source pin is not canonical: $($pin.RelativePath)"
        }
        $path = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $path $repositoryUnrealRoot)) {
            throw "R33 capture source escaped repository Unreal root: $path"
        }
        [void] (Assert-State ([pscustomobject] [ordered] @{
            Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256
        }) $path 'repository R33 capture source')
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
    param([object[]] $R33Tree)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($row in @($R33Tree)) {
        $relative = [string] (
            Get-RequiredPropertyValue $row 'RelativePath' 'R33 source row')
        if ($relative -match 'R34') {
            throw "R34 source is forbidden in the R33 capture baseline: $relative"
        }
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath=$relative.Replace('/', '\')
            Bytes=[int64] (Get-RequiredPropertyValue $row 'Bytes' 'R33 source row')
            Sha256=[string] (Get-RequiredPropertyValue $row 'Sha256' 'R33 source row')
        })
    }
    foreach ($pin in $captureSourcePins) {
        $relative = Get-CaptureTreeRelativePath $pin
        if (@($rows | Where-Object {
                $_.RelativePath -ceq $relative
            }).Count -ne 0) {
            throw "R33 transaction source tree unexpectedly owns capture source: $relative"
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
    param([object[]] $R33Tree)
    Assert-CaptureSourcePins
    $expectedIdentity = @(Get-ExpectedCaptureSourceTree $R33Tree)
    $current = @(Get-TreeReceipt $nativePluginSourceRoot)
    $baseJson = ConvertTo-Json `
        @(ConvertTo-TreeIdentity $R33Tree) -Depth 6 -Compress
    $expectedJson = ConvertTo-Json $expectedIdentity -Depth 6 -Compress
    $currentJson = ConvertTo-Json `
        @(ConvertTo-TreeIdentity $current) -Depth 6 -Compress
    if ($currentJson -cne $baseJson -and $currentJson -cne $expectedJson) {
        throw 'Native plugin Source tree is neither the exact R33 commit tree nor its exact idempotent capture-source successor.'
    }

    $journal = [Collections.Generic.List[object]]::new()
    try {
        foreach ($pin in $captureSourcePins) {
            $source = [IO.Path]::GetFullPath(
                (Join-Path $repositoryUnrealRoot $pin.RelativePath))
            $destination = [IO.Path]::GetFullPath(
                (Join-Path $nativeProjectRoot $pin.RelativePath))
            if (-not (Test-ContainedPath $destination $nativePluginSourceRoot)) {
                throw "Native R33 capture source escaped plugin Source: $destination"
            }
            $before = Get-FileState $destination
            $journal.Add([pscustomobject] [ordered] @{
                Path=$destination
                Before=$before
                Created=(-not $before.Present)
            })
            if ($before.Present) {
                [void] (Assert-State ([pscustomobject] [ordered] @{
                    Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256
                }) $destination 'idempotent native R33 capture source')
            }
            else {
                [IO.Directory]::CreateDirectory(
                    [IO.Path]::GetDirectoryName($destination)) | Out-Null
                [IO.File]::Copy($source, $destination, $false)
            }
            [void] (Assert-State ([pscustomobject] [ordered] @{
                Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256
            }) $destination 'promoted native R33 capture source')
        }
        $promotedTree = @(Get-TreeReceipt $nativePluginSourceRoot)
        if ((ConvertTo-Json @(ConvertTo-TreeIdentity $promotedTree) `
                -Depth 6 -Compress) -cne $expectedJson) {
            throw 'Post-promotion R33 capture Source-tree identity drifted.'
        }
        [pscustomobject] [ordered] @{
            ExpectedTree=@($promotedTree)
            Journal=@($journal)
        }
    }
    catch {
        $promotionFailure = $_.Exception
        $internalRollbackErrors = [Collections.Generic.List[string]]::new()
        for ($index = $journal.Count - 1; $index -ge 0; --$index) {
            $entry = $journal[$index]
            try {
                if (-not (Test-ContainedPath `
                        $entry.Path $nativePluginSourceRoot)) {
                    throw "Promotion journal path escaped plugin Source: $($entry.Path)"
                }
                if ([bool] $entry.Created -and
                    [IO.File]::Exists($entry.Path)) {
                    [IO.File]::Delete($entry.Path)
                }
                [void] (Assert-State `
                    $entry.Before $entry.Path `
                    'internally rolled-back partial R33 capture source promotion')
            }
            catch { $internalRollbackErrors.Add($_.Exception.Message) }
        }
        if ($internalRollbackErrors.Count -ne 0) {
            throw "R33 capture source promotion failed and internal rollback was incomplete: failure={$($promotionFailure.Message)} rollback={$([string]::Join(' | ', @($internalRollbackErrors)))}"
        }
        throw $promotionFailure
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
            'rolled-back R33 capture source')
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
        $mapFile $Admission.Map 'R33 capture-bound map'
    $runtime = Get-PathBoundReceipt `
        $runtimeEditorDll $Admission.RuntimeDll 'R33 capture-bound runtime DLL'
    $editor = Get-PathBoundReceipt `
        $editorDll $Admission.EditorDll 'R33 capture-bound editor DLL'
    $groundH = Get-PathBoundReceipt `
        $groundHeader $Admission.GroundHeader 'R33 capture-bound Ground header'
    $groundC = Get-PathBoundReceipt `
        $groundSource $Admission.GroundSource 'R33 capture-bound Ground source'
    $tree = @(Assert-TreeReceipt `
        $nativePluginSourceRoot $ExpectedCaptureTree `
        'R33 capture-bound native plugin Source')
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
    $r34 = @(Get-ChildItem -LiteralPath $nativePluginSourceRoot `
        -Recurse -File -Force | Where-Object {
            [IO.Path]::GetRelativePath($nativePluginSourceRoot, $_.FullName) `
                -match 'R34'
        })
    if ($r34.Count -ne 0) {
        throw "R34 source is forbidden during R33 capture: $($r34[0].FullName)"
    }
}

function Initialize-MemoryProbeType {
    if ($null -ne ('Triad.R33Capture.MemoryProbe' -as [type])) { return }
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Triad.R33Capture
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
    [uint64] [Triad.R33Capture.MemoryProbe]::AvailableCommitBytes()
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $free = Get-FreeVirtualBytes
    if ($free -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "R33_CAPTURE_LAUNCH_HEADROOM_REFUSED label=$Label freeVirtualBytes=$free required=$minimumSystemFreeVirtualAtLaunchBytes"
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
            throw "R33 capture launch attempted to disable AirSim: $token"
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
        throw "R33 capture owned-process identity changed at $Checkpoint."
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
        'UTRIADIstanaExploreV5DR33Player0CaptureLibrary',
        'GetIstanaExploreV5DR33Player0CaptureState',
        'SetIstanaExploreV5DR33Player0Presentation',
        'SetIstanaExploreV5DR33Player0CapturePose',
        'CaptureIstanaExploreV5DR33Player0ComparisonView',
        'FinishIstanaExploreV5DR33Player0CaptureRun',
        'ISTANA_EXPLORE_V5D_R33_PLAYER0_COMPARISON_CAPTURE_ACCEPTED',
        'ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor',
        'ValidateR33CesiumWorldTerrainReference',
        'ATRIADIstanaExploreV5DR32MediumDistanceTurfActor',
        'ValidateR32MediumDistanceTurf',
        'AirSimTriadRuntime', 'WeatherActor')) {
        if (-not (Test-FileContainsMarker $gameExe $marker)) {
            throw "Fresh R33 Game binary lacks marker: $marker"
        }
    }
    foreach ($marker in @('TRIADIstanaExploreV5DR34', 'ISTANA_EXPLORE_V5D_R34')) {
        if (Test-FileContainsMarker $gameExe $marker) {
            throw "Fresh R33 Game binary contains forbidden R34 marker: $marker"
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
        throw 'Remote Control port 30010 is already owned before R33 capture launch.'
    }
}

function Assert-RcPortOwnedByGame {
    param($Owned)
    $listeners = @(Get-NetTCPConnection -LocalPort 30010 `
        -State Listen -ErrorAction SilentlyContinue)
    if ($listeners.Count -ne 1 -or
        [int] $listeners[0].OwningProcess -ne [int] $Owned.Handle.Id) {
        throw 'Remote Control port 30010 is not owned by the exact R33 Game process.'
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
    param(
        $Response,
        [string] $Presentation,
        $Pose,
        [string] $Label)
    if ([bool] (Get-RequiredPropertyValue $Response 'ReturnValue' $Label) -ne
        $true) {
        throw "$Label returned false."
    }
    $report = [string] (
        Get-RequiredPropertyValue $Response 'OutReport' $Label)
    $markers = @(
        'ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_STATE_VALID',
        'exactPlayer0=true', 'r33ControllerOwner=1',
        "presentation=$Presentation",
        'googleIonAssetId=2275207', 'cwtIonAssetId=1',
        'sharedGeoreference=true', 'doubleVisible=false',
        'providerReadyProofClaimed=false',
        'accurateRealWorldTerrainClaimed=false',
        'surveyAccuracyClaimed=false', 'verticalDatumResolved=false',
        'heightSamplesPersisted=false',
        'airSimTriadRuntimeLoaded=true', 'weatherActorResolved=true',
        'visualCaptureAccepted=false', 'mapModified=false',
        'simulationCollisionNavigationSensorRfModified=false')
    switch ($Presentation) {
        'GooglePrimary' {
            $markers += @('googleVisible=true', 'cwtVisible=false')
        }
        'CwtWarming' {
            $markers += @('googleVisible=true', 'cwtVisible=false')
        }
        'CwtPresented' {
            $markers += @('googleVisible=false', 'cwtVisible=true')
        }
        default { throw "$Label requested an inadmissible presentation." }
    }
    if ($null -ne $Pose) {
        $markers += @("poseId=$($Pose.Id)", 'exactQaViewPose=true')
    }
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
        Update-OwnedMemoryGuard $Owned 'waiting for R33 capture API'
        $Owned.Handle.Refresh()
        if ($Owned.Handle.HasExited) {
            throw "R33 Game exited before capture API readiness: code=$($Owned.Handle.ExitCode)"
        }
        try {
            Assert-RcPortOwnedByGame $Owned
            $response = Invoke-RcCall 'GetIstanaExploreV5DR33Player0CaptureState' @{} 5
            if ([string] $response.OutReport -match
                'presentation=SafeLocal') {
                throw 'R33_CAPTURE_SAFE_LOCAL_OBSERVED during capture API readiness.'
            }
            if ([bool] $response.ReturnValue -eq $true -and
                [string] $response.OutReport -match
                    'ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_STATE_VALID') {
                return [string] $response.OutReport
            }
            $last = [string] $response.OutReport
        }
        catch {
            if ($_.Exception.Message.Contains(
                    'R33_CAPTURE_SAFE_LOCAL_OBSERVED',
                    [StringComparison]::Ordinal)) {
                throw
            }
            $last = $_.Exception.Message
        }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    throw "R33 capture API did not become ready: $last"
}

function Request-PresentationAndWait {
    param(
        $Owned,
        [string] $Presentation,
        [int] $TimeoutSeconds)
    $request = Invoke-RcCall 'SetIstanaExploreV5DR33Player0Presentation' `
        @{ PresentationId=$Presentation } 5
    $message = [string] (
        Get-RequiredPropertyValue $request 'OutMessage' `
            "request $Presentation")
    if ([bool] $request.ReturnValue -ne $true -or
        -not $message.Contains(
            'ISTANA_EXPLORE_V5D_R33_PLAYER0_PRESENTATION_REQUEST_ACCEPTED',
            [StringComparison]::Ordinal) -or
        -not $message.Contains(
            "requested=$Presentation", [StringComparison]::Ordinal) -or
        -not $message.Contains(
            'directSafeLocalTriggerUsed=false', [StringComparison]::Ordinal) -or
        -not $message.Contains(
            'networkSabotageUsed=false', [StringComparison]::Ordinal) -or
        $message.Contains(
            'current=SafeLocal', [StringComparison]::Ordinal)) {
        throw "Unexpected R33 presentation acknowledgement: $message"
    }
    $warmingObserved = $Presentation -eq 'CwtPresented' -and
        $message.Contains('current=CwtWarming', [StringComparison]::Ordinal)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $stableStarted = $null
    $samples = 0
    $last = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        Update-OwnedMemoryGuard $Owned "waiting for $Presentation"
        $response = Invoke-RcCall 'GetIstanaExploreV5DR33Player0CaptureState' @{} 5
        $report = [string] $response.OutReport
        if ($report.Contains(
                'presentation=SafeLocal',
                [StringComparison]::Ordinal)) {
            throw 'R33 presentation entered SafeLocal; this run cannot claim no failover/recovery diagnostic.'
        }
        if ([bool] $response.ReturnValue -eq $true -and
            $report.Contains(
                'presentation=CwtWarming',
                [StringComparison]::Ordinal)) {
            [void] (Assert-ExactStateResponse `
                $response 'CwtWarming' $null 'CWT warming observation')
            $warmingObserved = $true
            $stableStarted = $null
            $samples = 0
        }
        elseif ([bool] $response.ReturnValue -eq $true -and
            $report.Contains(
                "presentation=$Presentation",
                [StringComparison]::Ordinal)) {
            $last = Assert-ExactStateResponse `
                $response $Presentation $null "stable $Presentation"
            ++$samples
            if ($null -eq $stableStarted) {
                $stableStarted = [DateTime]::UtcNow
            }
            if (([DateTime]::UtcNow - $stableStarted).TotalSeconds -ge 3.0) {
                if ($Presentation -eq 'CwtPresented' -and
                    -not $warmingObserved) {
                    throw 'CwtPresented was reached without an observed CwtWarming state.'
                }
                return [pscustomobject] [ordered] @{
                    RequestedPresentation=$Presentation
                    RequestAcknowledgement=$message
                    WarmingObserved=$warmingObserved
                    StableSeconds=3
                    Samples=$samples
                    FinalReport=$last
                }
            }
        }
        else {
            $last = $report
            $stableStarted = $null
            $samples = 0
        }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    throw "R33 presentation did not stabilize: requested=$Presentation warmingObserved=$warmingObserved last={$last}"
}

function Wait-StablePose {
    param($Owned, $Plan, [int] $TimeoutSeconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $stableStarted = $null
    $samples = 0
    $last = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        Update-OwnedMemoryGuard $Owned `
            "holding $($Plan.Presentation) pose $($Plan.Pose.Id)"
        try {
            $response = Invoke-RcCall 'GetIstanaExploreV5DR33Player0CaptureState' @{} 5
            if ([string] $response.OutReport -match
                'presentation=SafeLocal') {
                throw "R33_CAPTURE_SAFE_LOCAL_OBSERVED while holding $($Plan.Id)."
            }
            $last = Assert-ExactStateResponse `
                $response $Plan.Presentation $Plan.Pose `
                "stable $($Plan.Presentation) pose $($Plan.Pose.Id)"
            ++$samples
            if ($null -eq $stableStarted) {
                $stableStarted = [DateTime]::UtcNow
            }
            if (([DateTime]::UtcNow - $stableStarted).TotalSeconds -ge 8.0) {
                return [pscustomobject] [ordered] @{
                    Presentation=$Plan.Presentation
                    PoseId=$Plan.Pose.Id
                    StableSeconds=8
                    Samples=$samples
                    FinalReport=$last
                }
            }
        }
        catch {
            if ($_.Exception.Message.Contains(
                    'R33_CAPTURE_SAFE_LOCAL_OBSERVED',
                    [StringComparison]::Ordinal)) {
                throw
            }
            $last = $_.Exception.Message
            $stableStarted = $null
            $samples = 0
        }
        Start-Sleep -Milliseconds $memoryWatchdogPollMilliseconds
    }
    throw "R33 pose/state did not remain stable: plan=$($Plan.Id) last={$last}"
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
    $expectedPoseIds = @(
        '075m','020m','095m','surroundings_oblique_macdonald')
    $expectedPlan = @(
        'GooglePrimary__075m',
        'GooglePrimary__020m',
        'GooglePrimary__095m',
        'GooglePrimary__surroundings_oblique_macdonald',
        'CwtPresented__075m',
        'CwtPresented__020m',
        'CwtPresented__095m',
        'CwtPresented__surroundings_oblique_macdonald')
    if ($poses.Count -ne 4 -or
        (ConvertTo-Json @($poses.Id) -Compress) -cne
            (ConvertTo-Json $expectedPoseIds -Compress) -or
        (ConvertTo-Json $presentations -Compress) -cne
            (ConvertTo-Json @('GooglePrimary','CwtPresented') -Compress) -or
        $capturePlan.Count -ne 8 -or
        (ConvertTo-Json @($capturePlan.Id) -Compress) -cne
            (ConvertTo-Json $expectedPlan -Compress)) {
        throw 'R33 capture pose/presentation roster drifted.'
    }
    $headerPath = Join-Path $repositoryUnrealRoot `
        $captureSourcePins[0].RelativePath
    $sourcePath = Join-Path $repositoryUnrealRoot `
        $captureSourcePins[1].RelativePath
    $header = [IO.File]::ReadAllText($headerPath)
    $source = [IO.File]::ReadAllText($sourcePath)
    foreach ($marker in @(
        'GetIstanaExploreV5DR33Player0CaptureState',
        'SetIstanaExploreV5DR33Player0Presentation',
        'SetIstanaExploreV5DR33Player0CapturePose',
        'CaptureIstanaExploreV5DR33Player0ComparisonView',
        'FinishIstanaExploreV5DR33Player0CaptureRun')) {
        if (-not $header.Contains($marker, [StringComparison]::Ordinal) -or
            -not $source.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R33 capture library lacks endpoint marker: $marker"
        }
    }
    foreach ($marker in @(
        'static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 4)',
        'ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor',
        'ValidateR33CesiumWorldTerrainReference',
        'RequestCwtPresentation',
        'ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary',
        'ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented',
        'GooglePhotorealistic3DTilesIonAssetId = 2275207',
        'CesiumWorldTerrainIonAssetId = 1',
        '#include "CesiumIonServer.h"',
        '!IsValid(OutState.GoogleTileset->GetCesiumIonServer())',
        '!IsValid(OutState.CwtTileset->GetCesiumIonServer())',
        'OutState.GoogleTileset->GetCesiumIonServer() !=',
        'OutState.CwtTileset->GetCesiumIonServer()',
        'ATRIADIstanaExploreV5DR32MediumDistanceTurfActor',
        'ValidateR32MediumDistanceTurf',
        'ValidateGroundVegetationRealism',
        'ValidateR29Vegetation',
        'ValidateCopernicusTerrainFallback',
        'ValidateTreeRealism',
        'AirSimTriadRuntime', 'WeatherActor',
        'SetResolution(2560, 1440, 1.0f)',
        'providerReadyProofClaimed=false',
        'accurateRealWorldTerrainClaimed=false',
        'surveyAccuracyClaimed=false',
        'verticalDatumResolved=false',
        'heightSamplesPersisted=false',
        'restoredPresentation=GooglePrimary')) {
        if (-not $source.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R33 capture source lacks fail-closed marker: $marker"
        }
    }
    if ($source.Contains('->EnterCwtPresented(',
            [StringComparison]::Ordinal) -or
        $source.Contains('GetIonAccessToken(',
            [StringComparison]::Ordinal) -or
        $source.Contains('DefaultIonAccessToken',
            [StringComparison]::Ordinal) -or
        $header.Contains('R34', [StringComparison]::Ordinal) -or
        $source.Contains('R34', [StringComparison]::Ordinal)) {
        throw 'R33 capture C++ bypasses readiness or contains R34 source.'
    }

    $contractPath = Join-Path $repositoryUnrealRoot `
        'SourceAssets\IstanaPublicViewExploreV5D\Terrain\R33CesiumWorldTerrainReference\r33_player0_visual_review.contract.json'
    $contract = Get-Content -LiteralPath $contractPath -Raw |
        ConvertFrom-Json -Depth 64
    $gate = Get-RequiredPropertyValue `
        $contract 'nativePlayer0CaptureGate' 'R33 visual-review contract'
    if ([string] $contract.schema -cne
            'triad.istana_explore_v5d.r33_player0_visual_review.v1' -or
        [string] $gate.schema -cne $schema -or
        [string] $gate.wrapper -cne
            'scripts/Capture-IstanaExploreV5DR33Player0Evidence.ps1' -or
        [bool] $gate.implemented -ne $true -or
        [bool] $gate.executed -ne $false -or
        [bool] $gate.accepted -ne $false -or
        [int] $gate.exactSourceTreeAdditionCount -ne 2 -or
        [int] $gate.nativeContentPackageCount -ne 0 -or
        [int] $gate.exactPoseCount -ne 4 -or
        [int] $gate.exactCoreCaptureCount -ne 8 -or
        (ConvertTo-Json @($gate.exactPoseIds) -Compress) -cne
            (ConvertTo-Json $expectedPoseIds -Compress) -or
        (ConvertTo-Json @($gate.exactPresentationStates) -Compress) -cne
            (ConvertTo-Json $presentations -Compress) -or
        (ConvertTo-Json @($gate.exactCaptureOrder) -Compress) -cne
            (ConvertTo-Json $expectedPlan -Compress) -or
        [bool] $gate.twoPhaseHumanVisualReviewRequired -ne $true -or
        [bool] $gate.executeCanAcceptVisualQuality -ne $false -or
        [bool] $gate.acceptanceLaunchesUnreal -ne $false -or
        [bool] $gate.acceptanceWritesNativeProject -ne $false -or
        [bool] $gate.r33CommitReceiptRequired -ne $true -or
        [bool] $gate.r33ReceiptMustBindAndReplayFullAcceptedR30R31R32Chain -ne $true -or
        [bool] $gate.fullNativePluginSourceTreeReceiptRequired -ne $true -or
        [bool] $gate.mapDllGroundNoMutationBindingsRequired -ne $true -or
        [bool] $gate.r34SourceAllowed -ne $false -or
        [bool] $gate.r34AdmissionAuthorized -ne $false -or
        [int64] $gate.googlePhotorealisticIonAssetId -ne 2275207L -or
        [int64] $gate.cesiumWorldTerrainIonAssetId -ne 1L -or
        [bool] $gate.sameGeoreferenceAndIonServerRequired -ne $true -or
        [bool] $gate.exactNonNullOpaqueIonServerRequired -ne $true -or
        [bool] $gate.serverObjectValidityProvesTokenPresence -ne $false -or
        [bool] $gate.serverObjectValidityProvesAssetEntitlement -ne $false -or
        [bool] $gate.ionTokenValueOrFingerprintInspectedByTriad -ne $false -or
        [bool] $gate.naturalCwtWarmingTransitionRequired -ne $true -or
        [bool] $gate.directEnterCwtPresentedAllowed -ne $false -or
        [bool] $gate.restoreGooglePrimaryBeforeExitRequired -ne $true -or
        [bool] $gate.safeLocalDiagnostic.included -ne $false -or
        [bool] $gate.safeLocalDiagnostic.deterministicallyTriggerableWithoutNetworkSabotage -ne $false -or
        [bool] $gate.safeLocalDiagnostic.networkOrEntitlementFailureInjected -ne $false -or
        [bool] $gate.safeLocalDiagnostic.failoverOrRecoveryAccepted -ne $false -or
        [int64] $gate.fixedLaunchFreeVirtualBytes -ne
            $minimumSystemFreeVirtualAtLaunchBytes -or
        [int64] $gate.fixedOwnedTreePrivateMemoryCeilingBytes -ne
            $privateMemoryCeilingBytes -or
        [int64] $gate.fixedContinuousFreeVirtualBytes -ne
            $minimumSystemFreeVirtualBytes -or
        [bool] $gate.serialFreshBuild -ne $true -or
        [bool] $gate.freshIsolatedCook -ne $true -or
        [bool] $gate.airSimTriadRuntimeRetained -ne $true -or
        [bool] $gate.providerReadyProofClaimed -ne $false -or
        [bool] $gate.accurateRealWorldTerrainClaimed -ne $false -or
        [bool] $gate.surveyAccuracyClaimed -ne $false -or
        [bool] $gate.verticalDatumResolved -ne $false -or
        [bool] $gate.heightCheckpointValidationComplete -ne $false -or
        [bool] $gate.performanceAcceptanceClaimed -ne $false -or
        [bool] $gate.hyperrealismClaimed -ne $false) {
        throw 'R33 visual-review contract drifted from the guarded wrapper.'
    }
    if (@($gate.captureSourceFiles).Count -ne 2) {
        throw 'R33 visual-review contract must pin exactly two capture sources.'
    }
    for ($index = 0; $index -lt 2; ++$index) {
        $contractPin = $gate.captureSourceFiles[$index]
        $wrapperPin = $captureSourcePins[$index]
        $expectedPath = ('unreal/' +
            $wrapperPin.RelativePath.Replace('\','/'))
        if ([string] $contractPin.path -cne $expectedPath -or
            [int64] $contractPin.bytes -ne [int64] $wrapperPin.Bytes -or
            [string] $contractPin.sha256 -cne [string] $wrapperPin.Sha256) {
            throw "R33 capture source pin drifted at index $index."
        }
    }
    [pscustomobject] [ordered] @{
        Schema=$schema
        ReviewContractSchema=[string] $contract.schema
        Status='STATIC_SELF_CHECK_PASS'
        RepositoryOnly=$true
        NativeAccess=$false
        UnrealLaunched=$false
        ExactPoseCount=4
        PoseIds=@($poses.Id)
        ExactPresentationStateCount=2
        PresentationStates=@($presentations)
        ExactCoreCaptureCount=8
        CaptureOrder=@($capturePlan.Id)
        ExactResolution='2560x1440'
        SdrRequired=$true
        DecodedDistinctPngProofRequired=$true
        R33CommitReceiptRequired=$true
        FullAcceptedR30R31R32ChainReplayRequired=$true
        R31CaptureSchema=$r31CaptureSchema
        R31ExactImageCount=6
        R31NaniteRasterPairReviewRequired=$true
        FullNativePluginSourceTreeReceiptRequired=$true
        MapDllGroundNoMutationBindingsRequired=$true
        NaturalCwtWarmingTransitionRequired=$true
        NonNullOpaqueIonServerRequired=$true
        IonTokenValueOrFingerprintInspectedByTriad=$false
        NonNullServerProvesTokenOrEntitlement=$false
        RestoreGooglePrimaryBeforeExitRequired=$true
        SafeLocalDiagnosticIncluded=$false
        SafeLocalDeterministicallyTriggerableWithoutNetworkSabotage=$false
        R34DependencyAllowed=$false
        R34AdmissionAuthorized=$false
        FixedLaunchFreeVirtualBytes=$minimumSystemFreeVirtualAtLaunchBytes
        FixedOwnedTreePrivateMemoryCeilingBytes=$privateMemoryCeilingBytes
        FixedContinuousFreeVirtualBytes=$minimumSystemFreeVirtualBytes
        SerialFreshBuild=$true
        FreshIsolatedCook=$true
        TwoPhaseHumanVisualReview=$true
        ExecuteCanAcceptVisualQuality=$false
        ProviderReadyProofClaimed=$false
        AccurateRealWorldTerrainClaimed=$false
        SurveyAccuracyClaimed=$false
        VerticalDatumResolved=$false
        HeightCheckpointValidationComplete=$false
        PerformanceAcceptanceClaimed=$false
        HyperrealismClaimed=$false
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
        throw 'Pending R33 receipt path is not the exact run-token evidence child.'
    }
    $file = Get-FileState $fullPath
    if (-not $file.Present -or
        $file.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw 'Pending R33 visual-review receipt hash mismatch.'
    }
    $receipt = Get-Content -LiteralPath $fullPath -Raw |
        ConvertFrom-Json -Depth 64
    if ([string] (Get-RequiredPropertyValue $receipt 'Schema' 'pending R33 receipt') -cne $schema -or
        [string] (Get-RequiredPropertyValue $receipt 'Status' 'pending R33 receipt') -cne 'PENDING_VISUAL_REVIEW' -or
        [string] (Get-RequiredPropertyValue $receipt 'RunToken' 'pending R33 receipt') -cne $ExpectedRunToken -or
        [string] (Get-RequiredPropertyValue $receipt 'NativeOrder' 'pending R33 receipt') -cne 'R33_COMMIT_THEN_R33_CAPTURE_NO_R34_AUTHORITY' -or
        [bool] (Get-RequiredPropertyValue $receipt 'R34DependencyAllowed' 'pending R33 receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactPoseCount' 'pending R33 receipt') -ne 4 -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactPresentationStateCount' 'pending R33 receipt') -ne 2 -or
        [int] (Get-RequiredPropertyValue $receipt 'ExactCoreCaptureCount' 'pending R33 receipt') -ne 8 -or
        [bool] (Get-RequiredPropertyValue $receipt 'MechanicalCaptureValidationPassed' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'ExplicitHumanReviewAcceptance' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ConfirmedEightImagesReviewed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewRequired' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'VisualReviewAccepted' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HumanVisualReviewAttested' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'AutomaticVisualAcceptanceAllowed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'R33CesiumWorldTerrainVisualQaAccepted' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'GooglePrimaryVisualQaAccepted' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CwtPresentedVisualQaAccepted' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'TreeMaterialResponseV3Preserved' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'R34AdmissionAuthorized' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'MapModifiedByCapture' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SerializedProviderConfigurationModified' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SimulationCollisionNavigationSensorRfModified' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderReadyProofClaimed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderReadyCaptureAccepted' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'ProviderTermsOrEntitlementVerified' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NonNullOpaqueIonServerRequired' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'IonTokenValueOrFingerprintInspectedByTriad' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NonNullServerProvesTokenOrEntitlement' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'AccurateRealWorldTerrainClaimed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SurveyAccuracyClaimed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'VerticalDatumResolved' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HeightCheckpointValidationComplete' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'PerformanceAcceptanceClaimed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'HyperrealismClaimed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'BotanicalSurveyOrCurrentConditionClaimed' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'SafeLocalDiagnosticIncluded' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'NetworkOrEntitlementFailureInjected' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'FailoverOrRecoveryAccepted' 'pending R33 receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $receipt 'CwtWarmingObservedAtLeastOnce' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'NoDoubleVisibleObserved' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'RuntimePresentationStateTransitionsPerformed' 'pending R33 receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $receipt 'RestoredGooglePrimaryBeforeExit' 'pending R33 receipt') -ne $true) {
        throw 'Pending R33 receipt failed the exact two-phase truth contract.'
    }

    $commitAdmission = Get-RequiredPropertyValue $receipt 'R33CommitAdmission' 'pending R33 receipt'
    $commitPath = [string] (Get-RequiredPropertyValue $commitAdmission 'Path' 'R33 commit admission')
    $commitFile = Get-RequiredPropertyValue $commitAdmission 'File' 'R33 commit admission'
    $admission = Assert-R33CommitReceipt $commitPath ([string] $commitFile.Sha256)
    if (-not [IO.Path]::GetFullPath($commitPath).Equals(
            $admission.R33.Path, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-StateMatches $commitFile $admission.R33.State) -or
        [string] (Get-RequiredPropertyValue $commitAdmission 'CallerSha256' 'R33 commit admission') -cne [string] $commitFile.Sha256 -or
        [string] (Get-RequiredPropertyValue $commitAdmission 'Schema' 'R33 commit admission') -cne $r33TransactionSchema -or
        [string] (Get-RequiredPropertyValue $commitAdmission 'Status' 'R33 commit admission') -cne 'COMMITTED') {
        throw 'Pending receipt no longer binds its exact R33 commit receipt.'
    }
    foreach ($commitBinding in @(
        [pscustomobject] @{ Name='SuccessorMap'; State=$admission.Map }
        [pscustomobject] @{ Name='RuntimeDllAfter'; State=$admission.RuntimeDll }
        [pscustomobject] @{ Name='EditorDllAfter'; State=$admission.EditorDll }
        [pscustomobject] @{ Name='GroundHeaderAfter'; State=$admission.GroundHeader }
        [pscustomobject] @{ Name='GroundSourceAfter'; State=$admission.GroundSource })) {
        $embeddedState = Get-RequiredPropertyValue `
            $commitAdmission $commitBinding.Name 'R33 commit admission'
        if (-not (Test-StateMatches $embeddedState $commitBinding.State)) {
            throw "Pending R33 commit admission drifted at $($commitBinding.Name)."
        }
    }
    $embeddedTree = @(Get-RequiredPropertyValue `
        $commitAdmission 'NativePluginSourceTreeAfter' 'R33 commit admission')
    if ((ConvertTo-Json $embeddedTree -Depth 6 -Compress) -cne
        (ConvertTo-Json @($admission.NativePluginSourceTree) `
            -Depth 6 -Compress)) {
        throw 'Pending R33 commit admission plugin Source tree drifted.'
    }
    if ((ConvertTo-Json (Get-RequiredPropertyValue `
                $commitAdmission 'TreeMaterialResponseV3' 'R33 commit admission') `
            -Depth 16 -Compress) -cne
        (ConvertTo-Json $admission.TreeMaterialResponseV3 `
            -Depth 16 -Compress)) {
        throw 'Pending R33 commit admission TreeRealism v3 identity drifted.'
    }
    foreach ($chainBinding in @(
        [pscustomobject] @{ Name='R30TransactionAdmission'; Admission=$admission.R30Transaction }
        [pscustomobject] @{ Name='R30CaptureAdmission'; Admission=$admission.R30Capture }
        [pscustomobject] @{ Name='R31TransactionAdmission'; Admission=$admission.R31Transaction }
        [pscustomobject] @{ Name='R31CaptureAdmission'; Admission=$admission.R31Capture }
        [pscustomobject] @{ Name='R32TransactionAdmission'; Admission=$admission.R32Transaction }
        [pscustomobject] @{ Name='R32CaptureAdmission'; Admission=$admission.R32Capture })) {
        $embedded = Get-RequiredPropertyValue $commitAdmission $chainBinding.Name 'R33 commit admission'
        $embeddedPath = [string] (Get-RequiredPropertyValue `
            $embedded 'Path' $chainBinding.Name)
        $embeddedFile = Get-RequiredPropertyValue `
            $embedded 'File' $chainBinding.Name
        if (-not [IO.Path]::GetFullPath($embeddedPath).Equals(
                $chainBinding.Admission.Path,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-StateMatches `
                $embeddedFile $chainBinding.Admission.State)) {
            throw "Pending R33 commit admission drifted at $($chainBinding.Name)."
        }
    }

    $binding = Get-RequiredPropertyValue $receipt 'BindingEvidence' 'pending R33 receipt'
    foreach ($row in @(
        [pscustomobject] @{ Name='MapCaptureBinding'; Path=$mapFile; State=$admission.Map; Label='map' }
        [pscustomobject] @{ Name='RuntimeEditorDllCaptureBinding'; Path=$runtimeEditorDll; State=$admission.RuntimeDll; Label='runtime DLL' }
        [pscustomobject] @{ Name='EditorDllCaptureBinding'; Path=$editorDll; State=$admission.EditorDll; Label='editor DLL' })) {
        $bound = Get-RequiredPropertyValue $binding $row.Name 'pending R33 binding'
        if (-not [IO.Path]::GetFullPath([string] $bound.Path).Equals(
                $row.Path, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-StateMatches $bound.State $row.State)) {
            throw "Pending R33 $($row.Label) binding drifted."
        }
        [void] (Assert-State $row.State $row.Path "pending R33 $($row.Label)")
    }
    foreach ($row in @(
        [pscustomobject] @{ Name='GroundHeaderCaptureBinding'; Path=$groundHeader; State=$admission.GroundHeader; Label='Ground header' }
        [pscustomobject] @{ Name='GroundSourceCaptureBinding'; Path=$groundSource; State=$admission.GroundSource; Label='Ground source' })) {
        $bound = Get-RequiredPropertyValue $binding $row.Name 'pending R33 binding'
        if (-not [IO.Path]::GetFullPath([string] $bound.Path).Equals(
                $row.Path, [StringComparison]::OrdinalIgnoreCase) -or
            [bool] $bound.Unchanged -ne $true -or
            -not (Test-StateMatches $bound $row.State) -or
            -not (Test-StateMatches $bound.ImmediatePreCapture $row.State) -or
            -not (Test-StateMatches $bound.ImmediatePostCapture $row.State)) {
            throw "Pending R33 $($row.Label) no-mutation binding drifted."
        }
        [void] (Assert-State $row.State $row.Path "pending R33 $($row.Label)")
    }
    $treeBinding = Get-RequiredPropertyValue $binding 'NativePluginSourceTreeCaptureBinding' 'pending R33 binding'
    $expectedTree = @(Get-RequiredPropertyValue $treeBinding 'BaselineAfterCaptureSourcePromotion' 'pending R33 tree')
    if (-not [IO.Path]::GetFullPath([string] $treeBinding.Root).Equals(
            $nativePluginSourceRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [bool] $treeBinding.Unchanged -ne $true -or
        [int] $treeBinding.FileCount -ne $expectedTree.Count -or
        (ConvertTo-Json $expectedTree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($treeBinding.ImmediatePreCapture) -Depth 6 -Compress) -or
        (ConvertTo-Json $expectedTree -Depth 6 -Compress) -cne
            (ConvertTo-Json @($treeBinding.ImmediatePostCapture) -Depth 6 -Compress)) {
        throw 'Pending R33 native plugin Source-tree no-mutation binding drifted.'
    }
    [void] (Assert-TreeReceipt $nativePluginSourceRoot $expectedTree 'pending R33 plugin Source')

    $captures = @(Get-RequiredPropertyValue $receipt 'Captures' 'pending R33 receipt')
    if ($captures.Count -ne 8) {
        throw 'Pending R33 capture must contain exactly eight image rows.'
    }
    $stateFileHashes = @{}
    $statePixelHashes = @{}
    foreach ($state in $presentations) {
        $stateFileHashes[$state] = [Collections.Generic.HashSet[string]]::new(
            [StringComparer]::Ordinal)
        $statePixelHashes[$state] = [Collections.Generic.HashSet[string]]::new(
            [StringComparer]::Ordinal)
    }
    $obliquePixelHashes = @{}
    $stateTransitionIdentity = @{}
    for ($index = 0; $index -lt $capturePlan.Count; ++$index) {
        $row = $captures[$index]
        $expected = $capturePlan[$index]
        $pose = Get-RequiredPropertyValue $row 'Pose' "pending R33 capture row $index"
        $image = Get-RequiredPropertyValue $row 'Image' "pending R33 capture row $index"
        $transition = Get-RequiredPropertyValue `
            $row 'PresentationTransition' "pending R33 capture row $index"
        $transitionMessage = [string] (Get-RequiredPropertyValue `
            $transition 'RequestAcknowledgement' "pending R33 transition row $index")
        $transitionReport = [string] (Get-RequiredPropertyValue `
            $transition 'FinalReport' "pending R33 transition row $index")
        $expectedWarming = $expected.Presentation -eq 'CwtPresented'
        $expectedCurrent = if ($expectedWarming) {
            'current=CwtWarming'
        }
        else {
            'current=GooglePrimary'
        }
        if ([string] (Get-RequiredPropertyValue $transition `
                'RequestedPresentation' "pending R33 transition row $index") -cne
                $expected.Presentation -or
            [bool] (Get-RequiredPropertyValue $transition `
                'WarmingObserved' "pending R33 transition row $index") -ne
                $expectedWarming -or
            [int] (Get-RequiredPropertyValue $transition `
                'StableSeconds' "pending R33 transition row $index") -ne 3 -or
            [int] (Get-RequiredPropertyValue $transition `
                'Samples' "pending R33 transition row $index") -lt 1 -or
            -not $transitionMessage.Contains(
                'ISTANA_EXPLORE_V5D_R33_PLAYER0_PRESENTATION_REQUEST_ACCEPTED',
                [StringComparison]::Ordinal) -or
            -not $transitionMessage.Contains(
                "requested=$($expected.Presentation)",
                [StringComparison]::Ordinal) -or
            -not $transitionMessage.Contains(
                $expectedCurrent, [StringComparison]::Ordinal) -or
            -not $transitionMessage.Contains(
                'directSafeLocalTriggerUsed=false',
                [StringComparison]::Ordinal) -or
            -not $transitionMessage.Contains(
                'networkSabotageUsed=false',
                [StringComparison]::Ordinal) -or
            $transitionMessage.Contains(
                'current=SafeLocal', [StringComparison]::Ordinal) -or
            -not $transitionReport.Contains(
                'ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_STATE_VALID',
                [StringComparison]::Ordinal) -or
            -not $transitionReport.Contains(
                "presentation=$($expected.Presentation)",
                [StringComparison]::Ordinal) -or
            -not $transitionReport.Contains(
                'doubleVisible=false', [StringComparison]::Ordinal) -or
            $transitionReport.Contains(
                'presentation=SafeLocal', [StringComparison]::Ordinal)) {
            throw "Pending R33 transition row $index failed grouped readiness replay."
        }
        $transitionIdentity = ConvertTo-Json $transition -Depth 8 -Compress
        if ($stateTransitionIdentity.ContainsKey($expected.Presentation)) {
            if ([string] $stateTransitionIdentity[$expected.Presentation] -cne
                $transitionIdentity) {
                throw "Pending R33 $($expected.Presentation) rows do not bind one grouped transition."
            }
        }
        else {
            $stateTransitionIdentity[$expected.Presentation] = $transitionIdentity
        }
        $slug = if ($expected.Presentation -eq 'GooglePrimary') {
            'google_primary'
        }
        else {
            'cwt_presented'
        }
        $expectedImagePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            ('explore_v5d_r33_player0_{0}_{1}_{2}.png' -f
                $slug, $expected.Pose.Id, $ExpectedRunToken)))
        if ([string] $row.Presentation -cne $expected.Presentation -or
            [string] $pose.Id -cne $expected.Pose.Id -or
            [double] $pose.X -ne [double] $expected.Pose.X -or
            [double] $pose.Y -ne [double] $expected.Pose.Y -or
            [double] $pose.Z -ne [double] $expected.Pose.Z -or
            [double] $pose.Pitch -ne [double] $expected.Pose.Pitch -or
            [double] $pose.Yaw -ne [double] $expected.Pose.Yaw -or
            [double] $pose.Roll -ne [double] $expected.Pose.Roll -or
            [string] $pose.ReviewPurpose -cne $expected.Pose.ReviewPurpose -or
            [bool] $row.NoDoubleVisibleObserved -ne $true -or
            [bool] $row.ProviderReadyProofClaimed -ne $false -or
            [bool] $row.AccurateRealWorldTerrainClaimed -ne $false -or
            -not [IO.Path]::GetFullPath([string] $image.Path).Equals(
                $expectedImagePath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Pending R33 capture row $index identity drifted."
        }
        if ($expected.Presentation -eq 'CwtPresented' -and
            [bool] $row.CwtPresentedReachedThroughReadinessStateMachine -ne $true) {
            throw "Pending R33 CWT row $index lacks readiness-state-machine proof."
        }
        $decoded = Get-PngReceipt $expectedImagePath
        if (-not (Test-StateMatches $decoded $image) -or
            [string] $decoded.DecodedBgraSha256 -cne
                [string] $image.DecodedBgraSha256 -or
            [int] $decoded.WidthPixels -ne 2560 -or
            [int] $decoded.HeightPixels -ne 1440 -or
            [bool] $decoded.NonBlank -ne $true -or
            -not $stateFileHashes[$expected.Presentation].Add(
                [string] $decoded.Sha256) -or
            -not $statePixelHashes[$expected.Presentation].Add(
                [string] $decoded.DecodedBgraSha256)) {
            throw "Pending R33 PNG row $index failed decode/hash/state-local distinctness replay."
        }
        if ($expected.Pose.Id -eq 'surroundings_oblique_macdonald') {
            $obliquePixelHashes[$expected.Presentation] =
                [string] $decoded.DecodedBgraSha256
        }
    }
    foreach ($state in $presentations) {
        if ($stateFileHashes[$state].Count -ne 4 -or
            $statePixelHashes[$state].Count -ne 4) {
            throw "Pending R33 $state images are not four-way distinct."
        }
    }
    if ($obliquePixelHashes['GooglePrimary'] -ceq
        $obliquePixelHashes['CwtPresented']) {
        throw 'R33 broad surrounding Google/CWT comparison images are identical.'
    }
    Assert-NoSuccessorNativeSource
    [void] (Assert-State $file $fullPath 'hash-pinned pending R33 visual-review receipt')
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
    Assert-NativeIdle 'before explicit R33 visual-review acceptance'
    $pendingAdmission = Assert-PendingCaptureReceipt `
        $Path $ExpectedSha256 $ExpectedRunToken
    $pending = $pendingAdmission.Receipt
    $binding = $pendingAdmission.BindingEvidence
    $immutable = Get-RequiredPropertyValue `
        $pending 'ImmutableContent' 'pending R33 receipt'
    Assert-ImmutableContentSnapshot $immutable
    $before = Assert-CaptureBindings `
        $pendingAdmission.Admission $pendingAdmission.CurrentTree
    $after = Assert-CaptureBindings `
        $pendingAdmission.Admission $pendingAdmission.CurrentTree
    if ((ConvertTo-Json $before -Depth 16 -Compress) -cne
        (ConvertTo-Json $after -Depth 16 -Compress)) {
        throw 'Native state changed during explicit R33 visual acceptance.'
    }
    [void] (Assert-State `
        $pendingAdmission.File $pendingAdmission.Path `
        'pending R33 receipt immediately before acceptance publication')
    Assert-ImmutableContentSnapshot $immutable
    Assert-NativeIdle 'before explicit R33 visual acceptance publication'

    $accepted = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='COMMITTED'
        RunToken=$ExpectedRunToken
        NativeOrder='R33_COMMIT_THEN_R33_CAPTURE_NO_R34_AUTHORITY'
        R34DependencyAllowed=$false
        PendingVisualReviewReceipt=[pscustomobject] [ordered] @{
            Path=$pendingAdmission.Path
            State=$pendingAdmission.File
            File=$pendingAdmission.File
            CallerSha256=$ExpectedSha256.ToUpperInvariant()
            Status='PENDING_VISUAL_REVIEW'
        }
        StaticReceipt=(Get-RequiredPropertyValue `
            $pending 'StaticReceipt' 'pending R33 receipt')
        R33CommitAdmission=(Get-RequiredPropertyValue `
            $pending 'R33CommitAdmission' 'pending R33 receipt')
        Map=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'MapCaptureBinding' 'R33 binding') `
            'State' 'R33 map binding')
        RuntimeEditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'RuntimeEditorDllCaptureBinding' 'R33 binding') `
            'State' 'R33 runtime DLL binding')
        EditorDll=(Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $binding 'EditorDllCaptureBinding' 'R33 binding') `
            'State' 'R33 editor DLL binding')
        GroundHeader=(Get-RequiredPropertyValue `
            $binding 'GroundHeaderCaptureBinding' 'R33 binding')
        GroundSource=(Get-RequiredPropertyValue `
            $binding 'GroundSourceCaptureBinding' 'R33 binding')
        NativePluginSourceTree=(Get-RequiredPropertyValue `
            $binding 'NativePluginSourceTreeCaptureBinding' 'R33 binding')
        SourcePromotion=(Get-RequiredPropertyValue `
            $pending 'SourcePromotion' 'pending R33 receipt')
        GameBinaryBefore=(Get-RequiredPropertyValue `
            $pending 'GameBinaryBefore' 'pending R33 receipt')
        GameBinaryAfter=(Get-RequiredPropertyValue `
            $pending 'GameBinaryAfter' 'pending R33 receipt')
        Build=(Get-RequiredPropertyValue `
            $pending 'Build' 'pending R33 receipt')
        Cook=(Get-RequiredPropertyValue `
            $pending 'Cook' 'pending R33 receipt')
        CookedClosure=(Get-RequiredPropertyValue `
            $pending 'CookedClosure' 'pending R33 receipt')
        Game=(Get-RequiredPropertyValue `
            $pending 'Game' 'pending R33 receipt')
        Captures=(Get-RequiredPropertyValue `
            $pending 'Captures' 'pending R33 receipt')
        ExactPoseCount=4
        ExactPresentationStateCount=2
        ExactCoreCaptureCount=8
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$true
        ConfirmedEightImagesReviewed=$true
        HumanVisualReviewAttested=$true
        AutomaticVisualAcceptanceAllowed=$false
        VisualReviewRequired=$false
        VisualReviewAccepted=$true
        R33CesiumWorldTerrainVisualQaAccepted=$true
        GooglePrimaryVisualQaAccepted=$true
        CwtPresentedVisualQaAccepted=$true
        TreeMaterialResponseV3Preserved=$true
        R34AdmissionAuthorized=$false
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        ProviderTermsOrEntitlementVerified=$false
        NonNullOpaqueIonServerRequired=$true
        IonTokenValueOrFingerprintInspectedByTriad=$false
        NonNullServerProvesTokenOrEntitlement=$false
        AccurateRealWorldTerrainClaimed=$false
        SurveyAccuracyClaimed=$false
        VerticalDatumResolved=$false
        HeightCheckpointValidationComplete=$false
        MapModifiedByCapture=$false
        SerializedProviderConfigurationModified=$false
        SimulationCollisionNavigationSensorRfModified=$false
        PerformanceAcceptanceClaimed=$false
        HyperrealismClaimed=$false
        BotanicalSurveyOrCurrentConditionClaimed=$false
        SafeLocalDiagnosticIncluded=$false
        NetworkOrEntitlementFailureInjected=$false
        FailoverOrRecoveryAccepted=$false
        CwtWarmingObservedAtLeastOnce=$true
        NoDoubleVisibleObserved=$true
        RuntimePresentationStateTransitionsPerformed=$true
        RestoredGooglePrimaryBeforeExit=$true
        NativeStateMutatedByAcceptance=$false
        UnrealLaunchedByAcceptance=$false
        AcceptanceRevalidation=[pscustomobject] [ordered] @{
            ImmediatePreAcceptance=$before
            ImmediatePostAcceptance=$after
            Unchanged=$true
            PendingReceiptHashPinned=$true
            EightPngsRedecodedAndRehashed=$true
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
        'pending R33 receipt after acceptance publication')
    $accepted | ConvertTo-Json -Depth 32
}

$modeCount = @($StaticSelfCheck, $Execute, $AcceptVisualReview |
    Where-Object { $_ }).Count
if ($modeCount -ne 1) {
    throw '-StaticSelfCheck, -Execute, and -AcceptVisualReview are mutually exclusive and exactly one is required.'
}
if ($ConfirmEightImagesReviewed -and -not $AcceptVisualReview) {
    throw '-ConfirmEightImagesReviewed is valid only with -AcceptVisualReview.'
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
    if (-not $ConfirmEightImagesReviewed) {
        throw 'R33 visual acceptance requires -ConfirmEightImagesReviewed after human review of all eight PNGs.'
    }
    if ([string]::IsNullOrWhiteSpace($PendingCaptureReceipt) -or
        $ExpectedPendingCaptureReceiptSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'R33 visual acceptance requires the pending receipt path and caller-pinned SHA-256.'
    }
    Invoke-VisualReviewAcceptance `
        $PendingCaptureReceipt `
        $ExpectedPendingCaptureReceiptSha256 `
        $RunToken
    exit 0
}

if ([string]::IsNullOrWhiteSpace($R33CommitReceipt) -or
    $ExpectedR33CommitReceiptSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
    throw 'Live R33 capture requires the committed R33 transaction receipt path and caller-pinned SHA-256.'
}

$evidenceRoot = [IO.Path]::GetFullPath(
    (Join-Path $nativeEvidenceBase $RunToken))
if (-not [IO.Path]::GetDirectoryName($evidenceRoot).Equals(
        $nativeEvidenceBase, [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($evidenceRoot) -cne $RunToken) {
    throw 'R33 evidence root is not the exact direct run-token child.'
}
$captureSourceJournal = @()
$transactionStarted = $false
$captureCompleted = $false
$primaryFailure = $null
$rollbackErrors = [Collections.Generic.List[string]]::new()
$admission = $null
$immutableBefore = $null
$r33CommitBefore = $null

try {
    if ([IO.Directory]::Exists($evidenceRoot)) {
        throw "R33 evidence token already exists: $evidenceRoot"
    }
    foreach ($required in @(
        $nativeProjectFile, $dotnet, $unrealBuildTool, $unrealEditorCmd,
        $mapFile, $runtimeEditorDll, $editorDll)) {
        if (-not [IO.File]::Exists($required)) {
            throw "Required R33 capture input is absent: $required"
        }
    }
    if (-not [IO.Directory]::Exists($airSimRuntimeContentRoot)) {
        throw 'AirSimTriadRuntime content is required for R33 capture.'
    }
    Assert-NativeIdle 'before R33 capture preflight'
    $prewriteAdmission = Assert-LaunchAdmission `
        'before any R33 capture write'
    $admission = Assert-R33CommitReceipt `
        $R33CommitReceipt $ExpectedR33CommitReceiptSha256
    $r33CommitBefore = Get-FileState $admission.R33.Path
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
        throw 'R33 capture preflight found an unreceipted native plugin Source tree.'
    }
    $immutableBefore = Get-ImmutableContentSnapshot
    $bindingBefore = [pscustomobject] [ordered] @{
        Map=Get-PathBoundReceipt `
        $mapFile $admission.Map 'pre-capture R33 map'
        RuntimeEditorDll=Get-PathBoundReceipt `
            $runtimeEditorDll $admission.RuntimeDll 'pre-capture R33 runtime DLL'
        EditorDll=Get-PathBoundReceipt `
            $editorDll $admission.EditorDll 'pre-capture R33 editor DLL'
        GroundHeader=Get-PathBoundReceipt `
            $groundHeader $admission.GroundHeader 'pre-capture R33 Ground header'
        GroundSource=Get-PathBoundReceipt `
            $groundSource $admission.GroundSource 'pre-capture R33 Ground source'
    }
    Assert-NativeIdle 'immediately before R33 evidence-root creation'
    [void] (Assert-LaunchAdmission `
        'immediately before R33 evidence-root creation')
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
        'capture-source-promoted R33 plugin Source')
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
        'R33 serial fresh Development Game build' `
        $dotnet $buildArguments $nativeProjectRoot `
        $buildStdout $buildStderr $BuildTimeoutSeconds
    $gameAfter = Assert-FreshFileState `
        $gameExe $buildStartedUtc 'R33 Development Game executable'
    Assert-GameBinaryMarkers
    [void] (Assert-CaptureBindings $admission $expectedCaptureTree)
    Assert-ImmutableContentSnapshot $immutableBefore
    [void] (Assert-State `
        $r33CommitBefore $admission.R33.Path 'R33 transaction receipt after build')

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
        throw 'R33 capture cook must be fresh and non-iterative.'
    }
    $cookReceipt = Invoke-GuardedCommand `
        'R33 fresh isolated Windows cook' `
        $unrealEditorCmd $cookArguments $nativeProjectRoot `
        $cookStdout $cookStderr $CookTimeoutSeconds
    Assert-NoFatalRuntimeLog $cookRuntimeLog 'R33 fresh isolated cook'
    $cookedClosure = Assert-CookedClosure `
        $cookedPlatformRoot $cookStartedUtc
    [void] (Assert-CaptureBindings $admission $expectedCaptureTree)
    Assert-ImmutableContentSnapshot $immutableBefore
    [void] (Assert-State `
        $r33CommitBefore $admission.R33.Path 'R33 transaction receipt after cook')

    $pluginSourceTreeImmediatePreCapture = @(
        Assert-TreeReceipt $nativePluginSourceRoot $expectedCaptureTree `
            'immediate pre-capture R33 plugin Source')
    $groundHeaderImmediatePreCapture = Get-PathBoundReceipt `
        $groundHeader $admission.GroundHeader `
        'immediate pre-capture R33 Ground header'
    $groundSourceImmediatePreCapture = Get-PathBoundReceipt `
        $groundSource $admission.GroundSource `
        'immediate pre-capture R33 Ground source'

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
        "-TRIADR33CaptureRun=$RunToken",
        "-TRIADR33CaptureOutputRoot=$evidenceRoot",
        "-abslog=$gameRuntimeLog")
    $gameOwned = Start-GuardedOwnedProcess `
        'R33 standalone cooked Player0 Game capture' `
        $gameExe $gameArguments $nativeProjectRoot `
        $gameStdout $gameStderr
    [void] (Wait-RuntimeCaptureReady $gameOwned $CaptureTimeoutSeconds)
    $captureReceipts = [Collections.Generic.List[object]]::new()
    $presentationTransitions = @{}
    $currentPresentation = ''
    foreach ($plan in $capturePlan) {
        $pose = $plan.Pose
        if ($currentPresentation -cne $plan.Presentation) {
            Update-OwnedMemoryGuard $gameOwned `
                "before presentation $($plan.Presentation)"
            $presentationTransitions[$plan.Presentation] =
                Request-PresentationAndWait `
                    $gameOwned $plan.Presentation $CaptureTimeoutSeconds
            $currentPresentation = $plan.Presentation
        }
        Update-OwnedMemoryGuard $gameOwned "before plan $($plan.Id)"
        $setPose = Invoke-RcCall `
            'SetIstanaExploreV5DR33Player0CapturePose' `
            @{ PoseId=[string] $pose.Id } 5
        $setMessage = [string] (
            Get-RequiredPropertyValue $setPose 'OutMessage' `
                "set R33 plan $($plan.Id)")
        if ([bool] $setPose.ReturnValue -ne $true -or
            -not $setMessage.Contains(
                'ISTANA_EXPLORE_V5D_R33_PLAYER0_POSE_PASS',
                [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                "poseId=$($pose.Id)", [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                "reviewPurpose=$($pose.ReviewPurpose)",
                [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                'exactQaViewPose=true', [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                'mapModified=false', [StringComparison]::Ordinal) -or
            -not $setMessage.Contains(
                'simulationCollisionNavigationSensorRfModified=false',
                [StringComparison]::Ordinal)) {
            throw "Unexpected R33 pose acknowledgement: $setMessage"
        }
        $stable = Wait-StablePose `
            $gameOwned $plan $CaptureTimeoutSeconds
        $preCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR33Player0CaptureState' @{} 5
        $preCaptureReport = Assert-ExactStateResponse `
            $preCapture $plan.Presentation $pose `
            "pre-capture $($plan.Id)"
        $requestedUtc = [DateTime]::UtcNow
        $capture = Invoke-RcCall `
            'CaptureIstanaExploreV5DR33Player0ComparisonView' `
            @{
                PresentationId=[string] $plan.Presentation
                PoseId=[string] $pose.Id
            } 5
        $captureMessage = [string] (
            Get-RequiredPropertyValue $capture 'OutMessage' `
                "capture R33 plan $($plan.Id)")
        foreach ($marker in @(
            'ISTANA_EXPLORE_V5D_R33_PLAYER0_COMPARISON_CAPTURE_ACCEPTED',
            "presentation=$($plan.Presentation)",
            "poseId=$($pose.Id)", 'width=2560', 'height=1440',
            'hdr=false', 'exactPlayer0=true', 'exactQaViewPose=true',
            'googleIonAssetId=2275207', 'cwtIonAssetId=1',
            'doubleVisible=false', 'visualReferenceOnly=true',
            'providerReadyProofClaimed=false',
            'accurateRealWorldTerrainClaimed=false',
            'surveyAccuracyClaimed=false', 'verticalDatumResolved=false',
            'heightSamplesPersisted=false', 'airSimTriadRuntimeLoaded=true',
            'mapModified=false',
            'simulationCollisionNavigationSensorRfModified=false',
            'wrapperMustDecodeAndPostValidate=true')) {
            if ([bool] $capture.ReturnValue -ne $true -or
                -not $captureMessage.Contains(
                    $marker, [StringComparison]::Ordinal)) {
                throw "Unexpected R33 capture acknowledgement marker '$marker': $captureMessage"
            }
        }
        $presentationSlug = if ($plan.Presentation -eq 'GooglePrimary') {
            'google_primary'
        }
        else {
            'cwt_presented'
        }
        $capturePath = [IO.Path]::GetFullPath((Join-Path `
            (Join-Path $evidenceRoot 'captures') `
            ('explore_v5d_r33_player0_{0}_{1}_{2}.png' -f
                $presentationSlug, $pose.Id, $RunToken)))
        if (-not (Test-ContainedPath $capturePath $evidenceRoot)) {
            throw "R33 capture path escaped evidence root: $capturePath"
        }
        $image = Wait-StableDecodedPng `
            $gameOwned $capturePath $requestedUtc $CaptureTimeoutSeconds
        $postCapture = Invoke-RcCall `
            'GetIstanaExploreV5DR33Player0CaptureState' @{} 5
        $postCaptureReport = Assert-ExactStateResponse `
            $postCapture $plan.Presentation $pose `
            "post-capture $($plan.Id)"
        $captureReceipts.Add([pscustomobject] [ordered] @{
            Presentation=$plan.Presentation
            Pose=[pscustomobject] [ordered] @{
                Id=$pose.Id
                X=$pose.X; Y=$pose.Y; Z=$pose.Z
                Pitch=$pose.Pitch; Yaw=$pose.Yaw; Roll=$pose.Roll
                ReviewPurpose=$pose.ReviewPurpose
            }
            PresentationTransition=$presentationTransitions[$plan.Presentation]
            SetPoseAcknowledgement=$setMessage
            StableStateWindow=$stable
            ImmediatePreCaptureState=$preCaptureReport
            CaptureAcknowledgement=$captureMessage
            Image=$image
            ImmediatePostCaptureState=$postCaptureReport
            NoDoubleVisibleObserved=$true
            CwtPresentedReachedThroughReadinessStateMachine=(
                $plan.Presentation -eq 'CwtPresented' -and
                [bool] $presentationTransitions['CwtPresented'].WarmingObserved)
            ProviderReadyProofClaimed=$false
            AccurateRealWorldTerrainClaimed=$false
        })
    }
    if ($captureReceipts.Count -ne 8 -or
        $false -in @($captureReceipts.Image.NonBlank) -or
        @($captureReceipts.Image.Sha256 | Sort-Object -Unique).Count -ne 8 -or
        @($captureReceipts.Image.DecodedBgraSha256 |
            Sort-Object -Unique).Count -ne 8) {
        throw 'The exact eight R33 Player0 PNGs are absent, blank, or not distinct.'
    }
    if (-not [bool] $presentationTransitions['CwtPresented'].WarmingObserved) {
        throw 'R33 CWT capture group lacks an observed CwtWarming transition.'
    }
    $googleOblique = @($captureReceipts | Where-Object {
        $_.Presentation -eq 'GooglePrimary' -and
        $_.Pose.Id -eq 'surroundings_oblique_macdonald'
    })
    $cwtOblique = @($captureReceipts | Where-Object {
        $_.Presentation -eq 'CwtPresented' -and
        $_.Pose.Id -eq 'surroundings_oblique_macdonald'
    })
    if ($googleOblique.Count -ne 1 -or $cwtOblique.Count -ne 1 -or
        [string] $googleOblique[0].Image.DecodedBgraSha256 -ceq
            [string] $cwtOblique[0].Image.DecodedBgraSha256) {
        throw 'R33 broad surrounding Google/CWT comparison proof is absent or identical.'
    }

    $finishTransportError = ''
    try {
        $finish = Invoke-RcCall `
            'FinishIstanaExploreV5DR33Player0CaptureRun' @{} 5
        $finishMessage = [string] $finish.OutMessage
        if ([bool] $finish.ReturnValue -ne $true -or
            -not $finishMessage.Contains(
                'ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_EXIT_ACCEPTED',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'restoredPresentation=GooglePrimary',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'safeLocalDirectTriggerUsed=false',
                [StringComparison]::Ordinal) -or
            -not $finishMessage.Contains(
                'networkSabotageUsed=false',
                [StringComparison]::Ordinal)) {
            throw "Unexpected R33 clean-exit acknowledgement: $finishMessage"
        }
    }
    catch {
        $gameOwned.Handle.Refresh()
        if (-not $gameOwned.Handle.HasExited) { throw }
        $finishTransportError = $_.Exception.Message
    }
    $gameReceipt = Wait-GuardedOwnedProcess `
        $gameOwned $ShutdownTimeoutSeconds
    Assert-NativeIdle 'after clean R33 Game exit'
    Assert-NoFatalRuntimeLog $gameRuntimeLog 'standalone R33 capture Game'
    $gameLogText = [IO.File]::ReadAllText($gameRuntimeLog)
    if (-not $gameLogText.Contains(
            'ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_EXIT_ACCEPTED',
            [StringComparison]::Ordinal)) {
        throw 'R33 Game log lacks the clean capture-exit marker.'
    }
    foreach ($row in $captureReceipts) {
        [void] (Assert-State $row.Image $row.Image.Path `
            "final R33 Player0 PNG $($row.Presentation) $($row.Pose.Id)")
    }
    [void] (Assert-State $gameAfter $gameExe `
        'post-capture R33 Development Game executable')
    $cookedClosurePostflight = @(
        Assert-TreeReceipt $cookedPlatformRoot $cookedClosure.Files `
            'post-capture cooked closure')
    $bindingAfter = Assert-CaptureBindings `
        $admission $expectedCaptureTree
    Assert-ImmutableContentSnapshot $immutableBefore
    [void] (Assert-State `
        $r33CommitBefore $admission.R33.Path 'post-capture R33 transaction receipt')
    Assert-NoSuccessorNativeSource
    $pluginSourceTreeImmediatePostCapture = @(
        Assert-TreeReceipt $nativePluginSourceRoot $expectedCaptureTree `
            'immediate post-capture R33 plugin Source')
    $groundHeaderImmediatePostCapture = Get-PathBoundReceipt `
        $groundHeader $admission.GroundHeader `
        'immediate post-capture R33 Ground header'
    $groundSourceImmediatePostCapture = Get-PathBoundReceipt `
        $groundSource $admission.GroundSource `
        'immediate post-capture R33 Ground source'

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
    $r33CommitAdmission = [pscustomobject] [ordered] @{
        Path=$admission.R33.Path
        File=$admission.R33.State
        CallerSha256=$ExpectedR33CommitReceiptSha256.ToUpperInvariant()
        Schema=$r33TransactionSchema
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
        R32TransactionAdmission=[pscustomobject] [ordered] @{
            Path=$admission.R32Transaction.Path
            File=$admission.R32Transaction.State
        }
        R32CaptureAdmission=[pscustomobject] [ordered] @{
            Path=$admission.R32Capture.Path
            File=$admission.R32Capture.State
        }
    }
    $pending = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='PENDING_VISUAL_REVIEW'
        RunToken=$RunToken
        NativeOrder='R33_COMMIT_THEN_R33_CAPTURE_NO_R34_AUTHORITY'
        R34DependencyAllowed=$false
        StaticReceipt=$staticReceipt
        PrewriteAdmission=$prewriteAdmission
        R33CommitAdmission=$r33CommitAdmission
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
        ExactPoseCount=4
        ExactPresentationStateCount=2
        ExactCoreCaptureCount=8
        MechanicalCaptureValidationPassed=$true
        ExplicitHumanReviewAcceptance=$false
        ConfirmedEightImagesReviewed=$false
        VisualReviewRequired=$true
        VisualReviewAccepted=$false
        HumanVisualReviewAttested=$false
        AutomaticVisualAcceptanceAllowed=$false
        R33CesiumWorldTerrainVisualQaAccepted=$false
        GooglePrimaryVisualQaAccepted=$false
        CwtPresentedVisualQaAccepted=$false
        TreeMaterialResponseV3Preserved=$true
        R34AdmissionAuthorized=$false
        ProviderReadyProofClaimed=$false
        ProviderReadyCaptureAccepted=$false
        ProviderTermsOrEntitlementVerified=$false
        NonNullOpaqueIonServerRequired=$true
        IonTokenValueOrFingerprintInspectedByTriad=$false
        NonNullServerProvesTokenOrEntitlement=$false
        AccurateRealWorldTerrainClaimed=$false
        SurveyAccuracyClaimed=$false
        VerticalDatumResolved=$false
        HeightCheckpointValidationComplete=$false
        MapModifiedByCapture=$false
        SerializedProviderConfigurationModified=$false
        SimulationCollisionNavigationSensorRfModified=$false
        PerformanceAcceptanceClaimed=$false
        HyperrealismClaimed=$false
        BotanicalSurveyOrCurrentConditionClaimed=$false
        SafeLocalDiagnosticIncluded=$false
        NetworkOrEntitlementFailureInjected=$false
        FailoverOrRecoveryAccepted=$false
        CwtWarmingObservedAtLeastOnce=$true
        NoDoubleVisibleObserved=$true
        RuntimePresentationStateTransitionsPerformed=$true
        RestoredGooglePrimaryBeforeExit=$true
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
        try { Assert-NativeIdle 'before R33 capture source rollback' }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackErrors.Count -eq 0) {
            try { Restore-CaptureSourceClosure $captureSourceJournal }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        if ($null -ne $admission) {
            try {
                [void] (Assert-State $admission.Map $mapFile 'rollback R33 map')
                [void] (Assert-State $admission.RuntimeDll $runtimeEditorDll `
                    'rollback R33 runtime editor DLL')
                [void] (Assert-State $admission.EditorDll $editorDll `
                    'rollback R33 editor DLL')
                [void] (Assert-State $admission.GroundHeader $groundHeader `
                    'rollback R33 Ground header')
                [void] (Assert-State $admission.GroundSource $groundSource `
                    'rollback R33 Ground source')
                [void] (Assert-State $r33CommitBefore $admission.R33.Path `
                    'rollback R33 transaction receipt')
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
            } else { 'Unknown R33 capture failure.' }
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
        throw "R33 capture failed and rollback was incomplete: failure={$($primaryFailure.Message)} rollback={$([string]::Join(' | ', @($rollbackErrors)))}"
    }
    throw $primaryFailure
}
