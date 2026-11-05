#pragma once
// Procedural pictures for Beat Sharks, painted once into ARGB rasters.
// Header-only so the art test can rasterise them without LVGL.
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::beat_sharks::art {

namespace pal = ui::palette;

inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;
inline constexpr int kSharkSize = 46;
inline constexpr int kIconSize = 40;
inline constexpr int kFinW = 26;
inline constexpr int kFinH = 16;
inline constexpr std::size_t kTempoIcons = 3;

// Row colours: your sound (purple), taps (orange), drums (blue). Shark size,
// not just colour, tells the three sounds in a row apart.
inline constexpr uint32_t kRowColors[3] = {pal::kPurple, pal::kOrange, pal::kBlue};
inline constexpr uint32_t kRowTints[3] = {0xEDE3FB, 0xFFEBD6, 0xDDEBFB};
inline constexpr float kSoundScale[3] = {1.0f, 0.76f, 0.54f};  // sound 1 big .. sound 3 small
inline constexpr uint32_t kWater = 0xBFE7F7;
inline constexpr uint32_t kDeepWater = 0x7FC8E8;
inline constexpr uint32_t kBelly = 0xF4F7FB;
inline constexpr uint32_t kRecordRed = 0xE53935;
inline constexpr uint32_t kPlayGreen = 0x3BB273;

// A friendly shark facing right, centred on (cx, cy) at scale k (1 = 42 px
// wide). `hat` adds the orange jar lid worn by the "your sound" row.
inline void paint_shark_at(ui::Canvas& c, float cx, float cy, float k, uint32_t rgb, bool hat) {
  const float rx = 17.0f * k, ry = 10.0f * k;
  const float edge = k > 0.7f ? 2.0f : 1.5f;
  ui::Path tail;
  tail.move(cx - rx * 0.7f, cy).line(cx - rx - 8.0f * k, cy - 9.0f * k).quad(cx - rx - 3.0f * k, cy, cx - rx - 8.0f * k,
                                                                             cy + 9.0f * k);
  ui::fill_path(c, tail, rgb);
  ui::stroke_path(c, tail, edge, pal::kInk, true);
  ui::Path fin;
  fin.move(cx - 10.0f * k, cy - ry + 2.0f * k).quad(cx - 8.0f * k, cy - ry - 6.0f * k, cx - 4.0f * k, cy - ry - 9.0f * k)
      .line(cx + 2.0f * k, cy - ry + 2.0f * k);
  ui::fill_path(c, fin, rgb);
  ui::stroke_path(c, fin, edge, pal::kInk, true);
  ui::blob(c, cx, cy, rx, ry, 0, rgb, pal::kInk, edge);
  ui::fill_ellipse(c, cx + 2.0f * k, cy + ry * 0.45f, rx * 0.7f, ry * 0.38f, 0, kBelly);
  const float er = 3.2f * k < 1.8f ? 1.8f : 3.2f * k;
  const float ex = cx + rx * 0.5f, ey = cy - ry * 0.25f;
  ui::fill_circle(c, ex, ey, er, pal::kWhite);
  ui::fill_circle(c, ex + er * 0.25f, ey, er * 0.6f, pal::kInk);
  ui::stroke_path(c, ui::Path{}.arc(cx + rx * 0.55f, cy + ry * 0.05f, ry * 0.45f, 0.5f, 1.9f, 8), k > 0.7f ? 1.6f : 1.2f,
                  pal::kInk);
  if (hat) {
    const float x0 = cx + 2.0f * k, x1 = cx + 13.0f * k, y0 = cy - ry - 3.5f * k, y1 = cy - ry + 1.5f * k;
    ui::paint(c, x0 - 1, y0 - 1, x1 + 1, y1 + 1, pal::kInk, 255,
              [=](float x, float y) { return ui::in_rect(x, y, x0 - 1, y0 - 1, x1 + 1, y1 + 1); });
    ui::paint(c, x0, y0, x1, y1, pal::kOrange, 255, [=](float x, float y) { return ui::in_rect(x, y, x0, y0, x1, y1); });
  }
}

// Shark for `row` (0..2) and `sound` (1..3) centred in a kSharkSize canvas.
inline void paint_shark(ui::Canvas& c, std::size_t row, uint8_t sound) {
  const float k = kSoundScale[(sound == 0 ? 0 : sound - 1) % 3];
  paint_shark_at(c, c.w * 0.5f + 4.0f * k, c.h * 0.5f + 4.5f * k, k, kRowColors[row % 3], row == 0);
}

// Empty cell: calm water with a faint bubble.
inline void paint_bubble(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_circle(c, cx - 3, cy + 4, 5.0f, pal::kWhite, 120);
  ui::fill_circle(c, cx + 5, cy - 5, 3.0f, pal::kWhite, 100);
  ui::fill_circle(c, cx - 4.5f, cy + 2.5f, 1.4f, pal::kWhite, 220);
}

inline void paint_play(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::Path p;
  p.move(cx - 9, cy - 13).line(cx + 14, cy).line(cx - 9, cy + 13);
  ui::fill_path(c, p, kPlayGreen);
  ui::stroke_path(c, p, 2.0f, pal::kInk, true);
}

inline void paint_stop(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::paint(c, cx - 12, cy - 12, cx + 12, cy + 12, pal::kInk, 255,
            [=](float x, float y) { return ui::in_rect(x, y, cx - 12, cy - 12, cx + 12, cy + 12); });
  ui::paint(c, cx - 10, cy - 10, cx + 10, cy + 10, pal::kBlue, 255,
            [=](float x, float y) { return ui::in_rect(x, y, cx - 10, cy - 10, cx + 10, cy + 10); });
}

inline void paint_record_dot(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_circle(c, cx, cy, 14.0f, kRecordRed);
  ui::stroke_circle(c, cx, cy, 14.0f, 2.0f, pal::kInk);
  ui::fill_circle(c, cx - 5, cy - 5, 3.5f, pal::kWhite, 150);
}

inline void paint_record_stop(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::paint(c, cx - 9, cy - 9, cx + 9, cy + 9, kRecordRed, 255,
            [=](float x, float y) { return ui::in_rect(x, y, cx - 9, cy - 9, cx + 9, cy + 9); });
}

inline void paint_sparkle(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_star(c, cx, cy + 1, 15.0f, 6.5f, pal::kInk);
  ui::fill_star(c, cx, cy + 1, 12.5f, 5.2f, pal::kYellow);
  ui::fill_star(c, cx + 12, cy - 12, 5.0f, 2.0f, pal::kOrange);
  ui::fill_star(c, cx - 12, cy + 12, 4.0f, 1.6f, pal::kPink);
}

inline void paint_turtle(ui::Canvas& c) {
  const float cx = c.w * 0.5f - 2, cy = c.h * 0.5f + 4;
  for (int side = -1; side <= 1; side += 2) ui::fill_ellipse(c, cx + side * 9.0f, cy + 3, 4.0f, 3.0f, 0, pal::kGreen);
  ui::fill_circle(c, cx + 16, cy - 1, 5.0f, pal::kGreen);
  ui::stroke_circle(c, cx + 16, cy - 1, 5.0f, 1.5f, pal::kInk);
  ui::fill_circle(c, cx + 18, cy - 2, 1.3f, pal::kInk);
  ui::paint(c, cx - 15, cy - 15, cx + 15, cy + 1, pal::kInk, 255,
            [=](float x, float y) { return y <= cy && ui::in_ellipse(x, y, cx, cy, 15, 14); });
  ui::paint(c, cx - 13, cy - 13, cx + 13, cy, 0x6FBF5A, 255,
            [=](float x, float y) { return y <= cy - 1 && ui::in_ellipse(x, y, cx, cy - 1, 13, 12); });
  ui::fill_circle(c, cx, cy - 6, 3.5f, pal::kGreen);
}

inline void paint_walk_shark(ui::Canvas& c) { paint_shark_at(c, c.w * 0.5f + 3, c.h * 0.5f + 3, 0.8f, pal::kBlue, false); }

inline void paint_rabbit(ui::Canvas& c) {
  const float cx = c.w * 0.5f + 4, cy = c.h * 0.5f + 6;
  for (int k = 0; k < 3; ++k) {
    const float y = cy - 4 + k * 5.0f;
    ui::stroke_path(c, ui::Path{}.move(cx - 19, y).line(cx - 11, y), 2.0f, pal::kInk);
  }
  ui::fill_ellipse(c, cx + 5, cy - 16, 2.8f, 9.0f, -0.25f, pal::kInk);
  ui::fill_ellipse(c, cx + 11, cy - 15, 2.8f, 9.0f, 0.3f, pal::kInk);
  ui::fill_ellipse(c, cx + 5, cy - 16, 1.6f, 7.5f, -0.25f, pal::kPink);
  ui::fill_ellipse(c, cx + 11, cy - 15, 1.6f, 7.5f, 0.3f, pal::kPink);
  ui::blob(c, cx - 1, cy + 2, 10.0f, 7.5f, 0, pal::kWhite, pal::kInk, 1.6f);
  ui::blob(c, cx + 9, cy - 5, 6.5f, 6.0f, 0, pal::kWhite, pal::kInk, 1.6f);
  ui::fill_circle(c, cx + 11, cy - 6, 1.4f, pal::kInk);
}

// 0 turtle (slow), 1 walking shark, 2 rabbit (fast).
inline void paint_tempo_icon(ui::Canvas& c, std::size_t tempo) {
  switch (tempo % kTempoIcons) {
    case 0: paint_turtle(c); break;
    case 1: paint_walk_shark(c); break;
    default: paint_rabbit(c); break;
  }
}

// The playhead: a dorsal fin cutting the water.
inline void paint_fin(ui::Canvas& c) {
  ui::Path p;
  p.move(2, c.h - 2.0f).quad(c.w * 0.45f, c.h * 0.5f, c.w * 0.62f, 1.5f).quad(c.w * 0.62f, c.h * 0.6f, c.w - 2.0f, c.h - 2.0f);
  ui::fill_path(c, p, 0x5B6B82);
  ui::stroke_path(c, p, 1.6f, pal::kInk, true);
}

inline void paint_star(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_star(c, cx, cy + 1, c.w * 0.45f, c.w * 0.2f, pal::kYellow);
}

inline void paint_tile_art(ui::Canvas& c) {
  // Waves along the bottom.
  const float right = c.w - 4.0f, bottom = c.h - 4.0f;
  ui::paint(c, 4, 46, right, bottom, kDeepWater, 255, [=](float x, float y) {
    return y >= 51.0f + 2.5f * std::sin(x * 0.5f) && x >= 4 && x <= right && y <= bottom;
  });
  paint_shark_at(c, 26, 40, 0.95f, pal::kBlue, false);
  paint_shark_at(c, 60, 28, 0.72f, pal::kOrange, false);
  paint_shark_at(c, 88, 18, 0.52f, pal::kPurple, true);
  // Notes bubbling up.
  ui::fill_circle(c, 46, 12, 3.5f, pal::kPink);
  ui::stroke_path(c, ui::Path{}.move(49, 12).line(49, 2), 2.0f, pal::kPink);
  ui::fill_circle(c, 74, 52, 3.0f, pal::kWhite);
}

}  // namespace little_arcade::beat_sharks::art
