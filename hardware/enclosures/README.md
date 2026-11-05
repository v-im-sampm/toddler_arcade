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
- **Display mounts:** four M3 screws pass through the MaTouch corner holes
  into heat-set inserts in bosses on the front shell. The mounts are at a
  60.3 x 54.5 mm pitch, rotated from the portrait orientation in the supplied
  dimension image to match the landscape opening. The image suggests the
  screen is centered on the board, but the PCB outline and board revision are
  not specified; the 3.1 mm board-edge margin is only an image estimate.

Model coordinates: the front face is at z = 0, the back at z = 36, and the
handle points to +y. Seen from the front, +x is on the viewer's **left**.

## Internal layout

- **Display:** the MaTouch sits face-down on a lip around the screen window.
  The display's USB-C is intentionally covered; there is no opening for it.
  Four M3 screws pass through its corner holes into heat-set inserts in bosses
  on the front shell. Insert pilot size, insert length, screw length, PCB
  outline, and board stack depth remain placeholders to verify against the
  exact board and insert in hand.
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
- **MAX98357A and INA boards:** these mount on rear-shell standoffs in the
  free area behind the display, between the speaker side and the battery tray:
  MAX at (-15, 20), INA at (-15, -6). Board outlines are the hole pitch plus
  `pcb_board_margin` on each side (placeholder).
- **Power switch:** a KCD1 snap-in rocker sits in a **recessed well on the
  back face**, speaker side, centered at `switch_center` = (-41, 0). The rocker
  stays below the well rim, so a parent can reach it with a fingertip, but
  bumps, small hands, or laying the case on its back are unlikely to toggle
  it. It is a deterrent, not a lock. The switch body hangs inside, over the
  shallow speaker area, clear of the display. The top and bottom walls were
  rejected because their flat band is shorter than the 13 x 19 mm cutout.
  Wire the switch first, then snap it in from outside. It is intended to sit
  in series between TP4057 OUT+ and the (TBD) 5 V boost input.

The model's asserts fail the render if any of these happen:

- A battery rib overlaps the display footprint or its mount bosses.
- The USB-C recess leaves the flat wall band or crosses the seam.
- The case is too shallow for the battery stack.
- The MAX or INA board hits the battery tray, charger, switch well, the other
  board, or the display stack (`display_stack_depth`).
- The switch well leaves the flat back face, or hits the battery tray or a
  case boss.
- The switch body hits the display footprint or the speaker.
- The rocker sticks out above the well rim.

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
as `"preview"` to see the assembled enclosure with ghosted components. Use
`part="layout"` to see the back shell with every internal component in color.

[`yellow_handle_views.scad`](yellow_handle_views.scad) produces the labeled
layout and section renders. Always pass `-D 'part="none"'` so the model's own
preview isn't drawn too:

```powershell
$v = "hardware\enclosures\yellow_handle_views.scad"
& $scad --backend=manifold -D 'part="none"' -D 'view="layout_iso"' --camera=0,0,0,70,0,-20,0 --viewall --autocenter --imgsize=1400,1050 -o layout_back.png $v
& $scad --backend=manifold -D 'part="none"' -D 'view="plan"' --camera=0,0,0,0,180,0,0 --projection=o --viewall --autocenter --imgsize=1400,1200 -o layout_plan.png $v
& $scad --backend=manifold --render -D 'part="none"' -D 'view="section_y"' --camera=0,0,0,90,0,180,0 --projection=o --viewall --autocenter --imgsize=1400,700 -o section_y0.png $v
& $scad --backend=manifold --render -D 'part="none"' -D 'view="section_x"' --camera=0,0,0,90,0,90,0 --projection=o --viewall --autocenter --imgsize=1400,700 -o section_x0.png $v
```

Other views: `front_layout`, `back_out`, `back_in`, `assembled`.

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
- `yellow_handle_front_outside.png`, `yellow_handle_front_inside.png` (four
  display insert bosses visible inside)
- `yellow_handle_back_outside.png`, `yellow_handle_back_inside.png` (the
  back interior, with the battery and charger ghosted)
- `yellow_handle_bezel.png`
- `yellow_handle_layout_back.png`, `yellow_handle_layout_front.png`: each
  shell with its components in color
- `yellow_handle_layout_plan.png`: labeled plan of every component, seen
  from the front; projected overlap with the rear components is intentional
  because the display is mounted on the front shell
- `yellow_handle_section_y0.png`, `yellow_handle_section_x0.png`: labeled
  cross-sections showing component depths
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
- A power switch: a KCD1 rocker, modeled in the back-face well.

The boost converter is not chosen or modeled yet. How 5 V reaches the MaTouch is
**TBD**: either an internal USB-C pigtail or a VBUS solder point. Do not
guess. Record the evidence in `docs/hardware.md` once it is decided. Wire the
battery to TP4057 B+/B-, and verify the pad labels on the board in hand.

## Fit status and measurements to replace

The repository has no dimensioned mechanical drawing for the MaTouch PCB,
and the MAX, INA, charger, and battery dimensions are not established. The
interfaces use editable placeholder values. Do not print a fit-critical
enclosure until the parts are measured and the parameters are updated.

Measure and update:

- **MaTouch PCB:** the supplied dimension image shows a 54.5 x 60.3 mm
  corner-hole pitch in portrait orientation and 3.2 mm holes; this model
  rotates that pattern for landscape and places four M3 insert mounts below
  the PCB. The outline estimate uses a 3.1 mm edge margin; confirm the pitch,
  screen offset, PCB outline, board revision, stack depth, and mount fit on
  the physical board. `display_insert_pilot_diameter`, `display_insert_length`,
  and `display_mount_screw_length` are placeholders; choose an insert and
  verify screw engagement before printing.
- **TP4057 board:** length and width (placeholder 19.5 x 13.8), PCB
  thickness, the USB-C offset from the board centerline, and the height of
  the solder joints underneath.
- **503450 battery:** the bay uses seller dimensions (34 x 50 x 5). Confirm
  the fit, including the protection-circuit end and the lead bend radius.
- **MAX and INA boards:** mounting-hole pitch, board outline
  (`pcb_board_margin`), and component height. The standoffs assume the
  display stack is no deeper than `display_stack_depth` (placeholder 8 mm).
- **KCD1 switch:** KCD1 dimensions vary by maker. The model uses typical
  listing values: 13 x 19 mm cutout, 15 x 21 mm flange, about 7 mm rocker
  height, about 18 mm below the panel including tabs, and a 0.6-2 mm snap-in
  panel (well floor 1.6 mm). Measure the switch in hand and confirm that the
  snap tabs grip the floor.
- **Display inserts:** choose an M3 heat-set insert, then measure its pilot
  diameter and length. Confirm the 8 mm screw length and adjust engagement
  after measuring the PCB thickness and mount height.
- **Speaker:** housing depth (placeholder 4 mm) and wire exit direction.
- **Envelope and fasteners:** check the overall envelope and handle against a
  child's grip and your print bed, and tune the screw, boss, and clearance
  parameters to your printer and fasteners.

No hardware fit, electrical, acoustic, print, or physical-device validation
has been performed.
