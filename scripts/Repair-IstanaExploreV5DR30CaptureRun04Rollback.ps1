<#
.SYNOPSIS
Complete the exact build-tree rollback for R30 capture run 04.

.DESCRIPTION
This one-purpose recovery script reconciles only the six build roots journaled by
r30-capture-20260908-04. It restores the 14 content differences and 18
timestamp-only differences from that run's immutable rollback backup, removes
the exact 24 generated additions and one added empty directory, and then hashes
every file in all six roots against the backup. It never edits the original
rollback.json; success is recorded below recovery\<token>\receipt.json.
#>
[CmdletBinding()]
param(
    [switch] $Execute,
    [ValidatePattern('^r30-capture-20260908-04-recovery-[0-9]{2}$')]
    [string] $RecoveryToken = 'r30-capture-20260908-04-recovery-01'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$nativeRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$runRoot = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04')
$backupRoot = [IO.Path]::GetFullPath((Join-Path $runRoot 'rollback\build'))
$admissionReceipt = [IO.Path]::GetFullPath((Join-Path $runRoot 'admission.json'))
$originalRollback = [IO.Path]::GetFullPath((Join-Path $runRoot 'rollback.json'))
$recoveryToken = $RecoveryToken
$recoveryBase = [IO.Path]::GetFullPath((Join-Path $runRoot 'recovery'))
$recoveryRoot = [IO.Path]::GetFullPath((Join-Path $recoveryBase $recoveryToken))
$recoveryReceipt = [IO.Path]::GetFullPath((Join-Path $recoveryRoot 'receipt.json'))
$failedRecoveryReceipt = [IO.Path]::GetFullPath((Join-Path $recoveryRoot 'failed.json'))
$commitReceipt = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json')

$evidencePins = @(
    [pscustomobject] @{
        Label = 'run-04 admission receipt'
        Path = $admissionReceipt
        Bytes = 73145L
        Sha256 = '0DC67FA74F1B9F09453FC542065BFD2DB716E2AFCC2D7E80D87A733789F8A292'
    },
    [pscustomobject] @{
        Label = 'run-04 rollback receipt'
        Path = $originalRollback
        Bytes = 994L
        Sha256 = '6B84089FD0B9C9B7274D38FFA8143E9DA867F1F2E2AD73E07548D0DF335F4808'
    },
    [pscustomobject] @{
        Label = 'committed R30 transaction receipt'
        Path = $commitReceipt
        Bytes = 65411L
        Sha256 = 'F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F'
    }
)

$expectedBackupFileCount = 3332
$expectedBackupBytes = 16529511957L
$expectedBackupManifestSha256 =
    'C8872BCAE283B311EBB08F4863A4F908B59141744C592D6CD903F6B234F951AD'
$pchRelative =
    'Intermediate\Build\Win64\x64\TRIADEditor\Development\UnrealEd\SharedPCH.UnrealEd.Project.ValApi.Cpp20.h.pch'
$pchBytes = 2229338112L
$pchSha256 =
    '43C0EEFE03AA9A52B52F85ACFF8BA284E42682186F4BA3D2270BDA7AA658258F'

$mapPin = [pscustomobject] @{
    Path = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
    Bytes = 37468415L
    Sha256 = '126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7'
}
$runtimeDllPin = [pscustomobject] @{
    Path = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
    Bytes = 5076992L
    Sha256 = '100B061CC5677508A67D644687923CA58F68F072305B6FF62B6BCD363D472028'
}
$editorDllPin = [pscustomobject] @{
    Path = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
    Bytes = 8448000L
    Sha256 = '3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553'
}

$rootPins = @(
    [pscustomobject] @{
        Label = 'Binaries'; FileCount = 27
        ManifestSha256 = '39966DFC7D32F849F3C1B0D8CA8E25D7660473606065C283530336608DE02BC8'
    },
    [pscustomobject] @{
        Label = 'Intermediate'; FileCount = 350
        ManifestSha256 = '3295DC529089CFCE7B16C30E43E261DD5BB90D02F2479E8056AE20E1D7C8908F'
    },
    [pscustomobject] @{
        Label = 'Plugins\AirSimTriadRuntime\Binaries'; FileCount = 11
        ManifestSha256 = '6270C9FDFA433D4F4A9B992731CBA653CC03437A5FA4273A48357CA8DB9FE535'
    },
    [pscustomobject] @{
        Label = 'Plugins\AirSimTriadRuntime\Intermediate'; FileCount = 1049
        ManifestSha256 = '3E5E5AFBEB6FB1B219F8674FD7A7423D83D8E80546474216132CE7FE3EC45E2F'
    },
    [pscustomobject] @{
        Label = 'Plugins\TRIADSensorFusion\Binaries'; FileCount = 15
        ManifestSha256 = 'DBD6BD90CB70DA0145189FFD8AB3A5E85FCCB9A5133BDC350935E86BC8B60F70'
    },
    [pscustomobject] @{
        Label = 'Plugins\TRIADSensorFusion\Intermediate'; FileCount = 1880
        ManifestSha256 = '82A368E72A1F8C3A68ACBA6F4EBD5D48CE44B1C35AFD232110905397D990FAAC'
    }
)
$rootRelatives = @($rootPins | ForEach-Object Label)

$pluginDevelopmentPrefix =
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealGame\Development\TRIADSensorFusion'
$contentDifferent = @(
    'Intermediate\Build\Win64\x64\UnrealGame\ActionHistory.bin',
    'Intermediate\CachedAssetRegistry_0.bin',
    'Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealGame\Inc\AirSimTriadRuntime\UHT\Timestamp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealGame\Inc\TRIADSensorFusion\UHT\Timestamp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealGame\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DTreeRealismActor.gen.cpp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealGame\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DTreeRealismActor.generated.h',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealGame\Inc\TRIADSensorFusion\UHT\TRIADSensorFusion.init.gen.cpp',
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV4LandscapeActor.cpp.obj'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5AppearanceActor.cpp.obj'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5BVisualActor.cpp.obj'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DContextPolicyActor.cpp.obj'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DTreeRealismActor.cpp.obj'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DTreeRealismActor.gen.cpp.obj'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADSensorFusion.init.gen.cpp.obj')
)
$contentDifferentCurrentBytes = @(
    87094L, 106447241L, 1742L, 5606L, 65850L, 3439L, 1535L,
    3751036L, 1741874L, 3265733L, 7980645L, 2126957L, 592554L, 35203L
)
$contentDifferentCurrentSha256 = @(
    '16D2049C7D450B5D8C8100D4E6DF636CCE1E3652D55C0DF722057A3C8604A16B',
    '5407B5E41872EEC6EF914BD04AD46C9BD55346DE9C067EE03E49C89B09660128',
    '62409C8BAD248C4C8B2658BE04D90907C69496E3974FDB3D5DC6D2C01CB24197',
    '72E74EBA5A3E226D00DADEA913493DFCE6295B782CBC516DA1E965DC5778E947',
    '6E8B8BF1E517564AED2E1602C117314BD1E61E2E35652FA8BC9E949E4CB4E88B',
    'D79523DDBB073790B00B643AA43EE62AC52C441A00CA676FFD4822D6B30D3C58',
    'B899047FF9D147E77D64CB097A7E54A471276786540F67B775DD55B12D7BE01C',
    '353AD4FB7838AFC7AB934F61DFCD446A8F868274845AD00BD6981684C733137A',
    '125D5A147EA064E6149D28DFAA83BCDE6589BD1752A0DA14D31A92D018B50896',
    'B1408630DB8D7AC2E495C3D72694C1CAF4A550126AC82B3E3A13A41F7DFA6995',
    '32BA87DC77A00E82F3F5260B3507E8B7BD0A831279358670338352ED9CDEF1C8',
    '0D58A50D280B1DB60FA9180435C9E473E1884B8CF1D3165EDEF72220C7EC59EC',
    '1A9E24E995277425E664FC848924FEC885FECA5F5BA222FC7D85108C29F27A4E',
    'EDAF6CCD72B926475AE886E6708BFAA12400FA6BB16C5DCE3978E4DC5CAC5C1A'
)
$contentDifferentCurrentTicks = @(
    639244432949954157L, 639244440555245468L, 639244432790881849L,
    639244432790881849L, 639244432788599765L, 639244432788579682L,
    639244432789561561L, 639244433005676851L, 639244433024711228L,
    639244433053542445L, 639244433146986940L, 639244433282828140L,
    639244433288855055L, 639244433292923345L
)
$contentDifferentCurrentPins = @{}
for ($contentIndex = 0; $contentIndex -lt $contentDifferent.Count; $contentIndex++) {
    $contentDifferentCurrentPins[[string] $contentDifferent[$contentIndex]] =
        [pscustomobject] @{
            Bytes=[int64] $contentDifferentCurrentBytes[$contentIndex]
            Sha256=[string] $contentDifferentCurrentSha256[$contentIndex]
            LastWriteUtcTicks=[int64] $contentDifferentCurrentTicks[$contentIndex]
        }
}

$timestampOnly = @(
    'Intermediate\PipInstall\extra_urls.txt',
    'Intermediate\PipInstall\Lib\site-packages\plugin_site_package.pth',
    'Intermediate\PipInstall\merged_requirements.in',
    'Intermediate\PipInstall\pyreqs_plugins.list',
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV4LandscapeActor.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV4LandscapeActor.cpp.sarif'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5AppearanceActor.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5AppearanceActor.cpp.sarif'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5BVisualActor.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5BVisualActor.cpp.sarif'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DContextPolicyActor.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DContextPolicyActor.cpp.sarif'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DTreeRealismActor.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DTreeRealismActor.cpp.sarif'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DTreeRealismActor.gen.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADIstanaExploreV5DTreeRealismActor.gen.cpp.sarif'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADSensorFusion.init.gen.cpp.dep.json'),
    (Join-Path $pluginDevelopmentPrefix 'TRIADSensorFusion.init.gen.cpp.sarif')
)

$pluginIntermediatePrefix = 'Plugins\TRIADSensorFusion\Intermediate'
$pluginUhtPrefix = Join-Path $pluginIntermediatePrefix 'Build\Win64\UnrealGame\Inc\TRIADSensorFusion\UHT'
$addedFiles = @(
    (Join-Path $pluginUhtPrefix 'TRIADIstanaExploreV5DR30FacadeLookdevActor.gen.cpp'),
    (Join-Path $pluginUhtPrefix 'TRIADIstanaExploreV5DR30FacadeLookdevActor.generated.h'),
    (Join-Path $pluginUhtPrefix 'TRIADIstanaExploreV5DR30Player0CaptureLibrary.gen.cpp'),
    (Join-Path $pluginUhtPrefix 'TRIADIstanaExploreV5DR30Player0CaptureLibrary.generated.h')
)
foreach ($stem in @(
    'TRIADIstanaExploreV5DR30FacadeLookdevActor.cpp',
    'TRIADIstanaExploreV5DR30FacadeLookdevActor.gen.cpp',
    'TRIADIstanaExploreV5DR30FacadeLookdevActorTests.cpp',
    'TRIADIstanaExploreV5DR30Player0CaptureLibrary.cpp',
    'TRIADIstanaExploreV5DR30Player0CaptureLibrary.gen.cpp')) {
    foreach ($extension in @('dep.json', 'obj', 'obj.rsp', 'sarif')) {
        $addedFiles += Join-Path $pluginDevelopmentPrefix "$stem.$extension"
    }
}
$addedFileSha256 = @(
    '034E4F551E175EE10A1A92DD0BBCC728F418854D36640E53242F776017AF13F4',
    '4043CE629BE9D4909E7201A1371A718877E57B2F29BB9ACCF8F2DAAD86FB3FED',
    'ADC42FF498DD0F103519483B6DF1EB1101DB8C82AC8450A52DB2C728DF541109',
    '61A607267807C6F744A7E72D7B926B7C0569E13FE325D281D19C58A156AE9EC9',
    'DA0B8A19497A918F958DE8E20EDEF2309721A438DAA319ED18EF69948A4DCF32',
    '8298825DE34929E09958360260ECF6B8F79AED3462DF06EE0E4116F89E922115',
    'BE1779AD4CEC730673EB15A5E10B0B3F7F03A5534E9FFE2D234719B63E5CF7AB',
    '82FA5F5E0E126CFBC7227F1AD9B23A4A6CB22136E9ED3895CF7A161496F0F9DB',
    'EBD4FF01BB873952EBBB781F417DCBABE17FE03FE4B5E992B48D713890B7B0F9',
    'B6F74F2AC1778EB08ECFF8F02F06506702481225D81180EE795F1B5D3E5295A9',
    '8091383C42C0D9F576A3E73F2ECD24E3F3310E4010DFC44705A693DDDD22E93E',
    '82FA5F5E0E126CFBC7227F1AD9B23A4A6CB22136E9ED3895CF7A161496F0F9DB',
    'BA86C6D0F2E3FF1229DB459F59C26A871E80AE151412048AF6495C4F44054290',
    '2062F5E13932297E3BFC5911150043979EAF2B988DA6B202BB8BB4A5C9A8CDFE',
    'A23667EB7DD71DF76EAB4BEB152B5D8526D3703AB71B6C1D3254726398AA92A1',
    '82FA5F5E0E126CFBC7227F1AD9B23A4A6CB22136E9ED3895CF7A161496F0F9DB',
    'F18FC7D9ADA32E93755011943A657F83C55C07CBD3669AAF35FEA76338247AA9',
    '4FF37E713DCD50253D89D9D9CDB31607DBC4B94DA14772C80E771A46B174ECB1',
    'B7AA2B67427775CC227CBE1B25117E53FF64E7B5B0E805765AE8811AFDFC54BD',
    '82FA5F5E0E126CFBC7227F1AD9B23A4A6CB22136E9ED3895CF7A161496F0F9DB',
    '341F8AEC0E008CE43CDF49217919515F7193EB411F6896DE117ABC77E304F48E',
    '1B00D44E0753C43DD264121D0C26CDF01668CFB6CBB6FCE28D58A5E36913F899',
    '664AF39E214083BCEA0049B5086F18C9FCD32392D09A87C2BA16EB724E8E5E2E',
    '82FA5F5E0E126CFBC7227F1AD9B23A4A6CB22136E9ED3895CF7A161496F0F9DB'
)
$addedFilePins = @{}
for ($addedIndex = 0; $addedIndex -lt $addedFiles.Count; $addedIndex++) {
    $addedFilePins[[string] $addedFiles[$addedIndex]] =
        [string] $addedFileSha256[$addedIndex]
}
$addedEmptyDirectory = 'Intermediate\DatasmithContentTemp'
$addedEmptyDirectoryCurrentUtc = [DateTime]::Parse(
    '2026-09-08T06:00:30.9390240Z',
    [Globalization.CultureInfo]::InvariantCulture,
    [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()

$captureSourceRelatives = @(
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR30Player0CaptureLibrary.h',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR30Player0CaptureLibrary.cpp'
)

$r30SourcePins = @(
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR30FacadeLookdevActor.h'; Bytes=7179L; Sha256='4A4E5BDDD6D66BE5013BE534FE157435DC9FCF80FCB2C2154EE85BB7DB46320A' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR30FacadeLookdevActor.cpp'; Bytes=34159L; Sha256='1AA6E641A018E8137499B658878EC5C228F68EFE73DF88E246C5A394BC390A1B' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR30FacadeLookdevActorTests.cpp'; Bytes=5126L; Sha256='27B1D8463087551C9DF62A6DEB7D51A673864F8D1517B2F38FCF6C3E480C42E3' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h'; Bytes=861L; Sha256='A83E7B04DD3F2BB5AF26F735312F29B6905A3C031661B869D5ACF231B639F59F' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.cpp'; Bytes=71395L; Sha256='9941F3CD6E263683E4416569DAC4D8F6F3E879FD03D5CF8BB3E56E5E2B229A9D' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.h'; Bytes=2481L; Sha256='B67D37B2A61D0FE3A973CA9B6D3291D256257DAE4B9E84C65338225C29CB8B48' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.cpp'; Bytes=43276L; Sha256='FEFA20211AFE55057F26AB42B64D9351A48F9645853F903E9BEE62BBEFC4D470' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTreeRealismActor.h'; Bytes=9942L; Sha256='5B1959612F4DE2C6A0BF585BF329EA6ECA77F923651C6F8FEE61EE444F0B1E14' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTreeRealismActor.cpp'; Bytes=101549L; Sha256='AB6083398877A62D6847D049ADD333FE4165A2B96C085B2EA47ACA11567B087E' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTreeRealismEditorLibrary.h'; Bytes=1552L; Sha256='CBD331068DFF3CE42DDF6A50806528FBCB024AC30E6FD0ACF81EC486B6AEFB0D' },
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp'; Bytes=83826L; Sha256='91EDEA04EF4D6E83551E3F4735F6CA4461059787177F7EDB94399F398CE1090F' }
)

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $fullPath.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith(
        $fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparsePathAncestors {
    param([string] $Path)
    $cursor = [IO.Path]::GetFullPath($Path)
    while (-not [IO.File]::Exists($cursor) -and
           -not [IO.Directory]::Exists($cursor)) {
        $parent = [IO.Path]::GetDirectoryName($cursor.TrimEnd('\'))
        if ([string]::IsNullOrWhiteSpace($parent) -or
            $parent.Equals($cursor, [StringComparison]::OrdinalIgnoreCase)) {
            throw "No existing ancestor was found for path: $Path"
        }
        $cursor = [IO.Path]::GetFullPath($parent)
    }
    while ($true) {
        $item = Get-Item -LiteralPath $cursor -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Recovery refuses a reparse-point path ancestor: $cursor"
        }
        $parent = [IO.Path]::GetDirectoryName($cursor.TrimEnd('\'))
        if ([string]::IsNullOrWhiteSpace($parent) -or
            $parent.Equals($cursor, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $cursor = [IO.Path]::GetFullPath($parent)
    }
}

function Get-FileIdentity {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{
            Present = $false
            Bytes = 0L
            Sha256 = 'ABSENT'
            LastWriteUtc = $null
        }
    }
    $item = Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        LastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

function Assert-FilePin {
    param($Pin, [string] $Label)
    $actual = Get-FileIdentity $Pin.Path
    if (-not $actual.Present -or
        [int64] $actual.Bytes -ne [int64] $Pin.Bytes -or
        [string] $actual.Sha256 -cne [string] $Pin.Sha256) {
        throw "$Label pin mismatch: $($Pin.Path)"
    }
    $actual
}

function Assert-NativeMutatorsQuiescent {
    $engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
    $projectFile = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'TRIAD.uproject'))
    $gameExe = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Binaries\Win64\TRIAD.exe'))
    $busy = @(
        Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object {
                $path = if ($_.ExecutablePath) {
                    [IO.Path]::GetFullPath([string] $_.ExecutablePath)
                } else { '' }
                $command = [string] $_.CommandLine
                $isEngineUnreal = [string] $_.Name -like 'UnrealEditor*'
                $isExactGame = $path.Equals(
                    $gameExe, [StringComparison]::OrdinalIgnoreCase)
                $isProjectCommand = -not [string]::IsNullOrWhiteSpace($command) -and
                    ($command.Contains($projectFile, [StringComparison]::OrdinalIgnoreCase) -or
                     $command.Contains($nativeRoot, [StringComparison]::OrdinalIgnoreCase))
                $isBuildTool = [string] $_.Name -in @(
                    'dotnet.exe', 'UnrealBuildTool.exe', 'UnrealHeaderTool.exe',
                    'MSBuild.exe', 'cl.exe', 'link.exe', 'rc.exe',
                    'ShaderCompileWorker.exe', 'UbaAgent.exe', 'UbaHost.exe',
                    'UbaCli.exe', 'LiveCodingConsole.exe',
                    'LiveCodingServer.exe')
                $isEngineUnreal -or $isExactGame -or $isBuildTool -or
                    ($isProjectCommand -and
                     ([string] $_.Name -like '*Unreal*' -or
                      [string] $_.Name -like '*TRIAD*'))
            } |
            Select-Object ProcessId, ParentProcessId, CreationDate, Name,
                ExecutablePath, CommandLine
    )
    if ($busy.Count -ne 0) {
        throw "Recovery refused while native mutators exist: $($busy | ConvertTo-Json -Compress)"
    }
    [pscustomobject] @{ NativeMutatorCount = 0; CheckedUtc = [DateTime]::UtcNow.ToString('o') }
}

function Assert-RelativePathSafe {
    param([string] $Relative)
    if ([IO.Path]::IsPathRooted($Relative) -or
        ($Relative -split '[\\/]' -contains '..')) {
        throw "Unsafe recovery-relative path: $Relative"
    }
    $target = [IO.Path]::GetFullPath((Join-Path $nativeRoot $Relative))
    $backup = [IO.Path]::GetFullPath((Join-Path $backupRoot $Relative))
    if (-not (Test-ContainedPath $target $nativeRoot) -or
        -not (Test-ContainedPath $backup $backupRoot)) {
        throw "Recovery path escaped its pinned root: $Relative"
    }
    [pscustomobject] @{ Target = $target; Backup = $backup }
}

function Get-RootReceipts {
    $receipts = [Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $rootRelatives) {
        $paths = Assert-RelativePathSafe $relativeRoot
        if (-not [IO.Directory]::Exists($paths.Target) -or
            -not [IO.Directory]::Exists($paths.Backup)) {
            throw "Pinned recovery root is absent: $relativeRoot"
        }
        $expected = @{}
        foreach ($file in Get-ChildItem -LiteralPath $paths.Backup -File -Recurse) {
            $relative = [IO.Path]::GetRelativePath($paths.Backup, $file.FullName)
            $expected[$relative] = [pscustomobject] @{
                Bytes = [int64] $file.Length
                Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
                LastWriteUtc = $file.LastWriteTimeUtc.ToString('o')
            }
        }
        $actualFiles = @(Get-ChildItem -LiteralPath $paths.Target -File -Recurse)
        $actual = @{}
        foreach ($file in $actualFiles) {
            $relative = [IO.Path]::GetRelativePath($paths.Target, $file.FullName)
            $actual[$relative] = [pscustomobject] @{
                Bytes = [int64] $file.Length
                Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
                LastWriteUtc = $file.LastWriteTimeUtc.ToString('o')
            }
        }
        $missing = @($expected.Keys | Where-Object { -not $actual.ContainsKey($_) } | Sort-Object)
        $added = @($actual.Keys | Where-Object { -not $expected.ContainsKey($_) } | Sort-Object)
        $different = @(
            $expected.Keys | Where-Object {
                $actual.ContainsKey($_) -and
                ([int64] $expected[$_].Bytes -ne [int64] $actual[$_].Bytes -or
                 [string] $expected[$_].Sha256 -cne [string] $actual[$_].Sha256 -or
                 [string] $expected[$_].LastWriteUtc -cne [string] $actual[$_].LastWriteUtc)
            } | Sort-Object
        )
        if ($missing.Count -ne 0 -or $added.Count -ne 0 -or $different.Count -ne 0) {
            throw "Full build-root recovery verification failed: root=$relativeRoot missing=$($missing.Count) added=$($added.Count) different=$($different.Count)"
        }
        $receipts.Add([pscustomobject] [ordered] @{
            RelativeRoot = $relativeRoot
            FileCount = $expected.Count
            MissingCount = 0
            AddedCount = 0
            DifferentCount = 0
            Exact = $true
        })
    }
    @($receipts)
}

function Initialize-RestartManagerProbe {
    if ($null -ne ('Triad.RollbackRecovery.RestartManagerProbe' -as [type])) {
        return
    }
    $source = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace Triad.RollbackRecovery
{
    public static class RestartManagerProbe
    {
        private const int ERROR_SUCCESS = 0;
        private const int ERROR_MORE_DATA = 234;
        private const int CCH_RM_SESSION_KEY = 32;
        private const int CCH_RM_MAX_APP_NAME = 255;
        private const int CCH_RM_MAX_SVC_NAME = 63;

        [StructLayout(LayoutKind.Sequential)]
        private struct RM_UNIQUE_PROCESS
        {
            public int dwProcessId;
            public uint ProcessStartTimeLow;
            public uint ProcessStartTimeHigh;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct RM_PROCESS_INFO
        {
            public RM_UNIQUE_PROCESS Process;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCH_RM_MAX_APP_NAME + 1)]
            public string strAppName;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCH_RM_MAX_SVC_NAME + 1)]
            public string strServiceShortName;
            public uint ApplicationType;
            public uint AppStatus;
            public uint TSSessionId;
            [MarshalAs(UnmanagedType.Bool)]
            public bool bRestartable;
        }

        [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)]
        private static extern int RmStartSession(
            out uint pSessionHandle, int dwSessionFlags,
            [Out] StringBuilder strSessionKey);

        [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)]
        private static extern int RmRegisterResources(
            uint dwSessionHandle, uint nFiles,
            [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPWStr)] string[] rgsFilenames,
            uint nApplications, IntPtr rgApplications,
            uint nServices, IntPtr rgsServiceNames);

        [DllImport("rstrtmgr.dll")]
        private static extern int RmGetList(
            uint dwSessionHandle, out uint pnProcInfoNeeded,
            ref uint pnProcInfo,
            [In, Out] RM_PROCESS_INFO[] rgAffectedApps,
            ref uint lpdwRebootReasons);

        [DllImport("rstrtmgr.dll")]
        private static extern int RmEndSession(uint pSessionHandle);

        public static string[] GetHolders(string path)
        {
            uint handle;
            StringBuilder key = new StringBuilder(CCH_RM_SESSION_KEY + 1);
            int result = RmStartSession(out handle, 0, key);
            if (result != ERROR_SUCCESS)
                throw new InvalidOperationException("RmStartSession failed: " + result);
            try
            {
                result = RmRegisterResources(handle, 1, new[] { path }, 0, IntPtr.Zero, 0, IntPtr.Zero);
                if (result != ERROR_SUCCESS)
                    throw new InvalidOperationException("RmRegisterResources failed: " + result);
                uint needed = 0;
                uint count = 0;
                uint reasons = 0;
                result = RmGetList(handle, out needed, ref count, null, ref reasons);
                if (result == ERROR_SUCCESS)
                    return Array.Empty<string>();
                if (result != ERROR_MORE_DATA)
                    throw new InvalidOperationException("RmGetList(size) failed: " + result);
                RM_PROCESS_INFO[] rows = new RM_PROCESS_INFO[needed];
                count = needed;
                result = RmGetList(handle, out needed, ref count, rows, ref reasons);
                if (result != ERROR_SUCCESS)
                    throw new InvalidOperationException("RmGetList(data) failed: " + result);
                List<string> holders = new List<string>();
                for (int i = 0; i < count; i++)
                    holders.Add(rows[i].Process.dwProcessId + ":" + rows[i].strAppName);
                return holders.ToArray();
            }
            finally
            {
                RmEndSession(handle);
            }
        }
    }
}
'@
    [void] (Add-Type -Language CSharp -TypeDefinition $source)
}

