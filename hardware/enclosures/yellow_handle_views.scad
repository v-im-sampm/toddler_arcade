// Layout and section views of yellow_handle.scad. Select with -D view="..." and always pass
// -D part="none"; see README.md for the exact commands and cameras.
include <yellow_handle.scad>
view = "layout_iso";
module upright() rotate([90, 0, 180]) children();
module outline_of(w, h, r, z) translate([0, 0, z]) linear_extrude(height = 0.6) difference() { rounded_rect_2d(w + 1.2, h + 1.2, r + 0.6); rounded_rect_2d(w, h, r); }
// Label readable when viewed from the front (camera at -z).
module label(t, p, s = 3.2) color("black") translate([p[0], p[1], -2]) mirror([1, 0, 0]) linear_extrude(height = 0.4) text(t, size = s, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
module back_components() { color("#C0C0C0") battery_ghost(); color("#2E8B57") charger_ghost(); color("#1F77B4") max_ghost(); color("#E377C2") switch_ghost(); }

if (view == "layout_iso") upright() { color("#EDB700") back_shell(false); back_components(); }
if (view == "front_layout") upright() { color("#F7C600") front_shell(false); color("#1E2A5A") display_module_ghost(); color("#D9534F") speaker_ghost(); color("#17BECF") mic_ghost(); }
if (view == "plan") {
  color("#EDB700") back_shell(false);
  back_components();
  color("#1E2A5A") translate([display_board_center[0], display_board_center[1], 0]) outline_of(display_board_width, display_board_height, 2, 0);
  color("#D9534F") translate([speaker_center_x, speaker_center_y, 0]) outline_of(speaker_width, speaker_height, speaker_width / 2, -0.5);
  color("#17BECF") translate([mic_center[0], mic_center[1], 0]) outline_of(mic_board_diameter, mic_board_diameter, mic_board_diameter / 2, -0.5);
  label("BATTERY", [battery_center[0], battery_center[1] + 4]);
  label("503450", [battery_center[0], battery_center[1] - 1], 2.6);
  label("TP4057", [charger_port_center_x, -30], 2.6);
  label("USB-C", [charger_port_center_x, -38.5], 2.2);
  label("MAX98357A", [max_board_center[0], max_board_center[1]], 2.6);
  label("MIC", [mic_center[0], mic_center[1] - 11], 2.6);
  label("INMP441", [mic_center[0], mic_center[1] - 14.5], 2.2);
  label("KCD1", [switch_center[0], switch_center[1] + 2.5], 2.6);
  label("SWITCH", [switch_center[0], switch_center[1] - 2.5], 2.2);
  label("SPEAKER", [speaker_center_x, speaker_center_y + 12.5], 2.2);
  label("DISPLAY OUTLINE", [17, screen_center_y - display_board_height / 2 + 3.5], 2.4);
}
section_y_at = 0;
section_x_at = -5;
module keep(axis) if (axis == "y") translate([-200, -200, -50]) cube([400, 200 + section_y_at, 200]); else translate([-200, -200, -50]) cube([200 + section_x_at, 400, 200]);
module cut(axis, c) color(c) intersection() { children(); keep(axis); }
module section(axis) {
  cut(axis, "#F7C600") front_shell(false);
  cut(axis, "#EDB700") back_shell(false);
  cut(axis, "#7B4FC9") bezel();
  cut(axis, "#1E2A5A") display_module_ghost();
  cut(axis, "#C0C0C0") battery_ghost();
  cut(axis, "#2E8B57") charger_ghost();
  cut(axis, "#D9534F") speaker_ghost();
  cut(axis, "#1F77B4") max_ghost();
  cut(axis, "#17BECF") mic_ghost();
  cut(axis, "#E377C2") switch_ghost();
}
module ylabel(t, x, z) color("black") translate([x, 1, z]) rotate([90, 0, 180]) linear_extrude(height = 0.4) text(t, size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
module xlabel(t, y, z) color("black") translate([section_x_at + 1, y, z]) rotate([90, 0, 90]) linear_extrude(height = 0.4) text(t, size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
if (view == "section_y") {
  section("y");
  ylabel("BATTERY", battery_center[0], 42); ylabel("KCD1 SWITCH", switch_center[0], 42);
  ylabel("DISPLAY (stack TBD)", 0, -9); ylabel("SPEAKER", speaker_center_x, -9); ylabel("MIC", mic_center[0], -9);
  ylabel("BACK", 62, 33); ylabel("FRONT", 62, 3);
}
if (view == "section_x") {
  section("x");
  xlabel("TP4057", -31, 42); xlabel("MAX98357A", max_board_center[1], 42);
  xlabel("DISPLAY (stack TBD)", screen_center_y, -9);
}
module bumper_pads_front() intersection() { bumper_pads(); front_half_space(); }
module bumper_pads_back() intersection() { bumper_pads(); back_half_space(); }
if (view == "back_out" || view == "back_in") upright() { color("#EDB700") back_shell(false); color("#7B4FC9") bumper_pads_back(); }
if (view == "front_out") upright() { color("#F7C600") front_shell(false); color("#7B4FC9") bumper_pads_front(); color("#7B4FC9") bezel(); }
if (view == "front_in") upright() { color("#F7C600") front_shell(false); color("#7B4FC9") bumper_pads_front(); }
if (view == "assembled") upright() { color("#F7C600") front_shell(false); color("#EDB700") back_shell(false); color("#7B4FC9") bumper_pads(); color("#7B4FC9") bezel(); color("#1E2A5A") display_ghost(); color("#E377C2") switch_ghost(); }
