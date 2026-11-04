#pragma once
#include <cstdint>

namespace little_arcade::board {

// NS2009 12-bit samples -> 320x240 landscape screen coordinates.
// Defaults come from the upstream example (Makerfabs 630f37c,
// example/LVGL_Touch/ui/ui.ino): raw X 200..3850 -> portrait 0..239,
// raw Y 400..3920 -> portrait 0..319, no flips, pressed when Z > 150.
// Not bench-calibrated yet (H02 5-point calibration is still open).
struct TouchCalibration {
  uint16_t x_min = 200;
  uint16_t x_max = 3850;
  uint16_t y_min = 400;
  uint16_t y_max = 3920;
  uint16_t z_threshold = 150;
};

// ST7789 MADCTL bits as passed to esp_lcd_panel_swap_xy/mirror. Touch must use
// the same values so taps land where pixels are drawn.
struct Orientation {
  bool swap_xy;
  bool mirror_x;
  bool mirror_y;
};
// Landscape guess equivalent to TFT_eSPI ST7789 rotation 1 (MV|MX). The
// upstream example only uses portrait, so this must be confirmed on the bench.
inline constexpr Orientation kLandscape{true, true, false};

inline constexpr int16_t kPanelWidth = 240;   // native portrait
inline constexpr int16_t kPanelHeight = 320;
inline constexpr uint16_t kNs2009Invalid = 0xFFF;  // seen on false reads

constexpr bool ns2009_pressed(uint16_t z, const TouchCalibration& c = {}) {
  return z > c.z_threshold && z != kNs2009Invalid;
}

constexpr int16_t scale_axis(uint16_t raw, uint16_t lo, uint16_t hi, int16_t size) {
  if (raw <= lo) return 0;
  if (raw >= hi) return static_cast<int16_t>(size - 1);
  return static_cast<int16_t>(static_cast<int32_t>(raw - lo) * (size - 1) / (hi - lo));
}

struct ScreenPoint {
  int16_t x;
  int16_t y;
};

constexpr ScreenPoint ns2009_to_screen(uint16_t raw_x, uint16_t raw_y, const TouchCalibration& c = {},
                                       const Orientation& o = kLandscape) {
  const int16_t px = scale_axis(raw_x, c.x_min, c.x_max, kPanelWidth);
  const int16_t py = scale_axis(raw_y, c.y_min, c.y_max, kPanelHeight);
  const int16_t col = o.mirror_x ? static_cast<int16_t>(kPanelWidth - 1 - px) : px;
  const int16_t row = o.mirror_y ? static_cast<int16_t>(kPanelHeight - 1 - py) : py;
  return o.swap_xy ? ScreenPoint{row, col} : ScreenPoint{col, row};
}

}  // namespace little_arcade::board
