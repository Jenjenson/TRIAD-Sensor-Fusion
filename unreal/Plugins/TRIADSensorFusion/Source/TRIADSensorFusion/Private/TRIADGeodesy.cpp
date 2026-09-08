#include "TRIADGeodesy.h"

#include "TRIADSensorFusionTypes.h"

namespace
{
constexpr double Wgs84SemiMajorAxisMeters = 6378137.0;
constexpr double Wgs84Flattening = 1.0 / 298.257223563;
constexpr double Wgs84SemiMinorAxisMeters =
    Wgs84SemiMajorAxisMeters * (1.0 - Wgs84Flattening);
constexpr double MeanEarthRadiusMeters = 6371008.8;

double NormalizeLongitudeRadians(double LongitudeRadians)
{
    return FMath::Fmod(LongitudeRadians + 3.0 * PI, 2.0 * PI) - PI;
}

double HaversineFallbackMeters(
    double LongitudeARadians,
    double LatitudeARadians,
    double LongitudeBRadians,
    double LatitudeBRadians)
{
    const double DeltaLatitude = LatitudeBRadians - LatitudeARadians;
    const double DeltaLongitude = NormalizeLongitudeRadians(LongitudeBRadians - LongitudeARadians);
    const double SinHalfLatitude = FMath::Sin(DeltaLatitude * 0.5);
    const double SinHalfLongitude = FMath::Sin(DeltaLongitude * 0.5);
    const double A = SinHalfLatitude * SinHalfLatitude +
        FMath::Cos(LatitudeARadians) * FMath::Cos(LatitudeBRadians) *
        SinHalfLongitude * SinHalfLongitude;
    return 2.0 * MeanEarthRadiusMeters *
        FMath::Asin(FMath::Sqrt(FMath::Clamp(A, 0.0, 1.0)));
}
}

