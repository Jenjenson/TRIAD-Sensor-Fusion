#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundleDirectory,
    [string]$Repository,
    [string]$Target = 'main',
    [switch]$Publish,
    [switch]$Finalize
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($Finalize -and -not $Publish) {
    throw '-Finalize requires -Publish.'
}

$bundlePath = (Get-Item -LiteralPath $BundleDirectory -Force -ErrorAction Stop).FullName
$manifestPath = Join-Path $bundlePath 'triad-release-manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 'triad.github_free_release_manifest.v1') {
    throw "Unsupported release manifest schema: $($manifest.schema)"
}

$repositoryName = if ([string]::IsNullOrWhiteSpace($Repository)) {
    [string]$manifest.githubRepository
}
else {
    $Repository
}
$tag = [string]$manifest.releaseTag
$assets = [Collections.Generic.List[string]]::new()
$assets.Add($manifestPath)

foreach ($bundleEntry in $manifest.bundles) {
    foreach ($part in $bundleEntry.parts) {
        $path = Join-Path $bundlePath ([string]$part.fileName)
        $file = Get-Item -LiteralPath $path -Force -ErrorAction Stop
        if ($file.Length -ge 2GB) {
            throw "Release asset exceeds GitHub's 2 GiB hard limit: $path"
        }
        $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne ([string]$part.sha256).ToLowerInvariant()) {
            throw "Release asset does not match the manifest: $path"
        }
        $assets.Add($file.FullName)
    }
}

[pscustomobject]@{
    Repository = $repositoryName
    Tag = $tag
    Assets = $assets.Count
    GiB = [math]::Round((($assets | ForEach-Object { (Get-Item -LiteralPath $_).Length } | Measure-Object -Sum).Sum) / 1GB, 3)
    Mode = if ($Publish) { if ($Finalize) { 'publish' } else { 'draft' } } else { 'verify-only' }
} | Format-List

if (-not $Publish) {
    Write-Host 'Verification completed. Pass -Publish to create and upload a draft GitHub release.'
    return
}

$gh = Get-Command gh -ErrorAction Stop
& $gh.Source auth status
if ($LASTEXITCODE -ne 0) {
    throw 'GitHub CLI authentication is required before publishing.'
}

$existing = & $gh.Source release view $tag --repo $repositoryName --json tagName --jq '.tagName' 2>$null
if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace(($existing | Out-String))) {
    throw "Release '$tag' already exists. This script never overwrites release assets."
}

$notes = @"
Versioned TRIAD collaboration assets for GitHub Free.

Download and verify these files with scripts/Install-GitHubFreeReleaseBundles.ps1.
The release manifest records archive sizes, SHA-256 values, extraction destinations,
and external prerequisites. Generated caches and credentials are not included.
"@

$createArguments = @(
    'release', 'create', $tag, $manifestPath,
    '--repo', $repositoryName,
    '--target', $Target,
    '--title', "TRIAD collaboration assets $($manifest.version)",
    '--notes', $notes,
    '--draft'
)
& $gh.Source @createArguments
if ($LASTEXITCODE -ne 0) {
    throw "Unable to create draft release '$tag'."
}

foreach ($asset in @($assets | Select-Object -Skip 1)) {
    & $gh.Source release upload $tag $asset --repo $repositoryName
    if ($LASTEXITCODE -ne 0) {
        throw "Upload failed for $asset. The draft release was retained for a resumable manual upload."
    }
}

if ($Finalize) {
    & $gh.Source release edit $tag --repo $repositoryName --draft=false
    if ($LASTEXITCODE -ne 0) {
        throw "Assets uploaded, but release '$tag' could not be published. It remains available as a draft."
    }
    Write-Host "Published release $tag."
}
else {
    Write-Host "Uploaded release $tag as a draft. Review it on GitHub before publishing."
}
