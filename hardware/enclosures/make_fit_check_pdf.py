"""Build a printable 1:1 fit-check PDF of the yellow carry-handle enclosure.

Every outline comes from yellow_handle.scad: OpenSCAD exports 2D sections and projections as
SVG, the script reads the current parameters through echo(), and reportlab draws everything at
exact scale on US Letter pages with 100 mm and 4 in check bars.

Usage (from the repository root, with reportlab installed):
    python hardware/enclosures/make_fit_check_pdf.py [--openscad PATH] [--out PATH]
"""
import argparse
import json
import re
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas

HERE = Path(__file__).resolve().parent
MODEL = HERE / "yellow_handle.scad"
DEFAULT_OPENSCAD = r"C:\Program Files\OpenSCAD (Nightly)\openscad.com"
DEFAULT_OUT = HERE / "exports" / "yellow_handle_fit_check.pdf"

MM = 72 / 25.4
PAGE_W, PAGE_H = letter[0] / MM, letter[1] / MM

PARAMS = [
    "body_width", "body_height", "body_depth", "seam_z", "wall",
    "display_board_center", "display_board_width", "display_board_height",
    "display_hole_pitch", "display_hole_diameter",
    "screen_center_x", "screen_center_y", "screen_window_width", "screen_window_height",
    "speaker_center_x", "speaker_center_y", "speaker_width", "speaker_height",
    "speaker_tab_length", "speaker_tab_width",
    "mic_center", "mic_board_diameter", "mic_board_flat_width", "mic_port_center", "mic_hole_diameter",
    "battery_center", "battery_pack_width", "battery_pack_length", "battery_pack_thickness",
    "charger_port_center_x", "charger_board_width", "charger_board_length", "inside_bottom_y",
    "max_board_center", "max_board_size", "max_hole_pitch",
    "switch_center", "switch_cutout_x", "switch_cutout_y", "switch_flange_x", "switch_flange_y",
    "usb_c_receptacle_width", "usb_c_receptacle_height", "usb_c_opening_clearance",
    "usb_c_plug_recess_width", "usb_c_plug_recess_height", "usb_c_center_z",
    "fastener_x_positions", "fastener_y_positions",
]

# 2D OpenSCAD expressions; every layer is in model millimeters.
LAYERS = {
    "front_silhouette": "projection() front_shell();",
    "front_face": "projection(cut = true) translate([0, 0, -0.05]) front_shell(false);",
    "front_through": "projection(cut = true) translate([0, 0, -front_skin / 2]) front_shell(false);",
    "front_features": "projection(cut = true) translate([0, 0, -(front_skin + 0.6)]) front_shell(false);",
    "bezel": "projection() bezel();",
    "back_silhouette": "projection() back_shell();",
    "back_skin": "projection(cut = true) translate([0, 0, -(body_depth - rear_skin / 2)]) back_shell(false);",
    "back_face": "projection(cut = true) translate([0, 0, -(body_depth - 0.1)]) back_shell(false);",
    "back_features": "projection(cut = true) translate([0, 0, -(inside_back_z - 1)]) back_shell(false);",
    "bottom_wall": (
        "mirror([1, 0, 0]) projection(cut = true) rotate([90, 0, 0])"
        " translate([0, -(body_bottom + wall / 2), 0]) union() { front_shell(false); back_shell(false); }"
    ),
    "side_profile": "projection() rotate([0, 0, -90]) rotate([0, 90, 0]) union() { front_shell(); back_shell(); }",
}


def run_openscad(openscad, tmp, name, body, ext):
    src = tmp / f"{name}.scad"
    out = tmp / f"{name}.{ext}"
    src.write_text(f'include <{MODEL.as_posix()}>\npart = "none";\n{body}\n')
    result = subprocess.run(
        [openscad, "--backend=manifold", "-o", str(out), str(src)],
        capture_output=True, text=True,
    )
    log = result.stdout + result.stderr
    if "ERROR:" in log or not out.exists():
        raise RuntimeError(f"OpenSCAD failed for {name}:\n{log}")
    return out


def read_params(openscad, tmp):
    pairs = ", ".join(f'["{p}", {p}]' for p in PARAMS)
    echo = run_openscad(openscad, tmp, "params", f"echo(params = [{pairs}]);", "echo").read_text()
    match = re.search(r"ECHO: params = (\[.*\])", echo)
    return {k: v for k, v in json.loads(match.group(1))}


