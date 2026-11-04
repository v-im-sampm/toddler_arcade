#pragma once
#include <cmath>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/ui/a8_raster.hpp"

namespace little_arcade::color_pop {

// Non-colour clue shapes (heart/wave/leaf/star) for rasterize_a8(), in pixels
// inside a b x b box. LVGL-free so the shapes are host-tested. Geometry
// follows docs/mockups/color_pop.html.
inline bool pattern_inside(Pattern p, float x, float y, float b) {
  using ui::in_circle;
  using ui::in_polygon;
  using ui::seg_dist2;
  const float cx = b * 0.5f;
  switch (p) {
    case Pattern::HEART: {
      const float s = b * 0.95f, cy = cx - s * 0.09f;
      const float tri[] = {cx - s * 0.5f, cy, cx + s * 0.5f, cy, cx, cy + s * 0.55f};
      return in_circle(x, y, cx - s * 0.25f, cy - s * 0.1f, s * 0.27f) ||
             in_circle(x, y, cx + s * 0.25f, cy - s * 0.1f, s * 0.27f) || in_polygon(x, y, tri, 3);
    }
    case Pattern::WAVE: {
      // Two sine strokes approximated by short round-capped segments.
      const float s = b * 0.9f, hw = s * 0.07f, x0 = cx - s * 0.5f;
      constexpr int kSegs = 16;
      constexpr float kTwoPi = 6.2831853f;
      const float offsets[2] = {-s * 0.22f, s * 0.22f};
      for (const float dy : offsets) {
        float px = x0, py = cx + dy;
        for (int i = 1; i <= kSegs; ++i) {
          const float t = static_cast<float>(i) / kSegs;
          const float nx = x0 + s * t, ny = cx + dy + std::sin(t * kTwoPi) * s * 0.15f;
          if (seg_dist2(x, y, px, py, nx, ny) <= hw * hw) return true;
          px = nx;
          py = ny;
        }
      }
      return false;
    }
    case Pattern::LEAF: {
      // Ellipse rotated 45 degrees with a transparent centre vein.
      const float s = b * 1.05f, dx = x - cx, dy = y - cx;
      const float u = 0.70710678f * (dx - dy), v = 0.70710678f * (dx + dy);
      const float a = s * 0.5f, bb = s * 0.28f;
      const bool body = (u * u) / (a * a) + (v * v) / (bb * bb) <= 1.0f;
      const bool vein = (v < 0 ? -v : v) < s * 0.03f && (u < 0 ? -u : u) < s * 0.36f;
      return body && !vein;
    }
    case Pattern::STAR: {
      const float s = b * 0.9f, cy = cx + b * 0.03f;
      // Unit star (outer 0.55, inner 0.24), first point straight up.
      static constexpr float kUnit[20] = {
          0.0f,     -0.55f,  0.1411f,  -0.1942f, 0.5231f,  -0.17f,   0.2283f, 0.0742f,
          0.3233f,  0.445f,  0.0f,     0.24f,    -0.3233f, 0.445f,   -0.2283f, 0.0742f,
          -0.5231f, -0.17f,  -0.1411f, -0.1942f};
      float pts[20];
      for (int i = 0; i < 10; ++i) {
        pts[2 * i] = cx + kUnit[2 * i] * s;
        pts[2 * i + 1] = cy + kUnit[2 * i + 1] * s;
      }
      return in_polygon(x, y, pts, 10);
    }
  }
  return false;
}

}  // namespace little_arcade::color_pop
