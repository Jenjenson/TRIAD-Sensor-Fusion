#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [int64] $ExpectedMapBytes = 34993427L,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedMapSha256 =
        '38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9',

    [int64] $ExpectedRuntimeDllBytes = 4585984L,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedRuntimeDllSha256 =
        '31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4',

    [int64] $ExpectedEditorDllBytes = 7671296L,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedEditorDllSha256 =
        '471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34',

    # R27 is opt-in so the historical R25 six-pose retry remains replayable.
    # Live R27 retry requires the exact successful transaction receipt in
    # addition to explicit map and DLL pins and expects the R27 seven-pose set.
    [switch] $RequireLandmarkVegetationR27,

    [string] $R27CommitReceiptPath = '',

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR27CommitReceiptSha256 = '',

    [ValidateRange(1, 3)]
    [int] $MaximumAttempts = 2,

    [ValidateRange(60, 900)]
    [int] $CooldownSeconds = 180,

    [ValidateRange(1, 100)]
    [int] $RateLimitAbortThreshold = 1,

    [ValidateRange(60, 900)]
    [int] $EditorTimeoutSeconds = 600,

    [ValidateRange(60, 300)]
    [int] $PieTimeoutSeconds = 120,

    [ValidateRange(60, 3600)]
    [int] $ProviderTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $CaptureTimeoutSeconds = 120,

    [switch] $StaticSelfCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$captureWrapper = Join-Path $PSScriptRoot `
    'Capture-IstanaExploreV5DVegetationProviderEvidence.ps1'
$mapFile =
    'D:\triad\TRIAD\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'
$runtimeDll =
    'D:\triad\TRIAD\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'
$editorDll =
    'D:\triad\TRIAD\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'
$outputRoot = 'D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D'
$logRoot = 'D:\triad\TRIAD\Saved\Logs'
$receiptPath = Join-Path $outputRoot `
    "explore_v5d_vegetation_provider_ready_retry_${RunToken}.json"
$rateLimitLogMarker = 'Received status code 429 for tile content '
$requiredContextFacadeR25Marker = 'contextFacadeR25=true'
$r27TransactionBase =
    'D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DLandmarkVegetationR27V1'
$projectFile = [IO.Path]::GetFullPath('D:\triad\TRIAD\TRIAD.uproject')
$nativeUE55EngineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$script:protectedUE54Before = $null
$script:r27CommitReceiptEvidence = $null
$expectedCaptureCount = if ($RequireLandmarkVegetationR27) { 7 } else { 6 }

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
        Bytes = [int64] $item.Length
        Sha256 = [string] (Get-FileHash -LiteralPath $fullPath `
            -Algorithm SHA256).Hash
    }
}

function Assert-IdentityUnchanged {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    $actual = Get-FileIdentity -Path ([string] $Expected.Path)
    if ($actual.Bytes -ne [int64] $Expected.Bytes -or
        $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "File identity changed at '$Checkpoint': $($Expected.Path) expected=$($Expected.Bytes)/$($Expected.Sha256) actual=$($actual.Bytes)/$($actual.Sha256)"
    }
}

function Assert-ExactIdentity {
    param(
        [Parameter(Mandatory = $true)] $Actual,
        [Parameter(Mandatory = $true)] [int64] $ExpectedBytes,
        [Parameter(Mandatory = $true)] [string] $ExpectedSha256,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    if ($Actual.Bytes -ne $ExpectedBytes -or
        $Actual.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "$Description identity mismatch: expected=$ExpectedBytes/$($ExpectedSha256.ToUpperInvariant()) actual=$($Actual.Bytes)/$($Actual.Sha256)"
    }
}

function Get-RequiredObjectProperty {
    param(
        [Parameter(Mandatory = $true)] $Object,
        [Parameter(Mandatory = $true)] [string] $Name,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Description lacks required property '$Name'."
    }
    $property.Value
}

function Assert-ReceiptFileIdentity {
    param(
        [Parameter(Mandatory = $true)] $ReceiptFile,
        [Parameter(Mandatory = $true)] $ActualPin,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    $receiptPath = [IO.Path]::GetFullPath(
        [string] (Get-RequiredObjectProperty $ReceiptFile 'Path' $Description))
    $receiptPresent = [bool] (Get-RequiredObjectProperty `
        $ReceiptFile 'Present' $Description)
    $receiptBytes = [int64] (Get-RequiredObjectProperty `
        $ReceiptFile 'Bytes' $Description)
    $receiptSha256 = [string] (Get-RequiredObjectProperty `
        $ReceiptFile 'Sha256' $Description)
    if (-not $receiptPresent -or $receiptPath -ine [string] $ActualPin.Path -or
        $receiptBytes -ne [int64] $ActualPin.Bytes -or
        $receiptSha256 -cne [string] $ActualPin.Sha256) {
        throw "$Description does not match the selected live file identity."
    }
}

