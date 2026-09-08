#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$')]
    [string] $RunToken,

    # These identities must come from the exact native build/promotion receipt.
    [int64] $ExpectedMapBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedMapSha256 = '',

    [int64] $ExpectedRuntimeDllBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedRuntimeDllSha256 = '',

    [int64] $ExpectedEditorDllBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedEditorDllSha256 = '',

    # The measurement window is deliberately bounded to five through ten
    # minutes. Editor startup and fail-closed teardown are outside this window.
    [ValidateRange(300, 600)]
    [int] $DiagnosticDurationSeconds = 600,

    [ValidateRange(2, 30)]
    [int] $TelemetryIntervalSeconds = 5,

    [ValidateRange(60, 900)]
    [int] $EditorTimeoutSeconds = 600,

    [ValidateRange(60, 300)]
    [int] $PieTimeoutSeconds = 120,

    # This is a scheduling-only profile. It never changes the serialized or
    # proof-time Cesium quality tuple. The editor-world tileset is paused as
    # soon as RC ownership is established, PIE inherits that paused state,
    # the exact 75 m camera pose is installed, and only the PIE tileset is
    # resumed. This prevents editor-world and initial-pose selection from
    # competing with the measured pose.
    [ValidateSet('EditorWorldSuspendedPoseBeforeResumeV1')]
    [string] $CesiumStreamingProfile =
        'EditorWorldSuspendedPoseBeforeResumeV1',

    [switch] $StaticSelfCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$diagnosticSchema =
    'triad.istana_explore_v5d.cesium_first_pose_memory_queue_diagnostic.v1'
$telemetrySchema =
    'triad.istana_explore_v5d.cesium_first_pose_memory_queue_telemetry.v1'
$diagnosticClassification =
    'EXPLICIT_NON_PROOF_FIRST_POSE_MEMORY_QUEUE_DIAGNOSTIC_V1'
$persistentCachePolicy =
    'PRESERVE_CESIUM_STANDARD_PERSISTENT_HTTP_CACHE_NO_CLEAR_NO_RELOCATION'
$privateMemoryCeilingBytes = 12884901888L # 12 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # 6 GiB
# The R27 live startup consumed about 1.95 GiB before the continuous 6 GiB
# floor contained it. Require 10 GiB before launch so the helper cannot enter
# that already-marginal band merely by completing its observed startup burst.
# This is a pre-launch admission gate, not a relaxation of the continuous
# watchdog threshold.
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # 10 GiB
$memoryWatchdogPollMilliseconds = 100
$memoryWatchdogPersistentBreachMilliseconds = 2000
$selectionStatsPulseMilliseconds = 250
$maximumLogDeltaBytes = 67108864L
$script:memoryWatchdog = $null

$editor =
    'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe'
$projectRoot = 'D:\triad\TRIAD'
$projectFile = 'D:\triad\TRIAD\TRIAD.uproject'
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile =
    'D:\triad\TRIAD\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'
$runtimeDll =
    'D:\triad\TRIAD\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'
$editorDll =
    'D:\triad\TRIAD\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'
$ddcRoot = 'D:\triad\TRIAD\Saved\DerivedDataCache'
$persistentCachePath = 'D:\triad\TRIAD\cesium-request-cache.sqlite'
$diagnosticRoot =
    'D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D'
$logRoot = 'D:\triad\TRIAD\Saved\Logs'
$logFile = Join-Path $logRoot `
    "Codex_V5D_CesiumFirstPoseDiagnosticV1_${RunToken}.log"
$diagnosticReceiptPath = Join-Path $diagnosticRoot `
    "explore_v5d_cesium_first_pose_memory_queue_diagnostic_v1_${RunToken}.json"
$rcCallUri = [uri] 'http://127.0.0.1:30010/remote/object/call'
$rcPropertyUri = [uri] 'http://127.0.0.1:30010/remote/object/property'
$remoteControlEndpointAllowlist = @(
    $rcCallUri.AbsoluteUri,
    $rcPropertyUri.AbsoluteUri
)
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$v5dLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$levelEditor = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$pieTilesetObjectPath =
    '/Game/Maps/UEDPIE_0_Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid:PersistentLevel.CesiumPhotorealisticContext_IstanaV5D'
$editorTilesetObjectPath =
    '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid:PersistentLevel.CesiumPhotorealisticContext_IstanaV5D'

# Every live user-owned UE5.4 CAPSTONE editor is protected as an exact set.
# This helper may run beside that set but never addresses it over RC or process
# control and fails closed if any protected identity changes.
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$nativeUE55EngineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')

$expectedProjectBytes = 1298L
$expectedProjectSha256 =
    '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'

$firstPose = [pscustomobject] [ordered] @{
    Label = '075m'
    NominalViewRangeMeters = 75.0
    X = 3500.0
    Y = 15000.0
    Z = 164.0
    Pitch = -1.252968
    Yaw = -90.0
    Roll = 0.0
    Source = 'accepted_r23b_far_to_near_range_rig_first_pose'
}

$requiredMapValidationMarkers = @(
    'ionAssetId=2275207',
    'maximumSse=1.0',
    'applyDpiScaling=false',
    'forbidHoles=true',
    'loadingDescendantLimit=20',
    'simultaneousLoads=12',
    'currentContextSuppressionContract=local_fallback_suppression_v2',
    'currentContextTriangles=43448',
    'currentContextSuppressedTriangles=96',
    'contextFacadeR25=true',
    'outerGroundLoadingFallbackProviderCoupled=true',
    'cesiumStandardPersistentHttpRequestCacheAcknowledged=true',
    'triadProviderTokenReadSerializedOrLogged=false',
    'triadProviderContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false'
)

function Get-FileIdentity {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($fullPath)) {
        throw "Required regular file is absent: $fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Required regular file is a reparse point: $fullPath"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = [string] (Get-FileHash -LiteralPath $fullPath `
            -Algorithm SHA256).Hash
    }
}

function Get-FileStateIdentity {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if ([IO.File]::Exists($fullPath)) {
        return Get-FileIdentity -Path $fullPath
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $false
        Bytes = 0L
        Sha256 = 'ABSENT'
    }
}

function Get-GrowingFileState {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($fullPath)) {
        throw "Persistent cache is absent: $fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Persistent cache is a reparse point: $fullPath"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $true
        Bytes = [int64] $item.Length
        LastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

function Assert-FilePin {
    param(
        [Parameter(Mandatory = $true)] $Pin,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    $actual = Get-FileStateIdentity -Path ([string] $Pin.Path)
    if ([bool] $actual.Present -ne [bool] $Pin.Present -or
        [int64] $actual.Bytes -ne [int64] $Pin.Bytes -or
        [string] $actual.Sha256 -cne [string] $Pin.Sha256) {
        throw "File identity changed at '$Checkpoint': $($Pin.Path)"
    }
}

function Assert-FilePins {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Pins,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    foreach ($pin in $Pins) {
        Assert-FilePin -Pin $pin -Checkpoint $Checkpoint
    }
}

function Assert-ExactPin {
    param(
        [Parameter(Mandatory = $true)] $Pin,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    if ([int64] $Pin.Bytes -ne $Bytes -or
        [string] $Pin.Sha256 -cne $Sha256.ToUpperInvariant()) {
        throw "$Description identity mismatch: expected=$Bytes/$($Sha256.ToUpperInvariant()) actual=$($Pin.Bytes)/$($Pin.Sha256)"
    }
}

function Get-StringSha256 {
    param([Parameter(Mandatory = $true)] [string] $Text)

    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        [Convert]::ToHexString($sha.ComputeHash($bytes))
    }
    finally {
        $sha.Dispose()
    }
}

function Sanitize-DiagnosticText {
    param([AllowEmptyString()] [string] $Text)

    if ([string]::IsNullOrEmpty($Text)) {
        return ''
    }
    $sanitized = [regex]::Replace(
        $Text,
        '(?i)https?://[^\s"''<>]+',
        '[REDACTED_URL]')
    [regex]::Replace(
        $sanitized,
        '(?i)(token|key|signature|credential|password)=([^\s&"''<>]+)',
        '$1=[REDACTED]')
}

$selfPin = Get-FileIdentity -Path $PSCommandPath

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Status = 'STATIC_SELF_CHECK_PASS'
        Schema = $diagnosticSchema
        Classification = $diagnosticClassification
        Script = $selfPin
        DiagnosticDurationSeconds = $DiagnosticDurationSeconds
        TelemetryIntervalSeconds = $TelemetryIntervalSeconds
        FirstPose = $firstPose
        MemoryGuard = [pscustomobject] [ordered] @{
            PrivateMemoryCeilingBytes = $privateMemoryCeilingBytes
            PrivateMemoryCeilingGiB = 12
            MinimumSystemFreeVirtualBytes =
                $minimumSystemFreeVirtualBytes
            MinimumSystemFreeVirtualGiB = 6
            MinimumSystemFreeVirtualAtLaunchBytes =
                $minimumSystemFreeVirtualAtLaunchBytes
            MinimumSystemFreeVirtualAtLaunchGiB = 10
            AppliedDuringStartupRcAndMeasurement = $true
            ContinuousWatchdog = [pscustomobject] [ordered] @{
                StartsImmediatelyAfterOwnedProcessLaunch = $true
                IndependentClrThread = $true
                PollMilliseconds = $memoryWatchdogPollMilliseconds
                PersistentBreachContainmentMilliseconds =
                    $memoryWatchdogPersistentBreachMilliseconds
                ExactPidCreationTimeAndExecutableRequiredForContainment =
                    $true
            }
        }
        CesiumStreamingIsolation = [pscustomobject] [ordered] @{
            Profile = $CesiumStreamingProfile
            EditorWorldSuspendedBeforeMapValidation = $true
            PieInheritsSuspendedState = $true
            ExactFirstPoseSetBeforePieResume = $true
            EditorWorldRemainsSuspendedDuringPie = $true
            SerializedOrProofTimeQualityTupleMutated = $false
            MapSaveRequested = $false
        }
        SelectionTelemetry = [pscustomobject] [ordered] @{
            Source = 'Cesium3DTileset.LogSelectionStats'
            PulseMilliseconds = $selectionStatsPulseMilliseconds
            MaximumReadDeltaBytes = $maximumLogDeltaBytes
            SanitizedAggregateOnly = $true
            Fields = @(
                'Visited', 'CulledVisited', 'Rendered', 'Culled',
                'Occluded', 'WaitingForOcclusionResults',
                'MaxDepthVisited', 'LoadingWorker', 'LoadingMain',
                'LoadedTilesPercent'
            )
        }
        RequiredMapValidationMarkers = $requiredMapValidationMarkers
        ProtectedUE54Policy =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        RemoteControlEndpointAllowlist = $remoteControlEndpointAllowlist
        PersistentCachePolicy = $persistentCachePolicy
        PersistentCachePath = $persistentCachePath
        ProofEligible = $false
        ProviderReadyProofClaimed = $false
        GlobalProviderReadyGateRequired = $false
        CaptureCount = 0
        PublicationPerformed = $false
        LiveEditorLaunched = $false
        NativeTreeReadOrWritten = $false
    } | ConvertTo-Json -Depth 12
    return
}

if ($ExpectedMapBytes -le 0 -or
    $ExpectedMapSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live diagnostic requires -ExpectedMapBytes and -ExpectedMapSha256 from the selected native map receipt.'
}
if ($ExpectedRuntimeDllBytes -le 0 -or $ExpectedEditorDllBytes -le 0 -or
    $ExpectedRuntimeDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$' -or
    $ExpectedEditorDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live diagnostic requires positive byte counts and SHA-256 identities for both selected DLLs.'
}
$ExpectedMapSha256 = $ExpectedMapSha256.ToUpperInvariant()
$ExpectedRuntimeDllSha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant()
$ExpectedEditorDllSha256 = $ExpectedEditorDllSha256.ToUpperInvariant()

function Test-ExactCommandLineToken {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [string] $Token
    )

    $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($Token) + '"?(?:\s|$)'
    [regex]::Matches($CommandLine, $pattern).Count -eq 1
}

function Get-ProcessIdentity {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $row = Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" `
        -ErrorAction SilentlyContinue
    if ($null -eq $row) {
        return $null
    }
    $creation = [DateTime] $row.CreationDate
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $row.ProcessId
        Name = [string] $row.Name
        ExecutablePath = [string] $row.ExecutablePath
        CommandLine = [string] $row.CommandLine
        CreationUtcTicks = [int64] $creation.ToUniversalTime().Ticks
        CreationTime = $creation.ToString('o')
    }
}

