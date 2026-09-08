#requires -Version 7.0

<#
.SYNOPSIS
Runs the strict Temasek Shophouse Phase-2 replacement transaction on an exact,
caller-pinned Landmark Vegetation R26 Istana hybrid predecessor.

.DESCRIPTION
The default invocation is a read-only preflight. -Execute is the only native
write authority. The transaction deterministically rebuilds and validates the
seven prepared Phase-2 interchange files in a private transaction staging
directory; promotes exactly seven reviewed repository files plus those seven
staged files; builds only TRIADSensorFusion and TRIADSensorFusionEditor; then
replaces the exact legacy one-mesh/nineteen-material Temasek package roster
with one mesh/fifteen non-foliage materials. It cold-validates assets and the
hybrid map and proves a second map apply is byte-idempotent.

The embedded R25 map and module pins are documentation/read-only-preflight
defaults only. Live execution requires explicit caller pins from the committed
R26 receipt. Before the first functional native mutation, every promoted
destination, all 172 scoped
build/UHT states, the exact map, and all twenty legacy Temasek packages are
copied without overwrite and hash-verified. Failure restores content, build
products, and source destinations in reverse order. No recursive deletion is
used. The durable transaction directory remains for audit.

Every editor stage uses a separate rendering-capable UE 5.5 process. Remote
Control is accepted only while port 30010 is solely owned by the exact process
returned by Start-Process. Every exact-token UE 5.4 CAPSTONE session present
at preflight is preserved as an immutable identity set.

Phase 2 deliberately contains zero baked foliage. It hands three source-local
tree anchors to ATRIADIstanaExploreV5DLandmarkVegetationActor. This wrapper
does not promote or place that separately committed R26 actor. Live execution
requires R26 and cold-validates landmarkVegetationR26=true before asset
evacuation, after the Phase-2 apply, and at final commit.

.EXAMPLE
.\Invoke-IstanaExploreV5DTemasekPhase2NativeTransactionV1.ps1 -RunToken reviewed_phase2

.EXAMPLE
# Supply all six map/DLL values from the committed R26 receipt for live use.
.\Invoke-IstanaExploreV5DTemasekPhase2NativeTransactionV1.ps1 -RunToken reviewed_phase2 -Execute -RequireLandmarkVegetationR26 -ExpectedMapBytes <R26-map-bytes> -ExpectedMapSha256 <R26-map-sha256> -ExpectedRuntimeDllBytes <R26-runtime-bytes> -ExpectedRuntimeDllSha256 <R26-runtime-sha256> -ExpectedEditorDllBytes <R26-editor-bytes> -ExpectedEditorDllSha256 <R26-editor-sha256>
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,

    [switch] $StaticSelfCheck,

    # R25 defaults are documentation/read-only convenience only. Live
    # execution requires every one of these six values to be passed explicitly
    # from the committed R26 predecessor receipt.
    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedMapBytes = 34993427L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedMapSha256 =
        '38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9',

    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedRuntimeDllBytes = 4585984L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedRuntimeDllSha256 =
        '31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4',

    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedEditorDllBytes = 7671296L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedEditorDllSha256 =
        '471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34',

    [switch] $RequireLandmarkVegetationR26,

    [ValidateRange(60, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema =
    'triad.istana_explore_v5d.temasek_phase2.native_transaction.v1'
$repositoryRoot = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo')
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot `
    'unreal'))
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'TRIAD.uproject'))
$nativeUE55EngineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $nativeUE55EngineRoot `
    'Engine\Build\BatchFiles\Build.bat'))
$editor = [IO.Path]::GetFullPath((Join-Path $nativeUE55EngineRoot `
    'Engine\Binaries\Win64\UnrealEditor.exe'))
$buildVersionFile = [IO.Path]::GetFullPath((Join-Path $nativeUE55EngineRoot `
    'Engine\Build\Build.version'))
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$hybridMapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$assetStageMapPackage = '/Game/Maps/Entry'
$hybridMapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DTemasekPhase2V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase `
    $RunToken))
$replaceDisplacedRoot = [IO.Path]::GetFullPath((Join-Path $transactionRoot `
    'replace_displaced'))
$script:replaceSequence = 0
$ddcRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\DerivedDataCache'))
$rcCallUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$temasekLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DTemasekShophouseEditorLibrary'
$hybridLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$temasekAssetRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse'))
$temasekSourceRootRelative =
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse'
$temasekRepoSourceRoot = [IO.Path]::GetFullPath((Join-Path `
    $repositoryUnrealRoot $temasekSourceRootRelative))
$builderFile = [IO.Path]::GetFullPath((Join-Path $temasekRepoSourceRoot `
    'build_temasek_shophouse_r24.py'))
$validatorFile = [IO.Path]::GetFullPath((Join-Path $temasekRepoSourceRoot `
    'validate_temasek_shophouse_r24.py'))

$expectedProjectPin = [pscustomobject] [ordered] @{
    Path = $nativeProjectFile
    Present = $true
    Bytes = 1298L
    Sha256 =
        '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
}
$expectedPredecessorMapPin = [pscustomobject] [ordered] @{
    Path = $hybridMapFile
    Present = $true
    Bytes = [int64] $ExpectedMapBytes
    Sha256 = ([string] $ExpectedMapSha256).ToUpperInvariant()
}
$expectedPredecessorRuntimeDllPin = [pscustomobject] [ordered] @{
    Path = $runtimeDll
    Present = $true
    Bytes = [int64] $ExpectedRuntimeDllBytes
    Sha256 = ([string] $ExpectedRuntimeDllSha256).ToUpperInvariant()
}
$expectedPredecessorEditorDllPin = [pscustomobject] [ordered] @{
    Path = $editorDll
    Present = $true
    Bytes = [int64] $ExpectedEditorDllBytes
    Sha256 = ([string] $ExpectedEditorDllSha256).ToUpperInvariant()
}

# Seven reviewed repository files are promoted. The seven generated payload
# files are admitted separately from a fresh deterministic staging build.
$repositorySourcePins = @(
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseActor.cpp'
        Bytes = 22760L
        Sha256 = '26765231E1E48196B75F786D640BC2E2DB872CDD665DC83F148F1D4623802E28'
        NativeBeforeBytes = 22824L
        NativeBeforeSha256 = '89EA99CC0888C006EBC495D4302009F1B381958BE4074E8DA89580D289FF4EA1'
    }
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseProvenance.h'
        Bytes = 5433L
        Sha256 = 'AC19B7B935D49F7C5CBD8DD7CCC891DF361B8E74B646ADB1FBD5C565ED91E1BC'
        NativeBeforeBytes = 4316L
        NativeBeforeSha256 = '1126B52219E0AC5320ED4F40863EFE147C0616F3520DD05349928D2166120EB8'
    }
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_RUNTIME'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'
        Bytes = 14772L
        Sha256 = 'A97778A9C4F4DC940C9F550C24376778999D28A5E7600467582EE397514CB992'
        NativeBeforeBytes = 13306L
        NativeBeforeSha256 = '41F1FB40C0EF75246DDE770015E5210E18D37DCCEBCC4C3DDA4F2E4A9BE4B368'
    }
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_RUNTIME_TEST'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'
        Bytes = 10610L
        Sha256 = '1B9EA89E7C86374C6B9D4D82DD8DCF3BD23CA8F3B35ECBA222074B5B836AFA90'
        NativeBeforeBytes = 9076L
        NativeBeforeSha256 = '7C1556C72F608624CC0657339CDE053D217215B8047C5A2A55D558B46E0B9044'
    }
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'
        Bytes = 66908L
        Sha256 = '6CE20C0BADBBC9291B739091B11E05436BCE71B928EBD3E276A52224EA06808C'
        NativeBeforeBytes = 68078L
        NativeBeforeSha256 = '020599213B1177819B0926F2E2A5BF1AE631DA62D6F71506A314636B13CF5706'
    }
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_EDITOR_API'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'
        Bytes = 9627L
        Sha256 = '81AC1C9EE3B708B1943B08C60A1AA922096A010A1631C9002AEE6DF2C2BC18AE'
        NativeBeforeBytes = 8682L
        NativeBeforeSha256 = '4DED8BD31A1A0CC1EBCCABA3BBF71D82E6F06A66C42A6B0B1FB804BEFCDCC481'
    }
    [pscustomobject] [ordered] @{
        Area = 'PHASE2_CONTRACT'
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\temasek_shophouse_r24.contract.json'
        Bytes = 10720L
        Sha256 = 'F304EE5D090D41F2331F293E6CD797A9B27D5B3B74DE44DCAA6A3CC2A66FE082'
        NativeBeforeBytes = 9474L
        NativeBeforeSha256 = '6EC251EF53D28B8D5D97CA9F93A380FBB1916E9F2AFA974EAC383CEBA71F345F'
    }
)

$generatedPins = @(
    [pscustomobject] [ordered] @{
        Name = 'IstanaPublicViewV5DR24TemasekShophouse.features.json'
        Bytes = 7376L
        Sha256 = '900BED02B0C5CBFD8D2668C017B33F34B800B354F251B57767DE243F8126E4E0'
        NativeBeforeBytes = 4720L
        NativeBeforeSha256 = 'C09D37505ED46073E91A799D59D845CA857E2164FB1E26F76A84C7614F4300C5'
    }
    [pscustomobject] [ordered] @{
        Name = 'IstanaPublicViewV5DR24TemasekShophouse.geometry.json'
        Bytes = 2149277L
        Sha256 = 'CC40E8C2C975D2A19124D74EAAC80B6C14C7121AD6BDDD616A740620737B05A7'
        NativeBeforeBytes = 3276163L
        NativeBeforeSha256 = '433645DF13FCA45C0BC5392DC1D7EB3A72F0F8395FE431540696194942CEE9DC'
    }
    [pscustomobject] [ordered] @{
        Name = 'IstanaPublicViewV5DR24TemasekShophouse.manifest.json'
        Bytes = 9131L
        Sha256 = 'ED46D1B6367859A42BF1404FE1C0F5E457C16276777818B510FD8A8CA3BE8D87'
        NativeBeforeBytes = 7095L
        NativeBeforeSha256 = '495BBDFF95FB6507E816301E514577FF27E697CCA650729F451D9F3BE68C4ECA'
    }
    [pscustomobject] [ordered] @{
        Name = 'IstanaPublicViewV5DR24TemasekShophouse.materials.json'
        Bytes = 6610L
        Sha256 = '945404D4E6EC0E873FCDCBDA13977A67747961A3ADAAD413D4C1D20690EBE3FF'
        NativeBeforeBytes = 8265L
        NativeBeforeSha256 = 'AE7600271DC222AE1E727E23241232C1179E8337B74541892EC177489110DE6A'
    }
    [pscustomobject] [ordered] @{
        Name = 'SM_IPV5D_R24_TemasekShophouse_Render.mtl'
        Bytes = 2355L
        Sha256 = '41D50D10924A2AA0FECAB2ADC48E7BC071A20ED8A9F2D109BE81E0D2EF2CE4CB'
        NativeBeforeBytes = 2938L
        NativeBeforeSha256 = '4647F2B9BEA229C016B5FFEA0136C40D8CB5EBC286BA095DC97E8C90E020C2FC'
    }
    [pscustomobject] [ordered] @{
        Name = 'SM_IPV5D_R24_TemasekShophouse_Render.obj'
        Bytes = 610314L
        Sha256 = 'F5ED94C312A38A542D9C078F19F16FAF04CEA6ECAE5B4C2CF0AA7F3D651B9007'
        NativeBeforeBytes = 980675L
        NativeBeforeSha256 = '81720789E5D96B32E00FF5744B1407730B2548C5CDF3818BE7998063E5E5676B'
    }
    [pscustomobject] [ordered] @{
        Name = 'temasek_shophouse_r24.unreal_placement.json'
        Bytes = 4517L
        Sha256 = '6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5'
        NativeBeforeBytes = 4517L
        NativeBeforeSha256 = '61C9CFB4D055401B5AE44CBD19A382DD1DCB2516F0766A0E6B928A706EA72987'
    }
)

$generatorAdmissionPins = @(
    [pscustomobject] [ordered] @{
        Role = 'PHASE2_BUILDER'; Path = $builderFile; Bytes = 73481L
        Sha256 = 'F0DA97AE8F1306F8F1E88BE977D4A91610F6D0D7A338F5DCAB6B1DE64B5A0231'
    }
    [pscustomobject] [ordered] @{
        Role = 'PHASE2_VALIDATOR'; Path = $validatorFile; Bytes = 27749L
        Sha256 = '2CC2DDF281F626FC37EBC9080754D24CF121CE3A284155012F9E87D58E577DEB'
    }
    [pscustomobject] [ordered] @{
        Role = 'PHASE2_SCHEMA'
        Path = [IO.Path]::GetFullPath((Join-Path $temasekRepoSourceRoot `
            'temasek_shophouse_r24.contract.schema.json'))
        Bytes = 7051L
        Sha256 = 'C820314C96B9D59E17B44E9002B83FB972BE2BAD332BE6B3DAE7CA252002D765'
    }
    [pscustomobject] [ordered] @{
        Role = 'PUBLIC_SOURCE_ROSTER'
        Path = [IO.Path]::GetFullPath((Join-Path $temasekRepoSourceRoot `
            'Sources\public_sources.json'))
        Bytes = 5870L
        Sha256 = 'E4D1B0F3648B87E577A004F158821EBD716E0288511C51A468E272D475DE009E'
    }
    [pscustomobject] [ordered] @{
        Role = 'OSM_FEATURE_RECEIPT'
        Path = [IO.Path]::GetFullPath((Join-Path $temasekRepoSourceRoot `
            'Sources\osm_feature_receipt.json'))
        Bytes = 3407L
        Sha256 = '7A687EB65588643F482F5F4BD4CEE9C42F39F638B594410336F50000A272D23B'
    }
)

$unchangedNativeSourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseActor.h'
        Bytes = 4064L
        Sha256 = 'D72DD0458B10D3EE5038B2E7623D405BCECB55B944B0BCCA271DDB25140F89FC'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h'
        Bytes = 496L
        Sha256 = '1CCF3D7F68B74325ABFBCBACCEA9ECD099EC8FA71D9A5A53C499DEE0157A4FB0'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h'
        Bytes = 1058L
        Sha256 = '3225071442862102AB6B7005ACCA61B0E385B47D9834819584554C9C8F8CC95A'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse\Sources\public_sources.json'
        Bytes = 5870L
        Sha256 = 'E4D1B0F3648B87E577A004F158821EBD716E0288511C51A468E272D475DE009E'
    }
)

