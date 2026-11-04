#pragma once
#include <cstdint>

namespace little_arcade {

inline constexpr int16_t kScreenWidth = 320;
inline constexpr int16_t kScreenHeight = 240;
inline constexpr int16_t kMinTarget = 48;   // minimum primary touch target edge (px)
inline constexpr int16_t kMinSpacing = 8;   // minimum gap between targets (px)

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;

  constexpr bool contains(int16_t px, int16_t py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
  constexpr bool on_screen() const {
    return x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= kScreenWidth && y + h <= kScreenHeight;
  }
  constexpr bool large_enough() const { return w >= kMinTarget && h >= kMinTarget; }
  constexpr bool overlaps(const Rect& o) const {
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
  }
};

// Fixed navigation: the Home button is always top-left and owned by the
// SceneManager. Games must keep their targets out of kHomeButton and should
// lay out inside kPlayArea.
inline constexpr Rect kHomeButton{0, 0, 56, 56};
inline constexpr Rect kPlayArea{0, 64, kScreenWidth, kScreenHeight - 64};

}  // namespace little_arcade
