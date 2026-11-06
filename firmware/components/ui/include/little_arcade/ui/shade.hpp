#pragma once
#include <cmath>
#include <cstdint>

#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/tokens.hpp"

namespace little_arcade::ui {

// Quality-bar painters: gradients, 3-tone shaded volumes, gloss, soft
// shadows and dithered RGB565 export. LVGL-free and host-tested. Like
// paint.hpp they run when a view is built, never per frame.

inline uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  uint32_t out = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    const float ca = static_cast<float>((a >> shift) & 0xFFu), cb = static_cast<float>((b >> shift) & 0xFFu);
    out |= static_cast<uint32_t>(ca + (cb - ca) * t + 0.5f) << shift;
  }
  return out;
}

// Light -> base over [0, 0.55], base -> shadow over [0.55, 1] (the sketch's
// radial gradient stops).
inline uint32_t ramp3(const tok::Hue& h, float t) {
  if (t <= 0.55f) return mix(h.light, h.base, t / 0.55f);
  return mix(h.base, h.shadow, (t - 0.55f) / 0.45f);
}

// paint() with a per-pixel colour: `color(x, y)` is sampled at pixel centres.
template <typename Inside, typename ColorFn>
void paint_fn(Canvas& c, float x0, float y0, float x1, float y1, uint8_t opa, Inside inside, ColorFn color,
              int ss = 4) {
  const int px0 = x0 < 0 ? 0 : static_cast<int>(x0);
  const int py0 = y0 < 0 ? 0 : static_cast<int>(y0);
  const int px1 = x1 > static_cast<float>(c.w) ? c.w : static_cast<int>(std::ceil(x1));
  const int py1 = y1 > static_cast<float>(c.h) ? c.h : static_cast<int>(std::ceil(y1));
  const float step = 1.0f / static_cast<float>(ss);
  const float lo = 0.5f * step, hi = (static_cast<float>(ss) - 0.5f) * step;
  const int total = ss * ss;
  for (int py = py0; py < py1; ++py) {
    const float fy = static_cast<float>(py);
    for (int px = px0; px < px1; ++px) {
      const float fx = static_cast<float>(px);
      const int corners = static_cast<int>(inside(fx + lo, fy + lo)) + static_cast<int>(inside(fx + hi, fy + lo)) +
                          static_cast<int>(inside(fx + lo, fy + hi)) + static_cast<int>(inside(fx + hi, fy + hi));
      if (corners == 0) continue;
      int hits = total;
      if (corners != 4) {
        hits = 0;
        for (int sy = 0; sy < ss; ++sy) {
          const float y = fy + (static_cast<float>(sy) + 0.5f) * step;
          for (int sx = 0; sx < ss; ++sx) {
            if (inside(fx + (static_cast<float>(sx) + 0.5f) * step, y)) ++hits;
          }
        }
        if (hits == 0) continue;
      }
      const uint32_t a = (static_cast<uint32_t>(hits) * opa + static_cast<uint32_t>(total) / 2) / static_cast<uint32_t>(total);
      uint32_t& d = c.px[py * c.w + px];
      d = blend_over(d, color(fx + 0.5f, fy + 0.5f), a);
    }
  }
}

inline void fill_rect(Canvas& c, float x0, float y0, float x1, float y1, uint32_t rgb, uint8_t opa = 255) {
  paint(c, x0, y0, x1, y1, rgb, opa, [=](float x, float y) { return x >= x0 && x < x1 && y >= y0 && y < y1; }, 1);
}

// Vertical gradient filling whole pixel rows [y0, y1) between x0 and x1.
inline void fill_vgradient(Canvas& c, int x0, int y0, int x1, int y1, uint32_t top, uint32_t bottom,
                           uint8_t opa = 255) {
  x0 = x0 < 0 ? 0 : x0;
  y0 = y0 < 0 ? 0 : y0;
  x1 = x1 > c.w ? c.w : x1;
  y1 = y1 > c.h ? c.h : y1;
  const float span = static_cast<float>(y1 - y0 > 1 ? y1 - y0 - 1 : 1);
  for (int y = y0; y < y1; ++y) {
    const uint32_t rgb = mix(top, bottom, static_cast<float>(y - y0) / span);
    for (int x = x0; x < x1; ++x) {
      uint32_t& d = c.px[y * c.w + x];
      d = blend_over(d, rgb, opa);
    }
  }
}