$buildProductPins = @(
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'; ExplicitPin=$true; Bytes=[int64]$ExpectedRuntimeDllBytes; Sha256=([string]$ExpectedRuntimeDllSha256).ToUpperInvariant() }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.exp'; ExplicitPin=$false }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.pdb'; ExplicitPin=$false }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'; ExplicitPin=$true; Bytes=[int64]$ExpectedEditorDllBytes; Sha256=([string]$ExpectedEditorDllSha256).ToUpperInvariant() }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.exp'; ExplicitPin=$false }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.pdb'; ExplicitPin=$false }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor.modules'; ExplicitPin=$false }
)

$landmarkR26NativeSourceRelativePaths = @(
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DLandmarkVegetationActor.h'
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp'
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h'
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp'
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h'
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
)

$compileArtifactSuffixes = @(
    '.dep.json', '.obj', '.obj.rsp', '.obj.rsp.old', '.sarif')
$compileArtifactSpecs = @(
    # The proven R25 closure remains journalled because ForceHeaderGeneration
    # and UBT dependency discovery may legitimately revisit these units.
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DContextPolicyActor.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DContextPolicyActor.gen.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DContextFacadeR25AssetFactory.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.gen.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DHybridEditorLibrary.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5AppearanceActor.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DGroundVegetationActor.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DMacDonaldHouseActor.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DTemasekShophouseActor.cpp'; Admission='R25_BASELINE_PHASE2_DIRECT' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp'; Admission='R25_BASELINE' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'; Admission='R25_BASELINE_PHASE2_DIRECT' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADSensorFusionEditor.init.gen.cpp'; Admission='R25_BASELINE' }
    # Phase-2-specific dependency and direct-build closure.
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DTemasekShophouseActor.gen.cpp'; Admission='PHASE2_DEPENDENCY' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'; Admission='PHASE2_DIRECT' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DTemasekShophouseProvenance.gen.cpp'; Admission='PHASE2_UHT' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'; Admission='PHASE2_DIRECT' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'; Admission='PHASE2_DIRECT' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.gen.cpp'; Admission='PHASE2_DEPENDENCY' }
    # The committed R26 successor is part of the forced-UHT build boundary.
    # These states are journalled even if UBT determines they are current, so
    # a failed Phase-2 build can restore the exact R26 predecessor surface.
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DLandmarkVegetationActor.cpp'; Admission='R26_SUCCESSOR_BOUNDARY' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp'; Admission='R26_SUCCESSOR_BOUNDARY' }
    [pscustomobject] @{ Module='TRIADSensorFusion'; Base='TRIADSensorFusion.init.gen.cpp'; Admission='R26_SUCCESSOR_BOUNDARY' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp'; Admission='R26_SUCCESSOR_BOUNDARY' }
    [pscustomobject] @{ Module='TRIADSensorFusionEditor'; Base='TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp'; Admission='R26_SUCCESSOR_BOUNDARY' }
)

$r25UhtOutputRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DContextPolicyActor.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DContextPolicyActor.generated.h'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.generated.h'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.generated.h'
)
$phase2UhtOutputRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DTemasekShophouseProvenance.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DTemasekShophouseProvenance.generated.h'
)
$r26UhtOutputRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DLandmarkVegetationActor.generated.h'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADSensorFusion.init.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.generated.h'
)
$observedUhtMutationRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\Timestamp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\Timestamp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADSensorFusionEditor.init.gen.cpp'
)
$observedCrossPluginUhtMutationRelativePaths = @(
    'Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealEditor\Inc\AirSimTriadRuntime\UHT\Timestamp'
)
$observedGlobalBuildMutationRelativePaths = @(
    'Intermediate\Build\SourceFileCache.bin'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.deps'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtpath'
    'Intermediate\Build\Win64\x64\UnrealEditor\ActionHistory.bin'
    'Intermediate\Build\Win64\x64\UnrealEditor\Development\DependencyCache.bin'
)
$buildAuxiliaryRelativePaths = @(
    foreach ($spec in $compileArtifactSpecs) {
        foreach ($suffix in $compileArtifactSuffixes) {
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
                $spec.Module + '\' + $spec.Base + $suffix
        }
    }
    foreach ($module in @('TRIADSensorFusion', 'TRIADSensorFusionEditor')) {
        foreach ($suffix in @(
                '.dll.rsp', '.dll.rsp.old', '.exp', '.lib',
                '.lib.rsp', '.lib.rsp.old')) {
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
                $module + '\UnrealEditor-' + $module + $suffix
        }
    }
    $r25UhtOutputRelativePaths
    $phase2UhtOutputRelativePaths
    $r26UhtOutputRelativePaths
    $observedUhtMutationRelativePaths
    $observedCrossPluginUhtMutationRelativePaths
    $observedGlobalBuildMutationRelativePaths
)

$dependencyClosureHeaderLeafNames = @(
    'TRIADIstanaExploreV5DTemasekShophouseActor.h'
    'TRIADIstanaExploreV5DTemasekShophouseProvenance.h'
    'TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h'
    'TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h'
)
$expectedDependencyClosureKeys = @(
    'TRIADSensorFusion|TRIADIstanaExploreV5DTemasekShophouseActor.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DTemasekShophouseActor.gen.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DTemasekShophouseProvenance.gen.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.gen.cpp'
)
$requiredDirectCompileLogLeafNames = @(
    'TRIADIstanaExploreV5DTemasekShophouseActor.cpp'
    'TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'
    'TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'
    'TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'
    'TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'
)

$legacyTemasekAssetPins = @(
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_Bark_PBR_R24.uasset'; Bytes=9823L; Sha256='F10C31AD2C57E8E2E06715CC2741C7E8216D2A7DB17029457005BF81F30FD0D4' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_Brass_PBR_R24.uasset'; Bytes=9807L; Sha256='4F1B111C7CD6D013EF7584C6CD138BC287C63C34C28716CD15B06AC3ECF3F310' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_CharcoalTrim_PBR_R24.uasset'; Bytes=10006L; Sha256='BAD26FA7DE4273B764BCC1F54BC8AD3168B2302B3765C8C314D19BE5916C841C' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_DarkWindowGlass_PBR_R24.uasset'; Bytes=9743L; Sha256='7AF62F3CBDB53E5EBFC474475DF6370AF1A97484377B2E6B2E96EFF92C0DC170' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_LeafDeep_PBR_R24.uasset'; Bytes=9925L; Sha256='9F0573D4C87D5C7B87BBF3042EEEFFFA40932C74DA487B79191E530EBD4BAD58' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_LeafLight_PBR_R24.uasset'; Bytes=9936L; Sha256='13DC6F431E6C670C937B83ED38156A3BC3B1AD2A63FCC43931C4C30ED7488D28' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_PaverDark_PBR_R24.uasset'; Bytes=9926L; Sha256='C00A759A8232122BCC5E6AE4EA7BE7CF2DEC757166A4742C298FECD8350225D8' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_PaverLight_PBR_R24.uasset'; Bytes=9943L; Sha256='82E8A1771321B6C77BE4282176C0B9FAEF45AF0CF4A75767CE4F645762C59409' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_PinkGreyMosaic_PBR_R24.uasset'; Bytes=9958L; Sha256='750AFE96931374677B61DA11B53729FA3B919CB9F10018C4A7BCE043B66DFE0F' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_PollinatorBloom_PBR_R24.uasset'; Bytes=10037L; Sha256='193FCDB3FC613A0892FEF5DB26D8E3F1B9D3FC1D270A5ACED6296AE6A02B97FD' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_PrecastConcrete_PBR_R24.uasset'; Bytes=10023L; Sha256='74E7559ED9E8F21F16316B883BF88C82E6C2D7D0DEDEC266C86E8D2150623CEC' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_Rainwater_PBR_R24.uasset'; Bytes=9654L; Sha256='6B74ACEBFB1E9DAF417BD5D8F58DF76DF5D0945274F0E62D9A5196D7BB260951' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_ReusedTimber_PBR_R24.uasset'; Bytes=9896L; Sha256='5188516FD2133B18A1DAECBF036DFD1EB9759CEC6ABDB0E95D3A3270C32C4473' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_ShadowRecess_PBR_R24.uasset'; Bytes=9614L; Sha256='D56D3DCAC0DF4ABC9218F2AD27EFECEF6737F0DB7CD4B407FB448484488A5B80' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_SoilMulch_PBR_R24.uasset'; Bytes=9927L; Sha256='8F4A12FFB249EA67932A9C75C879D7B7A9160FF90894B9A735D486C998F2786B' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_SolarPanel_PBR_R24.uasset'; Bytes=9755L; Sha256='D5795DBDBC454656925CBB6E0C920AB3287C7956A9801F46D0EA57BB53ABAD6F' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_Terracotta_PBR_R24.uasset'; Bytes=10018L; Sha256='49EAB4D51482750E7256A9FED078B413AAE510BDA37843C5204FB287A1CCD2DC' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_Timber_PBR_R24.uasset'; Bytes=9800L; Sha256='55E0A1FE568A44E7F698B9A9C21BCB1CD0C5B979336DD6A6A22353B4F57D70E9' }
    [pscustomobject] [ordered] @{ RelativePath='Materials\M_TSH_WhiteShanghaiPlaster_PBR_R24.uasset'; Bytes=10190L; Sha256='A6B0942ECD8A51051C2B05A20171D038B9393F5E64BAD2CD2A98CFD93FE83EDA' }
    [pscustomobject] [ordered] @{ RelativePath='SM_IPV5D_R24_TemasekShophouse_Render.uasset'; Bytes=243386L; Sha256='D10AA62BE0E2C2552D71FEF25E27C03BD293D4DFE7F15FE3799946B841DA5494' }
)

$phase2PrimaryAssetRelativePaths = @(
    'Materials\M_TSH_Brass_PBR_R24.uasset'
    'Materials\M_TSH_CharcoalTrim_PBR_R24.uasset'
    'Materials\M_TSH_DarkWindowGlass_PBR_R24.uasset'
    'Materials\M_TSH_PaverDark_PBR_R24.uasset'
    'Materials\M_TSH_PaverLight_PBR_R24.uasset'
    'Materials\M_TSH_PinkGreyMosaic_PBR_R24.uasset'
    'Materials\M_TSH_PrecastConcrete_PBR_R24.uasset'
    'Materials\M_TSH_Rainwater_PBR_R24.uasset'
    'Materials\M_TSH_ReusedTimber_PBR_R24.uasset'
    'Materials\M_TSH_ShadowRecess_PBR_R24.uasset'
    'Materials\M_TSH_SoilMulch_PBR_R24.uasset'
    'Materials\M_TSH_SolarPanel_PBR_R24.uasset'
    'Materials\M_TSH_Terracotta_PBR_R24.uasset'
    'Materials\M_TSH_Timber_PBR_R24.uasset'
    'Materials\M_TSH_WhiteShanghaiPlaster_PBR_R24.uasset'
    'SM_IPV5D_R24_TemasekShophouse_Render.uasset'
)
$removedLegacyFoliageAssetRelativePaths = @(
    'Materials\M_TSH_Bark_PBR_R24.uasset'
    'Materials\M_TSH_LeafDeep_PBR_R24.uasset'
    'Materials\M_TSH_LeafLight_PBR_R24.uasset'
    'Materials\M_TSH_PollinatorBloom_PBR_R24.uasset'
)

$immutableContentPins = @(
    [pscustomobject] [ordered] @{ Role='V5C_MASTER_MATERIAL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_ContextMassing_Master.uasset'; Bytes=29065L; Sha256='47B0F9F804858280B53D4D03CEEA8E078F01740920AF589EFEA676EBC4D057B4' }
    [pscustomobject] [ordered] @{ Role='V5C_OFFICIAL_WALL_MATERIAL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRender.uasset'; Bytes=15159L; Sha256='113CE217898FD5A65603493FFF82B4B0FC16C6AD3C0339097F61E78B911E326F' }
    [pscustomobject] [ordered] @{ Role='V5C_OFFICIAL_ROOF_MATERIAL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRoof.uasset'; Bytes=14843L; Sha256='4534461C849FD80A1D1BA34FE68239414B25F1FF31F4790BD857C9F828A8C943' }
    [pscustomobject] [ordered] @{ Role='V5C_FALLBACK_WALL_MATERIAL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRender.uasset'; Bytes=15170L; Sha256='227415242B7F0D0E61F3474C57D605848D24EF7574CD67C2242DED75B22F8079' }
    [pscustomobject] [ordered] @{ Role='V5C_FALLBACK_ROOF_MATERIAL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRoof.uasset'; Bytes=14693L; Sha256='BF11B2C73A066E03923B83AADD9C1222CD80377B44A90EBFAE868454C05898EB' }
    [pscustomobject] [ordered] @{ Role='V2_RENDER_MESH'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'; Bytes=761010L; Sha256='A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE' }
    [pscustomobject] [ordered] @{ Role='R25_MASTER'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\M_IPV5D_ContextFacadeR25_Master.uasset'; Bytes=29079L; Sha256='37C7305FC07DD076902A243410F7CB094356F8CBD333461E29EF3AC47694AC03' }
    [pscustomobject] [ordered] @{ Role='R25_FALLBACK_ROOF'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset'; Bytes=14792L; Sha256='E8F78D776EB235F1F25304678CCAD7F64DA125376BA04126DE2CE3BABCA64A18' }
    [pscustomobject] [ordered] @{ Role='R25_FALLBACK_WALL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackWall.uasset'; Bytes=14599L; Sha256='CD27BC83E1876E53B485F055D5D1F2E48999EE8CEAC733D7AB55F06CC6E81C9B' }
    [pscustomobject] [ordered] @{ Role='R25_OFFICIAL_ROOF'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialRoof.uasset'; Bytes=14709L; Sha256='98DE68DFB79083DF8D690C0222BAED926F414302E840A8C88950FD73DCE729D8' }
    [pscustomobject] [ordered] @{ Role='R25_OFFICIAL_WALL'; RelativePath='Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialWall.uasset'; Bytes=14637L; Sha256='2C783BA3D045FBC37432D117C372C1B6654B94ECDC3E9C8C051263E2492036DA' }
)

$nonAuthoritativeEphemeralStateExclusions = @(
    'Saved\Logs\**'
    'Saved\Crashes\**'
    'Saved\webcache_4430\**'
    'Saved\Autosaves\**'
    'Saved\Config\CrashReportClient\**'
    'Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini'
    'Saved\ShaderDebugInfo\**'
    'Saved\DerivedDataCache\**'
)

$script:selfPin = $null
$script:protectedUE54Before = $null
$script:promotionRows = @()
$script:builtProductPins = @()
$script:builtDllPins = @()
$script:landmarkR26SourceBoundary = $null
$script:buildRows = @()

function Test-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $fullPath.StartsWith(
        $fullRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
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

function Assert-ExistingNonReparseDirectory {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $item = Get-Item -LiteralPath ([IO.Path]::GetFullPath($Path)) `
        -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label must be an existing non-reparse directory: $Path"
    }
}

function Get-FileState {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($fullPath)) {
        if (Test-Path -LiteralPath $fullPath) {
            throw "Expected file state is occupied by a non-file: $fullPath"
        }
        return [pscustomobject] [ordered] @{
            Path = $fullPath
            Present = $false
            Bytes = 0L
            Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if ($item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Expected a regular non-reparse file: $fullPath"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = [string] (Get-FileHash -LiteralPath $fullPath `
            -Algorithm SHA256).Hash
    }
}

function New-ExpectedState {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [bool] $Present,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256
    )

    [pscustomobject] [ordered] @{
        Path = [IO.Path]::GetFullPath($Path)
        Present = $Present
        Bytes = $Bytes
        Sha256 = $Sha256
    }
}

function Assert-SameState {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-FileState -Path $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label changed: expected=$($Expected.Present)/$($Expected.Bytes)/$($Expected.Sha256) actual=$($actual.Present)/$($actual.Bytes)/$($actual.Sha256) path=$Path"
    }
    $actual
}

function Assert-NewPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NoReparseAncestor -Path $Path -Root $Root -Label $Label
    if (Test-Path -LiteralPath $Path) {
        throw "$Label must not overwrite an existing path: $Path"
    }
}

