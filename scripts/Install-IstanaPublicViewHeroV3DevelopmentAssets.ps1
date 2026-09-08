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
$expectedVisualSettingsSha256 = '6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c'
$expectedVisualSettingsBytes = 1022

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
        throw 'CRITICAL: protected v1 maps/assets or renderer settings changed file count during hero-v3 source installation.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected v1 map/asset or renderer setting changed during hero-v3 source installation: $($entry.Key)"
        }
    }
}

function Get-InstallSourceFiles {
    param([string] $SourceRoot)

    return @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache|\.venv)[\\/]' -and
        $_.FullName -notmatch '[\\/]HeroMaterialsV3_Determinism_[a-z0-9_]+(?:[\\/]|$)' -and
        $_.Extension -ne '.pyc'
    } | Sort-Object FullName)
}

function Assert-NoHeroMaterialsV3DeterminismTemporaryTree {
    param([string] $SourceRoot)

    $temporaryDirectories = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -Directory -Force |
        Where-Object {
            $_.Name -match '^HeroMaterialsV3_Determinism_[a-z0-9_]+$'
        })
    if ($temporaryDirectories.Count -ne 0) {
        $paths = @($temporaryDirectories | ForEach-Object FullName) -join ', '
        throw "Refusing hero-v3 installation while a material determinism temporary tree exists: $paths"
    }
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
        throw "Refusing hero-v3 installation because $Label contains generated Python cache state: $paths"
    }
}

function Assert-NoHeroV3GeneratedPythonCache {
    param(
        [string] $TargetRoot,
        [string] $Label
    )

    if (-not (Test-Path -LiteralPath $TargetRoot -PathType Container)) {
        return
    }
    $v3CacheFiles = @(Get-ChildItem -LiteralPath $TargetRoot -Recurse -File -Force |
        Where-Object {
            $_.Name -match '^test_istana_public_view_hero_v3_integration_contract\..*\.py[co]$'
        })
    if ($v3CacheFiles.Count -ne 0) {
        $paths = @($v3CacheFiles | ForEach-Object FullName) -join ', '
        throw "Refusing hero-v3 installation because $Label contains Hero-V3 generated Python cache state: $paths"
    }
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
            if ($LASTEXITCODE -eq 0 -and
                (($versions -join '').Trim()) -eq '12.3.0|2.3.5') {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }
    throw 'No validated material Python was found. Use the pinned HeroMaterialsV2 .venv or pass -MaterialPythonExecutable with exact Pillow 12.3.0 and NumPy 2.3.5; system-Python fallback is forbidden.'
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
        throw "Refusing to overwrite a different existing hero-v3 source file: $DestinationFile"
    }
}

function Get-RecognizedPluginUpdateDisposition {
    param(
        [string] $SourceFile,
        [string] $DestinationFile,
        [string] $RelativePath,
        [hashtable] $RecognizedPriorHashes,
        [hashtable] $RecognizedPriorReplacementHashes,
        [hashtable] $RecognizedReplacementSourceHashes
    )

    if (-not (Test-Path -LiteralPath $DestinationFile -PathType Leaf)) {
        return 'MISSING_ADDITIVE'
    }
    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($sourceHash -ceq $destinationHash) {
        return 'CURRENT_EQUAL'
    }
    if ($RecognizedPriorHashes.ContainsKey($RelativePath) -and
        $destinationHash -ceq $RecognizedPriorHashes[$RelativePath]) {
        return 'RECOGNIZED_PRIOR'
    }
    if ($RecognizedPriorReplacementHashes.ContainsKey($RelativePath) -and
        (@($RecognizedPriorReplacementHashes[$RelativePath]) -ccontains
            $destinationHash)) {
        if (-not $RecognizedReplacementSourceHashes.ContainsKey($RelativePath) -or
            $sourceHash -cne $RecognizedReplacementSourceHashes[$RelativePath]) {
            throw "Recognized plugin upgrade source is not the exact reviewed replacement: $SourceFile"
        }
        return 'RECOGNIZED_PRIOR_REPLACE'
    }
    throw "Refusing to overwrite an unknown or user-modified target plugin file: $DestinationFile"
}

