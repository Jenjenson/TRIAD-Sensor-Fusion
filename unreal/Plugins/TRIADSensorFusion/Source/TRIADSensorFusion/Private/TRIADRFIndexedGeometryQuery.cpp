#include "TRIADRFIndexedGeometryQuery.h"

#include <limits>
#include "TRIADGeodesy.h"

#include "Algo/Sort.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Math/Box.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 MaximumIdentifierCharacters = 256;
constexpr int32 MaximumAssetIdentifierCharacters = 160;
constexpr int32 HardMaximumJsonBytesPerDocument = 64 * 1024 * 1024;
constexpr int32 HardMaximumVertices = 1000000;
constexpr int32 HardMaximumTriangles = 1000000;
constexpr int32 HardMaximumSolids = 100000;
constexpr int32 HardMaximumSurfaces = 200000;
constexpr int32 HardMaximumWitnesses = 100000;
constexpr int32 HardMaximumMaterials = 4096;
constexpr int32 HardMaximumProfilesPerMaterial = 64;
constexpr int32 HardMaximumBVHDepth = 64;
constexpr int32 HardMaximumTrianglesPerLeaf = 64;
constexpr int64 HardMaximumOverlapPairChecks = 10000000;
constexpr int64 HardMaximumSelfIntersectionCandidateChecks = 10000000;
constexpr int64 HardMaximumCrossSolidTriangleCandidateChecks = 10000000;
constexpr int64 HardMaximumContainmentTriangleChecks = 30000000;
constexpr double MinimumTriangleAreaSquaredCentimeters = 1.0e-12;
constexpr double MinimumPositiveVolumeCubicCentimeters = 1.0e-9;
constexpr double DirectionParallelEpsilon = 1.0e-12;
constexpr double IncidenceTangentEpsilon = 1.0e-8;
constexpr double BarycentricTolerance = 1.0e-10;
constexpr double DeclaredGeometryToleranceCentimeters = 0.0001;
constexpr double TrianglePlaneParallelCrossSquaredTolerance = 1.0e-16;
constexpr double NormalAgreementCosine = 0.99999999;
constexpr const TCHAR* GeometrySchemaVersion =
    TEXT("triad.istana_public_view.rf_geometry.v1");
constexpr const TCHAR* MaterialCatalogSchemaVersion =
    TEXT("triad.rf_material_catalog.v1");
constexpr const TCHAR* ExpectedGeometryStatus =
    TEXT("SIMULATION_READY_ASSUMPTION_BOUND");
constexpr const TCHAR* ExpectedLogicalCoordinateSystem =
    TEXT("RIGHT_HANDED_Z_UP_LOCAL_METRES_APPROACH_PLUS_Y");
constexpr const TCHAR* RuntimeCoordinateSemantics =
    TEXT("UNREAL_CENTIMETERS_X_EAST_Y_NEGATED_APPROACH_Z_UP_OUTWARD_WINDING_PRESERVED");
constexpr const TCHAR* ExpectedModeledCoverageShape =
    TEXT("AXIS_ALIGNED_BOX");
constexpr const TCHAR* ExpectedModeledCoverageBoundaryInclusion =
    TEXT("CLOSED");
constexpr const TCHAR* ExpectedModeledCoverageFiniteSegmentPolicy =
    TEXT("BOTH_ENDPOINTS_AND_ENTIRE_FINITE_SEGMENT_MUST_BE_INSIDE_CLOSED_ENVELOPE");
constexpr const TCHAR* ExpectedModeledCoverageOutsideDomainPolicy =
    TEXT("REJECT_QUERY_NEVER_EMIT_CLEAR_OR_DIRECT_PATH");
constexpr const TCHAR* ExpectedModeledCoverageDerivation =
    TEXT("MINIMUM_CLOSED_AABB_CONTAINING_CANONICAL_VERTICES_AND_WITNESS_ENDPOINTS");
constexpr const TCHAR* ExpectedRuntimeModelSemantics =
    TEXT("PARAMETRIC_LOOKDEV_DIRECT_STRAIGHT_TRANSMISSION_SINGLE_SPECULAR_REFLECTION_V2_INCOHERENT_ONLY");

/**
 * Small self-contained SHA-256 implementation for canonical source binding.
 * FPlatformMisc::GetSHA256Signature is deliberately not used: the generic
 * implementation asserts at runtime on Win64 in installed UE 5.5 builds.
 */
class FPortableSHA256
{
public:
    void Update(const uint8* Data, uint64 ByteCount)
    {
        TotalByteCount += ByteCount;
        while (ByteCount > 0)
        {
            const uint32 CopyCount = static_cast<uint32>(FMath::Min<uint64>(
                ByteCount, 64 - BufferedByteCount));
            FMemory::Memcpy(Buffer + BufferedByteCount, Data, CopyCount);
            BufferedByteCount += CopyCount;
            Data += CopyCount;
            ByteCount -= CopyCount;
            if (BufferedByteCount == 64)
            {
                Transform(Buffer);
                BufferedByteCount = 0;
            }
        }
    }

    FString FinalHex()
    {
        const uint64 MessageBitCount = TotalByteCount * 8;
        Buffer[BufferedByteCount++] = 0x80;
        if (BufferedByteCount > 56)
        {
            while (BufferedByteCount < 64)
            {
                Buffer[BufferedByteCount++] = 0;
            }
            Transform(Buffer);
            BufferedByteCount = 0;
        }
        while (BufferedByteCount < 56)
        {
            Buffer[BufferedByteCount++] = 0;
        }
        for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
        {
            Buffer[56 + ByteIndex] = static_cast<uint8>(
                MessageBitCount >> (56 - ByteIndex * 8));
        }
        Transform(Buffer);

        FString Hex;
        Hex.Reserve(64);
        for (const uint32 Word : State)
        {
            for (int32 ByteIndex = 3; ByteIndex >= 0; --ByteIndex)
            {
                Hex += FString::Printf(
                    TEXT("%02x"),
                    static_cast<uint8>(Word >> (ByteIndex * 8)));
            }
        }
        return Hex;
    }

private:
    static uint32 RotateRight(uint32 Value, uint32 Count)
    {
        return (Value >> Count) | (Value << (32 - Count));
    }

    void Transform(const uint8 Block[64])
    {
        static constexpr uint32 RoundConstants[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32 Schedule[64];
        for (int32 Index = 0; Index < 16; ++Index)
        {
            Schedule[Index] =
                (static_cast<uint32>(Block[Index * 4]) << 24) |
                (static_cast<uint32>(Block[Index * 4 + 1]) << 16) |
                (static_cast<uint32>(Block[Index * 4 + 2]) << 8) |
                static_cast<uint32>(Block[Index * 4 + 3]);
        }
        for (int32 Index = 16; Index < 64; ++Index)
        {
            const uint32 SigmaZero =
                RotateRight(Schedule[Index - 15], 7) ^
                RotateRight(Schedule[Index - 15], 18) ^
                (Schedule[Index - 15] >> 3);
            const uint32 SigmaOne =
                RotateRight(Schedule[Index - 2], 17) ^
                RotateRight(Schedule[Index - 2], 19) ^
                (Schedule[Index - 2] >> 10);
            Schedule[Index] = Schedule[Index - 16] + SigmaZero +
                Schedule[Index - 7] + SigmaOne;
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
            const uint32 UpperSigmaOne =
                RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
            const uint32 Choice = (E & F) ^ ((~E) & G);
            const uint32 TemporaryOne = H + UpperSigmaOne + Choice +
                RoundConstants[Index] + Schedule[Index];
            const uint32 UpperSigmaZero =
                RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
            const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const uint32 TemporaryTwo = UpperSigmaZero + Majority;
            H = G;
            G = F;
            F = E;
            E = D + TemporaryOne;
            D = C;
            C = B;
            B = A;
            A = TemporaryOne + TemporaryTwo;
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

    uint32 State[8] = {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19};
    uint8 Buffer[64] = {};
    uint32 BufferedByteCount = 0;
    uint64 TotalByteCount = 0;
};

bool IsFiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN() &&
        FMath::IsFinite(Value.X) &&
        FMath::IsFinite(Value.Y) &&
        FMath::IsFinite(Value.Z);
}

bool IsPointInsideOrOnClosedBox(const FVector& Point, const FBox& Bounds)
{
    return IsFiniteVector(Point) && Bounds.IsValid &&
        Point.X >= Bounds.Min.X && Point.X <= Bounds.Max.X &&
        Point.Y >= Bounds.Min.Y && Point.Y <= Bounds.Max.Y &&
        Point.Z >= Bounds.Min.Z && Point.Z <= Bounds.Max.Z;
}

bool IsBoundedIdentifier(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > MaximumIdentifierCharacters)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        const bool bAllowed =
            FChar::IsAlnum(Character) || Character == TEXT('_') ||
            Character == TEXT('-') || Character == TEXT('.') ||
            Character == TEXT(':') || Character == TEXT('/');
        if (!bAllowed)
        {
            return false;
        }
    }
    return true;
}

bool IsSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

bool GetRequiredString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FString& OutValue,
    FString& OutError)
{
    if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue))
    {
        OutError = FString::Printf(TEXT("Required JSON string '%s' is missing."), Field);
        return false;
    }
    return true;
}

bool GetRequiredIdentifier(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FString& OutValue,
    FString& OutError)
{
    if (!GetRequiredString(Object, Field, OutValue, OutError) ||
        !IsBoundedIdentifier(OutValue))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("JSON identifier '%s' must use 1..256 conservative identifier characters."),
                Field);
        }
        return false;
    }
    return true;
}

bool GetRequiredNullableIdentifier(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FString& OutValue,
    FString& OutError)
{
    OutValue.Reset();
    if (!Object.IsValid())
    {
        OutError = FString::Printf(
            TEXT("Required nullable JSON identifier '%s' is missing."), Field);
        return false;
    }
    const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
    if (Value == nullptr || !Value->IsValid())
    {
        OutError = FString::Printf(
            TEXT("Required nullable JSON identifier '%s' is missing."), Field);
        return false;
    }
    if ((*Value)->Type == EJson::Null)
    {
        return true;
    }
    if ((*Value)->Type != EJson::String ||
        !(*Value)->TryGetString(OutValue) ||
        !IsBoundedIdentifier(OutValue))
    {
        OutValue.Reset();
        OutError = FString::Printf(
            TEXT("Nullable JSON identifier '%s' must be null or use conservative identifier characters."),
            Field);
        return false;
    }
    return true;
}

bool GetRequiredNumber(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    double& OutValue,
    FString& OutError)
{
    if (!Object.IsValid() || !Object->TryGetNumberField(Field, OutValue) ||
        !FMath::IsFinite(OutValue))
    {
        OutError = FString::Printf(TEXT("Required finite JSON number '%s' is missing."), Field);
        return false;
    }
    return true;
}

bool GetRequiredInteger(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    int32& OutValue,
    FString& OutError)
{
    double Number = 0.0;
    if (!GetRequiredNumber(Object, Field, Number, OutError) ||
        Number < 0.0 || Number > static_cast<double>(MAX_int32) ||
        FMath::FloorToDouble(Number) != Number)
    {
        OutError = FString::Printf(TEXT("JSON integer '%s' must be an exact non-negative int32."), Field);
        return false;
    }
    OutValue = static_cast<int32>(Number);
    return true;
}

bool GetRequiredBool(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    bool& OutValue,
    FString& OutError)
{
    if (!Object.IsValid() || !Object->TryGetBoolField(Field, OutValue))
    {
        OutError = FString::Printf(TEXT("Required JSON boolean '%s' is missing."), Field);
        return false;
    }
    return true;
}

bool GetRequiredBoolWithLegacyAlias(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const TCHAR* LegacyField,
    bool& OutValue,
    FString& OutError)
{
    if (Object.IsValid() && Object->HasField(Field) &&
        Object->HasField(LegacyField))
    {
        bool CurrentValue = false;
        bool LegacyValue = false;
        if (!GetRequiredBool(Object, Field, CurrentValue, OutError) ||
            !GetRequiredBool(Object, LegacyField, LegacyValue, OutError))
        {
            return false;
        }
        if (CurrentValue != LegacyValue)
        {
            OutError = FString::Printf(
                TEXT("JSON boolean '%s' conflicts with legacy alias '%s'."),
                Field,
                LegacyField);
            return false;
        }
        OutValue = CurrentValue;
        return true;
    }
    if (Object.IsValid() && Object->HasField(Field))
    {
        return GetRequiredBool(Object, Field, OutValue, OutError);
    }
    return GetRequiredBool(Object, LegacyField, OutValue, OutError);
}

bool GetRequiredArray(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const TArray<TSharedPtr<FJsonValue>>*& OutValues,
    FString& OutError)
{
    OutValues = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, OutValues) ||
        OutValues == nullptr)
    {
        OutError = FString::Printf(TEXT("Required JSON array '%s' is missing."), Field);
        return false;
    }
    return true;
}

bool GetRequiredObject(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    TSharedPtr<FJsonObject>& OutValue,
    FString& OutError)
{
    OutValue.Reset();
    const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
    if (!Object.IsValid() || !Object->TryGetObjectField(Field, ObjectValue) ||
        ObjectValue == nullptr || !ObjectValue->IsValid())
    {
        OutError = FString::Printf(TEXT("Required JSON object '%s' is missing."), Field);
        return false;
    }
    OutValue = *ObjectValue;
    return true;
}

bool ParseRoot(
    const FString& Json,
    TSharedPtr<FJsonObject>& OutRoot,
    FString& OutError)
{
    OutRoot.Reset();
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
    {
        OutError = TEXT("RF JSON document is not a valid JSON object.");
        return false;
    }
    return true;
}

bool ParseVectorArray(
    const TSharedPtr<FJsonValue>& Value,
    FVector& OutValue,
    FString& OutError)
{
    if (!Value.IsValid() || Value->Type != EJson::Array)
    {
        OutError = TEXT("RF vector must be a three-number JSON array.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Components = Value->AsArray();
    if (Components.Num() != 3)
    {
        OutError = TEXT("RF vector must contain exactly three numbers.");
        return false;
    }
    double Parsed[3] = {0.0, 0.0, 0.0};
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (!Components[Axis].IsValid() ||
            !Components[Axis]->TryGetNumber(Parsed[Axis]) ||
            !FMath::IsFinite(Parsed[Axis]))
        {
            OutError = TEXT("RF vector components must be finite numbers.");
            return false;
        }
    }
    OutValue = FVector(Parsed[0], Parsed[1], Parsed[2]);
    return true;
}

bool ParseObjectVector(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FVector& OutValue,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!GetRequiredArray(Object, Field, Values, OutError) || Values->Num() != 3)
    {
        OutError = FString::Printf(TEXT("JSON vector '%s' must contain exactly three numbers."), Field);
        return false;
    }
    double Parsed[3] = {0.0, 0.0, 0.0};
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (!(*Values)[Axis].IsValid() ||
            !(*Values)[Axis]->TryGetNumber(Parsed[Axis]) ||
            !FMath::IsFinite(Parsed[Axis]))
        {
            OutError = FString::Printf(TEXT("JSON vector '%s' contains a non-finite value."), Field);
            return false;
        }
    }
    OutValue = FVector(Parsed[0], Parsed[1], Parsed[2]);
    return true;
}

bool ParseNumberArray(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    TArray<double>& OutValues,
    FString& OutError)
{
    OutValues.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!GetRequiredArray(Object, Field, Values, OutError) || Values->IsEmpty())
    {
        OutError = FString::Printf(TEXT("JSON numeric array '%s' must be non-empty."), Field);
        return false;
    }
    OutValues.Reserve(Values->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        double Number = 0.0;
        if (!Value.IsValid() || !Value->TryGetNumber(Number) ||
            !FMath::IsFinite(Number))
        {
            OutError = FString::Printf(TEXT("JSON numeric array '%s' contains a non-finite value."), Field);
            return false;
        }
        OutValues.Add(Number);
    }
    return true;
}

bool ComputeUtf8Sha256(
    const FString& Text,
    FString& OutSha256,
    FString& OutError)
{
    FTCHARToUTF8 Utf8(*Text);
    FPortableSHA256 Hasher;
    Hasher.Update(
        reinterpret_cast<const uint8*>(Utf8.Get()),
        static_cast<uint64>(Utf8.Length()));
    OutSha256 = Hasher.FinalHex();
    if (!IsSha256(OutSha256))
    {
        OutError = TEXT("Portable SHA-256 calculation failed for RF JSON.");
        return false;
    }
    return true;
}

bool LoadCanonicalUtf8File(
    const FString& Path,
    int32 MaximumBytes,
    FString& OutText,
    FString& OutSha256,
    FString& OutError)
{
    OutText.Reset();
    OutSha256.Reset();
    const int64 FileSize = IFileManager::Get().FileSize(*Path);
    if (FileSize < 0)
    {
        OutError = FString::Printf(TEXT("RF JSON file does not exist: %s"), *Path);
        return false;
    }
    if (FileSize == 0 || FileSize > MaximumBytes || FileSize > MAX_int32)
    {
        OutError = FString::Printf(TEXT("RF JSON file size is outside the configured bound: %s"), *Path);
        return false;
    }
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != FileSize)
    {
        OutError = FString::Printf(TEXT("Could not read the complete RF JSON file: %s"), *Path);
        return false;
    }
    if (Bytes.Num() >= 3 && Bytes[0] == 0xef && Bytes[1] == 0xbb && Bytes[2] == 0xbf)
    {
        OutError = FString::Printf(TEXT("Canonical RF JSON must be UTF-8 without BOM: %s"), *Path);
        return false;
    }
    for (const uint8 Byte : Bytes)
    {
        if (Byte == 0 || Byte == '\r')
        {
            OutError = FString::Printf(TEXT("Canonical RF JSON must be NUL-free LF/UTF-8: %s"), *Path);
            return false;
        }
    }
    FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    OutText = FString(Converted.Length(), Converted.Get());
    const FTCHARToUTF8 RoundTrip(*OutText);
    if (RoundTrip.Length() != Bytes.Num() ||
        FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0)
    {
        OutText.Reset();
        OutError = FString::Printf(
            TEXT("Canonical RF JSON is not strict, round-trippable UTF-8: %s"),
            *Path);
        return false;
    }
    FPortableSHA256 Hasher;
    Hasher.Update(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
    OutSha256 = Hasher.FinalHex();
    if (!IsSha256(OutSha256))
    {
        OutText.Reset();
        OutSha256.Reset();
        OutError = FString::Printf(
            TEXT("Portable SHA-256 calculation failed for RF JSON file: %s"),
            *Path);
        return false;
    }
    return true;
}

FVector LogicalMetersToUnrealCentimeters(const FVector& LogicalMeters)
{
    return FVector(
        LogicalMeters.X * 100.0,
        -LogicalMeters.Y * 100.0,
        LogicalMeters.Z * 100.0);
}

FVector LogicalDirectionToUnreal(const FVector& LogicalDirection)
{
    return FVector(
        LogicalDirection.X,
        -LogicalDirection.Y,
        LogicalDirection.Z);
}

uint64 EdgeKey(int32 First, int32 Second)
{
    const uint32 Minimum = static_cast<uint32>(FMath::Min(First, Second));
    const uint32 Maximum = static_cast<uint32>(FMath::Max(First, Second));
    return (static_cast<uint64>(Minimum) << 32) | static_cast<uint64>(Maximum);
}

bool IntersectFiniteSegmentAabb(
    const FVector& Start,
    const FVector& End,
    const FBox& Bounds,
    double Padding)
{
    const FVector Direction = End - Start;
    double MinimumParameter = 0.0;
    double MaximumParameter = 1.0;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const double Lower = Bounds.Min[Axis] - Padding;
        const double Upper = Bounds.Max[Axis] + Padding;
        if (FMath::Abs(Direction[Axis]) <= DirectionParallelEpsilon)
        {
            if (Start[Axis] < Lower || Start[Axis] > Upper)
            {
                return false;
            }
            continue;
        }
        double First = (Lower - Start[Axis]) / Direction[Axis];
        double Second = (Upper - Start[Axis]) / Direction[Axis];
        if (First > Second)
        {
            Swap(First, Second);
        }
        MinimumParameter = FMath::Max(MinimumParameter, First);
        MaximumParameter = FMath::Min(MaximumParameter, Second);
        if (MinimumParameter > MaximumParameter)
        {
            return false;
        }
    }
    return true;
}

bool BoxesOverlapInclusive(
    const FBox& First,
    const FBox& Second,
    double ToleranceCentimeters)
{
    return First.IsValid && Second.IsValid &&
        First.Max.X >= Second.Min.X - ToleranceCentimeters &&
        Second.Max.X >= First.Min.X - ToleranceCentimeters &&
        First.Max.Y >= Second.Min.Y - ToleranceCentimeters &&
        Second.Max.Y >= First.Min.Y - ToleranceCentimeters &&
        First.Max.Z >= Second.Min.Z - ToleranceCentimeters &&
        Second.Max.Z >= First.Min.Z - ToleranceCentimeters;
}

struct FValidationPoint2
{
    double X = 0.0;
    double Y = 0.0;
};

FValidationPoint2 ProjectForValidation(const FVector& Point, int32 DroppedAxis)
{
    if (DroppedAxis == 0)
    {
        return {Point.Y, Point.Z};
    }
    if (DroppedAxis == 1)
    {
        return {Point.X, Point.Z};
    }
    return {Point.X, Point.Y};
}

FValidationPoint2 Subtract2(
    const FValidationPoint2& First,
    const FValidationPoint2& Second)
{
    return {First.X - Second.X, First.Y - Second.Y};
}