function Copy-NewPinnedFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Source,
        [Parameter(Mandatory = $true)] [string] $Destination,
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $DestinationRoot,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    [void] (Assert-SameState -Expected $Expected -Path $Source `
        -Label "$Label source")
    Assert-NewPath -Path $Destination -Root $DestinationRoot -Label $Label
    $parent = [IO.Path]::GetDirectoryName($Destination)
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        [void] [IO.Directory]::CreateDirectory($parent)
    }
    Assert-NoReparseAncestor -Path $Destination -Root $DestinationRoot `
        -Label $Label
    [IO.File]::Copy($Source, $Destination, $false)
    Assert-SameState -Expected $Expected -Path $Destination `
        -Label "$Label readback"
}

function Test-EquivalentFileState {
    param(
        [Parameter(Mandatory = $true)] $Left,
        [Parameter(Mandatory = $true)] $Right
    )

    [bool] $Left.Present -eq [bool] $Right.Present -and
        [int64] $Left.Bytes -eq [int64] $Right.Bytes -and
        [string] $Left.Sha256 -ceq [string] $Right.Sha256
}

function Replace-ExistingPinnedFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Source,
        [Parameter(Mandatory = $true)] [string] $Destination,
        [Parameter(Mandatory = $true)] $ExpectedSource,
        [Parameter(Mandatory = $true)] $ExpectedDestination,
        [Parameter(Mandatory = $true)] [string] $SourceRoot,
        [Parameter(Mandatory = $true)] [string] $DestinationRoot,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if ([string]::IsNullOrWhiteSpace($Source) -or
        [string]::IsNullOrWhiteSpace($Destination)) {
        throw "$Label refuses an empty source or destination path."
    }
    if (-not [bool] $ExpectedSource.Present -or
        -not [bool] $ExpectedDestination.Present) {
        throw "$Label requires present source and destination states."
    }
    Assert-NoReparseAncestor -Path $Source -Root $SourceRoot -Label $Label
    Assert-NoReparseAncestor -Path $Destination -Root $DestinationRoot `
        -Label $Label
    [void] (Assert-SameState -Expected $ExpectedSource -Path $Source `
        -Label "$Label source")
    [void] (Assert-SameState -Expected $ExpectedDestination `
        -Path $Destination -Label "$Label destination predecessor")
    Assert-ExistingNonReparseDirectory -Path $replaceDisplacedRoot `
        -Label "$Label displaced-audit root"
    $displaced = Join-Path $replaceDisplacedRoot `
        ('{0:D3}.displaced' -f $script:replaceSequence)
    ++$script:replaceSequence
    if ([string]::IsNullOrWhiteSpace($displaced)) {
        throw "$Label did not produce a non-empty displaced audit path."
    }
    Assert-NewPath -Path $displaced -Root $transactionRoot `
        -Label "$Label displaced audit"

    # File.Replace treats an empty optional backup string as an invalid path in
    # this PowerShell/.NET host. Always provide a fresh, non-empty transaction
    # audit path and retain the displaced predecessor as additional evidence.
    [IO.File]::Replace($Source, $Destination, $displaced, $true)
    [void] (Assert-SameState -Expected $ExpectedSource -Path $Destination `
        -Label "$Label successor")
    [void] (Assert-SameState -Expected $ExpectedDestination -Path $displaced `
        -Label "$Label displaced predecessor")
}

function Write-NewJsonReceipt {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] $Value,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NewPath -Path $Path -Root $Root -Label $Label
    $json = $Value | ConvertTo-Json -Depth 24
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($json + "`n")
    $stream = [IO.File]::Open(
        $Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write,
        [IO.FileShare]::None)
    try {
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
    Get-FileState -Path $Path
}

function Restore-ExactFileState {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [AllowNull()] $Backup,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $path = [IO.Path]::GetFullPath([string] $Expected.Path)
    Assert-NoReparseAncestor -Path $path -Root $Root -Label $Label
    $current = Get-FileState -Path $path
    if (Test-EquivalentFileState -Left $current -Right $Expected) {
        return
    }
    if (-not [bool] $Expected.Present) {
        if ([IO.File]::Exists($path)) {
            $item = Get-Item -LiteralPath $path -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) `
                    -ne 0) {
                throw "$Label refuses to remove a reparse file: $path"
            }
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
        }
        [void] (Assert-SameState -Expected $Expected -Path $path `
            -Label "$Label absent restore")
        return
    }
    if ($null -eq $Backup -or
        [string]::IsNullOrWhiteSpace([string] $Backup.Path)) {
        throw "$Label lacks a predecessor backup for $path"
    }
    [void] (Assert-SameState -Expected $Expected -Path $Backup.Path `
        -Label "$Label backup")
    $parent = [IO.Path]::GetDirectoryName($path)
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        [void] [IO.Directory]::CreateDirectory($parent)
    }
    $sibling = $path + '.triad-restore-' + [Guid]::NewGuid().ToString('N')
    Assert-NoReparseAncestor -Path $sibling -Root $Root -Label $Label
    [IO.File]::Copy([string] $Backup.Path, $sibling, $false)
    [void] (Assert-SameState -Expected $Expected -Path $sibling `
        -Label "$Label sibling")
    if ([IO.File]::Exists($path)) {
        $current = Get-FileState -Path $path
        Replace-ExistingPinnedFile -Source $sibling -Destination $path `
            -ExpectedSource $Expected -ExpectedDestination $current `
            -SourceRoot $Root -DestinationRoot $Root -Label $Label
    }
    else {
        [IO.File]::Move($sibling, $path)
    }
    [void] (Assert-SameState -Expected $Expected -Path $path `
        -Label "$Label restored")
}

function Get-RepositoryExpectedState {
    param([Parameter(Mandatory = $true)] $Pin)

    New-ExpectedState -Path ([string] $Pin.Path) -Present $true `
        -Bytes ([int64] $Pin.Bytes) -Sha256 ([string] $Pin.Sha256)
}

function Assert-GeneratorAdmissionPins {
    foreach ($pin in $generatorAdmissionPins) {
        $expected = Get-RepositoryExpectedState -Pin $pin
        [void] (Assert-SameState -Expected $expected -Path $pin.Path `
            -Label "generator input $($pin.Role)")
    }
}

function Get-PromotionRows {
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($pin in $repositorySourcePins) {
        $relative = [string] $pin.RelativePath
        if ([IO.Path]::IsPathRooted($relative) -or
            $relative -match '(^|[\\/])\.\.([\\/]|$)' -or
            -not $seen.Add($relative)) {
            throw "Repository promotion path is not exact and unique: $relative"
        }
        $source = [IO.Path]::GetFullPath((Join-Path `
            $repositoryUnrealRoot $relative))
        $destination = [IO.Path]::GetFullPath((Join-Path `
            $nativeProjectRoot $relative))
        $target = New-ExpectedState -Path $source -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        [void] (Assert-SameState -Expected $target -Path $source `
            -Label 'reviewed Phase-2 repository source')
        $before = New-ExpectedState -Path $destination -Present $true `
            -Bytes ([int64] $pin.NativeBeforeBytes) `
            -Sha256 ([string] $pin.NativeBeforeSha256)
        $rows.Add([pscustomobject] [ordered] @{
            Area = [string] $pin.Area
            Origin = 'REPOSITORY'
            RelativePath = $relative
            Source = $source
            Destination = $destination
            Target = $target
            NativeBefore = $before
            Backup = $null
        })
    }
    foreach ($pin in $generatedPins) {
        $relative = Join-Path $temasekSourceRootRelative `
            ('Generated\' + [string] $pin.Name)
        if (-not $seen.Add($relative)) {
            throw "Generated promotion path is not unique: $relative"
        }
        $staged = [IO.Path]::GetFullPath((Join-Path `
            $transactionRoot ('staged_generated\' + [string] $pin.Name)))
        $destination = [IO.Path]::GetFullPath((Join-Path `
            $nativeProjectRoot $relative))
        $target = New-ExpectedState -Path $staged -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        $before = New-ExpectedState -Path $destination -Present $true `
            -Bytes ([int64] $pin.NativeBeforeBytes) `
            -Sha256 ([string] $pin.NativeBeforeSha256)
        $rows.Add([pscustomobject] [ordered] @{
            Area = 'PHASE2_GENERATED'
            Origin = 'STAGED_DETERMINISTIC'
            RelativePath = $relative
            Source = $staged
            Destination = $destination
            Target = $target
            NativeBefore = $before
            Backup = $null
        })
    }
    if ($rows.Count -ne 14 -or $seen.Count -ne 14) {
        throw "Phase-2 promotion roster must contain exactly fourteen files; actual=$($rows.Count)."
    }
    @($rows)
}

function Assert-RepositorySourcesUnchanged {
    Assert-GeneratorAdmissionPins
    foreach ($row in @($script:promotionRows | Where-Object {
                $_.Origin -ceq 'REPOSITORY' })) {
        [void] (Assert-SameState -Expected $row.Target -Path $row.Source `
            -Label 'Phase-2 repository source boundary')
    }
}

function Assert-StagedGeneratedPayload {
    $root = Join-Path $transactionRoot 'staged_generated'
    Assert-ExistingNonReparseDirectory -Path $root `
        -Label 'Phase-2 staged payload root'
    $files = @(Get-ChildItem -LiteralPath $root -File -Force |
        Sort-Object Name)
    if ($files.Count -ne 7 -or
        @(Get-ChildItem -LiteralPath $root -Directory -Force).Count -ne 0) {
        throw 'Phase-2 staged payload must contain exactly seven files and no directories.'
    }
    foreach ($row in @($script:promotionRows | Where-Object {
                $_.Origin -ceq 'STAGED_DETERMINISTIC' })) {
        [void] (Assert-SameState -Expected $row.Target -Path $row.Source `
            -Label 'Phase-2 deterministic staged output')
    }
}

function Assert-NativeSourcePredecessors {
    foreach ($row in $script:promotionRows) {
        [void] (Assert-SameState -Expected $row.NativeBefore `
            -Path $row.Destination -Label 'Phase-2 native source predecessor')
    }
}

function Assert-NativeSourceTargets {
    foreach ($row in $script:promotionRows) {
        [void] (Assert-SameState -Expected $row.Target `
            -Path $row.Destination -Label 'Phase-2 native source target')
    }
}

function Assert-UnchangedNativeSources {
    foreach ($pin in $unchangedNativeSourcePins) {
        $workspace = [IO.Path]::GetFullPath((Join-Path `
            $repositoryUnrealRoot ([string] $pin.RelativePath)))
        $native = [IO.Path]::GetFullPath((Join-Path `
            $nativeProjectRoot ([string] $pin.RelativePath)))
        $expectedWorkspace = New-ExpectedState -Path $workspace `
            -Present $true -Bytes ([int64] $pin.Bytes) `
            -Sha256 ([string] $pin.Sha256)
        $expectedNative = New-ExpectedState -Path $native `
            -Present $true -Bytes ([int64] $pin.Bytes) `
            -Sha256 ([string] $pin.Sha256)
        [void] (Assert-SameState -Expected $expectedWorkspace `
            -Path $workspace -Label 'unchanged Phase-2 workspace dependency')
        [void] (Assert-SameState -Expected $expectedNative `
            -Path $native -Label 'unchanged Phase-2 native dependency')
    }
}

function Assert-ImmutableContentPins {
    foreach ($pin in $immutableContentPins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ([string] $pin.RelativePath)))
        $expected = New-ExpectedState -Path $path -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        [void] (Assert-SameState -Expected $expected -Path $path `
            -Label "immutable $($pin.Role)")
    }
}

function Assert-BuildRollbackRosterDefinition {
    if ($compileArtifactSuffixes.Count -ne 5 -or
        $compileArtifactSpecs.Count -ne 26 -or
        $r25UhtOutputRelativePaths.Count -ne 6 -or
        $phase2UhtOutputRelativePaths.Count -ne 2 -or
        $r26UhtOutputRelativePaths.Count -ne 5 -or
        $observedUhtMutationRelativePaths.Count -ne 3 -or
        $observedCrossPluginUhtMutationRelativePaths.Count -ne 1 -or
        $observedGlobalBuildMutationRelativePaths.Count -ne 6 -or
        $nonAuthoritativeEphemeralStateExclusions.Count -ne 8) {
        throw 'Phase-2 build rollback component rosters drifted from the reviewed 172-state post-R26 contract.'
    }
    $relativePaths = @(
        $buildProductPins | ForEach-Object { [string] $_.RelativePath }
        $buildAuxiliaryRelativePaths
    )
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $relativePaths) {
        if ([string]::IsNullOrWhiteSpace([string] $relative) -or
            [IO.Path]::IsPathRooted([string] $relative) -or
            [string] $relative -match '(^|[\\/])\.\.([\\/]|$)' -or
            -not $seen.Add([string] $relative)) {
            throw "Invalid or duplicate Phase-2 build rollback path: $relative"
        }
    }
    if ($buildProductPins.Count -ne 7 -or
        @($buildProductPins | Where-Object { $_.ExplicitPin }).Count -ne 2 -or
        $buildAuxiliaryRelativePaths.Count -ne 165 -or
        $relativePaths.Count -ne 172 -or $seen.Count -ne 172) {
        throw 'Phase-2 build rollback roster must contain exactly 7 binary product states plus 165 scoped post-R26 build/UHT states.'
    }
    [pscustomobject] [ordered] @{
        BinaryProductCount = 7
        ExplicitCallerPinnedBinaryCount = 2
        DynamicallyCapturedBinaryProductCount = 5
        ScopedBuildAndUhtCount = 165
        R26SuccessorBuildAndUhtAdditionCount = 30
        TotalBuildProductCount = 172
        WholeNativeTreeRollbackClaimed = $false
        NonAuthoritativeEphemeralStateExclusions =
            $nonAuthoritativeEphemeralStateExclusions
    }
}

function Get-PinnedBuildProductRows {
    [void] (Assert-BuildRollbackRosterDefinition)
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($pin in $buildProductPins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ([string] $pin.RelativePath)))
        $before = if ([bool] $pin.ExplicitPin) {
            $expected = New-ExpectedState -Path $path -Present $true `
                -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
            [void] (Assert-SameState -Expected $expected -Path $path `
                -Label 'caller-pinned predecessor DLL')
            $expected
        }
        else {
            $captured = Get-FileState -Path $path
            if (-not $captured.Present -or $captured.Bytes -le 0) {
                throw "Dynamic build predecessor is absent or empty: $path"
            }
            $captured
        }
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath = [string] $pin.RelativePath
            Path = $path
            Before = $before
            Backup = $null
            After = $null
            PinnedBinary = [bool] $pin.ExplicitPin
        })
        [void] $seen.Add([string] $pin.RelativePath)
    }
    foreach ($relative in $buildAuxiliaryRelativePaths) {
        if (-not $seen.Add([string] $relative)) {
            throw "Duplicate Phase-2 build rollback path: $relative"
        }
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ([string] $relative)))
        Assert-NoReparseAncestor -Path $path -Root $nativeProjectRoot `
            -Label 'Phase-2 auxiliary build product'
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath = [string] $relative
            Path = $path
            Before = Get-FileState -Path $path
            Backup = $null
            After = $null
            PinnedBinary = $false
        })
    }
    if ($rows.Count -ne 172 -or $seen.Count -ne 172) {
        throw "Phase-2 build rollback roster count drifted: $($rows.Count)."
    }
    @($rows)
}

