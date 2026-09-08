#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$')]
    [string] $RunToken,

    # Supply these from the exact native build/promotion receipt. They are not
    # inferred: the caller must deliberately select the map and both binaries
    # whose live presentation is being evidenced.
    [int64] $ExpectedMapBytes = 34993427L,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedMapSha256 =
        '38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9',

    [ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]
    [string] $ProviderEvidenceMode = 'TelemetryOnly',

    [ValidateRange(6, 120)]
    [int] $TelemetryDwellSeconds = 12,

    # The tree/turf presentation is implemented across both modules, so live
    # capture requires explicit identities for both DLLs.
    [int64] $ExpectedRuntimeDllBytes = 4585984L,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedRuntimeDllSha256 =
        '31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4',

    [int64] $ExpectedEditorDllBytes = 7671296L,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedEditorDllSha256 =
        '471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34',

    # R27 evidence is opt-in so the historical R25/R23B capture path remains
    # replayable. The strict mode requires the caller to bind all six native
    # identities explicitly and to pin the exact successful R27 commit receipt.
    [switch] $RequireLandmarkVegetationR27,

    [string] $R27CommitReceiptPath = '',

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR27CommitReceiptSha256 = '',

    # R28 evidence is a separate, mutually exclusive successor contract. The
    # six generic identities above must be supplied explicitly from the R28
    # commit receipt; no successor pin is inferred from the native tree.
    [switch] $RequireR28VisualSuccessor,

    [string] $R28CommitReceiptPath = '',

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR28CommitReceiptSha256 = '',

    [switch] $StaticSelfCheck,

    [ValidateRange(60, 900)]
    [int] $EditorTimeoutSeconds = 600,

    [ValidateRange(60, 300)]
    [int] $PieTimeoutSeconds = 120,

    [ValidateRange(60, 3600)]
    [int] $ProviderTimeoutSeconds = 900,

    # Zero preserves the original single-attempt behavior. The bounded
    # ProviderReady retry orchestrator supplies a positive value so this
    # strict leaf attempt stops at the first configured number of explicit
    # HTTP 429 tile responses, shuts PIE/editor down through the normal
    # owned-process cleanup path, and never publishes partial evidence.
    [ValidateRange(0, 100)]
    [int] $ProviderReadyRateLimitAbortThreshold = 0,

    [ValidateRange(30, 300)]
    [int] $CaptureTimeoutSeconds = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:r27MaterialBoundaryArmed = $false
$privateMemoryCeilingBytes = 12884901888L # 12 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # 6 GiB
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # 10 GiB
$memoryWatchdogPollMilliseconds = 100
$memoryWatchdogPersistentBreachMilliseconds = 2000
$captureTextureStreamingPoolMiB = 768
$captureTextureStreamingPoolOverrideToken =
    "-ini:Engine:[ConsoleVariables]:r.Streaming.PoolSize=$captureTextureStreamingPoolMiB"
$script:memoryWatchdog = $null
# This leaf now carries the same independent CLR-thread implementation used by
# the bounded Cesium diagnostic. It starts immediately after Start-Process,
# monitors through final teardown, and can contain only the exact PID/start
# time/executable tuple supplied by the owned process handle.
$continuousMemoryWatchdogIntegrated = $true

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
$suppressedFallbackMeshAsset =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'
$outerGroundMeshAsset =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\OuterGroundLoadingFallback\SM_IPV5D_OuterGroundLoadingFallback_Render.uasset'
$outerGroundMaterialAsset =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\OuterGroundLoadingFallback\Materials\M_IPV5D_OuterGroundLoadingFallback.uasset'
$r27TransactionBase =
    'D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DLandmarkVegetationR27V1'
$r28TransactionBase =
    'D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DVisualRealismR28V1'
$r27GrassMaterialRoot =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials'
$r27GrassMaterialAssets = @(
    (Join-Path $r27GrassMaterialRoot `
        'M_IPV5D_LandmarkTurf_R27_Manicured.uasset'),
    (Join-Path $r27GrassMaterialRoot `
        'M_IPV5D_LandmarkTurf_R27_Humid.uasset'),
    (Join-Path $r27GrassMaterialRoot `
        'M_IPV5D_LandmarkTurf_R27_Shade.uasset'),
    (Join-Path $r27GrassMaterialRoot `
        'M_IPV5D_LandmarkTurf_R27_DryEdge.uasset')
)
$ddcRoot = 'D:\triad\TRIAD\Saved\DerivedDataCache'
$outputRoot = 'D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D'
$logRoot = 'D:\triad\TRIAD\Saved\Logs'
$logFile = Join-Path $logRoot `
    "Codex_V5D_VegetationProvider_${RunToken}.log"
$evidenceManifestPath = Join-Path $outputRoot `
    "explore_v5d_vegetation_provider_evidence_${RunToken}.json"
$rcCallUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$v5dLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$landmarkVegetationLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary'
$temasekShophouseLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DTemasekShophouseEditorLibrary'
$levelEditor = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$strictFallbackCaptureAcknowledgementMarker =
    'localFallbackVisible=true localFallbackHidden=false'
$providerRateLimitLogMarker =
    'Received status code 429 for tile content '

# Every live user-owned UE5.4 CAPSTONE editor is protected as an exact set.
# The helper may run beside that set, but it never sends RC/quit/containment
# operations to those processes and fails closed if any identity changes.
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$nativeUE55EngineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')

$expectedProjectBytes = 1298L
$expectedProjectSha256 =
    '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'

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

function Get-SystemMemorySnapshot {
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    [pscustomobject] [ordered] @{
        ObservedUtc = [DateTime]::UtcNow.ToString('o')
        FreePhysicalBytes = [int64] $os.FreePhysicalMemory * 1KB
        FreeVirtualBytes = [int64] $os.FreeVirtualMemory * 1KB
    }
}

