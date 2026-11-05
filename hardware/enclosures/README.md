# Yellow carry-handle enclosure

`yellow_handle.scad` is a parametric OpenSCAD model of the yellow
"carry-handle explorer" concept. It is editable source, not a measured or
print-validated fit. All dimensions are in millimeters, and every parameter is at the top
of the file.

## Design

- **Pillow body:** 124 x 88 x 36 mm with 20 mm plan corners and 9 mm rounded
  edges on every face, so there are no sharp corners or edges. Wall thickness is
  2.4 mm. The body splits into a front and a back shell at `seam_z` (15 mm). A
  2 mm alignment lip on the back shell keys into the front shell.
- **Handle:** chunky and rounded, 74 mm wide, rising 31 mm above the body.
  The finger opening is 46 x 18 mm and its edges are rounded so a small hand
  can grip it comfortably.
- **Screen bezel (purple):** a separate raised frame, 2.4 mm proud, with a
  chamfered window. It is glued into a 0.8 mm pocket around the screen
  window. Printing it separately keeps the front shell flat for face-down
  printing. Set `bezel_separate = false` to merge it into the shell; that
  version needs supports.
- **Side bumpers (purple):** ribbed, chamfered grip pads on the left and right
  sides. They are part of the shell geometry. `part="bumpers"` exports only
  the pads, so they can be loaded as a color-modifier volume (Bambu Studio /
  OrcaSlicer) for a two-color print.
- **Speaker grille:** four horizontal slots to the viewer's right of the screen,
  with a retainer for the 15 x 10 mm speaker behind them. The mockup's mic dot
  is omitted because the parts list includes no microphone module.
- **Case screws:** four M2 x 12 self-tapping screws. They go in from the back
  through counterbored bosses and thread 6 mm into pilot holes in the
  front-shell bosses at (+/-48, +/-30).

Model coordinates: the front face is at z = 0, the back at z = 36, and the
handle points to +y. Seen from the front, +x is on the viewer's **left**.

## Internal layout

- **Display:** the MaTouch sits face-down on a lip around the screen window.
  The display's USB-C is intentionally covered; there is no opening for it.
- **Battery:** a 503450 single-cell LiPo pouch lies flat in a rear-shell tray
  directly behind the display, offset left. Front-shell ribs (outside the display
  footprint) stop 1.5 mm (`battery_hold_down_gap`) above the pouch, so it is
  never clamped and has room to swell. Use a thin foam pad, not glue, if it
  rattles. Never puncture, bend, or compress the pouch.
- **Charger:** a TP4057 1 A USB-C board sits in a cradle at the center of the
  bottom edge, raised on a platform so that its USB-C port (center
  `usb_c_center_z` = 22) lands on the flat band of the bottom wall behind the
  seam. The board has no mounting holes. Its USB-C end butts against the wall,
  which takes the plug-insertion load; add a dab of hot glue on a rail. The
  wall opening hugs the receptacle (8.94 x 3.26 mm plus clearance). An outer
  12.8 x 7.2 mm recess leaves a 0.8 mm lip so common cable overmolds seat
  fully. **This is the only external USB-C.**
- **MAX98357A and INA boards:** these mount on rear-shell standoffs on the
  speaker side, at (-42, 14) and (-41, -14).

The model's asserts fail the render if any of these happen:

- A battery rib overlaps the display footprint.
- The USB-C recess leaves the flat wall band or crosses the seam.
- The case is too shallow for the battery stack.

## Exporting

OpenSCAD 2021.01 is too slow on this model. Use a recent OpenSCAD
development snapshot (`winget install OpenSCAD.OpenSCAD.Nightly`) with the
Manifold backend; each part then renders in seconds. Run these from the
repository root:

```powershell
$scad = "C:\Program Files\OpenSCAD (Nightly)\openscad.com"
& $scad --backend=manifold -o front.stl  -D 'part="front"'  hardware\enclosures\yellow_handle.scad
& $scad --backend=manifold -o back.stl   -D 'part="back"'   hardware\enclosures\yellow_handle.scad
& $scad --backend=manifold -o bezel.stl  -D 'part="bezel"'  hardware\enclosures\yellow_handle.scad
& $scad --backend=manifold -o bumpers.stl -D 'part="bumpers"' hardware\enclosures\yellow_handle.scad
```

In the GUI, set `part` near the top of the file and render with F6. Leave it
as `"preview"` to see the assembled enclosure with ghosted display, battery,
and charger.

## Printing notes

- Print the front shell face-down, the back shell back-down, and the bezel
  flat-back-down.
- The rounded pillow edges meet the bed at a steep angle. Use a brim, and
  expect slight sag or use light tree supports on the first few millimeters.
- The handle overhang inside the finger opening needs supports.
- Prefer PETG or PLA+ for drop toughness. Choose filament colors yourself;
  nothing in the model assumes a particular material.

## Exports

[`exports/`](exports/) contains renders and STEP files generated from the
current SCAD parameters:

- `yellow_handle_assembled.png`, `yellow_handle_assembled_back.png`
- `yellow_handle_front_outside.png`, `yellow_handle_front_inside.png`
- `yellow_handle_back_outside.png`, `yellow_handle_back_inside.png` (the
  back interior, with the battery and charger ghosted)
- `yellow_handle_bezel.png`
- `yellow_handle_front.step`, `yellow_handle_back.step`,
  `yellow_handle_bezel.step`

OpenSCAD cannot write STEP files. These were converted from the rendered STL
meshes with OpenCascade (`cadquery-ocp`):

1. Sew the mesh.
2. Make a solid from the sewn shell.
3. Merge coplanar triangles.

Each file re-reads as one valid closed solid, but it is **faceted**: round
features are polygons, not true arcs. Use the STEP files to import the parts
into other CAD tools. Make real design changes in `yellow_handle.scad`, then
regenerate the exports; they go stale whenever the model changes.

## Power-path blocker (not modeled)

A 1S LiPo supplies 3.0-4.2 V. The MaTouch makes its 3.3 V with an LD1117
fed from VBUS through diode D3 (see `config/board_pins.json`), so it needs
about 5 V. Its headers expose only 3V3/GND. Two parts are therefore needed:

- A 5 V boost converter between TP4057 OUT+/OUT- and the MaTouch.
- A power switch (preferred).

Neither part is chosen or modeled yet. How 5 V reaches the MaTouch is
**TBD**: either an internal USB-C pigtail or a VBUS solder point. Do not
guess. Record the evidence in `docs/hardware.md` once it is decided. Wire the
battery to TP4057 B+/B-, and verify the pad labels on the board in hand.

## Fit status and measurements to replace

The repository has no dimensioned mechanical drawing for the MaTouch PCB,
and the MAX, INA, charger, and battery dimensions are not established. The
interfaces use editable placeholder values. Do not print a fit-critical
enclosure until the parts are measured and the parameters are updated.

Measure and update:

- **MaTouch PCB:** width and height (placeholder 62 x 46), the active display
  opening, and the **total stack depth** (glass, PCB, and rear components).
  Display retention is **TBD**: the lip holds the board, but nothing yet
  presses it forward from behind. Add standoffs or a foam pad once the stack
  depth is known.
- **TP4057 board:** length and width (placeholder 19.5 x 13.8), PCB
  thickness, the USB-C offset from the board centerline, and the height of
  the solder joints underneath.
- **503450 battery:** the bay uses seller dimensions (34 x 50 x 5). Confirm
  the fit, including the protection-circuit end and the lead bend radius.
- **MAX and INA boards:** mounting-hole pitch and board positions.
- **Speaker:** housing depth (placeholder 4 mm) and wire exit direction.
- **Envelope and fasteners:** check the overall envelope and handle against a
  child's grip and your print bed, and tune the screw, boss, and clearance
  parameters to your printer and fasteners.

No hardware fit, electrical, acoustic, print, or physical-device validation
has been performed.