function Get-ReviewedBuildDependencyClosure {
    if ($dependencyClosureHeaderLeafNames.Count -ne 4 -or
        $expectedDependencyClosureKeys.Count -ne 9) {
        throw 'Phase-2 dependency-closure definition drifted.'
    }
    $headers = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($leaf in $dependencyClosureHeaderLeafNames) {
        [void] $headers.Add([string] $leaf)
    }
    $expected = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($key in $expectedDependencyClosureKeys) {
        [void] $expected.Add([string] $key)
    }
    $actual = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $evidence = [Collections.Generic.List[object]]::new()
    foreach ($module in @('TRIADSensorFusion', 'TRIADSensorFusionEditor')) {
        $directory = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ('Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
                $module)))
        Assert-ExistingNonReparseDirectory -Path $directory `
            -Label "Phase-2 $module dependency directory"
        foreach ($file in @(Get-ChildItem -LiteralPath $directory `
                -Filter '*.dep.json' -File -Force | Sort-Object Name)) {
            $before = Get-FileState -Path $file.FullName
            $document = Get-Content -LiteralPath $file.FullName -Raw |
                ConvertFrom-Json -Depth 20
            [void] (Assert-SameState -Expected $before -Path $file.FullName `
                -Label 'stable Phase-2 compiler dependency evidence')
            $matches = @($document.Data.Includes | ForEach-Object {
                    [IO.Path]::GetFileName([string] $_)
                } | Where-Object { $headers.Contains($_) } |
                Sort-Object -Unique)
            if ($matches.Count -eq 0) { continue }
            $base = $file.Name.Substring(0, $file.Name.Length - 9)
            $key = "$module|$base"
            if (-not $expected.Contains($key)) {
                throw "Unjournalled Phase-2 dependency translation unit: $key"
            }
            [void] $actual.Add($key)
            $evidence.Add([pscustomobject] [ordered] @{
                Key = $key
                DependencyFile = $before
                MatchedHeaders = $matches
            })
        }
    }
    if ($actual.Count -ne 9) {
        $missing = @($expected | Where-Object { -not $actual.Contains($_) } |
            Sort-Object)
        throw "Phase-2 dependency closure incomplete: actual=$($actual.Count)/9 missing=$([string]::Join(',', $missing))"
    }
    [pscustomobject] [ordered] @{
        Status = 'EXACT_9_PHASE2_DEPENDENCY_CLOSURE_COVERED'
        Count = 9
        TranslationUnits = @($actual | Sort-Object)
        Evidence = @($evidence | Sort-Object Key)
    }
}

function Remove-ExactPhase2CompileObjectsForForcedRebuild {
    param([Parameter(Mandatory = $true)] [object[]] $BuildRows)

    if ($expectedDependencyClosureKeys.Count -ne 9) {
        throw 'Forced Phase-2 compile-object roster drifted from nine translation units.'
    }
    $removed = [Collections.Generic.List[object]]::new()
    foreach ($key in $expectedDependencyClosureKeys) {
        $parts = [string] $key -split '\|', 2
        if ($parts.Count -ne 2) {
            throw "Invalid Phase-2 dependency-closure key: $key"
        }
        $relative =
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
            $parts[0] + '\' + $parts[1] + '.obj'
        $matches = @($BuildRows | Where-Object {
                [string] $_.RelativePath -ceq $relative
            })
        if ($matches.Count -ne 1) {
            throw "Forced Phase-2 compile object lacks one exact rollback row: $relative"
        }
        $row = $matches[0]
        [void] (Assert-SameState -Expected $row.Before -Path $row.Path `
            -Label 'forced Phase-2 compile object predecessor')
        if ([IO.File]::Exists([string] $row.Path)) {
            $item = Get-Item -LiteralPath $row.Path -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) `
                    -ne 0) {
                throw "Forced Phase-2 compile object is a reparse file: $($row.Path)"
            }
            Remove-Item -LiteralPath $row.Path -Force -ErrorAction Stop
        }
        $absent = New-ExpectedState -Path $row.Path -Present $false `
            -Bytes 0L -Sha256 'ABSENT'
        [void] (Assert-SameState -Expected $absent -Path $row.Path `
            -Label 'forced Phase-2 compile object invalidation')
        $removed.Add([pscustomobject] [ordered] @{
            Key = [string] $key
            RelativePath = $relative
            Path = [string] $row.Path
            Predecessor = $row.Before
        })
    }
    if ($removed.Count -ne 9) {
        throw "Forced Phase-2 compile-object count drifted: $($removed.Count)."
    }
    @($removed)
}

function Get-LandmarkR26NativeReadiness {
    $states = [Collections.Generic.List[object]]::new()
    foreach ($relative in $landmarkR26NativeSourceRelativePaths) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        Assert-NoReparseAncestor -Path $path -Root $nativeProjectRoot `
            -Label 'Landmark Vegetation R26 native source boundary'
        $states.Add((Get-FileState -Path $path))
    }
    $allPresent = @($states | Where-Object { -not $_.Present }).Count -eq 0
    $markerEvidence = $false
    if ($allPresent) {
        $combined = [string]::Join("`n", @($states | ForEach-Object {
                    Get-Content -LiteralPath $_.Path -Raw
                }))
        $requiredMarkers = @(
            'ATRIADIstanaExploreV5DLandmarkVegetationActor'
            'ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap'
            'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap'
            'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID:'
            'landmarkVegetationR26=true'
            'mapIntegrated=true'
        )
        $markerEvidence = @($requiredMarkers | Where-Object {
                -not $combined.Contains($_, [StringComparison]::Ordinal)
            }).Count -eq 0
    }
    [pscustomobject] [ordered] @{
        Ready = [bool] ($allPresent -and $markerEvidence)
        AllSixSourceFilesPresent = [bool] $allPresent
        RequiredMarkersPresent = [bool] $markerEvidence
        States = @($states)
        RequiredColdEndpoint =
            'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap'
    }
}

function Assert-LandmarkR26NativeSourceReady {
    if (-not $RequireLandmarkVegetationR26) {
        throw 'Live Phase-2 execution requires -RequireLandmarkVegetationR26.'
    }
    $readiness = Get-LandmarkR26NativeReadiness
    if (-not $readiness.Ready) {
        throw 'Landmark Vegetation R26 native sources/endpoints are not ready; Phase 2 refuses to remove baked foliage before the separate R26 transaction lands.'
    }
    $readiness
}

function Assert-LandmarkR26SourceBoundaryUnchanged {
    if ($null -eq $script:landmarkR26SourceBoundary) {
        throw 'Landmark Vegetation R26 source boundary was not initialized.'
    }
    foreach ($state in $script:landmarkR26SourceBoundary.States) {
        [void] (Assert-SameState -Expected $state -Path $state.Path `
            -Label 'Landmark Vegetation R26 source boundary')
    }
    $readiness = Get-LandmarkR26NativeReadiness
    if (-not $readiness.Ready) {
        throw 'Landmark Vegetation R26 readiness markers changed.'
    }
}

function Get-LegacyTemasekAssetRows {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($pin in $legacyTemasekAssetPins) {
        $path = [IO.Path]::GetFullPath((Join-Path $temasekAssetRoot `
            ([string] $pin.RelativePath)))
        $expected = New-ExpectedState -Path $path -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath = [string] $pin.RelativePath
            Path = $path
            Before = $expected
            Backup = $null
        })
    }
    if ($rows.Count -ne 20) {
        throw 'Legacy Temasek predecessor roster must contain twenty packages.'
    }
    @($rows)
}

function Get-KnownTemasekArtifactInventory {
    $rootItem = Get-Item -LiteralPath $temasekAssetRoot -Force `
        -ErrorAction Stop
    if (-not $rootItem.PSIsContainer -or
        ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Temasek asset root is not a regular directory: $temasekAssetRoot"
    }
    $materialsRoot = [IO.Path]::GetFullPath((Join-Path `
        $temasekAssetRoot 'Materials'))
    $allowedDirectories = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    [void] $allowedDirectories.Add($materialsRoot)
    foreach ($directory in @(Get-ChildItem -LiteralPath $temasekAssetRoot `
            -Directory -Recurse -Force)) {
        $full = [IO.Path]::GetFullPath($directory.FullName)
        if (($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) `
                -ne 0 -or -not $allowedDirectories.Contains($full)) {
            throw "Unexpected/reparse directory in Temasek asset root: $full"
        }
    }
    $knownStems = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in $legacyTemasekAssetPins) {
        $path = [IO.Path]::GetFullPath((Join-Path $temasekAssetRoot `
            ([string] $pin.RelativePath)))
        [void] $knownStems.Add([IO.Path]::ChangeExtension($path, $null))
    }
    $allowedExtensions = @('.uasset', '.uexp', '.ubulk', '.uptnl')
    $files = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $temasekAssetRoot `
            -File -Recurse -Force | Sort-Object FullName)) {
        $full = [IO.Path]::GetFullPath($file.FullName)
        Assert-NoReparseAncestor -Path $full -Root $nativeProjectRoot `
            -Label 'known Temasek package artifact'
        if ([IO.Path]::GetExtension($full) -notin $allowedExtensions -or
            -not $knownStems.Contains([IO.Path]::ChangeExtension(
                    $full, $null))) {
            throw "Unexpected artifact in exact Temasek asset root: $full"
        }
        $files.Add((Get-FileState -Path $full))
    }
    [pscustomobject] [ordered] @{
        Root = $temasekAssetRoot
        Files = @($files)
        FileCount = $files.Count
    }
}

function Assert-LegacyTemasekAssetPredecessor {
    $inventory = Get-KnownTemasekArtifactInventory
    if ($inventory.FileCount -ne 20) {
        throw "Legacy Temasek predecessor must contain exactly twenty package files; actual=$($inventory.FileCount)."
    }
    foreach ($row in $script:legacyAssetRows) {
        [void] (Assert-SameState -Expected $row.Before -Path $row.Path `
            -Label 'legacy Temasek package predecessor')
    }
    $inventory
}

function Get-Phase2TemasekAssetInventory {
    $inventory = Get-KnownTemasekArtifactInventory
    if ($inventory.FileCount -ne 16) {
        throw "Phase-2 Temasek successor must contain exactly sixteen package files; actual=$($inventory.FileCount)."
    }
    $states = [Collections.Generic.List[object]]::new()
    foreach ($relative in $phase2PrimaryAssetRelativePaths) {
        $path = [IO.Path]::GetFullPath((Join-Path $temasekAssetRoot $relative))
        $state = Get-FileState -Path $path
        if (-not $state.Present -or $state.Bytes -le 0) {
            throw "Phase-2 Temasek primary package is absent/empty: $path"
        }
        $states.Add($state)
    }
    foreach ($relative in $removedLegacyFoliageAssetRelativePaths) {
        $path = [IO.Path]::GetFullPath((Join-Path $temasekAssetRoot $relative))
        $absent = New-ExpectedState -Path $path -Present $false `
            -Bytes 0L -Sha256 'ABSENT'
        [void] (Assert-SameState -Expected $absent -Path $path `
            -Label 'removed baked-foliage material')
    }
    [pscustomobject] [ordered] @{
        Root = $temasekAssetRoot
        Complete = $true
        Primary = @($states)
        Artifacts = @($inventory.Files)
        AssetCount = 16
        RemovedLegacyFoliageMaterialCount = 4
    }
}

