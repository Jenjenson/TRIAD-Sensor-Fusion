<#
.SYNOPSIS
Opens the local TRIAD placement, drone-ingress, and detection dashboard.

.DESCRIPTION
The dashboard builds a deterministic study-mode replay in memory by default.
It is not live Unreal telemetry, not field calibration, and not a physical-site
authorization. Pass -ReplayPath to view a previously exported
triad.dashboard_replay.v1 artifact instead.

Pass -LiveUrl with the exact loopback /api/snapshot endpoint to render current
runtime nodes, tracks, and cue links over a separately labelled frozen replay
reference. The Python client rejects non-loopback URLs, redirects, oversized
responses, and unsafe or malformed detection semantics.
#>
[CmdletBinding()]
param(
    [string] $ReplayPath,

    [string] $ScenarioId = 'clear-rf',

    [ValidateRange(1.0, 3600.0)]
    [double] $DurationSeconds = 150.0,

    [ValidateRange(0.1, 60.0)]
    [double] $CadenceSeconds = 1.0,

    [ValidateRange(0.1, 16.0)]
    [double] $Speed = 1.0,

    [string] $LiveUrl,

    [ValidateRange(0.1, 10.0)]
    [double] $LivePollSeconds = 0.25,

    [ValidateRange(0.1, 5.0)]
    [double] $LiveTimeoutSeconds = 0.75,

    [switch] $Autoplay,

    [switch] $ValidateOnly,

    [string] $PythonExecutable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-DashboardPython {
    param([Parameter(Mandatory = $true)][string] $Executable)

    try {
        $probeOutput = & $Executable -c 'import tkinter; import numpy' 2>&1
        return $LASTEXITCODE -eq 0
    }
    catch {
        return $false
    }
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$coreRoot = Join-Path $repositoryRoot 'core'
$sourceRoot = Join-Path $coreRoot 'src'
$modulePath = Join-Path $sourceRoot `
    'singapore_sensor_fusion\placement\dashboard_app.py'
if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
    throw "TRIAD dashboard source is missing: $modulePath"
}

if ([string]::IsNullOrWhiteSpace($PythonExecutable)) {
    $candidatePaths = [System.Collections.Generic.List[string]]::new()
    foreach ($workspaceCandidate in @(
        (Join-Path $coreRoot '.venv\Scripts\python.exe'),
        'C:\Users\Lyz\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    )) {
        if (Test-Path -LiteralPath $workspaceCandidate -PathType Leaf) {
            $candidatePaths.Add((Resolve-Path -LiteralPath $workspaceCandidate).Path)
        }
    }
    foreach ($commandName in @('python.exe', 'py.exe')) {
        $candidate = Get-Command $commandName -ErrorAction SilentlyContinue
        if ($null -ne $candidate -and -not $candidatePaths.Contains($candidate.Source)) {
            $candidatePaths.Add($candidate.Source)
        }
    }
    if ($candidatePaths.Count -eq 0) {
        throw 'No Python launcher was found. Install Python 3.11+ with Tk support or pass -PythonExecutable.'
    }
    $resolvedPython = $null
    foreach ($candidatePath in $candidatePaths) {
        if (Test-DashboardPython -Executable $candidatePath) {
            $resolvedPython = $candidatePath
            break
        }
    }
    if ([string]::IsNullOrWhiteSpace($resolvedPython)) {
        throw 'No usable Python was found. Install Python 3.11+ with tkinter and numpy, or pass -PythonExecutable.'
    }
}
else {
    $resolvedPython = (Resolve-Path -LiteralPath $PythonExecutable).Path
    if (-not (Test-DashboardPython -Executable $resolvedPython)) {
        throw "The selected Python cannot import tkinter and numpy: $resolvedPython"
    }
}

$arguments = @(
    '-m',
    'singapore_sensor_fusion.placement.dashboard_app'
)
if (-not [string]::IsNullOrWhiteSpace($ReplayPath)) {
    $resolvedReplay = (Resolve-Path -LiteralPath $ReplayPath).Path
    $arguments += @('--replay', $resolvedReplay)
}
else {
    $arguments += @(
        '--scenario-id', $ScenarioId,
        '--duration-seconds', $DurationSeconds.ToString(
            [Globalization.CultureInfo]::InvariantCulture),
        '--cadence-seconds', $CadenceSeconds.ToString(
            [Globalization.CultureInfo]::InvariantCulture)
    )
}
$arguments += @(
    '--speed', $Speed.ToString([Globalization.CultureInfo]::InvariantCulture)
)
if (-not [string]::IsNullOrWhiteSpace($LiveUrl)) {
    $arguments += @(
        '--live-url', $LiveUrl,
        '--live-poll-seconds', $LivePollSeconds.ToString(
            [Globalization.CultureInfo]::InvariantCulture),
        '--live-timeout-seconds', $LiveTimeoutSeconds.ToString(
            [Globalization.CultureInfo]::InvariantCulture)
    )
}
if ($Autoplay) {
    $arguments += '--autoplay'
}
if ($ValidateOnly) {
    $arguments += '--validate-only'
}

$previousPythonPath = [Environment]::GetEnvironmentVariable('PYTHONPATH', 'Process')
try {
    $env:PYTHONPATH = if ([string]::IsNullOrWhiteSpace($previousPythonPath)) {
        $sourceRoot
    }
    else {
        "$sourceRoot$([IO.Path]::PathSeparator)$previousPythonPath"
    }
    & $resolvedPython @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "TRIAD dashboard exited with code $LASTEXITCODE."
    }
}
finally {
    [Environment]::SetEnvironmentVariable(
        'PYTHONPATH', $previousPythonPath, 'Process')
}