function Test-ProcessIdentityEqual {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] $Actual
    )

    $null -ne $Actual -and
        [uint32] $Actual.ProcessId -eq [uint32] $Expected.ProcessId -and
        [int64] $Actual.CreationUtcTicks -eq [int64] $Expected.CreationUtcTicks -and
        [string] $Actual.Name -ceq [string] $Expected.Name -and
        [string] $Actual.ExecutablePath -ceq [string] $Expected.ExecutablePath -and
        [string] $Actual.CommandLine -ceq [string] $Expected.CommandLine
}

function Get-ProtectedUE54Identity {
    $matches = @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -and
                -not [string]::IsNullOrWhiteSpace([string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $protectedUE54Project)
        })
    $identities = @($matches | ForEach-Object {
        $processRow = $_
        $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                [string] $processRow.ExecutablePath)) {
            ''
        }
        else {
            [IO.Path]::GetFullPath([string] $processRow.ExecutablePath)
        }
        if ($processRow.Name -cne 'UnrealEditor.exe' -or
            $actualExecutable -ine $protectedUE54Editor) {
            throw 'The CAPSTONE project token is owned by an unexpected process; refusing the Cesium diagnostic.'
        }
        [pscustomobject] [ordered] @{
            ProcessId = [uint32] $processRow.ProcessId
            CreationUtcTicks =
                [int64] ([DateTimeOffset] $processRow.CreationDate).UtcTicks
            Name = [string] $processRow.Name
            ExecutablePath = $actualExecutable
            ProjectPath = $protectedUE54Project
            CommandLine = [string] $processRow.CommandLine
        }
    } | Sort-Object ProcessId)
    [pscustomobject] [ordered] @{
        State = if ($identities.Count -eq 0) { 'ABSENT' } else { 'PRESENT' }
        SessionCount = $identities.Count
        Sessions = @($identities)
    }
}

function Assert-ProtectedUE54Unchanged {
    param([Parameter(Mandatory = $true)] $Before)

    $after = Get-ProtectedUE54Identity
    $beforeCanonical = $Before | ConvertTo-Json -Compress -Depth 8
    $afterCanonical = $after | ConvertTo-Json -Compress -Depth 8
    if ($afterCanonical -cne $beforeCanonical) {
        throw 'Protected UE5.4/CAPSTONE identity set changed during the Cesium diagnostic.'
    }
    $after
}