// Any shape shaded as a lit volume: colour follows the distance from a light
// point (lx, ly) through ramp3 over `radius`.
template <typename Inside>
void fill_shaded(Canvas& c, float x0, float y0, float x1, float y1, Inside inside, const tok::Hue& hue, float lx,
                 float ly, float radius, uint8_t opa = 255) {
  const float inv = 1.0f / radius;
  paint_fn(c, x0, y0, x1, y1, opa, inside, [&](float x, float y) {
    const float dx = x - lx, dy = y - ly;
    return ramp3(hue, std::sqrt(dx * dx + dy * dy) * inv);
  });
}

// Shaded ellipse lit from the upper left, with a shadow-tone outline.
inline void shaded_ellipse(Canvas& c, float cx, float cy, float rx, float ry, const tok::Hue& hue,
                           float outline = 2.0f, float rot = 0.0f) {
  const Ellipse e(cx, cy, rx, ry, rot);
  const float m = (rx > ry ? rx : ry) + outline + 1.0f;
  const float r = rx > ry ? rx : ry;
  fill_shaded(c, cx - m, cy - m, cx + m, cy + m, [&e](float x, float y) { return e.contains(x, y); }, hue,
              cx - rx * 0.33f, cy - ry * 0.4f, r * 1.35f);
  if (outline > 0.0f) stroke_ellipse(c, cx, cy, rx, ry, rot, outline, hue.shadow);
}
inline void shaded_ball(Canvas& c, float cx, float cy, float r, const tok::Hue& hue, float outline = 2.0f) {
  shaded_ellipse(c, cx, cy, r, r, hue, outline);
}
// Shaded filled path (bodies, leaves, hulls) lit from its upper-left.
inline void shaded_path(Canvas& c, const Path& p, const tok::Hue& hue, float outline = 2.0f) {
  float x0, y0, x1, y1;
  p.bounds(1, x0, y0, x1, y1);
  const float w = x1 - x0, h = y1 - y0;
  fill_shaded(c, x0, y0, x1, y1, [&p](float x, float y) { return p.filled(x, y); }, hue, x0 + w * 0.3f,
              y0 + h * 0.25f, (w > h ? w : h) * 0.95f);
  if (outline > 0.0f) stroke_path(c, p, outline, hue.shadow, true);
}

// Rounded rectangle test and a shaded rounded panel.
inline bool in_round_rect(float x, float y, float x0, float y0, float x1, float y1, float r) {
  if (x < x0 || x >= x1 || y < y0 || y >= y1) return false;
  const float cx = x < x0 + r ? x0 + r : (x > x1 - r ? x1 - r : x);
  const float cy = y < y0 + r ? y0 + r : (y > y1 - r ? y1 - r : y);
  const float dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}
inline void shaded_round_rect(Canvas& c, float x0, float y0, float x1, float y1, float r, const tok::Hue& hue,
                              float outline = 2.0f) {
  const float w = x1 - x0, h = y1 - y0;
  fill_shaded(c, x0 - 1, y0 - 1, x1 + 1, y1 + 1,
              [=](float x, float y) { return in_round_rect(x, y, x0, y0, x1, y1, r); }, hue, x0 + w * 0.3f,
              y0 + h * 0.2f, (w > h ? w : h) * 1.0f);
  if (outline > 0.0f) {
    const float hw = outline * 0.5f;
    paint(c, x0 - 1, y0 - 1, x1 + 1, y1 + 1, hue.shadow, 255, [=](float x, float y) {
      return in_round_rect(x, y, x0 - hw, y0 - hw, x1 + hw, y1 + hw, r + hw) &&
             !in_round_rect(x, y, x0 + hw, y0 + hw, x1 - hw, y1 - hw, r - hw > 0 ? r - hw : 0);
    });
  }
}
inline void fill_round_rect(Canvas& c, float x0, float y0, float x1, float y1, float r, uint32_t rgb,
                            uint8_t opa = 255) {
  paint(c, x0 - 1, y0 - 1, x1 + 1, y1 + 1, rgb, opa,
        [=](float x, float y) { return in_round_rect(x, y, x0, y0, x1, y1, r); });
}

