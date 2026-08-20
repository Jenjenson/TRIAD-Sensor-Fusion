#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class ATRIADOperatorObserverActor;

/** Full-screen, input-transparent overlay painted from live observer state. */
class STRIADOperatorSlateWidget final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(STRIADOperatorSlateWidget) {}
        SLATE_ARGUMENT(TWeakObjectPtr<ATRIADOperatorObserverActor>, Observer)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

private:
    TWeakObjectPtr<ATRIADOperatorObserverActor> Observer;
    mutable FSlateBrush ObserverImageBrush;
    mutable FSlateBrush SensorImageBrush;
};