double Cross2(const FValidationPoint2& First, const FValidationPoint2& Second)
{
    return First.X * Second.Y - First.Y * Second.X;
}

double Cross2(
    const FValidationPoint2& A,
    const FValidationPoint2& B,
    const FValidationPoint2& C)
{
    return Cross2(Subtract2(B, A), Subtract2(C, A));
}

double Length2(const FValidationPoint2& Value)
{
    return FMath::Sqrt(Value.X * Value.X + Value.Y * Value.Y);
}

bool IsPointOnSegment2(
    const FValidationPoint2& Point,
    const FValidationPoint2& Start,
    const FValidationPoint2& End,
    double ToleranceCentimeters)
{
    const FValidationPoint2 Direction = Subtract2(End, Start);
    const double Length = Length2(Direction);
    if (Length <= DirectionParallelEpsilon)
    {
        return Length2(Subtract2(Point, Start)) <= ToleranceCentimeters;
    }
    if (FMath::Abs(Cross2(Direction, Subtract2(Point, Start))) >
        ToleranceCentimeters * Length)
    {
        return false;
    }
    const double Projection =
        (Point.X - Start.X) * Direction.X +
        (Point.Y - Start.Y) * Direction.Y;
    return Projection >= -ToleranceCentimeters * Length &&
        Projection <= Length * Length + ToleranceCentimeters * Length;
}

bool IsPointInTriangle2Inclusive(
    const FValidationPoint2& Point,
    const FValidationPoint2 Triangle[3],
    double ToleranceCentimeters)
{
    const double EdgeScale = FMath::Max(
        1.0,
        FMath::Max(
            Length2(Subtract2(Triangle[1], Triangle[0])),
            FMath::Max(
                Length2(Subtract2(Triangle[2], Triangle[1])),
                Length2(Subtract2(Triangle[0], Triangle[2])))));
    const double ToleranceArea = ToleranceCentimeters * EdgeScale;
    const double First = Cross2(Triangle[0], Triangle[1], Point);
    const double Second = Cross2(Triangle[1], Triangle[2], Point);
    const double Third = Cross2(Triangle[2], Triangle[0], Point);
    const bool bHasNegative =
        First < -ToleranceArea || Second < -ToleranceArea || Third < -ToleranceArea;
    const bool bHasPositive =
        First > ToleranceArea || Second > ToleranceArea || Third > ToleranceArea;
    return !(bHasNegative && bHasPositive);
}

bool CoplanarTrianglesHavePositiveAreaOverlap(
    const FValidationPoint2 First[3],
    const FValidationPoint2 Second[3],
    double ToleranceCentimeters)
{
    for (int32 TriangleIndex = 0; TriangleIndex < 2; ++TriangleIndex)
    {
        const FValidationPoint2* Triangle = TriangleIndex == 0 ? First : Second;
        for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
        {
            const FValidationPoint2 Edge = Subtract2(
                Triangle[(EdgeIndex + 1) % 3], Triangle[EdgeIndex]);
            const double AxisLength = Length2(Edge);
            if (AxisLength <= DirectionParallelEpsilon)
            {
                continue;
            }
            const FValidationPoint2 Axis = {-Edge.Y / AxisLength, Edge.X / AxisLength};
            double FirstMinimum = First[0].X * Axis.X + First[0].Y * Axis.Y;
            double FirstMaximum = FirstMinimum;
            double SecondMinimum = Second[0].X * Axis.X + Second[0].Y * Axis.Y;
            double SecondMaximum = SecondMinimum;
            for (int32 VertexIndex = 1; VertexIndex < 3; ++VertexIndex)
            {
                const double FirstProjection =
                    First[VertexIndex].X * Axis.X + First[VertexIndex].Y * Axis.Y;
                const double SecondProjection =
                    Second[VertexIndex].X * Axis.X + Second[VertexIndex].Y * Axis.Y;
                FirstMinimum = FMath::Min(FirstMinimum, FirstProjection);
                FirstMaximum = FMath::Max(FirstMaximum, FirstProjection);
                SecondMinimum = FMath::Min(SecondMinimum, SecondProjection);
                SecondMaximum = FMath::Max(SecondMaximum, SecondProjection);
            }
            const double Overlap =
                FMath::Min(FirstMaximum, SecondMaximum) -
                FMath::Max(FirstMinimum, SecondMinimum);
            if (Overlap <= ToleranceCentimeters)
            {
                return false;
            }
        }
    }
    return true;
}

void AppendUniqueValidationPoint(
    TArray<FVector>& Points,
    const FVector& Candidate,
    double ToleranceCentimeters)
{
    const double ToleranceSquared =
        ToleranceCentimeters * ToleranceCentimeters;
    for (const FVector& Existing : Points)
    {
        if (FVector::DistSquared(Existing, Candidate) <= ToleranceSquared)
        {
            return;
        }
    }
    Points.Add(Candidate);
}

void AppendCoplanarEdgeContacts(
    const FVector& FirstStart3,
    const FVector& FirstEnd3,
    const FVector& SecondStart3,
    const FVector& SecondEnd3,
    const FValidationPoint2& FirstStart2,
    const FValidationPoint2& FirstEnd2,
    const FValidationPoint2& SecondStart2,
    const FValidationPoint2& SecondEnd2,
    double ToleranceCentimeters,
    TArray<FVector>& OutContacts)
{
    const FValidationPoint2 FirstDirection = Subtract2(FirstEnd2, FirstStart2);
    const FValidationPoint2 SecondDirection = Subtract2(SecondEnd2, SecondStart2);
    const FValidationPoint2 StartDelta = Subtract2(SecondStart2, FirstStart2);
    const double Denominator = Cross2(FirstDirection, SecondDirection);
    const double Scale = FMath::Max(
        1.0,
        Length2(FirstDirection) + Length2(SecondDirection));
    const double CrossTolerance = ToleranceCentimeters * Scale;
    if (FMath::Abs(Denominator) > CrossTolerance)
    {
        const double FirstParameter = Cross2(StartDelta, SecondDirection) / Denominator;
        const double SecondParameter = Cross2(StartDelta, FirstDirection) / Denominator;
        const double ParameterTolerance = ToleranceCentimeters / Scale;
        if (FirstParameter >= -ParameterTolerance && FirstParameter <= 1.0 + ParameterTolerance &&
            SecondParameter >= -ParameterTolerance && SecondParameter <= 1.0 + ParameterTolerance)
        {
            AppendUniqueValidationPoint(
                OutContacts,
                FirstStart3 + (FirstEnd3 - FirstStart3) *
                    FMath::Clamp(FirstParameter, 0.0, 1.0),
                ToleranceCentimeters);
        }
        return;
    }
    if (FMath::Abs(Cross2(FirstDirection, StartDelta)) > CrossTolerance)
    {
        return;
    }
    if (IsPointOnSegment2(FirstStart2, SecondStart2, SecondEnd2, ToleranceCentimeters))
    {
        AppendUniqueValidationPoint(OutContacts, FirstStart3, ToleranceCentimeters);
    }
    if (IsPointOnSegment2(FirstEnd2, SecondStart2, SecondEnd2, ToleranceCentimeters))
    {
        AppendUniqueValidationPoint(OutContacts, FirstEnd3, ToleranceCentimeters);
    }
    if (IsPointOnSegment2(SecondStart2, FirstStart2, FirstEnd2, ToleranceCentimeters))
    {
        AppendUniqueValidationPoint(OutContacts, SecondStart3, ToleranceCentimeters);
    }
    if (IsPointOnSegment2(SecondEnd2, FirstStart2, FirstEnd2, ToleranceCentimeters))
    {
        AppendUniqueValidationPoint(OutContacts, SecondEnd3, ToleranceCentimeters);
    }
}

enum class EValidationTriangleContact : uint8
{
    None,
    Point,
    Segment,
    CoplanarArea
};

struct FValidationTriangleContact
{
    EValidationTriangleContact Kind = EValidationTriangleContact::None;
    FVector RepresentativePoint = FVector::ZeroVector;
};

enum class EValidationPointSolidRelation : uint8
{
    Outside,
    Boundary,
    Inside,
    Ambiguous
};

void CollectTrianglePlaneContacts(
    const FVector Triangle[3],
    const FVector& PlaneOrigin,
    const FVector& PlaneNormal,
    double ToleranceCentimeters,
    TArray<FVector>& OutPoints)
{
    double Distances[3];
    for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
    {
        Distances[VertexIndex] = FVector::DotProduct(
            Triangle[VertexIndex] - PlaneOrigin, PlaneNormal);
        if (FMath::Abs(Distances[VertexIndex]) <= ToleranceCentimeters)
        {
            AppendUniqueValidationPoint(
                OutPoints, Triangle[VertexIndex], ToleranceCentimeters);
        }
    }
    for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
    {
        const int32 NextIndex = (EdgeIndex + 1) % 3;
        const bool bStrictlyCrosses =
            (Distances[EdgeIndex] > ToleranceCentimeters &&
             Distances[NextIndex] < -ToleranceCentimeters) ||
            (Distances[EdgeIndex] < -ToleranceCentimeters &&
             Distances[NextIndex] > ToleranceCentimeters);
        if (!bStrictlyCrosses)
        {
            continue;
        }
        const double Parameter =
            Distances[EdgeIndex] /
            (Distances[EdgeIndex] - Distances[NextIndex]);
        AppendUniqueValidationPoint(
            OutPoints,
            Triangle[EdgeIndex] +
                (Triangle[NextIndex] - Triangle[EdgeIndex]) * Parameter,
            ToleranceCentimeters);
    }
}

FValidationTriangleContact ClassifyTriangleContact(
    const FVector First[3],
    const FVector Second[3],
    const FVector& FirstNormal,
    const FVector& SecondNormal,
    double ToleranceCentimeters)
{
    FValidationTriangleContact Result;
    FBox FirstBounds(ForceInit);
    FBox SecondBounds(ForceInit);
    for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
    {
        FirstBounds += First[VertexIndex];
        SecondBounds += Second[VertexIndex];
    }
    if (!BoxesOverlapInclusive(
            FirstBounds, SecondBounds, ToleranceCentimeters))
    {
        return Result;
    }

    const FVector PlaneLineDirection = FVector::CrossProduct(
        FirstNormal, SecondNormal);
    const auto IsTriangleWithinPlaneTolerance =
        [ToleranceCentimeters](
            const FVector Triangle[3],
            const FVector& PlaneOrigin,
            const FVector& PlaneNormal)
        {
            for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
            {
                if (FMath::Abs(FVector::DotProduct(
                        Triangle[VertexIndex] - PlaneOrigin,
                        PlaneNormal)) > ToleranceCentimeters)
                {
                    return false;
                }
            }
            return true;
        };
    const bool bMutuallyWithinPlaneTolerance =
        IsTriangleWithinPlaneTolerance(
            First, Second[0], SecondNormal) &&
        IsTriangleWithinPlaneTolerance(
            Second, First[0], FirstNormal);
    if (PlaneLineDirection.SizeSquared() <=
            TrianglePlaneParallelCrossSquaredTolerance &&
        !bMutuallyWithinPlaneTolerance)
    {
        return Result;
    }
    if (bMutuallyWithinPlaneTolerance)
    {
        const FVector AbsoluteNormal = FirstNormal.GetAbs();
        int32 DroppedAxis = 0;
        if (AbsoluteNormal.Y > AbsoluteNormal.X)
        {
            DroppedAxis = 1;
        }
        if (AbsoluteNormal.Z > AbsoluteNormal[DroppedAxis])
        {
            DroppedAxis = 2;
        }
        FValidationPoint2 First2[3];
        FValidationPoint2 Second2[3];
        for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
        {
            First2[VertexIndex] = ProjectForValidation(
                First[VertexIndex], DroppedAxis);
            Second2[VertexIndex] = ProjectForValidation(
                Second[VertexIndex], DroppedAxis);
        }
        TArray<FVector> Contacts;
        for (int32 FirstEdge = 0; FirstEdge < 3; ++FirstEdge)
        {
            for (int32 SecondEdge = 0; SecondEdge < 3; ++SecondEdge)
            {
                AppendCoplanarEdgeContacts(
                    First[FirstEdge], First[(FirstEdge + 1) % 3],
                    Second[SecondEdge], Second[(SecondEdge + 1) % 3],
                    First2[FirstEdge], First2[(FirstEdge + 1) % 3],
                    Second2[SecondEdge], Second2[(SecondEdge + 1) % 3],
                    ToleranceCentimeters, Contacts);
            }
        }
        for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
        {
            if (IsPointInTriangle2Inclusive(
                    First2[VertexIndex], Second2, ToleranceCentimeters))
            {
                AppendUniqueValidationPoint(
                    Contacts, First[VertexIndex], ToleranceCentimeters);
            }
            if (IsPointInTriangle2Inclusive(
                    Second2[VertexIndex], First2, ToleranceCentimeters))
            {
                AppendUniqueValidationPoint(
                    Contacts, Second[VertexIndex], ToleranceCentimeters);
            }
        }
        if (CoplanarTrianglesHavePositiveAreaOverlap(
                First2, Second2, ToleranceCentimeters))
        {
            Result.Kind = EValidationTriangleContact::CoplanarArea;
            if (!Contacts.IsEmpty())
            {
                for (const FVector& Point : Contacts)
                {
                    Result.RepresentativePoint += Point;
                }
                Result.RepresentativePoint /=
                    static_cast<double>(Contacts.Num());
            }
            else
            {
                Result.RepresentativePoint =
                    (First[0] + First[1] + First[2] +
                     Second[0] + Second[1] + Second[2]) / 6.0;
            }
            return Result;
        }
        if (Contacts.IsEmpty())
        {
            return Result;
        }
        Result.Kind = Contacts.Num() == 1
            ? EValidationTriangleContact::Point
            : EValidationTriangleContact::Segment;
        Result.RepresentativePoint = Contacts.Num() == 1
            ? Contacts[0]
            : (Contacts[0] + Contacts[1]) * 0.5;
        return Result;
    }

    TArray<FVector> FirstPlaneContacts;
    TArray<FVector> SecondPlaneContacts;
    CollectTrianglePlaneContacts(
        First, Second[0], SecondNormal,
        ToleranceCentimeters, FirstPlaneContacts);
    CollectTrianglePlaneContacts(
        Second, First[0], FirstNormal,
        ToleranceCentimeters, SecondPlaneContacts);
    if (FirstPlaneContacts.IsEmpty() || SecondPlaneContacts.IsEmpty())
    {
        return Result;
    }
    const FVector LineDirection = PlaneLineDirection.GetSafeNormal(
        TrianglePlaneParallelCrossSquaredTolerance);
    double FirstMinimum = FVector::DotProduct(FirstPlaneContacts[0], LineDirection);
    double FirstMaximum = FirstMinimum;
    for (const FVector& Point : FirstPlaneContacts)
    {
        const double Projection = FVector::DotProduct(Point, LineDirection);
        FirstMinimum = FMath::Min(FirstMinimum, Projection);
        FirstMaximum = FMath::Max(FirstMaximum, Projection);
    }
    double SecondMinimum = FVector::DotProduct(SecondPlaneContacts[0], LineDirection);
    double SecondMaximum = SecondMinimum;
    for (const FVector& Point : SecondPlaneContacts)
    {
        const double Projection = FVector::DotProduct(Point, LineDirection);
        SecondMinimum = FMath::Min(SecondMinimum, Projection);
        SecondMaximum = FMath::Max(SecondMaximum, Projection);
    }
    const double ContactMinimum = FMath::Max(FirstMinimum, SecondMinimum);
    const double ContactMaximum = FMath::Min(FirstMaximum, SecondMaximum);
    if (ContactMaximum < ContactMinimum - ToleranceCentimeters)
    {
        return Result;
    }
    const double ContactProjection =
        (ContactMinimum + ContactMaximum) * 0.5;
    Result.RepresentativePoint = FirstPlaneContacts[0] + LineDirection *
        (ContactProjection -
         FVector::DotProduct(FirstPlaneContacts[0], LineDirection));
    Result.Kind = ContactMaximum - ContactMinimum > ToleranceCentimeters
        ? EValidationTriangleContact::Segment
        : EValidationTriangleContact::Point;
    return Result;
}
}

struct FTRIADRFIndexedGeometryQuery::FImpl
{
    struct FRuntimeProfile
    {
        FString ProfileId;
        FString CalibrationProvenanceId;
        double MinimumFrequencyGHz = 0.0;
        double MaximumFrequencyGHz = 0.0;
        double MinimumIncidenceCosine = 0.0;
        double MaximumIncidenceCosine = 0.0;
        double PairedBoundaryTransmissionLossDb = 0.0;
        double BulkAttenuationDbPerMeter = 0.0;
        double ReflectionLossDb = 0.0;
        double EmpiricalGrazingReflectionLossDb = 0.0;
        bool bAllowsTransmission = false;
        bool bAllowsReflection = false;
    };

    struct FMaterial
    {
        FString MaterialId;
        FString ProvenanceId;
        FString CalibrationState;
        FString UncertaintyClass;
        TArray<FRuntimeProfile> Profiles;
    };

    struct FSolid
    {
        FString SolidId;
        FString PrimitiveType;
        FString Role;
        FString MaterialId;
        FString BoundaryRole;
        FString SourceClass;
        FString UncertaintyClass;
        FString ApertureId;
        FString ApertureState;
        int32 MaterialIndex = INDEX_NONE;
        int32 VertexStart = INDEX_NONE;
        int32 VertexCount = 0;
        int32 TriangleStart = INDEX_NONE;
        int32 TriangleCount = 0;
        FBox DeclaredBounds = FBox(ForceInit);
        FBox ActualBounds = FBox(ForceInit);
        bool bAxisAlignedBoxPrimitive = false;
        bool bIndexedClosedPolyhedronPrimitive = false;
    };

    struct FSurface
    {
        FString SurfaceId;
        FString SolidId;
        FString MaterialId;
        FString BoundaryRole;
        FString SourceClass;
        FString UncertaintyClass;
        FString ApertureId;
        FString ApertureState;
        int32 SolidIndex = INDEX_NONE;
        int32 MaterialIndex = INDEX_NONE;
        int32 TriangleStart = INDEX_NONE;
        int32 TriangleCount = 0;
        FVector DeclaredOutwardNormal = FVector::ZeroVector;
    };

    struct FTriangle
    {
        int32 VertexIndices[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
        int32 CanonicalTriangleIndex = INDEX_NONE;
        int32 SolidIndex = INDEX_NONE;
        int32 SurfaceIndex = INDEX_NONE;
        FVector OutwardNormal = FVector::ZeroVector;
        FBox Bounds = FBox(ForceInit);
        FVector Centroid = FVector::ZeroVector;
    };

    struct FBVHNode
    {
        FBox Bounds = FBox(ForceInit);
        int32 FirstPermutationIndex = 0;
        int32 TriangleCount = 0;
        int32 LeftChild = INDEX_NONE;
        int32 RightChild = INDEX_NONE;

        bool IsLeaf() const
        {
            return LeftChild == INDEX_NONE && RightChild == INDEX_NONE;
        }
    };

    enum class ETriangleContact : uint8
    {
        None,
        Hit,
        CoplanarAmbiguous,
        EndpointAmbiguous
    };

    struct FRawHit
    {
        double Parameter = 0.0;
        int32 TriangleIndex = INDEX_NONE;
        bool bOnTriangleEdge = false;
        ETRIADRFBoundaryCrossing Crossing = ETRIADRFBoundaryCrossing::Entry;
    };

    bool bReady = false;
    FTRIADRFIndexedGeometryMetadata Metadata;
    FBox ModeledCoverageBounds = FBox(ForceInit);
    TArray<FVector> Vertices;
    TArray<FTriangle> Triangles;
    TArray<FSolid> Solids;
    TArray<FSurface> Surfaces;
    TArray<FMaterial> Materials;
    TMap<FString, int32> MaterialById;
    TMap<FString, int32> SolidById;
    TMap<FString, int32> SurfaceById;
    TArray<int32> TrianglePermutation;
    TArray<FBVHNode> BVHNodes;

