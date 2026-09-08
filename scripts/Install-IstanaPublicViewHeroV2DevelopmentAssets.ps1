[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath
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
        throw 'CRITICAL: protected v1 maps/assets or renderer settings changed file count during hero-v2 source installation.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected v1 map/asset or renderer setting changed during hero-v2 source installation: $($entry.Key)"
        }
    }
}

function Get-InstallSourceFiles {
    param([string] $SourceRoot)

    return @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache|\.venv)[\\/]' -and
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
        throw "Refusing to overwrite a different existing hero-v2 source file: $DestinationFile"
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
        throw "Installed hero-v2 source file hash mismatch: $DestinationFile"
    }
}

function Resolve-ProjectDirectory {
    param([string] $Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        if ([System.IO.Path]::GetExtension($resolved) -ne '.uproject') {
            throw "ProjectPath file is not an Unreal project: $resolved"
        }
        return (Split-Path -Parent $resolved).TrimEnd('\')
    }
    return $resolved.TrimEnd('\')
}

function Invoke-FailClosedSourceValidation {
    param(
        [string] $PythonExecutable,
        [string] $GeometryRoot,
        [string] $MaterialRoot
    )

    $geometryValidator = Join-Path $GeometryRoot 'validate_hero_v2.py'
    $geometryOutput = & $PythonExecutable $geometryValidator `
        --root $GeometryRoot `
        --output (Join-Path $GeometryRoot 'Generated') `
        --freeze (Join-Path $GeometryRoot 'hero_v2.freeze.json') 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: hero-v2 geometry validation failed. $($geometryOutput -join [Environment]::NewLine)"
    }

    $materialBuilder = Join-Path $MaterialRoot 'build_hero_materials_v2.py'
    $materialOutput = & $PythonExecutable $materialBuilder `
        --source-dir (Join-Path $MaterialRoot 'Source') `
        --source-manifest (Join-Path $MaterialRoot 'Source\source_manifest.json') `
        --output (Join-Path $MaterialRoot 'Generated') `
        --validate-only 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: HeroMaterialsV2 validation failed. $($materialOutput -join [Environment]::NewLine)"
    }
}

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor process before installing hero-v2 plugin/source assets.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$baseInstaller = Join-Path $PSScriptRoot 'Install-IstanaDevelopmentAssets.ps1'
$sourceGeometryRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicViewV2'
$sourceMaterialRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV2'
$sourceReferenceRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaDigitalTwin\IstanaPublicView'
$integrationFreeze = Join-Path $repositoryRoot 'unreal\Plugins\TRIADSensorFusion\Resources\IstanaPublicViewHeroV2.integration.freeze.json'
$resolvedProject = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$targetGeometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV2'
$targetMaterialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV2'
$targetReferenceRoot = Join-Path $resolvedProject 'SourceAssets\IstanaDigitalTwin\IstanaPublicView'

$repositoryMaterialPython = Join-Path $sourceMaterialRoot '.venv\Scripts\python.exe'
$pythonExecutable = if (Test-Path -LiteralPath $repositoryMaterialPython -PathType Leaf) {
    $repositoryMaterialPython
}
else {
    $pythonCommand = Get-Command python -ErrorAction Stop
    $pythonCommand.Source
}

foreach ($requiredPath in @(
    $baseInstaller,
    $sourceGeometryRoot,
    $sourceMaterialRoot,
    $sourceReferenceRoot,
    (Join-Path $sourceGeometryRoot 'validate_hero_v2.py'),
    (Join-Path $sourceGeometryRoot 'Generated\SM_IstanaPublicViewV2_Building_Hero.obj'),
    (Join-Path $sourceMaterialRoot 'build_hero_materials_v2.py'),
    $integrationFreeze,
    $projectFile
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required hero-v2 installation input is missing: $requiredPath"
    }
}

Invoke-FailClosedSourceValidation `
    -PythonExecutable $pythonExecutable `
    -GeometryRoot $sourceGeometryRoot `
    -MaterialRoot $sourceMaterialRoot

$geometryFiles = Get-InstallSourceFiles -SourceRoot $sourceGeometryRoot
$materialFiles = Get-InstallSourceFiles -SourceRoot $sourceMaterialRoot
$referenceFiles = Get-InstallSourceFiles -SourceRoot $sourceReferenceRoot
if ($geometryFiles.Count -lt 1 -or $materialFiles.Count -lt 1 -or $referenceFiles.Count -lt 1) {
    throw 'ASSETS_MISSING: the frozen hero-v2 geometry, material, or licensed reference source tree is empty.'
}

# Preflight every additive destination before the first copy. A differing file
# is a hard stop, so installation cannot create a mixed-version source tree.
foreach ($sourceFile in $geometryFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourceGeometryRoot.Length).TrimStart('\')
    Assert-NonOverwritingFileCopy `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetGeometryRoot $relativePath)
}
foreach ($sourceFile in $materialFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourceMaterialRoot.Length).TrimStart('\')
    Assert-NonOverwritingFileCopy `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetMaterialRoot $relativePath)
}
foreach ($sourceFile in $referenceFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourceReferenceRoot.Length).TrimStart('\')
    Assert-NonOverwritingFileCopy `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetReferenceRoot $relativePath)
}

$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
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
    # The established installer updates the project-owned plugin source while
    # the v2 source trees below remain additive and hash-non-overwriting.
    $baseInstallResult = & $baseInstaller -ProjectPath $resolvedProject
    foreach ($sourceFile in $geometryFiles) {
        $relativePath = $sourceFile.FullName.Substring($sourceGeometryRoot.Length).TrimStart('\')
        Copy-NewOrEqualHashFile `
            -SourceFile $sourceFile.FullName `
            -DestinationFile (Join-Path $targetGeometryRoot $relativePath)
    }
    foreach ($sourceFile in $materialFiles) {
        $relativePath = $sourceFile.FullName.Substring($sourceMaterialRoot.Length).TrimStart('\')
        Copy-NewOrEqualHashFile `
            -SourceFile $sourceFile.FullName `
            -DestinationFile (Join-Path $targetMaterialRoot $relativePath)
    }
    # The geometry freeze is bound to this exact, licensed Commons evidence
    # manifest. Install it additively so validation in the project uses the
    # same bytes as validation in the collaboration repository.
    foreach ($sourceFile in $referenceFiles) {
        $relativePath = $sourceFile.FullName.Substring($sourceReferenceRoot.Length).TrimStart('\')
        Copy-NewOrEqualHashFile `
            -SourceFile $sourceFile.FullName `
            -DestinationFile (Join-Path $targetReferenceRoot $relativePath)
    }
    Invoke-FailClosedSourceValidation `
        -PythonExecutable $pythonExecutable `
        -GeometryRoot $targetGeometryRoot `
        -MaterialRoot $targetMaterialRoot
}
catch {
    $installError = $_
}

$protectedAfter = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
Assert-SameSnapshot -Before $protectedBefore -After $protectedAfter
if ($null -ne $installError) {
    throw $installError
}

[PSCustomObject]@{
    Operation = 'InstallIstanaPublicViewHeroV2DevelopmentAssets'
    Succeeded = $true
    Project = $resolvedProject
    GeometrySourceAssets = $targetGeometryRoot
    HeroMaterialSourceAssets = $targetMaterialRoot
    LicensedReferenceSourceAssets = $targetReferenceRoot
    GeometryFileCount = $geometryFiles.Count
    MaterialFileCount = $materialFiles.Count
    LicensedReferenceFileCount = $referenceFiles.Count
    ExistingDifferentFilesOverwritten = $false
    ProtectedV1ContentMutated = $false
    RendererSettingsMutated = $false
    NaniteRequired = $false
    ShaderModel6Required = $false
    BaseInstall = $baseInstallResult
}
