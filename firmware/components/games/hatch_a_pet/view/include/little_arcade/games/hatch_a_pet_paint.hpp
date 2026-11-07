#pragma once
#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/ui/ctx2d.hpp"

// Hatch-a-Pet drawing, ported call-for-call from the draw functions in
// v2/reference/hatch_a_pet.html. LVGL-free: the device/simulator view and the
// host reference-diff tool share it, so they produce the same pixels.
namespace little_arcade::hatch_a_pet {

// Display-list capacity for one Hatch-a-Pet frame. Sized from the peak use
// printed by hatch_a_pet_reference_render replaying the reference script
// (it checks for overflow with these limits).
inline ui::c2d::DisplayList::Limits display_list_limits() {
  ui::c2d::DisplayList::Limits l;
  l.points = 14336;
  l.contours = 1536;
  l.items = 512;
  l.gradients = 16;
  return l;
}

// The static part of drawBackground() (sky, sun, hills, meadow, grass
// tufts), drawn once and cached. Nothing drawn per frame overlaps it out of
// order, so the cached layer is pixel-equivalent.
void paint_background(ui::c2d::Context& ctx);

// One frame minus the cached background, in the reference drawScene() order:
// drawBackground's moving parts (clouds, flowers, fluff), shelf, nest back,
// bottom half, egg, nest front, pet, top half, choices, helper hand, fx,
// badge, chrome, then drawStart() in START. The mic key is faded (reference
// style) unless voice is enabled.
void paint_frame(ui::c2d::Context& ctx, const HatchAPetGame& game, bool mic_enabled);

}  // namespace little_arcade::hatch_a_pet
