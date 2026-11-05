// Parametric enclosure for the Little Arcade "Carry-Handle Explorer" (yellow concept #4).
// Pillow-shaped body, rounded tube handle, raised screen bezel and ribbed side grips.
// Dimensions are millimeters. Confirm all placeholder dimensions against the actual parts.

$fn = 40;

// "preview", "front", "back", "bezel" (separate purple part, print-oriented),
// "bumpers" (grip pads only, for use as a color modifier in a multi-material slicer), or
// "layout" (back shell plus colored stand-ins for every internal component).
part = "preview";

// Overall pillow-shaped body
body_width = 124;
body_height = 88;
body_depth = 36;
plan_corner_radius = 20; // Corner radius seen from the front.
edge_radius = 9; // Rounding of every front and back edge.
wall = 2.4;
seam_z = 15;
// Alignment lip on the back shell that slides inside the front shell.
seam_lip_height = 2;
seam_lip_thickness = 1.2;
seam_lip_clearance = 0.2;

// Rounded carry handle
handle_width = 74;
handle_rise = 31; // Above the top of the body.
handle_depth = 22;
handle_corner_radius = 13;
handle_edge_radius = 6;
handle_root_depth = 12; // How far the handle block is buried in the body.
handle_opening_width = 46;
handle_opening_height = 18;
handle_opening_radius = 8;
handle_flare_radius = 4; // Rounds the finger edges of the handle opening.

// Display opening and board-retaining lip
screen_center_x = 0;
screen_center_y = 2;
screen_window_width = 51.2;
screen_window_height = 38.4;
screen_window_radius = 4;
// The supplied Makerfabs dimension image gives the overall PCB outline, mounting ears included:
// 54.5 x 60.3 mm in portrait. This enclosure keeps the display landscape (rotated 90 degrees)
// with the board's USB-C/FPC end toward the speaker (-x). The outline is modeled as its
// bounding rectangle. Hole insets and the screen offset are measured from that image at
// ~8.55 px/mm; confirm them on the physical board.
display_board_width = 60.3;
display_board_height = 54.5;
display_hole_inset = [2.95, 2.85]; // Image estimate: board edge to hole center, per axis.
display_hole_pitch = [
    display_board_width - 2 * display_hole_inset[0],
    display_board_height - 2 * display_hole_inset[1]
];
display_hole_diameter = 3.2;
// Image estimate: the active screen sits 2.6 mm from the board center, away from the USB-C end.
display_screen_offset = [2.6, 0];
display_board_center = [
    screen_center_x - display_screen_offset[0],
    screen_center_y - display_screen_offset[1]
];
display_lip_depth = 1.4;
display_lip_overlap = 1.2;
display_pcb_thickness = 1.6; // Placeholder; measure the board.
display_mount_boss_diameter = 7.5;
display_insert_pilot_diameter = 4.2; // Placeholder; match this to the chosen M3 insert.
display_insert_length = 4.5; // Placeholder; match this to the chosen M3 insert.
display_mount_screw_length = 8; // Placeholder; confirm engagement with the chosen insert.

// Raised screen bezel. Separate by default so the front shell prints flat on its face;
// glue it into the shallow pocket. Set false to merge it into the front shell (needs supports).
bezel_separate = true;
bezel_border = 9;
bezel_corner_radius = 10;
bezel_height = 2.4; // Proud of the front face.
bezel_edge_radius = 1.6;
bezel_window_chamfer = 1.6;
bezel_pocket_depth = 0.8;
bezel_pocket_clearance = 0.2;

// Ribbed side grips (bumpers) on the left and right sides.
bumper_length = 60;
bumper_center_y = -4;
bumper_z_inset = 4; // From the front and back faces.
bumper_wrap = 12; // How far the grip reaches around from the side.
bumper_base = 1.2; // Pad thickness between ribs.
bumper_rib_height = 2.4;
bumper_rib_count = 5;
bumper_rib_width = 5;
bumper_rib_pitch = 10.5;
bumper_chamfer = 1.6;

// Charging board: Teyleten Robot TP4057 1A USB-C 1S charger with protection.
// Listing size ~19.5 x 13.8 mm; it has no mounting holes, so it sits in a cradle with its
// USB-C receptacle pressed against the bottom wall. Measure the board in hand and update.
charger_board_length = 19.5; // Along Y, USB-C end to B+/B- pad end.
charger_board_width = 13.8;
charger_pcb_thickness = 1.0;
charger_bottom_clearance = 1.6; // Pedestal height for solder joints under the PCB.
charger_port_center_x = 0;
charger_cradle_wall = 1.4;
charger_cradle_clearance = 0.3;
charger_cradle_lip = 1.2; // Rail height above the PCB's component face.
charger_wire_notch_width = 8;
// Port height from the front face. Keep it on the flat band of the bottom wall
// (edge_radius .. body_depth - edge_radius) and behind the seam; a platform raises the board.
usb_c_center_z = 22;
// USB-C receptacle shell (USB-IF nominal 8.94 x 3.26 mm) and the opening around it.
usb_c_receptacle_width = 8.94;
usb_c_receptacle_height = 3.26;
usb_c_opening_clearance = 0.35;
usb_c_corner_radius = 1.4;
// Outer recess so cable overmolds (~12 x 6.5 mm) can seat fully against a thin lip.
usb_c_plug_recess_width = 12.8;
usb_c_plug_recess_height = 7.2;
usb_c_lip_thickness = 0.8;

// Speaker: 15 x 10 mm oval (stadium) micro speaker with a mounting tab on one end and an
// 80 mm twisted lead, per the supplied image. It faces the front, stood upright (long axis
// along y). Model x runs toward the viewer's left when facing the screen, so negative x is
// on the right.
// Acoustics: a tiny driver loses most of its output when the rear wave leaks around the rim
// and cancels the front wave. A gasket seals the rim to the front skin so the case interior
// becomes the back box, a shallow front chamber spans the whole cone, the slots give at
// least ~50% open area over the cone, and a flared dish on the face acts as a short
// waveguide that eases the sound out and broadens it toward the child.
speaker_center_x = -45;
speaker_center_y = 0;
speaker_width = 10; // Short axis, along x.
speaker_height = 15; // Long axis, along y.
speaker_depth = 4; // Placeholder: measure the speaker housing including its wire exit.
speaker_tab_length = 3; // Placeholder: tab beyond the oval at the +y end.
speaker_tab_width = 5; // Placeholder.
speaker_tab_thickness = 0.5; // Placeholder.
speaker_cone_inset = 1; // Placeholder: frame width around the radiating cone.
speaker_gasket_thickness = 0.5; // Double-sided foam ring that seals the rim and holds it.
speaker_clearance = 0.3;
speaker_retainer_thickness = 1.2;
speaker_retainer_depth = speaker_gasket_thickness + speaker_depth;
speaker_wire_notch_width = 5; // Lead exit at the -y end of the retainer.
speaker_front_chamber_depth = 0.6; // Recess in the skin over the cone, linking every slot.
speaker_grille_slot_width = 1.6; // Small enough to keep fingers and pencils off the cone.
speaker_grille_bar_width = 1.0;
speaker_dish_depth = 0.8; // Flared mouth recessed into the face.
speaker_dish_flare = 3; // Mouth grows by this much per side from the cone opening.