    bool Parse(
        const FString& GeometryJson,
        const FString& MaterialCatalogJson,
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError);
    bool ParseMaterialCatalog(
        const TSharedPtr<FJsonObject>& Root,
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError);
    bool ParseGeometry(
        const TSharedPtr<FJsonObject>& Root,
        const FString& ActualCatalogSha256,
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError);
    bool ValidateTopologyAndBindings(
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError);
    bool ValidateAxisAlignedBoxMesh(
        const FSolid& Solid,
        FString& OutError) const;
    bool ValidateTriangleSelfIntersections(
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError) const;
    bool ValidatePositiveVolumeOverlaps(
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError) const;
    bool ClassifyPointAgainstSolid(
        const FVector& Point,
        int32 SolidIndex,
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        int64& InOutTriangleChecks,
        EValidationPointSolidRelation& OutRelation,
        FString& OutError) const;
    bool GenericSolidsHavePositiveVolumeOverlap(
        int32 FirstSolidIndex,
        int32 SecondSolidIndex,
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        int64& InOutTriangleCandidateChecks,
        int64& InOutContainmentTriangleChecks,
        bool& bOutOverlap,
        FString& OutError) const;
    int32 BuildBVHNode(
        int32 FirstPermutationIndex,
        int32 TriangleCount,
        int32 Depth,
        const FTRIADRFIndexedGeometryLoadLimits& Limits,
        FString& OutError);
    bool Trace(
        const FVector& Start,
        const FVector& End,
        double PositionToleranceCentimeters,
        TArray<FTRIADRFIndexedSegmentHit>& OutHits,
        FString& OutError) const;
    bool IsFiniteSegmentWithinModeledCoverage(
        const FVector& Start,
        const FVector& End,
        FString& OutError) const;
    ETriangleContact IntersectTriangle(
        int32 TriangleIndex,
        const FVector& Start,
        const FVector& End,
        double ParameterTolerance,
        double PositionToleranceCentimeters,
        FRawHit& OutHit) const;
    const FRuntimeProfile* SelectProfile(
        int32 MaterialIndex,
        double FrequencyGHz,
        FString& OutError) const;
};

bool FTRIADRFIndexedGeometryQuery::FImpl::ParseMaterialCatalog(
    const TSharedPtr<FJsonObject>& Root,
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError)
{
    if (!GetRequiredString(Root, TEXT("schemaVersion"), Metadata.MaterialCatalogSchemaVersion, OutError) ||
        Metadata.MaterialCatalogSchemaVersion != MaterialCatalogSchemaVersion)
    {
        OutError = TEXT("Unsupported RF material-catalog schemaVersion.");
        return false;
    }
    if (!GetRequiredIdentifier(Root, TEXT("catalogId"), Metadata.MaterialCatalogId, OutError))
    {
        return false;
    }
    FString CatalogStatus;
    if (!GetRequiredString(Root, TEXT("status"), CatalogStatus, OutError) ||
        CatalogStatus != TEXT("UNCALIBRATED_ASSUMPTION_PRIORS"))
    {
        OutError = TEXT("RF material catalog must retain UNCALIBRATED_ASSUMPTION_PRIORS status.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* MaterialValues = nullptr;
    if (!GetRequiredArray(Root, TEXT("materials"), MaterialValues, OutError) ||
        MaterialValues->IsEmpty() || MaterialValues->Num() > Limits.MaximumMaterials)
    {
        OutError = TEXT("RF material count is empty or exceeds the configured bound.");
        return false;
    }

    Materials.Reserve(MaterialValues->Num());
    for (const TSharedPtr<FJsonValue>& MaterialValue : *MaterialValues)
    {
        if (!MaterialValue.IsValid() || MaterialValue->Type != EJson::Object)
        {
            OutError = TEXT("Each RF material must be a JSON object.");
            return false;
        }
        const TSharedPtr<FJsonObject> MaterialObject = MaterialValue->AsObject();
        FMaterial Material;
        if (!GetRequiredIdentifier(MaterialObject, TEXT("materialId"), Material.MaterialId, OutError) ||
            !GetRequiredIdentifier(MaterialObject, TEXT("provenanceId"), Material.ProvenanceId, OutError) ||
            !GetRequiredString(MaterialObject, TEXT("calibrationState"), Material.CalibrationState, OutError) ||
            !GetRequiredString(MaterialObject, TEXT("uncertaintyClass"), Material.UncertaintyClass, OutError))
        {
            return false;
        }
        if (Material.CalibrationState != TEXT("UNCALIBRATED_ASSUMPTION"))
        {
            OutError = FString::Printf(
                TEXT("RF material '%s' is not an explicit uncalibrated assumption."),
                *Material.MaterialId);
            return false;
        }
        if (MaterialById.Contains(Material.MaterialId))
        {
            OutError = FString::Printf(TEXT("Duplicate RF materialId '%s'."), *Material.MaterialId);
            return false;
        }

        TSharedPtr<FJsonObject> FrequencyResponse;
        if (!GetRequiredObject(MaterialObject, TEXT("frequencyResponse"), FrequencyResponse, OutError))
        {
            return false;
        }
        TArray<double> Frequencies;
        TArray<double> Permittivity;
        TArray<double> LossTangent;
        TArray<double> Conductivity;
        if (!ParseNumberArray(FrequencyResponse, TEXT("frequencyGHz"), Frequencies, OutError) ||
            !ParseNumberArray(FrequencyResponse, TEXT("relativePermittivityReal"), Permittivity, OutError) ||
            !ParseNumberArray(FrequencyResponse, TEXT("lossTangent"), LossTangent, OutError) ||
            !ParseNumberArray(FrequencyResponse, TEXT("conductivitySiemensPerMeter"), Conductivity, OutError) ||
            Frequencies.Num() != Permittivity.Num() || Frequencies.Num() != LossTangent.Num() ||
            Frequencies.Num() != Conductivity.Num())
        {
            OutError = FString::Printf(TEXT("RF material '%s' has inconsistent frequency-response arrays."), *Material.MaterialId);
            return false;
        }
        for (int32 Index = 0; Index < Frequencies.Num(); ++Index)
        {
            if (Frequencies[Index] <= 0.0 || Permittivity[Index] < 1.0 ||
                LossTangent[Index] < 0.0 || Conductivity[Index] < 0.0 ||
                (Index > 0 && Frequencies[Index] <= Frequencies[Index - 1]))
            {
                OutError = FString::Printf(TEXT("RF material '%s' has invalid or unordered physical priors."), *Material.MaterialId);
                return false;
            }
        }

        TSharedPtr<FJsonObject> RuntimeProfileObject;
        if (!GetRequiredObject(MaterialObject, TEXT("runtimeInteractionProfileV2"), RuntimeProfileObject, OutError))
        {
            OutError = FString::Printf(
                TEXT("RF material '%s' has no explicit runtimeInteractionProfileV2; coefficients will not be invented."),
                *Material.MaterialId);
            return false;
        }
        FString RuntimeSemantics;
        FString RuntimeCalibrationState;
        FString RuntimeCalibrationProvenanceId;
        if (!GetRequiredString(RuntimeProfileObject, TEXT("modelSemantics"), RuntimeSemantics, OutError) ||
            RuntimeSemantics != ExpectedRuntimeModelSemantics ||
            !GetRequiredString(RuntimeProfileObject, TEXT("calibrationState"), RuntimeCalibrationState, OutError) ||
            RuntimeCalibrationState != TEXT("UNCALIBRATED") ||
            !GetRequiredIdentifier(RuntimeProfileObject, TEXT("calibrationProvenanceId"), RuntimeCalibrationProvenanceId, OutError))
        {
            OutError = FString::Printf(TEXT("RF material '%s' has an incompatible runtime interaction contract."), *Material.MaterialId);
            return false;
        }
        Metadata.RuntimeSemantics = RuntimeSemantics;

        const TArray<TSharedPtr<FJsonValue>>* ProfileValues = nullptr;
        if (!GetRequiredArray(RuntimeProfileObject, TEXT("profiles"), ProfileValues, OutError) ||
            ProfileValues->IsEmpty() || ProfileValues->Num() > Limits.MaximumProfilesPerMaterial)
        {
            OutError = FString::Printf(TEXT("RF material '%s' has an invalid bounded profile array."), *Material.MaterialId);
            return false;
        }
        TSet<FString> ProfileIds;
        for (const TSharedPtr<FJsonValue>& ProfileValue : *ProfileValues)
        {
            if (!ProfileValue.IsValid() || ProfileValue->Type != EJson::Object)
            {
                OutError = TEXT("Every RF runtime profile must be a JSON object.");
                return false;
            }
            const TSharedPtr<FJsonObject> ProfileObject = ProfileValue->AsObject();
            FRuntimeProfile Profile;
            Profile.CalibrationProvenanceId = RuntimeCalibrationProvenanceId;
            if (!GetRequiredIdentifier(ProfileObject, TEXT("profileId"), Profile.ProfileId, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("minimumFrequencyGHz"), Profile.MinimumFrequencyGHz, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("maximumFrequencyGHz"), Profile.MaximumFrequencyGHz, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("minimumIncidenceCosine"), Profile.MinimumIncidenceCosine, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("maximumIncidenceCosine"), Profile.MaximumIncidenceCosine, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("pairedBoundaryTransmissionLossDb"), Profile.PairedBoundaryTransmissionLossDb, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("bulkAttenuationDbPerMeter"), Profile.BulkAttenuationDbPerMeter, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("reflectionLossDb"), Profile.ReflectionLossDb, OutError) ||
                !GetRequiredNumber(ProfileObject, TEXT("empiricalGrazingReflectionLossDb"), Profile.EmpiricalGrazingReflectionLossDb, OutError) ||
                !GetRequiredBool(ProfileObject, TEXT("allowsTransmission"), Profile.bAllowsTransmission, OutError) ||
                !GetRequiredBool(ProfileObject, TEXT("allowsReflection"), Profile.bAllowsReflection, OutError))
            {
                return false;
            }
            if (ProfileIds.Contains(Profile.ProfileId) ||
                Profile.MinimumFrequencyGHz <= 0.0 ||
                Profile.MaximumFrequencyGHz < Profile.MinimumFrequencyGHz ||
                Profile.MinimumIncidenceCosine < IncidenceTangentEpsilon ||
                Profile.MaximumIncidenceCosine < Profile.MinimumIncidenceCosine ||
                Profile.MaximumIncidenceCosine > 1.0 ||
                Profile.PairedBoundaryTransmissionLossDb < 0.0 ||
                Profile.BulkAttenuationDbPerMeter < 0.0 ||
                Profile.ReflectionLossDb < 0.0 ||
                Profile.EmpiricalGrazingReflectionLossDb < 0.0)
            {
                OutError = FString::Printf(TEXT("RF runtime profile '%s' is duplicated or has invalid bounds/coefficient values."), *Profile.ProfileId);
                return false;
            }
            ProfileIds.Add(Profile.ProfileId);
            Material.Profiles.Add(MoveTemp(Profile));
        }
        Material.Profiles.Sort([](const FRuntimeProfile& First, const FRuntimeProfile& Second)
        {
            if (First.MinimumFrequencyGHz != Second.MinimumFrequencyGHz)
            {
                return First.MinimumFrequencyGHz < Second.MinimumFrequencyGHz;
            }
            return First.ProfileId < Second.ProfileId;
        });
        for (int32 FirstIndex = 0; FirstIndex < Material.Profiles.Num(); ++FirstIndex)
        {
            for (int32 SecondIndex = FirstIndex + 1; SecondIndex < Material.Profiles.Num(); ++SecondIndex)
            {
                const FRuntimeProfile& First = Material.Profiles[FirstIndex];
                const FRuntimeProfile& Second = Material.Profiles[SecondIndex];
                if (FMath::Max(First.MinimumFrequencyGHz, Second.MinimumFrequencyGHz) <
                    FMath::Min(First.MaximumFrequencyGHz, Second.MaximumFrequencyGHz))
                {
                    OutError = FString::Printf(TEXT("RF material '%s' has overlapping runtime profile interiors."), *Material.MaterialId);
                    return false;
                }
            }
        }
        MaterialById.Add(Material.MaterialId, Materials.Num());
        Materials.Add(MoveTemp(Material));
    }
    return true;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::ParseGeometry(
    const TSharedPtr<FJsonObject>& Root,
    const FString& ActualCatalogSha256,
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError)
{
    if (!GetRequiredString(Root, TEXT("schemaVersion"), Metadata.GeometrySchemaVersion, OutError) ||
        Metadata.GeometrySchemaVersion != GeometrySchemaVersion)
    {
        OutError = TEXT("Unsupported canonical RF geometry schemaVersion.");
        return false;
    }
    FString AssetId;
    if (!GetRequiredIdentifier(Root, TEXT("assetId"), AssetId, OutError) ||
        AssetId.Len() > MaximumAssetIdentifierCharacters)
    {
        OutError = TEXT("RF assetId must be a conservative identifier no longer than 160 characters.");
        return false;
    }
    if (!GetRequiredIdentifier(Root, TEXT("revision"), Metadata.GeometryRevision, OutError) ||
        !GetRequiredString(Root, TEXT("status"), Metadata.GeometryStatus, OutError) ||
        Metadata.GeometryStatus != ExpectedGeometryStatus)
    {
        OutError = TEXT("Canonical RF geometry does not retain SIMULATION_READY_ASSUMPTION_BOUND status.");
        return false;
    }
    FString ExpectedCatalogSha256;
    if (!GetRequiredString(Root, TEXT("contractSha256"), Metadata.ContractSha256, OutError) ||
        !IsSha256(Metadata.ContractSha256) ||
        !GetRequiredString(Root, TEXT("materialCatalogSha256"), ExpectedCatalogSha256, OutError) ||
        !IsSha256(ExpectedCatalogSha256) ||
        !ExpectedCatalogSha256.Equals(ActualCatalogSha256, ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Canonical RF geometry has an invalid contract hash or its material-catalog SHA-256 does not match the loaded catalog.");
        return false;
    }
    Metadata.MaterialCatalogSha256 = ActualCatalogSha256.ToLower();
    Metadata.GeometryQueryId = FString::Printf(
        TEXT("%s@catalog-sha256:%s"), *AssetId, *Metadata.MaterialCatalogSha256);
    if (Metadata.GeometryQueryId.Len() > MaximumIdentifierCharacters)
    {
        OutError = TEXT("Hash-bound RF geometryQueryId exceeds 256 characters.");
        return false;
    }

    TSharedPtr<FJsonObject> CoordinateObject;
    FString LogicalSystem;
    if (!GetRequiredObject(Root, TEXT("coordinateContract"), CoordinateObject, OutError) ||
        !GetRequiredString(CoordinateObject, TEXT("logicalSystem"), LogicalSystem, OutError) ||
        LogicalSystem != ExpectedLogicalCoordinateSystem)
    {
        OutError = TEXT("Canonical RF geometry coordinate contract is unsupported.");
        return false;
    }
    Metadata.CoordinateSemantics = RuntimeCoordinateSemantics;

    TSharedPtr<FJsonObject> CoverageObject;
    TSharedPtr<FJsonObject> CoverageBoundsObject;
    FString CoverageBoundaryInclusion;
    FString CoverageDerivation;
    bool bCoversCanonicalGeometry = false;
    bool bCoversOfflineWitnessEndpoints = false;
    if (!GetRequiredObject(Root, TEXT("modeledCoverageEnvelope"), CoverageObject, OutError) ||
        !GetRequiredIdentifier(CoverageObject, TEXT("coverageId"), Metadata.ModeledCoverageId, OutError) ||
        !GetRequiredIdentifier(CoverageObject, TEXT("scope"), Metadata.ModeledCoverageScope, OutError) ||
        !GetRequiredString(CoverageObject, TEXT("shape"), Metadata.ModeledCoverageShape, OutError) ||
        Metadata.ModeledCoverageShape != ExpectedModeledCoverageShape ||
        !GetRequiredString(CoverageObject, TEXT("boundaryInclusion"), CoverageBoundaryInclusion, OutError) ||
        CoverageBoundaryInclusion != ExpectedModeledCoverageBoundaryInclusion ||
        !GetRequiredString(CoverageObject, TEXT("finiteSegmentPolicy"), Metadata.ModeledCoverageFiniteSegmentPolicy, OutError) ||
        Metadata.ModeledCoverageFiniteSegmentPolicy != ExpectedModeledCoverageFiniteSegmentPolicy ||
        !GetRequiredString(CoverageObject, TEXT("outsideDomainPolicy"), Metadata.ModeledCoverageOutsideDomainPolicy, OutError) ||
        Metadata.ModeledCoverageOutsideDomainPolicy != ExpectedModeledCoverageOutsideDomainPolicy ||
        !GetRequiredString(CoverageObject, TEXT("derivation"), CoverageDerivation, OutError) ||
        CoverageDerivation != ExpectedModeledCoverageDerivation ||
        !GetRequiredBoolWithLegacyAlias(
            CoverageObject,
            TEXT("coversCanonicalGeometry"),
            TEXT("coversCanonicalHeroGeometry"),
            bCoversCanonicalGeometry,
            OutError) ||
        !GetRequiredBool(CoverageObject, TEXT("coversOfflineWitnessEndpoints"), bCoversOfflineWitnessEndpoints, OutError) ||
        !GetRequiredBool(CoverageObject, TEXT("coversOneKilometreAoi"), Metadata.bModeledCoverageCoversOneKilometreAoi, OutError) ||
        !GetRequiredBool(CoverageObject, TEXT("coversSurroundings"), Metadata.bModeledCoverageCoversSurroundings, OutError) ||
        !GetRequiredBool(CoverageObject, TEXT("surveyControlled"), Metadata.bModeledCoverageSurveyControlled, OutError) ||
        !GetRequiredBool(CoverageObject, TEXT("fieldValidated"), Metadata.bModeledCoverageFieldValidated, OutError) ||
        !bCoversCanonicalGeometry || !bCoversOfflineWitnessEndpoints)
    {
        if (OutError.Contains(TEXT("conflicts with legacy alias")))
        {
            return false;
        }
        OutError = TEXT("Canonical RF modeled coverage must declare a bounded non-empty scope, a closed AABB query contract, and coverage of its canonical geometry and witness endpoints. Coverage identity and claim flags are scenario-pinned metadata, not loader constants.");
        return false;
    }
    FVector LogicalCoverageMinimum;
    FVector LogicalCoverageMaximum;
    if (!GetRequiredObject(CoverageObject, TEXT("boundsMeters"), CoverageBoundsObject, OutError) ||
        !ParseObjectVector(CoverageBoundsObject, TEXT("min"), LogicalCoverageMinimum, OutError) ||
        !ParseObjectVector(CoverageBoundsObject, TEXT("max"), LogicalCoverageMaximum, OutError) ||
        LogicalCoverageMinimum.X >= LogicalCoverageMaximum.X ||
        LogicalCoverageMinimum.Y >= LogicalCoverageMaximum.Y ||
        LogicalCoverageMinimum.Z >= LogicalCoverageMaximum.Z)
    {
        OutError = TEXT("Canonical RF modeled coverage bounds are invalid or empty.");
        return false;
    }
    ModeledCoverageBounds = FBox(
        FVector(
            LogicalCoverageMinimum.X * 100.0,
            -LogicalCoverageMaximum.Y * 100.0,
            LogicalCoverageMinimum.Z * 100.0),
        FVector(
            LogicalCoverageMaximum.X * 100.0,
            -LogicalCoverageMinimum.Y * 100.0,
            LogicalCoverageMaximum.Z * 100.0));
    if (!ModeledCoverageBounds.IsValid ||
        !IsFiniteVector(ModeledCoverageBounds.Min) ||
        !IsFiniteVector(ModeledCoverageBounds.Max))
    {
        OutError = TEXT("Canonical RF modeled coverage bounds overflow Unreal centimetres.");
        return false;
    }
    Metadata.ModeledCoverageMinimumCentimeters = ModeledCoverageBounds.Min;
    Metadata.ModeledCoverageMaximumCentimeters = ModeledCoverageBounds.Max;

    // Optional additive V2 study-domain metadata. TightV1 deliberately omits
    // this object and retains its exact closed-AABB behavior.
    const TSharedPtr<FJsonObject>* StudyDomainPointer = nullptr;
    if (CoverageObject->TryGetObjectField(TEXT("studyDomain"), StudyDomainPointer))
    {
        const TSharedPtr<FJsonObject> StudyDomain =
            StudyDomainPointer ? *StudyDomainPointer : nullptr;
        TSharedPtr<FJsonObject> CenterWgs84;
        TSharedPtr<FJsonObject> ConfigurationBinding;
        TSharedPtr<FJsonObject> PerimeterSampling;
        TSharedPtr<FJsonObject> Frame;
        TSharedPtr<FJsonObject> TruthFlags;
        FString Shape;
        FString BoundaryInclusion;
        FString PointMembershipPolicy;
        FString StudyFiniteSegmentPolicy;
        bool bContainsEntireFiniteSegment = false;
        if (!GetRequiredIdentifier(StudyDomain, TEXT("domainId"), Metadata.StudyDomainId, OutError) ||
            !GetRequiredIdentifier(StudyDomain, TEXT("domainType"), Metadata.StudyDomainType, OutError) ||
            Metadata.StudyDomainType != TEXT("CLOSED_WGS84_GEODESIC_CIRCLE") ||
            !GetRequiredString(StudyDomain, TEXT("distanceMethod"), Metadata.StudyDomainDistanceMethod, OutError) ||
            Metadata.StudyDomainDistanceMethod != TEXT("SIGNED_WGS84_VINCENTY_INVERSE_GEODESIC_CENTER_DISTANCE_MINUS_RADIUS_METERS") ||
            !GetRequiredString(StudyDomain, TEXT("shape"), Shape, OutError) || Shape != TEXT("CIRCLE") ||
            !GetRequiredString(StudyDomain, TEXT("boundaryInclusion"), BoundaryInclusion, OutError) ||
            BoundaryInclusion != TEXT("CLOSED") ||
            !GetRequiredString(StudyDomain, TEXT("pointMembershipPolicy"), PointMembershipPolicy, OutError) ||
            PointMembershipPolicy != TEXT("WGS84_VINCENTY_INVERSE_SURFACE_DISTANCE_METERS_FROM_CENTER_TO_ENDPOINT_LESS_THAN_OR_EQUAL_TO_RADIUS_METERS") ||
            !GetRequiredString(StudyDomain, TEXT("finiteSegmentPolicy"), StudyFiniteSegmentPolicy, OutError) ||
            StudyFiniteSegmentPolicy != TEXT("BOTH_ENDPOINTS_MUST_HAVE_SIGNED_WGS84_VINCENTY_DISTANCE_LESS_THAN_OR_EQUAL_TO_ZERO; THE CLOSED 1000_M_GEODESIC_DISK_IS CONVEX FOR THE ADMITTED MINIMIZING SURFACE SEGMENT") ||
            !GetRequiredBool(StudyDomain, TEXT("containsEntireFiniteSegmentWhenBothEndpointsInside"), bContainsEntireFiniteSegment, OutError) ||
            !bContainsEntireFiniteSegment ||
            !GetRequiredNumber(StudyDomain, TEXT("radiusMeters"), Metadata.StudyDomainRadiusMeters, OutError) ||
            !FMath::IsFinite(Metadata.StudyDomainRadiusMeters) || Metadata.StudyDomainRadiusMeters <= 0.0 ||
            !GetRequiredObject(StudyDomain, TEXT("centerWgs84Degrees"), CenterWgs84, OutError) ||
            !GetRequiredNumber(CenterWgs84, TEXT("longitude"), Metadata.StudyDomainCenterWgs84Degrees.X, OutError) ||
            !GetRequiredNumber(CenterWgs84, TEXT("latitude"), Metadata.StudyDomainCenterWgs84Degrees.Y, OutError) ||
            !FMath::IsFinite(Metadata.StudyDomainCenterWgs84Degrees.X) ||
            !FMath::IsFinite(Metadata.StudyDomainCenterWgs84Degrees.Y) ||
            Metadata.StudyDomainCenterWgs84Degrees.X < -180.0 || Metadata.StudyDomainCenterWgs84Degrees.X > 180.0 ||
            Metadata.StudyDomainCenterWgs84Degrees.Y < -90.0 || Metadata.StudyDomainCenterWgs84Degrees.Y > 90.0 ||
            !GetRequiredObject(StudyDomain, TEXT("configurationBinding"), ConfigurationBinding, OutError) ||
            !GetRequiredBool(ConfigurationBinding, TEXT("enabled"), Metadata.bStudyDomainConfigurationEnabled, OutError) ||
            !GetRequiredString(ConfigurationBinding, TEXT("referenceName"), Metadata.StudyDomainConfigurationReferenceName, OutError) ||
            !GetRequiredString(ConfigurationBinding, TEXT("shape"), Metadata.StudyDomainConfigurationShape, OutError) ||
            !Metadata.bStudyDomainConfigurationEnabled ||
            Metadata.StudyDomainConfigurationReferenceName != Metadata.StudyDomainId ||
            Metadata.StudyDomainConfigurationShape != TEXT("Circle") ||
            !GetRequiredObject(StudyDomain, TEXT("perimeterSampling"), PerimeterSampling, OutError) ||
            !GetRequiredInteger(PerimeterSampling, TEXT("count"), Metadata.StudyDomainPerimeterSampleCount, OutError) ||
            Metadata.StudyDomainPerimeterSampleCount < 4 || Metadata.StudyDomainPerimeterSampleCount > 4096 ||
            !GetRequiredNumber(PerimeterSampling, TEXT("startAzimuthDegrees"), Metadata.StudyDomainPerimeterStartAzimuthDegrees, OutError) ||
            !GetRequiredNumber(PerimeterSampling, TEXT("stepDegrees"), Metadata.StudyDomainPerimeterStepDegrees, OutError) ||
            !GetRequiredString(PerimeterSampling, TEXT("azimuthConvention"), Metadata.StudyDomainPerimeterAzimuthConvention, OutError) ||
            Metadata.StudyDomainPerimeterAzimuthConvention != TEXT("CLOCKWISE_FROM_TRUE_NORTH") ||
            !FMath::IsFinite(Metadata.StudyDomainPerimeterStartAzimuthDegrees) ||
            !FMath::IsFinite(Metadata.StudyDomainPerimeterStepDegrees) ||
            !FMath::IsNearlyEqual(Metadata.StudyDomainPerimeterStepDegrees * Metadata.StudyDomainPerimeterSampleCount, 360.0, 1.0e-9) ||
            !GetRequiredObject(StudyDomain, TEXT("frame"), Frame, OutError) ||
            !GetRequiredString(Frame, TEXT("sourceGeodeticCrs"), Metadata.StudyDomainSourceGeodeticCrs, OutError) ||
            !GetRequiredString(Frame, TEXT("projectedConstructionCrs"), Metadata.StudyDomainProjectedConstructionCrs, OutError) ||
            !GetRequiredString(Frame, TEXT("logicalSystem"), Metadata.StudyDomainLogicalSystem, OutError) ||
            !GetRequiredString(Frame, TEXT("studyMembershipFrame"), Metadata.StudyDomainMembershipFrame, OutError) ||
            !GetRequiredBool(Frame, TEXT("surveyRegistered"), Metadata.bStudyDomainSurveyRegistered, OutError) ||
            !GetRequiredObject(StudyDomain, TEXT("truthFlags"), TruthFlags, OutError) ||
            !GetRequiredBool(TruthFlags, TEXT("containedByLoaderCoverageAabb"), Metadata.bStudyDomainContainedByLoaderCoverageAabb, OutError) ||
            !GetRequiredBool(TruthFlags, TEXT("coversEntireRadiusCircle"), Metadata.bStudyDomainCoversEntireRadiusCircle, OutError) ||
            !GetRequiredBool(TruthFlags, TEXT("cornersOutsideCircleExcludedFromStudy"), Metadata.bStudyDomainCornersOutsideCircleExcluded, OutError) ||
            Metadata.StudyDomainSourceGeodeticCrs != TEXT("EPSG:4326") ||
            Metadata.StudyDomainMembershipFrame != TEXT("WGS84_GEODETIC_SURFACE_HORIZONTAL_ALTITUDE_IGNORED") ||
            Metadata.StudyDomainLogicalSystem != LogicalSystem || Metadata.bStudyDomainSurveyRegistered ||
            !Metadata.bStudyDomainContainedByLoaderCoverageAabb ||
            !Metadata.bStudyDomainCoversEntireRadiusCircle ||
            !Metadata.bStudyDomainCornersOutsideCircleExcluded)
        {
            OutError = TEXT("Canonical RF closed WGS84 geodesic-circle studyDomain is malformed or weakens its fail-closed frame/perimeter/truth contract.");
            return false;
        }
        Metadata.bHasClosedWgs84GeodesicCircleStudyDomain = true;
    }

    const TArray<TSharedPtr<FJsonValue>>* CoverageExclusionValues = nullptr;
    if (!GetRequiredArray(CoverageObject, TEXT("explicitExclusions"), CoverageExclusionValues, OutError) ||
        CoverageExclusionValues->IsEmpty())
    {
        OutError = TEXT("Canonical RF modeled coverage must retain explicit exclusions.");
        return false;
    }
    for (const TSharedPtr<FJsonValue>& ExclusionValue : *CoverageExclusionValues)
    {
        FString Exclusion;
        if (!ExclusionValue.IsValid() || !ExclusionValue->TryGetString(Exclusion) || Exclusion.IsEmpty())
        {
            OutError = TEXT("Canonical RF modeled coverage exclusions must be non-empty strings.");
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* VertexValues = nullptr;
    if (!GetRequiredArray(Root, TEXT("verticesMeters"), VertexValues, OutError) ||
        VertexValues->IsEmpty() || VertexValues->Num() > Limits.MaximumVertices)
    {
        OutError = TEXT("Canonical RF vertex count is empty or exceeds the configured bound.");
        return false;
    }
    FBox DerivedLogicalCoverageBounds(ForceInit);
    Vertices.Reserve(VertexValues->Num());
    for (const TSharedPtr<FJsonValue>& VertexValue : *VertexValues)
    {
        FVector LogicalVertex;
        if (!ParseVectorArray(VertexValue, LogicalVertex, OutError))
        {
            return false;
        }
        const FVector UnrealVertex = LogicalMetersToUnrealCentimeters(LogicalVertex);
        if (!IsFiniteVector(UnrealVertex))
        {
            OutError = TEXT("Canonical RF vertex overflows Unreal centimetres.");
            return false;
        }
        if (!IsPointInsideOrOnClosedBox(UnrealVertex, ModeledCoverageBounds))
        {
            OutError = TEXT("Canonical RF vertex lies outside the declared modeled coverage envelope.");
            return false;
        }
        DerivedLogicalCoverageBounds += LogicalVertex;
        Vertices.Add(UnrealVertex);
    }

    const TArray<TSharedPtr<FJsonValue>>* WitnessValues = nullptr;
    if (!GetRequiredArray(Root, TEXT("witnesses"), WitnessValues, OutError) ||
        WitnessValues->IsEmpty() || WitnessValues->Num() > Limits.MaximumWitnesses)
    {
        OutError = TEXT("Canonical RF witness count is empty or exceeds the configured bound.");
        return false;
    }
    TSet<FString> WitnessIds;
    for (const TSharedPtr<FJsonValue>& WitnessValue : *WitnessValues)
    {
        if (!WitnessValue.IsValid() || WitnessValue->Type != EJson::Object)
        {
            OutError = TEXT("Every canonical RF witness must be a JSON object.");
            return false;
        }
        const TSharedPtr<FJsonObject> WitnessObject = WitnessValue->AsObject();
        FString WitnessId;
        FVector LogicalStart;
        FVector LogicalEnd;
        if (!GetRequiredIdentifier(WitnessObject, TEXT("witnessId"), WitnessId, OutError) ||
            !ParseObjectVector(WitnessObject, TEXT("startMeters"), LogicalStart, OutError) ||
            !ParseObjectVector(WitnessObject, TEXT("endMeters"), LogicalEnd, OutError))
        {
            const FString WitnessError = OutError;
            OutError = FString::Printf(
                TEXT("Canonical RF witness is malformed: %s"),
                *WitnessError);
            return false;
        }
        if (WitnessIds.Contains(WitnessId))
        {
            OutError = FString::Printf(
                TEXT("Canonical RF witness ID '%s' is duplicated."),
                *WitnessId);
            return false;
        }
        WitnessIds.Add(WitnessId);
        DerivedLogicalCoverageBounds += LogicalStart;
        DerivedLogicalCoverageBounds += LogicalEnd;
    }
    if (!DerivedLogicalCoverageBounds.IsValid ||
        DerivedLogicalCoverageBounds.Min.X != LogicalCoverageMinimum.X ||
        DerivedLogicalCoverageBounds.Min.Y != LogicalCoverageMinimum.Y ||
        DerivedLogicalCoverageBounds.Min.Z != LogicalCoverageMinimum.Z ||
        DerivedLogicalCoverageBounds.Max.X != LogicalCoverageMaximum.X ||
        DerivedLogicalCoverageBounds.Max.Y != LogicalCoverageMaximum.Y ||
        DerivedLogicalCoverageBounds.Max.Z != LogicalCoverageMaximum.Z)
    {
        OutError = TEXT(
            "Canonical RF modeled coverage bounds are not the exact minimum closed AABB containing canonical vertices and witness endpoints.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* TriangleValues = nullptr;
    if (!GetRequiredArray(Root, TEXT("triangles"), TriangleValues, OutError) ||
        TriangleValues->IsEmpty() || TriangleValues->Num() > Limits.MaximumTriangles)
    {
        OutError = TEXT("Canonical RF triangle count is empty or exceeds the configured bound.");
        return false;
    }
    Triangles.Reserve(TriangleValues->Num());
    for (int32 CanonicalIndex = 0; CanonicalIndex < TriangleValues->Num(); ++CanonicalIndex)
    {
        const TSharedPtr<FJsonValue>& TriangleValue = (*TriangleValues)[CanonicalIndex];
        if (!TriangleValue.IsValid() || TriangleValue->Type != EJson::Array ||
            TriangleValue->AsArray().Num() != 3)
        {
            OutError = TEXT("Every canonical RF triangle must contain exactly three vertex indices.");
            return false;
        }
        int32 LogicalIndices[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            double IndexNumber = 0.0;
            if (!TriangleValue->AsArray()[Corner]->TryGetNumber(IndexNumber) ||
                !FMath::IsFinite(IndexNumber) || IndexNumber < 0.0 ||
                IndexNumber >= Vertices.Num() || FMath::FloorToDouble(IndexNumber) != IndexNumber)
            {
                OutError = TEXT("Canonical RF triangle contains an invalid vertex index.");
                return false;
            }
            LogicalIndices[Corner] = static_cast<int32>(IndexNumber);
        }
        FTriangle Triangle;
        Triangle.CanonicalTriangleIndex = CanonicalIndex;
        // Logical->Unreal includes one reflection, so reverse exactly once.
        Triangle.VertexIndices[0] = LogicalIndices[0];
        Triangle.VertexIndices[1] = LogicalIndices[2];
        Triangle.VertexIndices[2] = LogicalIndices[1];
        if (Triangle.VertexIndices[0] == Triangle.VertexIndices[1] ||
            Triangle.VertexIndices[1] == Triangle.VertexIndices[2] ||
            Triangle.VertexIndices[2] == Triangle.VertexIndices[0])
        {
            OutError = TEXT("Canonical RF triangle repeats a vertex index.");
            return false;
        }
        const FVector& A = Vertices[Triangle.VertexIndices[0]];
        const FVector& B = Vertices[Triangle.VertexIndices[1]];
        const FVector& C = Vertices[Triangle.VertexIndices[2]];
        const FVector Cross = FVector::CrossProduct(B - A, C - A);
        if (!IsFiniteVector(Cross) || Cross.SizeSquared() <= MinimumTriangleAreaSquaredCentimeters)
        {
            OutError = TEXT("Canonical RF triangle is degenerate or non-finite.");
            return false;
        }
        Triangle.OutwardNormal = Cross.GetSafeNormal();
        Triangle.Bounds += A;
        Triangle.Bounds += B;
        Triangle.Bounds += C;
        Triangle.Centroid = (A + B + C) / 3.0;
        Triangles.Add(MoveTemp(Triangle));
    }

    const TArray<TSharedPtr<FJsonValue>>* SolidValues = nullptr;
    if (!GetRequiredArray(Root, TEXT("solids"), SolidValues, OutError) ||
        SolidValues->IsEmpty() || SolidValues->Num() > Limits.MaximumSolids)
    {
        OutError = TEXT("Canonical RF solid count is empty or exceeds the configured bound.");
        return false;
    }
    TArray<int32> VertexOwner;
    VertexOwner.Init(INDEX_NONE, Vertices.Num());
    Solids.Reserve(SolidValues->Num());
    for (const TSharedPtr<FJsonValue>& SolidValue : *SolidValues)
    {
        if (!SolidValue.IsValid() || SolidValue->Type != EJson::Object)
        {
            OutError = TEXT("Every canonical RF solid must be a JSON object.");
            return false;
        }
        const TSharedPtr<FJsonObject> SolidObject = SolidValue->AsObject();
        FSolid Solid;
        if (!GetRequiredIdentifier(SolidObject, TEXT("solidId"), Solid.SolidId, OutError) ||
            !GetRequiredIdentifier(SolidObject, TEXT("role"), Solid.Role, OutError) ||
            !GetRequiredIdentifier(SolidObject, TEXT("materialId"), Solid.MaterialId, OutError) ||
            !GetRequiredIdentifier(SolidObject, TEXT("boundaryRole"), Solid.BoundaryRole, OutError) ||
            !GetRequiredIdentifier(SolidObject, TEXT("sourceClass"), Solid.SourceClass, OutError) ||
            !GetRequiredIdentifier(SolidObject, TEXT("uncertaintyClass"), Solid.UncertaintyClass, OutError) ||
            !GetRequiredNullableIdentifier(SolidObject, TEXT("apertureId"), Solid.ApertureId, OutError) ||
            !GetRequiredNullableIdentifier(SolidObject, TEXT("apertureState"), Solid.ApertureState, OutError) ||
            !GetRequiredInteger(SolidObject, TEXT("vertexStart"), Solid.VertexStart, OutError) ||
            !GetRequiredInteger(SolidObject, TEXT("vertexCount"), Solid.VertexCount, OutError) ||
            !GetRequiredInteger(SolidObject, TEXT("triangleStart"), Solid.TriangleStart, OutError) ||
            !GetRequiredInteger(SolidObject, TEXT("triangleCount"), Solid.TriangleCount, OutError))
        {
            return false;
        }
        const int32* MaterialIndex = MaterialById.Find(Solid.MaterialId);
        if (SolidById.Contains(Solid.SolidId) || MaterialIndex == nullptr ||
            Solid.ApertureId.IsEmpty() != Solid.ApertureState.IsEmpty() ||
            Solid.VertexCount < 4 || Solid.TriangleCount < 4 ||
            Solid.VertexStart > Vertices.Num() - Solid.VertexCount ||
            Solid.TriangleStart > Triangles.Num() - Solid.TriangleCount)
        {
            OutError = FString::Printf(TEXT("RF solid '%s' is duplicate, unbound, empty, or out of range."), *Solid.SolidId);
            return false;
        }
        Solid.MaterialIndex = *MaterialIndex;
        TBitArray<> UsedSolidVertices(false, Solid.VertexCount);
        for (int32 VertexIndex = Solid.VertexStart;
             VertexIndex < Solid.VertexStart + Solid.VertexCount;
             ++VertexIndex)
        {
            if (VertexOwner[VertexIndex] != INDEX_NONE)
            {
                OutError = TEXT("Canonical RF solid vertex ranges overlap.");
                return false;
            }
            VertexOwner[VertexIndex] = Solids.Num();
            Solid.ActualBounds += Vertices[VertexIndex];
        }
        for (int32 TriangleIndex = Solid.TriangleStart;
             TriangleIndex < Solid.TriangleStart + Solid.TriangleCount;
             ++TriangleIndex)
        {
            FTriangle& Triangle = Triangles[TriangleIndex];
            if (Triangle.SolidIndex != INDEX_NONE)
            {
                OutError = TEXT("Canonical RF solid triangle ranges overlap.");
                return false;
            }
            Triangle.SolidIndex = Solids.Num();
            for (const int32 VertexIndex : Triangle.VertexIndices)
            {
                if (VertexIndex < Solid.VertexStart || VertexIndex >= Solid.VertexStart + Solid.VertexCount)
                {
                    OutError = FString::Printf(TEXT("RF solid '%s' triangle references a vertex outside its stable vertex range."), *Solid.SolidId);
                    return false;
                }
                UsedSolidVertices[VertexIndex - Solid.VertexStart] = true;
            }
        }
        if (UsedSolidVertices.Contains(false))
        {
            OutError = FString::Printf(
                TEXT("RF solid '%s' contains an unused vertex in its stable vertex range."),
                *Solid.SolidId);
            return false;
        }

        TSharedPtr<FJsonObject> BoundsObject;
        FVector LogicalMinimum;
        FVector LogicalMaximum;
        if (!GetRequiredObject(SolidObject, TEXT("boundsMeters"), BoundsObject, OutError) ||
            !ParseObjectVector(BoundsObject, TEXT("min"), LogicalMinimum, OutError) ||
            !ParseObjectVector(BoundsObject, TEXT("max"), LogicalMaximum, OutError) ||
            LogicalMinimum.X >= LogicalMaximum.X || LogicalMinimum.Y >= LogicalMaximum.Y ||
            LogicalMinimum.Z >= LogicalMaximum.Z)
        {
            OutError = FString::Printf(TEXT("RF solid '%s' has invalid declared bounds."), *Solid.SolidId);
            return false;
        }
        Solid.DeclaredBounds = FBox(
            FVector(LogicalMinimum.X * 100.0, -LogicalMaximum.Y * 100.0, LogicalMinimum.Z * 100.0),
            FVector(LogicalMaximum.X * 100.0, -LogicalMinimum.Y * 100.0, LogicalMaximum.Z * 100.0));
        if (!Solid.ActualBounds.Min.Equals(Solid.DeclaredBounds.Min, DeclaredGeometryToleranceCentimeters) ||
            !Solid.ActualBounds.Max.Equals(Solid.DeclaredBounds.Max, DeclaredGeometryToleranceCentimeters))
        {
            OutError = FString::Printf(TEXT("RF solid '%s' declared bounds do not match its indexed vertices."), *Solid.SolidId);
            return false;
        }

        TSharedPtr<FJsonObject> PrimitiveObject;
        if (!GetRequiredObject(
                SolidObject,
                TEXT("primitive"),
                PrimitiveObject,
                OutError))
        {
            OutError = FString::Printf(
                TEXT("RF solid '%s' has no canonical finite primitive."),
                *Solid.SolidId);
            return false;
        }
        FString PrimitiveType;
        if (!GetRequiredString(
                PrimitiveObject, TEXT("type"), PrimitiveType, OutError))
        {
            OutError = FString::Printf(TEXT("RF solid '%s' declares an unsupported primitive."), *Solid.SolidId);
            return false;
        }
        Solid.PrimitiveType = PrimitiveType;
        if (PrimitiveType == TEXT("AXIS_ALIGNED_BOX"))
        {
            FVector PrimitiveMinimum;
            FVector PrimitiveMaximum;
            if (!ParseObjectVector(PrimitiveObject, TEXT("minMeters"), PrimitiveMinimum, OutError) ||
                !ParseObjectVector(PrimitiveObject, TEXT("maxMeters"), PrimitiveMaximum, OutError) ||
                PrimitiveMinimum.X >= PrimitiveMaximum.X ||
                PrimitiveMinimum.Y >= PrimitiveMaximum.Y ||
                PrimitiveMinimum.Z >= PrimitiveMaximum.Z)
            {
                OutError = FString::Printf(
                    TEXT("RF solid '%s' primitive bounds are invalid."),
                    *Solid.SolidId);
                return false;
            }
            const FBox PrimitiveBounds(
                FVector(PrimitiveMinimum.X * 100.0, -PrimitiveMaximum.Y * 100.0, PrimitiveMinimum.Z * 100.0),
                FVector(PrimitiveMaximum.X * 100.0, -PrimitiveMinimum.Y * 100.0, PrimitiveMaximum.Z * 100.0));
            if (!PrimitiveBounds.Min.Equals(Solid.DeclaredBounds.Min, DeclaredGeometryToleranceCentimeters) ||
                !PrimitiveBounds.Max.Equals(Solid.DeclaredBounds.Max, DeclaredGeometryToleranceCentimeters))
            {
                OutError = FString::Printf(TEXT("RF solid '%s' primitive bounds do not match declared bounds."), *Solid.SolidId);
                return false;
            }
            Solid.bAxisAlignedBoxPrimitive = true;
        }
        else if (PrimitiveType == TEXT("INDEXED_CLOSED_POLYHEDRON"))
        {
            Solid.bIndexedClosedPolyhedronPrimitive = true;
        }
        else
        {
            OutError = FString::Printf(
                TEXT("RF solid '%s' declares an unsupported primitive."),
                *Solid.SolidId);
            return false;
        }
        SolidById.Add(Solid.SolidId, Solids.Num());
        Solids.Add(MoveTemp(Solid));
    }
    if (VertexOwner.Contains(INDEX_NONE))
    {
        OutError = TEXT("Canonical RF geometry contains vertices outside every stable solid range.");
        return false;
    }
    for (const FTriangle& Triangle : Triangles)
    {
        if (Triangle.SolidIndex == INDEX_NONE)
        {
            OutError = TEXT("Canonical RF geometry contains triangles outside every stable solid range.");
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* SurfaceValues = nullptr;
    if (!GetRequiredArray(Root, TEXT("surfaces"), SurfaceValues, OutError) ||
        SurfaceValues->IsEmpty() || SurfaceValues->Num() > Limits.MaximumSurfaces)
    {
        OutError = TEXT("Canonical RF surface count is empty or exceeds the configured bound.");
        return false;
    }
    TSet<FString> SurfaceIds;
    Surfaces.Reserve(SurfaceValues->Num());
    for (const TSharedPtr<FJsonValue>& SurfaceValue : *SurfaceValues)
    {
        if (!SurfaceValue.IsValid() || SurfaceValue->Type != EJson::Object)
        {
            OutError = TEXT("Every canonical RF surface must be a JSON object.");
            return false;
        }
        const TSharedPtr<FJsonObject> SurfaceObject = SurfaceValue->AsObject();
        FSurface Surface;
        FVector LogicalOutwardNormal;
        if (!GetRequiredIdentifier(SurfaceObject, TEXT("surfaceId"), Surface.SurfaceId, OutError) ||
            !GetRequiredIdentifier(SurfaceObject, TEXT("solidId"), Surface.SolidId, OutError) ||
            !GetRequiredIdentifier(SurfaceObject, TEXT("materialId"), Surface.MaterialId, OutError) ||
            !GetRequiredIdentifier(SurfaceObject, TEXT("boundaryRole"), Surface.BoundaryRole, OutError) ||
            !GetRequiredIdentifier(SurfaceObject, TEXT("sourceClass"), Surface.SourceClass, OutError) ||
            !GetRequiredIdentifier(SurfaceObject, TEXT("uncertaintyClass"), Surface.UncertaintyClass, OutError) ||
            !GetRequiredNullableIdentifier(SurfaceObject, TEXT("apertureId"), Surface.ApertureId, OutError) ||
            !GetRequiredNullableIdentifier(SurfaceObject, TEXT("apertureState"), Surface.ApertureState, OutError) ||
            !GetRequiredInteger(SurfaceObject, TEXT("triangleStart"), Surface.TriangleStart, OutError) ||
            !GetRequiredInteger(SurfaceObject, TEXT("triangleCount"), Surface.TriangleCount, OutError) ||
            !ParseObjectVector(SurfaceObject, TEXT("outwardNormal"), LogicalOutwardNormal, OutError))
        {
            return false;
        }
        const int32* SolidIndex = SolidById.Find(Surface.SolidId);
        const int32* MaterialIndex = MaterialById.Find(Surface.MaterialId);
        if (SurfaceIds.Contains(Surface.SurfaceId) || SolidIndex == nullptr || MaterialIndex == nullptr ||
            Surface.ApertureId.IsEmpty() != Surface.ApertureState.IsEmpty() ||
            Surface.TriangleCount <= 0 || Surface.TriangleStart > Triangles.Num() - Surface.TriangleCount)
        {
            OutError = FString::Printf(TEXT("RF surface '%s' is duplicate, unbound, empty, or out of range."), *Surface.SurfaceId);
            return false;
        }
        Surface.SolidIndex = *SolidIndex;
        Surface.MaterialIndex = *MaterialIndex;
        const FSolid& OwnerSolid = Solids[Surface.SolidIndex];
        if (OwnerSolid.MaterialIndex != Surface.MaterialIndex ||
            OwnerSolid.BoundaryRole != Surface.BoundaryRole ||
            OwnerSolid.SourceClass != Surface.SourceClass ||
            OwnerSolid.UncertaintyClass != Surface.UncertaintyClass ||
            OwnerSolid.ApertureId != Surface.ApertureId ||
            OwnerSolid.ApertureState != Surface.ApertureState)
        {
            OutError = FString::Printf(
                TEXT("RF surface '%s' material/provenance metadata differs from its finite solid."),
                *Surface.SurfaceId);
            return false;
        }
        Surface.DeclaredOutwardNormal = LogicalDirectionToUnreal(LogicalOutwardNormal);
        if (!IsFiniteVector(Surface.DeclaredOutwardNormal) ||
            !FMath::IsNearlyEqual(Surface.DeclaredOutwardNormal.SizeSquared(), 1.0, 1.0e-8))
        {
            OutError = FString::Printf(TEXT("RF surface '%s' outwardNormal must be unit length."), *Surface.SurfaceId);
            return false;
        }
        Surface.DeclaredOutwardNormal.Normalize();
        for (int32 TriangleIndex = Surface.TriangleStart;
             TriangleIndex < Surface.TriangleStart + Surface.TriangleCount;
             ++TriangleIndex)
        {
            FTriangle& Triangle = Triangles[TriangleIndex];
            if (Triangle.SurfaceIndex != INDEX_NONE || Triangle.SolidIndex != Surface.SolidIndex ||
                FVector::DotProduct(Triangle.OutwardNormal, Surface.DeclaredOutwardNormal) < NormalAgreementCosine)
            {
                OutError = FString::Printf(TEXT("RF surface '%s' range overlaps, crosses a solid, or disagrees with triangle winding."), *Surface.SurfaceId);
                return false;
            }
            Triangle.SurfaceIndex = Surfaces.Num();
        }
        SurfaceIds.Add(Surface.SurfaceId);
        SurfaceById.Add(Surface.SurfaceId, Surfaces.Num());
        Surfaces.Add(MoveTemp(Surface));
    }
    for (const FTriangle& Triangle : Triangles)
    {
        if (Triangle.SurfaceIndex == INDEX_NONE)
        {
            OutError = TEXT("Canonical RF geometry contains triangles outside every stable surface range.");
            return false;
        }
    }
    return true;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::ValidateAxisAlignedBoxMesh(
    const FSolid& Solid,
    FString& OutError) const
{
    if (Solid.VertexCount != 8 || Solid.TriangleCount != 12)
    {
        OutError = FString::Printf(
            TEXT("RF axis-aligned box '%s' must have exactly 8 vertices and 12 triangles."),
            *Solid.SolidId);
        return false;
    }

    TSet<int32> CornerMasks;
    for (int32 VertexOffset = 0; VertexOffset < Solid.VertexCount; ++VertexOffset)
    {
        const FVector& Vertex = Vertices[Solid.VertexStart + VertexOffset];
        int32 CornerMask = 0;
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const double Coordinate = Vertex[Axis];
            if (FMath::IsNearlyEqual(
                    Coordinate,
                    Solid.ActualBounds.Min[Axis],
                    DeclaredGeometryToleranceCentimeters))
            {
                continue;
            }
            if (FMath::IsNearlyEqual(
                    Coordinate,
                    Solid.ActualBounds.Max[Axis],
                    DeclaredGeometryToleranceCentimeters))
            {
                CornerMask |= 1 << Axis;
                continue;
            }
            OutError = FString::Printf(
                TEXT("RF axis-aligned box '%s' has a vertex that is not a declared min/max corner."),
                *Solid.SolidId);
            return false;
        }
        if (CornerMasks.Contains(CornerMask))
        {
            OutError = FString::Printf(
                TEXT("RF axis-aligned box '%s' does not contain eight unique min/max corners."),
                *Solid.SolidId);
            return false;
        }
        CornerMasks.Add(CornerMask);
    }
    if (CornerMasks.Num() != 8)
    {
        OutError = FString::Printf(
            TEXT("RF axis-aligned box '%s' does not contain all eight min/max corners."),
            *Solid.SolidId);
        return false;
    }

    const FVector ExpectedNormals[6] = {
        -FVector::ForwardVector,
        FVector::ForwardVector,
        -FVector::RightVector,
        FVector::RightVector,
        -FVector::UpVector,
        FVector::UpVector};
    int32 TriangleCounts[6] = {};
    double TriangleAreas[6] = {};
    for (int32 TriangleOffset = 0;
         TriangleOffset < Solid.TriangleCount;
         ++TriangleOffset)
    {
        const FTriangle& Triangle =
            Triangles[Solid.TriangleStart + TriangleOffset];
        int32 PlaneIndex = INDEX_NONE;
        for (int32 CandidatePlane = 0; CandidatePlane < 6; ++CandidatePlane)
        {
            const int32 Axis = CandidatePlane / 2;
            const double PlaneCoordinate = (CandidatePlane & 1) == 0
                ? Solid.ActualBounds.Min[Axis]
                : Solid.ActualBounds.Max[Axis];
            bool bWhollyOnPlane = true;
            for (const int32 VertexIndex : Triangle.VertexIndices)
            {
                bWhollyOnPlane &= FMath::IsNearlyEqual(
                    Vertices[VertexIndex][Axis],
                    PlaneCoordinate,
                    DeclaredGeometryToleranceCentimeters);
            }
            if (bWhollyOnPlane)
            {
                if (PlaneIndex != INDEX_NONE)
                {
                    OutError = FString::Printf(
                        TEXT("RF axis-aligned box '%s' has a degenerate triangle assigned to multiple face planes."),
                        *Solid.SolidId);
                    return false;
                }
                PlaneIndex = CandidatePlane;
            }
        }
        if (PlaneIndex == INDEX_NONE ||
            FVector::DotProduct(
                Triangle.OutwardNormal,
                ExpectedNormals[PlaneIndex]) < NormalAgreementCosine)
        {
            OutError = FString::Printf(
                TEXT("RF axis-aligned box '%s' has a triangle off its six box planes or with incorrect outward winding."),
                *Solid.SolidId);
            return false;
        }
        const FVector& A = Vertices[Triangle.VertexIndices[0]];
        const FVector& B = Vertices[Triangle.VertexIndices[1]];
        const FVector& C = Vertices[Triangle.VertexIndices[2]];
        ++TriangleCounts[PlaneIndex];
        TriangleAreas[PlaneIndex] +=
            FVector::CrossProduct(B - A, C - A).Size() * 0.5;
    }

    const FVector Size = Solid.ActualBounds.GetSize();
    const double ExpectedAreas[6] = {
        Size.Y * Size.Z,
        Size.Y * Size.Z,
        Size.X * Size.Z,
        Size.X * Size.Z,
        Size.X * Size.Y,
        Size.X * Size.Y};
    for (int32 PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
    {
        const double AreaTolerance = FMath::Max(
            1.0e-6,
            ExpectedAreas[PlaneIndex] * 1.0e-10);
        if (TriangleCounts[PlaneIndex] != 2 ||
            !FMath::IsNearlyEqual(
                TriangleAreas[PlaneIndex],
                ExpectedAreas[PlaneIndex],
                AreaTolerance))
        {
            OutError = FString::Printf(
                TEXT("RF axis-aligned box '%s' must have two complete triangles on every face plane."),
                *Solid.SolidId);
            return false;
        }
    }
    return true;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::ValidateTopologyAndBindings(
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError)
{
    for (int32 SolidIndex = 0; SolidIndex < Solids.Num(); ++SolidIndex)
    {
        const FSolid& Solid = Solids[SolidIndex];
        struct FEdgeUse
        {
            int32 Count = 0;
            int32 DirectionBalance = 0;
            int32 FirstTriangle = INDEX_NONE;
            int32 SecondTriangle = INDEX_NONE;
        };
        TMap<uint64, FEdgeUse> Edges;
        TArray<TArray<int32>> Neighbours;
        Neighbours.SetNum(Solid.TriangleCount);
        // Translate to a solid-local origin before accumulating tetrahedra;
        // Kahan compensation keeps the winding/volume proof stable far from
        // the world origin and rejects any intermediate non-finite result.
        const FVector VolumeOrigin = Vertices[Solid.VertexStart];
        double SignedSixVolume = 0.0;
        double VolumeCompensation = 0.0;
        for (int32 LocalTriangleIndex = 0; LocalTriangleIndex < Solid.TriangleCount; ++LocalTriangleIndex)
        {
            const int32 TriangleIndex = Solid.TriangleStart + LocalTriangleIndex;
            const FTriangle& Triangle = Triangles[TriangleIndex];
            const FVector& A = Vertices[Triangle.VertexIndices[0]];
            const FVector& B = Vertices[Triangle.VertexIndices[1]];
            const FVector& C = Vertices[Triangle.VertexIndices[2]];
            const double VolumeTerm = FVector::DotProduct(
                A - VolumeOrigin,
                FVector::CrossProduct(B - VolumeOrigin, C - VolumeOrigin));
            if (!FMath::IsFinite(VolumeTerm))
            {
                OutError = FString::Printf(
                    TEXT("RF solid '%s' produced a non-finite signed-volume term."),
                    *Solid.SolidId);
                return false;
            }
            const double CorrectedTerm = VolumeTerm - VolumeCompensation;
            const double NextVolume = SignedSixVolume + CorrectedTerm;
            VolumeCompensation =
                (NextVolume - SignedSixVolume) - CorrectedTerm;
            SignedSixVolume = NextVolume;
            for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
            {
                const int32 Start = Triangle.VertexIndices[EdgeIndex];
                const int32 End = Triangle.VertexIndices[(EdgeIndex + 1) % 3];
                FEdgeUse& Edge = Edges.FindOrAdd(EdgeKey(Start, End));
                ++Edge.Count;
                Edge.DirectionBalance += Start < End ? 1 : -1;
                if (Edge.FirstTriangle == INDEX_NONE)
                {
                    Edge.FirstTriangle = LocalTriangleIndex;
                }
                else if (Edge.SecondTriangle == INDEX_NONE)
                {
                    Edge.SecondTriangle = LocalTriangleIndex;
                }
            }
        }
        for (const TPair<uint64, FEdgeUse>& Pair : Edges)
        {
            const FEdgeUse& Edge = Pair.Value;
            if (Edge.Count != 2 || Edge.DirectionBalance != 0 ||
                Edge.FirstTriangle == INDEX_NONE || Edge.SecondTriangle == INDEX_NONE)
            {
                OutError = FString::Printf(TEXT("RF solid '%s' is not a closed consistently oriented two-manifold."), *Solid.SolidId);
                return false;
            }
            Neighbours[Edge.FirstTriangle].Add(Edge.SecondTriangle);
            Neighbours[Edge.SecondTriangle].Add(Edge.FirstTriangle);
        }
        if (!FMath::IsFinite(SignedSixVolume) ||
            SignedSixVolume / 6.0 <= MinimumPositiveVolumeCubicCentimeters)
        {
            OutError = FString::Printf(TEXT("RF solid '%s' is not outward-wound with positive finite volume."), *Solid.SolidId);
            return false;
        }
        TArray<int32> Stack;
        TBitArray<> Visited(false, Solid.TriangleCount);
        Stack.Add(0);
        Visited[0] = true;
        int32 VisitedCount = 0;
        while (!Stack.IsEmpty())
        {
            const int32 Current = Stack.Pop(EAllowShrinking::No);
            ++VisitedCount;
            for (const int32 Neighbour : Neighbours[Current])
            {
                if (!Visited[Neighbour])
                {
                    Visited[Neighbour] = true;
                    Stack.Add(Neighbour);
                }
            }
        }
        if (VisitedCount != Solid.TriangleCount)
        {
            OutError = FString::Printf(TEXT("RF solid '%s' contains disconnected boundary components under one stable ID."), *Solid.SolidId);
            return false;
        }
        if (Solid.bAxisAlignedBoxPrimitive &&
            !ValidateAxisAlignedBoxMesh(Solid, OutError))
        {
            return false;
        }
    }
    return ValidateTriangleSelfIntersections(Limits, OutError) &&
        ValidatePositiveVolumeOverlaps(Limits, OutError);
}

bool FTRIADRFIndexedGeometryQuery::FImpl::ValidateTriangleSelfIntersections(
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError) const
{
    int64 CandidateChecks = 0;
    for (const FSolid& Solid : Solids)
    {
        if (!Solid.bIndexedClosedPolyhedronPrimitive)
        {
            continue;
        }
        TArray<int32> OrderedTriangles;
        OrderedTriangles.Reserve(Solid.TriangleCount);
        for (int32 TriangleOffset = 0;
             TriangleOffset < Solid.TriangleCount;
             ++TriangleOffset)
        {
            OrderedTriangles.Add(Solid.TriangleStart + TriangleOffset);
        }
        OrderedTriangles.Sort([this](int32 First, int32 Second)
        {
            if (Triangles[First].Bounds.Min.X != Triangles[Second].Bounds.Min.X)
            {
                return Triangles[First].Bounds.Min.X < Triangles[Second].Bounds.Min.X;
            }
            return Triangles[First].CanonicalTriangleIndex <
                Triangles[Second].CanonicalTriangleIndex;
        });

        for (int32 OrderedIndex = 0;
             OrderedIndex < OrderedTriangles.Num();
             ++OrderedIndex)
        {
            const int32 FirstIndex = OrderedTriangles[OrderedIndex];
            const FTriangle& FirstTriangle = Triangles[FirstIndex];
            for (int32 OtherOrderedIndex = OrderedIndex + 1;
                 OtherOrderedIndex < OrderedTriangles.Num();
                 ++OtherOrderedIndex)
            {
                const int32 SecondIndex = OrderedTriangles[OtherOrderedIndex];
                const FTriangle& SecondTriangle = Triangles[SecondIndex];
                if (SecondTriangle.Bounds.Min.X >
                    FirstTriangle.Bounds.Max.X +
                        DeclaredGeometryToleranceCentimeters)
                {
                    break;
                }
                if (++CandidateChecks >
                    Limits.MaximumSelfIntersectionCandidateChecks)
                {
                    OutError = TEXT("RF indexed-polyhedron self-intersection validation exceeded its configured deterministic candidate-check ceiling.");
                    return false;
                }
                if (!BoxesOverlapInclusive(
                        FirstTriangle.Bounds,
                        SecondTriangle.Bounds,
                        DeclaredGeometryToleranceCentimeters))
                {
                    continue;
                }
                int32 SharedVertexCount = 0;
                int32 SharedVertexIndex = INDEX_NONE;
                for (const int32 FirstVertex : FirstTriangle.VertexIndices)
                {
                    for (const int32 SecondVertex : SecondTriangle.VertexIndices)
                    {
                        if (FirstVertex == SecondVertex)
                        {
                            ++SharedVertexCount;
                            SharedVertexIndex = FirstVertex;
                        }
                    }
                }
                FVector FirstPoints[3];
                FVector SecondPoints[3];
                for (int32 Corner = 0; Corner < 3; ++Corner)
                {
                    FirstPoints[Corner] =
                        Vertices[FirstTriangle.VertexIndices[Corner]];
                    SecondPoints[Corner] =
                        Vertices[SecondTriangle.VertexIndices[Corner]];
                }
                const FValidationTriangleContact Contact =
                    ClassifyTriangleContact(
                        FirstPoints,
                        SecondPoints,
                        FirstTriangle.OutwardNormal,
                        SecondTriangle.OutwardNormal,
                        DeclaredGeometryToleranceCentimeters);
                if (Contact.Kind == EValidationTriangleContact::None)
                {
                    continue;
                }
                if (SharedVertexCount >= 2 &&
                    Contact.Kind != EValidationTriangleContact::CoplanarArea)
                {
                    continue;
                }
                if (SharedVertexCount == 1 &&
                    Contact.Kind == EValidationTriangleContact::Point &&
                    FVector::DistSquared(
                        Contact.RepresentativePoint,
                        Vertices[SharedVertexIndex]) <=
                        FMath::Square(
                            4.0 * DeclaredGeometryToleranceCentimeters))
                {
                    continue;
                }
                OutError = FString::Printf(
                    TEXT("RF indexed closed polyhedron '%s' has a triangle self-intersection between canonical triangles %d and %d."),
                    *Solid.SolidId,
                    FirstTriangle.CanonicalTriangleIndex,
                    SecondTriangle.CanonicalTriangleIndex);
                return false;
            }
        }
    }
    return true;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::ClassifyPointAgainstSolid(
    const FVector& Point,
    int32 SolidIndex,
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    int64& InOutTriangleChecks,
    EValidationPointSolidRelation& OutRelation,
    FString& OutError) const
{
    OutRelation = EValidationPointSolidRelation::Outside;
    const FSolid& Solid = Solids[SolidIndex];
    const FVector Tolerance(
        DeclaredGeometryToleranceCentimeters,
        DeclaredGeometryToleranceCentimeters,
        DeclaredGeometryToleranceCentimeters);
    if (!IsFiniteVector(Point) ||
        Point.X < Solid.ActualBounds.Min.X - Tolerance.X ||
        Point.X > Solid.ActualBounds.Max.X + Tolerance.X ||
        Point.Y < Solid.ActualBounds.Min.Y - Tolerance.Y ||
        Point.Y > Solid.ActualBounds.Max.Y + Tolerance.Y ||
        Point.Z < Solid.ActualBounds.Min.Z - Tolerance.Z ||
        Point.Z > Solid.ActualBounds.Max.Z + Tolerance.Z)
    {
        return true;
    }

    double SignedSolidAngle = 0.0;
    for (int32 TriangleOffset = 0;
         TriangleOffset < Solid.TriangleCount;
         ++TriangleOffset)
    {
        if (++InOutTriangleChecks > Limits.MaximumContainmentTriangleChecks)
        {
            OutError = TEXT("RF indexed-polyhedron containment validation exceeded its configured deterministic triangle-check ceiling.");
            OutRelation = EValidationPointSolidRelation::Ambiguous;
            return false;
        }
        const FTriangle& Triangle =
            Triangles[Solid.TriangleStart + TriangleOffset];
        const FVector& A = Vertices[Triangle.VertexIndices[0]];
        const FVector& B = Vertices[Triangle.VertexIndices[1]];
        const FVector& C = Vertices[Triangle.VertexIndices[2]];
        const double PlaneDistance = FVector::DotProduct(
            Point - A, Triangle.OutwardNormal);
        if (FMath::Abs(PlaneDistance) <=
            DeclaredGeometryToleranceCentimeters)
        {
            const FVector EdgeZero = B - A;
            const FVector EdgeOne = C - A;
            const FVector PointDelta = Point - A;
            const double DotZeroZero = FVector::DotProduct(EdgeZero, EdgeZero);
            const double DotZeroOne = FVector::DotProduct(EdgeZero, EdgeOne);
            const double DotOneOne = FVector::DotProduct(EdgeOne, EdgeOne);
            const double DotPointZero = FVector::DotProduct(PointDelta, EdgeZero);
            const double DotPointOne = FVector::DotProduct(PointDelta, EdgeOne);
            const double Denominator =
                DotZeroZero * DotOneOne - DotZeroOne * DotZeroOne;
            if (Denominator > MinimumTriangleAreaSquaredCentimeters)
            {
                const double FirstBarycentric =
                    (DotOneOne * DotPointZero -
                     DotZeroOne * DotPointOne) / Denominator;
                const double SecondBarycentric =
                    (DotZeroZero * DotPointOne -
                     DotZeroOne * DotPointZero) / Denominator;
                if (FirstBarycentric >= -BarycentricTolerance &&
                    SecondBarycentric >= -BarycentricTolerance &&
                    FirstBarycentric + SecondBarycentric <=
                        1.0 + BarycentricTolerance)
                {
                    OutRelation = EValidationPointSolidRelation::Boundary;
                    return true;
                }
            }
        }

        const FVector RelativeA = A - Point;
        const FVector RelativeB = B - Point;
        const FVector RelativeC = C - Point;
        const double LengthA = RelativeA.Size();
        const double LengthB = RelativeB.Size();
        const double LengthC = RelativeC.Size();
        if (LengthA <= DeclaredGeometryToleranceCentimeters ||
            LengthB <= DeclaredGeometryToleranceCentimeters ||
            LengthC <= DeclaredGeometryToleranceCentimeters)
        {
            OutRelation = EValidationPointSolidRelation::Boundary;
            return true;
        }
        const double Numerator = FVector::DotProduct(
            RelativeA,
            FVector::CrossProduct(RelativeB, RelativeC));
        const double Denominator =
            LengthA * LengthB * LengthC +
            FVector::DotProduct(RelativeA, RelativeB) * LengthC +
            FVector::DotProduct(RelativeB, RelativeC) * LengthA +
            FVector::DotProduct(RelativeC, RelativeA) * LengthB;
        SignedSolidAngle += 2.0 * FMath::Atan2(Numerator, Denominator);
    }
    const double AbsoluteSolidAngle = FMath::Abs(SignedSolidAngle);
    if (AbsoluteSolidAngle > 2.0 * PI)
    {
        OutRelation = EValidationPointSolidRelation::Inside;
        return true;
    }
    if (AbsoluteSolidAngle < 1.0e-5)
    {
        OutRelation = EValidationPointSolidRelation::Outside;
        return true;
    }
    OutRelation = EValidationPointSolidRelation::Ambiguous;
    OutError = FString::Printf(
        TEXT("RF indexed closed polyhedron '%s' produced an ambiguous winding-number containment result."),
        *Solid.SolidId);
    return false;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::GenericSolidsHavePositiveVolumeOverlap(
    int32 FirstSolidIndex,
    int32 SecondSolidIndex,
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    int64& InOutTriangleCandidateChecks,
    int64& InOutContainmentTriangleChecks,
    bool& bOutOverlap,
    FString& OutError) const
{
    bOutOverlap = false;
    const FSolid& FirstSolid = Solids[FirstSolidIndex];
    const FSolid& SecondSolid = Solids[SecondSolidIndex];
    const double FirstMinimumExtent = FMath::Min(
        FirstSolid.ActualBounds.GetSize().X,
        FMath::Min(
            FirstSolid.ActualBounds.GetSize().Y,
            FirstSolid.ActualBounds.GetSize().Z));
    const double SecondMinimumExtent = FMath::Min(
        SecondSolid.ActualBounds.GetSize().X,
        FMath::Min(
            SecondSolid.ActualBounds.GetSize().Y,
            SecondSolid.ActualBounds.GetSize().Z));
    const double MinimumExtent = FMath::Min(
        FirstMinimumExtent, SecondMinimumExtent);
    const double ProbeDistance = FMath::Min(
        FMath::Max(
            8.0 * DeclaredGeometryToleranceCentimeters,
            MinimumExtent * 1.0e-8),
        MinimumExtent * 1.0e-3);

    auto ProbePoint = [this, &Limits, &InOutContainmentTriangleChecks,
                       &bOutOverlap, &OutError, FirstSolidIndex,
                       SecondSolidIndex](const FVector& Point) -> bool
    {
        EValidationPointSolidRelation FirstRelation;
        EValidationPointSolidRelation SecondRelation;
        if (!ClassifyPointAgainstSolid(
                Point,
                FirstSolidIndex,
                Limits,
                InOutContainmentTriangleChecks,
                FirstRelation,
                OutError) ||
            !ClassifyPointAgainstSolid(
                Point,
                SecondSolidIndex,
                Limits,
                InOutContainmentTriangleChecks,
                SecondRelation,
                OutError))
        {
            return false;
        }
        if (FirstRelation == EValidationPointSolidRelation::Inside &&
            SecondRelation == EValidationPointSolidRelation::Inside)
        {
            bOutOverlap = true;
        }
        return true;
    };

    for (int32 FirstOffset = 0;
         FirstOffset < FirstSolid.TriangleCount;
         ++FirstOffset)
    {
        const FTriangle& FirstTriangle =
            Triangles[FirstSolid.TriangleStart + FirstOffset];
        for (int32 SecondOffset = 0;
             SecondOffset < SecondSolid.TriangleCount;
             ++SecondOffset)
        {
            const FTriangle& SecondTriangle =
                Triangles[SecondSolid.TriangleStart + SecondOffset];
            if (++InOutTriangleCandidateChecks >
                Limits.MaximumCrossSolidTriangleCandidateChecks)
            {
                OutError = TEXT("RF indexed-polyhedron cross-solid validation exceeded its configured deterministic triangle-candidate ceiling.");
                return false;
            }
            if (!BoxesOverlapInclusive(
                    FirstTriangle.Bounds,
                    SecondTriangle.Bounds,
                    DeclaredGeometryToleranceCentimeters))
            {
                continue;
            }
            FVector FirstPoints[3];
            FVector SecondPoints[3];
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                FirstPoints[Corner] =
                    Vertices[FirstTriangle.VertexIndices[Corner]];
                SecondPoints[Corner] =
                    Vertices[SecondTriangle.VertexIndices[Corner]];
            }
            const FValidationTriangleContact Contact =
                ClassifyTriangleContact(
                    FirstPoints,
                    SecondPoints,
                    FirstTriangle.OutwardNormal,
                    SecondTriangle.OutwardNormal,
                    DeclaredGeometryToleranceCentimeters);
            if (Contact.Kind == EValidationTriangleContact::None)
            {
                continue;
            }
            FVector CommonInwardDirection =
                -FirstTriangle.OutwardNormal -
                SecondTriangle.OutwardNormal;
            if (!CommonInwardDirection.Normalize())
            {
                continue;
            }
            for (const double Scale : {1.0, 4.0, 16.0})
            {
                if (!ProbePoint(
                        Contact.RepresentativePoint +
                        CommonInwardDirection * ProbeDistance * Scale))
                {
                    return false;
                }
                if (bOutOverlap)
                {
                    return true;
                }
            }
        }
    }

    auto CheckVertices = [this, &Limits, &InOutContainmentTriangleChecks,
                          &bOutOverlap, &OutError](
        const FSolid& Candidate,
        int32 OtherSolidIndex) -> bool
    {
        for (int32 VertexOffset = 0;
             VertexOffset < Candidate.VertexCount;
             ++VertexOffset)
        {
            EValidationPointSolidRelation Relation;
            if (!ClassifyPointAgainstSolid(
                    Vertices[Candidate.VertexStart + VertexOffset],
                    OtherSolidIndex,
                    Limits,
                    InOutContainmentTriangleChecks,
                    Relation,
                    OutError))
            {
                return false;
            }
            if (Relation == EValidationPointSolidRelation::Inside)
            {
                bOutOverlap = true;
                return true;
            }
        }
        return true;
    };
    if (!CheckVertices(FirstSolid, SecondSolidIndex))
    {
        return false;
    }
    if (bOutOverlap)
    {
        return true;
    }
    if (!CheckVertices(SecondSolid, FirstSolidIndex))
    {
        return false;
    }
    if (bOutOverlap)
    {
        return true;
    }

    auto CheckInwardFaceSamples =
        [this, &Limits, &InOutContainmentTriangleChecks,
         &bOutOverlap, &OutError, ProbeDistance](
            const FSolid& Candidate,
            int32 CandidateSolidIndex,
            int32 OtherSolidIndex) -> bool
    {
        for (int32 TriangleOffset = 0;
             TriangleOffset < Candidate.TriangleCount;
             ++TriangleOffset)
        {
            const FTriangle& Triangle =
                Triangles[Candidate.TriangleStart + TriangleOffset];
            const FVector Sample = Triangle.Centroid -
                Triangle.OutwardNormal * ProbeDistance;
            EValidationPointSolidRelation CandidateRelation;
            EValidationPointSolidRelation OtherRelation;
            if (!ClassifyPointAgainstSolid(
                    Sample,
                    CandidateSolidIndex,
                    Limits,
                    InOutContainmentTriangleChecks,
                    CandidateRelation,
                    OutError) ||
                !ClassifyPointAgainstSolid(
                    Sample,
                    OtherSolidIndex,
                    Limits,
                    InOutContainmentTriangleChecks,
                    OtherRelation,
                    OutError))
            {
                return false;
            }
            if (CandidateRelation == EValidationPointSolidRelation::Inside &&
                OtherRelation == EValidationPointSolidRelation::Inside)
            {
                bOutOverlap = true;
                return true;
            }
        }
        return true;
    };
    if (!CheckInwardFaceSamples(
            FirstSolid, FirstSolidIndex, SecondSolidIndex))
    {
        return false;
    }
    if (bOutOverlap)
    {
        return true;
    }
    if (!CheckInwardFaceSamples(
            SecondSolid, SecondSolidIndex, FirstSolidIndex))
    {
        return false;
    }
    return true;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::ValidatePositiveVolumeOverlaps(
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError) const
{
    TArray<int32> OrderedSolids;
    OrderedSolids.Reserve(Solids.Num());
    for (int32 SolidIndex = 0; SolidIndex < Solids.Num(); ++SolidIndex)
    {
        OrderedSolids.Add(SolidIndex);
    }
    OrderedSolids.Sort([this](int32 First, int32 Second)
    {
        if (Solids[First].ActualBounds.Min.X != Solids[Second].ActualBounds.Min.X)
        {
            return Solids[First].ActualBounds.Min.X < Solids[Second].ActualBounds.Min.X;
        }
        return Solids[First].SolidId < Solids[Second].SolidId;
    });
    int64 PairChecks = 0;
    int64 TriangleCandidateChecks = 0;
    int64 ContainmentTriangleChecks = 0;
    for (int32 OrderedIndex = 0; OrderedIndex < OrderedSolids.Num(); ++OrderedIndex)
    {
        const FSolid& First = Solids[OrderedSolids[OrderedIndex]];
        for (int32 OtherOrderedIndex = OrderedIndex + 1;
             OtherOrderedIndex < OrderedSolids.Num();
             ++OtherOrderedIndex)
        {
            const FSolid& Second = Solids[OrderedSolids[OtherOrderedIndex]];
            if (Second.ActualBounds.Min.X >= First.ActualBounds.Max.X - DeclaredGeometryToleranceCentimeters)
            {
                break;
            }
            if (++PairChecks > HardMaximumOverlapPairChecks)
            {
                OutError = TEXT("RF solid overlap validation exceeded its compiled pair-check ceiling.");
                return false;
            }
            const FVector Overlap(
                FMath::Min(First.ActualBounds.Max.X, Second.ActualBounds.Max.X) - FMath::Max(First.ActualBounds.Min.X, Second.ActualBounds.Min.X),
                FMath::Min(First.ActualBounds.Max.Y, Second.ActualBounds.Max.Y) - FMath::Max(First.ActualBounds.Min.Y, Second.ActualBounds.Min.Y),
                FMath::Min(First.ActualBounds.Max.Z, Second.ActualBounds.Max.Z) - FMath::Max(First.ActualBounds.Min.Z, Second.ActualBounds.Min.Z));
            if (Overlap.X <= DeclaredGeometryToleranceCentimeters ||
                Overlap.Y <= DeclaredGeometryToleranceCentimeters ||
                Overlap.Z <= DeclaredGeometryToleranceCentimeters)
            {
                continue;
            }
            if (First.bAxisAlignedBoxPrimitive && Second.bAxisAlignedBoxPrimitive &&
                Overlap.X > DeclaredGeometryToleranceCentimeters &&
                Overlap.Y > DeclaredGeometryToleranceCentimeters &&
                Overlap.Z > DeclaredGeometryToleranceCentimeters)
            {
                OutError = FString::Printf(
                    TEXT("Axis-aligned RF solids '%s' and '%s' have a forbidden positive-volume overlap."),
                    *First.SolidId, *Second.SolidId);
                return false;
            }
            if (!First.bAxisAlignedBoxPrimitive ||
                !Second.bAxisAlignedBoxPrimitive)
            {
                bool bPositiveVolumeOverlap = false;
                const int32 FirstSolidIndex = OrderedSolids[OrderedIndex];
                const int32 SecondSolidIndex = OrderedSolids[OtherOrderedIndex];
                if (!GenericSolidsHavePositiveVolumeOverlap(
                        FirstSolidIndex,
                        SecondSolidIndex,
                        Limits,
                        TriangleCandidateChecks,
                        ContainmentTriangleChecks,
                        bPositiveVolumeOverlap,
                        OutError))
                {
                    return false;
                }
                if (bPositiveVolumeOverlap)
                {
                    OutError = FString::Printf(
                        TEXT("RF solids '%s' and '%s' have a forbidden positive-volume overlap or containment involving an indexed closed polyhedron."),
                        *First.SolidId,
                        *Second.SolidId);
                    return false;
                }
            }
        }
    }
    return true;
}

int32 FTRIADRFIndexedGeometryQuery::FImpl::BuildBVHNode(
    int32 FirstPermutationIndex,
    int32 TriangleCount,
    int32 Depth,
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError)
{
    if (Depth > Limits.MaximumBVHDepth)
    {
        OutError = TEXT("Deterministic RF BVH exceeds the configured depth bound.");
        return INDEX_NONE;
    }
    FBVHNode Node;
    Node.FirstPermutationIndex = FirstPermutationIndex;
    Node.TriangleCount = TriangleCount;
    FBox CentroidBounds(ForceInit);
    for (int32 Offset = 0; Offset < TriangleCount; ++Offset)
    {
        const FTriangle& Triangle = Triangles[TrianglePermutation[FirstPermutationIndex + Offset]];
        Node.Bounds += Triangle.Bounds;
        CentroidBounds += Triangle.Centroid;
    }
    const int32 NodeIndex = BVHNodes.Add(Node);
    if (TriangleCount <= Limits.MaximumTrianglesPerLeaf)
    {
        return NodeIndex;
    }
    const FVector Extent = CentroidBounds.GetSize();
    int32 Axis = 0;
    if (Extent.Y > Extent.X)
    {
        Axis = 1;
    }
    if (Extent.Z > Extent[Axis])
    {
        Axis = 2;
    }
    TArray<int32> SortedRange;
    SortedRange.Reserve(TriangleCount);
    for (int32 Offset = 0; Offset < TriangleCount; ++Offset)
    {
        SortedRange.Add(TrianglePermutation[FirstPermutationIndex + Offset]);
    }
    SortedRange.Sort([this, Axis](int32 First, int32 Second)
    {
        const double FirstCoordinate = Triangles[First].Centroid[Axis];
        const double SecondCoordinate = Triangles[Second].Centroid[Axis];
        if (FirstCoordinate != SecondCoordinate)
        {
            return FirstCoordinate < SecondCoordinate;
        }
        return Triangles[First].CanonicalTriangleIndex < Triangles[Second].CanonicalTriangleIndex;
    });
    for (int32 Offset = 0; Offset < TriangleCount; ++Offset)
    {
        TrianglePermutation[FirstPermutationIndex + Offset] = SortedRange[Offset];
    }
    const int32 LeftCount = TriangleCount / 2;
    const int32 RightCount = TriangleCount - LeftCount;
    const int32 Left = BuildBVHNode(FirstPermutationIndex, LeftCount, Depth + 1, Limits, OutError);
    if (Left == INDEX_NONE)
    {
        return INDEX_NONE;
    }
    const int32 Right = BuildBVHNode(FirstPermutationIndex + LeftCount, RightCount, Depth + 1, Limits, OutError);
    if (Right == INDEX_NONE)
    {
        return INDEX_NONE;
    }
    BVHNodes[NodeIndex].LeftChild = Left;
    BVHNodes[NodeIndex].RightChild = Right;
    BVHNodes[NodeIndex].TriangleCount = 0;
    return NodeIndex;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::Parse(
    const FString& GeometryJson,
    const FString& MaterialCatalogJson,
    const FTRIADRFIndexedGeometryLoadLimits& Limits,
    FString& OutError)
{
    bReady = false;
    if (GeometryJson.IsEmpty() || MaterialCatalogJson.IsEmpty() ||
        FTCHARToUTF8(*GeometryJson).Length() > Limits.MaximumJsonBytesPerDocument ||
        FTCHARToUTF8(*MaterialCatalogJson).Length() > Limits.MaximumJsonBytesPerDocument)
    {
        OutError = TEXT("RF JSON text is empty or exceeds the configured byte bound.");
        return false;
    }
    FString ActualGeometrySha256;
    FString ActualCatalogSha256;
    if (!ComputeUtf8Sha256(GeometryJson, ActualGeometrySha256, OutError) ||
        !ComputeUtf8Sha256(MaterialCatalogJson, ActualCatalogSha256, OutError))
    {
        return false;
    }
    Metadata.GeometrySha256 = ActualGeometrySha256.ToLower();
    TSharedPtr<FJsonObject> CatalogRoot;
    TSharedPtr<FJsonObject> GeometryRoot;
    if (!ParseRoot(MaterialCatalogJson, CatalogRoot, OutError) ||
        !ParseMaterialCatalog(CatalogRoot, Limits, OutError) ||
        !ParseRoot(GeometryJson, GeometryRoot, OutError) ||
        !ParseGeometry(GeometryRoot, ActualCatalogSha256, Limits, OutError) ||
        !ValidateTopologyAndBindings(Limits, OutError))
    {
        return false;
    }
    TrianglePermutation.Reserve(Triangles.Num());
    for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
    {
        TrianglePermutation.Add(TriangleIndex);
    }
    if (BuildBVHNode(0, Triangles.Num(), 0, Limits, OutError) == INDEX_NONE)
    {
        BVHNodes.Reset();
        TrianglePermutation.Reset();
        return false;
    }
    bReady = true;
    OutError.Reset();
    return true;
}

FTRIADRFIndexedGeometryQuery::FImpl::ETriangleContact
FTRIADRFIndexedGeometryQuery::FImpl::IntersectTriangle(
    int32 TriangleIndex,
    const FVector& Start,
    const FVector& End,
    double ParameterTolerance,
    double PositionToleranceCentimeters,
    FRawHit& OutHit) const
{
    const FTriangle& Triangle = Triangles[TriangleIndex];
    const FVector& A = Vertices[Triangle.VertexIndices[0]];
    const FVector& B = Vertices[Triangle.VertexIndices[1]];
    const FVector& C = Vertices[Triangle.VertexIndices[2]];
    const FVector Direction = End - Start;
    const FVector EdgeOne = B - A;
    const FVector EdgeTwo = C - A;
    const FVector P = FVector::CrossProduct(Direction, EdgeTwo);
    const double Determinant = FVector::DotProduct(EdgeOne, P);
    if (FMath::Abs(Determinant) <= DirectionParallelEpsilon)
    {
        const double StartPlaneDistance = FVector::DotProduct(Start - A, Triangle.OutwardNormal);
        const double EndPlaneDistance = FVector::DotProduct(End - A, Triangle.OutwardNormal);
        if (FMath::Abs(StartPlaneDistance) <= PositionToleranceCentimeters &&
            FMath::Abs(EndPlaneDistance) <= PositionToleranceCentimeters &&
            IntersectFiniteSegmentAabb(Start, End, Triangle.Bounds, PositionToleranceCentimeters))
        {
            return ETriangleContact::CoplanarAmbiguous;
        }
        return ETriangleContact::None;
    }
    const double InverseDeterminant = 1.0 / Determinant;
    const FVector T = Start - A;
    const double U = FVector::DotProduct(T, P) * InverseDeterminant;
    const FVector Q = FVector::CrossProduct(T, EdgeOne);
    const double V = FVector::DotProduct(Direction, Q) * InverseDeterminant;
    const double Parameter = FVector::DotProduct(EdgeTwo, Q) * InverseDeterminant;
    if (U < -BarycentricTolerance || V < -BarycentricTolerance ||
        U + V > 1.0 + BarycentricTolerance ||
        Parameter < -ParameterTolerance || Parameter > 1.0 + ParameterTolerance)
    {
        return ETriangleContact::None;
    }
    if (Parameter <= ParameterTolerance || Parameter >= 1.0 - ParameterTolerance)
    {
        return ETriangleContact::EndpointAmbiguous;
    }
    const FVector UnitDirection = Direction.GetSafeNormal();
    const double DirectionDotNormal = FVector::DotProduct(UnitDirection, Triangle.OutwardNormal);
    if (FMath::Abs(DirectionDotNormal) <= IncidenceTangentEpsilon)
    {
        return ETriangleContact::CoplanarAmbiguous;
    }
    OutHit.Parameter = Parameter;
    OutHit.TriangleIndex = TriangleIndex;
    OutHit.bOnTriangleEdge =
        U <= BarycentricTolerance || V <= BarycentricTolerance ||
        1.0 - U - V <= BarycentricTolerance;
    OutHit.Crossing = DirectionDotNormal < 0.0
        ? ETRIADRFBoundaryCrossing::Entry
        : ETRIADRFBoundaryCrossing::Exit;
    return ETriangleContact::Hit;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::IsFiniteSegmentWithinModeledCoverage(
    const FVector& Start,
    const FVector& End,
    FString& OutError) const
{
    OutError.Reset();
    if (!bReady || !ModeledCoverageBounds.IsValid)
    {
        OutError = TEXT("RF indexed geometry query is not loaded with a valid modeled coverage envelope.");
        return false;
    }
    if (!IsFiniteVector(Start) || !IsFiniteVector(End))
    {
        OutError = TEXT("RF modeled-coverage query endpoints must be finite.");
        return false;
    }
    const bool bStartInside = IsPointInsideOrOnClosedBox(Start, ModeledCoverageBounds);
    const bool bEndInside = IsPointInsideOrOnClosedBox(End, ModeledCoverageBounds);
    if (!bStartInside || !bEndInside)
    {
        OutError = FString::Printf(
            TEXT("RF finite segment lies outside modeled coverage envelope '%s' (%s); outside-domain queries are rejected and never interpreted as clear/direct paths."),
            *Metadata.ModeledCoverageId,
            *Metadata.ModeledCoverageScope);
        return false;
    }
    // A closed axis-aligned box is convex. Exact containment of both endpoints
    // therefore proves containment of every point on the complete segment.
    return true;
}

bool FTRIADRFIndexedGeometryQuery::FImpl::Trace(
    const FVector& Start,
    const FVector& End,
    double PositionToleranceCentimeters,
    TArray<FTRIADRFIndexedSegmentHit>& OutHits,
    FString& OutError) const
{
    OutHits.Reset();
    OutError.Reset();
    if (!bReady || BVHNodes.IsEmpty())
    {
        OutError = TEXT("RF indexed geometry query is not loaded and ready.");
        return false;
    }
    if (!IsFiniteVector(Start) || !IsFiniteVector(End) ||
        !FMath::IsFinite(PositionToleranceCentimeters) ||
        PositionToleranceCentimeters <= 0.0 || PositionToleranceCentimeters > 1.0)
    {
        OutError = TEXT("RF finite segment and position tolerance must be finite and bounded.");
        return false;
    }
    if (!IsFiniteSegmentWithinModeledCoverage(Start, End, OutError))
    {
        return false;
    }
    const double SegmentLength = FVector::Distance(Start, End);
    if (!FMath::IsFinite(SegmentLength) || SegmentLength <= PositionToleranceCentimeters)
    {
        OutError = TEXT("RF finite segment is zero-length or shorter than its geometry tolerance.");
        return false;
    }
    const double ParameterTolerance = PositionToleranceCentimeters / SegmentLength;
    TArray<FRawHit> RawHits;
    TArray<int32> NodeStack;
    NodeStack.Add(0);
    while (!NodeStack.IsEmpty())
    {
        const int32 NodeIndex = NodeStack.Pop(EAllowShrinking::No);
        const FBVHNode& Node = BVHNodes[NodeIndex];
        if (!IntersectFiniteSegmentAabb(Start, End, Node.Bounds, PositionToleranceCentimeters))
        {
            continue;
        }
        if (!Node.IsLeaf())
        {
            // Push right first so the deterministic left child is visited first.
            NodeStack.Add(Node.RightChild);
            NodeStack.Add(Node.LeftChild);
            continue;
        }
        for (int32 Offset = 0; Offset < Node.TriangleCount; ++Offset)
        {
            FRawHit Hit;
            const int32 TriangleIndex = TrianglePermutation[Node.FirstPermutationIndex + Offset];
            const ETriangleContact Contact = IntersectTriangle(
                TriangleIndex,
                Start,
                End,
                ParameterTolerance,
                PositionToleranceCentimeters,
                Hit);
            if (Contact == ETriangleContact::CoplanarAmbiguous)
            {
                OutError = FString::Printf(
                    TEXT("RF segment has tangent/coplanar ambiguous contact with canonical triangle %d."),
                    Triangles[TriangleIndex].CanonicalTriangleIndex);
                return false;
            }
            if (Contact == ETriangleContact::EndpointAmbiguous)
            {
                OutError = FString::Printf(
                    TEXT("RF segment endpoint lies on canonical triangle %d; endpoint containment is intentionally unsupported."),
                    Triangles[TriangleIndex].CanonicalTriangleIndex);
                return false;
            }
            if (Contact == ETriangleContact::Hit)
            {
                RawHits.Add(Hit);
            }
        }
    }
    RawHits.Sort([this](const FRawHit& First, const FRawHit& Second)
    {
        if (First.Parameter != Second.Parameter)
        {
            return First.Parameter < Second.Parameter;
        }
        const FTriangle& FirstTriangle = Triangles[First.TriangleIndex];
        const FTriangle& SecondTriangle = Triangles[Second.TriangleIndex];
        const FString& FirstSolid = Solids[FirstTriangle.SolidIndex].SolidId;
        const FString& SecondSolid = Solids[SecondTriangle.SolidIndex].SolidId;
        if (FirstSolid != SecondSolid)
        {
            return FirstSolid < SecondSolid;
        }
        return FirstTriangle.CanonicalTriangleIndex < SecondTriangle.CanonicalTriangleIndex;
    });

    for (int32 RawIndex = 0; RawIndex < RawHits.Num();)
    {
        const FRawHit& FirstRawHit = RawHits[RawIndex];
        const FTriangle& FirstTriangle = Triangles[FirstRawHit.TriangleIndex];
        int32 EndIndex = RawIndex + 1;
        while (EndIndex < RawHits.Num())
        {
            const FRawHit& OtherRawHit = RawHits[EndIndex];
            const FTriangle& OtherTriangle = Triangles[OtherRawHit.TriangleIndex];
            if (OtherTriangle.SolidIndex != FirstTriangle.SolidIndex ||
                FMath::Abs(OtherRawHit.Parameter - FirstRawHit.Parameter) > ParameterTolerance)
            {
                break;
            }
            ++EndIndex;
        }
        for (int32 DuplicateIndex = RawIndex + 1; DuplicateIndex < EndIndex; ++DuplicateIndex)
        {
            const FRawHit& Duplicate = RawHits[DuplicateIndex];
            const FTriangle& DuplicateTriangle = Triangles[Duplicate.TriangleIndex];
            if (Duplicate.Crossing != FirstRawHit.Crossing ||
                FVector::DotProduct(DuplicateTriangle.OutwardNormal, FirstTriangle.OutwardNormal) < NormalAgreementCosine)
            {
                OutError = FString::Printf(
                    TEXT("RF segment hits a sharp edge/corner ambiguously in solid '%s'."),
                    *Solids[FirstTriangle.SolidIndex].SolidId);
                return false;
            }
        }
        if (FirstRawHit.bOnTriangleEdge && EndIndex - RawIndex == 1)
        {
            OutError = FString::Printf(
                TEXT("RF segment has an unpaired triangle-edge contact in solid '%s'."),
                *Solids[FirstTriangle.SolidIndex].SolidId);
            return false;
        }
        const FSurface& Surface = Surfaces[FirstTriangle.SurfaceIndex];
        FTRIADRFIndexedSegmentHit Hit;
        Hit.SegmentParameter = FirstRawHit.Parameter;
        Hit.DistanceCentimeters = SegmentLength * FirstRawHit.Parameter;
        Hit.PointCentimeters = Start + (End - Start) * FirstRawHit.Parameter;
        Hit.OutwardNormal = FirstTriangle.OutwardNormal;
        Hit.Crossing = FirstRawHit.Crossing;
        Hit.CanonicalTriangleIndex = FirstTriangle.CanonicalTriangleIndex;
        Hit.TriangleId = FString::Printf(
            TEXT("%s:triangle:%09d"),
            *Metadata.GeometryRevision,
            FirstTriangle.CanonicalTriangleIndex);
        Hit.SolidId = Solids[FirstTriangle.SolidIndex].SolidId;
        Hit.SurfaceId = Surface.SurfaceId;
        Hit.MaterialId = Surface.MaterialId;
        Hit.SolidRole = Solids[FirstTriangle.SolidIndex].Role;
        Hit.BoundaryRole = Surface.BoundaryRole;
        Hit.SourceClass = Surface.SourceClass;
        Hit.UncertaintyClass = Surface.UncertaintyClass;
        Hit.ApertureId = Surface.ApertureId;
        Hit.ApertureState = Surface.ApertureState;
        OutHits.Add(MoveTemp(Hit));
        RawIndex = EndIndex;
    }

    TMap<FString, ETRIADRFBoundaryCrossing> ExpectedCrossing;
    TMap<FString, int32> CrossingCount;
    for (const FTRIADRFIndexedSegmentHit& Hit : OutHits)
    {
        ETRIADRFBoundaryCrossing* Expected = ExpectedCrossing.Find(Hit.SolidId);
        if (Expected == nullptr)
        {
            if (Hit.Crossing != ETRIADRFBoundaryCrossing::Entry)
            {
                OutError = FString::Printf(TEXT("RF segment starts inside solid '%s'; endpoint containment is intentionally unsupported."), *Hit.SolidId);
                OutHits.Reset();
                return false;
            }
            ExpectedCrossing.Add(Hit.SolidId, ETRIADRFBoundaryCrossing::Exit);
            CrossingCount.Add(Hit.SolidId, 1);
        }
        else
        {
            if (Hit.Crossing != *Expected)
            {
                OutError = FString::Printf(TEXT("RF solid '%s' has non-alternating oriented crossings."), *Hit.SolidId);
                OutHits.Reset();
                return false;
            }
            *Expected = Hit.Crossing == ETRIADRFBoundaryCrossing::Entry
                ? ETRIADRFBoundaryCrossing::Exit
                : ETRIADRFBoundaryCrossing::Entry;
            ++CrossingCount.FindChecked(Hit.SolidId);
        }
    }
    for (const TPair<FString, int32>& Pair : CrossingCount)
    {
        if ((Pair.Value % 2) != 0)
        {
            OutError = FString::Printf(TEXT("RF segment ends inside solid '%s'; endpoint containment is intentionally unsupported."), *Pair.Key);
            OutHits.Reset();
            return false;
        }
    }
    return true;
}

const FTRIADRFIndexedGeometryQuery::FImpl::FRuntimeProfile*
FTRIADRFIndexedGeometryQuery::FImpl::SelectProfile(
    int32 MaterialIndex,
    double FrequencyGHz,
    FString& OutError) const
{
    const FMaterial& Material = Materials[MaterialIndex];
    const FRuntimeProfile* Selected = nullptr;
    for (const FRuntimeProfile& Profile : Material.Profiles)
    {
        if (FrequencyGHz >= Profile.MinimumFrequencyGHz &&
            FrequencyGHz <= Profile.MaximumFrequencyGHz)
        {
            if (Selected != nullptr)
            {
                OutError = FString::Printf(
                    TEXT("RF material '%s' has ambiguous runtime profiles at %.9f GHz."),
                    *Material.MaterialId,
                    FrequencyGHz);
                return nullptr;
            }
            Selected = &Profile;
        }
    }
    if (Selected == nullptr)
    {
        OutError = FString::Printf(
            TEXT("RF material '%s' has no runtime profile at %.9f GHz."),
            *Material.MaterialId,
            FrequencyGHz);
    }
    return Selected;
}

FTRIADRFIndexedGeometryQuery::FTRIADRFIndexedGeometryQuery(
    const FTRIADRFIndexedGeometryLoadLimits& InLoadLimits)
    : Impl(MakeUnique<FImpl>())
    , LoadLimits(InLoadLimits)
{
}

FTRIADRFIndexedGeometryQuery::~FTRIADRFIndexedGeometryQuery() = default;

void FTRIADRFIndexedGeometryQuery::Reset()
{
    Impl = MakeUnique<FImpl>();
}

bool FTRIADRFIndexedGeometryQuery::LoadFromJsonFiles(
    const FString& GeometryJsonPath,
    const FString& MaterialCatalogJsonPath,
    FString& OutError)
{
    return LoadFromJsonFiles(
        GeometryJsonPath,
        MaterialCatalogJsonPath,
        FString(),
        FString(),
        OutError);
}

bool FTRIADRFIndexedGeometryQuery::LoadFromJsonFiles(
    const FString& GeometryJsonPath,
    const FString& MaterialCatalogJsonPath,
    const FString& ExpectedGeometrySha256,
    const FString& ExpectedMaterialCatalogSha256,
    FString& OutError)
{
    Reset();
    OutError.Reset();
    const bool bValidLimits =
        LoadLimits.MaximumJsonBytesPerDocument > 0 &&
        LoadLimits.MaximumJsonBytesPerDocument <= HardMaximumJsonBytesPerDocument &&
        LoadLimits.MaximumVertices > 0 && LoadLimits.MaximumVertices <= HardMaximumVertices &&
        LoadLimits.MaximumTriangles > 0 && LoadLimits.MaximumTriangles <= HardMaximumTriangles &&
        LoadLimits.MaximumSolids > 0 && LoadLimits.MaximumSolids <= HardMaximumSolids &&
        LoadLimits.MaximumSurfaces > 0 && LoadLimits.MaximumSurfaces <= HardMaximumSurfaces &&
        LoadLimits.MaximumWitnesses > 0 && LoadLimits.MaximumWitnesses <= HardMaximumWitnesses &&
        LoadLimits.MaximumMaterials > 0 && LoadLimits.MaximumMaterials <= HardMaximumMaterials &&
        LoadLimits.MaximumProfilesPerMaterial > 0 && LoadLimits.MaximumProfilesPerMaterial <= HardMaximumProfilesPerMaterial &&
        LoadLimits.MaximumBVHDepth > 0 && LoadLimits.MaximumBVHDepth <= HardMaximumBVHDepth &&
        LoadLimits.MaximumTrianglesPerLeaf > 0 && LoadLimits.MaximumTrianglesPerLeaf <= HardMaximumTrianglesPerLeaf &&
        LoadLimits.MaximumSelfIntersectionCandidateChecks > 0 && LoadLimits.MaximumSelfIntersectionCandidateChecks <= HardMaximumSelfIntersectionCandidateChecks &&
        LoadLimits.MaximumCrossSolidTriangleCandidateChecks > 0 && LoadLimits.MaximumCrossSolidTriangleCandidateChecks <= HardMaximumCrossSolidTriangleCandidateChecks &&
        LoadLimits.MaximumContainmentTriangleChecks > 0 && LoadLimits.MaximumContainmentTriangleChecks <= HardMaximumContainmentTriangleChecks;
    if (!bValidLimits)
    {
        OutError = TEXT("RF indexed-geometry load limits are invalid or exceed compiled ceilings.");
        return false;
    }
    const bool bHasExpectedGeometrySha256 = !ExpectedGeometrySha256.IsEmpty();
    const bool bHasExpectedCatalogSha256 = !ExpectedMaterialCatalogSha256.IsEmpty();
    if (bHasExpectedGeometrySha256 != bHasExpectedCatalogSha256 ||
        (bHasExpectedGeometrySha256 &&
            (!IsSha256(ExpectedGeometrySha256) ||
                !IsSha256(ExpectedMaterialCatalogSha256))))
    {
        OutError = TEXT("Expected RF geometry and material-catalog SHA-256 values must be supplied together as complete 64-digit hex strings.");
        return false;
    }
    FString GeometryJson;
    FString MaterialCatalogJson;
    FString GeometryBufferSha256;
    FString CatalogBufferSha256;
    if (!LoadCanonicalUtf8File(
            GeometryJsonPath,
            LoadLimits.MaximumJsonBytesPerDocument,
            GeometryJson,
            GeometryBufferSha256,
            OutError) ||
        !LoadCanonicalUtf8File(
            MaterialCatalogJsonPath,
            LoadLimits.MaximumJsonBytesPerDocument,
            MaterialCatalogJson,
            CatalogBufferSha256,
            OutError))
    {
        return false;
    }
    if (bHasExpectedGeometrySha256 &&
        !GeometryBufferSha256.Equals(ExpectedGeometrySha256, ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(
            TEXT("Dedicated RF geometry SHA-256 mismatch: expected %s, got %s."),
            *ExpectedGeometrySha256,
            *GeometryBufferSha256);
        return false;
    }
    if (bHasExpectedCatalogSha256 &&
        !CatalogBufferSha256.Equals(ExpectedMaterialCatalogSha256, ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(
            TEXT("Dedicated RF material catalog SHA-256 mismatch: expected %s, got %s."),
            *ExpectedMaterialCatalogSha256,
            *CatalogBufferSha256);
        return false;
    }
    TUniquePtr<FImpl> Candidate = MakeUnique<FImpl>();
    if (!Candidate->Parse(GeometryJson, MaterialCatalogJson, LoadLimits, OutError))
    {
        return false;
    }
    if (!Candidate->Metadata.GeometrySha256.Equals(
            GeometryBufferSha256,
            ESearchCase::CaseSensitive) ||
        !Candidate->Metadata.MaterialCatalogSha256.Equals(
            CatalogBufferSha256,
            ESearchCase::CaseSensitive))
    {
        OutError = TEXT("RF single-read buffer hashes differ from the exact buffers parsed by the transactional loader.");
        return false;
    }
    Impl = MoveTemp(Candidate);
    return true;
}

bool FTRIADRFIndexedGeometryQuery::LoadFromJsonStrings(
    const FString& GeometryJson,
    const FString& MaterialCatalogJson,
    FString& OutError)
{
    Reset();
    const bool bValidLimits =
        LoadLimits.MaximumJsonBytesPerDocument > 0 &&
        LoadLimits.MaximumJsonBytesPerDocument <= HardMaximumJsonBytesPerDocument &&
        LoadLimits.MaximumVertices > 0 && LoadLimits.MaximumVertices <= HardMaximumVertices &&
        LoadLimits.MaximumTriangles > 0 && LoadLimits.MaximumTriangles <= HardMaximumTriangles &&
        LoadLimits.MaximumSolids > 0 && LoadLimits.MaximumSolids <= HardMaximumSolids &&
        LoadLimits.MaximumSurfaces > 0 && LoadLimits.MaximumSurfaces <= HardMaximumSurfaces &&
        LoadLimits.MaximumWitnesses > 0 && LoadLimits.MaximumWitnesses <= HardMaximumWitnesses &&
        LoadLimits.MaximumMaterials > 0 && LoadLimits.MaximumMaterials <= HardMaximumMaterials &&
        LoadLimits.MaximumProfilesPerMaterial > 0 && LoadLimits.MaximumProfilesPerMaterial <= HardMaximumProfilesPerMaterial &&
        LoadLimits.MaximumBVHDepth > 0 && LoadLimits.MaximumBVHDepth <= HardMaximumBVHDepth &&
        LoadLimits.MaximumTrianglesPerLeaf > 0 && LoadLimits.MaximumTrianglesPerLeaf <= HardMaximumTrianglesPerLeaf &&
        LoadLimits.MaximumSelfIntersectionCandidateChecks > 0 && LoadLimits.MaximumSelfIntersectionCandidateChecks <= HardMaximumSelfIntersectionCandidateChecks &&
        LoadLimits.MaximumCrossSolidTriangleCandidateChecks > 0 && LoadLimits.MaximumCrossSolidTriangleCandidateChecks <= HardMaximumCrossSolidTriangleCandidateChecks &&
        LoadLimits.MaximumContainmentTriangleChecks > 0 && LoadLimits.MaximumContainmentTriangleChecks <= HardMaximumContainmentTriangleChecks;
    if (!bValidLimits)
    {
        OutError = TEXT("RF indexed-geometry load limits are invalid or exceed compiled ceilings.");
        return false;
    }
    TUniquePtr<FImpl> Candidate = MakeUnique<FImpl>();
    if (!Candidate->Parse(GeometryJson, MaterialCatalogJson, LoadLimits, OutError))
    {
        return false;
    }
    Impl = MoveTemp(Candidate);
    return true;
}

bool FTRIADRFIndexedGeometryQuery::ComputeCanonicalJsonSha256(
    const FString& JsonText,
    FString& OutSha256,
    FString& OutError)
{
    OutSha256.Reset();
    OutError.Reset();
    return ComputeUtf8Sha256(JsonText, OutSha256, OutError);
}

bool FTRIADRFIndexedGeometryQuery::IsReady() const
{
    return Impl.IsValid() && Impl->bReady;
}

int32 FTRIADRFIndexedGeometryQuery::GetVertexCount() const
{
    return IsReady() ? Impl->Vertices.Num() : 0;
}

int32 FTRIADRFIndexedGeometryQuery::GetTriangleCount() const
{
    return IsReady() ? Impl->Triangles.Num() : 0;
}

int32 FTRIADRFIndexedGeometryQuery::GetSolidCount() const
{
    return IsReady() ? Impl->Solids.Num() : 0;
}

int32 FTRIADRFIndexedGeometryQuery::GetSurfaceCount() const
{
    return IsReady() ? Impl->Surfaces.Num() : 0;
}

int32 FTRIADRFIndexedGeometryQuery::GetMaterialCount() const
{
    return IsReady() ? Impl->Materials.Num() : 0;
}

int32 FTRIADRFIndexedGeometryQuery::GetBVHNodeCount() const
{
    return IsReady() ? Impl->BVHNodes.Num() : 0;
}

const FTRIADRFIndexedGeometryMetadata& FTRIADRFIndexedGeometryQuery::GetMetadata() const
{
    return Impl->Metadata;
}

bool FTRIADRFIndexedGeometryQuery::IsFiniteSegmentWithinModeledCoverage(
    const FVector& StartCentimeters,
    const FVector& EndCentimeters,
    FString& OutError) const
{
    return Impl->IsFiniteSegmentWithinModeledCoverage(
        StartCentimeters,
        EndCentimeters,
        OutError);
}

bool FTRIADRFIndexedGeometryQuery::IsFiniteSegmentWithinAdmittedStudyDomain(
    const FVector& StartCentimeters,
    const FVector& EndCentimeters,
    const FVector& StartLongitudeLatitudeHeight,
    const FVector& EndLongitudeLatitudeHeight,
    double& OutStartSignedDistanceMeters,
    double& OutEndSignedDistanceMeters,
    FString& OutError) const
{
    OutStartSignedDistanceMeters = std::numeric_limits<double>::quiet_NaN();
    OutEndSignedDistanceMeters = std::numeric_limits<double>::quiet_NaN();
    if (!IsFiniteSegmentWithinModeledCoverage(StartCentimeters, EndCentimeters, OutError))
    {
        return false;
    }
    const FTRIADRFIndexedGeometryMetadata& Loaded = GetMetadata();
    if (!Loaded.bHasClosedWgs84GeodesicCircleStudyDomain)
    {
        return true;
    }
    if (!FMath::IsFinite(StartLongitudeLatitudeHeight.X) ||
        !FMath::IsFinite(StartLongitudeLatitudeHeight.Y) ||
        !FMath::IsFinite(StartLongitudeLatitudeHeight.Z) ||
        !FMath::IsFinite(EndLongitudeLatitudeHeight.X) ||
        !FMath::IsFinite(EndLongitudeLatitudeHeight.Y) ||
        !FMath::IsFinite(EndLongitudeLatitudeHeight.Z))
    {
        OutError = TEXT("RF AOI admission requires finite UE-to-WGS84 LLH endpoint transforms; no clear/direct or legacy fallback is allowed.");
        return false;
    }
    const double StartDistance = TRIAD::Geodesy::Wgs84DistanceMeters(
        Loaded.StudyDomainCenterWgs84Degrees.X,
        Loaded.StudyDomainCenterWgs84Degrees.Y,
        StartLongitudeLatitudeHeight.X,
        StartLongitudeLatitudeHeight.Y);
    const double EndDistance = TRIAD::Geodesy::Wgs84DistanceMeters(
        Loaded.StudyDomainCenterWgs84Degrees.X,
        Loaded.StudyDomainCenterWgs84Degrees.Y,
        EndLongitudeLatitudeHeight.X,
        EndLongitudeLatitudeHeight.Y);
    OutStartSignedDistanceMeters = StartDistance - Loaded.StudyDomainRadiusMeters;
    OutEndSignedDistanceMeters = EndDistance - Loaded.StudyDomainRadiusMeters;
    if (!FMath::IsFinite(OutStartSignedDistanceMeters) ||
        !FMath::IsFinite(OutEndSignedDistanceMeters) ||
        OutStartSignedDistanceMeters > 0.0 ||
        OutEndSignedDistanceMeters > 0.0)
    {
        OutError = FString::Printf(
            TEXT("RF endpoints are outside closed WGS84 study domain '%s' (txSignedDistanceMeters=%.9f rxSignedDistanceMeters=%.9f); no clear/direct or legacy fallback is allowed."),
            *Loaded.StudyDomainId,
            OutStartSignedDistanceMeters,
            OutEndSignedDistanceMeters);
        return false;
    }
    OutError.Reset();
    return true;
}

bool FTRIADRFIndexedGeometryQuery::TryGetMaterialMetadata(
    const FString& MaterialId,
    FTRIADRFIndexedMaterialMetadata& OutMetadata) const
{
    OutMetadata = FTRIADRFIndexedMaterialMetadata();
    if (!IsReady())
    {
        return false;
    }
    const int32* MaterialIndex = Impl->MaterialById.Find(MaterialId);
    if (MaterialIndex == nullptr)
    {
        return false;
    }
    const FImpl::FMaterial& Material = Impl->Materials[*MaterialIndex];
    OutMetadata.MaterialId = Material.MaterialId;
    OutMetadata.ProvenanceId = Material.ProvenanceId;
    OutMetadata.CalibrationState = Material.CalibrationState;
    OutMetadata.UncertaintyClass = Material.UncertaintyClass;
    OutMetadata.RuntimeProfileCount = Material.Profiles.Num();
    return true;
}

bool FTRIADRFIndexedGeometryQuery::TryGetSolidMetadata(
    const FString& SolidId,
    FTRIADRFIndexedSolidMetadata& OutMetadata) const
{
    OutMetadata = FTRIADRFIndexedSolidMetadata();
    if (!IsReady())
    {
        return false;
    }
    const int32* SolidIndex = Impl->SolidById.Find(SolidId);
    if (SolidIndex == nullptr)
    {
        return false;
    }
    const FImpl::FSolid& Solid = Impl->Solids[*SolidIndex];
    OutMetadata.SolidId = Solid.SolidId;
    OutMetadata.PrimitiveType = Solid.PrimitiveType;
    OutMetadata.Role = Solid.Role;
    OutMetadata.MaterialId = Solid.MaterialId;
    OutMetadata.BoundaryRole = Solid.BoundaryRole;
    OutMetadata.SourceClass = Solid.SourceClass;
    OutMetadata.UncertaintyClass = Solid.UncertaintyClass;
    OutMetadata.ApertureId = Solid.ApertureId;
    OutMetadata.ApertureState = Solid.ApertureState;
    OutMetadata.BoundsMinimumCentimeters = Solid.DeclaredBounds.Min;
    OutMetadata.BoundsMaximumCentimeters = Solid.DeclaredBounds.Max;
    OutMetadata.VertexStart = Solid.VertexStart;
    OutMetadata.VertexCount = Solid.VertexCount;
    OutMetadata.TriangleStart = Solid.TriangleStart;
    OutMetadata.TriangleCount = Solid.TriangleCount;
    return true;
}

bool FTRIADRFIndexedGeometryQuery::TryGetSurfaceMetadata(
    const FString& SurfaceId,
    FTRIADRFIndexedSurfaceMetadata& OutMetadata) const
{
    OutMetadata = FTRIADRFIndexedSurfaceMetadata();
    if (!IsReady())
    {
        return false;
    }
    const int32* SurfaceIndex = Impl->SurfaceById.Find(SurfaceId);
    if (SurfaceIndex == nullptr)
    {
        return false;
    }
    const FImpl::FSurface& Surface = Impl->Surfaces[*SurfaceIndex];
    OutMetadata.SurfaceId = Surface.SurfaceId;
    OutMetadata.SolidId = Surface.SolidId;
    OutMetadata.MaterialId = Surface.MaterialId;
    OutMetadata.BoundaryRole = Surface.BoundaryRole;
    OutMetadata.SourceClass = Surface.SourceClass;
    OutMetadata.UncertaintyClass = Surface.UncertaintyClass;
    OutMetadata.ApertureId = Surface.ApertureId;
    OutMetadata.ApertureState = Surface.ApertureState;
    OutMetadata.OutwardNormal = Surface.DeclaredOutwardNormal;
    OutMetadata.TriangleStart = Surface.TriangleStart;
    OutMetadata.TriangleCount = Surface.TriangleCount;
    return true;
}

bool FTRIADRFIndexedGeometryQuery::TraceSegment(
    const FVector& StartCentimeters,
    const FVector& EndCentimeters,
    double PositionToleranceCentimeters,
    TArray<FTRIADRFIndexedSegmentHit>& OutHits,
    FString& OutError) const
{
    return Impl->Trace(
        StartCentimeters,
        EndCentimeters,
        PositionToleranceCentimeters,
        OutHits,
        OutError);
}

bool FTRIADRFIndexedGeometryQuery::BuildPathCandidates(
    const FVector& TransmitterCentimeters,
    const FVector& ReceiverCentimeters,
    double FrequencyGHz,
    const FTRIADRFModelLimits& Limits,
    TArray<FTRIADRFPathCandidate>& OutCandidates,
    FString& OutError) const
{
    OutCandidates.Reset();
    OutError.Reset();
    if (!IsReady())
    {
        OutError = TEXT("RF indexed geometry query is not loaded and ready.");
        return false;
    }
    if (!FMath::IsFinite(FrequencyGHz) ||
        FrequencyGHz < Limits.MinimumFrequencyGHz ||
        FrequencyGHz > Limits.MaximumFrequencyGHz ||
        Limits.MaximumCandidatePaths < 1 || Limits.MaximumVerticesPerPath < 2 ||
        Limits.MaximumInteractionsPerPath < 0 ||
        Limits.MaximumContributorsPerInteraction < 1 ||
        Limits.MaximumContributorsPerInteraction > 32 ||
        !FMath::IsFinite(Limits.GeometryPositionToleranceCentimeters) ||
        Limits.GeometryPositionToleranceCentimeters <= 0.0 ||
        Limits.GeometryPositionToleranceCentimeters > 1.0)
    {
        OutError = TEXT("RF geometry query inputs or model limits are invalid/out of range.");
        return false;
    }
    const double PathLengthMeters =
        FVector::Distance(TransmitterCentimeters, ReceiverCentimeters) / 100.0;
    if (!FMath::IsFinite(PathLengthMeters) ||
        PathLengthMeters < Limits.MinimumPathLengthMeters ||
        PathLengthMeters > Limits.MaximumPathLengthMeters)
    {
        OutError = TEXT("RF query path length is outside the configured model envelope.");
        return false;
    }

    TArray<FTRIADRFIndexedSegmentHit> Hits;
    if (!TraceSegment(
            TransmitterCentimeters,
            ReceiverCentimeters,
            Limits.GeometryPositionToleranceCentimeters,
            Hits,
            OutError))
    {
        return false;
    }
    FTRIADRFPathCandidate Candidate;
    Candidate.GeometryQueryId = Impl->Metadata.GeometryQueryId;
    Candidate.VerticesCentimeters = {TransmitterCentimeters, ReceiverCentimeters};
    FTRIADRFClearSegmentWitness Witness;
    Witness.WitnessId = FString::Printf(
        TEXT("witness:%s:segment:0"), *Impl->Metadata.GeometryRevision);
    Witness.SegmentIndex = 0;
    Witness.bNoUnmodelledBlockingHit = true;
    Candidate.ClearSegmentWitnesses.Add(MoveTemp(Witness));
    if (Hits.IsEmpty())
    {
        Candidate.PathId = FString::Printf(TEXT("direct:%s"), *Impl->Metadata.GeometryRevision);
        Candidate.Kind = ETRIADRFPathKind::Direct;
        OutCandidates.Add(MoveTemp(Candidate));
        return true;
    }

    struct FPair
    {
        const FTRIADRFIndexedSegmentHit* Entry = nullptr;
        const FTRIADRFIndexedSegmentHit* Exit = nullptr;
        TArray<FTRIADRFInteractionContributor> Contributors;
    };
    TMap<FString, const FTRIADRFIndexedSegmentHit*> PendingEntry;
    TArray<FPair> Pairs;
    for (const FTRIADRFIndexedSegmentHit& Hit : Hits)
    {
        if (Hit.Crossing == ETRIADRFBoundaryCrossing::Entry)
        {
            if (PendingEntry.Contains(Hit.SolidId))
            {
                OutError = TEXT("RF segment contains an unpaired duplicate entry crossing.");
                return false;
            }
            PendingEntry.Add(Hit.SolidId, &Hit);
        }
        else
        {
            const FTRIADRFIndexedSegmentHit* const* Entry = PendingEntry.Find(Hit.SolidId);
            if (Entry == nullptr)
            {
                OutError = TEXT("RF segment contains an exit without a paired entry.");
                return false;
            }
            FPair Pair;
            Pair.Entry = *Entry;
            Pair.Exit = &Hit;
            FTRIADRFInteractionContributor Contributor;
            Contributor.SolidId = Pair.Entry->SolidId;
            Contributor.EntrySurfaceId = Pair.Entry->SurfaceId;
            Contributor.ExitSurfaceId = Pair.Exit->SurfaceId;
            Contributor.SourceClass = Pair.Entry->SourceClass;
            Contributor.UncertaintyClass = Pair.Entry->UncertaintyClass;
            Contributor.EntryPointCentimeters = Pair.Entry->PointCentimeters;
            Contributor.ExitPointCentimeters = Pair.Exit->PointCentimeters;
            Pair.Contributors.Add(MoveTemp(Contributor));
            Pairs.Add(MoveTemp(Pair));
            PendingEntry.Remove(Hit.SolidId);
        }
    }
    if (!PendingEntry.IsEmpty())
    {
        OutError = TEXT("RF segment has unpaired crossings.");
        return false;
    }
    Pairs.Sort([](const FPair& First, const FPair& Second)
    {
        if (First.Entry->SegmentParameter != Second.Entry->SegmentParameter)
        {
            return First.Entry->SegmentParameter < Second.Entry->SegmentParameter;
        }
        return First.Entry->SolidId < Second.Entry->SolidId;
    });

    // TraceSegment intentionally preserves every canonical solid boundary for
    // diagnostics and provenance.  Straight-transmission interactions instead
    // operate on resolvable material spans.  Two closed solids that meet at an
    // opposed, sub-tolerance boundary do not contain a resolvable air interval.
    // Charging each same-material solid as a separate slab would therefore add
    // an artificial second paired-boundary loss.  Conversely, this catalog has
    // no material-to-material transition coefficient, so a mixed-material
    // shared interface must fail closed rather than inventing one or double-
    // charging two unrelated air/material priors.
    const double SegmentLengthCentimeters = PathLengthMeters * 100.0;
    const double SeamParameterTolerance =
        Limits.GeometryPositionToleranceCentimeters / SegmentLengthCentimeters;
    TArray<FPair> TransmissionSpans;
    TransmissionSpans.Reserve(Pairs.Num());
    for (const FPair& Current : Pairs)
    {
        if (TransmissionSpans.IsEmpty())
        {
            TransmissionSpans.Add(Current);
            continue;
        }

        FPair& Previous = TransmissionSpans.Last();
        const double SignedParameterGap =
            Current.Entry->SegmentParameter - Previous.Exit->SegmentParameter;
        const double BoundaryDistanceCentimeters = FVector::Distance(
            Previous.Exit->PointCentimeters,
            Current.Entry->PointCentimeters);
        const bool bCoincidentWithinTolerance =
            FMath::Abs(SignedParameterGap) <= SeamParameterTolerance &&
            BoundaryDistanceCentimeters <= Limits.GeometryPositionToleranceCentimeters;

        if (SignedParameterGap < -SeamParameterTolerance)
        {
            OutError = FString::Printf(
                TEXT("RF solid intervals '%s' and '%s' overlap along the finite segment; transmission ordering is ambiguous."),
                *Previous.Exit->SolidId,
                *Current.Entry->SolidId);
            return false;
        }
        if (!bCoincidentWithinTolerance)
        {
            TransmissionSpans.Add(Current);
            continue;
        }

        if (FVector::DotProduct(
                Previous.Exit->OutwardNormal,
                Current.Entry->OutwardNormal) > -NormalAgreementCosine)
        {
            OutError = FString::Printf(
                TEXT("RF solid intervals '%s' and '%s' meet within geometry tolerance but their boundary normals are not opposed; interface semantics are ambiguous."),
                *Previous.Exit->SolidId,
                *Current.Entry->SolidId);
            return false;
        }
        if (Previous.Exit->MaterialId != Current.Entry->MaterialId)
        {
            OutError = FString::Printf(
                TEXT("Mixed-material shared RF interface between solids '%s' (%s) and '%s' (%s) is unresolved; no material-transition prior is available."),
                *Previous.Exit->SolidId,
                *Previous.Exit->MaterialId,
                *Current.Entry->SolidId,
                *Current.Entry->MaterialId);
            return false;
        }

        // Same-material continuity: retain the first physical entry as the
        // singular compatibility owner and extend through the last physical
        // exit, applying one paired-boundary prior. Contributors remains the
        // authoritative ordered provenance for every merged physical solid.
        Previous.Exit = Current.Exit;
        Previous.Contributors.Append(Current.Contributors);
        if (Previous.Contributors.Num() >
            Limits.MaximumContributorsPerInteraction)
        {
            OutError = TEXT("RF material-span contributors exceed the configured per-interaction bound.");
            return false;
        }
    }
    if (TransmissionSpans.Num() > Limits.MaximumInteractionsPerPath)
    {
        OutError = TEXT("RF material spans exceed the interaction bound.");
        return false;
    }

    Candidate.PathId = FString::Printf(TEXT("transmitted:%s"), *Impl->Metadata.GeometryRevision);
    Candidate.Kind = ETRIADRFPathKind::Transmitted;
    for (const FPair& Pair : TransmissionSpans)
    {
        const int32 SolidIndex = Impl->SolidById.FindChecked(Pair.Entry->SolidId);
        const FImpl::FSolid& Solid = Impl->Solids[SolidIndex];
        const FImpl::FRuntimeProfile* Profile = Impl->SelectProfile(
            Solid.MaterialIndex,
            FrequencyGHz,
            OutError);
        if (Profile == nullptr)
        {
            return false;
        }
        if (!Profile->bAllowsTransmission)
        {
            // A valid opaque material blocks this bounded straight path.  This
            // is a successful query with no admitted path, not malformed data.
            OutCandidates.Reset();
            OutError.Reset();
            return true;
        }
        FTRIADRFPathInteraction Interaction;
        Interaction.Kind = ETRIADRFInteractionKind::Transmission;
        Interaction.SegmentIndex = 0;
        Interaction.VertexIndex = INDEX_NONE;
        Interaction.EntryPointCentimeters = Pair.Entry->PointCentimeters;
        Interaction.ExitPointCentimeters = Pair.Exit->PointCentimeters;
        Interaction.SurfaceNormal = Pair.Entry->OutwardNormal;
        Interaction.Surface.SurfaceId = Pair.Entry->SurfaceId;
        Interaction.Surface.SolidId = Pair.Entry->SolidId;
        Interaction.Surface.MaterialId = Pair.Entry->MaterialId;
        Interaction.Surface.ProfileId = Profile->ProfileId;
        Interaction.Surface.SourceClass = Pair.Entry->SourceClass;
        Interaction.Surface.UncertaintyClass = Pair.Entry->UncertaintyClass;
        Interaction.Surface.CoefficientSelectionSemantics =
            TEXT("SINGLE_EXPLICIT_PROFILE_CLOSED_INTERVAL_NO_INTERPOLATION");
        Interaction.Surface.Contributors = Pair.Contributors;
        Interaction.Surface.MinimumFrequencyGHz = Profile->MinimumFrequencyGHz;
        Interaction.Surface.MaximumFrequencyGHz = Profile->MaximumFrequencyGHz;
        Interaction.Surface.MinimumIncidenceCosine = Profile->MinimumIncidenceCosine;
        Interaction.Surface.MaximumIncidenceCosine = Profile->MaximumIncidenceCosine;
        Interaction.Surface.PairedBoundaryTransmissionLossDb = Profile->PairedBoundaryTransmissionLossDb;
        Interaction.Surface.BulkAttenuationDbPerMeter = Profile->BulkAttenuationDbPerMeter;
        Interaction.Surface.ReflectionLossDb = Profile->ReflectionLossDb;
        Interaction.Surface.EmpiricalGrazingReflectionLossDb = Profile->EmpiricalGrazingReflectionLossDb;
        Interaction.Surface.bAllowsTransmission = Profile->bAllowsTransmission;
        Interaction.Surface.bAllowsReflection = Profile->bAllowsReflection;
        Interaction.Surface.CalibrationState = ETRIADRFMaterialCalibrationState::Uncalibrated;
        Interaction.Surface.CalibrationProvenanceId = FString::Printf(
            TEXT("%s/profile:%s@catalog-sha256:%s"),
            *Profile->CalibrationProvenanceId,
            *Profile->ProfileId,
            *Impl->Metadata.MaterialCatalogSha256);
        Candidate.Interactions.Add(MoveTemp(Interaction));
    }
    OutCandidates.Add(MoveTemp(Candidate));
    return true;
}
