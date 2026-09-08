[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ManifestPath,

    [Parameter(Mandatory = $true)]
    [string] $PolicyPath,

    [Parameter(Mandatory = $true)]
    [string] $ReceiptPath,

    [string] $PythonExecutable = 'python'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'unreal\SourceAssets\IstanaDigitalTwin'
$validator = Join-Path $sourceRoot 'validate_geospatial_authority.py'
$releaseRoot = Join-Path $sourceRoot 'Releases'

if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
    throw "GeospatialAuthorityV1 validator is missing: $validator"
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Manifest does not exist: $ManifestPath"
}
if (-not (Test-Path -LiteralPath $PolicyPath -PathType Leaf)) {
    throw "Authority policy does not exist: $PolicyPath"
}
if (Test-Path -LiteralPath $ReceiptPath) {
    throw "Receipt already exists and will not be overwritten: $ReceiptPath"
}

$resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath).Path
$resolvedPolicy = (Resolve-Path -LiteralPath $PolicyPath).Path
$resolvedReleaseRoot = (Resolve-Path -LiteralPath $releaseRoot).Path
$releasePrefix = $resolvedReleaseRoot.TrimEnd('\') + '\'
if (-not $resolvedManifest.StartsWith($releasePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The strict manifest must be below '$resolvedReleaseRoot'; got '$resolvedManifest'."
}
$releaseDirectory = Split-Path -Parent $resolvedManifest
$releaseDirectoryPrefix = $releaseDirectory.TrimEnd('\') + '\'
if (-not $resolvedPolicy.StartsWith($releaseDirectoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The reviewed policy must be inside the same release directory as the manifest."
}

$receiptFullPath = [System.IO.Path]::GetFullPath($ReceiptPath)
$receiptParent = Split-Path -Parent $receiptFullPath
if (
    -not $receiptParent.Equals($releaseDirectory, [System.StringComparison]::OrdinalIgnoreCase) -and
    -not $receiptParent.StartsWith($releaseDirectoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)
) {
    throw "The semantic receipt must be written inside the same controlled release directory."
}
if (-not (Test-Path -LiteralPath $receiptParent -PathType Container)) {
    New-Item -ItemType Directory -Path $receiptParent -Force | Out-Null
}
$resolvedReceipt = $receiptFullPath

& $PythonExecutable $validator $resolvedManifest $resolvedPolicy --receipt $resolvedReceipt
if ($LASTEXITCODE -ne 0) {
    throw "GeospatialAuthorityV1 validation failed closed with exit code $LASTEXITCODE."
}

Write-Warning 'GeoTIFF/GLB semantics passed and are hash-bound, but this does not authenticate survey truth, validate other formats, unlock Unreal import, or confer sensor/RF authority.'
