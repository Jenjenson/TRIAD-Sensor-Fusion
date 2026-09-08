[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [string] $UnrealEditorPath = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolved = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectFile = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
    $resolved
} else {
    Join-Path $resolved 'TRIAD.uproject'
}
if ([IO.Path]::GetExtension($projectFile) -ine '.uproject' -or
    -not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Exact TRIAD .uproject is missing: $projectFile"
}
if (-not (Test-Path -LiteralPath $UnrealEditorPath -PathType Leaf)) {
    throw "UE 5.5 editor is missing: $UnrealEditorPath"
}
if (@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'").Count -ne 0) {
    throw 'Close every Unreal Editor before starting the exact Explore V2 session.'
}

$projectDirectory = Split-Path -Parent $projectFile
$v2 = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Explore_v2.umap'
$v1 = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Explore_v1.umap'
$map = if (Test-Path -LiteralPath $v2 -PathType Leaf) {
    '/Game/Maps/Istana_PublicView_Explore_v2'
} elseif (Test-Path -LiteralPath $v1 -PathType Leaf) {
    '/Game/Maps/Istana_PublicView_Explore_v1'
} else {
    throw 'Neither the Explore V2 destination nor its validated Explore V1 source map exists.'
}

$ddc = Join-Path $projectDirectory 'Saved\TRIAD\DerivedDataCache'
$zen = Join-Path $projectDirectory 'Saved\TRIAD\Zen'
New-Item -ItemType Directory -Force -Path $ddc, $zen | Out-Null
$arguments = @(
    $projectFile,
    $map,
    '-DisablePlugin=AirSim',
    "-ddc=CreatePak+EnginePak+Local",
    "-LocalDataCachePath=$ddc",
    "-ZenDataPath=$zen",
    '-RemoteControlHttpServer',
    '-RCWebControlEnable',
    '-log'
)
$process = Start-Process -FilePath $UnrealEditorPath -ArgumentList $arguments -PassThru
[PSCustomObject]@{
    ProcessId = $process.Id
    Map = $map
    RemoteControl = 'http://127.0.0.1:30010'
    ProjectLocalDdc = $ddc
    ProjectLocalZen = $zen
}