function Assert-SystemStartupHeadroom {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    $snapshot = Get-SystemMemorySnapshot
    if ($snapshot.FreeVirtualBytes -lt
        $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "MEMORY_GUARD_STARTUP_HEADROOM: checkpoint=$Checkpoint freeVirtualBytes=$($snapshot.FreeVirtualBytes) requiredMinimumBytes=$minimumSystemFreeVirtualAtLaunchBytes"
    }
    $snapshot
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

function Assert-FilePin {
    param(
        [Parameter(Mandatory = $true)] $Pin,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    $actual = Get-FileStateIdentity -Path ([string] $Pin.Path)
    if ([bool] $actual.Present -ne [bool] $Pin.Present -or
        [int64] $actual.Bytes -ne [int64] $Pin.Bytes -or
        [string] $actual.Sha256 -cne [string] $Pin.Sha256) {
        throw "File identity changed at '$Checkpoint': $($Pin.Path) expected=$($Pin.Present)/$($Pin.Bytes)/$($Pin.Sha256) actual=$($actual.Present)/$($actual.Bytes)/$($actual.Sha256)"
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
    if ($RequireLandmarkVegetationR27 -and
        $script:r27MaterialBoundaryArmed) {
        # Individual pins reject mutation/removal; exact re-enumeration also
        # rejects a fifth package or sidecar appearing in the R27 namespace.
        [void] @(Get-ExactR27GrassMaterialPins)
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

function Assert-RequiredReportMarkers {
    param(
        [Parameter(Mandatory = $true)] [string] $Report,
        [Parameter(Mandatory = $true)] [string[]] $Markers,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    foreach ($marker in $Markers) {
        if (-not $Report.Contains($marker, [StringComparison]::Ordinal)) {
            throw "$Description lacks required marker: $marker"
        }
    }
}

function Assert-ReportMetricMinimum {
    param(
        [Parameter(Mandatory = $true)] [string] $Report,
        [Parameter(Mandatory = $true)] [string] $Name,
        [Parameter(Mandatory = $true)] [double] $Minimum,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    $pattern = '(?:^|\s)' + [regex]::Escape($Name) +
        '=([0-9]+(?:\.[0-9]+)?)'
    $match = [regex]::Match($Report, $pattern,
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if (-not $match.Success) {
        throw "$Description lacks one finite $Name metric."
    }
    $value = [double]::Parse($match.Groups[1].Value,
        [Globalization.CultureInfo]::InvariantCulture)
    if (-not [double]::IsFinite($value) -or $value -lt $Minimum) {
        throw "$Description has out-of-contract $Name=$value; minimum=$Minimum."
    }
}

function Get-RequiredObjectProperty {
    param(
        [Parameter(Mandatory = $true)] $Object,
        [Parameter(Mandatory = $true)] [string] $Name,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    if ($null -eq $Object) {
        throw "$Description is null while reading required property '$Name'."
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Description lacks required property '$Name'."
    }
    $property.Value
}

function Assert-ReceiptFilePin {
    param(
        [Parameter(Mandatory = $true)] $ReceiptPin,
        [Parameter(Mandatory = $true)] $LivePin,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    $receiptPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredObjectProperty $ReceiptPin 'Path' $Description))
    $receiptPresent = [bool] (
        Get-RequiredObjectProperty $ReceiptPin 'Present' $Description)
    $receiptBytes = [int64] (
        Get-RequiredObjectProperty $ReceiptPin 'Bytes' $Description)
    $receiptSha256 = [string] (
        Get-RequiredObjectProperty $ReceiptPin 'Sha256' $Description)
    if (-not $receiptPresent -or
        $receiptPath -ine [string] $LivePin.Path -or
        $receiptBytes -ne [int64] $LivePin.Bytes -or
        $receiptSha256 -cne [string] $LivePin.Sha256) {
        throw "$Description does not match the live exact file identity: receipt=$receiptPresent/$receiptPath/$receiptBytes/$receiptSha256 live=$($LivePin.Present)/$($LivePin.Path)/$($LivePin.Bytes)/$($LivePin.Sha256)"
    }
}

function Get-ExactR27GrassMaterialPins {
    if (-not [IO.Directory]::Exists($r27GrassMaterialRoot)) {
        throw "The R27 grass-material root is absent: $r27GrassMaterialRoot"
    }
    $rootItem = Get-Item -LiteralPath $r27GrassMaterialRoot -Force
    if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "The R27 grass-material root is a reparse point: $r27GrassMaterialRoot"
    }
    foreach ($directory in @(Get-ChildItem -LiteralPath $r27GrassMaterialRoot `
            -Directory -Recurse -Force)) {
        if (($directory.Attributes -band
                [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "The R27 material namespace contains a reparse directory: $($directory.FullName)"
        }
    }

    $actualPaths = @(Get-ChildItem -LiteralPath $r27GrassMaterialRoot `
        -File -Recurse -Force | ForEach-Object {
            [IO.Path]::GetFullPath($_.FullName)
        } | Sort-Object)
    $expectedPaths = @($r27GrassMaterialAssets | ForEach-Object {
        [IO.Path]::GetFullPath($_)
    } | Sort-Object)
    if ($actualPaths.Count -ne 4 -or $expectedPaths.Count -ne 4 -or
        [string]::Join('|', $actualPaths).ToLowerInvariant() -cne
            [string]::Join('|', $expectedPaths).ToLowerInvariant()) {
        throw "The R27 material namespace is not the exact four-package roster: actual=$([string]::Join(',', $actualPaths))"
    }
    @($expectedPaths | ForEach-Object { Get-FileIdentity -Path $_ })
}

function Get-ValidatedR27CommitReceipt {
    param(
        [Parameter(Mandatory = $true)] $MapPin,
        [Parameter(Mandatory = $true)] $RuntimePin,
        [Parameter(Mandatory = $true)] $EditorPin,
        [Parameter(Mandatory = $true)] [object[]] $MaterialPins
    )

    $receiptPath = [IO.Path]::GetFullPath($R27CommitReceiptPath)
    $transactionBase = [IO.Path]::GetFullPath($r27TransactionBase)
    $receiptParent = [IO.Path]::GetDirectoryName($receiptPath)
    $receiptGrandparent = [IO.Path]::GetDirectoryName($receiptParent)
    if ([IO.Path]::GetFileName($receiptPath) -cne 'commit.json' -or
        $receiptGrandparent -ine $transactionBase) {
        throw "R27 receipt must be one exact run-token commit.json directly below $transactionBase"
    }
    if ([IO.Path]::GetFileName($receiptParent) -cnotmatch
            '^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$') {
        throw 'R27 receipt parent is not a valid transaction run token.'
    }

    $receiptPin = Get-FileIdentity -Path $receiptPath
    Assert-ExactPin -Pin $receiptPin -Bytes $receiptPin.Bytes `
        -Sha256 $ExpectedR27CommitReceiptSha256 `
        -Description 'selected R27 commit receipt'
    $receipt = Get-Content -LiteralPath $receiptPath -Raw -Encoding utf8 |
        ConvertFrom-Json -Depth 32
    if ([string] (Get-RequiredObjectProperty $receipt 'Schema' 'R27 receipt') `
            -cne 'triad.istana_explore_v5d.landmark_vegetation_r27.native_transaction.v1' -or
        [string] (Get-RequiredObjectProperty $receipt 'Status' 'R27 receipt') `
            -cne 'PASS' -or
        [bool] (Get-RequiredObjectProperty $receipt `
            'VisualCaptureAccepted' 'R27 receipt') -or
        -not [bool] (Get-RequiredObjectProperty $receipt `
            'CaptureRevalidationRequired' 'R27 receipt') -or
        [bool] (Get-RequiredObjectProperty $receipt `
            'CollisionNavigationSensorRfAuthority' 'R27 receipt')) {
        throw 'R27 receipt lost its PASS/pending-visual-review/non-authoritative contract.'
    }
    if ([int] (Get-RequiredObjectProperty $receipt 'SourceCount' 'R27 receipt') `
            -ne 8 -or
        [int] (Get-RequiredObjectProperty $receipt `
            'Phase2CompiledSourceCount' 'R27 receipt') -ne 9 -or
        [int] (Get-RequiredObjectProperty $receipt `
            'RenderingCapableFreshEditorProcesses' 'R27 receipt') -ne 7) {
        throw 'R27 receipt lost its exact source/Phase-2/fresh-editor stage census.'
    }

    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt 'SuccessorMap' 'R27 receipt') `
        $MapPin 'R27 successor map receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt 'RuntimeDll' 'R27 receipt') `
        $RuntimePin 'R27 runtime DLL receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt 'EditorDll' 'R27 receipt') `
        $EditorPin 'R27 editor DLL receipt'

    $receiptMaterials = @(
        Get-RequiredObjectProperty $receipt 'R27MaterialPackages' 'R27 receipt')
    if ($receiptMaterials.Count -ne 4 -or $MaterialPins.Count -ne 4) {
        throw 'R27 receipt/live material rosters must each contain exactly four files.'
    }
    foreach ($materialPin in $MaterialPins) {
        $receiptMaterial = @($receiptMaterials | Where-Object {
            [IO.Path]::GetFullPath([string] $_.Path) -ieq
                [string] $materialPin.Path
        })
        if ($receiptMaterial.Count -ne 1) {
            throw "R27 receipt lacks exactly one material pin for $($materialPin.Path)"
        }
        Assert-ReceiptFilePin $receiptMaterial[0] $materialPin `
            'R27 material package receipt'
    }

    $expectedStages = @(
        '00_cold_validate_phase2_pre_r27',
        '01_build_r27_grass_materials',
        '02_validate_reusable_assets',
        '03_apply_r27_visual_correction',
        '04_cold_validate_r27_map',
        '05_idempotent_r27_apply',
        '06_cold_validate_phase2_post_r27'
    )
    $receiptStages = @(
        Get-RequiredObjectProperty $receipt 'Stages' 'R27 receipt')
    if ($receiptStages.Count -ne $expectedStages.Count) {
        throw 'R27 receipt does not contain the exact seven-stage sequence.'
    }
    for ($index = 0; $index -lt $expectedStages.Count; ++$index) {
        if ([string] (Get-RequiredObjectProperty $receiptStages[$index] `
                'Stage' "R27 receipt stage $index") -cne
            $expectedStages[$index]) {
            throw "R27 receipt stage order mismatch at index $index."
        }
    }

    [pscustomobject] [ordered] @{
        Pin = $receiptPin
        Receipt = $receipt
        RunToken = [IO.Path]::GetFileName($receiptParent)
    }
}

function Get-ValidatedR28CommitReceipt {
    param(
        [Parameter(Mandatory = $true)] $MapPin,
        [Parameter(Mandatory = $true)] $RuntimePin,
        [Parameter(Mandatory = $true)] $EditorPin
    )

    $receiptPath = [IO.Path]::GetFullPath($R28CommitReceiptPath)
    $transactionBase = [IO.Path]::GetFullPath($r28TransactionBase)
    $receiptParent = [IO.Path]::GetDirectoryName($receiptPath)
    $receiptGrandparent = [IO.Path]::GetDirectoryName($receiptParent)
    if ([IO.Path]::GetFileName($receiptPath) -cne 'commit.json' -or
        $receiptGrandparent -ine $transactionBase) {
        throw "R28 receipt must be one exact run-token commit.json directly below $transactionBase"
    }
    if ([IO.Path]::GetFileName($receiptParent) -cnotmatch
            '^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$') {
        throw 'R28 receipt parent is not a valid transaction run token.'
    }

    $receiptPin = Get-FileIdentity -Path $receiptPath
    Assert-ExactPin -Pin $receiptPin -Bytes $receiptPin.Bytes `
        -Sha256 $ExpectedR28CommitReceiptSha256 `
        -Description 'selected R28 commit receipt'
    $receipt = Get-Content -LiteralPath $receiptPath -Raw -Encoding utf8 |
        ConvertFrom-Json -Depth 32
    if ([string] (Get-RequiredObjectProperty $receipt 'Schema' 'R28 receipt') `
            -cne 'triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1' -or
        [string] (Get-RequiredObjectProperty $receipt 'Status' 'R28 receipt') `
            -cne 'PASS' -or
        [bool] (Get-RequiredObjectProperty $receipt `
            'VisualCaptureAccepted' 'R28 receipt') -or
        -not [bool] (Get-RequiredObjectProperty $receipt `
            'CaptureRevalidationRequired' 'R28 receipt') -or
        [bool] (Get-RequiredObjectProperty $receipt `
            'CollisionNavigationSensorRfTerrainAuthority' 'R28 receipt') -or
        [int] (Get-RequiredObjectProperty $receipt `
            'ExactlyNewAssetCount' 'R28 receipt') -ne 17) {
        throw 'R28 receipt lost its PASS/pending-capture/exact-17-asset/render-only non-authoritative contract.'
    }

    $expectedR27MapPin = [pscustomobject] [ordered] @{
        Path = [IO.Path]::GetFullPath($mapFile)
        Present = $true
        Bytes = 36335930L
        Sha256 =
            '9C9660B02F3B9FBF8C679182E8EB39B8FECD0A618AECA6ED546AAD39E0C895E5'
    }
    $expectedR27RuntimePin = [pscustomobject] [ordered] @{
        Path = [IO.Path]::GetFullPath($runtimeDll)
        Present = $true
        Bytes = 4676096L
        Sha256 =
            'C341A9F2583248824911EBB5904EC357E0DCB840304C637D87FD0CE87E67B910'
    }
    $expectedR27EditorPin = [pscustomobject] [ordered] @{
        Path = [IO.Path]::GetFullPath($editorDll)
        Present = $true
        Bytes = 7781888L
        Sha256 =
            '30569F04A956FDCC24F5AE8C0A99D1B70D86C03208E9497EE9BBD8480ABCC2B6'
    }
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt 'PredecessorMap' 'R28 receipt') `
        $expectedR27MapPin 'R28 exact R27 predecessor map receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt `
            'PredecessorRuntimeDll' 'R28 receipt') `
        $expectedR27RuntimePin 'R28 exact R27 predecessor runtime DLL receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt `
            'PredecessorEditorDll' 'R28 receipt') `
        $expectedR27EditorPin 'R28 exact R27 predecessor editor DLL receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt 'SuccessorMap' 'R28 receipt') `
        $MapPin 'R28 successor map receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt `
            'SuccessorRuntimeDll' 'R28 receipt') `
        $RuntimePin 'R28 successor runtime DLL receipt'
    Assert-ReceiptFilePin `
        (Get-RequiredObjectProperty $receipt `
            'SuccessorEditorDll' 'R28 receipt') `
        $EditorPin 'R28 successor editor DLL receipt'

    $expectedStages = @(
        '00_cold_validate_r27_pre_r28',
        '01_cold_validate_phase2_pre_r28',
        '02_build_r28_grass_materials',
        '03_validate_r28_grass_materials',
        '04_validate_r28_vegetation_assets',
        '05_ensure_r28_environment_assets',
        '06_validate_r28_environment_assets',
        '07_apply_combined_r28_visual_successor',
        '08_cold_validate_r28_environment_map',
        '09_cold_validate_combined_r28_map',
        '10_idempotent_combined_r28_apply',
        '11_postvalidate_combined_r28_r25_provider_contract',
        '12_cold_validate_phase2_post_r28',
        '13_postvalidate_r28_grass_materials'
    )
    $receiptStages = @(
        Get-RequiredObjectProperty $receipt 'Stages' 'R28 receipt')
    if ($receiptStages.Count -ne $expectedStages.Count) {
        throw 'R28 receipt does not contain the exact fourteen-stage sequence.'
    }
    for ($index = 0; $index -lt $expectedStages.Count; ++$index) {
        if ([string] (Get-RequiredObjectProperty $receiptStages[$index] `
                'Stage' "R28 receipt stage $index") -cne
            $expectedStages[$index]) {
            throw "R28 receipt stage order mismatch at index $index."
        }
    }

    [pscustomobject] [ordered] @{
        Pin = $receiptPin
        Receipt = $receipt
        RunToken = [IO.Path]::GetFileName($receiptParent)
    }
}

function New-VegetationRangePose {
    param(
        [Parameter(Mandatory = $true)] [string] $Label,
        [Parameter(Mandatory = $true)] [double] $DistanceMeters,
        [Parameter(Mandatory = $true)] [double] $PitchDegrees,
        [Parameter(Mandatory = $true)] [string] $Rationale,
        [Parameter(Mandatory = $true)] [string] $OutputPrefix,
        [double] $WorldXCentimeters = 3500.0,
        [double] $WorldYCentimeters = 15000.0,
        [double] $WorldZCentimeters = 164.0,
        [double] $CameraHeightAboveGroundCentimeters = 164.0,
        [double] $YawDegrees = -90.0,
        [string] $VisualAcceptanceRole = 'CONTEXT_ONLY',
        [string] $SceneTarget = 'R23B_FIXED_RANGE_RIG',
        [double[]] $TargetWorldCentimeters = @()
    )

    # The defaults replay the accepted R23B far-to-near range rig. R27 supplies
    # one explicit landmark-lawn override whose world coordinates are derived
    # from the actor's pinned MacDonald site anchor/yaw and analytic terrain.
    [pscustomobject] [ordered] @{
        Label = $Label
        NominalViewRangeMeters = $DistanceMeters
        X = $WorldXCentimeters
        Y = $WorldYCentimeters
        Z = $WorldZCentimeters
        CameraHeightAboveGroundCentimeters =
            $CameraHeightAboveGroundCentimeters
        Pitch = $PitchDegrees
        Yaw = $YawDegrees
        Roll = 0.0
        Rationale = $Rationale
        VisualAcceptanceRole = $VisualAcceptanceRole
        SceneTarget = $SceneTarget
        TargetWorldCentimeters = $TargetWorldCentimeters
        OutputFileName = "${OutputPrefix}_${Label}_${RunToken}.png"
    }
}

$captureFilenamePrefix = if ($RequireR28VisualSuccessor) {
    'explore_v5d_diagnostic_r28_visual_successor'
}
elseif ($ProviderEvidenceMode -cne 'ProviderReady') {
    'explore_v5d_diagnostic_vegetation_range'
}
else {
    'explore_v5d_vegetation_range'
}
$evidenceModeClassification =
    if ($RequireR28VisualSuccessor) {
        'R28_VISUAL_SUCCESSOR_PLAYER0_RASTER_EVIDENCE_PROVIDER_READINESS_SEPARATE'
    }
    elseif ($ProviderEvidenceMode -ceq 'ProviderReady') {
        'PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED'
    }
    elseif ($ProviderEvidenceMode -ceq 'ProviderFallback') {
        'STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF'
    }
    else { 'EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC' }
$r28EssentialVisualRoles = @(
    'R28_CLOSE_NORMAL_LAWN_TREE',
    'R28_ISTANA_WIDE',
    'R28_TEMASEK_MACDONALD_STREETSCAPE_CONTEXT',
    'R28_BROADER_SURROUNDINGS'
)
$poses = if ($RequireR28VisualSuccessor) {
    @(
        New-VegetationRangePose `
            -Label '020m_close_normal_lawn_tree_r28' `
            -DistanceMeters 20.0 -PitchDegrees -4.703535 `
            -Rationale 'Reuses the bounded 20 m diagnostic rig pose for a normal close Player0 view of the lawn and surrounding tree presentation.' `
            -VisualAcceptanceRole 'R28_CLOSE_NORMAL_LAWN_TREE' `
            -SceneTarget 'R28_LAWN_AND_TREE_NORMAL_CLOSE_CONTEXT' `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose -Label '075m_istana_wide_r28' `
            -DistanceMeters 75.0 -PitchDegrees -1.252968 `
            -Rationale 'Reuses the bounded 75 m diagnostic rig pose for the wide Istana composition and its immediate landscape frame.' `
            -VisualAcceptanceRole 'R28_ISTANA_WIDE' `
            -SceneTarget 'R28_ISTANA_WIDE_CONTEXT' `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose `
            -Label '012m_macdonald_streetscape_r28' `
            -DistanceMeters 12.0 `
            -WorldXCentimeters 35325.4728501344 `
            -WorldYCentimeters 90196.7700142270 `
            -WorldZCentimeters 182.041815825093 `
            -CameraHeightAboveGroundCentimeters 85.0 `
            -PitchDegrees -4.32374585326185 `
            -YawDegrees -71.8858221227921 `
            -Rationale 'Reuses the bounded MacDonald target-locked pose to classify the retained Temasek/MacDonald streetscape context without inventing a new camera trajectory.' `
            -VisualAcceptanceRole 'R28_TEMASEK_MACDONALD_STREETSCAPE_CONTEXT' `
            -SceneTarget 'R28_MACDONALD_STREETSCAPE_CONTEXT' `
            -TargetWorldCentimeters @(35698.5668011438, `
                89056.2434236907, 91.3132032703873) `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose `
            -Label '065m_broader_surroundings_r28' `
            -DistanceMeters 65.0 -PitchDegrees -1.44530995205658 `
            -Rationale 'Reuses the bounded 65 m diagnostic rig pose for broader surrounding greenery, public-realm, and architectural context.' `
            -VisualAcceptanceRole 'R28_BROADER_SURROUNDINGS' `
            -SceneTarget 'R28_BROADER_SURROUNDINGS_CONTEXT' `
            -OutputPrefix $captureFilenamePrefix
    )
}
else {
    @(
        New-VegetationRangePose -Label '075m' -DistanceMeters 75.0 `
            -PitchDegrees -1.252968 `
            -Rationale 'Beyond the 65 m inherited-turf composite cutoff; verifies the far presentation without claiming grass-blade resolution.' `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose -Label '065m' -DistanceMeters 65.0 `
            -PitchDegrees -1.44530995205658 `
            -Rationale 'Nominal 65 m boundary framing from the fixed range rig; the runtime actor report, not raster geometry, proves the cull policy.' `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose -Label '050m' -DistanceMeters 50.0 `
            -PitchDegrees -1.879639 `
            -Rationale 'Inside the 40-65 m inherited-turf composite band and within the intended medium-range tree presentation.' `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose -Label '020m' -DistanceMeters 20.0 `
            -PitchDegrees -4.703535 `
            -Rationale 'Near lawn view where the V5D grass material and automatic tree LOD presentation must remain readable.' `
            -OutputPrefix $captureFilenamePrefix
        if ($RequireLandmarkVegetationR27) {
            New-VegetationRangePose -Label '012m_ground_grazing_r27' `
                -DistanceMeters 12.0 `
                -WorldXCentimeters 35325.4728501344 `
                -WorldYCentimeters 90196.7700142270 `
                -WorldZCentimeters 182.041815825093 `
                -CameraHeightAboveGroundCentimeters 85.0 `
                -PitchDegrees -4.32374585326185 `
                -YawDegrees -71.8858221227921 `
                -Rationale 'Dedicated low 12 m MacDonald landmark-lawn view: the camera is 85 cm above the shared analytic terrain at local (0,-32 m) and targets local (0,-20 m), inside the R27 grass patch but outside the hardscape exclusion. It makes medium-distance blade silhouettes, density continuity, and material fade judgeable without an extreme close-up.' `
                -VisualAcceptanceRole 'PRIMARY_R27_GRASS_READABILITY_MANUAL_REVIEW' `
                -SceneTarget 'R27_MACDONALD_LANDMARK_GRASS_PATCH' `
                -TargetWorldCentimeters @(35698.5668011438, `
                    89056.2434236907, 91.3132032703873) `
                -OutputPrefix $captureFilenamePrefix
        }
        New-VegetationRangePose -Label '008m' -DistanceMeters 8.0 `
            -PitchDegrees -11.829499 `
            -Rationale 'Close lawn view for fine turf density and transition inspection.' `
            -OutputPrefix $captureFilenamePrefix
        New-VegetationRangePose -Label '002m' -DistanceMeters 2.0 `
            -PitchDegrees -55.084794 `
            -Rationale 'Very-close material/mesh inspection using the previously accepted R23B range pose.' `
            -OutputPrefix $captureFilenamePrefix
    )
}

$vegetationMapValidationMarkers = @(
    'cesiumGeoreference=(103.84288055,1.30709615,47.000)',
    'ionAssetId=2275207',
    'maximumSse=1.0',
    'applyDpiScaling=false',
    'forbidHoles=true',
    'loadingDescendantLimit=20',
    'authoredCoreClipShape=irregularEllipse64',
    'deterministicLocalSimulationLayersPreserved=true',
    'r24CoarseLocalLandmarkShellsSuppressed=true',
    'r24SuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490',
    'outerGroundLoadingFallbackProviderCoupled=true',
    'currentContextSuppressionContract=local_fallback_suppression_v2',
    'currentContextTriangles=43448',
    'currentContextSuppressedTriangles=96',
    'contextFacadeR25=true',
    'outerGroundLoadingFallbackTriangles=1280',
    'outerGroundLoadingFallbackSourceCorners=3840',
    'outerGroundLoadingFallbackRenderVertices=768',
    'outerGroundLoadingFallbackRenderOnly=true',
    'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false',
    'inheritedV5CPlanningGroundHidden=true',
    'treeRealism={ISTANA_EXPLORE_V5D_TREE_REALISM_VALID',
    'runtimeMinimumLod=0',
    'forcedLodModel=0',
    'sourceLOD0AvailableNearCamera=true',
    'automaticScreenSizeLod=true',
    'runtimeLodDistanceScale=1.8',
    'mediumRangeCrownDetailRetained=true',
    'allTreesForcedToLod0=false',
    'groundVegetation={ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID',
    'runtimeCompositeV5BAndV5DGrassOwnership=true',
    'compositeSourceGrassCullMeters=40..65',
    'compositeSourceGrassLodDistanceScale=1.10',
    'inheritedV5BSourceRendererPresentation=inactive_cold_visible_V5B_original',
    'sourceTerrainMeshMaterialTransformCollisionUntouched=true',
    'inheritedV5BCollisionOverlapNavigationRfSimulationUntouched=true'
)
$treeRuntimeMarkers = @(
    'sourceTrees=729',
    'runtimeMinimumLod=0',
    'forcedLodModel=0',
    'sourceLOD0AvailableNearCamera=true',
    'automaticScreenSizeLod=true',
    'runtimeLodDistanceScale=1.8',
    'mediumRangeCrownDetailRetained=true',
    'allTreesForcedToLod0=false',
    'sourceBlockersCollisionNavigationRfUntouched=true',
    'renderOnlyTransitions=true',
    'survey=false'
)
$groundRuntimeMarkers = @(
    'grassMicroDetail=18432',
    'inheritedSourceGrass=18432',
    'runtimeCompositeV5BAndV5DGrassOwnership=true',
    'compositeSourceGrassCullMeters=40..65',
    'compositeSourceGrassLodDistanceScale=1.10',
    'inheritedV5BSerializedCullLodWpoUntouched=true',
    'inheritedV5BRuntimeCullLodVisualOverrideOnly=true',
    'inheritedV5BCollisionOverlapNavigationRfSimulationUntouched=true',
    'inheritedV5BSourceRendererPresentation=visible_runtime_composite_40m_to65m',
    'renderOnly=true',
    'collision=false',
    'navigation=false',
    'sensorRfAuthority=false',
    'survey=false'
)
$r27GrassMaterialMarkers = @(
    'exactSavedPackages=4',
    'isolatedDerivativePackages=4',
    'exactDerivativeGraph=true',
    'compiledMaterials=4',
    'sourceMaterialsModified=false',
    'sourceStableVisibilityMeters=20,28',
    'targetStableVisibilityMeters=65,90',
    'visibilityGateCount=1',
    'componentFadeIntegrated=true',
    'evidenceRangeMeters=72.8',
    'materialVisibilityAtEvidenceRange=',
    'tallestCarrierTipProjectionPixelsAtEvidenceRange=',
    'projectionAssumption=perpendicularPinholeMaxSourceTip',
    'visualCaptureAccepted=false',
    'captureRevalidationRequired=true'
)
$temasekPhase2AssetMarkers = @(
    'assets=16',
    'meshTriangles=15760',
    'sourceVertices=9536',
    'proceduralComponents=828',
    'materialSlots=15',
    'foliageLayoutSchema=triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1',
    'foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor',
    'bakedFoliageRenderComponents=0',
    'foliageTreeAnchors=3',
    'renderOnly=true',
    'sensorRfAuthority=false'
)
$r27SuccessorMarkers = @(
    'landmarkVegetationR27=true',
    'mapIntegrated=true',
    'exactlyOneActor=true',
    'contextFacadeR25=true',
    'legacyTemasekBakedFoliageRemoved=true',
    'temasekMeshTriangles=15760',
    'temasekMaterialSlots=15',
    'bakedFoliageRenderComponents=0',
    'foliageTreeAnchors=3',
    'isolatedR27GrassMaterials=4',
    'exactDerivativeGraph=true',
    'compiledMaterials=4',
    'sourceStableVisibilityMeters=20,28',
    'targetStableVisibilityMeters=65,90',
    'visibilityGateCount=1',
    'componentFadeIntegrated=true',
    'grassCullCm=6500,9000',
    'evidenceRangeMeters=72.8',
    'materialVisibilityAtEvidenceRange=',
    'tallestCarrierTipProjectionPixelsAtEvidenceRange=',
    'projectionAssumption=perpendicularPinholeMaxSourceTip',
    'wpoDisableCm=2400',
    'grassInstances=3072',
    'maximumGrassPerSite=2048',
    'visualCaptureAccepted=false',
    'captureRevalidationRequired=true',
    'deterministic=true',
    'worldSpace=true',
    'nearestLandmarkPartition=true',
    'renderOnly=true',
    'collision=false',
    'navigation=false',
    'sensorAuthority=false',
    'rfAuthority=false',
    'sourceAssetsModified=false',
    'geometryExport=false',
    'providerSettingsUnchanged=true',
    'providerClipUnchanged=true'
)
$r28MapValidationMarkers = @(
    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID',
    'cleanSavedMap=true',
    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_VALID',
    'combinedR28=true',
    'environmentActors=1',
    'landmarkVegetationActors=1',
    'environmentProviderReadyMatchesPolicy=true',
    'environmentAssets=13',
    'landmarkAssets=14',
    'landmarkGrassInstances=6144',
    'landmarkGrassMaximumPerSite=4096',
    'temasekTreeRoster=umbrella,dome,umbrella',
    'temasekPhase2Retained=true',
    'legacyTemasekBakedFoliageRemoved=true',
    'contextFacadeR25Retained=true',
    'providerSettingsUnchanged=true',
    'providerClipUnchanged=true',
    'providerVisualOnly=true',
    'providerCollisionNavigationSensorRfAuthority=false',
    'providerTokenReadSerializedOrLogged=false',
    'providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false',
    'renderOnly=true',
    'visualCaptureAccepted=false',
    'captureRevalidationRequired=true'
)
$r28PieValidationMarkers = @(
    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_VALID',
    'exactPlayer0V5Camera=true',
    'r28MapWorldValidated=true',
    'r28ProviderNegativeAuthority=true',
    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_VALID',
    'ISTANA_EXPLORE_V5D_R28_PLAYER0_PRESENTATION_VALID',
    'r28EnvironmentPlayer0Visible=true',
    'r28EnvironmentSceneCaptureSensorExcluded=true',
    'r28EnvironmentRenderOnly=true',
    'r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false'
)
$r28StateValidationMarkers = @(
    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_QA_STATE_VALID',
    'r28VisualSuccessor=true',
    'r28MapWorldValidated=true',
    'r28ProviderNegativeAuthority=true',
    'r28EnvironmentPlayer0Visible=true',
    'r28EnvironmentSceneCaptureSensorExcluded=true',
    'exactQaViewPose=true',
    'localFallbackHidden=false'
)
$r28DiagnosticCaptureMarkers = @(
    'R28 VISUAL SUCCESSOR DIAGNOSTIC EVIDENCE:',
    'localFallbackVisible=true',
    'localFallbackHidden=false',
    'r28VisualSuccessor=true',
    'r28MapWorldValidated=true',
    'r28EnvironmentPlayer0Visible=true',
    'r28EnvironmentSceneCaptureSensorExcluded=true',
    'r28EnvironmentRenderOnly=true',
    'r28ProviderNegativeAuthority=true'
)
$manualVisualAcceptanceContract = [pscustomobject] [ordered] @{
    Status = 'PENDING_MANUAL_REVIEW'
    ReviewRequired = [bool] (
        $RequireLandmarkVegetationR27 -or $RequireR28VisualSuccessor)
    AutomatedHyperrealismClaimed = $false
    TechnicalCapturePassIsVisualAcceptance = $false
    PrimaryPose = if ($RequireR28VisualSuccessor) {
        '020m_close_normal_lawn_tree_r28'
    }
    elseif ($RequireLandmarkVegetationR27) {
        '012m_ground_grazing_r27'
    }
    else { $null }
    Criteria = if ($RequireR28VisualSuccessor) {
        @(
            'The 20 m normal close lawn/tree frame shows readable dense turf, grounded transitions, and structured canopy without requiring image zoom.',
            'The 75 m wide frame preserves a convincing Istana composition, landscape depth, and natural vegetation hierarchy.',
            'The target-locked 12 m MacDonald frame makes the retained Temasek/MacDonald streetscape context, vegetation grounding, and architectural relationship judgeable.',
            'The 65 m broader-surroundings frame shows plausible roads, nearby buildings, public-realm continuity, and tropical vegetation rather than an isolated hero asset.',
            'All four PNGs must include the fallback-visible R28 render-only actor in Player0 while its renderers remain excluded from sensor SceneCapture feeds.',
            'A reviewer must compare every PNG at native 2560x1440 resolution; technical VisualCaptureAccepted records complete validated raster evidence and does not itself claim photorealism or hyperrealism.'
        )
    }
    else {
        @(
            'At the dedicated 12 m low pose, individual blade or blade-cluster silhouettes remain readable without zooming into the image.',
            'The R27 lawn has no visible 20--28 m opacity hole, abrupt density wall, floating carpet, or conspicuous repeated tiling.',
            'The wider 20 m, 50 m, 65 m, and 75 m frames retain plausible lawn continuity and do not trade away the surrounding context.',
            'The Temasek legacy baked spherical foliage is absent; replacement trees, when in frame, must be judged for crown structure, scale, grounding, and repetition.',
            'A reviewer must compare the PNGs at native 2560x1440 resolution; semantic reports and a PASS capture manifest do not prove photorealism or hyperrealism.'
        )
    }
}

if ($ProviderEvidenceMode -cne 'ProviderReady' -and
    $ProviderReadyRateLimitAbortThreshold -ne 0) {
    throw '-ProviderReadyRateLimitAbortThreshold is valid only with -ProviderEvidenceMode ProviderReady.'
}
if ($RequireLandmarkVegetationR27 -and $RequireR28VisualSuccessor) {
    throw '-RequireLandmarkVegetationR27 and -RequireR28VisualSuccessor are mutually exclusive.'
}
if ($RequireR28VisualSuccessor -and
    $ProviderEvidenceMode -cne 'ProviderFallback') {
    throw 'R28 visual-successor Player0 evidence requires -ProviderEvidenceMode ProviderFallback so the render-only R28 environment is visible while remaining excluded from sensor SceneCapture feeds; provider readiness is recorded separately.'
}
if ($RequireR28VisualSuccessor) {
    $actualR28Roles = @($poses | ForEach-Object {
        [string] $_.VisualAcceptanceRole
    })
    if ($poses.Count -ne 4 -or
        (@($actualR28Roles | Sort-Object -Unique)).Count -ne 4 -or
        @($r28EssentialVisualRoles | Where-Object {
            $_ -cnotin $actualR28Roles
        }).Count -ne 0) {
        throw 'R28 evidence requires exactly four uniquely classified essential Player0 views.'
    }
}

$selfCheckBoundaryAssetPaths = @(
    $suppressedFallbackMeshAsset,
    $outerGroundMeshAsset,
    $outerGroundMaterialAsset
)
if ($RequireLandmarkVegetationR27) {
    $selfCheckBoundaryAssetPaths += $r27GrassMaterialAssets
}
$poseSequence = if ($RequireR28VisualSuccessor) {
    'bounded-four-view-r28-player0-visual-successor'
}
elseif ($RequireLandmarkVegetationR27) {
    'far-to-near-with-dedicated-12m-ground-grazing-r27-review'
}
else { 'far-to-near' }
$selfPin = Get-FileIdentity -Path $PSCommandPath

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Status = 'STATIC_SELF_CHECK_PASS'
        Schema =
            'triad.istana_explore_v5d.vegetation_provider.visual_capture_static_check.v1'
        Script = $selfPin
        ProviderEvidenceMode = $ProviderEvidenceMode
        EvidenceClassification = $evidenceModeClassification
        PresentationRevision = if ($RequireR28VisualSuccessor) {
            'R28_VISUAL_SUCCESSOR_STRICT_CAPTURE'
        }
        elseif ($RequireLandmarkVegetationR27) {
            'LANDMARK_VEGETATION_R27_STRICT_CAPTURE'
        }
        else { 'LEGACY_VEGETATION_RANGE_CAPTURE' }
        TelemetryDwellSeconds = $TelemetryDwellSeconds
        ProviderTimeoutSeconds = $ProviderTimeoutSeconds
        PoseOrder = $poseSequence
        PoseCount = $poses.Count
        Poses = $poses
        RuntimeContract = [pscustomobject] [ordered] @{
            MapValidationMarkers = $vegetationMapValidationMarkers
            TreeRuntimeLogPrefix =
                'LogTRIADIstanaExploreV5DTreeRealism: Display: ISTANA_EXPLORE_V5D_TREE_REALISM_VALID'
            TreeRuntimeMarkers = $treeRuntimeMarkers
            GroundRuntimeLogPrefix =
                'LogTRIADIstanaExploreV5DGroundVegetation: Display: ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID'
            GroundRuntimeMarkers = $groundRuntimeMarkers
            BoundaryAssetPaths = $selfCheckBoundaryAssetPaths
            LandmarkVegetationR27Required =
                [bool] $RequireLandmarkVegetationR27
            R28VisualSuccessorRequired =
                [bool] $RequireR28VisualSuccessor
            R28MapValidationFunction =
                'ValidateIstanaExploreV5DR28VisualSuccessorMap'
            R28MapValidationMarkers = $r28MapValidationMarkers
            R28PlayWorldValidationFunction =
                'ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld'
            R28PlayWorldValidationMarkers = $r28PieValidationMarkers
            R28PlayStateFunction =
                'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport'
            R28PlayStateMarkers = $r28StateValidationMarkers
            R28PoseFunction =
                'SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa'
            R28CaptureFunction =
                'CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView'
            R28DiagnosticCaptureMarkers = $r28DiagnosticCaptureMarkers
            R28EssentialVisualRoles = $r28EssentialVisualRoles
            LandmarkVegetationR27SuccessorMarkers =
                $r27SuccessorMarkers
            LandmarkVegetationR27GrassMaterialMarkers =
                $r27GrassMaterialMarkers
            TemasekPhase2AssetMarkers = $temasekPhase2AssetMarkers
            StrictFallbackCaptureAcknowledgementMarker =
                $strictFallbackCaptureAcknowledgementMarker
            ProviderReadyRateLimitAbortThreshold =
                $ProviderReadyRateLimitAbortThreshold
            ProviderRateLimitLogMarker = $providerRateLimitLogMarker
        }
        ExpectedNativeIdentities = [pscustomobject] [ordered] @{
            Map = [pscustomobject] [ordered] @{
                Bytes = $ExpectedMapBytes
                Sha256 = $ExpectedMapSha256
            }
            RuntimeDll = [pscustomobject] [ordered] @{
                Bytes = $ExpectedRuntimeDllBytes
                Sha256 = $ExpectedRuntimeDllSha256
            }
            EditorDll = [pscustomobject] [ordered] @{
                Bytes = $ExpectedEditorDllBytes
                Sha256 = $ExpectedEditorDllSha256
            }
        }
        R27CommitReceiptExpectation = [pscustomobject] [ordered] @{
            Required = [bool] $RequireLandmarkVegetationR27
            Path = $R27CommitReceiptPath
            Sha256 = $ExpectedR27CommitReceiptSha256
            Schema =
                'triad.istana_explore_v5d.landmark_vegetation_r27.native_transaction.v1'
            Status = 'PASS'
            ExactMaterialPackageCount = 4
            ExactFreshEditorStageCount = 7
        }
        R28CommitReceiptExpectation = [pscustomobject] [ordered] @{
            Required = [bool] $RequireR28VisualSuccessor
            Path = $R28CommitReceiptPath
            Sha256 = $ExpectedR28CommitReceiptSha256
            Schema =
                'triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1'
            Status = 'PASS'
            ExactNewAssetCount = 17
            VisualCaptureAcceptedBeforeThisCapture = $false
            CaptureRevalidationRequiredBeforeThisCapture = $true
            ExactR27Predecessor = [pscustomobject] [ordered] @{
                MapBytes = 36335930L
                MapSha256 =
                    '9C9660B02F3B9FBF8C679182E8EB39B8FECD0A618AECA6ED546AAD39E0C895E5'
                RuntimeDllBytes = 4676096L
                RuntimeDllSha256 =
                    'C341A9F2583248824911EBB5904EC357E0DCB840304C637D87FD0CE87E67B910'
                EditorDllBytes = 7781888L
                EditorDllSha256 =
                    '30569F04A956FDCC24F5AE8C0A99D1B70D86C03208E9497EE9BBD8480ABCC2B6'
            }
        }
        R28VisualAcceptance = [pscustomobject] [ordered] @{
            VisualCaptureAccepted = $false
            AcceptanceDeferredUntilLiveFilesExistAndValidate = $true
            ExactRequiredCaptureCount = 4
            ExactExpectedFilenames = @($poses | ForEach-Object {
                [string] $_.OutputFileName
            })
            EssentialRoles = $r28EssentialVisualRoles
            ProviderReadinessImplied = $false
        }
        ManualVisualAcceptance = $manualVisualAcceptanceContract
        MemorySafety = [pscustomobject] [ordered] @{
            PrivateMemoryCeilingBytes = $privateMemoryCeilingBytes
            PrivateMemoryCeilingGiB = 12
            MinimumSystemFreeVirtualBytes =
                $minimumSystemFreeVirtualBytes
            MinimumSystemFreeVirtualGiB = 6
            MinimumSystemFreeVirtualAtLaunchBytes =
                $minimumSystemFreeVirtualAtLaunchBytes
            MinimumSystemFreeVirtualAtLaunchGiB = 10
            PreLaunchAdmissionApplied = $true
            ContinuousMemoryWatchdogIntegrated =
                $continuousMemoryWatchdogIntegrated
            StrictProviderReadyLiveLaunchEnabled =
                $continuousMemoryWatchdogIntegrated
            PollMilliseconds = $memoryWatchdogPollMilliseconds
            PersistentBreachContainmentMilliseconds =
                $memoryWatchdogPersistentBreachMilliseconds
            CaptureTextureStreamingPoolMiB =
                $captureTextureStreamingPoolMiB
            CaptureTextureStreamingPoolOverrideToken =
                $captureTextureStreamingPoolOverrideToken
            CaptureTextureStreamingPoolApplication =
                'ENGINE_INI_CONSOLE_VARIABLES_PRE_MAP_LOAD_REQUIRED'
            RuntimeApplicationEvidenceAvailable = $false
            Coverage =
                'CONTINUOUS_FROM_IMMEDIATELY_AFTER_START_PROCESS_THROUGH_FINAL_TEARDOWN'
            IndependentClrThread = $true
            ExactPidCreationTimeAndExecutableRequiredForContainment = $true
            MemoryAlertSuppressesProofPublication = $true
            RequiredFutureHardening = $null
        }
        LiveEditorLaunched = $false
        NativeTreeReadOrWritten = $false
        ProtectedUE54Interaction =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        RemoteControlEndpointAllowlist = @($rcCallUri)
        ProviderGeometryExportedTracedAnalysedOrBaked = $false
    } | ConvertTo-Json -Depth 12
    return
}

if (-not $RequireLandmarkVegetationR27 -and
    ($PSBoundParameters.ContainsKey('R27CommitReceiptPath') -or
     $PSBoundParameters.ContainsKey('ExpectedR27CommitReceiptSha256'))) {
    throw 'R27 receipt parameters are valid only with -RequireLandmarkVegetationR27.'
}
if (-not $RequireR28VisualSuccessor -and
    ($PSBoundParameters.ContainsKey('R28CommitReceiptPath') -or
     $PSBoundParameters.ContainsKey('ExpectedR28CommitReceiptSha256'))) {
    throw 'R28 receipt parameters are valid only with -RequireR28VisualSuccessor.'
}
if ($RequireLandmarkVegetationR27 -or $RequireR28VisualSuccessor) {
    $strictRevision = if ($RequireR28VisualSuccessor) { 'R28' } else { 'R27' }
    foreach ($requiredIdentityParameter in @(
            'ExpectedMapBytes',
            'ExpectedMapSha256',
            'ExpectedRuntimeDllBytes',
            'ExpectedRuntimeDllSha256',
            'ExpectedEditorDllBytes',
            'ExpectedEditorDllSha256')) {
        if (-not $PSBoundParameters.ContainsKey($requiredIdentityParameter)) {
            throw "Strict $strictRevision live capture requires explicit caller-supplied -$requiredIdentityParameter from the selected commit receipt."
        }
    }
}
if ($RequireLandmarkVegetationR27) {
    if (-not $PSBoundParameters.ContainsKey('R27CommitReceiptPath') -or
        [string]::IsNullOrWhiteSpace($R27CommitReceiptPath) -or
        -not $PSBoundParameters.ContainsKey(
            'ExpectedR27CommitReceiptSha256') -or
        $ExpectedR27CommitReceiptSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
        throw 'Strict R27 live capture requires an explicit commit.json path and its exact SHA-256.'
    }
}
if ($RequireR28VisualSuccessor) {
    if (-not $PSBoundParameters.ContainsKey('R28CommitReceiptPath') -or
        [string]::IsNullOrWhiteSpace($R28CommitReceiptPath) -or
        -not $PSBoundParameters.ContainsKey(
            'ExpectedR28CommitReceiptSha256') -or
        $ExpectedR28CommitReceiptSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
        throw 'Strict R28 live capture requires an explicit commit.json path and its exact SHA-256.'
    }
}

if ($ExpectedMapBytes -le 0 -or
    $ExpectedMapSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live vegetation evidence requires -ExpectedMapBytes and -ExpectedMapSha256 from the selected native map receipt.'
}
if ($ExpectedRuntimeDllBytes -le 0 -or $ExpectedEditorDllBytes -le 0 -or
    $ExpectedRuntimeDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$' -or
    $ExpectedEditorDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live vegetation evidence requires positive byte counts and SHA-256 identities for both DLLs from the selected guarded native build.'
}
$ExpectedMapSha256 = $ExpectedMapSha256.ToUpperInvariant()
$ExpectedRuntimeDllSha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant()
$ExpectedEditorDllSha256 = $ExpectedEditorDllSha256.ToUpperInvariant()
if ($RequireLandmarkVegetationR27) {
    $ExpectedR27CommitReceiptSha256 =
        $ExpectedR27CommitReceiptSha256.ToUpperInvariant()
}
if ($RequireR28VisualSuccessor) {
    $ExpectedR28CommitReceiptSha256 =
        $ExpectedR28CommitReceiptSha256.ToUpperInvariant()
}
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
            $process = $_
            $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                    [string] $process.ExecutablePath)) {
                ''
            }
            else {
                [IO.Path]::GetFullPath([string] $process.ExecutablePath)
            }
            if ($process.Name -cne 'UnrealEditor.exe' -or
                $actualExecutable -ine $protectedUE54Editor) {
                throw 'The CAPSTONE project token is owned by an unexpected process; refusing the vegetation capture run.'
            }
            [pscustomobject] [ordered] @{
                ProcessId = [uint32] $process.ProcessId
                CreationUtcTicks =
                    [int64] ([DateTimeOffset] $process.CreationDate).UtcTicks
                Name = [string] $process.Name
                ExecutablePath = $actualExecutable
                ProjectPath = $protectedUE54Project
                CommandLine = [string] $process.CommandLine
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
        throw 'Protected UE5.4/CAPSTONE identity set changed during the vegetation capture run.'
    }
    $after
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

function Get-RcListeners {
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
        $Identity.CommandLine.Contains(
            $captureTextureStreamingPoolOverrideToken,
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('WebControl.StartServer',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains($logFile,
            [StringComparison]::OrdinalIgnoreCase) -and
        [Math]::Abs($Identity.CreationUtcTicks -
            $Handle.StartTime.ToUniversalTime().Ticks) -le
                [TimeSpan]::FromSeconds(2).Ticks
}

function Test-RcOwnership {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $listeners = @(Get-RcListeners)
    $listeners.Count -eq 1 -and [uint32] $listeners[0] -eq $ProcessId
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

function Assert-OwnedBoundary {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $Checkpoint,
        [switch] $SkipMemoryGuard
    )

    if (-not $SkipMemoryGuard) {
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint $Checkpoint
    }
    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
    $others = @(Get-NativeTRIADUnrealProcesses)
    if ($others.Count -ne 1 -or
        [uint32] $others[0].ProcessId -ne
            [uint32] $ExpectedProcess.ProcessId) {
        throw "The owned helper is not the sole non-protected Unreal editor at '$Checkpoint'."
    }
    if (-not (Test-RcOwnership -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId))) {
        throw "RC port 30010 is not solely owned by the helper at '$Checkpoint'."
    }
    if (-not $SkipMemoryGuard) {
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint $Checkpoint
    }
}

function Invoke-RcCall {
    param(
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcCallUri `
        -ContentType 'application/json' -Body $payload -TimeoutSec $TimeoutSec
}

function Invoke-OwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "before RC $FunctionName"
    $result = Invoke-RcCall -ObjectPath $ObjectPath `
        -FunctionName $FunctionName -Parameters $Parameters `
        -TimeoutSec $TimeoutSec
    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "after RC $FunctionName"
    $result
}

function Invoke-RequiredOwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [string] $TextProperty = '',
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $result = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $ObjectPath -FunctionName $FunctionName `
        -Parameters $Parameters -TimeoutSec $TimeoutSec
    if ($result.ReturnValue -ne $true) {
        $detail = if ([string]::IsNullOrWhiteSpace($TextProperty)) {
            ''
        }
        else {
            [string] $result.$TextProperty
        }
        throw "Required RC call failed: $FunctionName $detail"
    }
    if (-not [string]::IsNullOrWhiteSpace($TextProperty) -and
        [string]::IsNullOrWhiteSpace([string] $result.$TextProperty)) {
        throw "Required RC call returned blank $TextProperty`: $FunctionName"
    }
    $result
}

function Wait-OwnedPieState {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [bool] $Expected,
        [ValidateRange(5, 600)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = $null
    do {
        Start-Sleep -Milliseconds 500
        try {
            $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
                -ObjectPath $levelEditor -FunctionName 'IsInPlayInEditor' `
                -TimeoutSec 30
            if ([bool] $state.ReturnValue -eq $Expected) {
                return
            }
        }
        catch {
            $lastError = $_.Exception
            Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
                -Checkpoint 'PIE-state retry after RC error'
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    $suffix = if ($null -eq $lastError) {
        ''
    }
    else {
        " Last error: $($lastError.Message)"
    }
    throw "PIE did not reach IsInPlayInEditor=$Expected.$suffix"
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

function Test-ExactPoseState {
    param(
        [Parameter(Mandatory = $true)] $Response,
        [Parameter(Mandatory = $true)] $Pose,
        [Parameter(Mandatory = $true)]
        [ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]
        [string] $EvidenceMode
    )

    if ($Response.ReturnValue -ne $true) {
        return $false
    }
    $report = [string] $Response.OutReport
    $markers = @(
            "map=$mapPackage",
            'providerSiteClipActive=true',
            'authoredCoreVisualsVisible=true',
            'aerialProviderHandoffRequested=false',
            'aerialProviderHandoffActive=false',
            'stableVisualPolicy=true',
            'viewTargetMatchesPawn=true',
            'v5CameraProfile=true',
            'exactQaViewPose=true')
    if ($EvidenceMode -ceq 'ProviderReady') {
        $markers += @('providerReadyForProof=true', 'localFallbackHidden=true')
    }
    elseif ($EvidenceMode -ceq 'ProviderFallback') {
        $markers += @(
            'providerReadyForProof=false',
            'localFallbackHidden=false'
        )
    }
    if ($RequireR28VisualSuccessor) {
        $markers += $r28StateValidationMarkers
    }
    foreach ($marker in $markers) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            return $false
        }
    }
    $progressMatch = [regex]::Match($report,
        'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
    if (-not $progressMatch.Success) {
        return $false
    }
    $progress = Convert-InvariantDouble $progressMatch.Groups['Progress'].Value
    if ([double]::IsNaN($progress) -or [double]::IsInfinity($progress) -or
        $progress -lt 0.0 -or $progress -gt 100.0) {
        return $false
    }
    $number = '[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?'
    $pattern = 'viewLocationCm=X=(?<X>' + $number +
        ')\s+Y=(?<Y>' + $number + ')\s+Z=(?<Z>' + $number +
        ')\s+viewRotationDeg=P=(?<Pitch>' + $number +
        ')\s+Y=(?<Yaw>' + $number + ')\s+R=(?<Roll>' + $number +
        ')\s+exactQaViewPose=true'
    $match = [regex]::Match($report, $pattern,
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if (-not $match.Success) {
        return $false
    }
    $x = Convert-InvariantDouble $match.Groups['X'].Value
    $y = Convert-InvariantDouble $match.Groups['Y'].Value
    $z = Convert-InvariantDouble $match.Groups['Z'].Value
    $pitch = Convert-InvariantDouble $match.Groups['Pitch'].Value
    $yaw = Convert-InvariantDouble $match.Groups['Yaw'].Value
    $roll = Convert-InvariantDouble $match.Groups['Roll'].Value
    [Math]::Abs($x - [double] $Pose.X) -le 0.002 -and
        [Math]::Abs($y - [double] $Pose.Y) -le 0.002 -and
        [Math]::Abs($z - [double] $Pose.Z) -le 0.002 -and
        (Get-AngularDistanceDegrees $pitch ([double] $Pose.Pitch)) -le
            0.002 -and
        (Get-AngularDistanceDegrees $yaw ([double] $Pose.Yaw)) -le
            0.002 -and
        (Get-AngularDistanceDegrees $roll ([double] $Pose.Roll)) -le
            0.002
}

function Get-ProviderTelemetrySample {
    param([Parameter(Mandatory = $true)] $Response)

    $report = [string] $Response.OutReport
    $progressMatch = [regex]::Match($report,
        'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
    $readyMatch = [regex]::Match($report,
        'providerReadyForProof=(?<Value>true|false)')
    $fallbackMatch = [regex]::Match($report,
        'localFallbackHidden=(?<Value>true|false)')
    if (-not $progressMatch.Success -or -not $readyMatch.Success -or
        -not $fallbackMatch.Success) {
        throw "Provider telemetry fields are absent from V5D state: $report"
    }
    [pscustomobject] [ordered] @{
        ObservedUtc = [DateTime]::UtcNow.ToString('o')
        CesiumLoadProgress = Convert-InvariantDouble `
            $progressMatch.Groups['Progress'].Value
        ProviderReadyForProof =
            $readyMatch.Groups['Value'].Value -ceq 'true'
        LocalFallbackHidden =
            $fallbackMatch.Groups['Value'].Value -ceq 'true'
        Report = $report
    }
}

function Get-ProviderRateLimitEventCount {
    $logText = Get-SharedLogText -Path $logFile
    if ([string]::IsNullOrEmpty($logText)) {
        return 0
    }
    [regex]::Matches(
        $logText,
        [regex]::Escape($providerRateLimitLogMarker),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant).Count
}

function Assert-NoProviderReadyRateLimitBurst {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    if ($ProviderEvidenceMode -cne 'ProviderReady' -or
        $ProviderReadyRateLimitAbortThreshold -eq 0) {
        return
    }
    $observed = Get-ProviderRateLimitEventCount
    if ($observed -ge $ProviderReadyRateLimitAbortThreshold) {
        throw "PROVIDER_RATE_LIMIT_RETRYABLE checkpoint=$Checkpoint observed429=$observed threshold=$ProviderReadyRateLimitAbortThreshold"
    }
}

function Wait-StableExactPoseState {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] $Pose,
        [ValidateRange(30, 3600)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $requireReady = $ProviderEvidenceMode -ceq 'ProviderReady'
    $requireFallback = $ProviderEvidenceMode -ceq 'ProviderFallback'
    $stable = 0
    $reports = [Collections.Generic.List[string]]::new()
    $samples = [Collections.Generic.List[object]]::new()
    $firstAcceptedUtc = $null
    $lastReport = ''
    $lastProgress = ''
    $complete = $false
    do {
        Start-Sleep -Seconds 2
        $stateFunction = if ($RequireR28VisualSuccessor) {
            'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport'
        }
        else { 'GetIstanaExploreV5DHybridPlayStateReport' }
        $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
            -ObjectPath $v5dLibrary `
            -FunctionName $stateFunction `
            -TimeoutSec 120
        $lastReport = [string] $state.OutReport
        Assert-NoProviderReadyRateLimitBurst `
            -Checkpoint "pose-$($Pose.Label)-stabilization"
        $accepted = Test-ExactPoseState -Response $state -Pose $Pose `
            -EvidenceMode $ProviderEvidenceMode
        if ($accepted) {
            $sample = Get-ProviderTelemetrySample -Response $state
            $sampleUtc = [DateTime]::Parse(
                [string] $sample.ObservedUtc,
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::RoundtripKind)
            if ($null -eq $firstAcceptedUtc) {
                $firstAcceptedUtc = $sampleUtc
            }
            ++$stable
            $reports.Add($lastReport)
            $samples.Add($sample)
            $progress = [string] $sample.CesiumLoadProgress
            if ($requireReady) {
                $complete = $stable -ge 3
            }
            else {
                $complete = $stable -ge 3 -and
                    ($sampleUtc - $firstAcceptedUtc).TotalSeconds -ge
                        $TelemetryDwellSeconds
            }
        }
        else {
            $stable = 0
            $reports.Clear()
            $samples.Clear()
            $firstAcceptedUtc = $null
            $progressMatch = [regex]::Match($lastReport,
                'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
            $progress = if ($progressMatch.Success) {
                $progressMatch.Groups['Progress'].Value
            }
            else { '?' }
        }
        if ($progress -cne $lastProgress -or $accepted) {
            Write-Host "POSE=$($Pose.Label) MODE=$ProviderEvidenceMode PROVIDER_PROGRESS=$progress EXACT_STATE=$accepted STABLE=$stable COMPLETE=$complete"
            $lastProgress = $progress
        }
    } while (-not $complete -and [DateTime]::UtcNow -lt $deadline)
    if (-not $complete) {
        $expectation = if ($requireReady) {
            'three consecutive exact provider-ready readbacks'
        }
        elseif ($requireFallback) {
            "$TelemetryDwellSeconds seconds of consecutive exact-pose providerReadyForProof=false and localFallbackHidden=false readbacks"
        }
        else {
            "$TelemetryDwellSeconds seconds of consecutive exact-pose finite provider telemetry"
        }
        throw "Pose $($Pose.Label) did not reach $expectation`: $lastReport"
    }
    [pscustomobject] [ordered] @{
        Mode = $ProviderEvidenceMode
        ProviderReadyRequired = $requireReady
        ProviderFallbackRequired = $requireFallback
        TelemetryDwellSeconds = if ($requireReady) { 0 } else {
            $TelemetryDwellSeconds
        }
        Reports = @($reports)
        ProviderTelemetrySamples = @($samples)
        FinalReport = $lastReport
    }
}

function Get-PngEvidence {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [DateTime] $NotBeforeUtc
    )

    $pin = Get-FileIdentity -Path $Path
    $item = Get-Item -LiteralPath $pin.Path
    if ($item.LastWriteTimeUtc -lt $NotBeforeUtc) {
        throw "Capture is stale: $Path"
    }
    Add-Type -AssemblyName System.Drawing
    $stream = [IO.File]::Open($pin.Path, [IO.FileMode]::Open,
        [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $image = [Drawing.Image]::FromStream($stream, $true, $true)
        try {
            if ($image.RawFormat.Guid -ne
                    [Drawing.Imaging.ImageFormat]::Png.Guid -or
                $image.Width -ne 2560 -or $image.Height -ne 1440) {
                throw "Capture is not the required decodable 2560x1440 PNG: $Path"
            }
            [pscustomobject] [ordered] @{
                Path = $pin.Path
                Present = $true
                Bytes = $pin.Bytes
                Sha256 = $pin.Sha256
                Width = $image.Width
                Height = $image.Height
                LastWriteTimeUtc = $item.LastWriteTimeUtc.ToString('o')
            }
        }
        finally {
            $image.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Wait-StablePng {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [DateTime] $NotBeforeUtc,
        [ValidateRange(30, 300)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastLength = -1L
    $lastWriteTicks = -1L
    $stable = 0
    $lastError = $null
    do {
        Start-Sleep -Milliseconds 500
        Assert-ContinuousMemoryWatchdogHealthy `
            -Checkpoint 'waiting for stable capture PNG'
        if ([IO.File]::Exists([IO.Path]::GetFullPath($Path))) {
            $item = Get-Item -LiteralPath $Path
            if ($item.Length -gt 0 -and $item.Length -eq $lastLength -and
                $item.LastWriteTimeUtc.Ticks -eq $lastWriteTicks) {
                ++$stable
            }
            else {
                $stable = 0
            }
            $lastLength = [int64] $item.Length
            $lastWriteTicks = [int64] $item.LastWriteTimeUtc.Ticks
            if ($stable -ge 3) {
                try {
                    return Get-PngEvidence -Path $Path `
                        -NotBeforeUtc $NotBeforeUtc
                }
                catch {
                    $lastError = $_.Exception
                    $stable = 0
                }
            }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    $suffix = if ($null -eq $lastError) { '' } else {
        " Last decode error: $($lastError.Message)"
    }
    throw "Capture did not become a stable decodable PNG: $Path.$suffix"
}

function Assert-PostCaptureWorld {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] $Pose
    )

    $stateFunction = if ($RequireR28VisualSuccessor) {
        'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport'
    }
    else { 'GetIstanaExploreV5DHybridPlayStateReport' }
    $validationFunction = if ($RequireR28VisualSuccessor) {
        'ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld'
    }
    else { 'ValidateIstanaExploreV5DHybridPlayWorld' }
    $validationPrefix = if ($RequireR28VisualSuccessor) {
        'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_VALID'
    }
    else { 'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID' }
    $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $v5dLibrary `
        -FunctionName $stateFunction `
        -TimeoutSec 120
    if (-not (Test-ExactPoseState -Response $state -Pose $Pose `
            -EvidenceMode $ProviderEvidenceMode)) {
        throw "Post-capture state lost the exact $ProviderEvidenceMode pose contract for $($Pose.Label): $($state.OutReport)"
    }
    $providerTelemetry = Get-ProviderTelemetrySample -Response $state
    $validation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $ExpectedProcess -ObjectPath $v5dLibrary `
        -FunctionName $validationFunction `
        -TextProperty 'OutReport' -TimeoutSec 180
    if (-not ([string] $validation.OutReport).StartsWith(
            $validationPrefix,
            [StringComparison]::Ordinal)) {
        throw "Unexpected post-capture V5D validation for $($Pose.Label): $($validation.OutReport)"
    }
    if ($RequireR28VisualSuccessor) {
        Assert-RequiredReportMarkers `
            -Report ([string] $validation.OutReport) `
            -Markers $r28PieValidationMarkers `
            -Description "R28 post-capture validation for $($Pose.Label)"
    }
    [pscustomobject] [ordered] @{
        StateReport = [string] $state.OutReport
        ProviderTelemetry = $providerTelemetry
        ValidationReport = [string] $validation.OutReport
    }
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
        [ValidateRange(120, 600)] [int] $TimeoutSeconds = 420
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC EditorRequestEndPlay'
    $requestResult = 'returned'
    try {
        Invoke-RcCall -ObjectPath $levelEditor `
            -FunctionName 'EditorRequestEndPlay' `
            -TimeoutSec ([Math]::Min($TimeoutSeconds, 300)) | Out-Null
    }
    catch {
        $requestResult = $_.Exception.Message
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = ''
    do {
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
        if (Test-RcOwnership -ProcessId `
                ([uint32] $ExpectedProcess.ProcessId)) {
            try {
                $state = Invoke-RcCall -ObjectPath $levelEditor `
                    -FunctionName 'IsInPlayInEditor' -TimeoutSec 30
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
                $lastError = $_.Exception.Message
            }
        }
        Start-Sleep -Seconds 2
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "PIE teardown did not complete safely: request=$requestResult lastError=$lastError"
}

function Invoke-GracefulQuit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC QuitEditor' -SkipMemoryGuard
    $connectionResult = 'returned'
    try {
        Invoke-RcCall -ObjectPath $quitLibrary -FunctionName 'QuitEditor' `
            -TimeoutSec 30 | Out-Null
    }
    catch {
        $connectionResult = $_.Exception.Message
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

function Get-SharedLogText {
    param([Parameter(Mandatory = $true)] [string] $Path)

    if (-not [IO.File]::Exists($Path)) {
        return ''
    }
    $stream = [IO.File]::Open(
        $Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try {
        $reader = [IO.StreamReader]::new(
            $stream, [Text.UTF8Encoding]::new($false), $true, 4096, $true)
        try {
            return $reader.ReadToEnd()
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Get-TextureStreamingPoolApplicationEvidence {
    param([Parameter(Mandatory = $true)] [string] $LogText)

    if ([string]::IsNullOrWhiteSpace($LogText)) {
        return $null
    }

    $normalisedMapFile = $mapFile.Replace('\', '/')
    $mapLoadMarker = "Cmd: MAP LOAD FILE=`"$normalisedMapFile`""
    $mapLoadOffset = $LogText.IndexOf(
        $mapLoadMarker, [StringComparison]::OrdinalIgnoreCase)
    if ($mapLoadOffset -lt 0) {
        return $null
    }

    $engineConsoleVariablesMarker =
        'LogConfig: Applying CVar settings from Section [ConsoleVariables] File [Engine]'
    $engineConsoleVariablesOffset = $LogText.IndexOf(
        $engineConsoleVariablesMarker, [StringComparison]::Ordinal)
    if ($engineConsoleVariablesOffset -lt 0 -or
        $engineConsoleVariablesOffset -ge $mapLoadOffset) {
        throw 'The owned helper log did not prove Engine [ConsoleVariables] processing before exact map load.'
    }

    $poolPattern =
        'LogConfig: Set CVar \[\[r\.Streaming\.PoolSize:(?<Value>[0-9]+)\]\]'
    $poolAssignments = @([regex]::Matches(
        $LogText, $poolPattern,
        [Text.RegularExpressions.RegexOptions]::CultureInvariant))
    $preMapAssignments = @($poolAssignments | Where-Object {
        $_.Index -lt $mapLoadOffset
    })
    if ($preMapAssignments.Count -eq 0) {
        throw 'The owned helper log contains no texture-streaming pool assignment before exact map load.'
    }

    $lastPreMapAssignment = $preMapAssignments[-1]
    $lastPreMapValue = [string] $lastPreMapAssignment.Groups['Value'].Value
    if ($lastPreMapValue -cne [string] $captureTextureStreamingPoolMiB -or
        $lastPreMapAssignment.Index -le $engineConsoleVariablesOffset) {
        throw "The last pre-map texture-streaming pool assignment was not the required Engine [ConsoleVariables] value: expected=$captureTextureStreamingPoolMiB actual=$lastPreMapValue"
    }

    $postMapAssignments = @($poolAssignments | Where-Object {
        $_.Index -gt $mapLoadOffset
    })
    if ($postMapAssignments.Count -ne 0) {
        throw 'The owned helper log contains a post-map texture-streaming pool reassignment.'
    }

    [pscustomobject] [ordered] @{
        Applied = $true
        ExpectedPoolMiB = $captureTextureStreamingPoolMiB
        OverrideToken = $captureTextureStreamingPoolOverrideToken
        Source = 'ENGINE_INI_CONSOLE_VARIABLES'
        EngineConsoleVariablesMarker = $engineConsoleVariablesMarker
        EngineConsoleVariablesMarkerOffset = $engineConsoleVariablesOffset
        AppliedMarker = $lastPreMapAssignment.Value
        AppliedMarkerOffset = $lastPreMapAssignment.Index
        MapLoadMarker = $mapLoadMarker
        MapLoadMarkerOffset = $mapLoadOffset
        PreMapAssignmentValues = @($preMapAssignments | ForEach-Object {
            [string] $_.Groups['Value'].Value
        })
        LastPreMapAssignmentValue = $lastPreMapValue
        VerifiedBeforeExactMapLoad = $true
        PostMapAssignmentCount = 0
        NoPostMapReassignment = $true
    }
}

function Wait-EarlyTextureStreamingPoolApplication {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Assert-ContinuousMemoryWatchdogHealthy `
            -Checkpoint 'early texture-pool application proof'
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
        $evidence = Get-TextureStreamingPoolApplicationEvidence `
            -LogText (Get-SharedLogText -Path $logFile)
        if ($null -ne $evidence) {
            return $evidence
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timed out waiting for pre-map texture-streaming pool application proof after ${TimeoutSeconds}s."
}

function Wait-OwnedRuntimeLogReport {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ReportPrefix,
        [Parameter(Mandatory = $true)] [string[]] $RequiredMarkers,
        [Parameter(Mandatory = $true)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastCandidate = ''
    $lastMissing = @($RequiredMarkers)
    do {
        Assert-ContinuousMemoryWatchdogHealthy `
            -Checkpoint "waiting for runtime report $ReportPrefix"
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
        try {
            $logText = Get-SharedLogText -Path $logFile
            $lines = @($logText -split "\r?\n")
            for ($index = $lines.Count - 1; $index -ge 0; --$index) {
                $line = [string] $lines[$index]
                $prefixOffset = $line.IndexOf(
                    $ReportPrefix, [StringComparison]::Ordinal)
                if ($prefixOffset -lt 0) {
                    continue
                }
                $lastCandidate = $line.Substring($prefixOffset)
                $lastMissing = @($RequiredMarkers | Where-Object {
                        -not $lastCandidate.Contains(
                            $_, [StringComparison]::Ordinal)
                    })
                if ($lastMissing.Count -eq 0) {
                    return $lastCandidate
                }
                break
            }
        }
        catch [IO.IOException] {
            # Unreal may be flushing this exclusively for a brief instant.
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Owned runtime report '$ReportPrefix' did not reach its exact marker contract; missing=$([string]::Join(',', $lastMissing)) last=$lastCandidate"
}

function Assert-NoVegetationRuntimeFailure {
    param([Parameter(Mandatory = $true)] $ExpectedProcess)

    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
    Assert-NoProviderReadyRateLimitBurst `
        -Checkpoint 'vegetation-runtime-postcheck'
    $logText = Get-SharedLogText -Path $logFile
    foreach ($forbidden in @(
            'LogTRIADIstanaExploreV5DTreeRealism: Error:',
            'LogTRIADIstanaExploreV5DGroundVegetation: Error:',
            'V5D tree runtime failed closed',
            'V5D ground/vegetation runtime failed closed',
            'V5D terrain/provider/turf/edge presentation failed closed')) {
        if ($logText.Contains($forbidden, [StringComparison]::Ordinal)) {
            throw "Owned helper log contains a vegetation runtime failure marker: $forbidden"
        }
    }
}

if (-not (Test-Path -LiteralPath $outputRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $logRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $ddcRoot -PathType Container)) {
    throw 'The exact output, log, or DDC directory is absent.'
}
foreach ($path in @($logFile, $evidenceManifestPath) + @(
        $poses | ForEach-Object {
            Join-Path $outputRoot ([string] $_.OutputFileName)
        })) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite an existing vegetation evidence artifact: $path"
    }
}

$script:protectedUE54Before = Get-ProtectedUE54Identity
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    throw 'Vegetation capture requires UE5.5/native TRIAD helpers to be idle; the protected UE5.4/CAPSTONE set may remain open.'
}
if (@(Get-RcListeners).Count -ne 0) {
    throw 'Vegetation capture requires no existing RC port 30010 listener.'
}

$editorPin = Get-FileIdentity -Path $editor
$projectPin = Get-FileIdentity -Path $projectFile
$mapPin = Get-FileIdentity -Path $mapFile
$runtimeDllPin = Get-FileIdentity -Path $runtimeDll
$editorDllPin = Get-FileIdentity -Path $editorDll
$suppressedFallbackMeshPin =
    Get-FileIdentity -Path $suppressedFallbackMeshAsset
$outerGroundMeshPin = Get-FileIdentity -Path $outerGroundMeshAsset
$outerGroundMaterialPin = Get-FileIdentity -Path $outerGroundMaterialAsset
$r27GrassMaterialPins = @()
$r27CommitReceiptEvidence = $null
$r28CommitReceiptEvidence = $null
Assert-ExactPin -Pin $projectPin -Bytes $expectedProjectBytes `
    -Sha256 $expectedProjectSha256 -Description 'TRIAD project file'
Assert-ExactPin -Pin $mapPin -Bytes $ExpectedMapBytes `
    -Sha256 $ExpectedMapSha256 -Description 'selected V5D map'
Assert-ExactPin -Pin $runtimeDllPin -Bytes $ExpectedRuntimeDllBytes `
    -Sha256 $ExpectedRuntimeDllSha256 -Description 'selected runtime DLL'
Assert-ExactPin -Pin $editorDllPin -Bytes $ExpectedEditorDllBytes `
    -Sha256 $ExpectedEditorDllSha256 -Description 'selected editor DLL'
if ($RequireLandmarkVegetationR27) {
    $r27GrassMaterialPins = @(Get-ExactR27GrassMaterialPins)
    $r27CommitReceiptEvidence = Get-ValidatedR27CommitReceipt `
        -MapPin $mapPin -RuntimePin $runtimeDllPin `
        -EditorPin $editorDllPin -MaterialPins $r27GrassMaterialPins
}
if ($RequireR28VisualSuccessor) {
    $r28CommitReceiptEvidence = Get-ValidatedR28CommitReceipt `
        -MapPin $mapPin -RuntimePin $runtimeDllPin `
        -EditorPin $editorDllPin
}
$script:boundaryPins = @(
    $selfPin,
    $editorPin,
    $projectPin,
    $mapPin,
    $runtimeDllPin,
    $editorDllPin,
    $suppressedFallbackMeshPin,
    $outerGroundMeshPin,
    $outerGroundMaterialPin
)
if ($RequireLandmarkVegetationR27) {
    $script:boundaryPins += $r27GrassMaterialPins
    $script:boundaryPins += $r27CommitReceiptEvidence.Pin
    $script:r27MaterialBoundaryArmed = $true
}
if ($RequireR28VisualSuccessor) {
    $script:boundaryPins += $r28CommitReceiptEvidence.Pin
}
Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'
Initialize-ContinuousMemoryWatchdogType
$startupMemorySnapshot = Assert-SystemStartupHeadroom `
    -Checkpoint 'immediately before helper launch'

# The capture world has no skeletal, Control Rig, Android, modeling, source-code,
# telemetry, performance-advisor, or editor-data-storage assets. Keep their full
# enabled-by-default reverse-dependency closure out of this evidence-only editor;
# Cesium, Water/Niagara/Python, PCG, RemoteControl, TRIAD, and AirSimTriadRuntime
# remain available because they are part of the scene/runtime dependency closure.
# Apply the texture-pool cap through Engine.ini's ConsoleVariables section. UE
# consumes that section during LoadConsoleVariablesFromINI, before the map and its
# textures are loaded; ExecCmds is intentionally too late for this memory gate.
$argumentLine = "`"$projectFile`" $mapPackage -DisablePlugin=AirSim -DisablePlugins=ModelingToolsEditorMode,GeometryScripting,MeshModelingToolset,MeshModelingToolsetExp,MeshLODToolset,ToolPresets,StylusInput,PlanarCut,NNEDenoiser,SkeletalMeshModelingTools,EditorDataStorage,StudioTelemetry,EditorTelemetry,EditorPerformance,AnimationData,ControlRig,ControlRigModules,ControlRigSpline,DeformerGraph,FullBodyIK,IKRig,MetaHumanSDK,RigLogic,RigVM,SequencerAnimTools,Bridge,HairStrands,AndroidPermission,OnlineSubsystemGooglePlay,CLionSourceCodeAccess,CodeLiteSourceCodeAccess,GitSourceControl,KDevelopSourceCodeAccess,N10XSourceCodeAccess,NullSourceCodeAccess,PerforceSourceControl,PlasticSourceControl,RiderSourceCodeAccess,SubversionSourceControl,VisualStudioSourceCodeAccess,VisualStudioCodeSourceCodeAccess,PluginBrowser,PluginUtils,ChangelistReview,MobileLauncherProfileWizard -unattended -nop4 -NoSplash -NoAutoSave -RenderOffscreen -NoSound -asyncstaticmeshcompilation=0 -dx12 -sm6 -ResX=2560 -ResY=1440 -Windowed -ini:Engine:[HTTP]:HttpMaxConnectionsPerServer=12 -ini:Engine:[/Script/CesiumRuntime.CesiumRuntimeSettings]:MaxCacheItems=32768 $captureTextureStreamingPoolOverrideToken -DDC=InstalledNoZenLocalFallback -LocalDataCachePath=`"$ddcRoot`" -RemoteControlHttpServer -RCWebControlEnable -ExecCmds=`"WebControl.StartServer`" -abslog=`"$logFile`""
$oldLocalDdc = ${env:UE-LocalDataCachePath}
$process = $null
$launchedIdentity = $null
$pieMayBeActive = $false
$quitRequested = $false
$forcedContainment = $false
$exitCode = $null
$quitConnectionResult = ''
$endPlayEvidence = $null
$workflowError = $null
$continuousWatchdogSnapshot = $null
$watchdogMemoryAbort = $false
$memoryAbortCleanupObservations = @()
$cleanupErrors = [Collections.Generic.List[string]]::new()
$postconditionErrors = [Collections.Generic.List[string]]::new()
$captureEvidence = [Collections.Generic.List[object]]::new()
$projectIdentityReport = ''
$mapValidationReport = ''
$initialPieValidationReport = ''
$treeRuntimeReport = ''
$groundRuntimeReport = ''
$textureStreamingPoolApplicationEvidence = $null
$r27GrassMaterialValidationReport = ''
$temasekPhase2AssetValidationReport = ''
$r27SuccessorValidationReport = ''
$r28SuccessorValidationReport = ''
$playWorldValidationFunction = if ($RequireR28VisualSuccessor) {
    'ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld'
}
else { 'ValidateIstanaExploreV5DHybridPlayWorld' }
$playWorldValidationPrefix = if ($RequireR28VisualSuccessor) {
    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_VALID'
}
else { 'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID' }
$playStateFunction = if ($RequireR28VisualSuccessor) {
    'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport'
}
else { 'GetIstanaExploreV5DHybridPlayStateReport' }
$setPoseFunction = if ($RequireR28VisualSuccessor) {
    'SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa'
}
else { 'SetIstanaExploreV5DHybridPlayViewPoseForQa' }
$setPosePrefix = if ($RequireR28VisualSuccessor) {
    'EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_PASS:'
}
else { 'EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_PASS:' }

try {
    ${env:UE-LocalDataCachePath} = $ddcRoot
    $process = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden
    $script:memoryWatchdog = Start-ContinuousMemoryWatchdog -Handle $process
    $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        Assert-ContinuousMemoryWatchdogHealthy `
            -Checkpoint 'startup identity acquisition'
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

    $textureStreamingPoolApplicationEvidence =
        Wait-EarlyTextureStreamingPoolApplication `
            -ExpectedProcess $launchedIdentity -TimeoutSeconds 60

    $editorDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
    $projectIdentity = $null
    do {
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint 'editor readiness'
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $launchedIdentity | Out-Null
        if (Test-RcOwnership -ProcessId `
                ([uint32] $launchedIdentity.ProcessId)) {
            try {
                $projectIdentity = Invoke-OwnedRcCall `
                    -ExpectedProcess $launchedIdentity `
                    -ObjectPath $identityLibrary `
                    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
                    -Parameters @{ ExpectedProjectPath = $projectRoot } `
                    -TimeoutSec 30
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
    $projectIdentityReport = [string] $projectIdentity.OutReport

    $mapValidationFunction = if ($RequireR28VisualSuccessor) {
        'ValidateIstanaExploreV5DR28VisualSuccessorMap'
    }
    else { 'ValidateIstanaExploreV5DHybridMap' }
    $mapValidationPrefix = if ($RequireR28VisualSuccessor) {
        'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID'
    }
    else { 'ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID' }
    $mapValidation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
        -FunctionName $mapValidationFunction `
        -TextProperty 'OutReport' -TimeoutSec 900
    $mapValidationReport = [string] $mapValidation.OutReport
    if (-not $mapValidationReport.StartsWith(
            $mapValidationPrefix,
            [StringComparison]::Ordinal)) {
        throw "Unexpected V5D map validation: $mapValidationReport"
    }
    foreach ($marker in $vegetationMapValidationMarkers) {
        if (-not $mapValidationReport.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "V5D map validation lacks the vegetation/Cesium marker: $marker"
        }
    }
    if ($RequireR28VisualSuccessor) {
        Assert-RequiredReportMarkers -Report $mapValidationReport `
            -Markers $r28MapValidationMarkers `
            -Description 'R28 visual-successor pre-PIE map/world validation'
        $r28SuccessorValidationReport = $mapValidationReport
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint 'after strict R28 pre-PIE map/world validation'
    }

    if ($RequireLandmarkVegetationR27) {
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint 'before strict R27 semantic validation'

        # These are deliberately read-only validators. The capture path never
        # invokes BuildOrValidateLandmarkGrassMaterialsR27 or either R27 apply
        # endpoint; the selected commit receipt must already describe the
        # exact cold-validated successor being displayed.
        $r27GrassMaterialValidation = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity `
            -ObjectPath $landmarkVegetationLibrary `
            -FunctionName 'ValidateLandmarkGrassMaterialsR27' `
            -TextProperty 'OutReport' -TimeoutSec 900
        $r27GrassMaterialValidationReport =
            [string] $r27GrassMaterialValidation.OutReport
        if (-not $r27GrassMaterialValidationReport.StartsWith(
                'ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_VALID',
                [StringComparison]::Ordinal)) {
            throw "Unexpected R27 grass-material validation: $r27GrassMaterialValidationReport"
        }
        Assert-RequiredReportMarkers `
            -Report $r27GrassMaterialValidationReport `
            -Markers $r27GrassMaterialMarkers `
            -Description 'R27 grass-material validation'
        Assert-ReportMetricMinimum `
            -Report $r27GrassMaterialValidationReport `
            -Name 'materialVisibilityAtEvidenceRange' -Minimum 0.75 `
            -Description 'R27 grass-material validation'
        Assert-ReportMetricMinimum `
            -Report $r27GrassMaterialValidationReport `
            -Name 'tallestCarrierTipProjectionPixelsAtEvidenceRange' `
            -Minimum 1.5 -Description 'R27 grass-material validation'

        $temasekPhase2AssetValidation = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity `
            -ObjectPath $temasekShophouseLibrary `
            -FunctionName 'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
            -TextProperty 'OutReport' -TimeoutSec 900
        $temasekPhase2AssetValidationReport =
            [string] $temasekPhase2AssetValidation.OutReport
        if (-not $temasekPhase2AssetValidationReport.StartsWith(
                'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID',
                [StringComparison]::Ordinal)) {
            throw "Unexpected Temasek Phase-2 asset validation: $temasekPhase2AssetValidationReport"
        }
        Assert-RequiredReportMarkers `
            -Report $temasekPhase2AssetValidationReport `
            -Markers $temasekPhase2AssetMarkers `
            -Description 'Temasek Phase-2 asset validation'

        $r27SuccessorValidation = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName `
                'ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap' `
            -TextProperty 'OutReport' -TimeoutSec 900
        $r27SuccessorValidationReport =
            [string] $r27SuccessorValidation.OutReport
        if (-not $r27SuccessorValidationReport.StartsWith(
                'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_SUCCESSOR_VALID',
                [StringComparison]::Ordinal)) {
            throw "Unexpected R27 successor validation: $r27SuccessorValidationReport"
        }
        Assert-RequiredReportMarkers -Report $r27SuccessorValidationReport `
            -Markers $r27SuccessorMarkers `
            -Description 'R27 successor validation'
        Assert-ReportMetricMinimum -Report $r27SuccessorValidationReport `
            -Name 'materialVisibilityAtEvidenceRange' -Minimum 0.75 `
            -Description 'R27 successor validation'
        Assert-ReportMetricMinimum -Report $r27SuccessorValidationReport `
            -Name 'tallestCarrierTipProjectionPixelsAtEvidenceRange' `
            -Minimum 1.5 -Description 'R27 successor validation'
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint 'after strict R27 semantic validation'
    }

    $initialPie = Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity `
        -ObjectPath $levelEditor -FunctionName 'IsInPlayInEditor' `
        -TimeoutSec 30
    if ([bool] $initialPie.ReturnValue) {
        throw 'A fresh non-PIE editor world was not available.'
    }
    $pieMayBeActive = $true
    Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity `
        -ObjectPath $levelEditor -FunctionName 'EditorRequestBeginPlay' `
        -TimeoutSec 60 | Out-Null
    Wait-OwnedPieState -ExpectedProcess $launchedIdentity -Expected $true `
        -TimeoutSeconds $PieTimeoutSeconds

    $pieDeadline = [DateTime]::UtcNow.AddSeconds($PieTimeoutSeconds)
    $initialPieValidation = $null
    do {
        Start-Sleep -Seconds 1
        $initialPieValidation = Invoke-OwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName $playWorldValidationFunction `
            -TimeoutSec 180
        if ($initialPieValidation.ReturnValue -eq $true -and
            ([string] $initialPieValidation.OutReport).StartsWith(
                $playWorldValidationPrefix,
                [StringComparison]::Ordinal)) {
            break
        }
    } while ([DateTime]::UtcNow -lt $pieDeadline)
    if ($null -eq $initialPieValidation -or
        $initialPieValidation.ReturnValue -ne $true -or
        -not ([string] $initialPieValidation.OutReport).StartsWith(
            $playWorldValidationPrefix,
            [StringComparison]::Ordinal)) {
        throw "Initial V5D PIE validation failed: $($initialPieValidation.OutReport)"
    }
    $initialPieValidationReport = [string] $initialPieValidation.OutReport
    if ($RequireR28VisualSuccessor) {
        Assert-RequiredReportMarkers `
            -Report $initialPieValidationReport `
            -Markers $r28PieValidationMarkers `
            -Description 'initial R28 visual-successor PIE validation'
    }

    # The general PlayWorld validator proves world/camera/Cesium invariants but
    # intentionally returns only its concise PIE report. The two actor reports
    # below are therefore read from this helper's unique, create-new log and
    # must each contain the complete current runtime contract on one line.
    $treeRuntimeReport = Wait-OwnedRuntimeLogReport `
        -ExpectedProcess $launchedIdentity `
        -ReportPrefix 'LogTRIADIstanaExploreV5DTreeRealism: Display: ISTANA_EXPLORE_V5D_TREE_REALISM_VALID' `
        -RequiredMarkers $treeRuntimeMarkers `
        -TimeoutSeconds $PieTimeoutSeconds
    $groundRuntimeReport = Wait-OwnedRuntimeLogReport `
        -ExpectedProcess $launchedIdentity `
        -ReportPrefix 'LogTRIADIstanaExploreV5DGroundVegetation: Display: ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID' `
        -RequiredMarkers $groundRuntimeMarkers `
        -TimeoutSeconds $PieTimeoutSeconds
    Assert-NoVegetationRuntimeFailure -ExpectedProcess $launchedIdentity

    foreach ($pose in $poses) {
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint "before capture $($pose.Label)"
        $setPose = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName $setPoseFunction `
            -Parameters @{
                WorldViewLocationCentimeters = @{
                    X = [double] $pose.X
                    Y = [double] $pose.Y
                    Z = [double] $pose.Z
                }
                WorldViewRotationDegrees = @{
                    Pitch = [double] $pose.Pitch
                    Yaw = [double] $pose.Yaw
                    Roll = [double] $pose.Roll
                }
            } -TextProperty 'OutMessage' -TimeoutSec 60
        $setPoseMessage = [string] $setPose.OutMessage
        if (-not $setPoseMessage.StartsWith(
                $setPosePrefix,
                [StringComparison]::Ordinal) -or
            -not $setPoseMessage.Contains(
                'exactQaViewPose=true', [StringComparison]::Ordinal)) {
            throw "Unexpected pose acknowledgement for $($pose.Label): $setPoseMessage"
        }
        if ($RequireR28VisualSuccessor) {
            Assert-RequiredReportMarkers -Report $setPoseMessage `
                -Markers @(
                    'r28VisualSuccessor=true',
                    'r28MapWorldValidated=true',
                    'r28ProviderNegativeAuthority=true',
                    'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_VALID',
                    'ISTANA_EXPLORE_V5D_R28_PLAYER0_PRESENTATION_VALID') `
                -Description "R28 exact-pose acknowledgement for $($pose.Label)"
        }

        $stateWindow = Wait-StableExactPoseState `
            -ExpectedProcess $launchedIdentity -Pose $pose `
            -TimeoutSeconds $ProviderTimeoutSeconds
        $preCaptureState = Invoke-OwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName $playStateFunction `
            -TimeoutSec 120
        if (-not (Test-ExactPoseState -Response $preCaptureState `
                -Pose $pose -EvidenceMode $ProviderEvidenceMode)) {
            throw "The immediate pre-capture state lost the exact $ProviderEvidenceMode pose contract for $($pose.Label): $($preCaptureState.OutReport)"
        }
        $preCaptureProviderTelemetry =
            Get-ProviderTelemetrySample -Response $preCaptureState
        Assert-NoProviderReadyRateLimitBurst `
            -Checkpoint "pose-$($pose.Label)-immediate-pre-capture"
        $captureRequestedUtc = [DateTime]::UtcNow
        $captureFunction = if ($RequireR28VisualSuccessor) {
            'CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView'
        }
        elseif ($ProviderEvidenceMode -cne 'ProviderReady') {
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView'
        }
        else {
            'CaptureIstanaExploreV5DHybridPlayView'
        }
        $capture = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName $captureFunction `
            -Parameters @{ OutputFileName = [string] $pose.OutputFileName } `
            -TextProperty 'OutMessage' -TimeoutSec $CaptureTimeoutSeconds
        $captureMessage = [string] $capture.OutMessage
        $expectedCapturePrefix =
            if ($RequireR28VisualSuccessor) {
                'R28 VISUAL SUCCESSOR DIAGNOSTIC EVIDENCE:'
            }
            elseif ($ProviderEvidenceMode -cne 'ProviderReady') {
                'NON-PROOF DIAGNOSTIC EVIDENCE:'
            }
            else {
                'Accepted exact 2560x1440 HDR-off V5D Player0 proof capture'
            }
        if (-not $captureMessage.StartsWith($expectedCapturePrefix,
                [StringComparison]::Ordinal)) {
            throw "Unexpected capture acknowledgement for $($pose.Label): $captureMessage"
        }
        if ($ProviderEvidenceMode -ceq 'ProviderFallback' -and
            -not $captureMessage.Contains(
                $strictFallbackCaptureAcknowledgementMarker,
                [StringComparison]::Ordinal)) {
            throw "ProviderFallback capture acknowledgement lost its strict visible-fallback state for $($pose.Label): $captureMessage"
        }
        if ($RequireR28VisualSuccessor) {
            Assert-RequiredReportMarkers -Report $captureMessage `
                -Markers $r28DiagnosticCaptureMarkers `
                -Description "R28 Player0 capture acknowledgement for $($pose.Label)"
        }
        $png = Wait-StablePng `
            -Path (Join-Path $outputRoot ([string] $pose.OutputFileName)) `
            -NotBeforeUtc $captureRequestedUtc `
            -TimeoutSeconds $CaptureTimeoutSeconds
        $postCapture = Assert-PostCaptureWorld `
            -ExpectedProcess $launchedIdentity -Pose $pose
        Assert-NoVegetationRuntimeFailure -ExpectedProcess $launchedIdentity
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint "after capture $($pose.Label)"
        $captureEvidence.Add([pscustomobject] [ordered] @{
            Label = $pose.Label
            OutputFileName = $pose.OutputFileName
            Pose = $pose
            SetPoseMessage = $setPoseMessage
            ProviderEvidenceMode = $ProviderEvidenceMode
            ConsecutiveExactStateReports = @($stateWindow.Reports)
            ProviderTelemetrySamples =
                @($stateWindow.ProviderTelemetrySamples)
            ImmediatePreCaptureStateReport =
                [string] $preCaptureState.OutReport
            ImmediatePreCaptureProviderTelemetry =
                $preCaptureProviderTelemetry
            CaptureMessage = $captureMessage
            Png = $png
            PostCaptureStateReport = $postCapture.StateReport
            PostCaptureProviderTelemetry =
                $postCapture.ProviderTelemetry
            PostCaptureValidationReport = $postCapture.ValidationReport
        })
    }
}
catch {
    $workflowError = $_.Exception
}
finally {
    $preCleanupWatchdogSnapshot =
        Get-ContinuousMemoryWatchdogSnapshot
    if (Test-ContinuousMemoryWatchdogMemoryAlert `
            -Snapshot $preCleanupWatchdogSnapshot) {
        $watchdogMemoryAbort = $true
        $workflowError = [InvalidOperationException]::new(
            "$($preCleanupWatchdogSnapshot.AlertKind): source=continuous_watchdog privateBytes=$($preCleanupWatchdogSnapshot.AlertPrivateBytes) freeVirtualBytes=$($preCleanupWatchdogSnapshot.AlertAvailableCommitBytes) nonProofAbort=true")
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
                "Startup identity recovery failed: $($_.Exception.Message)")
        }
    }
    if ($null -ne $process -and $null -ne $launchedIdentity) {
        if ($watchdogMemoryAbort) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    # The independent watchdog retains its short containment
                    # deadline while the exact owned editor receives one
                    # immediate graceful-quit opportunity.
                    $quitRequested = $true
                    $quitConnectionResult = Invoke-GracefulQuit `
                        -ExpectedProcess $launchedIdentity -Handle $process
                }
            }
            catch {
                $cleanupErrors.Add(
                    "Emergency QuitEditor failed: $($_.Exception.Message)")
            }
        }
        try {
            $process.Refresh()
            if (-not $watchdogMemoryAbort -and
                -not $process.HasExited -and $pieMayBeActive) {
                $endPlayEvidence = Invoke-OwnedEndPlay `
                    -ExpectedProcess $launchedIdentity
                $pieMayBeActive = $false
            }
        }
        catch {
            $cleanupErrors.Add("EndPlay failed: $($_.Exception.Message)")
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
            $cleanupErrors.Add("QuitEditor failed: $($_.Exception.Message)")
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
            $cleanupErrors.Add("Exact helper containment failed: $($_.Exception.Message)")
        }
    }
    try {
        $continuousWatchdogSnapshot =
            Stop-ContinuousMemoryWatchdog
        if ($null -ne $continuousWatchdogSnapshot) {
            if (Test-ContinuousMemoryWatchdogMemoryAlert `
                    -Snapshot $continuousWatchdogSnapshot) {
                $watchdogMemoryAbort = $true
                $workflowError = [InvalidOperationException]::new(
                    "$($continuousWatchdogSnapshot.AlertKind): source=continuous_watchdog privateBytes=$($continuousWatchdogSnapshot.AlertPrivateBytes) freeVirtualBytes=$($continuousWatchdogSnapshot.AlertAvailableCommitBytes) nonProofAbort=true")
            }
            elseif (-not [string]::IsNullOrWhiteSpace(
                    [string] $continuousWatchdogSnapshot.MonitorError)) {
                $workflowError = [InvalidOperationException]::new(
                    "MEMORY_WATCHDOG_FAILURE: detail=$($continuousWatchdogSnapshot.MonitorError) nonProofAbort=true")
            }
        }
    }
    catch {
        $cleanupErrors.Add(
            "Continuous watchdog shutdown failed: $($_.Exception.Message)")
    }
    ${env:UE-LocalDataCachePath} = $oldLocalDdc
}

try {
    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
}
catch {
    $postconditionErrors.Add($_.Exception.Message)
}
try {
    Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'final postflight'
}
catch {
    $postconditionErrors.Add($_.Exception.Message)
}
try {
    $textureStreamingPoolApplicationEvidence =
        Get-TextureStreamingPoolApplicationEvidence `
            -LogText (Get-SharedLogText -Path $logFile)
    if ($null -eq $textureStreamingPoolApplicationEvidence) {
        throw 'Final owned-helper log lacks exact map-load texture-pool application evidence.'
    }
}
catch {
    $postconditionErrors.Add(
        "Texture-streaming pool application proof failed: $($_.Exception.Message)")
}
if ($null -eq $continuousWatchdogSnapshot) {
    $postconditionErrors.Add(
        'The continuous memory watchdog produced no final snapshot.')
}
else {
    if (-not [bool] $continuousWatchdogSnapshot.Started -or
        -not [bool] $continuousWatchdogSnapshot.Stopped -or
        [int64] $continuousWatchdogSnapshot.SampleCount -le 0) {
        $postconditionErrors.Add(
            'The continuous memory watchdog did not prove a started, sampled, stopped lifetime.')
    }
    if ($watchdogMemoryAbort -and
        -not [bool] $continuousWatchdogSnapshot.ExactIdentityVerifiedAtAlert) {
        $postconditionErrors.Add(
            'A memory alert lacked exact PID/start-time/executable verification.')
    }
    if ([bool] $continuousWatchdogSnapshot.ForceKillUsed -and
        -not [bool] $continuousWatchdogSnapshot.ExactIdentityVerifiedAtContainment) {
        $postconditionErrors.Add(
            'Watchdog containment lacked exact PID/start-time/executable verification.')
    }
    if (-not $watchdogMemoryAbort -and (
            -not [string]::IsNullOrWhiteSpace(
                [string] $continuousWatchdogSnapshot.AlertKind) -or
            -not [string]::IsNullOrWhiteSpace(
                [string] $continuousWatchdogSnapshot.MonitorError) -or
            [bool] $continuousWatchdogSnapshot.ForceKillUsed -or
            [bool] $continuousWatchdogSnapshot.ContainmentRefused)) {
        $postconditionErrors.Add(
            'The continuous memory watchdog did not finish in a proof-eligible clean state.')
    }
}
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    $postconditionErrors.Add(
        'A UE5.5/native TRIAD Unreal helper remains after final containment.')
}
if (@(Get-RcListeners).Count -ne 0) {
    $postconditionErrors.Add(
        'RC port 30010 still has a listener after final containment.')
}
if ($captureEvidence.Count -ne $poses.Count) {
    $postconditionErrors.Add(
        "Capture count mismatch: $($captureEvidence.Count)/$($poses.Count)")
}
$r28VisualCaptureAccepted = $false
$seenHashes = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($capture in $captureEvidence) {
    try {
        $verified = Get-PngEvidence -Path ([string] $capture.Png.Path) `
            -NotBeforeUtc ([DateTime]::SpecifyKind(
                [DateTime]::MinValue, [DateTimeKind]::Utc))
        if ($verified.Bytes -ne [int64] $capture.Png.Bytes -or
            $verified.Sha256 -cne [string] $capture.Png.Sha256) {
            throw "Capture identity changed after acceptance: $($capture.Png.Path)"
        }
        if (-not $seenHashes.Add([string] $verified.Sha256)) {
            throw "Two distinct vegetation poses produced the same PNG hash: $($verified.Sha256)"
        }
    }
    catch {
        $postconditionErrors.Add($_.Exception.Message)
    }
}
if ($RequireR28VisualSuccessor) {
    $expectedR28Filenames = @($poses | ForEach-Object {
        [string] $_.OutputFileName
    })
    $actualR28Filenames = @($captureEvidence | ForEach-Object {
        [string] $_.OutputFileName
    })
    $actualR28Roles = @($captureEvidence | ForEach-Object {
        [string] $_.Pose.VisualAcceptanceRole
    })
    if ($poses.Count -ne 4 -or $captureEvidence.Count -ne 4 -or
        [string]::Join('|', $actualR28Filenames) -cne
            [string]::Join('|', $expectedR28Filenames) -or
        @($r28EssentialVisualRoles | Where-Object {
            $_ -cnotin $actualR28Roles
        }).Count -ne 0) {
        $postconditionErrors.Add(
            'R28 evidence lost its exact four-file/four-essential-role roster.')
    }
    foreach ($capture in $captureEvidence) {
        try {
            if ([int64] $capture.Png.Bytes -le 0) {
                throw "R28 visual file is empty: $($capture.Png.Path)"
            }
            Assert-RequiredReportMarkers `
                -Report ([string] $capture.CaptureMessage) `
                -Markers $r28DiagnosticCaptureMarkers `
                -Description "R28 capture $($capture.Label)"
            Assert-RequiredReportMarkers `
                -Report ([string] $capture.ImmediatePreCaptureStateReport) `
                -Markers $r28StateValidationMarkers `
                -Description "R28 pre-capture state $($capture.Label)"
            Assert-RequiredReportMarkers `
                -Report ([string] $capture.PostCaptureStateReport) `
                -Markers $r28StateValidationMarkers `
                -Description "R28 post-capture state $($capture.Label)"
            Assert-RequiredReportMarkers `
                -Report ([string] $capture.PostCaptureValidationReport) `
                -Markers $r28PieValidationMarkers `
                -Description "R28 post-capture world $($capture.Label)"
        }
        catch {
            $postconditionErrors.Add($_.Exception.Message)
        }
    }
    try {
        Assert-RequiredReportMarkers -Report $r28SuccessorValidationReport `
            -Markers $r28MapValidationMarkers `
            -Description 'final R28 pre-PIE map/world report recheck'
        Assert-RequiredReportMarkers -Report $initialPieValidationReport `
            -Markers $r28PieValidationMarkers `
            -Description 'final initial R28 PIE report recheck'
    }
    catch {
        $postconditionErrors.Add($_.Exception.Message)
    }
}
if ($forcedContainment -and -not $watchdogMemoryAbort) {
    $postconditionErrors.Add(
        'The exact helper required forced containment; evidence cannot pass.')
}
if (-not $quitRequested -and -not $watchdogMemoryAbort) {
    $postconditionErrors.Add('Graceful QuitEditor was never requested.')
}
if ($exitCode -ne 0 -and -not $watchdogMemoryAbort) {
    $postconditionErrors.Add(
        "UE5.5 helper exit code was not zero: $exitCode")
}

if ($RequireR28VisualSuccessor -and $null -eq $workflowError -and
    $cleanupErrors.Count -eq 0 -and $postconditionErrors.Count -eq 0) {
    # This accepts the exact, complete, non-empty R28 Player0 raster set only.
    # It is deliberately independent from provider readiness and from manual
    # aesthetic/hyperrealism review.
    $r28VisualCaptureAccepted = $true
}

if ($watchdogMemoryAbort) {
    $memoryAbortCleanupObservations = @($cleanupErrors)
    $workflowText = if ($null -eq $workflowError) {
        'MEMORY_GUARD_UNKNOWN'
    }
    else { $workflowError.Message }
    throw "V5D vegetation/provider visual evidence aborted by the continuous memory guard; proofPublished=false visualCaptureAccepted=false partialEvidencePublicationAllowed=false workflow=$workflowText cleanup=$([string]::Join(' | ', @($memoryAbortCleanupObservations))) postconditions=$([string]::Join(' | ', @($postconditionErrors)))"
}

if ($null -ne $workflowError -or $cleanupErrors.Count -ne 0 -or
    $postconditionErrors.Count -ne 0) {
    $workflowText = if ($null -eq $workflowError) {
        'none'
    }
    else { $workflowError.Message }
    throw "V5D vegetation/provider visual evidence failed: workflow=$workflowText cleanup=$([string]::Join(' | ', @($cleanupErrors))) postconditions=$([string]::Join(' | ', @($postconditionErrors)))"
}

$result = [pscustomobject] [ordered] @{
    Schema =
        'triad.istana_explore_v5d.vegetation_provider.visual_evidence.v1'
    Status = 'PASS'
    Classification = if ($RequireR28VisualSuccessor) {
        'R28_VALIDATED_PLAYER0_RASTER_SET_COMPLETE_MANUAL_AESTHETIC_REVIEW_PENDING_PROVIDER_READINESS_SEPARATE'
    }
    elseif ($RequireLandmarkVegetationR27) {
        'R27_TECHNICAL_RASTER_CAPTURE_COMPLETE_MANUAL_VISUAL_ACCEPTANCE_PENDING'
    }
    elseif ($ProviderEvidenceMode -ceq 'ProviderFallback') {
        'STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF'
    }
    else {
        'RASTER_VISUAL_QA_EVIDENCE_NOT_SURVEY_NOT_PROVIDER_GEOMETRY_PROOF'
    }
    EvidenceModeClassification = $evidenceModeClassification
    PresentationRevision = if ($RequireR28VisualSuccessor) {
        'R28_VISUAL_SUCCESSOR_STRICT_CAPTURE'
    }
    elseif ($RequireLandmarkVegetationR27) {
        'LANDMARK_VEGETATION_R27_STRICT_CAPTURE'
    }
    else { 'LEGACY_VEGETATION_RANGE_CAPTURE' }
    Operation =
        if ($RequireR28VisualSuccessor) {
            "V5D_R28_VISUAL_SUCCESSOR_${ProviderEvidenceMode}_CAPTURE"
        }
        elseif ($RequireLandmarkVegetationR27) {
            "V5D_LANDMARK_VEGETATION_R27_${ProviderEvidenceMode}_CAPTURE"
        }
        else {
            "V5D_VEGETATION_NOMINAL_RANGE_${ProviderEvidenceMode}_CAPTURE"
        }
    RunToken = $RunToken
    VisualCaptureAccepted = [bool] $r28VisualCaptureAccepted
    ProviderEvidenceMode = $ProviderEvidenceMode
    TelemetryDwellSeconds = if ($ProviderEvidenceMode -cne 'ProviderReady') {
        $TelemetryDwellSeconds
    }
    else { 0 }
    PoseSequence = $poseSequence
    PoseOrder = @($poses | ForEach-Object { $_.Label })
    ScriptPin = $selfPin
    ProjectIdentityReport = $projectIdentityReport
    MapValidationReport = $mapValidationReport
    InitialPieValidationReport = $initialPieValidationReport
    TreeRuntimeReport = $treeRuntimeReport
    GroundVegetationRuntimeReport = $groundRuntimeReport
    R27GrassMaterialValidationReport =
        if ($RequireLandmarkVegetationR27) {
            $r27GrassMaterialValidationReport
        }
        else { $null }
    TemasekPhase2AssetValidationReport =
        if ($RequireLandmarkVegetationR27) {
            $temasekPhase2AssetValidationReport
        }
        else { $null }
    R27SuccessorValidationReport =
        if ($RequireLandmarkVegetationR27) {
            $r27SuccessorValidationReport
        }
        else { $null }
    R28SuccessorValidationReport = if ($RequireR28VisualSuccessor) {
        $r28SuccessorValidationReport
    }
    else { $null }
    RuntimeMarkerContract = [pscustomobject] [ordered] @{
        Tree = $treeRuntimeMarkers
        GroundVegetation = $groundRuntimeMarkers
        LandmarkVegetationR27 = if ($RequireLandmarkVegetationR27) {
            $r27SuccessorMarkers
        }
        else { @() }
        LandmarkGrassMaterialsR27 = if ($RequireLandmarkVegetationR27) {
            $r27GrassMaterialMarkers
        }
        else { @() }
        TemasekPhase2Assets = if ($RequireLandmarkVegetationR27) {
            $temasekPhase2AssetMarkers
        }
        else { @() }
        R28MapWorld = if ($RequireR28VisualSuccessor) {
            $r28MapValidationMarkers
        }
        else { @() }
        R28PlayWorld = if ($RequireR28VisualSuccessor) {
            $r28PieValidationMarkers
        }
        else { @() }
        R28PlayState = if ($RequireR28VisualSuccessor) {
            $r28StateValidationMarkers
        }
        else { @() }
        R28Player0Capture = if ($RequireR28VisualSuccessor) {
            $r28DiagnosticCaptureMarkers
        }
        else { @() }
    }
    MapPin = $mapPin
    RuntimeDllPin = $runtimeDllPin
    EditorDllPin = $editorDllPin
    SuppressedFallbackMeshAssetPin = $suppressedFallbackMeshPin
    OuterGroundMeshAssetPin = $outerGroundMeshPin
    OuterGroundMaterialAssetPin = $outerGroundMaterialPin
    R27CommitReceipt = if ($RequireLandmarkVegetationR27) {
        [pscustomobject] [ordered] @{
            Pin = $r27CommitReceiptEvidence.Pin
            TransactionRunToken = $r27CommitReceiptEvidence.RunToken
            Schema = [string] $r27CommitReceiptEvidence.Receipt.Schema
            Status = [string] $r27CommitReceiptEvidence.Receipt.Status
            VisualCaptureAcceptedBeforeThisCapture =
                [bool] $r27CommitReceiptEvidence.Receipt.VisualCaptureAccepted
            CaptureRevalidationRequiredBeforeThisCapture =
                [bool] $r27CommitReceiptEvidence.Receipt.CaptureRevalidationRequired
        }
    }
    else { $null }
    R28CommitReceipt = if ($RequireR28VisualSuccessor) {
        [pscustomobject] [ordered] @{
            Pin = $r28CommitReceiptEvidence.Pin
            TransactionRunToken = $r28CommitReceiptEvidence.RunToken
            Schema = [string] $r28CommitReceiptEvidence.Receipt.Schema
            Status = [string] $r28CommitReceiptEvidence.Receipt.Status
            PredecessorMap = $r28CommitReceiptEvidence.Receipt.PredecessorMap
            PredecessorRuntimeDll =
                $r28CommitReceiptEvidence.Receipt.PredecessorRuntimeDll
            PredecessorEditorDll =
                $r28CommitReceiptEvidence.Receipt.PredecessorEditorDll
            SuccessorMap = $r28CommitReceiptEvidence.Receipt.SuccessorMap
            SuccessorRuntimeDll =
                $r28CommitReceiptEvidence.Receipt.SuccessorRuntimeDll
            SuccessorEditorDll =
                $r28CommitReceiptEvidence.Receipt.SuccessorEditorDll
            ExactlyNewAssetCount =
                [int] $r28CommitReceiptEvidence.Receipt.ExactlyNewAssetCount
            VisualCaptureAcceptedBeforeThisCapture =
                [bool] $r28CommitReceiptEvidence.Receipt.VisualCaptureAccepted
            CaptureRevalidationRequiredBeforeThisCapture =
                [bool] $r28CommitReceiptEvidence.Receipt.CaptureRevalidationRequired
        }
    }
    else { $null }
    R27GrassMaterialPins = @($r27GrassMaterialPins)
    R28VisualEvidenceContract = [pscustomobject] [ordered] @{
        Required = [bool] $RequireR28VisualSuccessor
        ExactCaptureCount = if ($RequireR28VisualSuccessor) { 4 } else { 0 }
        ExactCaptureFilenames = if ($RequireR28VisualSuccessor) {
            @($poses | ForEach-Object { [string] $_.OutputFileName })
        }
        else { @() }
        EssentialViews = if ($RequireR28VisualSuccessor) {
            @($poses | ForEach-Object {
                [pscustomobject] [ordered] @{
                    Label = [string] $_.Label
                    Role = [string] $_.VisualAcceptanceRole
                    SceneTarget = [string] $_.SceneTarget
                    OutputFileName = [string] $_.OutputFileName
                }
            })
        }
        else { @() }
        EveryFileExistsAndIsNonzero = [bool] $r28VisualCaptureAccepted
        EveryMapWorldStateAndCaptureValidationPassed =
            [bool] $r28VisualCaptureAccepted
        R28ActorVisibleInPlayer0 = [bool] $r28VisualCaptureAccepted
        R28ActorExcludedFromSensorSceneCapture =
            [bool] $r28VisualCaptureAccepted
        ProviderReadinessImplied = $false
        VisualCaptureAccepted = [bool] $r28VisualCaptureAccepted
    }
    ManualVisualAcceptance = $manualVisualAcceptanceContract
    MemorySafety = [pscustomobject] [ordered] @{
        PrivateMemoryCeilingBytes = $privateMemoryCeilingBytes
        PrivateMemoryCeilingGiB = 12
        MinimumSystemFreeVirtualBytes =
            $minimumSystemFreeVirtualBytes
        MinimumSystemFreeVirtualGiB = 6
        MinimumSystemFreeVirtualAtLaunchBytes =
            $minimumSystemFreeVirtualAtLaunchBytes
        MinimumSystemFreeVirtualAtLaunchGiB = 10
        PreLaunchAdmissionApplied = $true
        PreLaunchSnapshot = $startupMemorySnapshot
        ContinuousMemoryWatchdogIntegrated =
            $continuousMemoryWatchdogIntegrated
        StrictProviderReadyLiveLaunchEnabled =
            $continuousMemoryWatchdogIntegrated
        PollMilliseconds = $memoryWatchdogPollMilliseconds
        PersistentBreachContainmentMilliseconds =
            $memoryWatchdogPersistentBreachMilliseconds
        CaptureTextureStreamingPoolMiB =
            $captureTextureStreamingPoolMiB
        CaptureTextureStreamingPoolOverrideToken =
            $captureTextureStreamingPoolOverrideToken
        CaptureTextureStreamingPoolApplication =
            'ENGINE_INI_CONSOLE_VARIABLES_PRE_MAP_LOAD_PROVEN'
        TextureStreamingPoolApplicationEvidence =
            $textureStreamingPoolApplicationEvidence
        Coverage =
            'CONTINUOUS_FROM_IMMEDIATELY_AFTER_START_PROCESS_THROUGH_FINAL_TEARDOWN'
        IndependentClrThread = $true
        ExactPidCreationTimeAndExecutableRequiredForContainment = $true
        MemoryAlertSuppressesProofPublication = $true
        WatchdogSnapshot = $continuousWatchdogSnapshot
        MemoryAbort = $watchdogMemoryAbort
        EmergencyCleanupObservations =
            @($memoryAbortCleanupObservations)
        RequiredFutureHardening = $null
    }
    EditorIdentity = $launchedIdentity
    Captures = @($captureEvidence)
    ProviderReadiness = [pscustomobject] [ordered] @{
        EvidenceMode = $ProviderEvidenceMode
        ProviderReadyProofClaimed =
            $ProviderEvidenceMode -ceq 'ProviderReady'
        ProviderReadyRequired =
            $ProviderEvidenceMode -ceq 'ProviderReady'
        LocalFallbackRequiredVisible =
            $ProviderEvidenceMode -ceq 'ProviderFallback'
        SeparatelyEvaluatedFromVisualCapture = $true
        ImpliedByVisualCaptureAccepted = $false
        R28VisualSuccessorProviderReady = if ($RequireR28VisualSuccessor) {
            $false
        }
        else { $null }
    }
    ProviderHandling = [pscustomobject] [ordered] @{
        Usage = 'VISUAL_BACKGROUND_PIXELS_IN_RASTER_SCREENSHOTS_ONLY'
        CaptureFunction = if ($RequireR28VisualSuccessor) {
            'CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView'
        }
        elseif ($ProviderEvidenceMode -cne 'ProviderReady') {
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView'
        }
        else { 'CaptureIstanaExploreV5DHybridPlayView' }
        EvidenceMode = $ProviderEvidenceMode
        StableExactPoseAndFiniteTelemetryDwellRequired =
            $ProviderEvidenceMode -cne 'ProviderReady'
        GlobalProviderReadyGateRequired =
            $ProviderEvidenceMode -ceq 'ProviderReady'
        StrictLocalFallbackGateRequired =
            $ProviderEvidenceMode -ceq 'ProviderFallback'
        RequiredProviderReadyForProof =
            if ($ProviderEvidenceMode -ceq 'ProviderFallback') { $false }
            elseif ($ProviderEvidenceMode -ceq 'ProviderReady') { $true }
            else { $null }
        RequiredLocalFallbackHidden =
            if ($ProviderEvidenceMode -ceq 'ProviderFallback') { $false }
            elseif ($ProviderEvidenceMode -ceq 'ProviderReady') { $true }
            else { $null }
        StrictDiagnosticFallbackAcknowledgementRequired =
            $ProviderEvidenceMode -ceq 'ProviderFallback'
        StrictDiagnosticFallbackAcknowledgementMarker =
            if ($ProviderEvidenceMode -ceq 'ProviderFallback') {
                $strictFallbackCaptureAcknowledgementMarker
            }
            else { $null }
        ProviderReadyProofClaimed =
            $ProviderEvidenceMode -ceq 'ProviderReady'
        ProviderReadyRateLimitAbortThreshold =
            $ProviderReadyRateLimitAbortThreshold
        ProviderTimeoutSeconds = $ProviderTimeoutSeconds
        ObservedProviderRateLimitEvents =
            Get-ProviderRateLimitEventCount
        GlobalLoadProgressRecordedAsTelemetryOnly = $true
        PoseLocalProviderReadinessClaimed = $false
        DedicatedProviderExclusionClaimed = $false
        PossibleProviderOverlapRemainsDisclosed = $true
        ProviderGeometryExported = $false
        ProviderGeometryTraced = $false
        ProviderGeometryAnalysed = $false
        ProviderGeometryDerived = $false
        ProviderGeometryBaked = $false
    }
    Authority = [pscustomobject] [ordered] @{
        VisualQaOnly = $true
        SurveyOrAsBuilt = $false
        Collision = $false
        Navigation = $false
        SensorOcclusion = $false
        RfGeometryOrMaterial = $false
        MaterialsCalibrated = $false
        NominalRangeLabelsAreGeometryOrCullProof = $false
        AutomatedPhotorealismOrHyperrealismClaim = $false
        TechnicalCapturePassIsManualVisualAcceptance = $false
    }
    EndPlayEvidence = $endPlayEvidence
    QuitConnectionResult = $quitConnectionResult
    GracefulQuitRequested = $quitRequested
    ForcedContainment = $forcedContainment
    EditorExitCode = $exitCode
    ProtectedUE54 = $script:protectedUE54Before
    FinalNativeTRIADUnrealProcessCount =
        @(Get-NativeTRIADUnrealProcesses).Count
    FinalRcListenerCount = @(Get-RcListeners).Count
    Log = $logFile
    EvidenceManifestPath = $evidenceManifestPath
}
$json = $result | ConvertTo-Json -Depth 20
$temporaryManifest = $evidenceManifestPath + '.tmp.' +
    [Guid]::NewGuid().ToString('N')
try {
    $stream = [IO.File]::Open($temporaryManifest, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $writer = [IO.StreamWriter]::new(
            $stream, [Text.UTF8Encoding]::new($false), 4096, $true)
        try {
            $writer.Write($json)
            $writer.Write([Environment]::NewLine)
            $writer.Flush()
            $stream.Flush($true)
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
    [IO.File]::Move($temporaryManifest, $evidenceManifestPath)
}
finally {
    if ([IO.File]::Exists($temporaryManifest)) {
        [IO.File]::Delete($temporaryManifest)
    }
}
$published = Get-FileIdentity -Path $evidenceManifestPath
[pscustomobject] [ordered] @{
    Status = 'PASS'
    EvidenceManifest = $published
    Captures = @($captureEvidence | ForEach-Object { $_.Png })
    VisualCaptureAccepted = [bool] $r28VisualCaptureAccepted
    ProviderReadinessImpliedByVisualCapture = $false
    ManualVisualAcceptanceStatus =
        $manualVisualAcceptanceContract.Status
    AutomatedHyperrealismClaimed = $false
    ProviderGeometryExportedTracedAnalysedDerivedOrBaked = $false
    ProtectedUE54Unchanged = $true
} | ConvertTo-Json -Depth 8
