<#
.SYNOPSIS
Installs only the additive Istana Public View Hero-V4 development layer.

.DESCRIPTION
Hero-V4 never installs or upgrades Hero-V3 implicitly. A fresh project must
first run Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1 as a separate
reviewed step, import and migrate the V3 assets/map, and close Unreal Editor.
This installer byte-proves those V3 source and visual-profile prerequisites,
requires every V3 plugin prerequisite to be current or an explicitly reviewed
read-only-compatible digest, and requires the expected imported V3 map, mesh,
and 57 texture package-name roster before it mutates any Hero-V4 path. Imported
UAsset payload/semantic authority is deliberately deferred to the V4 editor
Import/Migrate gates before the corresponding V4 package save. The installer
publishes validated V4 geometry/material source first and the four explicit V4
plugin files last.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [ValidateNotNullOrEmpty()]
    [string] $MaterialPythonExecutable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedBaseInstallerSha256 = '764089445b03dcecd5c706370301d4d82646f258ce90183a5ce52d6904808e15'
$expectedBaseInstallerBytes = 35079
$expectedVisualSettingsSha256 = '6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c'
$expectedVisualSettingsBytes = 1022

function Resolve-ProjectDirectory {
    param([string] $Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        if ([System.IO.Path]::GetExtension($resolved) -ine '.uproject') {
            throw "ProjectPath file is not an Unreal project: $resolved"
        }
        return (Split-Path -Parent $resolved).TrimEnd('\')
    }
    return $resolved.TrimEnd('\')
}

function Resolve-HeroMaterialPython {
    param(
        [string] $ExplicitOverride,
        [string[]] $PinnedCandidates
    )

    $candidates = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitOverride)) {
        $candidates.Add($ExplicitOverride)
    }
    foreach ($candidate in $PinnedCandidates) {
        $candidates.Add($candidate)
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $versions = @(& $candidate -B -c 'import PIL, numpy; print(PIL.__version__ + "|" + numpy.__version__)' 2>&1)
            if ($LASTEXITCODE -eq 0 -and (($versions -join '').Trim()) -ceq '12.3.0|2.3.5') {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }
    throw 'No validated material Python was found. Use the pinned HeroMaterialsV2 .venv or pass -MaterialPythonExecutable with exact Pillow 12.3.0 and NumPy 2.3.5; system-Python fallback is forbidden.'
}

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
            foreach ($file in Get-ChildItem -LiteralPath $fullPath -Recurse -File -Force | Sort-Object FullName) {
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
        throw 'CRITICAL: protected V1/V2/V3 content, settings, or shared plugin state changed file count during the additive Hero-V4 installation layer.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -cne $entry.Value) {
            throw "CRITICAL: protected V1/V2/V3 content, settings, or shared plugin state changed during the additive Hero-V4 installation layer: $($entry.Key)"
        }
    }
}

function Get-InstallSourceFiles {
    param([string] $SourceRoot)

    return @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Force | Where-Object {
        $_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache|\.venv)[\\/]' -and
        $_.FullName -notmatch '[\\/]HeroMaterialsV[34]_Determinism_[a-z0-9_]+(?:[\\/]|$)' -and
        $_.Extension -notin @('.pyc', '.pyo', '.pyd')
    } | Sort-Object FullName)
}

function Assert-NoGeneratedPythonCache {
    param(
        [string] $SourceRoot,
        [string] $Label
    )

    if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
        return
    }
    $cacheDirectories = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -Directory -Force |
        Where-Object { $_.Name -in @('__pycache__', '.pytest_cache') })
    $cacheFiles = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Force |
        Where-Object { $_.Extension -in @('.pyc', '.pyo', '.pyd') })
    if ($cacheDirectories.Count -ne 0 -or $cacheFiles.Count -ne 0) {
        $paths = @(
            $cacheDirectories | ForEach-Object FullName
            $cacheFiles | ForEach-Object FullName
        ) -join ', '
        throw "Refusing Hero-V4 installation because $Label contains generated Python cache state: $paths"
    }
}

function Assert-NoHeroV4GeneratedPythonCache {
    param(
        [string] $TargetRoot,
        [string] $Label
    )

    if (-not (Test-Path -LiteralPath $TargetRoot -PathType Container)) {
        return
    }
    $cacheFiles = @(Get-ChildItem -LiteralPath $TargetRoot -Recurse -File -Force |
        Where-Object {
            $_.Name -match '^test_istana_public_view_hero_v4_integration_contract\..*\.py[co]$'
        })
    if ($cacheFiles.Count -ne 0) {
        $paths = @($cacheFiles | ForEach-Object FullName) -join ', '
        throw "Refusing Hero-V4 installation because $Label contains generated Hero-V4 Python cache state: $paths"
    }
}

