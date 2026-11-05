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
- **Speaker port (viewer's right):** a small 1 W speaker cannot be made much
  louder by a printed horn, because its wavelengths are far larger than any
  shape that fits here. The port aims for the gains that are actually
  available:
  - **Sealed front.** The 15 x 10 mm oval sits portrait in a stadium
    retainer, and a 0.5 mm double-sided foam gasket (`speaker_gasket_thickness`)
    seals its rim to the skin. The sealed case interior becomes the speaker's
    back box. This stops the rear wave from cancelling the front wave, which
    is the main source of lost bass and volume in small toys.
  - **Open grille.** A 0.6 mm front chamber spans the whole cone. Three
    1.6 mm slots with 1.0 mm bars (`speaker_grille_*`) give at least 45%
    open area, which an assert enforces. The slots are narrow enough to keep
    fingers and pencils off the cone.
  - **Flared mouth.** A 0.8 mm-deep stadium dish flares 3 mm per side into
    the face (`speaker_dish_*`). It acts as a short waveguide and a visual
    cue for where the sound comes from.

  The speaker's tab points to +y. The 80 mm lead exits through a 5 mm notch
  at the -y end; run a bead of hot glue or sealant around the notch so the
  back box stays sealed.
- **Mic port (viewer's left):** see the INMP441 entry under Internal layout.
- **Case screws:** four M2 x 12 self-tapping screws. They go in from the back
  through counterbored bosses and thread 6 mm into pilot holes in the
  front-shell bosses at (+/-48, +/-30).
- **Display mounts:** four M2.5 screws pass through the MaTouch's 2.85 mm
  corner holes into heat-set inserts in bosses on the front shell. The hole
  pitch is measured on the board: 54.2 x 48.6 mm in landscape. The outline is
  58.8 x 53.0 mm: the 54.5 x 60.3 mm drawing outline printed about 1.5 mm too
  large in each direction. The board's USB-C end faces the speaker side (-x).
  The 2.6 mm screen offset away from the USB-C end (`display_screen_offset`)
  is still an image estimate.

Model coordinates: the front face is at z = 0, the back at z = 36, and the
handle points to +y. Seen from the front, +x is on the viewer's **left**.

## Internal layout

- **Display:** the MaTouch sits face-down on a lip around the screen window.
  The display's USB-C is intentionally covered; there is no opening for it.
  Four M2.5 screws pass through its corner holes into heat-set inserts in bosses
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
- **MAX98357A:** mounts on rear-shell standoffs in the free area behind the
  display, between the speaker side and the battery tray, at (-15, 20). The
  board is 17.8 mm along the 7-pin header and 18.8 mm from the header edge to
  the terminal block (`max_board_size`). It has only two holes, both at the
  terminal-block end, about 13.1 mm apart and 2.3 mm in from that edge
  (`max_hole_pitch`, `max_hole_inset`, photo estimates). These take M2 screws
  into 4.2 mm standoffs. Two solid rest posts (`max_rest_inset`) support the
  header end. **Solder the header (11.2 mm, `max_header_height`) and the
  terminal block on the component side, facing the display**, or wire directly
  to the pads. Pins pointing toward the back skin would not fit on the
  standoffs. Terminal-block height and depth are placeholders.
- **INMP441 mic (the "INA" board, `config/board_pins.json`):** a circular
  14 mm board, in a front-shell pocket at `mic_center` =
  (45, 0), on the viewer's left, mirroring the speaker. The INMP441 is **bottom-ported**: its sound hole goes through the
  PCB under the mic can. Mount it as follows:
  - Seat it with the **bare pinhole side facing the front skin**, so the mic
    can and header face into the case.
  - Put a foam gasket ring between the board and the skin, with the ring's
    hole over the port. `mic_underside_clearance` = 1.6 mm leaves room for
    the gasket and trimmed pin stubs.
  - The board rests on a ledge (`mic_ledge_inner_diameter`). Glue it at the
    rim.
  - A pry/wire notch faces the display.
  - The front face has a 2 mm acoustic hole in a 5 mm countersunk "mic dot".

  Check which side your board's port is on before assembly. The gasket keeps
  case-internal noise, including the speaker's back wave, out of the mic.
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
- The MAX board hits the battery tray, charger, switch well, or the display
  stack (`display_stack_depth`). Its front-facing header or terminal block
  comes within 1 mm of the display stack, or its holes sit too close to the
  board edge.
- A display mounting hole sits closer to the board edge than its radius.
- The mic pocket leaves the flat face or hits the display, a case boss, or a
  battery rib. The mic ledge is not narrower than the board.
- The speaker dish leaves the flat face or reaches the bezel. Grille bars are
  thinner than 0.8 mm, there are fewer than two slots, or the open area is
  below 45% of the cone. The retainer hits a case boss.
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

Three scripts regenerate everything in `exports/`. Run them from the
repository root:

```powershell
# All PNG renders, each with a fixed camera.
powershell -File hardware\enclosures\render_exports.ps1

# STEP files: render STLs, then convert them (needs `pip install cadquery-ocp`).
& $scad --backend=manifold -o front.stl -D 'part="front"' hardware\enclosures\yellow_handle.scad
& $scad --backend=manifold -o back.stl  -D 'part="back"'  hardware\enclosures\yellow_handle.scad
python hardware\enclosures\stl_to_step.py front.stl hardware\enclosures\exports\yellow_handle_front.step back.stl hardware\enclosures\exports\yellow_handle_back.step

# 1:1 printable fit-check PDF (needs `pip install reportlab`).
python hardware\enclosures\make_fit_check_pdf.py
```

Views in `yellow_handle_views.scad`: `assembled`, `front_out`, `front_in`,
`back_out`, `back_in`, `layout_iso`, `front_layout`, `plan`, `section_y`,
`section_x`.

## Printable fit check

`exports/yellow_handle_fit_check.pdf` is a six-page US Letter set drawn at
**exactly 1:1** from the model's own 2D sections. Print it at **100% /
Actual size** with "fit to page" turned off. Then measure the 100 mm and
4 in bars on every page; if they are off, the printer scaled the page.

1. Front face, outside: overall outline, screen window, speaker dish and
   slots, mic hole, and the bezel.
2. Front shell, inside: lay the MaTouch, speaker, and INMP441 on their
   outlines and the display-hole crosshairs.
3. Back shell, inside: battery tray, TP4057 cradle, MAX98357A standoffs, and
   the KCD1 cutout.
4. Back face, outside: switch well and screw counterbores.
5. Bottom-wall section with the USB-C opening, and the side profile.
6. Component templates for every assumed part size, labeled with the
   parameter to change if the real part doesn't match.

Paper checks outlines and hole patterns only, not depths. Print a thin test
slice of the shell before printing it in full.

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
- `yellow_handle_fit_check.pdf`: 1:1 printable fit check (see above)

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
and the MAX, charger, and battery dimensions are not established. The
interfaces use editable placeholder values. Do not print a fit-critical
enclosure until the parts are measured and the parameters are updated.

Measure and update:

- **MaTouch PCB:** hole pitch 54.2 x 48.6 and hole diameter 2.85 are
  measured. The 58.8 x 53.0 outline (`display_board_*`) comes from the drawing
  outline less the ~1.5 mm a print check showed. Confirm the outline, the
  screen offset (`display_screen_offset`, 2.6 mm image estimate), the board
  revision, the stack depth, and the mount fit on the physical board.
  `display_insert_pilot_diameter`, `display_insert_length`,
  and `display_mount_screw_length` are placeholders; choose an insert and
  verify screw engagement before printing.
- **TP4057 board:** length and width (placeholder 19.5 x 13.8), PCB
  thickness, the USB-C offset from the board centerline, and the height of
  the solder joints underneath.
- **503450 battery:** the bay uses seller dimensions (34 x 50 x 5). Confirm
  the fit, including the protection-circuit end and the lead bend radius.
- **MAX98357A board:** the outline (17.8 x 18.8) comes from the supplied
  drawing. The hole pitch (13.1), the hole inset (2.3), the terminal-block
  height and depth (`max_terminal_height`, `max_terminal_size`), and the rest-post
  positions are estimates or placeholders. The standoffs assume the
  display stack is no deeper than `display_stack_depth` (placeholder 8 mm).
- **KCD1 switch:** KCD1 dimensions vary by maker. The model uses typical
  listing values: 13 x 19 mm cutout, 15 x 21 mm flange, about 7 mm rocker
  height, about 18 mm below the panel including tabs, and a 0.6-2 mm snap-in
  panel (well floor 1.6 mm). Measure the switch in hand and confirm that the
  snap tabs grip the floor.
- **Display inserts:** choose an M2.5 heat-set insert, then measure its pilot
  diameter (placeholder 3.6) and length (placeholder 4). Confirm the 6 mm screw
  length and adjust engagement after measuring the PCB thickness and mount
  height.
- **Speaker:** housing depth (`speaker_depth`, placeholder 4 mm), the tab
  size (`speaker_tab_*`), and the frame width around the cone
  (`speaker_cone_inset`). The 15 x 10 mm oval and the tab on one end come
  from the supplied photo.
- **INMP441:** port position relative to the board center (`mic_port_offset`,
  placeholder [0, 0]), the gasket and pin-stub stack
  (`mic_underside_clearance`), and the solder-pad ring that the ledge may
  touch (`mic_ledge_inner_diameter`). The 14 mm circular board outline comes
  from the supplied photo.
- **Envelope and fasteners:** check the overall envelope and handle against a
  child's grip and your print bed, and tune the screw, boss, and clearance
  parameters to your printer and fasteners.

No hardware fit, electrical, acoustic, print, or physical-device validation
has been performed.
