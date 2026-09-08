#requires -Version 7.0

<#
.SYNOPSIS
Runs the strict V5D combined Visual Realism R28 transaction.

.DESCRIPTION
The default invocation is a read-only preflight. -Execute is the only native
write authority. The live path promotes exactly sixteen byte/hash-pinned code
files and five byte/hash-pinned R28 source-asset specifications, forces UHT
while rebuilding eight directly affected translation units, creates exactly
four isolated landmark-grass packages plus thirteen isolated environment
packages, and atomically promotes the exact R27 map to the combined R28 map.
It cold-validates the successor and proves a byte-stable idempotent apply.
The retained R27, R25, V2 and Phase-2 assets remain immutable. Visual
acceptance remains false until a fresh R28 evidence capture.

Live execution requires caller-supplied map and DLL receipts from the exact
committed R27 successor. Before the first native write it records and backs up
the exact map, every promoted destination, all seventeen initially absent R28
content packages, immutable ancestor content, and every file under the bounded
TRIAD plugin Binaries/Intermediate build roots plus the known project-global
UBT sidecars. A fixed 10 GiB system FreeVirtualMemory admission gate is applied
before the build and before every fresh helper launch. Failure restores every
journalled surface without recursively deleting a directory. A pre-existing
exact UE5.4 CAPSTONE process set is treated as immutable and is never stopped;
any UE5.5 editor/Cmd instance or RC owner is refused at each idle boundary.

.EXAMPLE
.\Invoke-IstanaExploreV5DVisualRealismR28NativeTransactionV1.ps1 -RunToken reviewed_r28

.EXAMPLE
# Supply all six values from the committed R27 receipt.
.\Invoke-IstanaExploreV5DVisualRealismR28NativeTransactionV1.ps1 -RunToken reviewed_r28 -Execute -RequireR27Predecessor -ExpectedMapBytes <bytes> -ExpectedMapSha256 <sha256> -ExpectedRuntimeDllBytes <bytes> -ExpectedRuntimeDllSha256 <sha256> -ExpectedEditorDllBytes <bytes> -ExpectedEditorDllSha256 <sha256>
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,

    [switch] $StaticSelfCheck,

    # Documentation/read-only defaults only. Live execution requires all six
    # parameters to be supplied explicitly from the committed R27 receipt.
    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedMapBytes = 36335930L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedMapSha256 =
        '9C9660B02F3B9FBF8C679182E8EB39B8FECD0A618AECA6ED546AAD39E0C895E5',

    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedRuntimeDllBytes = 4676096L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedRuntimeDllSha256 =
        'C341A9F2583248824911EBB5904EC357E0DCB840304C637D87FD0CE87E67B910',

    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedEditorDllBytes = 7781888L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedEditorDllSha256 =
        '30569F04A956FDCC24F5AE8C0A99D1B70D86C03208E9497EE9BBD8480ABCC2B6',

    [switch] $RequireR27Predecessor,

    [ValidateRange(60, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema =
    'triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$repositoryUnrealRoot = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal')
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD\TRIAD.uproject')
$engineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Build\BatchFiles\Build.bat'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DVisualRealismR28V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Intermediate'))
$ddcRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\DerivedDataCache'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$assetLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary'
$environmentLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR28EnvironmentEditorLibrary'
$temasekLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DTemasekShophouseEditorLibrary'
$hybridLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

$expectedProjectPin = [pscustomobject] [ordered] @{
    Path = $nativeProjectFile
    Present = $true
    Bytes = 1298L
    Sha256 =
        '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
}
$expectedMapPin = [pscustomobject] [ordered] @{
    Path = $mapFile
    Present = $true
    Bytes = [int64] $ExpectedMapBytes
    Sha256 = ([string] $ExpectedMapSha256).ToUpperInvariant()
}
$expectedRuntimeDllPin = [pscustomobject] [ordered] @{
    Path = $runtimeDll
    Present = $true
    Bytes = [int64] $ExpectedRuntimeDllBytes
    Sha256 = ([string] $ExpectedRuntimeDllSha256).ToUpperInvariant()
}
$expectedEditorDllPin = [pscustomobject] [ordered] @{
    Path = $editorDll
    Present = $true
    Bytes = [int64] $ExpectedEditorDllBytes
    Sha256 = ([string] $ExpectedEditorDllSha256).ToUpperInvariant()
}

# Exact R28 mutation closure. The native-before pins are the committed R27
# successor: ten existing files and six exact-absence additions. The Hybrid
# pair also owns the combined one-save transaction and R28-aware validation.
$sourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DLandmarkVegetationActor.h'
        Bytes = 12748L
        Sha256 = '1040C15E3B7274E8706B9F4A13A4C6FA3B67A234678771F4638CE01E06406BB3'
        NativeBeforePresent = $true
        NativeBeforeBytes = 11493L
        NativeBeforeSha256 = '1B73DECAEB572876817F63B3B4EA8C6362F0695C9F2468137EDA92D148107D08'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp'
        Bytes = 84297L
        Sha256 = '7F9B3F52286C2C02D4C1CB9993499BE7478D44BAB8DF7DA125941C53F476EE43'
        NativeBeforePresent = $true
        NativeBeforeBytes = 55817L
        NativeBeforeSha256 = '537C35F16F10C0E3BDFD4E25F41028E5FE2875134B08F0D3C92805A25BA00BEF'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h'
        Bytes = 12448L
        Sha256 = 'EF2D6A32781462565FAA568B51F3FE0B322623CF82090E399EF2311196D90553'
        NativeBeforePresent = $true
        NativeBeforeBytes = 11566L
        NativeBeforeSha256 = '17419FF25CD3BFECCFF082635F8ED1C9668BC4EA4D4BCA7E5D5EAEDAF7EBAD06'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp'
        Bytes = 726040L
        Sha256 = '1BDABF90F5E04BE0487DA93C0806A9E89DCAA05B1B35A77FA6C10857D6CE15B4'
        NativeBeforePresent = $true
        NativeBeforeBytes = 722720L
        NativeBeforeSha256 = 'B4371454C084CBE47D52E31421808C867004AE0418A66D560973C5C850AC713F'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h'
        Bytes = 2777L
        Sha256 = '6337874A05B8CBDF321C81B7F18B217B30788B63757FB797F0BAA8E06900F430'
        NativeBeforePresent = $true
        NativeBeforeBytes = 1619L
        NativeBeforeSha256 = '4718F81CDE54D999E6559382278C9EBBFE426936931F7B5669A84997B23F1A01'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp'
        Bytes = 53669L
        Sha256 = '6449EF8F534E2EAAB501063A5A0E6253E1715655C4A53E4A9020CD98B455032E'
        NativeBeforePresent = $true
        NativeBeforeBytes = 28044L
        NativeBeforeSha256 = '7926155C2AEEDABD19BC1C56534AEC3DE3C83E41D9382E5219B576E2D6EDA7F1'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h'
        Bytes = 14928L
        Sha256 = '52DE5BE0B7228669242C2620DFF2191004AE630520C9073729E3FA9543B26F01'
        NativeBeforePresent = $true
        NativeBeforeBytes = 11654L
        NativeBeforeSha256 = 'EE0A2CE3FAC359B7DDDF808775E748D7E29214437EFD438456D7333819F31661'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
        Bytes = 505613L
        Sha256 = 'FACFF7B8757DF68B824937CA04AE79F4B2B2A55D08B32711424E717BD0FE7DCD'
        NativeBeforePresent = $true
        NativeBeforeBytes = 452992L
        NativeBeforeSha256 = 'D82148FCB9E9B647B1D3DFD1C5E68582052EF6175160214771CF6398E6705317'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR28EnvironmentActor.h'
        Bytes = 4648L
        Sha256 = 'D38DAD76CEA609F032BA250E6648B3F66FA964DE7E6E3CC9BF4E692A57C7B703'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR28EnvironmentActor.cpp'
        Bytes = 17090L
        Sha256 = '2BBA03A7AF2559D2D0D312B5A875D99439D9545F3EDE327EAEEECDBDCBB38781'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h'
        Bytes = 950L
        Sha256 = 'CC916ADEFF7F60D24EEE6D8FA5220FF79A6139E7D8FBCDEE9E62DE0CAA53B3FD'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR28EnvironmentAssetFactory.cpp'
        Bytes = 48142L
        Sha256 = 'DE744EE356EBBF4E5CAD18F41E9B0D915EDCD541AF9481B113E84338FE110EEE'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h'
        Bytes = 1234L
        Sha256 = 'D979B3B833F85E4B2F1B7B7041742878C07A8B024066E473FCDD3144F6AA7284'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp'
        Bytes = 9898L
        Sha256 = '737190BE5C6A9B0A412224447A5929C237CC22651637483B545C553118D3923B'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h'
        Bytes = 10340L
        Sha256 = 'B81DE0307CC50F8FB89373900D0C7B63776C41419935FAB5F0EF64DB60E8894E'
        NativeBeforePresent = $true
        NativeBeforeBytes = 9003L
        NativeBeforeSha256 = '07858E83E7E73E103922A0FB3EA52FA76F30A9D96F1A402DA6631F75E9A73774'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Bytes = 125137L
        Sha256 = 'BF3750FD6A4C65885DC8E4D94BB97A2464F83CE0B9B5119830E1A0D17F1FF841'
        NativeBeforePresent = $true
        NativeBeforeBytes = 93879L
        NativeBeforeSha256 = '6E2C5697AE3949CFD009D4762A334E71A09DFAEA60F53823C670D19137C082EA'
    }
)

# The two generated OBJ files, their shared MTL, the generator manifest and
# the authored contract are promoted byte-for-byte. All five destinations are
# exact-absence R27 predecessors and are journalled independently from code.
$sourceAssetPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\Generated\SM_IPV5D_R28_ContextArchitecturalDressing_Render.obj'
        Bytes = 59534571L
        Sha256 = '77E17482E73AB65222D5B0B0C70A407DB898F360FE1051BB338FDF31C0B88096'
        NativeBeforePresent = $false; NativeBeforeBytes = 0L; NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\Generated\SM_IPV5D_R28_ConnectivePublicRealm_Render.obj'
        Bytes = 12484549L
        Sha256 = 'C8CD38ACB1C1730EA928FC31FC825C8803DF7B8F7E13878254776714AFE26842'
        NativeBeforePresent = $false; NativeBeforeBytes = 0L; NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\Generated\IstanaPublicViewV5DR28EnvironmentalDressing.mtl'
        Bytes = 1233L
        Sha256 = 'C237F52740740E95F510502F037D8C4CA9A3AA735C8EC47848DCB7B0D97CA946'
        NativeBeforePresent = $false; NativeBeforeBytes = 0L; NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\Generated\IstanaPublicViewV5DR28EnvironmentalDressing.manifest.json'
        Bytes = 20596L
        Sha256 = '3366F0B1D6C7A5801538B897488DCF506D1A1014AFB4E8676B64223420C8C641'
        NativeBeforePresent = $false; NativeBeforeBytes = 0L; NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing\r28_environmental_dressing.contract.json'
        Bytes = 9317L
        Sha256 = '0CA25D72C3FAAEA2AF445E74ED374AD9AAA149DDC4BDF0691DC235435D9C8DF8'
        NativeBeforePresent = $false; NativeBeforeBytes = 0L; NativeBeforeSha256 = 'ABSENT'
    }
)
$r28SourceAssetRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R28EnvironmentalDressing'))

# Exact Phase-2 compiled-source closure. These files are intentionally not
# promoted by R28, but the forced two-module build may compile them; byte drift
# is therefore refused before build and rechecked through commit.
$phase2CompiledSourcePins = @(
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseActor.cpp'; Bytes=22760L; Sha256='26765231E1E48196B75F786D640BC2E2DB872CDD665DC83F148F1D4623802E28' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseProvenance.h'; Bytes=5433L; Sha256='AC19B7B935D49F7C5CBD8DD7CCC891DF361B8E74B646ADB1FBD5C565ED91E1BC' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'; Bytes=14772L; Sha256='A97778A9C4F4DC940C9F550C24376778999D28A5E7600467582EE397514CB992' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'; Bytes=10610L; Sha256='1B9EA89E7C86374C6B9D4D82DD8DCF3BD23CA8F3B35ECBA222074B5B836AFA90' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'; Bytes=66908L; Sha256='6CE20C0BADBBC9291B739091B11E05436BCE71B928EBD3E276A52224EA06808C' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'; Bytes=9627L; Sha256='81AC1C9EE3B708B1943B08C60A1AA922096A010A1631C9002AEE6DF2C2BC18AE' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseActor.h'; Bytes=4064L; Sha256='D72DD0458B10D3EE5038B2E7623D405BCECB55B944B0BCCA271DDB25140F89FC' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h'; Bytes=496L; Sha256='1CCF3D7F68B74325ABFBCBACCEA9ECD099EC8FA71D9A5A53C499DEE0157A4FB0' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h'; Bytes=1058L; Sha256='3225071442862102AB6B7005ACCA61B0E385B47D9834819584554C9C8F8CC95A' }
)