function Assert-NoHeroMaterialsV4DeterminismTemporaryTree {
    param([string] $SourceRoot)

    if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
        return
    }
    $temporaryDirectories = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -Directory -Force |
        Where-Object { $_.Name -match '^HeroMaterialsV[34]_Determinism_[a-z0-9_]+$' })
    if ($temporaryDirectories.Count -ne 0) {
        $paths = @($temporaryDirectories | ForEach-Object FullName) -join ', '
        throw "Refusing Hero-V4 installation while a material determinism temporary tree exists: $paths"
    }
}

function Assert-ExactBaseInstaller {
    param([string] $InstallerPath)

    $item = Get-Item -LiteralPath $InstallerPath
    $digest = (Get-FileHash -LiteralPath $InstallerPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.Length -ne $expectedBaseInstallerBytes -or $digest -cne $expectedBaseInstallerSha256) {
        throw "The Hero-V4 layer requires the exact reviewed Hero-V3 base installer; its bytes differ: $InstallerPath"
    }
    return $digest
}

function Assert-ExactSharedHeroV3VisualAcceptanceSettings {
    param([string] $SettingsPath)

    $item = Get-Item -LiteralPath $SettingsPath
    $digest = (Get-FileHash -LiteralPath $SettingsPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.Length -ne $expectedVisualSettingsBytes -or $digest -cne $expectedVisualSettingsSha256) {
        throw "Hero-V4 must reuse the exact frozen Hero-V3 visual-acceptance settings bytes: $SettingsPath"
    }
    $text = Get-Content -LiteralPath $SettingsPath -Raw
    $settings = $text | ConvertFrom-Json
    if ([regex]::Matches($text, '(?m)^\s*"RpcEnabled"\s*:').Count -ne 1 -or
        [regex]::Matches($text, '(?m)^\s*"EnableRpc"\s*:').Count -ne 1 -or
        $settings.TRIADProfile -cne 'ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY' -or
        [bool] $settings.TRIADProductionSensorProfile -or
        $settings.SimMode -cne 'ComputerVision' -or
        [bool] $settings.RpcEnabled -or
        [bool] $settings.EnableRpc -or
        $settings.LocalHostIp -cne '127.0.0.1' -or
        [int] $settings.ApiServerPort -ne 41451 -or
        $null -ne $settings.PSObject.Properties['DefaultSensors'] -or
        $text -match '(?i)"SensorType"\s*:\s*6' -or
        $text -match '(?i)"[^"]*lidar[^"]*"\s*:') {
        throw 'The shared Hero-V3 visual-acceptance profile semantics changed; Hero-V4 installation is forbidden.'
    }
    return $digest
}

