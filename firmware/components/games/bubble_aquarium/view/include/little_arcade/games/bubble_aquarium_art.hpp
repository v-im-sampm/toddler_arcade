#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "little_arcade/games/bubble_aquarium.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::bubble_aquarium::art {

namespace pal = ui::palette;
inline constexpr int kFishW = 80;
inline constexpr int kFishH = 64;
inline constexpr int kBubbleSize = 32;
inline constexpr int kFoodSize = 24;
inline constexpr int kVisitorW = 72;
inline constexpr int kVisitorH = 64;
inline constexpr uint32_t kWater = 0xA9DBFB;
inline constexpr uint32_t kDeepWater = 0x6FBDF2;

inline void paint_fish(ui::Canvas& c, FishType type) {
  const float cx = c.w * 0.5f;
  const float cy = c.h * 0.5f;
  const bool puffy = type == FishType::PUFFY;
  const bool longfin = type == FishType::LONGFIN;
  const uint32_t body = type == FishType::STRIPEY ? pal::kOrange : (puffy ? pal::kPink : pal::kBlue);
  const float rx = longfin ? 25.0f : (puffy ? 19.0f : 23.0f);
  const float ry = puffy ? 19.0f : 14.0f;
  ui::Path tail;
  tail.move(cx - rx + 2, cy).line(cx - rx - (longfin ? 17 : 12), cy - 11)
      .line(cx - rx - (longfin ? 17 : 12), cy + 11);
  ui::fill_path(c, tail, body);
  ui::stroke_path(c, tail, 2, pal::kInk, true);
  ui::blob(c, cx, cy, rx, ry, 0, body, pal::kInk, 2);
  if (type == FishType::STRIPEY) {
    for (int i = -1; i <= 1; ++i) {
      ui::stroke_path(c, ui::Path{}.move(cx + i * 8.0f, cy - 9).line(cx + i * 8.0f, cy + 9), 2, 0xFFF3D6);
    }
  } else if (puffy) {
    ui::fill_circle(c, cx - 8, cy - 7, 2.2f, pal::kWhite);
    ui::fill_circle(c, cx + 2, cy - 10, 2.2f, pal::kWhite);
    ui::fill_circle(c, cx - 2, cy + 2, 2.2f, pal::kWhite);
  } else {
    ui::Path fin;
    fin.move(cx - 3, cy - 8).quad(cx + 2, cy - 24, cx + 12, cy - 10);
    ui::stroke_path(c, fin, 3, 0x9DB8FA, false);
  }
  ui::fill_circle(c, cx + rx * 0.45f, cy - 4, 4.2f, pal::kWhite);
  ui::fill_circle(c, cx + rx * 0.48f, cy - 4, 2.0f, pal::kInk);
  ui::stroke_path(c, ui::Path{}.arc(cx + rx * 0.64f, cy + 3, 4, 0.4f, 2.7f, 8), 1.5f, pal::kInk);
}

inline void paint_bubble(ui::Canvas& c) {
  const float r = c.w * 0.31f;
  ui::fill_circle(c, c.w * 0.5f, c.h * 0.5f, r, 0xDDF4FF, 90);
  ui::stroke_circle(c, c.w * 0.5f, c.h * 0.5f, r, 2, pal::kWhite);
  ui::fill_circle(c, c.w * 0.39f, c.h * 0.37f, 2.5f, pal::kWhite);
}

inline void paint_food(ui::Canvas& c) {
  ui::fill_circle(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.3f, pal::kYellow);
  ui::stroke_circle(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.3f, 2, pal::kInk);
  ui::stroke_path(c, ui::Path{}.move(c.w * 0.5f, c.h * 0.23f).line(c.w * 0.54f, c.h * 0.08f), 2, pal::kInk);
  ui::fill_ellipse(c, c.w * 0.65f, c.h * 0.22f, 4, 2, -0.5f, pal::kGreen);
}

inline void paint_visitor(ui::Canvas& c, Visitor visitor) {
  const float cx = c.w * 0.5f, cy = c.h * 0.55f;
  const uint32_t shell = visitor == Visitor::TURTLE ? pal::kGreen :
                         (visitor == Visitor::SEAHORSE ? pal::kYellow : pal::kPink);
  if (visitor == Visitor::TURTLE) {
    ui::blob(c, cx - 5, cy + 3, 22, 14, 0, shell, pal::kInk, 2);
    ui::fill_circle(c, cx + 21, cy, 7, shell);
    ui::fill_circle(c, cx + 23, cy - 2, 1.6f, pal::kInk);
    for (int side : {-1, 1}) ui::fill_ellipse(c, cx + side * 13, cy + 17, 7, 3, 0, shell);
  } else if (visitor == Visitor::SEAHORSE) {
    ui::blob(c, cx, cy, 15, 23, 0, shell, pal::kInk, 2);
    ui::fill_circle(c, cx + 9, cy - 16, 8, shell);
    ui::stroke_path(c, ui::Path{}.move(cx + 12, cy - 12).line(cx + 23, cy - 10), 3, pal::kInk);
    ui::stroke_path(c, ui::Path{}.arc(cx - 5, cy + 19, 8, 0, 4.5f, 10), 3, pal::kInk);
  } else {
    ui::fill_circle(c, cx, cy - 1, 20, shell);
    for (int i = -1; i <= 1; ++i)
      ui::stroke_path(c, ui::Path{}.move(cx + i * 12.0f, cy + 12).line(cx + i * 16.0f, cy + 26), 2, pal::kPurple);
    ui::fill_circle(c, cx - 7, cy - 3, 2, pal::kInk);
    ui::fill_circle(c, cx + 7, cy - 3, 2, pal::kInk);
  }
}

}  // namespace little_arcade::bubble_aquarium::art
