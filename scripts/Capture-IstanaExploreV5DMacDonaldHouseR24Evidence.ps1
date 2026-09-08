#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$')]
    [string] $RunToken,

    # Supply these from the selected committed successor receipt (currently
    # R25). They are deliberately not inferred because the map is the mutable
    # transaction product whose exact identity must be selected by the caller.
    [int64] $ExpectedMapBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedMapSha256 = '',

    [ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]
    [string] $ProviderEvidenceMode = 'TelemetryOnly',

    [ValidateRange(6, 120)]
    [int] $TelemetryDwellSeconds = 12,

    [int64] $ExpectedRuntimeDllBytes = 4275200,

    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedRuntimeDllSha256 =
        '47C66266773E6208313FA1CB51C20D1E335C6A5A841C4E137B415C4879148614',

    [int64] $ExpectedEditorDllBytes = 7135232,

    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedEditorDllSha256 =
        '0B7C337BF7949C650DD22B233AB5F4D4F925E2A3C91230429E66A757A31D6F77',

    [switch] $StaticSelfCheck,

    [ValidateRange(60, 900)]
    [int] $EditorTimeoutSeconds = 600,

    [ValidateRange(60, 300)]
    [int] $PieTimeoutSeconds = 120,

    [ValidateRange(60, 3600)]
    [int] $ProviderTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $CaptureTimeoutSeconds = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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
$ddcRoot = 'D:\triad\TRIAD\Saved\DerivedDataCache'
$outputRoot = 'D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D'
$logRoot = 'D:\triad\TRIAD\Saved\Logs'
$logFile = Join-Path $logRoot "Codex_V5D_R24_MacDonald_${RunToken}.log"
$evidenceManifestPath = Join-Path $outputRoot `
    "explore_v5d_r24_macdonald_evidence_${RunToken}.json"
$rcCallUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$v5dLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$levelEditor = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$strictFallbackCaptureAcknowledgementMarker =
    'localFallbackVisible=true localFallbackHidden=false'

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
$expectedPlacementBytes = 4192L
$expectedPlacementSha256 =
    'E5B40D02A3D712303A2D99734A42E9ADD68F4E528DED31DD763F37034961ED81'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$placementPath = Join-Path $repoRoot `
    'unreal\SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse\macdonald_house_r24.unreal_placement.json'
$macAssetRoot =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\Surroundings\R24MacDonaldHouse'
$macAssetRelativeStems = @(
    'SM_IPV5D_R24_MacDonaldHouse_Render',
    'Materials\M_MD_BalconyConcrete_PBR_R24',
    'Materials\M_MD_DarkMetal_PBR_R24',
    'Materials\M_MD_DarkWindowGlass_PBR_R24',
    'Materials\M_MD_GreenGlazedRoofTile_PBR_R24',
    'Materials\M_MD_HeritagePlaque_PBR_R24',
    'Materials\M_MD_MarbleColumn_PBR_R24',
    'Materials\M_MD_RedSandFacedBrick_PBR_R24',
    'Materials\M_MD_ShadowRecess_PBR_R24',
    'Materials\M_MD_WhitePaintedFrame_PBR_R24'
)
$packageArtifactSuffixes = @('.uasset', '.uexp', '.ubulk', '.uptnl')

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

function Convert-R24LocalMetersToWorldCentimeters {
    param([Parameter(Mandatory = $true)] [double[]] $LocalMeters)

    if ($LocalMeters.Count -ne 3) {
        throw 'R24 pose derivation requires exactly three local coordinates.'
    }
    $actorYawRadians = -161.885822122792 * [Math]::PI / 180.0
    $localX = [double] $LocalMeters[0]
    $localY = [double] $LocalMeters[1]
    $localZ = [double] $LocalMeters[2]
    $scaledX = 100.0 * 1.4799104705961 * $localX
    $scaledY = 100.0 * 1.98130612769146 * $localY
    $scaledZ = 100.0 * 1.3 * $localZ
    $worldX = 36320.390052826 +
        [Math]::Cos($actorYawRadians) * $scaledX -
        [Math]::Sin($actorYawRadians) * $scaledY
    $worldY = 87155.365772797 +
        [Math]::Sin($actorYawRadians) * $scaledX +
        [Math]::Cos($actorYawRadians) * $scaledY
    [double[]] @($worldX, $worldY, $scaledZ)
}

