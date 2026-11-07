#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "little_arcade/games/bubble_aquarium.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/tokens.hpp"

namespace little_arcade::bubble_aquarium::art {

namespace pal = ui::palette;
namespace tok = ui::tok;
inline constexpr int kFishW = 80;
inline constexpr int kFishH = 64;
inline constexpr int kBubbleSize = 48;
inline constexpr int kFoodSize = 16;
inline constexpr int kVisitorW = 72;
inline constexpr int kVisitorH = 64;
inline constexpr int kPlantW = 56;
inline constexpr int kPlantH = 84;
inline constexpr int kMoonSize = 40;
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
  ui::fill_ellipse(c, cx + 2, cy + ry * 0.78f, rx * 0.72f, 4, 0, pal::kInk, 28);
  ui::Path tail;
  tail.move(cx - rx + 2, cy).line(cx - rx - (longfin ? 17 : 12), cy - 11)
      .line(cx - rx - (longfin ? 17 : 12), cy + 11);
  ui::fill_path(c, tail, body);
  ui::stroke_path(c, tail, 2, pal::kInk, true);
  ui::blob(c, cx, cy, rx, ry, 0, body, pal::kInk, 2);
  ui::fill_ellipse(c, cx + 5, cy - ry * 0.48f, rx * 0.53f, ry * 0.28f, -0.15f, pal::kWhite, 100);
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
  ui::fill_circle(c, cx + rx * 0.46f, cy - 4.8f, 0.7f, pal::kWhite);
  ui::fill_ellipse(c, cx + rx * 0.83f, cy + 8, 3.2f, 2.0f, 0, tok::kPink.light, 200);
  ui::stroke_path(c, ui::Path{}.arc(cx + rx * 0.64f, cy + 3, 4, 0.4f, 2.7f, 8), 1.5f, pal::kInk);
}

inline void paint_bubble(ui::Canvas& c) {
  const float r = c.w * 0.44f;
  ui::fill_circle(c, c.w * 0.5f, c.h * 0.5f, r, 0xDDF4FF, 44);
  ui::stroke_circle(c, c.w * 0.5f, c.h * 0.5f, r, 2, pal::kWhite);
  ui::stroke_circle(c, c.w * 0.5f, c.h * 0.5f, r - 3, 1, 0x9FE5F6);
  ui::fill_ellipse(c, c.w * 0.39f, c.h * 0.35f, 4.2f, 2.2f, -0.7f, pal::kWhite, 240);
}

inline void paint_food(ui::Canvas& c) {
  ui::fill_ellipse(c, c.w * 0.5f, c.h * 0.6f, c.w * 0.32f, c.h * 0.27f, 0, 0xC98A4B);
  ui::stroke_ellipse(c, c.w * 0.5f, c.h * 0.6f, c.w * 0.32f, c.h * 0.27f, 0, 1.4f, 0x8E5A2B);
  ui::fill_circle(c, c.w * 0.38f, c.h * 0.48f, 1.1f, tok::kYellow.light);
}

inline void paint_plant(ui::Canvas& c) {
  const float ground = static_cast<float>(c.h - 4);
  const uint32_t dark = tok::kGreen.shadow;
  const uint32_t leaf = tok::kGreen.base;
  const uint32_t light = tok::kGreen.light;
  const float stems[] = {11, 24, 38, 48};
  for (int i = 0; i < 4; ++i) {
    const float x = stems[i];
    const float top = 14.0f + static_cast<float>((i * 13) % 25);
    ui::Path stem;
    stem.move(x, ground).quad(x + (i % 2 ? 10.0f : -8.0f), (ground + top) * 0.55f, x, top);
    ui::stroke_path(c, stem, 3.0f, dark);
    for (int side : {-1, 1}) {
      const float y = top + static_cast<float>((i + side + 2) * 6);
      if (y >= ground - 8) continue;
      ui::Path blade;
      blade.move(x, y + 9).quad(x + side * 13.0f, y - 2, x + side * 17.0f, y - 9);
      ui::fill_path(c, blade, leaf);
      ui::stroke_path(c, blade, 1.4f, dark, true);
      ui::stroke_path(c, ui::Path{}.move(x, y + 7).line(x + side * 13.0f, y - 5), 1.0f, light);
    }
  }
}

inline void paint_moon(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_circle(c, cx, cy, 14, tok::kYellow.base);
  ui::fill_circle(c, cx + 6, cy - 5, 12, 0xFFF3D6);
  ui::stroke_path(c, ui::Path{}.arc(cx + 1, cy, 13, -1.45f, 1.35f, 14), 2, tok::kOrange.shadow);
  ui::fill_circle(c, cx + 12, cy - 12, 2, tok::kPurple.light);
  ui::fill_circle(c, cx + 14, cy + 10, 1.4f, tok::kPurple.light);
}

inline void paint_sun(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  for (int i = 0; i < 8; ++i) {
    const float a = static_cast<float>(i) * 0.785398163f;
    ui::stroke_path(c, ui::Path{}.move(cx + std::cos(a) * 13, cy + std::sin(a) * 13)
                                  .line(cx + std::cos(a) * 17, cy + std::sin(a) * 17),
                    2.0f, tok::kOrange.base);
  }
  ui::fill_circle(c, cx, cy, 10, tok::kYellow.base);
  ui::stroke_circle(c, cx, cy, 10, 1.6f, tok::kOrange.shadow);
  ui::fill_circle(c, cx - 3, cy - 2, 1.2f, pal::kInk);
  ui::fill_circle(c, cx + 3, cy - 2, 1.2f, pal::kInk);
  ui::stroke_path(c, ui::Path{}.arc(cx, cy + 1, 3, 0.2f, 2.9f, 8), 1.2f, pal::kInk);
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
