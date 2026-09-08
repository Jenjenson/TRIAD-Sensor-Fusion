# Istana public-view acceptance gates

These gates test a photoreal public-reference reconstruction. Passing them does
not authorize the words `as-built`, `survey`, `digital twin`, `exact`, or
`one-to-one`.

## Rights and isolation

| Gate | Required result |
| --- | --- |
| Provenance | 100% of source photographs, textures, geometry and map data have file-level source, author, licence, hash and permitted-use records |
| Restricted content | Zero restricted capture, copied official-site pixels, interiors, concealed systems, security equipment, guard positions, access-control details or restricted routes |
| Provider data | Google/Cesium content is used only through the supported live integration, retains attribution and is not extracted into project assets |
| Map isolation | Only `/Game/Maps/Istana_PublicView_Exterior_v1` is created or saved; hashes of `SDTH` and both earlier Istana maps remain unchanged |
| Namespace isolation | New content remains under `/Game/TRIAD/IstanaPublicView`; neither existing Istana mesh nor the locked digital-twin import/build path is called or mutated |

## Reference and geometry

| Gate | Required result |
| --- | --- |
| View coverage | At least 6 modelling views and 3 held-out views spanning the front, both front obliques and architectural detail |
| Landmark fit | Held-out median reprojection error no greater than 0.75% of image diagonal and P95 no greater than 1.5% |
| Silhouette | Building-mask IoU at least 0.97 in the centred front and at least 0.93 in both held-out obliques |
| Public constraints | Horizontal envelope remains within 2% of its mapping reference and the tower target remains within 1% of 28 m |
| Hero parallax | Every named visible facade element produces the expected parallax at 10–25 m; no flat facade projection is accepted |
| Walk-around | A complete hero orbit shows no missing backside, texture swimming, z-fighting, floating plinth, exposed provider duplicate or camera-facing vegetation |

## Ground, context and vegetation

| Gate | Required result |
| --- | --- |
| Ground coverage | 100% of the inner one-kilometre circle has a Landscape or authored terrain-mesh visual surface |
| Flat-source rejection | Zero visible aerial-photo ground planes, flat fallback disks, image backdrops or billboard terrain inside the circle |
| Vegetation | Zero full-tree billboards, crossed-plane crowns or camera-facing tree impostors inside the circle |
| Volume | Every accepted tree has non-zero 3D trunk and crown bounds; hero trees also have major branch volume |
| Seams | No visible terrain holes or overlaps; engineered local seams have no greater than 2 cm positional gap in engine space |
| Local collision | Regression traces hit authored terrain/building collision and never hidden provider geometry or the flat fallback disk in normal mode |
| OSM context isolation | The visible ODbL building massing is `NoCollision`, ignores every trace channel, generates no overlaps, and is never navigation, target, occlusion, or sensor truth |
| Duplicate context | The frozen 180-box synthetic fallback remains imported/validated but is hidden and inactive whenever OSM massing is selected |
| OSM claim boundary | Coordinate alignment may pass while visual/photo acceptance remains explicitly false; mapping-grade footprints and approximate heights are not cadastral or survey evidence |

## Materials, lighting and images

| Gate | Required result |
| --- | --- |
| Colour state | Fixed exposure, fixed white balance and one documented display/OCIO transform are read back in Editor and PIE |
| Highlight retention | Less than 0.5% of the facade mask is clipped at display white in the neutral QA profile |
| Material hierarchy | Warm rendered plaster, paler trim, darker ivory/taupe louvres and charcoal slate remain visually separable in neutral daylight |
| Image similarity | After locked camera/crop/exposure alignment, modelling views achieve SSIM at least 0.90 and LPIPS at most 0.12; held-out views achieve SSIM at least 0.86 and LPIPS at most 0.16 |
| Editor/PIE parity | The same settled camera achieves SSIM at least 0.98 between Editor and PIE, with no fog obstruction |
| Artifact rejection | No local exposure, depth of field, motion blur, bloom, vignette or chromatic aberration during scientific comparison |

SSIM and LPIPS are supporting measurements, not proof of geometric truth. The
silhouette, landmark, holdout and walk-around gates remain mandatory.

## Runtime and streaming

| Gate | Required result |
| --- | --- |
| Representative run | 30 minutes at 1920x1080 with no missing World Partition cells, unloaded hero material or terminal LOD pop on the approved route |
| Performance | Average at least 30 fps, 1% low at least 24 fps, GPU P95 no greater than 30 ms, game/render-thread P95 no greater than 22 ms |
| Memory | Peak GPU memory no greater than 10 GiB and process RAM no greater than 24 GiB, with no texture/Nanite streaming overflow |
| Weather | Deterministic acceptance weather is read back after BeginPlay; fog cannot obscure any required camera |
| Camera | Player 0 uses the tagged public-view camera and its finite cached POV matches the camera component before capture |

## Human photographic test

Use at least 20 reviewers who did not build the scene. Randomize matched real
photographs and Unreal renders, remove filenames and metadata, and do not tell a
reviewer how many images of each class are present. The public-view claim may
add `difficult to distinguish from photography at the validated views` only
when reviewer accuracy is no greater than 60% and the 95% confidence interval
includes 50%.

That statement applies solely to the named camera, epoch, lighting and output
profile. It must not be generalized to arbitrary viewpoints or to simulation
accuracy.

## Automatic rejection

Reject the release if any of the following is true:

- an input lacks an allowlisted licence or provenance record;
- a renderer-facing asset uses a protected/reference-only image as texture;
- a public photograph is projected to fake facade depth;
- a full tree becomes an image plane or camera-facing card within one kilometre;
- an aerial image is the visible accepted ground;
- a held-out camera, oblique silhouette, 360-degree orbit or collision trace is
  omitted;
- a failed measurement is removed from the report;
- the build overwrites an existing map or asset; or
- the release is described as exact, one-to-one, survey-controlled or an
  as-built digital twin.