function Convert-ProtectedSnapshotForReceipt {
    param([Parameter(Mandatory = $true)] $Snapshot)

    [pscustomobject] [ordered] @{
        State = [string] $Snapshot.State
        SessionCount = [int] $Snapshot.SessionCount
        Sessions = @($Snapshot.Sessions | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId = [uint32] $_.ProcessId
                CreationUtcTicks = [int64] $_.CreationUtcTicks
                Name = [string] $_.Name
                ExecutablePath = [string] $_.ExecutablePath
                ProjectPath = [string] $_.ProjectPath
                CommandLineSha256 = Get-StringSha256 `
                    -Text ([string] $_.CommandLine)
            }
        })
    }
}

function Get-NativeTRIADUnrealProcesses {
    $enginePrefix = $nativeUE55EngineRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
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
            $isUE55 = $actualExecutable.StartsWith(
                $enginePrefix, [StringComparison]::OrdinalIgnoreCase)
            $isNativeTRIAD = -not [string]::IsNullOrWhiteSpace(
                    [string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $projectFile)
            $isUE55 -or $isNativeTRIAD
        }
    )
}

function Get-RemoteControlListeners {
    $rows = & "$env:SystemRoot\System32\netstat.exe" -ano -p TCP
    if ($LASTEXITCODE -ne 0) {
        throw 'netstat failed while checking Remote Control ownership.'
    }
    @(
        foreach ($line in $rows) {
            if ($line -match
                '^\s*TCP\s+\S+:30010\s+\S+\s+LISTENING\s+(\d+)\s*$') {
                [uint32] $Matches[1]
            }
        }
    )
}

function Assert-LaunchedProcessIdentity {
    param([Parameter(Mandatory = $true)] $Expected)

    $actual = Get-ProcessIdentity -ProcessId ([uint32] $Expected.ProcessId)
    if (-not (Test-ProcessIdentityEqual -Expected $Expected -Actual $actual)) {
        throw "Owned UE5.5 process changed identity or disappeared: PID=$($Expected.ProcessId)"
    }
    $actual
}

function Test-ExpectedHelperLaunchIdentity {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] $Identity,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    if ($null -eq $Identity -or $Identity.Name -cne 'UnrealEditor.exe' -or
        [string]::IsNullOrWhiteSpace($Identity.ExecutablePath) -or
        [string]::IsNullOrWhiteSpace($Identity.CommandLine) -or
        [IO.Path]::GetFullPath($Identity.ExecutablePath) -ine
            [IO.Path]::GetFullPath($editor)) {
        return $false
    }
    $projectPattern =
        '(?i)(?:^|\s)(?:"[^"]+\.uproject"|[^\s"]+\.uproject)(?:\s|$)'
    [regex]::Matches($Identity.CommandLine, $projectPattern).Count -eq 1 -and
        (Test-ExactCommandLineToken $Identity.CommandLine $projectFile) -and
        (Test-ExactCommandLineToken $Identity.CommandLine $mapPackage) -and
        $Identity.CommandLine.Contains('-NoAutoSave',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('-RemoteControlHttpServer',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('-RCWebControlEnable',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('WebControl.StartServer',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains($logFile,
            [StringComparison]::OrdinalIgnoreCase) -and
        [Math]::Abs($Identity.CreationUtcTicks -
            $Handle.StartTime.ToUniversalTime().Ticks) -le
                [TimeSpan]::FromSeconds(2).Ticks
}

function Test-RemoteControlOwnership {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $listeners = @(Get-RemoteControlListeners)
    $listeners.Count -eq 1 -and [uint32] $listeners[0] -eq $ProcessId
}

function Assert-OwnedBoundary {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
    $others = @(Get-NativeTRIADUnrealProcesses)
    if ($others.Count -ne 1 -or
        [uint32] $others[0].ProcessId -ne
            [uint32] $ExpectedProcess.ProcessId) {
        throw "The owned helper is not the sole non-protected Unreal editor at '$Checkpoint'."
    }
    if (-not (Test-RemoteControlOwnership -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId))) {
        throw "RC port 30010 is not solely owned by the helper at '$Checkpoint'."
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
                this.thread.Name = "TRIAD Cesium memory safety watchdog";
                this.thread.Start();
            }
        }

        private bool TryGetExactProcess(out Process process)
        {
            process = null;
            Process candidate = null;
            try
            {
                candidate = Process.GetProcessById(this.processId);
                candidate.Refresh();
                if (candidate.HasExited)
                {
                    return false;
                }
                long actualTicks = candidate.StartTime.ToUniversalTime().Ticks;
                ProcessModule mainModule = candidate.MainModule;
                if (mainModule == null ||
                    String.IsNullOrWhiteSpace(mainModule.FileName))
                {
                    return false;
                }
                string actualPath = Path.GetFullPath(mainModule.FileName);
                if (actualTicks != this.creationUtcTicks ||
                    !String.Equals(actualPath, this.executablePath,
                        StringComparison.OrdinalIgnoreCase))
                {
                    return false;
                }
                process = candidate;
                candidate = null;
                return true;
            }
            catch (ArgumentException)
            {
                return false;
            }
            catch (InvalidOperationException)
            {
                return false;
            }
            catch (System.ComponentModel.Win32Exception)
            {
                return false;
            }
            catch (NotSupportedException)
            {
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
            if (!this.TryGetExactProcess(out process))
                return;
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
                            this.exactIdentityVerifiedAtAlert = true;
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
            if (!this.TryGetExactProcess(out process))
            {
                lock (this.gate)
                    this.containmentRefused = true;
                return;
            }
            using (process)
            {
                lock (this.gate)
                    this.exactIdentityVerifiedAtContainment = true;
                try
                {
                    process.Kill();
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
        [System.Diagnostics.Process] $Handle
    )

    Initialize-ContinuousMemoryWatchdogType
    $Handle.Refresh()
    if ($Handle.HasExited) {
        throw 'Owned UE5.5 helper exited before watchdog startup.'
    }
    $watchdog =
        [Triad.CesiumDiagnostics.ContinuousMemoryWatchdog]::new(
            [int] $Handle.Id,
            [int64] $Handle.StartTime.ToUniversalTime().Ticks,
            [IO.Path]::GetFullPath($editor),
            [int64] $privateMemoryCeilingBytes,
            [int64] $minimumSystemFreeVirtualBytes,
            [int] $memoryWatchdogPollMilliseconds,
            [int] $memoryWatchdogPersistentBreachMilliseconds)
    $watchdog.Start()
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
        return
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) {
        throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($snapshot.MonitorError)"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.AlertKind)) {
        throw "$($snapshot.AlertKind): checkpoint=$Checkpoint source=continuous_watchdog privateBytes=$($snapshot.AlertPrivateBytes) freeVirtualBytes=$($snapshot.AlertAvailableCommitBytes)"
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

function Get-SystemMemorySnapshot {
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    [pscustomobject] [ordered] @{
        ObservedUtc = [DateTime]::UtcNow.ToString('o')
        TotalPhysicalBytes = [int64] $os.TotalVisibleMemorySize * 1KB
        FreePhysicalBytes = [int64] $os.FreePhysicalMemory * 1KB
        TotalVirtualBytes = [int64] $os.TotalVirtualMemorySize * 1KB
        FreeVirtualBytes = [int64] $os.FreeVirtualMemory * 1KB
    }
}

function Get-MemorySnapshot {
    param([Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle)

    $Handle.Refresh()
    if ($Handle.HasExited) {
        throw 'Owned UE5.5 helper exited before a memory sample.'
    }
    $system = Get-SystemMemorySnapshot
    [pscustomobject] [ordered] @{
        ObservedUtc = [string] $system.ObservedUtc
        ProcessPrivateBytes = [int64] $Handle.PrivateMemorySize64
        ProcessWorkingSetBytes = [int64] $Handle.WorkingSet64
        SystemTotalPhysicalBytes = [int64] $system.TotalPhysicalBytes
        SystemFreePhysicalBytes = [int64] $system.FreePhysicalBytes
        SystemTotalVirtualBytes = [int64] $system.TotalVirtualBytes
        SystemFreeVirtualBytes = [int64] $system.FreeVirtualBytes
    }
}

function Update-MemoryExtrema {
    param([Parameter(Mandatory = $true)] $Snapshot)

    $script:peakPrivateBytes = [Math]::Max(
        [int64] $script:peakPrivateBytes,
        [int64] $Snapshot.ProcessPrivateBytes)
    $script:peakWorkingSetBytes = [Math]::Max(
        [int64] $script:peakWorkingSetBytes,
        [int64] $Snapshot.ProcessWorkingSetBytes)
    $script:minimumFreePhysicalBytes = [Math]::Min(
        [int64] $script:minimumFreePhysicalBytes,
        [int64] $Snapshot.SystemFreePhysicalBytes)
    $script:minimumFreeVirtualBytes = [Math]::Min(
        [int64] $script:minimumFreeVirtualBytes,
        [int64] $Snapshot.SystemFreeVirtualBytes)
}

function Assert-SystemMemoryBudget {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    Assert-ContinuousMemoryWatchdogHealthy -Checkpoint $Checkpoint
    $snapshot = Get-SystemMemorySnapshot
    if ([int64] $snapshot.FreeVirtualBytes -lt
        $minimumSystemFreeVirtualBytes) {
        throw "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL: checkpoint=$Checkpoint freeVirtualBytes=$($snapshot.FreeVirtualBytes) requiredMinimumBytes=$minimumSystemFreeVirtualBytes"
    }
    Assert-ContinuousMemoryWatchdogHealthy -Checkpoint $Checkpoint
    $snapshot
}

function Assert-SystemStartupHeadroom {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    $snapshot = Get-SystemMemorySnapshot
    if ([int64] $snapshot.FreeVirtualBytes -lt
        $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "MEMORY_GUARD_STARTUP_HEADROOM: checkpoint=$Checkpoint freeVirtualBytes=$($snapshot.FreeVirtualBytes) requiredMinimumBytes=$minimumSystemFreeVirtualAtLaunchBytes"
    }
    $snapshot
}

function Assert-MemoryBudget {
    param(
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    Assert-ContinuousMemoryWatchdogHealthy -Checkpoint $Checkpoint
    $snapshot = Get-MemorySnapshot -Handle $Handle
    Update-MemoryExtrema -Snapshot $snapshot
    if ([int64] $snapshot.ProcessPrivateBytes -ge
        $privateMemoryCeilingBytes) {
        throw "MEMORY_GUARD_PRIVATE_BYTES: checkpoint=$Checkpoint privateBytes=$($snapshot.ProcessPrivateBytes) ceilingBytes=$privateMemoryCeilingBytes"
    }
    if ([int64] $snapshot.SystemFreeVirtualBytes -lt
        $minimumSystemFreeVirtualBytes) {
        throw "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL: checkpoint=$Checkpoint freeVirtualBytes=$($snapshot.SystemFreeVirtualBytes) requiredMinimumBytes=$minimumSystemFreeVirtualBytes"
    }
    Assert-ContinuousMemoryWatchdogHealthy -Checkpoint $Checkpoint
    $snapshot
}

function Invoke-GuardedRcJson {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [uri] $Uri,
        [Parameter(Mandatory = $true)] [hashtable] $Body,
        [Parameter(Mandatory = $true)] [string] $Operation,
        [ValidateRange(1, 900)] [int] $TimeoutSeconds = 60,
        [switch] $SkipMemoryGuard
    )

    if ($remoteControlEndpointAllowlist -cnotcontains $Uri.AbsoluteUri) {
        throw "RC endpoint is outside the exact diagnostic allowlist: $($Uri.AbsolutePath)"
    }
    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "before RC $Operation"
    if (-not $SkipMemoryGuard) {
        Assert-MemoryBudget -Handle $Handle `
            -Checkpoint "before RC $Operation" | Out-Null
    }

    $json = $Body | ConvertTo-Json -Depth 12 -Compress
    $content = [Net.Http.StringContent]::new(
        $json, [Text.Encoding]::UTF8, 'application/json')
    $cancellation = [Threading.CancellationTokenSource]::new()
    $response = $null
    try {
        $task = $script:httpClient.PutAsync(
            $Uri, $content, $cancellation.Token)
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $deadlineReason = 'RC_TIMEOUT'
        if ($script:diagnosticDeadlineIsActive -and
            $script:diagnosticHardDeadlineUtc -lt $deadline) {
            $deadline = $script:diagnosticHardDeadlineUtc
            $deadlineReason = 'DIAGNOSTIC_WINDOW_DEADLINE'
        }
        do {
            if ($task.Wait(250)) {
                break
            }
            Assert-ProtectedUE54Unchanged `
                -Before $script:protectedUE54Before | Out-Null
            Assert-LaunchedProcessIdentity -Expected $ExpectedProcess |
                Out-Null
            if (-not $SkipMemoryGuard) {
                Assert-MemoryBudget -Handle $Handle `
                    -Checkpoint "during RC $Operation" | Out-Null
            }
            if ([DateTime]::UtcNow -ge $deadline) {
                $cancellation.Cancel()
                throw "${deadlineReason}: operation=$Operation"
            }
        } while ($true)

        $response = $task.GetAwaiter().GetResult()
        $statusCode = [int] $response.StatusCode
        if (-not $response.IsSuccessStatusCode) {
            throw "RC_HTTP_STATUS: operation=$Operation statusCode=$statusCode"
        }
        $responseText =
            $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
            -Checkpoint "after RC $Operation"
        if (-not $SkipMemoryGuard) {
            Assert-MemoryBudget -Handle $Handle `
                -Checkpoint "after RC $Operation" | Out-Null
        }
        if ([string]::IsNullOrWhiteSpace($responseText)) {
            return $null
        }
        $responseText | ConvertFrom-Json -Depth 20
    }
    finally {
        if ($null -ne $response) {
            $response.Dispose()
        }
        $content.Dispose()
        $cancellation.Dispose()
    }
}

function Invoke-OwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 900)] [int] $TimeoutSeconds = 60,
        [switch] $SkipMemoryGuard
    )

    Invoke-GuardedRcJson -ExpectedProcess $ExpectedProcess -Handle $Handle `
        -Uri $rcCallUri -Operation "call:$FunctionName" `
        -Body @{
            objectPath = $ObjectPath
            functionName = $FunctionName
            parameters = $Parameters
        } -TimeoutSeconds $TimeoutSeconds `
        -SkipMemoryGuard:$SkipMemoryGuard
}

function Invoke-RequiredOwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [string] $TextProperty = '',
        [ValidateRange(1, 900)] [int] $TimeoutSeconds = 60
    )

    $result = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -Handle $Handle -ObjectPath $ObjectPath -FunctionName $FunctionName `
        -Parameters $Parameters -TimeoutSeconds $TimeoutSeconds
    if ($result.ReturnValue -ne $true) {
        throw "Required RC call failed: $FunctionName"
    }
    if (-not [string]::IsNullOrWhiteSpace($TextProperty) -and
        [string]::IsNullOrWhiteSpace([string] $result.$TextProperty)) {
        throw "Required RC call returned blank $TextProperty`: $FunctionName"
    }
    $result
}

function Get-OwnedTilesetSuspendUpdate {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [switch] $SkipMemoryGuard
    )

    $readback = Invoke-GuardedRcJson -ExpectedProcess $ExpectedProcess `
        -Handle $Handle -Uri $rcPropertyUri `
        -Operation 'property-read:SuspendUpdate' `
        -Body @{
            objectPath = $ObjectPath
            propertyName = 'SuspendUpdate'
            access = 'READ_ACCESS'
        } -TimeoutSeconds 30 -SkipMemoryGuard:$SkipMemoryGuard
    $readbackJson = $readback | ConvertTo-Json -Compress -Depth 5
    $match = [regex]::Match(
        $readbackJson,
        '"SuspendUpdate"\s*:\s*(?<Value>true|false)',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        throw 'SuspendUpdate readback was absent for the exact tileset object.'
    }
    $match.Groups['Value'].Value -ieq 'true'
}

function Set-OwnedTilesetSuspendUpdate {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [bool] $Suspended,
        [switch] $SkipMemoryGuard
    )

    Invoke-GuardedRcJson -ExpectedProcess $ExpectedProcess -Handle $Handle `
        -Uri $rcPropertyUri `
        -Operation "property-write:SuspendUpdate:$Suspended" `
        -Body @{
            objectPath = $ObjectPath
            propertyName = 'SuspendUpdate'
            access = 'WRITE_ACCESS'
            generateTransaction = $false
            propertyValue = $Suspended
        } -TimeoutSeconds 30 -SkipMemoryGuard:$SkipMemoryGuard | Out-Null

    $actual = Get-OwnedTilesetSuspendUpdate `
        -ExpectedProcess $ExpectedProcess -Handle $Handle `
        -ObjectPath $ObjectPath -SkipMemoryGuard:$SkipMemoryGuard
    if ($actual -ne $Suspended) {
        throw "SuspendUpdate readback did not equal requested state=$Suspended for the exact tileset object."
    }
}

