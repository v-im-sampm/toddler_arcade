# Asset inventory (exported from the original Canvas sketch)

The generated [manifest](generated/manifest.json) names each crop and
byte size; this summary groups the **228 PNG + BGRA8888 pairs**.

| Group | Frames | Source draw function | Crop |
|---|---:|---|---|
| Static backdrop, start overlay, chrome | 4 | `drawBackdrop`, `drawStart`, `drawChrome` | 320x240 / 48x48 |
| Fish (3 species x 2 faces x 3 moods x 6 tail poses) | 108 | `drawFish`, including original fins and face | 96x80 |
| Visitors (3 species x 6 poses) | 18 | `drawVisitor` | 112x88 |
| Back/front seaweed (4 regions x 6 poses) | 24 | `drawSeaweed` | 36x75–62x100 |
| Rays, caustics, surface (3 layers x 6 poses) | 18 | `drawRays`, `drawCaustics`, `drawSurface` | 320x14–320x144 |
| Buttons (4 icons x 2 light states x 6 poses) | 48 | `drawButtons` | 48x48 |
| Chest (4 lid angles) | 4 | `drawChest` | 64x56 |
| Bubble, pellet, fish shadow, helper hand | 4 | `drawBubbles`, `drawPellets`, `drawShadows`, `drawHelperHand` | 24x24–60x22 |

Build-time command: `node v2\native\bubble_aquarium\tools\export_assets.mjs`.
BGRA8888 is a direct LVGL image pixel buffer; PNGs are inspectable proofs of
the original artwork. No runtime JS or HTML is included in the executable.