// Rear-shell MAX98357A mount behind the display, components (terminal block and header) facing
// the front. Outline from the supplied dimension image: 17.8 mm along the header (x) by 18.8 mm
// from the header edge to the terminal block (y, terminal block toward +y). Two mounting holes
// sit at the terminal-block end; plain rest posts support the header end. Hole pitch and inset
// are estimated from the top-view photo at ~18.9 px/mm; confirm them on the board.
pcb_standoff_height = 4.2;
pcb_standoff_diameter = 5.2;
pcb_rest_diameter = 4;
m2_clearance_diameter = 2.3;
pcb_thickness = 1.6;
max_board_center = [-15, 20];
max_board_size = [17.8, 18.8];
max_hole_pitch = 13.1; // Image estimate, along x.
max_hole_inset = 2.3; // Image estimate: terminal-block edge to hole centers.
max_rest_inset = 5.5; // Header edge to rest posts; clears the header solder stubs.
max_header_height = 11.2; // Header pins on the terminal-block side (from the dimension image).
max_terminal_height = 10; // Placeholder: screw terminal height above the PCB.
max_terminal_size = [8.75, 7.5]; // x from the dimension image; y is a placeholder.

// Microphone: INMP441 I2S MEMS breakout (the "INA" module), circular 14 mm PCB
// per the supplied image. The INMP441 is bottom-ported: its sound
// hole goes through the PCB under the mic can. The board sits flat in a front-shell pocket
// opposite the speaker (far from it, to limit echo pickup), with the pinhole side facing a
// small hole in the front face and a foam gasket ring sealing between them.
mic_center = [45, 0]; // Viewer's left, mirroring the speaker.
mic_board_diameter = 14;
mic_pcb_thickness = 1.0;
mic_port_offset = [0, 0]; // Port (under the mic can) relative to board center. Placeholder.
mic_underside_clearance = 1.6; // Trimmed pin stubs + compressed foam gasket. Placeholder.
mic_header_height = 9; // Header pins and wires above the board, for clearance checks.
mic_hole_diameter = 2.0;
mic_dot_diameter = 5; // Shallow countersunk "mic dot" around the hole on the front face.
mic_dot_depth = 0.6;
mic_pocket_clearance = 0.3;
mic_pocket_wall = 1.2;
// Ledge under the board's round edge sets the gasket squeeze; keep it outside the pin pads.
mic_ledge_inner_diameter = 12.4; // Placeholder: check against the pad ring on the board.
mic_wire_notch_width = 8;
mic_wire_notch_angle = 180; // Pry/wire notch direction; 180 points toward the display.

// Power switch: KCD1-101 style snap-in mini rocker. Values are typical listing/datasheet
// figures that vary by maker; measure the switch in hand. It sits in a recessed well in the
// back face on the speaker side: a parent can reach it, but the rocker stays below the
// surface so bumps or laying the case on its back cannot toggle it.
switch_center = [-41, 0];
switch_cutout_x = 13; // Panel cutout 19 x 13; long side along y so the rocker tips up/down.
switch_cutout_y = 19;
switch_flange_x = 15; // Front flange 21 x 15.
switch_flange_y = 21;
switch_flange_thickness = 1.5;
switch_rocker_proud = 7; // Flange plus rocker above the panel face.
switch_body_depth = 13; // Housing behind the panel.
switch_below_panel_depth = 18; // Housing plus tabs with soldered wires.
switch_panel_thickness = 1.6; // Snap-in clips typically grip 0.6-2 mm panels.
switch_well_clearance = 3; // Fingertip room around the flange.
switch_well_wall = 1.6;
switch_well_corner_radius = 4;
switch_well_depth = switch_rocker_proud + 0.5;
switch_well_chamfer = 1;

// Display module stack behind the lip (glass + LCD + PCB + parts). Placeholder used only for
// the layout ghost and clearance asserts; measure the MaTouch board in hand.
display_stack_depth = 8;

// Case screws: M2 self-tapping, inserted from the back into counterbored bosses so the
// heads sit below the surface.
case_screw_x = 48;
case_screw_y = 30;
case_screw_boss_diameter = 7.2;
case_screw_clearance_diameter = 2.4;
case_screw_pilot_diameter = 1.7;
case_screw_head_diameter = 4.4;
case_screw_length = 12;
case_screw_engagement = 6; // Thread length in the front boss.

// Battery: 503450 1S LiPo pouch, 3.7 V 1000 mAh, 34 x 50 x 5 mm per the seller drawing
// (50 mm includes the protection-board end; leads exit that end). Lies flat on the rear skin
// behind the display, lead end toward the bottom (charging-board) edge.
battery_pack_width = 34;
battery_pack_length = 50;
battery_pack_thickness = 5;
battery_center = [24.5, 8];
battery_tray_wall = 1.6;
battery_tray_clearance = 1.0;
battery_tray_lip_height = 4;
battery_lead_notch_width = 12;
// Pouch cells swell with age; never clamp them. Front-shell ribs stop this far above the
// pouch, leaving room for a thin foam pad.
battery_hold_down_gap = 1.5;
// Ribs [center x, center y, size x, size y]; they must miss the display board footprint.
battery_hold_down_ribs = [[37, -13.5, 2, 7], [37, 18, 2, 12], [37, 29.5, 2, 2]];

