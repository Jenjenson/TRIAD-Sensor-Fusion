[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ManifestPath,

    [string] $PythonExecutable = 'python',

    [switch] $SchemaOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'unreal\SourceAssets\IstanaDigitalTwin'
$validator = Join-Path $sourceRoot 'validate_release.py'
$releaseRoot = Join-Path $sourceRoot 'Releases'
$exampleRoot = Join-Path $sourceRoot 'Examples'

if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
    throw "Digital-twin release validator is missing: $validator"
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Manifest does not exist: $ManifestPath"
}

$resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath).Path
$resolvedReleaseRoot = (Resolve-Path -LiteralPath $releaseRoot).Path
$resolvedExampleRoot = (Resolve-Path -LiteralPath $exampleRoot).Path
$allowedRoot = if ($SchemaOnly) { $resolvedExampleRoot } else { $resolvedReleaseRoot }
$rootPrefix = $allowedRoot.TrimEnd('\') + '\'
if (-not $resolvedManifest.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    $mode = if ($SchemaOnly) { 'schema-only example' } else { 'strict release' }
    throw "The $mode manifest must be below '$allowedRoot'; got '$resolvedManifest'."
}
if ([System.IO.Path]::GetExtension($resolvedManifest) -ne '.json') {
    throw 'Digital-twin release manifests must use the .json extension.'
}

$arguments = @($validator, $resolvedManifest)
if ($SchemaOnly) {
    $arguments += '--schema-only'
}

& $PythonExecutable @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Digital-twin release validation failed closed with exit code $LASTEXITCODE."
}

if ($SchemaOnly) {
    Write-Warning 'Schema-only validation succeeded. This is not an import-ready or fidelity-authority result.'
}
else {
    Write-Warning 'Stage-0 release integrity passed, but the release remains NON_IMPORTABLE until semantic geometry/terrain/vegetation and cryptographic trust-root verification tooling is reviewed and enabled.'
    Write-Warning 'Run Validate-IstanaGeospatialAuthorityV1.ps1 with an independently reviewed, exact-hash policy for the downstream GeoTIFF/GLB semantic admission step.'
}