function New-TargetLockedPose {
    param(
        [Parameter(Mandatory = $true)] [string] $Label,
        [Parameter(Mandatory = $true)] [double[]] $CameraLocalMeters,
        [Parameter(Mandatory = $true)] [double[]] $TargetLocalMeters,
        [Parameter(Mandatory = $true)] [string] $Rationale,
        [Parameter(Mandatory = $true)] [string] $OutputPrefix
    )

    $camera = [double[]] @(
        Convert-R24LocalMetersToWorldCentimeters `
            -LocalMeters $CameraLocalMeters)
    $target = [double[]] @(
        Convert-R24LocalMetersToWorldCentimeters `
            -LocalMeters $TargetLocalMeters)
    $cameraX = [double] $camera[0]
    $cameraY = [double] $camera[1]
    $cameraZ = [double] $camera[2]
    $targetX = [double] $target[0]
    $targetY = [double] $target[1]
    $targetZ = [double] $target[2]
    $dx = $targetX - $cameraX
    $dy = $targetY - $cameraY
    $dz = $targetZ - $cameraZ
    $horizontal = [Math]::Sqrt($dx * $dx + $dy * $dy)
    $distance = [Math]::Sqrt($horizontal * $horizontal + $dz * $dz)
    $yaw = [Math]::Atan2($dy, $dx) * 180.0 / [Math]::PI
    $pitch = [Math]::Atan2($dz, $horizontal) * 180.0 / [Math]::PI
    if ([Math]::Sqrt($cameraX * $cameraX + $cameraY * $cameraY) -gt
            120000.0 -or
        $cameraZ -lt 50.0 -or $cameraZ -gt 50000.0) {
        throw "Derived pose '$Label' is outside the native QA endpoint envelope."
    }
    [pscustomobject] [ordered] @{
        Label = $Label
        X = $cameraX
        Y = $cameraY
        Z = $cameraZ
        Pitch = [double] $pitch
        Yaw = [double] $yaw
        Roll = 0.0
        CameraLocalMeters = [double[]] $CameraLocalMeters
        TargetLocalMeters = [double[]] $TargetLocalMeters
        TargetWorldCentimeters = [double[]] $target
        TargetDistanceCentimeters = [double] $distance
        Rationale = $Rationale
        OutputFileName = "${OutputPrefix}_${Label}_${RunToken}.png"
    }
}

# The source facade faces local -Y. These three cameras therefore assess the
# interpreted public facade at about 25 m from its face, the facade/return/roof
# junction from a front corner, and the landmark's street-scale silhouette in
# provider context. They do not survey or reconstruct provider geometry.
$captureFilenamePrefix = if ($ProviderEvidenceMode -cne 'ProviderReady') {
    'explore_v5d_diagnostic_r24_macdonald'
}
else {
    'explore_v5d_r24_macdonald'
}
$evidenceModeClassification =
    if ($ProviderEvidenceMode -ceq 'ProviderReady') {
        'PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED'
    }
    elseif ($ProviderEvidenceMode -ceq 'ProviderFallback') {
        'STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF'
    }
    else { 'EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC' }
$facadeClosePose = New-TargetLockedPose -Label 'facade_close' `
    -CameraLocalMeters @(0.0, -23.0, 4.5) `
    -TargetLocalMeters @(0.0, -1.5, 13.0) `
    -Rationale 'Close public-facade reading of brick, repetitive white frames, dark glazing, entrance columns, balconies, plaque, and night-safe interpretation.' `
    -OutputPrefix $captureFilenamePrefix