fuse = 0.4; // Overlap so internal features merge with the shell skin.
front_skin = wall;
rear_skin = wall;
body_top = body_height / 2;
body_bottom = -body_height / 2;
inside_back_z = body_depth - rear_skin;
inside_bottom_y = body_bottom + wall;
// Charger PCB rests component-side-front; the receptacle sits on that face.
charger_pcb_front_z = usb_c_center_z + usb_c_receptacle_height / 2;
charger_pcb_back_z = charger_pcb_front_z + charger_pcb_thickness;
charger_floor_z = charger_pcb_back_z + charger_bottom_clearance;
charger_rail_front_z = charger_pcb_front_z - charger_cradle_lip;
handle_block_height = handle_rise + handle_root_depth;
handle_center_y = body_top + (handle_rise - handle_root_depth) / 2;
handle_front_z = (body_depth - handle_depth) / 2;
handle_back_z = handle_front_z + handle_depth;
handle_opening_cut_height = handle_opening_height + handle_root_depth;
handle_opening_center_y = body_top + (handle_opening_height - handle_root_depth) / 2;
fastener_x_positions = [-case_screw_x, case_screw_x];
fastener_y_positions = [-case_screw_y, case_screw_y];
case_screw_head_z = seam_z - case_screw_engagement + case_screw_length;
bezel_outer_width = screen_window_width + 2 * bezel_border;
bezel_outer_height = screen_window_height + 2 * bezel_border;
switch_well_width = switch_flange_x + 2 * switch_well_clearance;
switch_well_height = switch_flange_y + 2 * switch_well_clearance;
switch_panel_back_z = body_depth - switch_well_depth; // Face the flange rests on.
switch_panel_front_z = switch_panel_back_z - switch_panel_thickness;
switch_body_front_z = switch_panel_front_z - switch_below_panel_depth;
pcb_board_back_z = inside_back_z - pcb_standoff_height;
max_hole_positions = [
    for (sx = [-1, 1]) [max_board_center[0] + sx * max_hole_pitch / 2, max_board_center[1] + max_board_size[1] / 2 - max_hole_inset]
];
max_rest_positions = [
    for (sx = [-1, 1]) [max_board_center[0] + sx * max_hole_pitch / 2, max_board_center[1] - max_board_size[1] / 2 + max_rest_inset]
];
pcb_board_front_z = pcb_board_back_z - pcb_thickness;
mic_pocket_inner_diameter = mic_board_diameter + 2 * mic_pocket_clearance;
mic_pocket_outer_diameter = mic_pocket_inner_diameter + 2 * mic_pocket_wall;
mic_board_front_z = front_skin + mic_underside_clearance;
mic_pocket_top_z = mic_board_front_z + mic_pcb_thickness;
mic_port_center = mic_center + mic_port_offset;
speaker_cone_width = speaker_width - 2 * speaker_cone_inset;
speaker_cone_height = speaker_height - 2 * speaker_cone_inset;
speaker_mouth_width = speaker_cone_width + 2 * speaker_dish_flare;
speaker_mouth_height = speaker_cone_height + 2 * speaker_dish_flare;
speaker_grille_pitch = speaker_grille_slot_width + speaker_grille_bar_width;
speaker_grille_slot_count = floor((speaker_cone_width + speaker_grille_bar_width) / speaker_grille_pitch);
speaker_grille_bar_thickness = front_skin - speaker_dish_depth - speaker_front_chamber_depth;
speaker_retainer_outer_width = speaker_width + 2 * (speaker_clearance + speaker_retainer_thickness);
speaker_retainer_outer_height =
    speaker_height + speaker_tab_length + 2 * (speaker_clearance + speaker_retainer_thickness);
display_stack_back_z = front_skin + display_lip_depth + display_stack_depth;
display_mount_top_z = display_stack_back_z - display_pcb_thickness;

module rounded_rect_2d(width, height, radius) {
    r = min(radius, min(width, height) / 2 - 0.01);
    offset(r = r)
        square([width - 2 * r, height - 2 * r], center = true);
}

// Convex box from z = 0 to depth: plan corners of corner_r, front/back edges rounded by edge_r.
module rounded_box(width, height, depth, corner_r, edge_r) {
    e = min(edge_r, depth / 2 - 0.01);
    c = max(corner_r, e + 0.01);
    hull()
        for (sx = [-1, 1], sy = [-1, 1], z = [e, depth - e])
            translate([sx * (width / 2 - c), sy * (height / 2 - c), z])
                rotate_extrude()
                    intersection() {
                        translate([c - e, 0]) circle(r = e);
                        translate([0, -e]) square([c, 2 * e]);
                    }
}

// The body surface offset outward by t (inward when negative); exact for this shape.
module body_offset(t) {
    translate([0, 0, -t])
        rounded_box(
            body_width + 2 * t,
            body_height + 2 * t,
            body_depth + 2 * t,
            plan_corner_radius + t,
            edge_radius + t
        );
}

module body_outer() {
    body_offset(0);
}

module inner_cavity() {
    body_offset(-wall);
}

module inner_outline_2d() {
    rounded_rect_2d(body_width - 2 * wall, body_height - 2 * wall, plan_corner_radius - wall);
}

module handle_opening_layer(grow, z) {
    translate([0, handle_opening_center_y, z])
        linear_extrude(height = 0.01)
            rounded_rect_2d(
                handle_opening_width + 2 * grow,
                handle_opening_cut_height + 2 * grow,
                handle_opening_radius + grow
            );
}

// Through-opening with quarter-round finger edges on both faces.
module handle_opening_cut() {
    f = handle_flare_radius;
    steps = 6;
    function grow(s) = f - sqrt(f * f - (f - s) * (f - s));
    hull() {
        handle_opening_layer(0, handle_front_z + f);
        handle_opening_layer(0, handle_back_z - f);
    }
    for (i = [0 : steps - 1]) {
        s0 = f * i / steps;
        s1 = f * (i + 1) / steps;
        hull() {
            handle_opening_layer(grow(s0), handle_front_z + s0);
            handle_opening_layer(grow(s1), handle_front_z + s1);
        }
        hull() {
            handle_opening_layer(grow(s0), handle_back_z - s0);
            handle_opening_layer(grow(s1), handle_back_z - s1);
        }
    }
    hull() {
        handle_opening_layer(f, handle_front_z - 2);
        handle_opening_layer(f, handle_front_z + 0.01);
    }
    hull() {
        handle_opening_layer(f, handle_back_z - 0.01);
        handle_opening_layer(f, handle_back_z + 2);
    }
}

module handle() {
    difference() {
        translate([0, handle_center_y, handle_front_z])
            rounded_box(
                handle_width,
                handle_block_height,
                handle_depth,
                handle_corner_radius,
                handle_edge_radius
            );
        handle_opening_cut();
    }
}

