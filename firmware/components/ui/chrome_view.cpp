#include "little_arcade/ui/chrome_view.hpp"

#include "little_arcade/geometry.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/ui/icons.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::ui {

lv_color_t to_lv(uint32_t rgb) { return lv_color_hex(rgb); }

namespace {
lv_obj_t* make_button(lv_obj_t* parent, const Rect& r, const lv_image_dsc_t* icon) {
  lv_obj_t* btn = lv_obj_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_remove_flag(btn, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(btn, r.x + 4, r.y + 4);
  lv_obj_set_size(btn, r.w - 8, r.h - 8);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_set_style_bg_color(btn, to_lv(palette::kChrome), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, to_lv(palette::kChromeEdge), 0);
  lv_obj_set_style_border_width(btn, 2, 0);

  lv_obj_t* img = lv_image_create(btn);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(img, icon);
  lv_obj_set_style_image_recolor(img, to_lv(palette::kInk), 0);
  lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_center(img);
  return btn;
}
}  // namespace

void ChromeView::create(lv_obj_t* parent) {
  const float s = static_cast<float>(kIcon);
  home_btn_ = make_button(parent, kHomeButton, home_.build([s](float x, float y) { return home_icon(x, y, s); }));
  make_button(parent, kMicButton, mic_.build([s](float x, float y) { return mic_icon(x, y, s); }));
}

void ChromeView::set_home_visible(bool visible) {
  if (!home_btn_) return;
  if (visible) {
    lv_obj_remove_flag(home_btn_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(home_btn_, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace little_arcade::ui
