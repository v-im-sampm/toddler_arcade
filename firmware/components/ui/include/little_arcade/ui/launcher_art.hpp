#pragma once
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::ui {

// Launcher logo: a rainbow arc in the four game colours with two clouds.
// Decoration only, nothing to read. Fills an 80 x 48 canvas.
inline constexpr int kLogoW = 80;
inline constexpr int kLogoH = 48;

inline void paint_logo(Canvas& c) {
  constexpr float kPi = 3.1415926f;
  const float cx = 40.0f, cy = 38.0f;
  const uint32_t cols[] = {palette::kRed, palette::kYellow, palette::kGreen, palette::kBlue};
  for (int i = 0; i < 4; ++i) {
    const float r = 34.0f - 7.0f * static_cast<float>(i);
    paint(c, 0, 0, static_cast<float>(c.w), cy + 1, cols[i], 255,
          [=](float x, float y) { return in_arc(x, y, cx, cy, r, 3.0f, kPi, 2 * kPi); });
  }
  fill_circle(c, 6, 40, 6, palette::kWhite);
  fill_circle(c, 74, 40, 6, palette::kWhite);
}

// Next-page arrow for the launcher pager (A8 shape inside an s x s box).
inline bool next_arrow_icon(float x, float y, float s) {
  const float tri[] = {s * 0.3f, s * 0.15f, s * 0.75f, s * 0.5f, s * 0.3f, s * 0.85f};
  return in_polygon(x, y, tri, 3);
}

}  // namespace little_arcade::ui
