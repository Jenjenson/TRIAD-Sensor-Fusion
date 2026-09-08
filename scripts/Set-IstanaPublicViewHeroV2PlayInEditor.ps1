[CmdletBinding()]
param(
    [switch] $Stop,

    [ValidateRange(10, 900)]
    [int] $ReadinessWaitSeconds = 180,

    [ValidateRange(5, 60)]
    [int] $QuiesceDrainSeconds = 15,

    [string] $ProjectPath = 'D:\triad\TRIAD',

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$heroLibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV2EditorLibrary'
$runtimeLibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewEditorLibrary'
$levelEditorObjectPath = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$destinationMap = '/Game/Maps/Istana_PublicView_Exterior_v2'

function Invoke-RemoteObjectCall {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ObjectPath,

        [Parameter(Mandatory = $true)]
        [string] $FunctionName,

        [hashtable] $Parameters = @{},

        [ValidateRange(1, 900)]
        [int] $TimeoutSeconds = 30
    )

    $body = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 6
    return Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec $TimeoutSeconds
}

$identity = Invoke-RemoteObjectCall `
    -ObjectPath $heroLibraryObjectPath `
    -FunctionName 'ValidateIstanaPublicViewHeroV2RemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory }
if ($identity.ReturnValue -ne $true) {
    throw "Hero-v2 Remote Control project verification failed: $($identity.OutReport)"
}

$initialState = Invoke-RemoteObjectCall `
    -ObjectPath $levelEditorObjectPath `
    -FunctionName 'IsInPlayInEditor' `
    -TimeoutSeconds 10
$initiallyInPlay = [bool] $initialState.ReturnValue

if ($Stop) {
    if (-not $initiallyInPlay) {
        return [PSCustomObject]@{
            Operation = 'EditorRequestEndPlay'
            DestinationMap = $destinationMap
            PlayInEditor = $false
            ProjectIdentityVerified = $true
            StopWasNecessary = $false
        }
    }

    $quiesce = Invoke-RemoteObjectCall `
        -ObjectPath $runtimeLibraryObjectPath `
        -FunctionName 'QuiesceIstanaPublicViewHeroV2PlayWorldForStop' `
        -TimeoutSeconds 30
    if ($quiesce.ReturnValue -ne $true) {
        throw "Refusing unsafe EditorRequestEndPlay because hero-v2 AirSim quiescence was not verified. PIE was intentionally left running. $($quiesce.OutMessage)"
    }

    # The verified pause prevents new AirSim sensor work while this bounded
    # drain lets an already-running update release its Unreal actor pointers.
    Start-Sleep -Seconds $QuiesceDrainSeconds

    Invoke-RemoteObjectCall `
        -ObjectPath $levelEditorObjectPath `
        -FunctionName 'EditorRequestEndPlay' `
        -TimeoutSeconds 60 | Out-Null

    $stopDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $inPlay = $true
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RemoteObjectCall `
            -ObjectPath $levelEditorObjectPath `
            -FunctionName 'IsInPlayInEditor' `
            -TimeoutSeconds 10
        $inPlay = [bool] $state.ReturnValue
    } while ($inPlay -and [DateTime]::UtcNow -lt $stopDeadline)
    if ($inPlay) {
        throw 'EditorRequestEndPlay did not stop hero-v2 public-view PIE within 30 seconds.'
    }

    return [PSCustomObject]@{
        Operation = 'EditorRequestEndPlay'
        DestinationMap = $destinationMap
        PlayInEditor = $false
        ProjectIdentityVerified = $true
        StopWasNecessary = $true
        AirSimQuiescenceVerified = $true
        QuiesceDrainSeconds = $QuiesceDrainSeconds
        QuiesceReport = [string] $quiesce.OutMessage
    }
}

$mapValidation = Invoke-RemoteObjectCall `
    -ObjectPath $heroLibraryObjectPath `
    -FunctionName 'ValidateIstanaPublicViewHeroV2Map' `
    -TimeoutSeconds 900
if ($mapValidation.ReturnValue -ne $true) {
    throw "PIE start requires the exact validated hero-v2 map with v1 surroundings preserved: $($mapValidation.OutReport)"
}

$sessionWasAlreadyRunning = $initiallyInPlay
if (-not $initiallyInPlay) {
    Invoke-RemoteObjectCall `
        -ObjectPath $levelEditorObjectPath `
        -FunctionName 'EditorRequestBeginPlay' `
        -TimeoutSeconds 60 | Out-Null

    $startDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $inPlay = $false
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RemoteObjectCall `
            -ObjectPath $levelEditorObjectPath `
            -FunctionName 'IsInPlayInEditor' `
            -TimeoutSeconds 10
        $inPlay = [bool] $state.ReturnValue
    } while (-not $inPlay -and [DateTime]::UtcNow -lt $startDeadline)
    if (-not $inPlay) {
        throw 'EditorRequestBeginPlay did not enter hero-v2 public-view PIE within 30 seconds.'
    }
}

$requiredStableReadinessPolls = 3
$stableReadinessPolls = 0
$readinessDeadline = [DateTime]::UtcNow.AddSeconds($ReadinessWaitSeconds)
$playReadiness = $null
$readinessPollError = $null
do {
    Start-Sleep -Seconds 1
    try {
        $playReadiness = Invoke-RemoteObjectCall `
            -ObjectPath $runtimeLibraryObjectPath `
            -FunctionName 'ValidateIstanaPublicViewHeroV2PlayWorldReadiness' `
            -TimeoutSeconds 60
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
} while ($stableReadinessPolls -lt $requiredStableReadinessPolls -and
    [DateTime]::UtcNow -lt $readinessDeadline)

if (-not $playReadiness -or $playReadiness.ReturnValue -ne $true -or
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
    throw "Hero-v2 PIE readiness did not pass for $requiredStableReadinessPolls consecutive polls within $ReadinessWaitSeconds seconds. PIE was intentionally left running; retry readiness or use this script with -Stop for guarded teardown. Last report: $lastReport"
}

[PSCustomObject]@{
    Operation = if ($sessionWasAlreadyRunning) {
        'ValidateExistingHeroV2PublicViewPlaySession'
    }
    else {
        'EditorRequestBeginPlay'
    }
    DestinationMap = $destinationMap
    PlayInEditor = $true
    SessionWasAlreadyRunning = $sessionWasAlreadyRunning
    ProjectIdentityVerified = $true
    EditorMapValidation = 'ValidateIstanaPublicViewHeroV2Map'
    PlayWorldReadiness = 'ValidateIstanaPublicViewHeroV2PlayWorldReadiness'
    StableReadinessPolls = $stableReadinessPolls
    RequiredStableReadinessPolls = $requiredStableReadinessPolls
    ReadinessWaitSeconds = $ReadinessWaitSeconds
    Report = [string] $playReadiness.OutReport
}