function Invoke-FailClosedSourceValidation {
    param(
        [string] $PythonExecutable,
        [string] $GeometryRoot,
        [string] $MaterialRoot
    )

    $geometryOutput = @(& $PythonExecutable -B (Join-Path $GeometryRoot 'validate_hero_v4.py') `
        --root $GeometryRoot `
        --output (Join-Path $GeometryRoot 'Generated') `
        --freeze (Join-Path $GeometryRoot 'hero_v4.freeze.json') 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: Hero-V4 geometry/freeze validation failed. $($geometryOutput -join [Environment]::NewLine)"
    }

    $materialOutput = @(& $PythonExecutable -B `
        (Join-Path $MaterialRoot 'validate_hero_materials_v4.py') `
        --validate-only 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: HeroMaterialsV4 contract/interface/freeze validation failed. $($materialOutput -join [Environment]::NewLine)"
    }
}

function Assert-NewOrEqualHashFile {
    param(
        [string] $SourceFile,
        [string] $DestinationFile
    )

    if (-not (Test-Path -LiteralPath $DestinationFile -PathType Leaf)) {
        return
    }
    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($sourceHash -cne $destinationHash) {
        throw "Refusing to overwrite a different existing Hero-V4 file: $DestinationFile"
    }
}

function Copy-NewOrEqualHashFile {
    param(
        [string] $SourceFile,
        [string] $DestinationFile
    )

    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if (Test-Path -LiteralPath $DestinationFile -PathType Leaf) {
        $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($sourceHash -cne $destinationHash) {
            throw "Refusing to overwrite a different existing Hero-V4 file: $DestinationFile"
        }
        return 'CURRENT_EQUAL'
    }

    $destinationDirectory = Split-Path -Parent $DestinationFile
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    $stageFile = "$DestinationFile.hero_v4_add_$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        [System.IO.File]::Copy($SourceFile, $stageFile, $false)
        $stageHash = (Get-FileHash -LiteralPath $stageFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($stageHash -cne $sourceHash) {
            throw "Hero-V4 additive staging hash mismatch: $SourceFile"
        }
        try {
            [System.IO.File]::Move($stageFile, $DestinationFile)
        }
        catch {
            $publishError = $_
            if (Test-Path -LiteralPath $DestinationFile -PathType Leaf) {
                $collisionHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($collisionHash -ceq $sourceHash) {
                    return 'CURRENT_EQUAL'
                }
                throw "Refusing to overwrite a different file created during Hero-V4 additive publication: $DestinationFile"
            }
            throw $publishError
        }
        $installedHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($installedHash -cne $sourceHash) {
            throw "Installed Hero-V4 file hash mismatch: $DestinationFile"
        }
        return 'ADDED'
    }
    finally {
        if (Test-Path -LiteralPath $stageFile -PathType Leaf) {
            [System.IO.File]::Delete($stageFile)
        }
    }
}

function Get-RelativePath {
    param(
        [string] $Root,
        [string] $FullName
    )

    return $FullName.Substring($Root.TrimEnd('\').Length).TrimStart('\')
}

function Assert-NoUnexpectedTargetFiles {
    param(
        [string] $TargetRoot,
        [string[]] $ExpectedRelativePaths,
        [string] $Label
    )

    if (-not (Test-Path -LiteralPath $TargetRoot -PathType Container)) {
        return
    }
    $expected = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($relativePath in $ExpectedRelativePaths) {
        [void] $expected.Add($relativePath)
    }
    $unexpected = @(
        Get-ChildItem -LiteralPath $TargetRoot -Recurse -File -Force | ForEach-Object {
            $relativePath = Get-RelativePath -Root $TargetRoot -FullName $_.FullName
            if (-not $expected.Contains($relativePath)) {
                $relativePath
            }
        }
    )
    if ($unexpected.Count -ne 0) {
        throw "$Label contains files outside the exact reviewed Hero-V4 source roster: $($unexpected -join ', ')"
    }
}

function Assert-ExactInstalledPrerequisiteFile {
    param(
        [string] $SourceFile,
        [string] $DestinationFile,
        [string] $Label,
        [string[]] $AdditionalAcceptedDestinationSha256 = @()
    )

    if (-not (Test-Path -LiteralPath $SourceFile -PathType Leaf)) {
        throw "The repository prerequisite source is missing: $SourceFile"
    }
    if (-not (Test-Path -LiteralPath $DestinationFile -PathType Leaf)) {
        throw "Hero-V4 requires an exact installed Hero-V3 prerequisite ($Label): $DestinationFile. Run Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1 separately, import/migrate V3, close Unreal Editor, and retry."
    }

    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $accepted = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    [void] $accepted.Add($sourceHash)
    foreach ($digest in $AdditionalAcceptedDestinationSha256) {
        [void] $accepted.Add($digest.ToLowerInvariant())
    }
    if (-not $accepted.Contains($destinationHash)) {
        throw "Hero-V4 requires an exact current Hero-V3 prerequisite ($Label); target bytes differ: $DestinationFile"
    }
    return $destinationHash
}

function Assert-ExactInstalledPrerequisiteTree {
    param(
        [string] $SourceRoot,
        [string] $TargetRoot,
        [string] $Label
    )

    $sourceFiles = Get-InstallSourceFiles -SourceRoot $SourceRoot
    if ($sourceFiles.Count -eq 0) {
        throw "The repository $Label prerequisite tree is empty: $SourceRoot"
    }
    if (-not (Test-Path -LiteralPath $TargetRoot -PathType Container)) {
        throw "Hero-V4 requires the separately installed exact Hero-V3 $Label tree: $TargetRoot"
    }
    $relativePaths = @($sourceFiles | ForEach-Object {
        Get-RelativePath -Root $SourceRoot -FullName $_.FullName
    })
    Assert-NoUnexpectedTargetFiles `
        -TargetRoot $TargetRoot `
        -ExpectedRelativePaths $relativePaths `
        -Label "Installed Hero-V3 $Label prerequisite tree"
    foreach ($sourceFile in $sourceFiles) {
        $relativePath = Get-RelativePath -Root $SourceRoot -FullName $sourceFile.FullName
        Assert-ExactInstalledPrerequisiteFile `
            -SourceFile $sourceFile.FullName `
            -DestinationFile (Join-Path $TargetRoot $relativePath) `
            -Label "$Label/$relativePath" | Out-Null
    }
    return $sourceFiles.Count
}

function Get-ExpectedHeroV3TextureAssetNames {
    param([string] $GeneratedManifestPath)

    $manifest = Get-Content -LiteralPath $GeneratedManifestPath -Raw | ConvertFrom-Json
    if ($manifest.schema -cne 'triad.istana_hero_material_generated_pack.v3' -or
        [int] $manifest.outputCount -ne 57) {
        throw "The exact Hero-V3 generated material manifest contract changed: $GeneratedManifestPath"
    }
    $names = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($material in @($manifest.materials)) {
        foreach ($output in $material.outputs.PSObject.Properties) {
            $fileName = [string] $output.Value.file
            if ([System.IO.Path]::GetExtension($fileName) -cne '.png') {
                throw "Unexpected Hero-V3 generated texture filename: $fileName"
            }
            [void] $names.Add("$([System.IO.Path]::GetFileNameWithoutExtension($fileName)).uasset")
        }
    }
    foreach ($output in $manifest.sharedOutputs.PSObject.Properties) {
        $fileName = [string] $output.Value.file
        if ([System.IO.Path]::GetExtension($fileName) -cne '.png') {
            throw "Unexpected shared Hero-V3 generated texture filename: $fileName"
        }
        [void] $names.Add("$([System.IO.Path]::GetFileNameWithoutExtension($fileName)).uasset")
    }
    if ($names.Count -ne 57) {
        throw "The Hero-V3 generated manifest does not define exactly 57 unique texture assets: $GeneratedManifestPath"
    }
    return @($names | Sort-Object)
}

function Assert-ExactInstalledHeroV3TextureRoster {
    param(
        [string] $GeneratedManifestPath,
        [string] $TargetTextureRoot
    )

    if (-not (Test-Path -LiteralPath $TargetTextureRoot -PathType Container)) {
        throw "Hero-V4 requires the separately imported Hero-V3 texture namespace: $TargetTextureRoot"
    }
    $expectedNames = @(Get-ExpectedHeroV3TextureAssetNames `
        -GeneratedManifestPath $GeneratedManifestPath)
    $actualNames = @(
        Get-ChildItem -LiteralPath $TargetTextureRoot -File -Filter '*.uasset' -Force |
            ForEach-Object Name | Sort-Object
    )
    if ($actualNames.Count -ne 57 -or
        [string]::Join("`n", $actualNames) -cne [string]::Join("`n", $expectedNames)) {
        throw "Hero-V4 requires the exact 57-name frozen-contract Hero-V3 texture package roster and no differently named texture package; offline payload semantics remain editor-gated: $TargetTextureRoot"
    }
    return $actualNames.Count
}

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor process before installing Hero-V4 plugin/source assets.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$baseInstaller = Join-Path $PSScriptRoot 'Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1'
$sourcePluginRoot = Join-Path $repositoryRoot 'unreal\Plugins\TRIADSensorFusion'
$sourceGeometryRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicViewV4'
$sourceMaterialRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV4'
$sourceV3GeometryRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicViewV3'
$sourceV3MaterialRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV3'
$sourceV3ReferenceRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaDigitalTwin\IstanaPublicView'
$sourceV3GeneratedManifest = Join-Path $sourceV3MaterialRoot 'Generated\manifest.json'
$sourceMaterialFreeze = Join-Path $sourceMaterialRoot 'hero_materials_v4.freeze.json'
$sourceIntegrationFreeze = Join-Path $sourcePluginRoot 'Resources\IstanaPublicViewHeroV4.integration.freeze.json'
$sourceVisualSettings = Join-Path $repositoryRoot 'unreal\Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'
$resolvedProject = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$targetPluginRoot = Join-Path $resolvedProject 'Plugins\TRIADSensorFusion'
$targetGeometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV4'
$targetMaterialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV4'
$targetVisualSettings = Join-Path $resolvedProject 'Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'
$targetV3GeometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV3'
$targetV3MaterialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV3'
$targetV3ReferenceRoot = Join-Path $resolvedProject 'SourceAssets\IstanaDigitalTwin\IstanaPublicView'
$targetV3Map = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v3.umap'
$targetV3Mesh = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV3\Building\SM_IstanaPublicViewV3_Building_Hero.uasset'
$targetV3TextureRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV3\Textures'

if (-not (Test-Path -LiteralPath $sourceMaterialFreeze -PathType Leaf)) {
    throw "ASSETS_MISSING_OR_INVALID: the reviewed HeroMaterialsV4 freeze is absent. No Hero-V3 base installation or Hero-V4 target mutation ran: $sourceMaterialFreeze"
}

$v4PluginRelativePaths = @(
    'Source\TRIADSensorFusionEditor\Public\TRIADIstanaPublicViewHeroV4EditorLibrary.h',
    'Source\TRIADSensorFusionEditor\Private\TRIADIstanaPublicViewHeroV4EditorLibrary.cpp',
    'Tests\test_istana_public_view_hero_v4_integration_contract.py',
    'Resources\IstanaPublicViewHeroV4.integration.freeze.json'
)
$requiredSourcePaths = @(
    $baseInstaller,
    $projectFile,
    $sourceVisualSettings,
    $sourceV3GeneratedManifest,
    (Join-Path $sourceGeometryRoot 'validate_hero_v4.py'),
    (Join-Path $sourceGeometryRoot 'hero_v4.freeze.json'),
    (Join-Path $sourceGeometryRoot 'Generated\SM_IstanaPublicViewV4_Building_Hero.obj'),
    (Join-Path $sourceMaterialRoot 'validate_hero_materials_v4.py'),
    $sourceMaterialFreeze,
    $sourceIntegrationFreeze
)
$requiredSourcePaths += @($v4PluginRelativePaths | ForEach-Object { Join-Path $sourcePluginRoot $_ })
foreach ($requiredPath in $requiredSourcePaths) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required frozen Hero-V4 installation input is missing: $requiredPath"
    }
}
foreach ($requiredRoot in @(
    $sourceV3GeometryRoot,
    $sourceV3MaterialRoot,
    $sourceV3ReferenceRoot
)) {
    if (-not (Test-Path -LiteralPath $requiredRoot -PathType Container)) {
        throw "Required frozen Hero-V3 prerequisite source tree is missing: $requiredRoot"
    }
}

