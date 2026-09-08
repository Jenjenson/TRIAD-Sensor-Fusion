# Istana R33 Cesium readiness audit — 2026-09-08

> **Historical audit snapshot.** This document preserves the installation and
> memory state observed by its dated read-only audit. Its statements that R29
> was current and that no R30 commit/evidence root existed were later
> superseded by the committed R30-19 native receipt. R33 remains non-native and
> no R30 capture has been human-accepted. Use the
> [operator manual](ISTANA_OPERATOR_MANUAL.md#v5d-current-r30-native-scene-status)
> and [guarded execution guide](ISTANA_R30_R33_NATIVE_EXECUTION_GUIDE.md) for
> current operational status; do not reinterpret the historical hashes below
> as current pins.

Status: **source-ready after a fail-closed opaque-server fix; not native-live,
provider-ready, or terrain-accuracy-ready**.

This is a read-only audit of the repository and native installation. No Unreal
process was launched, no map or DLL was written, no token value or fingerprint
was inspected or emitted, and no height was invented or sampled.

## Native boundary observed

- Project: `D:\triad\TRIAD\TRIAD.uproject`, 1,298 bytes, SHA-256
  `42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3`.
  It targets Engine Association `5.5` and enables `CesiumForUnreal`,
  `TRIADSensorFusion`, `RemoteControl`, and `AirSimTriadRuntime`.
- Current target map: 37,465,844 bytes, SHA-256
  `A05C95CEF30DFF1675B049970D993F1DBA75CB88BE38BFC73E7C8077BC5C1F92`.
- Current runtime DLL: 4,981,248 bytes, SHA-256
  `30EF244443737D3BA909F96C2B5846951621378D32F81AAB13CF765635D42276`.
- Current editor DLL: 8,252,928 bytes, SHA-256
  `E0CC5AD603DCB3E243EAFEC09D4E66666CD6FE00922E4D57FB6DA87B7A8E61DE`.
- These are the exact retained R29 native pins. No R30, R31, R32, or R33
  commit receipt exists. The only R30 native transaction artifact is one
  rollback receipt; no R30 evidence root exists. Therefore R33 cannot legally
  bypass the required R30 commit/review, R31 commit/review, and R32
  commit/review sequence.

At `2026-09-07T22:34:48Z`, free commit was 4,953,522,176 bytes (4.613 GiB),
below the immutable 10 GiB launch gate and the 6 GiB continuous floor. The
user's UE 5.4 Capstone editor (PID 22908, 9,628,348,416 private bytes) remained
protected. This observation is
transient; every eventual execution must remeasure memory and re-pin all native
inputs immediately before the first write.

## Installed Cesium compatibility

Exactly one descriptor was found under the two approved search roots:

`C:\Program Files\Epic Games\UE_5.5\Engine\Plugins\Marketplace\Cesiumfo27e669801be6V8\CesiumForUnreal.uplugin`

It is the reviewed 1,214-byte descriptor, SHA-256
`77F60013ADAFAC1EADBC9364F7453E6CF0BD83AA824DFBE5291D5D0FF2F1D2A6`,
with descriptor version `78`, plugin version `2.18.0`, and engine version
`5.5.0`.

The installed source under
`C:\Program Files\Epic Games\UE_5.5\Engine\Plugins\Marketplace\Cesiumfo27e669801be6V8\Source\CesiumRuntime`
exposes the exact R33 API assumptions:

- `Public\Cesium3DTileset.h`, 49,183 bytes, SHA-256
  `54417AD36DC1FA028AF931E96DA9FBD69D4F142024910206BB066FE3DC434F00`:
  `OnCesium3DTilesetLoadFailure`, `SampleHeightMostDetailed`,
  `GetGeoreference`, `SetGeoreference`, `ResolveGeoreference`,
  `GetTilesetSource`, `SetTilesetSource`, `GetIonAssetID`, `SetIonAssetID`,
  `GetCesiumIonServer`, `SetCesiumIonServer`,
  `GetMaximumScreenSpaceError`, `SetMaximumScreenSpaceError`,
  `GetCreatePhysicsMeshes`, `SetCreatePhysicsMeshes`,
  `GetCreateNavCollision`, `SetCreateNavCollision`, and `GetLoadProgress`.
- `Public\CesiumGeoreference.h`, 32,031 bytes, SHA-256
  `41E439100B27595D36220A9630AE9FDC5D88CB161946F95A725F21A9A07C02F6`:
  `GetOriginLongitudeLatitudeHeight`, `GetOriginPlacement`, and `GetScale`.
- `Public\CesiumIonServer.h`, 4,552 bytes, SHA-256
  `699E694FCB15F43046710577372525E4A24B26BE2DCFA0D8580849505B6E30BE`:
  the runtime default server is expected at
  `/Game/CesiumSettings/CesiumIonServers/CesiumIonSaaS` and may still be null
  at runtime when the asset is absent.
- `Public\Cesium3DTilesetLoadFailureDetails.h`, 1,357 bytes, SHA-256
  `D1FCBAB1FDF6CC76426DDC75A70C4AEF1689B212FF366FC5AB2849D4050E8857`:
  exact fields `Tileset`, `Type`, `HttpStatusCode`, and `Message` used by the
  failover filter.
- `Private\Cesium3DTileset.cpp`, 76,736 bytes, SHA-256
  `8E9E3EF9CCC15F27B234E85099A9BC0DCDAA0E58BEF2C56DD280FA571D4B450B`
  provides the reviewed implementation-side evidence for those declarations.

The same installed API explicitly says `SampleHeightMostDetailed` returns
metres above the configured ellipsoid, usually WGS84, and ignores input height
unless sampling fails. That validates the repository's API assumption, not
Istana-area height accuracy or a Singapore Height Datum conversion.
Installed Cesium 2.18.0 therefore closes the API-availability assumption for
the R33 source, but it does not prove that TRIAD's promoted source compiles,
that either provider asset streams, or that any live readiness gate passes.

## Opaque provider configuration

The expected project server asset exists at
`D:\triad\TRIAD\Content\CesiumSettings\CesiumIonServers\CesiumIonSaaS.uasset`
(2,397 bytes), and the current map contains a reference to `CesiumIonSaaS`.
Only object/path and property-name markers were checked. Token contents and
fingerprints were deliberately not inspected.

Asset presence and a non-null `UCesiumIonServer` object do **not** prove that a
runtime token exists, is current, permits Google asset `2275207`, permits CWT
asset `1`, or satisfies current provider terms. Those facts require the native
provider/PIE checks and must remain false in source-only receipts.

## Fail-closed source correction

Before this audit, R33's pointer-identity tests could admit two equal null ion
server pointers as a shared server. Runtime resolver/configure/apply/validate/
request/readiness paths, context resolver/register/apply/validate paths, editor
resolver/predecessor/apply/successor-validation paths, and the post-transaction
Player0 capture-state resolver now all require valid, non-null opaque server
objects before comparing pointer identity. Equality-only mutations that remove
the two validity predicates are rejected by the focused source tests. The code never calls
`GetIonAccessToken`, reads `DefaultIonAccessToken`, or records a token value or
fingerprint.

The guarded wrapper's transitive pins were resealed to:

- Context-policy source: 156,491 bytes,
  `7A75941BBD020CBCA68E49C86B5234748EBF96BCACB615BB4C14B6459E904AB7`.
- R33 runtime source: 54,715 bytes,
  `0324CF4CC5674A22608A301C115010F8E1035BB94FE2D2667549FD1DE0B102CA`.
- R33 editor source: 37,009 bytes,
  `C76DBBF2ADEF6022211C6176AAAFF228CB1714899C0965A5EA275372466DD151`.
- R33 capture source: 28,764 bytes,
  `471EB8435EABE2170A58C97FC32677E4F450580BE931E65CB3894A32CB54E131`.
- R33 transaction contract: 22,421 bytes,
  `1A052B21307500B793ABB65EF4DE0DE1F62A3076D6EB6385B09BC8B9BA4EFE8E`.
- R33 visual-review contract: 3,668 bytes,
  `DAC2FB328107265C043ACBF9587E3791A457CD56917631E0A33BEFD6AD06DCA6`.
- Native transaction wrapper: 110,463 bytes,
  `2403D90E20CCAA1FF221A197930B487BB4FCF666FD7DFF0278F704EA0812961B`.
- Capture wrapper: 151,091 bytes,
  `D72711D9BBB6EB631D2499C7FE11E94CF1DDF659246A25E627BED04BC020A6A6`.

Focused R33 source/native/capture verification passes `49/49`; the adjacent
R29/R32/context/R33 regression sweep passes `93/93`. Both repository-only
wrapper static self-checks report `STATIC_SELF_CHECK_PASS` with
`NonNullOpaqueIonServerRequired=true`, token/fingerprint inspection false, and
token/entitlement proof false.

## What is ready and what remains open

Source-ready: exact two-role roster, shared georeference and site clip,
visual-only CWT asset `1`, collision/navigation/RF/sensor authority isolation,
exclusive Google/CWT/SafeLocal state logic, plugin descriptor compatibility,
strict R30→R33 receipt sequencing, rollback, and fixed memory guards.

Not live: R30–R33 native transactions and human-reviewed captures, current CWT
entitlement and streaming, hidden-warming behavior, failover/recovery,
provider terms, target-hardware cost, vertical-datum conversion, independent
checkpoints, and present-day terrain accuracy. CWT therefore remains an
optional visual reference, not authoritative simulation terrain.
