[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [string] $EngineRoot = 'C:\Program Files\Epic Games\UE_5.5',

    [switch] $Compile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$resolved = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
    Split-Path -Parent $resolved
} else { $resolved }
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Exact target TRIAD.uproject is missing: $projectFile"
}
if (@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'").Count -ne 0) {
    throw 'Close Unreal Editor before installing or compiling Explore V2 development files.'
}

$validator = Join-Path $repoRoot 'unreal\SourceAssets\IstanaPublicViewExploreV2\validate_explore_v2.py'
& py -3 -B $validator | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw 'Repository Explore V2 source validation failed.'
}

$files = @(
    'unreal\Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV2LandscapeActor.h',
    'unreal\Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV2LandscapeActor.cpp',
    'unreal\Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV2EditorLibrary.h',
    'unreal\Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV2EditorLibrary.cpp',
    'unreal\Plugins\TRIADSensorFusion\Tests\test_istana_explore_v2_map_editor_contract.py'
)
$installed = [System.Collections.Generic.List[string]]::new()
foreach ($relative in $files) {
    $source = Join-Path $repoRoot $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required stopped V2 source is missing: $source"
    }
    $targetRelative = $relative.Substring('unreal\'.Length)
    $target = Join-Path $projectDirectory $targetRelative
    $targetParent = Split-Path -Parent $target
    New-Item -ItemType Directory -Force -Path $targetParent | Out-Null
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($sourceHash -cne $targetHash) {
            throw "Add-only install refuses a different existing V2 source file: $target"
        }
    } else {
        Copy-Item -LiteralPath $source -Destination $target
        $installed.Add($target)
    }
}

$sourcePack = Join-Path $repoRoot 'unreal\SourceAssets\IstanaPublicViewExploreV2'
$targetPack = Join-Path $projectDirectory 'SourceAssets\IstanaPublicViewExploreV2'
if (Test-Path -LiteralPath $targetPack) {
    $sourceRows = @(Get-ChildItem $sourcePack -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($sourcePack.Length).TrimStart('\')
        "$relative|$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)"
    } | Sort-Object)
    $targetRows = @(Get-ChildItem $targetPack -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($targetPack.Length).TrimStart('\')
        "$relative|$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)"
    } | Sort-Object)
    if ([string]::Join("`n", $sourceRows) -cne [string]::Join("`n", $targetRows)) {
        throw "Add-only install refuses a different existing V2 source pack: $targetPack"
    }
} else {
    Copy-Item -LiteralPath $sourcePack -Destination $targetPack -Recurse
    $installed.Add($targetPack)
}

if ($Compile) {
    $build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    if (-not (Test-Path -LiteralPath $build -PathType Leaf)) {
        throw "UE build tool is missing: $build"
    }
    & $build UnrealEditor Win64 Development $projectFile `
        -DisablePlugin=AirSim `
        -Module=TRIADSensorFusion `
        -Module=TRIADSensorFusionEditor `
        -WaitMutex `
        -NoHotReloadFromIDE `
        -MaxParallelActions=1 `
        -NoUBA `
        -NoUBALocal
    if ($LASTEXITCODE -ne 0) {
        throw "Module-scoped Explore V2 UE build failed with exit code $LASTEXITCODE."
    }
}

[PSCustomObject]@{
    Operation = 'Install-IstanaExploreV2DevelopmentAssets'
    AddedPaths = @($installed)
    AddOnly = $true
    Compiled = [bool]$Compile
    ProtectedMapsOrContentTouched = $false
}
