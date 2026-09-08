#include "TRIADOperatorSlateWidget.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "TRIADGeodesy.h"
#include "TRIADOperatorObserverActor.h"
#include "TRIADSensorNodeActor.h"

namespace
{
const FSlateBrush* WhiteBrush()
{
    return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
}

FPaintGeometry PaintGeometryAt(
    const FGeometry& Geometry,
    const FVector2f& Position,
    const FVector2f& Size)
{
    return Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position));
}

void PaintBox(
    FSlateWindowElementList& Elements,
    int32 Layer,
    const FGeometry& Geometry,
    const FVector2f& Position,
    const FVector2f& Size,
    const FLinearColor& Color,
    const FSlateBrush* Brush = nullptr)
{
    FSlateDrawElement::MakeBox(
        Elements,
        Layer,
        PaintGeometryAt(Geometry, Position, Size),
        Brush ? Brush : WhiteBrush(),
        ESlateDrawEffect::None,
        Color);
}

void PaintText(
    FSlateWindowElementList& Elements,
    int32 Layer,
    const FGeometry& Geometry,
    const FVector2f& Position,
    const FString& Text,
    const FSlateFontInfo& Font,
    const FLinearColor& Color)
{
    FSlateDrawElement::MakeText(
        Elements,
        Layer,
        PaintGeometryAt(Geometry, Position, FVector2f(1.0f, 1.0f)),
        Text,
        Font,
        ESlateDrawEffect::None,
        Color);
}

void PaintLine(
    FSlateWindowElementList& Elements,
    int32 Layer,
    const FGeometry& Geometry,
    const FVector2f& Start,
    const FVector2f& End,
    const FLinearColor& Color,
    float Thickness)
{
    TArray<FVector2f> Points;
    Points.Add(Start);
    Points.Add(End);
    FSlateDrawElement::MakeLines(
        Elements,
        Layer,
        Geometry.ToPaintGeometry(),
        Points,
        ESlateDrawEffect::None,
        Color,
        true,
        Thickness);
}

void PaintPolyline(
    FSlateWindowElementList& Elements,
    int32 Layer,
    const FGeometry& Geometry,
    const TArray<FVector2f>& Points,
    const FLinearColor& Color,
    float Thickness)
{
    if (Points.Num() < 2)
    {
        return;
    }
    FSlateDrawElement::MakeLines(
        Elements,
        Layer,
        Geometry.ToPaintGeometry(),
        Points,
        ESlateDrawEffect::None,
        Color,
        true,
        Thickness);
}

void PaintCircle(
    FSlateWindowElementList& Elements,
    int32 Layer,
    const FGeometry& Geometry,
    const FVector2f& Center,
    float Radius,
    const FLinearColor& Color,
    float Thickness)
{
    TArray<FVector2f> Points;
    constexpr int32 SegmentCount = 20;
    Points.Reserve(SegmentCount + 1);
    for (int32 Index = 0; Index <= SegmentCount; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(SegmentCount);
        Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
    }
    PaintPolyline(Elements, Layer, Geometry, Points, Color, Thickness);
}

FString FormatRange(double RangeMeters)
{
    return RangeMeters >= 1000.0
        ? FString::Printf(TEXT("%.2f km"), RangeMeters / 1000.0)
        : FString::Printf(TEXT("%.0f m"), RangeMeters);
}
}

void STRIADOperatorSlateWidget::Construct(const FArguments& InArgs)
{
    Observer = InArgs._Observer;
    ObserverImageBrush.DrawAs = ESlateBrushDrawType::Image;
    SensorImageBrush.DrawAs = ESlateBrushDrawType::Image;
    SetVisibility(EVisibility::HitTestInvisible);
}

FVector2D STRIADOperatorSlateWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(1920.0, 1080.0);
}