function Assert-ExactHeroV3VisualAcceptanceSettings {
    param([string] $SettingsPath)

    $item = Get-Item -LiteralPath $SettingsPath
    $digest = (Get-FileHash -LiteralPath $SettingsPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.Length -ne $expectedVisualSettingsBytes -or
        $digest -cne $expectedVisualSettingsSha256) {
        throw "Refusing to install a Hero-V3 visual-acceptance profile other than the exact frozen repository file: $SettingsPath"
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
        throw 'The frozen Hero-V3 profile must contain both RPC compatibility keys exactly once and false, use ComputerVision with loopback API address, and declare no lidar.'
    }
    return $digest
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
            throw "Refusing to overwrite a different existing hero-v3 source file: $DestinationFile"
        }
        return
    }
    $destinationDirectory = Split-Path -Parent $DestinationFile
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    $stageFile = "$DestinationFile.hero_v3_add_$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        [System.IO.File]::Copy($SourceFile, $stageFile, $false)
        $stageHash = (Get-FileHash -LiteralPath $stageFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($stageHash -cne $sourceHash) {
            throw "Hero-v3 additive source staging hash mismatch: $SourceFile"
        }
        try {
            # The two-argument Move is an atomic, no-overwrite publish on the
            # same volume. A target created after the absence check wins.
            [System.IO.File]::Move($stageFile, $DestinationFile)
        }
        catch {
            $publishError = $_
            if (Test-Path -LiteralPath $DestinationFile -PathType Leaf) {
                $collisionSourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
                $collisionDestinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($collisionSourceHash -ceq $sourceHash -and
                    $collisionDestinationHash -ceq $sourceHash) {
                    return
                }
                throw "Refusing to overwrite a file created during hero-v3 additive publication: $DestinationFile"
            }
            throw $publishError
        }
        $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($destinationHash -cne $sourceHash) {
            throw "Installed hero-v3 source file hash mismatch: $DestinationFile"
        }
    }
    finally {
        if (Test-Path -LiteralPath $stageFile -PathType Leaf) {
            [System.IO.File]::Delete($stageFile)
        }
    }
}