function Assert-Phase2TemasekAssetInventorySame {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-Phase2TemasekAssetInventory
    foreach ($state in $Expected.Artifacts) {
        [void] (Assert-SameState -Expected $state -Path $state.Path `
            -Label $Label)
    }
    $actual
}

function Remove-ExactLegacyTemasekAssetPredecessor {
    Assert-NativeProjectIdle -Checkpoint 'before exact Temasek asset evacuation'
    [void] (Assert-LegacyTemasekAssetPredecessor)
    foreach ($row in $script:legacyAssetRows) {
        Assert-NoReparseAncestor -Path $row.Path -Root $temasekAssetRoot `
            -Label 'legacy Temasek package evacuation'
        Remove-Item -LiteralPath $row.Path -Force -ErrorAction Stop
    }
    $after = Get-KnownTemasekArtifactInventory
    if ($after.FileCount -ne 0) {
        throw 'Exact Temasek package evacuation left package artifacts behind.'
    }
    [pscustomobject] [ordered] @{
        Stage = 'EVACUATE_EXACT_20_LEGACY_TEMASEK_PACKAGES'
        Status = 'PASS'
        RemovedByExactFilePath = 20
        RecursiveDeleteUsed = $false
        VerifiedBackupsAlreadyDurable = $true
    }
}

function New-VerifiedTransactionBackups {
    param([Parameter(Mandatory = $true)] [object[]] $BuildRows)

    Assert-NativeProjectIdle -Checkpoint 'before Phase-2 backups'
    $sourceRoot = Join-Path $transactionRoot 'source_before'
    $buildRoot = Join-Path $transactionRoot 'build_before'
    $contentRoot = Join-Path $transactionRoot 'content_before'
    $assetRoot = Join-Path $contentRoot 'legacy_temasek_assets'
    foreach ($root in @(
            $sourceRoot, $buildRoot, $contentRoot, $assetRoot,
            $replaceDisplacedRoot)) {
        Assert-NewPath -Path $root -Root $transactionRoot `
            -Label 'Phase-2 backup directory'
        [void] [IO.Directory]::CreateDirectory($root)
    }
    for ($index = 0; $index -lt $script:promotionRows.Count; ++$index) {
        $row = $script:promotionRows[$index]
        $backup = Join-Path $sourceRoot ('{0:D2}.predecessor' -f $index)
        [void] (Copy-NewPinnedFile -Source $row.Destination `
            -Destination $backup -Expected $row.NativeBefore `
            -DestinationRoot $transactionRoot `
            -Label 'source predecessor backup')
        $row.Backup = Get-FileState -Path $backup
    }
    for ($index = 0; $index -lt $BuildRows.Count; ++$index) {
        $row = $BuildRows[$index]
        if ($row.Before.Present) {
            $backup = Join-Path $buildRoot ('{0:D3}.predecessor' -f $index)
            [void] (Copy-NewPinnedFile -Source $row.Path `
                -Destination $backup -Expected $row.Before `
                -DestinationRoot $transactionRoot `
                -Label 'build predecessor backup')
            $row.Backup = Get-FileState -Path $backup
        }
    }
    $mapBackup = Join-Path $contentRoot 'hybrid_map.predecessor.umap'
    [void] (Copy-NewPinnedFile -Source $hybridMapFile `
        -Destination $mapBackup -Expected $expectedPredecessorMapPin `
        -DestinationRoot $transactionRoot -Label 'map predecessor backup')
    for ($index = 0; $index -lt $script:legacyAssetRows.Count; ++$index) {
        $row = $script:legacyAssetRows[$index]
        $backup = Join-Path $assetRoot ('{0:D2}.uasset' -f $index)
        [void] (Copy-NewPinnedFile -Source $row.Path -Destination $backup `
            -Expected $row.Before -DestinationRoot $transactionRoot `
            -Label 'legacy Temasek asset backup')
        $row.Backup = Get-FileState -Path $backup
    }
    [pscustomobject] [ordered] @{
        MapBackup = Get-FileState -Path $mapBackup
        SourceBackupCount = 14
        BuildBackupCount = @($BuildRows | Where-Object {
                $_.Before.Present }).Count
        LegacyAssetBackupCount = 20
    }
}

function Publish-NativeSources {
    Assert-NativeProjectIdle -Checkpoint 'before Phase-2 source promotion'
    Assert-RepositorySourcesUnchanged
    Assert-StagedGeneratedPayload
    Assert-NativeSourcePredecessors
    for ($index = 0; $index -lt $script:promotionRows.Count; ++$index) {
        $row = $script:promotionRows[$index]
        $stage = Join-Path $transactionRoot `
            ('source_stage\{0:D2}.successor' -f $index)
        [void] (Copy-NewPinnedFile -Source $row.Source -Destination $stage `
            -Expected $row.Target -DestinationRoot $transactionRoot `
            -Label 'Phase-2 promoted source stage')
        Replace-ExistingPinnedFile -Source $stage `
            -Destination $row.Destination -ExpectedSource $row.Target `
            -ExpectedDestination $row.NativeBefore `
            -SourceRoot $transactionRoot -DestinationRoot $nativeProjectRoot `
            -Label 'Phase-2 promoted source'
    }
    Assert-NativeSourceTargets
}

function Restore-NativeSources {
    for ($index = $script:promotionRows.Count - 1; $index -ge 0; --$index) {
        $row = $script:promotionRows[$index]
        Restore-ExactFileState -Expected $row.NativeBefore `
            -Backup $row.Backup -Root $nativeProjectRoot `
            -Label 'Phase-2 source rollback'
    }
    Assert-NativeSourcePredecessors
}

function Restore-BuildProducts {
    param([Parameter(Mandatory = $true)] [object[]] $BuildRows)

    for ($index = $BuildRows.Count - 1; $index -ge 0; --$index) {
        $row = $BuildRows[$index]
        Restore-ExactFileState -Expected $row.Before -Backup $row.Backup `
            -Root $nativeProjectRoot -Label 'Phase-2 build rollback'
    }
}

function Restore-Phase2Content {
    param([Parameter(Mandatory = $true)] $MapBackup)

    Assert-NativeProjectIdle -Checkpoint 'before Phase-2 content rollback'
    $inventory = Get-KnownTemasekArtifactInventory
    foreach ($state in $inventory.Files) {
        Assert-NoReparseAncestor -Path $state.Path -Root $temasekAssetRoot `
            -Label 'Phase-2 successor cleanup'
        Remove-Item -LiteralPath $state.Path -Force -ErrorAction Stop
    }
    foreach ($row in $script:legacyAssetRows) {
        Restore-ExactFileState -Expected $row.Before -Backup $row.Backup `
            -Root $nativeProjectRoot -Label 'legacy Temasek asset rollback'
    }
    Restore-ExactFileState -Expected $expectedPredecessorMapPin `
        -Backup $MapBackup -Root $nativeProjectRoot `
        -Label 'Phase-2 map rollback'
    [void] (Assert-LegacyTemasekAssetPredecessor)
}

function Invoke-Phase2StagingBuild {
    Assert-GeneratorAdmissionPins
    $pythonCommand = Get-Command python -CommandType Application `
        -ErrorAction Stop | Select-Object -First 1
    $python = [IO.Path]::GetFullPath([string] $pythonCommand.Source)
    $pythonState = Get-FileState -Path $python
    if (-not $pythonState.Present -or $pythonState.Bytes -le 0) {
        throw 'The resolved Python executable is absent or empty.'
    }
    $stagingRoot = [IO.Path]::GetFullPath((Join-Path `
        $transactionRoot 'staged_generated'))
    Assert-NewPath -Path $stagingRoot -Root $transactionRoot `
        -Label 'fresh deterministic Phase-2 staging root'

    $builderOutput = @(& $python $builderFile --output-dir $stagingRoot 2>&1 |
        ForEach-Object { [string] $_ })
    $builderExitCode = $LASTEXITCODE
    if ($builderExitCode -ne 0) {
        throw "Phase-2 deterministic builder failed with exit code ${builderExitCode}: $([string]::Join(' | ', $builderOutput))"
    }
    Assert-StagedGeneratedPayload
    $validatorOutput = @(& $python $validatorFile --output-dir $stagingRoot `
        2>&1 | ForEach-Object { [string] $_ })
    $validatorExitCode = $LASTEXITCODE
    if ($validatorExitCode -ne 0) {
        throw "Phase-2 validator failed with exit code ${validatorExitCode}: $([string]::Join(' | ', $validatorOutput))"
    }
    $validation = ([string]::Join("`n", $validatorOutput) |
        ConvertFrom-Json -Depth 24)
    if ($validation.status -cne 'PASS' -or
        [int] $validation.counts.vertices -ne 9536 -or
        [int] $validation.counts.triangles -ne 15760 -or
        [int] $validation.counts.components -ne 828 -or
        [int] $validation.counts.materials -ne 15 -or
        $validation.deterministicRebuild -ne $true -or
        $validation.sourceOnlyNotLiveIntegrated -ne $true -or
        [int] $validation.foliageLayout.bakedRenderComponentCount -ne 0 -or
        @($validation.foliageLayout.treeAnchors).Count -ne 3) {
        throw 'Phase-2 staged validation lost its exact census, deterministic rebuild, source-only boundary, or foliage handoff.'
    }
    Assert-GeneratorAdmissionPins
    Assert-StagedGeneratedPayload
    $receipt = [pscustomobject] [ordered] @{
        Stage = 'GENERATE_AND_VALIDATE_PHASE2_PAYLOAD'
        Status = 'PASS'
        Python = $pythonState
        BuilderExitCode = [int] $builderExitCode
        ValidatorExitCode = [int] $validatorExitCode
        BuilderOutput = $builderOutput
        Validator = $validation
        Generated = @($script:promotionRows | Where-Object {
                $_.Origin -ceq 'STAGED_DETERMINISTIC'
            } | ForEach-Object { Get-FileState -Path $_.Source })
    }
    [void] (Write-NewJsonReceipt `
        -Path (Join-Path $transactionRoot 'phase2_staging.json') `
        -Value $receipt -Root $transactionRoot `
        -Label 'Phase-2 staging receipt')
    $receipt
}

function Test-ExactCommandLineToken {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [string] $Token
    )

    $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($Token) +
        '"?(?:\s|$)'
    [regex]::Matches($CommandLine, $pattern).Count -eq 1
}

function Get-ProcessIdentity {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $row = Get-CimInstance Win32_Process `
        -Filter "ProcessId = $ProcessId" -ErrorAction SilentlyContinue
    if ($null -eq $row) { return $null }
    $creation = [DateTimeOffset] $row.CreationDate
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $row.ProcessId
        CreationUtcTicks = [int64] $creation.UtcTicks
        Name = [string] $row.Name
        ExecutablePath = if ([string]::IsNullOrWhiteSpace(
                [string] $row.ExecutablePath)) { '' }
            else { [IO.Path]::GetFullPath([string] $row.ExecutablePath) }
        CommandLine = [string] $row.CommandLine
    }
}

function Test-ProcessIdentityEqual {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [AllowNull()] $Actual
    )

    $null -ne $Actual -and
        [uint32] $Actual.ProcessId -eq [uint32] $Expected.ProcessId -and
        [int64] $Actual.CreationUtcTicks -eq
            [int64] $Expected.CreationUtcTicks -and
        [string] $Actual.Name -ceq [string] $Expected.Name -and
        [string] $Actual.ExecutablePath -ceq
            [string] $Expected.ExecutablePath -and
        [string] $Actual.CommandLine -ceq [string] $Expected.CommandLine
}

function Get-ProtectedUE54Identity {
    $matches = @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -and
                -not [string]::IsNullOrWhiteSpace(
                    [string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $protectedUE54Project)
        })
    $identities = @($matches | ForEach-Object {
        $process = $_
        $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                [string] $process.ExecutablePath)) { '' }
            else { [IO.Path]::GetFullPath([string] $process.ExecutablePath) }
        if ($process.Name -cne 'UnrealEditor.exe' -or
            $actualExecutable -ine $protectedUE54Editor) {
            throw 'The CAPSTONE project token is owned by an unexpected process; refusing the Phase-2 transaction.'
        }
        [pscustomobject] [ordered] @{
            ProcessId = [uint32] $process.ProcessId
            CreationUtcTicks =
                [int64] ([DateTimeOffset] $process.CreationDate).UtcTicks
            Name = [string] $process.Name
            ExecutablePath = $actualExecutable
            ProjectPath = $protectedUE54Project
            CommandLine = [string] $process.CommandLine
        }
    } | Sort-Object ProcessId)
    [pscustomobject] [ordered] @{
        State = if ($identities.Count -eq 0) { 'ABSENT' } else { 'PRESENT' }
        SessionCount = $identities.Count
        Sessions = @($identities)
    }
}

function Assert-ProtectedUE54Unchanged {
    if ($null -eq $script:protectedUE54Before) {
        throw 'Protected UE5.4/CAPSTONE identity set was not initialized.'
    }
    $after = Get-ProtectedUE54Identity
    if (($script:protectedUE54Before | ConvertTo-Json -Compress -Depth 8) `
            -cne ($after | ConvertTo-Json -Compress -Depth 8)) {
        throw 'Protected UE5.4/CAPSTONE identity set changed during the Phase-2 transaction.'
    }
    $after
}

function Get-NativeTRIADUnrealProcesses {
    $enginePrefix = $nativeUE55EngineRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            if ($_.Name -notin @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe')) {
                return $false
            }
            $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                    [string] $_.ExecutablePath)) { '' }
                else { [IO.Path]::GetFullPath([string] $_.ExecutablePath) }
            $isUE55 = $actualExecutable.StartsWith(
                $enginePrefix, [StringComparison]::OrdinalIgnoreCase)
            $isNativeTRIAD = -not [string]::IsNullOrWhiteSpace(
                    [string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $nativeProjectFile)
            $isUE55 -or $isNativeTRIAD
        })
}

function Get-RemoteControlListeners {
    $rows = & "$env:SystemRoot\System32\netstat.exe" -ano -p TCP
    if ($LASTEXITCODE -ne 0) {
        throw 'netstat failed while checking Remote Control ownership.'
    }
    @(
        foreach ($line in $rows) {
            if ($line -match
                '^\s*TCP\s+\S+:30010\s+\S+\s+LISTENING\s+(\d+)\s*$') {
                [uint32] $Matches[1]
            }
        })
}

function Assert-NativeProjectIdle {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    [void] (Assert-ProtectedUE54Unchanged)
    $editors = @(Get-NativeTRIADUnrealProcesses)
    if ($editors.Count -ne 0) {
        throw "UE5.5/native TRIAD editors must be idle at '$Checkpoint' (PIDs=$([string]::Join(',', @($editors.ProcessId))))."
    }
    $listeners = @(Get-RemoteControlListeners)
    if ($listeners.Count -ne 0) {
        throw "RC port 30010 must be unowned at '$Checkpoint' (PIDs=$([string]::Join(',', $listeners)))."
    }
}

function Assert-LaunchedProcessIdentity {
    param([Parameter(Mandatory = $true)] $Expected)

    $actual = Get-ProcessIdentity -ProcessId ([uint32] $Expected.ProcessId)
    if (-not (Test-ProcessIdentityEqual -Expected $Expected -Actual $actual)) {
        throw "Owned UE5.5 helper changed identity or disappeared: PID=$($Expected.ProcessId)."
    }
    $actual
}

function Test-ExpectedHelperLaunchIdentity {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] $Identity,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $MapPackage,
        [Parameter(Mandatory = $true)] [string] $LogFile
    )

    if ($null -eq $Identity -or $Identity.Name -cne 'UnrealEditor.exe' -or
        [string]::IsNullOrWhiteSpace($Identity.ExecutablePath) -or
        [string]::IsNullOrWhiteSpace($Identity.CommandLine) -or
        [IO.Path]::GetFullPath($Identity.ExecutablePath) -ine $editor -or
        [uint32] $Identity.ProcessId -ne [uint32] $Handle.Id) {
        return $false
    }
    $projectPattern =
        '(?i)(?:^|\s)(?:"[^"]+\.uproject"|[^\s"]+\.uproject)(?:\s|$)'
    [regex]::Matches($Identity.CommandLine, $projectPattern).Count -eq 1 -and
        (Test-ExactCommandLineToken $Identity.CommandLine `
            $nativeProjectFile) -and
        (Test-ExactCommandLineToken $Identity.CommandLine $MapPackage) -and
        $Identity.CommandLine.Contains(
            '-NoAutoSave', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            '-RemoteControlHttpServer', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            '-RCWebControlEnable', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            'WebControl.StartServer', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            $LogFile, [StringComparison]::OrdinalIgnoreCase) -and
        [Math]::Abs($Identity.CreationUtcTicks -
            $Handle.StartTime.ToUniversalTime().Ticks) -le
                [TimeSpan]::FromSeconds(2).Ticks
}

function Assert-WorkspaceAndNativeBoundary {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    [void] (Assert-SameState -Expected $script:selfPin -Path $PSCommandPath `
        -Label "wrapper at $Checkpoint")
    Assert-RepositorySourcesUnchanged
    Assert-StagedGeneratedPayload
    Assert-NativeSourceTargets
    Assert-UnchangedNativeSources
    Assert-LandmarkR26SourceBoundaryUnchanged
    [void] (Assert-SameState -Expected $expectedProjectPin `
        -Path $nativeProjectFile -Label "project at $Checkpoint")
    Assert-ImmutableContentPins
    foreach ($pin in $script:builtProductPins) {
        [void] (Assert-SameState -Expected $pin -Path $pin.Path `
            -Label "built product at $Checkpoint")
    }
}

function Assert-PredecessorBoundary {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    [void] (Assert-SameState -Expected $script:selfPin -Path $PSCommandPath `
        -Label "wrapper predecessor at $Checkpoint")
    Assert-RepositorySourcesUnchanged
    Assert-NativeSourcePredecessors
    Assert-UnchangedNativeSources
    Assert-LandmarkR26SourceBoundaryUnchanged
    [void] (Assert-SameState -Expected $expectedProjectPin `
        -Path $nativeProjectFile -Label "project predecessor at $Checkpoint")
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label "map predecessor at $Checkpoint")
    Assert-ImmutableContentPins
    [void] (Assert-LegacyTemasekAssetPredecessor)
    foreach ($row in $script:buildRows) {
        [void] (Assert-SameState -Expected $row.Before -Path $row.Path `
            -Label "build predecessor at $Checkpoint")
    }
}

function Assert-OwnedBoundary {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $Checkpoint,
        [switch] $UsePredecessorBoundary
    )

    [void] (Assert-ProtectedUE54Unchanged)
    if ($UsePredecessorBoundary) {
        Assert-PredecessorBoundary -Checkpoint $Checkpoint
    }
    else {
        Assert-WorkspaceAndNativeBoundary -Checkpoint $Checkpoint
    }
    [void] (Assert-LaunchedProcessIdentity -Expected $ExpectedProcess)
    $nativeEditors = @(Get-NativeTRIADUnrealProcesses)
    if ($nativeEditors.Count -ne 1 -or
        [uint32] $nativeEditors[0].ProcessId -ne
            [uint32] $ExpectedProcess.ProcessId) {
        throw "The exact helper is not the sole UE5.5/native TRIAD editor at '$Checkpoint'."
    }
    $listeners = @(Get-RemoteControlListeners)
    if ($listeners.Count -ne 1 -or
        [uint32] $listeners[0] -ne [uint32] $ExpectedProcess.ProcessId) {
        throw "RC port 30010 is not solely owned by the exact helper at '$Checkpoint'."
    }
}

