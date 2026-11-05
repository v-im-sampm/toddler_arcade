#pragma once

#include <cstddef>
#include <cstdint>

#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::rainbow_piano::art {

namespace pal = ui::palette;

inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;
inline constexpr int kIconSize = 28;
inline constexpr uint32_t kKeyColors[] = {pal::kRed, pal::kOrange, pal::kYellow, pal::kGreen, pal::kPurple};
inline constexpr std::size_t kKeyShapeCount = 5;

inline void paint_key_shape(ui::Canvas& c, std::size_t key, uint32_t rgb) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  const float r = (c.w < c.h ? c.w : c.h) * 0.42f;
  switch (key % kKeyShapeCount) {
    case 0:
      ui::fill_circle(c, cx, cy, r, rgb);
      break;
    case 1:
      ui::paint(c, cx - r, cy - r, cx + r, cy + r, rgb,
                255, [=](float x, float y) { return ui::in_rect(x, y, cx - r, cy - r, cx + r, cy + r); });
      break;
    case 2: {
      ui::Path p;
      p.move(cx, cy - r).line(cx + r, cy + r).line(cx - r, cy + r);
      ui::fill_path(c, p, rgb);
      break;
    }
    case 3:
      ui::paint(c, cx - r, cy - r, cx + r, cy + r, rgb, 255,
                [=](float x, float y) { return ui::in_heart(x, y, cx, cy - r * 0.1f, r * 1.7f); });
      break;
    default:
      ui::fill_star(c, cx, cy, r, r * 0.44f, rgb);
      break;
  }
}

inline void paint_instrument_icon(ui::Canvas& c, std::size_t instrument, uint32_t rgb = pal::kInk) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  if (instrument == 0) {
    ui::Path bell;
    bell.move(cx - 11, cy + 5).quad(cx - 7, cy + 1, cx - 7, cy - 5).arc(cx, cy - 5, 7, 3.14159265f, 0, 12)
        .quad(cx + 7, cy + 1, cx + 11, cy + 5).line(cx - 11, cy + 5);
    ui::fill_path(c, bell, rgb);
    ui::fill_circle(c, cx, cy + 9, 2.5f, rgb);
  } else if (instrument == 1) {
    ui::Path flute;
    flute.move(cx - 12, cy + 8).line(cx + 12, cy - 8);
    ui::stroke_path(c, flute, 5, rgb);
    static constexpr float kHoles[] = {-5.0f, 2.0f, 9.0f};
    for (const float dx : kHoles) ui::fill_circle(c, cx + dx, cy - dx * 0.67f + 1, 1.4f, pal::kWhite);
  } else {
    ui::fill_ellipse(c, cx, cy + 2, 11, 9, 0, pal::kOrange);
    ui::stroke_ellipse(c, cx, cy + 2, 11, 9, 0, 2, rgb);
    ui::stroke_ellipse(c, cx, cy - 3, 11, 4, 0, 2, rgb);
    ui::Path stick;
    stick.move(cx - 7, cy - 12).line(cx + 6, cy + 10);
    ui::stroke_path(c, stick, 3, rgb);
  }
}

inline void paint_tile_art(ui::Canvas& c) {
  for (int band = 0; band < 5; ++band) {
    const float radius = 43.0f - band * 6.0f;
    const uint32_t color = kKeyColors[band];
    ui::paint(c, 9, 0, 99, 35, color, 255, [=](float x, float y) {
      return ui::in_arc(x, y, c.w * 0.5f, 42, radius, 2.0f, 3.14159265f, 6.2831853f);
    });
  }

  const float key_w = 16.0f, key_h = 40.0f, gap = 3.0f;
  const float left = (static_cast<float>(c.w) - (key_w * 5 + gap * 4)) * 0.5f;
  const float top = 21.0f;
  for (std::size_t i = 0; i < 5; ++i) {
    const float x = left + i * (key_w + gap);
    ui::paint(c, x, top, x + key_w, top + key_h, pal::kInk, 255,
              [=](float px, float py) { return ui::in_rect(px, py, x, top, x + key_w, top + key_h); });
    ui::paint(c, x + 1, top + 1, x + key_w - 1, top + key_h - 1, kKeyColors[i], 255,
              [=](float px, float py) { return ui::in_rect(px, py, x + 1, top + 1, x + key_w - 1, top + key_h - 1); });
    ui::Canvas icon{c.px + static_cast<int>(top + 9) * c.w + static_cast<int>(x + 4), 8, 8};
    paint_key_shape(icon, i, pal::kWhite);
  }
}

inline constexpr int kBannerW = 108;
inline constexpr int kBannerH = 50;

inline void paint_banner(ui::Canvas& c) {
  for (int band = 0; band < 5; ++band) {
    const float radius = 42.0f - band * 6.0f;
    const uint32_t color = kKeyColors[band];
    ui::paint(c, 7, 0, 101, 48, color, 255, [=](float x, float y) {
      return ui::in_arc(x, y, c.w * 0.5f, 47, radius, 2.4f, 3.14159265f, 6.2831853f);
    });
  }
}

}  // namespace little_arcade::rainbow_piano::art
