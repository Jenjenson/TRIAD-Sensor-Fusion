[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $PatchPath,

    [string] $BaseConfigPath = (Join-Path $PSScriptRoot '..\unreal\Config\IstanaSensorPlacement.base.json'),

    [Parameter(Mandatory = $true)]
    [string] $OutputPath,

    [switch] $Enable,

    [switch] $EnableOperatorObserver
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$baseFile = (Resolve-Path -LiteralPath $BaseConfigPath).Path
$patchFile = (Resolve-Path -LiteralPath $PatchPath).Path
$outputFile = [System.IO.Path]::GetFullPath($OutputPath)

if (Test-Path -LiteralPath $outputFile) {
    throw "Refusing to overwrite existing scenario config: $outputFile"
}

$base = Get-Content -Raw -LiteralPath $baseFile | ConvertFrom-Json
$patch = Get-Content -Raw -LiteralPath $patchFile | ConvertFrom-Json

if ($patch.schemaVersion -ne 'triad.unreal_sensor_nodes_patch.v1') {
    throw "Unsupported patch schema: $($patch.schemaVersion)"
}
if ($patch.simulationOnly -ne $true -or $patch.siteAuthorizationInferred -ne $false) {
    throw 'Patch is missing the required simulation/review safety declarations.'
}
if ($null -eq $patch.SensorNodes -or @($patch.SensorNodes).Count -eq 0) {
    throw 'Patch contains no recommended SensorNodes.'
}

$nodeIds = @($patch.SensorNodes | ForEach-Object { [string] $_.NodeId })
if ($nodeIds | Where-Object { [string]::IsNullOrWhiteSpace($_) }) {
    throw 'Every recommended sensor node must have a non-empty NodeId.'
}
if (@($nodeIds | Sort-Object -Unique).Count -ne $nodeIds.Count) {
    throw 'Recommended SensorNodes contain duplicate NodeId values.'
}

$base.SensorNodes = @($patch.SensorNodes)
$base.bEnabled = [bool] $Enable
if ($EnableOperatorObserver) {
    if ($null -eq $base.OperatorObserver) {
        throw 'Base config has no OperatorObserver settings to enable.'
    }
    $base.OperatorObserver.bEnabled = $true
}

$parent = Split-Path -Parent $outputFile
if (-not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}

$json = $base | ConvertTo-Json -Depth 100
$utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
$stream = [System.IO.File]::Open(
    $outputFile,
    [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None)
try {
    $writer = [System.IO.StreamWriter]::new($stream, $utf8WithoutBom)
    try {
        $writer.WriteLine($json)
        $writer.Flush()
    }
    finally {
        $writer.Dispose()
    }
}
catch {
    $stream.Dispose()
    if (Test-Path -LiteralPath $outputFile) {
        Remove-Item -LiteralPath $outputFile
    }
    throw
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputFile).Hash
[PSCustomObject]@{
    OutputPath = $outputFile
    Enabled = [bool] $base.bEnabled
    OperatorObserverEnabled = [bool] $base.OperatorObserver.bEnabled
    SensorNodeCount = @($base.SensorNodes).Count
    SourceRecommendationDigest = [string] $patch.sourceRecommendationDigest
    Sha256 = $hash
    LiveSingaporeConfigMutated = $false
}