function Invoke-RcCall {
    param(
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcCallUri `
        -ContentType 'application/json' -Body $payload -TimeoutSec $TimeoutSec
}

function Invoke-OwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60,
        [switch] $UsePredecessorBoundary
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "before RC $FunctionName" `
        -UsePredecessorBoundary:$UsePredecessorBoundary
    $result = Invoke-RcCall -ObjectPath $ObjectPath `
        -FunctionName $FunctionName -Parameters $Parameters `
        -TimeoutSec $TimeoutSec
    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "after RC $FunctionName" `
        -UsePredecessorBoundary:$UsePredecessorBoundary
    $result
}

function Invoke-RequiredOwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [Parameter(Mandatory = $true)] [string] $TextProperty,
        [Parameter(Mandatory = $true)] [string[]] $ExpectedPrefixes,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60,
        [switch] $UsePredecessorBoundary
    )

    $result = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $ObjectPath -FunctionName $FunctionName `
        -Parameters $Parameters -TimeoutSec $TimeoutSec `
        -UsePredecessorBoundary:$UsePredecessorBoundary
    $message = [string] $result.$TextProperty
    $matchedPrefix = @($ExpectedPrefixes | Where-Object {
            $message.StartsWith($_, [StringComparison]::Ordinal)
        } | Select-Object -First 1)
    if ($result.ReturnValue -ne $true -or $matchedPrefix.Count -ne 1) {
        throw "Required exact RC call failed: $FunctionName property=$TextProperty response=$message"
    }
    [pscustomobject] [ordered] @{
        Raw = $result
        Message = $message
        MatchedPrefix = [string] $matchedPrefix[0]
    }
}

function Wait-ExactProcessExit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Handle.Refresh()
        if ($Handle.HasExited) {
            $Handle.WaitForExit()
            return $true
        }
        $actual = Get-ProcessIdentity `
            -ProcessId ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'PID reuse or helper identity drift occurred while the exact process handle remained live.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Invoke-GracefulQuit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle,
        [switch] $UsePredecessorBoundary
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC QuitEditor' `
        -UsePredecessorBoundary:$UsePredecessorBoundary
    try {
        Invoke-RcCall -ObjectPath $quitLibrary -FunctionName 'QuitEditor' `
            -TimeoutSec 30 | Out-Null
    }
    catch {
        # The socket can close before the response when QuitEditor succeeds.
    }
    [void] (Assert-ProtectedUE54Unchanged)
    $Handle.Refresh()
    if (-not $Handle.HasExited) {
        $actual = Get-ProcessIdentity `
            -ProcessId ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'Owned process identity changed after QuitEditor.'
        }
    }
}

function Stop-ExactHelperForContainment {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle
    )

    $Handle.Refresh()
    if ($Handle.HasExited) { return }
    [void] (Assert-ProtectedUE54Unchanged)
    [void] (Assert-LaunchedProcessIdentity -Expected $ExpectedProcess)
    if ([uint32] $Handle.Id -ne [uint32] $ExpectedProcess.ProcessId) {
        throw 'Process handle does not match the exact UE5.5 helper; refusing containment.'
    }
    Stop-Process -Id ([int] $ExpectedProcess.ProcessId) -Force `
        -ErrorAction Stop
    if (-not (Wait-ExactProcessExit -ExpectedProcess $ExpectedProcess `
            -Handle $Handle -TimeoutSeconds 30)) {
        throw 'Exact UE5.5 helper did not exit after containment.'
    }
}

function Invoke-StrictColdEditorStage {
    param(
        [Parameter(Mandatory = $true)] [string] $Stage,
        [Parameter(Mandatory = $true)] [string] $MapPackage,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [Parameter(Mandatory = $true)] [string] $TextProperty,
        [Parameter(Mandatory = $true)] [string[]] $ExpectedPrefixes,
        [ValidateRange(1, 1800)] [int] $CallTimeoutSeconds = 900,
        [switch] $UsePredecessorBoundary
    )

    Assert-NativeProjectIdle -Checkpoint "before cold stage $Stage"
    if ($UsePredecessorBoundary) {
        Assert-PredecessorBoundary -Checkpoint "before stage $Stage"
    }
    else {
        Assert-WorkspaceAndNativeBoundary -Checkpoint "before stage $Stage"
    }
    $logRoot = Join-Path $transactionRoot 'logs'
    if (-not (Test-Path -LiteralPath $logRoot -PathType Container)) {
        [void] [IO.Directory]::CreateDirectory($logRoot)
    }
    $logFile = [IO.Path]::GetFullPath((Join-Path $logRoot `
        ("{0}.log" -f $Stage)))
    Assert-NewPath -Path $logFile -Root $transactionRoot `
        -Label "cold stage $Stage log"
    $argumentLine =
        "`"$nativeProjectFile`" $MapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-DDC=InstalledNoZenLocalFallback ' +
        "-LocalDataCachePath=`"$ddcRoot`" " +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$logFile`""
    $process = $null
    $identity = $null
    $workflowError = $null
    $cleanupErrors = [Collections.Generic.List[string]]::new()
    $forcedContainment = $false
    $quitRequested = $false
    $exitCode = $null
    $response = $null
    $projectReport = ''
    try {
        $process = Start-Process -FilePath $editor `
            -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot `
            -PassThru -WindowStyle Hidden
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
        do {
            $candidate = Get-ProcessIdentity `
                -ProcessId ([uint32] $process.Id)
            if (Test-ExpectedHelperLaunchIdentity -Identity $candidate `
                    -Handle $process -MapPackage $MapPackage `
                    -LogFile $logFile) {
                $identity = $candidate
                break
            }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) {
            throw "Cold stage $Stage did not prove its exact helper identity."
        }
        $editorDeadline = [DateTime]::UtcNow.AddSeconds(
            $EditorTimeoutSeconds)
        $projectIdentity = $null
        do {
            [void] (Assert-ProtectedUE54Unchanged)
            [void] (Assert-LaunchedProcessIdentity -Expected $identity)
            $listeners = @(Get-RemoteControlListeners)
            if ($listeners.Count -eq 1 -and
                [uint32] $listeners[0] -eq [uint32] $identity.ProcessId) {
                try {
                    $projectIdentity = Invoke-OwnedRcCall `
                        -ExpectedProcess $identity `
                        -ObjectPath $identityLibrary `
                        -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
                        -Parameters @{ ExpectedProjectPath = $nativeProjectRoot } `
                        -TimeoutSec 30 `
                        -UsePredecessorBoundary:$UsePredecessorBoundary
                }
                catch { $projectIdentity = $null }
                if ($null -ne $projectIdentity -and
                    $projectIdentity.ReturnValue -eq $true) { break }
            }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $editorDeadline)
        if ($null -eq $projectIdentity -or
            $projectIdentity.ReturnValue -ne $true) {
            throw "Cold stage $Stage did not prove exact project and RC ownership."
        }
        $projectReport = [string] $projectIdentity.OutReport
        $response = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $identity -ObjectPath $ObjectPath `
            -FunctionName $FunctionName -TextProperty $TextProperty `
            -ExpectedPrefixes $ExpectedPrefixes `
            -TimeoutSec $CallTimeoutSeconds `
            -UsePredecessorBoundary:$UsePredecessorBoundary
    }
    catch { $workflowError = $_.Exception }
    finally {
        if ($null -ne $process -and $null -eq $identity) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    $candidate = Get-ProcessIdentity `
                        -ProcessId ([uint32] $process.Id)
                    if (Test-ExpectedHelperLaunchIdentity -Identity $candidate `
                            -Handle $process -MapPackage $MapPackage `
                            -LogFile $logFile) {
                        $identity = $candidate
                    }
                    else {
                        throw 'Live started process could not be recovered as the exact owned helper.'
                    }
                }
            }
            catch {
                $cleanupErrors.Add(
                    "Startup identity recovery failed: $($_.Exception.Message)")
            }
        }
        if ($null -ne $process -and $null -ne $identity) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    $quitRequested = $true
                    Invoke-GracefulQuit -ExpectedProcess $identity `
                        -Handle $process `
                        -UsePredecessorBoundary:$UsePredecessorBoundary
                }
            }
            catch {
                $cleanupErrors.Add("QuitEditor failed: $($_.Exception.Message)")
            }
            try {
                $process.Refresh()
                if (-not $process.HasExited -and
                    -not (Wait-ExactProcessExit -ExpectedProcess $identity `
                        -Handle $process `
                        -TimeoutSeconds $ShutdownTimeoutSeconds)) {
                    $forcedContainment = $true
                    Stop-ExactHelperForContainment `
                        -ExpectedProcess $identity -Handle $process
                }
                $process.Refresh()
                if ($process.HasExited) {
                    $process.WaitForExit()
                    $exitCode = $process.ExitCode
                }
            }
            catch {
                $cleanupErrors.Add(
                    "Exact helper containment failed: $($_.Exception.Message)")
            }
        }
    }
    $postErrors = [Collections.Generic.List[string]]::new()
    try { [void] (Assert-ProtectedUE54Unchanged) }
    catch { $postErrors.Add($_.Exception.Message) }
    if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
        $postErrors.Add('A UE5.5/native TRIAD helper remains after cleanup.')
    }
    if (@(Get-RemoteControlListeners).Count -ne 0) {
        $postErrors.Add('RC port 30010 remains owned after cleanup.')
    }
    if ($forcedContainment) {
        $postErrors.Add('The exact helper required forced containment.')
    }
    if (-not $quitRequested) {
        $postErrors.Add('Graceful QuitEditor was never requested.')
    }
    if ($exitCode -ne 0) {
        $postErrors.Add("Exact helper exit code was not zero: $exitCode")
    }
    if ($null -ne $workflowError -or $cleanupErrors.Count -ne 0 -or
        $postErrors.Count -ne 0) {
        $workflowText = if ($null -eq $workflowError) { 'none' }
            else { $workflowError.Message }
        throw "Cold Phase-2 stage $Stage failed: workflow={$workflowText} cleanup={$([string]::Join(' | ', @($cleanupErrors)))} post={$([string]::Join(' | ', @($postErrors)))}"
    }
    Assert-NativeProjectIdle -Checkpoint "after cold stage $Stage"
    $logPin = Get-FileState -Path $logFile
    if (-not $logPin.Present -or $logPin.Bytes -le 0) {
        throw "Cold Phase-2 stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        MapPackage = $MapPackage
        Function = $FunctionName
        MatchedPrefix = $response.MatchedPrefix
        Message = $response.Message
        ProjectIdentityReport = $projectReport
        ProcessIdentity = $identity
        ExitCode = $exitCode
        Log = $logPin
        RenderingCapable = $true
        FreshProcess = $true
        ForcedContainment = $false
    }
}