function Get-ValidatedR27CommitReceipt {
    param(
        [Parameter(Mandatory = $true)] $MapPin,
        [Parameter(Mandatory = $true)] $RuntimePin,
        [Parameter(Mandatory = $true)] $EditorPin
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
    Assert-ExactIdentity -Actual $receiptPin -ExpectedBytes $receiptPin.Bytes `
        -ExpectedSha256 $ExpectedR27CommitReceiptSha256 `
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

    Assert-ReceiptFileIdentity `
        (Get-RequiredObjectProperty $receipt 'SuccessorMap' 'R27 receipt') `
        $MapPin 'R27 successor map receipt'
    Assert-ReceiptFileIdentity `
        (Get-RequiredObjectProperty $receipt 'RuntimeDll' 'R27 receipt') `
        $RuntimePin 'R27 runtime DLL receipt'
    Assert-ReceiptFileIdentity `
        (Get-RequiredObjectProperty $receipt 'EditorDll' 'R27 receipt') `
        $EditorPin 'R27 editor DLL receipt'

    $receiptMaterials = @(
        Get-RequiredObjectProperty $receipt 'R27MaterialPackages' 'R27 receipt')
    $expectedMaterialNames = @(
        'M_IPV5D_LandmarkTurf_R27_Manicured.uasset',
        'M_IPV5D_LandmarkTurf_R27_Humid.uasset',
        'M_IPV5D_LandmarkTurf_R27_Shade.uasset',
        'M_IPV5D_LandmarkTurf_R27_DryEdge.uasset'
    ) | Sort-Object
    $actualMaterialNames = @($receiptMaterials | ForEach-Object {
            [IO.Path]::GetFileName([string] (
                Get-RequiredObjectProperty $_ 'Path' 'R27 material receipt'))
        } | Sort-Object)
    if ($receiptMaterials.Count -ne 4 -or
        [string]::Join('|', $actualMaterialNames) -cne
            [string]::Join('|', $expectedMaterialNames)) {
        throw 'R27 receipt lost its exact four-package material roster.'
    }
    foreach ($material in $receiptMaterials) {
        if (-not [bool] (Get-RequiredObjectProperty `
                $material 'Present' 'R27 material receipt') -or
            [int64] (Get-RequiredObjectProperty `
                $material 'Bytes' 'R27 material receipt') -le 0 -or
            [string] (Get-RequiredObjectProperty `
                $material 'Sha256' 'R27 material receipt') -cnotmatch
                '^[0-9A-F]{64}$') {
            throw 'R27 receipt contains an invalid material package identity.'
        }
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

function Test-ExactCommandLineToken {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [string] $Token
    )

    $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($Token) + '"?(?:\s|$)'
    [regex]::Matches($CommandLine, $pattern).Count -eq 1
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
                throw 'The CAPSTONE project token is owned by an unexpected process; refusing ProviderReady retry orchestration.'
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
        throw 'Protected UE5.4/CAPSTONE identity set changed during ProviderReady retry orchestration.'
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
        })
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

function Assert-NoNativeTRIADOrRemoteControlOwner {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    if ($null -eq $script:protectedUE54Before) {
        throw 'Protected UE5.4/CAPSTONE preflight identity set was not established.'
    }
    [void] (Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before)
    $editors = @(Get-NativeTRIADUnrealProcesses)
    if ($editors.Count -ne 0) {
        $ids = @($editors | ForEach-Object { $_.ProcessId }) -join ','
        throw "UE5.5/native TRIAD Unreal helpers remain at '$Checkpoint' (PIDs=$ids); refusing retry orchestration."
    }
    $listeners = @(Get-RemoteControlListeners)
    if ($listeners.Count -ne 0) {
        throw "Remote Control port 30010 remains owned at '$Checkpoint' (PIDs=$([string]::Join(',', $listeners))); refusing retry orchestration."
    }
}

function Get-RateLimitCount {
    param([Parameter(Mandatory = $true)] [string] $Path)

    if (-not [IO.File]::Exists($Path)) {
        return 0
    }
    $stream = [IO.File]::Open(
        $Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try {
        $reader = [IO.StreamReader]::new(
            $stream, [Text.UTF8Encoding]::new($false), $true, 4096, $true)
        try {
            $text = $reader.ReadToEnd()
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
    [regex]::Matches(
        $text,
        [regex]::Escape($rateLimitLogMarker),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant).Count
}

function Assert-RetryBoundary {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    Assert-IdentityUnchanged -Expected $script:selfPin `
        -Checkpoint $Checkpoint
    Assert-IdentityUnchanged -Expected $script:captureWrapperPin `
        -Checkpoint $Checkpoint
    Assert-IdentityUnchanged -Expected $script:mapPin `
        -Checkpoint $Checkpoint
    Assert-IdentityUnchanged -Expected $script:runtimeDllPin `
        -Checkpoint $Checkpoint
    Assert-IdentityUnchanged -Expected $script:editorDllPin `
        -Checkpoint $Checkpoint
    if ($null -ne $script:r27CommitReceiptEvidence) {
        Assert-IdentityUnchanged `
            -Expected $script:r27CommitReceiptEvidence.Pin `
            -Checkpoint $Checkpoint
    }
    Assert-NoNativeTRIADOrRemoteControlOwner -Checkpoint $Checkpoint
}

function Wait-ExactCooldown {
    param(
        [Parameter(Mandatory = $true)] [int] $Seconds,
        [Parameter(Mandatory = $true)] [int] $CompletedAttempt
    )

    $started = [DateTime]::UtcNow
    $deadline = $started.AddSeconds($Seconds)
    Write-Host "PROVIDER_RATE_LIMIT_COOLDOWN attempt=$CompletedAttempt seconds=$Seconds cachePolicy=preserve_standard_persistent_http_cache"
    do {
        $remaining = ($deadline - [DateTime]::UtcNow).TotalSeconds
        if ($remaining -le 0.0) {
            break
        }
        $slice = [Math]::Min(30, [Math]::Max(1, [Math]::Ceiling($remaining)))
        Start-Sleep -Seconds $slice
        Assert-RetryBoundary `
            -Checkpoint "cooldown-after-attempt-$CompletedAttempt"
    } while ([DateTime]::UtcNow -lt $deadline)
    ([DateTime]::UtcNow - $started).TotalSeconds
}

$script:selfPin = Get-FileIdentity -Path $PSCommandPath
$script:captureWrapperPin = Get-FileIdentity -Path $captureWrapper

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Schema =
            'triad.istana_explore_v5d.vegetation_provider.ready_retry.static_check.v1'
        Status = 'STATIC_SELF_CHECK_PASS'
        Script = $script:selfPin
        StrictLeafWrapper = $script:captureWrapperPin
        ProviderEvidenceMode = 'ProviderReady'
        LandmarkVegetationR27Required =
            [bool] $RequireLandmarkVegetationR27
        ExpectedCaptureCount = $expectedCaptureCount
        MaximumAttempts = $MaximumAttempts
        CooldownSeconds = $CooldownSeconds
        RateLimitAbortThreshold = $RateLimitAbortThreshold
        RetryTrigger = 'EXPLICIT_TILE_HTTP_429_ONLY'
        ReadyThresholdPercent = 98.0
        RequiredConsecutiveReadySamples = 3
        ProviderTimeoutSeconds = $ProviderTimeoutSeconds
        AttemptIsolation = 'FRESH_WHOLE_HELPER_PROCESS_AND_UNIQUE_RUN_TOKEN'
        PinRevalidation = 'BEFORE_EVERY_ATTEMPT_AND_EVERY_COOLDOWN_SLICE'
        PersistentCachePolicy =
            'PRESERVE_CESIUM_STANDARD_PERSISTENT_HTTP_CACHE_NO_CLEAR_NO_RELOCATION'
        ProtectedUE54Policy =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        PartialEvidencePublicationAllowed = $false
        RequiredContextFacadeR25Marker =
            $requiredContextFacadeR25Marker
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
    } | ConvertTo-Json -Depth 8
    return
}

if ($RequireLandmarkVegetationR27) {
    foreach ($requiredIdentityParameter in @(
            'ExpectedMapBytes',
            'ExpectedMapSha256',
            'ExpectedRuntimeDllBytes',
            'ExpectedRuntimeDllSha256',
            'ExpectedEditorDllBytes',
            'ExpectedEditorDllSha256')) {
        if (-not $PSBoundParameters.ContainsKey($requiredIdentityParameter)) {
            throw "Strict R27 ProviderReady retry requires explicit caller-supplied -$requiredIdentityParameter from the selected R27 commit receipt."
        }
    }
    if (-not $PSBoundParameters.ContainsKey('R27CommitReceiptPath') -or
        [string]::IsNullOrWhiteSpace($R27CommitReceiptPath) -or
        -not $PSBoundParameters.ContainsKey(
            'ExpectedR27CommitReceiptSha256') -or
        $ExpectedR27CommitReceiptSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
        throw 'Strict R27 ProviderReady retry requires an explicit commit.json path and its exact SHA-256.'
    }
}
elseif ($PSBoundParameters.ContainsKey('R27CommitReceiptPath') -or
    $PSBoundParameters.ContainsKey('ExpectedR27CommitReceiptSha256')) {
    throw 'R27 receipt parameters are valid only with -RequireLandmarkVegetationR27.'
}

if ($ExpectedMapBytes -le 0 -or $ExpectedRuntimeDllBytes -le 0 -or
    $ExpectedEditorDllBytes -le 0 -or
    $ExpectedMapSha256 -cnotmatch '^[0-9A-Fa-f]{64}$' -or
    $ExpectedRuntimeDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$' -or
    $ExpectedEditorDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live ProviderReady retry requires positive byte counts and SHA-256 identities for the selected map and both guarded DLLs.'
}
$ExpectedMapSha256 = $ExpectedMapSha256.ToUpperInvariant()
$ExpectedRuntimeDllSha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant()
$ExpectedEditorDllSha256 = $ExpectedEditorDllSha256.ToUpperInvariant()
if ($RequireLandmarkVegetationR27) {
    $ExpectedR27CommitReceiptSha256 =
        $ExpectedR27CommitReceiptSha256.ToUpperInvariant()
}

if (-not (Test-Path -LiteralPath $outputRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $logRoot -PathType Container)) {
    throw 'The exact output or log directory is absent.'
}
if ([IO.File]::Exists($receiptPath)) {
    throw "Refusing to overwrite an existing retry receipt: $receiptPath"
}

$script:protectedUE54Before = Get-ProtectedUE54Identity
$script:mapPin = Get-FileIdentity -Path $mapFile
$script:runtimeDllPin = Get-FileIdentity -Path $runtimeDll
$script:editorDllPin = Get-FileIdentity -Path $editorDll
Assert-ExactIdentity -Actual $script:mapPin `
    -ExpectedBytes $ExpectedMapBytes -ExpectedSha256 $ExpectedMapSha256 `
    -Description 'selected V5D map'
Assert-ExactIdentity -Actual $script:runtimeDllPin `
    -ExpectedBytes $ExpectedRuntimeDllBytes `
    -ExpectedSha256 $ExpectedRuntimeDllSha256 `
    -Description 'selected runtime DLL'
Assert-ExactIdentity -Actual $script:editorDllPin `
    -ExpectedBytes $ExpectedEditorDllBytes `
    -ExpectedSha256 $ExpectedEditorDllSha256 `
    -Description 'selected editor DLL'
if ($RequireLandmarkVegetationR27) {
    $script:r27CommitReceiptEvidence = Get-ValidatedR27CommitReceipt `
        -MapPin $script:mapPin -RuntimePin $script:runtimeDllPin `
        -EditorPin $script:editorDllPin
}
Assert-RetryBoundary -Checkpoint 'retry-orchestrator-preflight'

$attempts = [Collections.Generic.List[object]]::new()
$successfulLeaf = $null
$successfulManifest = $null

for ($attempt = 1; $attempt -le $MaximumAttempts; ++$attempt) {
    Assert-RetryBoundary -Checkpoint "before-attempt-$attempt"
    $attemptToken = "${RunToken}_a${attempt}"
    $attemptLog = Join-Path $logRoot `
        "Codex_V5D_VegetationProvider_${attemptToken}.log"
    $startedUtc = [DateTime]::UtcNow
    $retryable = $false
    try {
        $leafArguments = @{
            RunToken = $attemptToken
            ExpectedMapBytes = $ExpectedMapBytes
            ExpectedMapSha256 = $ExpectedMapSha256
            ProviderEvidenceMode = 'ProviderReady'
            ExpectedRuntimeDllBytes = $ExpectedRuntimeDllBytes
            ExpectedRuntimeDllSha256 = $ExpectedRuntimeDllSha256
            ExpectedEditorDllBytes = $ExpectedEditorDllBytes
            ExpectedEditorDllSha256 = $ExpectedEditorDllSha256
            EditorTimeoutSeconds = $EditorTimeoutSeconds
            PieTimeoutSeconds = $PieTimeoutSeconds
            ProviderTimeoutSeconds = $ProviderTimeoutSeconds
            ProviderReadyRateLimitAbortThreshold =
                $RateLimitAbortThreshold
            CaptureTimeoutSeconds = $CaptureTimeoutSeconds
        }
        if ($RequireLandmarkVegetationR27) {
            $leafArguments.RequireLandmarkVegetationR27 = $true
            $leafArguments.R27CommitReceiptPath = $R27CommitReceiptPath
            $leafArguments.ExpectedR27CommitReceiptSha256 =
                $ExpectedR27CommitReceiptSha256
        }
        $leafOutput = @(& $captureWrapper @leafArguments)
        $leafJson = [string]::Join(
            [Environment]::NewLine,
            @($leafOutput | ForEach-Object { [string] $_ }))
        $leafResult = $leafJson | ConvertFrom-Json -Depth 20
        if ($leafResult.Status -cne 'PASS' -or
            $leafResult.Captures.Count -ne $expectedCaptureCount -or
            $leafResult.ProviderGeometryExportedTracedAnalysedDerivedOrBaked `
                -ne $false) {
            throw 'Strict leaf returned an invalid success envelope.'
        }
        $manifestPin = Get-FileIdentity `
            -Path ([string] $leafResult.EvidenceManifest.Path)
        if ($manifestPin.Bytes -ne
                [int64] $leafResult.EvidenceManifest.Bytes -or
            $manifestPin.Sha256 -cne
                [string] $leafResult.EvidenceManifest.Sha256) {
            throw 'Strict leaf evidence manifest identity changed before retry acceptance.'
        }
        $manifest = Get-Content -LiteralPath $manifestPin.Path -Raw |
            ConvertFrom-Json -Depth 30
        if ($manifest.Status -cne 'PASS' -or
            $manifest.ProviderEvidenceMode -cne 'ProviderReady' -or
            $manifest.Captures.Count -ne $expectedCaptureCount -or
            $manifest.ProviderHandling.RequiredProviderReadyForProof -ne
                $true -or
            $manifest.ProviderHandling.RequiredLocalFallbackHidden -ne
                $true -or
            $manifest.ProviderHandling.GlobalProviderReadyGateRequired -ne
                $true -or
            $manifest.ProviderHandling.ProviderReadyProofClaimed -ne $true -or
            -not ([string] $manifest.MapValidationReport).Contains(
                $requiredContextFacadeR25Marker,
                [StringComparison]::Ordinal) -or
            [int] $manifest.ProviderHandling.ProviderReadyRateLimitAbortThreshold `
                -ne $RateLimitAbortThreshold) {
            throw 'Strict leaf evidence manifest lost its ProviderReady proof contract.'
        }
        if ($RequireLandmarkVegetationR27 -and
            ($manifest.PresentationRevision -cne
                'LANDMARK_VEGETATION_R27_STRICT_CAPTURE' -or
            $manifest.PoseOrder -cne
                'far-to-near-with-dedicated-12m-ground-grazing-r27-review' -or
            $manifest.ManualVisualAcceptance.ReviewRequired -ne $true -or
            $manifest.ManualVisualAcceptance.PrimaryPose -cne
                '012m_ground_grazing_r27' -or
            $null -eq $manifest.R27CommitReceipt -or
            [string] $manifest.R27CommitReceipt.Pin.Path -ine
                [string] $script:r27CommitReceiptEvidence.Pin.Path -or
            [int64] $manifest.R27CommitReceipt.Pin.Bytes -ne
                [int64] $script:r27CommitReceiptEvidence.Pin.Bytes -or
            [string] $manifest.R27CommitReceipt.Pin.Sha256 -cne
                [string] $script:r27CommitReceiptEvidence.Pin.Sha256)) {
            throw 'Strict leaf evidence manifest lost its exact R27 capture/receipt contract.'
        }
        foreach ($captureEvidence in @($manifest.Captures)) {
            $readyReports = @(
                $captureEvidence.ConsecutiveExactStateReports)
            $readySamples = @(
                $captureEvidence.ProviderTelemetrySamples)
            if ($readyReports.Count -lt 3 -or $readySamples.Count -lt 3) {
                throw "Strict leaf capture $($captureEvidence.Label) has fewer than three consecutive ProviderReady state samples."
            }
            foreach ($readyReport in $readyReports) {
                if (-not ([string] $readyReport).Contains(
                        'providerReadyForProof=true',
                        [StringComparison]::Ordinal) -or
                    -not ([string] $readyReport).Contains(
                        'localFallbackHidden=true',
                        [StringComparison]::Ordinal)) {
                    throw "Strict leaf capture $($captureEvidence.Label) contains a non-ready stabilization report."
                }
            }
            foreach ($readySample in $readySamples) {
                if ($readySample.ProviderReadyForProof -ne $true -or
                    $readySample.LocalFallbackHidden -ne $true) {
                    throw "Strict leaf capture $($captureEvidence.Label) contains a non-ready telemetry sample."
                }
            }
            if ($captureEvidence.ImmediatePreCaptureProviderTelemetry.ProviderReadyForProof `
                    -ne $true -or
                $captureEvidence.ImmediatePreCaptureProviderTelemetry.LocalFallbackHidden `
                    -ne $true -or
                $captureEvidence.PostCaptureProviderTelemetry.ProviderReadyForProof `
                    -ne $true -or
                $captureEvidence.PostCaptureProviderTelemetry.LocalFallbackHidden `
                    -ne $true) {
                throw "Strict leaf capture $($captureEvidence.Label) lost ProviderReady immediately before or after capture."
            }
        }
        $rateLimitCount = Get-RateLimitCount -Path $attemptLog
        if ($rateLimitCount -ge $RateLimitAbortThreshold) {
            throw "Strict leaf published despite rate-limit abort threshold: observed429=$rateLimitCount threshold=$RateLimitAbortThreshold"
        }
        Assert-RetryBoundary -Checkpoint "after-successful-attempt-$attempt"
        $attempts.Add([pscustomobject] [ordered] @{
            Attempt = $attempt
            RunToken = $attemptToken
            StartedUtc = $startedUtc.ToString('o')
            EndedUtc = [DateTime]::UtcNow.ToString('o')
            Outcome = 'PASS'
            Retryable = $false
            ObservedHttp429Events = $rateLimitCount
            Log = Get-FileIdentity -Path $attemptLog
            EvidenceManifest = $manifestPin
        })
        $successfulLeaf = $leafResult
        $successfulManifest = $manifestPin
        break
    }
    catch {
        $failureMessage = [string] $_.Exception.Message
        $rateLimitCount = Get-RateLimitCount -Path $attemptLog
        # A retryable leaf failure has one exact expected shape: the explicit
        # 429 marker was the workflow error, graceful cleanup had no errors,
        # and the only postcondition failure is the deliberately incomplete
        # expected-capture set. Process-containment or unrelated validation errors
        # must never be laundered into a retry.
        $cleanRateLimitShutdown = $failureMessage -match
            '^V5D vegetation/provider visual evidence failed: workflow=PROVIDER_RATE_LIMIT_RETRYABLE .+ cleanup= postconditions=Capture count mismatch: (?<Completed>[0-9]+)/(?<Expected>[0-9]+)$'
        if ($cleanRateLimitShutdown) {
            $completedCaptures = [int] $Matches.Completed
            $reportedExpectedCaptures = [int] $Matches.Expected
            $cleanRateLimitShutdown =
                $reportedExpectedCaptures -eq $expectedCaptureCount -and
                $completedCaptures -ge 0 -and
                $completedCaptures -lt $expectedCaptureCount
        }
        $retryable =
            $failureMessage.Contains(
                'PROVIDER_RATE_LIMIT_RETRYABLE',
                [StringComparison]::Ordinal) -and
            $rateLimitCount -ge $RateLimitAbortThreshold -and
            $cleanRateLimitShutdown
        $logPin = if ([IO.File]::Exists($attemptLog)) {
            Get-FileIdentity -Path $attemptLog
        }
        else { $null }
        $attempts.Add([pscustomobject] [ordered] @{
            Attempt = $attempt
            RunToken = $attemptToken
            StartedUtc = $startedUtc.ToString('o')
            EndedUtc = [DateTime]::UtcNow.ToString('o')
            Outcome = 'FAIL_CLOSED'
            Retryable = $retryable
            CleanGracefulRateLimitShutdown = $cleanRateLimitShutdown
            FailureCode = if ($retryable) {
                'EXPLICIT_TILE_HTTP_429_THRESHOLD'
            }
            else { 'NON_RETRYABLE_STRICT_LEAF_FAILURE' }
            ObservedHttp429Events = $rateLimitCount
            Log = $logPin
            EvidenceManifestPublished = $false
        })
        Assert-RetryBoundary -Checkpoint "after-failed-attempt-$attempt"
        if (-not $retryable) {
            throw "ProviderReady retry stopped after non-retryable strict leaf failure on attempt $attempt`: $failureMessage"
        }
        if ($attempt -eq $MaximumAttempts) {
            throw "ProviderReady retry exhausted $MaximumAttempts attempts after explicit HTTP 429 rate limiting; no retry receipt or proof manifest was published."
        }
    }

    $actualCooldownSeconds = Wait-ExactCooldown `
        -Seconds $CooldownSeconds -CompletedAttempt $attempt
    $attempts[$attempts.Count - 1] |
        Add-Member -NotePropertyName ActualCooldownSeconds `
            -NotePropertyValue $actualCooldownSeconds
    Assert-RetryBoundary -Checkpoint "after-cooldown-attempt-$attempt"
}

if ($null -eq $successfulLeaf -or $null -eq $successfulManifest) {
    throw 'ProviderReady retry ended without one accepted strict leaf result.'
}

Assert-RetryBoundary -Checkpoint 'before-retry-receipt'
$protectedUE54After = Assert-ProtectedUE54Unchanged `
    -Before $script:protectedUE54Before

$result = [pscustomobject] [ordered] @{
    Schema =
        'triad.istana_explore_v5d.vegetation_provider.ready_retry.v1'
    Status = 'PASS'
    RunToken = $RunToken
    ProviderEvidenceMode = 'ProviderReady'
    LandmarkVegetationR27Required = [bool] $RequireLandmarkVegetationR27
    ExpectedCaptureCount = $expectedCaptureCount
    MaximumAttempts = $MaximumAttempts
    AttemptsUsed = $attempts.Count
    CooldownSeconds = $CooldownSeconds
    RateLimitAbortThreshold = $RateLimitAbortThreshold
    RetryTrigger = 'EXPLICIT_TILE_HTTP_429_ONLY'
    ReadyThresholdPercent = 98.0
    RequiredConsecutiveReadySamples = 3
    ProviderTimeoutSeconds = $ProviderTimeoutSeconds
    AttemptIsolation = 'FRESH_WHOLE_HELPER_PROCESS_AND_UNIQUE_RUN_TOKEN'
    PinRevalidation = 'BEFORE_EVERY_ATTEMPT_AND_EVERY_COOLDOWN_SLICE'
    PersistentCachePolicy =
        'PRESERVE_CESIUM_STANDARD_PERSISTENT_HTTP_CACHE_NO_CLEAR_NO_RELOCATION'
    PersistentCacheHitClaimed = $false
    PartialEvidencePublicationAllowed = $false
    ProtectedUE54Policy =
        'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
    ProtectedUE54Before = $script:protectedUE54Before
    ProtectedUE54After = $protectedUE54After
    OrchestratorPin = $script:selfPin
    StrictLeafWrapperPin = $script:captureWrapperPin
    MapPin = $script:mapPin
    RuntimeDllPin = $script:runtimeDllPin
    EditorDllPin = $script:editorDllPin
    R27CommitReceipt = if ($RequireLandmarkVegetationR27) {
        [pscustomobject] [ordered] @{
            Pin = $script:r27CommitReceiptEvidence.Pin
            TransactionRunToken = $script:r27CommitReceiptEvidence.RunToken
            Schema = [string] $script:r27CommitReceiptEvidence.Receipt.Schema
            Status = [string] $script:r27CommitReceiptEvidence.Receipt.Status
        }
    }
    else { $null }
    Attempts = @($attempts)
    SuccessfulEvidenceManifest = $successfulManifest
}
$json = $result | ConvertTo-Json -Depth 14
$temporaryReceipt = $receiptPath + '.tmp.' +
    [Guid]::NewGuid().ToString('N')
try {
    $stream = [IO.File]::Open(
        $temporaryReceipt, [IO.FileMode]::CreateNew,
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
    [IO.File]::Move($temporaryReceipt, $receiptPath)
}
finally {
    if ([IO.File]::Exists($temporaryReceipt)) {
        [IO.File]::Delete($temporaryReceipt)
    }
}
$receiptPin = Get-FileIdentity -Path $receiptPath
[pscustomobject] [ordered] @{
    Status = 'PASS'
    RetryReceipt = $receiptPin
    SuccessfulEvidenceManifest = $successfulManifest
    AttemptsUsed = $attempts.Count
    ExpectedCaptureCount = $expectedCaptureCount
    LandmarkVegetationR27Required = [bool] $RequireLandmarkVegetationR27
    ProviderReadyThresholdPercent = 98.0
    RequiredConsecutiveReadySamples = 3
} | ConvertTo-Json -Depth 8