def read_svg(path):
    """Return closed polylines in model coordinates (y up)."""
    text = path.read_text()
    polys = []
    for d in re.findall(r'd="([^"]*)"', text):
        for sub in re.split(r"[Mm]", d):
            pts = [(float(x), -float(y)) for x, y in re.findall(r"(-?[\d.e+-]+),(-?[\d.e+-]+)", sub)]
            if len(pts) > 2:
                polys.append(pts)
    return polys


def area(poly):
    return abs(sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]))) / 2


def holes_only(polys):
    """Drop the outer boundary of a section so only its openings remain."""
    return sorted(polys, key=area)[:-1] if polys else []


class Sheet:
    """One page; draws model geometry into a page region at 1:1."""

    def __init__(self, c, title, notes):
        self.c = c
        c.setFont("Helvetica-Bold", 13)
        c.drawString(12 * MM, (PAGE_H - 14) * MM, title)
        c.setFont("Helvetica", 7.5)
        for i, line in enumerate(notes):
            c.drawString(12 * MM, (PAGE_H - 20 - 3.6 * i) * MM, line)
        self.scale_bars()

    def scale_bars(self):
        c = self.c
        c.setStrokeColorRGB(0, 0, 0)
        c.setFillColorRGB(0, 0, 0)
        c.setLineWidth(0.6)
        x0, y = 12, 20
        c.line(x0 * MM, y * MM, (x0 + 100) * MM, y * MM)
        for i in range(11):
            tick = 3 if i % 5 == 0 else 1.8
            c.line((x0 + 10 * i) * MM, y * MM, (x0 + 10 * i) * MM, (y + tick) * MM)
        c.setFont("Helvetica", 7)
        c.drawString(x0 * MM, (y - 3.5) * MM, "0")
        c.drawRightString((x0 + 100) * MM, (y - 3.5) * MM, "100 mm")
        y2 = 10
        c.line(x0 * MM, y2 * MM, (x0 + 101.6) * MM, y2 * MM)
        for i in range(5):
            c.line((x0 + 25.4 * i) * MM, y2 * MM, (x0 + 25.4 * i) * MM, (y2 + 3) * MM)
        c.drawString(x0 * MM, (y2 - 3.5) * MM, "0")
        c.drawRightString((x0 + 101.6) * MM, (y2 - 3.5) * MM, "4 in")
        c.setFont("Helvetica-Bold", 7.5)
        c.drawString(118 * MM, 20 * MM, "PRINT AT 100% / ACTUAL SIZE - turn off 'fit to page'.")
        c.setFont("Helvetica", 7.5)
        c.drawString(118 * MM, 16.5 * MM, "Measure the bars: 100 mm and 4 in exactly. If not,")
        c.drawString(118 * MM, 13 * MM, "the print was scaled and is not valid for fit checks.")

    def place(self, extents, region, mirror=False):
        """Center model extents (minx, miny, maxx, maxy) in a page region (x, y, w, h)."""
        minx, miny, maxx, maxy = extents
        rx, ry, rw, rh = region
        self.mirror = mirror
        self.ox = rx + rw / 2 + (1 if mirror else -1) * (minx + maxx) / 2
        self.oy = ry + rh / 2 - (miny + maxy) / 2
        if maxx - minx > rw or maxy - miny > rh:
            raise ValueError("drawing does not fit its region")

    def pt(self, x, y):
        return ((self.ox - x if self.mirror else self.ox + x) * MM, (self.oy + y) * MM)

    def polys(self, polys, stroke=(0, 0, 0), width=0.5, fill=None, dash=None):
        c = self.c
        if not polys:
            return
        path = c.beginPath()
        for poly in polys:
            path.moveTo(*self.pt(*poly[0]))
            for p in poly[1:]:
                path.lineTo(*self.pt(*p))
            path.close()
        c.setStrokeColorRGB(*stroke)
        c.setLineWidth(width)
        c.setDash(*(dash or [[], 0]))
        if fill:
            c.setFillColorRGB(*fill)
        c.drawPath(path, stroke=1, fill=1 if fill else 0, fillMode=0)
        c.setDash([], 0)

    def rect(self, center, w, h, **kw):
        cx, cy = center
        self.polys([[(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2)]], **kw)

    def circle(self, center, d, n=72, **kw):
        import math
        cx, cy = center
        self.polys([[(cx + d / 2 * math.cos(2 * math.pi * i / n), cy + d / 2 * math.sin(2 * math.pi * i / n)) for i in range(n)]], **kw)

    def stadium(self, center, w, h, n=24, **kw):
        import math
        cx, cy = center
        r = min(w, h) / 2
        pts = []
        if h >= w:
            for i in range(n + 1):
                a = math.pi * i / n
                pts.append((cx + r * math.cos(a), cy + h / 2 - r + r * math.sin(a)))
            for i in range(n + 1):
                a = math.pi + math.pi * i / n
                pts.append((cx + r * math.cos(a), cy - h / 2 + r + r * math.sin(a)))
        else:
            for i in range(n + 1):
                a = -math.pi / 2 + math.pi * i / n
                pts.append((cx + w / 2 - r + r * math.cos(a), cy + r * math.sin(a)))
            for i in range(n + 1):
                a = math.pi / 2 + math.pi * i / n
                pts.append((cx - w / 2 + r + r * math.cos(a), cy + r * math.sin(a)))
        self.polys([pts], **kw)

    def cross(self, p, size=4, stroke=(0, 0, 0)):
        c = self.c
        c.setStrokeColorRGB(*stroke)
        c.setLineWidth(0.35)
        x, y = p
        c.line(*self.pt(x - size / 2, y), *self.pt(x + size / 2, y))
        c.line(*self.pt(x, y - size / 2), *self.pt(x, y + size / 2))

    def label(self, p, text, size=6.5, color=(0, 0, 0), bold=False):
        c = self.c
        c.setFillColorRGB(*color)
        c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
        x, y = self.pt(*p)
        c.drawCentredString(x, y - size * 0.35, text)
        c.setFillColorRGB(0, 0, 0)


