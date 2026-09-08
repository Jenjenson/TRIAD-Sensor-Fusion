# Istana vegetation visual targets

This note fixes the public-reference visual target for the V5D environment. It
does not claim a botanical survey, a current tree census, or exact placement of
individual protected-site vegetation.

## Public-reference target

- The primary approach should read as a mature tropical avenue dominated by
  lofty, broad umbrella crowns. Singapore's National Heritage Board describes
  the Rain Trees lining the Istana main gate as *Samanea saman*, planted in the
  1970s, with umbrella-shaped crowns that may reach roughly 25 m.
- The grounds should also contain rounded, dense heritage-tree silhouettes. An
  NParks heritage-tree record identifies a 23.2 m Tamarind on the Istana grounds
  and describes its crown as round.
- Palms and lower ornamental planting belong around formal gardens and edges,
  while the main lawn should remain visibly manicured and open enough to retain
  the real site's ceremonial character.

Public references:

- [National Heritage Board: Iconic Trees in Singapore's Civic District](https://www.roots.gov.sg/stories-landing/stories/iconic-trees-in-singapores-civic-district)
- [NParks Heritage Trees: Tamarind at the Istana](https://heritagetrees.nparks.gov.sg/ht-2018-295/)
- [The Istana: The Grounds](https://www.istana.gov.sg/visit-and-explore/the-grounds/)

## Current source mapping

The current five-form roster is directionally suitable for that public target:

| Runtime form | Source family | Intended visual role |
| --- | --- | --- |
| Umbrella | Poly Haven `island_tree_02` derivative | Rain-Tree-like avenue massing and broad shade canopy |
| Dense dome | Poly Haven `tree_small_02` derivative | Rounded secondary and heritage-canopy variation |
| High fork | Poly Haven `island_tree_01` derivative | Mature open-trunk silhouettes and canopy-height variation |
| Columnar narrow | Poly Haven Jacaranda derivative | Vertical rhythm and non-umbrella diversity away from the main avenue |
| Palm | Existing palm derivative | Formal-garden and ornamental accents |

These are high-resolution source-derived meshes, not low-polygon placeholders.
Their remaining weakness is repeated silhouette and material response, not a
lack of source polygon density. No current form should be presented as an exact
species model of a particular Istana tree.

## Native and prepared realism changes

- The committed R30-19 runtime tree pass permits source LOD 0 near the viewer
  rather than forcing LOD 1.
- R30-19 retains automatic LOD selection, with a `1.8` instance LOD-distance
  scale to hold branch and leaf silhouette detail farther from the camera.
- The source-only R32 successor adds 4,608 deterministic modeled turf clusters
  across twelve profile and silhouette buckets, fading from 65 m and ending at
  90 m. This is intended to make grass structure readable without a close-up
  camera, but it is not visible in the native map.

The R30 tree behavior is native but has no accepted Player0 visual capture. The
latest run09 capture failed closed before review and rolled back cleanly. R32
turf remains source-prepared and cannot be promoted until R30 is explicitly
human-accepted and the ordered successor gates are satisfied.

## Native visual acceptance

The tree and turf pass is acceptable only when reviewed at fixed Player0 views:

1. 2 m and 8 m: bark, branch forks, leaf cards, blade height, and material
   response do not read as oversized, flat, or plastic.
2. 20 m: individual tree forms remain distinguishable and turf still has a
   visible modeled silhouette.
3. 65 m, 75 m, 90 m, and 95 m: turf transitions smoothly into the underlying
   lawn without a bald band, hard cutoff, moire, or obvious repeating carrier
   pattern.
4. Main approach and oblique surroundings: umbrella crowns create the intended
   mature tropical avenue while dome, high-fork, columnar, and palm forms prevent
   obvious cloning.
5. Performance is measured on the approved target-hardware manifest; visual
   acceptance alone cannot authorize a regression outside that budget.

If these views still expose obvious tree cloning after the LOD and R32 turf
changes are natively captured, the next asset action is a bounded additional
tropical-canopy variant. It must be evaluated for silhouette value before being
added; downloading another large mesh merely to increase polygon count is not
an acceptance criterion.