$baseInstallerSha256 = Assert-ExactBaseInstaller -InstallerPath $baseInstaller
$visualSettingsSha256 = Assert-ExactSharedHeroV3VisualAcceptanceSettings -SettingsPath $sourceVisualSettings
Assert-NoHeroMaterialsV4DeterminismTemporaryTree -SourceRoot $sourceMaterialRoot
Assert-NoHeroMaterialsV4DeterminismTemporaryTree -SourceRoot $sourceV3MaterialRoot
Assert-NoGeneratedPythonCache -SourceRoot $sourcePluginRoot -Label 'repository plugin source'
Assert-NoGeneratedPythonCache -SourceRoot $sourceGeometryRoot -Label 'repository Hero-V4 geometry source'
Assert-NoGeneratedPythonCache -SourceRoot $sourceMaterialRoot -Label 'repository HeroMaterialsV4 source'
Assert-NoGeneratedPythonCache -SourceRoot $sourceV3GeometryRoot -Label 'repository Hero-V3 geometry prerequisite source'
Assert-NoGeneratedPythonCache -SourceRoot $sourceV3MaterialRoot -Label 'repository HeroMaterialsV3 prerequisite source'
Assert-NoGeneratedPythonCache -SourceRoot $sourceV3ReferenceRoot -Label 'repository Hero-V3 public-reference prerequisite source'

