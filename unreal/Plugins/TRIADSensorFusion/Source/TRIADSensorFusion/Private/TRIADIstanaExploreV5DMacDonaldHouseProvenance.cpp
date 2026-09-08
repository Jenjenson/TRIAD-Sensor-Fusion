#include "TRIADIstanaExploreV5DMacDonaldHouseProvenance.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"

namespace
{
const FString SourceObjProjectRelativePath(
    TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
         "R24MacDonaldHouse/Generated/"
         "SM_IPV5D_R24_MacDonaldHouse_Render.obj"));
const FString RenderPayloadSchema(
    TEXT("triad.istana_explore_v5d.r24_macdonald_house."
         "cooked_render_payload.v1"));

bool IsCanonicalUppercaseSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!((Character >= TEXT('0') && Character <= TEXT('9')) ||
              (Character >= TEXT('A') && Character <= TEXT('F'))))
        {
            return false;
        }
    }
    return true;
}

void AppendUInt32LittleEndian(TArray<uint8>& Bytes, uint32 Value)
{
    Bytes.Add(static_cast<uint8>(Value));
    Bytes.Add(static_cast<uint8>(Value >> 8));
    Bytes.Add(static_cast<uint8>(Value >> 16));
    Bytes.Add(static_cast<uint8>(Value >> 24));
}

void AppendFloatBits(TArray<uint8>& Bytes, float Value)
{
    static_assert(sizeof(float) == sizeof(uint32));
    uint32 Bits = 0;
    FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
    AppendUInt32LittleEndian(Bytes, Bits);
}

void AppendUtf8(TArray<uint8>& Bytes, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    AppendUInt32LittleEndian(Bytes, static_cast<uint32>(Utf8.Length()));
    if (Utf8.Length() > 0)
    {
        Bytes.Append(
            reinterpret_cast<const uint8*>(Utf8.Get()),
            Utf8.Length());
    }
}

uint32 RotateRight(uint32 Value, uint32 Count)
{
    return (Value >> Count) | (Value << (32u - Count));
}

FString Sha256(const TArray<uint8>& Input)
{
    static const uint32 RoundConstants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    uint32 State[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    TArray<uint8> Message = Input;
    const uint64 BitLength = static_cast<uint64>(Input.Num()) * 8u;
    Message.Add(0x80u);
    while ((Message.Num() % 64) != 56)
    {
        Message.Add(0u);
    }
    for (int32 Shift = 56; Shift >= 0; Shift -= 8)
    {
        Message.Add(static_cast<uint8>(BitLength >> Shift));
    }

    for (int32 Offset = 0; Offset < Message.Num(); Offset += 64)
    {
        uint32 Schedule[64] = {};
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const int32 Byte = Offset + Index * 4;
            Schedule[Index] =
                (static_cast<uint32>(Message[Byte]) << 24) |
                (static_cast<uint32>(Message[Byte + 1]) << 16) |
                (static_cast<uint32>(Message[Byte + 2]) << 8) |
                static_cast<uint32>(Message[Byte + 3]);
        }
        for (int32 Index = 16; Index < 64; ++Index)
        {
            const uint32 S0 = RotateRight(Schedule[Index - 15], 7) ^
                RotateRight(Schedule[Index - 15], 18) ^
                (Schedule[Index - 15] >> 3);
            const uint32 S1 = RotateRight(Schedule[Index - 2], 17) ^
                RotateRight(Schedule[Index - 2], 19) ^
                (Schedule[Index - 2] >> 10);
            Schedule[Index] = Schedule[Index - 16] + S0 +
                Schedule[Index - 7] + S1;
        }

        uint32 A = State[0];
        uint32 B = State[1];
        uint32 C = State[2];
        uint32 D = State[3];
        uint32 E = State[4];
        uint32 F = State[5];
        uint32 G = State[6];
        uint32 H = State[7];
        for (int32 Index = 0; Index < 64; ++Index)
        {
            const uint32 Sum1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^
                RotateRight(E, 25);
            const uint32 Choice = (E & F) ^ ((~E) & G);
            const uint32 Temporary1 = H + Sum1 + Choice +
                RoundConstants[Index] + Schedule[Index];
            const uint32 Sum0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^
                RotateRight(A, 22);
            const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const uint32 Temporary2 = Sum0 + Majority;
            H = G;
            G = F;
            F = E;
            E = D + Temporary1;
            D = C;
            C = B;
            B = A;
            A = Temporary1 + Temporary2;
        }
        State[0] += A;
        State[1] += B;
        State[2] += C;
        State[3] += D;
        State[4] += E;
        State[5] += F;
        State[6] += G;
        State[7] += H;
    }

    uint8 Digest[32] = {};
    for (int32 Index = 0; Index < 8; ++Index)
    {
        Digest[Index * 4] = static_cast<uint8>(State[Index] >> 24);
        Digest[Index * 4 + 1] = static_cast<uint8>(State[Index] >> 16);
        Digest[Index * 4 + 2] = static_cast<uint8>(State[Index] >> 8);
        Digest[Index * 4 + 3] = static_cast<uint8>(State[Index]);
    }
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToUpper();
}
} // namespace

