[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor process before installing source or source assets.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourcePlugin = Join-Path $repositoryRoot 'unreal\Plugins\TRIADSensorFusion'
$sourceAssets = Join-Path $repositoryRoot 'unreal\SourceAssets\Istana'
$sourceVisualSettings = Join-Path $repositoryRoot 'unreal\Config\IstanaVisualAcceptance.settings.json'
$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path.TrimEnd('\')
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$targetPlugin = Join-Path $resolvedProject 'Plugins\TRIADSensorFusion'
$targetAssets = Join-Path $resolvedProject 'SourceAssets\Istana'
$targetVisualSettings = Join-Path $resolvedProject 'Config\IstanaVisualAcceptance.settings.json'
$sdthMap = Join-Path $resolvedProject 'Content\SDTH.umap'

foreach ($requiredPath in @($projectFile, $sourcePlugin, $sourceAssets, $sourceVisualSettings, $sdthMap)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required path is missing: $requiredPath"
    }
}

$beforeSdth = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdthMap).Hash
$sourceVisualSettingsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceVisualSettings).Hash
if (Test-Path -LiteralPath $targetVisualSettings -PathType Leaf) {
    $existingVisualSettingsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetVisualSettings).Hash
    if ($existingVisualSettingsHash -ne $sourceVisualSettingsHash) {
        throw "Refusing to overwrite a different project visual-acceptance settings file: $targetVisualSettings"
    }
}
New-Item -ItemType Directory -Force -Path `
    $targetPlugin, `
    $targetAssets, `
    (Split-Path -Parent $targetVisualSettings) | Out-Null

Copy-Item -LiteralPath (Join-Path $sourcePlugin 'TRIADSensorFusion.uplugin') `
    -Destination (Join-Path $targetPlugin 'TRIADSensorFusion.uplugin') -Force
Copy-Item -LiteralPath (Join-Path $sourcePlugin 'README.md') `
    -Destination (Join-Path $targetPlugin 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $sourcePlugin 'Source') `
    -Destination $targetPlugin -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourcePlugin 'Resources') `
    -Destination $targetPlugin -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourcePlugin 'Tests') `
    -Destination $targetPlugin -Recurse -Force

Get-ChildItem -LiteralPath $sourceAssets -Force | ForEach-Object {
    if ($_.Name -notin @('__pycache__', '.pytest_cache')) {
        Copy-Item -LiteralPath $_.FullName -Destination $targetAssets -Recurse -Force
    }
}

if (-not (Test-Path -LiteralPath $targetVisualSettings -PathType Leaf)) {
    Copy-Item -LiteralPath $sourceVisualSettings -Destination $targetVisualSettings
}
$installedVisualSettingsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetVisualSettings).Hash
if ($installedVisualSettingsHash -ne $sourceVisualSettingsHash) {
    throw 'Installed project visual-acceptance settings hash does not match the repository-owned source.'
}

$afterSdth = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdthMap).Hash
if ($beforeSdth -ne $afterSdth) {
    throw 'Safety invariant failed: Content/SDTH.umap changed during source installation.'
}

$textureManifest = Join-Path $targetAssets 'Generated\Textures\IstanaPbrTextures.manifest.json'
$meshManifest = Join-Path $targetAssets 'Generated\IstanaExterior.manifest.json'
foreach ($manifest in @($textureManifest, $meshManifest)) {
    if (-not (Test-Path -LiteralPath $manifest)) {
        throw "Installed asset manifest is missing: $manifest"
    }
}

[PSCustomObject]@{
    Project = $resolvedProject
    PluginSource = Join-Path $targetPlugin 'Source'
    IstanaSourceAssets = $targetAssets
    MeshManifest = $meshManifest
    TextureManifest = $textureManifest
    VisualAcceptanceSettings = $targetVisualSettings
    VisualAcceptanceSettingsSha256 = $installedVisualSettingsHash
    SdthSha256Unchanged = $afterSdth
}
