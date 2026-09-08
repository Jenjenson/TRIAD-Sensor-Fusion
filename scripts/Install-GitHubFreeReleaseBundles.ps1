#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Repository = 'Jenjenson/TRIAD-Sensor-Fusion',
    [string]$Tag = 'latest',
    [string]$ManifestPath,
    [string]$CacheDirectory = (Join-Path $env:LOCALAPPDATA 'TRIAD\ReleaseBundles'),
    [string]$UnrealProjectDestination,
    [string[]]$Bundle,
    [switch]$AllowExisting,
    [switch]$SkipRepositoryLinks
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-OrCreateDirectory {
    param([Parameter(Mandatory)][string]$Path)
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath)) {
        New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    if (-not $item.PSIsContainer) {
        throw "Expected a directory: $fullPath"
    }
    return $item.FullName.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}

function Assert-SafeArchive {
    param([Parameter(Mandatory)][string]$ArchivePath)

    $entries = @(& tar -tf $ArchivePath)
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to list archive entries: $ArchivePath"
    }
    foreach ($entryValue in $entries) {
        $entry = ([string]$entryValue).Replace('\', '/')
        if (
            [string]::IsNullOrWhiteSpace($entry) -or
            $entry.StartsWith('/') -or
            $entry -match '^[A-Za-z]:' -or
            $entry.Split('/') -contains '..'
        ) {
            throw "Unsafe archive entry '$entry' in $ArchivePath"
        }
    }
}

function Invoke-Gh {
    param([Parameter(Mandatory)][string[]]$Arguments)
    $gh = Get-Command gh -ErrorAction Stop
    $output = @(& $gh.Source @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub CLI failed: gh $($Arguments -join ' ')"
    }
    return $output
}

$repositoryPath = Resolve-OrCreateDirectory -Path $RepositoryRoot
$resolvedTag = $Tag
$localManifest = $null

if (-not [string]::IsNullOrWhiteSpace($ManifestPath)) {
    $localManifest = (Get-Item -LiteralPath $ManifestPath -Force -ErrorAction Stop).FullName
}
else {
    Invoke-Gh -Arguments @('auth', 'status') | Out-Null
    if ($Tag -eq 'latest') {
        $resolvedTag = (Invoke-Gh -Arguments @(
            'release', 'view', '--repo', $Repository, '--json', 'tagName', '--jq', '.tagName'
        ) | Select-Object -First 1).Trim()
        if ([string]::IsNullOrWhiteSpace($resolvedTag)) {
            throw "GitHub did not return a latest release tag for $Repository."
        }
    }
    $tagCache = Resolve-OrCreateDirectory -Path (Join-Path $CacheDirectory $resolvedTag)
    Invoke-Gh -Arguments @(
        'release', 'download', $resolvedTag,
        '--repo', $Repository,
        '--pattern', 'triad-release-manifest.json',
        '--dir', $tagCache,
        '--clobber'
    ) | Out-Null
    $localManifest = Join-Path $tagCache 'triad-release-manifest.json'
}

$manifest = Get-Content -LiteralPath $localManifest -Raw | ConvertFrom-Json
if ($manifest.schema -ne 'triad.github_free_release_manifest.v1') {
    throw "Unsupported release manifest schema: $($manifest.schema)"
}
if ($resolvedTag -ne 'latest' -and $manifest.releaseTag -ne $resolvedTag) {
    throw "Manifest tag '$($manifest.releaseTag)' does not match requested tag '$resolvedTag'."
}

$manifestDirectory = Split-Path -Parent $localManifest
$selectedBundles = @($manifest.bundles)
if ($null -ne $Bundle -and $Bundle.Count -gt 0) {
    $requested = [Collections.Generic.HashSet[string]]::new(
        [string[]]$Bundle,
        [StringComparer]::OrdinalIgnoreCase
    )
    $selectedBundles = @($selectedBundles | Where-Object { $requested.Contains([string]$_.name) })
    if ($selectedBundles.Count -ne $requested.Count) {
        throw 'One or more requested bundle names do not exist in the release manifest.'
    }
}

