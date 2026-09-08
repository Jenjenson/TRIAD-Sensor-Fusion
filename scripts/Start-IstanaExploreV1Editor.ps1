[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [ValidateNotNullOrEmpty()]
    [string] $UnrealEditorPath = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe',

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010',

    [ValidateRange(30, 900)]
    [int] $ReadinessTimeoutSeconds = 300,

    [switch] $ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-RemoteObjectCall {
    param([string] $ObjectPath, [string] $FunctionName, [hashtable] $Parameters = @{})
    $body = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 6
    return Invoke-RestMethod `
        -Uri ([uri]::new($RemoteControlUrl, '/remote/object/call')) `
        -Method Put -ContentType 'application/json' -Body $body -TimeoutSec 900
}

if ($RemoteControlUrl.Scheme -cne 'http' -or
    $RemoteControlUrl.Host -cne '127.0.0.1' -or
    $RemoteControlUrl.Port -ne 30010 -or
    $RemoteControlUrl.AbsolutePath -cne '/' -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Query) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Fragment) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.UserInfo)) {
    throw 'Explore editor launch requires exact loopback Remote Control http://127.0.0.1:30010.'
}

$resolved = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
    if ([IO.Path]::GetExtension($resolved) -ine '.uproject') {
        throw "ProjectPath is not a .uproject: $resolved"
    }
    Split-Path -Parent $resolved
}
else { $resolved }
$projectDirectory = [IO.Path]::GetFullPath($projectDirectory).TrimEnd('\')
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
$v5Map = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v5.umap'
$exploreMap = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Explore_v1.umap'
$exploreHeader = Join-Path $projectDirectory 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreEditorLibrary.h'
foreach ($required in @($projectFile, $v5Map, $exploreHeader)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required Explore editor input is missing: $required"
    }
}
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor before the guarded Explore launch.'
}

$hasExploreMap = Test-Path -LiteralPath $exploreMap -PathType Leaf
$selectedMapPackage = if ($hasExploreMap) {
    '/Game/Maps/Istana_PublicView_Explore_v1'
}
else {
    '/Game/Maps/Istana_PublicView_Exterior_v5'
}
$exploreDdcRoot = Join-Path $projectDirectory 'Saved\IstanaExploreV1DerivedDataCache'
$exploreZenRoot = Join-Path $projectDirectory 'Saved\IstanaExploreV1ZenData'
$preflight = [PSCustomObject]@{
    Operation = 'StartIstanaExploreV1Editor'
    ValidateOnly = [bool] $ValidateOnly
    ProjectFile = $projectFile
    SelectedMap = $selectedMapPackage
    ExploreMapPresent = $hasExploreMap
    RemoteControlUrl = 'http://127.0.0.1:30010'
    LocalDerivedDataCache = $exploreDdcRoot
    ZenDataPath = $exploreZenRoot
}
if ($ValidateOnly) { return $preflight }
foreach ($cacheRoot in @($exploreDdcRoot, $exploreZenRoot)) {
    New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
}

$editor = (Resolve-Path -LiteralPath $UnrealEditorPath).Path
if ([IO.Path]::GetFileName($editor) -ine 'UnrealEditor.exe') {
    throw "UnrealEditorPath is not UnrealEditor.exe: $editor"
}
$arguments = @(
    "`"$projectFile`"",
    $selectedMapPackage,
    '-RCWebControlEnable',
    '-ExecCmds="WebControl.StartServer"',
    "-LocalDataCachePath=$exploreDdcRoot",
    "-ZenDataPath=$exploreZenRoot"
)
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Normal -PassThru
$infoEndpoint = [uri]::new($RemoteControlUrl, '/remote/info')
$deadline = [DateTime]::UtcNow.AddSeconds($ReadinessTimeoutSeconds)
$ready = $false
do {
    $process.Refresh()
    if ($process.HasExited) {
        throw "Unreal Editor exited before Explore Remote Control was ready (exit $($process.ExitCode))."
    }
    try {
        Invoke-RestMethod -Uri $infoEndpoint -Method Get -TimeoutSec 2 | Out-Null
        $ready = $true
    }
    catch { Start-Sleep -Seconds 1 }
} while (-not $ready -and [DateTime]::UtcNow -lt $deadline)
if (-not $ready) {
    throw 'Unreal Editor did not expose Explore Remote Control before the timeout.'
}

$exploreLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$identity = Invoke-RemoteObjectCall `
    -ObjectPath $exploreLibrary `
    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory }
if ($identity.ReturnValue -ne $true) {
    throw "Connected editor project identity failed: $($identity.OutReport)"
}
if ($hasExploreMap) {
    $validation = Invoke-RemoteObjectCall `
        -ObjectPath $exploreLibrary `
        -FunctionName 'ValidateIstanaExploreV1Map'
}
else {
    $validation = Invoke-RemoteObjectCall `
        -ObjectPath '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV5EditorLibrary' `
        -FunctionName 'ValidateIstanaPublicViewHeroV5Map'
}
if ($validation.ReturnValue -ne $true) {
    throw "The selected source/destination map failed validation: $($validation.OutReport)"
}

$preflight | Add-Member -NotePropertyName ProcessId -NotePropertyValue $process.Id
$preflight | Add-Member -NotePropertyName RemoteControlReady -NotePropertyValue $true
$preflight | Add-Member -NotePropertyName ProjectIdentityVerified -NotePropertyValue $true
$preflight | Add-Member -NotePropertyName CurrentMapValidated -NotePropertyValue $true
$preflight
