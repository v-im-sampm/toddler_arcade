#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::sound_jar::art {

namespace pal = ui::palette;

inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;
inline constexpr int kIconSize = 36;
inline constexpr int kCritterSize = 50;
inline constexpr std::size_t kCritters = 5;
// Pads: big creature = low voice ... tiny creature = high voice. Size and
// position carry the meaning, so colour is never the only cue.
inline constexpr float kCritterRadius[kCritters] = {21.0f, 17.0f, 14.0f, 10.0f, 7.0f};
inline constexpr uint32_t kCritterColors[kCritters] = {pal::kRed, pal::kOrange, pal::kYellow, pal::kGreen, pal::kPurple};
inline constexpr uint32_t kPadTints[kCritters] = {0xFFE1E1, 0xFFEBD6, 0xFFF5CC, 0xE0F5E0, 0xEDE3FB};
inline constexpr uint32_t kJarGlass = 0xEAF7FF;
inline constexpr uint32_t kWaveColor = pal::kPurple;
inline constexpr uint32_t kRecordRed = 0xE53935;

inline void paint_critter(ui::Canvas& c, std::size_t i) {
  const float r = kCritterRadius[i % kCritters];
  const float cx = c.w * 0.5f, cy = c.h * 0.5f + 2.0f;
  // Little feet first so the body overlaps them.
  ui::fill_ellipse(c, cx - r * 0.45f, cy + r * 0.85f, r * 0.28f, r * 0.16f, 0, pal::kInk);
  ui::fill_ellipse(c, cx + r * 0.45f, cy + r * 0.85f, r * 0.28f, r * 0.16f, 0, pal::kInk);
  ui::blob(c, cx, cy, r, r * 0.9f, 0, kCritterColors[i % kCritters], pal::kInk, r > 10 ? 2.0f : 1.5f);
  const float er = r * 0.24f < 2.2f ? 2.2f : r * 0.24f;
  for (int side = -1; side <= 1; side += 2) {
    const float ex = cx + side * r * 0.38f, ey = cy - r * 0.22f;
    ui::fill_circle(c, ex, ey, er, pal::kWhite);
    ui::fill_circle(c, ex, ey + er * 0.2f, er * 0.55f, pal::kInk);
  }
  // Open singing mouth.
  ui::fill_ellipse(c, cx, cy + r * 0.38f, r * 0.24f, r * 0.2f, 0, pal::kMouth);
}

// 0 backwards, 1 echo, 2 robot, 3 wobble.
inline void paint_effect_icon(ui::Canvas& c, std::size_t effect, uint32_t ink = pal::kInk) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  switch (effect % 4) {
    case 0: {  // rewind: two triangles pointing left
      for (int k = 0; k < 2; ++k) {
        const float x = cx - 1.0f + k * 12.0f;
        ui::Path p;
        p.move(x - 12, cy).line(x, cy - 10).line(x, cy + 10);
        ui::fill_path(c, p, ink);
      }
      break;
    }
    case 1: {  // echo: a dot with fading rings
      ui::fill_circle(c, cx - 10, cy, 4.5f, ink);
      const uint8_t opa[] = {255, 190, 120};
      for (int k = 0; k < 3; ++k) {
        const float rr = 8.0f + k * 6.0f;
        ui::paint(c, 0, 0, static_cast<float>(c.w), static_cast<float>(c.h), ink, opa[k], [=](float x, float y) {
          return ui::in_arc(x, y, cx - 10, cy, rr, 1.6f, -0.85f, 0.85f);
        });
      }
      break;
    }
    case 2: {  // robot head
      ui::Path ant;
      ant.move(cx, cy - 9).line(cx, cy - 14);
      ui::stroke_path(c, ant, 2.0f, ink);
      ui::fill_circle(c, cx, cy - 15, 2.5f, pal::kRed);
      ui::paint(c, cx - 12, cy - 9, cx + 12, cy + 13, ink, 255,
                [=](float x, float y) { return ui::in_rect(x, y, cx - 12, cy - 9, cx + 12, cy + 13); });
      ui::paint(c, cx - 10, cy - 7, cx + 10, cy + 11, 0xCFD8E6, 255,
                [=](float x, float y) { return ui::in_rect(x, y, cx - 10, cy - 7, cx + 10, cy + 11); });
      ui::fill_circle(c, cx - 5, cy - 1, 3.0f, pal::kBlue);
      ui::fill_circle(c, cx + 5, cy - 1, 3.0f, pal::kBlue);
      for (int k = 0; k < 4; ++k) {
        const float x = cx - 6 + k * 4.0f;
        ui::paint(c, x, cy + 5, x + 2, cy + 9, ink, 255,
                  [=](float px, float py) { return ui::in_rect(px, py, x, cy + 5, x + 2, cy + 9); });
      }
      break;
    }
    default: {  // wobble: a wavy line
      ui::Path p;
      p.move(cx - 15, cy);
      for (int k = 1; k <= 30; ++k) {
        const float t = static_cast<float>(k) / 30.0f;
        p.line(cx - 15 + 30 * t, cy - 8.0f * std::sin(t * 6.2831853f * 1.5f));
      }
      ui::stroke_path(c, p, 3.5f, ink);
      break;
    }
  }
}

