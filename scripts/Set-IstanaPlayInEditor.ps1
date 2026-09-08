[CmdletBinding()]
param(
    [switch] $Stop,
    [switch] $VisualAcceptanceOnly,
    [ValidateRange(5, 1800)]
    [int] $ReadinessWaitSeconds = 600,
    [ValidateRange(5, 60)]
    [int] $QuiesceDrainSeconds = 15,
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$libraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}

$identityBody = @{
    objectPath = $libraryObjectPath
    functionName = 'ValidateIstanaRemoteControlProject'
    parameters = @{ ExpectedProjectPath = $projectDirectory }
} | ConvertTo-Json -Depth 5
$identity = Invoke-RestMethod -Uri $endpoint -Method Put -ContentType 'application/json' -Body $identityBody -TimeoutSec 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project verification failed: $($identity.OutReport)"
}

if ($VisualAcceptanceOnly) {
    $matchingVisualEditors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" |
        Where-Object {
            -not [string]::IsNullOrWhiteSpace([string] $_.CommandLine) -and
            $_.CommandLine.IndexOf(
                $projectDirectory,
                [StringComparison]::OrdinalIgnoreCase) -ge 0 -and
            $_.CommandLine -match '(?i)(?:^|\s)-TRIADIstanaVisualAcceptance(?:\s|$)' -and
            $_.CommandLine -match '(?i)-settings=.*IstanaVisualAcceptance\.settings\.json'
        })
    if ($matchingVisualEditors.Count -ne 1) {
        throw 'VisualAcceptanceOnly requires exactly one connected editor launched by Start-IstanaVisualAcceptanceEditor.ps1 with the isolated settings file and runtime flag.'
    }
}

$objectPath = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$stateBody = @{
    objectPath = $objectPath
    functionName = 'IsInPlayInEditor'
    parameters = @{}
} | ConvertTo-Json -Depth 5
$initialState = Invoke-RestMethod `
    -Uri $endpoint `
    -Method Put `
    -ContentType 'application/json' `
    -Body $stateBody `
    -TimeoutSec 10
$initiallyInPlay = [bool] $initialState.ReturnValue

if ($Stop) {
    # Never strand PIE behind a malformed editor-map validator. Identity and
    # actual current PIE state are the only stop preconditions.
    if (-not $initiallyInPlay) {
        return [PSCustomObject]@{
            Operation = 'EditorRequestEndPlay'
            PlayInEditor = $false
            ProjectIdentityVerified = $true
            StopWasNecessary = $false
        }
    }

    $quiesceBody = @{
        objectPath = $libraryObjectPath
        functionName = 'QuiesceIstanaPlayWorldForStop'
        parameters = @{}
    } | ConvertTo-Json -Depth 5
    $quiesceVerified = $false
    $quiesceReport = 'Quiescence call was not completed.'
    try {
        $quiesce = Invoke-RestMethod `
            -Uri $endpoint `
            -Method Put `
            -ContentType 'application/json' `
            -Body $quiesceBody `
            -TimeoutSec 30
        $quiesceVerified = $quiesce.ReturnValue -eq $true
        $quiesceReport = [string] $quiesce.OutMessage
    }
    catch {
        $quiesceReport = "Quiescence request failed: $($_.Exception.Message)"
    }

    if (-not $quiesceVerified) {
        throw "Refusing unsafe EditorRequestEndPlay because AirSim quiescence was not verified. PIE was intentionally left running; do not use the toolbar Stop with an enabled lidar profile. Restart later with the VISUAL_ACCEPTANCE_ONLY no-lidar profile, or apply an upstream AirSim stop/join fix. Report: $quiesceReport"
    }

    # AirSim's lidar uses ParallelFor with raw Unreal actor pointers. Pausing
    # prevents new physics/sensor iterations; this bounded interval lets an
    # already-running scan drain before actors begin EndPlay.
    Start-Sleep -Seconds $QuiesceDrainSeconds

    $stopBody = @{
        objectPath = $objectPath
        functionName = 'EditorRequestEndPlay'
        parameters = @{}
    } | ConvertTo-Json -Depth 5
    Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $stopBody `
        -TimeoutSec 60 | Out-Null

    $stopDeadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RestMethod `
            -Uri $endpoint `
            -Method Put `
            -ContentType 'application/json' `
            -Body $stateBody `
            -TimeoutSec 10
        $inPlay = [bool] $state.ReturnValue
    } while ($inPlay -and [DateTime]::UtcNow -lt $stopDeadline)
    if ($inPlay) {
        throw 'EditorRequestEndPlay did not stop PIE within 30 seconds.'
    }

    return [PSCustomObject]@{
        Operation = 'EditorRequestEndPlay'
        PlayInEditor = $false
        ProjectIdentityVerified = $true
        StopWasNecessary = $true
        AirSimQuiescenceVerified = $quiesceVerified
        QuiesceDrainSeconds = $QuiesceDrainSeconds
        QuiesceReport = $quiesceReport
    }
}