$unrealDestination = $null
if ($selectedBundles.destination -contains 'unreal-project') {
    if ([string]::IsNullOrWhiteSpace($UnrealProjectDestination)) {
        throw 'UnrealProjectDestination is required for the unreal-project bundle.'
    }
    $unrealDestination = Resolve-OrCreateDirectory -Path $UnrealProjectDestination
}

$destinations = @{}
foreach ($bundleEntry in $selectedBundles) {
    $destination = switch ([string]$bundleEntry.destination) {
        'repository-unreal' { Resolve-OrCreateDirectory -Path (Join-Path $repositoryPath 'unreal') }
        'unreal-project' { $unrealDestination }
        default { throw "Unsupported bundle destination '$($bundleEntry.destination)'." }
    }
    $destinations[[string]$bundleEntry.name] = $destination

    if (-not $AllowExisting) {
        foreach ($rootValue in $bundleEntry.includedRoots) {
            $rootPath = Join-Path $destination ([string]$rootValue)
            if (Test-Path -LiteralPath $rootPath) {
                throw "Destination already contains '$rootPath'. Use a fresh destination or explicitly pass -AllowExisting."
            }
        }
    }
}

foreach ($bundleEntry in $selectedBundles) {
    $destination = $destinations[[string]$bundleEntry.name]
    foreach ($part in $bundleEntry.parts) {
        $fileName = [string]$part.fileName
        if ([IO.Path]::GetFileName($fileName) -ne $fileName) {
            throw "Manifest contains an unsafe release-asset name: $fileName"
        }
        $archivePath = Join-Path $manifestDirectory $fileName
        if (-not (Test-Path -LiteralPath $archivePath)) {
            if (-not [string]::IsNullOrWhiteSpace($ManifestPath)) {
                throw "Local release asset is missing next to the manifest: $archivePath"
            }
            Invoke-Gh -Arguments @(
                'release', 'download', $resolvedTag,
                '--repo', $Repository,
                '--pattern', $fileName,
                '--dir', $manifestDirectory
            ) | Out-Null
        }

        $archive = Get-Item -LiteralPath $archivePath -Force
        if ([long]$archive.Length -ne [long]$part.bytes) {
            throw "Release-asset size mismatch for $fileName."
        }
        $actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne ([string]$part.sha256).ToLowerInvariant()) {
            throw "Release-asset SHA-256 mismatch for $fileName."
        }

        Assert-SafeArchive -ArchivePath $archivePath
        & tar -xf $archivePath -C $destination
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to extract $fileName into $destination."
        }
        Write-Host "Installed $fileName"
    }
}

if (-not $SkipRepositoryLinks -and $null -ne $unrealDestination) {
    $links = @(
        [pscustomobject]@{
            Target = Join-Path $repositoryPath 'unreal\SourceAssets'
            Link = Join-Path $unrealDestination 'SourceAssets'
        },
        [pscustomobject]@{
            Target = Join-Path $repositoryPath 'unreal\Plugins\TRIADSensorFusion'
            Link = Join-Path $unrealDestination 'Plugins\TRIADSensorFusion'
        }
    )

    foreach ($linkEntry in $links) {
        if (-not (Test-Path -LiteralPath $linkEntry.Target)) {
            throw "Cannot create repository link because its target is missing: $($linkEntry.Target)"
        }
        if (Test-Path -LiteralPath $linkEntry.Link) {
            Write-Warning "Repository link already exists and was left unchanged: $($linkEntry.Link)"
            continue
        }
        $linkParent = Split-Path -Parent $linkEntry.Link
        if (-not (Test-Path -LiteralPath $linkParent)) {
            New-Item -ItemType Directory -Path $linkParent -Force | Out-Null
        }
        $itemType = if ($IsWindows) { 'Junction' } else { 'SymbolicLink' }
        New-Item -ItemType $itemType -Path $linkEntry.Link -Target $linkEntry.Target | Out-Null
        Write-Host "Linked $($linkEntry.Link) -> $($linkEntry.Target)"
    }
}

Write-Host "Installed TRIAD release bundles for tag $($manifest.releaseTag)."
Write-Host 'Install the external prerequisites listed in triad-release-manifest.json before opening the Unreal project.'
