#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$UnrealProject,
    [string]$ConfigurationPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'distribution\github-free-release-bundles.json'),
    [string]$OutputDirectory,
    [string]$Version = [DateTime]::UtcNow.ToString('yyyy.MM.dd.HHmm'),
    [long]$MaximumAssetBytes = 0,
    [switch]$PlanOnly,
    [switch]$OverwriteOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Directory {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Label)

    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer) {
        throw "$Label is not a directory: $Path"
    }
    return $item.FullName.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}

function Convert-ToForwardSlash {
    param([Parameter(Mandatory)][string]$Path)
    return $Path.Replace('\', '/')
}

function Test-ExcludedPath {
    param(
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Patterns
    )

    foreach ($patternValue in $Patterns) {
        $pattern = Convert-ToForwardSlash ([string]$patternValue)
        if ($RelativePath -like $pattern) {
            return $true
        }
    }
    return $false
}

function Get-BundleInventory {
    param(
        [Parameter(Mandatory)][string]$BasePath,
        [Parameter(Mandatory)][object[]]$Includes,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Excludes
    )

    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $inventory = [Collections.Generic.List[object]]::new()
    $basePrefix = $BasePath + [IO.Path]::DirectorySeparatorChar

    foreach ($includeValue in $Includes) {
        $include = Convert-ToForwardSlash ([string]$includeValue)
        if ([IO.Path]::IsPathRooted($include) -or $include.Split('/') -contains '..') {
            throw "Bundle include must remain relative to its base path: $include"
        }

        $candidate = [IO.Path]::GetFullPath((Join-Path $BasePath $include))
        if (
            -not $candidate.Equals($BasePath, [StringComparison]::OrdinalIgnoreCase) -and
            -not $candidate.StartsWith($basePrefix, [StringComparison]::OrdinalIgnoreCase)
        ) {
            throw "Bundle include escapes its base path: $include"
        }
        if (-not (Test-Path -LiteralPath $candidate)) {
            throw "Required bundle input is missing: $candidate"
        }

        $candidateItem = Get-Item -LiteralPath $candidate -Force
        $files = if ($candidateItem.PSIsContainer) {
            @(Get-ChildItem -LiteralPath $candidate -File -Recurse -Force -FollowSymlink -ErrorAction Stop)
        }
        else {
            @($candidateItem)
        }

        foreach ($file in $files) {
            $relative = Convert-ToForwardSlash ([IO.Path]::GetRelativePath($BasePath, $file.FullName))
            if ($relative.Split('/') -contains '..') {
                throw "Resolved bundle file escapes its base path: $($file.FullName)"
            }
            if (Test-ExcludedPath -RelativePath $relative -Patterns $Excludes) {
                continue
            }
            if ($relative.Contains([char]10) -or $relative.Contains([char]13)) {
                throw "Archive file-list entries cannot contain a newline: $relative"
            }
            if ($seen.Add($relative)) {
                $inventory.Add([pscustomobject]@{
                    RelativePath = $relative
                    FullName = $file.FullName
                    Bytes = [long]$file.Length
                })
            }
        }
    }

    return @($inventory | Sort-Object RelativePath)
}

function New-PartPlan {
    param(
        [Parameter(Mandatory)][object[]]$Inventory,
        [Parameter(Mandatory)][long]$LimitBytes
    )

    # Leave room for tar headers and long-path records. The produced archive is
    # checked again before it can enter the release manifest.
    $archiveBudget = $LimitBytes - 16MB
    $perEntryOverhead = 4096L
    if ($archiveBudget -le 0) {
        throw 'MaximumAssetBytes is too small to create a safe tar archive.'
    }

    $parts = [Collections.Generic.List[object]]::new()
    $current = [Collections.Generic.List[object]]::new()
    $estimatedBytes = 0L

    foreach ($item in $Inventory) {
        $entryCost = [long]$item.Bytes + $perEntryOverhead
        if ($entryCost -gt $archiveBudget) {
            throw "A single file cannot fit under the configured release-asset limit: $($item.RelativePath)"
        }

        if ($current.Count -gt 0 -and ($estimatedBytes + $entryCost) -gt $archiveBudget) {
            $parts.Add([pscustomobject]@{
                Files = @($current)
                PayloadBytes = [long](($current | Measure-Object Bytes -Sum).Sum)
                EstimatedArchiveBytes = $estimatedBytes
            })
            $current = [Collections.Generic.List[object]]::new()
            $estimatedBytes = 0L
        }

        $current.Add($item)
        $estimatedBytes += $entryCost
    }

    if ($current.Count -gt 0) {
        $parts.Add([pscustomobject]@{
            Files = @($current)
            PayloadBytes = [long](($current | Measure-Object Bytes -Sum).Sum)
            EstimatedArchiveBytes = $estimatedBytes
        })
    }

    return @($parts)
}

if ($Version -notmatch '^[A-Za-z0-9._-]+$') {
    throw 'Version may contain only letters, numbers, periods, underscores, and hyphens.'
}

$repositoryPath = Resolve-Directory -Path $RepositoryRoot -Label 'RepositoryRoot'
$configurationFile = Get-Item -LiteralPath $ConfigurationPath -Force -ErrorAction Stop
$configuration = Get-Content -LiteralPath $configurationFile.FullName -Raw | ConvertFrom-Json
if ($configuration.schema -ne 'triad.github_free_release_bundles.v1') {
    throw "Unsupported bundle configuration schema: $($configuration.schema)"
}

$configuredLimit = [long]$configuration.maximumReleaseAssetBytes
$assetLimit = if ($MaximumAssetBytes -gt 0) { $MaximumAssetBytes } else { $configuredLimit }
if ($assetLimit -le 0 -or $assetLimit -ge 2GB) {
    throw 'MaximumAssetBytes must be positive and strictly below GitHub Releases 2 GiB per-file limit.'
}

$unrealProjectPath = $null
if ($configuration.bundles.source -contains 'unreal-project') {
    if ([string]::IsNullOrWhiteSpace($UnrealProject)) {
        throw 'UnrealProject is required by the configured unreal-project bundle.'
    }
    $unrealProjectPath = Resolve-Directory -Path $UnrealProject -Label 'UnrealProject'
}

$bundlePlans = [Collections.Generic.List[object]]::new()
foreach ($bundle in $configuration.bundles) {
    $sourceRoot = switch ([string]$bundle.source) {
        'repository' { $repositoryPath }
        'unreal-project' { $unrealProjectPath }
        default { throw "Unsupported bundle source '$($bundle.source)' for bundle '$($bundle.name)'." }
    }
    $basePath = [IO.Path]::GetFullPath((Join-Path $sourceRoot ([string]$bundle.basePath)))
    $inventory = Get-BundleInventory -BasePath $basePath -Includes @($bundle.includes) -Excludes @($bundle.excludes)
    if ([string]$bundle.source -eq 'unreal-project') {
        $forbiddenNativePaths = @(
            'Config/DefaultEngine.ini',
            'Content/CesiumSettings/*'
        )
        foreach ($item in $inventory) {
            foreach ($forbiddenPattern in $forbiddenNativePaths) {
                if ($item.RelativePath -like $forbiddenPattern) {
                    throw "Credential-bearing native path must not enter a release bundle: $($item.RelativePath)"
                }
            }
        }
    }
    if ($inventory.Count -eq 0) {
        throw "Bundle '$($bundle.name)' did not resolve any files."
    }
    $parts = New-PartPlan -Inventory $inventory -LimitBytes $assetLimit
    $bundlePlans.Add([pscustomobject]@{
        Name = [string]$bundle.name
        Source = [string]$bundle.source
        BasePath = $basePath
        Destination = [string]$bundle.destination
        IncludedRoots = @($bundle.includes | ForEach-Object { Convert-ToForwardSlash ([string]$_) })
        Excludes = @($bundle.excludes | ForEach-Object { Convert-ToForwardSlash ([string]$_) })
        Files = $inventory
        Parts = $parts
        TotalFiles = $inventory.Count
        TotalBytes = [long](($inventory | Measure-Object Bytes -Sum).Sum)
    })
}

$planRows = foreach ($bundlePlan in $bundlePlans) {
    [pscustomobject]@{
        Bundle = $bundlePlan.Name
        Files = $bundlePlan.TotalFiles
        GiB = [math]::Round($bundlePlan.TotalBytes / 1GB, 3)
        Parts = $bundlePlan.Parts.Count
        Destination = $bundlePlan.Destination
    }
}
$planRows | Format-Table -AutoSize

if ($PlanOnly) {
    Write-Host 'Plan-only validation completed; no archives or manifests were written.'
    return
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    throw 'OutputDirectory is required unless PlanOnly is selected.'
}
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $outputPath)) {
    New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
}
$outputPath = Resolve-Directory -Path $outputPath -Label 'OutputDirectory'

