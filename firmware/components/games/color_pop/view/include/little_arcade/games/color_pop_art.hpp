#pragma once
#include "little_arcade/games/color_pop_patterns.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::color_pop {

// Launcher tile picture (docs/mockups/hatch_a_pet.html, GAMES[0]): three
// bubbles in the game's own colours, each with its pattern clue.
inline constexpr int kTileW = 108;  // ui::LauncherView::kTileArtW/H
inline constexpr int kTileH = 68;

inline void paint_tile_art(ui::Canvas& c) {
  namespace pal = ui::palette;
  struct B {
    float x, y;
    Color color;
    uint32_t rgb;
  };
  const B bubbles[] = {{22, 42, Color::RED, pal::kRed}, {86, 42, Color::YELLOW, pal::kYellow}, {54, 26, Color::BLUE, pal::kBlue}};
  constexpr float kR = 17.0f, kBox = 20.0f;
  for (const B& b : bubbles) {
    ui::fill_circle(c, b.x + 2, b.y + 3, kR, pal::kInk, 64);
    ui::fill_circle(c, b.x, b.y, kR, b.rgb);
    ui::stroke_circle(c, b.x, b.y, kR, 2, pal::kInk);
    ui::fill_circle(c, b.x - 9, b.y - 7, 2.5f, pal::kWhite);
    const float ox = b.x - kBox * 0.5f, oy = b.y + 1 - kBox * 0.5f;
    const Pattern p = pattern_for(b.color);
    ui::paint(c, ox, oy, ox + kBox, oy + kBox, pal::kWhite, 255,
              [=](float x, float y) { return pattern_inside(p, x - ox, y - oy, kBox); });
  }
}

}  // namespace little_arcade::color_pop
