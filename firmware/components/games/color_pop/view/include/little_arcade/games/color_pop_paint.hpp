#pragma once
#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/ui/ctx2d.hpp"

// Color Pop drawing, ported call-for-call from the draw functions in
// v2/reference/quality_bar.html. LVGL-free: the device/simulator view and the
// host reference-diff tool share it, so they produce the same pixels.
namespace little_arcade::color_pop {

// Display-list capacity for one Color Pop frame. Peak use replaying the
// reference script is 143 items / 4610 points / 363 contours
// (color_pop_reference_render checks for overflow with these limits).
inline ui::c2d::DisplayList::Limits display_list_limits() {
  ui::c2d::DisplayList::Limits l;
  l.points = 10240;
  l.contours = 1024;
  l.items = 384;
  return l;
}

// paintBg(): the static sky / sea / sand layer, drawn once and cached.
void paint_background(ui::c2d::Context& ctx);

// One frame minus the cached background, in the reference order:
// drawBackground (clouds, shimmer), bubbles, crab, helper hand, fx, badge,
// chrome. The mic key is faded (reference style) unless voice is enabled.
void paint_frame(ui::c2d::Context& ctx, const ColorPopGame& game, bool mic_enabled);

}  // namespace little_arcade::color_pop
