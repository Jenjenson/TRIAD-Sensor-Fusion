#Requires -Version 5.1

<#+
.SYNOPSIS
Builds a closed, non-reparse input snapshot for the V5D successor promotion.

.DESCRIPTION
The reviewed workspace exposes SourceAssets through a junction. The promotion
transaction deliberately accepts only one ordinary source root, so this helper
copies the manifest's exact 51 promotion sources and one verified-noop source
into a fresh composite root on C:. Three SourceAssets files and ten exact
historical inputs required by this immutable 2026-09-05 manifest are read from
their explicit physical Saved root. Every other file is read from the ordinary
workspace Unreal root. Each historical input is emitted at its canonical
manifest path; the closure path never enters the composite snapshot.

The destination must not exist and must be disjoint from both source roots and
the native project. Every source is receipt-checked before the first write,
copied with File.Copy(..., false), then source and destination are rehashed.
The sibling .complete.json receipt is flushed and atomically published as the
last write. A missing receipt therefore means the snapshot is incomplete.

.EXAMPLE
.\New-IstanaExploreV5DVisualQualityPromotionInputSnapshot.ps1 `
  -DestinationRoot 'C:\TRIADPromotionInputs\v5d-successor-20260905T140000Z'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $DestinationRoot,
    [string] $WorkspaceUnrealRoot = (Join-Path $PSScriptRoot '..\unreal'),
    [string] $WorkspaceSourceAssetsRoot = 'D:\triad\TRIAD\Saved\TRIAD\CDriveRelief\20260830_workspace\TRIAD-Sensor-Fusion-Repo_unreal_SourceAssets',
    [string] $NativeProjectRoot = 'D:\triad\TRIAD',
    [string] $ManifestPath = (Join-Path $PSScriptRoot 'Promote-IstanaExploreV5DVisualQualitySuccessor.manifest.json')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedManifestBytes = 29317
$ExpectedManifestSha256 = '9DF232B3724A43C621DFAD03E611D6DC5B5C779135577CC1DDA375B293004072'
$ExpectedManifestSchema = 'triad.istana_explore_v5d.visual_quality_successor_promotion.v1'
$ExpectedManifestVersion = 1
$ExpectedPromotionId = 'istana_explore_v5d_visual_quality_successor_2026-09-05'
$ExpectedPromotionFileCount = 51
$ExpectedVerifiedNoopCount = 1
$ExpectedSnapshotFileCount = 52
$SourceAssetsPrefix = 'SourceAssets/'
$HistoricalPromotionClosureRootRelativePath = 'IstanaPublicViewExploreV5D/PublicRealm/NativeSourceClosure/VQSP20260905'
$ExpectedHistoricalPromotionOverrideCount = 10
$HistoricalPromotionOverrides = @(
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h'
        ClosureFile = '00_ContextPolicy.h'
        Bytes = 8694L
        Sha256 = '88A4000BA015308E50C909B333E8337649409179165138E092CA8DE63393C981'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp'
        ClosureFile = '01_ContextPolicy.cpp'
        Bytes = 86305L
        Sha256 = '75A753072A8914D505314A6AF395AFAD89AA919A0E1C8B6253EEF67B072338D1'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h'
        ClosureFile = '02_CurrentSurroundingsEditor.h'
        Bytes = 1722L
        Sha256 = 'E752E5767ABBBA900E2957231D8E1FE3CD4E03AAE8180FF08EE85346DE06243F'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp'
        ClosureFile = '03_CurrentSurroundingsEditor.cpp'
        Bytes = 12313L
        Sha256 = '2E52D294F856C00B17ACAE0299447CBC11BA4E423F3D37A25CCC7E2C8300923D'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h'
        ClosureFile = '04_HybridEditor.h'
        Bytes = 9021L
        Sha256 = '950ACEFDAF9EB1B3B21C535DB7E5482C217F1D72C23FF9995CAC9DA1758DFE47'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
        ClosureFile = '05_HybridEditor.cpp'
        Bytes = 377184L
        Sha256 = '9490D09F9FC453777F5187C2440F144F01227B5D7E9F079B22D710B403B4F75D'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/outer_ground_loading_fallback_v1.contract.json'
        ClosureFile = '06_OuterGround.contract.json'
        Bytes = 9600L
        Sha256 = '8815790E9BC0D7E4149DF67759F73A9F98738EACED490984B0F5519B883DDE83'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json'
        ClosureFile = '07_OuterGround.acceptance.lock.json'
        Bytes = 3478L
        Sha256 = '736785C3AEB8D7D2E2DDC4943314F450DD2DD548C6010E9FBC51D0CEF3C1EBDE'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json'
        ClosureFile = '08_OuterGround.manifest.json'
        Bytes = 11710L
        Sha256 = '9A9A12D127ED34D227F909F06DB55804B741F9ABBB22814BEF82DDC2301FB959'
    },
    [pscustomobject] [ordered] @{
        CanonicalSource = 'Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md'
        ClosureFile = '09_ProviderQuality.md'
        Bytes = 4661L
        Sha256 = '79A38816CAE908B21434F780DE883331D703BE5236E66729B0D8DBB1C903FB00'
    }
)
$ExpectedSourceAssetsSources = @(
    'SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/istana_public_view_v5d_public_realm.contract.json',
    'SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/IstanaPublicViewV5DPublicRealm.manifest.json',
    'SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/IstanaPublicViewV5DPublicRealm.acceptance.lock.json'
)

function Get-NormalizedDirectoryPath {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    $pathRoot = [IO.Path]::GetPathRoot($fullPath)
    if ([StringComparer]::OrdinalIgnoreCase.Equals(
        $fullPath.TrimEnd('\', '/'), $pathRoot.TrimEnd('\', '/'))) {
        return $pathRoot
    }
    return $fullPath.TrimEnd('\', '/')
}

function Test-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root
    )

    $fullPath = Get-NormalizedDirectoryPath -Path $Path
    $fullRoot = Get-NormalizedDirectoryPath -Path $Root
    if ([StringComparer]::OrdinalIgnoreCase.Equals($fullPath, $fullRoot)) {
        return $true
    }
    $prefix = if ($fullRoot.EndsWith(
        [string] [IO.Path]::DirectorySeparatorChar)) {
        $fullRoot
    } else {
        $fullRoot + [IO.Path]::DirectorySeparatorChar
    }
    return $fullPath.StartsWith(
        $prefix,
        [StringComparison]::OrdinalIgnoreCase)
}

function Assert-ExistingNonReparseDirectory {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = Get-NormalizedDirectoryPath -Path $Path
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label must be an existing non-reparse directory: $fullPath"
    }
}

function Assert-NoReparseAncestor {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = Get-NormalizedDirectoryPath -Path $Root
    if (-not (Test-ContainedPath -Path $fullPath -Root $fullRoot)) {
        throw "$Label escaped its reviewed root: $fullPath"
    }
    $relative = $fullPath.Substring($fullRoot.Length).TrimStart('\', '/')
    $segments = @($relative -split '[\\/]')
    $cursor = $fullRoot
    for ($index = 0; $index -lt ($segments.Count - 1); ++$index) {
        $cursor = Join-Path $cursor $segments[$index]
        if (-not (Test-Path -LiteralPath $cursor)) {
            break
        }
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
        if (-not $item.PSIsContainer -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label traverses a non-directory or reparse ancestor: $cursor"
        }
    }
}

function Assert-CanonicalRelativePath {
    param(
        [Parameter(Mandatory = $true)] [string] $RelativePath,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $parts = @($RelativePath -split '/')
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or
        [IO.Path]::IsPathRooted($RelativePath) -or
        $RelativePath.Contains('\') -or
        $RelativePath.StartsWith('/') -or
        $RelativePath.EndsWith('/') -or
        $RelativePath -match '[:*?]' -or
        @($parts | Where-Object {
            [string]::IsNullOrWhiteSpace($_) -or $_ -in @('.', '..')
        }).Count -ne 0) {
        throw "$Label is not a canonical forward-slash relative path: $RelativePath"
    }
}

function Join-ReviewedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $RelativePath,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-CanonicalRelativePath -RelativePath $RelativePath -Label $Label
    $fullPath = [IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    if (-not (Test-ContainedPath -Path $fullPath -Root $Root)) {
        throw "$Label escaped its reviewed root: $RelativePath"
    }
    return $fullPath
}

function Get-FileReceipt {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if ($item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Expected a regular non-reparse file: $fullPath"
    }
    return [pscustomobject] [ordered] @{
        Path = $fullPath
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
    }
}

function Assert-PinnedReceipt {
    param(
        [Parameter(Mandatory = $true)] [object] $Receipt,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if ($Receipt.Bytes -ne $Bytes -or $Receipt.Sha256 -cne $Sha256) {
        throw "$Label receipt mismatch: path=$($Receipt.Path) expected=$Bytes`:$Sha256 actual=$($Receipt.Bytes)`:$($Receipt.Sha256)"
    }
}

function Assert-SameReceipt {
    param(
        [Parameter(Mandatory = $true)] [object] $Expected,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-FileReceipt -Path $Path
    Assert-PinnedReceipt -Receipt $actual -Bytes ([int64] $Expected.Bytes) `
        -Sha256 ([string] $Expected.Sha256) -Label $Label
    return $actual
}