function Get-BinarySearchText {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    [Text.Encoding]::ASCII.GetString($bytes) + "`n" +
        [Text.Encoding]::Unicode.GetString($bytes)
}

function Assert-FreshPhase2Reflection {
    param([Parameter(Mandatory = $true)] [object[]] $BuildRows)

    $generatedPath = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
        $phase2UhtOutputRelativePaths[0]))
    $row = @($BuildRows | Where-Object { $_.Path -ieq $generatedPath })
    if ($row.Count -ne 1) {
        throw 'Phase-2 provenance generated-code rollback row is missing.'
    }
    $after = Get-FileState -Path $generatedPath
    if (-not $after.Present -or $after.Bytes -le 0 -or
        $after.Sha256 -ceq $row[0].Before.Sha256) {
        throw 'Forced UHT did not produce distinct Phase-2 provenance generated code.'
    }
    $generatedText = Get-Content -LiteralPath $generatedPath -Raw
    foreach ($marker in @(
            'FoliageLayoutSchema', 'FoliageRenderOwnerClass',
            'FoliageTreeAnchorsLocalMeters',
            'BakedFoliageRenderComponents',
            'bFoliageRenderedByLandmarkVegetationActor')) {
        if (-not $generatedText.Contains($marker,
                [StringComparison]::Ordinal)) {
            throw "Fresh provenance reflection lacks marker: $marker"
        }
    }
    $editorText = Get-BinarySearchText -Path $editorDll
    foreach ($endpoint in @(
            'ImportIstanaExploreV5DTemasekShophouseR24Assets',
            'ValidateIstanaExploreV5DTemasekShophouseR24Assets',
            'ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap',
            'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap')) {
        if (-not $editorText.Contains($endpoint,
                [StringComparison]::Ordinal)) {
            throw "Built editor DLL lacks endpoint: $endpoint"
        }
    }
    foreach ($marker in @(
            'TRIAD_TEMASEK_PHASE2_FRESH_ROSTER_EXPECTED=16',
            'sixteen non-null assets')) {
        if (-not $editorText.Contains($marker,
                [StringComparison]::Ordinal)) {
            throw "Built editor DLL lacks fresh Phase-2 roster marker: $marker"
        }
    }
    if ($editorText.Contains('twenty non-null assets',
            [StringComparison]::Ordinal)) {
        throw 'Built editor DLL retained the stale twenty-asset Temasek import invariant.'
    }
    $runtimeText = Get-BinarySearchText -Path $runtimeDll
    foreach ($marker in @(
            'bFoliageRenderedByLandmarkVegetationActor',
            'triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1')) {
        if (-not $runtimeText.Contains($marker,
                [StringComparison]::Ordinal)) {
            throw "Built runtime DLL lacks Phase-2 marker: $marker"
        }
    }
    [pscustomobject] [ordered] @{
        Status = 'FRESH_PHASE2_UHT_AND_DLL_REFLECTION_VALID'
        ForcedHeaderGeneration = $true
        ProvenanceGeneratedCode = $after
        R26ValidatorEndpointRetained = $true
        FreshTemasekRosterExpectedAssets = 16
        StaleTwentyAssetInvariantAbsent = $true
    }
}

function Invoke-Phase2Build {
    param([Parameter(Mandatory = $true)] [object[]] $BuildRows)

    Assert-NativeProjectIdle -Checkpoint 'before Phase-2 module build'
    Assert-RepositorySourcesUnchanged
    Assert-StagedGeneratedPayload
    Assert-NativeSourceTargets
    Assert-UnchangedNativeSources
    Assert-LandmarkR26SourceBoundaryUnchanged
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label 'pre-build caller-pinned map')
    Assert-ImmutableContentPins
    # File.Replace intentionally preserves the reviewed repository file's
    # timestamp. That timestamp may predate a stale predecessor object even
    # when the promoted content hash changed. Invalidate only the nine already
    # journalled dependency-closure .obj files so UBT must compile the exact
    # promoted Phase-2 source graph.
    $forcedCompileObjects = @(
        Remove-ExactPhase2CompileObjectsForForcedRebuild `
            -BuildRows $BuildRows)
    $buildLog = Join-Path $transactionRoot 'build.log'
    Assert-NewPath -Path $buildLog -Root $transactionRoot `
        -Label 'Phase-2 build log'
    $arguments = @(
        'UnrealEditor', 'Win64', 'Development', $nativeProjectFile,
        '-DisablePlugin=AirSim',
        '-Module=TRIADSensorFusion',
        '-Module=TRIADSensorFusionEditor',
        '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBTMakefiles',
        '-ForceHeaderGeneration', '-MaxParallelActions=1',
        '-NoUBA', '-NoUBALocal'
    )
    & $buildTool @arguments *> $buildLog
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "Phase-2 module build failed with exit code $exitCode. Log=$buildLog"
    }
    $buildLogText = Get-Content -LiteralPath $buildLog -Raw
    if ($requiredDirectCompileLogLeafNames.Count -ne 5) {
        throw 'Required direct Phase-2 compile-log roster drifted.'
    }
    foreach ($leaf in $requiredDirectCompileLogLeafNames) {
        $marker = "Compile [x64] $leaf"
        if (-not $buildLogText.Contains($marker,
                [StringComparison]::Ordinal)) {
            throw "Phase-2 build log lacks forced direct compile action: $leaf"
        }
    }
    foreach ($forced in $forcedCompileObjects) {
        $state = Get-FileState -Path $forced.Path
        if (-not $state.Present -or $state.Bytes -le 0) {
            throw "Forced Phase-2 compile object was not rebuilt: $($forced.Path)"
        }
    }
    Assert-NativeProjectIdle -Checkpoint 'after Phase-2 module build'
    Assert-RepositorySourcesUnchanged
    Assert-StagedGeneratedPayload
    Assert-NativeSourceTargets
    Assert-UnchangedNativeSources
    Assert-LandmarkR26SourceBoundaryUnchanged
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label 'post-build caller-pinned map')
    Assert-ImmutableContentPins
    foreach ($row in $BuildRows) {
        $row.After = Get-FileState -Path $row.Path
        if ($row.RelativePath -in @($buildProductPins.RelativePath) -and
            (-not $row.After.Present -or $row.After.Bytes -le 0)) {
            throw "Phase-2 build product is absent or empty: $($row.Path)"
        }
    }
    $runtimeAfter = Get-FileState -Path $runtimeDll
    $editorAfter = Get-FileState -Path $editorDll
    if ($runtimeAfter.Sha256 -ceq $expectedPredecessorRuntimeDllPin.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedPredecessorEditorDllPin.Sha256) {
        throw 'Phase-2 build did not produce distinct runtime and editor DLL receipts.'
    }
    $reflection = Assert-FreshPhase2Reflection -BuildRows $BuildRows
    $script:builtDllPins = @($runtimeAfter, $editorAfter)
    $script:builtProductPins = @($BuildRows | ForEach-Object { $_.After })
    [pscustomobject] [ordered] @{
        Stage = 'BUILD_TWO_ALLOWLISTED_MODULES'
        Status = 'PASS'
        ExitCode = [int] $exitCode
        ModuleAllowlist = @('TRIADSensorFusion', 'TRIADSensorFusionEditor')
        MaximumParallelActions = 1
        ForceSourceDiscovery = '-NoUBTMakefiles'
        ForcedHeaderGeneration = $true
        ForcedCompileObjectCount = $forcedCompileObjects.Count
        RequiredDirectCompileLogCount =
            $requiredDirectCompileLogLeafNames.Count
        RequiredDirectCompileLogLeafNames =
            $requiredDirectCompileLogLeafNames
        Log = Get-FileState -Path $buildLog
        RuntimeDll = $runtimeAfter
        EditorDll = $editorAfter
        Reflection = $reflection
        TotalBuildProductRollbackCount = 172
    }
}