$forbiddenMaterialPayloads = @(Get-ChildItem -LiteralPath $sourceMaterialRoot -Recurse -File -Force |
    Where-Object { $_.Extension -in @('.png', '.jpg', '.jpeg', '.tif', '.tiff', '.exr', '.uasset', '.uexp', '.ubulk') })
if ($forbiddenMaterialPayloads.Count -ne 0) {
    throw "HeroMaterialsV4 must remain reference-only and cannot install texture/package payloads: $(@($forbiddenMaterialPayloads | ForEach-Object FullName) -join ', ')"
}

$repositoryMaterialPythonV2 = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV2\.venv\Scripts\python.exe'
$pythonExecutable = Resolve-HeroMaterialPython `
    -ExplicitOverride $MaterialPythonExecutable `
    -PinnedCandidates @($repositoryMaterialPythonV2)
Invoke-FailClosedSourceValidation `
    -PythonExecutable $pythonExecutable `
    -GeometryRoot $sourceGeometryRoot `
    -MaterialRoot $sourceMaterialRoot

$geometryFiles = Get-InstallSourceFiles -SourceRoot $sourceGeometryRoot
$materialFiles = Get-InstallSourceFiles -SourceRoot $sourceMaterialRoot
$pluginFiles = @($v4PluginRelativePaths | ForEach-Object { Get-Item -LiteralPath (Join-Path $sourcePluginRoot $_) })
if ($geometryFiles.Count -eq 0 -or $materialFiles.Count -eq 0 -or $pluginFiles.Count -ne 4) {
    throw 'ASSETS_MISSING: the exact Hero-V4 additive source or plugin roster is incomplete.'
}

# V4 never runs the V3 installer. Its recursive plugin roster would include the
# current V4 files and could publish them before V4 source validation/copying.
# Instead, byte-prove the separately installed V3 source/profile inputs,
# accept only current or explicitly reviewed-compatible plugin bytes, and
# require its imported package roster before any V4 target mutation. The V4
# editor gates own UAsset payload/semantic validation.
Assert-ExactInstalledPrerequisiteFile `
    -SourceFile $sourceVisualSettings `
    -DestinationFile $targetVisualSettings `
    -Label 'visual-acceptance settings' | Out-Null