# Focused immutable R25/V2 source closure plus the exact existing synthetic
# visual-terrain OBJ sampled by R28. Together with the Phase-2 closure, these
# pins prove that the transaction did not silently replace an ancestor factory,
# the V2 provenance contract, or its terrain admission input.
$immutableAncestorSourcePins = @(
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h'; Bytes=709L; Sha256='A3D6114BA9450E9A32B0CAE51713B9A56C31AA08EAAD665A0132F6C9CF227CF0' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DContextFacadeR25AssetFactory.cpp'; Bytes=41282L; Sha256='82376BEFB4261283466226A019C751E35B0FC59DA42FE5D9DEA0842A54ADD298' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h'; Bytes=4864L; Sha256='98FCDB85DB1B1F0A40656AFB86482BCF089945418F77AE69FFE0CCFF527022F2' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.cpp'; Bytes=3702L; Sha256='78C78D9C55900618E766FEFEC7F8555120D6B3C7AD598A7FC1E7656C4CC9EFEA' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DLocalFallbackSuppressionV2ProvenanceTests.cpp'; Bytes=3548L; Sha256='580831307282D8529258A485C64D336A01C11EAB86128111DD7A5A72BF372C03' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicView\Generated\SM_IstanaPublicView_Terrain.obj'; Bytes=3909911L; Sha256='78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23' }
)

$immutableContentPins = @(
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_ContextMassing_Master.uasset'; Bytes = 29065L; Sha256 = '47B0F9F804858280B53D4D03CEEA8E078F01740920AF589EFEA676EBC4D057B4' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRender.uasset'; Bytes = 15159L; Sha256 = '113CE217898FD5A65603493FFF82B4B0FC16C6AD3C0339097F61E78B911E326F' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRoof.uasset'; Bytes = 14843L; Sha256 = '4534461C849FD80A1D1BA34FE68239414B25F1FF31F4790BD857C9F828A8C943' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRender.uasset'; Bytes = 15170L; Sha256 = '227415242B7F0D0E61F3474C57D605848D24EF7574CD67C2242DED75B22F8079' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRoof.uasset'; Bytes = 14693L; Sha256 = 'BF11B2C73A066E03923B83AADD9C1222CD80377B44A90EBFAE868454C05898EB' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'; Bytes = 761010L; Sha256 = 'A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\M_IPV5D_ContextFacadeR25_Master.uasset'; Bytes = 29079L; Sha256 = '37C7305FC07DD076902A243410F7CB094356F8CBD333461E29EF3AC47694AC03' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialWall.uasset'; Bytes = 14637L; Sha256 = '2C783BA3D045FBC37432D117C372C1B6654B94ECDC3E9C8C051263E2492036DA' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialRoof.uasset'; Bytes = 14709L; Sha256 = '98DE68DFB79083DF8D690C0222BAED926F414302E840A8C88950FD73DCE729D8' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackWall.uasset'; Bytes = 14599L; Sha256 = 'CD27BC83E1876E53B485F055D5D1F2E48999EE8CEAC733D7AB55F06CC6E81C9B' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset'; Bytes = 14792L; Sha256 = 'E8F78D776EB235F1F25304678CCAD7F64DA125376BA04126DE2CE3BABCA64A18' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_Manicured.uasset'; Bytes = 41931L; Sha256 = 'C8FEBBA3C670695234A8D964095DD4741403094BBC4596DCEB84FA97CFAB3832' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_Humid.uasset'; Bytes = 41590L; Sha256 = '8524B7FA9C1FECD79CEE68369EC1C2DCE32A392D52C60B3BBDE4C451B4C6C442' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_Shade.uasset'; Bytes = 41555L; Sha256 = '8D67EC4810E0A8583640B5C430DC1418C9166BB583767F064455B4BC60BC6E3E' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_DryEdge.uasset'; Bytes = 42412L; Sha256 = '8C4B2264ED600A4CAE91641C42850737265F61FA610D0C17DDD0DCF8363DCCB2' }
)

$r28GrassMaterialRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials'))
$r28GrassMaterialRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_Manicured.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_Humid.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_Shade.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_DryEdge.uasset'
)
$r28EnvironmentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28'))
$r28EnvironmentRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\M_IPV5D_R28_Surface_Master.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_GlassCool.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_GlassWarm.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_FrameLight.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_FrameDark.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_RoofTrim.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_Asphalt.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_Curb.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_Sidewalk.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_Verge.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Materials\MI_IPV5D_R28_OuterGround.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Meshes\SM_IPV5D_R28_ConnectivePublicRealm_Render.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28\Meshes\SM_IPV5D_R28_ContextArchitecturalDressing_Render.uasset'
)
$r28NewAssetRelativePaths = @(
    @($r28GrassMaterialRelativePaths) + @($r28EnvironmentRelativePaths))
$r28NewContentRoots = @($r28GrassMaterialRoot, $r28EnvironmentRoot)

$globalBuildRelativePaths = @(
    'Intermediate\Build\SourceFileCache.bin'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.deps'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtpath'
    'Intermediate\Build\Win64\x64\UnrealEditor\ActionHistory.bin'
    'Intermediate\Build\Win64\x64\UnrealEditor\Development\DependencyCache.bin'
    'Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealEditor\Inc\AirSimTriadRuntime\UHT\Timestamp'
)

$r28DirectCompileObjectRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR28EnvironmentActor.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DContextPolicyActor.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR28EnvironmentAssetFactory.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DHybridEditorLibrary.cpp.obj'
)