function Assert-InitialNativePins {
    [void] (Assert-SameState -Expected $expectedProjectPin `
        -Path $nativeProjectFile -Label 'native project file')
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label 'caller-pinned hybrid predecessor map')
    [void] (Assert-SameState -Expected $expectedPredecessorRuntimeDllPin `
        -Path $runtimeDll -Label 'caller-pinned runtime DLL')
    [void] (Assert-SameState -Expected $expectedPredecessorEditorDllPin `
        -Path $editorDll -Label 'caller-pinned editor DLL')
    Assert-GeneratorAdmissionPins
    Assert-NativeSourcePredecessors
    Assert-UnchangedNativeSources
    Assert-ImmutableContentPins
    [void] (Assert-LegacyTemasekAssetPredecessor)
}

$script:selfPin = Get-FileState -Path $PSCommandPath
$script:promotionRows = @(Get-PromotionRows)
$script:legacyAssetRows = @(Get-LegacyTemasekAssetRows)
$buildRollbackContract = Assert-BuildRollbackRosterDefinition

if ($Execute -and $StaticSelfCheck) {
    throw '-Execute and -StaticSelfCheck are mutually exclusive.'
}

if ($StaticSelfCheck) {
    Assert-GeneratorAdmissionPins
    [pscustomobject] [ordered] @{
        Schema = "$schema.static_check"
        Status = 'STATIC_SELF_CHECK_PASS'
        Script = $script:selfPin
        ExecuteRequested = $false
        NativeTreeReadOrWritten = $false
        UnrealBuildOrEditorLaunched = $false
        EmbeddedDefaultPredecessor = 'R25_DOCUMENTATION_ONLY'
        LiveExecuteRequiresExplicitCallerPins = @(
            'ExpectedMapBytes', 'ExpectedMapSha256',
            'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
            'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256')
        LiveExecuteRequiresLandmarkVegetationR26 = $true
        RequiredR26ColdEndpoint =
            'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap'
        PromotionCount = 14
        RepositoryPromotionCount = 7
        DeterministicGeneratedPromotionCount = 7
        LegacyAssetCount = 20
        SuccessorAssetCount = 16
        RemovedBakedFoliageMaterialCount = 4
        Phase2Census = [pscustomobject] [ordered] @{
            SourceVertices = 9536
            Triangles = 15760
            Components = 828
            Materials = 15
            BakedFoliageRenderComponents = 0
            TreeAnchorsHandedToR26 = 3
        }
        BuildRollbackContract = $buildRollbackContract
        FailureRollbackOrder = @(
            'TEMASEK_CONTENT_AND_MAP',
            'ONE_HUNDRED_SEVENTY_TWO_BUILD_PRODUCT_STATES',
            'FOURTEEN_SOURCE_DESTINATIONS')
        ProtectedUE54Policy =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        RemoteControlPolicy =
            'SOLE_EXACT_START_PROCESS_HANDLE_PID_CREATION_EXE_PROJECT_MAP_LOG_AND_PORT_OWNER'
        LandmarkVegetationR26OwnedByThisTransaction = $false
        KnownGap =
            'Live execution is intentionally unavailable until the caller supplies the exact committed R26 map/DLL receipt; the wrapper then validates R26 before and after the zero-baked-foliage Phase-2 replacement.'
        WholeNativeTreeRollbackClaimed = $false
        RecursiveDeleteUsed = $false
    } | ConvertTo-Json -Depth 24
    return
}

if ($Execute) {
    foreach ($name in @(
            'ExpectedMapBytes', 'ExpectedMapSha256',
            'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
            'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256')) {
        if (-not $PSBoundParameters.ContainsKey($name)) {
            throw "Live Phase-2 execution requires explicit caller-supplied -$name from the committed R26 predecessor receipt."
        }
    }
    if (-not $RequireLandmarkVegetationR26) {
        throw 'Live Phase-2 execution requires -RequireLandmarkVegetationR26.'
    }
}

Assert-ExistingNonReparseDirectory -Path $repositoryRoot `
    -Label 'repository root'
Assert-ExistingNonReparseDirectory -Path $repositoryUnrealRoot `
    -Label 'repository Unreal root'
Assert-ExistingNonReparseDirectory -Path $nativeProjectRoot `
    -Label 'native project root'
Assert-ExistingNonReparseDirectory -Path $nativeUE55EngineRoot `
    -Label 'UE5.5 engine root'
Assert-ExistingNonReparseDirectory -Path $ddcRoot `
    -Label 'native Derived Data Cache root'
foreach ($boundaryPath in @(
        $nativeProjectFile, $hybridMapFile, $runtimeDll, $editorDll,
        (Join-Path $temasekAssetRoot '__sentinel__'),
        (Join-Path $transactionRoot '__sentinel__'))) {
    Assert-NoReparseAncestor -Path $boundaryPath -Root $nativeProjectRoot `
        -Label 'reviewed Phase-2 native boundary'
}
$buildVersion = Get-Content -LiteralPath $buildVersionFile -Raw |
    ConvertFrom-Json
if ([int] $buildVersion.MajorVersion -ne 5 -or
    [int] $buildVersion.MinorVersion -ne 5) {
    throw 'The reviewed engine root no longer reports Unreal Engine 5.5.'
}
$script:protectedUE54Before = Get-ProtectedUE54Identity
Assert-NativeProjectIdle -Checkpoint 'initial Phase-2 preflight'
Assert-InitialNativePins
$script:buildRows = @(Get-PinnedBuildProductRows)
$dependencyClosure = Get-ReviewedBuildDependencyClosure
$r26Readiness = Get-LandmarkR26NativeReadiness
if ($RequireLandmarkVegetationR26) {
    $r26Readiness = Assert-LandmarkR26NativeSourceReady
}
$script:landmarkR26SourceBoundary = $r26Readiness
Assert-NewPath -Path $transactionRoot -Root $nativeProjectRoot `
    -Label 'fresh Phase-2 transaction root'

$preflight = [pscustomobject] [ordered] @{
    Schema = $schema
    Status = 'PREFLIGHT_PASS'
    ExecuteRequested = [bool] $Execute
    RunToken = $RunToken
    CallerSuppliedPredecessorPins = [pscustomobject] [ordered] @{
        Map = $expectedPredecessorMapPin
        RuntimeDll = $expectedPredecessorRuntimeDllPin
        EditorDll = $expectedPredecessorEditorDllPin
        AllSixExplicitForExecute = [bool] $Execute
    }
    R25DefaultsAreDocumentationOnly = $true
    LandmarkVegetationR26RequiredForExecute = $true
    LandmarkVegetationR26 = $r26Readiness
    PromotionCount = 14
    PromotionRows = @($script:promotionRows | ForEach-Object {
        [pscustomobject] [ordered] @{
            Area = $_.Area
            Origin = $_.Origin
            RelativePath = $_.RelativePath
            Target = $_.Target
            NativeBefore = $_.NativeBefore
        }
    })
    BuildRollbackContract = $buildRollbackContract
    DependencyClosure = $dependencyClosure
    BuildProductPredecessors = @($script:buildRows | ForEach-Object {
        $_.Before
    })
    LegacyTemasekAssetCount = 20
    Phase2TemasekAssetCount = 16
    ImmutableR25V5CV2AssetCount = $immutableContentPins.Count
    ProtectedUE54 = $script:protectedUE54Before
    TransactionRoot = $transactionRoot
    NativeMutated = $false
}

if (-not $Execute) {
    $preflight | ConvertTo-Json -Depth 24
    return
}

# Final gate immediately before the first D: write. R26 source readiness and
# caller pins have already been proven, but are deliberately re-read here.
Assert-NativeProjectIdle -Checkpoint 'final Phase-2 pre-write gate'
[void] (Assert-SameState -Expected $script:selfPin -Path $PSCommandPath `
    -Label 'Phase-2 wrapper final pre-write pin')
Assert-RepositorySourcesUnchanged
Assert-InitialNativePins
[void] (Assert-LandmarkR26NativeSourceReady)
foreach ($row in $script:buildRows) {
    [void] (Assert-SameState -Expected $row.Before -Path $row.Path `
        -Label 'build product final pre-write pin')
}
$finalDependencyClosure = Get-ReviewedBuildDependencyClosure
if (($dependencyClosure | ConvertTo-Json -Compress -Depth 24) -cne
    ($finalDependencyClosure | ConvertTo-Json -Compress -Depth 24)) {
    throw 'Phase-2 dependency closure changed before the first native write.'
}
Assert-NewPath -Path $transactionRoot -Root $nativeProjectRoot `
    -Label 'fresh Phase-2 transaction root'
[void] [IO.Directory]::CreateDirectory($transactionRoot)

$backups = $null
$stageResults = [Collections.Generic.List[object]]::new()
try {
    $backups = New-VerifiedTransactionBackups -BuildRows $script:buildRows
    $prepared = [pscustomobject] [ordered] @{
        Schema = "$schema.prepared"
        Status = 'PREPARED'
        RunToken = $RunToken
        Preflight = $preflight
        MapBackup = $backups.MapBackup
        SourceBackupCount = $backups.SourceBackupCount
        BuildBackupCount = $backups.BuildBackupCount
        LegacyAssetBackupCount = $backups.LegacyAssetBackupCount
        BackupWritesWereNonOverwriting = $true
        BackupsVerifiedBeforeFunctionalNativeMutation = $true
    }
    $preparedReceipt = Write-NewJsonReceipt `
        -Path (Join-Path $transactionRoot 'prepared.json') `
        -Value $prepared -Root $transactionRoot `
        -Label 'durable Phase-2 prepared receipt'

    # Validate the R26 predecessor using the still-unmodified R26 binaries and
    # legacy Temasek package roster. After compiling Phase 2, those legacy
    # packages intentionally no longer satisfy the new asset census.
    $r26Before = Invoke-StrictColdEditorStage `
        -Stage '00_validate_r26_predecessor' `
        -MapPackage $hybridMapPackage -ObjectPath $hybridLibrary `
        -FunctionName 'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap' `
        -TextProperty 'OutReport' `
        -ExpectedPrefixes @(
            'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID:') `
        -UsePredecessorBoundary
    $stageResults.Add($r26Before)
    if (-not $r26Before.Message.Contains(
            'landmarkVegetationR26=true', [StringComparison]::Ordinal)) {
        throw 'R26 predecessor validation did not acknowledge landmarkVegetationR26=true.'
    }
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label 'R26 predecessor validation map')
    [void] (Assert-LegacyTemasekAssetPredecessor)

    $staging = Invoke-Phase2StagingBuild
    $stageResults.Add($staging)

    Publish-NativeSources
    $stageResults.Add([pscustomobject] [ordered] @{
        Stage = 'PROMOTE_EXACT_7_REPOSITORY_PLUS_7_GENERATED_FILES'
        Status = 'PASS'
        Count = 14
    })

    $buildResult = Invoke-Phase2Build -BuildRows $script:buildRows
    $stageResults.Add($buildResult)

    $evacuation = Remove-ExactLegacyTemasekAssetPredecessor
    $stageResults.Add($evacuation)
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label 'map after exact asset evacuation')

    $import = Invoke-StrictColdEditorStage `
        -Stage '01_import_phase2_assets' `
        -MapPackage $assetStageMapPackage -ObjectPath $temasekLibrary `
        -FunctionName 'ImportIstanaExploreV5DTemasekShophouseR24Assets' `
        -TextProperty 'OutMessage' `
        -ExpectedPrefixes @(
            'EXPLORE_V5D_R24_TEMASEK_ASSET_IMPORT_PASS:')
    foreach ($marker in @(
            '15,760-triangle', 'fifteen deterministic',
            'bakedFoliageRenderComponents=0', 'foliageTreeAnchors=3')) {
        if (-not $import.Message.Contains($marker,
                [StringComparison]::Ordinal)) {
            throw "Phase-2 import acknowledgement lacks marker: $marker"
        }
    }
    $stageResults.Add($import)
    $assetsAfterImport = Get-Phase2TemasekAssetInventory
    [void] (Assert-SameState -Expected $expectedPredecessorMapPin `
        -Path $hybridMapFile -Label 'map after Phase-2 asset import')

    $coldAssets = Invoke-StrictColdEditorStage `
        -Stage '02_cold_validate_phase2_assets' `
        -MapPackage $assetStageMapPackage -ObjectPath $temasekLibrary `
        -FunctionName 'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
        -TextProperty 'OutReport' `
        -ExpectedPrefixes @(
            'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID')
    $stageResults.Add($coldAssets)
    $assetsAfterColdValidation = Assert-Phase2TemasekAssetInventorySame `
        -Expected $assetsAfterImport -Label 'cold Phase-2 asset validation'

    $apply = Invoke-StrictColdEditorStage `
        -Stage '03_apply_phase2_to_r26_map' `
        -MapPackage $hybridMapPackage -ObjectPath $hybridLibrary `
        -FunctionName 'ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap' `
        -TextProperty 'OutMessage' `
        -ExpectedPrefixes @(
            'IDEMPOTENT_EXPLORE_V5D_R24_TEMASEK_ALREADY_VALID:',
            'EXPLORE_V5D_R24_TEMASEK_APPLY_PASS:')
    $stageResults.Add($apply)
    $successorMapPin = Get-FileState -Path $hybridMapFile
    if (-not $successorMapPin.Present -or $successorMapPin.Bytes -le 0) {
        throw 'Phase-2 map apply did not retain a non-empty map.'
    }
    [void] (Assert-Phase2TemasekAssetInventorySame `
        -Expected $assetsAfterImport -Label 'Phase-2 asset after map apply')
    Assert-ImmutableContentPins

    $r26After = Invoke-StrictColdEditorStage `
        -Stage '04_cold_validate_r26_successor' `
        -MapPackage $hybridMapPackage -ObjectPath $hybridLibrary `
        -FunctionName 'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap' `
        -TextProperty 'OutReport' `
        -ExpectedPrefixes @(
            'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID:')
    if (-not $r26After.Message.Contains(
            'landmarkVegetationR26=true', [StringComparison]::Ordinal)) {
        throw 'Phase-2 successor lost the R26 landmark vegetation acknowledgement.'
    }
    $stageResults.Add($r26After)
    [void] (Assert-SameState -Expected $successorMapPin `
        -Path $hybridMapFile -Label 'map after R26 successor validation')

    $hybridAfter = Invoke-StrictColdEditorStage `
        -Stage '05_cold_validate_hybrid_successor' `
        -MapPackage $hybridMapPackage -ObjectPath $hybridLibrary `
        -FunctionName 'ValidateIstanaExploreV5DHybridMap' `
        -TextProperty 'OutReport' `
        -ExpectedPrefixes @('ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID')
    $stageResults.Add($hybridAfter)
    [void] (Assert-SameState -Expected $successorMapPin `
        -Path $hybridMapFile -Label 'map after hybrid successor validation')

    $idempotenceBefore = Get-FileState -Path $hybridMapFile
    $idempotence = Invoke-StrictColdEditorStage `
        -Stage '06_idempotent_phase2_apply' `
        -MapPackage $hybridMapPackage -ObjectPath $hybridLibrary `
        -FunctionName 'ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap' `
        -TextProperty 'OutMessage' `
        -ExpectedPrefixes @(
            'IDEMPOTENT_EXPLORE_V5D_R24_TEMASEK_ALREADY_VALID:')
    $stageResults.Add($idempotence)
    [void] (Assert-SameState -Expected $idempotenceBefore `
        -Path $hybridMapFile `
        -Label 'idempotent Phase-2 second apply map receipt')

    $finalR26 = Invoke-StrictColdEditorStage `
        -Stage '07_final_cold_validate_r26' `
        -MapPackage $hybridMapPackage -ObjectPath $hybridLibrary `
        -FunctionName 'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap' `
        -TextProperty 'OutReport' `
        -ExpectedPrefixes @(
            'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID:')
    $stageResults.Add($finalR26)
    [void] (Assert-SameState -Expected $successorMapPin `
        -Path $hybridMapFile -Label 'final R26 Phase-2 map')

    Assert-NativeProjectIdle -Checkpoint 'final Phase-2 commit'
    Assert-WorkspaceAndNativeBoundary -Checkpoint 'final Phase-2 commit'
    [void] (Assert-Phase2TemasekAssetInventorySame `
        -Expected $assetsAfterImport -Label 'final Phase-2 asset')
    $commit = [pscustomobject] [ordered] @{
        Schema = "$schema.commit"
        Status = 'PASS'
        RunToken = $RunToken
        TransactionRoot = $transactionRoot
        PreparedReceipt = $preparedReceipt
        CallerPinnedPredecessorMap = $expectedPredecessorMapPin
        SuccessorMap = $successorMapPin
        MapBytesChanged = [bool] (
            $successorMapPin.Sha256 -cne $expectedPredecessorMapPin.Sha256)
        RuntimeDll = $script:builtDllPins[0]
        EditorDll = $script:builtDllPins[1]
        PromotionCount = 14
        TotalBuildProductRollbackCount = 172
        LegacyAssetCount = 20
        Phase2AssetCount = 16
        RemovedBakedFoliageMaterialCount = 4
        Phase2Census = [pscustomobject] [ordered] @{
            SourceVertices = 9536
            Triangles = 15760
            Components = 828
            MaterialSlots = 15
            BakedFoliageRenderComponents = 0
            FoliageTreeAnchors = 3
        }
        R26ValidatedBeforeReplacement = $true
        R26ValidatedAfterReplacement = $true
        LandmarkVegetationR26MapIntegrated = $true
        LandmarkVegetationR26OwnedByThisTransaction = $false
        IdempotentSecondApplyMapBytesAndHashUnchanged = $true
        ImportedAssets = $assetsAfterImport
        ColdValidatedAssets = $assetsAfterColdValidation
        Stages = @($stageResults)
        ProtectedUE54 = Assert-ProtectedUE54Unchanged
        ImmutableR25V5CV2AssetsUnchanged = $true
        CollisionNavigationSensorRfAuthorityChanged = $false
        WholeNativeTreeRollbackClaimed = $false
        ForcedContainmentUsed = $false
        RecursiveDeleteUsed = $false
    }
    $commitReceipt = Write-NewJsonReceipt `
        -Path (Join-Path $transactionRoot 'commit.json') `
        -Value $commit -Root $transactionRoot `
        -Label 'durable Phase-2 commit receipt'
    $commit | Add-Member -NotePropertyName CommitReceipt `
        -NotePropertyValue $commitReceipt
    $commit | ConvertTo-Json -Depth 24
}
catch {
    $transactionError = $_.Exception.Message
    $rollbackErrors = [Collections.Generic.List[string]]::new()
    if ($null -ne $backups) {
        try {
            Assert-NativeProjectIdle -Checkpoint 'Phase-2 rollback entry'
            Restore-Phase2Content -MapBackup $backups.MapBackup
        }
        catch { $rollbackErrors.Add("content=$($_.Exception.Message)") }
        try {
            Assert-NativeProjectIdle -Checkpoint 'before Phase-2 build rollback'
            Restore-BuildProducts -BuildRows $script:buildRows
        }
        catch { $rollbackErrors.Add("build=$($_.Exception.Message)") }
        try {
            Assert-NativeProjectIdle -Checkpoint 'before Phase-2 source rollback'
            Restore-NativeSources
        }
        catch { $rollbackErrors.Add("source=$($_.Exception.Message)") }
    }
    try { [void] (Assert-ProtectedUE54Unchanged) }
    catch { $rollbackErrors.Add("capstone=$($_.Exception.Message)") }
    $failure = [pscustomobject] [ordered] @{
        Schema = "$schema.failure"
        Status = if ($rollbackErrors.Count -eq 0) {
            'FAILED_AND_ROLLED_BACK'
        }
        else { 'FAILED_ROLLBACK_INCOMPLETE' }
        RunToken = $RunToken
        TransactionRoot = $transactionRoot
        Error = $transactionError
        RollbackErrors = @($rollbackErrors)
        CompletedStages = @($stageResults)
        RollbackOrder = @(
            'TEMASEK_CONTENT_AND_MAP',
            'ONE_HUNDRED_SEVENTY_TWO_BUILD_PRODUCT_STATES',
            'FOURTEEN_SOURCE_DESTINATIONS')
        BackupsRetained = $true
        RecursiveDeleteUsed = $false
        WholeNativeTreeRollbackClaimed = $false
    }
    try {
        [void] (Write-NewJsonReceipt `
            -Path (Join-Path $transactionRoot 'failure.json') `
            -Value $failure -Root $transactionRoot `
            -Label 'durable Phase-2 failure receipt')
    }
    catch {
        $failure.RollbackErrors +=
            "failureReceipt=$($_.Exception.Message)"
        $failure.Status = 'FAILED_ROLLBACK_INCOMPLETE'
    }
    $failure | ConvertTo-Json -Depth 24 | Write-Output
    throw "Phase-2 native transaction failed: $transactionError; rollbackStatus=$($failure.Status)"
}