function Set-OwnedLogSelectionStats {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [bool] $Enabled,
        [switch] $SkipMemoryGuard
    )

    Invoke-GuardedRcJson -ExpectedProcess $ExpectedProcess -Handle $Handle `
        -Uri $rcPropertyUri `
        -Operation "property-write:LogSelectionStats:$Enabled" `
        -Body @{
            objectPath = $pieTilesetObjectPath
            propertyName = 'LogSelectionStats'
            access = 'WRITE_ACCESS'
            generateTransaction = $false
            propertyValue = $Enabled
        } -TimeoutSeconds 30 -SkipMemoryGuard:$SkipMemoryGuard | Out-Null

    $readback = Invoke-GuardedRcJson -ExpectedProcess $ExpectedProcess `
        -Handle $Handle -Uri $rcPropertyUri `
        -Operation 'property-read:LogSelectionStats' `
        -Body @{
            objectPath = $pieTilesetObjectPath
            propertyName = 'LogSelectionStats'
            access = 'READ_ACCESS'
        } -TimeoutSeconds 30 -SkipMemoryGuard:$SkipMemoryGuard
    $readbackJson = $readback | ConvertTo-Json -Compress -Depth 5
    $match = [regex]::Match(
        $readbackJson,
        '"LogSelectionStats"\s*:\s*(?<Value>true|false)',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success -or
        (($match.Groups['Value'].Value -ieq 'true') -ne $Enabled)) {
        throw "LogSelectionStats readback did not equal requested state=$Enabled."
    }
}

