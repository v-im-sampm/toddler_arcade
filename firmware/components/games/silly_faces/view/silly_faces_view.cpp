#include "little_arcade/games/silly_faces_view.hpp"

#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::silly_faces {
namespace {
using ui::to_lv;
namespace pal = ui::palette;
lv_obj_t* plain(lv_obj_t* parent) {
  auto* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void paint_button(lv_obj_t* o, uint32_t fill, int radius = 13) {
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_color(o, to_lv(fill), 0);
  lv_obj_set_style_bg_grad_color(o, to_lv(ui::mix(fill, ui::tok::kCream.light, 0.24f)), 0);
  lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(o, to_lv(ui::tok::kInk), 0);
  lv_obj_set_style_border_width(o, 2, 0);
}
lv_obj_t* button(lv_obj_t* parent, const Rect& r, const char* text, uint32_t fill) {
  lv_obj_t* shadow = plain(parent);
  lv_obj_set_pos(shadow, r.x + 5, r.y + 6);
  lv_obj_set_size(shadow, r.w - 6, r.h - 6);
  lv_obj_set_style_radius(shadow, r.w > r.h ? 14 : LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(shadow, to_lv(ui::tok::kInk), 0);
  lv_obj_set_style_bg_opa(shadow, LV_OPA_30, 0);
  auto* o = plain(parent);
  lv_obj_set_pos(o, r.x + 2, r.y + 2);
  lv_obj_set_size(o, r.w - 4, r.h - 4);
  paint_button(o, fill, r.w > r.h ? 14 : LV_RADIUS_CIRCLE);
  auto* label = lv_label_create(o);
  lv_label_set_text_static(label, text);
  lv_obj_center(label);
  return o;
}
}  // namespace

void SillyFacesView::create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_t* backdrop = ui::show_backdrop(parent, ui::World::MIRROR_POND);
  face_art_.allocate(art::kFaceW, art::kFaceH);
  face_ = lv_image_create(parent);
  lv_obj_remove_flag(face_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(face_, (kScreenWidth - art::kFaceW) / 2, 48);

  auto* eyes_bg = plain(parent);
  lv_obj_set_pos(eyes_bg, SillyFacesGame::kEyesZone.x, SillyFacesGame::kEyesZone.y);
  lv_obj_set_size(eyes_bg, SillyFacesGame::kEyesZone.w, SillyFacesGame::kEyesZone.h);
  paint_button(eyes_bg, 0xFFF8EC, 20);
  lv_obj_set_style_bg_opa(eyes_bg, LV_OPA_30, 0);
  eyes_zone_ = eyes_bg;
  auto* mouth_bg = plain(parent);
  lv_obj_set_pos(mouth_bg, SillyFacesGame::kMouthZone.x, SillyFacesGame::kMouthZone.y);
  lv_obj_set_size(mouth_bg, SillyFacesGame::kMouthZone.w, SillyFacesGame::kMouthZone.h);
  paint_button(mouth_bg, 0xFFF8EC, 20);
  lv_obj_set_style_bg_opa(mouth_bg, LV_OPA_30, 0);
  mouth_zone_ = mouth_bg;
  // Keep the tap zones behind the artwork. Touch routing remains owned by the game.
  lv_obj_move_to_index(eyes_zone_, 0);
  lv_obj_move_to_index(mouth_zone_, 1);
  lv_obj_move_to_index(face_, 2);
  if (backdrop) lv_obj_move_to_index(backdrop, 0);
  show_button_ = button(parent, SillyFacesGame::kShowButton, "SHOW", pal::kGreen);
  mix_button_ = button(parent, SillyFacesGame::kMixButton, "MIX", pal::kYellow);
  next_button_ = button(parent, SillyFacesGame::kNewMonsterButton, "NEW", pal::kPink);
  prompt_button_ = button(parent, SillyFacesGame::kPromptBadge, "?", pal::kWhite);
  for (std::size_t i = 0; i < kEmotionCount; ++i) {
    stickers_[i] = plain(parent);
    lv_obj_set_pos(stickers_[i], 134 + static_cast<int>(i) * 14, 220);
    lv_obj_set_size(stickers_[i], 9, 9);
    paint_button(stickers_[i], pal::kChromeEdge, LV_RADIUS_CIRCLE);
  }
  initialized_ = false;
}

void SillyFacesView::sync(const SillyFacesGame& game, uint32_t now_ms) {
  (void)now_ms;
  const Face f = game.face();
  const int face_key = static_cast<int>(f.eyes) | (static_cast<int>(f.mouth) << 3) |
                       (static_cast<int>(game.body_color()) << 6) | (static_cast<int>(game.topper()) << 9) |
                       (game.showing_silly() ? 1 << 12 : 0) | (static_cast<int>(game.showing()) << 13);
  if ((!initialized_ || face_key != shown_face_) && face_art_.ready()) {
    shown_face_ = face_key;
    ui::Canvas c = face_art_.canvas();
    c.clear();
    const bool act_out = game.phase() == Phase::CELEBRATE && !game.showing_silly();
    art::paint_face(c, act_out ? game.showing() : f.eyes, act_out ? game.showing() : f.mouth,
                    game.body_color(), game.topper());
    if (face_art_.ready()) {
      lv_image_set_src(face_, face_art_.dsc());
      lv_obj_invalidate(face_);
    }
  }
  const int hint = game.hint_active() ? 1 : 0;
  if (!initialized_ || hint != shown_hint_) {
    shown_hint_ = hint;
    const uint32_t color = hint ? pal::kYellow : pal::kChromeEdge;
    lv_obj_set_style_border_color(eyes_zone_, to_lv(color), 0);
    lv_obj_set_style_border_color(mouth_zone_, to_lv(color), 0);
    lv_obj_set_style_border_width(eyes_zone_, hint ? 3 : 2, 0);
    lv_obj_set_style_border_width(mouth_zone_, hint ? 3 : 2, 0);
    lv_obj_set_style_outline_width(prompt_button_, hint ? 2 : 0, 0);
    lv_obj_set_style_outline_color(prompt_button_, to_lv(pal::kYellow), 0);
  }
  const uint8_t made = game.emotions_made();
  for (std::size_t i = 0; i < kEmotionCount; ++i) {
    lv_obj_set_style_bg_color(stickers_[i], to_lv((made & (1u << i)) ? pal::kYellow : pal::kChromeEdge), 0);
  }
  const Part bouncing = game.bouncing();
  lv_obj_set_style_outline_color(eyes_zone_, to_lv(pal::kGreen), 0);
  lv_obj_set_style_outline_width(eyes_zone_, bouncing == Part::EYES ? 2 : 0, 0);
  lv_obj_set_style_outline_color(mouth_zone_, to_lv(pal::kOrange), 0);
  lv_obj_set_style_outline_width(mouth_zone_, bouncing == Part::MOUTH ? 2 : 0, 0);
  lv_obj_set_style_outline_width(show_button_, bouncing == Part::WHOLE ? 3 : 0, 0);
  lv_obj_set_style_outline_width(mix_button_, bouncing == Part::WHOLE ? 3 : 0, 0);
  lv_obj_set_style_outline_width(next_button_, bouncing == Part::WHOLE ? 3 : 0, 0);
  initialized_ = true;
}

}  // namespace little_arcade::silly_faces
