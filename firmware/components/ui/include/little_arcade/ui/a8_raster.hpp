#pragma once
#include <cstdint>

namespace little_arcade::ui {

// Antialiased alpha (A8) rasterizer for simple procedural icons. Pure C++, no
// LVGL dependency, so shapes are host-testable and the same maths can be reused
// by any renderer. Runs once at view creation, never per frame.
//
// `inside(x, y)` receives sample positions in pixel units (0..w, 0..h) and
// returns true when the sample is covered. Coverage is averaged over an
// ss x ss grid per pixel.
template <typename Inside>
void rasterize_a8(uint8_t* out, int w, int h, Inside inside, int ss = 4) {
  const float step = 1.0f / static_cast<float>(ss);
  const int total = ss * ss;
  for (int py = 0; py < h; ++py) {
    for (int px = 0; px < w; ++px) {
      int hits = 0;
      for (int sy = 0; sy < ss; ++sy) {
        for (int sx = 0; sx < ss; ++sx) {
          const float x = static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) * step;
          const float y = static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) * step;
          if (inside(x, y)) ++hits;
        }
      }
      out[py * w + px] = static_cast<uint8_t>((hits * 255 + total / 2) / total);
    }
  }
}

// Geometry helpers for `inside` predicates.
inline bool in_circle(float x, float y, float cx, float cy, float r) {
  const float dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}

inline bool in_rect(float x, float y, float x0, float y0, float x1, float y1) {
  return x >= x0 && x < x1 && y >= y0 && y < y1;
}

// Even-odd point-in-polygon test; `pts` is n (x, y) pairs.
inline bool in_polygon(float x, float y, const float* pts, int n) {
  bool in = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const float xi = pts[2 * i], yi = pts[2 * i + 1];
    const float xj = pts[2 * j], yj = pts[2 * j + 1];
    if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
  }
  return in;
}

// Distance from (x, y) to segment (ax, ay)-(bx, by), squared.
inline float seg_dist2(float x, float y, float ax, float ay, float bx, float by) {
  const float vx = bx - ax, vy = by - ay;
  const float len2 = vx * vx + vy * vy;
  float t = len2 > 0.0f ? ((x - ax) * vx + (y - ay) * vy) / len2 : 0.0f;
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  const float dx = x - (ax + t * vx), dy = y - (ay + t * vy);
  return dx * dx + dy * dy;
}

}  // namespace little_arcade::ui