// Region for the right-hand grip, chamfered so the pad edges are not sharp steps.
module bumper_region() {
    x_in = body_width / 2 - bumper_wrap;
    x_side = body_width / 2;
    x_out = body_width / 2 + bumper_rib_height + 0.5;
    z1 = bumper_z_inset;
    z2 = body_depth - bumper_z_inset;
    c = bumper_chamfer;
    y0 = bumper_center_y - bumper_length / 2;
    hull() {
        translate([x_in, y0, z1]) cube([0.01, bumper_length, z2 - z1]);
        translate([x_side, y0, z1]) cube([0.01, bumper_length, z2 - z1]);
        translate([x_out, y0 + c, z1 + c]) cube([0.01, bumper_length - 2 * c, z2 - z1 - 2 * c]);
    }
}

module bumper_rib_bands() {
    c = bumper_chamfer * 0.75;
    for (i = [0 : bumper_rib_count - 1]) {
        y = bumper_center_y + (i - (bumper_rib_count - 1) / 2) * bumper_rib_pitch;
        hull() {
            translate([body_width / 2 - bumper_wrap, y - bumper_rib_width / 2, -1])
                cube([0.01, bumper_rib_width, body_depth + 2]);
            translate([body_width / 2, y - bumper_rib_width / 2, -1])
                cube([0.01, bumper_rib_width, body_depth + 2]);
            translate([body_width / 2 + bumper_rib_height + 0.5, y - bumper_rib_width / 2 + c, -1])
                cube([0.01, bumper_rib_width - 2 * c, body_depth + 2]);
        }
    }
}

// Raised grip pads only (outside the body surface), both sides.
module bumper_pads() {
    difference() {
        for (side = [0, 1])
            mirror([side, 0, 0]) {
                intersection() {
                    body_offset(bumper_base);
                    bumper_region();
                }
                intersection() {
                    body_offset(bumper_rib_height);
                    bumper_region();
                    bumper_rib_bands();
                }
            }
        body_outer();
    }
}

module bezel_outline_2d(grow = 0) {
    translate([screen_center_x, screen_center_y])
        rounded_rect_2d(
            bezel_outer_width + 2 * grow,
            bezel_outer_height + 2 * grow,
            bezel_corner_radius + grow
        );
}

// Bezel in assembly position: proud of the front face (z < 0), seated in a pocket.
module bezel() {
    total = bezel_height + bezel_pocket_depth;
    e = bezel_edge_radius;
    c = bezel_corner_radius;
    ch = bezel_window_chamfer;
    difference() {
        translate([screen_center_x, screen_center_y, -bezel_height])
            hull()
                for (sx = [-1, 1], sy = [-1, 1])
                    translate([sx * (bezel_outer_width / 2 - c), sy * (bezel_outer_height / 2 - c), 0]) {
                        translate([0, 0, e])
                            rotate_extrude()
                                translate([c - e, 0])
                                    circle(r = e);
                        translate([0, 0, e])
                            cylinder(r = c, h = total - e);
                    }
        translate([screen_center_x, screen_center_y, 0]) {
            hull() {
                translate([0, 0, -bezel_height + ch])
                    linear_extrude(height = total + 1)
                        rounded_rect_2d(screen_window_width, screen_window_height, screen_window_radius);
                translate([0, 0, -bezel_height - 0.01])
                    linear_extrude(height = 0.01)
                        rounded_rect_2d(
                            screen_window_width + 2 * ch,
                            screen_window_height + 2 * ch,
                            screen_window_radius + ch
                        );
            }
            translate([0, 0, -bezel_height - 1])
                linear_extrude(height = 1)
                    rounded_rect_2d(
                        screen_window_width + 2 * ch,
                        screen_window_height + 2 * ch,
                        screen_window_radius + ch
                    );
        }
    }
}

module display_lip() {
    difference() {
        translate([display_board_center[0], display_board_center[1], front_skin - fuse])
            linear_extrude(height = display_lip_depth + fuse)
                rounded_rect_2d(display_board_width, display_board_height, 3);
        translate([screen_center_x, screen_center_y, front_skin - fuse - 0.05])
                linear_extrude(height = display_lip_depth + fuse + 0.1)
                    rounded_rect_2d(
                        screen_window_width + 2 * display_lip_overlap,
                        screen_window_height + 2 * display_lip_overlap,
                        2
                    );
    }
}

module stadium_2d(width, height) rounded_rect_2d(width, height, min(width, height) / 2);

// Acoustic opening: flared dish on the face, slotted grille bars, and a front chamber
// behind the bars that spans the whole cone.
module speaker_grille() {
    translate([speaker_center_x, speaker_center_y, 0]) {
        hull() {
            translate([0, 0, -0.01])
                linear_extrude(height = 0.01) stadium_2d(speaker_mouth_width, speaker_mouth_height);
            translate([0, 0, speaker_dish_depth])
                linear_extrude(height = 0.01) stadium_2d(speaker_cone_width, speaker_cone_height);
        }
        translate([0, 0, -1])
            linear_extrude(height = front_skin + 2)
                intersection() {
                    stadium_2d(speaker_cone_width, speaker_cone_height);
                    for (i = [0 : speaker_grille_slot_count - 1])
                        translate([(i - (speaker_grille_slot_count - 1) / 2) * speaker_grille_pitch, 0])
                            square([speaker_grille_slot_width, speaker_cone_height + 2], center = true);
                }
        translate([0, 0, front_skin - speaker_front_chamber_depth])
            linear_extrude(height = speaker_front_chamber_depth + 0.01)
                stadium_2d(speaker_cone_width, speaker_cone_height);
    }
}

module speaker_outline_2d(grow) {
    stadium_2d(speaker_width + 2 * grow, speaker_height + 2 * grow);
    translate([0, speaker_height / 2 + speaker_tab_length / 2 - 0.5])
        square([speaker_tab_width + 2 * grow, speaker_tab_length + 1 + 2 * grow], center = true);
}

// Snug wall around the oval and tab; the gasket under the rim does the sealing.
module speaker_retainer() {
    translate([speaker_center_x, speaker_center_y, front_skin - fuse])
        linear_extrude(height = speaker_retainer_depth + fuse)
            difference() {
                speaker_outline_2d(speaker_clearance + speaker_retainer_thickness);
                speaker_outline_2d(speaker_clearance);
                translate([0, -speaker_height / 2])
                    square([speaker_wire_notch_width, 4 * speaker_retainer_thickness], center = true);
            }
}