def extents(*poly_sets):
    xs = [x for polys in poly_sets for poly in polys for x, _ in poly]
    ys = [y for polys in poly_sets for poly in polys for _, y in poly]
    return min(xs), min(ys), max(xs), max(ys)


YELLOW = (1.0, 0.95, 0.70)
GRAY = (0.93, 0.93, 0.93)
FEATURE = (0.98, 0.80, 0.30)
BLUE = (0.10, 0.25, 0.75)
RED = (0.80, 0.15, 0.15)
CYAN = (0.0, 0.55, 0.65)
GREEN = (0.10, 0.50, 0.25)
PURPLE = (0.45, 0.30, 0.75)
PINK = (0.80, 0.30, 0.60)
DASH = [[2.5, 1.5], 0]
MAIN = (12, 30, PAGE_W - 24, PAGE_H - 30 - 46)


def holes(p, center, pitch, d):
    cx, cy = center
    return [(cx + sx * pitch[0] / 2, cy + sy * pitch[1] / 2) for sx in (-1, 1) for sy in (-1, 1)]


def page_front_outside(c, L, P):
    s = Sheet(c, "1. Front face - outside, as you face the screen", [
        "Cut along the outer line to check the overall size, handle, and how it feels in a small hand.",
        "Yellow = front face. White = openings: screen window, speaker dish and slots, mic dimple and hole, bezel pocket.",
        "Purple = the separate bezel. Hold the MaTouch behind the cut-out to check the screen fills the window.",
    ])
    s.place(extents(L["front_silhouette"]), MAIN, mirror=True)
    s.polys(L["front_silhouette"], width=0.8)
    s.polys(L["front_face"], fill=YELLOW, width=0.4)
    s.polys(holes_only(L["front_through"]), width=0.4)
    s.polys(L["bezel"], stroke=PURPLE, width=0.6, dash=DASH)
    s.label((P["screen_center_x"], P["screen_center_y"]), "SCREEN WINDOW", bold=True)
    s.label((P["speaker_center_x"], P["speaker_center_y"] - P["speaker_height"] / 2 - 6), "SPEAKER")
    s.label((P["mic_center"][0], P["mic_center"][1] - 6), "MIC")
    s.label((0, P["body_height"] / 2 + 10), "HANDLE")
    c.showPage()


