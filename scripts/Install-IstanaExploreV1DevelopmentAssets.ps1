[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [ValidateNotNullOrEmpty()]
    [string] $UnrealEngineRoot = 'C:\Program Files\Epic Games\UE_5.5',

    [switch] $Compile,

    [switch] $ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-ProjectDirectory {
    param([string] $Path)
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $directory = if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        if ([IO.Path]::GetExtension($resolved) -ine '.uproject') {
            throw "ProjectPath is not a .uproject: $resolved"
        }
        Split-Path -Parent $resolved
    }
    else { $resolved }
    $projectFile = Join-Path $directory 'TRIAD.uproject'
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        throw "The exact TRIAD.uproject is missing: $projectFile"
    }
    return [IO.Path]::GetFullPath($directory).TrimEnd('\')
}

function Get-RelativeInstallFiles {
    param([string] $Root)
    return @(
        Get-ChildItem -LiteralPath $Root -Recurse -File -Force |
            Where-Object {
                $_.FullName -notmatch '[\\/]__pycache__[\\/]' -and
                $_.Extension -notin @('.pyc', '.pyo') -and
                $_.Name -notlike '*.partial'
            } |
            Sort-Object FullName |
            ForEach-Object {
                [PSCustomObject]@{
                    Relative = $_.FullName.Substring($Root.Length).TrimStart('\')
                    Source = $_.FullName
                    Length = $_.Length
                    Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
                }
            }
    )
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourcePlugin = Join-Path $repositoryRoot 'unreal\Plugins\TRIADSensorFusion'
$sourceExplore = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicViewExploreV1'
$projectDirectory = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
$targetPlugin = Join-Path $projectDirectory 'Plugins\TRIADSensorFusion'
$targetExplore = Join-Path $projectDirectory 'SourceAssets\IstanaPublicViewExploreV1'

$validator = Join-Path $sourceExplore 'validate_explore_v1.py'
& py -3 -B $validator | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw 'Repository Explore V1 source validation failed; no project files were changed.'
}

$pluginRelative = @(
    'Source\TRIADSensorFusion\Public\TRIADIstanaFreeRoamPawn.h',
    'Source\TRIADSensorFusion\Private\TRIADIstanaFreeRoamPawn.cpp',
    'Source\TRIADSensorFusion\Private\TRIADIstanaExploreGameMode.h',
    'Source\TRIADSensorFusion\Private\TRIADIstanaExploreGameMode.cpp',
    'Source\TRIADSensorFusion\Public\TRIADIstanaExploreLandscapeActor.h',
    'Source\TRIADSensorFusion\Private\TRIADIstanaExploreLandscapeActor.cpp',
    'Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreEditorLibrary.h',
    'Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreEditorLibrary.cpp',
    'Tests\test_istana_explore_free_roam_contract.py',
    'Tests\test_istana_explore_map_editor_contract.py'
)

$installRows = @()
foreach ($relative in $pluginRelative) {
    $source = Join-Path $sourcePlugin $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required Explore plugin source is missing: $source"
    }
    $installRows += [PSCustomObject]@{
        Source = $source
        Destination = Join-Path $targetPlugin $relative
        Length = (Get-Item -LiteralPath $source).Length
        Sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    }
}
foreach ($row in Get-RelativeInstallFiles -Root $sourceExplore) {
    $installRows += [PSCustomObject]@{
        Source = $row.Source
        Destination = Join-Path $targetExplore $row.Relative
        Length = $row.Length
        Sha256 = $row.Sha256
    }
}

# Refuse every mismatch before making the first additive copy.
foreach ($row in $installRows) {
    if (Test-Path -LiteralPath $row.Destination -PathType Leaf) {
        $existing = Get-Item -LiteralPath $row.Destination
        $digest = (Get-FileHash -LiteralPath $row.Destination -Algorithm SHA256).Hash
        if ($existing.Length -ne $row.Length -or $digest -cne $row.Sha256) {
            throw "Explore installer refuses to overwrite a mismatched existing file: $($row.Destination)"
        }
    }
}

$preflight = [PSCustomObject]@{
    Operation = 'InstallIstanaExploreV1DevelopmentAssets'
    ValidateOnly = [bool] $ValidateOnly
    ProjectFile = $projectFile
    SourceFiles = $installRows.Count
    SourceValidation = 'PASS'
    AdditiveOnly = $true
    CompileRequested = [bool] $Compile
}
if ($ValidateOnly) { return $preflight }
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close every Unreal Editor before installing or compiling Explore V1.'
}

$copied = 0
foreach ($row in $installRows) {
    if (-not (Test-Path -LiteralPath $row.Destination -PathType Leaf)) {
        $directory = Split-Path -Parent $row.Destination
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        Copy-Item -LiteralPath $row.Source -Destination $row.Destination
        ++$copied
    }
}
foreach ($row in $installRows) {
    if (-not (Test-Path -LiteralPath $row.Destination -PathType Leaf) -or
        (Get-Item -LiteralPath $row.Destination).Length -ne $row.Length -or
        (Get-FileHash -LiteralPath $row.Destination -Algorithm SHA256).Hash -cne
            $row.Sha256) {
        throw "Installed Explore file failed exact readback: $($row.Destination)"
    }
}

$compiled = $false
if ($Compile) {
    $build = Join-Path $UnrealEngineRoot 'Engine\Build\BatchFiles\Build.bat'
    if (-not (Test-Path -LiteralPath $build -PathType Leaf)) {
        throw "Unreal Build.bat is missing: $build"
    }
    & $build UnrealEditor Win64 Development $projectFile `
        -DisablePlugin=AirSim `
        -Module=TRIADSensorFusion `
        -Module=TRIADSensorFusionEditor `
        -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=1 -NoUBA -NoUBALocal
    if ($LASTEXITCODE -ne 0) {
        throw 'Explore source installed, but the live-project Unreal compile failed. Keep the editor closed and inspect the UBT log before launch.'
    }
    $compiled = $true
}

$preflight | Add-Member -NotePropertyName CopiedFiles -NotePropertyValue $copied
$preflight | Add-Member -NotePropertyName InstalledFilesVerified -NotePropertyValue $true
$preflight | Add-Member -NotePropertyName Compiled -NotePropertyValue $compiled
$nextStep = if ($compiled) {
    'Start-IstanaExploreV1Editor.ps1, then Build-IstanaExploreV1Map.ps1.'
}
else {
    'Compile/rebuild the TRIAD plugin with the editor closed, then start the Explore editor.'
}
$preflight | Add-Member -NotePropertyName NextStep -NotePropertyValue $nextStep
$preflight
