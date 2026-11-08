#include "launcher/menu_view.hpp"

#include <cmath>

namespace launcher {
namespace {
constexpr int kPad = 6;          // frame border + margin around the snapshot
constexpr int kThumbScale = 102; // 320x240 -> ~128x96 (LV_SCALE_NONE = 256)
constexpr int kPressDrop = 4;    // pressed tile sinks onto its lip
constexpr float kBobPeriod = 2.4f;
struct Style { uint32_t border, lip; };
constexpr Style kStyle[2] = {{0xF47B6B, 0xC2584B}, {0x3BB3C3, 0x2A8795}};

lv_obj_t* rounded(lv_obj_t* parent, const Rect& r, int dy, uint32_t fill, uint32_t border) {
  auto* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(o, r.x, r.y + dy);
  lv_obj_set_size(o, r.w, r.h);
  lv_obj_set_style_radius(o, 16, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  if (border != fill) {
    lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(o, 4, 0);
  }
  return o;
}
}  // namespace

lv_obj_t* MenuView::image(lv_obj_t* parent, const lv_image_dsc_t* src, int x, int y, int scale) {
  auto* o = lv_image_create(parent);
  lv_image_set_src(o, src);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_pivot(o, 0, 0);
  if (scale != LV_SCALE_NONE) lv_image_set_scale(o, scale);
  lv_obj_set_pos(o, x, y);
  return o;
}

void MenuView::add_actor(lv_obj_t* obj, int tile, int x, int y, float phase, float amp) {
  if (actor_count_ >= static_cast<int>(actors_.size())) return;
  actors_[actor_count_++] = Actor{obj, tile, x, y, y, phase, amp};
}

bool MenuView::create(lv_obj_t* screen, const MenuArt& art) {
  if (!art.cp_bg || !art.cp_crab || !art.aq_backdrop || !art.aq_fish || !art.aq_bubble) return false;
  for (const auto* b : art.cp_bubbles)
    if (!b) return false;
  lv_obj_remove_style_all(screen);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xCDEFFF), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0xFFF1D2), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  for (int i = 0; i < 2; ++i) {
    const Rect& r = kTiles[i];
    rounded(screen, r, kPressDrop + 1, kStyle[i].lip, kStyle[i].lip);
    frames_[i] = rounded(screen, r, 0, 0xFFFFFF, kStyle[i].border);
    thumbs_[i] = image(screen, i == 0 ? art.cp_bg : art.aq_backdrop, r.x + kPad, r.y + kPad, kThumbScale);
    add_actor(frames_[i], i, r.x, r.y, 0, 0);
    add_actor(thumbs_[i], i, r.x + kPad, r.y + kPad, 0, 0);
  }

  // Color Pop: three bubbles rising out of the top of the tile, the happy crab in the sand.
  const Rect& cp = kTiles[0];
  const int bx[3] = {6, 50, 94}, by[3] = {-26, -36, -24};
  for (int k = 0; k < 3; ++k)
    add_actor(image(screen, art.cp_bubbles[k], cp.x + bx[k], cp.y + by[k], 140), 0, cp.x + bx[k], cp.y + by[k],
              k * 2.1f, 3.0f);
  add_actor(image(screen, art.cp_crab, cp.x + 86, cp.y + 58, 230), 0, cp.x + 86, cp.y + 58, 1.3f, 1.5f);

  // Bubble Aquarium: a happy fish swimming in the tile with bubbles escaping above it.
  const Rect& aq = kTiles[1];
  add_actor(image(screen, art.aq_fish, aq.x + 34, aq.y + 22, 190), 1, aq.x + 34, aq.y + 22, 0.7f, 3.0f);
  add_actor(image(screen, art.aq_bubble, aq.x + 18, aq.y - 22, 128), 1, aq.x + 18, aq.y - 22, 2.4f, 3.0f);
  add_actor(image(screen, art.aq_bubble, aq.x + 98, aq.y - 30, 160), 1, aq.x + 98, aq.y - 30, 4.0f, 3.0f);
  return true;
}

void MenuView::sync(const Shell& shell) {
  const float t = static_cast<float>(shell.time());
  for (int i = 0; i < actor_count_; ++i) {
    Actor& a = actors_[i];
    const int press = shell.pressed() == a.tile ? kPressDrop : 0;
    const float bob = a.amp * std::sin(t * 6.2831853f / kBobPeriod + a.phase);
    const int y = a.y + press + static_cast<int>(std::lround(bob));
    if (y != a.cur_y) {
      lv_obj_set_y(a.obj, y);
      a.cur_y = y;
    }
  }
}

}  // namespace launcher