// Specular gloss for a ball of radius r centred on (cx, cy): soft oval + dot.
inline void gloss(Canvas& c, float cx, float cy, float r, uint8_t opa = 190) {
  fill_ellipse(c, cx - r * 0.37f, cy - r * 0.47f, r * 0.24f, r * 0.13f, -0.6f, tok::kWhite, opa);
  fill_circle(c, cx - r * 0.1f, cy - r * 0.67f, r * 0.06f + 0.6f, tok::kWhite, opa);
}

// Feathered contact shadow (ink at low opacity, three soft rings).
inline void soft_shadow(Canvas& c, float cx, float cy, float rx, float ry, uint8_t opa = 46) {
  fill_ellipse(c, cx, cy, rx * 1.15f, ry * 1.25f, 0.0f, tok::kInk, static_cast<uint8_t>(opa / 3));
  fill_ellipse(c, cx, cy, rx, ry, 0.0f, tok::kInk, static_cast<uint8_t>(opa / 2));
  fill_ellipse(c, cx, cy, rx * 0.8f, ry * 0.75f, 0.0f, tok::kInk, static_cast<uint8_t>(opa / 2));
}

// Cartoon eye: white, ink pupil looking toward (lx, ly) in [-1, 1], catch light.
inline void cartoon_eye(Canvas& c, float cx, float cy, float r, float lx = 0.0f, float ly = 0.0f,
                        uint32_t edge = tok::kInk) {
  fill_circle(c, cx, cy, r, tok::kWhite);
  stroke_circle(c, cx, cy, r, r > 6 ? 1.6f : 1.2f, edge);
  const float pr = r * 0.48f, px = cx + lx * r * 0.32f, py = cy + ly * r * 0.32f;
  fill_circle(c, px, py, pr, tok::kInk);
  fill_circle(c, px - pr * 0.35f, py - pr * 0.4f, pr * 0.32f + 0.4f, tok::kWhite);
}
// Smile arc (ink), from angle 0.15*pi to 0.85*pi like the sketch.
inline void smile(Canvas& c, float cx, float cy, float r, float width = 2.0f, uint32_t rgb = tok::kInk) {
  stroke_path(c, Path{}.arc(cx, cy, r, 0.15f * 3.14159265f, 0.85f * 3.14159265f, 12), width, rgb);
}
inline void cheeks(Canvas& c, float cx, float cy, float dx, float r) {
  fill_circle(c, cx - dx, cy, r, tok::kCheek, 115);
  fill_circle(c, cx + dx, cy, r, tok::kCheek, 115);
}

// Five-point star with a light core (sparkles, rewards).
inline void shaded_star(Canvas& c, float cx, float cy, float ro, const tok::Hue& hue) {
  Path p;
  for (int i = 0; i < 10; ++i) {
    const float r = (i % 2) ? ro * 0.47f : ro, a = -1.5707963f + static_cast<float>(i) * 0.62831853f;
    p.line(cx + std::cos(a) * r, cy + std::sin(a) * r);
  }
  shaded_path(c, p, hue, ro > 8 ? 1.8f : 1.2f);
}

// Opaque ARGB canvas -> RGB565 with a 4x4 ordered dither so soft gradients do
// not band on the 16-bit panel. Alpha is ignored (backdrops are opaque).
inline void to_rgb565_dithered(const Canvas& src, uint16_t* dst) {
  static constexpr uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  for (int y = 0; y < src.h; ++y) {
    for (int x = 0; x < src.w; ++x) {
      const uint32_t p = src.px[y * src.w + x];
      const uint32_t t = kBayer[y & 3][x & 3];
      uint32_t r = ((p >> 16) & 0xFFu) + t / 2, g = ((p >> 8) & 0xFFu) + t / 4, b = (p & 0xFFu) + t / 2;
      r = r > 255 ? 255 : r;
      g = g > 255 ? 255 : g;
      b = b > 255 ? 255 : b;
      dst[y * src.w + x] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
  }
}

}  // namespace little_arcade::ui