function Assert-PchUnheld {
    $pch = [IO.Path]::GetFullPath((Join-Path $nativeRoot $pchRelative))
    $state = Assert-FilePin ([pscustomobject] @{
        Path=$pch; Bytes=$pchBytes; Sha256=$pchSha256
    }) 'run-04 mapped PCH'
    Initialize-RestartManagerProbe
    $holders = @([Triad.RollbackRecovery.RestartManagerProbe]::GetHolders($pch))
    if ($holders.Count -ne 0) {
        throw "Recovery refused because Restart Manager reports PCH holders: $([string]::Join(',', $holders))"
    }
    $stream = $null
    try {
        $stream = [IO.File]::Open(
            $pch, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite,
            [IO.FileShare]::None)
    }
    finally {
        if ($null -ne $stream) { $stream.Dispose() }
    }
    [pscustomobject] [ordered] @{
        Path=$pch
        Bytes=[int64] $state.Bytes
        Sha256=[string] $state.Sha256
        RestartManagerHolderCount=0
        ExclusiveOpenProbe='PASS'
    }
}

function Assert-NoBuildTreeReparsePoints {
    foreach ($relativeRoot in $rootRelatives) {
        foreach ($base in @($nativeRoot, $backupRoot)) {
            $root = [IO.Path]::GetFullPath((Join-Path $base $relativeRoot))
            if (-not [IO.Directory]::Exists($root)) {
                throw "Pinned build root is absent: $root"
            }
            $entries = @((Get-Item -LiteralPath $root -Force)) +
                @(Get-ChildItem -LiteralPath $root -Force -Recurse)
            $reparse = @($entries | Where-Object {
                ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
            })
            if ($reparse.Count -ne 0) {
                throw "Recovery refuses reparse traversal below ${root}: $($reparse[0].FullName)"
            }
        }
    }
}

