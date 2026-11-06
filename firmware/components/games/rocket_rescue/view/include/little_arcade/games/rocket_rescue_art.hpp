#pragma once

#include <cstdint>
#include <initializer_list>

#include "little_arcade/games/rocket_rescue.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::rocket_rescue::art {

namespace pal = ui::palette;
inline constexpr int kPlanetW = 96;
inline constexpr int kPlanetH = 64;
inline constexpr int kRocketSize = 40;
inline constexpr int kStarSize = 24;
inline constexpr uint32_t kSpace = 0xDCEBFF;

inline void paint_planet(ui::Canvas& c, Look look, Friend friend_type) {
  const float cx = c.w * 0.5f, cy = c.h * 0.54f;
  const uint32_t surface = look == Look::RINGED ? 0x8AC9E8 :
                           (look == Look::SPOTTED ? 0xF2A65A : 0x8BCB8A);
  if (look == Look::RINGED)
    ui::stroke_ellipse(c, cx, cy + 2, 34, 8, -0.2f, 4, 0x7C74C8);
  ui::fill_circle(c, cx, cy, 25, surface);
  ui::stroke_circle(c, cx, cy, 25, 2, pal::kInk);
  if (look == Look::SPOTTED) {
    ui::fill_circle(c, cx - 10, cy - 7, 4, 0xFFD397);
    ui::fill_circle(c, cx + 9, cy + 6, 5, 0xFFD397);
    ui::fill_circle(c, cx + 5, cy - 13, 2.5f, 0xFFD397);
  } else if (look == Look::STRIPED) {
    ui::stroke_path(c, ui::Path{}.move(cx - 11, cy - 18).line(cx - 7, cy + 18), 4, 0xD8F1D1);
    ui::stroke_path(c, ui::Path{}.move(cx + 5, cy - 23).line(cx + 9, cy + 17), 4, 0xD8F1D1);
  }
  const uint32_t buddy = friend_type == Friend::MOON_PUP ? pal::kWhite :
                         (friend_type == Friend::STAR_BUNNY ? 0xF7C2D0 : 0xA7E6F5);
  const float bx = cx, by = cy - 1;
  ui::fill_circle(c, bx, by, 10, buddy);
  if (friend_type == Friend::STAR_BUNNY) {
    ui::fill_ellipse(c, bx - 5, by - 15, 3, 8, -0.2f, buddy);
    ui::fill_ellipse(c, bx + 5, by - 15, 3, 8, 0.2f, buddy);
  } else if (friend_type == Friend::COMET_KITTY) {
    ui::fill_path(c, ui::Path{}.move(bx - 8, by - 4).line(bx - 6, by - 15).line(bx, by - 9), buddy);
    ui::fill_path(c, ui::Path{}.move(bx + 8, by - 4).line(bx + 6, by - 15).line(bx, by - 9), buddy);
  }
  ui::fill_circle(c, bx - 3, by - 1, 1.2f, pal::kInk);
  ui::fill_circle(c, bx + 3, by - 1, 1.2f, pal::kInk);
  ui::stroke_path(c, ui::Path{}.arc(bx, by + 1, 3, 0.2f, 2.9f, 6), 1.2f, pal::kInk);
}

inline void paint_rocket(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::Path body;
  body.move(cx, cy - 17).quad(cx + 13, cy - 4, cx + 10, cy + 12)
      .line(cx, cy + 17).line(cx - 10, cy + 12).quad(cx - 13, cy - 4, cx, cy - 17);
  ui::fill_path(c, body, 0xF4F7FB);
  ui::stroke_path(c, body, 2, pal::kInk, true);
  ui::fill_circle(c, cx, cy - 3, 4.5f, 0x6FBDF2);
  ui::stroke_circle(c, cx, cy - 3, 4.5f, 1.5f, pal::kInk);
  ui::fill_path(c, ui::Path{}.move(cx - 8, cy + 10).line(cx - 15, cy + 17).line(cx - 5, cy + 14), pal::kOrange);
  ui::fill_path(c, ui::Path{}.move(cx + 8, cy + 10).line(cx + 15, cy + 17).line(cx + 5, cy + 14), pal::kOrange);
}

inline void paint_star(ui::Canvas& c) {
  ui::fill_star(c, c.w * 0.5f, c.h * 0.5f, c.w * 0.43f, c.w * 0.18f, pal::kYellow);
  ui::stroke_circle(c, c.w * 0.5f, c.h * 0.5f, c.w * 0.42f, 1.4f, pal::kInk);
}

}  // namespace little_arcade::rocket_rescue::art
