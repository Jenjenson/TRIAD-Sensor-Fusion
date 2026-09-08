[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ProtectedSnapshot {
    param([string[]] $LiteralPaths)

    $snapshot = [System.Collections.Generic.Dictionary[string, string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $LiteralPaths) {
        $fullPath = [System.IO.Path]::GetFullPath($path)
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $snapshot[$fullPath] = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
            continue
        }
        if (Test-Path -LiteralPath $fullPath -PathType Container) {
            foreach ($file in Get-ChildItem -LiteralPath $fullPath -Recurse -File | Sort-Object FullName) {
                $snapshot[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            }
            continue
        }
        $snapshot["MISSING::$fullPath"] = 'MISSING'
    }
    return ,$snapshot
}

function Assert-SameSnapshot {
    param(
        [System.Collections.Generic.Dictionary[string, string]] $Before,
        [System.Collections.Generic.Dictionary[string, string]] $After
    )

    if ($Before.Count -ne $After.Count) {
        throw 'CRITICAL: protected maps/assets changed file count during public-view source installation.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected map/asset changed during public-view source installation: $($entry.Key)"
        }
    }
}

function Get-InstallSourceFiles {
    param([string] $SourceRoot)

    return @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache)[\\/]' -and
        $_.Extension -ne '.pyc'
    } | Sort-Object FullName)
}

function Assert-NonOverwritingFileCopy {
    param(
        [string] $SourceFile,
        [string] $DestinationFile
    )

    if (-not (Test-Path -LiteralPath $DestinationFile -PathType Leaf)) {
        return
    }
    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash
    if ($sourceHash -ne $destinationHash) {
        throw "Refusing to overwrite different existing public-view source/config file: $DestinationFile"
    }
}

function Copy-NewOrEqualHashFile {
    param(
        [string] $SourceFile,
        [string] $DestinationFile
    )

    Assert-NonOverwritingFileCopy -SourceFile $SourceFile -DestinationFile $DestinationFile
    if (Test-Path -LiteralPath $DestinationFile -PathType Leaf) {
        return
    }
    $destinationDirectory = Split-Path -Parent $DestinationFile
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    Copy-Item -LiteralPath $SourceFile -Destination $DestinationFile
    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash
    if ($sourceHash -ne $destinationHash) {
        throw "Installed public-view file hash mismatch: $DestinationFile"
    }
}

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor process before installing public-view plugin/source assets.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$baseInstaller = Join-Path $PSScriptRoot 'Install-IstanaDevelopmentAssets.ps1'
$sourceRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView'
$sourceValidator = Join-Path $sourceRoot 'validate_public_view_assets.py'
$osmContextValidator = Join-Path $sourceRoot 'validate_public_view_osm_context.py'
$sourceSettings = Join-Path $repositoryRoot 'unreal\Config\IstanaPublicView.settings.json'
$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path.TrimEnd('\')
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$targetRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView'
$targetSettings = Join-Path $resolvedProject 'Config\IstanaPublicView.settings.json'

foreach ($requiredPath in @(
    $baseInstaller,
    $sourceRoot,
    $sourceValidator,
    $osmContextValidator,
    $sourceSettings,
    $projectFile
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required public-view installation input is missing: $requiredPath"
    }
}
$osmContextValidation = & python $osmContextValidator --root $sourceRoot 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "ASSETS_MISSING_OR_INVALID: repository ODbL context validation failed before installation. $($osmContextValidation -join [Environment]::NewLine)"
}

$sourceValidation = & python $sourceValidator --root $sourceRoot 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "ASSETS_MISSING_OR_INVALID: repository public-view source validation failed before installation. $($sourceValidation -join [Environment]::NewLine)"
}

$sourceFiles = Get-InstallSourceFiles -SourceRoot $sourceRoot
if ($sourceFiles.Count -lt 1) {
    throw "ASSETS_MISSING: no public-view source files were found under $sourceRoot"
}

# Preflight every destination before the first public-view copy, so a
# differing target cannot leave a mixed-version partial source tree.
foreach ($sourceFile in $sourceFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourceRoot.Length).TrimStart('\')
    Assert-NonOverwritingFileCopy `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetRoot $relativePath)
}
Assert-NonOverwritingFileCopy -SourceFile $sourceSettings -DestinationFile $targetSettings

$protectedPaths = @(
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'),
    (Join-Path $resolvedProject 'Content\TRIAD\Istana'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView')
)
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
$installError = $null
$baseInstallResult = $null
try {
    # Reuse the established editor-closed plugin/source installer, then add
    # only this isolated public-view source/config namespace.
    $baseInstallResult = & $baseInstaller -ProjectPath $resolvedProject
    foreach ($sourceFile in $sourceFiles) {
        $relativePath = $sourceFile.FullName.Substring($sourceRoot.Length).TrimStart('\')
        Copy-NewOrEqualHashFile `
            -SourceFile $sourceFile.FullName `
            -DestinationFile (Join-Path $targetRoot $relativePath)
    }
    Copy-NewOrEqualHashFile -SourceFile $sourceSettings -DestinationFile $targetSettings

    $targetValidator = Join-Path $targetRoot 'validate_public_view_assets.py'
    $targetOsmContextValidator = Join-Path $targetRoot 'validate_public_view_osm_context.py'
    $targetValidation = & python $targetValidator --root $targetRoot 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed public-view source validation failed. $($targetValidation -join [Environment]::NewLine)"
    }
    $targetOsmContextValidation = & python $targetOsmContextValidator --root $targetRoot 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed ODbL context validation failed. $($targetOsmContextValidation -join [Environment]::NewLine)"
    }
}
catch {
    $installError = $_
}

$protectedAfter = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
Assert-SameSnapshot -Before $protectedBefore -After $protectedAfter
if ($null -ne $installError) {
    throw $installError
}

$installedSettingsHash = (Get-FileHash -LiteralPath $targetSettings -Algorithm SHA256).Hash
$sourceSettingsHash = (Get-FileHash -LiteralPath $sourceSettings -Algorithm SHA256).Hash
if ($installedSettingsHash -ne $sourceSettingsHash) {
    throw 'Installed IstanaPublicView.settings.json does not match the repository source.'
}

[PSCustomObject]@{
    Operation = 'InstallIstanaPublicViewDevelopmentAssets'
    Succeeded = $true
    Project = $resolvedProject
    PluginSource = Join-Path $resolvedProject 'Plugins\TRIADSensorFusion\Source'
    PublicViewSourceAssets = $targetRoot
    PublicViewSettings = $targetSettings
    PublicViewSettingsSha256 = $installedSettingsHash
    InstalledFileCount = $sourceFiles.Count + 1
    ExistingDifferentPublicViewFilesOverwritten = $false
    ProtectedContentMutated = $false
    OSMContextStatus = 'COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED'
    OSMAttribution = '© OpenStreetMap contributors; ODbL-1.0'
    BaseInstall = $baseInstallResult
}