function Get-AllowedBuildPathPair {
    param([string] $Relative)
    if ([IO.Path]::IsPathRooted($Relative) -or
        ($Relative -split '[\\/]' -contains '..')) {
        throw "Unsafe build-relative recovery path: $Relative"
    }
    $target = [IO.Path]::GetFullPath((Join-Path $nativeRoot $Relative))
    $matchingRoots = @($rootRelatives | Where-Object {
        $allowedRoot = [IO.Path]::GetFullPath((Join-Path $nativeRoot $_))
        -not $target.Equals($allowedRoot, [StringComparison]::OrdinalIgnoreCase) -and
            (Test-ContainedPath $target $allowedRoot)
    })
    if ($matchingRoots.Count -ne 1) {
        throw "Mutation path is not a strict descendant of exactly one pinned build root: $Relative"
    }
    $backup = [IO.Path]::GetFullPath((Join-Path $backupRoot $Relative))
    $allowedBackupRoot = [IO.Path]::GetFullPath(
        (Join-Path $backupRoot $matchingRoots[0]))
    if (-not (Test-ContainedPath $backup $allowedBackupRoot) -or
        $backup.Equals($allowedBackupRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Backup path escaped its matching pinned build root: $Relative"
    }
    [pscustomobject] @{
        RelativePath=$Relative
        RootLabel=[string] $matchingRoots[0]
        Target=$target
        Backup=$backup
    }
}

function Get-OrdinalSortedStrings {
    param([object[]] $Values)
    [string[]] $copy = @($Values | ForEach-Object { [string] $_ })
    [Array]::Sort($copy, [StringComparer]::Ordinal)
    $copy
}

function Get-LfUtf8Sha256 {
    param([object[]] $Lines)
    [string[]] $sorted = @(Get-OrdinalSortedStrings $Lines)
    $payload = [string]::Join("`n", $sorted) + "`n"
    $encoding = [Text.UTF8Encoding]::new($false)
    [Convert]::ToHexString(
        [Security.Cryptography.SHA256]::HashData($encoding.GetBytes($payload)))
}

function Get-BuildSnapshot {
    param([ValidateSet('Native','Backup')] [string] $Kind)
    $base = if ($Kind -ceq 'Native') { $nativeRoot } else { $backupRoot }
    $allFiles = [Collections.Generic.List[object]]::new()
    $allDirectories = [Collections.Generic.List[string]]::new()
    $allLines = [Collections.Generic.List[string]]::new()
    $perRoot = [Collections.Generic.List[object]]::new()
    foreach ($rootPin in $rootPins) {
        $rootLabel = [string] $rootPin.Label
        $root = [IO.Path]::GetFullPath((Join-Path $base $rootLabel))
        if (-not [IO.Directory]::Exists($root)) {
            throw "$Kind snapshot root is absent: $root"
        }
        foreach ($directory in Get-ChildItem -LiteralPath $root -Directory -Force -Recurse) {
            $rootRelative = [IO.Path]::GetRelativePath($root, $directory.FullName)
            $allDirectories.Add((Join-Path $rootLabel $rootRelative))
        }
        $rootLines = [Collections.Generic.List[string]]::new()
        $rootBytes = 0L
        foreach ($file in Get-ChildItem -LiteralPath $root -File -Force -Recurse) {
            $rootRelative = [IO.Path]::GetRelativePath($root, $file.FullName)
            $nativeRelative = Join-Path $rootLabel $rootRelative
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            $ticks = [int64] $file.LastWriteTimeUtc.Ticks
            $slashRelative = $rootRelative.Replace([char]0x5C, [char]0x2F)
            $line = "$rootLabel|$slashRelative|$([int64] $file.Length)|$ticks|$hash"
            $row = [pscustomobject] [ordered] @{
                RootLabel=$rootLabel
                RootRelativePath=$rootRelative
                NativeRelativePath=$nativeRelative
                FullPath=[IO.Path]::GetFullPath($file.FullName)
                Bytes=[int64] $file.Length
                Sha256=$hash
                LastWriteUtcTicks=$ticks
                LastWriteUtc=$file.LastWriteTimeUtc.ToString('o')
                ManifestLine=$line
            }
            $allFiles.Add($row)
            $rootLines.Add($line)
            $allLines.Add($line)
            $rootBytes += [int64] $file.Length
        }
        $perRoot.Add([pscustomobject] [ordered] @{
            RootLabel=$rootLabel
            FileCount=$rootLines.Count
            TotalBytes=$rootBytes
            ManifestSha256=Get-LfUtf8Sha256 @($rootLines)
        })
    }
    [pscustomobject] [ordered] @{
        Kind=$Kind
        Files=@($allFiles)
        Directories=@($allDirectories)
        FileCount=$allFiles.Count
        TotalBytes=[int64] (@($allFiles | Measure-Object -Property Bytes -Sum).Sum)
        ManifestSha256=Get-LfUtf8Sha256 @($allLines)
        PerRoot=@($perRoot)
    }
}

function New-RowMap {
    param([object[]] $Rows)
    $map = [Collections.Generic.Dictionary[string,object]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $Rows) {
        $map.Add([string] $row.NativeRelativePath, $row)
    }
    $map
}

function Compare-BuildSnapshots {
    param($Baseline, $Current)
    $baselineMap = New-RowMap @($Baseline.Files)
    $currentMap = New-RowMap @($Current.Files)
    $missing = [Collections.Generic.List[string]]::new()
    $added = [Collections.Generic.List[string]]::new()
    $contentDifferent = [Collections.Generic.List[string]]::new()
    $timestampOnly = [Collections.Generic.List[string]]::new()
    foreach ($entry in $baselineMap.GetEnumerator()) {
        if (-not $currentMap.ContainsKey($entry.Key)) {
            $missing.Add($entry.Key)
            continue
        }
        $expected = $entry.Value
        $actual = $currentMap[$entry.Key]
        if ([int64] $expected.Bytes -ne [int64] $actual.Bytes -or
            [string] $expected.Sha256 -cne [string] $actual.Sha256) {
            $contentDifferent.Add($entry.Key)
        }
        elseif ([int64] $expected.LastWriteUtcTicks -ne
                [int64] $actual.LastWriteUtcTicks) {
            $timestampOnly.Add($entry.Key)
        }
    }
    foreach ($entry in $currentMap.GetEnumerator()) {
        if (-not $baselineMap.ContainsKey($entry.Key)) {
            $added.Add($entry.Key)
        }
    }

    $baselineDirectories = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($path in @($Baseline.Directories)) { [void] $baselineDirectories.Add($path) }
    $currentDirectories = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($path in @($Current.Directories)) { [void] $currentDirectories.Add($path) }
    $missingDirectories = @($baselineDirectories | Where-Object {
        -not $currentDirectories.Contains($_)
    })
    $addedDirectories = @($currentDirectories | Where-Object {
        -not $baselineDirectories.Contains($_)
    })
    [pscustomobject] [ordered] @{
        Missing=@(Get-OrdinalSortedStrings @($missing))
        Added=@(Get-OrdinalSortedStrings @($added))
        ContentDifferent=@(Get-OrdinalSortedStrings @($contentDifferent))
        TimestampOnly=@(Get-OrdinalSortedStrings @($timestampOnly))
        MissingDirectories=@(Get-OrdinalSortedStrings $missingDirectories)
        AddedDirectories=@(Get-OrdinalSortedStrings $addedDirectories)
    }
}

function Assert-ExactPathSet {
    param([object[]] $Actual, [object[]] $Expected, [string] $Label)
    $actualJson = ConvertTo-Json @(Get-OrdinalSortedStrings $Actual) -Compress
    $expectedJson = ConvertTo-Json @(Get-OrdinalSortedStrings $Expected) -Compress
    if ($actualJson -cne $expectedJson) {
        throw "$Label set drifted: expected=$expectedJson actual=$actualJson"
    }
}

function Assert-PinnedBackupSnapshot {
    param($Snapshot)
    if ([int] $Snapshot.FileCount -ne $expectedBackupFileCount -or
        [int64] $Snapshot.TotalBytes -ne $expectedBackupBytes -or
        [string] $Snapshot.ManifestSha256 -cne $expectedBackupManifestSha256) {
        throw "Rollback backup closure pin mismatch: files=$($Snapshot.FileCount) bytes=$($Snapshot.TotalBytes) manifest=$($Snapshot.ManifestSha256)"
    }
    $actualRoots = @{}
    foreach ($row in @($Snapshot.PerRoot)) { $actualRoots[[string] $row.RootLabel] = $row }
    foreach ($pin in $rootPins) {
        if (-not $actualRoots.ContainsKey([string] $pin.Label)) {
            throw "Rollback backup root receipt is absent: $($pin.Label)"
        }
        $actual = $actualRoots[[string] $pin.Label]
        if ([int] $actual.FileCount -ne [int] $pin.FileCount -or
            [string] $actual.ManifestSha256 -cne [string] $pin.ManifestSha256) {
            throw "Rollback backup per-root pin mismatch: $($pin.Label)"
        }
    }
    [pscustomobject] [ordered] @{
        FileCount=[int] $Snapshot.FileCount
        TotalBytes=[int64] $Snapshot.TotalBytes
        ManifestSha256=[string] $Snapshot.ManifestSha256
        PerRoot=@($Snapshot.PerRoot)
    }
}

function Assert-PreflightDelta {
    param($Baseline, $Current, $Delta)
    Assert-ExactPathSet @($Delta.Missing) @() 'missing baseline file'
    Assert-ExactPathSet @($Delta.ContentDifferent) @($contentDifferent) 'content-different file'
    Assert-ExactPathSet @($Delta.TimestampOnly) @($timestampOnly) 'timestamp-only file'
    Assert-ExactPathSet @($Delta.Added) @($addedFiles) 'added file'
    Assert-ExactPathSet @($Delta.MissingDirectories) @() 'missing baseline directory'
    Assert-ExactPathSet @($Delta.AddedDirectories) @($addedEmptyDirectory) 'added directory'
    if ([int] $Current.FileCount -ne $expectedBackupFileCount + $addedFiles.Count) {
        throw "Unexpected current file count: $($Current.FileCount)"
    }
    $currentMap = New-RowMap @($Current.Files)
    foreach ($relative in $contentDifferent) {
        if (-not $currentMap.ContainsKey($relative) -or
            -not $contentDifferentCurrentPins.ContainsKey($relative)) {
            throw "Audited content-different file is absent at preflight: $relative"
        }
        $actual = $currentMap[$relative]
        $pin = $contentDifferentCurrentPins[$relative]
        if ([int64] $actual.Bytes -ne [int64] $pin.Bytes -or
            [string] $actual.Sha256 -cne [string] $pin.Sha256 -or
            [int64] $actual.LastWriteUtcTicks -ne [int64] $pin.LastWriteUtcTicks) {
            throw "Audited content-different file changed before recovery: $relative"
        }
    }
    foreach ($relative in $addedFiles) {
        if (-not $currentMap.ContainsKey($relative)) {
            throw "Audited added file is absent at preflight: $relative"
        }
        $actualHash = [string] $currentMap[$relative].Sha256
        $expectedHash = [string] $addedFilePins[$relative]
        if ($actualHash -cne $expectedHash) {
            throw "Audited added file changed before quarantine: $relative"
        }
    }
    [pscustomobject] [ordered] @{
        BaselineFileCount=[int] $Baseline.FileCount
        CurrentFileCount=[int] $Current.FileCount
        MissingCount=0
        ContentDifferentCount=$Delta.ContentDifferent.Count
        TimestampOnlyCount=$Delta.TimestampOnly.Count
        AddedCount=$Delta.Added.Count
        MissingDirectoryCount=0
        AddedEmptyDirectories=@($Delta.AddedDirectories)
        ContentDifferentFilesStatePinned=$true
        AddedFilesHashPinned=$true
    }
}

function Assert-ReceiptFileState {
    param([string] $Path, $Expected, [string] $Label)
    $actual = Get-FileIdentity $Path
    if (-not $actual.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label identity mismatch: $Path"
    }
    $actual
}

function Assert-ReceiptTree {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $expectedMap = [Collections.Generic.Dictionary[string,object]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $Expected) { $expectedMap.Add([string] $row.RelativePath, $row) }
    $files = @(Get-ChildItem -LiteralPath $Root -File -Force -Recurse)
    if ($files.Count -ne $expectedMap.Count) {
        throw "$Label file-count mismatch: expected=$($expectedMap.Count) actual=$($files.Count)"
    }
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($Root, $file.FullName)
        if (-not $expectedMap.ContainsKey($relative)) {
            throw "$Label has an unexpected file: $relative"
        }
        [void] (Assert-ReceiptFileState $file.FullName $expectedMap[$relative] $Label)
    }
    [pscustomobject] @{ Label=$Label; FileCount=$files.Count; Exact=$true }
}

