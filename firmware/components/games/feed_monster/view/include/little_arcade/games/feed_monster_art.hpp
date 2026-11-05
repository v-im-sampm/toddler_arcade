#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::feed_monster::art {

namespace pal = ui::palette;

inline void paint_face(ui::Canvas& c, float cx, float cy, float s) {
  for (const float side : {-1.0f, 1.0f}) {
    const float ex = cx + side * 13.0f * s;
    ui::fill_circle(c, ex, cy, 7.0f * s, pal::kWhite);
    ui::stroke_circle(c, ex, cy, 7.0f * s, 1.5f * s, pal::kInk);
    ui::fill_circle(c, ex + s, cy + s, 3.8f * s, pal::kInk);
    ui::fill_circle(c, ex + 2.0f * s, cy - s, 1.4f * s, pal::kWhite);
    ui::fill_circle(c, cx + side * 29.0f * s, cy + 12.0f * s, 5.0f * s, pal::kCheek, 115);
  }
  ui::Path smile;
  smile.arc(cx, cy + 8.0f * s, 8.0f * s, 0.2f, 3.1415926f - 0.2f, 16);
  ui::stroke_path(c, smile, 2.0f * s, pal::kInk);
}

inline void paint_monster(ui::Canvas& c, float cx, float cy, float s = 1.0f) {
  const uint32_t body = pal::kPurple;
  ui::Path horns;
  horns.move(cx - 29 * s, cy - 28 * s).line(cx - 38 * s, cy - 47 * s).line(cx - 17 * s, cy - 37 * s);
  ui::stroke_path(c, horns, 9 * s, body);
  horns.move(cx + 29 * s, cy - 28 * s).line(cx + 38 * s, cy - 47 * s).line(cx + 17 * s, cy - 37 * s);
  ui::stroke_path(c, horns, 9 * s, body);
  ui::fill_circle(c, cx - 38 * s, cy - 47 * s, 4.5f * s, pal::kYellow);
  ui::stroke_circle(c, cx - 38 * s, cy - 47 * s, 4.5f * s, 1.5f * s, pal::kInk);
  ui::fill_circle(c, cx + 38 * s, cy - 47 * s, 4.5f * s, pal::kYellow);
  ui::stroke_circle(c, cx + 38 * s, cy - 47 * s, 4.5f * s, 1.5f * s, pal::kInk);
  ui::blob(c, cx, cy + 3 * s, 49 * s, 47 * s, 0, body, pal::kInk, 2 * s);
  ui::fill_ellipse(c, cx - 23 * s, cy - 18 * s, 10 * s, 5 * s, -0.5f, pal::kWhite, 100);
  paint_face(c, cx, cy - 7 * s, s);
  ui::blob(c, cx, cy + 31 * s, 28 * s, 14 * s, 0, 0xD6BDF0, pal::kInk, 1.5f * s);
  for (const float side : {-1.0f, 1.0f}) {
    ui::fill_ellipse(c, cx + side * 28 * s, cy + 45 * s, 11 * s, 5 * s, side * 0.18f, 0x7A4FB8);
  }
}

inline void paint_snack(ui::Canvas& c, float cx, float cy, float s = 1.0f) {
  ui::fill_circle(c, cx, cy + 2 * s, 9 * s, pal::kOrange);
  ui::stroke_circle(c, cx, cy + 2 * s, 9 * s, 1.5f * s, pal::kInk);
  ui::Path stem;
  stem.move(cx, cy - 6 * s).line(cx + 2 * s, cy - 12 * s);
  ui::stroke_path(c, stem, 2 * s, pal::kInk);
  ui::blob(c, cx + 5 * s, cy - 9 * s, 5 * s, 2.5f * s, -0.5f, pal::kGreen, pal::kInk, s);
}

inline void paint_bowl(ui::Canvas& c) {
  ui::blob(c, 54, 59, 43, 26, 0, 0xF4A261, pal::kInk, 2);
  ui::fill_ellipse(c, 54, 49, 44, 10, 0, 0xFFD1A8);
  ui::stroke_ellipse(c, 54, 49, 44, 10, 0, 2, pal::kInk);
  ui::fill_ellipse(c, 54, 49, 36, 5, 0, 0xD9762A);
  paint_snack(c, 32, 29, 0.86f);
  ui::fill_circle(c, 57, 28, 8, pal::kYellow);
  ui::stroke_circle(c, 57, 28, 8, 1.5f, pal::kInk);
  ui::fill_ellipse(c, 79, 32, 8, 6, -0.2f, pal::kGreen);
  ui::stroke_ellipse(c, 79, 32, 8, 6, -0.2f, 1.5f, pal::kInk);
}

inline constexpr int kMonsterW = 144;
inline constexpr int kMonsterH = 120;
inline constexpr int kBowlW = 108;
inline constexpr int kBowlH = 90;
inline constexpr int kSnackW = 24;
inline constexpr int kSnackH = 24;
inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;

inline void paint_monster_image(ui::Canvas& c) {
  paint_monster(c, kMonsterW * 0.5f, 64, 1.0f);
}

inline void paint_bowl_image(ui::Canvas& c) { paint_bowl(c); }

inline void paint_snack_image(ui::Canvas& c) { paint_snack(c, 12, 12, 0.8f); }

inline void paint_tile_art(ui::Canvas& c) {
  paint_monster(c, 70, 43, 0.42f);
  paint_snack(c, 29, 42, 0.85f);
}

}  // namespace little_arcade::feed_monster::art