$frontCornerObliquePose = New-TargetLockedPose `
    -Label 'front_corner_oblique' `
    -CameraLocalMeters @(-26.0, -32.0, 9.0) `
    -TargetLocalMeters @(0.0, -1.0, 14.5) `
    -Rationale 'Oblique reading of facade depth, balcony projection, facade return, roof ribs, and material transitions.' `
    -OutputPrefix $captureFilenamePrefix
$streetscapeContextPose = New-TargetLockedPose `
    -Label 'streetscape_context' `
    -CameraLocalMeters @(-65.0, -85.0, 30.0) `
    -TargetLocalMeters @(0.0, 0.0, 15.0) `
    -Rationale 'Wider public streetscape composition showing silhouette and scale against visual-only provider surroundings while leaving possible provider overlap disclosed.' `
    -OutputPrefix $captureFilenamePrefix
$poses = @(
    $facadeClosePose,
    $frontCornerObliquePose,
    $streetscapeContextPose
)

$macDonaldMapValidationMarkers = @(
    'cesiumGeoreference=(103.84288055,1.30709615,47.000)',
    'ionAssetId=2275207',
    'maximumSse=1.0',
    'applyDpiScaling=false',
    'forbidHoles=true',
    'loadingDescendantLimit=20',
    'authoredCoreClipShape=irregularEllipse64',
    'deterministicLocalSimulationLayersPreserved=true',
    'r24MacDonaldHouseCount=1',
    'r24MacDonaldDedicatedOverlayVisible=true',
    'r24TemasekShophouseCount=1',
    'r24TemasekDedicatedOverlayVisible=true',
    'r24VisibilityInvariantAcrossProviderTransitions=true',
    'r24ProviderStateTelemetryOnly=true',
    'r24CoarseLocalLandmarkShellsSuppressed=true',
    'r24SuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490',
    'r24ProviderAndDedicatedOverlayOverlapUnresolved=true',
    'r24DedicatedProviderExclusion=false',
    'r24ProviderReadyLiveSuccessor=false',
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
    'macDonaldHouse={ISTANA_EXPLORE_V5D_R24_MACDONALD_VALID'
)