function Assert-ProtectedNativeState {
    param($Admission)
    $projectFile = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'TRIAD.uproject'))
    $airSimDescriptor = [IO.Path]::GetFullPath(
        (Join-Path $nativeRoot 'Plugins\AirSimTriadRuntime\AirSimTriadRuntime.uplugin'))
    [void] (Assert-ReceiptFileState $projectFile $Admission.ImmutableNativeState.ProjectFile 'TRIAD project descriptor')
    [void] (Assert-ReceiptFileState $airSimDescriptor $Admission.ImmutableNativeState.AirSimDescriptor 'AirSim descriptor')
    [void] (Assert-FilePin $mapPin 'R30 map')
    [void] (Assert-FilePin $runtimeDllPin 'R30 runtime editor DLL')
    [void] (Assert-FilePin $editorDllPin 'R30 editor DLL')
    $treeSpecs = @(
        [pscustomobject] @{ Name='R30'; Root='Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30' },
        [pscustomobject] @{ Name='R29Facade'; Root='Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29' },
        [pscustomobject] @{ Name='R29Vegetation'; Root='Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29' },
        [pscustomobject] @{ Name='R29Terrain'; Root='Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback' },
        [pscustomobject] @{ Name='TreeRealism'; Root='Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism' },
        [pscustomobject] @{ Name='ContextFacadeR25'; Root='Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25' },
        [pscustomobject] @{ Name='AirSimContent'; Root='Plugins\AirSimTriadRuntime\Content' }
    )
    $trees = [Collections.Generic.List[object]]::new()
    foreach ($spec in $treeSpecs) {
        $expected = @($Admission.ImmutableNativeState.($spec.Name))
        $root = [IO.Path]::GetFullPath((Join-Path $nativeRoot $spec.Root))
        $trees.Add((Assert-ReceiptTree $root $expected "protected $($spec.Name) tree"))
    }
    foreach ($pin in $r30SourcePins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeRoot $pin.RelativePath))
        if (-not (Test-ContainedPath $path $nativeRoot)) {
            throw "R30 source pin escaped native root: $($pin.RelativePath)"
        }
        [void] (Assert-ReceiptFileState $path $pin 'committed R30 source')
    }
    foreach ($relative in $captureSourceRelatives) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeRoot $relative))
        if ([IO.File]::Exists($path)) {
            throw "Temporary capture-library source does not match absent prestate: $path"
        }
    }
    [pscustomobject] [ordered] @{
        ProjectDescriptorExact=$true
        AirSimDescriptorExact=$true
        MapAndEditorDllPinsExact=$true
        ContentTrees=@($trees)
        CommittedR30SourcePinCount=$r30SourcePins.Count
        CaptureLibrarySourcesAbsent=$true
    }
}