# Starting uses only the persisted editor-world structural contract. Cesium
# readiness must be proved in the rebased PlayWorld after AirSim initializes.
$mapValidationFunction = 'ValidateIstanaRuntimeMapV2'
$mapBody = @{
    objectPath = $libraryObjectPath
    functionName = $mapValidationFunction
    parameters = @{}
} | ConvertTo-Json -Depth 5
$mapValidation = Invoke-RestMethod `
    -Uri $endpoint `
    -Method Put `
    -ContentType 'application/json' `
    -Body $mapBody `
    -TimeoutSec 60
if ($mapValidation.ReturnValue -ne $true) {
    throw "PIE start requires the structurally valid loaded v2 map: $($mapValidation.OutReport)"
}

$sessionWasAlreadyRunning = $initiallyInPlay
if (-not $initiallyInPlay) {
    $startBody = @{
        objectPath = $objectPath
        functionName = 'EditorRequestBeginPlay'
        parameters = @{}
    } | ConvertTo-Json -Depth 5
    Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $startBody `
        -TimeoutSec 60 | Out-Null

    $startDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $inPlay = $false
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RestMethod `
            -Uri $endpoint `
            -Method Put `
            -ContentType 'application/json' `
            -Body $stateBody `
            -TimeoutSec 10
        $inPlay = [bool] $state.ReturnValue
    } while (-not $inPlay -and [DateTime]::UtcNow -lt $startDeadline)
    if (-not $inPlay) {
        throw 'EditorRequestBeginPlay did not enter PIE within 30 seconds.'
    }
}

$playReadinessFunction = if ($VisualAcceptanceOnly) {
    'ValidateIstanaVisualAcceptancePlayWorldReadiness'
}
else {
    'ValidateIstanaPlayWorldReadiness'
}
$requiredStableReadinessPolls = if ($VisualAcceptanceOnly) { 3 } else { 1 }
$stableReadinessPolls = 0
$playReadinessBody = @{
    objectPath = $libraryObjectPath
    functionName = $playReadinessFunction
    parameters = @{}
} | ConvertTo-Json -Depth 5
$readinessDeadline = [DateTime]::UtcNow.AddSeconds($ReadinessWaitSeconds)
$playReadiness = $null
$readinessPollError = $null
do {
    Start-Sleep -Seconds 1
    try {
        $playReadiness = Invoke-RestMethod `
            -Uri $endpoint `
            -Method Put `
            -ContentType 'application/json' `
            -Body $playReadinessBody `
            -TimeoutSec 30
        if ($playReadiness.ReturnValue -eq $true) {
            ++$stableReadinessPolls
        }
        else {
            $stableReadinessPolls = 0
        }
    }
    catch {
        $readinessPollError = $_.Exception.Message
        break
    }
} while ($playReadiness -and
    $stableReadinessPolls -lt $requiredStableReadinessPolls -and
    [DateTime]::UtcNow -lt $readinessDeadline)

if (-not $playReadiness -or
    $playReadiness.ReturnValue -ne $true -or
    $stableReadinessPolls -lt $requiredStableReadinessPolls) {
    $lastReport = if ($readinessPollError) {
        "Remote Control poll failed: $readinessPollError"
    }
    elseif ($playReadiness) {
        [string] $playReadiness.OutReport
    }
    else {
        'No readiness response was returned.'
    }
    throw "Istana PlayWorld readiness '$playReadinessFunction' did not pass for $requiredStableReadinessPolls consecutive poll(s) within $ReadinessWaitSeconds seconds. PIE was intentionally left running so Cesium can continue streaming and to avoid an unsafe AirSim lidar teardown. Re-run this command to poll the same session, or use -Stop for quiesced explicit teardown. Last report: $lastReport"
}

[PSCustomObject]@{
    Operation = if ($sessionWasAlreadyRunning) {
        'ValidateExistingPlaySession'
    }
    else {
        'EditorRequestBeginPlay'
    }
    PlayInEditor = $true
    SessionWasAlreadyRunning = $sessionWasAlreadyRunning
    ProjectIdentityVerified = $true
    EditorMapValidation = $mapValidationFunction
    PlayWorldReadiness = $playReadinessFunction
    VisualAcceptanceOnly = [bool] $VisualAcceptanceOnly
    StableReadinessPolls = $stableReadinessPolls
    RequiredStableReadinessPolls = $requiredStableReadinessPolls
    ReadinessWaitSeconds = $ReadinessWaitSeconds
    Report = $playReadiness.OutReport
}
