#requires -Version 7.0

<#
.SYNOPSIS
Runs the guarded V5D Broad Shell R31 native successor transaction.

.DESCRIPTION
The default invocation and -StaticSelfCheck are repository-only. -Execute is
the sole native-write authority and requires caller receipts for the exact
promoted-R30 map, both DLLs, and the two context-policy source files that R31
updates. R30 must already be committed and captured. The live transaction
promotes four new editor files and two exact journaled context-policy successors
from an immutable, versioned R31 repository source-closure overlay into their
canonical native source destinations. It also promotes one hash-bound R31
contract and the four exact contract-referenced files absent
from the native tree. Eleven already-present contract-referenced files are
hash-admitted and held immutable. It then builds serially, creates exactly five
isolated material packages, and performs one guarded map save.

R31 reuses the existing context-policy component in place. It changes only its
exact 17-entry material override roster and never calls a configuration path
that could reset provider state. Failure restores the map, both pre-existing
context-policy files, initially absent R31 files/content, and the complete
plugin build surfaces from exact journals.

.EXAMPLE
.\Invoke-IstanaExploreV5DBroadShellR31NativeTransactionV1.ps1 -RunToken review -StaticSelfCheck

.EXAMPLE
.\Invoke-IstanaExploreV5DBroadShellR31NativeTransactionV1.ps1 -RunToken reviewed -Execute -RequireR30Predecessor -R30CommitReceipt <r30-transaction-commit.json> -ExpectedR30CommitReceiptSha256 <sha256> -R30CaptureCommitReceipt <r30-capture-commit.json> -ExpectedR30CaptureCommitReceiptSha256 <sha256> -ExpectedMapBytes <r30-bytes> -ExpectedMapSha256 <r30-sha256> -ExpectedRuntimeDllBytes <r30-bytes> -ExpectedRuntimeDllSha256 <r30-sha256> -ExpectedEditorDllBytes <r30-bytes> -ExpectedEditorDllSha256 <r30-sha256> -ExpectedContextPolicyHeaderBytes <bytes> -ExpectedContextPolicyHeaderSha256 <sha256> -ExpectedContextPolicySourceBytes <bytes> -ExpectedContextPolicySourceSha256 <sha256>
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,
    [switch] $StaticSelfCheck,
    [switch] $RequireR30Predecessor,

    [string] $R30CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR30CommitReceiptSha256 = '',
    [string] $R30CaptureCommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR30CaptureCommitReceiptSha256 = '',

    [long] $ExpectedMapBytes = 0L,
    [string] $ExpectedMapSha256 = '',
    [long] $ExpectedRuntimeDllBytes = 0L,
    [string] $ExpectedRuntimeDllSha256 = '',
    [long] $ExpectedEditorDllBytes = 0L,
    [string] $ExpectedEditorDllSha256 = '',
    [long] $ExpectedContextPolicyHeaderBytes = 0L,
    [string] $ExpectedContextPolicyHeaderSha256 = '',
    [long] $ExpectedContextPolicySourceBytes = 0L,
    [string] $ExpectedContextPolicySourceSha256 = '',

    [ValidateRange(120, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$privateMemoryCeilingBytes = 12884901888L # fixed 12 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # fixed continuous 6 GiB
$memoryWatchdogPollMilliseconds = 500
$memoryWatchdogPersistentBreachMilliseconds = 2000
$script:memoryWatchdog = $null
$script:ownedNativeProcessHandles = [Collections.Generic.List[object]]::new()
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'))
$dotnet = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'))
$unrealBuildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'))
$engineSourceRoot = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Source'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath('C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1'))
$r30TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$r30EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence')
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$r31ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31'))
$r30ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'))
$r29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'))
$r28ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28'))
$vegetationR29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
$landmarkVegetationR28ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28'))
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$terrainR29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback'))
$localFallbackSuppressionV2ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2'))
$outerGroundLoadingFallbackContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\OuterGroundLoadingFallback'))
$v5cContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5C'))
$contextFacadeR25ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25'))
$contextTextureRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\Textures'))
$heroV2ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV2'))
$heroV3ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV3'))
$heroV4ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV4'))
$heroV5ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicView\HeroMaterialsV5'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$shellLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR31BroadShellEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

$sourcePins = @(
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure\TRIADIstanaExploreV5DContextPolicyActor.h'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h'
        Bytes = 10649L; Sha256 = '9114F728E337523DF3AD0FE5021685683C41BB048CB0049713C2ECC9342D962B'
        NativeBeforePresent = $true
        NativeBeforeBytes = $ExpectedContextPolicyHeaderBytes
        NativeBeforeSha256 = $ExpectedContextPolicyHeaderSha256.ToUpperInvariant()
    }
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Bytes = 134398L; Sha256 = '58074AC6449BD6FBF7D86DDB876B9CD3B7BEFF03162DAA9EFE10C0A08E0DF4C4'
        NativeBeforePresent = $true
        NativeBeforeBytes = $ExpectedContextPolicySourceBytes
        NativeBeforeSha256 = $ExpectedContextPolicySourceSha256.ToUpperInvariant()
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR31BroadShellAssetFactory.h'
        Bytes = 735L; Sha256 = 'E4DC8E285A05A96DD927D032E42FC0139FEF099A51B5F806945732B7D59FFF45'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR31BroadShellAssetFactory.cpp'
        Bytes = 147770L; Sha256 = 'F1F89219896F8710AA4A42FA3C8E19B82ADDEA107062AA600DE0F00BAF04523F'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR31BroadShellEditorLibrary.h'
        Bytes = 1544L; Sha256 = '9DD63C0D38508F750CBEBE5388586ACFD5BC94F605EF05B512018F202B877A49'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR31BroadShellEditorLibrary.cpp'
        Bytes = 34184L; Sha256 = '8F4006EB722451E1ABB988FA7ACCB6A17CE1BC5165506A6560A51FBEA7120272'
        NativeBeforePresent = $false
    }
)

$sourceAssetPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\r31_broad_shell_lookdev.contract.json'
        Bytes = 30110L
        Sha256 = '5ED127F1072A127A9ECEE51D1BD9496CE05E43E57FCB90F92A560840D6E97F27'
        NativeBeforePresent = $false
    }
)

# Exact order is sourceMesh.sourceObj + five sourcePins + nine texturePins in
# r31_broad_shell_lookdev.contract.json. The four generated V2 source records
# are absent from the admitted promoted-R30 native tree and are the only rows
# copied; the other eleven must already match their repository/contract pins.
$contractReferencedPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Generated\IstanaExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.obj'
        Bytes = 6354063L; Sha256 = '99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110'
        NativeBeforePresent = $false; Promote = $true
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Generated\IstanaExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render.mtl'
        Bytes = 2645L; Sha256 = '751DE195642892F781731EBD0F9EB3C05F23731CC674F76E7EF335E4F909342A'
        NativeBeforePresent = $false; Promote = $true
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Generated\IstanaExploreV5D\LocalFallbackSuppressionV2\IstanaPublicViewV5DLocalFallbackSuppression.v2.metadata.json'
        Bytes = 94841L; Sha256 = '31A32BCB8DAED756E0B8D90D0EE795A43B389BFB3148322A0FAC761A9BD73477'
        NativeBeforePresent = $false; Promote = $true
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Generated\IstanaExploreV5D\LocalFallbackSuppressionV2\IstanaPublicViewV5DLocalFallbackSuppression.v2.manifest.json'
        Bytes = 1763L; Sha256 = '7D455FE8C1E057F2380BE4941026F511AE5D5A1817795F238496FCEAD77EDE04'
        NativeBeforePresent = $false; Promote = $true
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\local_fallback_suppression_v2.contract.json'
        Bytes = 6952L; Sha256 = 'EAA570EC3F6DCA0B47CD4F346E73EE879E9C94AC951CE5FE456E3C4DFCFAB6A6'
        NativeBeforePresent = $true; NativeBeforeBytes = 6952L
        NativeBeforeSha256 = 'EAA570EC3F6DCA0B47CD4F346E73EE879E9C94AC951CE5FE456E3C4DFCFAB6A6'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\manifest.json'
        Bytes = 7952L; Sha256 = '2B59B328A4E5F4963D4FB3B006EBA7D12F42316B981E88B8AEEBD0718643A87A'
        NativeBeforePresent = $true; NativeBeforeBytes = 7952L
        NativeBeforeSha256 = '2B59B328A4E5F4963D4FB3B006EBA7D12F42316B981E88B8AEEBD0718643A87A'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Plaster_BaseColor.png'
        Bytes = 7890921L; Sha256 = '7C05A613C000DDCCBA5DF7D5F9A370E4F6A22721260D0CAB8F2DA151A098ADFC'
        NativeBeforePresent = $true; NativeBeforeBytes = 7890921L
        NativeBeforeSha256 = '7C05A613C000DDCCBA5DF7D5F9A370E4F6A22721260D0CAB8F2DA151A098ADFC'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Plaster_Normal.png'
        Bytes = 2500685L; Sha256 = '37EA9E604129D6180819DAC7F20D128CA062D2E4AEECF3D4868FD933C4F217F1'
        NativeBeforePresent = $true; NativeBeforeBytes = 2500685L
        NativeBeforeSha256 = '37EA9E604129D6180819DAC7F20D128CA062D2E4AEECF3D4868FD933C4F217F1'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Plaster_ORM.png'
        Bytes = 869018L; Sha256 = 'F11318DD9B8CF6B05045ADE2F8BEC80179C90AA170BE0FB4B2CD37AAEEABBB9F'
        NativeBeforePresent = $true; NativeBeforeBytes = 869018L
        NativeBeforeSha256 = 'F11318DD9B8CF6B05045ADE2F8BEC80179C90AA170BE0FB4B2CD37AAEEABBB9F'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Stone_BaseColor.png'
        Bytes = 9259452L; Sha256 = '15866B6CFCB7CC1FB2D5ED89FAFA72EED9FE87C8D531638B88B5E9F62015108A'
        NativeBeforePresent = $true; NativeBeforeBytes = 9259452L
        NativeBeforeSha256 = '15866B6CFCB7CC1FB2D5ED89FAFA72EED9FE87C8D531638B88B5E9F62015108A'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Stone_Normal.png'
        Bytes = 3945419L; Sha256 = 'FDF23DECC3699928217A0B4D8F3B4A189AB16E4791520F9B94838DD65868B101'
        NativeBeforePresent = $true; NativeBeforeBytes = 3945419L
        NativeBeforeSha256 = 'FDF23DECC3699928217A0B4D8F3B4A189AB16E4791520F9B94838DD65868B101'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Stone_ORM.png'
        Bytes = 1513197L; Sha256 = 'A54CAC5ABB3FB783D707A2D43854BA29D296F9A9CBF9B447D389B04EA426F943'
        NativeBeforePresent = $true; NativeBeforeBytes = 1513197L
        NativeBeforeSha256 = 'A54CAC5ABB3FB783D707A2D43854BA29D296F9A9CBF9B447D389B04EA426F943'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Slate_BaseColor.png'
        Bytes = 7255179L; Sha256 = '80731216A8DFEAAF7974630D4E8592492007308028D16BC2470B9A28C356B957'
        NativeBeforePresent = $true; NativeBeforeBytes = 7255179L
        NativeBeforeSha256 = '80731216A8DFEAAF7974630D4E8592492007308028D16BC2470B9A28C356B957'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Slate_Normal.png'
        Bytes = 3249242L; Sha256 = '4EED13B4F055F25910E9F02A5F7695A154CF44B475FA8877CDE5CA75F616821B'
        NativeBeforePresent = $true; NativeBeforeBytes = 3249242L
        NativeBeforeSha256 = '4EED13B4F055F25910E9F02A5F7695A154CF44B475FA8877CDE5CA75F616821B'; Promote = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicView\Textures\Generated\T_IPV_Slate_ORM.png'
        Bytes = 1053293L; Sha256 = 'B20D11D3E6B6BE5406FAAF5B0CFF3802DA4666BAE14F363A534F42D748FFA29B'
        NativeBeforePresent = $true; NativeBeforeBytes = 1053293L
        NativeBeforeSha256 = 'B20D11D3E6B6BE5406FAAF5B0CFF3802DA4666BAE14F363A534F42D748FFA29B'; Promote = $false
    }
)

$r31ContentRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31\Materials\M_IPV5D_R31_BroadShellPBR_Master.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31\Materials\MI_IPV5D_R31_OfficialWall.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31\Materials\MI_IPV5D_R31_OfficialRoof.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31\Materials\MI_IPV5D_R31_FallbackWall.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31\Materials\MI_IPV5D_R31_FallbackRoof.uasset'
)

function Get-FileState {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{
            Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState $Path
    if ($actual.Present -ne $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label state mismatch: $Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Get-RequiredReceiptProperty {
    param($Object, [string] $Name, [string] $Label)
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Label lacks required property '$Name'."
    }
    $property.Value
}

function Read-HashPinnedDirectCommitReceipt {
    param(
        [string] $Path,
        [string] $ExpectedSha256,
        [string] $DirectParent,
        [string] $Label
    )
    if ([string]::IsNullOrWhiteSpace($Path) -or
        [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        throw "$Label requires an explicit path and SHA-256."
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $directory = [IO.Path]::GetDirectoryName($fullPath)
    $parent = [IO.Path]::GetDirectoryName($directory)
    $token = [IO.Path]::GetFileName($directory)
    if (-not [IO.Path]::GetFileName($fullPath).Equals(
            'commit.json', [StringComparison]::Ordinal) -or
        -not $parent.Equals(
            [IO.Path]::GetFullPath($DirectParent),
            [StringComparison]::OrdinalIgnoreCase) -or
        $token -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "$Label must be direct safe-token child commit.json below $DirectParent : $fullPath"
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or
        $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "$Label SHA-256 mismatch: expected=$($ExpectedSha256.ToUpperInvariant()) actual=$($state.Sha256)"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Token = $token
        State = $state
        Receipt = Get-Content -LiteralPath $fullPath -Raw |
            ConvertFrom-Json -Depth 32
    }
}

function Test-ReceiptStateMatches {
    param($State, [long] $Bytes, [string] $Sha256)
    $null -ne $State -and
        [bool] (Get-RequiredReceiptProperty $State 'Present' 'receipt state') -eq $true -and
        [int64] (Get-RequiredReceiptProperty $State 'Bytes' 'receipt state') -eq $Bytes -and
        [string] (Get-RequiredReceiptProperty $State 'Sha256' 'receipt state') -ceq $Sha256.ToUpperInvariant()
}

function Assert-TreeMaterialResponseV3State {
    param($Receipt, [string] $Label)
    $nativePins = @(Get-RequiredReceiptProperty $Receipt 'NativeSourcePins' $Label)
    $closurePins = @(Get-RequiredReceiptProperty $Receipt 'SourceClosurePins' $Label)
    $contentBefore = @(Get-RequiredReceiptProperty $Receipt 'ContentBefore' $Label)
    $contentAfter = @(Get-RequiredReceiptProperty $Receipt 'ContentAfter' $Label)
    if ([string] (Get-RequiredReceiptProperty $Receipt 'Status' $Label) -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        $nativePins.Count -ne 4 -or $closurePins.Count -ne 6 -or
        $contentAfter.Count -ne $contentBefore.Count + 13 -or
        [int] (Get-RequiredReceiptProperty $Receipt 'ResponseMaterialPackageCount' $Label) -ne 13 -or
        [int] (Get-RequiredReceiptProperty $Receipt 'ReboundManagedMeshPackageCount' $Label) -ne 5 -or
        [int] (Get-RequiredReceiptProperty $Receipt 'RuntimeResponseMidCount' $Label) -ne 26 -or
        [bool] (Get-RequiredReceiptProperty $Receipt 'TreePlacementGeometryOpacityWindAuthorityModified' $Label) -ne $false) {
        throw "$Label failed the exact TreeRealism v3 closure contract."
    }
    foreach ($pin in @($nativePins) + @($closurePins)) {
        $relative = [string] (Get-RequiredReceiptProperty $pin 'RelativePath' "$Label pin")
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('..')) {
            throw "$Label has a noncanonical source pin: $relative"
        }
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "$Label source pin escaped the native project: $path"
        }
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] (Get-RequiredReceiptProperty $pin 'Bytes' "$Label pin")
            Sha256=[string] (Get-RequiredReceiptProperty $pin 'Sha256' "$Label pin")
        }) $path "$Label retained source pin")
    }
    $actualTree = @(Get-TreeReceipt $treeRealismContentRoot)
    if ((ConvertTo-Json @($contentAfter) -Depth 8 -Compress) -cne
        (ConvertTo-Json @($actualTree) -Depth 8 -Compress)) {
        throw "$Label current TreeRealism content no longer matches R30."
    }
    $Receipt
}

