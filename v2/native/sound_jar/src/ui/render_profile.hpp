#pragma once
// Diagnostic: full-screen render cost attributed to each visible child of the screen (hide it, re-render, and
// compare with the baseline). Used by the desktop --profile option and the device bench image.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "lvgl.h"
#include "ui/asset_pack.hpp"

namespace sound_jar {

struct LayerCost { int index; uint32_t saved_us; const char* name; };

// Image children show a pack sprite (Sprite::dsc is its first member); anything else is a custom draw object.
inline const char* layer_name(lv_obj_t* obj) {
  if (!lv_obj_check_type(obj, &lv_image_class)) return "draw";
  const void* src = lv_image_get_src(obj);
  return src ? reinterpret_cast<const Sprite*>(src)->name : "?";
}

// `now_us` is a monotonic microsecond clock. Returns the baseline full-screen render time and fills `costs`
// (largest first) with the time saved when each currently visible child is hidden. Restores visibility.
template <typename Clock>
uint32_t profile_layers(lv_display_t* display, Clock now_us, std::vector<LayerCost>& costs, int reps = 3) {
  lv_obj_t* screen = lv_screen_active();
  auto render = [&] {
    uint32_t best = UINT32_MAX;
    for (int r = 0; r < reps; ++r) {
      lv_obj_invalidate(screen);
      const uint64_t t0 = now_us();
      lv_refr_now(display);
      best = std::min(best, static_cast<uint32_t>(now_us() - t0));
    }
    return best;
  };
  const uint32_t base = render();
  costs.clear();
  const int count = static_cast<int>(lv_obj_get_child_count(screen));
  for (int i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(screen, i);
    if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
    const uint32_t t = render();
    lv_obj_remove_flag(child, LV_OBJ_FLAG_HIDDEN);
    costs.push_back({i, base > t ? base - t : 0, layer_name(child)});
  }
  std::sort(costs.begin(), costs.end(), [](const LayerCost& a, const LayerCost& b) { return a.saved_us > b.saved_us; });
  lv_obj_invalidate(screen);
  lv_refr_now(display);
  return base;
}

// One line: the baseline and the `top` most expensive children as "#index:name=us".
inline int format_profile(char* out, std::size_t size, uint32_t base, const std::vector<LayerCost>& costs, int top) {
  int len = std::snprintf(out, size, "full-screen render %u us, %zu visible children; top:", static_cast<unsigned>(base),
                          costs.size());
  for (int k = 0; k < top && k < static_cast<int>(costs.size()) && len < static_cast<int>(size); ++k)
    len += std::snprintf(out + len, size - len, " #%d:%s=%u", costs[k].index, costs[k].name,
                         static_cast<unsigned>(costs[k].saved_us));
  return len;
}

}  // namespace sound_jar