int32 STRIADOperatorSlateWidget::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const ATRIADOperatorObserverActor* ObserverActor = Observer.Get();
    if (!ObserverActor || !ObserverActor->IsOverlayVisible())
    {
        return LayerId;
    }

    const FVector2f ViewSize = FVector2f(AllottedGeometry.GetLocalSize());
    if (ViewSize.X < 640.0f || ViewSize.Y < 360.0f)
    {
        return LayerId;
    }

    const float UiScale = FMath::Clamp(FMath::Min(ViewSize.X / 1920.0f, ViewSize.Y / 1080.0f), 0.65f, 1.5f);
    const float Margin = 18.0f * UiScale;
    const float HeaderHeight = 44.0f * UiScale;
    const float PanelHeight = FMath::Min(ViewSize.Y * 0.69f, 730.0f * UiScale);
    const float PanelWidth = ViewSize.X - Margin * 2.0f;
    const float RightWidth = FMath::Clamp(PanelWidth * 0.32f, 350.0f * UiScale, 560.0f * UiScale);
    const float Gap = 12.0f * UiScale;
    const float LeftWidth = PanelWidth - RightWidth - Gap;
    const FVector2f PanelOrigin(Margin, Margin);
    const FVector2f CameraOrigin(Margin, Margin + HeaderHeight);
    const FVector2f CameraSize(LeftWidth, PanelHeight - HeaderHeight);
    const FVector2f RightOrigin(CameraOrigin.X + LeftWidth + Gap, CameraOrigin.Y);
    const FVector2f RightSize(RightWidth, CameraSize.Y);

    const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::RoundToInt(12.0f * UiScale));
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::RoundToInt(14.0f * UiScale));
    const FSlateFontInfo BoldFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::RoundToInt(15.0f * UiScale));
    const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::RoundToInt(18.0f * UiScale));
    const FLinearColor Cyan(0.0f, 0.85f, 1.0f, 1.0f);
    const FLinearColor Green(0.15f, 1.0f, 0.48f, 1.0f);
    const FLinearColor Amber(1.0f, 0.65f, 0.05f, 1.0f);
    const FLinearColor Muted(0.58f, 0.70f, 0.76f, 1.0f);
    const FLinearColor Dark(0.006f, 0.025f, 0.035f, 0.94f);

    PaintBox(OutDrawElements, LayerId, AllottedGeometry, PanelOrigin, FVector2f(PanelWidth, PanelHeight), Dark);
    PaintBox(OutDrawElements, LayerId + 1, AllottedGeometry, PanelOrigin, FVector2f(PanelWidth, 3.0f * UiScale), Cyan);
    PaintText(
        OutDrawElements,
        LayerId + 2,
        AllottedGeometry,
        PanelOrigin + FVector2f(14.0f, 10.0f) * UiScale,
        TEXT("TRIAD LIVE INBOUND TRACK VIEWER  |  SENSOR-GATED  |  DETECTION-ONLY"),
        HeaderFont,
        FLinearColor::White);
    PaintText(
        OutDrawElements,
        LayerId + 2,
        AllottedGeometry,
        PanelOrigin + FVector2f(PanelWidth - 355.0f * UiScale, 13.0f * UiScale),
        TEXT("O HIDE/SHOW   N NEXT CONTACT"),
        SmallFont,
        Muted);

    const bool bFreshContact = ObserverActor->HasFreshSelectedContact();
    const FTRIADOperatorObservedContact* Contact = ObserverActor->GetSelectedContact();
    PaintBox(OutDrawElements, LayerId + 1, AllottedGeometry, CameraOrigin, CameraSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.96f));

    if (bFreshContact && Contact)
    {
        if (UTextureRenderTarget2D* ObserverTarget = ObserverActor->GetObserverRenderTarget())
        {
            if (ObserverImageBrush.GetResourceObject() != ObserverTarget)
            {
                ObserverImageBrush.SetResourceObject(ObserverTarget);
                ObserverImageBrush.ImageSize = FVector2D(ObserverTarget->SizeX, ObserverTarget->SizeY);
            }
            PaintBox(
                OutDrawElements,
                LayerId + 2,
                AllottedGeometry,
                CameraOrigin + FVector2f(3.0f, 3.0f) * UiScale,
                CameraSize - FVector2f(6.0f, 6.0f) * UiScale,
                FLinearColor::White,
                &ObserverImageBrush);
        }

        PaintBox(
            OutDrawElements,
            LayerId + 3,
            AllottedGeometry,
            CameraOrigin + FVector2f(12.0f, 12.0f) * UiScale,
            FVector2f(520.0f, 52.0f) * UiScale,
            FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
        PaintText(
            OutDrawElements,
            LayerId + 4,
            AllottedGeometry,
            CameraOrigin + FVector2f(22.0f, 20.0f) * UiScale,
            TEXT("SIMULATION OBSERVER VIEW - NOT SENSOR EVIDENCE"),
            BoldFont,
            Amber);
        PaintText(
            OutDrawElements,
            LayerId + 4,
            AllottedGeometry,
            CameraOrigin + FVector2f(22.0f, 42.0f) * UiScale,
            ObserverActor->GetCameraModeLabel(),
            SmallFont,
            FLinearColor::White);

        const FVector2f PresentationReticleCenter = CameraOrigin + CameraSize * 0.5f;
        const float ReticleHalfWidth = 18.0f * UiScale;
        const float ReticleHalfHeight = 13.0f * UiScale;
        const float ReticleCorner = 7.0f * UiScale;
        PaintLine(OutDrawElements, LayerId + 4, AllottedGeometry,
            PresentationReticleCenter + FVector2f(-ReticleHalfWidth, -ReticleHalfHeight),
            PresentationReticleCenter + FVector2f(-ReticleHalfWidth + ReticleCorner, -ReticleHalfHeight), Amber, 2.0f * UiScale);
        PaintLine(OutDrawElements, LayerId + 4, AllottedGeometry,
            PresentationReticleCenter + FVector2f(-ReticleHalfWidth, -ReticleHalfHeight),
            PresentationReticleCenter + FVector2f(-ReticleHalfWidth, -ReticleHalfHeight + ReticleCorner), Amber, 2.0f * UiScale);
        PaintLine(OutDrawElements, LayerId + 4, AllottedGeometry,
            PresentationReticleCenter + FVector2f(ReticleHalfWidth, ReticleHalfHeight),
            PresentationReticleCenter + FVector2f(ReticleHalfWidth - ReticleCorner, ReticleHalfHeight), Amber, 2.0f * UiScale);
        PaintLine(OutDrawElements, LayerId + 4, AllottedGeometry,
            PresentationReticleCenter + FVector2f(ReticleHalfWidth, ReticleHalfHeight),
            PresentationReticleCenter + FVector2f(ReticleHalfWidth, ReticleHalfHeight - ReticleCorner), Amber, 2.0f * UiScale);
        PaintText(OutDrawElements, LayerId + 4, AllottedGeometry,
            PresentationReticleCenter + FVector2f(-108.0f, 20.0f) * UiScale,
            TEXT("PRESENTATION RETICLE - NOT A DETECTOR BOX"), SmallFont, Amber);

        const FVector2f SensorTileSize(CameraSize.X * 0.37f, CameraSize.Y * 0.33f);
        const FVector2f SensorTileOrigin(
            CameraOrigin.X + CameraSize.X - SensorTileSize.X - 14.0f * UiScale,
            CameraOrigin.Y + CameraSize.Y - SensorTileSize.Y - 14.0f * UiScale);
        PaintBox(OutDrawElements, LayerId + 4, AllottedGeometry, SensorTileOrigin, SensorTileSize, FLinearColor(0.0f, 0.02f, 0.025f, 0.94f));
        PaintBox(OutDrawElements, LayerId + 5, AllottedGeometry, SensorTileOrigin, FVector2f(SensorTileSize.X, 3.0f * UiScale), Green);

        UTextureRenderTarget2D* SensorTarget = nullptr;
        FTRIADPTZConfirmationResult SensorConfirmation;
        const bool bHasSensorImage = ObserverActor->TryGetFreshSensorView(SensorTarget, SensorConfirmation);
        if (bHasSensorImage && SensorTarget)
        {
            if (SensorImageBrush.GetResourceObject() != SensorTarget)
            {
                SensorImageBrush.SetResourceObject(SensorTarget);
                SensorImageBrush.ImageSize = FVector2D(SensorTarget->SizeX, SensorTarget->SizeY);
            }
            const double SourceWidth = FMath::Max(SensorConfirmation.ImageWidthPixels, 1);
            const double SourceHeight = FMath::Max(SensorConfirmation.ImageHeightPixels, 1);
            const double BoxCenterX = FMath::Clamp(
                (SensorConfirmation.BoundingBoxMinimumX + SensorConfirmation.BoundingBoxMaximumX) * 0.5 / SourceWidth,
                0.0,
                1.0);
            const double BoxCenterY = FMath::Clamp(
                (SensorConfirmation.BoundingBoxMinimumY + SensorConfirmation.BoundingBoxMaximumY) * 0.5 / SourceHeight,
                0.0,
                1.0);
            constexpr double CropExtent = 0.12;
            const double CropMinimumX = FMath::Clamp(BoxCenterX - CropExtent, 0.0, 1.0 - CropExtent * 2.0);
            const double CropMinimumY = FMath::Clamp(BoxCenterY - CropExtent, 0.0, 1.0 - CropExtent * 2.0);
            const double CropMaximumX = CropMinimumX + CropExtent * 2.0;
            const double CropMaximumY = CropMinimumY + CropExtent * 2.0;
            SensorImageBrush.SetUVRegion(FBox2f(
                FVector2f(CropMinimumX, CropMinimumY),
                FVector2f(CropMaximumX, CropMaximumY)));
            const FVector2f SensorImageOrigin = SensorTileOrigin + FVector2f(4.0f, 28.0f) * UiScale;
            const FVector2f SensorImageSize(
                SensorTileSize.X - 8.0f * UiScale,
                SensorTileSize.Y - 32.0f * UiScale);
            PaintBox(
                OutDrawElements,
                LayerId + 5,
                AllottedGeometry,
                SensorImageOrigin,
                SensorImageSize,
                FLinearColor::White,
                &SensorImageBrush);

            const double NormalizedMinimumX = SensorConfirmation.BoundingBoxMinimumX / SourceWidth;
            const double NormalizedMinimumY = SensorConfirmation.BoundingBoxMinimumY / SourceHeight;
            const double NormalizedMaximumX = SensorConfirmation.BoundingBoxMaximumX / SourceWidth;
            const double NormalizedMaximumY = SensorConfirmation.BoundingBoxMaximumY / SourceHeight;
            const FVector2f BoxMinimum = SensorImageOrigin + FVector2f(
                static_cast<float>((NormalizedMinimumX - CropMinimumX) / (CropMaximumX - CropMinimumX)) * SensorImageSize.X,
                static_cast<float>((NormalizedMinimumY - CropMinimumY) / (CropMaximumY - CropMinimumY)) * SensorImageSize.Y);
            const FVector2f BoxMaximum = SensorImageOrigin + FVector2f(
                static_cast<float>((NormalizedMaximumX - CropMinimumX) / (CropMaximumX - CropMinimumX)) * SensorImageSize.X,
                static_cast<float>((NormalizedMaximumY - CropMinimumY) / (CropMaximumY - CropMinimumY)) * SensorImageSize.Y);
            if (BoxMaximum.X > BoxMinimum.X && BoxMaximum.Y > BoxMinimum.Y)
            {
                PaintLine(OutDrawElements, LayerId + 7, AllottedGeometry, BoxMinimum, FVector2f(BoxMaximum.X, BoxMinimum.Y), Amber, 2.0f * UiScale);
                PaintLine(OutDrawElements, LayerId + 7, AllottedGeometry, FVector2f(BoxMaximum.X, BoxMinimum.Y), BoxMaximum, Amber, 2.0f * UiScale);
                PaintLine(OutDrawElements, LayerId + 7, AllottedGeometry, BoxMaximum, FVector2f(BoxMinimum.X, BoxMaximum.Y), Amber, 2.0f * UiScale);
                PaintLine(OutDrawElements, LayerId + 7, AllottedGeometry, FVector2f(BoxMinimum.X, BoxMaximum.Y), BoxMinimum, Amber, 2.0f * UiScale);
            }
            const FVector2f SemanticsOrigin(
                SensorImageOrigin.X,
                SensorImageOrigin.Y + SensorImageSize.Y - 19.0f * UiScale);
            PaintBox(OutDrawElements, LayerId + 7, AllottedGeometry,
                SemanticsOrigin, FVector2f(SensorImageSize.X, 19.0f * UiScale), FLinearColor(0.0f, 0.0f, 0.0f, 0.74f));
            PaintText(OutDrawElements, LayerId + 8, AllottedGeometry,
                SemanticsOrigin + FVector2f(5.0f, 2.0f) * UiScale,
                TEXT("YELLOW OUTLINE = SIM PROJECTION, NOT MODEL OUTPUT"), SmallFont, Amber);
            PaintText(
                OutDrawElements,
                LayerId + 6,
                AllottedGeometry,
                SensorTileOrigin + FVector2f(9.0f, 7.0f) * UiScale,
                FString::Printf(TEXT("LIVE EO/PTZ DIGITAL CROP | %s | CONF %.2f"), *Contact->ReportingNodeId, SensorConfirmation.Confidence),
                SmallFont,
                Green);
        }
        else
        {
            PaintText(
                OutDrawElements,
                LayerId + 6,
                AllottedGeometry,
                SensorTileOrigin + FVector2f(10.0f, 10.0f) * UiScale,
                FString::Printf(TEXT("NEAREST REPORTING SENSOR | %s"), *Contact->ReportingNodeId),
                SmallFont,
                Green);
            PaintText(
                OutDrawElements,
                LayerId + 6,
                AllottedGeometry,
                SensorTileOrigin + FVector2f(18.0f, SensorTileSize.Y * 0.48f),
                TEXT("RF/RADAR CONTACT\nNO FRESH EO CONFIRMATION"),
                BodyFont,
                Muted);
        }
    }
    else
    {
        PaintText(
            OutDrawElements,
            LayerId + 3,
            AllottedGeometry,
            CameraOrigin + FVector2f(CameraSize.X * 0.23f, CameraSize.Y * 0.44f),
            TEXT("WAITING FOR CURRENT SENSOR CONTACT"),
            HeaderFont,
            Muted);
        PaintText(
            OutDrawElements,
            LayerId + 3,
            AllottedGeometry,
            CameraOrigin + FVector2f(CameraSize.X * 0.25f, CameraSize.Y * 0.51f),
            TEXT("No drone view or map position is shown without fresh RF/radar evidence."),
            SmallFont,
            Muted);
    }

    // Right-side minimap and contact explanation.
    PaintBox(OutDrawElements, LayerId + 1, AllottedGeometry, RightOrigin, RightSize, FLinearColor(0.005f, 0.035f, 0.045f, 0.98f));
    PaintText(
        OutDrawElements,
        LayerId + 2,
        AllottedGeometry,
        RightOrigin + FVector2f(12.0f, 10.0f) * UiScale,
        TEXT("SINGAPORE APPROACH MINIMAP"),
        BoldFont,
        Cyan);

    const FVector2f MapOrigin = RightOrigin + FVector2f(14.0f, 40.0f) * UiScale;
    const FVector2f MapSize(RightSize.X - 28.0f * UiScale, RightSize.Y * 0.54f);
    PaintBox(OutDrawElements, LayerId + 2, AllottedGeometry, MapOrigin, MapSize, FLinearColor(0.005f, 0.065f, 0.075f, 1.0f));

    for (int32 GridIndex = 1; GridIndex < 4; ++GridIndex)
    {
        const float Fraction = static_cast<float>(GridIndex) / 4.0f;
        PaintLine(OutDrawElements, LayerId + 3, AllottedGeometry,
            MapOrigin + FVector2f(MapSize.X * Fraction, 0.0f),
            MapOrigin + FVector2f(MapSize.X * Fraction, MapSize.Y),
            FLinearColor(0.10f, 0.24f, 0.27f, 0.65f), 1.0f);
        PaintLine(OutDrawElements, LayerId + 3, AllottedGeometry,
            MapOrigin + FVector2f(0.0f, MapSize.Y * Fraction),
            MapOrigin + FVector2f(MapSize.X, MapSize.Y * Fraction),
            FLinearColor(0.10f, 0.24f, 0.27f, 0.65f), 1.0f);
    }

    const FTRIADSimulationPerimeter& Perimeter = ObserverActor->GetSimulationPerimeter();
    double PerimeterMinimumLongitude = FMath::Min(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees);
    double PerimeterMaximumLongitude = FMath::Max(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees);
    double PerimeterMinimumLatitude = FMath::Min(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees);
    double PerimeterMaximumLatitude = FMath::Max(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees);
    if (TRIAD::Geodesy::IsCircle(Perimeter))
    {
        const FVector2D North = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Perimeter.CenterLongitudeDegrees, Perimeter.CenterLatitudeDegrees, 0.0, Perimeter.RadiusMeters);
        const FVector2D East = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Perimeter.CenterLongitudeDegrees, Perimeter.CenterLatitudeDegrees, 90.0, Perimeter.RadiusMeters);
        const FVector2D South = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Perimeter.CenterLongitudeDegrees, Perimeter.CenterLatitudeDegrees, 180.0, Perimeter.RadiusMeters);
        const FVector2D West = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Perimeter.CenterLongitudeDegrees, Perimeter.CenterLatitudeDegrees, 270.0, Perimeter.RadiusMeters);
        PerimeterMinimumLongitude = West.X;
        PerimeterMaximumLongitude = East.X;
        PerimeterMinimumLatitude = South.Y;
        PerimeterMaximumLatitude = North.Y;
    }
    const double LongitudePadding = FMath::Max((PerimeterMaximumLongitude - PerimeterMinimumLongitude) * 0.15, 0.0005);
    const double LatitudePadding = FMath::Max((PerimeterMaximumLatitude - PerimeterMinimumLatitude) * 0.15, 0.0005);
    const double MinimumLongitude = PerimeterMinimumLongitude - LongitudePadding;
    const double MaximumLongitude = PerimeterMaximumLongitude + LongitudePadding;
    const double MinimumLatitude = PerimeterMinimumLatitude - LatitudePadding;
    const double MaximumLatitude = PerimeterMaximumLatitude + LatitudePadding;
    const auto ToMap = [&](double Longitude, double Latitude)
    {
        const float X = static_cast<float>(FMath::Clamp((Longitude - MinimumLongitude) / (MaximumLongitude - MinimumLongitude), 0.0, 1.0));
        const float Y = 1.0f - static_cast<float>(FMath::Clamp((Latitude - MinimumLatitude) / (MaximumLatitude - MinimumLatitude), 0.0, 1.0));
        return MapOrigin + FVector2f(X * MapSize.X, Y * MapSize.Y);
    };

    TArray<FVector2f> PerimeterPoints;
    if (TRIAD::Geodesy::IsCircle(Perimeter))
    {
        PerimeterPoints.Reserve(65);
        for (int32 PointIndex = 0; PointIndex <= 64; ++PointIndex)
        {
            const FVector2D Point = TRIAD::Geodesy::Wgs84DestinationDegrees(
                Perimeter.CenterLongitudeDegrees,
                Perimeter.CenterLatitudeDegrees,
                static_cast<double>(PointIndex) * 360.0 / 64.0,
                Perimeter.RadiusMeters);
            PerimeterPoints.Add(ToMap(Point.X, Point.Y));
        }
    }
    else
    {
        const FVector2f PerimeterNorthWest = ToMap(PerimeterMinimumLongitude, PerimeterMaximumLatitude);
        const FVector2f PerimeterSouthEast = ToMap(PerimeterMaximumLongitude, PerimeterMinimumLatitude);
        PerimeterPoints = {
            PerimeterNorthWest,
            FVector2f(PerimeterSouthEast.X, PerimeterNorthWest.Y),
            PerimeterSouthEast,
            FVector2f(PerimeterNorthWest.X, PerimeterSouthEast.Y),
            PerimeterNorthWest};
    }
    PaintPolyline(OutDrawElements, LayerId + 4, AllottedGeometry, PerimeterPoints, Amber, 2.0f * UiScale);
    PaintText(OutDrawElements, LayerId + 5, AllottedGeometry,
        PerimeterPoints[0] + FVector2f(6.0f, 5.0f) * UiScale,
        TEXT("SIMULATION PERIMETER"), SmallFont, Amber);

    for (const FTRIADGeodeticSensorNode& Node : ObserverActor->GetMinimapNodes())
    {
        const bool bSelectedNode = Contact && Node.NodeId == Contact->ReportingNodeId;
        const FVector2f Point = ToMap(Node.LongitudeDegrees, Node.LatitudeDegrees);
        PaintCircle(
            OutDrawElements,
            LayerId + 5,
            AllottedGeometry,
            Point,
            (bSelectedNode ? 7.0f : 4.0f) * UiScale,
            bSelectedNode ? Amber : Cyan,
            bSelectedNode ? 3.0f : 2.0f);
    }

    if (bFreshContact && Contact)
    {
        const FTRIADGeodeticSensorNode* SelectedNode = ObserverActor->GetMinimapNodes().FindByPredicate(
            [Contact](const FTRIADGeodeticSensorNode& Node)
            {
                return Node.NodeId == Contact->ReportingNodeId;
            });

        if (Contact->bHasRadarPositionEstimate)
        {
            const FVector2f TargetPoint = ToMap(Contact->EstimatedLongitudeDegrees, Contact->EstimatedLatitudeDegrees);
            if (SelectedNode)
            {
                PaintLine(
                    OutDrawElements,
                    LayerId + 5,
                    AllottedGeometry,
                    ToMap(SelectedNode->LongitudeDegrees, SelectedNode->LatitudeDegrees),
                    TargetPoint,
                    Green,
                    2.0f * UiScale);
            }

            TArray<FVector2f> TrailPoints;
            for (const FVector2D& LongitudeLatitude : ObserverActor->GetSelectedRadarTrail())
            {
                TrailPoints.Add(ToMap(LongitudeLatitude.X, LongitudeLatitude.Y));
            }
            PaintPolyline(OutDrawElements, LayerId + 5, AllottedGeometry, TrailPoints, Amber, 2.0f * UiScale);
            if (TrailPoints.Num() >= 2)
            {
                const FVector2f Direction = (TrailPoints.Last() - TrailPoints[TrailPoints.Num() - 2]).GetSafeNormal();
                if (!Direction.IsNearlyZero())
                {
                    const FVector2f ArrowTip = TargetPoint + Direction * 20.0f * UiScale;
                    const FVector2f ArrowSide(-Direction.Y, Direction.X);
                    PaintLine(OutDrawElements, LayerId + 6, AllottedGeometry, TargetPoint, ArrowTip, Amber, 3.0f * UiScale);
                    PaintLine(OutDrawElements, LayerId + 6, AllottedGeometry, ArrowTip, ArrowTip - Direction * 7.0f * UiScale + ArrowSide * 5.0f * UiScale, Amber, 3.0f * UiScale);
                    PaintLine(OutDrawElements, LayerId + 6, AllottedGeometry, ArrowTip, ArrowTip - Direction * 7.0f * UiScale - ArrowSide * 5.0f * UiScale, Amber, 3.0f * UiScale);
                }
            }
            PaintCircle(OutDrawElements, LayerId + 6, AllottedGeometry, TargetPoint, 8.0f * UiScale, Amber, 3.0f * UiScale);
            PaintLine(OutDrawElements, LayerId + 6, AllottedGeometry,
                TargetPoint - FVector2f(10.0f, 0.0f) * UiScale,
                TargetPoint + FVector2f(10.0f, 0.0f) * UiScale,
                Amber, 2.0f * UiScale);
            PaintLine(OutDrawElements, LayerId + 6, AllottedGeometry,
                TargetPoint - FVector2f(0.0f, 10.0f) * UiScale,
                TargetPoint + FVector2f(0.0f, 10.0f) * UiScale,
                Amber, 2.0f * UiScale);
        }

        // Other currently radar-positioned contacts remain visible as small points.
        for (const FTRIADOperatorObservedContact& Other : ObserverActor->GetCurrentContacts())
        {
            if (Other.ContactId != Contact->ContactId && Other.bHasRadarPositionEstimate)
            {
                PaintCircle(
                    OutDrawElements,
                    LayerId + 5,
                    AllottedGeometry,
                    ToMap(Other.EstimatedLongitudeDegrees, Other.EstimatedLatitudeDegrees),
                    3.0f * UiScale,
                    FLinearColor(1.0f, 0.3f, 0.12f, 0.9f),
                    2.0f);
            }
        }
    }

    PaintText(OutDrawElements, LayerId + 6, AllottedGeometry,
        MapOrigin + FVector2f(MapSize.X - 20.0f * UiScale, 6.0f * UiScale), TEXT("N"), BoldFont, FLinearColor::White);

    const FVector2f DetailOrigin = RightOrigin + FVector2f(14.0f, RightSize.Y * 0.59f);
    if (bFreshContact && Contact)
    {
        const FString PositionText = Contact->bHasRadarPositionEstimate
            ? TEXT("RADAR POSITION ESTIMATE - SOLID MAP MARKER")
            : TEXT("RF-ONLY CONTACT - PRECISE MAP POSITION UNAVAILABLE");
        const FString RoleText = Contact->bAuthoredAttackScenario
            ? TEXT("AUTHORED EXERCISE ROLE: ATTACKER - NOT INFERRED")
            : TEXT("AUTHORED EXERCISE ROLE: OTHER - NOT INFERRED");
        const FString RadarText = Contact->bDetectedBySearchRadar
            ? FString::Printf(TEXT("RADAR CONFIDENCE  %.2f"), Contact->RadarConfidence)
            : TEXT("RADAR  NOT DETECTED");
        const FString RFText = Contact->bDetectedByRF
            ? FString::Printf(TEXT("RF MAX SNR  %.1f dB"), Contact->MaximumRFSnrDb)
            : TEXT("RF  NOT DETECTED");
        const TArray<TPair<FString, FLinearColor>> DetailLines = {
            {FString::Printf(TEXT("CONTACT  %s"), *Contact->ContactId), FLinearColor::White},
            {FString::Printf(TEXT("CURRENT OBSERVATION  %s"), *Contact->SensorSummary), Green},
            {FString::Printf(TEXT("NEAREST REPORTING NODE  %s"), *Contact->ReportingNodeId), Cyan},
            {FString::Printf(TEXT("REPORTED RANGE  %s"), *FormatRange(Contact->ReportedRangeMeters)), FLinearColor::White},
            {RadarText, Contact->bDetectedBySearchRadar ? Green : Muted},
            {RFText, Contact->bDetectedByRF ? Green : Muted},
            {FString::Printf(TEXT("WEATHER  %s | SIMULATED ENVIRONMENT"), *Contact->WeatherProfile), Cyan},
            {PositionText, Contact->bHasRadarPositionEstimate ? Green : Amber},
            {FString::Printf(TEXT("SIMULATION ROUTE  %s | %s"), *Contact->IngressCorridorId, *Contact->AirspaceState), Muted},
            {RoleText, Amber},
            {TEXT("Observer camera follows actor truth for presentation only."), Muted}
        };
        float TextY = 0.0f;
        for (const TPair<FString, FLinearColor>& Line : DetailLines)
        {
            PaintText(
                OutDrawElements,
                LayerId + 6,
                AllottedGeometry,
                DetailOrigin + FVector2f(0.0f, TextY),
                Line.Key,
                TextY < 1.0f ? BoldFont : SmallFont,
                Line.Value);
            TextY += 22.0f * UiScale;
        }
    }
    else
    {
        PaintText(OutDrawElements, LayerId + 6, AllottedGeometry,
            DetailOrigin, TEXT("NO CURRENT SENSOR DETECTION"), BoldFont, Muted);
        PaintText(OutDrawElements, LayerId + 6, AllottedGeometry,
            DetailOrigin + FVector2f(0.0f, 28.0f) * UiScale,
            TEXT("The panel fails closed and does not reveal scenario targets."), SmallFont, Muted);
    }

    return LayerId + 7;
}
