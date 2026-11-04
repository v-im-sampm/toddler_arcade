#pragma once
#include "little_arcade/ui/a8_raster.hpp"

namespace little_arcade::ui {

// Navigation icon shapes for rasterize_a8(). Coordinates are in pixels inside
// an s x s box. Pictures only: no text, so no reading is required.

inline bool home_icon(float x, float y, float s) {
  const float u = x / s, v = y / s;
  const float roof[] = {0.12f, 0.55f, 0.5f, 0.16f, 0.88f, 0.55f};
  const bool body = in_rect(u, v, 0.24f, 0.5f, 0.76f, 0.86f);
  const bool door = in_rect(u, v, 0.42f, 0.64f, 0.58f, 0.86f);
  return (in_polygon(u, v, roof, 3) || body) && !door;
}

inline bool mic_icon(float x, float y, float s) {
  const float u = x / s, v = y / s;
  // Capsule head.
  const bool head = in_rect(u, v, 0.38f, 0.22f, 0.62f, 0.48f) || in_circle(u, v, 0.5f, 0.22f, 0.12f) ||
                    in_circle(u, v, 0.5f, 0.48f, 0.12f);
  // Lower half ring (the cradle).
  const float dx = u - 0.5f, dy = v - 0.46f, d2 = dx * dx + dy * dy;
  const bool cradle = v >= 0.46f && d2 <= 0.25f * 0.25f && d2 >= 0.18f * 0.18f;
  const bool stem = in_rect(u, v, 0.46f, 0.7f, 0.54f, 0.82f);
  const bool base = in_rect(u, v, 0.32f, 0.8f, 0.68f, 0.88f);
  return head || cradle || stem || base;
}

}  // namespace little_arcade::ui
