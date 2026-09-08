[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [switch] $Stop,

    [ValidateRange(10, 300)]
    [int] $ReadinessWaitSeconds = 120,

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$exploreLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$levelEditor = '/Script/LevelEditor.Default__LevelEditorSubsystem'

function Invoke-RemoteObjectCall {
    param(
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [int] $TimeoutSeconds = 60
    )
    $body = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 6
    return Invoke-RestMethod `
        -Uri ([uri]::new($RemoteControlUrl, '/remote/object/call')) `
        -Method Put -ContentType 'application/json' -Body $body -TimeoutSec $TimeoutSeconds
}

if ($RemoteControlUrl.Scheme -cne 'http' -or
    $RemoteControlUrl.Host -cne '127.0.0.1' -or
    $RemoteControlUrl.Port -ne 30010 -or
    $RemoteControlUrl.AbsolutePath -cne '/' -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Query) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Fragment) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.UserInfo)) {
    throw 'Explore PIE requires exact loopback Remote Control http://127.0.0.1:30010.'
}

$resolved = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
    Split-Path -Parent $resolved
}
else { $resolved }
$projectDirectory = [IO.Path]::GetFullPath($projectDirectory).TrimEnd('\')
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
$exploreMap = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Explore_v1.umap'
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf) -or
    -not (Test-Path -LiteralPath $exploreMap -PathType Leaf)) {
    throw 'The exact TRIAD project or built Explore map is missing.'
}
$editors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
if ($editors.Count -ne 1 -or
    [string]::IsNullOrWhiteSpace([string] $editors[0].CommandLine) -or
    ([string] $editors[0].CommandLine) -notmatch [regex]::Escape($projectFile)) {
    throw 'Explore PIE requires exactly one Unreal Editor on the requested TRIAD project.'
}

$identity = Invoke-RemoteObjectCall `
    -ObjectPath $exploreLibrary `
    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory }
if ($identity.ReturnValue -ne $true) {
    throw "Explore PIE project identity failed: $($identity.OutReport)"
}
$state = Invoke-RemoteObjectCall `
    -ObjectPath $levelEditor `
    -FunctionName 'IsInPlayInEditor' `
    -TimeoutSeconds 10
$inPlay = [bool] $state.ReturnValue

if ($Stop) {
    if (-not $inPlay) {
        return [PSCustomObject]@{
            Operation = 'EditorRequestEndPlay'
            PlayInEditor = $false
            StopWasNecessary = $false
        }
    }
    $quiesce = Invoke-RemoteObjectCall `
        -ObjectPath $exploreLibrary `
        -FunctionName 'QuiesceIstanaExploreV1PlayWorldForStop'
    if ($quiesce.ReturnValue -ne $true) {
        throw "Explore PIE refused scripted stop: $($quiesce.OutMessage)"
    }
    Invoke-RemoteObjectCall `
        -ObjectPath $levelEditor `
        -FunctionName 'EditorRequestEndPlay' | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RemoteObjectCall `
            -ObjectPath $levelEditor `
            -FunctionName 'IsInPlayInEditor' `
            -TimeoutSeconds 10
        $inPlay = [bool] $state.ReturnValue
    } while ($inPlay -and [DateTime]::UtcNow -lt $deadline)
    if ($inPlay) { throw 'Explore PIE did not stop within 30 seconds.' }
    return [PSCustomObject]@{
        Operation = 'EditorRequestEndPlay'
        PlayInEditor = $false
        StopWasNecessary = $true
        Quiescence = [string] $quiesce.OutMessage
    }
}

$mapValidation = Invoke-RemoteObjectCall `
    -ObjectPath $exploreLibrary `
    -FunctionName 'ValidateIstanaExploreV1Map'
if ($mapValidation.ReturnValue -ne $true) {
    throw "Explore PIE requires the exact open Explore map: $($mapValidation.OutReport)"
}
if (-not $inPlay) {
    Invoke-RemoteObjectCall `
        -ObjectPath $levelEditor `
        -FunctionName 'EditorRequestBeginPlay' | Out-Null
}

$stable = 0
$last = $null
$deadline = [DateTime]::UtcNow.AddSeconds($ReadinessWaitSeconds)
do {
    Start-Sleep -Seconds 1
    $last = Invoke-RemoteObjectCall `
        -ObjectPath $exploreLibrary `
        -FunctionName 'ValidateIstanaExploreV1PlayWorld'
    if ($last.ReturnValue -eq $true) { ++$stable } else { $stable = 0 }
} while ($stable -lt 3 -and [DateTime]::UtcNow -lt $deadline)
if ($stable -lt 3) {
    throw "Explore PIE readiness did not remain valid for three polls: $($last.OutReport)"
}

[PSCustomObject]@{
    Operation = if ($inPlay) { 'ValidateExistingExplorePlaySession' } else { 'EditorRequestBeginPlay' }
    DestinationMap = '/Game/Maps/Istana_PublicView_Explore_v1'
    PlayInEditor = $true
    StableReadinessPolls = $stable
    Report = [string] $last.OutReport
    Controls = 'Click viewport; WASD move; mouse look; E/Q up/down; Shift boost; Ctrl precision; Escape stops PIE; Shift+F1 releases mouse.'
}
