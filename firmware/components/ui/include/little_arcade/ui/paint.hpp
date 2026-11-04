#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

#include "little_arcade/ui/a8_raster.hpp"

namespace little_arcade::ui {

// Software painter for multi-colour procedural art (creatures, eggs, tiles).
// Pure C++, no LVGL dependency, so it is host-tested. It is meant for
// one-off image building outside the frame loop, never per frame.
//
// Pixels are straight (non-premultiplied) 0xAARRGGBB, which is LVGL's
// LV_COLOR_FORMAT_ARGB8888 in memory on little-endian targets.
struct Canvas {
  uint32_t* px = nullptr;
  int w = 0;
  int h = 0;

  void clear() { std::memset(px, 0, sizeof(uint32_t) * static_cast<std::size_t>(w) * static_cast<std::size_t>(h)); }
  uint32_t at(int x, int y) const { return px[y * w + x]; }
};

// Source-over for straight alpha. `a` is the source alpha (0..255).
inline uint32_t blend_over(uint32_t dst, uint32_t rgb, uint32_t a) {
  if (a == 0) return dst;
  const uint32_t da = dst >> 24;
  if (a >= 255 || da == 0) return (a << 24) | (rgb & 0xFFFFFFu);
  const uint32_t dw = da * (255u - a) / 255u;  // destination weight
  const uint32_t oa = a + dw;
  uint32_t out = oa << 24;
  for (int shift = 0; shift <= 16; shift += 8) {
    const uint32_t s = (rgb >> shift) & 0xFFu, d = (dst >> shift) & 0xFFu;
    out |= ((s * a + d * dw + oa / 2) / oa) << shift;
  }
  return out;
}

// Paints `rgb` at opacity `opa` wherever `inside(x, y)` holds, inside the box
// [x0, x1) x [y0, y1) (clamped to the canvas). Coverage is supersampled
// ss x ss per pixel for antialiased edges. The four corner sub-samples are
// tested first: when they agree the pixel is treated as fully in or out, so
// only edge pixels pay for all ss*ss samples (features narrower than about
// one pixel are not used by the art).
template <typename Inside>
void paint(Canvas& c, float x0, float y0, float x1, float y1, uint32_t rgb, uint8_t opa, Inside inside, int ss = 4) {
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
      d = blend_over(d, rgb, a);
    }
  }
}

// Ellipse with radii (rx, ry) rotated by `rot` radians around its centre,
// with the trigonometry and reciprocals computed once.
struct Ellipse {
  float cx, cy, cs, sn, irx2, iry2;
  Ellipse(float cx_, float cy_, float rx, float ry, float rot = 0.0f)
      : cx(cx_), cy(cy_), cs(std::cos(rot)), sn(std::sin(rot)), irx2(1.0f / (rx * rx)), iry2(1.0f / (ry * ry)) {}
  bool contains(float x, float y) const {
    const float dx = x - cx, dy = y - cy;
    const float u = dx * cs + dy * sn, v = -dx * sn + dy * cs;
    return u * u * irx2 + v * v * iry2 <= 1.0f;
  }
};

inline bool in_ellipse(float x, float y, float cx, float cy, float rx, float ry, float rot = 0.0f) {
  return Ellipse(cx, cy, rx, ry, rot).contains(x, y);
}

// Stroke of radius-r circle arc between angles a0..a1 (radians, y down, a0 < a1).
inline bool in_arc(float x, float y, float cx, float cy, float r, float half_width, float a0, float a1) {
  const float dx = x - cx, dy = y - cy;
  const float d = std::sqrt(dx * dx + dy * dy);
  if (d < r - half_width || d > r + half_width) return false;
  float a = std::atan2(dy, dx);
  constexpr float kTwoPi = 6.2831853f;
  while (a < a0) a += kTwoPi;
  return a <= a1;
}

// Polyline / polygon outline sampled from lines and curves. Fixed capacity,
// no heap. Used for filled paths (in_polygon) and round-capped strokes.
struct Path {
  static constexpr int kMax = 96;
  float pts[2 * kMax];
  int n = 0;

  Path& move(float x, float y) {
    n = 0;
    return line(x, y);
  }
  Path& line(float x, float y) {
    if (n < kMax) {
      pts[2 * n] = x;
      pts[2 * n + 1] = y;
      ++n;
    }
    return *this;
  }
  Path& quad(float cx, float cy, float x, float y, int segs = 10) {
    const float x0 = pts[2 * (n - 1)], y0 = pts[2 * (n - 1) + 1];
    for (int i = 1; i <= segs; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(segs), u = 1.0f - t;
      line(u * u * x0 + 2 * u * t * cx + t * t * x, u * u * y0 + 2 * u * t * cy + t * t * y);
    }
    return *this;
  }
  Path& cubic(float c1x, float c1y, float c2x, float c2y, float x, float y, int segs = 12) {
    const float x0 = pts[2 * (n - 1)], y0 = pts[2 * (n - 1) + 1];
    for (int i = 1; i <= segs; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(segs), u = 1.0f - t;
      const float b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
      line(b0 * x0 + b1 * c1x + b2 * c2x + b3 * x, b0 * y0 + b1 * c1y + b2 * c2y + b3 * y);
    }
    return *this;
  }
  // Circular arc from angle a0 to a1 (either direction), appended as points.
  Path& arc(float cx, float cy, float r, float a0, float a1, int segs = 16) {
    for (int i = 0; i <= segs; ++i) {
      const float a = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(segs);
      line(cx + std::cos(a) * r, cy + std::sin(a) * r);
    }
    return *this;
  }