function Write-JsonNewAtomic {
    param($Value, [string] $Path, [string] $AllowedRoot)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not (Test-ContainedPath $full $AllowedRoot) -or
        $full.Equals([IO.Path]::GetFullPath($AllowedRoot), [StringComparison]::OrdinalIgnoreCase) -or
        [IO.File]::Exists($full)) {
        throw "Atomic recovery receipt refused unsafe or existing path: $full"
    }
    [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($full))
    $temporary = "$full.tmp.$PID"
    try {
        $Value | ConvertTo-Json -Depth 16 |
            Set-Content -LiteralPath $temporary -Encoding utf8NoBOM
        $stagedState = Get-FileIdentity $temporary
    }
    catch {
        if ([IO.File]::Exists($temporary)) {
            Remove-Item -LiteralPath $temporary -Force
        }
        throw
    }
    try {
        Move-Item -LiteralPath $temporary -Destination $full
    }
    catch {
        if ([IO.File]::Exists($temporary)) {
            Remove-Item -LiteralPath $temporary -Force
        }
        throw
    }
    [pscustomobject] [ordered] @{
        Path=$full
        Present=$true
        Bytes=[int64] $stagedState.Bytes
        Sha256=[string] $stagedState.Sha256
        LastWriteUtc=[string] $stagedState.LastWriteUtc
    }
}