namespace TRIAD::Geodesy
{
bool IsCircle(const FTRIADSimulationPerimeter& Perimeter)
{
    return Perimeter.Shape.Equals(TEXT("Circle"), ESearchCase::IgnoreCase);
}

bool ValidatePerimeter(const FTRIADSimulationPerimeter& Perimeter, FString& OutError)
{
    OutError.Reset();
    if (!Perimeter.bEnabled)
    {
        return true;
    }

    if (Perimeter.Shape.IsEmpty() ||
        Perimeter.Shape.Equals(TEXT("Rectangle"), ESearchCase::IgnoreCase))
    {
        if (!FMath::IsFinite(Perimeter.MinimumLongitudeDegrees) ||
            !FMath::IsFinite(Perimeter.MaximumLongitudeDegrees) ||
            !FMath::IsFinite(Perimeter.MinimumLatitudeDegrees) ||
            !FMath::IsFinite(Perimeter.MaximumLatitudeDegrees) ||
            Perimeter.MinimumLongitudeDegrees < -180.0 ||
            Perimeter.MinimumLongitudeDegrees > 180.0 ||
            Perimeter.MaximumLongitudeDegrees < -180.0 ||
            Perimeter.MaximumLongitudeDegrees > 180.0 ||
            Perimeter.MinimumLatitudeDegrees < -90.0 ||
            Perimeter.MinimumLatitudeDegrees > 90.0 ||
            Perimeter.MaximumLatitudeDegrees < -90.0 ||
            Perimeter.MaximumLatitudeDegrees > 90.0)
        {
            OutError = TEXT("Rectangle perimeter coordinates must be finite WGS84 longitude/latitude values.");
            return false;
        }
        if (FMath::IsNearlyEqual(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees) ||
            FMath::IsNearlyEqual(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees))
        {
            OutError = TEXT("Rectangle perimeter must have non-zero longitude and latitude spans.");
            return false;
        }
        return true;
    }

    if (IsCircle(Perimeter))
    {
        if (!FMath::IsFinite(Perimeter.CenterLongitudeDegrees) ||
            !FMath::IsFinite(Perimeter.CenterLatitudeDegrees) ||
            Perimeter.CenterLongitudeDegrees < -180.0 ||
            Perimeter.CenterLongitudeDegrees > 180.0 ||
            Perimeter.CenterLatitudeDegrees < -90.0 ||
            Perimeter.CenterLatitudeDegrees > 90.0 ||
            !FMath::IsFinite(Perimeter.RadiusMeters) ||
            Perimeter.RadiusMeters <= 0.0)
        {
            OutError = TEXT("Circle perimeter requires a finite WGS84 center and RadiusMeters > 0.");
            return false;
        }
        return true;
    }

    OutError = FString::Printf(
        TEXT("Unsupported SimulationPerimeter.Shape '%s'; expected Rectangle or Circle."),
        *Perimeter.Shape);
    return false;
}

double Wgs84DistanceMeters(
    double LongitudeADegrees,
    double LatitudeADegrees,
    double LongitudeBDegrees,
    double LatitudeBDegrees)
{
    const double LongitudeA = FMath::DegreesToRadians(LongitudeADegrees);
    const double LatitudeA = FMath::DegreesToRadians(LatitudeADegrees);
    const double LongitudeB = FMath::DegreesToRadians(LongitudeBDegrees);
    const double LatitudeB = FMath::DegreesToRadians(LatitudeBDegrees);
    const double L = NormalizeLongitudeRadians(LongitudeB - LongitudeA);

    const double U1 = FMath::Atan((1.0 - Wgs84Flattening) * FMath::Tan(LatitudeA));
    const double U2 = FMath::Atan((1.0 - Wgs84Flattening) * FMath::Tan(LatitudeB));
    const double SinU1 = FMath::Sin(U1);
    const double CosU1 = FMath::Cos(U1);
    const double SinU2 = FMath::Sin(U2);
    const double CosU2 = FMath::Cos(U2);

    double Lambda = L;
    double SinSigma = 0.0;
    double CosSigma = 1.0;
    double Sigma = 0.0;
    double SinAlpha = 0.0;
    double CosSquaredAlpha = 1.0;
    double CosTwoSigmaM = 0.0;
    bool bConverged = false;

    for (int32 Iteration = 0; Iteration < 100; ++Iteration)
    {
        const double SinLambda = FMath::Sin(Lambda);
        const double CosLambda = FMath::Cos(Lambda);
        const double T1 = CosU2 * SinLambda;
        const double T2 = CosU1 * SinU2 - SinU1 * CosU2 * CosLambda;
        SinSigma = FMath::Sqrt(T1 * T1 + T2 * T2);
        if (SinSigma <= UE_DOUBLE_SMALL_NUMBER)
        {
            return 0.0;
        }
        CosSigma = SinU1 * SinU2 + CosU1 * CosU2 * CosLambda;
        Sigma = FMath::Atan2(SinSigma, CosSigma);
        SinAlpha = CosU1 * CosU2 * SinLambda / SinSigma;
        CosSquaredAlpha = 1.0 - SinAlpha * SinAlpha;
        CosTwoSigmaM = CosSquaredAlpha > UE_DOUBLE_SMALL_NUMBER
            ? CosSigma - 2.0 * SinU1 * SinU2 / CosSquaredAlpha
            : 0.0;
        const double C = Wgs84Flattening / 16.0 * CosSquaredAlpha *
            (4.0 + Wgs84Flattening * (4.0 - 3.0 * CosSquaredAlpha));
        const double PreviousLambda = Lambda;
        Lambda = L + (1.0 - C) * Wgs84Flattening * SinAlpha *
            (Sigma + C * SinSigma *
                (CosTwoSigmaM + C * CosSigma *
                    (-1.0 + 2.0 * CosTwoSigmaM * CosTwoSigmaM)));
        if (FMath::Abs(Lambda - PreviousLambda) < 1.0e-12)
        {
            bConverged = true;
            break;
        }
    }

    if (!bConverged)
    {
        return HaversineFallbackMeters(LongitudeA, LatitudeA, LongitudeB, LatitudeB);
    }

    const double U2Term = CosSquaredAlpha *
        (Wgs84SemiMajorAxisMeters * Wgs84SemiMajorAxisMeters -
            Wgs84SemiMinorAxisMeters * Wgs84SemiMinorAxisMeters) /
        (Wgs84SemiMinorAxisMeters * Wgs84SemiMinorAxisMeters);
    const double A = 1.0 + U2Term / 16384.0 *
        (4096.0 + U2Term * (-768.0 + U2Term * (320.0 - 175.0 * U2Term)));
    const double B = U2Term / 1024.0 *
        (256.0 + U2Term * (-128.0 + U2Term * (74.0 - 47.0 * U2Term)));
    const double DeltaSigma = B * SinSigma *
        (CosTwoSigmaM + B / 4.0 *
            (CosSigma * (-1.0 + 2.0 * CosTwoSigmaM * CosTwoSigmaM) -
                B / 6.0 * CosTwoSigmaM *
                    (-3.0 + 4.0 * SinSigma * SinSigma) *
                    (-3.0 + 4.0 * CosTwoSigmaM * CosTwoSigmaM)));
    return Wgs84SemiMinorAxisMeters * A * (Sigma - DeltaSigma);
}

FVector2D Wgs84DestinationDegrees(
    double StartLongitudeDegrees,
    double StartLatitudeDegrees,
    double BearingDegrees,
    double DistanceMeters)
{
    if (DistanceMeters <= 0.0)
    {
        return FVector2D(StartLongitudeDegrees, StartLatitudeDegrees);
    }

    const double Alpha1 = FMath::DegreesToRadians(BearingDegrees);
    const double SinAlpha1 = FMath::Sin(Alpha1);
    const double CosAlpha1 = FMath::Cos(Alpha1);
    const double Latitude1 = FMath::DegreesToRadians(StartLatitudeDegrees);
    const double Longitude1 = FMath::DegreesToRadians(StartLongitudeDegrees);
    const double TanU1 = (1.0 - Wgs84Flattening) * FMath::Tan(Latitude1);
    const double CosU1 = 1.0 / FMath::Sqrt(1.0 + TanU1 * TanU1);
    const double SinU1 = TanU1 * CosU1;
    const double Sigma1 = FMath::Atan2(TanU1, CosAlpha1);
    const double SinAlpha = CosU1 * SinAlpha1;
    const double CosSquaredAlpha = 1.0 - SinAlpha * SinAlpha;
    const double U2Term = CosSquaredAlpha *
        (Wgs84SemiMajorAxisMeters * Wgs84SemiMajorAxisMeters -
            Wgs84SemiMinorAxisMeters * Wgs84SemiMinorAxisMeters) /
        (Wgs84SemiMinorAxisMeters * Wgs84SemiMinorAxisMeters);
    const double A = 1.0 + U2Term / 16384.0 *
        (4096.0 + U2Term * (-768.0 + U2Term * (320.0 - 175.0 * U2Term)));
    const double B = U2Term / 1024.0 *
        (256.0 + U2Term * (-128.0 + U2Term * (74.0 - 47.0 * U2Term)));

    double Sigma = DistanceMeters / (Wgs84SemiMinorAxisMeters * A);
    double PreviousSigma = TNumericLimits<double>::Max();
    double CosTwoSigmaM = 0.0;
    double SinSigma = 0.0;
    double CosSigma = 1.0;
    for (int32 Iteration = 0;
         Iteration < 100 && FMath::Abs(Sigma - PreviousSigma) > 1.0e-12;
         ++Iteration)
    {
        CosTwoSigmaM = FMath::Cos(2.0 * Sigma1 + Sigma);
        SinSigma = FMath::Sin(Sigma);
        CosSigma = FMath::Cos(Sigma);
        const double DeltaSigma = B * SinSigma *
            (CosTwoSigmaM + B / 4.0 *
                (CosSigma * (-1.0 + 2.0 * CosTwoSigmaM * CosTwoSigmaM) -
                    B / 6.0 * CosTwoSigmaM *
                        (-3.0 + 4.0 * SinSigma * SinSigma) *
                        (-3.0 + 4.0 * CosTwoSigmaM * CosTwoSigmaM)));
        PreviousSigma = Sigma;
        Sigma = DistanceMeters / (Wgs84SemiMinorAxisMeters * A) + DeltaSigma;
    }

    const double T = SinU1 * SinSigma - CosU1 * CosSigma * CosAlpha1;
    const double Latitude2 = FMath::Atan2(
        SinU1 * CosSigma + CosU1 * SinSigma * CosAlpha1,
        (1.0 - Wgs84Flattening) * FMath::Sqrt(SinAlpha * SinAlpha + T * T));
    const double Lambda = FMath::Atan2(
        SinSigma * SinAlpha1,
        CosU1 * CosSigma - SinU1 * SinSigma * CosAlpha1);
    const double C = Wgs84Flattening / 16.0 * CosSquaredAlpha *
        (4.0 + Wgs84Flattening * (4.0 - 3.0 * CosSquaredAlpha));
    const double L = Lambda - (1.0 - C) * Wgs84Flattening * SinAlpha *
        (Sigma + C * SinSigma *
            (CosTwoSigmaM + C * CosSigma *
                (-1.0 + 2.0 * CosTwoSigmaM * CosTwoSigmaM)));
    return FVector2D(
        FMath::RadiansToDegrees(NormalizeLongitudeRadians(Longitude1 + L)),
        FMath::RadiansToDegrees(Latitude2));
}

bool Wgs84ToSvy21Meters(
    double LongitudeDegrees,
    double LatitudeDegrees,
    FVector2D& OutEastingNorthingMeters,
    FString& OutError)
{
    OutEastingNorthingMeters = FVector2D::ZeroVector;
    if (!FMath::IsFinite(LongitudeDegrees) || !FMath::IsFinite(LatitudeDegrees) ||
        LongitudeDegrees < 103.0 || LongitudeDegrees > 105.0 ||
        LatitudeDegrees < 0.0 || LatitudeDegrees > 3.0)
    {
        OutError = TEXT("EPSG:3414 forward projection requires finite WGS84 coordinates within the bounded Singapore-area zone.");
        return false;
    }
    constexpr double E2 = Wgs84Flattening * (2.0 - Wgs84Flattening);
    constexpr double Ep2 = E2 / (1.0 - E2);
    constexpr double K0 = 1.0;
    constexpr double FalseEasting = 28001.642;
    constexpr double FalseNorthing = 38744.572;
    const double Latitude = FMath::DegreesToRadians(LatitudeDegrees);
    const double Longitude = FMath::DegreesToRadians(LongitudeDegrees);
    // EPSG:3414 latitude of natural origin: 1°22'00"N.
    const double Latitude0 = FMath::DegreesToRadians(1.3666666666666667);
    const double Longitude0 = FMath::DegreesToRadians(103.83333333333333);
    const double E4 = E2 * E2;
    const double E6 = E4 * E2;
    const auto MeridianArc = [=](double Phi)
    {
        return Wgs84SemiMajorAxisMeters *
            ((1.0 - E2 / 4.0 - 3.0 * E4 / 64.0 - 5.0 * E6 / 256.0) * Phi -
             (3.0 * E2 / 8.0 + 3.0 * E4 / 32.0 + 45.0 * E6 / 1024.0) * FMath::Sin(2.0 * Phi) +
             (15.0 * E4 / 256.0 + 45.0 * E6 / 1024.0) * FMath::Sin(4.0 * Phi) -
             (35.0 * E6 / 3072.0) * FMath::Sin(6.0 * Phi));
    };
    const double SinLatitude = FMath::Sin(Latitude);
    const double CosLatitude = FMath::Cos(Latitude);
    const double TanLatitude = FMath::Tan(Latitude);
    const double N = Wgs84SemiMajorAxisMeters /
        FMath::Sqrt(1.0 - E2 * SinLatitude * SinLatitude);
    const double T = TanLatitude * TanLatitude;
    const double C = Ep2 * CosLatitude * CosLatitude;
    const double A = CosLatitude * NormalizeLongitudeRadians(Longitude - Longitude0);
    const double A2 = A * A;
    const double A3 = A2 * A;
    const double A4 = A2 * A2;
    const double A5 = A4 * A;
    const double A6 = A3 * A3;
    const double Easting = FalseEasting + K0 * N *
        (A + (1.0 - T + C) * A3 / 6.0 +
         (5.0 - 18.0 * T + T * T + 72.0 * C - 58.0 * Ep2) * A5 / 120.0);
    const double Northing = FalseNorthing + K0 *
        (MeridianArc(Latitude) - MeridianArc(Latitude0) + N * TanLatitude *
            (A2 / 2.0 + (5.0 - T + 9.0 * C + 4.0 * C * C) * A4 / 24.0 +
             (61.0 - 58.0 * T + T * T + 600.0 * C - 330.0 * Ep2) * A6 / 720.0));
    if (!FMath::IsFinite(Easting) || !FMath::IsFinite(Northing))
    {
        OutError = TEXT("EPSG:3414 forward projection produced a non-finite coordinate.");
        return false;
    }
    OutEastingNorthingMeters = FVector2D(Easting, Northing);
    OutError.Reset();
    return true;
}

double SignedDistanceToPerimeterMeters(
    const FTRIADSimulationPerimeter& Perimeter,
    double LongitudeDegrees,
    double LatitudeDegrees)
{
    if (IsCircle(Perimeter))
    {
        const double CenterDistanceMeters = Wgs84DistanceMeters(
            Perimeter.CenterLongitudeDegrees,
            Perimeter.CenterLatitudeDegrees,
            LongitudeDegrees,
            LatitudeDegrees);
        const double SignedDistance = CenterDistanceMeters - Perimeter.RadiusMeters;
        return FMath::Abs(SignedDistance) <= 1.0e-6 ? 0.0 : SignedDistance;
    }

    const double MinimumLongitude = FMath::Min(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees);
    const double MaximumLongitude = FMath::Max(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees);
    const double MinimumLatitude = FMath::Min(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees);
    const double MaximumLatitude = FMath::Max(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees);
    const double MidLatitudeRadians = FMath::DegreesToRadians((MinimumLatitude + MaximumLatitude) * 0.5);
    const double MetersPerDegreeLatitude = PI * Wgs84SemiMajorAxisMeters / 180.0;
    const double MetersPerDegreeLongitude = MetersPerDegreeLatitude *
        FMath::Max(FMath::Abs(FMath::Cos(MidLatitudeRadians)), 0.000001);

    const double X = LongitudeDegrees * MetersPerDegreeLongitude;
    const double WestX = MinimumLongitude * MetersPerDegreeLongitude;
    const double EastX = MaximumLongitude * MetersPerDegreeLongitude;
    const double Y = LatitudeDegrees * MetersPerDegreeLatitude;
    const double SouthY = MinimumLatitude * MetersPerDegreeLatitude;
    const double NorthY = MaximumLatitude * MetersPerDegreeLatitude;

    const double OutsideDx = X < WestX ? WestX - X : X > EastX ? X - EastX : 0.0;
    const double OutsideDy = Y < SouthY ? SouthY - Y : Y > NorthY ? Y - NorthY : 0.0;
    if (OutsideDx > 0.0 || OutsideDy > 0.0)
    {
        return FMath::Sqrt(OutsideDx * OutsideDx + OutsideDy * OutsideDy);
    }

    const double NearestInsideEdgeMeters = FMath::Min(
        FMath::Min(X - WestX, EastX - X),
        FMath::Min(Y - SouthY, NorthY - Y));
    return -FMath::Max(NearestInsideEdgeMeters, 0.0);
}
}
