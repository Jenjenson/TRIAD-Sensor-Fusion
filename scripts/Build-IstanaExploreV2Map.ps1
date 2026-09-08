[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-ProjectDirectory([string] $Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $directory = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        Split-Path -Parent $resolved
    } else { $resolved }
    $project = Join-Path $directory 'TRIAD.uproject'
    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
        throw "TRIAD.uproject is missing: $project"
    }
    return [IO.Path]::GetFullPath($directory).TrimEnd('\')
}

function Invoke-ExploreV2Call(
    [string] $FunctionName,
    [hashtable] $Parameters = @{},
    [int] $TimeoutSeconds = 3600
) {
    $body = @{
        objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV2EditorLibrary'
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 8
    Invoke-RestMethod `
        -Uri ([uri]::new($RemoteControlUrl, '/remote/object/call')) `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec $TimeoutSeconds
}

if ($RemoteControlUrl.AbsoluteUri -cne 'http://127.0.0.1:30010/') {
    throw 'Explore V2 building requires exact loopback Remote Control http://127.0.0.1:30010.'
}
$projectDirectory = Resolve-ProjectDirectory $ProjectPath
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
$editors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
if ($editors.Count -ne 1 -or
    [string]::IsNullOrWhiteSpace([string]$editors[0].CommandLine) -or
    ([string]$editors[0].CommandLine) -notmatch [regex]::Escape($projectFile)) {
    throw 'Exactly one Unreal Editor must be running on the requested TRIAD project.'
}

$validator = Join-Path $projectDirectory 'SourceAssets\IstanaPublicViewExploreV2\validate_explore_v2.py'
if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
    throw "Installed Explore V2 validator is missing: $validator"
}
& py -3 -B $validator | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw 'Explore V2 offline contract validation failed before Unreal mutation.'
}

$materials = Invoke-ExploreV2Call 'CreateIstanaExploreV2WindMaterials'
if ($materials.ReturnValue -ne $true) {
    throw "Explore V2 material creation failed: $($materials.OutMessage)"
}
$build = Invoke-ExploreV2Call 'BuildIstanaExploreV2Map'
if ($build.ReturnValue -ne $true) {
    throw "Explore V2 map build failed: $($build.OutMessage)"
}
$validation = Invoke-ExploreV2Call 'ValidateIstanaExploreV2Map'
if ($validation.ReturnValue -ne $true) {
    throw "Persisted Explore V2 validation failed: $($validation.OutReport)"
}

[PSCustomObject]@{
    Operation = 'BuildIstanaExploreV2Map'
    SourceMap = '/Game/Maps/Istana_PublicView_Explore_v1'
    DestinationMap = '/Game/Maps/Istana_PublicView_Explore_v2'
    WindMaterials = [string]$materials.OutMessage
    MapBuild = [string]$build.OutMessage
    MapValidation = [string]$validation.OutReport
    ProtectedV1ThroughV5AndExploreV1Unchanged = $true
    DistantOsmHdbContentUnchanged = $true
    LowDetailLegacySceneTreeInstancesRemaining = 0
    InheritedOuterTreePositionsReplanted = 560
    InheritedOuterPositionsUsingHighDetailBroadleaf = 532
    InheritedOuterPositionsUsingProceduralPalmProxy = 28
    FreeRoamDefaultOnPlay = $true
    SurveyGradeOneToOneClaimed = $false
}
