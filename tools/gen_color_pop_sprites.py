#!/usr/bin/env python3
"""Bakes Color Pop's PixelRoot32 4bpp sprites (pure Python, no dependencies).

Output: firmware/components/games/color_pop/pr32/color_pop_sprites.{hpp,cpp}

Sprites hold palette *indices*, not colours: the scene builds each 16-entry
RGB565 lookup table at runtime (bubble colour, fade alpha, the sea behind),
so one set of bubble shapes serves all four colours. Geometry follows the
retired canvas painter (bubble/glyph/crab/chrome/hand paths in the original
quality_bar.html port). Coverage is supersampled (4 sub-rows, exact spans in
x); gradients are ordered-dithered.

Usage: python tools/gen_color_pop_sprites.py
"""
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "firmware", "components", "games", "color_pop", "pr32")

R = 30.0
BUBBLE_MIN_R, BUBBLE_MAX_R = 10, 42
SUB = 4  # sub-rows per pixel
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


# ---------------------------------------------------------------- geometry
def circle(cx, cy, r, n=None):
    n = n or max(24, int(r * 6))
    return [(cx + math.cos(2 * math.pi * i / n) * r, cy + math.sin(2 * math.pi * i / n) * r) for i in range(n)]


def ellipse(cx, cy, rx, ry, rot=0.0, n=64):
    c, s = math.cos(rot), math.sin(rot)
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = math.cos(a) * rx, math.sin(a) * ry
        pts.append((cx + x * c - y * s, cy + x * s + y * c))
    return pts


def reverse(poly):
    return list(reversed(poly))


def round_rect(x, y, w, h, r, n=10):
    r = max(0.0, min(r, w / 2, h / 2))
    pts = []
    corners = [(x + w - r, y + r, -math.pi / 2), (x + w - r, y + h - r, 0.0), (x + r, y + h - r, math.pi / 2),
               (x + r, y + r, math.pi)]
    for cx, cy, a0 in corners:
        for i in range(n + 1):
            a = a0 + (math.pi / 2) * i / n
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return pts


def capsule(x0, y0, x1, y1, hw):
    """Round-capped segment as one convex polygon."""
    dx, dy = x1 - x0, y1 - y0
    ang = math.atan2(dy, dx)
    pts = []
    n = 8
    for i in range(n + 1):  # cap around (x1, y1)
        a = ang - math.pi / 2 + math.pi * i / n
        pts.append((x1 + math.cos(a) * hw, y1 + math.sin(a) * hw))
    for i in range(n + 1):  # cap around (x0, y0)
        a = ang + math.pi / 2 + math.pi * i / n
        pts.append((x0 + math.cos(a) * hw, y0 + math.sin(a) * hw))
    return pts


def polyline(points, hw):
    return [capsule(points[i][0], points[i][1], points[i + 1][0], points[i + 1][1], hw) for i in range(len(points) - 1)]


def cubic(p0, p1, p2, p3, n=16):
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
                    u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1]))
    return out


def quad(p0, p1, p2, n=16):
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0], u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))
    return out


def transform(polys, dx, dy, rot=0.0):
    c, s = math.cos(rot), math.sin(rot)
    return [[(x * c - y * s + dx, x * s + y * c + dy) for x, y in p] for p in polys]


def signed_area(p):
    return sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p))) / 2


def ccw(p):
    return p if signed_area(p) > 0 else reverse(p)


def coverage(polys, w, h):
    """Nonzero-winding coverage of `polys` on a w x h pixel grid (0..1)."""
    cov = [[0.0] * w for _ in range(h)]
    edges = []
    for p in polys:
        for i in range(len(p)):
            x0, y0 = p[i]
            x1, y1 = p[(i + 1) % len(p)]
            if y0 != y1:
                edges.append((x0, y0, x1, y1, 1 if y1 > y0 else -1))
    for row in range(h):
        for k in range(SUB):
            ys = row + (k + 0.5) / SUB
            xs = []
            for x0, y0, x1, y1, d in edges:
                if (y0 <= ys < y1) or (y1 <= ys < y0):
                    xs.append((x0 + (ys - y0) * (x1 - x0) / (y1 - y0), d))
            if not xs:
                continue
            xs.sort()
            wind = 0
            start = None
            for x, d in xs:
                prev = wind
                wind += d
                if prev == 0 and wind != 0:
                    start = x
                elif prev != 0 and wind == 0 and start is not None:
                    add_span(cov[row], start, x, w)
                    start = None
    return cov