// Locating ring and edge ledge for the mic board, with a pry/wire notch.
module mic_pocket() {
    module notch() rotate(mic_wire_notch_angle)
        translate([0, -mic_wire_notch_width / 2]) square([mic_pocket_outer_diameter, mic_wire_notch_width]);
    translate([mic_center[0], mic_center[1], front_skin - fuse]) {
        linear_extrude(height = mic_pocket_top_z - front_skin + fuse)
            difference() {
                circle(d = mic_pocket_outer_diameter);
                circle(d = mic_pocket_inner_diameter);
                notch();
            }
        linear_extrude(height = mic_board_front_z - front_skin + fuse)
            difference() {
                circle(d = mic_pocket_inner_diameter + 0.1);
                circle(d = mic_ledge_inner_diameter);
                notch();
            }
    }
}

module mic_port_cut() {
    translate([mic_port_center[0], mic_port_center[1], 0]) {
        translate([0, 0, -1])
            cylinder(h = front_skin + 2, d = mic_hole_diameter);
        translate([0, 0, -0.01])
            cylinder(h = mic_dot_depth + 0.01, d1 = mic_dot_diameter + 2 * mic_dot_depth, d2 = mic_dot_diameter);
    }
}

module usb_c_cutout() {
    // Through-opening hugging the receptacle shell.
    translate([charger_port_center_x, body_bottom - 1, usb_c_center_z])
        rotate([-90, 0, 0])
            linear_extrude(height = wall + 2)
                rounded_rect_2d(
                    usb_c_receptacle_width + 2 * usb_c_opening_clearance,
                    usb_c_receptacle_height + 2 * usb_c_opening_clearance,
                    usb_c_corner_radius
                );
    // Outer recess leaves only usb_c_lip_thickness so the plug seats fully.
    translate([charger_port_center_x, body_bottom - 1, usb_c_center_z])
        rotate([-90, 0, 0])
            linear_extrude(height = 1 + wall - usb_c_lip_thickness)
                rounded_rect_2d(usb_c_plug_recess_width, usb_c_plug_recess_height, 2.5);
}

charger_pocket_width = charger_board_width + 2 * charger_cradle_clearance;
charger_pocket_length = charger_board_length + charger_cradle_clearance;
charger_outer_width = charger_pocket_width + 2 * charger_cradle_wall;
charger_outer_length = charger_pocket_length + charger_cradle_wall;

// Platform from the rear skin, side rails and an end stop around the charger PCB. The USB-C
// end butts the bottom wall, which takes the plug-insertion load. Secure the board with a dot
// of hot glue on a rail.
module charger_cradle() {
    translate([charger_port_center_x, 0, 0]) {
        translate([-charger_outer_width / 2, inside_bottom_y - fuse, charger_floor_z])
            cube([charger_outer_width, charger_outer_length + fuse, inside_back_z - charger_floor_z + fuse]);
        translate([0, 0, charger_rail_front_z])
            linear_extrude(height = charger_floor_z - charger_rail_front_z + fuse)
                difference() {
                    translate([-charger_outer_width / 2, inside_bottom_y - fuse])
                        square([charger_outer_width, charger_outer_length + fuse]);
                    translate([-charger_pocket_width / 2, inside_bottom_y - 1])
                        square([charger_pocket_width, charger_pocket_length + 1]);
                    // Wire exit for B+/B- and OUT+/OUT- at the far end.
                    translate([-charger_wire_notch_width / 2, inside_bottom_y + charger_pocket_length - 1])
                        square([charger_wire_notch_width, charger_cradle_wall + 2]);
                }
        // Pedestal pads at the PCB corners clear the underside solder joints.
        for (sx = [-1, 1], y = [inside_bottom_y + 1, inside_bottom_y + charger_pocket_length - 4])
            translate([sx * (charger_pocket_width / 2 - 1.5) - 1.5, y, charger_pcb_back_z])
                cube([3, 3, charger_bottom_clearance + fuse]);
    }
}

module charger_ghost() {
    translate([charger_port_center_x - charger_board_width / 2, inside_bottom_y, charger_pcb_front_z])
        cube([charger_board_width, charger_board_length, charger_pcb_thickness]);
    translate([charger_port_center_x, inside_bottom_y, usb_c_center_z])
        rotate([-90, 0, 0])
            linear_extrude(height = 7.4)
                rounded_rect_2d(usb_c_receptacle_width, usb_c_receptacle_height, 1.6);
}

module front_case_bosses() {
    for (x = fastener_x_positions, y = fastener_y_positions)
        difference() {
            translate([x, y, front_skin - fuse])
                cylinder(h = seam_z - front_skin + fuse, d = case_screw_boss_diameter);
            translate([x, y, seam_z - case_screw_engagement - 1])
                cylinder(h = case_screw_engagement + 1.1, d = case_screw_pilot_diameter);
        }
}

module back_case_bosses() {
    for (x = fastener_x_positions, y = fastener_y_positions)
        translate([x, y, seam_z])
            cylinder(h = inside_back_z - seam_z + fuse, d = case_screw_boss_diameter);
}

module case_screw_holes() {
    for (x = fastener_x_positions, y = fastener_y_positions) {
        translate([x, y, seam_z - 0.05])
            cylinder(h = body_depth, d = case_screw_clearance_diameter);
        translate([x, y, case_screw_head_z])
            cylinder(h = body_depth, d = case_screw_head_diameter);
    }
}

module seam_lip() {
    translate([0, 0, seam_z - seam_lip_height])
        linear_extrude(height = seam_lip_height + fuse)
            difference() {
                offset(delta = -seam_lip_clearance) inner_outline_2d();
                offset(delta = -(seam_lip_clearance + seam_lip_thickness)) inner_outline_2d();
            }
    // Root band above the seam ties the inset lip into the back-shell wall.
    translate([0, 0, seam_z])
        linear_extrude(height = 1)
            difference() {
                offset(delta = fuse) inner_outline_2d();
                offset(delta = -(seam_lip_clearance + seam_lip_thickness)) inner_outline_2d();
            }
}

module pcb_standoffs(holes, rests = []) {
    for (p = holes)
        difference() {
            translate([p[0], p[1], inside_back_z - pcb_standoff_height])
                cylinder(h = pcb_standoff_height + fuse, d = pcb_standoff_diameter);
            translate([p[0], p[1], inside_back_z - pcb_standoff_height - 0.05])
                cylinder(h = pcb_standoff_height + 0.05, d = m2_clearance_diameter);
        }
    for (p = rests)
        translate([p[0], p[1], inside_back_z - pcb_standoff_height])
            cylinder(h = pcb_standoff_height + fuse, d = pcb_rest_diameter);
}