inline void paint_record_dot(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_circle(c, cx, cy, 15.0f, kRecordRed);
  ui::stroke_circle(c, cx, cy, 15.0f, 2.0f, pal::kInk);
  ui::fill_circle(c, cx - 5, cy - 5, 4.0f, pal::kWhite, 150);
}

inline void paint_stop_square(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::paint(c, cx - 10, cy - 10, cx + 10, cy + 10, kRecordRed, 255,
            [=](float x, float y) { return ui::in_rect(x, y, cx - 10, cy - 10, cx + 10, cy + 10); });
}

inline void paint_sparkle(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_star(c, cx, cy + 1, 15.0f, 6.5f, pal::kInk);
  ui::fill_star(c, cx, cy + 1, 12.5f, 5.2f, pal::kYellow);
  ui::fill_star(c, cx + 12, cy - 11, 5.0f, 2.0f, pal::kOrange);
  ui::fill_star(c, cx - 12, cy + 11, 4.0f, 1.6f, pal::kPink);
}

inline void paint_star(ui::Canvas& c) {
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  ui::fill_star(c, cx, cy + 1, c.w * 0.45f, c.w * 0.2f, pal::kYellow);
}

inline void paint_tile_art(ui::Canvas& c) {
  const float cx = c.w * 0.5f;
  // Glass jar with a lid.
  ui::paint(c, cx - 26, 14, cx + 26, 64, pal::kInk, 255, [=](float x, float y) {
    return ui::in_rect(x, y, cx - 26, 22, cx + 26, 58) || ui::in_ellipse(x, y, cx, 58, 26, 6) ||
           ui::in_rect(x, y, cx - 22, 14, cx + 22, 24);
  });
  ui::paint(c, cx - 24, 16, cx + 24, 62, kJarGlass, 255, [=](float x, float y) {
    return ui::in_rect(x, y, cx - 24, 24, cx + 24, 58) || ui::in_ellipse(x, y, cx, 58, 24, 4.5f);
  });
  ui::paint(c, cx - 25, 6, cx + 25, 18, pal::kOrange, 255,
            [=](float x, float y) { return ui::in_rect(x, y, cx - 25, 8, cx + 25, 17); });
  ui::stroke_path(c, ui::Path{}.move(cx - 25, 8).line(cx + 25, 8).line(cx + 25, 17).line(cx - 25, 17), 2.0f, pal::kInk,
                  true);
  // Sound wave inside.
  static constexpr float kBars[] = {4, 9, 15, 11, 18, 8, 13, 6, 3};
  for (int k = 0; k < 9; ++k) {
    const float x = cx - 20 + k * 5.0f, h = kBars[k];
    ui::paint(c, x, 41 - h, x + 3, 41 + h, kWaveColor, 255,
              [=](float px, float py) { return ui::in_rect(px, py, x, 41 - h, x + 3, 41 + h); });
  }
  // Notes flying out.
  ui::fill_circle(c, cx + 36, 18, 4.0f, pal::kPink);
  ui::stroke_path(c, ui::Path{}.move(cx + 39.5f, 18).line(cx + 39.5f, 5), 2.0f, pal::kPink);
  ui::fill_circle(c, cx - 37, 26, 3.5f, pal::kGreen);
  ui::stroke_path(c, ui::Path{}.move(cx - 34, 26).line(cx - 34, 14), 2.0f, pal::kGreen);
}

// Waveform picture for the jar: one bar per thumbnail column, mirrored
// around the centre line. Plain pixel fills (no supersampling) so it is cheap
// enough to repaint whenever the clip changes.
inline void paint_wave(ui::Canvas& c, const uint8_t* columns, std::size_t count, bool reversed, uint32_t rgb) {
  c.clear();
  if (count == 0) return;
  const int mid = c.h / 2;
  const uint32_t color = 0xFF000000u | (rgb & 0xFFFFFFu);
  for (int x = 0; x < c.w; ++x) {
    std::size_t col = static_cast<std::size_t>(x) * count / static_cast<std::size_t>(c.w);
    if (reversed) col = count - 1 - col;
    int half = columns[col] * (mid - 1) / 255;
    if (half < 1) half = 1;  // a flat line still shows where the sound is
    for (int y = mid - half; y <= mid + half; ++y) c.px[y * c.w + x] = color;
  }
}

}  // namespace little_arcade::sound_jar::art
