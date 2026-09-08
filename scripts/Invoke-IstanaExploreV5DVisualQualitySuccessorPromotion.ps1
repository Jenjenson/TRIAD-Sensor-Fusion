#Requires -Version 5.1

<#+
.SYNOPSIS
Safely promotes the reviewed Istana Explore V5D visual-quality successor files.

.DESCRIPTION
The default invocation is a read-only preflight. Use -Apply only after reviewing
the printed plan. The script accepts exactly the byte/hash-pinned files in the
adjacent authoritative manifest. It never builds Unreal or mutates the map.

Before its first native write it proves that no Unreal editor or commandlet is
using the native TRIAD project and that the exact identity of every unrelated
editor present at preflight is unchanged. It also proves the native map is the
exact predecessor, the canonical Saved parent inputs are unchanged, every
workspace source matches its receipt, and every native destination is either
the admitted predecessor or an idempotent copy.

.EXAMPLE
.\Invoke-IstanaExploreV5DVisualQualitySuccessorPromotion.ps1

.EXAMPLE
.\Invoke-IstanaExploreV5DVisualQualitySuccessorPromotion.ps1 -Apply

.EXAMPLE
.\Invoke-IstanaExploreV5DVisualQualitySuccessorPromotion.ps1 -RecoverTransactionRoot '<timestamped transaction root>'
#>
[CmdletBinding()]
param(
    [string] $WorkspaceUnrealRoot = (Join-Path $PSScriptRoot '..\unreal'),
    [string] $NativeProjectRoot = 'D:\triad\TRIAD',
    [string] $ManifestPath = (Join-Path $PSScriptRoot 'Promote-IstanaExploreV5DVisualQualitySuccessor.manifest.json'),
    [string] $BackupBaseRoot = '',
    [string] $RecoverTransactionRoot = '',
    [switch] $Apply
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedManifestBytes = 29317
$ExpectedManifestSha256 = '9DF232B3724A43C621DFAD03E611D6DC5B5C779135577CC1DDA375B293004072'
$ExpectedManifestSchema = 'triad.istana_explore_v5d.visual_quality_successor_promotion.v1'
$ExpectedManifestVersion = 1
$ExpectedPromotionId = 'istana_explore_v5d_visual_quality_successor_2026-09-05'
$ExpectedFileCount = 51
$ExpectedCanonicalParentCount = 3
$ExpectedVerifiedNoopCount = 1
$AllowedPolicies = @(
    'must-be-missing',
    'exact-old-hash-backup-and-replace'
)
$AllowedAreas = @(
    'runtime',
    'editor',
    'suppression-tool',
    'suppression-generated',
    'outer-ground-tool',
    'outer-ground-generated',
    'public-realm-lineage',
    'documentation',
    'contract-test'
)
$script:UnrealEditorProcessSnapshotInitialized = $false
$script:ReviewedUnrealEditorProcessSnapshot = @()

function Test-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    return $fullPath.StartsWith(
        $fullRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

function Assert-ExistingNonReparseDirectory {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
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

    if (-not (Test-ContainedPath -Path $Path -Root $Root)) {
        throw "$Label escaped its reviewed root: $Path"
    }
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $relative = [IO.Path]::GetFullPath($Path).Substring(
        $fullRoot.Length).TrimStart('\', '/')
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
        $parts.Count -eq 0 -or
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
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [switch] $AllowMissing
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath)) {
        if (-not $AllowMissing) {
            throw "Required file is missing: $fullPath"
        }
        return [pscustomobject] [ordered] @{
            Path = $fullPath
            Exists = $false
            Bytes = $null
            Sha256 = $null
        }
    }
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if ($item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Expected a regular non-reparse file: $fullPath"
    }
    return [pscustomobject] [ordered] @{
        Path = $fullPath
        Exists = $true
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

    if (-not $Receipt.Exists -or
        $Receipt.Bytes -ne $Bytes -or
        $Receipt.Sha256 -cne $Sha256) {
        throw "$Label receipt mismatch: path=$($Receipt.Path) expected=$Bytes`:$Sha256 actual=$($Receipt.Bytes)`:$($Receipt.Sha256)"
    }
}

function Assert-SameReceipt {
    param(
        [Parameter(Mandatory = $true)] [object] $Expected,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-FileReceipt -Path $Path -AllowMissing
    if ($actual.Exists -ne $Expected.Exists -or
        ($actual.Exists -and
         ($actual.Bytes -ne $Expected.Bytes -or
          $actual.Sha256 -cne $Expected.Sha256))) {
        throw "$Label drifted: path=$Path expected=$($Expected.Bytes)`:$($Expected.Sha256) actual=$($actual.Bytes)`:$($actual.Sha256)"
    }
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
    $difference = @(Compare-Object -ReferenceObject $expected -DifferenceObject $actual)
    if ($difference.Count -ne 0 -or $actual.Count -ne $expected.Count) {
        throw "$Label properties differ: expected=$($expected -join ',') actual=$($actual -join ',')"
    }
}

function Assert-Sha256Text {
    param(
        [Parameter(Mandatory = $true)] [string] $Value,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if ($Value -cnotmatch '^[0-9A-F]{64}$') {
        throw "$Label must be one uppercase SHA-256 digest."
    }
}

function Assert-PositiveByteCount {
    param(
        [Parameter(Mandatory = $true)] [object] $Value,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $parsed = [int64] 0
    if (-not [int64]::TryParse([string] $Value, [ref] $parsed) -or
        $parsed -le 0 -or [string] $parsed -cne [string] $Value) {
        throw "$Label must be a positive integer byte count."
    }
}

function Get-CanonicalUnrealProjectPathFromCommandLine {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [uint32] $ProcessId
    )

    $normalizedCommandLine = $CommandLine.Replace('/', '\')
    $projectPattern =
        '(?i)(?:"(?<Project>[A-Z]:\\[^\"]+?\.uproject)"|(?<Project>[A-Z]:\\[^\s\"]+?\.uproject))'
    $matches = @([regex]::Matches($normalizedCommandLine, $projectPattern))
    if ($matches.Count -ne 1) {
        throw "Unreal editor PID $ProcessId does not expose exactly one rooted .uproject argument; refusing promotion."
    }
    $projectPath = [string] $matches[0].Groups['Project'].Value
    try {
        $resolvedProjectPath = [IO.Path]::GetFullPath($projectPath)
    }
    catch {
        throw "Unreal editor PID $ProcessId exposes an invalid .uproject path; refusing promotion."
    }
    if (-not [IO.Path]::IsPathRooted($resolvedProjectPath) -or
        [IO.Path]::GetExtension($resolvedProjectPath) -ine '.uproject') {
        throw "Unreal editor PID $ProcessId exposes a non-canonical project argument; refusing promotion."
    }
    return $resolvedProjectPath
}

function Get-ReviewedUnrealEditorProcessSnapshot {
    param(
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    $resolvedNativeRoot = [IO.Path]::GetFullPath(
        $NativeRoot).Replace('/', '\').TrimEnd('\')
    $running = @(Get-CimInstance -ClassName Win32_Process `
        -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" `
        -Property ProcessId, Name, ExecutablePath, CreationDate, CommandLine `
        -ErrorAction Stop)
    $seenPids = [Collections.Generic.HashSet[uint32]]::new()
    $snapshot = [Collections.Generic.List[object]]::new()
    foreach ($process in $running) {
        $processId = [uint32] $process.ProcessId
        $name = [string] $process.Name
        $executablePath = [string] $process.ExecutablePath
        $commandLine = [string] $process.CommandLine
        if ($processId -eq 0 -or
            $name -cnotin @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -or
            [string]::IsNullOrWhiteSpace($executablePath) -or
            $null -eq $process.CreationDate -or
            [string]::IsNullOrWhiteSpace($commandLine) -or
            -not $seenPids.Add($processId)) {
            throw 'Unreal editor process identity is incomplete or duplicated; refusing promotion.'
        }
        try {
            $creationUtc = ([datetime] $process.CreationDate).
                ToUniversalTime().ToString(
                    'O', [Globalization.CultureInfo]::InvariantCulture)
        }
        catch {
            throw "Unreal editor process creation time is unreadable for PID $processId."
        }
        try {
            $resolvedExecutablePath = [IO.Path]::GetFullPath($executablePath)
        }
        catch {
            throw "Unreal editor executable path is invalid for PID $processId."
        }
        if (-not [IO.Path]::IsPathRooted($resolvedExecutablePath)) {
            throw "Unreal editor executable path is not rooted for PID $processId."
        }
        $projectPath = Get-CanonicalUnrealProjectPathFromCommandLine `
            -CommandLine $commandLine -ProcessId $processId
        $normalizedCommandLine = $commandLine.Replace('/', '\')
        $nativeReferencePattern = [regex]::Escape($resolvedNativeRoot) +
            '(?=$|[\\"\s])'
        if ((Test-ContainedPath -Path $projectPath -Root $resolvedNativeRoot) -or
            [regex]::IsMatch(
                $normalizedCommandLine,
                $nativeReferencePattern,
                [Text.RegularExpressions.RegexOptions]::IgnoreCase) -or
            (Test-ContainedPath `
                -Path $resolvedExecutablePath -Root $resolvedNativeRoot)) {
            throw "Native TRIAD Unreal editor/helper is running at PID $processId; refusing promotion."
        }
        $snapshot.Add([pscustomobject] [ordered] @{
            ProcessId = $processId
            Name = $name
            ExecutablePath = $executablePath
            CreationUtc = $creationUtc
            CommandLine = $commandLine
        })
    }
    return @($snapshot | Sort-Object ProcessId)
}

function Initialize-ReviewedUnrealEditorProcessSnapshot {
    param(
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    if ($script:UnrealEditorProcessSnapshotInitialized) {
        throw 'Unreal editor process identity snapshot was already initialized.'
    }
    $script:ReviewedUnrealEditorProcessSnapshot = @(
        Get-ReviewedUnrealEditorProcessSnapshot -NativeRoot $NativeRoot)
    $script:UnrealEditorProcessSnapshotInitialized = $true
}

function Assert-UnrealEditorProcessSnapshotUnchanged {
    if (-not $script:UnrealEditorProcessSnapshotInitialized) {
        throw 'Unreal editor process identity snapshot was not initialized.'
    }
    $expected = @($script:ReviewedUnrealEditorProcessSnapshot)
    $actual = @(Get-ReviewedUnrealEditorProcessSnapshot `
        -NativeRoot $script:ResolvedNativeRoot)
    if ($actual.Count -ne $expected.Count) {
        throw 'Unreal editor process set changed after preflight; refusing promotion.'
    }
    $identityFields = @(
        'ProcessId', 'Name', 'ExecutablePath', 'CreationUtc', 'CommandLine')
    for ($index = 0; $index -lt $expected.Count; ++$index) {
        Assert-ExactProperties -Value $expected[$index] `
            -Names $identityFields -Label 'reviewed Unreal editor identity'
        Assert-ExactProperties -Value $actual[$index] `
            -Names $identityFields -Label 'current Unreal editor identity'
        if ([uint32] $actual[$index].ProcessId -ne
                [uint32] $expected[$index].ProcessId -or
            [string] $actual[$index].Name -cne
                [string] $expected[$index].Name -or
            [string] $actual[$index].ExecutablePath -cne
                [string] $expected[$index].ExecutablePath -or
            [string] $actual[$index].CreationUtc -cne
                [string] $expected[$index].CreationUtc -or
            [string] $actual[$index].CommandLine -cne
                [string] $expected[$index].CommandLine) {
            throw 'Unreal editor process identity changed after preflight; refusing promotion.'
        }
    }
}

function Assert-AreaPathBoundary {
    param([Parameter(Mandatory = $true)] [object] $Entry)

    $source = [string] $Entry.source
    $destination = [string] $Entry.destination
    switch ([string] $Entry.area) {
        'runtime' {
            $prefix = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/'
            if (-not $source.StartsWith($prefix) -or
                -not $destination.StartsWith($prefix)) {
                throw "Runtime entry escaped its exact module root: $source"
            }
        }
        'editor' {
            $prefix = 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/'
            if (-not $source.StartsWith($prefix) -or
                -not $destination.StartsWith($prefix)) {
                throw "Editor entry escaped its exact module root: $source"
            }
        }
        'suppression-tool' {
            $prefix = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/'
            if (-not $source.StartsWith($prefix) -or
                $source.StartsWith($prefix + 'OuterGroundFallback/') -or
                $source -cne $destination) {
                throw "Suppression-tool entry escaped its exact tool root: $source"
            }
        }
        'suppression-generated' {
            $v1SourcePrefix = 'Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/'
            $v1DestinationPrefix = 'Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/LocalFallbackSuppressionV1/'
            $v2SourcePrefix = 'Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/'
            $v2DestinationPrefix = 'Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/LocalFallbackSuppressionV2/'
            $isV1 = $source.StartsWith($v1SourcePrefix) -and
                $destination -ceq ($v1DestinationPrefix +
                    $source.Substring($v1SourcePrefix.Length))
            $isV2 = $source.StartsWith($v2SourcePrefix) -and
                $destination -ceq ($v2DestinationPrefix +
                    $source.Substring($v2SourcePrefix.Length))
            if (-not $isV1 -and -not $isV2) {
                throw "Suppression-generated entry escaped its exact roots: $source"
            }
        }
        'outer-ground-tool' {
            $prefix = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/'
            if (-not $source.StartsWith($prefix) -or
                $source.StartsWith($prefix + 'Generated/') -or
                $source -cne $destination) {
                throw "Outer-ground tool entry escaped its exact tool root: $source"
            }
        }
        'outer-ground-generated' {
            $prefix = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/'
            if (-not $source.StartsWith($prefix) -or
                $source -cne $destination) {
                throw "Outer-ground generated entry escaped its exact generated root: $source"
            }
        }
        'public-realm-lineage' {
            $allowed = @(
                'SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/istana_public_view_v5d_public_realm.contract.json',
                'SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/IstanaPublicViewV5DPublicRealm.manifest.json',
                'SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/IstanaPublicViewV5DPublicRealm.acceptance.lock.json',
                'Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DPublicRealmAssetFactory.cpp'
            )
            if ($source -notin $allowed -or $source -cne $destination) {
                throw "Public-realm lineage entry escaped the four reviewed files: $source"
            }
        }
        'documentation' {
            $providerDoc = 'Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md'
            $outerReadme = 'Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/README.md'
            if ($source -notin @($providerDoc, $outerReadme) -or
                $source -cne $destination) {
                throw "Documentation entry is outside the two reviewed files: $source"
            }
        }
        'contract-test' {
            $allowed = @(
                'Plugins/TRIADSensorFusion/Tests/test_istana_explore_v5d_local_fallback_suppression_v2.py',
                'Plugins/TRIADSensorFusion/Tests/test_istana_explore_v5d_local_fallback_suppression_v2_integration.py',
                'Plugins/TRIADSensorFusion/Tests/test_istana_explore_v5d_context_policy_successor_contract.py'
            )
            if ($source -cnotin $allowed -or $source -cne $destination) {
                throw "Contract-test entry is outside the three reviewed V2 files: $source"
            }
        }
        default {
            throw "Unknown manifest area: $($Entry.area)"
        }
    }
}

function Read-AuthoritativeManifest {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $receipt = Get-FileReceipt -Path $Path
    Assert-PinnedReceipt -Receipt $receipt -Bytes $ExpectedManifestBytes `
        -Sha256 $ExpectedManifestSha256 -Label 'authoritative promotion manifest'
    $manifest = Get-Content -LiteralPath $receipt.Path -Raw -ErrorAction Stop |
        ConvertFrom-Json -ErrorAction Stop

    Assert-ExactProperties -Value $manifest -Names @(
        'schema', 'version', 'promotionId', 'predecessorMap',
        'canonicalParentSavedInputs', 'verifiedNoopFiles', 'files'
    ) -Label 'manifest root'
    if ([string] $manifest.schema -cne $ExpectedManifestSchema -or
        [int] $manifest.version -ne $ExpectedManifestVersion -or
        [string] $manifest.promotionId -cne $ExpectedPromotionId) {
        throw 'Promotion manifest schema, version, or identity is not the reviewed value.'
    }

    Assert-ExactProperties -Value $manifest.predecessorMap `
        -Names @('path', 'bytes', 'sha256') -Label 'predecessorMap'
    Assert-CanonicalRelativePath -RelativePath ([string] $manifest.predecessorMap.path) `
        -Label 'predecessorMap.path'
    Assert-PositiveByteCount -Value $manifest.predecessorMap.bytes `
        -Label 'predecessorMap.bytes'
    Assert-Sha256Text -Value ([string] $manifest.predecessorMap.sha256) `
        -Label 'predecessorMap.sha256'

    $parents = @($manifest.canonicalParentSavedInputs)
    $verifiedNoops = @($manifest.verifiedNoopFiles)
    $files = @($manifest.files)
    if ($parents.Count -ne $ExpectedCanonicalParentCount -or
        $verifiedNoops.Count -ne $ExpectedVerifiedNoopCount -or
        $files.Count -ne $ExpectedFileCount) {
        throw "Manifest roster count changed: parents=$($parents.Count) verifiedNoops=$($verifiedNoops.Count) files=$($files.Count)"
    }

    $parentPaths = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($parent in $parents) {
        Assert-ExactProperties -Value $parent -Names @('path', 'bytes', 'sha256') `
            -Label 'canonical parent Saved receipt'
        Assert-CanonicalRelativePath -RelativePath ([string] $parent.path) `
            -Label 'canonical parent Saved path'
        if (-not ([string] $parent.path).StartsWith(
            'Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/')) {
            throw "Canonical parent escaped its exact Saved root: $($parent.path)"
        }
        if (-not $parentPaths.Add([string] $parent.path)) {
            throw "Duplicate canonical parent path: $($parent.path)"
        }
        Assert-PositiveByteCount -Value $parent.bytes -Label 'canonical parent bytes'
        Assert-Sha256Text -Value ([string] $parent.sha256) `
            -Label 'canonical parent sha256'
    }

    $sources = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $destinations = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $verifiedNoops) {
        Assert-ExactProperties -Value $entry -Names @(
            'area', 'source', 'destination', 'bytes', 'sha256'
        ) -Label 'verified-noop file entry'
        if ([string] $entry.area -notin $AllowedAreas) {
            throw "Unknown verified-noop area for source: $($entry.source)"
        }
        Assert-CanonicalRelativePath -RelativePath ([string] $entry.source) `
            -Label 'verified-noop source'
        Assert-CanonicalRelativePath -RelativePath ([string] $entry.destination) `
            -Label 'verified-noop destination'
        Assert-AreaPathBoundary -Entry $entry
        if (-not $sources.Add([string] $entry.source) -or
            -not $destinations.Add([string] $entry.destination)) {
            throw "Duplicate verified-noop source or destination: $($entry.source)"
        }
        Assert-PositiveByteCount -Value $entry.bytes `
            -Label 'verified-noop bytes'
        Assert-Sha256Text -Value ([string] $entry.sha256) `
            -Label 'verified-noop sha256'
    }
    foreach ($entry in $files) {
        Assert-ExactProperties -Value $entry -Names @(
            'area', 'source', 'destination', 'bytes', 'sha256',
            'destinationPolicy', 'expectedOld'
        ) -Label 'promotion file entry'
        if ([string] $entry.area -notin $AllowedAreas -or
            [string] $entry.destinationPolicy -notin $AllowedPolicies) {
            throw "Unknown area or destination policy for source: $($entry.source)"
        }
        Assert-CanonicalRelativePath -RelativePath ([string] $entry.source) `
            -Label 'file source'
        Assert-CanonicalRelativePath -RelativePath ([string] $entry.destination) `
            -Label 'file destination'
        Assert-AreaPathBoundary -Entry $entry
        if (-not $sources.Add([string] $entry.source) -or
            -not $destinations.Add([string] $entry.destination)) {
            throw "Duplicate source or destination in manifest: $($entry.source)"
        }
        if ($parentPaths.Contains([string] $entry.destination) -or
            [string] $entry.destination -ceq [string] $manifest.predecessorMap.path) {
            throw "Promotion file overlaps a protected map or canonical input: $($entry.destination)"
        }
        Assert-PositiveByteCount -Value $entry.bytes -Label 'file bytes'
        Assert-Sha256Text -Value ([string] $entry.sha256) -Label 'file sha256'

        if ([string] $entry.destinationPolicy -ceq 'must-be-missing') {
            if ($null -ne $entry.expectedOld) {
                throw "must-be-missing entry cannot admit an old destination: $($entry.destination)"
            }
        }
        else {
            if ($null -eq $entry.expectedOld) {
                throw "Replacement entry lacks its exact old receipt: $($entry.destination)"
            }
            Assert-ExactProperties -Value $entry.expectedOld `
                -Names @('bytes', 'sha256') -Label 'expectedOld'
            Assert-PositiveByteCount -Value $entry.expectedOld.bytes `
                -Label 'expectedOld.bytes'
            Assert-Sha256Text -Value ([string] $entry.expectedOld.sha256) `
                -Label 'expectedOld.sha256'
        }
    }

    return [pscustomobject] [ordered] @{
        Receipt = $receipt
        Value = $manifest
    }
}

function New-PromotionPlan {
    param(
        [Parameter(Mandatory = $true)] [object] $Manifest,
        [Parameter(Mandatory = $true)] [string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    $rows = [Collections.Generic.List[object]]::new()
    foreach ($entry in @($Manifest.files)) {
        $source = Join-ReviewedPath -Root $WorkspaceRoot `
            -RelativePath ([string] $entry.source) -Label 'workspace source'
        $destination = Join-ReviewedPath -Root $NativeRoot `
            -RelativePath ([string] $entry.destination) -Label 'native destination'
        Assert-NoReparseAncestor -Path $source -Root $WorkspaceRoot `
            -Label 'workspace source'
        Assert-NoReparseAncestor -Path $destination -Root $NativeRoot `
            -Label 'native destination'

        $sourceReceipt = Get-FileReceipt -Path $source
        Assert-PinnedReceipt -Receipt $sourceReceipt -Bytes ([int64] $entry.bytes) `
            -Sha256 ([string] $entry.sha256) -Label 'workspace source'
        $destinationReceipt = Get-FileReceipt -Path $destination -AllowMissing
        $targetReceipt = [pscustomobject] [ordered] @{
            Path = $destination
            Exists = $true
            Bytes = [int64] $entry.bytes
            Sha256 = [string] $entry.sha256
        }

        $alreadyPromoted = $destinationReceipt.Exists -and
            $destinationReceipt.Bytes -eq $targetReceipt.Bytes -and
            $destinationReceipt.Sha256 -ceq $targetReceipt.Sha256
        if ($alreadyPromoted) {
            $action = 'unchanged'
        }
        elseif ([string] $entry.destinationPolicy -ceq 'must-be-missing') {
            if ($destinationReceipt.Exists) {
                throw "Unexpected existing create-only destination; refusing overwrite: $destination"
            }
            $action = 'create'
        }
        else {
            if (-not $destinationReceipt.Exists) {
                throw "Exact-old replacement destination is unexpectedly missing: $destination"
            }
            Assert-PinnedReceipt -Receipt $destinationReceipt `
                -Bytes ([int64] $entry.expectedOld.bytes) `
                -Sha256 ([string] $entry.expectedOld.sha256) `
                -Label 'native exact-old predecessor'
            $action = 'replace'
        }

        $rows.Add([pscustomobject] [ordered] @{
            Area = [string] $entry.area
            RelativeSource = [string] $entry.source
            RelativeDestination = [string] $entry.destination
            Policy = [string] $entry.destinationPolicy
            Action = $action
            Ordinal = $null
            Source = $source
            Destination = $destination
            SourceReceipt = $sourceReceipt
            DestinationBefore = $destinationReceipt
            TargetReceipt = $targetReceipt
            StagedPath = $null
            BackupPath = $null
            DisplacedPath = $null
        })
    }
    return @($rows)
}

function New-VerifiedNoopPlan {
    param(
        [Parameter(Mandatory = $true)] [object] $Manifest,
        [Parameter(Mandatory = $true)] [string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    $rows = [Collections.Generic.List[object]]::new()
    foreach ($entry in @($Manifest.verifiedNoopFiles)) {
        $source = Join-ReviewedPath -Root $WorkspaceRoot `
            -RelativePath ([string] $entry.source) `
            -Label 'verified-noop workspace source'
        $destination = Join-ReviewedPath -Root $NativeRoot `
            -RelativePath ([string] $entry.destination) `
            -Label 'verified-noop native destination'
        Assert-NoReparseAncestor -Path $source -Root $WorkspaceRoot `
            -Label 'verified-noop workspace source'
        Assert-NoReparseAncestor -Path $destination -Root $NativeRoot `
            -Label 'verified-noop native destination'

        $sourceReceipt = Get-FileReceipt -Path $source
        Assert-PinnedReceipt -Receipt $sourceReceipt `
            -Bytes ([int64] $entry.bytes) -Sha256 ([string] $entry.sha256) `
            -Label 'verified-noop workspace source'
        $destinationReceipt = Get-FileReceipt -Path $destination
        Assert-PinnedReceipt -Receipt $destinationReceipt `
            -Bytes ([int64] $entry.bytes) -Sha256 ([string] $entry.sha256) `
            -Label 'verified-noop native destination'

        $rows.Add([pscustomobject] [ordered] @{
            Area = [string] $entry.area
            RelativeSource = [string] $entry.source
            RelativeDestination = [string] $entry.destination
            Policy = 'verified-noop'
            Action = 'verified-noop'
            Ordinal = $null
            Source = $source
            Destination = $destination
            SourceReceipt = $sourceReceipt
            DestinationBefore = $destinationReceipt
            TargetReceipt = $destinationReceipt
            StagedPath = $null
            BackupPath = $null
            DisplacedPath = $null
        })
    }
    return @($rows)
}

function Assert-ProtectedNativeInputs {
    param(
        [Parameter(Mandatory = $true)] [object] $Manifest,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    $mapPath = Join-ReviewedPath -Root $NativeRoot `
        -RelativePath ([string] $Manifest.predecessorMap.path) `
        -Label 'predecessor map'
    Assert-NoReparseAncestor -Path $mapPath -Root $NativeRoot `
        -Label 'predecessor map'
    $mapReceipt = Get-FileReceipt -Path $mapPath
    Assert-PinnedReceipt -Receipt $mapReceipt `
        -Bytes ([int64] $Manifest.predecessorMap.bytes) `
        -Sha256 ([string] $Manifest.predecessorMap.sha256) `
        -Label 'native predecessor map'

    $parentReceipts = [Collections.Generic.List[object]]::new()
    foreach ($parent in @($Manifest.canonicalParentSavedInputs)) {
        $path = Join-ReviewedPath -Root $NativeRoot `
            -RelativePath ([string] $parent.path) -Label 'canonical parent Saved input'
        Assert-NoReparseAncestor -Path $path -Root $NativeRoot `
            -Label 'canonical parent Saved input'
        $receipt = Get-FileReceipt -Path $path
        Assert-PinnedReceipt -Receipt $receipt -Bytes ([int64] $parent.bytes) `
            -Sha256 ([string] $parent.sha256) `
            -Label 'canonical parent Saved input'
        $parentReceipts.Add($receipt)
    }
    return [pscustomobject] [ordered] @{
        Map = $mapReceipt
        Parents = @($parentReceipts)
    }
}

function Assert-PlanStable {
    param([Parameter(Mandatory = $true)] [object[]] $Rows)

    foreach ($row in $Rows) {
        [void] (Assert-SameReceipt -Expected $row.SourceReceipt `
            -Path $row.Source -Label 'workspace source')
        [void] (Assert-SameReceipt -Expected $row.DestinationBefore `
            -Path $row.Destination -Label 'native destination predecessor')
        Assert-NoReparseAncestor -Path $row.Source `
            -Root $script:ResolvedWorkspaceRoot -Label 'workspace source'
        Assert-NoReparseAncestor -Path $row.Destination `
            -Root $script:ResolvedNativeRoot -Label 'native destination'
    }
}

function New-ContainedDirectoryChain {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    if (-not (Test-ContainedPath -Path $fullPath -Root $Root)) {
        throw "$Label escaped its reviewed root: $fullPath"
    }
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $relative = $fullPath.Substring($fullRoot.Length).TrimStart('\', '/')
    $segments = @($relative -split '[\\/]')
    $cursor = $fullRoot
    foreach ($segment in $segments) {
        $cursor = Join-Path $cursor $segment
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if (-not $item.PSIsContainer -or
                ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Label traverses a non-directory or reparse path: $cursor"
            }
        }
        else {
            [void] (New-Item -ItemType Directory -Path $cursor -ErrorAction Stop)
            $created = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if (-not $created.PSIsContainer -or
                ($created.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Label created an invalid directory: $cursor"
            }
        }
    }
}

function New-FreshContainedDirectory {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $parent = [IO.Path]::GetDirectoryName($fullPath)
    New-ContainedDirectoryChain -Path $parent -Root $Root `
        -Label "$Label parent"
    if (Test-Path -LiteralPath $fullPath) {
        throw "$Label must be fresh and non-overwriting: $fullPath"
    }
    # New-Item intentionally has no -Force. A path created between the
    # preceding check and this call is a terminating collision, never reused.
    [void] (New-Item -ItemType Directory -Path $fullPath -ErrorAction Stop)
    $created = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if (-not $created.PSIsContainer -or
        ($created.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label created an invalid directory: $fullPath"
    }
}

function Get-ReviewedRelativePath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    if (-not (Test-ContainedPath -Path $fullPath -Root $fullRoot)) {
        throw "$Label escaped its reviewed root: $fullPath"
    }
    $relative = $fullPath.Substring($fullRoot.Length).TrimStart('\', '/')
    $canonical = $relative.Replace('\', '/')
    Assert-CanonicalRelativePath -RelativePath $canonical -Label $Label
    return $canonical
}

function Get-AdjacentTransactionPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Destination,
        [Parameter(Mandatory = $true)] [string] $TransactionId,
        [Parameter(Mandatory = $true)] [int] $Ordinal,
        [Parameter(Mandatory = $true)]
        [ValidateSet('publish', 'recover')] [string] $Purpose
    )

    if ($TransactionId -cnotmatch
        '^\d{8}T\d{9}Z-p\d+-[0-9a-f]{8}$' -or $Ordinal -lt 0) {
        throw "Invalid transaction identity or ordinal: $TransactionId/$Ordinal"
    }
    $filename = '.triad-v5d-{0}-{1:D3}-{2}.tmp' -f `
        $TransactionId, $Ordinal, $Purpose
    return [IO.Path]::GetFullPath((Join-Path `
        ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Destination))) `
        $filename))
}

function Copy-NewPinnedFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Source,
        [Parameter(Mandatory = $true)] [string] $Destination,
        [Parameter(Mandatory = $true)] [object] $Expected,
        [Parameter(Mandatory = $true)] [string] $SourceRoot,
        [Parameter(Mandatory = $true)] [string] $DestinationRoot,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NoReparseAncestor -Path $Source -Root $SourceRoot `
        -Label "$Label source"
    Assert-NoReparseAncestor -Path $Destination -Root $DestinationRoot `
        -Label "$Label destination"
    [void] (Assert-SameReceipt -Expected $Expected -Path $Source `
        -Label "$Label source")
    if (Test-Path -LiteralPath $Destination) {
        throw "$Label destination must be absent: $Destination"
    }
    # File.Copy(..., false) is the non-overwriting native copy primitive. It
    # cannot silently clobber a path created during the final collision window.
    [IO.File]::Copy($Source, $Destination, $false)
    [void] (Assert-SameReceipt -Expected ([pscustomobject] [ordered] @{
        Path = $Destination
        Exists = $true
        Bytes = $Expected.Bytes
        Sha256 = $Expected.Sha256
    }) -Path $Destination -Label "$Label readback")
}

function Write-DurableNewJsonReceipt {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [object] $Value,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    Assert-NoReparseAncestor -Path $fullPath -Root $Root -Label $Label
    New-ContainedDirectoryChain -Path ([IO.Path]::GetDirectoryName($fullPath)) `
        -Root $Root -Label "$Label parent"
    if (Test-Path -LiteralPath $fullPath) {
        throw "$Label is non-overwriting and already exists: $fullPath"
    }

    $temporary = $fullPath + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    Assert-NoReparseAncestor -Path $temporary -Root $Root `
        -Label "$Label temporary"
    if (Test-Path -LiteralPath $temporary) {
        throw "$Label temporary collision: $temporary"
    }
    $json = ($Value | ConvertTo-Json -Depth 16) + [Environment]::NewLine
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
    Assert-NoReparseAncestor -Path $fullPath -Root $Root -Label $Label
    if (Test-Path -LiteralPath $fullPath) {
        throw "$Label appeared before atomic publication: $fullPath"
    }
    [IO.File]::Move($temporary, $fullPath)
    $receipt = Get-FileReceipt -Path $fullPath
    if ($receipt.Bytes -ne $temporaryReceipt.Bytes -or
        $receipt.Sha256 -cne $temporaryReceipt.Sha256) {
        throw "$Label durable publication readback failed: $fullPath"
    }
    return $receipt
}

function New-PreparedTransactionValue {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Rows,
        [Parameter(Mandatory = $true)] [string] $TransactionId,
        [Parameter(Mandatory = $true)] [object] $ManifestReceipt,
        [Parameter(Mandatory = $true)] [string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    $journalRows = [Collections.Generic.List[object]]::new()
    foreach ($row in $Rows) {
        $journalRows.Add([pscustomobject] [ordered] @{
            Ordinal = [int] $row.Ordinal
            Area = $row.Area
            RelativeSource = $row.RelativeSource
            RelativeDestination = $row.RelativeDestination
            Action = $row.Action
            BeforeExists = [bool] $row.DestinationBefore.Exists
            BeforeBytes = $row.DestinationBefore.Bytes
            BeforeSha256 = $row.DestinationBefore.Sha256
            TargetBytes = [int64] $row.TargetReceipt.Bytes
            TargetSha256 = $row.TargetReceipt.Sha256
            StageRelativePath = Get-ReviewedRelativePath `
                -Path $row.StagedPath -Root $NativeRoot `
                -Label 'journal stage path'
            BackupRelativePath = if ($null -ne $row.BackupPath) {
                Get-ReviewedRelativePath -Path $row.BackupPath `
                    -Root $NativeRoot -Label 'journal backup path'
            } else {
                $null
            }
            DisplacedRelativePath = if ($null -ne $row.DisplacedPath) {
                Get-ReviewedRelativePath -Path $row.DisplacedPath `
                    -Root $NativeRoot -Label 'journal displaced path'
            } else {
                $null
            }
        })
    }
    return [pscustomobject] [ordered] @{
        Schema = 'triad.istana_explore_v5d.visual_quality_successor_transaction.v1'
        Version = 1
        State = 'PREPARED'
        TransactionId = $TransactionId
        PromotionId = $ExpectedPromotionId
        CreatedUtc = [DateTime]::UtcNow.ToString('o')
        WorkspaceUnrealRoot = [IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\', '/')
        NativeProjectRoot = [IO.Path]::GetFullPath($NativeRoot).TrimEnd('\', '/')
        ManifestBytes = [int64] $ManifestReceipt.Bytes
        ManifestSha256 = $ManifestReceipt.Sha256
        Rows = @($journalRows)
    }
}

function Assert-ClosedTransactionReceipt {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Outcome,
        [Parameter(Mandatory = $true)] [object] $JournalReceipt,
        [Parameter(Mandatory = $true)] [string] $TransactionId,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    Assert-NoReparseAncestor -Path $Path -Root $NativeRoot `
        -Label "$Outcome transaction receipt"
    $receipt = Get-FileReceipt -Path $Path
    $value = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop |
        ConvertFrom-Json -ErrorAction Stop
    $journalValue = Get-Content -LiteralPath $JournalReceipt.Path `
        -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    Assert-ExactProperties -Value $value -Names @(
        'Schema', 'Version', 'Outcome', 'TransactionId', 'JournalBytes',
        'JournalSha256', 'RowCount', 'CompletedUtc'
    ) -Label "$Outcome transaction receipt"
    if ([string] $value.Schema -cne
            'triad.istana_explore_v5d.visual_quality_successor_transaction_close.v1' -or
        [int] $value.Version -ne 1 -or
        [string] $value.Outcome -cne $Outcome -or
        [string] $value.TransactionId -cne $TransactionId -or
        [int64] $value.JournalBytes -ne $JournalReceipt.Bytes -or
        [string] $value.JournalSha256 -cne $JournalReceipt.Sha256 -or
        [int] $value.RowCount -ne @($journalValue.Rows).Count -or
        [int] $value.RowCount -le 0 -or
        [string]::IsNullOrWhiteSpace([string] $value.CompletedUtc)) {
        throw "$Outcome transaction receipt is not authentic: $Path"
    }
    return $receipt
}

function Get-PendingTransactionRoots {
    param(
        [Parameter(Mandatory = $true)] [string] $BackupBase,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    if (-not (Test-Path -LiteralPath $BackupBase)) {
        return @()
    }
    Assert-ExistingNonReparseDirectory -Path $BackupBase `
        -Label 'promotion backup base'
    Assert-NoReparseAncestor -Path (Join-Path $BackupBase '__sentinel__') `
        -Root $NativeRoot -Label 'promotion backup base'
    $pending = [Collections.Generic.List[string]]::new()
    foreach ($directory in @(Get-ChildItem -LiteralPath $BackupBase `
        -Directory -Force -ErrorAction Stop)) {
        if (($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Transaction discovery encountered a reparse directory: $($directory.FullName)"
        }
        $journalPath = Join-Path $directory.FullName 'prepared.json'
        $committedPath = Join-Path $directory.FullName 'committed.json'
        $recoveredPath = Join-Path $directory.FullName 'recovered.json'
        $hasJournal = Test-Path -LiteralPath $journalPath -PathType Leaf
        $hasCommitted = Test-Path -LiteralPath $committedPath -PathType Leaf
        $hasRecovered = Test-Path -LiteralPath $recoveredPath -PathType Leaf
        if (-not $hasJournal) {
            if ($hasCommitted -or $hasRecovered) {
                throw "Transaction close receipt exists without prepared journal: $($directory.FullName)"
            }
            continue
        }
        if ($hasCommitted -and $hasRecovered) {
            throw "Transaction has conflicting close receipts: $($directory.FullName)"
        }
        $journalReceipt = Get-FileReceipt -Path $journalPath
        if ($hasCommitted) {
            [void] (Assert-ClosedTransactionReceipt -Path $committedPath `
                -Outcome 'COMMITTED' -JournalReceipt $journalReceipt `
                -TransactionId $directory.Name -NativeRoot $NativeRoot)
        }
        elseif ($hasRecovered) {
            [void] (Assert-ClosedTransactionReceipt -Path $recoveredPath `
                -Outcome 'RECOVERED' -JournalReceipt $journalReceipt `
                -TransactionId $directory.Name -NativeRoot $NativeRoot)
        }
        else {
            $pending.Add($directory.FullName)
        }
    }
    return @($pending)
}

function Read-PreparedTransactionPlan {
    param(
        [Parameter(Mandatory = $true)] [string] $TransactionRoot,
        [Parameter(Mandatory = $true)] [string] $BackupBase,
        [Parameter(Mandatory = $true)] [object] $Manifest,
        [Parameter(Mandatory = $true)] [object] $ManifestReceipt,
        [Parameter(Mandatory = $true)] [string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    $fullRoot = [IO.Path]::GetFullPath($TransactionRoot).TrimEnd('\', '/')
    $fullBase = [IO.Path]::GetFullPath($BackupBase).TrimEnd('\', '/')
    if ([IO.Path]::GetDirectoryName($fullRoot) -cne $fullBase) {
        throw "Recovery transaction must be one direct child of the backup base: $fullRoot"
    }
    Assert-ExistingNonReparseDirectory -Path $fullRoot `
        -Label 'recovery transaction root'
    Assert-NoReparseAncestor -Path (Join-Path $fullRoot '__sentinel__') `
        -Root $NativeRoot -Label 'recovery transaction root'
    $journalPath = Join-Path $fullRoot 'prepared.json'
    $committedPath = Join-Path $fullRoot 'committed.json'
    $recoveredPath = Join-Path $fullRoot 'recovered.json'
    if ((Test-Path -LiteralPath $committedPath) -or
        (Test-Path -LiteralPath $recoveredPath)) {
        throw "Closed transaction cannot be recovered again: $fullRoot"
    }
    Assert-NoReparseAncestor -Path $journalPath -Root $NativeRoot `
        -Label 'prepared transaction journal'
    $journalReceipt = Get-FileReceipt -Path $journalPath
    $journal = Get-Content -LiteralPath $journalPath -Raw -ErrorAction Stop |
        ConvertFrom-Json -ErrorAction Stop
    Assert-ExactProperties -Value $journal -Names @(
        'Schema', 'Version', 'State', 'TransactionId', 'PromotionId',
        'CreatedUtc', 'WorkspaceUnrealRoot', 'NativeProjectRoot',
        'ManifestBytes', 'ManifestSha256', 'Rows'
    ) -Label 'prepared transaction journal'
    $transactionId = [string] $journal.TransactionId
    if ([string] $journal.Schema -cne
            'triad.istana_explore_v5d.visual_quality_successor_transaction.v1' -or
        [int] $journal.Version -ne 1 -or
        [string] $journal.State -cne 'PREPARED' -or
        $transactionId -cne [IO.Path]::GetFileName($fullRoot) -or
        [string] $journal.PromotionId -cne $ExpectedPromotionId -or
        [string] $journal.WorkspaceUnrealRoot -cne
            [IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\', '/') -or
        [string] $journal.NativeProjectRoot -cne
            [IO.Path]::GetFullPath($NativeRoot).TrimEnd('\', '/') -or
        [int64] $journal.ManifestBytes -ne $ManifestReceipt.Bytes -or
        [string] $journal.ManifestSha256 -cne $ManifestReceipt.Sha256 -or
        [string]::IsNullOrWhiteSpace([string] $journal.CreatedUtc)) {
        throw "Prepared transaction journal identity is not authentic: $journalPath"
    }

    $manifestByDestination = @{}
    foreach ($entry in @($Manifest.files)) {
        $manifestByDestination[[string] $entry.destination] = $entry
    }
    $journalRows = @($journal.Rows)
    if ($journalRows.Count -le 0 -or
        $journalRows.Count -gt @($Manifest.files).Count) {
        throw "Prepared transaction journal row count is invalid: $($journalRows.Count)"
    }
    $seenOrdinals = [Collections.Generic.HashSet[int]]::new()
    $seenDestinations = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($journalRow in $journalRows) {
        Assert-ExactProperties -Value $journalRow -Names @(
            'Ordinal', 'Area', 'RelativeSource', 'RelativeDestination',
            'Action', 'BeforeExists', 'BeforeBytes', 'BeforeSha256',
            'TargetBytes', 'TargetSha256', 'StageRelativePath',
            'BackupRelativePath', 'DisplacedRelativePath'
        ) -Label 'prepared transaction row'
        $ordinal = [int] $journalRow.Ordinal
        $relativeDestination = [string] $journalRow.RelativeDestination
        if ($ordinal -lt 0 -or $ordinal -ge $journalRows.Count -or
            -not $seenOrdinals.Add($ordinal) -or
            -not $seenDestinations.Add($relativeDestination) -or
            -not $manifestByDestination.ContainsKey($relativeDestination)) {
            throw "Prepared transaction row is duplicate or not manifest-owned: $relativeDestination"
        }
        $entry = $manifestByDestination[$relativeDestination]
        if ([string] $journalRow.Area -cne [string] $entry.area -or
            [string] $journalRow.RelativeSource -cne [string] $entry.source -or
            [int64] $journalRow.TargetBytes -ne [int64] $entry.bytes -or
            [string] $journalRow.TargetSha256 -cne [string] $entry.sha256) {
            throw "Prepared transaction row differs from authoritative manifest: $relativeDestination"
        }
        $action = [string] $journalRow.Action
        if ($action -ceq 'create') {
            if ([string] $entry.destinationPolicy -cne 'must-be-missing' -or
                [bool] $journalRow.BeforeExists -or
                $null -ne $journalRow.BeforeBytes -or
                $null -ne $journalRow.BeforeSha256 -or
                $null -ne $journalRow.BackupRelativePath -or
                $null -ne $journalRow.DisplacedRelativePath) {
                throw "Create journal row has invalid predecessor ownership: $relativeDestination"
            }
            $before = [pscustomobject] [ordered] @{
                Path = $null
                Exists = $false
                Bytes = $null
                Sha256 = $null
            }
        }
        elseif ($action -ceq 'replace') {
            if ([string] $entry.destinationPolicy -cne
                    'exact-old-hash-backup-and-replace' -or
                -not [bool] $journalRow.BeforeExists -or
                [int64] $journalRow.BeforeBytes -ne [int64] $entry.expectedOld.bytes -or
                [string] $journalRow.BeforeSha256 -cne
                    [string] $entry.expectedOld.sha256 -or
                $null -eq $journalRow.BackupRelativePath -or
                $null -eq $journalRow.DisplacedRelativePath) {
                throw "Replacement journal row has invalid predecessor ownership: $relativeDestination"
            }
            $before = [pscustomobject] [ordered] @{
                Path = $null
                Exists = $true
                Bytes = [int64] $journalRow.BeforeBytes
                Sha256 = [string] $journalRow.BeforeSha256
            }
        }
        else {
            throw "Prepared transaction row has unknown action: $action"
        }

        $destination = Join-ReviewedPath -Root $NativeRoot `
            -RelativePath $relativeDestination -Label 'recovery destination'
        $stage = Join-ReviewedPath -Root $NativeRoot `
            -RelativePath ([string] $journalRow.StageRelativePath) `
            -Label 'recovery publish stage'
        $expectedStage = Get-AdjacentTransactionPath `
            -Destination $destination -TransactionId $transactionId `
            -Ordinal $ordinal -Purpose 'publish'
        if ($stage -cne $expectedStage) {
            throw "Journal stage is not the exact adjacent transaction path: $stage"
        }
        if ([IO.Path]::GetDirectoryName($stage) -cne
            [IO.Path]::GetDirectoryName($destination)) {
            throw "Journal stage is not beside its destination: $stage"
        }
        $backup = $null
        $displaced = $null
        if ($action -ceq 'replace') {
            $backup = Join-ReviewedPath -Root $NativeRoot `
                -RelativePath ([string] $journalRow.BackupRelativePath) `
                -Label 'recovery predecessor backup'
            $expectedBackup = Join-Path (Join-Path $fullRoot 'native_before') `
                ('{0:D3}.predecessor' -f $ordinal)
            if ($backup -cne [IO.Path]::GetFullPath($expectedBackup)) {
                throw "Journal backup is not the exact transaction-owned path: $backup"
            }
            $displaced = Join-ReviewedPath -Root $NativeRoot `
                -RelativePath ([string] $journalRow.DisplacedRelativePath) `
                -Label 'recovery displaced predecessor'
            $expectedDisplaced = $backup + '.displaced'
            if ($displaced -cne [IO.Path]::GetFullPath($expectedDisplaced)) {
                throw "Journal displaced path is not transaction-owned: $displaced"
            }
        }
        $recoveryStage = Get-AdjacentTransactionPath `
            -Destination $destination -TransactionId $transactionId `
            -Ordinal $ordinal -Purpose 'recover'
        $quarantine = Join-Path (Join-Path $fullRoot 'recovered_new') `
            ('{0:D3}.published' -f $ordinal)
        foreach ($pathToCheck in @(
            $destination, $stage, $recoveryStage, $quarantine, $displaced
        )) {
            if ($null -eq $pathToCheck) {
                continue
            }
            Assert-NoReparseAncestor -Path $pathToCheck -Root $NativeRoot `
                -Label 'recovery transaction path'
        }
        if ($null -ne $backup) {
            Assert-NoReparseAncestor -Path $backup -Root $NativeRoot `
                -Label 'recovery predecessor backup'
        }

        $target = [pscustomobject] [ordered] @{
            Path = $destination
            Exists = $true
            Bytes = [int64] $journalRow.TargetBytes
            Sha256 = [string] $journalRow.TargetSha256
        }
        $before.Path = $destination
        $current = Get-FileReceipt -Path $destination -AllowMissing
        $isBefore = $current.Exists -eq $before.Exists -and
            (-not $current.Exists -or
             ($current.Bytes -eq $before.Bytes -and
              $current.Sha256 -ceq $before.Sha256))
        $isTarget = $current.Exists -and
            $current.Bytes -eq $target.Bytes -and
            $current.Sha256 -ceq $target.Sha256
        if (-not $isBefore -and -not $isTarget) {
            throw "Recovery refuses foreign or mixed destination bytes: $destination"
        }
        $stageReceipt = Get-FileReceipt -Path $stage -AllowMissing
        if ($stageReceipt.Exists) {
            Assert-PinnedReceipt -Receipt $stageReceipt -Bytes $target.Bytes `
                -Sha256 $target.Sha256 -Label 'recovery publish stage'
        }
        $displacedReceipt = $null
        if ($action -ceq 'replace') {
            $backupReceipt = Get-FileReceipt -Path $backup
            Assert-PinnedReceipt -Receipt $backupReceipt -Bytes $before.Bytes `
                -Sha256 $before.Sha256 -Label 'recovery predecessor backup'
            $recoveryStageReceipt = Get-FileReceipt `
                -Path $recoveryStage -AllowMissing
            if ($recoveryStageReceipt.Exists) {
                Assert-PinnedReceipt -Receipt $recoveryStageReceipt `
                    -Bytes $before.Bytes -Sha256 $before.Sha256 `
                    -Label 'recovery atomic stage'
            }
            $displacedReceipt = Get-FileReceipt -Path $displaced -AllowMissing
            if ($displacedReceipt.Exists) {
                Assert-PinnedReceipt -Receipt $displacedReceipt `
                    -Bytes $before.Bytes -Sha256 $before.Sha256 `
                    -Label 'published displaced predecessor'
            }
        }
        $quarantineReceipt = Get-FileReceipt -Path $quarantine -AllowMissing
        if ($quarantineReceipt.Exists) {
            if (-not $isBefore) {
                throw "Unexpected recovery quarantine ownership: $quarantine"
            }
            Assert-PinnedReceipt -Receipt $quarantineReceipt `
                -Bytes $target.Bytes -Sha256 $target.Sha256 `
                -Label 'recovery quarantine'
        }

        # A matching live hash is not sufficient ownership. Publication
        # consumes the prepared stage, and replacement publication also emits
        # the authenticated displaced predecessor. Recovery emits quarantine.
        # Only these exact, mutually exclusive filesystem states are admitted.
        if ($isTarget) {
            if ($stageReceipt.Exists -or $quarantineReceipt.Exists -or
                ($action -ceq 'replace' -and -not $displacedReceipt.Exists)) {
                throw "Target bytes lack transaction-owned publication evidence: $destination"
            }
            $currentState = 'published'
        }
        elseif ($quarantineReceipt.Exists) {
            if ($stageReceipt.Exists -or
                ($action -ceq 'replace' -and -not $displacedReceipt.Exists)) {
                throw "Recovered bytes have inconsistent transaction evidence: $destination"
            }
            $currentState = 'recovered'
        }
        else {
            if (-not $stageReceipt.Exists -or
                ($action -ceq 'replace' -and $displacedReceipt.Exists)) {
                throw "Predecessor bytes lack the exact prepared transaction stage: $destination"
            }
            $currentState = 'prepared'
        }
        $rows.Add([pscustomobject] [ordered] @{
            Ordinal = $ordinal
            Action = $action
            Destination = $destination
            StagePath = $stage
            BackupPath = $backup
            DisplacedPath = $displaced
            RecoveryStagePath = $recoveryStage
            QuarantinePath = $quarantine
            BeforeReceipt = $before
            TargetReceipt = $target
            CurrentState = $currentState
        })
    }
    return [pscustomobject] [ordered] @{
        TransactionRoot = $fullRoot
        TransactionId = $transactionId
        JournalPath = $journalPath
        JournalReceipt = $journalReceipt
        Rows = @($rows)
    }
}

function Write-TransactionCloseReceipt {
    param(
        [Parameter(Mandatory = $true)] [object] $Plan,
        [Parameter(Mandatory = $true)]
        [ValidateSet('COMMITTED', 'RECOVERED')] [string] $Outcome,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    Assert-NoReparseAncestor -Path $Plan.JournalPath -Root $NativeRoot `
        -Label "$Outcome prepared transaction journal"
    [void] (Assert-SameReceipt -Expected $Plan.JournalReceipt `
        -Path $Plan.JournalPath `
        -Label "$Outcome prepared transaction journal")
    $filename = if ($Outcome -ceq 'COMMITTED') {
        'committed.json'
    } else {
        'recovered.json'
    }
    $value = [pscustomobject] [ordered] @{
        Schema = 'triad.istana_explore_v5d.visual_quality_successor_transaction_close.v1'
        Version = 1
        Outcome = $Outcome
        TransactionId = $Plan.TransactionId
        JournalBytes = [int64] $Plan.JournalReceipt.Bytes
        JournalSha256 = $Plan.JournalReceipt.Sha256
        RowCount = @($Plan.Rows).Count
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
    }
    return Write-DurableNewJsonReceipt `
        -Path (Join-Path $Plan.TransactionRoot $filename) -Value $value `
        -Root $NativeRoot -Label "$Outcome transaction receipt"
}

function Invoke-DurableRecovery {
    param(
        [Parameter(Mandatory = $true)] [string] $TransactionRoot,
        [Parameter(Mandatory = $true)] [string] $BackupBase,
        [Parameter(Mandatory = $true)] [object] $Manifest,
        [Parameter(Mandatory = $true)] [object] $ManifestReceipt,
        [Parameter(Mandatory = $true)] [string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)] [string] $NativeRoot
    )

    # Read-PreparedTransactionPlan authenticates the entire journal, every
    # backup/stage, and every live destination before the first recovery write.
    $plan = Read-PreparedTransactionPlan -TransactionRoot $TransactionRoot `
        -BackupBase $BackupBase -Manifest $Manifest `
        -ManifestReceipt $ManifestReceipt -WorkspaceRoot $WorkspaceRoot `
        -NativeRoot $NativeRoot
    $rows = @($plan.Rows)
    for ($index = $rows.Count - 1; $index -ge 0; --$index) {
        $row = $rows[$index]
        Assert-UnrealEditorProcessSnapshotUnchanged
        Assert-ExistingNonReparseDirectory -Path $NativeRoot `
            -Label 'immediate recovery native root'
        Assert-ExistingNonReparseDirectory -Path $BackupBase `
            -Label 'immediate recovery backup base'
        Assert-NoReparseAncestor -Path (Join-Path $BackupBase '__sentinel__') `
            -Root $NativeRoot -Label 'immediate recovery backup base'
        [void] (Assert-ProtectedNativeInputs -Manifest $Manifest `
            -NativeRoot $NativeRoot)
        Assert-ExistingNonReparseDirectory -Path $plan.TransactionRoot `
            -Label 'recovery transaction root'
        foreach ($pathToCheck in @(
            $row.Destination,
            $row.StagePath,
            $row.RecoveryStagePath,
            $row.QuarantinePath,
            $row.DisplacedPath
        )) {
            if ($null -eq $pathToCheck) {
                continue
            }
            Assert-NoReparseAncestor -Path $pathToCheck -Root $NativeRoot `
                -Label 'immediate recovery path'
        }
        if ($null -ne $row.BackupPath) {
            Assert-NoReparseAncestor -Path $row.BackupPath -Root $NativeRoot `
                -Label 'immediate recovery backup'
        }

        $live = Get-FileReceipt -Path $row.Destination -AllowMissing
        $isBefore = $live.Exists -eq $row.BeforeReceipt.Exists -and
            (-not $live.Exists -or
             ($live.Bytes -eq $row.BeforeReceipt.Bytes -and
              $live.Sha256 -ceq $row.BeforeReceipt.Sha256))
        if ($isBefore) {
            continue
        }
        $isTarget = $live.Exists -and
            $live.Bytes -eq $row.TargetReceipt.Bytes -and
            $live.Sha256 -ceq $row.TargetReceipt.Sha256
        if (-not $isTarget) {
            throw "Recovery lost transaction ownership; refusing overwrite: $($row.Destination)"
        }
        if (Test-Path -LiteralPath $row.StagePath) {
            throw "Recovery lost publication ownership; stage reappeared: $($row.StagePath)"
        }
        if ($row.Action -ceq 'replace') {
            [void] (Assert-SameReceipt -Expected $row.BeforeReceipt `
                -Path $row.DisplacedPath `
                -Label 'immediate displaced predecessor ownership')
        }

        if ($row.Action -ceq 'create') {
            if (Test-Path -LiteralPath $row.QuarantinePath) {
                throw "Recovery quarantine appeared before create rollback: $($row.QuarantinePath)"
            }
            New-ContainedDirectoryChain `
                -Path ([IO.Path]::GetDirectoryName($row.QuarantinePath)) `
                -Root $NativeRoot -Label 'recovery quarantine parent'
            Assert-NoReparseAncestor -Path $row.Destination -Root $NativeRoot `
                -Label 'create rollback destination'
            Assert-NoReparseAncestor -Path $row.QuarantinePath -Root $NativeRoot `
                -Label 'create rollback quarantine'
            [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
                -Path $row.Destination -Label 'owned create rollback target')
            [IO.File]::Move($row.Destination, $row.QuarantinePath)
            [void] (Assert-SameReceipt -Expected ([pscustomobject] [ordered] @{
                Path = $row.Destination
                Exists = $false
                Bytes = $null
                Sha256 = $null
            }) -Path $row.Destination -Label 'restored missing predecessor')
            [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
                -Path $row.QuarantinePath -Label 'quarantined published file')
        }
        else {
            $backup = Get-FileReceipt -Path $row.BackupPath
            Assert-PinnedReceipt -Receipt $backup `
                -Bytes $row.BeforeReceipt.Bytes `
                -Sha256 $row.BeforeReceipt.Sha256 `
                -Label 'immediate recovery predecessor backup'
            $recoveryStage = Get-FileReceipt `
                -Path $row.RecoveryStagePath -AllowMissing
            if (-not $recoveryStage.Exists) {
                Copy-NewPinnedFile -Source $row.BackupPath `
                    -Destination $row.RecoveryStagePath `
                    -Expected $row.BeforeReceipt -SourceRoot $NativeRoot `
                    -DestinationRoot $NativeRoot `
                    -Label 'atomic recovery stage'
            }
            else {
                Assert-PinnedReceipt -Receipt $recoveryStage `
                    -Bytes $row.BeforeReceipt.Bytes `
                    -Sha256 $row.BeforeReceipt.Sha256 `
                    -Label 'resumed atomic recovery stage'
            }
            if (Test-Path -LiteralPath $row.QuarantinePath) {
                throw "Recovery quarantine appeared before replacement rollback: $($row.QuarantinePath)"
            }
            New-ContainedDirectoryChain `
                -Path ([IO.Path]::GetDirectoryName($row.QuarantinePath)) `
                -Root $NativeRoot -Label 'replacement recovery quarantine parent'
            Assert-NoReparseAncestor -Path $row.Destination -Root $NativeRoot `
                -Label 'replacement rollback destination'
            Assert-NoReparseAncestor -Path $row.RecoveryStagePath `
                -Root $NativeRoot -Label 'replacement rollback stage'
            [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
                -Path $row.Destination -Label 'owned replacement rollback target')
            [IO.File]::Replace(
                $row.RecoveryStagePath,
                $row.Destination,
                $row.QuarantinePath,
                $true)
            [void] (Assert-SameReceipt -Expected $row.BeforeReceipt `
                -Path $row.Destination -Label 'atomically restored predecessor')
            [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
                -Path $row.QuarantinePath -Label 'quarantined published target')
        }
    }

    foreach ($row in $rows) {
        Assert-NoReparseAncestor -Path $row.Destination -Root $NativeRoot `
            -Label 'final recovery destination'
        [void] (Assert-SameReceipt -Expected $row.BeforeReceipt `
            -Path $row.Destination -Label 'final recovered predecessor')
    }
    $closeReceipt = Write-TransactionCloseReceipt -Plan $plan `
        -Outcome 'RECOVERED' -NativeRoot $NativeRoot
    return [pscustomobject] [ordered] @{
        Status = 'RECOVERY_PASS'
        TransactionRoot = $plan.TransactionRoot
        RecoveredRows = $rows.Count
        JournalSha256 = $plan.JournalReceipt.Sha256
        RecoveryReceiptSha256 = $closeReceipt.Sha256
    }
}

# Read-only preflight begins here. Nothing above or below this boundary writes
# into the native project until either the explicit -Apply or explicit recovery
# branch and all of that branch's complete preflight checks have passed.
if ($Apply -and -not [string]::IsNullOrWhiteSpace($RecoverTransactionRoot)) {
    throw '-Apply and -RecoverTransactionRoot are mutually exclusive.'
}
$script:ResolvedNativeRoot = [IO.Path]::GetFullPath(
    $NativeProjectRoot).TrimEnd('\', '/')
Initialize-ReviewedUnrealEditorProcessSnapshot `
    -NativeRoot $script:ResolvedNativeRoot
Assert-UnrealEditorProcessSnapshotUnchanged
$script:ResolvedWorkspaceRoot = [IO.Path]::GetFullPath(
    $WorkspaceUnrealRoot).TrimEnd('\', '/')
Assert-ExistingNonReparseDirectory -Path $script:ResolvedWorkspaceRoot `
    -Label 'workspace Unreal root'
Assert-ExistingNonReparseDirectory -Path $script:ResolvedNativeRoot `
    -Label 'native project root'

$projectPath = Join-Path $script:ResolvedNativeRoot 'TRIAD.uproject'
Assert-NoReparseAncestor -Path $projectPath -Root $script:ResolvedNativeRoot `
    -Label 'native project marker'
$projectReceipt = Get-FileReceipt -Path $projectPath
$resolvedManifestPath = [IO.Path]::GetFullPath($ManifestPath)
$manifestBundle = Read-AuthoritativeManifest -Path $resolvedManifestPath
$manifest = $manifestBundle.Value
$protectedReceipts = Assert-ProtectedNativeInputs -Manifest $manifest `
    -NativeRoot $script:ResolvedNativeRoot

if ([string]::IsNullOrWhiteSpace($BackupBaseRoot)) {
    $resolvedBackupBase = Join-Path $script:ResolvedNativeRoot `
        'Saved\TRIAD\PromotionBackups\IstanaExploreV5DVisualQualitySuccessor'
}
else {
    $resolvedBackupBase = [IO.Path]::GetFullPath($BackupBaseRoot)
}
if (-not (Test-ContainedPath -Path $resolvedBackupBase `
    -Root $script:ResolvedNativeRoot)) {
    throw "Backup base must remain inside the native project: $resolvedBackupBase"
}
Assert-NoReparseAncestor -Path (Join-Path $resolvedBackupBase '__sentinel__') `
    -Root $script:ResolvedNativeRoot -Label 'backup base'

$pendingTransactions = @(Get-PendingTransactionRoots `
    -BackupBase $resolvedBackupBase -NativeRoot $script:ResolvedNativeRoot)
if ([string]::IsNullOrWhiteSpace($RecoverTransactionRoot) -and
    $pendingTransactions.Count -ne 0) {
    throw "Pending durable promotion transaction requires explicit recovery: $($pendingTransactions -join ',')"
}

# Pending state is discovered before any new plan snapshot is constructed.
# Recovery returns immediately; a later apply invocation necessarily rebuilds
# DestinationBefore from the recovered filesystem.
if (-not [string]::IsNullOrWhiteSpace($RecoverTransactionRoot)) {
    $resolvedRecoveryRoot = [IO.Path]::GetFullPath(
        $RecoverTransactionRoot).TrimEnd('\', '/')
    $matchingPending = @($pendingTransactions | Where-Object {
        [IO.Path]::GetFullPath($_).TrimEnd('\', '/') -ceq $resolvedRecoveryRoot
    })
    if ($matchingPending.Count -ne 1) {
        throw "Recovery root is not the one exact pending transaction: $resolvedRecoveryRoot"
    }
    # Complete recovery preflight is repeated immediately before its first
    # write inside Invoke-DurableRecovery.
    Assert-UnrealEditorProcessSnapshotUnchanged
    [void] (Assert-SameReceipt -Expected $manifestBundle.Receipt `
        -Path $resolvedManifestPath -Label 'authoritative manifest for recovery')
    [void] (Assert-SameReceipt -Expected $projectReceipt `
        -Path $projectPath -Label 'native project marker for recovery')
    [void] (Assert-ProtectedNativeInputs -Manifest $manifest `
        -NativeRoot $script:ResolvedNativeRoot)
    $recoveryResult = Invoke-DurableRecovery `
        -TransactionRoot $resolvedRecoveryRoot `
        -BackupBase $resolvedBackupBase -Manifest $manifest `
        -ManifestReceipt $manifestBundle.Receipt `
        -WorkspaceRoot $script:ResolvedWorkspaceRoot `
        -NativeRoot $script:ResolvedNativeRoot
    $recoveryResult | ConvertTo-Json -Depth 6
    return
}

$rows = @(New-PromotionPlan -Manifest $manifest `
    -WorkspaceRoot $script:ResolvedWorkspaceRoot `
    -NativeRoot $script:ResolvedNativeRoot)
$verifiedNoopRows = @(New-VerifiedNoopPlan -Manifest $manifest `
    -WorkspaceRoot $script:ResolvedWorkspaceRoot `
    -NativeRoot $script:ResolvedNativeRoot)
$allRows = @($rows) + @($verifiedNoopRows)

$changedRows = @($rows | Where-Object { $_.Action -cne 'unchanged' })
$replacementRows = @($changedRows | Where-Object { $_.Action -ceq 'replace' })
$transactionId = [DateTime]::UtcNow.ToString(
    'yyyyMMddTHHmmssfffZ', [Globalization.CultureInfo]::InvariantCulture) +
    "-p$PID-" + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$backupRoot = Join-Path $resolvedBackupBase $transactionId
if ($changedRows.Count -ne 0) {
    if (Test-Path -LiteralPath $backupRoot) {
        throw "Fresh timestamped transaction root already exists: $backupRoot"
    }
    Assert-NoReparseAncestor -Path (Join-Path $backupRoot '__sentinel__') `
        -Root $script:ResolvedNativeRoot -Label 'transaction root'
    for ($index = 0; $index -lt $changedRows.Count; ++$index) {
        $row = $changedRows[$index]
        $row.Ordinal = $index
        $row.StagedPath = Get-AdjacentTransactionPath `
            -Destination $row.Destination -TransactionId $transactionId `
            -Ordinal $index -Purpose 'publish'
        Assert-NoReparseAncestor -Path $row.StagedPath `
            -Root $script:ResolvedNativeRoot -Label 'adjacent publish stage'
        if (Test-Path -LiteralPath $row.StagedPath) {
            throw "Unexpected existing adjacent publish stage: $($row.StagedPath)"
        }
        if ($row.Action -ceq 'replace') {
            $row.BackupPath = Join-Path (Join-Path $backupRoot 'native_before') `
                ('{0:D3}.predecessor' -f $index)
            $row.DisplacedPath = $row.BackupPath + '.displaced'
            Assert-NoReparseAncestor -Path $row.BackupPath `
                -Root $script:ResolvedNativeRoot -Label 'native backup'
            Assert-NoReparseAncestor -Path $row.DisplacedPath `
                -Root $script:ResolvedNativeRoot `
                -Label 'atomic publish displaced predecessor'
            if (Test-Path -LiteralPath $row.DisplacedPath) {
                throw "Unexpected existing displaced predecessor: $($row.DisplacedPath)"
            }
        }
    }
}

# Final read-only gate. This deliberately repeats every mutable external check
# immediately before the -Apply decision.
Assert-UnrealEditorProcessSnapshotUnchanged
[void] (Assert-SameReceipt -Expected $manifestBundle.Receipt `
    -Path $resolvedManifestPath -Label 'authoritative manifest')
[void] (Assert-SameReceipt -Expected $projectReceipt `
    -Path $projectPath -Label 'native project marker')
[void] (Assert-ProtectedNativeInputs -Manifest $manifest `
    -NativeRoot $script:ResolvedNativeRoot)
Assert-PlanStable -Rows $allRows

$preflightResult = [pscustomobject] [ordered] @{
    Status = 'PREFLIGHT_PASS'
    ApplyRequested = [bool] $Apply
    PromotionId = $ExpectedPromotionId
    Manifest = $resolvedManifestPath
    WorkspaceUnrealRoot = $script:ResolvedWorkspaceRoot
    NativeProjectRoot = $script:ResolvedNativeRoot
    PredecessorMapSha256 = $protectedReceipts.Map.Sha256
    CanonicalParentSavedInputs = $protectedReceipts.Parents.Count
    ManifestFiles = $rows.Count
    VerifiedNoopFiles = $verifiedNoopRows.Count
    CreateFiles = @($changedRows | Where-Object { $_.Action -ceq 'create' }).Count
    ReplaceFiles = $replacementRows.Count
    UnchangedFiles = @($rows | Where-Object { $_.Action -ceq 'unchanged' }).Count
    PendingTransactions = 0
    UnrelatedUnrealEditorProcesses = @(
        $script:ReviewedUnrealEditorProcessSnapshot)
    TimestampedBackupRoot = if ($changedRows.Count -ne 0) {
        $backupRoot
    } else {
        $null
    }
}

if (-not $Apply) {
    $preflightResult | ConvertTo-Json -Depth 6
    return
}

# Native write boundary. Every source, destination, map, canonical Saved input,
# process guard, path boundary, pending journal, and backup collision was proven.
if ($changedRows.Count -eq 0) {
    $preflightResult.Status = 'APPLY_PASS_ALREADY_PROMOTED'
    $preflightResult | ConvertTo-Json -Depth 6
    return
}

Assert-UnrealEditorProcessSnapshotUnchanged
[void] (Assert-ProtectedNativeInputs -Manifest $manifest `
    -NativeRoot $script:ResolvedNativeRoot)
Assert-PlanStable -Rows $allRows
if (Test-Path -LiteralPath $backupRoot) {
    throw "Transaction root appeared after preflight; refusing all native writes: $backupRoot"
}
New-FreshContainedDirectory -Path $backupRoot `
    -Root $script:ResolvedNativeRoot -Label 'transaction root'

# Stage every source beside its destination with non-overwriting File.Copy and
# hash it before the durable journal or any destination publication.
foreach ($row in $changedRows) {
    Assert-UnrealEditorProcessSnapshotUnchanged
    [void] (Assert-SameReceipt -Expected $row.SourceReceipt `
        -Path $row.Source -Label 'workspace source before staging')
    New-ContainedDirectoryChain `
        -Path ([IO.Path]::GetDirectoryName($row.StagedPath)) `
        -Root $script:ResolvedNativeRoot -Label 'adjacent stage parent'
    Copy-NewPinnedFile -Source $row.Source -Destination $row.StagedPath `
        -Expected $row.SourceReceipt `
        -SourceRoot $script:ResolvedWorkspaceRoot `
        -DestinationRoot $script:ResolvedNativeRoot `
        -Label 'adjacent publish stage'
}

# Authenticate every differing predecessor into the fresh timestamped root.
# File.Copy(..., false) makes every backup non-overwriting.
foreach ($row in $replacementRows) {
    Assert-UnrealEditorProcessSnapshotUnchanged
    [void] (Assert-SameReceipt -Expected $row.DestinationBefore `
        -Path $row.Destination -Label 'native predecessor before backup')
    New-ContainedDirectoryChain `
        -Path ([IO.Path]::GetDirectoryName($row.BackupPath)) `
        -Root $script:ResolvedNativeRoot -Label 'backup parent'
    Copy-NewPinnedFile -Source $row.Destination `
        -Destination $row.BackupPath -Expected $row.DestinationBefore `
        -SourceRoot $script:ResolvedNativeRoot `
        -DestinationRoot $script:ResolvedNativeRoot `
        -Label 'authenticated predecessor backup'
}

Assert-UnrealEditorProcessSnapshotUnchanged
[void] (Assert-SameReceipt -Expected $manifestBundle.Receipt `
    -Path $resolvedManifestPath -Label 'authoritative manifest before journal')
[void] (Assert-SameReceipt -Expected $projectReceipt `
    -Path $projectPath -Label 'native project marker before journal')
[void] (Assert-ProtectedNativeInputs -Manifest $manifest `
    -NativeRoot $script:ResolvedNativeRoot)
Assert-PlanStable -Rows $allRows

# This flushed, atomically published journal is the durable ownership boundary.
# No destination is changed until it and every authenticated backup exist.
$journalValue = New-PreparedTransactionValue -Rows $changedRows `
    -TransactionId $transactionId `
    -ManifestReceipt $manifestBundle.Receipt `
    -WorkspaceRoot $script:ResolvedWorkspaceRoot `
    -NativeRoot $script:ResolvedNativeRoot
$journalPath = Join-Path $backupRoot 'prepared.json'
$journalReceipt = Write-DurableNewJsonReceipt -Path $journalPath `
    -Value $journalValue -Root $script:ResolvedNativeRoot `
    -Label 'prepared transaction journal'
$transactionPlan = [pscustomobject] [ordered] @{
    TransactionRoot = $backupRoot
    TransactionId = $transactionId
    JournalPath = $journalPath
    JournalReceipt = $journalReceipt
    Rows = $changedRows
}

try {
    foreach ($row in $changedRows) {
        Assert-UnrealEditorProcessSnapshotUnchanged
        [void] (Assert-ProtectedNativeInputs -Manifest $manifest `
            -NativeRoot $script:ResolvedNativeRoot)
        Assert-NoReparseAncestor -Path $row.Destination `
            -Root $script:ResolvedNativeRoot `
            -Label 'immediate publish destination'
        Assert-NoReparseAncestor -Path $row.StagedPath `
            -Root $script:ResolvedNativeRoot `
            -Label 'immediate adjacent publish stage'
        if ($null -ne $row.DisplacedPath) {
            Assert-NoReparseAncestor -Path $row.DisplacedPath `
                -Root $script:ResolvedNativeRoot `
                -Label 'immediate displaced predecessor'
            if (Test-Path -LiteralPath $row.DisplacedPath) {
                throw "Displaced predecessor appeared before atomic publish: $($row.DisplacedPath)"
            }
        }
        [void] (Assert-SameReceipt -Expected $row.SourceReceipt `
            -Path $row.Source -Label 'workspace source at publish')
        [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
            -Path $row.StagedPath -Label 'adjacent stage at publish')
        [void] (Assert-SameReceipt -Expected $row.DestinationBefore `
            -Path $row.Destination -Label 'owned predecessor at publish')

        if ($row.Action -ceq 'create') {
            # Same-directory rename is atomic and has no overwrite flag.
            [IO.File]::Move($row.StagedPath, $row.Destination)
        }
        else {
            # File.Replace consumes the same-directory stage atomically. A
            # termination can expose only the exact predecessor or exact target.
            [IO.File]::Replace(
                $row.StagedPath,
                $row.Destination,
                $row.DisplacedPath,
                $true)
            [void] (Assert-SameReceipt -Expected $row.DestinationBefore `
                -Path $row.DisplacedPath `
                -Label 'atomically displaced predecessor')
        }
        [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
            -Path $row.Destination -Label 'atomic promoted destination')
    }

    Assert-UnrealEditorProcessSnapshotUnchanged
    [void] (Assert-SameReceipt -Expected $manifestBundle.Receipt `
        -Path $resolvedManifestPath -Label 'authoritative manifest final')
    [void] (Assert-SameReceipt -Expected $projectReceipt `
        -Path $projectPath -Label 'native project marker final')
    [void] (Assert-ProtectedNativeInputs -Manifest $manifest `
        -NativeRoot $script:ResolvedNativeRoot)
    foreach ($row in $allRows) {
        [void] (Assert-SameReceipt -Expected $row.SourceReceipt `
            -Path $row.Source -Label 'workspace source final')
        [void] (Assert-SameReceipt -Expected $row.TargetReceipt `
            -Path $row.Destination -Label 'final promoted destination')
    }
    $commitReceipt = Write-TransactionCloseReceipt -Plan $transactionPlan `
        -Outcome 'COMMITTED' -NativeRoot $script:ResolvedNativeRoot
}
catch {
    $primaryError = $_.Exception.Message
    $recovery = $null
    $recoveryError = $null
    try {
        $recovery = Invoke-DurableRecovery -TransactionRoot $backupRoot `
            -BackupBase $resolvedBackupBase -Manifest $manifest `
            -ManifestReceipt $manifestBundle.Receipt `
            -WorkspaceRoot $script:ResolvedWorkspaceRoot `
            -NativeRoot $script:ResolvedNativeRoot
    }
    catch {
        $recoveryError = $_.Exception.Message
    }
    if ($null -ne $recovery) {
        throw "V5D promotion failed and durable recovery passed; original=$primaryError recoveryReceipt=$($recovery.RecoveryReceiptSha256)"
    }
    else {
        throw "V5D promotion failed; durable recovery stopped fail-closed. original=$primaryError recovery=$recoveryError transaction=$backupRoot"
    }
}

$preflightResult.Status = 'APPLY_PASS'
$preflightResult | Add-Member -NotePropertyName `
    'PreparedJournalSha256' -NotePropertyValue $journalReceipt.Sha256
$preflightResult | Add-Member -NotePropertyName `
    'CommitReceiptSha256' -NotePropertyValue $commitReceipt.Sha256
$preflightResult | Add-Member -NotePropertyName `
    'EveryDestinationRehashed' -NotePropertyValue $true
$preflightResult | Add-Member -NotePropertyName `
    'BackupsNonOverwriting' -NotePropertyValue $true
$preflightResult | Add-Member -NotePropertyName `
    'AtomicReplacement' -NotePropertyValue $true
$preflightResult | Add-Member -NotePropertyName `
    'MapMutated' -NotePropertyValue $false
$preflightResult | ConvertTo-Json -Depth 6