function Assert-ExactProperties {
    param(
        [Parameter(Mandatory = $true)] [object] $Value,
        [Parameter(Mandatory = $true)] [string[]] $Names,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = @($Value.PSObject.Properties.Name | Sort-Object -Unique)
    $expected = @($Names | Sort-Object -Unique)
    $difference = @(Compare-Object -ReferenceObject $expected `
        -DifferenceObject $actual)
    if ($difference.Count -ne 0 -or $actual.Count -ne $expected.Count) {
        throw "$Label properties differ: expected=$($expected -join ',') actual=$($actual -join ',')"
    }
}

function Read-ExactRoster {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $manifestReceipt = Get-FileReceipt -Path $Path
    Assert-PinnedReceipt -Receipt $manifestReceipt `
        -Bytes $ExpectedManifestBytes -Sha256 $ExpectedManifestSha256 `
        -Label 'authoritative promotion manifest'
    $manifest = Get-Content -LiteralPath $manifestReceipt.Path -Raw `
        -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    Assert-ExactProperties -Value $manifest -Names @(
        'schema', 'version', 'promotionId', 'predecessorMap',
        'canonicalParentSavedInputs', 'verifiedNoopFiles', 'files'
    ) -Label 'manifest root'
    if ([string] $manifest.schema -cne $ExpectedManifestSchema -or
        [int] $manifest.version -ne $ExpectedManifestVersion -or
        [string] $manifest.promotionId -cne $ExpectedPromotionId -or
        @($manifest.files).Count -ne $ExpectedPromotionFileCount -or
        @($manifest.verifiedNoopFiles).Count -ne $ExpectedVerifiedNoopCount) {
        throw 'Promotion manifest identity or closed roster count changed.'
    }

    $roster = @($manifest.files) + @($manifest.verifiedNoopFiles)
    if ($roster.Count -ne $ExpectedSnapshotFileCount) {
        throw "Snapshot roster count changed: $($roster.Count)"
    }
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $roster) {
        Assert-CanonicalRelativePath -RelativePath ([string] $entry.source) `
            -Label 'snapshot source'
        if (-not $seen.Add([string] $entry.source) -or
            [int64] $entry.bytes -le 0 -or
            [string] $entry.sha256 -cnotmatch '^[0-9A-F]{64}$') {
            throw "Snapshot source entry is duplicate or malformed: $($entry.source)"
        }
    }
    $splitSources = @($roster | Where-Object {
        ([string] $_.source).StartsWith(
            $SourceAssetsPrefix, [StringComparison]::Ordinal)
    } | ForEach-Object { [string] $_.source } | Sort-Object)
    $expectedSplitSources = @($ExpectedSourceAssetsSources | Sort-Object)
    $difference = @(Compare-Object -CaseSensitive `
        -ReferenceObject $expectedSplitSources -DifferenceObject $splitSources)
    if ($difference.Count -ne 0 -or
        $splitSources.Count -ne $ExpectedSourceAssetsSources.Count) {
        throw 'The exact three physical SourceAssets sources changed.'
    }
    if (@($HistoricalPromotionOverrides).Count -ne
        $ExpectedHistoricalPromotionOverrideCount) {
        throw 'The exact historical-promotion override count changed.'
    }
    $overrideSeen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $closureFileSeen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($override in @($HistoricalPromotionOverrides)) {
        Assert-ExactProperties -Value $override -Names @(
            'CanonicalSource', 'ClosureFile', 'Bytes', 'Sha256'
        ) -Label 'historical-promotion override'
        $canonicalSource = [string] $override.CanonicalSource
        $closureFile = [string] $override.ClosureFile
        Assert-CanonicalRelativePath -RelativePath $canonicalSource `
            -Label 'historical-promotion canonical source'
        Assert-CanonicalRelativePath -RelativePath $closureFile `
            -Label 'historical-promotion closure file'
        if ($canonicalSource.StartsWith(
                $SourceAssetsPrefix, [StringComparison]::Ordinal) -or
            -not $overrideSeen.Add($canonicalSource) -or
            -not $closureFileSeen.Add($closureFile) -or
            [int64] $override.Bytes -le 0 -or
            [string] $override.Sha256 -cnotmatch '^[0-9A-F]{64}$') {
            throw "Historical-promotion override is duplicate or malformed: $canonicalSource"
        }
        $manifestMatches = @($roster | Where-Object {
            [string] $_.source -ceq $canonicalSource
        })
        if ($manifestMatches.Count -ne 1 -or
            [int64] $manifestMatches[0].bytes -ne [int64] $override.Bytes -or
            [string] $manifestMatches[0].sha256 -cne [string] $override.Sha256) {
            throw "Historical-promotion override is absent from or disagrees with the immutable manifest: $canonicalSource"
        }
    }
    return [pscustomobject] [ordered] @{
        Manifest = $manifest
        ManifestReceipt = $manifestReceipt
        Roster = $roster
    }
}

function Resolve-SourceLocation {
    param(
        [Parameter(Mandatory = $true)] [object] $Entry,
        [Parameter(Mandatory = $true)] [string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)] [string] $SourceAssetsRoot
    )

    $relative = [string] $Entry.source
    $matchingOverrides = @($HistoricalPromotionOverrides | Where-Object {
        [string] $_.CanonicalSource -ceq $relative
    })
    if ($matchingOverrides.Count -gt 1) {
        throw "Duplicate historical-promotion override resolution: $relative"
    }
    if ($matchingOverrides.Count -eq 1) {
        $override = $matchingOverrides[0]
        if ([int64] $Entry.bytes -ne [int64] $override.Bytes -or
            [string] $Entry.sha256 -cne [string] $override.Sha256) {
            throw "Immutable promotion manifest no longer pins the reviewed historical input: $relative"
        }
        $root = $SourceAssetsRoot
        $closureRelativePath = `
            $HistoricalPromotionClosureRootRelativePath + '/' + `
            [string] $override.ClosureFile
        $path = Join-ReviewedPath -Root $root `
            -RelativePath $closureRelativePath `
            -Label 'historical promotion source closure'
        $kind = 'historical-promotion-source-closure'
    }
    elseif ($relative.StartsWith(
        $SourceAssetsPrefix, [StringComparison]::Ordinal)) {
        if ($relative -cnotin $ExpectedSourceAssetsSources) {
            throw "Unreviewed physical SourceAssets source: $relative"
        }
        $root = $SourceAssetsRoot
        $physicalRelative = $relative.Substring($SourceAssetsPrefix.Length)
        $path = Join-ReviewedPath -Root $root `
            -RelativePath $physicalRelative -Label 'physical SourceAssets source'
        $kind = 'physical-source-assets'
    }
    else {
        $root = $WorkspaceRoot
        $path = Join-ReviewedPath -Root $root -RelativePath $relative `
            -Label 'workspace Unreal source'
        $kind = 'workspace-unreal'
    }
    Assert-ExistingNonReparseDirectory -Path $root -Label "$kind root"
    Assert-NoReparseAncestor -Path $path -Root $root -Label $kind
    return [pscustomobject] [ordered] @{
        Path = $path
        Root = $root
        Kind = $kind
    }
}