$selfPin = Get-FileIdentity -Path $PSCommandPath
$placementPin = Get-FileIdentity -Path $placementPath
Assert-ExactPin -Pin $placementPin -Bytes $expectedPlacementBytes `
    -Sha256 $expectedPlacementSha256 `
    -Description 'R24 MacDonald House placement receipt'

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Status = 'STATIC_SELF_CHECK_PASS'
        Schema =
            'triad.istana_explore_v5d.r24_macdonald.visual_capture_static_check.v1'
        Script = $selfPin
        PlacementReceipt = $placementPin
        ProviderEvidenceMode = $ProviderEvidenceMode
        EvidenceClassification = $evidenceModeClassification
        TelemetryDwellSeconds = $TelemetryDwellSeconds
        PoseCount = $poses.Count
        Poses = $poses
        RuntimeContract = [pscustomobject] [ordered] @{
            MapValidationMarkers = $macDonaldMapValidationMarkers
            BoundaryAssetPaths = @(
                $suppressedFallbackMeshAsset,
                $outerGroundMeshAsset,
                $outerGroundMaterialAsset
            )
            StrictFallbackCaptureAcknowledgementMarker =
                $strictFallbackCaptureAcknowledgementMarker
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

if ($ExpectedMapBytes -le 0 -or
    $ExpectedMapSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live R24 visual evidence requires -ExpectedMapBytes and -ExpectedMapSha256 from the successful native-transaction successor receipt.'
}
if ($ExpectedRuntimeDllBytes -le 0 -or $ExpectedEditorDllBytes -le 0) {
    throw 'Live R24 visual evidence requires positive frozen runtime/editor DLL byte counts.'
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
            throw 'The CAPSTONE project token is owned by an unexpected process; refusing the R24 capture run.'
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
        throw 'Protected UE5.4/CAPSTONE identity set changed during the R24 capture run.'
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
    if (-not (Test-RcOwnership -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId))) {
        throw "RC port 30010 is not solely owned by the helper at '$Checkpoint'."
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
        $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
            -ObjectPath $v5dLibrary `
            -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
            -TimeoutSec 120
        $lastReport = [string] $state.OutReport
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

    $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $v5dLibrary `
        -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
        -TimeoutSec 120
    if (-not (Test-ExactPoseState -Response $state -Pose $Pose `
            -EvidenceMode $ProviderEvidenceMode)) {
        throw "Post-capture state lost the exact $ProviderEvidenceMode pose contract for $($Pose.Label): $($state.OutReport)"
    }
    $providerTelemetry = Get-ProviderTelemetrySample -Response $state
    $validation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $ExpectedProcess -ObjectPath $v5dLibrary `
        -FunctionName 'ValidateIstanaExploreV5DHybridPlayWorld' `
        -TextProperty 'OutReport' -TimeoutSec 180
    if (-not ([string] $validation.OutReport).StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID',
            [StringComparison]::Ordinal)) {
        throw "Unexpected post-capture V5D validation for $($Pose.Label): $($validation.OutReport)"
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
        -Checkpoint 'before RC QuitEditor'
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

function Get-ExactMacAssetPins {
    if (-not (Test-Path -LiteralPath $macAssetRoot -PathType Container)) {
        throw "MacDonald House native asset root is absent: $macAssetRoot"
    }
    $allowed = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($stem in $macAssetRelativeStems) {
        foreach ($suffix in $packageArtifactSuffixes) {
            [void] $allowed.Add([IO.Path]::GetFullPath(
                (Join-Path $macAssetRoot "$stem$suffix")))
        }
        $primary = Join-Path $macAssetRoot "$stem.uasset"
        if (-not [IO.File]::Exists([IO.Path]::GetFullPath($primary))) {
            throw "Required MacDonald House primary package is absent: $primary"
        }
    }
    foreach ($file in Get-ChildItem -LiteralPath $macAssetRoot -File -Recurse) {
        if (-not $allowed.Contains([IO.Path]::GetFullPath($file.FullName))) {
            throw "Unexpected file in the exact MacDonald House asset root: $($file.FullName)"
        }
    }
    @(
        foreach ($path in @($allowed | Sort-Object)) {
            Get-FileStateIdentity -Path $path
        }
    )
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
        throw "Refusing to overwrite an existing R24 evidence artifact: $path"
    }
}

$script:protectedUE54Before = Get-ProtectedUE54Identity
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    throw 'R24 capture requires UE5.5/native TRIAD helpers to be idle; the protected UE5.4/CAPSTONE set may remain open.'
}
if (@(Get-RcListeners).Count -ne 0) {
    throw 'R24 capture requires no existing RC port 30010 listener.'
}

$editorPin = Get-FileIdentity -Path $editor
$projectPin = Get-FileIdentity -Path $projectFile
$mapPin = Get-FileIdentity -Path $mapFile
$runtimeDllPin = Get-FileIdentity -Path $runtimeDll
$editorDllPin = Get-FileIdentity -Path $editorDll
$macAssetPins = @(Get-ExactMacAssetPins)
$suppressedFallbackMeshPin =
    Get-FileIdentity -Path $suppressedFallbackMeshAsset
$outerGroundMeshPin = Get-FileIdentity -Path $outerGroundMeshAsset
$outerGroundMaterialPin = Get-FileIdentity -Path $outerGroundMaterialAsset
Assert-ExactPin -Pin $projectPin -Bytes $expectedProjectBytes `
    -Sha256 $expectedProjectSha256 -Description 'TRIAD project file'
Assert-ExactPin -Pin $mapPin -Bytes $ExpectedMapBytes `
    -Sha256 $ExpectedMapSha256 -Description 'selected committed V5D map successor'
Assert-ExactPin -Pin $runtimeDllPin -Bytes $ExpectedRuntimeDllBytes `
    -Sha256 $ExpectedRuntimeDllSha256 -Description 'selected runtime DLL'
Assert-ExactPin -Pin $editorDllPin -Bytes $ExpectedEditorDllBytes `
    -Sha256 $ExpectedEditorDllSha256 -Description 'selected editor DLL'
$script:boundaryPins = @(
    $selfPin,
    $placementPin,
    $editorPin,
    $projectPin,
    $mapPin,
    $runtimeDllPin,
    $editorDllPin,
    $suppressedFallbackMeshPin,
    $outerGroundMeshPin,
    $outerGroundMaterialPin
) + $macAssetPins
Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'

$argumentLine = "`"$projectFile`" $mapPackage -DisablePlugin=AirSim -unattended -nop4 -NoSplash -NoAutoSave -RenderOffscreen -NoSound -asyncstaticmeshcompilation=0 -dx12 -sm6 -ResX=2560 -ResY=1440 -Windowed -ini:Engine:[HTTP]:HttpMaxConnectionsPerServer=12 -ini:Engine:[/Script/CesiumRuntime.CesiumRuntimeSettings]:MaxCacheItems=32768 -DDC=InstalledNoZenLocalFallback -LocalDataCachePath=`"$ddcRoot`" -RemoteControlHttpServer -RCWebControlEnable -ExecCmds=`"WebControl.StartServer`" -abslog=`"$logFile`""
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
$cleanupErrors = [Collections.Generic.List[string]]::new()
$postconditionErrors = [Collections.Generic.List[string]]::new()
$captureEvidence = [Collections.Generic.List[object]]::new()
$projectIdentityReport = ''
$mapValidationReport = ''
$initialPieValidationReport = ''

try {
    ${env:UE-LocalDataCachePath} = $ddcRoot
    $process = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden
    $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
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

    $mapValidation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
        -FunctionName 'ValidateIstanaExploreV5DHybridMap' `
        -TextProperty 'OutReport' -TimeoutSec 900
    $mapValidationReport = [string] $mapValidation.OutReport
    if (-not $mapValidationReport.StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID',
            [StringComparison]::Ordinal)) {
        throw "Unexpected V5D map validation: $mapValidationReport"
    }
    foreach ($marker in $macDonaldMapValidationMarkers) {
        if (-not $mapValidationReport.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "V5D map validation lacks the R24 evidence marker: $marker"
        }
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
            -FunctionName 'ValidateIstanaExploreV5DHybridPlayWorld' `
            -TimeoutSec 180
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
        throw "Initial V5D PIE validation failed: $($initialPieValidation.OutReport)"
    }
    $initialPieValidationReport = [string] $initialPieValidation.OutReport

    foreach ($pose in $poses) {
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint "before capture $($pose.Label)"
        $setPose = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName 'SetIstanaExploreV5DHybridPlayViewPoseForQa' `
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
                'EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_PASS:',
                [StringComparison]::Ordinal) -or
            -not $setPoseMessage.Contains(
                'exactQaViewPose=true', [StringComparison]::Ordinal)) {
            throw "Unexpected pose acknowledgement for $($pose.Label): $setPoseMessage"
        }

        $stateWindow = Wait-StableExactPoseState `
            -ExpectedProcess $launchedIdentity -Pose $pose `
            -TimeoutSeconds $ProviderTimeoutSeconds
        $preCaptureState = Invoke-OwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
            -TimeoutSec 120
        if (-not (Test-ExactPoseState -Response $preCaptureState `
                -Pose $pose -EvidenceMode $ProviderEvidenceMode)) {
            throw "The immediate pre-capture state lost the exact $ProviderEvidenceMode pose contract for $($pose.Label): $($preCaptureState.OutReport)"
        }
        $preCaptureProviderTelemetry =
            Get-ProviderTelemetrySample -Response $preCaptureState
        $captureRequestedUtc = [DateTime]::UtcNow
        $captureFunction = if ($ProviderEvidenceMode -cne 'ProviderReady') {
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
            if ($ProviderEvidenceMode -cne 'ProviderReady') {
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
        $png = Wait-StablePng `
            -Path (Join-Path $outputRoot ([string] $pose.OutputFileName)) `
            -NotBeforeUtc $captureRequestedUtc `
            -TimeoutSeconds $CaptureTimeoutSeconds
        $postCapture = Assert-PostCaptureWorld `
            -ExpectedProcess $launchedIdentity -Pose $pose
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
        try {
            $process.Refresh()
            if (-not $process.HasExited -and $pieMayBeActive) {
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
            if (-not $process.HasExited) {
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
    ${env:UE-LocalDataCachePath} = $oldLocalDdc
}

$protectedUE54After = $null
try {
    $protectedUE54After = Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before
}
catch {
    $postconditionErrors.Add($_.Exception.Message)
}
try {
    Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'final postflight'
    [void] (Get-ExactMacAssetPins)
}
catch {
    $postconditionErrors.Add($_.Exception.Message)
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
            throw "Two distinct R24 poses produced the same PNG hash: $($verified.Sha256)"
        }
    }
    catch {
        $postconditionErrors.Add($_.Exception.Message)
    }
}
if ($forcedContainment) {
    $postconditionErrors.Add(
        'The exact helper required forced containment; evidence cannot pass.')
}
if (-not $quitRequested) {
    $postconditionErrors.Add('Graceful QuitEditor was never requested.')
}
if ($exitCode -ne 0) {
    $postconditionErrors.Add(
        "UE5.5 helper exit code was not zero: $exitCode")
}

if ($null -ne $workflowError -or $cleanupErrors.Count -ne 0 -or
    $postconditionErrors.Count -ne 0) {
    $workflowText = if ($null -eq $workflowError) {
        'none'
    }
    else { $workflowError.Message }
    throw "R24 MacDonald House visual evidence failed: workflow=$workflowText cleanup=$([string]::Join(' | ', @($cleanupErrors))) postconditions=$([string]::Join(' | ', @($postconditionErrors)))"
}

$result = [pscustomobject] [ordered] @{
    Schema =
        'triad.istana_explore_v5d.r24_macdonald.visual_evidence.v1'
    Status = 'PASS'
    Classification = if ($ProviderEvidenceMode -ceq 'ProviderFallback') {
        'STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF'
    }
    else {
        'RASTER_VISUAL_QA_EVIDENCE_NOT_SURVEY_NOT_PROVIDER_GEOMETRY_PROOF'
    }
    EvidenceModeClassification = $evidenceModeClassification
    Operation =
        "V5D_R24_MACDONALD_CLOSE_OBLIQUE_CONTEXT_${ProviderEvidenceMode}_CAPTURE"
    RunToken = $RunToken
    ProviderEvidenceMode = $ProviderEvidenceMode
    TelemetryDwellSeconds = if ($ProviderEvidenceMode -cne 'ProviderReady') {
        $TelemetryDwellSeconds
    }
    else { 0 }
    PoseOrder = @($poses | ForEach-Object { $_.Label })
    ScriptPin = $selfPin
    PlacementReceiptPin = $placementPin
    ProjectIdentityReport = $projectIdentityReport
    MapValidationReport = $mapValidationReport
    InitialPieValidationReport = $initialPieValidationReport
    MapPin = $mapPin
    RuntimeDllPin = $runtimeDllPin
    EditorDllPin = $editorDllPin
    MacDonaldHouseAssetPins = $macAssetPins
    SuppressedFallbackMeshAssetPin = $suppressedFallbackMeshPin
    OuterGroundMeshAssetPin = $outerGroundMeshPin
    OuterGroundMaterialAssetPin = $outerGroundMaterialPin
    EditorIdentity = $launchedIdentity
    Captures = @($captureEvidence)
    ProviderHandling = [pscustomobject] [ordered] @{
        Usage = 'VISUAL_BACKGROUND_PIXELS_IN_RASTER_SCREENSHOTS_ONLY'
        CaptureFunction = if ($ProviderEvidenceMode -cne 'ProviderReady') {
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
        GlobalLoadProgressRecordedAsTelemetryOnly = $true
        LandmarkSpecificProviderReadinessClaimed = $false
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
    }
    EndPlayEvidence = $endPlayEvidence
    QuitConnectionResult = $quitConnectionResult
    GracefulQuitRequested = $quitRequested
    ForcedContainment = $forcedContainment
    EditorExitCode = $exitCode
    ProtectedUE54 = $script:protectedUE54Before
    ProtectedUE54Before = $script:protectedUE54Before
    ProtectedUE54After = $protectedUE54After
    FinalNonProtectedEditorCount = @(Get-NativeTRIADUnrealProcesses).Count
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
    ProviderGeometryExportedTracedAnalysedDerivedOrBaked = $false
    ProtectedUE54Unchanged = $true
} | ConvertTo-Json -Depth 8