module display_mount_bosses() {
    for (
        x = [
            display_board_center[0] - display_hole_pitch[0] / 2,
            display_board_center[0] + display_hole_pitch[0] / 2
        ],
        y = [
            display_board_center[1] - display_hole_pitch[1] / 2,
            display_board_center[1] + display_hole_pitch[1] / 2
        ]
    ) {
        difference() {
            translate([x, y, front_skin - fuse])
                cylinder(h = display_mount_top_z - front_skin + fuse, d = display_mount_boss_diameter);
            translate([x, y, display_mount_top_z - display_insert_length])
                cylinder(h = display_insert_length + fuse, d = display_insert_pilot_diameter);
        }
    }
}

battery_pocket_width = battery_pack_width + 2 * battery_tray_clearance;
battery_pocket_length = battery_pack_length + 2 * battery_tray_clearance;
battery_tray_outer_width = battery_pocket_width + 2 * battery_tray_wall;
battery_tray_outer_length = battery_pocket_length + 2 * battery_tray_wall;
battery_top_z = inside_back_z - battery_pack_thickness;

module battery_tray() {
    translate([battery_center[0], battery_center[1], inside_back_z - battery_tray_lip_height])
        linear_extrude(height = battery_tray_lip_height + fuse)
            difference() {
                square([battery_tray_outer_width, battery_tray_outer_length], center = true);
                square([battery_pocket_width, battery_pocket_length], center = true);
                // Lead exit through the bottom-facing end wall.
                translate([0, -battery_pocket_length / 2])
                    square([battery_lead_notch_width, 4 * battery_tray_wall], center = true);
            }
}

// Ribs on the front shell, outside the display footprint, that keep the pouch in its tray.
module battery_hold_down() {
    rib_height = battery_top_z - battery_hold_down_gap - front_skin;
    for (r = battery_hold_down_ribs)
        translate([r[0] - r[2] / 2, r[1] - r[3] / 2, front_skin - fuse])
            cube([r[2], r[3], rib_height + fuse]);
}

// Stand-in for the screen glass, preview only.
module display_ghost() {
    translate([display_board_center[0], display_board_center[1], front_skin + display_lip_depth])
        linear_extrude(height = 1)
            rounded_rect_2d(display_board_width - 2, display_board_height - 2, 2);
}

// Walls and floor (switch panel) of the recessed switch well, added inside the back shell.
module switch_well_body() {
    translate([switch_center[0], switch_center[1], switch_panel_front_z])
        linear_extrude(height = body_depth - switch_panel_front_z)
            rounded_rect_2d(
                switch_well_width + 2 * switch_well_wall,
                switch_well_height + 2 * switch_well_wall,
                switch_well_corner_radius + switch_well_wall
            );
}

module switch_well_cut() {
    c = switch_well_chamfer;
    translate([switch_center[0], switch_center[1], 0]) {
        translate([0, 0, switch_panel_back_z])
            linear_extrude(height = switch_well_depth + 1)
                rounded_rect_2d(switch_well_width, switch_well_height, switch_well_corner_radius);
        // Softened rim at the back face.
        hull() {
            translate([0, 0, body_depth - c])
                linear_extrude(height = 0.01)
                    rounded_rect_2d(switch_well_width, switch_well_height, switch_well_corner_radius);
            translate([0, 0, body_depth + 0.01])
                linear_extrude(height = 0.01)
                    rounded_rect_2d(
                        switch_well_width + 2 * c,
                        switch_well_height + 2 * c,
                        switch_well_corner_radius + c
                    );
        }
        // Square-cornered snap-in cutout so the clips seat fully.
        translate([0, 0, switch_panel_front_z - 1])
            linear_extrude(height = switch_panel_thickness + 2)
                square([switch_cutout_x, switch_cutout_y], center = true);
    }
}

// Component stand-ins in assembly position, for layout views and clearance checks.
module display_module_ghost() {
    translate([display_board_center[0], display_board_center[1], front_skin + display_lip_depth])
        linear_extrude(height = display_stack_depth)
            rounded_rect_2d(display_board_width, display_board_height, 2);
}

module speaker_ghost() {
    translate([speaker_center_x, speaker_center_y, front_skin + speaker_gasket_thickness]) {
        linear_extrude(height = speaker_depth) stadium_2d(speaker_width, speaker_height);
        linear_extrude(height = speaker_tab_thickness) speaker_outline_2d(0);
    }
}

module board_ghost(center, size) {
    translate([center[0], center[1], pcb_board_back_z - pcb_thickness / 2])
        cube([size[0], size[1], pcb_thickness], center = true);
}

module max_ghost() {
    board_ghost(max_board_center, max_board_size);
    // Screw terminal at the +y end and the 7-pin header along the -y edge, both facing front.
    translate([
        max_board_center[0] - max_terminal_size[0] / 2,
        max_board_center[1] + max_board_size[1] / 2 - max_terminal_size[1],
        pcb_board_front_z - max_terminal_height
    ])
        cube([max_terminal_size[0], max_terminal_size[1], max_terminal_height]);
    translate([max_board_center[0] - 9, max_board_center[1] - max_board_size[1] / 2 + 1, pcb_board_front_z - max_header_height])
        cube([18, 2.6, max_header_height]);
}
module mic_ghost() {
    translate([mic_center[0], mic_center[1], mic_board_front_z])
        linear_extrude(height = mic_pcb_thickness)
            circle(d = mic_board_diameter);
    // Foam gasket ring between the PCB pinhole side and the front skin.
    translate([mic_port_center[0], mic_port_center[1], front_skin])
        linear_extrude(height = mic_underside_clearance)
            difference() { circle(d = 6); circle(d = 3); }
    // Mic can (inward side) plus header pins and wires.
    translate([mic_port_center[0], mic_port_center[1], mic_pocket_top_z + 0.5])
        cube([4, 3, 1], center = true);
    translate([mic_center[0] - 4, mic_center[1] - 5, mic_pocket_top_z])
        cube([8, 10, mic_header_height]);
}

module switch_ghost() {
    translate([switch_center[0], switch_center[1], 0]) {
        translate([0, 0, switch_panel_front_z - switch_body_depth / 2])
            cube([switch_cutout_x - 0.4, switch_cutout_y - 0.4, switch_body_depth], center = true);
        for (dy = [-4, 4])
            translate([0, dy, (switch_body_front_z + switch_panel_front_z - switch_body_depth) / 2])
                cube([0.8, 4.8, switch_below_panel_depth - switch_body_depth], center = true);
        translate([0, 0, switch_panel_back_z + switch_flange_thickness / 2])
            cube([switch_flange_x, switch_flange_y, switch_flange_thickness], center = true);
        // Rocker shown in the "on" (top end pressed) position.
        hull() {
            translate([0, 0, switch_panel_back_z + switch_flange_thickness])
                cube([switch_flange_x - 4, switch_flange_y - 5, 0.01], center = true);
            translate([0, -(switch_flange_y - 5) / 2 + 1, switch_panel_back_z + switch_rocker_proud - 0.5])
                cube([switch_flange_x - 4, 2, 1], center = true);
            translate([0, (switch_flange_y - 5) / 2 - 1, switch_panel_back_z + switch_rocker_proud - 3])
                cube([switch_flange_x - 4, 2, 1], center = true);
        }
    }
}

