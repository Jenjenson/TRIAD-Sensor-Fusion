# Istana Explore V5D dynamic-range realism contract

## Outcome

The V5D hybrid map gains one appearance-only dynamic-range actor. Its single
unbound post-process component preserves highlight separation while lifting
crushed foliage and lawn shadows. A restrained global grade removes the prior
green-biased oversaturation without changing the inherited 80,000-lux
clear-day lighting or the V5 pawn's neutral physical camera profile.

## Composition order

In UE 5.5, world post-process volumes are blended before the main camera. The
V5 camera intentionally overrides local exposure to neutral at full weight.
The actor therefore owns a transient camera modifier that resubmits the same
component settings with `VTBlendOrder_Override`. The modifier never changes
the point of view, FOV, camera component, or physical exposure values.

## Frozen values

| Setting | Value |
| --- | ---: |
| Post-process components owned | 1 |
| Priority / blend weight | 6400 / 1.0 |
| Highlight contrast | 0.66 |
| Shadow contrast | 0.58 |
| Detail strength | 1.01 |
| Blurred luminance blend / kernel | 0.55 / 50% |
| Middle-grey bias | 0.0 |
| Global saturation / contrast | 0.970 / 0.990 |

No LUT, curve asset, custom blendable, auto-exposure, exposure bias, physical
camera, white-balance, bloom, material, or emissive override is admitted.

## Builder integration

Include:

```cpp
#include "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.h"
```

Before the hybrid builder's first destination save, call:

```cpp
FString DynamicRangeReport;
if (!UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary::
        ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(
            Target,
            true,
            DynamicRangeReport))
{
    // Abort the atomic build. Do not save a partial destination map.
}
```

The target-world validator should require exactly one
`ATRIADIstanaExploreV5DDynamicRangeRealismActor` and call
`ValidateDynamicRangeRealism(false, Report)`. The PIE validator should call
`ValidateDynamicRangeRealism(true, Report)` after runtime activation.

## Claim boundary

This layer is look development only. It does not establish current weather,
survey/as-built truth, display calibration, measured camera response, material
truth, or collision/navigation/sensor/RF authority.