# UHT may preserve unchanged generated-source bytes even with
# -ForceHeaderGeneration.  Invalidating the exact seven generated object files
# makes the subsequent build log and recreated object receipts causal proof
# that every required reflection translation unit reached the linker.
$r28GeneratedCompileObjectRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR28EnvironmentActor.gen.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DContextPolicyActor.gen.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.gen.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.gen.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp.obj'
)

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $candidate = [IO.Path]::GetFullPath($Path)
    $boundary = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparseAncestor {
    param([string] $Path, [string] $StopRoot)
    $cursor = [IO.DirectoryInfo] [IO.Path]::GetFullPath($Path)
    $stop = [IO.Path]::GetFullPath($StopRoot).TrimEnd('\')
    while ($null -ne $cursor) {
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse points are forbidden in transaction paths: $($cursor.FullName)"
        }
        if ($cursor.FullName.TrimEnd('\').Equals(
                $stop, [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
        $cursor = $cursor.Parent
    }
    throw "Path did not terminate at the expected root: $Path root=$StopRoot"
}

function Get-FileState {
    param([string] $Path)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($full)) {
        return [pscustomobject] [ordered] @{
            Path = $full; Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $full
    [pscustomobject] [ordered] @{
        Path = $full
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState -Path $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label pin mismatch: expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Assert-BinaryContainsEncodedMarkers {
    param(
        [string] $Path,
        [string[]] $Markers,
        [string] $Label
    )
    $bytes = [IO.File]::ReadAllBytes([IO.Path]::GetFullPath($Path))
    if ($bytes.Length -le 0) {
        throw "$Label is absent or empty: $Path"
    }
    # Unreal TEXT() literals are UTF-16LE on Windows, while reflection/symbol
    # strings can be narrow UTF-8/ASCII. Search exact encoded byte sequences,
    # not an ASCII-decoded DLL: the latter inserts NULs between TCHARs and
    # falsely rejects valid code. Hex substring matching is alignment agnostic
    # and remains valid when the linker pools a literal as part of a longer one.
    $binaryHex = [Convert]::ToHexString($bytes)
    foreach ($marker in $Markers) {
        $utf8Hex = [Convert]::ToHexString(
            [Text.Encoding]::UTF8.GetBytes($marker))
        $utf16LeHex = [Convert]::ToHexString(
            [Text.Encoding]::Unicode.GetBytes($marker))
        if (-not $binaryHex.Contains(
                $utf8Hex, [StringComparison]::Ordinal) -and
            -not $binaryHex.Contains(
                $utf16LeHex, [StringComparison]::Ordinal)) {
            throw "$Label lacks required encoded marker (UTF-8 or UTF-16LE): $marker"
        }
    }
}

function Assert-SourcePins {
    param([switch] $NativeBefore, [switch] $NativeAfter)
    foreach ($pin in $sourcePins) {
        $repo = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot `
            $pin.RelativePath))
        $expectedRepo = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expectedRepo $repo 'repository source')
        if ($NativeBefore) {
            $expectedNative = [pscustomobject] @{
                Present = $pin.NativeBeforePresent
                Bytes = $pin.NativeBeforeBytes
                Sha256 = $pin.NativeBeforeSha256
            }
            [void] (Assert-State $expectedNative `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native R27 source predecessor')
        }
        if ($NativeAfter) {
            [void] (Assert-State $expectedRepo `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'promoted native R28 source')
        }
    }
}

function Assert-SourceAssetPins {
    param([switch] $NativeBefore, [switch] $NativeAfter)
    foreach ($pin in $sourceAssetPins) {
        $repo = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot `
            $pin.RelativePath))
        $expectedRepo = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expectedRepo $repo 'repository R28 source asset')
        if ($NativeBefore) {
            $expectedNative = [pscustomobject] @{
                Present = $pin.NativeBeforePresent
                Bytes = $pin.NativeBeforeBytes
                Sha256 = $pin.NativeBeforeSha256
            }
            [void] (Assert-State $expectedNative `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native R27 source-asset predecessor')
        }
        if ($NativeAfter) {
            [void] (Assert-State $expectedRepo `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'promoted native R28 source asset')
        }
    }
}

function Assert-Phase2CompiledSourcePins {
    param([switch] $Native)
    foreach ($pin in $phase2CompiledSourcePins) {
        $expected = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expected `
            (Join-Path $repositoryUnrealRoot $pin.RelativePath) `
            'repository Phase-2 compiled source')
        if ($Native) {
            [void] (Assert-State $expected `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native committed Phase-2 compiled source')
        }
    }
}

function Assert-ImmutableAncestorSourcePins {
    param([switch] $Native)
    foreach ($pin in $immutableAncestorSourcePins) {
        $expected = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expected `
            (Join-Path $repositoryUnrealRoot $pin.RelativePath) `
            'repository immutable R25/V2/visual-terrain source')
        if ($Native) {
            [void] (Assert-State $expected `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native immutable R25/V2/visual-terrain source')
        }
    }
}

function Assert-ImmutableContent {
    foreach ($pin in $immutableContentPins) {
        $expected = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expected `
            (Join-Path $nativeProjectRoot $pin.RelativePath) `
            'immutable R27/R25/V2 content')
    }
}

function Get-R28NewAssetInventory {
    $inventory = [Collections.Generic.List[string]]::new()
    foreach ($root in $r28NewContentRoots) {
        if (-not [IO.Directory]::Exists($root)) { continue }
        foreach ($file in @(Get-ChildItem -LiteralPath $root -File -Recurse)) {
            $inventory.Add([IO.Path]::GetFullPath($file.FullName))
        }
    }
    @($inventory | Sort-Object -Unique)
}

function Assert-R28NewAssetPredecessor {
    $inventory = @(Get-R28NewAssetInventory)
    if ($inventory.Count -ne 0) {
        throw "R28 content predecessor must contain exactly zero files; actual=$([string]::Join(',', $inventory))"
    }
}

function Get-R28GrassSuccessorPins {
    $inventory = if ([IO.Directory]::Exists($r28GrassMaterialRoot)) {
        @(Get-ChildItem -LiteralPath $r28GrassMaterialRoot -File -Recurse |
            ForEach-Object { [IO.Path]::GetFullPath($_.FullName) } |
            Sort-Object -Unique)
    }
    else { @() }
    $expected = @($r28GrassMaterialRelativePaths | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
    } | Sort-Object)
    if (($inventory | ConvertTo-Json -Compress) -cne
        ($expected | ConvertTo-Json -Compress)) {
        throw "R28 grass successor must contain exactly four packages; actual=$([string]::Join(',', $inventory))"
    }
    @($expected | ForEach-Object {
        $state = Get-FileState $_
        if (-not $state.Present -or $state.Bytes -le 0 -or
            $state.Sha256.Length -ne 64) {
            throw "R28 grass successor package is absent or empty: $_"
        }
        $state
    })
}

function Assert-R28GrassPins {
    param([object[]] $Pins)
    if ($Pins.Count -ne 4) {
        throw 'R28 grass pin roster must contain exactly four files.'
    }
    foreach ($pin in $Pins) {
        [void] (Assert-State $pin $pin.Path 'R28 grass successor')
    }
    [void] (Get-R28GrassSuccessorPins)
}

function Get-R28EnvironmentInventory {
    if (-not [IO.Directory]::Exists($r28EnvironmentRoot)) { return @() }
    @(Get-ChildItem -LiteralPath $r28EnvironmentRoot -File -Recurse |
        ForEach-Object { [IO.Path]::GetFullPath($_.FullName) } |
        Sort-Object -Unique)
}

function Assert-R28EnvironmentPredecessor {
    $inventory = @(Get-R28EnvironmentInventory)
    if ($inventory.Count -ne 0) {
        throw "R28 environment predecessor must contain exactly zero files; actual=$([string]::Join(',', $inventory))"
    }
}

function Get-R28PromotionDirectoryInventory {
    $rows = [Collections.Generic.List[string]]::new()
    foreach ($root in @($r28NewContentRoots) + @($r28SourceAssetRoot)) {
        $fullRoot = [IO.Path]::GetFullPath($root)
        $cursor = $fullRoot
        while ($true) {
            $atNativeRoot = $cursor.TrimEnd('\').Equals(
                $nativeProjectRoot.TrimEnd('\'),
                [StringComparison]::OrdinalIgnoreCase)
            if (-not $atNativeRoot -and
                -not (Test-ContainedPath $cursor $nativeProjectRoot)) {
                throw "R28 promotion directory chain escaped native project: $cursor"
            }
            if ([IO.Directory]::Exists($cursor)) {
                $rows.Add($cursor)
            }
            if ($atNativeRoot) { break }
            $parent = [IO.Path]::GetDirectoryName($cursor)
            if ([string]::IsNullOrWhiteSpace($parent) -or
                $parent.Equals($cursor, [StringComparison]::OrdinalIgnoreCase)) {
                throw "R28 promotion directory chain did not reach native project: $fullRoot"
            }
            $cursor = [IO.Path]::GetFullPath($parent)
        }
        if ([IO.Directory]::Exists($fullRoot)) {
            foreach ($directory in @(Get-ChildItem -LiteralPath $fullRoot `
                    -Directory -Recurse)) {
                $rows.Add([IO.Path]::GetFullPath($directory.FullName))
            }
        }
    }
    @($rows | Sort-Object -Unique)
}

function Restore-R28PromotionDirectories {
    param([string[]] $Before)
    $beforeSet = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $Before) { [void] $beforeSet.Add($path) }
    $current = @(Get-R28PromotionDirectoryInventory |
        Sort-Object { $_.Length } -Descending)
    foreach ($path in $current) {
        if ($beforeSet.Contains($path)) { continue }
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "R28 directory rollback target escaped native project: $path"
        }
        $item = Get-Item -LiteralPath $path -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "R28 directory rollback refused reparse point: $path"
        }
        # Non-recursive deletion is intentionally fail-closed if an
        # unjournalled file or directory remains.
        [IO.Directory]::Delete($path, $false)
    }
    $after = @(Get-R28PromotionDirectoryInventory)
    if (($Before | Sort-Object | ConvertTo-Json -Compress) -cne
        ($after | Sort-Object | ConvertTo-Json -Compress)) {
        throw 'R28 promotion-directory rollback did not restore the exact predecessor inventory.'
    }
}

function Get-R28NewAssetSuccessorPins {
    $inventory = @(Get-R28NewAssetInventory)
    $expected = @($r28NewAssetRelativePaths | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
    } | Sort-Object)
    if (($inventory | ConvertTo-Json -Compress) -cne
        ($expected | ConvertTo-Json -Compress)) {
        throw "R28 content successor must contain exactly seventeen package files; actual=$([string]::Join(',', $inventory))"
    }
    @($expected | ForEach-Object {
        $state = Get-FileState $_
        if (-not $state.Present -or $state.Bytes -le 0 -or
            $state.Sha256.Length -ne 64 -or
            [IO.Path]::GetExtension($_) -cne '.uasset') {
            throw "R28 successor package is absent, empty, or not a uasset: $_"
        }
        $state
    })
}

function Assert-R28NewAssetPins {
    param([object[]] $Pins)
    if ($Pins.Count -ne 17) {
        throw 'R28 new-package pin roster must contain exactly seventeen files.'
    }
    foreach ($pin in $Pins) {
        [void] (Assert-State $pin $pin.Path 'R28 package successor')
    }
    [void] (Get-R28NewAssetSuccessorPins)
}

function Get-SystemFreeVirtualMemoryBytes {
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    if ($null -eq $os -or $null -eq $os.FreeVirtualMemory) {
        throw 'Win32_OperatingSystem did not provide FreeVirtualMemory.'
    }
    [int64] $os.FreeVirtualMemory * 1KB
}

function Assert-LaunchAdmission {
    param([string] $Checkpoint)
    $freeVirtualBytes = Get-SystemFreeVirtualMemoryBytes
    if ($freeVirtualBytes -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "Fixed 10 GiB FreeVirtualMemory launch admission refused ${Checkpoint}: available=$freeVirtualBytes required=$minimumSystemFreeVirtualAtLaunchBytes"
    }
    [pscustomobject] [ordered] @{
        Checkpoint = $Checkpoint
        FreeVirtualMemoryBytes = $freeVirtualBytes
        RequiredFreeVirtualMemoryBytes =
            $minimumSystemFreeVirtualAtLaunchBytes
    }
}

function Get-ProcessRecord {
    param([uint32] $ProcessId)
    $cim = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" `
        -ErrorAction Stop
    if ($null -eq $cim) { return $null }
    $exe = [string] $cim.ExecutablePath
    $commandLine = [string] $cim.CommandLine
    if ([string]::IsNullOrWhiteSpace($exe) -or
        [string]::IsNullOrWhiteSpace($commandLine) -or
        $null -eq $cim.CreationDate) {
        # Win32_Process can be observable before these identity fields are
        # populated. Callers poll instead of treating this partial row as an
        # owned process identity.
        return $null
    }
    $creation = [DateTime] $cim.CreationDate
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $ProcessId
        ExecutablePath = [IO.Path]::GetFullPath($exe)
        CommandLine = $commandLine
        StartTimeUtcTicks = $creation.ToUniversalTime().Ticks
    }
}

function Test-Win32ProcessPresent {
    param([uint32] $ProcessId)
    $null -ne (Get-CimInstance Win32_Process `
        -Filter "ProcessId=$ProcessId" -ErrorAction Stop)
}

function Test-ProcessRecordEqual {
    param($Expected, $Actual)
    $null -ne $Expected -and $null -ne $Actual -and
        [uint32] $Actual.ProcessId -eq [uint32] $Expected.ProcessId -and
        [int64] $Actual.StartTimeUtcTicks -eq
            [int64] $Expected.StartTimeUtcTicks -and
        ([string] $Actual.ExecutablePath).Equals(
            [string] $Expected.ExecutablePath,
            [StringComparison]::OrdinalIgnoreCase) -and
        ([string] $Actual.CommandLine).Equals(
            [string] $Expected.CommandLine,
            [StringComparison]::Ordinal)
}

function Get-ProtectedSnapshot {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($cim in @(Get-CimInstance Win32_Process -Filter `
            "Name='UnrealEditor.exe'" -ErrorAction Stop)) {
        if ([string]::IsNullOrWhiteSpace([string] $cim.ExecutablePath) -or
            -not ([IO.Path]::GetFullPath([string] $cim.ExecutablePath).Equals(
                $protectedEditor, [StringComparison]::OrdinalIgnoreCase)) -or
            -not ([string] $cim.CommandLine).Contains(
                $protectedProject, [StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $record = Get-ProcessRecord -ProcessId ([uint32] $cim.ProcessId)
        if ($null -ne $record) { $rows.Add($record) }
    }
    @($rows | Sort-Object ProcessId)
}

function Assert-ProtectedUnchanged {
    param([object[]] $Expected)
    $actual = @(Get-ProtectedSnapshot)
    if (($Expected | ConvertTo-Json -Depth 6 -Compress) -cne
        ($actual | ConvertTo-Json -Depth 6 -Compress)) {
        throw 'The protected UE5.4 CAPSTONE process identity set changed.'
    }
}

function Get-RcOwners {
    # Querying all listeners makes a successful zero-row port filter distinct
    # from a failed authority query. Cleanup and rollback must fail closed if
    # the TCP ownership provider is unavailable.
    @((Get-NetTCPConnection -State Listen -ErrorAction Stop |
        Where-Object { [uint16] $_.LocalPort -eq 30010 } |
        Select-Object -ExpandProperty OwningProcess -Unique))
}

function Assert-NativeIdle {
    param([string] $Checkpoint)
    $offenders = [Collections.Generic.List[string]]::new()
    foreach ($cim in @(Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object { $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') })) {
        $exe = [string] $cim.ExecutablePath
        $cmd = [string] $cim.CommandLine
        if ([string]::IsNullOrWhiteSpace($exe) -or
            [string]::IsNullOrWhiteSpace($cmd)) {
            $offenders.Add(
                "pid=$($cim.ProcessId) exe=<identity-incomplete> cmd=<identity-incomplete>")
        }
        elseif (([IO.Path]::GetFullPath($exe).StartsWith(
                $engineRoot, [StringComparison]::OrdinalIgnoreCase)) -or
            $cmd.Contains($nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)) {
            $offenders.Add("pid=$($cim.ProcessId) exe=$exe cmd=$cmd")
        }
    }
    if ($offenders.Count -ne 0 -or @(Get-RcOwners).Count -ne 0) {
        throw "Native Unreal/RC boundary is not idle at ${Checkpoint}: $([string]::Join(' | ', @($offenders))) rc=$([string]::Join(',', @(Get-RcOwners)))"
    }
}

function Invoke-RcCall {
    param([string] $ObjectPath, [string] $FunctionName,
        [hashtable] $Parameters = @{}, [int] $TimeoutSec = 60)
    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' `
        -Body $payload -TimeoutSec $TimeoutSec
}

function Assert-HelperIdentity {
    param($Expected, [Diagnostics.Process] $Handle)
    $actual = Get-ProcessRecord -ProcessId ([uint32] $Handle.Id)
    if (-not (Test-ProcessRecordEqual $Expected $actual) -or
        [uint32] $Handle.Id -ne [uint32] $Expected.ProcessId -or
        -not $actual.ExecutablePath.Equals(
            $editor, [StringComparison]::OrdinalIgnoreCase) -or
        -not $actual.CommandLine.Contains(
            $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Owned Unreal helper identity changed.'
    }
    $actual
}

function Wait-ExpectedHelperIdentity {
    param(
        [Diagnostics.Process] $Handle,
        [string] $MapPackage,
        [string] $Log,
        [int64] $LaunchStartTimeUtcTicks,
        [int] $TimeoutSeconds = 20
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $candidate = $null
        try {
            $candidate = Get-ProcessRecord -ProcessId ([uint32] $Handle.Id)
        }
        catch {
            # Process identity fields can be transiently unavailable during
            # editor startup. Keep the exact launched PID under observation.
        }
        if ($null -ne $candidate -and
            [uint32] $candidate.ProcessId -eq [uint32] $Handle.Id -and
            ($LaunchStartTimeUtcTicks -le 0 -or
             [Math]::Abs(
                [int64] $candidate.StartTimeUtcTicks -
                $LaunchStartTimeUtcTicks) -le
                    [TimeSpan]::FromSeconds(2).Ticks) -and
            $candidate.ExecutablePath.Equals(
                $editor, [StringComparison]::OrdinalIgnoreCase) -and
            $candidate.CommandLine.Contains(
                $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -and
            $candidate.CommandLine.Contains(
                $MapPackage, [StringComparison]::Ordinal) -and
            $candidate.CommandLine.Contains(
                $Log, [StringComparison]::OrdinalIgnoreCase)) {
            return $candidate
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    $null
}

function Wait-ExactHelperExit {
    param(
        $Identity,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $actual = Get-ProcessRecord -ProcessId ([uint32] $Identity.ProcessId)
        $owners = @(Get-RcOwners)
        if ($null -eq $actual) {
            if (-not (Test-Win32ProcessPresent `
                        ([uint32] $Identity.ProcessId)) -and
                $owners -notcontains [uint32] $Identity.ProcessId) {
                return $true
            }
        }
        elseif (-not (Test-ProcessRecordEqual $Identity $actual)) {
            throw 'PID reuse or exact helper identity drift occurred while waiting for exit.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Wait-LaunchedHelperBoundaryReleased {
    param(
        [uint32] $LaunchProcessId,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $present = Test-Win32ProcessPresent $LaunchProcessId
        $owners = @(Get-RcOwners)
        if (-not $present -and $owners.Count -eq 0) {
            return $true
        }
        if ($owners.Count -ne 0 -and
            $owners -notcontains $LaunchProcessId) {
            throw 'A foreign RC owner appeared while containing the launched helper.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Stop-LaunchedHelperBeforeIdentity {
    param(
        [Diagnostics.Process] $Handle,
        [uint32] $LaunchProcessId,
        [int64] $LaunchStartTimeUtcTicks
    )
    if ([uint32] $Handle.Id -ne $LaunchProcessId) {
        throw 'Returned process handle no longer matches its launch PID receipt.'
    }
    $handleStartTicks = $Handle.StartTime.ToUniversalTime().Ticks
    if ($LaunchStartTimeUtcTicks -gt 0 -and
        [int64] $handleStartTicks -ne $LaunchStartTimeUtcTicks) {
        throw 'Returned process handle no longer matches its launch-time receipt.'
    }
    # Identity never became queryable, so RC and PID-based termination are
    # forbidden. Kill only through the exact Process object returned by this
    # wrapper's Start-Process call, then use fail-closed Win32/RC absence as
    # the authoritative containment boundary.
    $Handle.Kill()
    if (-not (Wait-LaunchedHelperBoundaryReleased $LaunchProcessId 30)) {
        throw 'Unidentified launched helper did not exit after handle containment.'
    }
}

function Stop-OwnedHelper {
    param($Identity, [Diagnostics.Process] $Handle)
    if ([uint32] $Handle.Id -ne [uint32] $Identity.ProcessId) {
        throw 'Process handle does not match the exact owned Unreal helper.'
    }
    $actual = Get-ProcessRecord -ProcessId ([uint32] $Identity.ProcessId)
    if ($null -eq $actual) {
        if ((Test-Win32ProcessPresent ([uint32] $Identity.ProcessId)) -or
            @(Get-RcOwners).Count -ne 0) {
            throw 'Owned helper identity is incomplete or its RC listener remains.'
        }
        return
    }
    [void] (Assert-HelperIdentity $Identity $Handle)

    # A stage can fail before RC finishes starting. Give only the exact owned
    # PID a bounded chance to expose port 30010, then request graceful exit.
    $quitRequested = $false
    $gracefulDeadline = [DateTime]::UtcNow.AddSeconds(
        [Math]::Min(60, $ShutdownTimeoutSeconds))
    do {
        [void] (Assert-HelperIdentity $Identity $Handle)
        $owners = @(Get-RcOwners)
        if ($owners.Count -eq 1 -and
            [uint32] $owners[0] -eq [uint32] $Identity.ProcessId) {
            try {
                [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30)
            }
            catch {
                # The socket can close before QuitEditor returns.
            }
            $quitRequested = $true
            break
        }
        if ($owners.Count -ne 0) {
            throw 'RC port ownership changed before exact helper shutdown.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $gracefulDeadline)

    if ($quitRequested -and
        (Wait-ExactHelperExit $Identity $ShutdownTimeoutSeconds)) {
        if (@(Get-RcOwners).Count -ne 0) {
            throw 'RC listener remained after exact helper graceful exit.'
        }
        return
    }

    # Forced containment is permitted only after re-proving PID, creation
    # ticks, executable, and full command line. Never trust HasExited or
    # WaitForExit alone; Win32 and RC are the authoritative exit boundaries.
    [void] (Assert-HelperIdentity $Identity $Handle)
    $Handle.Kill()
    if (-not (Wait-ExactHelperExit $Identity 30)) {
        throw 'Exact owned Unreal helper did not exit after containment.'
    }
    if (@(Get-RcOwners).Count -ne 0) {
        throw 'RC listener remained after exact helper containment.'
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $ObjectPath,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix,
        [hashtable] $Parameters = @{}
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableAncestorSourcePins -Native
    Assert-ImmutableContent
    if (@($script:r28GrassPins).Count -ne 0) {
        Assert-R28GrassPins $script:r28GrassPins
    }
    if (@($script:r28NewAssetPins).Count -ne 0) {
        Assert-R28NewAssetPins $script:r28NewAssetPins
    }
    $logDir = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logDir)
    $log = [IO.Path]::GetFullPath((Join-Path $logDir "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine =
        "`"$nativeProjectFile`" $mapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-DDC=InstalledNoZenLocalFallback ' +
        "-LocalDataCachePath=`"$ddcRoot`" " +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $launchAdmission = Assert-LaunchAdmission "before helper $Stage"
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    # Register the returned process handle immediately. If identity discovery
    # fails during the startup race, both this stage and the outer transaction
    # finally block still retain an exact cleanup target.
    $script:activeHelperHandle = $handle
    $script:activeHelperIdentity = $null
    $script:activeHelperMapPackage = $mapPackage
    $script:activeHelperLog = $log
    $launchProcessId = [uint32] $handle.Id
    $script:activeHelperLaunchProcessId = $launchProcessId
    $launchStartTimeUtcTicks = 0L
    try {
        $launchStartTimeUtcTicks =
            $handle.StartTime.ToUniversalTime().Ticks
    }
    catch {
        # The unique run-token log path still binds identity while Win32
        # CreationDate is acquired by the bounded identity poll below.
    }
    $script:activeHelperLaunchStartTimeUtcTicks =
        $launchStartTimeUtcTicks
    $identity = $null
    $response = $null
    $stageError = $null
    try {
        $identity = Wait-ExpectedHelperIdentity `
            -Handle $handle -MapPackage $mapPackage -Log $log `
            -LaunchStartTimeUtcTicks $launchStartTimeUtcTicks `
            -TimeoutSeconds 20
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }
        $script:activeHelperIdentity = $identity

        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        $identityResponse = $null
        do {
            Assert-ProtectedUnchanged $script:protectedBefore
            [void] (Assert-HelperIdentity $identity $handle)
            $owners = @(Get-RcOwners)
            if ($owners.Count -eq 1 -and
                [uint32] $owners[0] -eq [uint32] $identity.ProcessId) {
                try {
                    $identityResponse = Invoke-RcCall $identityLibrary `
                        'ValidateIstanaExploreRemoteControlProject' `
                        @{ ExpectedProjectPath = $nativeProjectRoot } 30
                }
                catch { $identityResponse = $null }
                if ($null -ne $identityResponse -and
                    $identityResponse.ReturnValue -eq $true) { break }
            }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $identityResponse -or
            $identityResponse.ReturnValue -ne $true) {
            throw "Could not prove exact project/RC ownership for $Stage"
        }
        [void] (Assert-HelperIdentity $identity $handle)
        $response = Invoke-RcCall $ObjectPath $FunctionName $Parameters `
            $EditorTimeoutSeconds
        [void] (Assert-HelperIdentity $identity $handle)
        $text = [string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or
            -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: $FunctionName response=$text"
        }
    }
    catch { $stageError = $_.Exception }
    finally {
        if ($null -eq $identity) {
            try {
                # A startup error must not make the exact process unowned.
                # Re-prove all Win32 identity fields before any shutdown call.
                $identity = Wait-ExpectedHelperIdentity `
                    -Handle $handle -MapPackage $mapPackage -Log $log `
                    -LaunchStartTimeUtcTicks $launchStartTimeUtcTicks `
                    -TimeoutSeconds 30
                if ($null -eq $identity) {
                    throw 'Could not recover exact helper identity during cleanup.'
                }
                $script:activeHelperIdentity = $identity
            }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
            }
        }
        if ($null -ne $identity) {
            try { Stop-OwnedHelper $identity $handle }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else {
                    $stageError = [InvalidOperationException]::new(
                        "Stage error={$($stageError.Message)}; exact helper cleanup error={$($_.Exception.Message)}")
                }
            }
        }
        # Even a fully proven identity can become temporarily unavailable
        # inside Stop-OwnedHelper. If its graceful/re-proven path throws while
        # the launch PID remains, contain through the immutable returned
        # handle receipt before releasing rollback.
        try {
            if (Test-Win32ProcessPresent $launchProcessId) {
                Stop-LaunchedHelperBeforeIdentity `
                    -Handle $handle `
                    -LaunchProcessId $launchProcessId `
                    -LaunchStartTimeUtcTicks $launchStartTimeUtcTicks
            }
            elseif (@(Get-RcOwners).Count -ne 0) {
                throw 'RC listener remains after launched helper exit.'
            }
        }
        catch {
            if ($null -eq $stageError) { $stageError = $_.Exception }
            else {
                $stageError = [InvalidOperationException]::new(
                    "Stage error={$($stageError.Message)}; launch-handle fallback containment error={$($_.Exception.Message)}")
            }
        }
    }
    $postCleanupErrors = [Collections.Generic.List[string]]::new()
    try { Assert-ProtectedUnchanged $script:protectedBefore }
    catch { $postCleanupErrors.Add($_.Exception.Message) }
    try { Assert-NativeIdle "after $Stage" }
    catch { $postCleanupErrors.Add($_.Exception.Message) }
    try {
        if ((Test-Win32ProcessPresent $launchProcessId) -or
            @(Get-RcOwners).Count -ne 0) {
            throw 'Exact launched helper/RC boundary remains after stage cleanup.'
        }
    }
    catch { $postCleanupErrors.Add($_.Exception.Message) }
    if ($postCleanupErrors.Count -ne 0) {
        $original = if ($null -eq $stageError) { 'none' }
            else { $stageError.Message }
        throw "Stage $Stage cleanup boundary failed: original={$original} cleanup={$([string]::Join(' | ', @($postCleanupErrors)))}"
    }
    $script:activeHelperHandle = $null
    $script:activeHelperIdentity = $null
    $script:activeHelperMapPackage = $null
    $script:activeHelperLog = $null
    $script:activeHelperLaunchProcessId = 0
    $script:activeHelperLaunchStartTimeUtcTicks = 0L
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item $log).Length -le 0) {
        throw "Stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        Function = $FunctionName
        ExpectedPrefix = $ExpectedPrefix
        Message = [string] $response.$TextProperty
        ProcessIdentity = $identity
        LaunchAdmission = $launchAdmission
        Log = Get-FileState $log
    }
}

function Get-BuildSurfacePaths {
    $paths = [Collections.Generic.List[string]]::new()
    foreach ($root in @($pluginBinaryRoot, $pluginIntermediateRoot)) {
        if ([IO.Directory]::Exists($root)) {
            foreach ($file in @(Get-ChildItem -LiteralPath $root -File -Recurse)) {
                $paths.Add([IO.Path]::GetFullPath($file.FullName))
            }
        }
    }
    foreach ($relative in $globalBuildRelativePaths) {
        $paths.Add([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
    }
    @($paths | Sort-Object -Unique)
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
            Path = $path
            RelativePath = $relative
            Before = $state
            Backup = if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Remove-R28DirectCompileObjectsForForcedRebuild {
    param([object[]] $BuildRows)
    if ($r28DirectCompileObjectRelativePaths.Count -ne 8) {
        throw 'R28 direct compile-object roster must contain eight translation units.'
    }
    $removed = [Collections.Generic.List[object]]::new()
    foreach ($relative in $r28DirectCompileObjectRelativePaths) {
        $matches = @($BuildRows | Where-Object {
                [string] $_.RelativePath -ceq [string] $relative
            })
        if ($matches.Count -ne 1) {
            throw "R28 direct compile object lacks one exact rollback row: $relative"
        }
        $row = $matches[0]
        [void] (Assert-State $row.Before $row.Path `
            'R28 direct compile object predecessor')
        if ([IO.File]::Exists($row.Path)) {
            $item = Get-Item -LiteralPath $row.Path -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "R28 direct compile object is a reparse file: $($row.Path)"
            }
            Remove-Item -LiteralPath $row.Path -Force
        }
        $absent = [pscustomobject] @{
            Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
        [void] (Assert-State $absent $row.Path `
            'R28 forced compile-object invalidation')
        $removed.Add($row)
    }
    @($removed)
}

function Remove-R28GeneratedCompileObjectsForForcedRebuild {
    param([object[]] $BuildRows)
    if ($r28GeneratedCompileObjectRelativePaths.Count -ne 7) {
        throw 'R28 generated compile-object roster must contain seven reflection translation units.'
    }
    $removed = [Collections.Generic.List[object]]::new()
    foreach ($relative in $r28GeneratedCompileObjectRelativePaths) {
        $matches = @($BuildRows | Where-Object {
                [string] $_.RelativePath -ceq [string] $relative
            })
        if ($matches.Count -ne 1) {
            throw "R28 generated compile object lacks one exact rollback row: $relative"
        }
        $row = $matches[0]
        [void] (Assert-State $row.Before $row.Path `
            'R28 generated compile object predecessor')
        if ([IO.File]::Exists($row.Path)) {
            $item = Get-Item -LiteralPath $row.Path -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "R28 generated compile object is a reparse file: $($row.Path)"
            }
            Remove-Item -LiteralPath $row.Path -Force
        }
        $absent = [pscustomobject] @{
            Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
        [void] (Assert-State $absent $row.Path `
            'R28 forced generated compile-object invalidation')
        $removed.Add($row)
    }
    @($removed)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows | Sort-Object RelativePath -Descending)) {
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
        [void] (Assert-State $row.Before $row.Path 'rollback state')
    }
}

function Restore-BuildSurface {
    param([object[]] $Rows)
    $before = @{}
    foreach ($row in $Rows) { $before[$row.Path.ToLowerInvariant()] = $true }
    foreach ($path in @(Get-BuildSurfacePaths)) {
        if (-not $before.ContainsKey($path.ToLowerInvariant()) -and
            [IO.File]::Exists($path)) {
            if (-not (Test-ContainedPath $path $pluginBinaryRoot) -and
                -not (Test-ContainedPath $path $pluginIntermediateRoot) -and
                -not ($globalBuildRelativePaths -contains
                    [IO.Path]::GetRelativePath($nativeProjectRoot, $path))) {
                throw "Rollback refused non-allowlisted new build file: $path"
            }
            Remove-Item -LiteralPath $path -Force
        }
    }
    Restore-FileJournal $Rows
}

function Restore-MapFromJournal {
    param($MapRow)
    $temporary = [IO.Path]::GetFullPath((Join-Path `
        ([IO.Path]::GetDirectoryName($mapFile)) `
        ("TRIAD_R28_Wrapper_Restore_{0}.tmp" -f [Guid]::NewGuid().ToString('N'))))
    if (-not (Test-ContainedPath $temporary `
            ([IO.Path]::GetDirectoryName($mapFile)))) {
        throw 'Map rollback temporary escaped the map directory.'
    }
    Copy-Item -LiteralPath $MapRow.Backup -Destination $temporary
    [void] (Assert-State $MapRow.Before $temporary 'map rollback temporary')
    Move-Item -LiteralPath $temporary -Destination $mapFile -Force
    [void] (Assert-State $MapRow.Before $mapFile 'restored R27 map')
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 16) {
        throw 'R28 code-source roster must contain exactly sixteen files.'
    }
    if ($sourceAssetPins.Count -ne 5) {
        throw 'R28 source-asset roster must contain exactly five files.'
    }
    if ($phase2CompiledSourcePins.Count -ne 9) {
        throw 'Phase-2 compiled-source closure must contain nine files.'
    }
    if ($immutableAncestorSourcePins.Count -ne 6) {
        throw 'Immutable R25/V2 and exact visual-terrain source closure must contain six files.'
    }
    if (@($sourcePins | Where-Object NativeBeforePresent).Count -ne 10 -or
        @($sourcePins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 6) {
        throw 'R27 code predecessor must contain ten present and six absent destinations.'
    }
    if (@($sourceAssetPins | Where-Object NativeBeforePresent).Count -ne 0) {
        throw 'All five R28 source-asset destinations must be exact-absence predecessors.'
    }
    if ($r28DirectCompileObjectRelativePaths.Count -ne 8) {
        throw 'R28 direct compile-object roster must contain eight files.'
    }
    if ($r28GrassMaterialRelativePaths.Count -ne 4 -or
        $r28EnvironmentRelativePaths.Count -ne 13 -or
        $r28NewAssetRelativePaths.Count -ne 17) {
        throw 'R28 new content roster must be exactly 4 grass + 13 environment packages.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        if (-not $seen.Add($pin.RelativePath)) {
            throw "Duplicate R28 promotion path: $($pin.RelativePath)"
        }
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Non-canonical R28 promotion path: $($pin.RelativePath)"
        }
    }
    Assert-SourcePins
    Assert-SourceAssetPins
    Assert-Phase2CompiledSourcePins
    Assert-ImmutableAncestorSourcePins
    $header = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h') -Raw
    $assetHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h') -Raw
    $source = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp') -Raw
    $contextPolicySource = Get-Content -LiteralPath (Join-Path `
        $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp') -Raw
    $combinedSourceClosure = $source + [Environment]::NewLine + `
        $contextPolicySource
    $environmentHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h') -Raw
    $environmentSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp') -Raw
    foreach ($endpoint in @(
        'ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap',
        'ValidateIstanaExploreV5DR28VisualSuccessorMap',
        'ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld',
        'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport',
        'SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa',
        'CaptureIstanaExploreV5DR28VisualSuccessorPlayView',
        'CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView')) {
        if (-not $header.Contains($endpoint, [StringComparison]::Ordinal) -or
            -not $source.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "Missing combined R28 endpoint in pinned repo source: $endpoint"
        }
    }
    foreach ($endpoint in @(
        'BuildOrValidateLandmarkGrassMaterialsR28',
        'ValidateLandmarkGrassMaterialsR28',
        'ValidateReusableLandmarkVegetationAssetsR28',
        'ConfigureLandmarkVegetationActorR28')) {
        if (-not $assetHeader.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "Missing R28 landmark endpoint in pinned repo source: $endpoint"
        }
    }
    foreach ($endpoint in @(
        'EnsureR28EnvironmentAssets',
        'ValidateR28EnvironmentAssets',
        'ApplyR28EnvironmentToLoadedV5DHybridMap',
        'ValidateR28EnvironmentInLoadedV5DHybridMap')) {
        if (-not $environmentHeader.Contains(
                $endpoint, [StringComparison]::Ordinal) -or
            -not $environmentSource.Contains(
                $endpoint, [StringComparison]::Ordinal)) {
            throw "Missing R28 environment endpoint in pinned repo source: $endpoint"
        }
    }
    foreach ($marker in @(
        'V5DVisualRealismR28V1',
        'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_VALID',
        'EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS',
        'IDEMPOTENT_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_ALREADY_VALID',
        'providerCollisionNavigationSensorRfAuthority=false',
        'r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false',
        'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false',
        'providerTokenReadSerializedOrLogged=false',
        'providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false',
        'visualCaptureAccepted=false',
        'captureRevalidationRequired=true')) {
        if (-not $combinedSourceClosure.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "Missing combined R28 marker in pinned repo source closure: $marker"
        }
    }
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'STATIC_SELF_CHECK_PASS'
        CodeSourceCount = $sourcePins.Count
        SourceAssetSpecCount = $sourceAssetPins.Count
        Phase2CompiledSourceCount = $phase2CompiledSourcePins.Count
        ImmutableAncestorSourceCount = $immutableAncestorSourcePins.Count
        DirectCompileObjectCount = $r28DirectCompileObjectRelativePaths.Count
        GeneratedCompileObjectCount =
            $r28GeneratedCompileObjectRelativePaths.Count
        ExactlyNewAssetCount = $r28NewAssetRelativePaths.Count
        R28GrassPackageCount = $r28GrassMaterialRelativePaths.Count
        R28EnvironmentPackageCount = $r28EnvironmentRelativePaths.Count
        MinimumSystemFreeVirtualAtLaunchBytes =
            $minimumSystemFreeVirtualAtLaunchBytes
        NativeTreeReadOrWritten = $false
        UnrealBuildOrEditorLaunched = $false
        PredecessorMap = $expectedMapPin
        PredecessorRuntimeDll = $expectedRuntimeDllPin
        PredecessorEditorDll = $expectedEditorDllPin
        LandmarkGrassInstances = 6144
        LandmarkGrassMaximumPerSite = 4096
        R28MatureTemasekCanopyConfigured = $true
        CollisionNavigationSensorRfTerrainAuthority = $false
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
}

$static = Assert-StaticContract
if ($StaticSelfCheck) {
    $static | ConvertTo-Json -Depth 12
    return
}

if ($Execute) {
    foreach ($name in @(
            'ExpectedMapBytes', 'ExpectedMapSha256',
            'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
            'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256')) {
        if (-not $PSBoundParameters.ContainsKey($name)) {
            throw "Live R28 execution requires explicit caller-supplied -$name from the committed R27 receipt."
        }
    }
    if (-not $RequireR27Predecessor) {
        throw 'Live R28 execution requires -RequireR27Predecessor.'
    }
}

foreach ($directory in @(
        $repositoryUnrealRoot,
        $nativeProjectRoot,
        $engineRoot,
        [IO.Path]::GetDirectoryName($transactionBase))) {
    if (-not [IO.Directory]::Exists($directory)) {
        throw "Required directory is absent: $directory"
    }
}
Assert-NoReparseAncestor $nativeProjectRoot ([IO.Path]::GetPathRoot($nativeProjectRoot))
Assert-NoReparseAncestor ([IO.Path]::GetDirectoryName($transactionBase)) `
    $nativeProjectRoot
foreach ($root in @($r28NewContentRoots) + @($r28SourceAssetRoot)) {
    $existingBoundary = $root
    while (-not [IO.Directory]::Exists($existingBoundary)) {
        $parent = [IO.Path]::GetDirectoryName($existingBoundary)
        if ([string]::IsNullOrWhiteSpace($parent) -or
            -not (Test-ContainedPath $parent $nativeProjectRoot)) {
            throw "R28 content root lacks a contained existing ancestor: $root"
        }
        $existingBoundary = $parent
    }
    Assert-NoReparseAncestor $existingBoundary $nativeProjectRoot
}
$script:protectedBefore = @(Get-ProtectedSnapshot)
Assert-ProtectedUnchanged $script:protectedBefore
Assert-NativeIdle 'preflight'
[void] (Assert-State $expectedProjectPin $nativeProjectFile 'project')
[void] (Assert-State $expectedMapPin $mapFile 'exact R27 predecessor map')
[void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'exact R27 runtime DLL')
[void] (Assert-State $expectedEditorDllPin $editorDll 'exact R27 editor DLL')
Assert-SourcePins -NativeBefore
Assert-SourceAssetPins -NativeBefore
Assert-Phase2CompiledSourcePins -Native
Assert-ImmutableAncestorSourcePins -Native
Assert-ImmutableContent
Assert-R28NewAssetPredecessor
if ([IO.Directory]::Exists($transactionRoot) -or
    [IO.File]::Exists($transactionRoot)) {
    throw "Transaction path already exists and will not be reused: $transactionRoot"
}

$preflight = [pscustomobject] [ordered] @{
    Schema = $schema
    Status = if ($Execute) { 'EXECUTION_PREFLIGHT_PASS' } else { 'READ_ONLY_PREFLIGHT_PASS' }
    ExecuteRequested = [bool] $Execute
    CodeSourceCount = $sourcePins.Count
    SourceAssetSpecCount = $sourceAssetPins.Count
    Phase2CompiledSourceCount = $phase2CompiledSourcePins.Count
    ImmutableAncestorSourceCount = $immutableAncestorSourcePins.Count
    DirectCompileObjectCount = $r28DirectCompileObjectRelativePaths.Count
    GeneratedCompileObjectCount =
        $r28GeneratedCompileObjectRelativePaths.Count
    PredecessorMap = Get-FileState $mapFile
    PredecessorRuntimeDll = Get-FileState $runtimeDll
    PredecessorEditorDll = Get-FileState $editorDll
    ImmutableContentCount = $immutableContentPins.Count
    R28NewAssetPredecessorCount = @(Get-R28NewAssetInventory).Count
    ExactlyNewAssetTargetCount = $r28NewAssetRelativePaths.Count
    MinimumSystemFreeVirtualAtLaunchBytes =
        $minimumSystemFreeVirtualAtLaunchBytes
    ProtectedUE54Sessions = @($script:protectedBefore)
    NativeTreeWritten = $false
    UnrealBuildOrEditorLaunched = $false
}
if (-not $Execute) {
    $preflight | ConvertTo-Json -Depth 12
    return
}

$prewriteAdmission = Assert-LaunchAdmission `
    'before transaction directory and first native write'
$r28PromotionDirectoriesBefore = @(Get-R28PromotionDirectoryInventory)
[void] [IO.Directory]::CreateDirectory($transactionRoot)
Assert-NoReparseAncestor $transactionRoot $nativeProjectRoot
$prewriteAdmissionReceiptPath = Join-Path $transactionRoot `
    'prewrite-admission.json'
$prewriteAdmissionReceipt = [pscustomobject] [ordered] @{
    Schema = $schema
    Status = 'PREWRITE_ADMISSION_PASS'
    RunToken = $RunToken
    Admission = $prewriteAdmission
    NativeMutationStarted = $false
}
$prewriteAdmissionReceipt | ConvertTo-Json -Depth 8 | Set-Content `
    -LiteralPath $prewriteAdmissionReceiptPath -Encoding utf8NoBOM -NoNewline
$prewriteAdmissionReceiptPin = Get-FileState $prewriteAdmissionReceiptPath
$journalRoot = Join-Path $transactionRoot 'journal'
[void] [IO.Directory]::CreateDirectory($journalRoot)
Assert-NoReparseAncestor $journalRoot $nativeProjectRoot
$sourcePaths = @($sourcePins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
})
$sourceAssetPaths = @($sourceAssetPins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
})
$contentPaths = @($immutableContentPins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
}) + @($r28NewAssetRelativePaths | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
})
$sourceJournal = @(New-FileJournal $sourcePaths `
    (Join-Path $journalRoot 'source'))
$sourceAssetJournal = @(New-FileJournal $sourceAssetPaths `
    (Join-Path $journalRoot 'source-assets'))
$mapJournal = @(New-FileJournal @($mapFile) `
    (Join-Path $journalRoot 'map'))
$contentJournal = @(New-FileJournal $contentPaths `
    (Join-Path $journalRoot 'content'))
$buildPathsBefore = @(
    @(Get-BuildSurfacePaths) +
    @($r28DirectCompileObjectRelativePaths | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
    }) +
    @($r28GeneratedCompileObjectRelativePaths | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
    }) | Sort-Object -Unique
)
$buildJournal = @(New-FileJournal $buildPathsBefore `
    (Join-Path $journalRoot 'build'))
$stageResults = [Collections.Generic.List[object]]::new()
$committed = $false
$transactionError = $null
$rollbackReport = $null
$script:activeHelperHandle = $null
$script:activeHelperIdentity = $null
$script:activeHelperMapPackage = $null
$script:activeHelperLog = $null
$script:activeHelperLaunchProcessId = 0
$script:activeHelperLaunchStartTimeUtcTicks = 0L
$script:r28GrassPins = @()
$script:r28NewAssetPins = @()

try {
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'final prewrite gate'
    [void] (Assert-State $expectedProjectPin $nativeProjectFile 'final project')
    [void] (Assert-State $expectedMapPin $mapFile 'final R27 map')
    [void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'final R27 runtime DLL')
    [void] (Assert-State $expectedEditorDllPin $editorDll 'final R27 editor DLL')
    Assert-SourcePins -NativeBefore
    Assert-SourceAssetPins -NativeBefore
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableAncestorSourcePins -Native
    Assert-ImmutableContent
    Assert-R28NewAssetPredecessor

    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        $source = Join-Path $repositoryUnrealRoot $pin.RelativePath
        $target = Join-Path $nativeProjectRoot $pin.RelativePath
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter
    Assert-NoReparseAncestor $r28SourceAssetRoot $nativeProjectRoot
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableAncestorSourcePins -Native
    [void] (Assert-State $expectedMapPin $mapFile 'post-promotion map')
    Assert-ImmutableContent
    Assert-R28NewAssetPredecessor

    $forcedDirectCompileObjects = @(
        Remove-R28DirectCompileObjectsForForcedRebuild $buildJournal)
    $forcedGeneratedCompileObjects = @(
        Remove-R28GeneratedCompileObjectsForForcedRebuild $buildJournal)
    $buildLog = Join-Path $transactionRoot 'build.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development', $nativeProjectFile,
        '-DisablePlugin=AirSim',
        '-Module=TRIADSensorFusion',
        '-Module=TRIADSensorFusionEditor',
        '-WaitMutex',
        '-NoHotReloadFromIDE',
        '-NoUBTMakefiles',
        '-ForceHeaderGeneration',
        '-MaxParallelActions=1',
        '-NoUBA',
        '-NoUBALocal'
    )
    $buildAdmission = Assert-LaunchAdmission 'before R28 two-module build'
    & $buildTool @buildArguments *> $buildLog
    if ($LASTEXITCODE -ne 0) {
        throw "R28 two-module build failed with exit code $LASTEXITCODE. Log=$buildLog"
    }
    $buildLogText = Get-Content -LiteralPath $buildLog -Raw
    foreach ($row in $forcedDirectCompileObjects) {
        $leaf = [IO.Path]::GetFileNameWithoutExtension($row.Path)
        if (-not $buildLogText.Contains(
                "Compile [x64] $leaf", [StringComparison]::Ordinal)) {
            throw "R28 build log lacks forced direct compile action: $leaf"
        }
        $rebuilt = Get-FileState $row.Path
        if (-not $rebuilt.Present -or $rebuilt.Bytes -le 0) {
            throw "R28 direct compile object was not rebuilt: $($row.Path)"
        }
        if ($row.Before.Present -and
            $rebuilt.Sha256 -ceq $row.Before.Sha256) {
            throw "R28 direct compile object is byte-identical to its predecessor after forced rebuild: $($row.Path)"
        }
    }
    Assert-NativeIdle 'after R28 build'
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableAncestorSourcePins -Native
    [void] (Assert-State $expectedMapPin $mapFile 'post-build map')
    Assert-ImmutableContent
    Assert-R28NewAssetPredecessor
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Bytes -le 0 -or $editorAfter.Bytes -le 0 -or
        $runtimeAfter.Sha256 -ceq $expectedRuntimeDllPin.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditorDllPin.Sha256 -or
        $runtimeAfter.Sha256 -ceq $editorAfter.Sha256) {
        throw 'R28 build did not produce two distinct, changed, non-empty DLL receipts.'
    }
    $actorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp'
    $environmentActorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DR28EnvironmentActor.gen.cpp'
    $contextPolicyGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DContextPolicyActor.gen.cpp'
    $landmarkEditorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp'
    $groundEditorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.gen.cpp'
    $environmentEditorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.gen.cpp'
    $hybridGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
    # UHT deliberately preserves the bytes and write time of an unchanged
    # generated unit even under -ForceHeaderGeneration.  Timestamp comparison
    # is therefore not causal proof.  The seven generated object files were
    # journalled and made absent above; prove that this exact build ran UHT,
    # compiled every required reflection unit once, recreated every object,
    # linked both owning modules, and emitted the exact generated markers.
    $buildLogLines = @(Get-Content -LiteralPath $buildLog)
    $uhtManifest = Join-Path $nativeProjectRoot `
        'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest'
    $expectedUhtInvocation =
        "Running Internal UnrealHeaderTool $nativeProjectFile $uhtManifest -WarningsAsErrors -installed"
    $uhtInvocationIndexes = @()
    $uhtWriteIndexes = @()
    $uhtCompletionIndexes = @()
    for ($index = 0; $index -lt $buildLogLines.Count; ++$index) {
        $line = [string] $buildLogLines[$index]
        if ($line.Contains($expectedUhtInvocation, [StringComparison]::Ordinal)) {
            $uhtInvocationIndexes += $index
        }
        if ($line.Trim().Equals(
                'Total of 14 written', [StringComparison]::Ordinal)) {
            $uhtWriteIndexes += $index
        }
        if ($line.Contains(
                'Reflection code generated for UnrealEditor in ',
                [StringComparison]::Ordinal)) {
            $uhtCompletionIndexes += $index
        }
    }
    if ($uhtInvocationIndexes.Count -ne 1 -or
        $uhtWriteIndexes.Count -ne 1 -or
        $uhtCompletionIndexes.Count -ne 1 -or
        $uhtInvocationIndexes[0] -ge $uhtWriteIndexes[0] -or
        $uhtWriteIndexes[0] -ge $uhtCompletionIndexes[0]) {
        throw 'Forced-UHT exact invocation/write/completion sequence is absent, duplicated, or unordered.'
    }
    foreach ($failureMarker in @(
            'UnrealHeaderTool failed',
            'Failed to generate code',
            'CompilationResultException')) {
        if ($buildLogText.Contains(
                $failureMarker, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Forced-UHT log contains a failure marker: $failureMarker"
        }
    }

    $runtimeLinkLeaf = 'UnrealEditor-TRIADSensorFusion.dll'
    $editorLinkLeaf = 'UnrealEditor-TRIADSensorFusionEditor.dll'
    $moduleLinkIndexes = @{}
    foreach ($linkLeaf in @($runtimeLinkLeaf, $editorLinkLeaf)) {
        $linkPattern = '^\s*\[\d+/\d+\]\s+Link \[x64\]\s+' +
            [regex]::Escape($linkLeaf) + '\s*$'
        $linkMatchIndexes = @()
        for ($index = 0; $index -lt $buildLogLines.Count; ++$index) {
            if ([string] $buildLogLines[$index] -cmatch $linkPattern) {
                $linkMatchIndexes += $index
            }
        }
        if ($linkMatchIndexes.Count -ne 1 -or
            $linkMatchIndexes[0] -le $uhtCompletionIndexes[0]) {
            throw "R28 build log lacks one ordered exact module link action: $linkLeaf"
        }
        $moduleLinkIndexes[$linkLeaf] = $linkMatchIndexes[0]
    }

    $generatedReflectionSpecs = @(
        [pscustomobject] [ordered] @{
            Path = $actorGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[0]
            ModuleLinkLeaf = $runtimeLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ATRIADIstanaExploreV5DLandmarkVegetationActor)',
                'Z_Construct_UClass_ATRIADIstanaExploreV5DLandmarkVegetationActor')
        }
        [pscustomobject] [ordered] @{
            Path = $environmentActorGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[1]
            ModuleLinkLeaf = $runtimeLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ATRIADIstanaExploreV5DR28EnvironmentActor)',
                'Z_Construct_UClass_ATRIADIstanaExploreV5DR28EnvironmentActor',
                'execSetProviderReady',
                'execValidateR28Environment')
        }
        [pscustomobject] [ordered] @{
            Path = $contextPolicyGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[2]
            ModuleLinkLeaf = $runtimeLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ATRIADIstanaExploreV5DContextPolicyActor)',
                'Z_Construct_UClass_ATRIADIstanaExploreV5DContextPolicyActor')
        }
        [pscustomobject] [ordered] @{
            Path = $landmarkEditorGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[3]
            ModuleLinkLeaf = $editorLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary)',
                'Z_Construct_UClass_UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary',
                'execBuildOrValidateLandmarkGrassMaterialsR28',
                'execValidateLandmarkGrassMaterialsR28',
                'execValidateReusableLandmarkVegetationAssetsR28',
                'execConfigureLandmarkVegetationActorR28')
        }
        [pscustomobject] [ordered] @{
            Path = $groundEditorGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[4]
            ModuleLinkLeaf = $editorLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UTRIADIstanaExploreV5DGroundVegetationEditorLibrary)',
                'Z_Construct_UClass_UTRIADIstanaExploreV5DGroundVegetationEditorLibrary',
                'execUpgradeGroundVegetationRealismGrassSystemToR23B',
                'execValidateGroundVegetationRealismPassInLoadedV5DHybridMap')
        }
        [pscustomobject] [ordered] @{
            Path = $environmentEditorGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[5]
            ModuleLinkLeaf = $editorLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary)',
                'Z_Construct_UClass_UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary',
                'execEnsureR28EnvironmentAssets',
                'execValidateR28EnvironmentAssets',
                'execApplyR28EnvironmentToLoadedV5DHybridMap',
                'execValidateR28EnvironmentInLoadedV5DHybridMap')
        }
        [pscustomobject] [ordered] @{
            Path = $hybridGenerated
            ObjectRelativePath = $r28GeneratedCompileObjectRelativePaths[6]
            ModuleLinkLeaf = $editorLinkLeaf
            Markers = @(
                'IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UTRIADIstanaExploreV5DHybridEditorLibrary)',
                'Z_Construct_UClass_UTRIADIstanaExploreV5DHybridEditorLibrary',
                'execApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap',
                'execValidateIstanaExploreV5DR28VisualSuccessorMap',
                'execValidateIstanaExploreV5DR28VisualSuccessorPlayWorld',
                'execGetIstanaExploreV5DR28VisualSuccessorPlayStateReport',
                'execSetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa',
                'execCaptureIstanaExploreV5DR28VisualSuccessorPlayView',
                'execCaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView')
        }
    )
    if ($generatedReflectionSpecs.Count -ne 7 -or
        $forcedGeneratedCompileObjects.Count -ne 7) {
        throw 'R28 reflection proof roster must contain exactly seven generated translation units.'
    }
    $generatedTextByPath = @{}
    $generatedReflectionProofs =
        [Collections.Generic.List[object]]::new()
    foreach ($spec in $generatedReflectionSpecs) {
        $generated = [IO.Path]::GetFullPath([string] $spec.Path)
        $generatedState = Get-FileState $generated
        if (-not $generatedState.Present -or $generatedState.Bytes -le 0) {
            throw "Forced-UHT output is absent or empty: $generated"
        }
        $generatedItem = Get-Item -LiteralPath $generated -Force
        if (($generatedItem.Attributes -band
                [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Forced-UHT output is a reparse file: $generated"
        }
        $generatedText = Get-Content -LiteralPath $generated -Raw
        foreach ($marker in @($spec.Markers)) {
            if (-not $generatedText.Contains(
                    $marker, [StringComparison]::Ordinal)) {
                throw "Forced-UHT output lacks exact reflection marker '$marker': $generated"
            }
        }
        [void] (Assert-State $generatedState $generated `
            'stable generated reflection source read')

        $generatedLeaf = [IO.Path]::GetFileName($generated)
        $compilePattern = '^\s*\[\d+/\d+\]\s+Compile \[x64\]\s+' +
            [regex]::Escape($generatedLeaf) + '\s*$'
        $compileIndexes = @()
        for ($index = 0; $index -lt $buildLogLines.Count; ++$index) {
            if ([string] $buildLogLines[$index] -cmatch $compilePattern) {
                $compileIndexes += $index
            }
        }
        $linkIndex = [int] $moduleLinkIndexes[[string] $spec.ModuleLinkLeaf]
        if ($compileIndexes.Count -ne 1 -or
            $compileIndexes[0] -le $uhtCompletionIndexes[0] -or
            $compileIndexes[0] -ge $linkIndex) {
            throw "R28 build log lacks one ordered exact generated compile action: $generatedLeaf"
        }
        $objectMatches = @($forcedGeneratedCompileObjects | Where-Object {
                [string] $_.RelativePath -ceq
                    [string] $spec.ObjectRelativePath
            })
        if ($objectMatches.Count -ne 1) {
            throw "R28 generated object lacks one invalidated journal row: $($spec.ObjectRelativePath)"
        }
        $objectState = Get-FileState $objectMatches[0].Path
        if (-not $objectState.Present -or $objectState.Bytes -le 0) {
            throw "R28 generated compile object was not recreated: $($objectMatches[0].Path)"
        }
        $generatedTextByPath[$generated] = $generatedText
        $generatedReflectionProofs.Add([pscustomobject] [ordered] @{
            GeneratedSource = $generatedState
            GeneratedObject = $objectState
            CompileLogLineNumber = $compileIndexes[0] + 1
            ModuleLinkLeaf = [string] $spec.ModuleLinkLeaf
            MarkerCount = @($spec.Markers).Count
        })
    }
    $hybridGeneratedText = [string] $generatedTextByPath[$hybridGenerated]
    $landmarkEditorGeneratedText =
        [string] $generatedTextByPath[$landmarkEditorGenerated]
    $environmentActorGeneratedText =
        [string] $generatedTextByPath[$environmentActorGenerated]
    $environmentEditorGeneratedText =
        [string] $generatedTextByPath[$environmentEditorGenerated]
    foreach ($endpoint in @(
        'ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap',
        'ValidateIstanaExploreV5DR28VisualSuccessorMap',
        'ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld',
        'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport',
        'SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa',
        'CaptureIstanaExploreV5DR28VisualSuccessorPlayView',
        'CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView')) {
        if (-not $hybridGeneratedText.Contains(
                $endpoint, [StringComparison]::Ordinal)) {
            throw "Fresh Hybrid UHT endpoint gate failed: $endpoint"
        }
    }
    Assert-BinaryContainsEncodedMarkers $runtimeDll @(
        'ATRIADIstanaExploreV5DLandmarkVegetationActor',
        'ATRIADIstanaExploreV5DR28EnvironmentActor',
        'ATRIADIstanaExploreV5DContextPolicyActor',
        'SetProviderReady',
        'ValidateR28Environment',
        'VISUAL_ASSUMPTION_BOUND_R28_DENSE_TURF_MATURE_TROPICAL_CANOPY',
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_VALID',
        'ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_VALID',
        'r28EnvironmentOptional=true',
        'r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false',
        'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false',
        'existingSimulationRfInputsModified=false') `
        'Fresh runtime DLL'
    Assert-BinaryContainsEncodedMarkers $editorDll @(
        'TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R28_SINGLE_GATE_VISIBILITY_65M_90M',
        'ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_PASS',
        'ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_ASSETS_VALID',
        'ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_APPLY_PASS',
        'EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS',
        'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID',
        'providerCollisionNavigationSensorRfAuthority=false',
        'providerTokenReadSerializedOrLogged=false',
        'providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false',
        'V5D_R23B_DERIVATIVE_MATERIAL_VALID',
        'EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_FINAL_MUTATION_GATE') `
        'Fresh editor DLL'
    Assert-BinaryContainsEncodedMarkers $editorDll @(
        'UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary',
        'UTRIADIstanaExploreV5DGroundVegetationEditorLibrary',
        'UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary',
        'UTRIADIstanaExploreV5DHybridEditorLibrary',
        'ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap',
        'ValidateIstanaExploreV5DR28VisualSuccessorMap',
        'ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld',
        'GetIstanaExploreV5DR28VisualSuccessorPlayStateReport',
        'SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa',
        'CaptureIstanaExploreV5DR28VisualSuccessorPlayView',
        'CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView',
        'BuildOrValidateLandmarkGrassMaterialsR28',
        'ValidateLandmarkGrassMaterialsR28',
        'ValidateReusableLandmarkVegetationAssetsR28',
        'ConfigureLandmarkVegetationActorR28',
        'UpgradeGroundVegetationRealismGrassSystemToR23B',
        'ValidateGroundVegetationRealismPassInLoadedV5DHybridMap',
        'EnsureR28EnvironmentAssets',
        'ValidateR28EnvironmentAssets',
        'ApplyR28EnvironmentToLoadedV5DHybridMap',
        'ValidateR28EnvironmentInLoadedV5DHybridMap') `
        'Fresh editor reflection DLL'
    foreach ($endpoint in @(
        'BuildOrValidateLandmarkGrassMaterialsR28',
        'ValidateLandmarkGrassMaterialsR28',
        'ValidateReusableLandmarkVegetationAssetsR28',
        'ConfigureLandmarkVegetationActorR28')) {
        if (-not $landmarkEditorGeneratedText.Contains(
                $endpoint, [StringComparison]::Ordinal)) {
            throw "Fresh landmark editor UHT gate failed: $endpoint"
        }
    }
    foreach ($endpoint in @(
        'EnsureR28EnvironmentAssets',
        'ValidateR28EnvironmentAssets',
        'ApplyR28EnvironmentToLoadedV5DHybridMap',
        'ValidateR28EnvironmentInLoadedV5DHybridMap')) {
        if (-not $environmentEditorGeneratedText.Contains(
                $endpoint, [StringComparison]::Ordinal)) {
            throw "Fresh environment editor UHT gate failed: $endpoint"
        }
    }
    foreach ($endpoint in @('SetProviderReady', 'ValidateR28Environment')) {
        if (-not $environmentActorGeneratedText.Contains(
                $endpoint, [StringComparison]::Ordinal)) {
            throw "Fresh environment actor UHT gate failed: $endpoint"
        }
    }
    foreach ($proof in $generatedReflectionProofs) {
        [void] (Assert-State $proof.GeneratedSource `
            $proof.GeneratedSource.Path 'post-marker generated source')
        [void] (Assert-State $proof.GeneratedObject `
            $proof.GeneratedObject.Path 'post-marker generated object')
    }
    [void] (Assert-State $runtimeAfter $runtimeDll `
        'post-marker runtime DLL')
    [void] (Assert-State $editorAfter $editorDll `
        'post-marker editor DLL')

    $stageResults.Add((Invoke-ColdStage `
        '00_cold_validate_r27_pre_r28' $hybridLibrary `
        'ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap' `
        'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_SUCCESSOR_VALID'))
    [void] (Assert-State $expectedMapPin $mapFile `
        'pre-R28 R27-map validation stability')

    $stageResults.Add((Invoke-ColdStage `
        '01_cold_validate_phase2_pre_r28' $temasekLibrary `
        'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
        'OutReport' 'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID'))
    [void] (Assert-State $expectedMapPin $mapFile `
        'pre-R28 Phase-2 validation stability')

    $mapBeforeAssetBuild = Get-FileState $mapFile
    $stageResults.Add((Invoke-ColdStage `
        '02_build_r28_grass_materials' $assetLibrary `
        'BuildOrValidateLandmarkGrassMaterialsR28' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_BUILD_PASS'))
    Assert-NoReparseAncestor $r28GrassMaterialRoot $nativeProjectRoot
    [void] (Assert-State $mapBeforeAssetBuild $mapFile `
        'R28 grass-build map stability')
    $script:r28GrassPins = @(Get-R28GrassSuccessorPins)
    Assert-R28GrassPins $script:r28GrassPins
    Assert-R28EnvironmentPredecessor
    foreach ($marker in @(
            'createdPackages=4', 'exactSavedPackages=4',
            'exactR27Sources=4', 'allowedGraphDeltas=visibilityLabel,colorCode',
            'dryNearBlend=0.26', 'thatchNearBlend=0.46',
            'farDryBlend=0.12', 'farThatchBlend=0.025',
            'finalTint=0.90,1.12,0.90', 'finalChroma=0.94',
            'sourceMaterialsModified=false', 'mapsSaved=0',
            'visualCaptureAccepted=false',
            'captureRevalidationRequired=true')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R28 grass-build response lacks required marker: $marker"
        }
    }
    Assert-ImmutableContent
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter

    $stageResults.Add((Invoke-ColdStage `
        '03_validate_r28_grass_materials' $assetLibrary `
        'ValidateLandmarkGrassMaterialsR28' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_VALID'))
    [void] (Assert-State $mapBeforeAssetBuild $mapFile `
        'R28 grass-validation map stability')
    Assert-R28GrassPins $script:r28GrassPins
    Assert-R28EnvironmentPredecessor
    foreach ($marker in @(
            'exactSavedPackages=4', 'isolatedDerivativePackages=4',
            'exactR27Sources=4', 'completeCanonicalR23BGraph=true',
            'allowedGraphDeltas=visibilityLabel,colorCode',
            'compiledMaterials=4', 'sourceMaterialsModified=false',
            'mapsSaved=0', 'visualCaptureAccepted=false',
            'captureRevalidationRequired=true')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R28 grass-validation response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '04_validate_r28_vegetation_assets' $assetLibrary `
        'ValidateReusableLandmarkVegetationAssetsR28' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_ASSETS_VALID'))
    [void] (Assert-State $mapBeforeAssetBuild $mapFile `
        'R28 landmark-asset validation map stability')
    Assert-R28GrassPins $script:r28GrassPins
    Assert-R28EnvironmentPredecessor
    foreach ($marker in @(
            'exactAssets=14', 'reusedUnmodifiedAssets=10',
            'isolatedR28GrassMaterials=4', 'exactDerivativeGraph=true',
            'compiledMaterials=4', 'macDonaldGrass=3072',
            'temasekGrass=3072', 'maximumGrassPerSite=4096',
            'hardscapeRenderClearanceCm=35.0',
            'temasekTreeRoster=umbrella,dome,umbrella',
            'sourceTreeAnchorTranslationsPreserved=true',
            'treeScalesAnisotropic=true', 'mapMutation=false',
            'minimumNominalCarrierCoverage=0.75',
            'assetsCreated=false', 'collision=false', 'navigation=false',
            'sensorAuthority=false', 'rfAuthority=false',
            'visualCaptureAccepted=false',
            'captureRevalidationRequired=true')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R28 vegetation-asset response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '05_ensure_r28_environment_assets' $environmentLibrary `
        'EnsureR28EnvironmentAssets' 'OutMessage' `
        'EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_PASS'))
    Assert-NoReparseAncestor $r28EnvironmentRoot $nativeProjectRoot
    [void] (Assert-State $mapBeforeAssetBuild $mapFile `
        'R28 environment-build map stability')
    $script:r28NewAssetPins = @(Get-R28NewAssetSuccessorPins)
    Assert-R28NewAssetPins $script:r28NewAssetPins
    foreach ($marker in @(
            'exact 13-asset isolated namespace',
            'assets=13', 'meshes=2', 'materials=11',
            'architectureTriangles=149758',
            'architectureSourceCorners=449274',
            'architectureWindows=14786',
            'connectivePublicRealmTriangles=32310',
            'priorityBuildingParts=40',
            'priorityBuildingCoverageComplete=true',
            'baselineBuildingParts=85',
            'retainedBaselineBuildingParts=41',
            'selectedBuildingParts=128',
            'newlySelectedBuildingParts=87',
            'evidenceCameraCount=4',
            'minimumEvidenceCameraProxyCoverage=0.443796940',
            'priorityStreetscapeCameraCount=2',
            'minimumPriorityStreetscapeCameraProxyCoverage=0.721584324',
            'omnidirectionalSectorsMeetingQuota=24',
            'architecturePartCeiling=128',
            'architectureWindowCeiling=15000',
            'architectureTriangleCeiling=150000',
            'architectureSourceCornerCeiling=450000',
            'publicRoadSegments=1923', 'terrainDrapeSourcePinned=true',
            'terrainSamplesResolved=64620', 'terrainSamplesUnresolved=0',
            'proceduralMicroSurface=true', 'sidewalkJointCues=true',
            'roughnessVariation=true', 'viewDependentGlazingCue=true',
            'opaqueGlazing=true', 'glazingTransparencyClaimed=false',
            'screenSpaceReflections=false',
            'textureInputs=0',
            'runtimeGeometry=false', 'collision=false', 'navigation=false',
            'existingSurroundingsPublicRealmOuterGroundRfInputsModified=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R28 environment-build response lacks required marker: $marker"
        }
    }
    Assert-ImmutableContent
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter

    $stageResults.Add((Invoke-ColdStage `
        '06_validate_r28_environment_assets' $environmentLibrary `
        'ValidateR28EnvironmentAssets' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_ASSETS_VALID'))
    [void] (Assert-State $mapBeforeAssetBuild $mapFile `
        'R28 environment-validation map stability')
    Assert-R28NewAssetPins $script:r28NewAssetPins

    $stageResults.Add((Invoke-ColdStage `
        '07_apply_combined_r28_visual_successor' $hybridLibrary `
        'ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap' `
        'OutMessage' 'EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS' `
        @{
            ExpectedPredecessorBytes = [int64] $expectedMapPin.Bytes
            ExpectedPredecessorSha256 = [string] $expectedMapPin.Sha256
            VerifiedExternalBackupFilename = [string] $mapJournal[0].Backup
        }))
    $successorPin = Get-FileState $mapFile
    if (-not $successorPin.Present -or $successorPin.Bytes -le 0 -or
        $successorPin.Sha256.Length -ne 64 -or
        ($successorPin.Bytes -eq $expectedMapPin.Bytes -and
         $successorPin.Sha256 -ceq $expectedMapPin.Sha256)) {
        throw 'Combined R28 apply did not produce a distinct positive successor map receipt.'
    }
    foreach ($marker in @(
            'oneSave=true', 'environmentActorAdded=1',
            'landmarkActorReconfigured=1', 'environmentAssets=13',
            'landmarkAssets=14', 'landmarkGrassInstances=6144',
            'landmarkGrassMaximumPerSite=4096',
            'temasekTreeRoster=umbrella,dome,umbrella',
            'contextFacadeR25Retained=true', 'temasekPhase2Retained=true',
            'providerSettingsUnchanged=true', 'providerClipUnchanged=true',
            'collisionNavigationSensorRfAuthority=false',
            'externalBackupVerified=true', 'rollbackOwnedByWrapper=true',
            'visualCaptureAccepted=false',
            'captureRevalidationRequired=true')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "Combined R28 apply response lacks required marker: $marker"
        }
    }
    Assert-R28NewAssetPins $script:r28NewAssetPins
    Assert-ImmutableContent
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter

    $stageResults.Add((Invoke-ColdStage `
        '08_cold_validate_r28_environment_map' $environmentLibrary `
        'ValidateR28EnvironmentInLoadedV5DHybridMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_MAP_VALID'))
    [void] (Assert-State $successorPin $mapFile `
        'R28 environment-map validation stability')
    foreach ($marker in @(
            'classActors=1', 'taggedActors=1',
            'providerVisibilityMirrored=true', 'providerReady=false',
            'visible=true', 'components=3', 'renderOnly=true',
            'identity=true', 'mapSavedByThisValidator=false',
            'sceneCapture=false', 'collision=false', 'navigation=false',
            'sensorRfAuthority=false', 'runtimeGeometry=false',
            'existingSimulationRfInputsModified=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R28 environment-map response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '09_cold_validate_combined_r28_map' $hybridLibrary `
        'ValidateIstanaExploreV5DR28VisualSuccessorMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID'))
    [void] (Assert-State $successorPin $mapFile `
        'combined R28 cold-validation stability')
    foreach ($marker in @(
            'combinedR28=true', 'environmentActors=1',
            'landmarkVegetationActors=1',
            'environmentProviderReadyMatchesPolicy=true',
            'environmentAssets=13', 'landmarkAssets=14',
            'landmarkGrassInstances=6144',
            'landmarkGrassMaximumPerSite=4096',
            'temasekTreeRoster=umbrella,dome,umbrella',
            'legacyTemasekBakedFoliageRemoved=true',
            'temasekPhase2Retained=true',
            'contextFacadeR25Retained=true',
            'providerSettingsUnchanged=true', 'providerClipUnchanged=true',
            'providerVisualOnly=true',
            'providerCollisionNavigationSensorRfAuthority=false',
            'r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false',
            'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false',
            'providerTokenReadSerializedOrLogged=false',
            'providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false',
            'renderOnly=true', 'visualCaptureAccepted=false',
            'captureRevalidationRequired=true')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "Combined R28 validation response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '10_idempotent_combined_r28_apply' $hybridLibrary `
        'ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap' `
        'OutMessage' `
        'IDEMPOTENT_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_ALREADY_VALID' `
        @{
            ExpectedPredecessorBytes = [int64] $expectedMapPin.Bytes
            ExpectedPredecessorSha256 = [string] $expectedMapPin.Sha256
            VerifiedExternalBackupFilename = [string] $mapJournal[0].Backup
        }))
    [void] (Assert-State $successorPin $mapFile `
        'combined R28 idempotent-map stability')
    if (-not $stageResults[$stageResults.Count - 1].Message.Contains(
            'oneSave=false', [StringComparison]::Ordinal)) {
        throw 'Combined R28 idempotent response did not prove oneSave=false.'
    }

    $stageResults.Add((Invoke-ColdStage `
        '11_postvalidate_combined_r28_r25_provider_contract' $hybridLibrary `
        'ValidateIstanaExploreV5DR28VisualSuccessorMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID'))
    [void] (Assert-State $successorPin $mapFile `
        'post-idempotence R28 map stability')
    foreach ($marker in @(
            'contextFacadeR25Retained=true',
            'environmentProviderReadyMatchesPolicy=true',
            'providerVisualOnly=true',
            'providerCollisionNavigationSensorRfAuthority=false',
            'providerTokenReadSerializedOrLogged=false',
            'providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "Post-R28 R25/provider response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '12_cold_validate_phase2_post_r28' $temasekLibrary `
        'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
        'OutReport' 'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID'))
    [void] (Assert-State $successorPin $mapFile `
        'post-R28 Phase-2 validation stability')

    $stageResults.Add((Invoke-ColdStage `
        '13_postvalidate_r28_grass_materials' $assetLibrary `
        'ValidateLandmarkGrassMaterialsR28' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R28_VALID'))
    [void] (Assert-State $successorPin $mapFile `
        'post-R28 grass-validation stability')

    Assert-R28NewAssetPins $script:r28NewAssetPins
    Assert-ImmutableContent
    Assert-SourcePins -NativeAfter
    Assert-SourceAssetPins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableAncestorSourcePins -Native
    [void] (Assert-State $expectedMapPin $mapJournal[0].Backup `
        'preserved exact R27 map journal backup')
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'commit gate'

    $environmentBoundary = $r28EnvironmentRoot.TrimEnd('\') + '\'
    $environmentSuccessorPins = @($script:r28NewAssetPins |
        Where-Object { $_.Path.StartsWith(
                $environmentBoundary,
                [StringComparison]::OrdinalIgnoreCase) })
    if ($environmentSuccessorPins.Count -ne 13 -or
        $script:r28GrassPins.Count -ne 4 -or
        $script:r28NewAssetPins.Count -ne 17) {
        throw 'Final R28 asset receipt split is not exactly 4 grass + 13 environment.'
    }

    $commit = [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'PASS'
        RunToken = $RunToken
        PredecessorMap = $expectedMapPin
        PredecessorRuntimeDll = $expectedRuntimeDllPin
        PredecessorEditorDll = $expectedEditorDllPin
        SuccessorMap = $successorPin
        SuccessorRuntimeDll = $runtimeAfter
        SuccessorEditorDll = $editorAfter
        CodeSourceCount = $sourcePins.Count
        SourceAssetSpecCount = $sourceAssetPins.Count
        Phase2CompiledSourceCount = $phase2CompiledSourcePins.Count
        ImmutableAncestorSourceCount = $immutableAncestorSourcePins.Count
        ImmutableContentCount = $immutableContentPins.Count
        ExactlyNewAssetCount = $script:r28NewAssetPins.Count
        R28GrassPackages = @($script:r28GrassPins)
        R28EnvironmentPackages = @($environmentSuccessorPins)
        PromotedSourceAssets = @($sourceAssetPins | ForEach-Object {
            Get-FileState (Join-Path $nativeProjectRoot $_.RelativePath)
        })
        PromotionDirectoriesBefore = @($r28PromotionDirectoriesBefore)
        PromotionDirectoriesAfter = @(Get-R28PromotionDirectoryInventory)
        BuildSurfaceStateCount = $buildJournal.Count
        ForcedDirectCompileObjectCount =
            $r28DirectCompileObjectRelativePaths.Count
        ForcedGeneratedCompileObjectCount =
            $r28GeneratedCompileObjectRelativePaths.Count
        ForcedHeaderGeneration = $true
        UhtEvidence = [pscustomobject] [ordered] @{
            ExactInvocationCount = $uhtInvocationIndexes.Count
            ExactWriteCountMarker = 14
            CompletionCount = $uhtCompletionIndexes.Count
            InvocationLogLineNumber = $uhtInvocationIndexes[0] + 1
            WriteCountLogLineNumber = $uhtWriteIndexes[0] + 1
            CompletionLogLineNumber = $uhtCompletionIndexes[0] + 1
        }
        GeneratedReflectionProofs = @($generatedReflectionProofs)
        GeneratedCompileActionCount = $generatedReflectionProofs.Count
        ModuleLinkActions = @(
            [pscustomobject] [ordered] @{
                Leaf = $runtimeLinkLeaf
                LogLineNumber = [int] $moduleLinkIndexes[$runtimeLinkLeaf] + 1
            }
            [pscustomobject] [ordered] @{
                Leaf = $editorLinkLeaf
                LogLineNumber = [int] $moduleLinkIndexes[$editorLinkLeaf] + 1
            }
        )
        ModuleLinkActionCount = $moduleLinkIndexes.Count
        BuildLaunchAdmission = $buildAdmission
        PrewriteLaunchAdmission = $prewriteAdmission
        PrewriteAdmissionReceipt = $prewriteAdmissionReceiptPin
        MinimumSystemFreeVirtualAtLaunchBytes =
            $minimumSystemFreeVirtualAtLaunchBytes
        RenderingCapableFreshEditorProcesses = $stageResults.Count
        Stages = @($stageResults)
        ProtectedUE54Sessions = @($script:protectedBefore)
        ProtectedUE54SessionsUnchanged = $true
        UE55OrRemoteControlOwnerAcceptedOutsideOwnedHelpers = $false
        SerializedProviderReady = $false
        SerializedLocalFallbackVisible = $true
        SerializedProviderCoherenceValidated = $true
        ProviderFalseTrueFalseRuntimeTransitionProven = $false
        CollisionNavigationSensorRfTerrainAuthority = $false
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $commitPath `
        -Encoding utf8NoBOM -NoNewline
    $committed = $true
    $commit | ConvertTo-Json -Depth 20
}
catch {
    $transactionError = $_.Exception
}
finally {
    if (-not $committed) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        # Defense in depth: a stage that throws anywhere between Start-Process
        # and its post-cleanup boundary leaves its exact handle registered.
        # Re-prove the complete Win32 identity, contain only that process, and
        # require RC release before permitting any rollback write.
        if ($null -ne $script:activeHelperHandle) {
            try {
                if ($null -eq $script:activeHelperIdentity) {
                    $script:activeHelperIdentity =
                        Wait-ExpectedHelperIdentity `
                            -Handle $script:activeHelperHandle `
                            -MapPackage $script:activeHelperMapPackage `
                            -Log $script:activeHelperLog `
                            -LaunchStartTimeUtcTicks `
                                $script:activeHelperLaunchStartTimeUtcTicks `
                            -TimeoutSeconds 30
                }
                $ownedStopError = $null
                if ($null -ne $script:activeHelperIdentity) {
                    try {
                        Stop-OwnedHelper $script:activeHelperIdentity `
                            $script:activeHelperHandle
                    }
                    catch { $ownedStopError = $_.Exception }
                }
                if (Test-Win32ProcessPresent `
                        $script:activeHelperLaunchProcessId) {
                    try {
                        Stop-LaunchedHelperBeforeIdentity `
                            -Handle $script:activeHelperHandle `
                            -LaunchProcessId `
                                $script:activeHelperLaunchProcessId `
                            -LaunchStartTimeUtcTicks `
                                $script:activeHelperLaunchStartTimeUtcTicks
                    }
                    catch {
                        $prior = if ($null -eq $ownedStopError) { 'none' }
                            else { $ownedStopError.Message }
                        throw "Exact helper stop error={$prior}; launch-handle fallback error={$($_.Exception.Message)}"
                    }
                }
                if (Test-Win32ProcessPresent `
                        $script:activeHelperLaunchProcessId) {
                    throw 'Launched helper remains after outer containment.'
                }
                if (@(Get-RcOwners).Count -ne 0) {
                    throw 'RC listener remains after outer helper containment.'
                }
                $script:activeHelperHandle = $null
                $script:activeHelperIdentity = $null
                $script:activeHelperLaunchProcessId = 0
                $script:activeHelperLaunchStartTimeUtcTicks = 0L
            }
            catch {
                $rollbackErrors.Add(
                    "owned helper containment: $($_.Exception.Message)")
            }
        }
        try { Assert-NativeIdle 'rollback entry' }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackErrors.Count -eq 0) {
            try { Restore-MapFromJournal $mapJournal[0] }
            catch { $rollbackErrors.Add("map: $($_.Exception.Message)") }
            try { Restore-BuildSurface $buildJournal }
            catch { $rollbackErrors.Add("build: $($_.Exception.Message)") }
            try { Restore-FileJournal $contentJournal }
            catch { $rollbackErrors.Add("content: $($_.Exception.Message)") }
            try { Restore-FileJournal $sourceAssetJournal }
            catch { $rollbackErrors.Add("source assets: $($_.Exception.Message)") }
            try { Restore-FileJournal $sourceJournal }
            catch { $rollbackErrors.Add("source: $($_.Exception.Message)") }
            try {
                Restore-R28PromotionDirectories `
                    $r28PromotionDirectoriesBefore
            }
            catch {
                $rollbackErrors.Add(
                    "promotion directories: $($_.Exception.Message)")
            }
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore }
        catch { $rollbackErrors.Add("protected: $($_.Exception.Message)") }
        $rollbackReport = [pscustomobject] [ordered] @{
            Schema = $schema
            Status = if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            RunToken = $RunToken
            Failure = if ($null -ne $transactionError) { $transactionError.Message } else { 'unknown' }
            Errors = @($rollbackErrors)
            RestoredPredecessorMap = Get-FileState $mapFile
            RestoredRuntimeDll = Get-FileState $runtimeDll
            RestoredEditorDll = Get-FileState $editorDll
            PrewriteLaunchAdmission = $prewriteAdmission
            PrewriteAdmissionReceipt = $prewriteAdmissionReceiptPin
            ProtectedUE54SessionsUnchanged = $rollbackErrors.Count -eq 0
        }
        $rollbackPath = Join-Path $transactionRoot 'rollback.json'
        $rollbackReport | ConvertTo-Json -Depth 16 | Set-Content `
            -LiteralPath $rollbackPath -Encoding utf8NoBOM -NoNewline
    }
}

if (-not $committed) {
    throw "R28 native transaction failed and reported $($rollbackReport.Status): $($transactionError.Message). Receipt=$(Join-Path $transactionRoot 'rollback.json')"
}