module components() {
    color("#1E2A5A") display_module_ghost();
    color("#C0C0C0") battery_ghost();
    color("#2E8B57") charger_ghost();
    color("#D9534F") speaker_ghost();
    color("#1F77B4") max_ghost();
    color("#17BECF") mic_ghost();
    color("#E377C2") switch_ghost();
}

module battery_ghost() {
    translate([battery_center[0], battery_center[1], inside_back_z - battery_pack_thickness / 2])
        cube([battery_pack_width, battery_pack_length, battery_pack_thickness], center = true);
}

module shell(with_bumpers) {
    difference() {
        union() {
            body_outer();
            handle();
            if (with_bumpers) bumper_pads();
        }
        inner_cavity();
    }
}

module front_half_space() {
    translate([-200, -200, -50]) cube([400, 400, 50 + seam_z]);
}

module back_half_space() {
    translate([-200, -200, seam_z]) cube([400, 400, 100]);
}

module front_shell(with_bumpers = true) {
    difference() {
        union() {
            intersection() {
                shell(with_bumpers);
                front_half_space();
            }
            intersection() {
                union() {
                    display_lip();
                    display_mount_bosses();
                    speaker_retainer();
                    mic_pocket();
                    front_case_bosses();
                    battery_hold_down();
                }
                body_outer();
            }
        }
        translate([screen_center_x, screen_center_y, -1])
            linear_extrude(height = front_skin + 2)
                rounded_rect_2d(screen_window_width, screen_window_height, screen_window_radius);
        speaker_grille();
        mic_port_cut();
        if (bezel_separate)
            translate([0, 0, -1])
                linear_extrude(height = 1 + bezel_pocket_depth)
                    bezel_outline_2d(bezel_pocket_clearance);
    }
    if (!bezel_separate) bezel();
}

module back_shell(with_bumpers = true) {
    difference() {
        union() {
            intersection() {
                shell(with_bumpers);
                back_half_space();
            }
            intersection() {
                union() {
                    seam_lip();
                    back_case_bosses();
                    pcb_standoffs(max_hole_positions, max_rest_positions);
                    charger_cradle();
                    battery_tray();
                    switch_well_body();
                }
                body_outer();
            }
        }
        case_screw_holes();
        usb_c_cutout();
        switch_well_cut();
    }
}

if (part == "front") {
    front_shell();
} else if (part == "back") {
    back_shell();
} else if (part == "bezel") {
    // Flat back face down for printing.
    rotate([0, 180, 0])
        translate([0, 0, -bezel_pocket_depth])
            bezel();
} else if (part == "bumpers") {
    bumper_pads();
} else if (part == "preview") {
    color("#F7C600") front_shell(false);
    color("#EDB700") back_shell(false);
    color("#7B4FC9") bumper_pads();
    color("#7B4FC9") bezel();
    color("#1E2A5A") display_ghost();
    %components();
} else if (part == "layout") {
    // Back shell with every internal component in place (front shell removed).
    color("#EDB700") back_shell(false);
    components();
}

function rects_overlap(a, b) = a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3];
function centered_rect(c, w, h) = [c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2];

display_rect = centered_rect(display_board_center, display_board_width + 2, display_board_height + 2);
battery_rect = centered_rect(battery_center, battery_pack_width, battery_pack_length);
battery_tray_rect = centered_rect(battery_center, battery_tray_outer_width, battery_tray_outer_length);
charger_rect = [
    charger_port_center_x - charger_outer_width / 2, inside_bottom_y,
    charger_port_center_x + charger_outer_width / 2, inside_bottom_y + charger_outer_length
];
flat_x = body_width / 2 - edge_radius;
flat_y = body_height / 2 - edge_radius;

assert(battery_top_z - battery_hold_down_gap > front_skin + 1, "Battery is too thick for body_depth");
assert(
    abs(battery_center[0]) + battery_pocket_width / 2 < flat_x
        && abs(battery_center[1]) + battery_pocket_length / 2 < flat_y,
    "Battery pocket runs into the rounded back edge"
);
for (r = battery_hold_down_ribs) {
    rib = centered_rect([r[0], r[1]], r[2], r[3]);
    assert(!rects_overlap(rib, display_rect), "Battery hold-down rib hits the display board footprint");
    assert(
        rib[0] >= battery_rect[0] && rib[2] <= battery_rect[2]
            && rib[1] >= battery_rect[1] && rib[3] <= battery_rect[3],
        "Battery hold-down rib is not over the battery"
    );
}
assert(!rects_overlap(charger_rect, battery_tray_rect), "Charger cradle overlaps the battery tray");
assert(
    usb_c_center_z - usb_c_plug_recess_height / 2 > max(seam_z, edge_radius)
        && usb_c_center_z + usb_c_plug_recess_height / 2 < body_depth - edge_radius,
    "USB-C recess must be behind the seam and on the flat band of the bottom wall"
);
assert(charger_floor_z < inside_back_z, "Charger platform is taller than the case allows");
assert(
    case_screw_head_z > seam_z + 2 && case_screw_head_z < inside_back_z,
    "Case screw head seat is outside the back boss; check screw length and engagement"
);
assert(
    abs(screen_center_x) + bezel_outer_width / 2 < flat_x
        && abs(screen_center_y) + bezel_outer_height / 2 < flat_y,
    "Bezel runs onto the rounded front edge"
);
assert(
    abs(speaker_center_x) + speaker_mouth_width / 2 < flat_x
        && abs(speaker_center_y) + speaker_mouth_height / 2 < flat_y
        && abs(speaker_center_x - screen_center_x) - speaker_mouth_width / 2 > bezel_outer_width / 2,
    "Speaker dish overlaps the bezel or the rounded edge"
);
assert(speaker_grille_bar_thickness >= 0.8, "Speaker grille bars are too thin; reduce dish or chamber depth");
assert(speaker_grille_slot_count >= 2, "Speaker cone is too narrow for the grille slots");
assert(
    speaker_grille_slot_count * speaker_grille_slot_width * (speaker_cone_height - 2)
        >= 0.45 * speaker_cone_width * speaker_cone_height,
    "Speaker grille open area is too small for the cone"
);
assert(
    handle_opening_width + 2 * (handle_flare_radius + handle_edge_radius) < handle_width,
    "Handle posts are too thin for the opening and edge rounding"
);

