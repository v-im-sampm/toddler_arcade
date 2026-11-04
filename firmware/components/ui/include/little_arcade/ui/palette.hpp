#pragma once
#include <cstdint>

namespace little_arcade::ui {

// Shared 24-bit palette (0xRRGGBB). Matches docs/mockups/color_pop.html.
// Renderers convert to their native format (RGB565 on the ST7789).
namespace palette {
inline constexpr uint32_t kSky = 0xBFE8FF;
inline constexpr uint32_t kSkyDark = 0x9FD4F5;
inline constexpr uint32_t kGround = 0xA8E6A1;
inline constexpr uint32_t kChrome = 0xFFFFFF;
inline constexpr uint32_t kChromeEdge = 0x5B6B8C;
inline constexpr uint32_t kInk = 0x33415C;
inline constexpr uint32_t kWhite = 0xFFFFFF;
inline constexpr uint32_t kRed = 0xE84A4A;
inline constexpr uint32_t kBlue = 0x3A7BE0;
inline constexpr uint32_t kGreen = 0x3FB24A;
inline constexpr uint32_t kYellow = 0xF7C631;
}  // namespace palette

}  // namespace little_arcade::ui