def add_span(line, xa, xb, w):
    xa, xb = max(0.0, xa), min(float(w), xb)
    if xb <= xa:
        return
    ia, ib = int(xa), int(math.ceil(xb)) - 1
    for i in range(ia, min(ib, w - 1) + 1):
        lo, hi = max(xa, i), min(xb, i + 1)
        if hi > lo:
            line[i] += (hi - lo) / SUB


# ---------------------------------------------------------------- sprite
class Img:
    def __init__(self, name, w, h, ox, oy):
        assert 0 < w <= 255 and 0 < h <= 255, (name, w, h)
        self.name, self.w, self.h, self.ox, self.oy = name, w, h, ox, oy
        self.px = [[0] * w for _ in range(h)]

    def shapes(self, polys):
        """Polygons given around the anchor -> sprite pixel space."""
        return [[(x + self.ox, y + self.oy) for x, y in p] for p in polys]

    def paint(self, polys, index, threshold=0.5, aa_index=None, aa_lo=0.2):
        cov = coverage(self.shapes(polys), self.w, self.h)
        for y in range(self.h):
            for x in range(self.w):
                c = cov[y][x]
                if c >= threshold:
                    self.px[y][x] = index(x, y) if callable(index) else index
                elif aa_index is not None and c >= aa_lo and self.px[y][x] == 0:
                    self.px[y][x] = aa_index
        return cov


