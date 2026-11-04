#pragma once
// MaTouch Lite display (ST7789 over SPI) and touch (NS2009 over I2C) drivers.
// Pins come from the generated board_pins.hpp (source-reviewed, not yet
// bench-verified). ESP-IDF only.
#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "little_arcade/touch_calibration.hpp"

namespace little_arcade::board {

// Called from the SPI ISR when a draw_bitmap transfer has finished.
using FlushDoneFn = void (*)(void* ctx);

struct DisplayConfig {
  uint32_t pclk_hz = 27 * 1000 * 1000;  // upstream TFT_eSPI SPI_FREQUENCY
  std::size_t max_transfer_bytes = 0;   // largest single draw_bitmap
  Orientation orientation = kLandscape;
  FlushDoneFn on_flush_done = nullptr;
  void* ctx = nullptr;
};

// Initialises SPI bus + ST7789 panel (BGR, inversion on, as upstream). The
// backlight stays off until backlight_set(true).
esp_err_t display_init(const DisplayConfig& config, esp_lcd_panel_handle_t* out_panel);
void backlight_set(bool on);

struct TouchSample {
  uint16_t raw_x;
  uint16_t raw_y;
  uint16_t raw_z;
  bool pressed;
};

esp_err_t touch_init();
// Polls the NS2009 (Z first, then X/Y only when pressed).
esp_err_t touch_read(TouchSample* out, const TouchCalibration& calibration = {});

}  // namespace little_arcade::board