$tarCommand = Get-Command tar -ErrorAction Stop
$listDirectory = Join-Path $outputPath '.file-lists'
if (-not (Test-Path -LiteralPath $listDirectory)) {
    New-Item -ItemType Directory -Path $listDirectory -Force | Out-Null
}

$manifestBundles = [Collections.Generic.List[object]]::new()
foreach ($bundlePlan in $bundlePlans) {
    $manifestParts = [Collections.Generic.List[object]]::new()
    for ($partIndex = 0; $partIndex -lt $bundlePlan.Parts.Count; $partIndex++) {
        $part = $bundlePlan.Parts[$partIndex]
        $partNumber = $partIndex + 1
        $archiveName = 'triad-{0}-{1}-part{2:D3}.tar' -f $bundlePlan.Name, $Version, $partNumber
        $archivePath = Join-Path $outputPath $archiveName
        $listPath = Join-Path $listDirectory ($archiveName + '.txt')

        if ((Test-Path -LiteralPath $archivePath) -and -not $OverwriteOutput) {
            throw "Output archive already exists. Use -OverwriteOutput only for an intentional rebuild: $archivePath"
        }
        if (Test-Path -LiteralPath $archivePath) {
            Remove-Item -LiteralPath $archivePath -Force
        }

        @($part.Files.RelativePath) | Set-Content -LiteralPath $listPath -Encoding utf8NoBOM
        & $tarCommand.Source -cf $archivePath -C $bundlePlan.BasePath -T $listPath
        if ($LASTEXITCODE -ne 0) {
            throw "tar failed while creating $archiveName (exit code $LASTEXITCODE)."
        }

        $archive = Get-Item -LiteralPath $archivePath -Force
        if ($archive.Length -ge $assetLimit) {
            throw "Produced release asset is too large ($($archive.Length) bytes): $archiveName"
        }
        $hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        $manifestParts.Add([pscustomobject]@{
            fileName = $archiveName
            sha256 = $hash
            bytes = [long]$archive.Length
            fileCount = $part.Files.Count
            payloadBytes = [long]$part.PayloadBytes
        })
        Remove-Item -LiteralPath $listPath -Force
    }

    $manifestBundles.Add([pscustomobject]@{
        name = $bundlePlan.Name
        destination = $bundlePlan.Destination
        includedRoots = $bundlePlan.IncludedRoots
        excludedPatterns = $bundlePlan.Excludes
        totalFiles = $bundlePlan.TotalFiles
        totalPayloadBytes = $bundlePlan.TotalBytes
        parts = @($manifestParts)
    })
}

$manifest = [ordered]@{
    schema = 'triad.github_free_release_manifest.v1'
    version = $Version
    releaseTag = "assets-v$Version"
    githubRepository = [string]$configuration.githubRepository
    createdUtc = [DateTime]::UtcNow.ToString('o')
    unrealEngineVersion = [string]$configuration.unrealEngineVersion
    maximumReleaseAssetBytes = $assetLimit
    sourceCommit = (& git -C $repositoryPath rev-parse HEAD 2>$null)
    bundles = @($manifestBundles)
    externalPrerequisites = @($configuration.externalPrerequisites)
}

$manifestPath = Join-Path $outputPath 'triad-release-manifest.json'
if ((Test-Path -LiteralPath $manifestPath) -and -not $OverwriteOutput) {
    throw "Release manifest already exists: $manifestPath"
}
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $manifestPath -Encoding utf8NoBOM

Write-Host "Release bundles created at: $outputPath"
Write-Host "Manifest: $manifestPath"
Write-Host "Draft release tag: $($manifest.releaseTag)"