  bool filled(float x, float y) const { return in_polygon(x, y, pts, n); }
  bool stroked(float x, float y, float half_width, bool closed = false) const {
    const float hw2 = half_width * half_width;
    auto near = [&](int i, int j) {
      const float ax = pts[2 * i], ay = pts[2 * i + 1], bx = pts[2 * j], by = pts[2 * j + 1];
      if (x < (ax < bx ? ax : bx) - half_width || x > (ax > bx ? ax : bx) + half_width ||
          y < (ay < by ? ay : by) - half_width || y > (ay > by ? ay : by) + half_width) {
        return false;
      }
      return seg_dist2(x, y, ax, ay, bx, by) <= hw2;
    };
    for (int i = 1; i < n; ++i) {
      if (near(i - 1, i)) return true;
    }
    return closed && n > 1 && near(n - 1, 0);
  }
  // Bounding box grown by `pad`.
  void bounds(float pad, float& x0, float& y0, float& x1, float& y1) const {
    x0 = y0 = 1e9f;
    x1 = y1 = -1e9f;
    for (int i = 0; i < n; ++i) {
      x0 = pts[2 * i] < x0 ? pts[2 * i] : x0;
      x1 = pts[2 * i] > x1 ? pts[2 * i] : x1;
      y0 = pts[2 * i + 1] < y0 ? pts[2 * i + 1] : y0;
      y1 = pts[2 * i + 1] > y1 ? pts[2 * i + 1] : y1;
    }
    x0 -= pad;
    y0 -= pad;
    x1 += pad;
    y1 += pad;
  }
};

// Convenience painters in the style of a 2D canvas API.
inline void fill_circle(Canvas& c, float cx, float cy, float r, uint32_t rgb, uint8_t opa = 255) {
  paint(c, cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, rgb, opa,
        [=](float x, float y) { return in_circle(x, y, cx, cy, r); });
}
inline void stroke_circle(Canvas& c, float cx, float cy, float r, float width, uint32_t rgb) {
  const float hw = width * 0.5f;
  paint(c, cx - r - hw - 1, cy - r - hw - 1, cx + r + hw + 1, cy + r + hw + 1, rgb, 255, [=](float x, float y) {
    return in_circle(x, y, cx, cy, r + hw) && !in_circle(x, y, cx, cy, r - hw);
  });
}
inline void fill_ellipse(Canvas& c, float cx, float cy, float rx, float ry, float rot, uint32_t rgb, uint8_t opa = 255) {
  const float m = (rx > ry ? rx : ry) + 1;
  const Ellipse e(cx, cy, rx, ry, rot);
  paint(c, cx - m, cy - m, cx + m, cy + m, rgb, opa, [&e](float x, float y) { return e.contains(x, y); });
}
inline void stroke_ellipse(Canvas& c, float cx, float cy, float rx, float ry, float rot, float width, uint32_t rgb) {
  const float hw = width * 0.5f, m = (rx > ry ? rx : ry) + hw + 1;
  const Ellipse outer(cx, cy, rx + hw, ry + hw, rot), inner(cx, cy, rx - hw, ry - hw, rot);
  paint(c, cx - m, cy - m, cx + m, cy + m, rgb, 255,
        [&](float x, float y) { return outer.contains(x, y) && !inner.contains(x, y); });
}
// Filled ellipse with an outline, the most common creature primitive.
inline void blob(Canvas& c, float cx, float cy, float rx, float ry, float rot, uint32_t fill, uint32_t edge,
                 float width = 2.0f) {
  fill_ellipse(c, cx, cy, rx, ry, rot, fill);
  stroke_ellipse(c, cx, cy, rx, ry, rot, width, edge);
}
inline void fill_path(Canvas& c, const Path& p, uint32_t rgb, uint8_t opa = 255) {
  float x0, y0, x1, y1;
  p.bounds(1, x0, y0, x1, y1);
  paint(c, x0, y0, x1, y1, rgb, opa, [&p](float x, float y) { return p.filled(x, y); });
}
inline void stroke_path(Canvas& c, const Path& p, float width, uint32_t rgb, bool closed = false, uint8_t opa = 255) {
  const float hw = width * 0.5f;
  float x0, y0, x1, y1;
  p.bounds(hw + 1, x0, y0, x1, y1);
  paint(c, x0, y0, x1, y1, rgb, opa, [&p, hw, closed](float x, float y) { return p.stroked(x, y, hw, closed); });
}
// Five-pointed star, first point straight up.
inline void fill_star(Canvas& c, float cx, float cy, float ro, float ri, uint32_t rgb) {
  Path p;
  for (int i = 0; i < 10; ++i) {
    const float r = (i % 2) ? ri : ro, a = -1.5707963f + static_cast<float>(i) * 0.62831853f;
    p.line(cx + std::cos(a) * r, cy + std::sin(a) * r);
  }
  fill_path(c, p, rgb);
}
// Heart of size s centred on (cx, cy) (matches the mockups' heart()).
inline bool in_heart(float x, float y, float cx, float cy, float s) {
  const float tri[] = {cx - s * 0.5f, cy, cx + s * 0.5f, cy, cx, cy + s * 0.55f};
  return in_circle(x, y, cx - s * 0.25f, cy - s * 0.1f, s * 0.27f) ||
         in_circle(x, y, cx + s * 0.25f, cy - s * 0.1f, s * 0.27f) || in_polygon(x, y, tri, 3);
}

// Number of pixels with any coverage (alpha > 0). Used by tests.
inline int count_opaque(const Canvas& c) {
  int n = 0;
  for (int i = 0; i < c.w * c.h; ++i) n += (c.px[i] >> 24) != 0;
  return n;
}

}  // namespace little_arcade::ui