Assert-ExactSharedHeroV3VisualAcceptanceSettings -SettingsPath $targetVisualSettings | Out-Null
Assert-NoHeroV4GeneratedPythonCache -TargetRoot $targetPluginRoot -Label 'target project plugin'
Assert-NoHeroMaterialsV4DeterminismTemporaryTree -SourceRoot $targetV3MaterialRoot
Assert-NoGeneratedPythonCache -SourceRoot $targetV3GeometryRoot -Label 'installed Hero-V3 geometry prerequisite source'
Assert-NoGeneratedPythonCache -SourceRoot $targetV3MaterialRoot -Label 'installed HeroMaterialsV3 prerequisite source'
Assert-NoGeneratedPythonCache -SourceRoot $targetV3ReferenceRoot -Label 'installed Hero-V3 public-reference prerequisite source'
$installedV3GeometryFileCount = Assert-ExactInstalledPrerequisiteTree `
    -SourceRoot $sourceV3GeometryRoot `
    -TargetRoot $targetV3GeometryRoot `
    -Label 'geometry source'
$installedV3MaterialFileCount = Assert-ExactInstalledPrerequisiteTree `
    -SourceRoot $sourceV3MaterialRoot `
    -TargetRoot $targetV3MaterialRoot `
    -Label 'material source'
$installedV3ReferenceFileCount = Assert-ExactInstalledPrerequisiteTree `
    -SourceRoot $sourceV3ReferenceRoot `
    -TargetRoot $targetV3ReferenceRoot `
    -Label 'public-reference source'

$v4PluginRelativePathSet = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($relativePath in $v4PluginRelativePaths) {
    [void] $v4PluginRelativePathSet.Add($relativePath)
}
$allRepositoryPluginFiles = @(
    Get-Item -LiteralPath (Join-Path $sourcePluginRoot 'TRIADSensorFusion.uplugin')
    Get-Item -LiteralPath (Join-Path $sourcePluginRoot 'README.md')
    Get-InstallSourceFiles -SourceRoot (Join-Path $sourcePluginRoot 'Source')
    Get-InstallSourceFiles -SourceRoot (Join-Path $sourcePluginRoot 'Resources')
    Get-InstallSourceFiles -SourceRoot (Join-Path $sourcePluginRoot 'Tests')
)
$v3BasePluginFiles = @($allRepositoryPluginFiles | Where-Object {
    $relativePath = Get-RelativePath -Root $sourcePluginRoot -FullName $_.FullName
    -not $v4PluginRelativePathSet.Contains($relativePath)
})
if ($v3BasePluginFiles.Count -eq 0) {
    throw 'The exact shared Hero-V3/base plugin prerequisite roster is empty.'
}
$recognizedInstalledV3ReadmeSha256 = '584b43bd1c3fdfa0f5cf3d00167f7688c8fd01b4745505d05917d7a0b09ee8d4'
$v3DiagnosticCompatibleRelativePath = 'Source\TRIADSensorFusionEditor\Private\TRIADIstanaPublicViewHeroV3EditorLibrary.cpp'
$expectedCurrentV3DiagnosticSha256 = '3c2f916845c421070e7a64c25ea95cd8de2fbea95800f4db166edd3a7177fdee'
$recognizedCompatibleV3DiagnosticSha256 = 'f48379a6c236b65553d2ef41fcea2c8932b53cc7e138da5daa667aa2a92857a9'
$v3DiagnosticTestCompatibleRelativePath = 'Tests\test_istana_public_view_hero_v3_integration_contract.py'
$expectedCurrentV3DiagnosticTestSha256 = 'd040466ec9eda50afb94e2b210cd2ae6e964bfc72a91012df78c5b6388988746'
$recognizedCompatibleV3DiagnosticTestSha256 = '59543e0254b44d5baa6ca548e58767ddee64a54b4a53065653317aa289f577ac'
$reviewedCompatibleV3PluginPrerequisites = [System.Collections.Generic.List[object]]::new()
foreach ($sourceFile in $v3BasePluginFiles) {
    $relativePath = Get-RelativePath -Root $sourcePluginRoot -FullName $sourceFile.FullName
    $acceptedPrior = @()
    if ($relativePath -ceq 'README.md') {
        # The reviewed V3 installer intentionally preserves this exact legacy
        # README; it is documentation-only and not a runtime prerequisite.
        $acceptedPrior = @($recognizedInstalledV3ReadmeSha256)
    }
    elseif ($relativePath -ceq $v3DiagnosticCompatibleRelativePath) {
        $sourceHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($sourceHash -cne $expectedCurrentV3DiagnosticSha256) {
            throw "The repository Hero-V3 diagnostic source is not its exact reviewed current revision: $($sourceFile.FullName)"
        }
        # The live f483 revision differs only in the later-reviewed neutral
        # capture diagnostic string. It remains runtime-compatible with V4 and
        # is accepted strictly as a read-only prerequisite; V4 never upgrades
        # or overwrites the V3 implementation.
        $acceptedPrior = @($recognizedCompatibleV3DiagnosticSha256)
    }
    elseif ($relativePath -ceq $v3DiagnosticTestCompatibleRelativePath) {
        $sourceHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($sourceHash -cne $expectedCurrentV3DiagnosticTestSha256) {
            throw "The repository Hero-V3 diagnostic regression test is not its exact reviewed current revision: $($sourceFile.FullName)"
        }
        # This exact prior test predates only the terminal success-diagnostic
        # assertions corresponding to the compatible f483 implementation. It
        # is accepted read-only and is never copied over or upgraded by V4.
        $acceptedPrior = @($recognizedCompatibleV3DiagnosticTestSha256)
    }
    $installedHash = Assert-ExactInstalledPrerequisiteFile `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetPluginRoot $relativePath) `
        -Label "shared plugin/$relativePath" `
        -AdditionalAcceptedDestinationSha256 $acceptedPrior
    $sourceHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($installedHash -cne $sourceHash) {
        $reviewedCompatibleV3PluginPrerequisites.Add([PSCustomObject]@{
            RelativePath = $relativePath
            InstalledSha256 = $installedHash
            RepositorySha256 = $sourceHash
            Disposition = 'REVIEWED_COMPATIBLE_READ_ONLY'
        })
    }
}
foreach ($requiredInstalledPackage in @($targetV3Map, $targetV3Mesh)) {
    if (-not (Test-Path -LiteralPath $requiredInstalledPackage -PathType Leaf)) {
        throw "Hero-V4 requires the separately imported/migrated Hero-V3 package before installation: $requiredInstalledPackage"
    }
}
$installedV3TextureCount = Assert-ExactInstalledHeroV3TextureRoster `
    -GeneratedManifestPath $sourceV3GeneratedManifest `
    -TargetTextureRoot $targetV3TextureRoot