max_rect = centered_rect(max_board_center, max_board_size[0], max_board_size[1]);
switch_body_rect = centered_rect(switch_center, switch_cutout_x, switch_cutout_y);
switch_well_rect = centered_rect(
    switch_center,
    switch_well_width + 2 * switch_well_wall,
    switch_well_height + 2 * switch_well_wall
);
speaker_rect = [
    speaker_center_x - speaker_retainer_outer_width / 2,
    speaker_center_y - speaker_height / 2 - speaker_clearance - speaker_retainer_thickness,
    speaker_center_x + speaker_retainer_outer_width / 2,
    speaker_center_y - speaker_height / 2 - speaker_clearance - speaker_retainer_thickness + speaker_retainer_outer_height
];
for (x = fastener_x_positions, y = fastener_y_positions)
    assert(
        !rects_overlap(speaker_rect, centered_rect([x, y], case_screw_boss_diameter, case_screw_boss_diameter)),
        "Speaker retainer hits a case screw boss"
    );
display_mount_height = display_mount_top_z - front_skin;
assert(
    min(display_hole_inset) > display_hole_diameter / 2,
    "Display holes must be inside the board outline"
);
assert(
    abs(display_board_center[0]) + display_board_width / 2 < flat_x
        && abs(display_board_center[1]) + display_board_height / 2 < flat_y,
    "Display PCB outline runs onto the rounded front edge"
);
assert(
    display_mount_height > display_insert_length && display_mount_top_z < seam_z,
    "Display inserts do not fit in the front-shell mounts"
);
assert(
    display_insert_pilot_diameter > 0 && display_insert_pilot_diameter < display_mount_boss_diameter,
    "Display insert pilot must fit inside the mount boss"
);
for (
    x = [
        display_board_center[0] - display_hole_pitch[0] / 2,
        display_board_center[0] + display_hole_pitch[0] / 2
    ],
    y = [
        display_board_center[1] - display_hole_pitch[1] / 2,
        display_board_center[1] + display_hole_pitch[1] / 2
    ]
) {
    display_mount_rect = centered_rect([x, y], display_mount_boss_diameter, display_mount_boss_diameter);
    assert(!rects_overlap(display_mount_rect, speaker_rect), "Display mount overlaps the speaker");
    for (r = battery_hold_down_ribs)
        assert(
            !rects_overlap(display_mount_rect, centered_rect([r[0], r[1]], r[2], r[3])),
            "Display mount overlaps a battery hold-down rib"
        );
    for (cx = fastener_x_positions, cy = fastener_y_positions)
        assert(
            !rects_overlap(display_mount_rect, centered_rect([cx, cy], case_screw_boss_diameter, case_screw_boss_diameter)),
            "Display mount overlaps a case screw boss"
        );
}
assert(!rects_overlap(max_rect, battery_tray_rect), "MAX board overlaps the battery tray");
assert(!rects_overlap(max_rect, charger_rect), "MAX board overlaps the charger cradle");
assert(!rects_overlap(max_rect, switch_well_rect), "MAX board overlaps the switch well");

mic_rect = centered_rect(mic_center, mic_pocket_outer_diameter, mic_pocket_outer_diameter);
assert(
    abs(mic_port_center[0]) + mic_dot_diameter / 2 + mic_dot_depth < flat_x
        && abs(mic_port_center[1]) + mic_dot_diameter / 2 + mic_dot_depth < flat_y,
    "Mic hole runs onto the rounded front edge"
);
assert(
    norm(mic_port_offset) + mic_hole_diameter / 2 < mic_board_diameter / 2 - 1,
    "Mic port offset puts the hole outside the board"
);
assert(
    mic_ledge_inner_diameter < mic_board_diameter - 1 && mic_ledge_inner_diameter > norm(mic_port_offset) * 2 + 6,
    "Mic ledge must sit under the board edge and clear the gasket"
);
assert(!rects_overlap(mic_rect, display_rect), "Mic pocket hits the display board footprint");
assert(!rects_overlap(mic_rect, speaker_rect), "Mic pocket hits the speaker");
for (r = battery_hold_down_ribs)
    assert(
        !rects_overlap(mic_rect, centered_rect([r[0], r[1]], r[2], r[3])),
        "Mic pocket overlaps a battery hold-down rib"
    );
for (x = fastener_x_positions, y = fastener_y_positions)
    assert(
        !rects_overlap(mic_rect, centered_rect([x, y], case_screw_boss_diameter, case_screw_boss_diameter)),
        "Mic pocket hits a case screw boss"
    );
assert(mic_pocket_top_z < seam_z, "Mic pocket is taller than the front shell");
assert(
    mic_pocket_top_z + mic_header_height < battery_top_z - battery_hold_down_gap,
    "Mic header and wires hit the battery"
);
assert(
    pcb_board_front_z - max(max_header_height, max_terminal_height) > display_stack_back_z + 1,
    "MAX header or screw terminal hits the display stack"
);
assert(
    max_hole_inset > m2_clearance_diameter / 2 + 0.5 && max_rest_inset - pcb_rest_diameter / 2 > 3
        && max_hole_pitch + pcb_standoff_diameter < max_board_size[0] + 2,
    "MAX standoffs must sit under the board and clear the header solder stubs"
);
assert(
    abs(switch_center[0]) + switch_well_width / 2 + switch_well_chamfer < flat_x
        && abs(switch_center[1]) + switch_well_height / 2 + switch_well_chamfer < flat_y,
    "Switch well runs onto the rounded back edge"
);
assert(!rects_overlap(switch_body_rect, display_rect), "Switch body hits the display board footprint");
assert(!rects_overlap(switch_well_rect, battery_tray_rect), "Switch well overlaps the battery tray");
assert(
    !rects_overlap(switch_body_rect, speaker_rect) || switch_body_front_z > front_skin + speaker_retainer_depth,
    "Switch terminals hit the speaker"
);
for (x = fastener_x_positions, y = fastener_y_positions)
    assert(
        !rects_overlap(switch_well_rect, centered_rect([x, y], case_screw_boss_diameter, case_screw_boss_diameter)),
        "Switch well hits a case screw boss"
    );
assert(switch_rocker_proud < switch_well_depth, "Rocker sticks out of the switch well");
