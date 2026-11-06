#include "little_arcade/ui/chrome_view.hpp"

#include "little_arcade/geometry.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/ui/icons.hpp"
#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/tokens.hpp"

namespace little_arcade::ui {

lv_color_t to_lv(uint32_t rgb) { return lv_color_hex(rgb); }

namespace {
lv_obj_t* make_button(lv_obj_t* parent, const Rect& r, const lv_image_dsc_t* icon) {
  // Quality-bar chrome: cream rounded key with a darker "lip" underneath.
  constexpr int kInset = 6, kRadius = 14, kLip = 3;
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(root, r.x + kInset, r.y + kInset - 1);
  lv_obj_set_size(root, r.w - 2 * kInset, r.h - 2 * kInset + kLip);

  lv_obj_t* lip = lv_obj_create(root);
  lv_obj_remove_style_all(lip);
  lv_obj_set_pos(lip, 0, kLip);
  lv_obj_set_size(lip, r.w - 2 * kInset, r.h - 2 * kInset);
  lv_obj_set_style_radius(lip, kRadius, 0);
  lv_obj_set_style_bg_color(lip, to_lv(tok::kCream.shadow), 0);
  lv_obj_set_style_bg_opa(lip, LV_OPA_COVER, 0);

  lv_obj_t* btn = lv_obj_create(root);
  lv_obj_remove_style_all(btn);
  lv_obj_remove_flag(btn, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(btn, 0, 0);
  lv_obj_set_size(btn, r.w - 2 * kInset, r.h - 2 * kInset);
  lv_obj_set_style_radius(btn, kRadius, 0);
  lv_obj_set_style_bg_color(btn, to_lv(tok::kCream.light), 0);
  lv_obj_set_style_bg_grad_color(btn, to_lv(tok::kCream.base), 0);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, to_lv(tok::kCream.shadow), 0);
  lv_obj_set_style_border_width(btn, 2, 0);

  lv_obj_t* img = lv_image_create(btn);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(img, icon);
  lv_image_set_scale(img, 220);
  lv_obj_set_style_image_recolor(img, to_lv(tok::kInk), 0);
  lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_center(img);
  return root;
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