function Assert-DisjointRoots {
    param(
        [Parameter(Mandatory = $true)] [string] $Left,
        [Parameter(Mandatory = $true)] [string] $Right,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if ((Test-ContainedPath -Path $Left -Root $Right) -or
        (Test-ContainedPath -Path $Right -Root $Left)) {
        throw "$Label must be disjoint: left=$Left right=$Right"
    }
}

function New-ContainedDirectoryChain {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    if (-not (Test-ContainedPath -Path $fullPath -Root $fullRoot)) {
        throw "$Label escaped its reviewed root: $fullPath"
    }
    $relative = $fullPath.Substring($fullRoot.Length).TrimStart('\', '/')
    if ([string]::IsNullOrWhiteSpace($relative)) {
        return
    }
    $cursor = $fullRoot
    foreach ($segment in @($relative -split '[\\/]')) {
        $cursor = Join-Path $cursor $segment
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if (-not $item.PSIsContainer -or
                ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Label traverses a non-directory or reparse path: $cursor"
            }
        }
        else {
            [void] (New-Item -ItemType Directory -Path $cursor `
                -ErrorAction Stop)
            $created = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if (-not $created.PSIsContainer -or
                ($created.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Label created an invalid directory: $cursor"
            }
        }
    }
}

function Write-DurableCompleteReceipt {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [object] $Value,
        [Parameter(Mandatory = $true)] [string] $ReviewedParent
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    Assert-NoReparseAncestor -Path $fullPath -Root $ReviewedParent `
        -Label 'complete receipt'
    if (Test-Path -LiteralPath $fullPath) {
        throw "Complete receipt must be absent: $fullPath"
    }
    $temporary = $fullPath + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    Assert-NoReparseAncestor -Path $temporary -Root $ReviewedParent `
        -Label 'complete receipt temporary'
    if (Test-Path -LiteralPath $temporary) {
        throw "Complete receipt temporary collision: $temporary"
    }
    $json = ($Value | ConvertTo-Json -Depth 12) + [Environment]::NewLine
    $payload = [Text.UTF8Encoding]::new($false).GetBytes($json)
    $stream = [IO.FileStream]::new(
        $temporary,
        [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write,
        [IO.FileShare]::None,
        4096,
        [IO.FileOptions]::WriteThrough)
    try {
        $stream.Write($payload, 0, $payload.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
    $temporaryReceipt = Get-FileReceipt -Path $temporary
    Assert-NoReparseAncestor -Path $fullPath -Root $ReviewedParent `
        -Label 'complete receipt publication'
    if (Test-Path -LiteralPath $fullPath) {
        throw "Complete receipt appeared before publication: $fullPath"
    }
    [IO.File]::Move($temporary, $fullPath)
    return Assert-SameReceipt -Expected $temporaryReceipt -Path $fullPath `
        -Label 'atomically published complete receipt'
}

# Complete read-only preflight. No destination path is created above or inside
# this section. All 52 source receipts are captured before the first write.
$resolvedWorkspaceRoot = [IO.Path]::GetFullPath(
    $WorkspaceUnrealRoot).TrimEnd('\', '/')
$resolvedSourceAssetsRoot = [IO.Path]::GetFullPath(
    $WorkspaceSourceAssetsRoot).TrimEnd('\', '/')
$resolvedNativeRoot = [IO.Path]::GetFullPath(
    $NativeProjectRoot).TrimEnd('\', '/')
$resolvedDestinationRoot = [IO.Path]::GetFullPath(
    $DestinationRoot).TrimEnd('\', '/')
$destinationDrive = [IO.Path]::GetPathRoot($resolvedDestinationRoot)
if (-not [StringComparer]::OrdinalIgnoreCase.Equals($destinationDrive, 'C:\')) {
    throw "Promotion input snapshot must be a fresh C: path: $resolvedDestinationRoot"
}
$destinationParent = [IO.Path]::GetDirectoryName($resolvedDestinationRoot)
Assert-ExistingNonReparseDirectory -Path $resolvedWorkspaceRoot `
    -Label 'workspace Unreal root'
Assert-ExistingNonReparseDirectory -Path $resolvedSourceAssetsRoot `
    -Label 'workspace SourceAssets physical root'
Assert-ExistingNonReparseDirectory -Path $destinationParent `
    -Label 'snapshot destination parent'
Assert-NoReparseAncestor `
    -Path (Join-Path $resolvedWorkspaceRoot '__root_sentinel__') `
    -Root ([IO.Path]::GetPathRoot($resolvedWorkspaceRoot)) `
    -Label 'workspace Unreal root ancestry'
Assert-NoReparseAncestor `
    -Path (Join-Path $resolvedSourceAssetsRoot '__root_sentinel__') `
    -Root ([IO.Path]::GetPathRoot($resolvedSourceAssetsRoot)) `
    -Label 'workspace SourceAssets physical-root ancestry'
Assert-NoReparseAncestor `
    -Path (Join-Path $destinationParent '__parent_sentinel__') `
    -Root $destinationDrive -Label 'snapshot destination-parent ancestry'
Assert-NoReparseAncestor -Path $resolvedDestinationRoot -Root $destinationDrive `
    -Label 'snapshot destination'
if (Test-Path -LiteralPath $resolvedDestinationRoot) {
    throw "Snapshot destination must be fresh and absent: $resolvedDestinationRoot"
}
Assert-DisjointRoots -Left $resolvedDestinationRoot `
    -Right $resolvedWorkspaceRoot -Label 'snapshot/workspace roots'
Assert-DisjointRoots -Left $resolvedDestinationRoot `
    -Right $resolvedSourceAssetsRoot -Label 'snapshot/SourceAssets roots'
Assert-DisjointRoots -Left $resolvedDestinationRoot `
    -Right $resolvedNativeRoot -Label 'snapshot/native roots'
$completeReceiptPath = $resolvedDestinationRoot + '.complete.json'
Assert-NoReparseAncestor -Path $completeReceiptPath -Root $destinationDrive `
    -Label 'snapshot complete receipt'
if (Test-Path -LiteralPath $completeReceiptPath) {
    throw "Snapshot complete receipt must be absent: $completeReceiptPath"
}

$manifestBundle = Read-ExactRoster -Path ([IO.Path]::GetFullPath($ManifestPath))
$rows = [Collections.Generic.List[object]]::new()
foreach ($entry in @($manifestBundle.Roster)) {
    $location = Resolve-SourceLocation -Entry $entry `
        -WorkspaceRoot $resolvedWorkspaceRoot `
        -SourceAssetsRoot $resolvedSourceAssetsRoot
    $preReceipt = Get-FileReceipt -Path $location.Path
    Assert-PinnedReceipt -Receipt $preReceipt -Bytes ([int64] $entry.bytes) `
        -Sha256 ([string] $entry.sha256) `
        -Label "$($location.Kind) snapshot source preflight"
    $destination = Join-ReviewedPath -Root $resolvedDestinationRoot `
        -RelativePath ([string] $entry.source) -Label 'snapshot destination file'
    $rows.Add([pscustomobject] [ordered] @{
        RelativePath = [string] $entry.source
        Source = $location.Path
        SourceRoot = $location.Root
        SourceKind = $location.Kind
        SourcePreReceipt = $preReceipt
        Destination = $destination
        SourcePostReceipt = $null
        SnapshotPostReceipt = $null
    })
}
if ($rows.Count -ne $ExpectedSnapshotFileCount) {
    throw "Preflight row count changed: $($rows.Count)"
}

# First write: create the one fresh root. Failure leaves an incomplete root
# without the sibling completion receipt, which a later invocation never reuses.
[void] (New-Item -ItemType Directory -Path $resolvedDestinationRoot `
    -ErrorAction Stop)
Assert-ExistingNonReparseDirectory -Path $resolvedDestinationRoot `
    -Label 'created snapshot root'

foreach ($row in $rows) {
    Assert-ExistingNonReparseDirectory -Path $row.SourceRoot `
        -Label "$($row.SourceKind) root before copy"
    Assert-NoReparseAncestor -Path $row.Source -Root $row.SourceRoot `
        -Label "$($row.SourceKind) source before copy"
    [void] (Assert-SameReceipt -Expected $row.SourcePreReceipt `
        -Path $row.Source -Label 'snapshot source before copy')
    New-ContainedDirectoryChain `
        -Path ([IO.Path]::GetDirectoryName($row.Destination)) `
        -Root $resolvedDestinationRoot -Label 'snapshot destination parent'
    Assert-NoReparseAncestor -Path $row.Destination `
        -Root $resolvedDestinationRoot -Label 'snapshot destination before copy'
    if (Test-Path -LiteralPath $row.Destination) {
        throw "Snapshot destination file must be absent: $($row.Destination)"
    }
    [IO.File]::Copy($row.Source, $row.Destination, $false)
    [void] (Assert-SameReceipt -Expected $row.SourcePreReceipt `
        -Path $row.Destination -Label 'snapshot copy readback')
}

$receiptRows = [Collections.Generic.List[object]]::new()
foreach ($row in $rows) {
    Assert-ExistingNonReparseDirectory -Path $row.SourceRoot `
        -Label "$($row.SourceKind) root after copy"
    Assert-NoReparseAncestor -Path $row.Source -Root $row.SourceRoot `
        -Label "$($row.SourceKind) source after copy"
    Assert-NoReparseAncestor -Path $row.Destination `
        -Root $resolvedDestinationRoot -Label 'snapshot destination after copy'
    $row.SourcePostReceipt = Assert-SameReceipt `
        -Expected $row.SourcePreReceipt -Path $row.Source `
        -Label 'snapshot source postflight'
    $row.SnapshotPostReceipt = Assert-SameReceipt `
        -Expected $row.SourcePreReceipt -Path $row.Destination `
        -Label 'snapshot destination postflight'
    $receiptRows.Add([pscustomobject] [ordered] @{
        RelativePath = $row.RelativePath
        SourceKind = $row.SourceKind
        SourcePreBytes = [int64] $row.SourcePreReceipt.Bytes
        SourcePreSha256 = $row.SourcePreReceipt.Sha256
        SourcePostBytes = [int64] $row.SourcePostReceipt.Bytes
        SourcePostSha256 = $row.SourcePostReceipt.Sha256
        SnapshotPostBytes = [int64] $row.SnapshotPostReceipt.Bytes
        SnapshotPostSha256 = $row.SnapshotPostReceipt.Sha256
    })
}

$completeValue = [pscustomobject] [ordered] @{
    Schema = 'triad.istana_explore_v5d.visual_quality_promotion_input_snapshot.v1'
    Version = 1
    State = 'COMPLETE'
    PromotionId = $ExpectedPromotionId
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    SnapshotRoot = $resolvedDestinationRoot
    WorkspaceUnrealRoot = $resolvedWorkspaceRoot
    WorkspaceSourceAssetsRoot = $resolvedSourceAssetsRoot
    ManifestPath = $manifestBundle.ManifestReceipt.Path
    ManifestBytes = [int64] $manifestBundle.ManifestReceipt.Bytes
    ManifestSha256 = $manifestBundle.ManifestReceipt.Sha256
    SnapshotFileCount = $rows.Count
    PhysicalSourceAssetsFileCount = @($rows | Where-Object {
        $_.SourceKind -ceq 'physical-source-assets'
    }).Count
    HistoricalSourceClosureFileCount = @($rows | Where-Object {
        $_.SourceKind -ceq 'historical-promotion-source-closure'
    }).Count
    Files = @($receiptRows)
}

# Final write: durable temporary payload followed by same-directory atomic move.
# Nothing after this call mutates the filesystem.
$completeReceipt = Write-DurableCompleteReceipt `
    -Path $completeReceiptPath -Value $completeValue `
    -ReviewedParent $destinationDrive

[pscustomobject] [ordered] @{
    Status = 'SNAPSHOT_PASS'
    SnapshotRoot = $resolvedDestinationRoot
    CompleteReceipt = $completeReceipt.Path
    CompleteReceiptBytes = [int64] $completeReceipt.Bytes
    CompleteReceiptSha256 = $completeReceipt.Sha256
    SnapshotFileCount = $rows.Count
    PhysicalSourceAssetsFileCount = $completeValue.PhysicalSourceAssetsFileCount
    HistoricalSourceClosureFileCount = $completeValue.HistoricalSourceClosureFileCount
    ManifestSha256 = $manifestBundle.ManifestReceipt.Sha256
} | ConvertTo-Json -Depth 4