def pack(img):
    stride = (img.w + 1) // 2
    data = bytearray(stride * img.h)
    mins, maxs = [], []
    for y in range(img.h):
        row = img.px[y]
        lo, hi = img.w, 0
        for x in range(img.w):
            v = row[x] & 0xF
            if v:
                lo, hi = min(lo, x), max(hi, x + 1)
                data[y * stride + x // 2] |= v << (4 * (x & 1))  # low nibble = even column
        if hi == 0:
            lo = hi = 0
        mins.append(lo)
        maxs.append(hi)
    return bytes(data), mins, maxs


# ---------------------------------------------------------------- art
def bubble(r):
    k = r / R
    pad = 2
    shadow_bottom = (R + 6 + 4) * k
    w = int(math.ceil(2 * (r + k) + 2 * pad))
    ox = w // 2
    oy = int(math.ceil(r + k + pad))
    h = int(math.ceil(oy + shadow_bottom + pad))
    img = Img("bubble_%d" % r, w, h, ox, oy)
    # 13: drop shadow (ink 0.15)
    img.paint([ccw(ellipse(2 * k, (R + 6) * k, R * 0.6 * k, 4 * k))], 13)
    # 1..9: radial gradient (canvas createRadialGradient(-10,-12,4, 0,0,R))
    c0x, c0y, r0, r1 = -10 * k, -12 * k, 4 * k, R * k
    cdx, cdy, dr = -c0x, -c0y, r1 - r0
    a = cdx * cdx + cdy * cdy - dr * dr

    def grad(x, y):
        px, py = x + 0.5 - ox - c0x, y + 0.5 - oy - c0y
        b = px * cdx + py * cdy + r0 * dr
        c = px * px + py * py - r0 * r0
        disc = max(0.0, b * b - a * c)
        ts = [(b + math.sqrt(disc)) / a, (b - math.sqrt(disc)) / a]
        t = max(t for t in ts if r0 + t * dr >= 0) if any(r0 + t * dr >= 0 for t in ts) else 0.0
        t = min(1.0, max(0.0, t))
        level = int(math.floor(t * 8 + BAYER4[y & 3][x & 3] / 16.0))
        return 1 + min(8, max(0, level))

    img.paint([ccw(circle(0, 0, r))], grad)
    # 11: outline (shadow colour, 2px in bubble space), 12: outer fringe
    hw = max(0.6, k)
    img.paint([ccw(circle(0, 0, r + hw)), reverse(ccw(circle(0, 0, r - hw)))], 11, aa_index=12, aa_lo=0.2)
    # 14: specular highlight + dot (white 0.75)
    img.paint([ccw(ellipse(-11 * k, -14 * k, 7 * k, 4 * k, -0.6)), ccw(circle(-3 * k, -20 * k, max(0.8, 1.8 * k), 16))], 14,
              threshold=0.45)
    return img


def glyph_polys(kind, s, lw):
    if kind == 0:  # HEART
        p = [(0, s * 0.55)]
        p += cubic(p[-1], (-s * 1.1, -s * 0.1), (-s * 0.5, -s * 0.95), (0, -s * 0.35))
        p += cubic(p[-1], (s * 0.5, -s * 0.95), (s * 1.1, -s * 0.1), (0, s * 0.55))
        return [ccw(p[:-1])]
    if kind == 1:  # WAVE
        polys = []
        for oy in (-s * 0.3, s * 0.3):
            pts = [(-s + i * s / 10, oy + math.sin(i / 20 * math.pi * 4) * s * 0.22) for i in range(21)]
            polys += [ccw(c) for c in polyline(pts, lw / 2)]
        return polys
    if kind == 2:  # LEAF
        p = [(-s * 0.8, s * 0.6)]
        p += quad(p[-1], (-s * 0.8, -s * 0.8), (s * 0.8, -s * 0.6))
        p += quad(p[-1], (s * 0.8, s * 0.8), (-s * 0.8, s * 0.6))
        return [ccw(p[:-1])]
    pts = []  # STAR
    for i in range(10):
        rr = s if i % 2 == 0 else s * 0.45
        a = -math.pi / 2 + i * math.pi / 5
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    return [ccw(pts)]


def glyph(kind, r):
    k = r / R
    s = 12 * k
    lw = max(1.2, 3 * k)
    ext = int(math.ceil(s * 1.15 + lw)) + 1
    img = Img("glyph_%d_%d" % (kind, r), 2 * ext, 2 * ext, ext, ext)
    # 15: glyph (white 0.9), 10: anti-aliased edge
    img.paint(glyph_polys(kind, s, lw), 15, threshold=0.6, aa_index=10, aa_lo=0.25)
    return img


def crab_body():
    img = Img("crab_body", 46, 32, 23, 25)  # anchor = crab origin; body ellipse at (0,-10)
    img.paint([ccw(ellipse(0, -10, 21, 14))], 2, aa_index=4, aa_lo=0.25)  # outline + fringe
    img.paint([ccw(ellipse(0, -10, 19, 12))], 1)
    img.paint([ccw(circle(-12, -8, 3, 20)), ccw(circle(12, -8, 3, 20))], 3)
    return img


def cloud():
    img = Img("cloud", 44, 26, 10, 16)
    polys = [ccw(circle(0, 0, 9)), ccw(circle(11, -4, 11)), ccw(circle(23, 0, 8))]
    img.paint(polys, 1, aa_index=2, aa_lo=0.3)
    return img


def sand():
    top = [(0.0, 204.0)] + [(float(x), 202 + math.sin(x / 23.0) * 3) for x in range(0, 321, 8)] + [(320.0, 240.0),
                                                                                                    (0.0, 240.0)]
    body = ccw(top)
    halves = []
    for i, x0 in enumerate((0, 160)):
        img = Img("sand_%d" % i, 160, 41, -x0, -199)  # anchor = screen origin
        img.paint([body], 1, aa_index=2, aa_lo=0.25)
        halves.append(img)
    # Speckles: Mulberry32(7), 70 dots of kSandShadow at alpha 0.35..0.65.
    rng = Mulberry32(7)
    alpha = [[[0.0] * 160 for _ in range(41)] for _ in range(2)]
    for _ in range(70):
        a = 0.35 + rng.next() * 0.3
        x = rng.next() * 320
        y = 210 + rng.next() * 30
        rad = 0.6 + rng.next() * 1.2
        for i, img in enumerate(halves):
            cov = coverage(img.shapes([ccw(circle(x, y, rad, 16))]), 160, 41)
            for yy in range(41):
                for xx in range(160):
                    if cov[yy][xx] > 0:
                        e = min(1.0, cov[yy][xx]) * a
                        alpha[i][yy][xx] = 1 - (1 - alpha[i][yy][xx]) * (1 - e)
    for i, img in enumerate(halves):
        for yy in range(41):
            for xx in range(160):
                e = alpha[i][yy][xx]
                if e >= 0.15 and img.px[yy][xx] == 1:
                    img.px[yy][xx] = 3 if e < 0.4 else (4 if e < 0.55 else 5)
    return halves


class Mulberry32:
    def __init__(self, seed):
        self.a = seed & 0xFFFFFFFF

    def next(self):
        self.a = (self.a + 0x6D2B79F5) & 0xFFFFFFFF
        a = self.a
        t = ((a ^ (a >> 15)) * (1 | a)) & 0xFFFFFFFF
        t = ((t + (((t ^ (t >> 7)) * (61 | t)) & 0xFFFFFFFF)) & 0xFFFFFFFF) ^ t
        return ((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0


def home_key():
    img = Img("home_key", 46, 46, -5, -5)  # anchor = screen origin, key at (6,6) 44x44
    img.paint([ccw(round_rect(5, 5, 46, 46, 15)), reverse(ccw(round_rect(7, 7, 42, 42, 13)))], 2, aa_index=4)
    img.paint([ccw(round_rect(7, 7, 42, 42, 13))], 1)
    house = [(28, 15), (41, 27), (37, 27), (37, 39), (19, 39), (19, 27), (15, 27)]
    img.paint([ccw(house)], 3, aa_index=5, aa_lo=0.3)
    return img


def mic_key(on):
    img = Img("mic_key_%s" % ("on" if on else "off"), 44, 44, -270, -6)
    cream, ink, ink_aa, edge = (1, 3, 5, 6) if on else (7, 8, 9, 9)
    img.paint([ccw(round_rect(270, 6, 44, 44, 14))], cream, aa_index=edge, aa_lo=0.3)
    arc = [(292 + math.cos(a) * 9, 25 + math.sin(a) * 9)
           for a in [0.1 * math.pi + i * 0.8 * math.pi / 16 for i in range(17)]]
    polys = [ccw(round_rect(287, 14, 10, 18, 5))] + [ccw(c) for c in polyline(arc, 1.0)] + [
        ccw(capsule(292, 34, 292, 40, 1.0))]
    img.paint(polys, ink, aa_index=ink_aa, aa_lo=0.3)
    return img


def hand():
    img = Img("hand", 34, 38, 15, 21)  # anchor = (b.x + 14, y), pre-rotated by -0.5 rad
    rot = -0.5
    rr_out = round_rect(-4.75, -16.75, 9.5, 19.5, 4.75)
    rr_in = round_rect(-3.25, -15.25, 6.5, 16.5, 3.25)
    el_out = ellipse(0, 6, 9.75, 8.75)
    el_in = ellipse(0, 6, 8.25, 7.25)
    img.paint(transform([ccw(rr_out)], 0, 0, rot), 2, aa_index=3, aa_lo=0.3)
    img.paint(transform([ccw(rr_in)], 0, 0, rot), 1)
    # The palm covers the finger's lower outline.
    cov = coverage(img.shapes(transform([ccw(el_out)], 0, 0, rot)), img.w, img.h)
    for y in range(img.h):
        for x in range(img.w):
            if cov[y][x] >= 0.5:
                img.px[y][x] = 2
            elif cov[y][x] >= 0.3 and img.px[y][x] == 0:
                img.px[y][x] = 3
    img.paint(transform([ccw(el_in)], 0, 0, rot), 1)
    return img


STAR_MIN_R, STAR_MAX_R = 2, 7


def star_bits(r):
    """(2r+1)^2 1bpp star (outer r+0.5, inner 0.45x). Renderer.cpp reads bit
    (width-1) as the leftmost pixel, despite the Sprite struct's comment."""
    n = 2 * r + 1
    ro = r + 0.5
    pts = []
    for i in range(10):
        rr = ro if i % 2 == 0 else ro * 0.45
        a = -math.pi / 2 + i * math.pi / 5
        pts.append((n / 2 + math.cos(a) * rr, n / 2 + math.sin(a) * rr))
    cov = coverage([ccw(pts)], n, n)
    rows = []
    for y in range(n):
        v = 0
        for x in range(n):
            if cov[y][x] >= 0.45:
                v |= 1 << (n - 1 - x)
        rows.append(v)
    return rows


# ---------------------------------------------------------------- emit
def emit():
    arts = []  # (cpp symbol, Img)
    bubbles = [bubble(r) for r in range(BUBBLE_MIN_R, BUBBLE_MAX_R + 1)]
    glyphs = [[glyph(kind, r) for r in range(BUBBLE_MIN_R, BUBBLE_MAX_R + 1)] for kind in range(4)]
    singles = [("kCrabBody", crab_body()), ("kCloud", cloud())]
    sand_l, sand_r = sand()
    singles += [("kSandLeft", sand_l), ("kSandRight", sand_r), ("kHomeKey", home_key()), ("kMicKeyOn", mic_key(True)),
                ("kMicKeyOff", mic_key(False)), ("kHand", hand())]

    total = [0]
    lines = []

    def blob(img):
        data, mins, maxs = pack(img)
        total[0] += len(data) + 2 * img.h
        n = img.name
        lines.append("const uint8_t %s_px[] = {%s};" % (n, ",".join(str(b) for b in data)))
        lines.append("const uint8_t %s_min[] = {%s};" % (n, ",".join(map(str, mins))))
        lines.append("const uint8_t %s_max[] = {%s};" % (n, ",".join(map(str, maxs))))
        return "{{%s_px, kIdentity, %d, %d, 16, %s_min, %s_max}, %d, %d}" % (n, img.w, img.h, n, n, img.ox, img.oy)

    bubble_init = [blob(b) for b in bubbles]
    glyph_init = [[blob(g) for g in row] for row in glyphs]
    single_init = [(sym, blob(img)) for sym, img in singles]
    stars = [star_bits(r) for r in range(STAR_MIN_R, STAR_MAX_R + 1)]
    for r, rows in zip(range(STAR_MIN_R, STAR_MAX_R + 1), stars):
        total[0] += 2 * len(rows)
        lines.append("const uint16_t kStar%d_rows[] = {%s};" % (r, ",".join("0x%04X" % v for v in rows)))

    cpp = []
    cpp.append("// Generated by tools/gen_color_pop_sprites.py. Do not edit.")
    cpp.append('#include "color_pop_sprites.hpp"')
    cpp.append("")
    cpp.append("namespace little_arcade::color_pop::sprites {")
    cpp.append("namespace {")
    cpp.append("using pixelroot32::graphics::Color;")
    cpp.append("const Color kIdentity[16] = {%s};" % ", ".join("static_cast<Color>(%d)" % i for i in range(16)))
    cpp += lines
    cpp.append("}  // namespace")
    cpp.append("")
    cpp.append("const Art kBubble[kBubbleSizes] = {\n  %s};" % ",\n  ".join(bubble_init))
    cpp.append("const Art kGlyph[4][kBubbleSizes] = {\n  %s};" % ",\n  ".join(
        "{%s}" % ",\n   ".join(row) for row in glyph_init))
    for sym, init in single_init:
        cpp.append("const Art %s = %s;" % (sym, init))
    cpp.append("const pixelroot32::graphics::Sprite kStar[kStarSizes] = {%s};" % ", ".join(
        "{kStar%d_rows, %d, %d}" % (r, 2 * r + 1, 2 * r + 1) for r in range(STAR_MIN_R, STAR_MAX_R + 1)))
    cpp.append("")
    cpp.append("}  // namespace little_arcade::color_pop::sprites")

    hpp = """// Generated by tools/gen_color_pop_sprites.py. Do not edit.
#pragma once
#include <cstddef>
#include <cstdint>

#include "graphics/Renderer.h"

// Color Pop's baked PixelRoot32 sprites. Pixels are palette indices; the scene
// supplies the 16-entry RGB565 table per draw (see color_pop_scene.cpp).
// Draw each Art at (anchor_x - ox, anchor_y - oy).
namespace little_arcade::color_pop::sprites {

struct Art {
  pixelroot32::graphics::Sprite4bpp sprite;
  int16_t ox, oy;
};

inline constexpr int kBubbleMinR = %d, kBubbleMaxR = %d;
inline constexpr int kBubbleSizes = kBubbleMaxR - kBubbleMinR + 1;

// Bubble body around its centre. Indices: 1..9 radial gradient (t = 0..1 in
// eighths), 11 outline, 12 outline fringe, 13 drop shadow, 14 highlight.
extern const Art kBubble[kBubbleSizes];
// Pattern glyph around the bubble centre, [Pattern][radius]. 15 glyph, 10 edge.
extern const Art kGlyph[4][kBubbleSizes];
// Crab body around the crab origin: 1 body, 2 outline, 3 cheek, 4 fringe.
extern const Art kCrabBody;
// Cloud around its first puff: 1 cloud, 2 edge.
extern const Art kCloud;
// Beach (screen anchored): 1 sand, 2 sand/sea edge, 3..5 speckles.
extern const Art kSandLeft, kSandRight;
// Chrome keys (screen anchored). 1 cream, 2 border, 3 ink, 4 border edge,
// 5 ink edge, 6 key edge; mic off: 7 cream, 8 ink, 9 edge (all at 0.45).
extern const Art kHomeKey, kMicKeyOn, kMicKeyOff;
// Helper hand around its pivot, pre-rotated: 1 white, 2 ink, 3 ink edge.
extern const Art kHand;
// 1bpp stars for star particles, outer radius kStarMinR.. (2r+1 square).
inline constexpr int kStarMinR = %d, kStarMaxR = %d;
inline constexpr int kStarSizes = kStarMaxR - kStarMinR + 1;
extern const pixelroot32::graphics::Sprite kStar[kStarSizes];

inline constexpr std::size_t kBakedBytes = %d;

}  // namespace little_arcade::color_pop::sprites
""" % (BUBBLE_MIN_R, BUBBLE_MAX_R, STAR_MIN_R, STAR_MAX_R, total[0])

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "color_pop_sprites.cpp"), "w", newline="\n") as f:
        f.write("\n".join(cpp) + "\n")
    with open(os.path.join(OUT_DIR, "color_pop_sprites.hpp"), "w", newline="\n") as f:
        f.write(hpp)
    print("wrote %d bytes of sprite data" % total[0])


if __name__ == "__main__":
    emit()