function Assert-R30CommitAndCaptureReceipts {
    $commitAdmission = Read-HashPinnedDirectCommitReceipt `
        -Path $R30CommitReceipt `
        -ExpectedSha256 $ExpectedR30CommitReceiptSha256 `
        -DirectParent $r30TransactionBase `
        -Label 'R30 transaction receipt'
    $commit = $commitAdmission.Receipt
    if ([string] (Get-RequiredReceiptProperty $commit 'Schema' 'R30 transaction receipt') -cne
            'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1' -or
        [string] (Get-RequiredReceiptProperty $commit 'Status' 'R30 transaction receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredReceiptProperty $commit 'RunToken' 'R30 transaction receipt') -cne $commitAdmission.Token -or
        [int] (Get-RequiredReceiptProperty $commit 'R30ContentPackageCount' 'R30 transaction receipt') -ne 12 -or
        [string] (Get-RequiredReceiptProperty $commit 'PredecessorVegetationOwner' 'R30 transaction receipt') -cne 'R29' -or
        [bool] (Get-RequiredReceiptProperty $commit 'VegetationMutationAllowed' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $commit 'R29FacadeMeshRetained' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $commit 'R28PublicRealmRetained' 'R30 transaction receipt') -ne $true -or
        [int] (Get-RequiredReceiptProperty $commit 'R29EnvironmentActiveRenderers' 'R30 transaction receipt') -ne 0 -or
        [bool] (Get-RequiredReceiptProperty $commit 'R29EnvironmentConcurrentRenderingAllowed' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $commit 'TreeRealismValidated' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $commit 'TreeMaterialResponseV3Promoted' 'R30 transaction receipt') -ne $true -or
        [int] (Get-RequiredReceiptProperty $commit 'TreeResponseMaterialPackageCount' 'R30 transaction receipt') -ne 13 -or
        [int] (Get-RequiredReceiptProperty $commit 'TreeDerivativeMeshPackageCount' 'R30 transaction receipt') -ne 5 -or
        [int] (Get-RequiredReceiptProperty $commit 'TreeRuntimeResponseMidCount' 'R30 transaction receipt') -ne 26 -or
        [bool] (Get-RequiredReceiptProperty $commit 'TreePlacementGeometryOpacityWindAuthorityModified' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $commit 'TerrainR29Validated' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $commit 'ContextPolicyShellValidatedBeforeAndAfter' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $commit 'SimulationCollisionNavigationSensorRfAuthority' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $commit 'VisualCaptureAccepted' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $commit 'CaptureRevalidationRequired' 'R30 transaction receipt') -ne $true) {
        throw 'R30 transaction receipt failed the exact committed predecessor contract.'
    }
    $treeMaterialResponseV3 = Assert-TreeMaterialResponseV3State `
        (Get-RequiredReceiptProperty $commit 'TreeMaterialResponseV3' 'R30 transaction receipt') `
        'R30 TreeRealism v3 predecessor'
    $commitMap = Get-RequiredReceiptProperty $commit 'SuccessorMap' 'R30 transaction receipt'
    $commitRuntime = Get-RequiredReceiptProperty $commit 'RuntimeDllAfter' 'R30 transaction receipt'
    $commitEditor = Get-RequiredReceiptProperty $commit 'EditorDllAfter' 'R30 transaction receipt'
    if (-not (Test-ReceiptStateMatches $commitMap $ExpectedMapBytes $ExpectedMapSha256) -or
        -not (Test-ReceiptStateMatches $commitRuntime $ExpectedRuntimeDllBytes $ExpectedRuntimeDllSha256) -or
        -not (Test-ReceiptStateMatches $commitEditor $ExpectedEditorDllBytes $ExpectedEditorDllSha256)) {
        throw 'Explicit R31 map/DLL pins do not match the admitted R30 committed successor receipt.'
    }

    $captureAdmission = Read-HashPinnedDirectCommitReceipt `
        -Path $R30CaptureCommitReceipt `
        -ExpectedSha256 $ExpectedR30CaptureCommitReceiptSha256 `
        -DirectParent $r30EvidenceBase `
        -Label 'R30 Player0 capture receipt'
    $capture = $captureAdmission.Receipt
    if ([string] (Get-RequiredReceiptProperty $capture 'Schema' 'R30 capture receipt') -cne
            'triad.istana_explore_v5d.r30_player0_capture.v1' -or
        [string] (Get-RequiredReceiptProperty $capture 'Status' 'R30 capture receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredReceiptProperty $capture 'RunToken' 'R30 capture receipt') -cne $captureAdmission.Token -or
        [string] (Get-RequiredReceiptProperty $capture 'NativeOrder' 'R30 capture receipt') -cne
            'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' -or
        [bool] (Get-RequiredReceiptProperty $capture 'R31DependencyAllowed' 'R30 capture receipt') -ne $false -or
        [int] (Get-RequiredReceiptProperty $capture 'ExactPoseCount' 'R30 capture receipt') -ne 5 -or
        [bool] (Get-RequiredReceiptProperty $capture 'MechanicalCaptureValidationPassed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'ExplicitHumanReviewAcceptance' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'ConfirmedFiveImagesReviewed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'HumanVisualReviewAttested' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'AutomaticVisualAcceptanceAllowed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $capture 'VisualReviewRequired' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $capture 'VisualReviewAccepted' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'ProviderFallbackVisualQaAccepted' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'TreeMaterialResponseV3Reviewed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'R31AdmissionAuthorized' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $capture 'ProviderReadyProofClaimed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $capture 'HyperrealismClaimed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $capture 'MapModifiedByCapture' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $capture 'SimulationCollisionNavigationSensorRfModified' 'R30 capture receipt') -ne $false) {
        throw 'R30 Player0 capture receipt failed the exact committed visual-QA contract.'
    }
    $embeddedCommit = Get-RequiredReceiptProperty $capture 'R30CommitAdmission' 'R30 capture receipt'
    $embeddedCommitFile = Get-RequiredReceiptProperty $embeddedCommit 'File' 'R30 capture commit admission'
    if (-not [IO.Path]::GetFullPath(
            [string] (Get-RequiredReceiptProperty $embeddedCommit 'Path' 'R30 capture commit admission')).Equals(
                $commitAdmission.Path, [StringComparison]::OrdinalIgnoreCase) -or
        [string] (Get-RequiredReceiptProperty $embeddedCommitFile 'Sha256' 'R30 capture commit admission file') -cne
            $commitAdmission.State.Sha256 -or
        -not (Test-ReceiptStateMatches (
            Get-RequiredReceiptProperty $embeddedCommit 'SuccessorMap' 'R30 capture commit admission') $ExpectedMapBytes $ExpectedMapSha256) -or
        -not (Test-ReceiptStateMatches (
            Get-RequiredReceiptProperty $embeddedCommit 'RuntimeDllAfter' 'R30 capture commit admission') $ExpectedRuntimeDllBytes $ExpectedRuntimeDllSha256) -or
        -not (Test-ReceiptStateMatches (
            Get-RequiredReceiptProperty $embeddedCommit 'EditorDllAfter' 'R30 capture commit admission') $ExpectedEditorDllBytes $ExpectedEditorDllSha256)) {
        throw 'R30 Player0 capture receipt is not bound to the admitted R30 transaction/map/DLL identities.'
    }
    $embeddedTreeMaterialResponseV3 = Get-RequiredReceiptProperty `
        $embeddedCommit 'TreeMaterialResponseV3' 'R30 capture commit admission'
    if ((ConvertTo-Json $treeMaterialResponseV3 -Depth 12 -Compress) -cne
        (ConvertTo-Json $embeddedTreeMaterialResponseV3 -Depth 12 -Compress)) {
        throw 'R30 Player0 capture is not bound to the exact TreeRealism v3 transaction identity.'
    }
    [pscustomobject] [ordered] @{
        Transaction = $commitAdmission
        Capture = $captureAdmission
        Map = $commitMap
        RuntimeDll = $commitRuntime
        EditorDll = $commitEditor
        TreeMaterialResponseV3 = $treeMaterialResponseV3
    }
}

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $fullPath.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-PinRelativePath {
    param(
        [object] $Pin,
        [ValidateSet('Repository', 'Native')]
        [string] $Surface
    )
    $explicitName = if ($Surface -ceq 'Repository') {
        'RepositoryRelativePath'
    }
    else {
        'NativeRelativePath'
    }
    $explicitProperty = $Pin.PSObject.Properties[$explicitName]
    if ($null -ne $explicitProperty) {
        $value = [string] $explicitProperty.Value
        if ([string]::IsNullOrWhiteSpace($value)) {
            throw "R31 pin has an empty $explicitName."
        }
        return $value
    }
    $samePathProperty = $Pin.PSObject.Properties['RelativePath']
    if ($null -eq $samePathProperty -or
        [string]::IsNullOrWhiteSpace([string] $samePathProperty.Value)) {
        throw "R31 pin lacks both $explicitName and same-path RelativePath semantics."
    }
    [string] $samePathProperty.Value
}

function Assert-CanonicalPinRelativePath {
    param([string] $RelativePath, [string] $Label)
    if ([IO.Path]::IsPathRooted($RelativePath) -or
        $RelativePath.Contains('..')) {
        throw "Non-canonical $Label path: $RelativePath"
    }
}

function Assert-Pins {
    param([object[]] $Pins, [string] $Root, [switch] $NativeAfter)
    foreach ($pin in $Pins) {
        $surface = if ($NativeAfter) { 'Native' } else { 'Repository' }
        $relativePath = Get-PinRelativePath $pin $surface
        Assert-CanonicalPinRelativePath $relativePath "$surface promotion"
        $path = [IO.Path]::GetFullPath((Join-Path $Root $relativePath))
        if (-not (Test-ContainedPath $path $Root)) {
            throw "$surface promotion path escaped root: $path"
        }
        $expected = [pscustomobject] @{
            Present = $true
            Bytes = [int64] $pin.Bytes
            Sha256 = [string] $pin.Sha256
        }
        [void] (Assert-State $expected $path 'pinned source')
    }
}

function Assert-NativePreState {
    param([object[]] $Pins)
    foreach ($pin in $Pins) {
        $relativePath = Get-PinRelativePath $pin 'Native'
        Assert-CanonicalPinRelativePath $relativePath 'native prestate'
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relativePath))
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Native prestate path escaped project root: $path"
        }
        if ($pin.NativeBeforePresent) {
            if ([int64] $pin.NativeBeforeBytes -le 0 -or
                [string] $pin.NativeBeforeSha256 -notmatch '^[A-F0-9]{64}$') {
                throw "Existing R31 overlay target lacks an explicit native-preimage receipt: $path"
            }
            $expected = [pscustomobject] @{
                Present = $true
                Bytes = [int64] $pin.NativeBeforeBytes
                Sha256 = [string] $pin.NativeBeforeSha256
            }
            [void] (Assert-State $expected $path 'explicit native source preimage')
        }
        elseif ([IO.File]::Exists($path)) {
            throw "Initially absent R31 promotion target already exists: $path"
        }
    }
}

function Get-TreeReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath = [IO.Path]::GetRelativePath($Root, $_.FullName)
                Bytes = [int64] $_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            }
        }
    )
}

function Assert-TreeReceipt {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual = @(Get-TreeReceipt $Root)
    $beforeJson = ConvertTo-Json @($Expected) -Depth 4 -Compress
    $afterJson = ConvertTo-Json @($actual) -Depth 4 -Compress
    if ($beforeJson -cne $afterJson) {
        throw "$Label immutable tree changed: $Root"
    }
}

function Assert-RequiredImmutablePrestate {
    param([Collections.IDictionary] $Receipt)
    if (@($Receipt.R30FacadeLookdev).Count -ne 12) {
        throw 'Promoted R30 facade lookdev immutable root must contain exactly 12 package files.'
    }
    foreach ($name in @(
        'R29FacadeEnvironment', 'R28Environment', 'VegetationR29',
        'LandmarkVegetationR28', 'TreeRealism', 'TerrainR29',
        'LocalFallbackSuppressionV2', 'OuterGroundLoadingFallback',
        'InheritedV5C',
        'ContextFacadeR25', 'ContextTextures', 'HeroMaterialsV2',
        'HeroMaterialsV3', 'HeroMaterialsV4', 'HeroMaterialsV5')) {
        if (@($Receipt[$name]).Count -le 0) {
            throw "Required retained immutable root is missing or empty: $name"
        }
    }
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Journal target escaped native project: $path"
        }
        $state = Get-FileState $path
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $path)
        $backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $relative))
        if ($state.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $path -Destination $backup
            [void] (Assert-State $state $backup 'journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path = $path; RelativePath = $relative; Before = $state
            Backup = if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows)) {
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
            [void] (Assert-State $row.Before $row.Path 'restored file')
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
    }
}

function New-DirectoryPresenceJournal {
    param([string[]] $FilePaths)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($filePath in @($FilePaths | Sort-Object -Unique)) {
        $directory = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($filePath))
        while ((Test-ContainedPath $directory $nativeProjectRoot) -and
            -not $directory.Equals($nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
            if ($seen.Add($directory)) {
                $rows.Add([pscustomobject] [ordered] @{
                    Path = $directory
                    BeforePresent = [IO.Directory]::Exists($directory)
                })
            }
            $directory = [IO.Path]::GetDirectoryName($directory)
        }
    }
    @($rows)
}

function Restore-DirectoryPresenceJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows | Sort-Object { ([string] $_.Path).Length } -Descending)) {
        $path = [IO.Path]::GetFullPath([string] $row.Path)
        if (-not (Test-ContainedPath $path $nativeProjectRoot) -or
            $path.Equals($nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Directory rollback target escaped or equalled native root: $path"
        }
        if ([bool] $row.BeforePresent) {
            if (-not [IO.Directory]::Exists($path)) {
                throw "Pre-existing directory disappeared during R31 rollback: $path"
            }
        }
        elseif ([IO.Directory]::Exists($path)) {
            if (@(Get-ChildItem -LiteralPath $path -Force).Count -ne 0) {
                throw "Initially absent R31 source directory is non-empty after file rollback: $path"
            }
            [IO.Directory]::Delete($path, $false)
        }
    }
}

function New-TreeJournal {
    param([string[]] $Roots, [string] $BackupRoot)
    $journals = [Collections.Generic.List[object]]::new()
    foreach ($root in $Roots) {
        $directories = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -Directory -Recurse | ForEach-Object FullName) + @($root)
        } else { @() }
        $files = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object FullName)
        } else { @() }
        $journals.Add([pscustomobject] [ordered] @{
            Root = $root
            Directories = @($directories | Sort-Object -Unique)
            Files = @(New-FileJournal $files $BackupRoot)
        })
    }
    @($journals)
}

function Restore-TreeJournal {
    param([object[]] $Journals)
    foreach ($journal in @($Journals)) {
        $beforeFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($row in @($journal.Files)) { [void] $beforeFiles.Add([string] $row.Path) }
        if ([IO.Directory]::Exists($journal.Root)) {
            foreach ($file in @(Get-ChildItem -LiteralPath $journal.Root -File -Recurse)) {
                if (-not $beforeFiles.Contains($file.FullName)) {
                    Remove-Item -LiteralPath $file.FullName -Force
                }
            }
        }
        Restore-FileJournal @($journal.Files)
        $beforeDirectories = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($directory in @($journal.Directories)) { [void] $beforeDirectories.Add([string] $directory) }
        if ([IO.Directory]::Exists($journal.Root)) {
            $currentDirectories = @(
                Get-ChildItem -LiteralPath $journal.Root -Directory -Recurse | ForEach-Object FullName
            ) + @($journal.Root)
            foreach ($directory in @($currentDirectories | Sort-Object Length -Descending)) {
                if (-not $beforeDirectories.Contains($directory) -and
                    @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
                    [IO.Directory]::Delete($directory, $false)
                }
            }
        }
    }
}

function Remove-IsolatedR31Content {
    if (-not [IO.Directory]::Exists($r31ContentRoot)) { return }
    if (-not (Test-ContainedPath $r31ContentRoot (Join-Path $nativeProjectRoot 'Content'))) {
        throw 'R31 content rollback root escaped native Content.'
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $r31ContentRoot -File -Recurse)) {
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $directories = @(Get-ChildItem -LiteralPath $r31ContentRoot -Directory -Recurse | ForEach-Object FullName) + @($r31ContentRoot)
    foreach ($directory in @($directories | Sort-Object Length -Descending)) {
        if (@(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
            [IO.Directory]::Delete($directory, $false)
        }
    }
}

function Get-ProtectedProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.ExecutablePath -and
            [IO.Path]::GetFullPath($_.ExecutablePath).Equals($protectedEditor, [StringComparison]::OrdinalIgnoreCase) -and
            $_.CommandLine -and $_.CommandLine.Contains($protectedProject, [StringComparison]::OrdinalIgnoreCase)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId = [uint32] $_.ProcessId
                CreationDate = [string] $_.CreationDate
                ExecutablePath = [IO.Path]::GetFullPath($_.ExecutablePath)
                CommandLine = [string] $_.CommandLine
            }
        }
    )
}

function Assert-ProtectedUnchanged {
    param([object[]] $Before)
    if ((ConvertTo-Json @($Before) -Compress) -cne
        (ConvertTo-Json @(Get-ProtectedProcesses) -Compress)) {
        throw 'Protected UE5.4 CAPSTONE process set changed.'
    }
}

function Get-NativeMutatorProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $isEngineEditor = $_.ExecutablePath -and
                [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith(
                    $engineRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
                $_.Name -like 'UnrealEditor*'
            $isExactProjectCommand = $_.CommandLine -and
                ($_.CommandLine.Contains(
                    $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -or
                 $_.CommandLine.Contains(
                    $nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase))
            $isBuildTool = $_.Name -in @(
                'dotnet.exe', 'UnrealBuildTool.exe',
                'UnrealHeaderTool.exe', 'MSBuild.exe',
                'cl.exe', 'link.exe', 'rc.exe',
                'ShaderCompileWorker.exe')
            $isExactProjectUbt = $isExactProjectCommand -and
                $_.CommandLine.Contains(
                    'UnrealBuildTool', [StringComparison]::OrdinalIgnoreCase)
            $isEngineEditor -or $isExactProjectUbt -or
                ($isExactProjectCommand -and $isBuildTool)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId = [uint32] $_.ProcessId
                ParentProcessId = [uint32] $_.ParentProcessId
                CreationDate = [string] $_.CreationDate
                Name = [string] $_.Name
                ExecutablePath = if ($_.ExecutablePath) {
                    [IO.Path]::GetFullPath($_.ExecutablePath)
                } else { '' }
                CommandLine = [string] $_.CommandLine
            }
        }
    )
}

function Register-OwnedNativeProcessHandle {
    param(
        [Parameter(Mandatory = $true)]
        [Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)]
        [string] $Label,
        [Parameter(Mandatory = $true)]
        [string] $ExpectedExecutablePath
    )

    $Handle.Refresh()
    if ($Handle.HasExited) {
        throw "Owned native process exited before registration: $Label"
    }
    $script:ownedNativeProcessHandles.Add(
        [pscustomobject] [ordered] @{
            Handle = $Handle
            ProcessId = [int] $Handle.Id
            StartUtcTicks = [int64] $Handle.StartTime.ToUniversalTime().Ticks
            ExpectedExecutablePath = [IO.Path]::GetFullPath(
                $ExpectedExecutablePath)
            Label = $Label
        })
}

function Get-ActiveOwnedNativeProcessHandles {
    @(
        foreach ($record in $script:ownedNativeProcessHandles) {
            $record.Handle.Refresh()
            if (-not $record.Handle.HasExited) { $record }
        }
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $busy = @(Get-NativeMutatorProcesses)
    if ($busy.Count -ne 0) {
        $summary = [string]::Join(
            '; ',
            @($busy | ForEach-Object {
                "pid=$($_.ProcessId) name=$($_.Name)"
            }))
        throw "$Label refused: UE5.5 editor/helper or exact-project build mutator is already present: $summary"
    }
}

function Wait-NativeMutationQuiescence {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Label,
        [ValidateRange(5, 120)]
        [int] $TimeoutSeconds = 60
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $busy = @(Get-NativeMutatorProcesses)
        $activeOwned = @(Get-ActiveOwnedNativeProcessHandles)
        if ($busy.Count -eq 0 -and $activeOwned.Count -eq 0) {
            return [pscustomobject] [ordered] @{
                Label = $Label
                Status = 'QUIESCENT'
                RegisteredOwnedProcessCount =
                    $script:ownedNativeProcessHandles.Count
                ActiveOwnedProcessCount = 0
                NativeMutatorProcessCount = 0
            }
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)

    $busy = @(Get-NativeMutatorProcesses)
    $activeOwned = @(Get-ActiveOwnedNativeProcessHandles)
    $summary = [string]::Join(
        '; ',
        @($busy | ForEach-Object {
            "mutatorPid=$($_.ProcessId) name=$($_.Name)"
        }) +
        @($activeOwned | ForEach-Object {
            "ownedPid=$($_.ProcessId) label=$($_.Label)"
        }))
    throw "$Label refused filesystem mutation because owned build/editor process trees are not quiescent: $summary"
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $freeBytes = [int64] $os.FreeVirtualMemory * 1024L
    if ($freeBytes -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label refused by fixed 10 GiB FreeVirtualMemory gate: available=$freeBytes"
    }
    [pscustomobject] [ordered] @{
        Label = $Label
        MinimumBytes = $minimumSystemFreeVirtualAtLaunchBytes
        ObservedBytes = $freeBytes
        Status = 'PASS'
    }
}

function Invoke-RcCall {
    param(
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters,
        [int] $TimeoutSeconds
    )
    $body = [ordered] @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
        generateTransaction = $false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' -Body $body -TimeoutSec $TimeoutSeconds
}

function Get-OwnedHelperIdentity {
    param([Diagnostics.Process] $Handle, [string] $Log)
    $record = Get-CimInstance Win32_Process -Filter "ProcessId=$($Handle.Id)" -ErrorAction Stop
    if ($null -eq $record -or -not $record.ExecutablePath -or
        -not [IO.Path]::GetFullPath($record.ExecutablePath).Equals($editor, [StringComparison]::OrdinalIgnoreCase) -or
        -not $record.CommandLine.Contains($mapPackage, [StringComparison]::Ordinal) -or
        -not $record.CommandLine.Contains($Log, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Could not prove exact launched UE5.5 helper identity.'
    }
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $record.ProcessId
        CreationDate = [string] $record.CreationDate
        ExecutablePath = [IO.Path]::GetFullPath($record.ExecutablePath)
        CommandLine = [string] $record.CommandLine
    }
}

function Stop-OwnedHelper {
    param(
        [Diagnostics.Process] $Handle,
        $Identity,
        [string] $Log
    )
    if ([uint32] $Handle.Id -ne [uint32] $Identity.ProcessId) {
        throw 'Process handle does not match exact owned helper.'
    }
    try { [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) } catch {}
    if (-not $Handle.WaitForExit($ShutdownTimeoutSeconds * 1000)) {
        $currentIdentity = Get-OwnedHelperIdentity $Handle $Log
        if ([uint32] $currentIdentity.ProcessId -ne
                [uint32] $Identity.ProcessId -or
            [string] $currentIdentity.CreationDate -cne
                [string] $Identity.CreationDate -or
            [string] $currentIdentity.ExecutablePath -cne
                [string] $Identity.ExecutablePath -or
            [string] $currentIdentity.CommandLine -cne
                [string] $Identity.CommandLine) {
            throw 'Timed-out helper no longer matches the exact owned process identity.'
        }
        # Forced containment is limited to the still-proven handle returned by
        # this wrapper and its descendants. Kill(true) is required so spawned
        # ShaderCompileWorker processes cannot survive into rollback.
        $Handle.Kill($true)
        if (-not $Handle.WaitForExit(30000)) {
            throw 'Exact owned helper process tree did not exit after bounded containment.'
        }
    }
}

function Initialize-ContinuousMemoryWatchdogType {
    if ($null -ne ('Triad.CesiumDiagnostics.ContinuousMemoryWatchdog' -as
            [type])) {
        return
    }

    # This monitor runs on its own CLR thread. That distinction is deliberate:
    # the main PowerShell runspace can be blocked in process discovery, CIM, or
    # an RC request while Unreal is still allocating. The watchdog therefore
    # covers the entire interval from immediately after Start-Process through
    # final teardown, rather than relying on cooperative sampling alone.
    $source = @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace Triad.CesiumDiagnostics
{
    public sealed class ContinuousMemoryWatchdogSnapshot
    {
        public bool Started { get; internal set; }
        public bool Stopped { get; internal set; }
        public long SampleCount { get; internal set; }
        public long PeakOwnedProcessTreePrivateBytes { get; internal set; }
        public ulong MinimumAvailableCommitBytes { get; internal set; }
        public string LastSampleUtc { get; internal set; }
        public string AlertKind { get; internal set; }
        public string AlertObservedUtc { get; internal set; }
        public long AlertOwnedProcessTreePrivateBytes { get; internal set; }
        public ulong AlertAvailableCommitBytes { get; internal set; }
        public bool ExactIdentityVerifiedAtAlert { get; internal set; }
        public bool ExactIdentityVerifiedAtContainment { get; internal set; }
        public bool ForceKillUsed { get; internal set; }
        public bool ContainmentRefused { get; internal set; }
        public string MonitorError { get; internal set; }
    }

    public sealed class ContinuousMemoryWatchdog : IDisposable
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        private sealed class MemoryStatusEx
        {
            public uint Length = (uint)Marshal.SizeOf(typeof(MemoryStatusEx));
            public uint MemoryLoad;
            public ulong TotalPhysical;
            public ulong AvailablePhysical;
            public ulong TotalPageFile;
            public ulong AvailablePageFile;
            public ulong TotalVirtual;
            public ulong AvailableVirtual;
            public ulong AvailableExtendedVirtual;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern bool GlobalMemoryStatusEx(
            [In, Out] MemoryStatusEx buffer);

        private const uint TH32CS_SNAPPROCESS = 0x00000002;
        private static readonly IntPtr InvalidHandleValue = new IntPtr(-1);

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct ProcessEntry32
        {
            public uint Size;
            public uint Usage;
            public uint ProcessId;
            public IntPtr DefaultHeapId;
            public uint ModuleId;
            public uint ThreadCount;
            public uint ParentProcessId;
            public int BasePriority;
            public uint Flags;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string ExecutableName;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr CreateToolhelp32Snapshot(
            uint flags, uint processId);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool Process32FirstW(
            IntPtr snapshot, ref ProcessEntry32 entry);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool Process32NextW(
            IntPtr snapshot, ref ProcessEntry32 entry);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        private readonly object gate = new object();
        private readonly int processId;
        private readonly long creationUtcTicks;
        private readonly string executablePath;
        private readonly long privateCeilingBytes;
        private readonly ulong minimumAvailableCommitBytes;
        private readonly int pollMilliseconds;
        private readonly int persistentBreachMilliseconds;
        private readonly ManualResetEvent stopEvent = new ManualResetEvent(false);
        private Thread thread;
        private bool started;
        private bool stopped;
        private long sampleCount;
        private long peakOwnedProcessTreePrivateBytes;
        private ulong minimumObservedAvailableCommitBytes = ulong.MaxValue;
        private string lastSampleUtc = String.Empty;
        private string alertKind = String.Empty;
        private string alertObservedUtc = String.Empty;
        private long alertOwnedProcessTreePrivateBytes;
        private ulong alertAvailableCommitBytes;
        private bool exactIdentityVerifiedAtAlert;
        private bool exactIdentityVerifiedAtContainment;
        private bool forceKillUsed;
        private bool containmentRefused;
        private string monitorError = String.Empty;
        private DateTime? continuousBreachStartedUtc;

        public ContinuousMemoryWatchdog(
            int processId,
            long creationUtcTicks,
            string executablePath,
            long privateCeilingBytes,
            long minimumAvailableCommitBytes,
            int pollMilliseconds,
            int persistentBreachMilliseconds)
        {
            this.processId = processId;
            this.creationUtcTicks = creationUtcTicks;
            this.executablePath = Path.GetFullPath(executablePath);
            this.privateCeilingBytes = privateCeilingBytes;
            this.minimumAvailableCommitBytes =
                checked((ulong)minimumAvailableCommitBytes);
            this.pollMilliseconds = pollMilliseconds;
            this.persistentBreachMilliseconds = persistentBreachMilliseconds;
        }

        public void Start()
        {
            lock (this.gate)
            {
                if (this.started)
                    throw new InvalidOperationException("Watchdog already started.");
                this.started = true;
                this.thread = new Thread(this.Run);
                this.thread.IsBackground = true;
                this.thread.Name = "TRIAD owned-process memory safety watchdog";
                this.thread.Start();
            }
        }

        private bool TryGetExactProcess(
            out Process process,
            out bool processExited,
            out string failure)
        {
            process = null;
            processExited = false;
            failure = String.Empty;
            Process candidate = null;
            try
            {
                candidate = Process.GetProcessById(this.processId);
                candidate.Refresh();
                if (candidate.HasExited)
                {
                    processExited = true;
                    return false;
                }
                long actualTicks = candidate.StartTime.ToUniversalTime().Ticks;
                ProcessModule mainModule = candidate.MainModule;
                if (mainModule == null ||
                    String.IsNullOrWhiteSpace(mainModule.FileName))
                {
                    failure = "Exact owned-process executable lookup returned no path.";
                    return false;
                }
                string actualPath = Path.GetFullPath(mainModule.FileName);
                if (actualTicks != this.creationUtcTicks ||
                    !String.Equals(actualPath, this.executablePath,
                        StringComparison.OrdinalIgnoreCase))
                {
                    failure = "Exact owned-process identity changed while monitored.";
                    return false;
                }
                process = candidate;
                candidate = null;
                return true;
            }
            catch (ArgumentException)
            {
                processExited = true;
                return false;
            }
            catch (InvalidOperationException ex)
            {
                failure = "Exact owned-process lookup failed: " +
                    ex.GetType().Name;
                return false;
            }
            catch (System.ComponentModel.Win32Exception ex)
            {
                failure = "Exact owned-process lookup failed: " +
                    ex.GetType().Name;
                return false;
            }
            catch (NotSupportedException ex)
            {
                failure = "Exact owned-process lookup failed: " +
                    ex.GetType().Name;
                return false;
            }
            finally
            {
                if (candidate != null)
                    candidate.Dispose();
            }
        }

        private static bool IsInOwnedProcessTree(
            int candidateProcessId,
            int rootProcessId,
            Dictionary<int, int> parentByProcessId)
        {
            int current = candidateProcessId;
            HashSet<int> visited = new HashSet<int>();
            while (current > 0 && visited.Add(current))
            {
                if (current == rootProcessId)
                    return true;
                int parent;
                if (!parentByProcessId.TryGetValue(current, out parent))
                    return false;
                current = parent;
            }
            return false;
        }

        private bool TryGetOwnedProcessTreePrivateBytes(
            Process exactRoot,
            out long privateBytes,
            out string failure)
        {
            privateBytes = 0L;
            failure = String.Empty;
            Dictionary<int, int> parentByProcessId =
                new Dictionary<int, int>();
            IntPtr snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0U);
            if (snapshot == InvalidHandleValue)
            {
                failure = "Owned process-tree snapshot failed with Win32 error " +
                    Marshal.GetLastWin32Error().ToString();
                return false;
            }
            try
            {
                ProcessEntry32 entry = new ProcessEntry32();
                entry.Size = (uint)Marshal.SizeOf(typeof(ProcessEntry32));
                if (!Process32FirstW(snapshot, ref entry))
                {
                    failure = "Owned process-tree enumeration failed with Win32 error " +
                        Marshal.GetLastWin32Error().ToString();
                    return false;
                }
                do
                {
                    parentByProcessId[(int)entry.ProcessId] =
                        (int)entry.ParentProcessId;
                    entry.Size = (uint)Marshal.SizeOf(typeof(ProcessEntry32));
                }
                while (Process32NextW(snapshot, ref entry));
            }
            finally
            {
                CloseHandle(snapshot);
            }
            if (!parentByProcessId.ContainsKey(this.processId))
            {
                failure = "Exact owned root was absent from the process-tree snapshot.";
                return false;
            }

            try
            {
                foreach (int candidateId in parentByProcessId.Keys)
                {
                    if (!IsInOwnedProcessTree(
                            candidateId, this.processId, parentByProcessId))
                        continue;
                    Process candidate = null;
                    try
                    {
                        candidate = candidateId == this.processId
                            ? exactRoot
                            : Process.GetProcessById(candidateId);
                        candidate.Refresh();
                        if (candidate.HasExited)
                            continue;
                        long startTicks =
                            candidate.StartTime.ToUniversalTime().Ticks;
                        if (startTicks < this.creationUtcTicks ||
                            (candidateId == this.processId &&
                             startTicks != this.creationUtcTicks))
                        {
                            failure = "Owned process-tree member identity predates or mismatches the exact root.";
                            return false;
                        }
                        privateBytes = checked(
                            privateBytes + candidate.PrivateMemorySize64);
                    }
                    catch (ArgumentException)
                    {
                        if (candidateId == this.processId)
                        {
                            failure = "Exact owned root exited during process-tree sampling.";
                            return false;
                        }
                    }
                    catch (InvalidOperationException)
                    {
                        if (candidateId == this.processId)
                        {
                            failure = "Exact owned root became unavailable during process-tree sampling.";
                            return false;
                        }
                    }
                    catch (System.ComponentModel.Win32Exception ex)
                    {
                        failure = "Owned process-tree member lookup failed: " +
                            ex.GetType().Name;
                        return false;
                    }
                    finally
                    {
                        if (candidate != null &&
                            candidateId != this.processId)
                            candidate.Dispose();
                    }
                }
            }
            catch (OverflowException)
            {
                failure = "Owned process-tree private-memory sum overflowed.";
                return false;
            }
            return true;
        }

        private void SetMonitorError(string value)
        {
            lock (this.gate)
            {
                if (String.IsNullOrEmpty(this.monitorError))
                    this.monitorError = value;
            }
        }

        private void Sample()
        {
            Process process;
            bool processExited;
            string failure;
            if (!this.TryGetExactProcess(
                    out process, out processExited, out failure))
            {
                if (!processExited)
                {
                    this.SetMonitorError(String.IsNullOrWhiteSpace(failure)
                        ? "Exact owned-process lookup failed without a reason."
                        : failure);
                }
                return;
            }
            using (process)
            {
                MemoryStatusEx memory = new MemoryStatusEx();
                if (!GlobalMemoryStatusEx(memory))
                {
                    this.SetMonitorError(
                        "GlobalMemoryStatusEx failed with Win32 error " +
                        Marshal.GetLastWin32Error().ToString());
                    return;
                }
                process.Refresh();
                long ownedProcessTreePrivateBytes;
                string treeFailure;
                if (!this.TryGetOwnedProcessTreePrivateBytes(
                        process, out ownedProcessTreePrivateBytes,
                        out treeFailure))
                {
                    this.SetMonitorError(String.IsNullOrWhiteSpace(treeFailure)
                        ? "Owned process-tree private-memory sampling failed without a reason."
                        : treeFailure);
                    return;
                }
                ulong availableCommitBytes = memory.AvailablePageFile;
                DateTime now = DateTime.UtcNow;
                string kind =
                    ownedProcessTreePrivateBytes >= this.privateCeilingBytes
                    ? "MEMORY_GUARD_OWNED_PROCESS_TREE_PRIVATE_BYTES"
                    : availableCommitBytes < this.minimumAvailableCommitBytes
                        ? "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL"
                        : String.Empty;

                lock (this.gate)
                {
                    ++this.sampleCount;
                    if (ownedProcessTreePrivateBytes >
                        this.peakOwnedProcessTreePrivateBytes)
                        this.peakOwnedProcessTreePrivateBytes =
                            ownedProcessTreePrivateBytes;
                    if (availableCommitBytes <
                        this.minimumObservedAvailableCommitBytes)
                        this.minimumObservedAvailableCommitBytes =
                            availableCommitBytes;
                    this.lastSampleUtc = now.ToString("o");
                    if (!String.IsNullOrEmpty(kind))
                    {
                        if (String.IsNullOrEmpty(this.alertKind))
                        {
                            this.alertKind = kind;
                            this.alertObservedUtc = now.ToString("o");
                            this.alertOwnedProcessTreePrivateBytes =
                                ownedProcessTreePrivateBytes;
                            this.alertAvailableCommitBytes =
                                availableCommitBytes;
                            this.exactIdentityVerifiedAtAlert = true;
                        }
                        if (!this.continuousBreachStartedUtc.HasValue)
                            this.continuousBreachStartedUtc = now;
                    }
                    else
                    {
                        this.continuousBreachStartedUtc = null;
                    }
                }

                DateTime? breachStarted;
                lock (this.gate)
                    breachStarted = this.continuousBreachStartedUtc;
                if (breachStarted.HasValue &&
                    (now - breachStarted.Value).TotalMilliseconds >=
                        this.persistentBreachMilliseconds)
                {
                    this.ContainExactProcess();
                }
            }
        }

        private void ContainExactProcess()
        {
            Process process;
            bool processExited;
            string failure;
            if (!this.TryGetExactProcess(
                    out process, out processExited, out failure))
            {
                lock (this.gate)
                    this.containmentRefused = true;
                if (!processExited)
                {
                    this.SetMonitorError(String.IsNullOrWhiteSpace(failure)
                        ? "Exact owned-process containment lookup failed without a reason."
                        : failure);
                }
                return;
            }
            using (process)
            {
                lock (this.gate)
                    this.exactIdentityVerifiedAtContainment = true;
                try
                {
                    process.Kill(true);
                    lock (this.gate)
                        this.forceKillUsed = true;
                }
                catch (Exception ex)
                {
                    this.SetMonitorError(
                        "Exact-process containment failed: " +
                        ex.GetType().Name);
                }
            }
        }

        private void Run()
        {
            try
            {
                while (!this.stopEvent.WaitOne(0))
                {
                    this.Sample();
                    if (this.stopEvent.WaitOne(this.pollMilliseconds))
                        break;
                }
            }
            catch (Exception ex)
            {
                this.SetMonitorError(
                    "Watchdog thread failed: " + ex.GetType().Name);
            }
            finally
            {
                lock (this.gate)
                    this.stopped = true;
            }
        }

        public ContinuousMemoryWatchdogSnapshot GetSnapshot()
        {
            lock (this.gate)
            {
                return new ContinuousMemoryWatchdogSnapshot
                {
                    Started = this.started,
                    Stopped = this.stopped,
                    SampleCount = this.sampleCount,
                    PeakOwnedProcessTreePrivateBytes =
                        this.peakOwnedProcessTreePrivateBytes,
                    MinimumAvailableCommitBytes =
                        this.minimumObservedAvailableCommitBytes == ulong.MaxValue
                            ? 0UL
                            : this.minimumObservedAvailableCommitBytes,
                    LastSampleUtc = this.lastSampleUtc,
                    AlertKind = this.alertKind,
                    AlertObservedUtc = this.alertObservedUtc,
                    AlertOwnedProcessTreePrivateBytes =
                        this.alertOwnedProcessTreePrivateBytes,
                    AlertAvailableCommitBytes =
                        this.alertAvailableCommitBytes,
                    ExactIdentityVerifiedAtAlert =
                        this.exactIdentityVerifiedAtAlert,
                    ExactIdentityVerifiedAtContainment =
                        this.exactIdentityVerifiedAtContainment,
                    ForceKillUsed = this.forceKillUsed,
                    ContainmentRefused = this.containmentRefused,
                    MonitorError = this.monitorError
                };
            }
        }

        public void Stop()
        {
            this.stopEvent.Set();
            Thread local;
            lock (this.gate)
                local = this.thread;
            if (local != null && local != Thread.CurrentThread)
                local.Join(5000);
        }

        public void Dispose()
        {
            this.Stop();
            this.stopEvent.Dispose();
        }
    }
}
'@
    Add-Type -TypeDefinition $source -Language CSharp -ErrorAction Stop
}

function Start-ContinuousMemoryWatchdog {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)]
        [string] $ExpectedExecutablePath,
        [Parameter(Mandatory = $true)]
        [string] $OwnedProcessLabel
    )

    Initialize-ContinuousMemoryWatchdogType
    $Handle.Refresh()
    if ($Handle.HasExited) {
        throw "Owned $OwnedProcessLabel exited before watchdog startup."
    }
    $watchdog =
        [Triad.CesiumDiagnostics.ContinuousMemoryWatchdog]::new(
            [int] $Handle.Id,
            [int64] $Handle.StartTime.ToUniversalTime().Ticks,
            [IO.Path]::GetFullPath($ExpectedExecutablePath),
            [int64] $privateMemoryCeilingBytes,
            [int64] $minimumSystemFreeVirtualBytes,
            [int] $memoryWatchdogPollMilliseconds,
            [int] $memoryWatchdogPersistentBreachMilliseconds)
    $watchdog.Start()
    $sampleDeadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $snapshot = $watchdog.GetSnapshot()
        if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) {
            $watchdog.Stop()
            $watchdog.Dispose()
            throw "MEMORY_WATCHDOG_FAILURE: process=$OwnedProcessLabel detail=$($snapshot.MonitorError)"
        }
        if ([int64] $snapshot.SampleCount -gt 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $sampleDeadline)
    if ([int64] $snapshot.SampleCount -le 0 -or
        [string]::IsNullOrWhiteSpace($snapshot.LastSampleUtc)) {
        $watchdog.Stop()
        $watchdog.Dispose()
        throw "MEMORY_WATCHDOG_NO_SAMPLES: process=$OwnedProcessLabel"
    }
    $watchdog
}

function Get-ContinuousMemoryWatchdogSnapshot {
    if ($null -eq $script:memoryWatchdog) {
        return $null
    }
    $script:memoryWatchdog.GetSnapshot()
}

function Test-ContinuousMemoryWatchdogMemoryAlert {
    param([AllowNull()] $Snapshot)

    $null -ne $Snapshot -and
        -not [string]::IsNullOrWhiteSpace($Snapshot.AlertKind) -and
        $Snapshot.AlertKind.StartsWith(
            'MEMORY_GUARD_', [StringComparison]::Ordinal)
}

function Assert-ContinuousMemoryWatchdogHealthy {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    $snapshot = Get-ContinuousMemoryWatchdogSnapshot
    if ($null -eq $snapshot) {
        throw "MEMORY_WATCHDOG_MISSING: checkpoint=$Checkpoint"
    }
    if (-not $snapshot.Started -or [int64] $snapshot.SampleCount -le 0 -or
        [string]::IsNullOrWhiteSpace($snapshot.LastSampleUtc)) {
        throw "MEMORY_WATCHDOG_NO_SAMPLES: checkpoint=$Checkpoint sampleCount=$($snapshot.SampleCount)"
    }
    $lastSampleUtc = [DateTime]::MinValue
    if (-not [DateTime]::TryParse(
            [string] $snapshot.LastSampleUtc,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind,
            [ref] $lastSampleUtc) -or
        ([DateTime]::UtcNow - $lastSampleUtc.ToUniversalTime()).TotalMilliseconds -gt
            [Math]::Max(5000, $memoryWatchdogPollMilliseconds * 6)) {
        throw "MEMORY_WATCHDOG_STALE_SAMPLE: checkpoint=$Checkpoint lastSampleUtc=$($snapshot.LastSampleUtc)"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) {
        throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($snapshot.MonitorError)"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.AlertKind)) {
        throw "$($snapshot.AlertKind): checkpoint=$Checkpoint source=continuous_watchdog ownedProcessTreePrivateBytes=$($snapshot.AlertOwnedProcessTreePrivateBytes) freeVirtualBytes=$($snapshot.AlertAvailableCommitBytes)"
    }
}

function Assert-ContinuousMemoryWatchdogSnapshotComplete {
    param(
        [Parameter(Mandatory = $true)] $Snapshot,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    if (-not $Snapshot.Started -or [int64] $Snapshot.SampleCount -le 0 -or
        [string]::IsNullOrWhiteSpace($Snapshot.LastSampleUtc)) {
        throw "MEMORY_WATCHDOG_NO_SAMPLES: checkpoint=$Checkpoint sampleCount=$($Snapshot.SampleCount)"
    }
    $lastSampleUtc = [DateTime]::MinValue
    if (-not [DateTime]::TryParse(
            [string] $Snapshot.LastSampleUtc,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind,
            [ref] $lastSampleUtc) -or
        ([DateTime]::UtcNow - $lastSampleUtc.ToUniversalTime()).TotalMilliseconds -gt
            [Math]::Max(5000, $memoryWatchdogPollMilliseconds * 6)) {
        throw "MEMORY_WATCHDOG_STALE_SAMPLE: checkpoint=$Checkpoint lastSampleUtc=$($Snapshot.LastSampleUtc)"
    }
    if (-not [string]::IsNullOrWhiteSpace($Snapshot.MonitorError)) {
        throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($Snapshot.MonitorError)"
    }
    if (-not [string]::IsNullOrWhiteSpace($Snapshot.AlertKind)) {
        throw "$($Snapshot.AlertKind): checkpoint=$Checkpoint source=continuous_watchdog ownedProcessTreePrivateBytes=$($Snapshot.AlertOwnedProcessTreePrivateBytes) freeVirtualBytes=$($Snapshot.AlertAvailableCommitBytes)"
    }
}

function Stop-ContinuousMemoryWatchdog {
    if ($null -eq $script:memoryWatchdog) {
        return $null
    }
    $script:memoryWatchdog.Stop()
    $snapshot = $script:memoryWatchdog.GetSnapshot()
    $script:memoryWatchdog.Dispose()
    $script:memoryWatchdog = $null
    $snapshot
}

function Invoke-GuardedOwnedBuild {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments,
        [Parameter(Mandatory = $true)]
        [string] $StandardOutputLog,
        [Parameter(Mandatory = $true)]
        [string] $StandardErrorLog
    )

    Assert-NativeIdle 'before guarded owned UBT build'
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission 'before guarded owned UBT build')
    foreach ($logPath in @($StandardOutputLog, $StandardErrorLog)) {
        if ([IO.File]::Exists($logPath)) {
            throw "Guarded owned UBT build log already exists: $logPath"
        }
    }
    $escapedArguments = @(
        ('"{0}"' -f $unrealBuildTool)
        $Arguments | ForEach-Object {
            if ($_ -match '[\s"]') {
                '"{0}"' -f $_.Replace('"', '\"')
            }
            else { $_ }
        }
    )
    $argumentLine = [string]::Join(' ', $escapedArguments)
    $handle = Start-Process `
        -FilePath $dotnet `
        -ArgumentList $argumentLine `
        -WorkingDirectory $engineSourceRoot `
        -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog `
        -PassThru `
        -WindowStyle Hidden
    Register-OwnedNativeProcessHandle `
        -Handle $handle `
        -Label 'UnrealBuildTool dotnet host' `
        -ExpectedExecutablePath $dotnet
    $ownedStartTicks = [int64] $handle.StartTime.ToUniversalTime().Ticks
    $buildError = $null
    $memorySnapshot = $null
    $exitCode = $null
    try {
        $script:memoryWatchdog = Start-ContinuousMemoryWatchdog `
            -Handle $handle `
            -ExpectedExecutablePath $dotnet `
            -OwnedProcessLabel 'UnrealBuildTool dotnet host'
        do {
            Assert-ContinuousMemoryWatchdogHealthy `
                -Checkpoint 'guarded owned UBT build'
        } while (-not $handle.WaitForExit($memoryWatchdogPollMilliseconds))
        Assert-ContinuousMemoryWatchdogHealthy `
            -Checkpoint 'guarded owned UBT build process exit'
        $exitCode = $handle.ExitCode
        if ($exitCode -ne 0) {
            throw "R31 broad shell build failed: exit=$exitCode"
        }
    }
    catch {
        $buildError = $_.Exception
    }
    finally {
        if (-not $handle.HasExited) {
            try {
                $handle.Refresh()
                $actualExecutable = [IO.Path]::GetFullPath(
                    $handle.MainModule.FileName)
                if ([int64] $handle.StartTime.ToUniversalTime().Ticks -ne
                        $ownedStartTicks -or
                    -not $actualExecutable.Equals(
                        $dotnet, [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Owned UBT containment refused because exact identity could not be re-proven.'
                }
                $handle.Kill($true)
                if (-not $handle.WaitForExit(30000)) {
                    throw 'Exact owned UBT process tree did not exit after bounded containment.'
                }
            }
            catch {
                if ($null -eq $buildError) { $buildError = $_.Exception }
                else {
                    $buildError = [InvalidOperationException]::new(
                        "build={$($buildError.Message)} containment={$($_.Exception.Message)}")
                }
            }
        }
        try {
            $memorySnapshot = Stop-ContinuousMemoryWatchdog
            if ($null -eq $memorySnapshot) {
                throw 'MEMORY_WATCHDOG_MISSING: checkpoint=guarded owned UBT final snapshot'
            }
            Assert-ContinuousMemoryWatchdogSnapshotComplete `
                -Snapshot $memorySnapshot `
                -Checkpoint 'guarded owned UBT final snapshot'
        }
        catch {
            if ($null -eq $buildError) { $buildError = $_.Exception }
            else {
                $buildError = [InvalidOperationException]::new(
                    "build={$($buildError.Message)} memoryGuard={$($_.Exception.Message)}")
            }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'after guarded owned UBT build'
    if ($null -ne $buildError) { throw $buildError }
    if (-not [IO.File]::Exists($StandardOutputLog) -or
        (Get-Item -LiteralPath $StandardOutputLog).Length -le 0) {
        throw 'Guarded owned UBT build did not persist a non-empty stdout log.'
    }
    [pscustomobject] [ordered] @{
        ProcessId = [int] $handle.Id
        ExecutablePath = $dotnet
        UnrealBuildTool = $unrealBuildTool
        ExitCode = [int] $exitCode
        ContinuousMemoryGuard = $memorySnapshot
        StandardOutputLog = Get-FileState $StandardOutputLog
        StandardErrorLog = Get-FileState $StandardErrorLog
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix,
        [hashtable] $Parameters = @{}
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission "before helper $Stage")
    $logRoot = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logRoot)
    $log = [IO.Path]::GetFullPath((Join-Path $logRoot "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine = "`"$nativeProjectFile`" $mapPackage -d3d12 -sm6 " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    Register-OwnedNativeProcessHandle `
        -Handle $handle `
        -Label "UE5.5 helper $Stage" `
        -ExpectedExecutablePath $editor
    $identity = $null
    $response = $null
    $stageError = $null
    $memorySnapshot = $null
    try {
        $script:memoryWatchdog = Start-ContinuousMemoryWatchdog `
            -Handle $handle `
            -ExpectedExecutablePath $editor `
            -OwnedProcessLabel "UE5.5 helper $Stage"
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage watchdog startup"
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage identity wait"
            try { $identity = Get-OwnedHelperIdentity $handle $log } catch { $identity = $null }
            if ($null -ne $identity) { break }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }
        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        do {
            Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage RC readiness wait"
            try {
                $ready = Invoke-RcCall $identityLibrary 'ValidateIstanaExploreRemoteControlProject' @{ ExpectedProjectPath = $nativeProjectRoot } 30
            } catch { $ready = $null }
            if ($null -ne $ready -and $ready.ReturnValue -eq $true) { break }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $ready -or $ready.ReturnValue -ne $true) {
            throw "Remote-control identity did not become ready for $Stage"
        }
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage before endpoint"
        [void] (Get-OwnedHelperIdentity $handle $log)
        $response = Invoke-RcCall $shellLibrary $FunctionName $Parameters $EditorTimeoutSeconds
        Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage after endpoint"
        $text = [string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or
            -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: response=$text"
        }
    } catch { $stageError = $_.Exception }
    finally {
        if ($null -eq $identity -and -not $handle.HasExited) {
            try { $identity = Get-OwnedHelperIdentity $handle $log }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} identityCleanup={$($_.Exception.Message)}") }
            }
        }
        if ($null -ne $identity -and -not $handle.HasExited) {
            try {
                Assert-ContinuousMemoryWatchdogHealthy -Checkpoint "$Stage before helper teardown"
                Stop-OwnedHelper $handle $identity $log
            }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} cleanup={$($_.Exception.Message)}") }
            }
        }
        try {
            $memorySnapshot = Stop-ContinuousMemoryWatchdog
            if ($null -eq $memorySnapshot) {
                throw "MEMORY_WATCHDOG_MISSING: checkpoint=$Stage final snapshot"
            }
            Assert-ContinuousMemoryWatchdogSnapshotComplete `
                -Snapshot $memorySnapshot `
                -Checkpoint "$Stage final snapshot"
        } catch {
            if ($null -eq $stageError) { $stageError = $_.Exception }
            else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} memoryGuard={$($_.Exception.Message)}") }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle "after $Stage"
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item -LiteralPath $log).Length -le 0) {
        throw "Stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        Function = $FunctionName
        ExpectedPrefix = $ExpectedPrefix
        Message = [string] $response.$TextProperty
        ProcessIdentity = $identity
        ContinuousMemoryGuard = $memorySnapshot
        Log = Get-FileState $log
    }
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 6 -or $sourceAssetPins.Count -ne 1 -or
        $contractReferencedPins.Count -ne 15 -or
        $r31ContentRelativePaths.Count -ne 5) {
        throw 'R31 broad shell closure must be exactly 6 code + 1 source contract + 15 referenced files + 5 content packages.'
    }
    $seenRepository = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $seenNative = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $existingPinCount = 0
    $absentPinCount = 0
    foreach ($pin in @($sourcePins) + @($sourceAssetPins) + @($contractReferencedPins)) {
        $repositoryRelativePath = Get-PinRelativePath $pin 'Repository'
        $nativeRelativePath = Get-PinRelativePath $pin 'Native'
        Assert-CanonicalPinRelativePath $repositoryRelativePath 'repository'
        Assert-CanonicalPinRelativePath $nativeRelativePath 'native'
        if (-not $seenRepository.Add($repositoryRelativePath)) {
            throw "Duplicate R31 broad shell repository path: $repositoryRelativePath"
        }
        if (-not $seenNative.Add($nativeRelativePath)) {
            throw "Duplicate R31 broad shell native destination: $nativeRelativePath"
        }
        if ($repositoryRelativePath.Contains('R29Vegetation', [StringComparison]::Ordinal) -or
            $repositoryRelativePath.Contains('LandmarkVegetation', [StringComparison]::Ordinal) -or
            $nativeRelativePath.Contains('R29Vegetation', [StringComparison]::Ordinal) -or
            $nativeRelativePath.Contains('LandmarkVegetation', [StringComparison]::Ordinal)) {
            throw "Vegetation source escaped into R31 broad shell mutation closure: repository=$repositoryRelativePath native=$nativeRelativePath"
        }
        if ($pin.NativeBeforePresent) { ++$existingPinCount } else { ++$absentPinCount }
    }
    $expectedContextRepositoryRoot =
        'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure'
    $expectedContextNativeRoot =
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion'
    foreach ($mapping in @(
        [pscustomobject] @{
            Pin = $sourcePins[0]
            Repository = "$expectedContextRepositoryRoot\TRIADIstanaExploreV5DContextPolicyActor.h"
            Native = "$expectedContextNativeRoot\Public\TRIADIstanaExploreV5DContextPolicyActor.h"
        }
        [pscustomobject] @{
            Pin = $sourcePins[1]
            Repository = "$expectedContextRepositoryRoot\TRIADIstanaExploreV5DContextPolicyActor.cpp"
            Native = "$expectedContextNativeRoot\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp"
        })) {
        if ($null -eq $mapping.Pin.PSObject.Properties['RepositoryRelativePath'] -or
            $null -eq $mapping.Pin.PSObject.Properties['NativeRelativePath'] -or
            $null -ne $mapping.Pin.PSObject.Properties['RelativePath'] -or
            (Get-PinRelativePath $mapping.Pin 'Repository') -cne $mapping.Repository -or
            (Get-PinRelativePath $mapping.Pin 'Native') -cne $mapping.Native) {
            throw 'R31 context-policy source-closure mapping drifted from explicit repository/native semantics.'
        }
    }
    $promotedReferencedPins = @($contractReferencedPins | Where-Object { $_.Promote -eq $true })
    $retainedReferencedPins = @($contractReferencedPins | Where-Object { $_.Promote -eq $false })
    if ($existingPinCount -ne 13 -or $absentPinCount -ne 9 -or
        $promotedReferencedPins.Count -ne 4 -or
        $retainedReferencedPins.Count -ne 11 -or
        @($promotedReferencedPins | Where-Object { $_.NativeBeforePresent }).Count -ne 0 -or
        @($retainedReferencedPins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 0) {
        throw 'R31 must replace two receipt-bound context-policy files, create five R31 source/contract files, promote four absent referenced files, and retain eleven exact referenced files.'
    }
    Assert-Pins $sourcePins $repositoryUnrealRoot
    Assert-Pins $sourceAssetPins $repositoryUnrealRoot
    Assert-Pins $contractReferencedPins $repositoryUnrealRoot
    $contextHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[0] 'Repository')) -Raw
    $contextSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[1] 'Repository')) -Raw
    $factory = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[3] 'Repository')) -Raw
    $editorHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[4] 'Repository')) -Raw
    $editorSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[5] 'Repository')) -Raw
    $contract = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourceAssetPins[0] 'Repository')) -Raw | ConvertFrom-Json -Depth 20
    foreach ($forbidden in @(
        'TRIADIstanaExploreV5DR33',
        'RegisterR33CesiumWorldTerrainController',
        'CesiumWorldTerrainReferenceActor')) {
        if ($contextHeader.Contains($forbidden, [StringComparison]::Ordinal) -or
            $contextSource.Contains($forbidden, [StringComparison]::Ordinal)) {
            throw "R33 source or declaration contaminated the immutable R31 context-policy closure: $forbidden"
        }
    }
    $contractClosureFiles = @($contract.nativeSourceClosure.files)
    if ([string] $contract.nativeSourceClosure.schema -cne
            'triad.istana_explore_v5d.r31_context_policy_source_closure.v1' -or
        [bool] $contract.nativeSourceClosure.immutable -ne $true -or
        [bool] $contract.nativeSourceClosure.r33SourceOrDeclarationAllowed -ne $false -or
        $contractClosureFiles.Count -ne 2) {
        throw 'R31 contract lost its exact immutable pre-R33 native source closure.'
    }
    for ($index = 0; $index -lt 2; ++$index) {
        $contractPin = $contractClosureFiles[$index]
        $wrapperPin = $sourcePins[$index]
        $expectedRepositoryFile = 'unreal/' +
            (Get-PinRelativePath $wrapperPin 'Repository').Replace('\', '/')
        $expectedNativeFile =
            (Get-PinRelativePath $wrapperPin 'Native').Replace('\', '/')
        if ([string] $contractPin.repositoryFile -cne $expectedRepositoryFile -or
            [string] $contractPin.nativeRelativePath -cne $expectedNativeFile -or
            [int64] $contractPin.bytes -ne [int64] $wrapperPin.Bytes -or
            [string] $contractPin.sha256 -cne [string] $wrapperPin.Sha256) {
            throw "R31 native source-closure contract pin drifted at index $index."
        }
    }
    $contractReferencedRows = @(
        @($contract.sourceMesh.sourceObj) +
        @($contract.sourcePins) +
        @($contract.texturePins)
    )
    if ($contractReferencedRows.Count -ne $contractReferencedPins.Count) {
        throw 'R31 wrapper/contract referenced-file closure count drifted.'
    }
    for ($index = 0; $index -lt $contractReferencedPins.Count; ++$index) {
        $wrapperPin = $contractReferencedPins[$index]
        $contractPin = $contractReferencedRows[$index]
        $expectedContractPath = 'unreal/' +
            (Get-PinRelativePath $wrapperPin 'Repository').Replace('\', '/')
        if ([string] $contractPin.file -cne $expectedContractPath -or
            [int64] $contractPin.bytes -ne [int64] $wrapperPin.Bytes -or
            [string] $contractPin.sha256 -cne [string] $wrapperPin.Sha256) {
            throw "R31 wrapper/contract referenced-file pin drifted at index $index."
        }
    }
    foreach ($marker in @(
        'ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene',
        'ValidateCurrentSurroundingsBroadShellR31ForInheritedScene',
        'ApplyCurrentSurroundingsBroadShellR31')) {
        if (-not $contextHeader.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R31 broad shell context-policy declaration missing: $marker"
        }
    }
    foreach ($marker in @(
        'ContextFacadeR25Overrides',
        'BroadShellR31Overrides[]',
        'ValidateContextFacadeR25Overrides',
        'ValidateBroadShellR31Overrides',
        'ValidateAdmittedCurrentShellMaterialState',
        'EAdmittedCurrentShellMaterialState::ContextFacadeR25',
        'EAdmittedCurrentShellMaterialState::BroadShellR31',
        'bVersionedOverridesUseExactV2Mesh',
        'ApplyCurrentSurroundingsBroadShellR31')) {
        if (-not $contextSource.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R31 compatible context-policy marker missing: $marker"
        }
    }
    $applyStart = $contextSource.IndexOf(
        'ApplyCurrentSurroundingsBroadShellR31(FString& OutError)',
        [StringComparison]::Ordinal)
    $applyEnd = $contextSource.IndexOf(
        'SuppressInheritedPlanningGroundPresentation',
        [Math]::Max($applyStart, 0),
        [StringComparison]::Ordinal)
    if ($applyStart -lt 0 -or $applyEnd -le $applyStart -or
        $contextSource.Substring($applyStart, $applyEnd - $applyStart).Contains(
            'ConfigureCurrentSurroundingsAndOuterGroundPresentation',
            [StringComparison]::Ordinal)) {
        throw 'R31 in-place apply path is absent or calls the provider-resetting configuration path.'
    }
    foreach ($receipt in @(
        'ExpectedMasterExpressionCount = 42',
        'ExpectedScalarParameterCount = 22',
        'ExpectedTextureParameterCount = 3',
        'WallVerticalWeatherMaskParameter',
        'SeamSafeMetricUv->Inputs.Num() != 4',
        'float2 wallTangentRaw = VertexTangentWS.xy',
        'float tangentQuantizationLevels = 4096.0',
        'float tangentQuantizationPhase = 89.0 / 1048576.0',
        'float wallUMetres = signedWorldAxisMetres / max(quantizedDominantMagnitude, 0.0001)',
        'float sourceAbsoluteZMetres = 1.0 - UV0.y',
        'float facadeSignalA = 0.5 + 0.5 * sin',
        'familyWeights /= max',
        'float2 q = float2(facadeUMetres / bay + familyPhaseU, sourceAbsoluteZMetres / storey + familyPhaseV)',
        'float dividerMask = insetGlass',
        'float revealMask = outerAperture',
        'float cellOccupancy = lerp',
        'float glassFresnel = pow',
        'float atmosphere = saturate',
        'Surface->Inputs.Num() != 23',
        'ExpectedScalarDefaults',
        'ExpectedInputExpressions',
        'InputMatchesExactOutput',
        'Input.Mask == Output.Mask',
        'InputIsExactlyDisconnected',
        'OutputsMatchExplicitR31Contract',
        'OutputMatches(5, TEXT("RGBA"), 1, 1, 1, 1, 1)',
        'OutputMatches(2, TEXT("Z"), 1, 0, 0, 1, 0)',
        'ExpectedNodeDescriptions',
        'ExpectedNodeClasses',
        'Actual->GetClass() != Pair.Value',
        'Expression->GetOuter() != Material',
        'Expression->Function || Expression->SubgraphExpression',
        'ExpressionCollection.EditorComments.IsEmpty()',
        'ExpressionCollection.ExpressionExecBegin',
        'ExpressionCollection.ExpressionExecEnd',
        'Material->bEnableExecWire',
        'MaterialAttributes.PropertyConnectedMask',
        'ExactBaseBlend->ConstAlpha',
        'ExactNormalBlend->ConstAlpha',
        'ExactAoBlend->ConstAlpha',
        'const bool bQuerySucceeded = Registry.Get().GetAssetsByPath',
        'ValidatePinnedRepositoryFile',
        'SourcePins->Num() != 5',
        'TexturePins->Num() != 9',
        'Normalized.RightChop(7)',
        'ValidateSingleTextureSourceProvenance',
        'FMD5Hash::HashFile',
        'Texture->Source.GetIdString().IsEmpty()',
        'Texture->Source.GetNumLayers() != 1',
        'Texture->Source.GetNumMips() != 1',
        'Texture->Source.GetFormat() != TSF_BGRA8',
        'Texture->Filter != TF_Default',
        'Texture->VirtualTextureStreaming || Texture->NeverStream',
        'Texture->CompressionNoAlpha',
        'texturePayloadBinding=source-provenance-admitted-existing-payload',
        'sourceIdClaimedAsPayloadDigest=false',
        'embeddedPixelByteEqualityClaim=false',
        'SamplerSource != SSM_FromTextureAsset',
        'MipValueMode != TMVM_None',
        'ConstCoordinate != 0',
        'ConstMipValue != INDEX_NONE',
        'EditorOnly->BaseColor.UseConstant',
        'UE_ARRAY_COUNT(EditorOnly->CustomizedUVs) != 8',
        'StaticParameters.StaticSwitchParameters.IsEmpty()',
        'EMaterialParameterAssociation::GlobalParameter',
        'ShaderMap->IsCompilationFinalized()',
        'ShaderMap->CompiledSuccessfully()',
        'textureBindings=12',
        'uniqueTextureDependencies=9',
        'retainedTriangles=43448',
        'materialOnly=true',
        'meshPackageMutated=false')) {
        if (-not $factory.Contains($receipt, [StringComparison]::Ordinal)) {
            throw "R31 broad shell graph/source receipt missing: $receipt"
        }
    }
    foreach ($forbidden in @(
        'float2 buildingCell =',
        'float verticalDirt = wallMask',
        'float plinthMask = wallMask',
        'facadePlaneDistanceKey = round',
        'step(0.25, facade',
        'step(0.50, facade',
        'step(0.75, facade')) {
        if ($factory.Contains($forbidden, [StringComparison]::Ordinal)) {
            throw "R31 broad shell contains forbidden discontinuous or false-height marker: $forbidden"
        }
    }
    foreach ($endpoint in @(
        'EnsureR31BroadShellAssets',
        'ValidateR31BroadShellAssets',
        'ApplyR31BroadShellToLoadedV5DHybridMap',
        'CommitR31BroadShellToLoadedV5DHybridMap',
        'ValidateR31BroadShellInLoadedV5DHybridMap')) {
        if (-not $editorHeader.Contains($endpoint, [StringComparison]::Ordinal) -or
            -not $editorSource.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "R31 broad shell editor endpoint missing: $endpoint"
        }
    }
    foreach ($marker in @(
        'ValidatePromotedR30Predecessor',
        'ValidateR30FacadeLookdev',
        'ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene',
        'ValidateCurrentSurroundingsBroadShellR31ForInheritedScene',
        'ApplyCurrentSurroundingsBroadShellR31',
        'ExpectedCurrentSurroundingsV2MeshObjectPath()',
        'ExactV2ComponentCount != 1',
        'R28FacadeClassCount != 0',
        'R29FacadeClassCount != 0',
        'VegetationClassCount != 1',
        'LandmarkVegetationClassCount != 0',
        'TerrainClassCount != 1',
        'TreeClassCount != 1',
        'OutRoster.R30->bProviderReady !=',
        'Transaction->IsOutstanding()',
        'UndoTransaction(false)',
        'componentReusedInPlace=true',
        'duplicateShellComponents=0',
        'providerStatePreserved=true',
        'geographyModified=false')) {
        if (-not $editorSource.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R31 broad shell editor invariant missing: $marker"
        }
    }
    $wrapperText = Get-Content -LiteralPath $PSCommandPath -Raw
    foreach ($marker in @(
        'function Assert-RequiredImmutablePrestate',
        'R30FacadeLookdev).Count -ne 12',
        'Required retained immutable root is missing or empty',
        'function Assert-R30CommitAndCaptureReceipts',
        'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31',
        'Test-ReceiptStateMatches $commitMap $ExpectedMapBytes',
        '$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L',
        '$minimumSystemFreeVirtualBytes = 6442450944L',
        '$privateMemoryCeilingBytes = 12884901888L',
        '$contractReferencedPins.Count -ne 15',
        '$promotedReferencedPins.Count -ne 4',
        '$retainedReferencedPins.Count -ne 11',
        'Assert-NativePreState $contractReferencedPins',
        'Assert-Pins $contractReferencedPins $nativeProjectRoot -NativeAfter',
        'Restore-FileJournal $contractReferencedJournal',
        'Restore-DirectoryPresenceJournal $contractReferencedDirectoryJournal',
        '$Handle.Kill($true)',
        'CreationDate -cne')) {
        if (-not $wrapperText.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R31 immutable-prestate guard missing: $marker"
        }
    }
    if ($contract.schema -cne 'triad.istana_explore_v5d.r31_broad_shell_lookdev.v1' -or
        [int] $contract.sourceMesh.sourceObj.faceCount -ne 43448 -or
        [int] $contract.sourceMesh.sourceObj.vertexRecordCount -ne 24522 -or
        [int] $contract.sourceMesh.sourceObj.textureCoordinateRecordCount -ne 130632 -or
        [int] $contract.sourceMesh.importedAsset.triangleCount -ne 43448 -or
        [int] $contract.sourceMesh.importedAsset.materialSlotCount -ne 17 -or
        [int] $contract.sourceMesh.importedAsset.renderVertexCount -ne 24468 -or
        [int] $contract.sourceMesh.importedAsset.meshDescriptionVertexInstanceCount -ne 130344 -or
        @($contract.semanticSlotRoles).Count -ne 17 -or
        @($contract.assetPackage.instances).Count -ne 4 -or
        @($contract.sourcePins).Count -ne 5 -or
        @($contract.texturePins).Count -ne 9 -or
        @($contract.assetPackage.instances | Where-Object { $_.role -like '*Wall' -and [double] $_.wallVerticalWeatherMask -ne 1.0 }).Count -ne 0 -or
        @($contract.assetPackage.instances | Where-Object { $_.role -like '*Roof' -and [double] $_.wallVerticalWeatherMask -ne 0.0 }).Count -ne 0 -or
        [bool] $contract.textureAssetBinding.sourceFileSha256Pinned -ne $true -or
        [bool] $contract.textureAssetBinding.singleSourceImportPathRequired -ne $true -or
        [bool] $contract.textureAssetBinding.storedSourceMd5MustMatchCurrentFile -ne $true -or
        [bool] $contract.textureAssetBinding.sourceIdRequiredNonEmpty -ne $true -or
        [bool] $contract.textureAssetBinding.sourceIdClaimedAsPayloadDigest -ne $false -or
        [bool] $contract.textureAssetBinding.embeddedPixelByteEqualityClaim -ne $false -or
        [bool] $contract.textureAssetBinding.nativeTextureTreeImmutableDuringTransaction -ne $true -or
        [int] $contract.nativeTransaction.contractReferencedFileCount -ne 15 -or
        [int] $contract.nativeTransaction.promotedContractReferencedFileCount -ne 4 -or
        [int] $contract.nativeTransaction.retainedContractReferencedFileCount -ne 11 -or
        [double] $contract.nativeTransaction.preflightFreeVirtualGiB -ne 10 -or
        [double] $contract.nativeTransaction.buildFreeVirtualGiB -ne 6 -or
        [double] $contract.nativeTransaction.ownedProcessPrivateMemoryCeilingGiB -ne 12) {
        throw 'R31 source contract lost its exact V2/material-role/native-guard closure.'
    }
    $expectedSlots = @(
        [pscustomobject] @{ Name='MAT_BOTTOM_HIDDEN'; Role='FallbackRoof'; Triangles=9436 }
        [pscustomobject] @{ Name='MAT_COMMERCIAL_HINT'; Role='FallbackWall'; Triangles=2166 }
        [pscustomobject] @{ Name='MAT_GENERIC_BUILDING_HINT'; Role='FallbackWall'; Triangles=14696 }
        [pscustomobject] @{ Name='MAT_HEALTHCARE_HINT'; Role='FallbackWall'; Triangles=278 }
        [pscustomobject] @{ Name='MAT_HOTEL_HINT'; Role='OfficialWall'; Triangles=414 }
        [pscustomobject] @{ Name='MAT_INDUSTRIAL_HINT'; Role='FallbackWall'; Triangles=8 }
        [pscustomobject] @{ Name='MAT_RELIGIOUS_HINT'; Role='OfficialWall'; Triangles=150 }
        [pscustomobject] @{ Name='MAT_RESIDENTIAL_HINT'; Role='FallbackWall'; Triangles=6316 }
        [pscustomobject] @{ Name='MAT_ROOF_COMMERCIAL_HINT'; Role='FallbackRoof'; Triangles=1029 }
        [pscustomobject] @{ Name='MAT_ROOF_GENERIC_BUILDING_HINT'; Role='FallbackRoof'; Triangles=5452 }
        [pscustomobject] @{ Name='MAT_ROOF_HEALTHCARE_HINT'; Role='FallbackRoof'; Triangles=131 }
        [pscustomobject] @{ Name='MAT_ROOF_HOTEL_HINT'; Role='OfficialRoof'; Triangles=169 }
        [pscustomobject] @{ Name='MAT_ROOF_INDUSTRIAL_HINT'; Role='FallbackRoof'; Triangles=2 }
        [pscustomobject] @{ Name='MAT_ROOF_RELIGIOUS_HINT'; Role='OfficialRoof'; Triangles=53 }
        [pscustomobject] @{ Name='MAT_ROOF_RESIDENTIAL_HINT'; Role='FallbackRoof'; Triangles=2684 }
        [pscustomobject] @{ Name='MAT_ROOF_TRANSPORT_HINT'; Role='FallbackRoof'; Triangles=132 }
        [pscustomobject] @{ Name='MAT_TRANSPORT_HINT'; Role='FallbackWall'; Triangles=332 }
    )
    $slotTriangleTotal = 0
    for ($index = 0; $index -lt $expectedSlots.Count; ++$index) {
        $actual = @($contract.semanticSlotRoles[$index])
        $expected = $expectedSlots[$index]
        if ($actual.Count -ne 3 -or
            [string] $actual[0] -cne $expected.Name -or
            [string] $actual[1] -cne $expected.Role -or
            [int] $actual[2] -ne $expected.Triangles) {
            throw "R31 exact imported-V2 slot mapping drifted at index $index."
        }
        $slotTriangleTotal += [int] $actual[2]
    }
    if ($slotTriangleTotal -ne 43448) {
        throw "R31 exact imported-V2 slot triangle census drifted: $slotTriangleTotal"
    }
    if ((ConvertTo-Json @($contract.nativeTransaction.unrealBuildToolArguments) -Compress) -cne
        (ConvertTo-Json @('-NoUBA', '-NoUBALocal', '-MaxParallelActions=1') -Compress)) {
        throw 'R31 source contract lost the exact serial UnrealBuildTool argument roster.'
    }
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'STATIC_SELF_CHECK_PASS'
        CodeSourceCount = $sourcePins.Count
        SourceContractCount = $sourceAssetPins.Count
        ContractReferencedFileCount = $contractReferencedPins.Count
        PromotedContractReferencedFileCount = $promotedReferencedPins.Count
        RetainedContractReferencedFileCount = $retainedReferencedPins.Count
        NewContentPackageCount = $r31ContentRelativePaths.Count
        NativeTreeWritten = $false
        UnrealBuildOrEditorLaunched = $false
        NativeSourceClosureFileCount = $contractClosureFiles.Count
        NativeSourceClosureImmutable = $true
        R33SourceOrDeclarationAllowed = $false
        V2BroadShellMeshRetained = $true
        V2BroadShellTriangles = 43448
        V2BroadShellMaterialSlots = 17
        ContextFacadeR25StrictValidatorRetained = $true
        AdmittedCurrentShellDispatcher = $true
        R30PredecessorRequired = $true
        R30CoexistenceRequired = $true
        R31ComponentReuseInPlace = $true
        R28PublicRealmRetained = $true
        R30AndR29ContentMutationAllowed = $false
        VegetationMutationAllowed = $false
        TerrainMutationAllowed = $false
        ProviderPolicyMutationAllowed = $false
        SimulationSensorRfAuthority = $false
    }
}

$staticReceipt = Assert-StaticContract
if ($StaticSelfCheck) {
    $staticReceipt | ConvertTo-Json -Depth 8
    exit 0
}
if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'READ_ONLY_REPOSITORY_PREFLIGHT_PASS'
        ExecuteRequiredForNativeWrites = $true
        ExplicitPredecessorAndDllReceiptsRequired = $true
        NativeTreeWritten = $false
        UnrealBuildOrEditorLaunched = $false
        StaticReceipt = $staticReceipt
    } | ConvertTo-Json -Depth 8
    exit 0
}

foreach ($name in @(
    'R30CommitReceipt', 'ExpectedR30CommitReceiptSha256',
    'R30CaptureCommitReceipt', 'ExpectedR30CaptureCommitReceiptSha256',
    'ExpectedMapBytes', 'ExpectedMapSha256',
    'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
    'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256',
    'ExpectedContextPolicyHeaderBytes', 'ExpectedContextPolicyHeaderSha256',
    'ExpectedContextPolicySourceBytes', 'ExpectedContextPolicySourceSha256')) {
    if (-not $PSBoundParameters.ContainsKey($name)) {
        throw "Live R31 broad shell execution requires explicit caller-supplied parameter: $name"
    }
}
if (-not $RequireR30Predecessor) {
    throw 'Live R31 broad shell execution requires -RequireR30Predecessor after the independent R30 commit/capture decision.'
}
foreach ($pair in @(
    @($ExpectedMapBytes, $ExpectedMapSha256),
    @($ExpectedRuntimeDllBytes, $ExpectedRuntimeDllSha256),
    @($ExpectedEditorDllBytes, $ExpectedEditorDllSha256),
    @($ExpectedContextPolicyHeaderBytes, $ExpectedContextPolicyHeaderSha256),
    @($ExpectedContextPolicySourceBytes, $ExpectedContextPolicySourceSha256))) {
    if ([int64] $pair[0] -le 0 -or [string] $pair[1] -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'Live R31 broad shell receipts require positive bytes and exact SHA-256.'
    }
}
foreach ($required in @(
    $nativeProjectFile, $buildTool, $dotnet, $unrealBuildTool, $editor,
    $mapFile, $runtimeDll, $editorDll)) {
    if (-not [IO.File]::Exists($required)) { throw "Required native file missing: $required" }
}
$r30Admission = Assert-R30CommitAndCaptureReceipts
if ([IO.Directory]::Exists($transactionRoot)) {
    throw "Transaction run token already exists: $transactionRoot"
}
if ([IO.Directory]::Exists($r31ContentRoot)) {
    throw "Initially absent R31 broad shell content root already exists: $r31ContentRoot"
}
Assert-NativePreState $sourcePins
Assert-NativePreState $sourceAssetPins
Assert-NativePreState $contractReferencedPins
$expectedMap = [pscustomobject] @{ Present = $true; Bytes = $ExpectedMapBytes; Sha256 = $ExpectedMapSha256.ToUpperInvariant() }
$expectedRuntime = [pscustomobject] @{ Present = $true; Bytes = $ExpectedRuntimeDllBytes; Sha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant() }
$expectedEditor = [pscustomobject] @{ Present = $true; Bytes = $ExpectedEditorDllBytes; Sha256 = $ExpectedEditorDllSha256.ToUpperInvariant() }
[void] (Assert-State $expectedMap $mapFile 'explicit promoted-R30 predecessor map')
[void] (Assert-State $expectedRuntime $runtimeDll 'runtime DLL predecessor')
[void] (Assert-State $expectedEditor $editorDll 'editor DLL predecessor')
$script:protectedBefore = @(Get-ProtectedProcesses)
Assert-NativeIdle 'prewrite boundary'
Assert-ProtectedUnchanged $script:protectedBefore
$prewriteAdmission = Assert-LaunchAdmission 'before first native write'

$stageResults = [Collections.Generic.List[object]]::new()
$committed = $false
$failure = $null
$transactionStarted = $false
$sourceJournal = @()
$sourceAssetJournal = @()
$contractReferencedJournal = @()
$contractReferencedDirectoryJournal = @()
$mapJournal = @()
$buildJournal = @()
$backupMap = ''
$immutableBefore = $null
try {
    [void] [IO.Directory]::CreateDirectory($transactionRoot)
    $transactionStarted = $true
    $journalRoot = Join-Path $transactionRoot 'rollback'
    $sourceDestinations = @($sourcePins | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $_ 'Native')))
    })
    $sourceAssetDestinations = @($sourceAssetPins | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $_ 'Native')))
    })
    $contractReferencedDestinations = @(
        $contractReferencedPins |
            Where-Object { $_.Promote -eq $true } |
            ForEach-Object { [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath)) }
    )
    $sourceJournal = @(New-FileJournal $sourceDestinations (Join-Path $journalRoot 'source'))
    $sourceAssetJournal = @(New-FileJournal $sourceAssetDestinations (Join-Path $journalRoot 'source-assets'))
    $contractReferencedJournal = @(New-FileJournal $contractReferencedDestinations (Join-Path $journalRoot 'contract-referenced'))
    $contractReferencedDirectoryJournal = @(New-DirectoryPresenceJournal $contractReferencedDestinations)
    $mapJournal = @(New-FileJournal @($mapFile) (Join-Path $journalRoot 'map'))
    $buildJournal = @(New-TreeJournal @($pluginBinaryRoot, $pluginIntermediateRoot) (Join-Path $journalRoot 'build'))
    $immutableBefore = [ordered] @{
        R30FacadeLookdev = @(Get-TreeReceipt $r30ContentRoot)
        R29FacadeEnvironment = @(Get-TreeReceipt $r29ContentRoot)
        R28Environment = @(Get-TreeReceipt $r28ContentRoot)
        VegetationR29 = @(Get-TreeReceipt $vegetationR29ContentRoot)
        LandmarkVegetationR28 = @(Get-TreeReceipt $landmarkVegetationR28ContentRoot)
        TreeRealism = @(Get-TreeReceipt $treeRealismContentRoot)
        TerrainR29 = @(Get-TreeReceipt $terrainR29ContentRoot)
        LocalFallbackSuppressionV2 = @(Get-TreeReceipt $localFallbackSuppressionV2ContentRoot)
        OuterGroundLoadingFallback = @(Get-TreeReceipt $outerGroundLoadingFallbackContentRoot)
        InheritedV5C = @(Get-TreeReceipt $v5cContentRoot)
        ContextFacadeR25 = @(Get-TreeReceipt $contextFacadeR25ContentRoot)
        ContextTextures = @(Get-TreeReceipt $contextTextureRoot)
        HeroMaterialsV2 = @(Get-TreeReceipt $heroV2ContentRoot)
        HeroMaterialsV3 = @(Get-TreeReceipt $heroV3ContentRoot)
        HeroMaterialsV4 = @(Get-TreeReceipt $heroV4ContentRoot)
        HeroMaterialsV5 = @(Get-TreeReceipt $heroV5ContentRoot)
    }
    Assert-RequiredImmutablePrestate $immutableBefore
    $backupMap = [IO.Path]::GetFullPath((Join-Path $transactionRoot 'Istana_PublicView_Explore_v5d_hybrid.r30-predecessor.umap'))
    Copy-Item -LiteralPath $mapFile -Destination $backupMap
    [void] (Assert-State $expectedMap $backupMap 'external map backup')

    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        $source = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot (Get-PinRelativePath $pin 'Repository')))
        $destination = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $pin 'Native')))
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination
    }
    foreach ($pin in @($contractReferencedPins | Where-Object { $_.Promote -eq $true })) {
        $source = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot $pin.RelativePath))
        $destination = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $pin.RelativePath))
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination
    }
    Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter
    Assert-Pins $sourceAssetPins $nativeProjectRoot -NativeAfter
    Assert-Pins $contractReferencedPins $nativeProjectRoot -NativeAfter
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before build'
    $buildAdmission = Assert-LaunchAdmission 'before forced R31 broad shell build'
    $buildLog = Join-Path $transactionRoot 'build.stdout.log'
    $buildErrorLog = Join-Path $transactionRoot 'build.stderr.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development',
        "-Project=$nativeProjectFile", '-WaitMutex', '-NoHotReloadFromIDE',
        '-Module=TRIADSensorFusion', '-Module=TRIADSensorFusionEditor',
        '-ForceHeaderGeneration', '-NoUBTMakefiles',
        '-MaxParallelActions=1',
        '-NoUBA', '-NoUBALocal'
    )
    $buildGuard = Invoke-GuardedOwnedBuild `
        -Arguments $buildArguments `
        -StandardOutputLog $buildLog `
        -StandardErrorLog $buildErrorLog
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Sha256 -ceq $expectedRuntime.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditor.Sha256) {
        throw 'Forced build did not produce fresh runtime and editor DLL receipts.'
    }
    $runtimeBytes = [IO.File]::ReadAllBytes($runtimeDll)
    $editorBytes = [IO.File]::ReadAllBytes($editorDll)
    $runtimeText = [Text.Encoding]::ASCII.GetString($runtimeBytes) + [Text.Encoding]::Unicode.GetString($runtimeBytes)
    $editorText = [Text.Encoding]::ASCII.GetString($editorBytes) + [Text.Encoding]::Unicode.GetString($editorBytes)
    foreach ($marker in @('ApplyCurrentSurroundingsBroadShellR31', 'ValidateCurrentSurroundingsBroadShellR31ForInheritedScene', 'BroadShellR31Overrides')) {
        if (-not $runtimeText.Contains($marker, [StringComparison]::Ordinal)) { throw "Runtime DLL lacks R31 broad shell marker: $marker" }
    }
    foreach ($marker in @('CommitR31BroadShellToLoadedV5DHybridMap', 'ValidateR31BroadShellInLoadedV5DHybridMap')) {
        if (-not $editorText.Contains($marker, [StringComparison]::Ordinal)) { throw "Editor DLL lacks R31 broad shell marker: $marker" }
    }

    $stageResults.Add((Invoke-ColdStage '01_ensure_r31_broad_shell_lookdev_assets' 'EnsureR31BroadShellAssets' 'OutMessage' 'EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_PASS'))
    foreach ($relative in $r31ContentRelativePaths) {
        $state = Get-FileState ([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
        if (-not $state.Present -or $state.Bytes -le 0 -or $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "R31 broad shell content package missing after asset stage: $relative"
        }
    }
    if (@(Get-ChildItem -LiteralPath $r31ContentRoot -File -Recurse).Count -ne 5) {
        throw 'R31 broad shell content root is not the exact five-file namespace.'
    }
    $stageResults.Add((Invoke-ColdStage '02_validate_r31_broad_shell_lookdev_assets' 'ValidateR31BroadShellAssets' 'OutReport' 'ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_ASSETS_VALID'))
    $stageResults.Add((Invoke-ColdStage '03_commit_r31_broad_shell_lookdev_replacement' 'CommitR31BroadShellToLoadedV5DHybridMap' 'OutReport' 'ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_PASS' @{
        ExpectedPredecessorBytes = [int64] $ExpectedMapBytes
        ExpectedPredecessorSha256 = $ExpectedMapSha256.ToUpperInvariant()
        VerifiedExternalBackupFilename = $backupMap
    }))
    $stageResults.Add((Invoke-ColdStage '04_cold_validate_r31_broad_shell_lookdev_replacement' 'ValidateR31BroadShellInLoadedV5DHybridMap' 'OutReport' 'ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_MAP_VALID'))
    $successQuiescence = Wait-NativeMutationQuiescence `
        -Label 'before R31 success receipts and commit receipt' `
        -TimeoutSeconds 60

    Assert-TreeReceipt $r30ContentRoot $immutableBefore.R30FacadeLookdev 'R30 facade lookdev'
    Assert-TreeReceipt $r29ContentRoot $immutableBefore.R29FacadeEnvironment 'R29 facade environment'
    Assert-TreeReceipt $r28ContentRoot $immutableBefore.R28Environment 'R28 environment/public realm'
    Assert-TreeReceipt $vegetationR29ContentRoot $immutableBefore.VegetationR29 'R29 vegetation'
    Assert-TreeReceipt $landmarkVegetationR28ContentRoot $immutableBefore.LandmarkVegetationR28 'R28 landmark vegetation'
    Assert-TreeReceipt $treeRealismContentRoot $immutableBefore.TreeRealism 'tree realism'
    Assert-TreeReceipt $terrainR29ContentRoot $immutableBefore.TerrainR29 'R29 Copernicus terrain fallback'
    Assert-TreeReceipt $localFallbackSuppressionV2ContentRoot $immutableBefore.LocalFallbackSuppressionV2 'exact V2 local-fallback suppression mesh'
    Assert-TreeReceipt $outerGroundLoadingFallbackContentRoot $immutableBefore.OuterGroundLoadingFallback 'outer-ground loading fallback'
    Assert-TreeReceipt $v5cContentRoot $immutableBefore.InheritedV5C 'inherited V5C content'
    Assert-TreeReceipt $contextFacadeR25ContentRoot $immutableBefore.ContextFacadeR25 'current-surroundings context facade R25'
    Assert-TreeReceipt $contextTextureRoot $immutableBefore.ContextTextures 'context-admitted public-view textures'
    Assert-TreeReceipt $heroV2ContentRoot $immutableBefore.HeroMaterialsV2 'HeroMaterialsV2'
    Assert-TreeReceipt $heroV3ContentRoot $immutableBefore.HeroMaterialsV3 'HeroMaterialsV3'
    Assert-TreeReceipt $heroV4ContentRoot $immutableBefore.HeroMaterialsV4 'HeroMaterialsV4'
    Assert-TreeReceipt $heroV5ContentRoot $immutableBefore.HeroMaterialsV5 'HeroMaterialsV5'
    Assert-Pins $sourcePins $repositoryUnrealRoot
    Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter
    Assert-Pins $sourceAssetPins $repositoryUnrealRoot
    Assert-Pins $sourceAssetPins $nativeProjectRoot -NativeAfter
    Assert-Pins $contractReferencedPins $nativeProjectRoot -NativeAfter
    [void] (Assert-State $r30Admission.Transaction.State $r30Admission.Transaction.Path 'R30 transaction receipt')
    [void] (Assert-State $r30Admission.Capture.State $r30Admission.Capture.Path 'R30 Player0 capture receipt')
    [void] (Assert-State $expectedMap $backupMap 'preserved external map backup')
    $successorMap = Get-FileState $mapFile
    if (-not $successorMap.Present -or $successorMap.Bytes -le 0 -or
        $successorMap.Sha256 -notmatch '^[A-F0-9]{64}$' -or
        ($successorMap.Bytes -eq $expectedMap.Bytes -and $successorMap.Sha256 -ceq $expectedMap.Sha256)) {
        throw 'R31 successor map receipt did not change from exact promoted-R30 predecessor.'
    }
    $commit = [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'COMMITTED'
        RunToken = $RunToken
        PrewriteAdmission = $prewriteAdmission
        BuildAdmission = $buildAdmission
        BuildContinuousMemoryGuard = $buildGuard
        NativeMutationQuiescence = $successQuiescence
        PredecessorMap = $expectedMap
        SuccessorMap = $successorMap
        ExternalBackup = Get-FileState $backupMap
        RuntimeDllBefore = $expectedRuntime
        RuntimeDllAfter = $runtimeAfter
        EditorDllBefore = $expectedEditor
        EditorDllAfter = $editorAfter
        StageResults = @($stageResults)
        NativeSourceClosureFiles = @(
            $sourcePins[0..1] | ForEach-Object {
                [pscustomobject] [ordered] @{
                    RepositoryRelativePath = Get-PinRelativePath $_ 'Repository'
                    NativeRelativePath = Get-PinRelativePath $_ 'Native'
                    State = Get-FileState ([IO.Path]::GetFullPath(
                        (Join-Path $nativeProjectRoot (Get-PinRelativePath $_ 'Native'))))
                }
            }
        )
        R33SourceOrDeclarationAllowed = $false
        ContractReferencedFileCount = $contractReferencedPins.Count
        PromotedContractReferencedFileCount = @($contractReferencedPins | Where-Object { $_.Promote -eq $true }).Count
        RetainedContractReferencedFileCount = @($contractReferencedPins | Where-Object { $_.Promote -eq $false }).Count
        ContractReferencedFiles = @(
            $contractReferencedPins | ForEach-Object {
                [pscustomobject] [ordered] @{
                    RelativePath = [string] $_.RelativePath
                    Promoted = [bool] $_.Promote
                    State = Get-FileState ([IO.Path]::GetFullPath(
                        (Join-Path $nativeProjectRoot $_.RelativePath)))
                }
            }
        )
        R30TransactionAdmission = [pscustomobject] [ordered] @{
            Path = $r30Admission.Transaction.Path
            File = $r30Admission.Transaction.State
        }
        R30CaptureAdmission = [pscustomobject] [ordered] @{
            Path = $r30Admission.Capture.Path
            File = $r30Admission.Capture.State
        }
        R31ContentPackageCount = 5
        R30FacadeLookdevAssetsModified = $false
        R30FacadeCueCoexists = $true
        R29FacadeEnvironmentAssetsModified = $false
        V2BroadShellMeshRetained = $true
        R28PublicRealmRetained = $true
        R29EnvironmentAssetsModified = $false
        VegetationMutationAllowed = $false
        TreeRealismValidated = $true
        TreeMaterialResponseV3 = $r30Admission.TreeMaterialResponseV3
        TreeMaterialResponseV3PromotedAtR30 = $true
        TreeMaterialResponseV3Preserved = $true
        TreeResponseMaterialPackageCount = 13
        TreeDerivativeMeshPackageCount = 5
        TreeRuntimeResponseMidCount = 26
        TreePlacementGeometryOpacityWindAuthorityModified = $false
        TerrainR29Validated = $true
        ContextPolicyShellValidatedBeforeAndAfter = $true
        ContextTextureAssetsModified = $false
        HeroMaterialsDependencyAllowed = $false
        SimulationCollisionNavigationSensorRfAuthority = $false
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM
    $committed = $true
    $commit | ConvertTo-Json -Depth 12
}
catch {
    $failure = $_.Exception
}
finally {
    if (-not $committed -and $transactionStarted) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        $rollbackMayMutateFilesystem = $false
        try {
            [void] (Wait-NativeMutationQuiescence `
                -Label 'before R31 filesystem rollback restoration' `
                -TimeoutSeconds 60)
            $rollbackMayMutateFilesystem = $true
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackMayMutateFilesystem) {
            try { Restore-FileJournal $mapJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Remove-IsolatedR31Content } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-TreeJournal $buildJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $contractReferencedJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-DirectoryPresenceJournal $contractReferencedDirectoryJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $sourceAssetJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $sourceJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { [void] (Assert-State $expectedMap $mapFile 'rollback promoted-R30 map') } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { [void] (Assert-State $expectedRuntime $runtimeDll 'rollback runtime DLL') } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { [void] (Assert-State $expectedEditor $editorDll 'rollback editor DLL') } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Assert-NativePreState $sourcePins } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Assert-NativePreState $sourceAssetPins } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Assert-NativePreState $contractReferencedPins } catch { $rollbackErrors.Add($_.Exception.Message) }
            if ([IO.Directory]::Exists($r31ContentRoot)) {
                $rollbackErrors.Add("R31 content root remained after rollback: $r31ContentRoot")
            }
            try { [void] (Assert-State $r30Admission.Transaction.State $r30Admission.Transaction.Path 'rollback R30 transaction receipt') } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { [void] (Assert-State $r30Admission.Capture.State $r30Admission.Capture.Path 'rollback R30 capture receipt') } catch { $rollbackErrors.Add($_.Exception.Message) }
            if ($null -ne $immutableBefore) {
                foreach ($entry in @(
                    [pscustomobject] @{ Root = $r30ContentRoot; Expected = $immutableBefore.R30FacadeLookdev; Label = 'rollback R30 facade lookdev' }
                    [pscustomobject] @{ Root = $r29ContentRoot; Expected = $immutableBefore.R29FacadeEnvironment; Label = 'rollback R29 facade environment' }
                    [pscustomobject] @{ Root = $r28ContentRoot; Expected = $immutableBefore.R28Environment; Label = 'rollback R28 environment' }
                    [pscustomobject] @{ Root = $vegetationR29ContentRoot; Expected = $immutableBefore.VegetationR29; Label = 'rollback R29 vegetation' }
                    [pscustomobject] @{ Root = $landmarkVegetationR28ContentRoot; Expected = $immutableBefore.LandmarkVegetationR28; Label = 'rollback R28 vegetation' }
                    [pscustomobject] @{ Root = $treeRealismContentRoot; Expected = $immutableBefore.TreeRealism; Label = 'rollback tree realism' }
                    [pscustomobject] @{ Root = $terrainR29ContentRoot; Expected = $immutableBefore.TerrainR29; Label = 'rollback R29 terrain' }
                    [pscustomobject] @{ Root = $localFallbackSuppressionV2ContentRoot; Expected = $immutableBefore.LocalFallbackSuppressionV2; Label = 'rollback exact V2 local-fallback suppression mesh' }
                    [pscustomobject] @{ Root = $outerGroundLoadingFallbackContentRoot; Expected = $immutableBefore.OuterGroundLoadingFallback; Label = 'rollback outer-ground loading fallback' }
                    [pscustomobject] @{ Root = $v5cContentRoot; Expected = $immutableBefore.InheritedV5C; Label = 'rollback inherited V5C content' }
                    [pscustomobject] @{ Root = $contextFacadeR25ContentRoot; Expected = $immutableBefore.ContextFacadeR25; Label = 'rollback R25 materials' }
                    [pscustomobject] @{ Root = $contextTextureRoot; Expected = $immutableBefore.ContextTextures; Label = 'rollback textures' }
                    [pscustomobject] @{ Root = $heroV2ContentRoot; Expected = $immutableBefore.HeroMaterialsV2; Label = 'rollback HeroMaterialsV2' }
                    [pscustomobject] @{ Root = $heroV3ContentRoot; Expected = $immutableBefore.HeroMaterialsV3; Label = 'rollback HeroMaterialsV3' }
                    [pscustomobject] @{ Root = $heroV4ContentRoot; Expected = $immutableBefore.HeroMaterialsV4; Label = 'rollback HeroMaterialsV4' }
                    [pscustomobject] @{ Root = $heroV5ContentRoot; Expected = $immutableBefore.HeroMaterialsV5; Label = 'rollback HeroMaterialsV5' })) {
                    try { Assert-TreeReceipt $entry.Root $entry.Expected $entry.Label } catch { $rollbackErrors.Add($_.Exception.Message) }
                }
            }
        }
        else {
            $rollbackErrors.Add(
                'Filesystem rollback was intentionally not attempted while an owned build/editor process tree remained active; exact journals were retained for guarded recovery.')
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore } catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback = [pscustomobject] [ordered] @{
            Schema = $schema
            Status = if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            Failure = if ($null -eq $failure) { 'unknown' } else { $failure.Message }
            RollbackErrors = @($rollbackErrors)
        }
        $rollback | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $transactionRoot 'rollback.json') -Encoding utf8NoBOM
        if ($rollbackErrors.Count -ne 0) {
            throw "R31 broad shell transaction failed and rollback was incomplete: failure={$($rollback.Failure)} rollback={$([string]::Join(' | ', @($rollbackErrors)))}"
        }
    }
}
if ($null -ne $failure) { throw $failure }