def page_front_inside(c, L, P):
    s = Sheet(c, "2. Front shell - inside, looking into the open shell", [
        "Lay the parts on the page: MaTouch screen-side down with its corner holes on the blue crosshairs; speaker",
        "face-down in the red outline; INMP441 pinhole side down on the cyan outline with its mic can over the cyan cross.",
        "Gray = front skin (white = openings). Orange = walls, bosses, retainers, and ribs 0.6 mm above the skin.",
    ])
    s.place(extents(L["front_silhouette"]), MAIN, mirror=False)
    s.polys(L["front_silhouette"], width=0.8)
    s.polys(L["front_through"], fill=GRAY, width=0.3)
    s.polys(L["front_features"], fill=FEATURE, width=0.4)
    cx, cy = P["display_board_center"]
    s.rect((cx, cy), P["display_board_width"], P["display_board_height"], stroke=BLUE, width=0.6, dash=DASH)
    for h in holes(P, (cx, cy), P["display_hole_pitch"], P["display_hole_diameter"]):
        s.circle(h, P["display_hole_diameter"], stroke=BLUE, width=0.5)
        s.cross(h, 7, BLUE)
    s.label((cx, cy - P["display_board_height"] / 2 + 4), "MaTouch PCB outline (estimate)", color=BLUE)
    sp = (P["speaker_center_x"], P["speaker_center_y"])
    s.stadium(sp, P["speaker_width"], P["speaker_height"], stroke=RED, width=0.6, dash=DASH)
    s.rect((sp[0], sp[1] + P["speaker_height"] / 2 + P["speaker_tab_length"] / 2),
           P["speaker_tab_width"], P["speaker_tab_length"], stroke=RED, width=0.5, dash=DASH)
    s.label((sp[0], sp[1] - P["speaker_height"] / 2 - 5), "SPEAKER", color=RED)
    s.circle(P["mic_center"], P["mic_board_diameter"], stroke=CYAN, width=0.6, dash=DASH)
    s.cross(P["mic_port_center"], 5, CYAN)
    s.label((P["mic_center"][0], P["mic_center"][1] - P["mic_board_diameter"] / 2 - 4), "INMP441", color=CYAN)
    c.showPage()


def page_back_inside(c, L, P):
    s = Sheet(c, "3. Back shell - inside, looking into the open shell", [
        "Lay the parts on the page: battery flat in the gray outline, TP4057 component side up with its USB-C at the",
        "bottom edge, MAX98357A holes on the blue crosshairs, KCD1 body in the pink cutout.",
        "Gray = back skin (white = openings). Orange = tray, cradle, standoffs, switch well, and bosses 1 mm off the skin.",
    ])
    s.place(extents(L["back_silhouette"]), MAIN, mirror=True)
    s.polys(L["back_silhouette"], width=0.8)
    s.polys(L["back_skin"], fill=GRAY, width=0.3)
    s.polys(L["back_features"], fill=FEATURE, width=0.4)
    s.rect(P["battery_center"], P["battery_pack_width"], P["battery_pack_length"], stroke=(0.3, 0.3, 0.3), width=0.6, dash=DASH)
    s.label(P["battery_center"], "503450 BATTERY", bold=True)
    ch_y = P["inside_bottom_y"] + P["charger_board_length"] / 2
    s.rect((P["charger_port_center_x"], ch_y), P["charger_board_width"], P["charger_board_length"], stroke=GREEN, width=0.6, dash=DASH)
    s.label((P["charger_port_center_x"], ch_y), "TP4057", color=GREEN)
    mc = P["max_board_center"]
    s.rect(mc, P["max_board_size"][0], P["max_board_size"][1], stroke=BLUE, width=0.6, dash=DASH)
    for h in holes(P, mc, P["max_hole_pitch"], 2):
        s.cross(h, 4, BLUE)
    s.label(mc, "MAX98357A", color=BLUE)
    s.rect(P["switch_center"], P["switch_cutout_x"], P["switch_cutout_y"], stroke=PINK, width=0.6, dash=DASH)
    s.label(P["switch_center"], "KCD1", color=PINK)
    s.label((P["charger_port_center_x"], -P["body_height"] / 2 - 4), "USB-C (bottom wall)", color=GREEN)
    c.showPage()


def page_back_outside(c, L, P):
    s = Sheet(c, "4. Back face - outside", [
        "Yellow = back face. White = the recessed KCD1 well and the four case-screw counterbores.",
        "Pink dashed = the 13 x 19 snap-in cutout in the well floor and the 15 x 21 flange. Hold the switch over it.",
    ])
    s.place(extents(L["back_silhouette"]), MAIN, mirror=False)
    s.polys(L["back_silhouette"], width=0.8)
    s.polys(L["back_face"], fill=YELLOW, width=0.4)
    s.rect(P["switch_center"], P["switch_cutout_x"], P["switch_cutout_y"], stroke=PINK, width=0.6, dash=DASH)
    s.rect(P["switch_center"], P["switch_flange_x"], P["switch_flange_y"], stroke=PINK, width=0.4, dash=DASH)
    s.label((P["switch_center"][0], P["switch_center"][1] + P["switch_flange_y"] / 2 + 7), "POWER SWITCH", color=PINK)
    c.showPage()