UTRIADIstanaExploreV5DMacDonaldHouseProvenance::
    UTRIADIstanaExploreV5DMacDonaldHouseProvenance()
{
    SetCanonicalContract(FString());
}

void UTRIADIstanaExploreV5DMacDonaldHouseProvenance::
    SetCanonicalContract(const FString& InCookedRenderPayloadSha256)
{
    SourceOutputSetSha256 =
        TEXT("D72869336402E3A9FEAD8E2E747D655CAD21B79E416D3CF2D99007F3AA5D6FAA");
    RenderObjSha256 =
        TEXT("BE049C7E1A8AB2D9DED98C027478EA5A5183F0C2CF2FFC206289D1FF47F955DE");
    CanonicalGeometrySha256 =
        TEXT("9F629115A98BE5A257D06ACD864CE40170C8EFCE57BF60D7D145B85D7682C69C");
    PlacementReceiptSha256 =
        TEXT("E5B40D02A3D712303A2D99734A42E9ADD68F4E528DED31DD763F37034961ED81");
    OSMFeatureGeometrySha256 =
        TEXT("755536CDE8E666826FA6A325BCD707BDC7E0132C018BC87F0ABE8275F4557A39");
    OSMPartGeometrySha256 =
        TEXT("B7204683A26E7DA56E1A97E516E36220C1764341CA485B904D736F509B481A34");
    SourceObjProjectRelativePath = ::SourceObjProjectRelativePath;
    CookedRenderPayloadSchema = RenderPayloadSchema;
    CookedRenderPayloadSha256 = InCookedRenderPayloadSha256;
    Components = 576;
    SourceVertices = 4692;
    Triangles = 7080;
    MaterialSlots = 9;
    bProceduralTextureFreePbrPresentation = true;
    bProceduralPbrMaterialsCalibrated = false;
    bRenderOnly = true;
    bCollisionNavigationSensorOrRfAuthority = false;
    bSurveyAsBuiltOrOneToOne = false;
    bProviderReadyLiveSuccessor = false;
}