function Test-SnapshotRowAtPath {
    param($Row, [string] $Path)
    if (-not [IO.File]::Exists($Path)) { return $false }
    $state = Get-FileIdentity $Path
    $ticks = (Get-Item -LiteralPath $Path -Force).LastWriteTimeUtc.Ticks
    $state.Present -and
        [int64] $state.Bytes -eq [int64] $Row.Bytes -and
        [string] $state.Sha256 -ceq [string] $Row.Sha256 -and
        [int64] $ticks -eq [int64] $Row.LastWriteUtcTicks
}

function Invoke-CompensatingPreflightRestore {
    param(
        $PreflightSnapshot,
        [string] $PreRecoveryRoot,
        [string] $QuarantineRoot
    )
    [void] (Assert-NativeMutatorsQuiescent)
    [void] (Assert-PchUnheld)
    Assert-NoBuildTreeReparsePoints
    $preflightMap = New-RowMap @($PreflightSnapshot.Files)
    $additionRestoreCount = 0
    $contentRestoreCount = 0
    $timestampRestoreCount = 0

    foreach ($relative in $addedFiles) {
        if (-not $preflightMap.ContainsKey($relative)) {
            throw "Compensation lacks the preflight addition row: $relative"
        }
        $pair = Get-AllowedBuildPathPair $relative
        $expected = $preflightMap[$relative]
        $quarantinedPath = [IO.Path]::GetFullPath(
            (Join-Path $QuarantineRoot $relative))
        if (-not (Test-ContainedPath $quarantinedPath $QuarantineRoot)) {
            throw "Compensation quarantine path escaped: $relative"
        }
        if ([IO.File]::Exists($quarantinedPath)) {
            Assert-NoReparsePathAncestors $quarantinedPath
            if ([IO.File]::Exists($pair.Target)) {
                throw "Compensation found both native and quarantined addition: $relative"
            }
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($pair.Target))
            Move-Item -LiteralPath $quarantinedPath -Destination $pair.Target
            $additionRestoreCount++
        }
        [IO.File]::SetLastWriteTimeUtc(
            $pair.Target,
            [DateTime]::new(
                [int64] $expected.LastWriteUtcTicks,
                [DateTimeKind]::Utc))
        if (-not (Test-SnapshotRowAtPath $expected $pair.Target)) {
            throw "Compensation failed to restore added file: $relative"
        }
    }

    foreach ($relative in $contentDifferent) {
        if (-not $preflightMap.ContainsKey($relative)) {
            throw "Compensation lacks the preflight content row: $relative"
        }
        $pair = Get-AllowedBuildPathPair $relative
        $expected = $preflightMap[$relative]
        if (-not (Test-SnapshotRowAtPath $expected $pair.Target)) {
            $preserved = [IO.Path]::GetFullPath(
                (Join-Path $PreRecoveryRoot $relative))
            if (-not (Test-ContainedPath $preserved $PreRecoveryRoot) -or
                -not [IO.File]::Exists($preserved)) {
                throw "Compensation pre-recovery copy is absent: $relative"
            }
            Assert-NoReparsePathAncestors $preserved
            $preservedState = Get-FileIdentity $preserved
            if ([int64] $preservedState.Bytes -ne [int64] $expected.Bytes -or
                [string] $preservedState.Sha256 -cne [string] $expected.Sha256) {
                throw "Compensation pre-recovery copy changed: $relative"
            }
            Copy-Item -LiteralPath $preserved -Destination $pair.Target -Force
            $contentRestoreCount++
        }
        [IO.File]::SetLastWriteTimeUtc(
            $pair.Target,
            [DateTime]::new(
                [int64] $expected.LastWriteUtcTicks,
                [DateTimeKind]::Utc))
        if (-not (Test-SnapshotRowAtPath $expected $pair.Target)) {
            throw "Compensation failed to restore content-different file: $relative"
        }
    }

    foreach ($relative in $timestampOnly) {
        if (-not $preflightMap.ContainsKey($relative)) {
            throw "Compensation lacks the preflight timestamp row: $relative"
        }
        $pair = Get-AllowedBuildPathPair $relative
        $expected = $preflightMap[$relative]
        $state = Get-FileIdentity $pair.Target
        if (-not $state.Present -or
            [int64] $state.Bytes -ne [int64] $expected.Bytes -or
            [string] $state.Sha256 -cne [string] $expected.Sha256) {
            $backupState = Get-FileIdentity $pair.Backup
            if (-not $backupState.Present -or
                [int64] $backupState.Bytes -ne [int64] $expected.Bytes -or
                [string] $backupState.Sha256 -cne [string] $expected.Sha256) {
                throw "Compensation timestamp-only backup cannot reproduce preflight content: $relative"
            }
            Copy-Item -LiteralPath $pair.Backup -Destination $pair.Target -Force
        }
        [IO.File]::SetLastWriteTimeUtc(
            $pair.Target,
            [DateTime]::new(
                [int64] $expected.LastWriteUtcTicks,
                [DateTimeKind]::Utc))
        if (-not (Test-SnapshotRowAtPath $expected $pair.Target)) {
            throw "Compensation failed to restore timestamp-only file: $relative"
        }
        $timestampRestoreCount++
    }

    $emptyPair = Get-AllowedBuildPathPair $addedEmptyDirectory
    if ([IO.File]::Exists($emptyPair.Target)) {
        throw "Compensation found a file at the added empty-directory path."
    }
    if (-not [IO.Directory]::Exists($emptyPair.Target)) {
        [void] [IO.Directory]::CreateDirectory($emptyPair.Target)
    }
    if (@(Get-ChildItem -LiteralPath $emptyPair.Target -Force).Count -ne 0) {
        throw "Compensation cannot restore a nonempty added directory."
    }
    [IO.Directory]::SetLastWriteTimeUtc(
        $emptyPair.Target, $addedEmptyDirectoryCurrentUtc)

    [void] (Assert-NativeMutatorsQuiescent)
    [void] (Assert-PchUnheld)
    $restoredSnapshot = Get-BuildSnapshot 'Native'
    $delta = Compare-BuildSnapshots $PreflightSnapshot $restoredSnapshot
    Assert-ExactPathSet @($delta.Missing) @() 'compensation missing file'
    Assert-ExactPathSet @($delta.Added) @() 'compensation added file'
    Assert-ExactPathSet @($delta.ContentDifferent) @() 'compensation content difference'
    Assert-ExactPathSet @($delta.TimestampOnly) @() 'compensation timestamp difference'
    Assert-ExactPathSet @($delta.MissingDirectories) @() 'compensation missing directory'
    Assert-ExactPathSet @($delta.AddedDirectories) @() 'compensation added directory'
    if ([int] $restoredSnapshot.FileCount -ne [int] $PreflightSnapshot.FileCount -or
        [int64] $restoredSnapshot.TotalBytes -ne [int64] $PreflightSnapshot.TotalBytes -or
        [string] $restoredSnapshot.ManifestSha256 -cne
            [string] $PreflightSnapshot.ManifestSha256) {
        throw 'Compensation did not reproduce the complete preflight closure.'
    }
    [pscustomobject] [ordered] @{
        Status='ROLLED_BACK_TO_PREFLIGHT'
        AddedFilesRestoredFromQuarantine=$additionRestoreCount
        ContentFilesRestoredFromPreservedCopies=$contentRestoreCount
        TimestampFilesRestored=$timestampRestoreCount
        EmptyDirectoryRestored=$true
        PreflightManifestSha256=[string] $PreflightSnapshot.ManifestSha256
        RestoredManifestSha256=[string] $restoredSnapshot.ManifestSha256
        FullClosureExact=$true
    }
}

