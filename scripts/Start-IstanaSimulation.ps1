[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD\TRIAD.uproject',

    [Parameter(Mandatory = $true)]
    [string] $ScenarioConfigPath,

    [ValidateSet(
        '/Game/Maps/Istana_1km',
        '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid')]
    [string] $MapPackage = '/Game/Maps/Istana_1km',

    [string] $UnrealEngineRoot = 'C:\Program Files\Epic Games\UE_5.5'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$scenario = (Resolve-Path -LiteralPath $ScenarioConfigPath).Path
$editor = Join-Path $UnrealEngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$mapLeaf = switch ($MapPackage) {
    '/Game/Maps/Istana_1km' { 'Istana_1km.umap' }
    '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid' {
        'Istana_PublicView_Explore_v5d_hybrid.umap'
    }
    default { throw "Unsupported Istana map package: $MapPackage" }
}
$mapFile = Join-Path (Split-Path -Parent $project) "Content\Maps\$mapLeaf"

if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) {
    throw "Unreal Editor was not found: $editor"
}
if (-not (Test-Path -LiteralPath $mapFile -PathType Leaf)) {
    throw "Selected Istana map is missing: package=$MapPackage file=$mapFile"
}

$config = Get-Content -Raw -LiteralPath $scenario | ConvertFrom-Json
if ($config.bEnabled -ne $true) {
    throw 'Scenario config is disabled. Review it and rebuild with New-IstanaScenarioConfig.ps1 -Enable.'
}
if ($null -eq $config.SensorNodes -or @($config.SensorNodes).Count -eq 0) {
    throw 'Scenario config contains no recommended SensorNodes.'
}
if ([string] $config.SimulationPerimeter.Shape -ne 'Circle' -or
    [double] $config.SimulationPerimeter.RadiusMeters -ne 1000.0) {
    throw 'Scenario config is not the exact 1,000 m Istana circular AOI.'
}
if ($config.bUseDedicatedRFPropagation -eq $true) {
    if ($config.bRequireDedicatedRFReady -ne $true) {
        throw 'Dedicated RF propagation must fail closed with bRequireDedicatedRFReady=true.'
    }
    if ($config.bDedicatedRFFrameIsWorldOriginIdentity -ne $true) {
        throw 'Dedicated RF propagation requires the explicit world-frame assertion.'
    }
    if ([string] $config.DedicatedRFExpectedWorldPackageName -cne $MapPackage) {
        throw "Dedicated RF config/map mismatch: expected=$($config.DedicatedRFExpectedWorldPackageName) selected=$MapPackage"
    }
}

$arguments = @(
    "`"$project`"",
    $MapPackage,
    "-TRIADScenarioConfig=`"$scenario`""
)

# This is intentionally a visible interactive editor window for the operator.
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Normal -PassThru
[PSCustomObject]@{
    ProcessId = $process.Id
    Map = $MapPackage
    ScenarioConfig = $scenario
    SensorNodeCount = @($config.SensorNodes).Count
    DedicatedRFPropagation = $config.bUseDedicatedRFPropagation -eq $true
}
