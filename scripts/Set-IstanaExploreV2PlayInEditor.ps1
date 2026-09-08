[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [switch] $Stop,

    [ValidateRange(10, 300)]
    [int] $ReadinessWaitSeconds = 180,

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$v1Library = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$v2Library = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV2EditorLibrary'
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

if ($RemoteControlUrl.AbsoluteUri -cne 'http://127.0.0.1:30010/') {
    throw 'Explore V2 PIE requires exact loopback Remote Control http://127.0.0.1:30010.'
}
$resolved = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
    Split-Path -Parent $resolved
} else { $resolved }
$projectDirectory = [IO.Path]::GetFullPath($projectDirectory).TrimEnd('\')
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
$exploreMap = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Explore_v2.umap'
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf) -or
    -not (Test-Path -LiteralPath $exploreMap -PathType Leaf)) {
    throw 'The exact TRIAD project or built Explore V2 map is missing.'
}
$editors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
if ($editors.Count -ne 1 -or
    [string]::IsNullOrWhiteSpace([string] $editors[0].CommandLine) -or
    ([string] $editors[0].CommandLine) -notmatch [regex]::Escape($projectFile)) {
    throw 'Explore V2 PIE requires exactly one Unreal Editor on the requested TRIAD project.'
}

$identity = Invoke-RemoteObjectCall `
    -ObjectPath $v1Library `
    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory }
if ($identity.ReturnValue -ne $true) {
    throw "Explore V2 PIE project identity failed: $($identity.OutReport)"
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
        -ObjectPath $v2Library `
        -FunctionName 'QuiesceIstanaExploreV2PlayWorldForStop'
    if ($quiesce.ReturnValue -ne $true) {
        throw "Explore V2 PIE refused scripted stop: $($quiesce.OutMessage)"
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
    if ($inPlay) { throw 'Explore V2 PIE did not stop within 30 seconds.' }
    return [PSCustomObject]@{
        Operation = 'EditorRequestEndPlay'
        PlayInEditor = $false
        StopWasNecessary = $true
        Quiescence = [string] $quiesce.OutMessage
    }
}

$mapValidation = Invoke-RemoteObjectCall `
    -ObjectPath $v2Library `
    -FunctionName 'ValidateIstanaExploreV2Map'
if ($mapValidation.ReturnValue -ne $true) {
    throw "Explore V2 PIE requires the exact open V2 map: $($mapValidation.OutReport)"
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
        -ObjectPath $v2Library `
        -FunctionName 'ValidateIstanaExploreV2PlayWorld'
    if ($last.ReturnValue -eq $true) { ++$stable } else { $stable = 0 }
} while ($stable -lt 3 -and [DateTime]::UtcNow -lt $deadline)
if ($stable -lt 3) {
    throw "Explore V2 PIE readiness did not remain valid for three polls: $($last.OutReport)"
}

[PSCustomObject]@{
    Operation = if ($inPlay) { 'ValidateExistingExploreV2PlaySession' } else { 'EditorRequestBeginPlay' }
    DestinationMap = '/Game/Maps/Istana_PublicView_Explore_v2'
    PlayInEditor = $true
    StableReadinessPolls = $stable
    Report = [string] $last.OutReport
    Controls = 'Click viewport; WASD move; mouse look; E/Q up/down; Shift boost; Ctrl precision; Escape stops PIE; Shift+F1 releases mouse.'
}