function Copy-RecognizedPriorHashUpgrade {
    param(
        [string] $SourceFile,
        [string] $DestinationFile,
        [string] $RelativePath,
        [hashtable] $RecognizedPriorReplacementHashes,
        [hashtable] $RecognizedReplacementSourceHashes
    )

    if (-not $RecognizedPriorReplacementHashes.ContainsKey($RelativePath) -or
        -not $RecognizedReplacementSourceHashes.ContainsKey($RelativePath) -or
        -not (Test-Path -LiteralPath $DestinationFile -PathType Leaf)) {
        throw "Refusing an unrecognized or missing plugin upgrade target: $DestinationFile"
    }
    $sourceHash = (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $priorHashes = @($RecognizedPriorReplacementHashes[$RelativePath])
    $replacementHash = [string] $RecognizedReplacementSourceHashes[$RelativePath]
    $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $priorHash = $destinationHash
    if ($priorHashes -cnotcontains $destinationHash -or
        $sourceHash -cne $replacementHash -or
        $sourceHash -ceq $priorHash) {
        throw "Recognized plugin upgrade precondition changed before replacement: $DestinationFile"
    }

    $stageFile = "$DestinationFile.hero_v3_upgrade_$([Guid]::NewGuid().ToString('N')).tmp"
    $backupFile = "$DestinationFile.hero_v3_backup_$([Guid]::NewGuid().ToString('N')).tmp"
    $rollbackDiscardFile = "$DestinationFile.hero_v3_failed_replacement_$([Guid]::NewGuid().ToString('N')).tmp"
    $replaced = $false
    $committed = $false
    $rollbackExpectedHash = $null
    try {
        Copy-Item -LiteralPath $SourceFile -Destination $stageFile
        $stageHash = (Get-FileHash -LiteralPath $stageFile -Algorithm SHA256).Hash.ToLowerInvariant()
        $destinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($stageHash -cne $replacementHash -or $destinationHash -cne $priorHash) {
            throw "Recognized plugin upgrade staging or target recheck failed: $DestinationFile"
        }
        [System.IO.File]::Replace($stageFile, $DestinationFile, $backupFile, $true)
        $replaced = $true
        $rollbackExpectedHash = (Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($rollbackExpectedHash -cne $priorHash) {
            throw "Recognized plugin upgrade detected a late target change during replacement: $DestinationFile"
        }
        $installedHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($installedHash -cne $replacementHash) {
            throw "Recognized plugin upgrade hash verification failed: $DestinationFile"
        }
        $committed = $true
    }
    catch {
        $operationError = $_
        if ($replaced) {
            try {
                if (-not (Test-Path -LiteralPath $backupFile -PathType Leaf)) {
                    throw "Recognized plugin upgrade backup is absent after replacement: $backupFile"
                }
                if ([string]::IsNullOrEmpty($rollbackExpectedHash)) {
                    $rollbackExpectedHash = (Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash.ToLowerInvariant()
                }
                $currentDestinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($currentDestinationHash -cne $replacementHash) {
                    throw "Destination changed after the reviewed replacement; preserve the destination and displaced backup for recovery."
                }
                [System.IO.File]::Replace(
                    $backupFile,
                    $DestinationFile,
                    $rollbackDiscardFile,
                    $true)
                $restoredHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
                if (-not (Test-Path -LiteralPath $rollbackDiscardFile -PathType Leaf)) {
                    throw "Recognized plugin upgrade rollback discard is absent after replacement: $rollbackDiscardFile"
                }
                $rollbackDiscardHash = (Get-FileHash -LiteralPath $rollbackDiscardFile -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($rollbackDiscardHash -cne $replacementHash) {
                    # A writer won after the current-destination check. Put
                    # its exact displaced bytes back and retain our prior
                    # backup instead of deleting or overwriting either side.
                    $interveningHash = $rollbackDiscardHash
                    [System.IO.File]::Replace(
                        $rollbackDiscardFile,
                        $DestinationFile,
                        $backupFile,
                        $true)
                    $recoveredDestinationHash = (Get-FileHash -LiteralPath $DestinationFile -Algorithm SHA256).Hash.ToLowerInvariant()
                    $recoveredBackupHash = (Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash.ToLowerInvariant()
                    if ($recoveredDestinationHash -cne $interveningHash -or
                        $recoveredBackupHash -cne $restoredHash) {
                        throw "Late rollback race recovery did not preserve the exact intervening and displaced digests."
                    }
                    throw "Destination changed during rollback; its exact bytes were restored and the displaced backup was preserved."
                }
                if ($restoredHash -cne $rollbackExpectedHash) {
                    throw "Recognized plugin upgrade rollback did not restore the exact displaced digest; preserve recovery files."
                }
                [System.IO.File]::Delete($rollbackDiscardFile)
            }
            catch {
                throw "Recognized plugin upgrade failed and exact rollback also failed. Preserve '$backupFile' and '$rollbackDiscardFile' and stop: $($_.Exception.Message)"
            }
        }
        throw $operationError
    }
    finally {
        if (Test-Path -LiteralPath $stageFile -PathType Leaf) {
            [System.IO.File]::Delete($stageFile)
        }
        if ($committed -and (Test-Path -LiteralPath $backupFile -PathType Leaf)) {
            [System.IO.File]::Delete($backupFile)
        }
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

    $geometryValidator = Join-Path $GeometryRoot 'validate_hero_v3.py'
    $geometryOutput = & $PythonExecutable -B $geometryValidator `
        --root $GeometryRoot `
        --output (Join-Path $GeometryRoot 'Generated') `
        --freeze (Join-Path $GeometryRoot 'hero_v3.freeze.json') 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: hero-v3 geometry validation failed. $($geometryOutput -join [Environment]::NewLine)"
    }

    $materialBuilder = Join-Path $MaterialRoot 'build_hero_materials_v3.py'
    $materialOutput = & $PythonExecutable -B $materialBuilder `
        --source-dir (Join-Path $MaterialRoot 'Source\PolyHaven') `
        --source-manifest (Join-Path $MaterialRoot 'Source\source_manifest.json') `
        --output (Join-Path $MaterialRoot 'Generated') `
        --validate-only 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: HeroMaterialsV3 validation failed. $($materialOutput -join [Environment]::NewLine)"
    }
}

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor process before installing hero-v3 plugin/source assets.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourcePluginRoot = Join-Path $repositoryRoot 'unreal\Plugins\TRIADSensorFusion'
$sourceGeometryRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicViewV3'
$sourceMaterialRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV3'
$sourceReferenceRoot = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaDigitalTwin\IstanaPublicView'
$sourceVisualSettings = Join-Path $repositoryRoot 'unreal\Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'
$integrationFreeze = Join-Path $repositoryRoot 'unreal\Plugins\TRIADSensorFusion\Resources\IstanaPublicViewHeroV3.integration.freeze.json'
$resolvedProject = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$targetGeometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV3'
$targetMaterialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV3'
$targetReferenceRoot = Join-Path $resolvedProject 'SourceAssets\IstanaDigitalTwin\IstanaPublicView'
$targetPluginRoot = Join-Path $resolvedProject 'Plugins\TRIADSensorFusion'
$targetVisualSettings = Join-Path $resolvedProject 'Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'

$repositoryMaterialPythonV2 = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV2\.venv\Scripts\python.exe'
$pythonExecutable = Resolve-HeroMaterialPython `
    -ExplicitOverride $MaterialPythonExecutable `
    -PinnedCandidates @($repositoryMaterialPythonV2)

foreach ($requiredPath in @(
    $sourcePluginRoot,
    $sourceGeometryRoot,
    $sourceMaterialRoot,
    $sourceReferenceRoot,
    $sourceVisualSettings,
    (Join-Path $sourceGeometryRoot 'validate_hero_v3.py'),
    (Join-Path $sourceGeometryRoot 'Generated\SM_IstanaPublicViewV3_Building_Hero.obj'),
    (Join-Path $sourceMaterialRoot 'build_hero_materials_v3.py'),
    $integrationFreeze,
    $projectFile
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required hero-v3 installation input is missing: $requiredPath"
    }
}
Assert-ExactHeroV3VisualAcceptanceSettings -SettingsPath $sourceVisualSettings | Out-Null

# The material builder creates this exact root-local prefix only while its
# byte-identical rebuild gate is active. Refuse installation before enumerating
# any copy set, and retain a component-level exclusion as defence in depth.
Assert-NoHeroMaterialsV3DeterminismTemporaryTree -SourceRoot $sourceMaterialRoot
Assert-NoGeneratedPythonCache `
    -SourceRoot $sourcePluginRoot `
    -Label 'repository plugin source'
Assert-NoHeroV3GeneratedPythonCache `
    -TargetRoot $targetPluginRoot `
    -Label 'target project plugin'

Invoke-FailClosedSourceValidation `
    -PythonExecutable $pythonExecutable `
    -GeometryRoot $sourceGeometryRoot `
    -MaterialRoot $sourceMaterialRoot

$geometryFiles = Get-InstallSourceFiles -SourceRoot $sourceGeometryRoot
$materialFiles = Get-InstallSourceFiles -SourceRoot $sourceMaterialRoot
$referenceFiles = Get-InstallSourceFiles -SourceRoot $sourceReferenceRoot
$pluginFiles = @(
    Get-Item -LiteralPath (Join-Path $sourcePluginRoot 'TRIADSensorFusion.uplugin')
    Get-Item -LiteralPath (Join-Path $sourcePluginRoot 'README.md')
    Get-InstallSourceFiles -SourceRoot (Join-Path $sourcePluginRoot 'Source')
    Get-InstallSourceFiles -SourceRoot (Join-Path $sourcePluginRoot 'Resources')
    Get-InstallSourceFiles -SourceRoot (Join-Path $sourcePluginRoot 'Tests')
)
if ($geometryFiles.Count -lt 1 -or $materialFiles.Count -lt 1 -or
    $referenceFiles.Count -lt 1 -or $pluginFiles.Count -lt 1) {
    throw 'ASSETS_MISSING: the frozen hero-v3 geometry, material, or licensed reference source tree is empty.'
}

# The established pre-V3 README is preserved byte-for-byte. The sole permitted
# permitted replacements are the exact previously reviewed Hero-V3 C++
# implementations whose frozen-material key typo and legacy-OBJ slot-order
# mismatch failed closed before any package save, the exact reviewed render-QA
# implementation that restores neutral exposure and scoped indirect fill, its
# exact diagnostic-correctness follow-up,
# the editor Build.cs whose missing direct StaticMeshDescription dependency
# failed at the modular link, plus their exact regression-test versions. Every
# other source/test file must be current-equal or additive; any unknown different
# hash is a hard stop.
$recognizedPriorPluginHashes = @{
    'README.md' = '584b43bd1c3fdfa0f5cf3d00167f7688c8fd01b4745505d05917d7a0b09ee8d4'
}
$recognizedPriorPluginReplacementHashes = @{
    'Source\TRIADSensorFusionEditor\Private\TRIADIstanaPublicViewHeroV3EditorLibrary.cpp' = @(
        'b79da60b0c793094b8e753bb7903af52623858b1f190cdb5cc3e79dbbbf9f53e',
        'aab3b1de39ac2536fb1886f76cb1e59b0ff8da3d94126e17ae8bada4b80463b8',
        '9273f8067378893658338ef4bd50363b3d1fc68a753ad33616b0dc88c9c6ce08',
        'c86b93a45cae283c84bbba822a816a76b7a516db0aa3d351ecb4561592c61706',
        'f48379a6c236b65553d2ef41fcea2c8932b53cc7e138da5daa667aa2a92857a9'
    )
    'Source\TRIADSensorFusionEditor\TRIADSensorFusionEditor.Build.cs' = @(
        '1df3a2115cd414072d5f9f895b52719cd0c69e23d3bc84582190bf8abb50dc84'
    )
    'Tests\test_istana_public_view_hero_v3_integration_contract.py' = @(
        '159b7ded558197544d72c44af86cb5209594f57c89f0e947e719b5617aab69bc',
        'c06d7eee5a8251a03ff9384cb5d2d41b677f32a5939781d8ee5e7fe6cd16186c',
        'f64bcd9b71bf106044fd62aca6d7f245038d498427c955e82d9c90a3c96651f0',
        'f1ff3b9057db2992e05f30d4a70507019deb7fca08aacbcc232178a5affa020f',
        '0a64ff660b87d7fd3e2d39faf3ebbd5812617096ed8a9a391879dddb12643e6b',
        '59543e0254b44d5baa6ca548e58767ddee64a54b4a53065653317aa289f577ac'
    )
}
$recognizedPluginReplacementSourceHashes = @{
    'Source\TRIADSensorFusionEditor\Private\TRIADIstanaPublicViewHeroV3EditorLibrary.cpp' = '3c2f916845c421070e7a64c25ea95cd8de2fbea95800f4db166edd3a7177fdee'
    'Source\TRIADSensorFusionEditor\TRIADSensorFusionEditor.Build.cs' = '34ba790e50b87d495c2eff41aa4afbc2798af8524dd891740dccab466df973f5'
    'Tests\test_istana_public_view_hero_v3_integration_contract.py' = 'd040466ec9eda50afb94e2b210cd2ae6e964bfc72a91012df78c5b6388988746'
}
$recognizedPriorPluginUpdates = [System.Collections.Generic.List[string]]::new()
$recognizedPriorPluginReplacements = [System.Collections.Generic.List[string]]::new()
foreach ($sourceFile in $pluginFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourcePluginRoot.Length).TrimStart('\')
    $disposition = Get-RecognizedPluginUpdateDisposition `
        -SourceFile $sourceFile.FullName `
        -DestinationFile (Join-Path $targetPluginRoot $relativePath) `
        -RelativePath $relativePath `
        -RecognizedPriorHashes $recognizedPriorPluginHashes `
        -RecognizedPriorReplacementHashes $recognizedPriorPluginReplacementHashes `
        -RecognizedReplacementSourceHashes $recognizedPluginReplacementSourceHashes
    if ($disposition -ceq 'RECOGNIZED_PRIOR') {
        $recognizedPriorPluginUpdates.Add($relativePath)
    }
    elseif ($disposition -ceq 'RECOGNIZED_PRIOR_REPLACE') {
        $recognizedPriorPluginReplacements.Add($relativePath)
    }
}
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
Assert-NonOverwritingFileCopy `
    -SourceFile $sourceVisualSettings `
    -DestinationFile $targetVisualSettings

$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v2.umap'),
    (Join-Path $resolvedProject 'Content\TRIAD\Istana'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV2'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV2'),
    (Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV2')
)
$legacyVisualSettings = Join-Path $resolvedProject 'Config\IstanaVisualAcceptance.settings.json'
if (Test-Path -LiteralPath $legacyVisualSettings -PathType Leaf) {
    # Existing V1/V2 profile bytes are immutable. On a fresh project the base
    # installer may add that established file, but Hero V3 never replaces it.
    $protectedPaths += $legacyVisualSettings
}
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
$installError = $null
try {
    # Install each reviewed plugin file independently. Current-equal files are
    # untouched, missing V3 files are additive, the exact pre-V3 README remains
    # preserved, and the reviewed fail-closed C++ fixes, direct editor-module
    # dependency, and regression test are replaced atomically only from their
    # exact prior digests. This avoids the legacy installer's broad recursive
    # -Force copy.
    foreach ($sourceFile in $pluginFiles) {
        $relativePath = $sourceFile.FullName.Substring($sourcePluginRoot.Length).TrimStart('\')
        $targetFile = Join-Path $targetPluginRoot $relativePath
        $disposition = Get-RecognizedPluginUpdateDisposition `
            -SourceFile $sourceFile.FullName `
            -DestinationFile $targetFile `
            -RelativePath $relativePath `
            -RecognizedPriorHashes $recognizedPriorPluginHashes `
            -RecognizedPriorReplacementHashes $recognizedPriorPluginReplacementHashes `
            -RecognizedReplacementSourceHashes $recognizedPluginReplacementSourceHashes
        if ($disposition -ceq 'RECOGNIZED_PRIOR') {
            continue
        }
        if ($disposition -ceq 'RECOGNIZED_PRIOR_REPLACE') {
            Copy-RecognizedPriorHashUpgrade `
                -SourceFile $sourceFile.FullName `
                -DestinationFile $targetFile `
                -RelativePath $relativePath `
                -RecognizedPriorReplacementHashes $recognizedPriorPluginReplacementHashes `
                -RecognizedReplacementSourceHashes $recognizedPluginReplacementSourceHashes
            continue
        }
        Copy-NewOrEqualHashFile `
            -SourceFile $sourceFile.FullName `
            -DestinationFile $targetFile
    }
    foreach ($sourceFile in $pluginFiles) {
        $relativePath = $sourceFile.FullName.Substring($sourcePluginRoot.Length).TrimStart('\')
        $targetFile = Join-Path $targetPluginRoot $relativePath
        $disposition = Get-RecognizedPluginUpdateDisposition `
            -SourceFile $sourceFile.FullName `
            -DestinationFile $targetFile `
            -RelativePath $relativePath `
            -RecognizedPriorHashes $recognizedPriorPluginHashes `
            -RecognizedPriorReplacementHashes $recognizedPriorPluginReplacementHashes `
            -RecognizedReplacementSourceHashes $recognizedPluginReplacementSourceHashes
        if ($disposition -notin @('CURRENT_EQUAL', 'RECOGNIZED_PRIOR')) {
            throw "Controlled Hero-V3 plugin installation did not reach an accepted state: $targetFile"
        }
    }
    Assert-NoGeneratedPythonCache `
        -SourceRoot $sourcePluginRoot `
        -Label 'repository plugin source after controlled installation'
    Assert-NoHeroV3GeneratedPythonCache `
        -TargetRoot $targetPluginRoot `
        -Label 'target project plugin after controlled installation'
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
    # V3 uses a dedicated, additive profile so the established V1/V2
    # visual-acceptance settings remain byte-for-byte untouched. The profile
    # carries both the upstream-documented and deployed-AirSim-1.8.1 RPC keys,
    # each false, and is never substituted with a user-global AirSim file.
    Copy-NewOrEqualHashFile `
        -SourceFile $sourceVisualSettings `
        -DestinationFile $targetVisualSettings
    Assert-ExactHeroV3VisualAcceptanceSettings -SettingsPath $targetVisualSettings | Out-Null
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
    Operation = 'InstallIstanaPublicViewHeroV3DevelopmentAssets'
    Succeeded = $true
    Project = $resolvedProject
    GeometrySourceAssets = $targetGeometryRoot
    HeroMaterialSourceAssets = $targetMaterialRoot
    LicensedReferenceSourceAssets = $targetReferenceRoot
    VisualAcceptanceSettings = $targetVisualSettings
    GeometryFileCount = $geometryFiles.Count
    MaterialFileCount = $materialFiles.Count
    LicensedReferenceFileCount = $referenceFiles.Count
    PluginFileCount = $pluginFiles.Count
    RecognizedPriorPluginFilesPreserved = @($recognizedPriorPluginUpdates)
    RecognizedPriorPluginFilesReplaced = @($recognizedPriorPluginReplacements)
    UnrecognizedDifferentFilesOverwritten = $false
    ProtectedV1ContentMutated = $false
    ExistingLegacyVisualSettingsMutated = $false
    RendererSettingsMutated = $false
    NaniteRequired = $false
    ShaderModel6Required = $false
    BroadLegacyForceInstallerCalled = $false
}
