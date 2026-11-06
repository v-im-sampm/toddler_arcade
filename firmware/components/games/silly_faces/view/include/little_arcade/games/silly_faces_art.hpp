#pragma once

#include <cstdint>
#include <initializer_list>

#include "little_arcade/games/silly_faces.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::silly_faces::art {

namespace pal = ui::palette;
inline constexpr int kFaceW = 128;
inline constexpr int kFaceH = 136;
inline constexpr uint32_t kBodyColors[kBodyColorCount] = {
    0x9B7BEA, 0x49C3B5, 0xF09A62, 0xE879A5,
};

inline void paint_face(ui::Canvas& c, Emotion eyes, Emotion mouth, uint8_t body_color, Topper topper) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f + 3;
  const uint32_t body = kBodyColors[body_color % kBodyColorCount];
  if (topper == Topper::ANTENNAE) {
    ui::stroke_path(c, ui::Path{}.move(cx, cy - 46).line(cx - 8, cy - 62), 4, pal::kInk);
    ui::stroke_path(c, ui::Path{}.move(cx, cy - 46).line(cx + 8, cy - 62), 4, pal::kInk);
    ui::fill_circle(c, cx - 8, cy - 63, 4, pal::kYellow);
    ui::fill_circle(c, cx + 8, cy - 63, 4, pal::kPink);
  } else if (topper == Topper::HORNS) {
    ui::fill_path(c, ui::Path{}.move(cx - 31, cy - 31).line(cx - 38, cy - 52).line(cx - 16, cy - 39), pal::kYellow);
    ui::fill_path(c, ui::Path{}.move(cx + 31, cy - 31).line(cx + 38, cy - 52).line(cx + 16, cy - 39), pal::kYellow);
  } else {
    ui::fill_circle(c, cx - 15, cy - 43, 9, body);
    ui::fill_circle(c, cx, cy - 49, 10, body);
    ui::fill_circle(c, cx + 15, cy - 43, 9, body);
  }
  ui::blob(c, cx, cy, 47, 48, 0, body, pal::kInk, 3);
  ui::fill_circle(c, cx - 18, cy + 2, 5, 0xF6A2B0, 130);
  ui::fill_circle(c, cx + 18, cy + 2, 5, 0xF6A2B0, 130);
  const bool sleepy = eyes == Emotion::SLEEPY;
  const bool surprised = eyes == Emotion::SURPRISED;
  for (int side : {-1, 1}) {
    const float ex = cx + static_cast<float>(side) * 16.0f;
    if (sleepy) {
      ui::stroke_path(c, ui::Path{}.move(ex - 6, cy - 8).quad(ex, cy - 12, ex + 6, cy - 8), 3, pal::kInk);
    } else {
      ui::fill_ellipse(c, ex, cy - 8, 6.0f, surprised ? 8.0f : 7.0f, 0.0f, pal::kWhite);
      ui::stroke_ellipse(c, ex, cy - 8, 6.0f, surprised ? 8.0f : 7.0f, 0.0f, 1.4f, pal::kInk);
      ui::fill_circle(c, ex + 1, cy - 8, 2.5f, pal::kInk);
      ui::fill_circle(c, ex + 2, cy - 9, 0.8f, pal::kWhite);
    }
  }
  if (mouth == Emotion::SURPRISED) {
    ui::fill_ellipse(c, cx, cy + 17, 7, 9, 0, pal::kInk);
    ui::fill_ellipse(c, cx, cy + 15, 3, 4, 0, pal::kPink);
  } else if (mouth == Emotion::SAD) {
    ui::stroke_path(c, ui::Path{}.arc(cx, cy + 27, 11, 3.35f, 6.05f, 12), 3, pal::kInk);
  } else if (mouth == Emotion::SLEEPY) {
    ui::stroke_path(c, ui::Path{}.move(cx - 8, cy + 17).line(cx + 8, cy + 17), 3, pal::kInk);
  } else {
    ui::stroke_path(c, ui::Path{}.arc(cx, cy + 10, 11, 0.2f, 2.94f, 12), 3, pal::kInk);
  }
}

inline void paint_tile_art(ui::Canvas& c) {
  paint_face(c, Emotion::HAPPY, Emotion::SURPRISED, 0, Topper::ANTENNAE);
}

}  // namespace little_arcade::silly_faces::art