if ($Execute) {
    $startedUtc = [DateTime]::UtcNow
    $mutationStarted = $false
    $recoveryCommitted = $false
    $current = $null
    $currentMap = $null
    $admission = $null
    $preRecoveryRoot = $null
    $quarantineRoot = $null
    $preRecoveryCopies = [Collections.Generic.List[object]]::new()
    $quarantined = [Collections.Generic.List[object]]::new()
    $restored = [Collections.Generic.List[object]]::new()
    try {
        if (-not $runRoot.Equals(
                [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04'),
                [StringComparison]::OrdinalIgnoreCase) -or
            -not $backupRoot.Equals(
                [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\rollback\build'),
                [StringComparison]::OrdinalIgnoreCase) -or
            -not $nativeRoot.Equals(
                [IO.Path]::GetFullPath('D:\triad\TRIAD'),
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Pinned recovery root identities drifted.'
        }
        if (-not (Test-ContainedPath $recoveryRoot $runRoot) -or
            $recoveryRoot.Equals($runRoot, [StringComparison]::OrdinalIgnoreCase) -or
            [IO.Directory]::Exists($recoveryRoot) -or
            [IO.File]::Exists($recoveryRoot)) {
            throw "Recovery output must be a new strict evidence child: $recoveryRoot"
        }
        if ($contentDifferent.Count -ne 14 -or $timestampOnly.Count -ne 18 -or
            $contentDifferentCurrentPins.Count -ne 14 -or
            $addedFiles.Count -ne 24 -or $addedFilePins.Count -ne 24 -or
            $rootRelatives.Count -ne 6 -or $r30SourcePins.Count -ne 11) {
            throw 'Pinned recovery target counts drifted.'
        }

        $evidenceBefore = @($evidencePins | ForEach-Object {
            Assert-FilePin $_ ([string] $_.Label)
        })
        $rollback = Get-Content -LiteralPath $originalRollback -Raw | ConvertFrom-Json
        $admission = Get-Content -LiteralPath $admissionReceipt -Raw | ConvertFrom-Json
        $commit = Get-Content -LiteralPath $commitReceipt -Raw | ConvertFrom-Json
        if ([string] $rollback.RunToken -cne 'r30-capture-20260908-04' -or
            [string] $rollback.Status -cne 'ROLLBACK_INCOMPLETE' -or
            [bool] $rollback.NativeMutationQuiescenceProven -ne $true -or
            [string] $admission.Status -cne 'ADMITTED_BEFORE_NATIVE_SOURCE_PROMOTION' -or
            [string] $admission.RunToken -cne 'r30-capture-20260908-04' -or
            [string] $commit.Status -cne 'COMMITTED' -or
            [string] $commit.RunToken -cne 'r30-native-20260908-19') {
            throw 'Pinned evidence receipts do not authorize this exact recovery.'
        }

        foreach ($path in @($nativeRoot, $runRoot, $backupRoot, $recoveryRoot)) {
            Assert-NoReparsePathAncestors $path
        }
        Assert-NoBuildTreeReparsePoints
        $nativeIdleBefore = Assert-NativeMutatorsQuiescent
        $pchBefore = Assert-PchUnheld

        $baseline = Get-BuildSnapshot 'Backup'
        $backupClosure = Assert-PinnedBackupSnapshot $baseline
        $current = Get-BuildSnapshot 'Native'
        $preflightDelta = Compare-BuildSnapshots $baseline $current
        $preflight = Assert-PreflightDelta $baseline $current $preflightDelta
        $protectedBefore = Assert-ProtectedNativeState $admission

        $evidencePreMutation = @($evidencePins | ForEach-Object {
            Assert-FilePin $_ ([string] $_.Label)
        })
        [void] (Assert-NativeMutatorsQuiescent)
        [void] (Assert-PchUnheld)

        [void] [IO.Directory]::CreateDirectory($recoveryRoot)
        $mutationStarted = $true
        $preRecoveryRoot = [IO.Path]::GetFullPath(
            (Join-Path $recoveryRoot 'pre-recovery-current'))
        $quarantineRoot = [IO.Path]::GetFullPath(
            (Join-Path $recoveryRoot 'quarantine'))
        [void] [IO.Directory]::CreateDirectory($preRecoveryRoot)
        [void] [IO.Directory]::CreateDirectory($quarantineRoot)
        foreach ($path in @($recoveryRoot, $preRecoveryRoot, $quarantineRoot)) {
            Assert-NoReparsePathAncestors $path
        }

        $currentMap = New-RowMap @($current.Files)
        foreach ($relative in $contentDifferent) {
            $pair = Get-AllowedBuildPathPair $relative
            Assert-NoReparsePathAncestors $pair.Target
            if (-not $currentMap.ContainsKey($relative)) {
                throw "Pre-recovery source vanished: $relative"
            }
            $expectedCurrent = $currentMap[$relative]
            $currentState = Get-FileIdentity $pair.Target
            if (-not $currentState.Present -or
                [int64] $currentState.Bytes -ne [int64] $expectedCurrent.Bytes -or
                [string] $currentState.Sha256 -cne [string] $expectedCurrent.Sha256) {
                throw "Pre-recovery source changed after preflight: $relative"
            }
            $destination = [IO.Path]::GetFullPath(
                (Join-Path $preRecoveryRoot $relative))
            if (-not (Test-ContainedPath $destination $preRecoveryRoot) -or
                [IO.File]::Exists($destination)) {
                throw "Unsafe or existing pre-recovery copy destination: $destination"
            }
            Assert-NoReparsePathAncestors $destination
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($destination))
            Assert-NoReparsePathAncestors ([IO.Path]::GetDirectoryName($destination))
            Copy-Item -LiteralPath $pair.Target -Destination $destination
            [IO.File]::SetLastWriteTimeUtc(
                $destination,
                [DateTime]::new(
                    [int64] $expectedCurrent.LastWriteUtcTicks,
                    [DateTimeKind]::Utc))
            $copyState = Get-FileIdentity $destination
            if ([int64] $copyState.Bytes -ne [int64] $expectedCurrent.Bytes -or
                [string] $copyState.Sha256 -cne [string] $expectedCurrent.Sha256) {
                throw "Pre-recovery copy verification failed: $relative"
            }
            $preRecoveryCopies.Add([pscustomobject] [ordered] @{
                RelativePath=$relative
                Bytes=[int64] $copyState.Bytes
                Sha256=[string] $copyState.Sha256
                LastWriteUtcTicks=[int64] $expectedCurrent.LastWriteUtcTicks
                PreservedAt=$destination
            })
        }

        foreach ($relative in $addedFiles) {
            $pair = Get-AllowedBuildPathPair $relative
            Assert-NoReparsePathAncestors $pair.Target
            if ([IO.File]::Exists($pair.Backup)) {
                throw "Added path unexpectedly exists in the pinned backup: $relative"
            }
            $state = Get-FileIdentity $pair.Target
            if (-not $state.Present -or
                [string] $state.Sha256 -cne [string] $addedFilePins[$relative]) {
                throw "Added file failed its immediate quarantine pin: $relative"
            }
            $destination = [IO.Path]::GetFullPath(
                (Join-Path $quarantineRoot $relative))
            if (-not (Test-ContainedPath $destination $quarantineRoot) -or
                [IO.File]::Exists($destination)) {
                throw "Unsafe or existing quarantine destination: $destination"
            }
            Assert-NoReparsePathAncestors $destination
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($destination))
            Assert-NoReparsePathAncestors ([IO.Path]::GetDirectoryName($destination))
            Move-Item -LiteralPath $pair.Target -Destination $destination
            $quarantineState = Get-FileIdentity $destination
            if ([IO.File]::Exists($pair.Target) -or
                -not $quarantineState.Present -or
                [string] $quarantineState.Sha256 -cne [string] $addedFilePins[$relative]) {
                throw "Exact quarantine move failed: $relative"
            }
            $quarantined.Add([pscustomobject] [ordered] @{
                RelativePath=$relative
                Bytes=[int64] $quarantineState.Bytes
                Sha256=[string] $quarantineState.Sha256
                QuarantinedAt=$destination
            })
        }

        $baselineMap = New-RowMap @($baseline.Files)
        foreach ($relative in @($contentDifferent) + @($timestampOnly)) {
            $pair = Get-AllowedBuildPathPair $relative
            if (-not $baselineMap.ContainsKey($relative) -or
                -not $currentMap.ContainsKey($relative) -or
                -not [IO.File]::Exists($pair.Backup)) {
                throw "Pinned baseline restore source is absent: $relative"
            }
            $expected = $baselineMap[$relative]
            $preflightCurrent = $currentMap[$relative]
            Assert-NoReparsePathAncestors $pair.Target
            Assert-NoReparsePathAncestors $pair.Backup
            if (-not (Test-SnapshotRowAtPath $preflightCurrent $pair.Target)) {
                throw "Restore target changed after the exact preflight: $relative"
            }
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($pair.Target))
            $lastCopyError = $null
            for ($attempt = 1; $attempt -le 5; $attempt++) {
                try {
                    if ($contentDifferentCurrentPins.ContainsKey($relative)) {
                        Copy-Item -LiteralPath $pair.Backup -Destination $pair.Target -Force
                    }
                    else {
                        $currentContent = Get-FileIdentity $pair.Target
                        if ([int64] $currentContent.Bytes -ne [int64] $expected.Bytes -or
                            [string] $currentContent.Sha256 -cne [string] $expected.Sha256) {
                            throw "Timestamp-only target content drifted: $relative"
                        }
                    }
                    [IO.File]::SetLastWriteTimeUtc(
                        $pair.Target,
                        [DateTime]::new(
                            [int64] $expected.LastWriteUtcTicks,
                            [DateTimeKind]::Utc))
                    $lastCopyError = $null
                    break
                }
                catch {
                    $lastCopyError = $_.Exception
                    if ($attempt -lt 5) { Start-Sleep -Milliseconds 250 }
                }
            }
            if ($null -ne $lastCopyError) { throw $lastCopyError }
            $actual = Get-FileIdentity $pair.Target
            $actualTicks = (Get-Item -LiteralPath $pair.Target -Force).LastWriteTimeUtc.Ticks
            if ([int64] $actual.Bytes -ne [int64] $expected.Bytes -or
                [string] $actual.Sha256 -cne [string] $expected.Sha256 -or
                [int64] $actualTicks -ne [int64] $expected.LastWriteUtcTicks) {
                throw "Exact baseline restore failed: $relative"
            }
            $restored.Add([pscustomobject] [ordered] @{
                RelativePath=$relative
                Bytes=[int64] $actual.Bytes
                Sha256=[string] $actual.Sha256
                LastWriteUtcTicks=[int64] $actualTicks
            })
        }

        $emptyPair = Get-AllowedBuildPathPair $addedEmptyDirectory
        Assert-NoReparsePathAncestors $emptyPair.Target
        if (-not [IO.Directory]::Exists($emptyPair.Target) -or
            [IO.File]::Exists($emptyPair.Target) -or
            @(Get-ChildItem -LiteralPath $emptyPair.Target -Force).Count -ne 0) {
            throw "Exact added empty-directory precondition failed: $($emptyPair.Target)"
        }
        [IO.Directory]::Delete($emptyPair.Target, $false)
        if ([IO.Directory]::Exists($emptyPair.Target)) {
            throw "Added empty directory survived bounded removal: $($emptyPair.Target)"
        }

        $nativeIdleAfter = Assert-NativeMutatorsQuiescent
        $pchAfter = Assert-PchUnheld
        $post = Get-BuildSnapshot 'Native'
        $postDelta = Compare-BuildSnapshots $baseline $post
        Assert-ExactPathSet @($postDelta.Missing) @() 'post-recovery missing file'
        Assert-ExactPathSet @($postDelta.Added) @() 'post-recovery added file'
        Assert-ExactPathSet @($postDelta.ContentDifferent) @() 'post-recovery content difference'
        Assert-ExactPathSet @($postDelta.TimestampOnly) @() 'post-recovery timestamp difference'
        Assert-ExactPathSet @($postDelta.MissingDirectories) @() 'post-recovery missing directory'
        Assert-ExactPathSet @($postDelta.AddedDirectories) @() 'post-recovery added directory'
        if ([int] $post.FileCount -ne $expectedBackupFileCount -or
            [int64] $post.TotalBytes -ne $expectedBackupBytes -or
            [string] $post.ManifestSha256 -cne $expectedBackupManifestSha256) {
            throw 'Post-recovery build closure does not equal the immutable backup pin.'
        }
        $baselineAfter = Get-BuildSnapshot 'Backup'
        $backupClosureAfter = Assert-PinnedBackupSnapshot $baselineAfter
        $protectedAfter = Assert-ProtectedNativeState $admission
        $evidenceAfter = @($evidencePins | ForEach-Object {
            Assert-FilePin $_ ([string] $_.Label)
        })

        $receipt = [pscustomobject] [ordered] @{
            Schema='triad.istana_explore_v5d.r30_capture_rollback_recovery.v1'
            Status='RECOVERED'
            SourceRunToken='r30-capture-20260908-04'
            RecoveryToken=$recoveryToken
            StartedUtc=$startedUtc.ToString('o')
            CompletedUtc=[DateTime]::UtcNow.ToString('o')
            EvidencePinsBefore=$evidenceBefore
            EvidencePinsImmediatelyBeforeMutation=$evidencePreMutation
            EvidencePinsAfter=$evidenceAfter
            BackupClosure=$backupClosure
            BackupClosureAfter=$backupClosureAfter
            Preflight=$preflight
            NativeIdleBefore=$nativeIdleBefore
            NativeIdleAfter=$nativeIdleAfter
            PchBefore=$pchBefore
            PchAfter=$pchAfter
            Mutation=[pscustomobject] [ordered] @{
                PreRecoveryCurrentCopies=@($preRecoveryCopies)
                QuarantinedAddedFiles=@($quarantined)
                RestoredBaselineFiles=@($restored)
                EmptyDirectoryRemoved=$addedEmptyDirectory
            }
            Postflight=[pscustomobject] [ordered] @{
                FileCount=[int] $post.FileCount
                TotalBytes=[int64] $post.TotalBytes
                ManifestSha256=[string] $post.ManifestSha256
                PerRoot=@($post.PerRoot)
                MissingCount=0
                AddedCount=0
                ContentDifferentCount=0
                TimestampOnlyCount=0
                MissingDirectoryCount=0
                AddedDirectoryCount=0
            }
            ProtectedStateBefore=$protectedBefore
            ProtectedStateAfter=$protectedAfter
            OriginalRollbackReceiptPreserved=$true
            RecoveryArtifactsPreserved=$true
        }
        $receiptState = Write-JsonNewAtomic $receipt $recoveryReceipt $recoveryRoot
        $recoveryCommitted = $true
        [pscustomobject] [ordered] @{
            Receipt=$receipt
            ReceiptFile=$receiptState
        } | ConvertTo-Json -Depth 18
        return
    }
    catch {
        $failure = $_.Exception
        $compensation = $null
        $compensationError = $null
        $publishedReceiptError = $null
        if (-not $recoveryCommitted -and [IO.File]::Exists($recoveryReceipt)) {
            try {
                $published = Get-Content -LiteralPath $recoveryReceipt -Raw |
                    ConvertFrom-Json
                if ([string] $published.Schema -cne
                        'triad.istana_explore_v5d.r30_capture_rollback_recovery.v1' -or
                    [string] $published.Status -cne 'RECOVERED' -or
                    [string] $published.SourceRunToken -cne
                        'r30-capture-20260908-04' -or
                    [string] $published.RecoveryToken -cne $recoveryToken) {
                    throw 'Published receipt content does not identify this recovery.'
                }
                $recoveryCommitted = $true
            }
            catch { $publishedReceiptError = $_.Exception }
        }
        if ($mutationStarted -and -not $recoveryCommitted -and
            $null -eq $publishedReceiptError -and
            $null -ne $current -and
            -not [string]::IsNullOrWhiteSpace($preRecoveryRoot) -and
            -not [string]::IsNullOrWhiteSpace($quarantineRoot)) {
            try {
                $compensation = Invoke-CompensatingPreflightRestore `
                    $current $preRecoveryRoot $quarantineRoot
                if ($null -ne $admission) {
                    [void] (Assert-ProtectedNativeState $admission)
                }
                foreach ($pin in $evidencePins) {
                    [void] (Assert-FilePin $pin ([string] $pin.Label))
                }
            }
            catch { $compensationError = $_.Exception }
        }
        if ($mutationStarted -and [IO.Directory]::Exists($recoveryRoot) -and
            -not [IO.File]::Exists($recoveryReceipt) -and
            -not [IO.File]::Exists($failedRecoveryReceipt)) {
            try {
                $failed = [pscustomobject] [ordered] @{
                    Schema='triad.istana_explore_v5d.r30_capture_rollback_recovery.v1'
                    Status='FAILED'
                    SourceRunToken='r30-capture-20260908-04'
                    RecoveryToken=$recoveryToken
                    StartedUtc=$startedUtc.ToString('o')
                    FailedUtc=[DateTime]::UtcNow.ToString('o')
                    Error=$failure.Message
                    Compensation=if ($null -ne $compensation) {
                        $compensation
                    } else { $null }
                    CompensationError=if ($null -ne $compensationError) {
                        $compensationError.Message
                    } else { $null }
                    PublishedReceiptError=if ($null -ne $publishedReceiptError) {
                        $publishedReceiptError.Message
                    } else { $null }
                    PreRecoveryCurrentCopies=@($preRecoveryCopies)
                    QuarantinedAddedFiles=@($quarantined)
                    RestoredBaselineFiles=@($restored)
                    OriginalEvidenceAndRollbackBackupMustBePreserved=$true
                }
                [void] (Write-JsonNewAtomic $failed $failedRecoveryReceipt $recoveryRoot)
            }
            catch { }
        }
        if ($null -ne $compensationError) {
            throw [InvalidOperationException]::new(
                "Recovery failed and compensation was incomplete: recovery={$($failure.Message)} compensation={$($compensationError.Message)}",
                $failure)
        }
        if ($null -ne $publishedReceiptError) {
            throw [InvalidOperationException]::new(
                "Recovery state requires manual inspection because a non-verifiable receipt path was published: $($publishedReceiptError.Message)",
                $failure)
        }
        if ($recoveryCommitted) {
            throw [InvalidOperationException]::new(
                "Recovery committed successfully but response serialization failed; use the published receipt: $recoveryReceipt",
                $failure)
        }
        if ($null -ne $compensation) {
            throw [InvalidOperationException]::new(
                "Recovery failed but exact preflight state was restored: $($failure.Message)",
                $failure)
        }
        throw $failure
    }
}

if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Status = 'INERT'
        Message = 'Supply -Execute to perform the pinned run-04 recovery.'
        RecoveryToken = $recoveryToken
        ContentRestoreCount = $contentDifferent.Count
        TimestampRestoreCount = $timestampOnly.Count
        AddedFileQuarantineCount = $addedFiles.Count
        AddedEmptyDirectoryRemovalCount = 1
        BuildRootCount = $rootRelatives.Count
    } | ConvertTo-Json -Depth 5
    return
}