$geometryRelativePaths = @($geometryFiles | ForEach-Object {
    Get-RelativePath -Root $sourceGeometryRoot -FullName $_.FullName
})
$materialRelativePaths = @($materialFiles | ForEach-Object {
    Get-RelativePath -Root $sourceMaterialRoot -FullName $_.FullName
})
Assert-NoUnexpectedTargetFiles `
    -TargetRoot $targetGeometryRoot `
    -ExpectedRelativePaths $geometryRelativePaths `
    -Label 'Target Hero-V4 geometry source namespace'
Assert-NoUnexpectedTargetFiles `
    -TargetRoot $targetMaterialRoot `
    -ExpectedRelativePaths $materialRelativePaths `
    -Label 'Target HeroMaterialsV4 source namespace'

foreach ($sourceFile in $geometryFiles) {
    $relativePath = Get-RelativePath -Root $sourceGeometryRoot -FullName $sourceFile.FullName
    Assert-NewOrEqualHashFile `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetGeometryRoot $relativePath)
}
foreach ($sourceFile in $materialFiles) {
    $relativePath = Get-RelativePath -Root $sourceMaterialRoot -FullName $sourceFile.FullName
    Assert-NewOrEqualHashFile `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetMaterialRoot $relativePath)
}
foreach ($sourceFile in $pluginFiles) {
    $relativePath = Get-RelativePath -Root $sourcePluginRoot -FullName $sourceFile.FullName
    Assert-NewOrEqualHashFile `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetPluginRoot $relativePath)
}

$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
    $targetVisualSettings,
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v3.umap'),
    (Join-Path $resolvedProject 'Content\TRIAD\Istana'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV2'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV3'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV2'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV3'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV2'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV3'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaDigitalTwin\IstanaPublicView'),
    (Join-Path $targetPluginRoot 'TRIADSensorFusion.uplugin'),
    (Join-Path $targetPluginRoot 'README.md'),
    (Join-Path $targetPluginRoot 'Source\TRIADSensorFusionEditor\TRIADSensorFusionEditor.Build.cs'),
    (Join-Path $targetPluginRoot 'Source\TRIADSensorFusionEditor\Public\TRIADIstanaPublicViewHeroV3EditorLibrary.h'),
    (Join-Path $targetPluginRoot 'Source\TRIADSensorFusionEditor\Private\TRIADIstanaPublicViewHeroV3EditorLibrary.cpp'),
    (Join-Path $targetPluginRoot 'Tests\test_istana_public_view_hero_v3_integration_contract.py'),
    (Join-Path $targetPluginRoot 'Resources\IstanaPublicViewHeroV3.integration.freeze.json')
)
$protectedPaths += @($v3BasePluginFiles | ForEach-Object {
    $relativePath = Get-RelativePath -Root $sourcePluginRoot -FullName $_.FullName
    Join-Path $targetPluginRoot $relativePath
})
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
$addedFiles = [System.Collections.Generic.List[string]]::new()
$equalFiles = [System.Collections.Generic.List[string]]::new()
$installError = $null
try {
    foreach ($sourceFile in $geometryFiles) {
        $relativePath = Get-RelativePath -Root $sourceGeometryRoot -FullName $sourceFile.FullName
        $targetFile = Join-Path $targetGeometryRoot $relativePath
        $disposition = Copy-NewOrEqualHashFile -SourceFile $sourceFile.FullName -DestinationFile $targetFile
        if ($disposition -ceq 'ADDED') { $addedFiles.Add($targetFile) } else { $equalFiles.Add($targetFile) }
    }
    foreach ($sourceFile in $materialFiles) {
        $relativePath = Get-RelativePath -Root $sourceMaterialRoot -FullName $sourceFile.FullName
        $targetFile = Join-Path $targetMaterialRoot $relativePath
        $disposition = Copy-NewOrEqualHashFile -SourceFile $sourceFile.FullName -DestinationFile $targetFile
        if ($disposition -ceq 'ADDED') { $addedFiles.Add($targetFile) } else { $equalFiles.Add($targetFile) }
    }

    # Validate the exact installed V4 source trees before publishing any V4
    # plugin byte. If validation fails, no editor-callable V4 surface exists.
    Assert-NoUnexpectedTargetFiles `
        -TargetRoot $targetGeometryRoot `
        -ExpectedRelativePaths $geometryRelativePaths `
        -Label 'Installed Hero-V4 geometry source namespace'
    Assert-NoUnexpectedTargetFiles `
        -TargetRoot $targetMaterialRoot `
        -ExpectedRelativePaths $materialRelativePaths `
        -Label 'Installed HeroMaterialsV4 source namespace'
    Invoke-FailClosedSourceValidation `
        -PythonExecutable $pythonExecutable `
        -GeometryRoot $targetGeometryRoot `
        -MaterialRoot $targetMaterialRoot

    # These are the only V4 plugin files. They are intentionally published
    # last, after every input/precondition/collision check and target-source
    # validation has passed.
    foreach ($sourceFile in $pluginFiles) {
        $relativePath = Get-RelativePath -Root $sourcePluginRoot -FullName $sourceFile.FullName
        $targetFile = Join-Path $targetPluginRoot $relativePath
        $disposition = Copy-NewOrEqualHashFile -SourceFile $sourceFile.FullName -DestinationFile $targetFile
        if ($disposition -ceq 'ADDED') { $addedFiles.Add($targetFile) } else { $equalFiles.Add($targetFile) }
    }
    Assert-NoHeroV4GeneratedPythonCache -TargetRoot $targetPluginRoot -Label 'installed target project plugin'
    Assert-ExactSharedHeroV3VisualAcceptanceSettings -SettingsPath $targetVisualSettings | Out-Null
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
    Operation = 'InstallIstanaPublicViewHeroV4DevelopmentAssets'
    Succeeded = $true
    Project = $resolvedProject
    V3BaseInstallerCalled = $false
    V3BaseInstallerSha256 = $baseInstallerSha256
    V3SourceAndProfilePrerequisitesByteExact = $true
    V3PluginPrerequisitesCurrentOrReviewedCompatible = $true
    ReviewedCompatibleV3PluginPrerequisiteCount = $reviewedCompatibleV3PluginPrerequisites.Count
    ReviewedCompatibleV3PluginPrerequisites = @($reviewedCompatibleV3PluginPrerequisites)
    V3PluginPrerequisitesOverwritten = $false
    ImportedV3PackageRosterPresent = $true
    ImportedV3SemanticValidationDeferredToV4EditorGate = $true
    FreshProjectRequiresSeparateV3Install = $true
    InstalledV3GeometrySourceFileCount = $installedV3GeometryFileCount
    InstalledV3MaterialSourceFileCount = $installedV3MaterialFileCount
    InstalledV3PublicReferenceFileCount = $installedV3ReferenceFileCount
    InstalledV3BasePluginFileCount = $v3BasePluginFiles.Count
    InstalledV3TexturePackageCount = $installedV3TextureCount
    GeometrySourceAssets = $targetGeometryRoot
    HeroMaterialSourceAssets = $targetMaterialRoot
    VisualAcceptanceSettings = $targetVisualSettings
    VisualAcceptanceSettingsSha256 = $visualSettingsSha256
    SharedHeroV3VisualProfileReused = $true
    GeometryFreezeValidated = $true
    MaterialFreezeValidated = $true
    IntegrationFreezeInstalledAdditively = $true
    GeometryFileCount = $geometryFiles.Count
    MaterialFileCount = $materialFiles.Count
    ExplicitV4PluginFileCount = $pluginFiles.Count
    AddedFileCount = $addedFiles.Count
    CurrentEqualFileCount = $equalFiles.Count
    AddedFiles = @($addedFiles)
    CurrentEqualFiles = @($equalFiles)
    RecognizedPriorHashReplacementUsed = $false
    UnrecognizedDifferentFilesOverwritten = $false
    V1V2V3ProtectedStateMutated = $false
    RendererSettingsMutated = $false
    NaniteRequired = $false
    ShaderModel6Required = $false
}
