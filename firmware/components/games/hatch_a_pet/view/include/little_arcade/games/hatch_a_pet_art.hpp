#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::hatch_a_pet {

// Procedural art for Hatch-a-Pet, ported from docs/mockups/hatch_a_pet.html.
// LVGL-free so geometry is host-tested; the view paints these into ARGB images
// once (per pattern / per creature) and never per frame.
namespace art {

namespace pal = ui::palette;

// Egg = two half-ellipses (narrow top, round bottom), joined at 58% height.
inline bool egg_inside(float x, float y, float cx, float top, float w, float h, float grow = 0.0f) {
  const float mid = top + h * 0.58f;
  const float rx = w * 0.5f + grow;
  const float ry = (y < mid ? h * 0.58f : h * 0.42f) + grow;
  const float dx = (x - cx) / rx, dy = (y - mid) / ry;
  return dx * dx + dy * dy <= 1.0f;
}

// Crack zigzag across the egg in egg-normalised coordinates (u: -0.5..0.5
// across the width, v: 0..1 down the height). It also splits the shell into
// the top and bottom pieces that fly apart when the creature hatches.
inline constexpr int kCrackPoints = 7;
inline constexpr float kCrack[kCrackPoints][2] = {{-0.5f, 0.50f}, {-0.32f, 0.42f}, {-0.18f, 0.53f}, {-0.02f, 0.43f},
                                                   {0.14f, 0.54f},  {0.30f, 0.44f},  {0.5f, 0.52f}};

// Crack height (v) at horizontal position u, by linear interpolation.
inline float crack_v(float u) {
  if (u <= kCrack[0][0]) return kCrack[0][1];
  for (int i = 1; i < kCrackPoints; ++i) {
    if (u <= kCrack[i][0]) {
      const float t = (u - kCrack[i - 1][0]) / (kCrack[i][0] - kCrack[i - 1][0]);
      return kCrack[i - 1][1] + t * (kCrack[i][1] - kCrack[i - 1][1]);
    }
  }
  return kCrack[kCrackPoints - 1][1];
}

// Crack points revealed after `cracks` of `needed` taps (0 = no crack line).
inline int visible_crack_points(int cracks, int needed) {
  if (cracks <= 0 || needed <= 0) return 0;
  const float f = cracks >= needed ? 1.0f : static_cast<float>(cracks) / static_cast<float>(needed);
  return static_cast<int>(std::lround(static_cast<float>(kCrackPoints - 1) * f)) + 1;
}

inline uint32_t pattern_rgb(EggPattern p) {
  switch (p) {
    case EggPattern::SPOTS: return 0x3FB8AF;
    case EggPattern::STRIPES: return 0xF27BA6;
    case EggPattern::ZIGZAGS: return 0x9B6BD6;
    case EggPattern::STARS: return 0xF59B3A;
  }
  return 0x3FB8AF;
}

enum class Piece : uint8_t { WHOLE, TOP, BOTTOM };

// Is (x, y) inside the requested shell piece of the egg at (cx, top, w, h)?
inline bool in_piece(Piece piece, float x, float y, float cx, float top, float w, float h) {
  if (piece == Piece::WHOLE) return true;
  const bool above = y < top + crack_v((x - cx) / w) * h;
  return piece == Piece::TOP ? above : !above;
}

// Paints a patterned egg (or one shell piece) with an ink outline.
inline void paint_egg(ui::Canvas& c, EggPattern pattern, float cx, float top, float w, float h,
                      Piece piece = Piece::WHOLE) {
  using ui::in_circle;
  const float x0 = cx - w * 0.5f - 2, x1 = cx + w * 0.5f + 2, y0 = top - 2, y1 = top + h + 2;
  auto layer = [&](uint32_t rgb, uint8_t opa, auto shape) {
    ui::paint(c, x0, y0, x1, y1, rgb, opa, [&](float x, float y) {
      return egg_inside(x, y, cx, top, w, h) && in_piece(piece, x, y, cx, top, w, h) && shape(x, y);
    });
  };
  layer(pal::kShell, 255, [](float, float) { return true; });

  const uint32_t col = pattern_rgb(pattern);
  switch (pattern) {
    case EggPattern::SPOTS: {
      static constexpr float kSpots[][3] = {{-0.2f, 0.22f, 9},  {0.18f, 0.3f, 11}, {-0.28f, 0.62f, 10}, {0.08f, 0.7f, 13},
                                            {0.3f, 0.6f, 7},   {-0.05f, 0.12f, 6}, {-0.1f, 0.88f, 8}};
      const float k = w / 112.0f;
      layer(col, 255, [&](float x, float y) {
        for (const auto& s : kSpots) {
          if (in_circle(x, y, cx + s[0] * w, top + s[1] * h, s[2] * k)) return true;
        }
        return false;
      });
      break;
    }
    case EggPattern::STRIPES: {
      const float half = 5.0f * h / 136.0f;
      layer(col, 255, [&](float, float y) {
        const float v[] = {0.2f, 0.36f, 0.68f, 0.84f};
        for (const float s : v) {
          if (y >= top + s * h - half && y < top + s * h + half) return true;
        }
        return false;
      });
      break;
    }
    case EggPattern::ZIGZAGS: {
      ui::Path rows[2];
      const float v[] = {0.24f, 0.72f}, amp = 7.0f * h / 136.0f;
      for (int r = 0; r < 2; ++r) {
        for (int i = 0; i <= 8; ++i) {
          rows[r].line(cx - w * 0.5f + static_cast<float>(i) * w / 8.0f, top + v[r] * h + ((i % 2) ? amp : -amp));
        }
      }
      const float hw = 3.0f * w / 112.0f;
      layer(col, 255, [&](float x, float y) { return rows[0].stroked(x, y, hw) || rows[1].stroked(x, y, hw); });
      break;
    }
    case EggPattern::STARS: {
      static constexpr float kStars[][3] = {
          {-0.18f, 0.2f, 9}, {0.2f, 0.3f, 11}, {-0.25f, 0.66f, 10}, {0.12f, 0.74f, 12}, {0.0f, 0.92f, 7}};
      const float k = w / 112.0f;
      ui::Path stars[5];
      for (int s = 0; s < 5; ++s) {
        const float scx = cx + kStars[s][0] * w, scy = top + kStars[s][1] * h, ro = kStars[s][2] * k;
        for (int i = 0; i < 10; ++i) {
          const float r = (i % 2) ? ro * 0.45f : ro, a = -1.5707963f + static_cast<float>(i) * 0.62831853f;
          stars[s].line(scx + std::cos(a) * r, scy + std::sin(a) * r);
        }
      }
      layer(col, 255, [&](float x, float y) {
        for (const auto& s : stars) {
          if (s.filled(x, y)) return true;
        }
        return false;
      });
      break;
    }
  }
  // Soft highlight, then the outline.
  const ui::Ellipse shine(cx - w * 0.22f, top + h * 0.28f, w * 0.09f, h * 0.12f, -0.4f);
  layer(pal::kWhite, 204, [&](float x, float y) { return shine.contains(x, y); });
  ui::paint(c, x0, y0, x1, y1, pal::kInk, 255, [&](float x, float y) {
    return egg_inside(x, y, cx, top, w, h, 1.0f) && !egg_inside(x, y, cx, top, w, h, -1.0f) &&
           in_piece(piece, x, y, cx, top, w, h);
  });
}

// Static crack line (first `points` crack points), for tile and badge art.
inline void paint_crack(ui::Canvas& c, float cx, float top, float w, float h, int points, float width = 3.0f) {
  if (points < 2) return;
  ui::Path p;
  for (int i = 0; i < points && i < kCrackPoints; ++i) p.line(cx + kCrack[i][0] * w, top + kCrack[i][1] * h);
  ui::stroke_path(c, p, width, pal::kInk);
}

// Shared face: eyes with pupils and glints, rosy cheeks and a smile.
inline void paint_face(ui::Canvas& c, float cx, float cy, float s, float gap) {
  for (const float sx : {-1.0f, 1.0f}) {
    const float ex = cx + sx * gap * s;
    ui::fill_circle(c, ex, cy, 9 * s, pal::kWhite);
    ui::stroke_circle(c, ex, cy, 9 * s, 1.5f, pal::kInk);
    ui::fill_circle(c, ex + 1.5f * s, cy + 1.5f * s, 5 * s, pal::kInk);
    ui::fill_circle(c, ex + 3 * s, cy - 1 * s, 1.8f * s, pal::kWhite);
    ui::fill_circle(c, cx + sx * (gap + 9) * s, cy + 12 * s, 5 * s, pal::kCheek, 115);
  }
  const float mx = cx, my = cy + 9 * s, r = 8 * s, hw = 1.5f * s;
  ui::paint(c, mx - r - 2, my - 2, mx + r + 2, my + r + 2, pal::kInk, 255,
            [=](float x, float y) { return ui::in_arc(x, y, mx, my, r, hw, 0.2f, 3.1415926f - 0.2f); });
}

// Creature centred at (cx, cy) at scale s. At s = 1 it spans about +-70 px.
inline void paint_creature(ui::Canvas& c, Creature kind, float cx, float cy, float s) {
  constexpr float kPi = 3.1415926f;
  const uint32_t ink = pal::kInk;
  switch (kind) {
    case Creature::BLOOP: {
      ui::Path antenna;
      antenna.move(cx, cy - 30 * s).quad(cx - 4 * s, cy - 52 * s, cx + 10 * s, cy - 56 * s);
      antenna.arc(cx + 10 * s, cy - 48 * s, 8 * s, -kPi / 2, kPi * 1.2f, 20);
      ui::stroke_path(c, antenna, 5 * s, 0x1E9C90);
      ui::blob(c, cx, cy + 10 * s, 50 * s, 44 * s, 0, 0x2EC4B6, ink);
      ui::fill_ellipse(c, cx - 22 * s, cy - 10 * s, 10 * s, 6 * s, -0.5f, pal::kWhite, 128);
      paint_face(c, cx, cy + 6 * s, s, 15);
      break;
    }
    case Creature::FLUFFWING: {
      const uint32_t col = 0xFFB48A;
      for (const float sx : {-1.0f, 1.0f}) {
        ui::blob(c, cx + sx * 50 * s, cy - 2 * s, 18 * s, 10 * s, sx * -0.5f, pal::kWhite, ink);
      }
      for (int i = 0; i < 12; ++i) {
        const float a = static_cast<float>(i) * kPi / 6;
        ui::fill_circle(c, cx + std::cos(a) * 38 * s, cy + 6 * s + std::sin(a) * 36 * s, 13 * s, col);
      }
      ui::fill_circle(c, cx, cy + 6 * s, 40 * s, col);
      paint_face(c, cx, cy + 4 * s, s, 14);
      break;
    }
    case Creature::SPROUTLING: {
      ui::Path stem;
      stem.move(cx, cy - 36 * s).line(cx + 2 * s, cy - 50 * s);
      ui::stroke_path(c, stem, 4 * s, 0x2E8B3A);
      ui::blob(c, cx + 14 * s, cy - 56 * s, 16 * s, 8 * s, -0.5f, pal::kGreen, ink);
      ui::blob(c, cx, cy + 10 * s, 36 * s, 50 * s, 0, 0x7DD35F, ink);
      paint_face(c, cx, cy + 2 * s, s, 12);
      break;
    }
    case Creature::ZIGZAG: {
      const uint32_t col = 0x9B6BD6, dark = 0x7A4FB8;
      ui::Path tail;
      tail.move(cx + 30 * s, cy + 30 * s).line(cx + 44 * s, cy + 14 * s).line(cx + 52 * s, cy + 36 * s);
      tail.line(cx + 60 * s, cy + 16 * s).line(cx + 62 * s, cy + 2 * s);
      ui::stroke_path(c, tail, 9 * s, col);
      for (const float dx : {-16.0f, 0.0f, 16.0f}) {
        ui::Path spike;
        spike.move(cx + (dx - 8) * s, cy - 32 * s).line(cx + dx * s, cy - 46 * s).line(cx + (dx + 8) * s, cy - 32 * s);
        ui::fill_path(c, spike, dark);
      }
      ui::blob(c, cx, cy + 8 * s, 42 * s, 42 * s, 0, col, ink);
      for (const float sx : {-1.0f, 1.0f}) ui::fill_ellipse(c, cx + sx * 24 * s, cy + 50 * s, 10 * s, 6 * s, 0, dark);
      ui::fill_ellipse(c, cx, cy + 22 * s, 24 * s, 16 * s, 0, 0xC7A8EE);
      paint_face(c, cx, cy - 2 * s, s, 14);
      break;
    }
    case Creature::PUDDLEPUP: {
      ui::Path body;
      body.move(cx, cy - 48 * s).cubic(cx + 18 * s, cy - 24 * s, cx + 44 * s, cy - 4 * s, cx + 44 * s, cy + 18 * s);
      body.arc(cx, cy + 18 * s, 44 * s, 0, kPi, 20);
      body.cubic(cx - 44 * s, cy - 4 * s, cx - 18 * s, cy - 24 * s, cx, cy - 48 * s);
      ui::fill_path(c, body, 0x4FA3F7);
      ui::stroke_path(c, body, 2, ink, true);
      for (const float sx : {-1.0f, 1.0f}) {
        ui::blob(c, cx + sx * 40 * s, cy + 6 * s, 10 * s, 22 * s, sx * 0.35f, 0x2C6FB8, ink);
      }
      paint_face(c, cx, cy + 8 * s, s, 15);
      ui::fill_ellipse(c, cx, cy + 15 * s, 5 * s, 3.5f * s, 0, ink);
      break;
    }
    case Creature::GLIMMERBUG: {
      for (const float sx : {-1.0f, 1.0f}) {
        ui::Path a;
        a.move(cx + sx * 8 * s, cy - 36 * s).line(cx + sx * 20 * s, cy - 54 * s);
        ui::stroke_path(c, a, 3 * s, ink);
        ui::fill_circle(c, cx + sx * 20 * s, cy - 54 * s, 5 * s, pal::kYellow);
        ui::stroke_circle(c, cx + sx * 20 * s, cy - 54 * s, 5 * s, 1.5f, ink);
      }
      ui::fill_circle(c, cx, cy + 6 * s, 46 * s, pal::kYellow);
      ui::stroke_circle(c, cx, cy + 6 * s, 46 * s, 2, ink);
      static constexpr float kDots[][3] = {{-26, -14, 6}, {24, -18, 5}, {-30, 18, 5}, {30, 14, 6}, {-12, 36, 4}, {14, 36, 5}};
      for (const auto& d : kDots) ui::fill_circle(c, cx + d[0] * s, cy + d[1] * s, d[2] * s, 0xE0892B);
      ui::Path seam;
      seam.move(cx, cy - 38 * s).line(cx, cy - 18 * s);
      ui::stroke_path(c, seam, 2, 0xC9862A);
      paint_face(c, cx, cy + 4 * s, s, 13);
      break;
    }
    case Creature::ROLYPOLE: {
      const float bx = cx, by = cy + 6 * s;
      ui::fill_circle(c, bx, by, 46 * s, 0xF59B3A);
      for (const float dx : {-30.0f, 30.0f}) {
        const float ex = bx + dx * s, rot = dx > 0 ? 0.25f : -0.25f, m = 50 * s + 1;
        const ui::Ellipse band(ex, by, 8 * s, 50 * s, rot);
        ui::paint(c, ex - m, by - m, ex + m, by + m, 0xD9762A, 255, [=, &band](float x, float y) {
          return ui::in_circle(x, y, bx, by, 45 * s) && band.contains(x, y);
        });
      }
      ui::stroke_circle(c, bx, by, 46 * s, 2, ink);
      paint_face(c, cx, cy + 2 * s, s, 13);
      break;
    }
    case Creature::SNORKLE: {
      const uint32_t col = pal::kPink;
      ui::blob(c, cx - 6 * s, cy + 10 * s, 42 * s, 40 * s, 0, col, ink);
      for (const float sx : {-1.0f, 1.0f}) {
        ui::blob(c, cx - 6 * s + sx * 30 * s, cy - 26 * s, 9 * s, 12 * s, sx * 0.4f, 0xD95C8C, ink);
      }
      ui::Path snout;
      snout.move(cx + 12 * s, cy + 18 * s).quad(cx + 46 * s, cy + 20 * s, cx + 50 * s, cy - 2 * s);
      snout.arc(cx + 42 * s, cy - 2 * s, 8 * s, 0, -kPi * 1.1f, 16);
      ui::stroke_path(c, snout, 13 * s, col);
      paint_face(c, cx - 10 * s, cy, s, 12);
      break;
    }
  }
}

// Image sizes used by the view (and checked by the host tests).
inline constexpr int kEggImgW = 116;  // a 112x136 egg plus a 2 px outline margin
inline constexpr int kEggImgH = 140;
inline constexpr int kCreatureImg = 144;  // creature at s = 1 centred in a square
inline constexpr int kEggIconW = 34;      // badge and "another egg" button picture
inline constexpr int kEggIconH = 42;
inline constexpr int kTileW = 108;  // launcher tile picture (ui::LauncherView::kTileArtW/H)
inline constexpr int kTileH = 68;

inline void paint_egg_image(ui::Canvas& c, EggPattern p, Piece piece) {
  paint_egg(c, p, kEggImgW * 0.5f, 2, 112, 136, piece);
}
inline void paint_egg_icon(ui::Canvas& c, EggPattern p) {
  paint_egg(c, p, kEggIconW * 0.5f, 2, kEggIconW - 4, kEggIconH - 4);
}
inline void paint_creature_image(ui::Canvas& c, Creature kind) {
  paint_creature(c, kind, kCreatureImg * 0.5f, kCreatureImg * 0.5f, 1.0f);
}
// Launcher tile: a cracked spotted egg next to a small BLOOP.
inline void paint_tile_art(ui::Canvas& c) {
  paint_egg(c, EggPattern::SPOTS, 40, 5, 44, 56);
  paint_crack(c, 40, 5, 44, 56, visible_crack_points(3, 4), 2.0f);
  paint_creature(c, Creature::BLOOP, 84, 44, 0.32f);
}

}  // namespace art
}  // namespace little_arcade::hatch_a_pet