def page_profiles(c, L, P):
    s = Sheet(c, "5. Bottom wall (USB-C) and side profile", [
        "Top: section through the bottom wall, seen from below. White = USB-C opening and plug recess. Push a USB-C",
        "cable plug against the print to check the recess. Bottom: side silhouette with the handle, for depth and grip.",
    ])
    top = (MAIN[0], MAIN[1] + MAIN[3] / 2, MAIN[2], MAIN[3] / 2)
    s.place(extents(L["bottom_wall"]), top)
    s.polys(L["bottom_wall"], fill=YELLOW, width=0.4)
    d = P["body_depth"]
    s.label((0, 6), "FRONT FACE")
    s.label((0, -d - 5), "BACK FACE")
    s.label((P["charger_port_center_x"], -P["usb_c_center_z"] - 7), "USB-C", color=GREEN)
    bottom = (MAIN[0], MAIN[1], MAIN[2], MAIN[3] / 2)
    s.place(extents(L["side_profile"]), bottom)
    s.polys(L["side_profile"], fill=YELLOW, width=0.5)
    s.label((0, 5), "FRONT FACE")
    s.label((0, -d - 5), "BACK FACE")
    s.label((P["body_height"] / 2 + 14, 5), "HANDLE")
    c.showPage()


def page_templates(c, P):
    s = Sheet(c, "6. Component templates (the sizes the model assumes)", [
        "Lay each real part on its outline. If one does not match, update the named parameter in yellow_handle.scad,",
        "re-run this script, and reprint. Dashed = placeholder or estimate; solid = from a supplied drawing or photo.",
    ])
    s.ox, s.oy, s.mirror = 0, 0, False
    x0, top = 16, PAGE_H - 52

    dw, dh = P["display_board_width"], P["display_board_height"]
    dc = (x0 + dw / 2, top - dh / 2)
    s.rect(dc, dw, dh, stroke=BLUE, width=0.6, dash=DASH)
    for h in holes(P, dc, P["display_hole_pitch"], P["display_hole_diameter"]):
        s.circle(h, P["display_hole_diameter"], stroke=BLUE, width=0.6)
        s.cross(h, 7, BLUE)
    s.label((dc[0], dc[1] + 4), "MaTouch 2.4 (landscape)", color=BLUE, bold=True)
    s.label((dc[0], dc[1] - 1), f"holes {P['display_hole_pitch'][0]} x {P['display_hole_pitch'][1]}, d{P['display_hole_diameter']}", color=BLUE)
    s.label((dc[0], dc[1] - 5), "display_hole_pitch / display_board_*", color=BLUE, size=5.5)

    bw, bl = P["battery_pack_width"], P["battery_pack_length"]
    bc = (x0 + dw + 10 + bw / 2, top - bl / 2)
    s.rect(bc, bw, bl, stroke=(0.3, 0.3, 0.3), width=0.6)
    s.label((bc[0], bc[1] + 3), "503450 battery", bold=True)
    s.label((bc[0], bc[1] - 2), f"{bw} x {bl} x {P['battery_pack_thickness']}")
    s.label((bc[0], bc[1] - 6), "battery_pack_*", size=5.5)

    cw, cl = P["charger_board_width"], P["charger_board_length"]
    cc = (bc[0] + bw / 2 + 12 + cw / 2, top - cl / 2)
    s.rect(cc, cw, cl, stroke=GREEN, width=0.6, dash=DASH)
    uw, uh = P["usb_c_receptacle_width"], P["usb_c_receptacle_height"]
    s.rect((cc[0], cc[1] - cl / 2 + uh / 2), uw, uh, stroke=GREEN, width=0.5)
    s.label((cc[0], cc[1] + cl / 2 + 3), "TP4057", color=GREEN, bold=True)
    s.label((cc[0], cc[1] - cl / 2 - 4), f"{cw} x {cl}", color=GREEN)

    row2 = top - max(dh, bl) - 14
    mw, mh = P["max_board_size"]
    mc = (x0 + mw / 2, row2 - mh / 2)
    s.rect(mc, mw, mh, stroke=BLUE, width=0.6, dash=DASH)
    for h in holes(P, mc, P["max_hole_pitch"], 2):
        s.cross(h, 4, BLUE)
    s.label((mc[0], mc[1] - mh / 2 - 4), f"MAX98357A {mw} x {mh}", color=BLUE)
    s.label((mc[0], mc[1] - mh / 2 - 8), "max_hole_pitch, pcb_board_margin", color=BLUE, size=5.5)

    kc = (mc[0] + mw / 2 + 14 + P["switch_flange_x"] / 2, row2 - P["switch_flange_y"] / 2)
    s.rect(kc, P["switch_flange_x"], P["switch_flange_y"], stroke=PINK, width=0.5, dash=DASH)
    s.rect(kc, P["switch_cutout_x"], P["switch_cutout_y"], stroke=PINK, width=0.6)
    s.label((kc[0], kc[1] - P["switch_flange_y"] / 2 - 4), "KCD1 cutout / flange", color=PINK)
    s.label((kc[0], kc[1] - P["switch_flange_y"] / 2 - 8), "switch_cutout_*, switch_flange_*", color=PINK, size=5.5)

    import math
    md = P["mic_board_diameter"]
    micc = (kc[0] + P["switch_flange_x"] / 2 + 14 + md / 2, row2 - md / 2)
    fw = P["mic_board_flat_width"]
    half = math.asin(min(1, fw / md))
    pts = []
    for a0 in (-half, math.pi - half):
        for i in range(25):
            a = a0 + 2 * half * i / 24
            pts.append((micc[0] + md / 2 * math.sin(a), micc[1] + md / 2 * math.cos(a)))
    s.polys([pts], stroke=CYAN, width=0.6)
    s.cross(micc, 4, CYAN)
    s.label((micc[0], micc[1] - md / 2 - 4), f"INMP441 d{md}, flats {fw}", color=CYAN)
    s.label((micc[0], micc[1] - md / 2 - 8), "mic_board_*; + = mic port", color=CYAN, size=5.5)

    spc = (micc[0] + md / 2 + 14 + P["speaker_width"] / 2, row2 - P["speaker_height"] / 2 - P["speaker_tab_length"])
    s.stadium(spc, P["speaker_width"], P["speaker_height"], stroke=RED, width=0.6)
    s.rect((spc[0], spc[1] + P["speaker_height"] / 2 + P["speaker_tab_length"] / 2),
           P["speaker_tab_width"], P["speaker_tab_length"], stroke=RED, width=0.5, dash=DASH)
    s.label((spc[0], spc[1] - P["speaker_height"] / 2 - 4), f"Speaker {P['speaker_width']} x {P['speaker_height']}", color=RED)
    s.label((spc[0], spc[1] - P["speaker_height"] / 2 - 8), "speaker_* (tab: placeholder)", color=RED, size=5.5)

    row3 = row2 - 46
    oc = (x0 + 20, row3)
    s.rect(oc, P["usb_c_plug_recess_width"], P["usb_c_plug_recess_height"], stroke=GREEN, width=0.6)
    oc2 = 2 * P["usb_c_opening_clearance"]
    s.rect(oc, uw + oc2, uh + oc2, stroke=GREEN, width=0.6)
    s.label((oc[0], oc[1] - P["usb_c_plug_recess_height"] / 2 - 4), "USB-C opening + plug recess", color=GREEN)
    s.label((oc[0], oc[1] - P["usb_c_plug_recess_height"] / 2 - 8), "usb_c_*", color=GREEN, size=5.5)
    c.showPage()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--openscad", default=DEFAULT_OPENSCAD)
    ap.add_argument("--out", default=str(DEFAULT_OUT))
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        params = read_params(args.openscad, tmp)
        with ThreadPoolExecutor(max_workers=4) as pool:
            futures = {k: pool.submit(run_openscad, args.openscad, tmp, k, v, "svg") for k, v in LAYERS.items()}
            layers = {k: read_svg(f.result()) for k, f in futures.items()}

    c = canvas.Canvas(args.out, pagesize=letter)
    c.setTitle("Little Arcade yellow enclosure - 1:1 fit check")
    page_front_outside(c, layers, params)
    page_front_inside(c, layers, params)
    page_back_inside(c, layers, params)
    page_back_outside(c, layers, params)
    page_profiles(c, layers, params)
    page_templates(c, params)
    c.save()
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