bool UTRIADIstanaExploreV5DMacDonaldHouseProvenance::
    ComputeCookedRenderPayloadSha256(
        const UStaticMesh* Mesh,
        FString& OutSha256,
        FString& OutError)
{
    OutSha256.Reset();
    static const bool bSha256ImplementationSelfTest = []
    {
        const TArray<uint8> Empty;
        return Sha256(Empty) ==
            TEXT("E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
    }();
    if (!bSha256ImplementationSelfTest)
    {
        OutError = TEXT("Cooked render-payload SHA-256 implementation failed its empty-vector self-test.");
        return false;
    }
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    if (!Mesh || !Mesh->bAllowCPUAccess || !RenderData ||
        RenderData->LODResources.Num() != 1)
    {
        OutError = TEXT("Cooked render-payload digest requires one CPU-readable raster LOD.");
        return false;
    }
    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    const FPositionVertexBuffer& Positions =
        Lod.VertexBuffers.PositionVertexBuffer;
    if (!Positions.GetVertexData() || Positions.GetNumVertices() == 0 ||
        Lod.IndexBuffer.GetIndexDataSize() <= 0)
    {
        OutError = TEXT("Cooked render-payload buffers are not CPU resident.");
        return false;
    }
    const FIndexArrayView Indices = Lod.IndexBuffer.GetArrayView();
    if (Indices.Num() <= 0 || Indices.Num() != Lod.GetNumTriangles() * 3)
    {
        OutError = TEXT("Cooked render-payload index buffer is incomplete.");
        return false;
    }

    TArray<uint8> Payload;
    Payload.Reserve(
        256 + static_cast<int32>(Positions.GetNumVertices()) * 12 +
        Indices.Num() * 4 + Lod.Sections.Num() * 32);
    AppendUtf8(Payload, RenderPayloadSchema);
    AppendUInt32LittleEndian(Payload, Positions.GetNumVertices());
    for (uint32 Index = 0; Index < Positions.GetNumVertices(); ++Index)
    {
        const FVector3f& Position = Positions.VertexPosition(Index);
        if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) ||
            !FMath::IsFinite(Position.Z))
        {
            OutError = TEXT("Cooked render-payload contains a non-finite position.");
            return false;
        }
        AppendFloatBits(Payload, Position.X);
        AppendFloatBits(Payload, Position.Y);
        AppendFloatBits(Payload, Position.Z);
    }
    AppendUInt32LittleEndian(Payload, static_cast<uint32>(Indices.Num()));
    for (int32 Index = 0; Index < Indices.Num(); ++Index)
    {
        if (Indices[Index] >= Positions.GetNumVertices())
        {
            OutError = TEXT("Cooked render-payload contains an out-of-range index.");
            return false;
        }
        AppendUInt32LittleEndian(Payload, Indices[Index]);
    }
    AppendUInt32LittleEndian(Payload, static_cast<uint32>(Lod.Sections.Num()));
    for (const FStaticMeshSection& Section : Lod.Sections)
    {
        AppendUInt32LittleEndian(Payload, static_cast<uint32>(Section.MaterialIndex));
        AppendUInt32LittleEndian(Payload, Section.FirstIndex);
        AppendUInt32LittleEndian(Payload, Section.NumTriangles);
        AppendUInt32LittleEndian(Payload, Section.MinVertexIndex);
        AppendUInt32LittleEndian(Payload, Section.MaxVertexIndex);
        AppendUInt32LittleEndian(Payload, Section.bEnableCollision ? 1u : 0u);
        AppendUInt32LittleEndian(Payload, Section.bCastShadow ? 1u : 0u);
        AppendUInt32LittleEndian(Payload, Section.bVisibleInRayTracing ? 1u : 0u);
        AppendUInt32LittleEndian(Payload, Section.bAffectDistanceFieldLighting ? 1u : 0u);
        AppendUInt32LittleEndian(Payload, Section.bForceOpaque ? 1u : 0u);
    }
    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    AppendUInt32LittleEndian(Payload, static_cast<uint32>(Materials.Num()));
    for (const FStaticMaterial& Material : Materials)
    {
        AppendUtf8(Payload, Material.MaterialSlotName.ToString());
#if WITH_EDITORONLY_DATA
        AppendUtf8(Payload, Material.ImportedMaterialSlotName.ToString());
#else
        AppendUtf8(Payload, Material.MaterialSlotName.ToString());
#endif
        AppendUtf8(
            Payload,
            Material.MaterialInterface
                ? Material.MaterialInterface->GetPathName()
                : FString());
    }
    OutSha256 = Sha256(Payload);
    if (!IsCanonicalUppercaseSha256(OutSha256))
    {
        OutError = TEXT("Cooked render-payload SHA-256 did not canonicalize.");
        OutSha256.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DMacDonaldHouseProvenance::
    IsCanonicalContract() const
{
    return SourceOutputSetSha256 ==
            TEXT("D72869336402E3A9FEAD8E2E747D655CAD21B79E416D3CF2D99007F3AA5D6FAA") &&
        RenderObjSha256 ==
            TEXT("BE049C7E1A8AB2D9DED98C027478EA5A5183F0C2CF2FFC206289D1FF47F955DE") &&
        CanonicalGeometrySha256 ==
            TEXT("9F629115A98BE5A257D06ACD864CE40170C8EFCE57BF60D7D145B85D7682C69C") &&
        PlacementReceiptSha256 ==
            TEXT("E5B40D02A3D712303A2D99734A42E9ADD68F4E528DED31DD763F37034961ED81") &&
        OSMFeatureGeometrySha256 ==
            TEXT("755536CDE8E666826FA6A325BCD707BDC7E0132C018BC87F0ABE8275F4557A39") &&
        OSMPartGeometrySha256 ==
            TEXT("B7204683A26E7DA56E1A97E516E36220C1764341CA485B904D736F509B481A34") &&
        SourceObjProjectRelativePath == ::SourceObjProjectRelativePath &&
        CookedRenderPayloadSchema == RenderPayloadSchema &&
        IsCanonicalUppercaseSha256(CookedRenderPayloadSha256) &&
        Components == 576 && SourceVertices == 4692 && Triangles == 7080 &&
        MaterialSlots == 9 && bProceduralTextureFreePbrPresentation &&
        !bProceduralPbrMaterialsCalibrated && bRenderOnly &&
        !bCollisionNavigationSensorOrRfAuthority &&
        !bSurveyAsBuiltOrOneToOne && !bProviderReadyLiveSuccessor;
}
