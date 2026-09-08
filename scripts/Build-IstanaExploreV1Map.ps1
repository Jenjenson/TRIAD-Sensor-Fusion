[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
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

function Invoke-ExploreCall {
    param(
        [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [int] $TimeoutSeconds = 3600
    )
    $body = @{
        objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 8
    return Invoke-RestMethod `
        -Uri ([uri]::new($RemoteControlUrl, '/remote/object/call')) `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec $TimeoutSeconds
}

function Get-TreeDigest {
    param([string] $Root)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Protected content directory is missing: $Root"
    }
    $rows = @(
        Get-ChildItem -LiteralPath $Root -Recurse -File -Force |
            Sort-Object FullName |
            ForEach-Object {
                $relative = $_.FullName.Substring($Root.Length).TrimStart('\')
                "$relative|$($_.Length)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
            }
    )
    if ($rows.Count -eq 0) {
        throw "Protected content directory is empty: $Root"
    }
    return [string]::Join("`n", $rows)
}

if ($RemoteControlUrl.Scheme -cne 'http' -or
    $RemoteControlUrl.Host -cne '127.0.0.1' -or
    $RemoteControlUrl.Port -ne 30010 -or
    $RemoteControlUrl.AbsolutePath -cne '/' -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Query) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Fragment) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.UserInfo)) {
    throw 'Explore map building requires exact loopback Remote Control http://127.0.0.1:30010.'
}

$projectDirectory = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
$editors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
if ($editors.Count -ne 1 -or
    [string]::IsNullOrWhiteSpace([string] $editors[0].CommandLine) -or
    ([string] $editors[0].CommandLine) -notmatch [regex]::Escape($projectFile)) {
    throw 'Exactly one Unreal Editor must be running on the requested TRIAD project.'
}

$sourceRoot = Join-Path $projectDirectory 'SourceAssets\IstanaPublicViewExploreV1'
$validator = Join-Path $sourceRoot 'validate_explore_v1.py'
if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
    throw "The installed Explore V1 source validator is missing: $validator"
}
& py -3 -B $validator | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw 'Explore V1 offline source validation failed; no Unreal mutation was requested.'
}

$protectedMaps = 1..5 | ForEach-Object {
    Join-Path $projectDirectory "Content\Maps\Istana_PublicView_Exterior_v$_.umap"
}
$protectedMapHashes = @{}
foreach ($map in $protectedMaps) {
    if (-not (Test-Path -LiteralPath $map -PathType Leaf)) {
        throw "Protected V1-V5 map is missing: $map"
    }
    $protectedMapHashes[$map] = (Get-FileHash -LiteralPath $map -Algorithm SHA256).Hash
}
$contextRoot = Join-Path $projectDirectory 'Content\TRIAD\IstanaPublicView\Context'
$contextBefore = Get-TreeDigest -Root $contextRoot

$identity = Invoke-ExploreCall `
    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory } `
    -TimeoutSeconds 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project identity failed: $($identity.OutReport)"
}

$import = Invoke-ExploreCall -FunctionName 'ImportIstanaExploreV1Assets'
if ($import.ReturnValue -ne $true) {
    throw "Explore asset import failed: $($import.OutMessage)"
}
$build = Invoke-ExploreCall -FunctionName 'BuildIstanaExploreV1Map'
if ($build.ReturnValue -ne $true) {
    throw "Explore map build failed: $($build.OutMessage)"
}
$validation = Invoke-ExploreCall -FunctionName 'ValidateIstanaExploreV1Map'
if ($validation.ReturnValue -ne $true) {
    throw "Persisted Explore map validation failed: $($validation.OutReport)"
}

foreach ($map in $protectedMaps) {
    if (-not (Test-Path -LiteralPath $map -PathType Leaf) -or
        (Get-FileHash -LiteralPath $map -Algorithm SHA256).Hash -cne
            $protectedMapHashes[$map]) {
        throw "A protected V1-V5 map changed during additive Explore creation: $map"
    }
}
$contextAfter = Get-TreeDigest -Root $contextRoot
if ($contextAfter -cne $contextBefore) {
    throw 'The distant OSM/HDB content package tree changed during Explore creation.'
}

[PSCustomObject]@{
    Operation = 'BuildIstanaExploreV1Map'
    DestinationMap = '/Game/Maps/Istana_PublicView_Explore_v1'
    SourceValidation = 'PASS'
    AssetImport = [string] $import.OutMessage
    MapBuild = [string] $build.OutMessage
    MapValidation = [string] $validation.OutReport
    ProtectedV1ThroughV5MapsUnchanged = $true
    DistantOsmHdbContentUnchanged = $true
    FreeRoamDefaultOnPlay = $true
}