function Wait-OwnedPieState {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [bool] $Expected,
        [ValidateRange(5, 600)] [int] $TimeoutSeconds,
        [switch] $SkipMemoryGuard
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
            -Handle $Handle -ObjectPath $levelEditor `
            -FunctionName 'IsInPlayInEditor' -TimeoutSeconds 30 `
            -SkipMemoryGuard:$SkipMemoryGuard
        if ([bool] $state.ReturnValue -eq $Expected) {
            return
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "PIE did not reach IsInPlayInEditor=$Expected."
}

function Convert-InvariantDouble {
    param([Parameter(Mandatory = $true)] [string] $Text)

    [double]::Parse($Text, [Globalization.NumberStyles]::Float,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-AngularDistanceDegrees {
    param(
        [Parameter(Mandatory = $true)] [double] $A,
        [Parameter(Mandatory = $true)] [double] $B
    )

    [Math]::Abs((($A - $B + 540.0) % 360.0) - 180.0)
}

function Get-FirstPoseStateTelemetry {
    param(
        [Parameter(Mandatory = $true)] $Response,
        [Parameter(Mandatory = $true)] $Pose
    )

    if ($Response.ReturnValue -ne $true) {
        throw 'V5D state report returned false during the diagnostic.'
    }
    $report = [string] $Response.OutReport
    foreach ($marker in @(
            "map=$mapPackage",
            'providerSiteClipActive=true',
            'stableVisualPolicy=true',
            'viewTargetMatchesPawn=true',
            'v5CameraProfile=true',
            'exactQaViewPose=true')) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            throw "First-pose state lost required marker: $marker"
        }
    }

    $progressMatch = [regex]::Match($report,
        'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
    $readyMatch = [regex]::Match($report,
        'providerReadyForProof=(?<Value>true|false)')
    $fallbackMatch = [regex]::Match($report,
        'localFallbackHidden=(?<Value>true|false)')
    if (-not $progressMatch.Success -or -not $readyMatch.Success -or
        -not $fallbackMatch.Success) {
        throw 'Sanitized provider state fields are absent from the V5D report.'
    }
    $progress = Convert-InvariantDouble `
        $progressMatch.Groups['Progress'].Value
    if ([double]::IsNaN($progress) -or
        [double]::IsInfinity($progress) -or
        $progress -lt 0.0 -or $progress -gt 100.0) {
        throw 'Cesium load progress was outside 0 through 100.'
    }

    $number = '[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?'
    $posePattern = 'viewLocationCm=X=(?<X>' + $number +
        ')\s+Y=(?<Y>' + $number + ')\s+Z=(?<Z>' + $number +
        ')\s+viewRotationDeg=P=(?<Pitch>' + $number +
        ')\s+Y=(?<Yaw>' + $number + ')\s+R=(?<Roll>' + $number +
        ')\s+exactQaViewPose=true'
    $poseMatch = [regex]::Match(
        $report, $posePattern,
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if (-not $poseMatch.Success) {
        throw 'Exact first-pose coordinates are absent from the V5D report.'
    }
    $x = Convert-InvariantDouble $poseMatch.Groups['X'].Value
    $y = Convert-InvariantDouble $poseMatch.Groups['Y'].Value
    $z = Convert-InvariantDouble $poseMatch.Groups['Z'].Value
    $pitch = Convert-InvariantDouble $poseMatch.Groups['Pitch'].Value
    $yaw = Convert-InvariantDouble $poseMatch.Groups['Yaw'].Value
    $roll = Convert-InvariantDouble $poseMatch.Groups['Roll'].Value
    if ([Math]::Abs($x - [double] $Pose.X) -gt 0.002 -or
        [Math]::Abs($y - [double] $Pose.Y) -gt 0.002 -or
        [Math]::Abs($z - [double] $Pose.Z) -gt 0.002 -or
        (Get-AngularDistanceDegrees $pitch ([double] $Pose.Pitch)) -gt
            0.002 -or
        (Get-AngularDistanceDegrees $yaw ([double] $Pose.Yaw)) -gt
            0.002 -or
        (Get-AngularDistanceDegrees $roll ([double] $Pose.Roll)) -gt
            0.002) {
        throw 'V5D diagnostic camera drifted from the exact first pose.'
    }

    [pscustomobject] [ordered] @{
        CesiumLoadProgress = $progress
        ProviderReadyForProof =
            $readyMatch.Groups['Value'].Value -ceq 'true'
        LocalFallbackHidden =
            $fallbackMatch.Groups['Value'].Value -ceq 'true'
        ExactQaViewPose = $true
    }
}

function Read-SharedLogDelta {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [ref] $Offset
    )

    if (-not [IO.File]::Exists($Path)) {
        return ''
    }
    $stream = [IO.File]::Open(
        $Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try {
        if ($stream.Length -lt [int64] $Offset.Value) {
            throw 'Owned Unreal log shrank during the diagnostic.'
        }
        $remaining = [int64] $stream.Length - [int64] $Offset.Value
        if ($remaining -gt $maximumLogDeltaBytes) {
            throw "SANITIZED_LOG_DELTA_LIMIT: deltaBytes=$remaining maximumBytes=$maximumLogDeltaBytes"
        }
        if ($remaining -eq 0) {
            return ''
        }
        [void] $stream.Seek([int64] $Offset.Value, [IO.SeekOrigin]::Begin)
        $bytes = [byte[]]::new([int] $remaining)
        $readTotal = 0
        while ($readTotal -lt $bytes.Length) {
            $read = $stream.Read(
                $bytes, $readTotal, $bytes.Length - $readTotal)
            if ($read -le 0) {
                break
            }
            $readTotal += $read
        }
        $Offset.Value = [int64] $Offset.Value + [int64] $readTotal
        [Text.Encoding]::UTF8.GetString($bytes, 0, $readTotal)
    }
    finally {
        $stream.Dispose()
    }
}

function Get-CompleteLogText {
    param(
        [AllowEmptyString()] [string] $Delta,
        [Parameter(Mandatory = $true)] [ref] $Carry
    )

    $combined = [string] $Carry.Value + $Delta
    $lastLineFeed = $combined.LastIndexOf("`n", [StringComparison]::Ordinal)
    if ($lastLineFeed -lt 0) {
        if ($combined.Length -gt 1048576) {
            throw 'SANITIZED_LOG_LINE_LIMIT: incomplete line exceeded 1 MiB.'
        }
        $Carry.Value = $combined
        return ''
    }
    $complete = $combined.Substring(0, $lastLineFeed + 1)
    $Carry.Value = $combined.Substring($lastLineFeed + 1)
    $complete
}

$selectionStatsPattern = [regex]::new(
    'LogCesium:\s+Display:\s+CesiumPhotorealisticContext_IstanaV5D[^:]*:\s+' +
    '(?<ElapsedMs>\d+) ms, Unreal Frame #(?<UnrealFrame>\d+), ' +
    'Tileset Frame: #(?<TilesetFrame>\d+), Visited (?<Visited>\d+), ' +
    'Culled Visited (?<CulledVisited>\d+), Rendered (?<Rendered>\d+), ' +
    'Culled (?<Culled>\d+), Occluded (?<Occluded>\d+), ' +
    'Waiting For Occlusion Results (?<Waiting>\d+), ' +
    'Max Depth Visited: (?<MaxDepth>\d+), ' +
    'Loading-Worker (?<LoadingWorker>\d+), ' +
    'Loading-Main (?<LoadingMain>\d+), Loaded tiles ' +
    '(?<LoadedPercent>[0-9]+(?:\.[0-9]+)?)%',
    [Text.RegularExpressions.RegexOptions]::CultureInvariant)

function Update-SanitizedLogAggregates {
    param([AllowEmptyString()] [string] $Text)

    if ([string]::IsNullOrEmpty($Text)) {
        return 0
    }
    $script:http429EventCount += [regex]::Matches(
        $Text,
        'Received status code 429 for tile content ',
        [Text.RegularExpressions.RegexOptions]::CultureInvariant).Count
    $matches = @($selectionStatsPattern.Matches($Text))
    foreach ($match in $matches) {
        $script:latestSelectionStats = [pscustomobject] [ordered] @{
            ObservedUtc = [DateTime]::UtcNow.ToString('o')
            CesiumElapsedMilliseconds = [int64] $match.Groups['ElapsedMs'].Value
            UnrealFrame = [int64] $match.Groups['UnrealFrame'].Value
            TilesetFrame = [int64] $match.Groups['TilesetFrame'].Value
            Visited = [int64] $match.Groups['Visited'].Value
            CulledVisited = [int64] $match.Groups['CulledVisited'].Value
            Rendered = [int64] $match.Groups['Rendered'].Value
            Culled = [int64] $match.Groups['Culled'].Value
            Occluded = [int64] $match.Groups['Occluded'].Value
            WaitingForOcclusionResults =
                [int64] $match.Groups['Waiting'].Value
            MaxDepthVisited = [int64] $match.Groups['MaxDepth'].Value
            LoadingWorker = [int64] $match.Groups['LoadingWorker'].Value
            LoadingMain = [int64] $match.Groups['LoadingMain'].Value
            LoadedTilesPercent = Convert-InvariantDouble `
                $match.Groups['LoadedPercent'].Value
        }
        $script:selectionStatsObservedCount += 1
    }
    $matches.Count
}

function Write-SanitizedTelemetry {
    param([Parameter(Mandatory = $true)] $Sample)

    $line = [pscustomobject] [ordered] @{
        Schema = $telemetrySchema
        Classification = $diagnosticClassification
        Sequence = [int] $Sample.Sequence
        ObservedUtc = [string] $Sample.ObservedUtc
        ElapsedSeconds = [double] $Sample.ElapsedSeconds
        ProcessPrivateBytes = [int64] $Sample.ProcessPrivateBytes
        ProcessWorkingSetBytes = [int64] $Sample.ProcessWorkingSetBytes
        SystemFreePhysicalBytes = [int64] $Sample.SystemFreePhysicalBytes
        SystemFreeVirtualBytes = [int64] $Sample.SystemFreeVirtualBytes
        CesiumLoadProgress = [double] $Sample.CesiumLoadProgress
        ProviderReadyForProof = [bool] $Sample.ProviderReadyForProof
        LocalFallbackHidden = [bool] $Sample.LocalFallbackHidden
        FreshSelectionStatsCount = [int] $Sample.FreshSelectionStatsCount
        SelectionStats = $Sample.SelectionStats
        Http429EventCount = [int] $Sample.Http429EventCount
        ProofEligible = $false
    } | ConvertTo-Json -Compress -Depth 8
    Write-Information $line -InformationAction Continue
}

function Wait-ExactProcessExit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Handle.Refresh()
        if ($Handle.HasExited) {
            $Handle.WaitForExit()
            return $true
        }
        $actual = Get-ProcessIdentity -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'PID reuse or helper identity drift occurred while the owned process handle remained live.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Invoke-OwnedEndPlay {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [ValidateRange(120, 600)] [int] $TimeoutSeconds = 420
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC EditorRequestEndPlay'
    $requestResult = 'returned'
    try {
        Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess -Handle $Handle `
            -ObjectPath $levelEditor -FunctionName 'EditorRequestEndPlay' `
            -TimeoutSeconds ([Math]::Min($TimeoutSeconds, 300)) `
            -SkipMemoryGuard | Out-Null
    }
    catch {
        $requestResult = Sanitize-DiagnosticText $_.Exception.Message
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
        if (Test-RemoteControlOwnership -ProcessId `
                ([uint32] $ExpectedProcess.ProcessId)) {
            try {
                $state = Invoke-OwnedRcCall `
                    -ExpectedProcess $ExpectedProcess -Handle $Handle `
                    -ObjectPath $levelEditor `
                    -FunctionName 'IsInPlayInEditor' -TimeoutSeconds 30 `
                    -SkipMemoryGuard
                if ([bool] $state.ReturnValue -eq $false) {
                    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
                        -Checkpoint 'after completed PIE teardown'
                    return [pscustomobject] [ordered] @{
                        RequestResult = $requestResult
                        Completed = $true
                    }
                }
            }
            catch {
                # Continue bounded teardown polling. Exact identity and RC
                # ownership are rechecked on every iteration.
            }
        }
        Start-Sleep -Seconds 2
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "PIE teardown did not complete safely: request=$requestResult"
}

function Invoke-GracefulQuit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC QuitEditor'
    $connectionResult = 'returned'
    try {
        Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess -Handle $Handle `
            -ObjectPath $quitLibrary -FunctionName 'QuitEditor' `
            -TimeoutSeconds 30 -SkipMemoryGuard | Out-Null
    }
    catch {
        $connectionResult = Sanitize-DiagnosticText $_.Exception.Message
    }
    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    $Handle.Refresh()
    if (-not $Handle.HasExited) {
        $actual = Get-ProcessIdentity -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'Owned process identity changed after QuitEditor.'
        }
    }
    $connectionResult
}

function Stop-ExactHelperForContainment {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    $Handle.Refresh()
    if ($Handle.HasExited) {
        return
    }
    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
    if ([uint32] $Handle.Id -ne [uint32] $ExpectedProcess.ProcessId) {
        throw 'Process handle does not match the exact UE5.5 helper; refusing containment.'
    }
    Stop-Process -Id ([int] $ExpectedProcess.ProcessId) -Force `
        -ErrorAction Stop
    if (-not (Wait-ExactProcessExit -ExpectedProcess $ExpectedProcess `
            -Handle $Handle -TimeoutSeconds 30)) {
        throw 'Exact UE5.5 helper did not exit after containment.'
    }
}

if (-not (Test-Path -LiteralPath $diagnosticRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $logRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $ddcRoot -PathType Container)) {
    throw 'The exact diagnostic, log, or persistent DDC directory is absent.'
}
foreach ($path in @($logFile, $diagnosticReceiptPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite an existing Cesium diagnostic artifact: $path"
    }
}

$cacheBefore = Get-GrowingFileState -Path $persistentCachePath
Assert-SystemStartupHeadroom -Checkpoint 'preflight before helper launch' |
    Out-Null
Assert-SystemMemoryBudget -Checkpoint 'preflight continuous floor' | Out-Null
$script:protectedUE54Before = Get-ProtectedUE54Identity
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    throw 'Cesium diagnostic requires UE5.5/native TRIAD helpers to be idle; the protected UE5.4/CAPSTONE set may remain open.'
}
if (@(Get-RemoteControlListeners).Count -ne 0) {
    throw 'Cesium diagnostic requires no existing RC port 30010 listener.'
}

$editorPin = Get-FileIdentity -Path $editor
$projectPin = Get-FileIdentity -Path $projectFile
$mapPin = Get-FileIdentity -Path $mapFile
$runtimeDllPin = Get-FileIdentity -Path $runtimeDll
$editorDllPin = Get-FileIdentity -Path $editorDll
Assert-ExactPin -Pin $projectPin -Bytes $expectedProjectBytes `
    -Sha256 $expectedProjectSha256 -Description 'TRIAD project file'
Assert-ExactPin -Pin $mapPin -Bytes $ExpectedMapBytes `
    -Sha256 $ExpectedMapSha256 -Description 'selected V5D map'
Assert-ExactPin -Pin $runtimeDllPin -Bytes $ExpectedRuntimeDllBytes `
    -Sha256 $ExpectedRuntimeDllSha256 -Description 'selected runtime DLL'
Assert-ExactPin -Pin $editorDllPin -Bytes $ExpectedEditorDllBytes `
    -Sha256 $ExpectedEditorDllSha256 -Description 'selected editor DLL'
$script:boundaryPins = @(
    $selfPin,
    $editorPin,
    $projectPin,
    $mapPin,
    $runtimeDllPin,
    $editorDllPin
)
Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'

# No cache-clear, cache-relocation, capture, screenshot, or proof-publication
# command is present. The standard persistent request cache remains in place.
$argumentLine = "`"$projectFile`" $mapPackage -DisablePlugin=AirSim -unattended -nop4 -NoSplash -NoAutoSave -RenderOffscreen -NoSound -asyncstaticmeshcompilation=0 -dx12 -sm6 -ResX=2560 -ResY=1440 -Windowed -ini:Engine:[HTTP]:HttpMaxConnectionsPerServer=12 -ini:Engine:[/Script/CesiumRuntime.CesiumRuntimeSettings]:MaxCacheItems=32768 -DDC=InstalledNoZenLocalFallback -LocalDataCachePath=`"$ddcRoot`" -RemoteControlHttpServer -RCWebControlEnable -ExecCmds=`"WebControl.StartServer`" -abslog=`"$logFile`""
$oldLocalDdc = ${env:UE-LocalDataCachePath}
$process = $null
$launchedIdentity = $null
$pieMayBeActive = $false
$editorTilesetSuspendApplied = $false
$pieInheritedSuspendedState = $false
$exactFirstPoseSetBeforeResume = $false
$pieTilesetResumedAtFirstPose = $false
$editorWorldRemainedSuspendedDuringPie = $false
$editorTilesetRestoredBeforeQuit = $false
$editorTilesetRestorationDisposition = 'not_applied'
$selectionStatsMayBeEnabled = $false
$quitRequested = $false
$forcedContainment = $false
$exitCode = $null
$quitConnectionResult = ''
$endPlayEvidence = $null
$workflowError = $null
$cleanupErrors = [Collections.Generic.List[string]]::new()
$postconditionErrors = [Collections.Generic.List[string]]::new()
$telemetry = [Collections.Generic.List[object]]::new()
$mapValidationReportAccepted = $false
$initialPieValidationAccepted = $false
$firstPoseAcknowledgementAccepted = $false
$diagnosticWindowCompleted = $false
$diagnosticStartedUtc = $null
$diagnosticEndedUtc = $null
$logOffset = 0L
$logCarry = ''
$script:latestSelectionStats = $null
$script:selectionStatsObservedCount = 0
$script:http429EventCount = 0
$script:peakPrivateBytes = 0L
$script:peakWorkingSetBytes = 0L
$script:minimumFreePhysicalBytes = [int64]::MaxValue
$script:minimumFreeVirtualBytes = [int64]::MaxValue
$script:diagnosticDeadlineIsActive = $false
$script:diagnosticHardDeadlineUtc = [DateTime]::MaxValue
$script:memoryWatchdog = $null
$continuousWatchdogSnapshot = $null
$watchdogMemoryAbort = $false
$memoryAbortCleanupObservations = @()
$script:httpClient = [Net.Http.HttpClient]::new()
$script:httpClient.Timeout = [Threading.Timeout]::InfiniteTimeSpan

# Compile the independent monitor before Unreal starts, so there is no
# unguarded C# compilation interval after the first editor allocation.
Initialize-ContinuousMemoryWatchdogType

try {
    ${env:UE-LocalDataCachePath} = $ddcRoot
    $process = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden
    $script:memoryWatchdog = Start-ContinuousMemoryWatchdog -Handle $process
    $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        Assert-MemoryBudget -Handle $process `
            -Checkpoint 'startup identity acquisition' | Out-Null
        $candidate = Get-ProcessIdentity -ProcessId ([uint32] $process.Id)
        if ($null -ne $candidate -and
            -not [string]::IsNullOrWhiteSpace($candidate.ExecutablePath) -and
            -not [string]::IsNullOrWhiteSpace($candidate.CommandLine) -and
            $candidate.CreationUtcTicks -gt 0) {
            $launchedIdentity = $candidate
            break
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $identityDeadline)
    if (-not (Test-ExpectedHelperLaunchIdentity -Identity $launchedIdentity `
            -Handle $process)) {
        throw 'Launched process did not prove the exact UE5.5/project/map/RC/log identity.'
    }

    $editorDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
    $projectIdentity = $null
    do {
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $launchedIdentity | Out-Null
        Assert-MemoryBudget -Handle $process `
            -Checkpoint 'editor readiness' | Out-Null
        if (Test-RemoteControlOwnership -ProcessId `
                ([uint32] $launchedIdentity.ProcessId)) {
            try {
                $projectIdentity = Invoke-OwnedRcCall `
                    -ExpectedProcess $launchedIdentity -Handle $process `
                    -ObjectPath $identityLibrary `
                    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
                    -Parameters @{ ExpectedProjectPath = $projectRoot } `
                    -TimeoutSeconds 30
            }
            catch {
                $projectIdentity = $null
            }
            if ($null -ne $projectIdentity -and
                $projectIdentity.ReturnValue -eq $true) {
                break
            }
        }
        Start-Sleep -Seconds 2
    } while ([DateTime]::UtcNow -lt $editorDeadline)
    if ($null -eq $projectIdentity -or
        $projectIdentity.ReturnValue -ne $true) {
        throw 'The helper did not prove exact RC ownership and project identity.'
    }

    # Pause the editor-world selector at the first owned RC boundary. The
    # serialized quality tuple is validated immediately afterward and the map
    # pin proves that this transient property write is never saved.
    Set-OwnedTilesetSuspendUpdate `
        -ExpectedProcess $launchedIdentity -Handle $process `
        -ObjectPath $editorTilesetObjectPath -Suspended $true
    $editorTilesetSuspendApplied = $true

    $mapValidation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $launchedIdentity -Handle $process `
        -ObjectPath $v5dLibrary `
        -FunctionName 'ValidateIstanaExploreV5DHybridMap' `
        -TextProperty 'OutReport' -TimeoutSeconds 300
    $mapValidationReport = [string] $mapValidation.OutReport
    if (-not $mapValidationReport.StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID',
            [StringComparison]::Ordinal)) {
        throw 'Unexpected V5D map validation prefix.'
    }
    foreach ($marker in $requiredMapValidationMarkers) {
        if (-not $mapValidationReport.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "V5D map validation lacks required diagnostic marker: $marker"
        }
    }
    $mapValidationReportAccepted = $true

    $initialPie = Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity `
        -Handle $process -ObjectPath $levelEditor `
        -FunctionName 'IsInPlayInEditor' -TimeoutSeconds 30
    if ([bool] $initialPie.ReturnValue) {
        throw 'A fresh non-PIE editor world was not available.'
    }
    $pieMayBeActive = $true
    Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity -Handle $process `
        -ObjectPath $levelEditor -FunctionName 'EditorRequestBeginPlay' `
        -TimeoutSeconds 60 | Out-Null
    Wait-OwnedPieState -ExpectedProcess $launchedIdentity -Handle $process `
        -Expected $true -TimeoutSeconds $PieTimeoutSeconds

    $pieInheritedSuspendedState = Get-OwnedTilesetSuspendUpdate `
        -ExpectedProcess $launchedIdentity -Handle $process `
        -ObjectPath $pieTilesetObjectPath
    if (-not $pieInheritedSuspendedState) {
        throw 'PIE did not inherit the editor-world SuspendUpdate=true staging state.'
    }

    $pieDeadline = [DateTime]::UtcNow.AddSeconds($PieTimeoutSeconds)
    $initialPieValidation = $null
    do {
        Start-Sleep -Seconds 1
        $initialPieValidation = Invoke-OwnedRcCall `
            -ExpectedProcess $launchedIdentity -Handle $process `
            -ObjectPath $v5dLibrary `
            -FunctionName 'ValidateIstanaExploreV5DHybridPlayWorld' `
            -TimeoutSeconds 120
        if ($initialPieValidation.ReturnValue -eq $true -and
            ([string] $initialPieValidation.OutReport).StartsWith(
                'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID',
                [StringComparison]::Ordinal)) {
            break
        }
    } while ([DateTime]::UtcNow -lt $pieDeadline)
    if ($null -eq $initialPieValidation -or
        $initialPieValidation.ReturnValue -ne $true -or
        -not ([string] $initialPieValidation.OutReport).StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID',
            [StringComparison]::Ordinal)) {
        throw 'Initial V5D PIE validation failed.'
    }
    $initialPieValidationAccepted = $true

    $setPose = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $launchedIdentity -Handle $process `
        -ObjectPath $v5dLibrary `
        -FunctionName 'SetIstanaExploreV5DHybridPlayViewPoseForQa' `
        -Parameters @{
            WorldViewLocationCentimeters = @{
                X = [double] $firstPose.X
                Y = [double] $firstPose.Y
                Z = [double] $firstPose.Z
            }
            WorldViewRotationDegrees = @{
                Pitch = [double] $firstPose.Pitch
                Yaw = [double] $firstPose.Yaw
                Roll = [double] $firstPose.Roll
            }
        } -TextProperty 'OutMessage' -TimeoutSeconds 60
    $setPoseMessage = [string] $setPose.OutMessage
    if (-not $setPoseMessage.StartsWith(
            'EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_PASS:',
            [StringComparison]::Ordinal) -or
        -not $setPoseMessage.Contains(
            'exactQaViewPose=true', [StringComparison]::Ordinal)) {
        throw 'Exact first-pose acknowledgement was not accepted.'
    }
    $firstPoseAcknowledgementAccepted = $true
    $exactFirstPoseSetBeforeResume = $true

    # Resume only the PIE copy after the exact measured pose exists. The
    # editor-world copy stays paused for the whole diagnostic, eliminating a
    # second selector/load queue without changing 1 px SSE or any other
    # provider-quality field.
    Set-OwnedTilesetSuspendUpdate `
        -ExpectedProcess $launchedIdentity -Handle $process `
        -ObjectPath $pieTilesetObjectPath -Suspended $false
    $pieTilesetResumedAtFirstPose = $true
    $editorWorldRemainedSuspendedDuringPie =
        Get-OwnedTilesetSuspendUpdate `
            -ExpectedProcess $launchedIdentity -Handle $process `
            -ObjectPath $editorTilesetObjectPath
    if (-not $editorWorldRemainedSuspendedDuringPie) {
        throw 'Editor-world Cesium selector resumed while PIE was active.'
    }

    # Start at byte zero to aggregate startup 429s, but never surface or retain
    # arbitrary raw log lines. Only exact numeric Cesium fields are retained.
    $initialLogDelta = Read-SharedLogDelta -Path $logFile `
        -Offset ([ref] $logOffset)
    $initialCompleteLog = Get-CompleteLogText -Delta $initialLogDelta `
        -Carry ([ref] $logCarry)
    [void] (Update-SanitizedLogAggregates -Text $initialCompleteLog)
    $initialLogDelta = $null
    $initialCompleteLog = $null

    $diagnosticStartedUtc = [DateTime]::UtcNow
    $diagnosticDeadline =
        $diagnosticStartedUtc.AddSeconds($DiagnosticDurationSeconds)
    $script:diagnosticHardDeadlineUtc = $diagnosticDeadline
    $script:diagnosticDeadlineIsActive = $true
    $sequence = 0
    :diagnosticLoop do {
        try {
            $sampleCycleStartedUtc = [DateTime]::UtcNow
            Assert-FilePins -Pins $script:boundaryPins `
                -Checkpoint "diagnostic sample $sequence"
            Assert-OwnedBoundary -ExpectedProcess $launchedIdentity `
                -Checkpoint "diagnostic sample $sequence"
            Assert-MemoryBudget -Handle $process `
                -Checkpoint "before selection-stats pulse $sequence" |
                Out-Null

            $selectionStatsMayBeEnabled = $true
            Set-OwnedLogSelectionStats -ExpectedProcess $launchedIdentity `
                -Handle $process -Enabled $true
            Start-Sleep -Milliseconds $selectionStatsPulseMilliseconds
            Set-OwnedLogSelectionStats -ExpectedProcess $launchedIdentity `
                -Handle $process -Enabled $false
            $selectionStatsMayBeEnabled = $false

            $logDelta = Read-SharedLogDelta -Path $logFile `
                -Offset ([ref] $logOffset)
            $completeLogDelta = Get-CompleteLogText -Delta $logDelta `
                -Carry ([ref] $logCarry)
            $freshSelectionStatsCount =
                Update-SanitizedLogAggregates -Text $completeLogDelta
            $logDelta = $null
            $completeLogDelta = $null

            $state = Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity `
                -Handle $process -ObjectPath $v5dLibrary `
                -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
                -TimeoutSeconds 120
            $stateTelemetry = Get-FirstPoseStateTelemetry `
                -Response $state -Pose $firstPose
            $afterStateMemory = Assert-MemoryBudget -Handle $process `
                -Checkpoint "after diagnostic state $sequence"

            $sample = [pscustomobject] [ordered] @{
                Sequence = $sequence
                ObservedUtc = [string] $afterStateMemory.ObservedUtc
                ElapsedSeconds = [Math]::Round(
                    ([DateTime]::UtcNow - $diagnosticStartedUtc).TotalSeconds,
                    3)
                ProcessPrivateBytes =
                    [int64] $afterStateMemory.ProcessPrivateBytes
                ProcessWorkingSetBytes =
                    [int64] $afterStateMemory.ProcessWorkingSetBytes
                SystemFreePhysicalBytes =
                    [int64] $afterStateMemory.SystemFreePhysicalBytes
                SystemFreeVirtualBytes =
                    [int64] $afterStateMemory.SystemFreeVirtualBytes
                CesiumLoadProgress =
                    [double] $stateTelemetry.CesiumLoadProgress
                ProviderReadyForProof =
                    [bool] $stateTelemetry.ProviderReadyForProof
                LocalFallbackHidden =
                    [bool] $stateTelemetry.LocalFallbackHidden
                ExactQaViewPose = $true
                FreshSelectionStatsCount = [int] $freshSelectionStatsCount
                SelectionStats = $script:latestSelectionStats
                Http429EventCount = [int] $script:http429EventCount
            }
            $telemetry.Add($sample)
            Write-SanitizedTelemetry -Sample $sample
            $sequence += 1

            $nextSampleUtc = $sampleCycleStartedUtc.AddSeconds(
                $TelemetryIntervalSeconds)
            while ([DateTime]::UtcNow -lt $nextSampleUtc -and
                [DateTime]::UtcNow -lt $diagnosticDeadline) {
                Start-Sleep -Seconds 1
                Assert-ProtectedUE54Unchanged `
                    -Before $script:protectedUE54Before | Out-Null
                Assert-LaunchedProcessIdentity -Expected $launchedIdentity |
                    Out-Null
                Assert-MemoryBudget -Handle $process `
                    -Checkpoint "between diagnostic samples $sequence" |
                    Out-Null
            }
        }
        catch {
            if ($_.Exception.Message.StartsWith(
                    'DIAGNOSTIC_WINDOW_DEADLINE:',
                    [StringComparison]::Ordinal)) {
                $diagnosticWindowCompleted = $true
                break diagnosticLoop
            }
            throw
        }
    } while ([DateTime]::UtcNow -lt $diagnosticDeadline)
    $diagnosticEndedUtc = [DateTime]::UtcNow
    $script:diagnosticDeadlineIsActive = $false
    if (-not $diagnosticWindowCompleted) {
        $diagnosticWindowCompleted = $true
    }

    if ($telemetry.Count -eq 0) {
        throw 'No sanitized periodic telemetry samples were recorded.'
    }
    if ($script:selectionStatsObservedCount -eq 0) {
        throw 'CESIUM_SELECTION_STATS_UNOBSERVED: LogSelectionStats produced no parseable aggregate line.'
    }
}
catch {
    $workflowError = $_.Exception
}
finally {
    $script:diagnosticDeadlineIsActive = $false
    $preCleanupWatchdogSnapshot =
        Get-ContinuousMemoryWatchdogSnapshot
    if (Test-ContinuousMemoryWatchdogMemoryAlert `
            -Snapshot $preCleanupWatchdogSnapshot) {
        $watchdogMemoryAbort = $true
        $workflowError = [InvalidOperationException]::new(
            "$($preCleanupWatchdogSnapshot.AlertKind): source=continuous_watchdog privateBytes=$($preCleanupWatchdogSnapshot.AlertPrivateBytes) freeVirtualBytes=$($preCleanupWatchdogSnapshot.AlertAvailableCommitBytes)")
    }
    if ($null -ne $process -and $null -eq $launchedIdentity) {
        try {
            $process.Refresh()
            if (-not $process.HasExited) {
                $candidate = Get-ProcessIdentity -ProcessId `
                    ([uint32] $process.Id)
                if (Test-ExpectedHelperLaunchIdentity -Identity $candidate `
                        -Handle $process) {
                    $launchedIdentity = $candidate
                }
                else {
                    throw 'Live started process could not be recovered as the exact owned helper.'
                }
            }
        }
        catch {
            $cleanupErrors.Add(
                "Startup identity recovery failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
    }
    if ($null -ne $process -and $null -ne $launchedIdentity) {
        if ($watchdogMemoryAbort) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    # The watchdog has already proved a threshold breach. Give
                    # the exact owned editor one immediate graceful-quit
                    # opportunity while the independent watchdog retains the
                    # short persistent-breach containment deadline.
                    $quitRequested = $true
                    $quitConnectionResult = Invoke-GracefulQuit `
                        -ExpectedProcess $launchedIdentity -Handle $process
                }
            }
            catch {
                $cleanupErrors.Add(
                    "Emergency QuitEditor failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
            }
        }
        try {
            $process.Refresh()
            if (-not $watchdogMemoryAbort -and
                -not $process.HasExited -and $pieMayBeActive -and
                $selectionStatsMayBeEnabled) {
                Set-OwnedLogSelectionStats `
                    -ExpectedProcess $launchedIdentity -Handle $process `
                    -Enabled $false -SkipMemoryGuard
                $selectionStatsMayBeEnabled = $false
            }
        }
        catch {
            $cleanupErrors.Add(
                "LogSelectionStats restore failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $watchdogMemoryAbort -and
                -not $process.HasExited -and $pieMayBeActive -and
                ($pieInheritedSuspendedState -or
                    $pieTilesetResumedAtFirstPose)) {
                Set-OwnedTilesetSuspendUpdate `
                    -ExpectedProcess $launchedIdentity -Handle $process `
                    -ObjectPath $pieTilesetObjectPath -Suspended $true `
                    -SkipMemoryGuard
            }
        }
        catch {
            $cleanupErrors.Add(
                "PIE tileset suspend before teardown failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $watchdogMemoryAbort -and
                -not $process.HasExited -and $pieMayBeActive) {
                $endPlayEvidence = Invoke-OwnedEndPlay `
                    -ExpectedProcess $launchedIdentity -Handle $process
                $pieMayBeActive = $false
            }
        }
        catch {
            $cleanupErrors.Add(
                "EndPlay failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $watchdogMemoryAbort -and
                -not $process.HasExited -and
                $editorTilesetSuspendApplied) {
                Set-OwnedTilesetSuspendUpdate `
                    -ExpectedProcess $launchedIdentity -Handle $process `
                    -ObjectPath $editorTilesetObjectPath -Suspended $false `
                    -SkipMemoryGuard
                $editorTilesetRestoredBeforeQuit = $true
                $editorTilesetRestorationDisposition =
                    'restored_false_before_quit'
            }
            elseif ($process.HasExited -and
                $editorTilesetSuspendApplied) {
                $editorTilesetRestorationDisposition =
                    'transient_state_destroyed_with_process'
            }
        }
        catch {
            $cleanupErrors.Add(
                "Editor tileset restore failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $process.HasExited -and -not $quitRequested) {
                $quitRequested = $true
                $quitConnectionResult = Invoke-GracefulQuit `
                    -ExpectedProcess $launchedIdentity -Handle $process
            }
        }
        catch {
            $cleanupErrors.Add(
                "QuitEditor failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $process.HasExited -and
                -not (Wait-ExactProcessExit -ExpectedProcess $launchedIdentity `
                    -Handle $process -TimeoutSeconds 180)) {
                $forcedContainment = $true
                Stop-ExactHelperForContainment `
                    -ExpectedProcess $launchedIdentity -Handle $process
            }
            $process.Refresh()
            if ($process.HasExited) {
                $process.WaitForExit()
                $exitCode = $process.ExitCode
            }
        }
        catch {
            $cleanupErrors.Add(
                "Exact helper containment failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
    }
    if ($null -ne $process) {
        try {
            $process.Refresh()
            if ($process.HasExited) {
                $process.WaitForExit()
                $exitCode = $process.ExitCode
                if ($editorTilesetSuspendApplied -and
                    -not $editorTilesetRestoredBeforeQuit) {
                    $editorTilesetRestorationDisposition =
                        'transient_state_destroyed_with_process'
                }
            }
        }
        catch {
            $cleanupErrors.Add(
                "Final process exit observation failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
        }
    }
    try {
        $continuousWatchdogSnapshot =
            Stop-ContinuousMemoryWatchdog
        if ($null -ne $continuousWatchdogSnapshot) {
            $script:peakPrivateBytes = [Math]::Max(
                [int64] $script:peakPrivateBytes,
                [int64] $continuousWatchdogSnapshot.PeakPrivateBytes)
            if ([uint64] $continuousWatchdogSnapshot.MinimumAvailableCommitBytes -gt 0) {
                $watchdogMinimum = [int64] [Math]::Min(
                    [decimal] $continuousWatchdogSnapshot.MinimumAvailableCommitBytes,
                    [decimal] [int64]::MaxValue)
                $script:minimumFreeVirtualBytes = [Math]::Min(
                    [int64] $script:minimumFreeVirtualBytes,
                    $watchdogMinimum)
            }
            if (Test-ContinuousMemoryWatchdogMemoryAlert `
                    -Snapshot $continuousWatchdogSnapshot) {
                $watchdogMemoryAbort = $true
                $workflowError = [InvalidOperationException]::new(
                    "$($continuousWatchdogSnapshot.AlertKind): source=continuous_watchdog privateBytes=$($continuousWatchdogSnapshot.AlertPrivateBytes) freeVirtualBytes=$($continuousWatchdogSnapshot.AlertAvailableCommitBytes)")
            }
        }
    }
    catch {
        $cleanupErrors.Add(
            "Continuous watchdog shutdown failed: $(Sanitize-DiagnosticText $_.Exception.Message)")
    }
    ${env:UE-LocalDataCachePath} = $oldLocalDdc
    $script:httpClient.Dispose()
}

$protectedUE54After = $null
try {
    $protectedUE54After = Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before
}
catch {
    $postconditionErrors.Add(
        (Sanitize-DiagnosticText $_.Exception.Message))
}
try {
    Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'final postflight'
}
catch {
    $postconditionErrors.Add(
        (Sanitize-DiagnosticText $_.Exception.Message))
}
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    $postconditionErrors.Add(
        'A UE5.5/native TRIAD Unreal helper remains after final containment.')
}
if (@(Get-RemoteControlListeners).Count -ne 0) {
    $postconditionErrors.Add(
        'RC port 30010 still has a listener after final containment.')
}
if ($forcedContainment -and -not $watchdogMemoryAbort) {
    $postconditionErrors.Add(
        'The exact helper required forced containment; the diagnostic cannot pass.')
}
if (-not $quitRequested -and -not $watchdogMemoryAbort) {
    $postconditionErrors.Add('Graceful QuitEditor was never requested.')
}
if ($exitCode -ne 0 -and -not $watchdogMemoryAbort) {
    $postconditionErrors.Add(
        "UE5.5 helper exit code was not zero: $exitCode")
}

$cacheAfter = $null
try {
    $cacheAfter = Get-GrowingFileState -Path $persistentCachePath
    if ([string] $cacheAfter.Path -cne [string] $cacheBefore.Path) {
        throw 'Persistent cache path changed during the diagnostic.'
    }
}
catch {
    $postconditionErrors.Add(
        (Sanitize-DiagnosticText $_.Exception.Message))
}

# A watchdog-triggered resource abort is itself the measurement result. Once
# exact containment, RC release, immutable pins, CAPSTONE preservation, and
# cache-path preservation have all passed, teardown errors caused by the
# emergency exit are retained as observations in the non-proof receipt rather
# than suppressing that receipt.
if ($watchdogMemoryAbort -and $cleanupErrors.Count -ne 0) {
    $memoryAbortCleanupObservations = @($cleanupErrors)
    $cleanupErrors.Clear()
}

if ($cleanupErrors.Count -ne 0 -or $postconditionErrors.Count -ne 0) {
    $workflowText = if ($null -eq $workflowError) {
        'none'
    }
    else { Sanitize-DiagnosticText $workflowError.Message }
    throw "V5D Cesium diagnostic failed closed before receipt: workflow=$workflowText cleanup=$([string]::Join(' | ', @($cleanupErrors))) postconditions=$([string]::Join(' | ', @($postconditionErrors)))"
}

$workflowMessage = if ($null -eq $workflowError) {
    ''
}
else { Sanitize-DiagnosticText $workflowError.Message }
$outcome = if ($null -eq $workflowError -and
    $diagnosticWindowCompleted) {
    'COMPLETED_NON_PROOF_DIAGNOSTIC'
}
elseif ($workflowMessage.StartsWith(
        'MEMORY_GUARD_', [StringComparison]::Ordinal)) {
    'ABORTED_BY_MEMORY_GUARD_NON_PROOF'
}
else {
    'FAILED_NON_PROOF_DIAGNOSTIC'
}

$logIdentity = if ($watchdogMemoryAbort) {
    # A startup breach can be contained before Unreal creates -abslog. The
    # non-proof safety receipt must still record that exact absence.
    Get-FileStateIdentity -Path $logFile
}
else {
    Get-FileIdentity -Path $logFile
}
$result = [pscustomobject] [ordered] @{
    Schema = $diagnosticSchema
    Classification = $diagnosticClassification
    Outcome = $outcome
    ProofEligible = $false
    ProviderReadyProofClaimed = $false
    GlobalProviderReadyGateRequired = $false
    CaptureCount = 0
    PublicationPerformed = $false
    DiagnosticReceiptIsProofPublication = $false
    RunToken = $RunToken
    Script = $selfPin
    StartedUtc = if ($null -eq $diagnosticStartedUtc) {
        $null
    }
    else { $diagnosticStartedUtc.ToString('o') }
    EndedUtc = if ($null -eq $diagnosticEndedUtc) {
        [DateTime]::UtcNow.ToString('o')
    }
    else { $diagnosticEndedUtc.ToString('o') }
    RequestedDurationSeconds = $DiagnosticDurationSeconds
    WindowCompleted = $diagnosticWindowCompleted
    FirstPose = $firstPose
    FirstPoseAcknowledgementAccepted =
        $firstPoseAcknowledgementAccepted
    MapValidationAccepted = $mapValidationReportAccepted
    InitialPieValidationAccepted = $initialPieValidationAccepted
    MemoryGuard = [pscustomobject] [ordered] @{
        PrivateMemoryCeilingBytes = $privateMemoryCeilingBytes
        MinimumSystemFreeVirtualBytes =
            $minimumSystemFreeVirtualBytes
        MinimumSystemFreeVirtualAtLaunchBytes =
            $minimumSystemFreeVirtualAtLaunchBytes
        Coverage =
            'CONTINUOUS_FROM_IMMEDIATELY_AFTER_START_PROCESS_THROUGH_FINAL_TEARDOWN_PLUS_COOPERATIVE_BOUNDARY_SAMPLES'
        PeakPrivateBytes = $script:peakPrivateBytes
        PeakWorkingSetBytes = $script:peakWorkingSetBytes
        MinimumFreePhysicalBytes = if (
            $script:minimumFreePhysicalBytes -eq [int64]::MaxValue) {
            $null
        }
        else { $script:minimumFreePhysicalBytes }
        MinimumFreeVirtualBytes = if (
            $script:minimumFreeVirtualBytes -eq [int64]::MaxValue) {
            $null
        }
        else { $script:minimumFreeVirtualBytes }
        Trigger = if ($workflowMessage.StartsWith(
                'MEMORY_GUARD_', [StringComparison]::Ordinal)) {
            $workflowMessage
        }
        else { $null }
        ContinuousWatchdog = [pscustomobject] [ordered] @{
            PollMilliseconds = $memoryWatchdogPollMilliseconds
            PersistentBreachContainmentMilliseconds =
                $memoryWatchdogPersistentBreachMilliseconds
            IndependentClrThread = $true
            Snapshot = $continuousWatchdogSnapshot
            MemoryAbort = $watchdogMemoryAbort
            EmergencyCleanupObservations =
                @($memoryAbortCleanupObservations)
        }
    }
    CesiumStreamingIsolation = [pscustomobject] [ordered] @{
        Profile = $CesiumStreamingProfile
        EditorTilesetObjectPath = $editorTilesetObjectPath
        PieTilesetObjectPath = $pieTilesetObjectPath
        EditorWorldSuspendAppliedBeforeMapValidation =
            $editorTilesetSuspendApplied
        PieInheritedSuspendedState = $pieInheritedSuspendedState
        ExactFirstPoseSetBeforePieResume =
            $exactFirstPoseSetBeforeResume
        PieTilesetResumedAtFirstPose =
            $pieTilesetResumedAtFirstPose
        EditorWorldRemainedSuspendedDuringPie =
            $editorWorldRemainedSuspendedDuringPie
        EditorTilesetRestoredBeforeQuit =
            $editorTilesetRestoredBeforeQuit
        EditorTilesetRestorationDisposition =
            $editorTilesetRestorationDisposition
        SerializedOrProofTimeQualityTupleMutated = $false
        MaximumScreenSpaceError = 1.0
        MaximumSimultaneousTileLoads = 12
        MapSaveRequested = $false
    }
    SelectionTelemetry = [pscustomobject] [ordered] @{
        Source = 'Cesium3DTileset.LogSelectionStats'
        PulseMilliseconds = $selectionStatsPulseMilliseconds
        ParsedAggregateEventCount =
            $script:selectionStatsObservedCount
        Http429EventCount = $script:http429EventCount
        SanitizedAggregateOnly = $true
        RawLogLinesCopiedToReceipt = $false
        Samples = @($telemetry)
    }
    PersistentCache = [pscustomobject] [ordered] @{
        Policy = $persistentCachePolicy
        Before = $cacheBefore
        After = $cacheAfter
        ByteDelta = [int64] $cacheAfter.Bytes - [int64] $cacheBefore.Bytes
        RemovedClearedOrRelocatedByWrapper = $false
    }
    Isolation = [pscustomobject] [ordered] @{
        ProtectedUE54Policy =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        ProtectedUE54Before = Convert-ProtectedSnapshotForReceipt `
            -Snapshot $script:protectedUE54Before
        ProtectedUE54After = Convert-ProtectedSnapshotForReceipt `
            -Snapshot $protectedUE54After
        NativeUE55AndTRIADHelpersIdleBeforeAndAfter = $true
        RemoteControlEndpointAllowlist =
            $remoteControlEndpointAllowlist
        RemoteControlPort = 30010
        RemoteControlReleased = $true
        GracefulQuitRequested = $quitRequested
        ForcedContainmentUsed =
            $forcedContainment -or
            ($null -ne $continuousWatchdogSnapshot -and
                [bool] $continuousWatchdogSnapshot.ForceKillUsed)
        MainThreadForcedContainmentUsed = $forcedContainment
        ContinuousWatchdogForcedContainmentUsed =
            $null -ne $continuousWatchdogSnapshot -and
            [bool] $continuousWatchdogSnapshot.ForceKillUsed
        ExitCode = $exitCode
        EndPlay = $endPlayEvidence
        QuitConnectionResult = $quitConnectionResult
    }
    Pins = [pscustomobject] [ordered] @{
        Editor = $editorPin
        Project = $projectPin
        Map = $mapPin
        RuntimeDll = $runtimeDllPin
        EditorDll = $editorDllPin
    }
    Log = $logIdentity
    Failure = if ($null -eq $workflowError) {
        $null
    }
    else {
        [pscustomobject] [ordered] @{
            SanitizedMessage = $workflowMessage
            ContainsRawProviderUrlOrCredential = $false
        }
    }
    ProviderGeometryExportedTracedAnalysedOrBaked = $false
    ProviderUrlOrCredentialCopiedToTelemetryOrReceipt = $false
}

$json = $result | ConvertTo-Json -Depth 20
$temporaryReceipt = $diagnosticReceiptPath + '.tmp.' +
    [Guid]::NewGuid().ToString('N')
try {
    [IO.File]::WriteAllText(
        $temporaryReceipt,
        $json + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
    if ([IO.File]::Exists($diagnosticReceiptPath)) {
        throw "Diagnostic receipt destination appeared before publication: $diagnosticReceiptPath"
    }
    [IO.File]::Move($temporaryReceipt, $diagnosticReceiptPath)
}
finally {
    if ([IO.File]::Exists($temporaryReceipt)) {
        [IO.File]::Delete($temporaryReceipt)
    }
}

$published = Get-FileIdentity -Path $diagnosticReceiptPath
if ($outcome -cne 'COMPLETED_NON_PROOF_DIAGNOSTIC') {
    throw "V5D Cesium diagnostic ended as $outcome; non-proof receipt=$($published.Path) sha256=$($published.Sha256) failure=$workflowMessage"
}

[pscustomobject] [ordered] @{
    Status = 'CESIUM_FIRST_POSE_MEMORY_QUEUE_DIAGNOSTIC_V1_PASS'
    Classification = $diagnosticClassification
    ProofEligible = $false
    CaptureCount = 0
    PublicationPerformed = $false
    Receipt = $published
} | ConvertTo-Json -Depth 8
