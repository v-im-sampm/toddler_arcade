// Parametric enclosure for the Little Arcade "Carry-Handle Explorer" (yellow concept #4).
// Pillow-shaped body, rounded tube handle, raised screen bezel and ribbed side grips.
// Dimensions are millimeters. Confirm all placeholder dimensions against the actual parts.

$fn = 40;

// "preview", "front", "back", "bezel" (separate purple part, print-oriented), or
// "bumpers" (grip pads only, for use as a color modifier in a multi-material slicer).
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
display_board_width = 62; // Placeholder: measure the MaTouch PCB, not only its active screen.
display_board_height = 46; // Placeholder.
display_lip_depth = 1.4;
display_lip_overlap = 1.2;

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

// Speaker mount and grille; the supplied speaker image labels the speaker as 15 x 10 mm.
// Model x runs toward the viewer's left when facing the screen, so negative x is on the right.
speaker_center_x = -45;
speaker_center_y = 0;
speaker_width = 15;
speaker_height = 10;
speaker_depth = 4; // Placeholder: measure the speaker housing including its wire exit.
speaker_clearance = 0.5;
speaker_retainer_thickness = 1.2;
speaker_retainer_depth = speaker_depth + speaker_clearance;
speaker_grille_slot_length = 12;
speaker_grille_slot_width = 1.6;
speaker_grille_slot_count = 4;
speaker_grille_pitch = 3;

// Rear-shell board mounts; board outlines and hole pitches are editable placeholders.
pcb_standoff_height = 4.2;
pcb_standoff_diameter = 5.2;
m2_clearance_diameter = 2.3;
max_board_center = [-42, 14];
max_hole_pitch = [18, 13];
ina_board_center = [-41, -14];
ina_hole_pitch = [20, 15];

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
battery_hold_down_ribs = [[37, -6, 2, 12], [37, 18, 2, 12], [21, 29.5, 18, 2]];

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
    translate([screen_center_x, screen_center_y, front_skin - fuse])
        difference() {
            linear_extrude(height = display_lip_depth + fuse)
                rounded_rect_2d(display_board_width, display_board_height, 3);
            translate([0, 0, -0.05])
                linear_extrude(height = display_lip_depth + fuse + 0.1)
                    rounded_rect_2d(
                        screen_window_width + 2 * display_lip_overlap,
                        screen_window_height + 2 * display_lip_overlap,
                        2
                    );
        }
}

module speaker_grille() {
    for (i = [0 : speaker_grille_slot_count - 1]) {
        y = speaker_center_y + (i - (speaker_grille_slot_count - 1) / 2) * speaker_grille_pitch;
        translate([speaker_center_x, y, -1])
            linear_extrude(height = front_skin + 2)
                rounded_rect_2d(
                    speaker_grille_slot_length,
                    speaker_grille_slot_width,
                    speaker_grille_slot_width / 2
                );
    }
}

module speaker_retainer() {
    outer_w = speaker_width + 2 * speaker_clearance + 2 * speaker_retainer_thickness;
    outer_h = speaker_height + 2 * speaker_clearance + 2 * speaker_retainer_thickness;
    translate([speaker_center_x, speaker_center_y, front_skin - fuse])
        linear_extrude(height = speaker_retainer_depth + fuse)
            difference() {
                square([outer_w, outer_h], center = true);
                square([outer_w - 2 * speaker_retainer_thickness, outer_h - 2 * speaker_retainer_thickness], center = true);
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

module pcb_standoffs(center, hole_pitch) {
    for (
        x = [center[0] - hole_pitch[0] / 2, center[0] + hole_pitch[0] / 2],
        y = [center[1] - hole_pitch[1] / 2, center[1] + hole_pitch[1] / 2]
    ) {
        difference() {
            translate([x, y, inside_back_z - pcb_standoff_height])
                cylinder(h = pcb_standoff_height + fuse, d = pcb_standoff_diameter);
            translate([x, y, inside_back_z - pcb_standoff_height - 0.05])
                cylinder(h = pcb_standoff_height + 0.05, d = m2_clearance_diameter);
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
    translate([screen_center_x, screen_center_y, front_skin + display_lip_depth])
        linear_extrude(height = 1)
            rounded_rect_2d(display_board_width - 2, display_board_height - 2, 2);
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
                    speaker_retainer();
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
                    pcb_standoffs(max_board_center, max_hole_pitch);
                    pcb_standoffs(ina_board_center, ina_hole_pitch);
                    charger_cradle();
                    battery_tray();
                }
                body_outer();
            }
        }
        case_screw_holes();
        usb_c_cutout();
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
    %battery_ghost();
    %charger_ghost();
}

function rects_overlap(a, b) = a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3];
function centered_rect(c, w, h) = [c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2];

display_rect = centered_rect([screen_center_x, screen_center_y], display_board_width + 2, display_board_height + 2);
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
    abs(speaker_center_x) + speaker_grille_slot_length / 2 < flat_x
        && abs(speaker_center_x - screen_center_x) - speaker_grille_slot_length / 2 > bezel_outer_width / 2,
    "Speaker grille overlaps the bezel or the rounded edge"
);
assert(
    handle_opening_width + 2 * (handle_flare_radius + handle_edge_radius) < handle_width,
    "Handle posts are too thin for the opening and edge rounding"
);
