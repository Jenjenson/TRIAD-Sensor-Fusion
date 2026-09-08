#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADGeodesy.h"
#include "TRIADSensorFusionTypes.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaGeodesicCircleTest,
    "TRIAD.SensorFusion.Istana.GeodesicCircle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaGeodesicCircleTest::RunTest(const FString& Parameters)
{
    FTRIADSimulationPerimeter Circle;
    Circle.Shape = TEXT("Circle");
    Circle.CenterLongitudeDegrees = 103.84288055;
    Circle.CenterLatitudeDegrees = 1.30709615;
    Circle.RadiusMeters = 1000.0;

    FString ValidationError;
    TestTrue(TEXT("valid circle contract"), TRIAD::Geodesy::ValidatePerimeter(Circle, ValidationError));
    TestTrue(TEXT("case-insensitive circle shape"), TRIAD::Geodesy::IsCircle(Circle));
    TestEqual(
        TEXT("center is 1000 m inside"),
        TRIAD::Geodesy::SignedDistanceToPerimeterMeters(
            Circle,
            Circle.CenterLongitudeDegrees,
            Circle.CenterLatitudeDegrees),
        -1000.0);

    for (double BearingDegrees : {0.0, 45.0, 90.0, 180.0, 270.0, 315.0})
    {
        const FVector2D Boundary = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Circle.CenterLongitudeDegrees,
            Circle.CenterLatitudeDegrees,
            BearingDegrees,
            Circle.RadiusMeters);
        const double InverseMeters = TRIAD::Geodesy::Wgs84DistanceMeters(
            Circle.CenterLongitudeDegrees,
            Circle.CenterLatitudeDegrees,
            Boundary.X,
            Boundary.Y);
        TestTrue(
            FString::Printf(TEXT("direct/inverse closes at bearing %.0f"), BearingDegrees),
            FMath::Abs(InverseMeters - Circle.RadiusMeters) < 0.001);
        TestTrue(
            FString::Printf(TEXT("inclusive boundary is zero at bearing %.0f"), BearingDegrees),
            FMath::Abs(TRIAD::Geodesy::SignedDistanceToPerimeterMeters(
                Circle,
                Boundary.X,
                Boundary.Y)) < 0.001);
    }

    const FVector2D Outside = TRIAD::Geodesy::Wgs84DestinationDegrees(
        Circle.CenterLongitudeDegrees,
        Circle.CenterLatitudeDegrees,
        120.0,
        1250.0);
    TestTrue(
        TEXT("1250 m point is about 250 m outside"),
        FMath::Abs(TRIAD::Geodesy::SignedDistanceToPerimeterMeters(
            Circle,
            Outside.X,
            Outside.Y) - 250.0) < 0.001);

    Circle.RadiusMeters = 0.0;
    TestFalse(TEXT("zero-radius circle rejected"), TRIAD::Geodesy::ValidatePerimeter(Circle, ValidationError));

    struct FSvy21Fixture
    {
        double Longitude;
        double Latitude;
        double Easting;
        double Northing;
    };
    const FSvy21Fixture Fixtures[] = {
        {103.84288055, 1.30709615, 29064.15860639389, 32157.571268641685},
        {103.84288055, 1.3161397971885311, 29064.154792215944, 33157.57128260344},
        {103.84923425660789, 1.3134909662202767, 29771.262709719216, 32864.68076349905},
        {103.85186602528947, 1.3070961338236966, 30064.158637627355, 32157.57506967263},
        {103.84923422446337, 1.3007013172765294, 29771.26808518709, 31450.467149253647},
        {103.84288055, 1.2980525021576885, 29064.162394277504, 31157.571254679777},
        {103.83652687553663, 1.3007013172765294, 28357.054503070034, 31450.461800078687},
        {103.83389507471054, 1.3070961338236966, 28064.158601454757, 32157.56746761098},
        {103.83652684339212, 1.3134909662202767, 28357.049127599217, 32864.675361735404}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fixtures); ++Index)
    {
        FVector2D Projected;
        FString ProjectionError;
        TestTrue(FString::Printf(TEXT("EPSG:3414 fixture %d projects"), Index),
            TRIAD::Geodesy::Wgs84ToSvy21Meters(
                Fixtures[Index].Longitude, Fixtures[Index].Latitude,
                Projected, ProjectionError));
        TestTrue(FString::Printf(TEXT("EPSG:3414 fixture %d is sub-millimetre exact"), Index),
            Projected.Equals(
                FVector2D(Fixtures[Index].Easting, Fixtures[Index].Northing),
                0.0005));
    }
    FVector2D InvalidProjection;
    TestFalse(TEXT("Non-finite EPSG:3414 input fails closed"),
        TRIAD::Geodesy::Wgs84ToSvy21Meters(
            std::numeric_limits<double>::quiet_NaN(), 1.30709615,
            InvalidProjection, ValidationError));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADRectanglePerimeterCompatibilityTest,
    "TRIAD.SensorFusion.Istana.RectangleCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADRectanglePerimeterCompatibilityTest::RunTest(const FString& Parameters)
{
    FTRIADSimulationPerimeter Rectangle;
    // Shape defaults to Rectangle so legacy JSON that omits Shape is unchanged.
    Rectangle.MinimumLongitudeDegrees = 103.80;
    Rectangle.MaximumLongitudeDegrees = 103.90;
    Rectangle.MinimumLatitudeDegrees = 1.25;
    Rectangle.MaximumLatitudeDegrees = 1.35;
    FString ValidationError;
    TestTrue(TEXT("legacy rectangle valid"), TRIAD::Geodesy::ValidatePerimeter(Rectangle, ValidationError));
    TestFalse(TEXT("legacy rectangle is not circle"), TRIAD::Geodesy::IsCircle(Rectangle));
    TestTrue(
        TEXT("rectangle center remains inside"),
        TRIAD::Geodesy::SignedDistanceToPerimeterMeters(Rectangle, 103.85, 1.30) < 0.0);
    TestEqual(
        TEXT("rectangle west boundary remains inclusive"),
        TRIAD::Geodesy::SignedDistanceToPerimeterMeters(Rectangle, 103.80, 1.30),
        0.0);
    TestTrue(
        TEXT("rectangle east exterior remains positive"),
        TRIAD::Geodesy::SignedDistanceToPerimeterMeters(Rectangle, 103.91, 1.30) > 0.0);
    return true;
}

#endif
